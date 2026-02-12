#include <QTest>
#include <QCoreApplication>

#include "event_bus_service.h"

using namespace mpf;

class TestEventBus : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Subscribe/Publish
    void testSubscribe();
    void testUnsubscribe();
    void testUnsubscribeAll();
    void testPublishSync();
    void testPublishAsync();
    void testNullHandlerRejected();

    // Wildcard matching
    void testSingleWildcard();
    void testDoubleWildcard();
    void testMixedWildcards();
    void testMatchesTopic();

    // Options
    void testPriority();
    void testReceiveOwnEvents();

    // Query
    void testSubscriberCount();
    void testActiveTopics();
    void testTopicStats();
    void testSubscriptionsFor();

    // Request/Response
    void testRegisterHandler();
    void testRequestResponse();
    void testRequestNoHandler();
    void testUnregisterHandler();
    void testUnregisterAllHandlers();
    void testHasHandler();
    void testRequestHandlerException();
    void testDuplicateHandler();
    void testRequestFromQml();

    // Edge cases
    void testMultipleSubscribers();
    void testNoSubscribers();

private:
    EventBusService* m_bus = nullptr;
};

void TestEventBus::initTestCase()
{
    qDebug() << "========== EventBus Test Suite (v3, no legacy) ==========";
}

void TestEventBus::cleanupTestCase()
{
    qDebug() << "========== Tests Complete ==========";
}

void TestEventBus::init()
{
    m_bus = new EventBusService(this);
}

void TestEventBus::cleanup()
{
    delete m_bus;
    m_bus = nullptr;
}

// =============================================================================
// Subscribe/Publish
// =============================================================================

void TestEventBus::testSubscribe()
{
    int received = 0;
    QString subId = m_bus->subscribe("orders/created", "plugin-a",
        [&received](const Event&) { received++; });

    QVERIFY(!subId.isEmpty());
    QCOMPARE(m_bus->totalSubscribers(), 1);
    QVERIFY(m_bus->activeTopics().contains("orders/created"));
}

void TestEventBus::testUnsubscribe()
{
    QString subId = m_bus->subscribe("orders/created", "plugin-a",
        [](const Event&) {});
    QCOMPARE(m_bus->totalSubscribers(), 1);

    QVERIFY(m_bus->unsubscribe(subId));
    QCOMPARE(m_bus->totalSubscribers(), 0);
    QVERIFY(!m_bus->unsubscribe("non-existent"));
}

void TestEventBus::testUnsubscribeAll()
{
    m_bus->subscribe("t1", "plugin-a", [](const Event&) {});
    m_bus->subscribe("t2", "plugin-a", [](const Event&) {});
    m_bus->subscribe("t3", "plugin-b", [](const Event&) {});

    QCOMPARE(m_bus->totalSubscribers(), 3);
    m_bus->unsubscribeAll("plugin-a");
    QCOMPARE(m_bus->totalSubscribers(), 1);
    QVERIFY(m_bus->subscriptionsFor("plugin-a").isEmpty());
    QCOMPARE(m_bus->subscriptionsFor("plugin-b").size(), 1);
}

void TestEventBus::testPublishSync()
{
    QList<Event> received;
    m_bus->subscribe("orders/created", "plugin-a",
        [&received](const Event& e) { received.append(e); });

    int notified = m_bus->publishSync("orders/created",
        {{"orderId", "123"}, {"amount", 99.99}}, "plugin-b");

    QCOMPARE(notified, 1);
    QCOMPARE(received.size(), 1);
    QCOMPARE(received[0].topic, QString("orders/created"));
    QCOMPARE(received[0].data["orderId"].toString(), QString("123"));
    QCOMPARE(received[0].senderId, QString("plugin-b"));
}

void TestEventBus::testPublishAsync()
{
    QList<Event> received;
    m_bus->subscribe("orders/created", "plugin-a",
        [&received](const Event& e) { received.append(e); });

    m_bus->publish("orders/created", {{"key", "val"}}, "plugin-b");
    QCOMPARE(received.size(), 0);  // Not yet

    QCoreApplication::processEvents();
    QCOMPARE(received.size(), 1);
    QCOMPARE(received[0].data["key"].toString(), QString("val"));
}

void TestEventBus::testNullHandlerRejected()
{
    QString subId = m_bus->subscribe("test", "plugin-a", IEventBus::EventHandler{});
    QVERIFY(subId.isEmpty());
    QCOMPARE(m_bus->totalSubscribers(), 0);
}

// =============================================================================
// Wildcard matching
// =============================================================================

void TestEventBus::testSingleWildcard()
{
    m_bus->subscribe("orders/*", "plugin-a", [](const Event&) {});

    QCOMPARE(m_bus->subscriberCount("orders/created"), 1);
    QCOMPARE(m_bus->subscriberCount("orders/updated"), 1);
    QCOMPARE(m_bus->subscriberCount("orders/items/added"), 0);
    QCOMPARE(m_bus->subscriberCount("orders"), 0);
}

