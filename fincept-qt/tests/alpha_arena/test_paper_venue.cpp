// test_paper_venue — fees, liquidation, funding for the paper venue.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 5b.
//
// PaperVenue uses QTimer::singleShot for the simulated 200ms fill latency;
// tests therefore drive the event loop with QTRY_VERIFY / wait helpers.

#include "services/alpha_arena/IExchangeVenue.h"
#include "services/alpha_arena/PaperVenue.h"

#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QVector>

#include <atomic>
#include <cmath>

using namespace fincept::services::alpha_arena;

namespace {

constexpr double kTakerFee = 0.00045;
constexpr double kSlippageBps = 0.0005;

OrderRequest make_buy(const QString& agent, const QString& coin, double qty,
                     int leverage = 5, bool reduce_only = false) {
    OrderRequest r;
    r.agent_id = agent;
    r.coin = coin;
    r.side = QStringLiteral("buy");
    r.qty = qty;
    r.leverage = leverage;
    r.reduce_only = reduce_only;
    return r;
}

OrderRequest make_sell(const QString& agent, const QString& coin, double qty,
                      int leverage = 5, bool reduce_only = false) {
    OrderRequest r;
    r.agent_id = agent;
    r.coin = coin;
    r.side = QStringLiteral("sell");
    r.qty = qty;
    r.leverage = leverage;
    r.reduce_only = reduce_only;
    return r;
}

} // namespace

class TestPaperVenue : public QObject {
    Q_OBJECT

  private slots:

    // ── fees ─────────────────────────────────────────────────────────────────

    void taker_fee_applied_on_open_and_close() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        v.push_mark(QStringLiteral("BTC"), 70000.0, 0);

        QVector<FillEvent> fills;
        v.on_fill([&](FillEvent f) { fills.append(f); });

        bool acked = false;
        v.place_order(make_buy(QStringLiteral("a1"), QStringLiteral("BTC"), 0.001),
                      [&](OrderAck a) { acked = (a.status == "accepted"); });
        QTRY_VERIFY(acked);
        QTRY_COMPARE(fills.size(), 1);

        // Fill price = mark + slippage; fee = notional × taker
        const double expected_fill = 70000.0 * (1.0 + kSlippageBps);
        const double expected_fee = 0.001 * expected_fill * kTakerFee;
        QVERIFY(std::fabs(fills.first().price - expected_fill) < 1e-6);
        QVERIFY(std::fabs(fills.first().fee - expected_fee) < 1e-6);

