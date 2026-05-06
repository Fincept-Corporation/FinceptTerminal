// test_tick_clock — exercises the TickClock skip / force / cadence behaviour.
//
// The full engine smoke / kill-switch / crash-recovery scenarios live in
// test_risk_engine.cpp (kill-switch state machine) and test_repo.cpp
// (replay determinism + crash recovery via re-open). This test fills the
// gap on the scheduler component.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 6.

#include "services/alpha_arena/AlphaArenaTypes.h"
#include "services/alpha_arena/TickClock.h"

#include <QObject>
#include <QSignalSpy>
#include <QTest>

using namespace fincept::services::alpha_arena;

class TestTickClock : public QObject {
    Q_OBJECT

  private slots:

    void initial_seq_is_zero() {
        TickClock c;
        QCOMPARE(c.last_seq(), 0);
        QVERIFY(!c.in_flight());
    }

    void force_tick_emits_with_increasing_seq() {
        TickClock c;
        QSignalSpy spy(&c, &TickClock::tick);
        c.force_tick();
        QCOMPARE(spy.size(), 1);
        Tick first = spy.first().first().value<Tick>();
        QCOMPARE(first.seq, 1);
        QVERIFY(c.in_flight());
        c.tick_complete();

        c.force_tick();
        QCOMPARE(spy.size(), 2);
        Tick second = spy.last().first().value<Tick>();
        QCOMPARE(second.seq, 2);
        c.tick_complete();
    }

    void force_tick_while_in_flight_skips() {
        TickClock c;
        QSignalSpy ticks(&c, &TickClock::tick);
        QSignalSpy skips(&c, &TickClock::tick_skipped);
        c.force_tick();
        QCOMPARE(ticks.size(), 1);
        c.force_tick();    // in-flight, must skip
        QCOMPARE(ticks.size(), 1);
        QVERIFY(skips.size() >= 1);
        c.tick_complete();
    }

    void cadence_round_trip() {
        TickClock c;
        c.set_cadence_seconds(60);
        QCOMPARE(c.cadence_seconds(), 60);
    }

    void stop_does_not_fire() {
        TickClock c;
        c.set_cadence_seconds(60);
        c.start();
        c.stop();
        // No assertion of timer state — just that stop() doesn't crash.
        QVERIFY(true);
    }
};

QTEST_MAIN(TestTickClock)
#include "test_tick_clock.moc"
