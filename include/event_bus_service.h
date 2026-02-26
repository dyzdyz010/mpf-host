#pragma once

#include <mpf/interfaces/ieventbus.h>

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QRegularExpression>

namespace mpf {

/**
 * @brief Default event bus service implementation
 *
 * Provides publish/subscribe messaging with:
 * - Wildcard topic matching (* and **)
 * - Priority-based delivery ordering
 * - Async and sync event delivery
 * - Thread-safe operations
 */
class EventBusService : public QObject, public IEventBus
{
    Q_OBJECT
    Q_PROPERTY(int totalSubscribers READ totalSubscribers NOTIFY subscribersChanged)
    Q_PROPERTY(QStringList topics READ activeTopics NOTIFY topicsChanged)

public:
    explicit EventBusService(QObject* parent = nullptr);
    ~EventBusService() override;

    // IEventBus interface - Publishing
    Q_INVOKABLE int publish(const QString& topic,
                            const QVariantMap& data,
                            const QString& senderId = {}) override;

    Q_INVOKABLE int publishSync(const QString& topic,
                                const QVariantMap& data,
                                const QString& senderId = {}) override;

    // IEventBus interface - Subscribing
    Q_INVOKABLE QString subscribe(const QString& pattern,
                                  const QString& subscriberId,
                                  const SubscriptionOptions& options = {}) override;

    Q_INVOKABLE bool unsubscribe(const QString& subscriptionId) override;
    Q_INVOKABLE void unsubscribeAll(const QString& subscriberId) override;

    // IEventBus interface - Query
    Q_INVOKABLE int subscriberCount(const QString& topic) const override;
    Q_INVOKABLE QStringList activeTopics() const override;
    Q_INVOKABLE TopicStats topicStats(const QString& topic) const override;
    Q_INVOKABLE QStringList subscriptionsFor(const QString& subscriberId) const override;
    Q_INVOKABLE bool matchesTopic(const QString& topic, const QString& pattern) const override;

    // QML-friendly overloads (simpler signatures)
    Q_INVOKABLE QString subscribeSimple(const QString& pattern, const QString& subscriberId);
    Q_INVOKABLE QVariantMap topicStatsAsVariant(const QString& topic) const;

    // Property accessor
    int totalSubscribers() const;

signals:
    /**
     * @brief Emitted when an event is published (for QML/C++ subscribers)
     * @param topic The event topic
     * @param data The event payload
     * @param senderId The sender's plugin ID
     */
    void eventPublished(const QString& topic, const QVariantMap& data, const QString& senderId);

    /**
     * @brief Emitted when subscriber count changes
     */
    void subscribersChanged();

    /**
     * @brief Emitted when active topics change
     */
    void topicsChanged();

    /**
     * @brief Emitted when a subscription is added
     */
    void subscriptionAdded(const QString& subscriptionId, const QString& pattern);

    /**
     * @brief Emitted when a subscription is removed
     */
    void subscriptionRemoved(const QString& subscriptionId);

private:
    struct Subscription {
        QString id;
        QString pattern;
        QString subscriberId;
        SubscriptionOptions options;
        QRegularExpression regex;
    };

    struct TopicData {
        QString topic;
        qint64 eventCount = 0;
        qint64 lastEventTime = 0;
    };

    int deliverEvent(const Event& event, bool synchronous);
    QRegularExpression compilePattern(const QString& pattern) const;
    QList<const Subscription*> findMatchingSubscriptions(const QString& topic) const;

    mutable QMutex m_mutex;
    QHash<QString, Subscription> m_subscriptions;       // subscriptionId -> Subscription
    QHash<QString, QStringList> m_subscriberIndex;      // subscriberId -> [subscriptionIds]
    QHash<QString, TopicData> m_topicStats;             // topic -> stats
};

} // namespace mpf