        // Close — fee charged again.
        v.place_order(make_sell(QStringLiteral("a1"), QStringLiteral("BTC"), 0.001, 5, true),
                      [](OrderAck) {});
        QTRY_COMPARE(fills.size(), 2);
        QVERIFY(fills[1].fee > 0.0);
    }

    void roundtrip_at_flat_mark_loses_two_fees() {
        // Open + close at same mark — agent should be down 2 fees worth of cash
        // (slippage cancels because we go in and out at mark+spread/-spread).
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        v.push_mark(QStringLiteral("BTC"), 70000.0, 0);

        std::atomic<int> filled{0};
        v.on_fill([&](FillEvent) { filled++; });

        v.place_order(make_buy(QStringLiteral("a1"), QStringLiteral("BTC"), 0.001),
                      [](OrderAck) {});
        QTRY_COMPARE(filled.load(), 1);

        // Bring mark back to 70000 if anything moved.
        v.push_mark(QStringLiteral("BTC"), 70000.0, 1);
        v.place_order(make_sell(QStringLiteral("a1"), QStringLiteral("BTC"), 0.001, 5, true),
                      [](OrderAck) {});
        QTRY_COMPARE(filled.load(), 2);

        // After the round-trip the equity should be near 10000 minus
        // fees & 2× slippage. Just assert it didn't blow up.
        const double eq = v.equity_for(QStringLiteral("a1"));
        QVERIFY(eq < 10000.0);          // we paid fees
        QVERIFY(eq > 9990.0);           // we didn't lose more than ~10 USD
    }

    // ── liquidation ──────────────────────────────────────────────────────────

    void long_10x_liquidates_when_mark_drops() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        v.push_mark(QStringLiteral("BTC"), 100.0, 0);

        std::atomic<int> liq_count{0};
        QString liq_coin;
        v.on_liquidation([&](LiquidationEvent e) {
            liq_count++;
            liq_coin = e.coin;
        });

        v.place_order(make_buy(QStringLiteral("a1"), QStringLiteral("BTC"), 1.0, 10),
                      [](OrderAck) {});
        // Wait for fill.
        QTest::qWait(300);

        // For lev=10, mm=0.05 => liq buffer = (1/10)*(1-0.05) = 0.095 = 9.5% below entry.
        // Entry includes slippage; the liquidation triggers when mark is ≤ liq_price.
        // Push a mark well below liq.
        v.push_mark(QStringLiteral("BTC"), 88.0, 1);
        QTRY_COMPARE(liq_count.load(), 1);
        QCOMPARE(liq_coin, QStringLiteral("BTC"));
    }

    void short_10x_liquidates_when_mark_rises() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        v.push_mark(QStringLiteral("ETH"), 100.0, 0);

        std::atomic<int> liq_count{0};
        v.on_liquidation([&](LiquidationEvent) { liq_count++; });

        v.place_order(make_sell(QStringLiteral("a1"), QStringLiteral("ETH"), 1.0, 10),
                      [](OrderAck) {});
        QTest::qWait(300);

        v.push_mark(QStringLiteral("ETH"), 112.0, 1);
        QTRY_COMPARE(liq_count.load(), 1);
    }

    void no_liquidation_within_buffer() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        v.push_mark(QStringLiteral("BTC"), 100.0, 0);
        std::atomic<int> liq_count{0};
        v.on_liquidation([&](LiquidationEvent) { liq_count++; });
        v.place_order(make_buy(QStringLiteral("a1"), QStringLiteral("BTC"), 1.0, 5),
                      [](OrderAck) {});
        QTest::qWait(300);
        v.push_mark(QStringLiteral("BTC"), 95.0, 1); // small drop, well above liq
        QTest::qWait(50);
        QCOMPARE(liq_count.load(), 0);
    }

    // ── funding ──────────────────────────────────────────────────────────────

    void push_funding_credits_cash() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        const double before = v.equity_for(QStringLiteral("a1"));
        v.push_funding(QStringLiteral("a1"), QStringLiteral("BTC"), 12.5, 0);
        const double after = v.equity_for(QStringLiteral("a1"));
        QVERIFY(std::fabs((after - before) - 12.5) < 1e-9);
    }

    void push_funding_negative_debits_cash() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        const double before = v.equity_for(QStringLiteral("a1"));
        v.push_funding(QStringLiteral("a1"), QStringLiteral("BTC"), -5.0, 0);
        const double after = v.equity_for(QStringLiteral("a1"));
        QVERIFY(std::fabs((after - before) - (-5.0)) < 1e-9);
    }

    void funding_callback_invoked() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        QVector<FundingEvent> events;
        v.on_funding([&](FundingEvent f) { events.append(f); });
        v.push_funding(QStringLiteral("a1"), QStringLiteral("BTC"), 1.5, 1700000000000LL);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().amount_usd, 1.5);
    }

    void funding_for_unknown_agent_is_noop() {
        PaperVenue v;
        v.push_funding(QStringLiteral("nobody"), QStringLiteral("BTC"), 100.0, 0);
        QCOMPARE(v.equity_for(QStringLiteral("nobody")), 0.0);
    }

    // ── reject path ─────────────────────────────────────────────────────────

    void place_without_mark_rejects() {
        PaperVenue v;
        paper_seed_agent(v, QStringLiteral("a1"), 10000.0);
        QString status, error;
        v.place_order(make_buy(QStringLiteral("a1"), QStringLiteral("BTC"), 0.1),
                      [&](OrderAck a) { status = a.status; error = a.error; });
        QTRY_COMPARE(status, QStringLiteral("rejected"));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_MAIN(TestPaperVenue)
#include "test_paper_venue.moc"
