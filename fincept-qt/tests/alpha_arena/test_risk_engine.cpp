// test_risk_engine — 30+ unit cases covering accept/reject/amend per rule
// plus the per-agent kill-switch state machine.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 5a.

#include "services/alpha_arena/RiskEngine.h"
#include "services/alpha_arena/AlphaArenaSchema.h"
#include "services/alpha_arena/AlphaArenaTypes.h"

#include <QObject>
#include <QTest>

using namespace fincept::services::alpha_arena;

namespace {

ProposedAction make_buy(double qty = 0.01, int lev = 5,
                       double mark = 70000.0, double pt = 75000.0, double sl = 68000.0) {
    ProposedAction a;
    a.signal = Signal::BuyToEnter;
    a.coin = QStringLiteral("BTC");
    a.quantity = qty;
    a.leverage = lev;
    a.profit_target = pt;
    a.stop_loss = sl;
    a.invalidation_condition = QStringLiteral("breakdown");
    a.confidence = 0.7;
    a.risk_usd = std::abs(mark - sl) * qty;
    a.justification = QStringLiteral("trend continuation");
    return a;
}

ProposedAction make_sell(double qty = 0.01, int lev = 5,
                        double mark = 70000.0, double pt = 65000.0, double sl = 72000.0) {
    ProposedAction a;
    a.signal = Signal::SellToEnter;
    a.coin = QStringLiteral("BTC");
    a.quantity = qty;
    a.leverage = lev;
    a.profit_target = pt;
    a.stop_loss = sl;
    a.invalidation_condition = QStringLiteral("breakup");
    a.confidence = 0.7;
    a.risk_usd = std::abs(mark - sl) * qty;
    a.justification = QStringLiteral("rejection at resistance");
    return a;
}

AgentRiskState make_agent_state(double equity = 100000.0, double mark = 70000.0) {
    AgentRiskState s;
    s.equity = equity;
    s.existing_qty_in_coin = 0.0;
    s.last_entry_utc_ms_on_coin = 0;
    s.now_utc_ms = 1700000000000LL;
    s.maintenance_margin_frac = 0.05;
    s.mark_price = mark;
    return s;
}

} // namespace

class TestRiskEngine : public QObject {
    Q_OBJECT

  private slots:

    // ── accept / amend basics ───────────────────────────────────────────────

