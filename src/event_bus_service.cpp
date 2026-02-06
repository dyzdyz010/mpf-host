#include "event_bus_service.h"
#include "cross_dll_safety.h"

#include <QDateTime>
#include <QMetaObject>
#include <QUuid>
#include <QDebug>

#include <algorithm>

namespace mpf {

using CrossDllSafety::deepCopy;

EventBusService::EventBusService(QObject* parent)
    : QObject(parent)
{
}

EventBusService::~EventBusService() = default;

int EventBusService::publish(const QString& topic,
                              const QVariantMap& data,
                              const QString& senderId)
{
    Event event;
    event.topic = topic;
    event.senderId = senderId;
    event.data = data;
    event.timestamp = QDateTime::currentMSecsSinceEpoch();

    return deliverEvent(event, false);  // async
}

int EventBusService::publishSync(const QString& topic,
                                  const QVariantMap& data,
                                  const QString& senderId)
{
    Event event;
    event.topic = topic;
    event.senderId = senderId;
    event.data = data;
    event.timestamp = QDateTime::currentMSecsSinceEpoch();

    return deliverEvent(event, true);  // sync
}

int EventBusService::deliverEvent(const Event& event, bool synchronous)
{
    QList<const Subscription*> matches;

    {
        QMutexLocker locker(&m_mutex);

        // Update topic stats
        TopicData& stats = m_topicStats[event.topic];
        stats.topic = event.topic;
        stats.eventCount++;
        stats.lastEventTime = event.timestamp;

        // Find matching subscriptions
        matches = findMatchingSubscriptions(event.topic);
    }

    if (matches.isEmpty()) {
        return 0;
    }

    // Sort by priority (descending - higher priority first)
    std::sort(matches.begin(), matches.end(),
              [](const Subscription* a, const Subscription* b) {
                  return a->options.priority > b->options.priority;
              });

    int notified = 0;

    for (const Subscription* sub : matches) {
        // Skip if sender doesn't want own events
        if (!sub->options.receiveOwnEvents && sub->subscriberId == event.senderId) {
            continue;
        }

        notified++;
    }

    // Emit signal for subscribers
    if (synchronous) {
        // Direct emission (blocking)
        emit eventPublished(event.topic, event.data, event.senderId);
    } else {
        // Queued emission (async)
        QMetaObject::invokeMethod(this, [this, event]() {
            emit eventPublished(event.topic, event.data, event.senderId);
        }, Qt::QueuedConnection);
    }

    return notified;
}

QString EventBusService::subscribe(const QString& pattern,
                                    const QString& subscriberId,
                                    const SubscriptionOptions& options)
{
    Subscription sub;
    sub.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // Deep copy strings from plugin to ensure they're in host's heap
    sub.pattern = deepCopy(pattern);
    sub.subscriberId = deepCopy(subscriberId);
    sub.options = options;
    sub.regex = compilePattern(pattern);

    {
        QMutexLocker locker(&m_mutex);
        m_subscriptions.insert(sub.id, sub);
        m_subscriberIndex[sub.subscriberId].append(sub.id);
    }

    qDebug() << "EventBus: Subscribed" << subscriberId << "to" << pattern
             << "id:" << sub.id;

    emit subscriptionAdded(sub.id, pattern);
    emit subscribersChanged();
    emit topicsChanged();

    // Deep copy before returning
    return deepCopy(sub.id);
}

bool EventBusService::unsubscribe(const QString& subscriptionId)
{
    QString subscriberId;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_subscriptions.find(subscriptionId);
        if (it == m_subscriptions.end()) {
            return false;
        }

        subscriberId = it->subscriberId;
        m_subscriptions.erase(it);
        m_subscriberIndex[subscriberId].removeAll(subscriptionId);

        if (m_subscriberIndex[subscriberId].isEmpty()) {
            m_subscriberIndex.remove(subscriberId);
        }
    }

    qDebug() << "EventBus: Unsubscribed" << subscriptionId;

    emit subscriptionRemoved(subscriptionId);
    emit subscribersChanged();
    emit topicsChanged();

    return true;
}

void EventBusService::unsubscribeAll(const QString& subscriberId)
{
    QStringList ids;

    {
        QMutexLocker locker(&m_mutex);
        ids = m_subscriberIndex.take(subscriberId);

        for (const QString& id : ids) {
            m_subscriptions.remove(id);
        }
    }

    for (const QString& id : ids) {
        emit subscriptionRemoved(id);
    }

    if (!ids.isEmpty()) {
        qDebug() << "EventBus: Unsubscribed all for" << subscriberId
                 << "(" << ids.size() << "subscriptions)";
        emit subscribersChanged();
        emit topicsChanged();
    }
}

int EventBusService::subscriberCount(const QString& topic) const
{
    QMutexLocker locker(&m_mutex);

    int count = 0;
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        if (it->regex.match(topic).hasMatch()) {
            count++;
        }
    }
    return count;
}

QStringList EventBusService::activeTopics() const
{
    QMutexLocker locker(&m_mutex);

    QSet<QString> patterns;
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        patterns.insert(it->pattern);
    }
    return deepCopy(patterns.values());
}

TopicStats EventBusService::topicStats(const QString& topic) const
{
    QMutexLocker locker(&m_mutex);

    TopicStats stats;
    stats.topic = topic;
    stats.subscriberCount = 0;

    // Count subscribers
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        if (it->regex.match(topic).hasMatch()) {
            stats.subscriberCount++;
        }
    }

    // Get event stats
    auto dataIt = m_topicStats.find(topic);
    if (dataIt != m_topicStats.end()) {
        stats.eventCount = dataIt->eventCount;
        stats.lastEventTime = dataIt->lastEventTime;
    }

    return stats;
}

QStringList EventBusService::subscriptionsFor(const QString& subscriberId) const
{
    QMutexLocker locker(&m_mutex);
    return deepCopy(m_subscriberIndex.value(subscriberId));
}

bool EventBusService::matchesTopic(const QString& topic, const QString& pattern) const
{
    QRegularExpression regex = compilePattern(pattern);
    return regex.match(topic).hasMatch();
}

QString EventBusService::subscribeSimple(const QString& pattern, const QString& subscriberId)
{
    return subscribe(pattern, subscriberId, SubscriptionOptions{});
}

QVariantMap EventBusService::topicStatsAsVariant(const QString& topic) const
{
    return deepCopy(topicStats(topic).toVariantMap());
}

int EventBusService::totalSubscribers() const
{
    QMutexLocker locker(&m_mutex);
    return m_subscriptions.size();
}

QRegularExpression EventBusService::compilePattern(const QString& pattern) const
{
    // Convert topic pattern to regex:
    // ** -> .+    (matches multiple levels, must be done first)
    // *  -> [^/]+ (matches single level)

    QString regex = QRegularExpression::escape(pattern);
    regex.replace("\\*\\*", "<<DOUBLE_STAR>>");  // Placeholder to avoid conflicts
    regex.replace("\\*", "[^/]+");
    regex.replace("<<DOUBLE_STAR>>", ".+");
    regex = "^" + regex + "$";

    return QRegularExpression(regex);
}

QList<const EventBusService::Subscription*> EventBusService::findMatchingSubscriptions(const QString& topic) const
{
    // Note: must be called with m_mutex held
    QList<const Subscription*> result;

    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        if (it->regex.match(topic).hasMatch()) {
            result.append(&(*it));
        }
    }

    return result;
}

} // namespace mpf
