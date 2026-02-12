#pragma once

#include <mpf/interfaces/ieventbus.h>

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QRegularExpression>

namespace mpf {

/**
 * @brief Event bus service implementation
 *
 * Provides:
 * - Publish/Subscribe with callback handlers
 * - Request/Response for synchronous cross-plugin calls
 * - Wildcard topic matching (* and **)
 * - Priority-based delivery ordering
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

    // ===== Publish/Subscribe =====

    Q_INVOKABLE int publish(const QString& topic,
                            const QVariantMap& data,
                            const QString& senderId = {}) override;

    Q_INVOKABLE int publishSync(const QString& topic,
                                const QVariantMap& data,
                                const QString& senderId = {}) override;

    QString subscribe(const QString& pattern,
                      const QString& subscriberId,
                      EventHandler handler,
                      const SubscriptionOptions& options = {}) override;

    Q_INVOKABLE bool unsubscribe(const QString& subscriptionId) override;
    Q_INVOKABLE void unsubscribeAll(const QString& subscriberId) override;

    // ===== Request/Response =====

    bool registerHandler(const QString& topic,
                         const QString& handlerId,
                         RequestHandler handler) override;
    bool unregisterHandler(const QString& topic) override;
    void unregisterAllHandlers(const QString& handlerId) override;
    std::optional<QVariantMap> request(const QString& topic,
                                       const QVariantMap& data = {},
                                       const QString& senderId = {},
                                       int timeoutMs = 0) override;
    bool hasHandler(const QString& topic) const override;

    // QML-friendly request (returns map with __success field)
    Q_INVOKABLE QVariantMap requestFromQml(const QString& topic,
                                           const QVariantMap& data = {},
                                           const QString& senderId = {},
                                           int timeoutMs = 0);

    // ===== Query =====

    Q_INVOKABLE int subscriberCount(const QString& topic) const override;
    Q_INVOKABLE QStringList activeTopics() const override;
    Q_INVOKABLE TopicStats topicStats(const QString& topic) const override;
    Q_INVOKABLE QStringList subscriptionsFor(const QString& subscriberId) const override;
    Q_INVOKABLE bool matchesTopic(const QString& topic, const QString& pattern) const override;

    Q_INVOKABLE QVariantMap topicStatsAsVariant(const QString& topic) const;
    Q_INVOKABLE bool hasHandlerQml(const QString& topic) const { return hasHandler(topic); }

    int totalSubscribers() const;

signals:
    void subscribersChanged();
    void topicsChanged();
    void subscriptionAdded(const QString& subscriptionId, const QString& pattern);
    void subscriptionRemoved(const QString& subscriptionId);

private:
    struct Subscription {
        QString id;
        QString pattern;
        QString subscriberId;
        SubscriptionOptions options;
        QRegularExpression regex;
        EventHandler handler;
    };

    struct TopicData {
        QString topic;
        qint64 eventCount = 0;
        qint64 lastEventTime = 0;
    };

    struct HandlerEntry {
        QString topic;
        QString handlerId;
        RequestHandler handler;
    };

    int deliverEvent(const Event& event, bool synchronous);
    QRegularExpression compilePattern(const QString& pattern) const;
    QList<const Subscription*> findMatchingSubscriptions(const QString& topic) const;

    mutable QMutex m_mutex;
    QHash<QString, Subscription> m_subscriptions;
    QHash<QString, QStringList> m_subscriberIndex;
    QHash<QString, TopicData> m_topicStats;
    QHash<QString, HandlerEntry> m_requestHandlers;
    QHash<QString, QStringList> m_handlerIndex;
};

} // namespace mpf
