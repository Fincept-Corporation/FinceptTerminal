// Fincept Terminal — DataHub Phase 1 unit tests
//
// Validates the core pub/sub primitive without any producers or
// app-wide plumbing. Uses Qt Test.

#include "datahub/DataHub.h"
#include "datahub/Producer.h"

#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QVariant>

#include <atomic>

using fincept::datahub::DataHub;
using fincept::datahub::Producer;
using fincept::datahub::TopicPolicy;

namespace {

/// Minimal producer that records each `refresh()` call it receives and
/// lets the test drive what it publishes back.
class RecordingProducer : public Producer {
  public:
    explicit RecordingProducer(QStringList patterns, int rps = 0)
        : patterns_(std::move(patterns)), rps_(rps) {}

    QStringList topic_patterns() const override { return patterns_; }
    int max_requests_per_sec() const override { return rps_; }

    void refresh(const QStringList& topics) override {
        ++refresh_count;
        last_refresh_topics = topics;
        for (const auto& t : topics)
            all_requested_topics.append(t);
    }

    int refresh_count = 0;
    QStringList last_refresh_topics;
    QStringList all_requested_topics;

  private:
    QStringList patterns_;
    int rps_;
};

} // namespace

class TestDataHub : public QObject {
    Q_OBJECT

  private slots:

    // Drain any queued invocations between tests so timer fires from one
    // test don't leak into the next. DataHub is a singleton so state is
    // shared across the whole run; we reset per-test side-effects here.
    void cleanup() {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }

    // Core flow — subscribe, publish, slot invoked.
    void subscribe_publish_delivers() {
        auto& hub = DataHub::instance();
        QObject owner;
        QVariant received;
        hub.subscribe(&owner, "test:basic:1",
                      [&](const QVariant& v) { received = v; });

        hub.publish("test:basic:1", QVariant(42));
        QTRY_COMPARE(received.toInt(), 42);
    }

    // Subscriber receives cached value immediately if one exists + is fresh.
    void subscribe_receives_initial_value() {
        auto& hub = DataHub::instance();

        // Seed a value first, then subscribe.
        hub.publish("test:initial:1", QVariant(QString("hello")));

        QObject owner;
        QVariant received;
        hub.subscribe(&owner, "test:initial:1",
                      [&](const QVariant& v) { received = v; });

        // Initial delivery is queued — pump the event loop.
        QTRY_COMPARE(received.toString(), QString("hello"));
    }

    // Destroying the owner auto-unsubscribes.
    void destroyed_owner_auto_unsubscribes() {
        auto& hub = DataHub::instance();
        int call_count = 0;

        {
            QObject owner;
            hub.subscribe(&owner, "test:destroy:1",
                          [&](const QVariant&) { ++call_count; });
            hub.publish("test:destroy:1", QVariant(1));
            QTRY_COMPARE(call_count, 1);
        } // owner destroyed here

        // Give the queued destroyed() → on_owner_destroyed a chance to run.
        // processEvents once may only deliver the first queued item; run a
        // couple to drain all cascaded queued invocations.
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        hub.publish("test:destroy:1", QVariant(2));
        QCoreApplication::processEvents();

        // call_count must remain 1 — subscriber is gone.
        QCOMPARE(call_count, 1);

        // Subscriber count for this topic should have dropped to 0.
        // After unsubscribe the subscriptions_ bucket is erased, so stats()
        // reports the default zero (via missing bucket).
        const auto stats = hub.stats();
        for (const auto& s : stats)
            if (s.topic == "test:destroy:1")
                QCOMPARE(s.subscriber_count, 0);
    }

    // Wildcard subscription receives every matching topic.
    void wildcard_pattern_matches() {
        auto& hub = DataHub::instance();
        QObject owner;
        QStringList seen;
        hub.subscribe_pattern(&owner, "test:wild:*",
            [&](const QString& t, const QVariant&) { seen.append(t); });

        hub.publish("test:wild:alpha", QVariant(1));
        hub.publish("test:wild:beta",  QVariant(2));
        hub.publish("test:other:gamma", QVariant(3));

        QCoreApplication::processEvents();
        QCOMPARE(seen.size(), 2);
        QVERIFY(seen.contains("test:wild:alpha"));
        QVERIFY(seen.contains("test:wild:beta"));
        QVERIFY(!seen.contains("test:other:gamma"));
    }

    // Scheduler skips push-only topics.
    void push_only_skips_scheduler() {
        auto& hub = DataHub::instance();
        RecordingProducer prod({"test:push:*"});
        hub.register_producer(&prod);

        TopicPolicy p;
        p.push_only = true;
        hub.set_policy_pattern("test:push:*", p);

        QObject owner;
        hub.subscribe(&owner, "test:push:live",
                      [](const QVariant&) {});

        // Drive the scheduler a handful of times — it must not call refresh()
        // on push-only topics.
        for (int i = 0; i < 5; ++i) hub.tick_scheduler();

        QCOMPARE(prod.refresh_count, 0);
        hub.unregister_producer(&prod);
    }