void TestEventBus::testDoubleWildcard()
{
    m_bus->subscribe("orders/**", "plugin-a", [](const Event&) {});

    QCOMPARE(m_bus->subscriberCount("orders/created"), 1);
    QCOMPARE(m_bus->subscriberCount("orders/items/added"), 1);
    QCOMPARE(m_bus->subscriberCount("orders"), 0);
    QCOMPARE(m_bus->subscriberCount("products/created"), 0);
}

void TestEventBus::testMixedWildcards()
{
    m_bus->subscribe("*/items/**", "plugin-a", [](const Event&) {});

    QCOMPARE(m_bus->subscriberCount("orders/items/added"), 1);
    QCOMPARE(m_bus->subscriberCount("products/items/removed"), 1);
    QCOMPARE(m_bus->subscriberCount("orders/created"), 0);
}

void TestEventBus::testMatchesTopic()
{
    QVERIFY(m_bus->matchesTopic("orders/created", "orders/*"));
    QVERIFY(m_bus->matchesTopic("orders/items/added", "orders/**"));
    QVERIFY(!m_bus->matchesTopic("orders/items/added", "orders/*"));
    QVERIFY(!m_bus->matchesTopic("orders", "orders/*"));
}

// =============================================================================
// Options
// =============================================================================

void TestEventBus::testPriority()
{
    QStringList order;

    SubscriptionOptions low;
    low.priority = 1;
    low.async = false;
    m_bus->subscribe("test", "low", [&order](const Event&) { order.append("low"); }, low);

    SubscriptionOptions high;
    high.priority = 10;
    high.async = false;
    m_bus->subscribe("test", "high", [&order](const Event&) { order.append("high"); }, high);

    m_bus->publishSync("test", {}, "sender");

    QCOMPARE(order.size(), 2);
    QCOMPARE(order[0], QString("high"));
    QCOMPARE(order[1], QString("low"));
}

void TestEventBus::testReceiveOwnEvents()
{
    int received = 0;
    m_bus->subscribe("test", "plugin-a",
        [&received](const Event&) { received++; });

    m_bus->publishSync("test", {}, "plugin-a");
    QCOMPARE(received, 0);  // Default: don't receive own

    m_bus->publishSync("test", {}, "plugin-b");
    QCOMPARE(received, 1);

    // Now with receiveOwnEvents
    m_bus->unsubscribeAll("plugin-a");
    received = 0;

    SubscriptionOptions opts;
    opts.receiveOwnEvents = true;
    opts.async = false;
    m_bus->subscribe("test", "plugin-a",
        [&received](const Event&) { received++; }, opts);

    m_bus->publishSync("test", {}, "plugin-a");
    QCOMPARE(received, 1);
}

// =============================================================================
// Query
// =============================================================================

void TestEventBus::testSubscriberCount()
{
    m_bus->subscribe("orders/*", "a", [](const Event&) {});
    m_bus->subscribe("orders/created", "b", [](const Event&) {});
    m_bus->subscribe("products/*", "c", [](const Event&) {});

    QCOMPARE(m_bus->subscriberCount("orders/created"), 2);
    QCOMPARE(m_bus->subscriberCount("orders/updated"), 1);
    QCOMPARE(m_bus->subscriberCount("products/new"), 1);
    QCOMPARE(m_bus->subscriberCount("unknown"), 0);
}

void TestEventBus::testActiveTopics()
{
    m_bus->subscribe("orders/created", "a", [](const Event&) {});
    m_bus->subscribe("orders/*", "b", [](const Event&) {});
    m_bus->subscribe("products/**", "c", [](const Event&) {});

    QStringList topics = m_bus->activeTopics();
    QCOMPARE(topics.size(), 3);
    QVERIFY(topics.contains("orders/created"));
    QVERIFY(topics.contains("orders/*"));
    QVERIFY(topics.contains("products/**"));
}

void TestEventBus::testTopicStats()
{
    SubscriptionOptions sync;
    sync.async = false;
    m_bus->subscribe("orders/created", "a", [](const Event&) {}, sync);
    m_bus->subscribe("orders/*", "b", [](const Event&) {}, sync);

    m_bus->publishSync("orders/created", {}, "sender");
    m_bus->publishSync("orders/created", {}, "sender");
    m_bus->publishSync("orders/created", {}, "sender");

    TopicStats stats = m_bus->topicStats("orders/created");
    QCOMPARE(stats.subscriberCount, 2);
    QCOMPARE(stats.eventCount, 3);
    QVERIFY(stats.lastEventTime > 0);
}

void TestEventBus::testSubscriptionsFor()
{
    QString s1 = m_bus->subscribe("t1", "plugin-a", [](const Event&) {});
    QString s2 = m_bus->subscribe("t2", "plugin-a", [](const Event&) {});
    m_bus->subscribe("t3", "plugin-b", [](const Event&) {});

    QStringList subs = m_bus->subscriptionsFor("plugin-a");
    QCOMPARE(subs.size(), 2);
    QVERIFY(subs.contains(s1));
    QVERIFY(subs.contains(s2));
}

