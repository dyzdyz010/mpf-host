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

// =============================================================================
// Publish/Subscribe
// =============================================================================

int EventBusService::publish(const QString& topic,
                              const QVariantMap& data,
                              const QString& senderId)
{
    Event event;
    event.topic = topic;
    event.senderId = senderId;
    event.data = data;
    event.timestamp = QDateTime::currentMSecsSinceEpoch();
    return deliverEvent(event, false);
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
    return deliverEvent(event, true);
}

int EventBusService::deliverEvent(const Event& event, bool synchronous)
{
    QList<const Subscription*> matches;

    {
        QMutexLocker locker(&m_mutex);

        TopicData& stats = m_topicStats[event.topic];
        stats.topic = event.topic;
        stats.eventCount++;
        stats.lastEventTime = event.timestamp;

        matches = findMatchingSubscriptions(event.topic);
    }

    if (matches.isEmpty()) {
        return 0;
    }

    // Sort by priority (descending)
    std::sort(matches.begin(), matches.end(),
              [](const Subscription* a, const Subscription* b) {
                  return a->options.priority > b->options.priority;
              });

    int notified = 0;

    for (const Subscription* sub : matches) {
        if (!sub->options.receiveOwnEvents && sub->subscriberId == event.senderId) {
            continue;
        }
        notified++;

        if (sub->options.async && !synchronous) {
            Event eventCopy = event;
            eventCopy.topic = deepCopy(event.topic);
            eventCopy.senderId = deepCopy(event.senderId);
            eventCopy.data = deepCopy(event.data);
            auto handler = sub->handler;
            QMetaObject::invokeMethod(this, [handler, eventCopy]() {
                handler(eventCopy);
            }, Qt::QueuedConnection);
        } else {
            sub->handler(event);
        }
    }

    return notified;
}

QString EventBusService::subscribe(const QString& pattern,
                                    const QString& subscriberId,
                                    EventHandler handler,
                                    const SubscriptionOptions& options)
{
    if (!handler) {
        qWarning() << "EventBus: Cannot subscribe with null handler";
        return {};
    }

    Subscription sub;
    sub.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sub.pattern = deepCopy(pattern);
    sub.subscriberId = deepCopy(subscriberId);
    sub.options = options;
    sub.regex = compilePattern(pattern);
    sub.handler = std::move(handler);

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

// =============================================================================
// Request/Response
// =============================================================================

bool EventBusService::registerHandler(const QString& topic,
                                       const QString& handlerId,
                                       RequestHandler handler)
{
    if (topic.isEmpty() || !handler) {
        qWarning() << "EventBus: Cannot register handler with empty topic or null handler";
        return false;
    }

    QString topicCopy = deepCopy(topic);
    QString handlerIdCopy = deepCopy(handlerId);

    QMutexLocker locker(&m_mutex);

    if (m_requestHandlers.contains(topicCopy)) {
        qWarning() << "EventBus: Handler already registered for topic:" << topicCopy
                   << "by" << m_requestHandlers[topicCopy].handlerId;
        return false;
    }

    HandlerEntry entry;
    entry.topic = topicCopy;
    entry.handlerId = handlerIdCopy;
    entry.handler = std::move(handler);

    m_requestHandlers.insert(topicCopy, entry);
    m_handlerIndex[handlerIdCopy].append(topicCopy);

    qDebug() << "EventBus: Registered handler for" << topicCopy << "by" << handlerIdCopy;
    return true;
}

bool EventBusService::unregisterHandler(const QString& topic)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_requestHandlers.find(topic);
    if (it == m_requestHandlers.end()) {
        return false;
    }

    QString handlerId = it->handlerId;
    m_requestHandlers.erase(it);
    m_handlerIndex[handlerId].removeAll(topic);
    if (m_handlerIndex[handlerId].isEmpty()) {
        m_handlerIndex.remove(handlerId);
    }

    qDebug() << "EventBus: Unregistered handler for" << topic;
    return true;
}

void EventBusService::unregisterAllHandlers(const QString& handlerId)
{
    QMutexLocker locker(&m_mutex);

    QStringList topics = m_handlerIndex.take(handlerId);
    for (const QString& topic : topics) {
        m_requestHandlers.remove(topic);
    }

    if (!topics.isEmpty()) {
        qDebug() << "EventBus: Unregistered all handlers for" << handlerId
                 << "(" << topics.size() << "topics)";
    }
}

std::optional<QVariantMap> EventBusService::request(const QString& topic,
                                                     const QVariantMap& data,
                                                     const QString& senderId,
                                                     int timeoutMs)
{
    Q_UNUSED(timeoutMs);

    RequestHandler handler;

    {
        QMutexLocker locker(&m_mutex);
        auto it = m_requestHandlers.find(topic);
        if (it == m_requestHandlers.end()) {
            qDebug() << "EventBus: No handler for request:" << topic;
            return std::nullopt;
        }
        handler = it->handler;
    }

    Event event;
    event.topic = topic;
    event.senderId = senderId;
    event.data = data;
    event.timestamp = QDateTime::currentMSecsSinceEpoch();

    try {
        QVariantMap response = handler(event);
        return deepCopy(response);
    } catch (const std::exception& e) {
        qWarning() << "EventBus: Handler exception for" << topic << ":" << e.what();
        return std::nullopt;
    } catch (...) {
        qWarning() << "EventBus: Handler unknown exception for" << topic;
        return std::nullopt;
    }
}

bool EventBusService::hasHandler(const QString& topic) const
{
    QMutexLocker locker(&m_mutex);
    return m_requestHandlers.contains(topic);
}

QVariantMap EventBusService::requestFromQml(const QString& topic,
                                             const QVariantMap& data,
                                             const QString& senderId,
                                             int timeoutMs)
{
    auto result = request(topic, data, senderId, timeoutMs);
    if (result.has_value()) {
        QVariantMap response = result.value();
        response["__success"] = true;
        return response;
    }
    return {{"__success", false}, {"__error", "No handler or handler failed"}};
}

// =============================================================================
// Query
// =============================================================================

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

    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        if (it->regex.match(topic).hasMatch()) {
            stats.subscriberCount++;
        }
    }

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

QVariantMap EventBusService::topicStatsAsVariant(const QString& topic) const
{
    return deepCopy(topicStats(topic).toVariantMap());
}

int EventBusService::totalSubscribers() const
{
    QMutexLocker locker(&m_mutex);
    return m_subscriptions.size();
}

// =============================================================================
// Internal
// =============================================================================

QRegularExpression EventBusService::compilePattern(const QString& pattern) const
{
    QString regex = QRegularExpression::escape(pattern);
    regex.replace("\\*\\*", "<<DOUBLE_STAR>>");
    regex.replace("\\*", "[^/]+");
    regex.replace("<<DOUBLE_STAR>>", ".+");
    regex = "^" + regex + "$";
    return QRegularExpression(regex);
}

QList<const EventBusService::Subscription*> EventBusService::findMatchingSubscriptions(const QString& topic) const
{
    QList<const Subscription*> result;
    for (auto it = m_subscriptions.constBegin(); it != m_subscriptions.constEnd(); ++it) {
        if (it->regex.match(topic).hasMatch()) {
            result.append(&(*it));
        }
    }
    return result;
}

} // namespace mpf
