// test_context_builder — assert byte-equality across agents in baseline mode,
// per-agent variation in situational mode, and stability under successive
// invocations.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 3.

#include "services/alpha_arena/ContextBuilder.h"
#include "services/alpha_arena/AlphaArenaTypes.h"
#include "services/alpha_arena/AlphaArenaSchema.h"

#include <QObject>
#include <QString>
#include <QTest>
#include <QVector>

#include <optional>

using namespace fincept::services::alpha_arena;

namespace {

MarketSnapshot make_market(const QString& coin, double mid) {
    MarketSnapshot m;
    m.coin = coin;
    m.mid = mid;
    m.bid = mid - 0.5;
    m.ask = mid + 0.5;
    m.open_interest = 12345.6789;
    m.funding_rate = 0.0001;
    m.ema20 = mid;
    m.rsi7 = 50.0;
    m.macd = 0.1;
    m.macd_signal = 0.05;
    Bar b{1700000000000LL, mid - 1, mid + 1, mid - 1.5, mid, 100.0};
    m.bars_3m_24h = {b, b};
    m.bars_4h_30d = {b};
    return m;
}

TickContext make_tick_context() {
    TickContext c;
    c.tick.utc_ms = 1700000000000LL;
    c.tick.seq = 7;
    c.markets = {make_market(QStringLiteral("BTC"), 70000.0),
                 make_market(QStringLiteral("ETH"), 3500.0)};
    return c;
}

AgentAccountState make_account(double equity) {
    AgentAccountState s;
    s.equity = equity;
    s.cash = equity * 0.6;
    s.return_pct = 0.0;
    s.sharpe = 0.0;
    s.fees_paid = 0.0;
    return s;
}

} // namespace

class TestContextBuilder : public QObject {
    Q_OBJECT

  private slots:

    void system_prompt_stable_for_mode() {
        QString a = build_system_prompt(CompetitionMode::Baseline);
        QString b = build_system_prompt(CompetitionMode::Baseline);
        QCOMPARE(a, b);
    }

    void system_prompt_differs_per_mode() {
        QString a = build_system_prompt(CompetitionMode::Baseline);
        QString b = build_system_prompt(CompetitionMode::Monk);
        QString c = build_system_prompt(CompetitionMode::Situational);
        QVERIFY(a != b);
        QVERIFY(a != c);
        QVERIFY(b != c);
    }

    void user_prompt_byte_equal_across_agents_baseline() {
        const auto ctx = make_tick_context();
        const auto acct = make_account(10000.0);
        QString p1 = build_user_prompt(ctx, acct, CompetitionMode::Baseline, std::nullopt);
        // Six "agents" all see identical inputs in baseline mode.
        for (int i = 0; i < 6; ++i) {
            QString pn = build_user_prompt(ctx, acct, CompetitionMode::Baseline, std::nullopt);
            QCOMPARE(pn, p1);
        }
    }

    void user_prompt_diff_when_account_diff() {
        const auto ctx = make_tick_context();
        QString a = build_user_prompt(ctx, make_account(10000.0), CompetitionMode::Baseline, std::nullopt);
        QString b = build_user_prompt(ctx, make_account(20000.0), CompetitionMode::Baseline, std::nullopt);
        QVERIFY(a != b);
    }

    void user_prompt_situational_per_agent_diff() {
        const auto ctx = make_tick_context();
        const auto acct = make_account(10000.0);

        SituationalContext sit_a;
        PeerPositionView pv;
        pv.agent_display_name = QStringLiteral("agent-A");
        pv.coin = QStringLiteral("BTC");
        pv.qty = 0.1;
        pv.leverage = 5;
        sit_a.peers = {pv};

        SituationalContext sit_b = sit_a;
        sit_b.peers[0].agent_display_name = QStringLiteral("agent-B");
        sit_b.peers[0].qty = -0.2;

        QString a = build_user_prompt(ctx, acct, CompetitionMode::Situational, sit_a);
        QString b = build_user_prompt(ctx, acct, CompetitionMode::Situational, sit_b);
        QVERIFY(a != b);
    }

    void user_prompt_oldest_to_newest_ordering_is_stable() {
        // Two calls with the same TickContext must produce equal output;
        // ContextBuilder must not be sensitive to call-order side effects.
        const auto ctx = make_tick_context();
        const auto acct = make_account(10000.0);
        QString a = build_user_prompt(ctx, acct, CompetitionMode::Baseline, std::nullopt);
        QString b = build_user_prompt(ctx, acct, CompetitionMode::Baseline, std::nullopt);
        QCOMPARE(a, b);
    }

    void user_prompt_changes_with_tick_seq() {
        TickContext ctx_a = make_tick_context();
        TickContext ctx_b = make_tick_context();
        ctx_b.tick.seq = ctx_a.tick.seq + 1;
        ctx_b.tick.utc_ms = ctx_a.tick.utc_ms + 60000;
        const auto acct = make_account(10000.0);
        QString a = build_user_prompt(ctx_a, acct, CompetitionMode::Baseline, std::nullopt);
        QString b = build_user_prompt(ctx_b, acct, CompetitionMode::Baseline, std::nullopt);
        QVERIFY(a != b);
    }
};

QTEST_MAIN(TestContextBuilder)
#include "test_context_builder.moc"