// =============================================================================
// Request/Response
// =============================================================================

void TestEventBus::testRegisterHandler()
{
    bool ok = m_bus->registerHandler("orders/getAll", "plugin-orders",
        [](const Event&) -> QVariantMap { return {{"orders", QVariantList{}}}; });
    QVERIFY(ok);
    QVERIFY(m_bus->hasHandler("orders/getAll"));
}

void TestEventBus::testRequestResponse()
{
    m_bus->registerHandler("orders/getById", "plugin-orders",
        [](const Event& e) -> QVariantMap {
            return {
                {"id", e.data["id"]},
                {"customer", "John"},
                {"amount", 99.99}
            };
        });

    auto result = m_bus->request("orders/getById", {{"id", "42"}}, "dashboard");
    QVERIFY(result.has_value());
    QCOMPARE(result->value("id").toString(), QString("42"));
    QCOMPARE(result->value("customer").toString(), QString("John"));
}

void TestEventBus::testRequestNoHandler()
{
    auto result = m_bus->request("nonexistent", {}, "sender");
    QVERIFY(!result.has_value());
}

void TestEventBus::testUnregisterHandler()
{
    m_bus->registerHandler("test", "a", [](const Event&) -> QVariantMap { return {}; });
    QVERIFY(m_bus->hasHandler("test"));
    QVERIFY(m_bus->unregisterHandler("test"));
    QVERIFY(!m_bus->hasHandler("test"));
    QVERIFY(!m_bus->unregisterHandler("test"));
}

void TestEventBus::testUnregisterAllHandlers()
{
    m_bus->registerHandler("a", "plugin-a", [](const Event&) -> QVariantMap { return {}; });
    m_bus->registerHandler("b", "plugin-a", [](const Event&) -> QVariantMap { return {}; });
    m_bus->registerHandler("c", "plugin-b", [](const Event&) -> QVariantMap { return {}; });

    m_bus->unregisterAllHandlers("plugin-a");
    QVERIFY(!m_bus->hasHandler("a"));
    QVERIFY(!m_bus->hasHandler("b"));
    QVERIFY(m_bus->hasHandler("c"));
}

void TestEventBus::testHasHandler()
{
    QVERIFY(!m_bus->hasHandler("any"));
    m_bus->registerHandler("orders/count", "orders",
        [](const Event&) -> QVariantMap { return {{"count", 42}}; });
    QVERIFY(m_bus->hasHandler("orders/count"));
    QVERIFY(!m_bus->hasHandler("orders/other"));
}

void TestEventBus::testRequestHandlerException()
{
    m_bus->registerHandler("broken", "broken",
        [](const Event&) -> QVariantMap { throw std::runtime_error("boom"); });

    auto result = m_bus->request("broken", {}, "sender");
    QVERIFY(!result.has_value());
}

void TestEventBus::testDuplicateHandler()
{
    QVERIFY(m_bus->registerHandler("dup", "a",
        [](const Event&) -> QVariantMap { return {{"from", "a"}}; }));
    QVERIFY(!m_bus->registerHandler("dup", "b",
        [](const Event&) -> QVariantMap { return {{"from", "b"}}; }));

    auto result = m_bus->request("dup", {}, "sender");
    QCOMPARE(result->value("from").toString(), QString("a"));
}

void TestEventBus::testRequestFromQml()
{
    m_bus->registerHandler("qml/test", "qml-plugin",
        [](const Event&) -> QVariantMap { return {{"msg", "hello"}}; });

    QVariantMap ok = m_bus->requestFromQml("qml/test");
    QCOMPARE(ok["__success"].toBool(), true);
    QCOMPARE(ok["msg"].toString(), QString("hello"));

    QVariantMap fail = m_bus->requestFromQml("nope");
    QCOMPARE(fail["__success"].toBool(), false);
}

// =============================================================================
// Edge cases
// =============================================================================

void TestEventBus::testMultipleSubscribers()
{
    int count = 0;
    SubscriptionOptions sync;
    sync.async = false;

    m_bus->subscribe("shared", "a", [&count](const Event&) { count++; }, sync);
    m_bus->subscribe("shared", "b", [&count](const Event&) { count++; }, sync);
    m_bus->subscribe("shared", "c", [&count](const Event&) { count++; }, sync);

    int notified = m_bus->publishSync("shared", {}, "external");
    QCOMPARE(notified, 3);
    QCOMPARE(count, 3);
}

void TestEventBus::testNoSubscribers()
{
    int notified = m_bus->publishSync("nobody/listening", {}, "sender");
    QCOMPARE(notified, 0);
}

QTEST_MAIN(TestEventBus)
#include "test_event_bus.moc"
