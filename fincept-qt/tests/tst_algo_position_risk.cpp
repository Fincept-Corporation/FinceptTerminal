// Unit tests for the risk bookkeeping in src/algo_engine/PositionManager.{h,cpp}
//
// PositionManager.cpp includes only its own header, which includes
// algo_engine/AlgoEngineTypes.h (Qt Core + plain structs). One extra TU.
//
// Covers the (more conservative) risk rules the live / paper runner relies on:
//   * fees are carried with the position and deducted from the trade's P&L,
//   * the daily-loss limit counts the open position's UNREALIZED loss,
//   * the max-drawdown limit measures realized + unrealized P&L from its peak and
//     stays latched across a day rollover until a manual clear,
//   * the daily figures roll over on the EXCHANGE trading date and survive a
//     restart only within the same trading day,
// plus the behaviour the F&O selftest pins (latch on the realizing exit).

#include "algo_engine/AlgoEngineTypes.h"
#include "algo_engine/PositionManager.h"

#include <QTest>

#include <cmath>

using fincept::algo::PositionManager;
using fincept::algo::PositionSide;
using fincept::algo::RiskState;

class TstAlgoPositionRisk : public QObject {
    Q_OBJECT

  private slots:
    void fees_are_deducted_from_trade_pnl() {
        PositionManager pm("fees", 0, 0, 0, 0, 0);
        pm.record_entry(PositionSide::Long, 100, 50.0, 0, 5.0);
        pm.update_price(50.0);
        // Flat price: the open position is the entry fee under water.
        QCOMPARE(pm.metrics().unrealized_pnl, -5.0);
        const double net = pm.record_exit(100, 51.0, 1, 4.0);
        QCOMPARE(net, 100.0 - 5.0 - 4.0);
        QCOMPARE(pm.last_exit_gross(), 100.0);
        QCOMPARE(pm.metrics().total_pnl, 91.0);
        QCOMPARE(pm.metrics().total_fees, 9.0);
        QCOMPARE(pm.metrics().winning_trades, 1);
    }

    void short_side_pnl() {
        PositionManager pm("short", 0, 0, 0, 0, 0);
        pm.record_entry(PositionSide::Short, 10, 100.0, 0);
        pm.update_price(90.0);
        QCOMPARE(pm.metrics().unrealized_pnl, 100.0);
        QCOMPARE(pm.record_exit(10, 95.0, 1), 50.0);
    }

    void daily_loss_counts_unrealized() {
        // Limit 1000; no realized loss yet, but the open position is down 1200.
        PositionManager pm("daily", 0, 0, 0, 0, 1000.0);
        pm.record_entry(PositionSide::Long, 100, 50.0, 0);
        pm.update_price(38.0);
        const auto sig = pm.check_risk(38.0);
        QVERIFY(sig.has_value());
        QCOMPARE(sig->reason, QStringLiteral("daily_loss_limit"));
        QCOMPARE(sig->side, QStringLiteral("SELL"));
        QCOMPARE(sig->quantity, 100.0);
        QVERIFY(pm.is_paused());
        QVERIFY(pm.metrics().risk_halted);
    }

    void daily_loss_latches_on_the_realizing_exit() {
        PositionManager pm("latch", 0.0, 0.0, 0.0, 0.0, 1000.0);
        pm.record_entry(PositionSide::Long, 100, 50.0, 0);
        pm.record_exit(100, 45.0, 1); // -500
        QVERIFY(!pm.is_paused());
        pm.record_entry(PositionSide::Long, 100, 50.0, 2);
        pm.record_exit(100, 39.0, 3); // -1100 → -1600 cumulative
        QVERIFY(pm.is_paused());
        pm.reset_daily();
        QVERIFY(!pm.is_paused());
    }

