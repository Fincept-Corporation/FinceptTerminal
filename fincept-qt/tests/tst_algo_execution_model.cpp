// Unit tests for src/algo_engine/ExecutionModel.{h,cpp}
//
// ExecutionModel.cpp includes only its own header (Qt Core: QString, QJsonObject,
// QVector, QCoreApplication::translate). One extra translation unit.
//
// What matters here is that the backtester and the paper simulator charge the
// SAME, correct costs: adverse slippage, commission floors / caps, side-specific
// taxes, lot rounding that never buys a fraction of a lot, and sizing that never
// spends more cash than it has once fees are included.

#include "algo_engine/ExecutionModel.h"

#include <QJsonObject>
#include <QTest>

#include <cmath>

using fincept::algo::CostModel;
using fincept::algo::FillSide;
namespace ex = fincept::algo::execution;

class TstAlgoExecutionModel : public QObject {
    Q_OBJECT

  private slots:
    void slippage_is_adverse() {
        CostModel m;
        m.slippage_bps = 10; // 0.10 %
        QCOMPARE(ex::slipped_price(m, FillSide::Buy, 100.0, 0.0), 100.1);
        QCOMPARE(ex::slipped_price(m, FillSide::Sell, 100.0, 0.0), 99.9);
        m.slippage_atr_frac = 0.5;
        QCOMPARE(ex::slipped_price(m, FillSide::Buy, 100.0, 2.0), 101.1);
        QCOMPARE(ex::slipped_price(m, FillSide::Sell, 100.0, 2.0), 98.9);
        // A NaN ATR (not yet defined) contributes nothing.
        QCOMPARE(ex::slipped_price(m, FillSide::Buy, 100.0, std::nan("")), 100.1);
        // Never negative.
        m.slippage_bps = 20000;
        QVERIFY(ex::slipped_price(m, FillSide::Sell, 1.0, 0.0) > 0.0);
    }

    void commission_floor_and_cap() {
        CostModel m;
        m.commission_per_unit = 0.005;
        m.commission_min = 1.0;
        // 10 shares × 0.005 = 0.05 → floored at 1.00
        QCOMPARE(ex::fees(m, FillSide::Buy, 10, 50.0).commission, 1.0);
        // 1000 shares × 0.005 = 5.00
        QCOMPARE(ex::fees(m, FillSide::Buy, 1000, 50.0).commission, 5.0);

        CostModel capped;
        capped.commission_pct = 0.03;
        capped.commission_max = 20.0;
        // 0.03 % of 10,000 = 3.00 (under the cap)
        QCOMPARE(ex::fees(capped, FillSide::Buy, 100, 100.0).commission, 3.0);
        // 0.03 % of 1,000,000 = 300 → capped at 20
        QCOMPARE(ex::fees(capped, FillSide::Buy, 10000, 100.0).commission, 20.0);
    }

    void taxes_follow_the_side() {
        CostModel m;
        m.buy_tax_pct = 0.5; // UK-style stamp duty on purchases only
        QCOMPARE(ex::fees(m, FillSide::Buy, 100, 10.0).taxes, 5.0);
        QCOMPARE(ex::fees(m, FillSide::Sell, 100, 10.0).taxes, 0.0);
        QCOMPARE(ex::fees(m, FillSide::Sell, 100, 10.0).total(), 0.0);
    }

    void zero_model_is_free() {
        CostModel m;
        QVERIFY(m.is_zero());
        const auto f = ex::fill(m, FillSide::Buy, 123.45, 10, 1.0);
        QCOMPARE(f.price, 123.45);
        QCOMPARE(f.fees.total(), 0.0);
        QCOMPARE(f.slippage_cost, 0.0);
    }

    void lot_rounding() {
        CostModel m;
        m.lot_size = 100; // TSE trading unit
        QCOMPARE(ex::round_quantity(m, 950), 900.0);
        QCOMPARE(ex::round_quantity(m, 99), 0.0);
        // Floating error does not lose a lot.
        CostModel frac;
        frac.lot_size = 0.1;
        QCOMPARE(ex::round_quantity(frac, 0.30000000000000004), 0.3);
        QCOMPARE(ex::round_quantity(frac, 0.29999999999), 0.3);
        CostModel crypto;
        crypto.lot_size = 0.0001;
        QCOMPARE(ex::round_quantity(crypto, 0.123456), 0.1234);
    }