    void hold_always_accepted() {
        ProposedAction h;
        h.signal = Signal::Hold;
        auto v = evaluate_action(h, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    void close_with_position_accepted() {
        ProposedAction c;
        c.signal = Signal::Close;
        auto s = make_agent_state();
        s.existing_qty_in_coin = 0.5;
        auto v = evaluate_action(c, s, CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    void close_with_no_position_rejected() {
        ProposedAction c;
        c.signal = Signal::Close;
        auto v = evaluate_action(c, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void clean_buy_accepted() {
        auto v = evaluate_action(make_buy(), make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    void clean_sell_accepted() {
        auto v = evaluate_action(make_sell(), make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    // ── one-position-per-coin ────────────────────────────────────────────────

    void duplicate_long_rejected() {
        auto s = make_agent_state();
        s.existing_qty_in_coin = 0.05;
        auto v = evaluate_action(make_buy(), s, CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void duplicate_short_rejected() {
        auto s = make_agent_state();
        s.existing_qty_in_coin = -0.05;
        auto v = evaluate_action(make_sell(), s, CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    // ── leverage cap ─────────────────────────────────────────────────────────

    void leverage_above_max_rejected() {
        auto a = make_buy(0.01, 25);
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void leverage_zero_rejected() {
        auto a = make_buy(0.01, 0);
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void leverage_one_accepted() {
        auto a = make_buy(0.001, 1);
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    // ── stop side checks ────────────────────────────────────────────────────

    void long_with_stop_above_mark_rejected() {
        auto a = make_buy(0.01, 5, 70000.0, 75000.0, 71000.0); // sl above mark
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void long_with_pt_below_mark_rejected() {
        auto a = make_buy(0.01, 5, 70000.0, 69000.0, 68000.0); // pt below mark
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void short_with_stop_below_mark_rejected() {
        auto a = make_sell(0.01, 5, 70000.0, 65000.0, 69000.0); // sl below mark
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void short_with_pt_above_mark_rejected() {
        auto a = make_sell(0.01, 5, 70000.0, 71000.0, 72000.0); // pt above mark
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    // ── liquidation buffer (mm = 0.05, lev=20 ⇒ buffer ≈ 4.75% ⇒ reject) ────

    void liq_buffer_below_15_pct_rejected() {
        auto a = make_buy(0.001, 20);
        a.stop_loss = 69000.0; // valid
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void liq_buffer_at_low_lev_accepted() {
        auto a = make_buy(0.001, 5);  // buffer ≈ 19% ⇒ accept
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    // ── 2% risk cap ──────────────────────────────────────────────────────────

    void risk_above_2pct_rejected() {
        // equity 100k, 2% cap = 2000. Set sl distance and qty so risk = 5000.
        auto a = make_buy(1.0, 5, 70000.0, 75000.0, 65000.0);
        a.risk_usd = 5000.0; // |70000-65000| * 1.0 = 5000
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void risk_just_under_2pct_accepted() {
        auto a = make_buy(0.001, 5, 70000.0, 75000.0, 68000.0); // risk ≈ 2 USD
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    // ── risk_usd drift → amend ───────────────────────────────────────────────

    void risk_usd_drift_amends() {
        auto a = make_buy(0.001, 5, 70000.0, 75000.0, 68000.0);
        a.risk_usd = a.risk_usd * 2.0; // 100% drift
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Amend);
        QVERIFY(v.amended.has_value());
        QVERIFY(std::fabs(v.amended->risk_usd - 2.0) < 1.0);
    }

    void risk_usd_within_tolerance_accepted() {
        auto a = make_buy(0.001, 5, 70000.0, 75000.0, 68000.0);
        a.risk_usd = a.risk_usd * 1.03; // 3% drift, within 5%
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    // ── Monk mode ────────────────────────────────────────────────────────────

    void monk_cooldown_blocks_re_entry() {
        auto s = make_agent_state();
        s.last_entry_utc_ms_on_coin = s.now_utc_ms - (5LL * 60LL * 1000LL); // 5 min ago
        auto v = evaluate_action(make_buy(), s, CompetitionMode::Monk);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void monk_cooldown_expired_allows_entry() {
        auto s = make_agent_state();
        s.last_entry_utc_ms_on_coin = s.now_utc_ms - (40LL * 60LL * 1000LL); // 40 min ago
        auto v = evaluate_action(make_buy(), s, CompetitionMode::Monk);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    void monk_notional_above_25_pct_rejected() {
        // equity 100k, 25% = 25k notional cap. qty=1 BTC at 70k = 70k notional.
        auto a = make_buy(1.0, 5);
        a.risk_usd = 2.0; // keep risk cap untouched
        a.stop_loss = 70000.0 - 1.99; // ensure risk_usd ~= 2.0
        a.profit_target = 71000.0;
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Monk);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void monk_notional_under_25_pct_accepted() {
        // 25% of 100k = 25k. qty=0.001 BTC at 70k = 70 USD notional.
        auto v = evaluate_action(make_buy(0.001, 5), make_agent_state(), CompetitionMode::Monk);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Accept);
    }

    // ── boundary / sanity rejects ────────────────────────────────────────────

    void zero_quantity_rejected() {
        auto a = make_buy(0.0);
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void zero_pt_rejected() {
        auto a = make_buy();
        a.profit_target = 0.0;
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void zero_sl_rejected() {
        auto a = make_buy();
        a.stop_loss = 0.0;
        auto v = evaluate_action(a, make_agent_state(), CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void zero_mark_rejected() {
        auto s = make_agent_state();
        s.mark_price = 0.0;
        auto v = evaluate_action(make_buy(), s, CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    void zero_equity_rejected() {
        auto s = make_agent_state();
        s.equity = 0.0;
        auto v = evaluate_action(make_buy(), s, CompetitionMode::Baseline);
        QCOMPARE(v.outcome, RiskVerdict::Outcome::Reject);
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    void recompute_risk_usd_correct() {
        ProposedAction a = make_buy(2.0, 5, 100.0, 110.0, 90.0);
        QCOMPARE(recompute_risk_usd(a, 100.0), 20.0);
    }

    void liquidation_price_long() {
        // mm=0.05, lev=10 ⇒ liq = entry × (1 − 0.1 × 0.95) = 0.905 × entry
        QCOMPARE(estimated_liquidation_price(100.0, 10, true, 0.05), 90.5);
    }

    void liquidation_price_short() {
        QCOMPARE(estimated_liquidation_price(100.0, 10, false, 0.05), 109.5);
    }

    // ── circuit-breaker state machine ────────────────────────────────────────

    void circuit_starts_closed() {
        CircuitState s = advance_circuit(CircuitState{}, false, false, 0.0);
        QVERIFY(!s.open);
    }

    void circuit_opens_on_three_parse_failures() {
        CircuitState s{};
        s = advance_circuit(s, true, false, 0.0);
        s = advance_circuit(s, true, false, 0.0);
        QVERIFY(!s.open);
        s = advance_circuit(s, true, false, 0.0);
        QVERIFY(s.open);
    }

    void circuit_opens_on_three_risk_rejects() {
        CircuitState s{};
        s = advance_circuit(s, false, true, 0.0);
        s = advance_circuit(s, false, true, 0.0);
        QVERIFY(!s.open);
        s = advance_circuit(s, false, true, 0.0);
        QVERIFY(s.open);
    }

    void circuit_opens_on_50_pct_drawdown() {
        CircuitState s{};
        s = advance_circuit(s, false, false, 0.55);
        QVERIFY(s.open);
    }

    void circuit_resets_consecutive_on_success() {
        CircuitState s{};
        s = advance_circuit(s, true, false, 0.0);
        s = advance_circuit(s, true, false, 0.0);
        QCOMPARE(s.consecutive_parse_failures, 2);
        s = advance_circuit(s, false, false, 0.0);
        QCOMPARE(s.consecutive_parse_failures, 0);
    }

    void circuit_remains_open_once_open() {
        CircuitState s{};
        s.open = true;
        s = advance_circuit(s, false, false, 0.0);
        QVERIFY(s.open);
    }
};

QTEST_MAIN(TestRiskEngine)
#include "test_risk_engine.moc"