    void drawdown_limit_includes_unrealized_and_survives_rollover() {
        PositionManager pm("dd", 0, 0, 0, 0, 0);
        pm.set_max_drawdown_limit(300.0);
        pm.reset_daily_if_new_day(20261116);
        pm.record_entry(PositionSide::Long, 100, 10.0, 0);
        pm.update_price(14.0); // +400 peak
        QVERIFY(!pm.check_risk(14.0).has_value());
        pm.update_price(11.0); // +100 → 300 below the peak
        const auto sig = pm.check_risk(11.0);
        QVERIFY(sig.has_value());
        QCOMPARE(sig->reason, QStringLiteral("max_drawdown_limit"));
        QVERIFY(pm.is_paused());
        QCOMPARE(pm.metrics().max_drawdown, 300.0);
        pm.record_exit(100, 11.0, 1);
        // Not a daily limit: a new trading day does not clear it.
        QVERIFY(pm.reset_daily_if_new_day(20261117));
        QVERIFY(pm.is_paused());
        // A manual clear does, and re-bases the peak (no immediate re-trip).
        pm.clear_halts();
        QVERIFY(!pm.is_paused());
        pm.record_entry(PositionSide::Long, 100, 11.0, 2);
        pm.update_price(10.5);
        QVERIFY(!pm.check_risk(10.5).has_value());
    }

    void exchange_day_rollover() {
        PositionManager pm("roll", 0, 0, 0, 0, 1000.0);
        QVERIFY(!pm.reset_daily_if_new_day(20261116)); // first stamp
        pm.record_entry(PositionSide::Long, 100, 50.0, 0);
        pm.record_exit(100, 38.0, 1); // -1200 → latched
        QVERIFY(pm.is_paused());
        QCOMPARE(pm.metrics().trades_today, 1);
        QVERIFY(!pm.reset_daily_if_new_day(20261116)); // same exchange day
        QVERIFY(pm.is_paused());
        QVERIFY(pm.reset_daily_if_new_day(20261117)); // next exchange day
        QVERIFY(!pm.is_paused());
        QCOMPARE(pm.metrics().daily_pnl, 0.0);
        QCOMPARE(pm.metrics().trades_today, 0);
        QVERIFY(!pm.reset_daily_if_new_day(0)); // unknown day: no-op
    }

    void restore_only_within_the_same_trading_day() {
        RiskState rs;
        rs.day_key = 20261116;
        rs.daily_pnl = -1500;
        rs.paused_by_loss_limit = true;
        rs.trades_today = 3;
        rs.peak_equity = 800;
        rs.halt_reason = QStringLiteral("daily_loss_limit");

        PositionManager same("same", 0, 0, 0, 0, 1000.0);
        same.restore_risk_state(rs, 20261116, 12.5);
        QVERIFY(same.is_paused());
        QCOMPARE(same.metrics().daily_pnl, -1500.0);
        QCOMPARE(same.metrics().trades_today, 3);
        QCOMPARE(same.metrics().total_fees, 12.5);

        PositionManager next("next", 0, 0, 0, 0, 1000.0);
        next.restore_risk_state(rs, 20261117, 0);
        QVERIFY(!next.is_paused());
        QCOMPARE(next.metrics().daily_pnl, 0.0);

        // A drawdown halt is restored whatever the day.
        RiskState dd;
        dd.day_key = 20261110;
        dd.halted_by_drawdown = true;
        dd.halt_reason = QStringLiteral("max_drawdown_limit");
        PositionManager halted("dd", 0, 0, 0, 0, 0);
        halted.restore_risk_state(dd, 20261117, 0);
        QVERIFY(halted.is_paused());
        QCOMPARE(halted.metrics().halt_reason, QStringLiteral("max_drawdown_limit"));
    }

    void no_limits_no_signals() {
        PositionManager pm("none", 0, 0, 0, 0, 0);
        pm.record_entry(PositionSide::Long, 1, 100.0, 0);
        pm.update_price(1.0);
        QVERIFY(!pm.check_risk(1.0).has_value());
        QVERIFY(!pm.is_paused());
    }
};

QTEST_GUILESS_MAIN(TstAlgoPositionRisk)
#include "tst_algo_position_risk.moc"