    void sizing_never_overspends() {
        CostModel m;
        m.commission_pct = 0.1;
        m.buy_tax_pct = 0.1;
        m.commission_min = 5;
        const double cash = 10000.0;
        const double px = 99.0;
        const double qty = ex::max_affordable_qty(m, FillSide::Buy, cash, px);
        QVERIFY(qty > 0);
        QCOMPARE(qty, std::floor(qty)); // whole shares with lot 1
        const double cost = qty * px + ex::fees(m, FillSide::Buy, qty, px).total();
        QVERIFY(cost <= cash);
        // One more share would not fit.
        const double more = (qty + 1) * px + ex::fees(m, FillSide::Buy, qty + 1, px).total();
        QVERIFY(more > cash);
    }

    void unaffordable_lot_gives_zero() {
        CostModel m;
        m.lot_size = 100;
        // 100 shares at 150 = 15,000 > 10,000 → no fill (the backtester reports it).
        QCOMPARE(ex::max_affordable_qty(m, FillSide::Buy, 10000, 150.0), 0.0);
        // Fractional lots can buy part of an expensive coin.
        CostModel crypto = ex::preset(QStringLiteral("crypto_spot")).model;
        const double q = ex::max_affordable_qty(crypto, FillSide::Buy, 1000, 60000.0);
        QVERIFY(q > 0 && q < 1);
        QVERIFY(q * 60000.0 + ex::fees(crypto, FillSide::Buy, q, 60000.0).total() <= 1000.0);
    }

    void fill_breakdown() {
        CostModel m;
        m.commission_pct = 0.1;
        m.sell_tax_pct = 0.1;
        m.slippage_bps = 10;
        const auto f = ex::fill(m, FillSide::Sell, 200.0, 50, 0.0);
        QCOMPARE(f.price, 199.8);
        QCOMPARE(f.notional, 199.8 * 50);
        QVERIFY(std::abs(f.slippage_cost - 10.0) < 1e-9);
        QVERIFY(std::abs(f.fees.commission - 9.99) < 1e-9);
        QVERIFY(std::abs(f.fees.taxes - 9.99) < 1e-9);
        // A limit fill (take-profit) pays no slippage.
        const auto lim = ex::fill(m, FillSide::Sell, 200.0, 50, 0.0, false);
        QCOMPARE(lim.price, 200.0);
        QCOMPARE(lim.slippage_cost, 0.0);
    }

    void borrow_cost_for_shorts() {
        CostModel m;
        m.borrow_pct_annual = 3.65;
        QVERIFY(std::abs(ex::borrow_cost(m, 10000, 10) - 10.0) < 1e-9);
        QCOMPARE(ex::borrow_cost(m, 10000, 0), 0.0);
    }

    void json_round_trip() {
        CostModel m = ex::preset(QStringLiteral("in_equity_intraday")).model;
        m.lot_size = 25;
        const CostModel back = CostModel::from_json(m.to_json());
        QVERIFY(back == m);
        QCOMPARE(back.preset, m.preset);
        // Negative / garbage values are clamped, an empty object is the zero model.
        QJsonObject bad;
        bad["commission_pct"] = -3;
        bad["lot_size"] = 0;
        const CostModel clean = CostModel::from_json(bad);
        QCOMPARE(clean.commission_pct, 0.0);
        QCOMPARE(clean.lot_size, 1.0);
        QVERIFY(CostModel::from_json(QJsonObject()).is_zero());
    }

    void presets_are_labelled_and_mapped() {
        const auto all = ex::presets();
        QVERIFY(all.size() >= 5);
        QCOMPARE(all.first().id, QStringLiteral("none"));
        QVERIFY(all.first().model.is_zero());
        for (const auto& p : all) {
            QVERIFY(!p.label.isEmpty());
            QVERIFY(!p.note.isEmpty());
            QCOMPARE(p.model.preset, p.id);
        }
        QCOMPARE(ex::default_preset_for(QStringLiteral("NSE"), true), QStringLiteral("in_equity_intraday"));
        QCOMPARE(ex::default_preset_for(QStringLiteral("IN"), false), QStringLiteral("in_equity_delivery"));
        QCOMPARE(ex::default_preset_for(QStringLiteral("US"), true), QStringLiteral("us_equity_zero"));
        QCOMPARE(ex::default_preset_for(QStringLiteral("DE"), false), QStringLiteral("eu_equity"));
        QCOMPARE(ex::default_preset_for(QStringLiteral("ZZ"), false), QStringLiteral("none"));
        QCOMPARE(ex::preset(QStringLiteral("does-not-exist")).id, QStringLiteral("none"));
    }
};

QTEST_GUILESS_MAIN(TstAlgoExecutionModel)
#include "tst_algo_execution_model.moc"