    // request() respects in-flight dedup: two back-to-back requests for
    // the same topic trigger only one refresh().
    void request_dedups_in_flight() {
        auto& hub = DataHub::instance();
        RecordingProducer prod({"test:flight:*"});
        hub.register_producer(&prod);

        // Disable coalesce window for deterministic synchronous testing —
        // request() then dispatches on the next event-loop iteration instead
        // of waiting ~100ms. Restored at end of test.
        const int saved_coalesce = hub.coalesce_window_ms();
        hub.set_coalesce_window_ms(0);

        QObject owner;
        hub.subscribe(&owner, "test:flight:1",
                      [](const QVariant&) {});

        hub.request("test:flight:1");
        hub.request("test:flight:1");  // should be deduped — already in-flight

        // Let the zero-ms coalesce timer fire once.
        QTRY_COMPARE(prod.refresh_count, 1);

        // Once the producer publishes, the in-flight flag clears and the
        // next request() should go through again.
        hub.publish("test:flight:1", QVariant(1));
        QCoreApplication::processEvents();

        hub.request("test:flight:1");
        QTRY_COMPARE(prod.refresh_count, 2);

        hub.unregister_producer(&prod);
        hub.set_coalesce_window_ms(saved_coalesce);
    }

    // Cross-thread publish() must marshal to the hub thread safely.
    void cross_thread_publish_marshals() {
        auto& hub = DataHub::instance();
        QObject owner;
        std::atomic<int> received{0};
        hub.subscribe(&owner, "test:xthread:1",
            [&](const QVariant& v) { received = v.toInt(); });

        // Publish from a worker thread.
        QThread* worker = QThread::create([&] {
            hub.publish("test:xthread:1", QVariant(77));
        });
        worker->start();
        worker->wait();

        QTRY_COMPARE(received.load(), 77);
        worker->deleteLater();
    }

    // peek() reads without subscribing and returns the last published value.
    void peek_reads_without_subscribing() {
        auto& hub = DataHub::instance();
        QCOMPARE(hub.peek("test:peek:unset").isValid(), false);
        hub.publish("test:peek:set", QVariant(QString("seeded")));
        QCoreApplication::processEvents();
        QCOMPARE(hub.peek("test:peek:set").toString(), QString("seeded"));
    }

    // Scheduler respects ttl_ms (does not refresh inside the TTL window).
    void scheduler_respects_ttl() {
        auto& hub = DataHub::instance();
        RecordingProducer prod({"test:ttl:*"});
        hub.register_producer(&prod);

        // Clear any stale pattern policy from prior tests + use sync coalesce.
        hub.clear_policy_pattern("test:ttl:*");
        const int saved_coalesce = hub.coalesce_window_ms();
        hub.set_coalesce_window_ms(0);

        TopicPolicy p;
        p.ttl_ms = 60'000;
        p.min_interval_ms = 0;
        hub.set_policy_pattern("test:ttl:*", p);

        QObject owner;
        hub.subscribe(&owner, "test:ttl:1", [](const QVariant&) {});

        // subscribe() auto cold-starts a forced request — give the coalesce
        // flush a chance to run and drive refresh_count to 1.
        QTRY_COMPARE(prod.refresh_count, 1);

        // Producer publishes; TTL is 60 s so next tick should skip.
        hub.publish("test:ttl:1", QVariant(1));
        QCoreApplication::processEvents();

        hub.tick_scheduler();
        QCOMPARE(prod.refresh_count, 1);  // unchanged

        hub.unregister_producer(&prod);
        hub.set_coalesce_window_ms(saved_coalesce);
    }

    // peek() returns invalid when value is past TTL; peek_raw() returns anyway.
    void peek_respects_ttl() {
        auto& hub = DataHub::instance();

        TopicPolicy p;
        p.ttl_ms = 1;  // effectively immediate expiry
        p.min_interval_ms = 0;
        hub.clear_policy_pattern(QStringLiteral("test:peek:*"));  // idempotent if absent
        hub.set_policy_pattern(QStringLiteral("test:peek:*"), p);

        hub.publish(QStringLiteral("test:peek:ttl:1"), QVariant(QString("stale")));
        QCoreApplication::processEvents();

        QVERIFY(hub.peek_raw(QStringLiteral("test:peek:ttl:1")).isValid());
        QTest::qWait(5);  // exceed ttl
        QVERIFY(!hub.peek(QStringLiteral("test:peek:ttl:1")).isValid());   // stale → invalid
        QVERIFY(hub.peek_raw(QStringLiteral("test:peek:ttl:1")).isValid()); // raw still there
    }
};

QTEST_MAIN(TestDataHub)
#include "test_datahub.moc"
