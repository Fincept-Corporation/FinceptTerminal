// Unit tests for the Derivatives tab's analytics (pure C++ / Qt Core):
//   src/services/options/VolSurface      — raw-SVI fit, butterfly / calendar checks, smile stats
//   src/services/options/StrategyMath    — payoff, breakevens, max P/L, POP, Greeks, margin, templates
//   src/services/options/ScenarioMath    — shocks, spot × vol grid, stress presets, probability cone
//   src/services/options/RatesMath       — par-curve bootstrap
//   src/services/options/DerivMarketData — parsers of scripts/derivatives/deriv_market.py replies
//       and the chain analysis, on the recorded fixtures in tests/data/derivatives/
//       (provenance in tests/data/derivatives/PROVENANCE.txt).
// References:
//   [GJ14] Gatheral & Jacquier (2014) "Arbitrage-free SVI volatility surfaces", Quantitative
//          Finance 14(1): density factor g(k), eq. (2.1); Vogt's parameters, Example 3.1
//          (recomputed here: g(k) < 0 near k ≈ 0.88).
//   [Hull] Hull, Options, Futures and Other Derivatives, ch. 4 (bootstrap of par yields).
//   Cboe Margin Manual / Reg-T strategy-based rules for the margin cases.

#include "services/options/DerivMarketData.h"
#include "services/options/OptionModels.h"
#include "services/options/RatesMath.h"
#include "services/options/ScenarioMath.h"
#include "services/options/StrategyMath.h"
#include "services/options/VolSurface.h"

#include <QFile>
#include <QtTest>

#include <cmath>

using namespace fincept::services::options;
namespace st = fincept::services::options::strategy;
namespace sc = fincept::services::options::scenario;
namespace mk = fincept::services::options::market;

namespace {

bool near(double a, double b, double tol) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol;
}

double ncdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

st::Leg opt(st::LegKind k, double qty, double K, double T, double px, double iv = 0.2) {
    st::Leg l;
    l.kind = k;
    l.quantity = qty;
    l.strike = K;
    l.expiry_years = T;
    l.entry_price = px;
    l.iv = iv;
    return l;
}

QByteArray fixture(const char* name) {
    QString root = qEnvironmentVariable("FINCEPT_DERIV_TEST_DATA");
    if (root.isEmpty())
        root = QFINDTESTDATA("data/derivatives");
    QFile f(root + QLatin1Char('/') + QLatin1String(name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

} // namespace

class TestDerivAnalytics : public QObject {
    Q_OBJECT

  private slots:
    // ── SVI ──────────────────────────────────────────────────────────────────
    void svi_recovers_exact_parameters() {
        const vol::SviParams p{0.01, 0.1, -0.5, 0.0, 0.15};
        const double T = 0.5;
        std::vector<double> k, iv, w;
        for (int i = 0; i < 40; ++i) {
            const double x = -0.5 + 0.9 * i / 39.0;
            k.push_back(x);
            iv.push_back(vol::svi_vol(p, x, T));
            w.push_back(1.0);
        }
        const vol::SviFit f = vol::fit_svi(k, iv, w, T);
        QVERIFY(f.ok);
        QVERIFY2(f.rmse_vol < 1e-5, qPrintable(QString::number(f.rmse_vol)));
        QVERIFY(near(f.params.a, p.a, 1e-3) && near(f.params.b, p.b, 1e-3) && near(f.params.rho, p.rho, 1e-2) &&
                near(f.params.m, p.m, 1e-2) && near(f.params.sigma, p.sigma, 1e-2));
        // Deterministic ±0.2 vol-point noise: still a tight fit.
        std::vector<double> noisy = iv;
        for (size_t i = 0; i < noisy.size(); ++i)
            noisy[i] += (i % 2 ? 1 : -1) * 0.002;
        const vol::SviFit g = vol::fit_svi(k, noisy, w, T);
        QVERIFY(g.ok && g.rmse_vol < 0.003);
        // Too few points: "no fit", never garbage.
        const vol::SviFit h = vol::fit_svi({0.0, 0.1}, {0.2, 0.21}, {}, T);
        QVERIFY(!h.ok && !h.message.empty());
    }

    void butterfly_and_calendar_checks() {
        // [GJ14] Example 3.1 (Vogt): arbitrageable raw SVI, T = 1.
        const vol::SviParams vogt{-0.0410, 0.1331, 0.3060, 0.3586, 0.4153};
        const vol::ArbitrageCheck bad = vol::butterfly_check(vogt, -1.5, 1.5, 301);
        QVERIFY(!bad.ok && bad.violations > 0 && bad.worst < -0.02);
        QVERIFY(near(bad.worst_k, 0.88, 0.05));
        const vol::SviParams good{0.04, 0.1, -0.3, 0.0, 0.2};
        QVERIFY(vol::butterfly_check(good, -1.5, 1.5, 301).ok);
        // Calendar: the later slice must not have less total variance anywhere.
        vol::SviParams later = good;
        later.a += 0.01;
        QVERIFY(vol::calendar_check(good, later, -1, 1).ok);
        vol::SviParams crossing = good;
        crossing.a -= 0.02;
        crossing.b += 0.05;
        const vol::ArbitrageCheck cal = vol::calendar_check(good, crossing, -1, 1);
        QVERIFY(!cal.ok && cal.violations > 0);
    }

    void smile_statistics() {
        // Flat 20 % smile: ATM 20 %, risk reversal and butterfly zero.
        std::vector<vol::SmileQuote> flat;
        for (int i = -10; i <= 10; ++i) {
            vol::SmileQuote s;
            s.k = 0.03 * i;
            s.iv = 0.2;
            s.is_call = s.k >= 0;
            const double st_ = 0.2 * std::sqrt(0.25);
            const double d1 = (-s.k + 0.5 * st_ * st_) / st_;
            s.fwd_delta = s.is_call ? ncdf(d1) : ncdf(d1) - 1;
            flat.push_back(s);
        }
        const vol::SmileStats a = vol::smile_stats(flat, 100, 0.25);
        QVERIFY(near(a.atm_iv, 0.2, 1e-12) && a.has_25d && near(a.rr25, 0, 1e-12) && near(a.bf25, 0, 1e-12));
        QVERIFY(near(a.move_1sd, 100 * 0.2 * 0.5, 1e-9));
        // Put skew: puts richer → negative risk reversal.
        for (auto& s : flat)
            s.iv = 0.2 - 0.1 * s.k;
        const vol::SmileStats b = vol::smile_stats(flat, 100, 0.25);
        QVERIFY(b.rr25 < 0);
        QVERIFY(near(vol::forward_vol(0.2, 0.25, 0.22, 1.0), std::sqrt((0.0484 - 0.01) / 0.75), 1e-12));
        QCOMPARE(vol::forward_vol(0.4, 0.5, 0.2, 1.0), -1.0); // calendar arbitrage
    }

    // ── Strategy ─────────────────────────────────────────────────────────────
    void straddle_breakevens_and_pop() {
        st::Spec s;
        s.legs = {opt(st::LegKind::Call, 1, 100, 0.25, 5.0), opt(st::LegKind::Put, 1, 100, 0.25, 4.0)};
        const st::Market m{100, 0.0, 0.0};
        QVERIFY(near(st::net_premium(s), 900, 1e-9));
        const auto be = st::breakevens(s, m, 1, 300);
        QCOMPARE(be.size(), size_t(2));
        QVERIFY(near(be[0], 91, 1e-6) && near(be[1], 109, 1e-6)); // strike ± premium
        const st::Extremes ex = st::extremes(s, m);
        QVERIFY(ex.profit_unbounded && !ex.loss_unbounded && near(ex.max_loss, -900, 1e-6));
        const st::Probability p = st::probability_of_profit(s, m, 0.2);
        QVERIFY(p.valid && p.pop > 0 && p.pop < 1);
        // Analytic: P(S_T < 91) + P(S_T > 109) under the lognormal.
        const double sd = 0.2 * 0.5, mu = std::log(100.0) - 0.5 * 0.04 * 0.25;
        const double expect = ncdf((std::log(91.0) - mu) / sd) + 1 - ncdf((std::log(109.0) - mu) / sd);
        QVERIFY2(near(p.pop, expect, 1e-7), qPrintable(QStringLiteral("%1 vs %2").arg(p.pop).arg(expect)));
    }

    void iron_condor_and_verticals() {
        st::Spec ic;
        ic.legs = {opt(st::LegKind::Put, 1, 90, 0.1, 1.0), opt(st::LegKind::Put, -1, 95, 0.1, 2.0),
                   opt(st::LegKind::Call, -1, 105, 0.1, 2.0), opt(st::LegKind::Call, 1, 110, 0.1, 1.0)};
        const st::Market m{100, 0.03, 0.0};
        QVERIFY(near(st::net_premium(ic), -200, 1e-9)); // credit
        const st::Extremes ex = st::extremes(ic, m);
        QVERIFY(!ex.profit_unbounded && !ex.loss_unbounded);
        QVERIFY(near(ex.max_profit, 200, 1e-6) && near(ex.max_loss, -300, 1e-6)); // width − credit
        const st::MarginEstimate mg = st::margin_estimate(ic, m, false);
        QVERIFY(near(mg.requirement, 300, 1e-6));
        // Bull call spread: max profit = width − debit, max loss = debit.
        st::Spec bcs;
        bcs.legs = {opt(st::LegKind::Call, 1, 100, 0.1, 3.0), opt(st::LegKind::Call, -1, 105, 0.1, 1.0)};
        const st::Extremes e2 = st::extremes(bcs, m);
        QVERIFY(near(e2.max_profit, 300, 1e-6) && near(e2.max_loss, -200, 1e-6));
    }

    void covered_call_equals_short_put() {
        // Stock − call(K) ≡ cash − put(K) at expiry: a covered call at 100 with a 105 call sold
        // for 2 has the same P&L as a short 105 put sold for 7.
        st::Spec cc;
        st::Leg stock;
        stock.kind = st::LegKind::Underlying;
        stock.quantity = 100;
        stock.entry_price = 100;
        cc.legs = {stock, opt(st::LegKind::Call, -1, 105, 0.2, 2.0)};
        st::Spec sp;
        sp.legs = {opt(st::LegKind::Put, -1, 105, 0.2, 7.0)};
        const st::Market m{100, 0.02, 0.0};
        for (double S : {50.0, 90.0, 100.0, 105.0, 120.0, 200.0})
            QVERIFY(near(st::pnl_at(cc, m, S, 0.2), st::pnl_at(sp, m, S, 0.2), 1e-9));
        // Margin: covered → only the stock (50 %).
        QVERIFY(near(st::margin_estimate(cc, m, false).requirement, 5000, 1e-6));
    }

    void naked_margin_rules() {
        const st::Market m{100, 0.03, 0.0};
        st::Spec sp;
        sp.legs = {opt(st::LegKind::Put, -1, 95, 0.1, 2.0)};
        // (premium 2 + max(20 % × 100 − 5 OTM, 10 % × 95)) × 100 = 1,700.
        QVERIFY(near(st::margin_estimate(sp, m, false).requirement, 1700, 1e-6));
        // Broad index: 15 % → (2 + max(15 − 5, 9.5)) × 100 = 1,200.
        QVERIFY(near(st::margin_estimate(sp, m, true).requirement, 1200, 1e-6));
        // Short straddle: the larger side + the other's premium: max(2,500 + 400, 2,400 + 500) = 2,900.
        st::Spec ss;
        ss.legs = {opt(st::LegKind::Call, -1, 100, 0.1, 5.0), opt(st::LegKind::Put, -1, 100, 0.1, 4.0)};
        QVERIFY(near(st::margin_estimate(ss, m, false).requirement, 2900, 1e-6));
    }

    void greeks_aggregate() {
        st::Spec s;
        s.legs = {opt(st::LegKind::Call, 2, 100, 0.5, 6.0, 0.25), opt(st::LegKind::Put, -1, 95, 0.5, 3.0, 0.27)};
        const st::Market m{101, 0.04, 0.01};
        const auto c = models::bsm_full(true, 101, 100, 0.5, 0.04, 0.01, 0.25);
        const auto p = models::bsm_full(false, 101, 95, 0.5, 0.04, 0.01, 0.27);
        const st::PositionGreeks g = st::greeks(s, m);
        QVERIFY(near(g.delta, 200 * c.delta - 100 * p.delta, 1e-9));
        QVERIFY(near(g.vega_pt, (200 * c.vega - 100 * p.vega) / 100, 1e-9));
        QVERIFY(near(g.theta_day, (200 * c.theta - 100 * p.theta) / 365, 1e-9));
        QVERIFY(near(g.dollar_delta, g.delta * 101, 1e-9));
    }

    void templates_instantiate() {
        st::ChainAccess ch;
        ch.expiry_count = 2;
        ch.strikes = [](int) {
            std::vector<double> v;
            for (int k = 80; k <= 120; k += 5)
                v.push_back(k);
            return v;
        };
        ch.years = [](int e) { return e == 0 ? 30 / 365.0 : 60 / 365.0; };
        ch.quote = [&ch](int e, double K, bool call, double* mid, double* iv) {
            *iv = 0.2;
            *mid = models::bsm_price(call, 100, K, ch.years(e), 0.0, 0.0, 0.2);
            return true;
        };
        std::string err;
        const auto legs = st::instantiate(*st::find_template("iron_condor"), ch, 0, 100.4, 1, 1, 100, &err);
        QVERIFY2(err.empty(), err.c_str());
        QCOMPARE(legs.size(), size_t(4));
        QVERIFY(near(legs[0].strike, 90, 0) && near(legs[1].strike, 95, 0) && near(legs[2].strike, 105, 0) &&
                near(legs[3].strike, 110, 0));
        const auto cal = st::instantiate(*st::find_template("call_calendar"), ch, 0, 100, 1, 1, 100, &err);
        QCOMPARE(cal.size(), size_t(2));
        QVERIFY(cal[1].expiry_years > cal[0].expiry_years);
        ch.expiry_count = 1; // no later expiry
        const auto none = st::instantiate(*st::find_template("call_calendar"), ch, 0, 100, 1, 1, 100, &err);
        QVERIFY(none.empty() && !err.empty());
        const auto wide = st::instantiate(*st::find_template("iron_condor"), ch, 0, 100, 5, 1, 100, &err);
        QVERIFY(wide.empty() && !err.empty());
        QVERIFY(st::templates().size() >= 20);
    }

    // ── Scenarios ────────────────────────────────────────────────────────────
    void scenarios() {
        st::Spec s;
        s.legs = {opt(st::LegKind::Call, 1, 100, 0.25, 5.0, 0.25), opt(st::LegKind::Put, 1, 100, 0.25, 4.0, 0.25)};
        const st::Market m{100, 0.03, 0.0};
        QCOMPARE(sc::evaluate(s, m, {}).pnl, 0.0);
        const auto spots = sc::symmetric_axis(20, 4), vols = sc::symmetric_axis(10, 2);
        const sc::Grid g = sc::pnl_grid(s, m, spots, vols, 0, 0, nullptr);
        QCOMPARE(int(g.pnl.size()), 9);
        QCOMPARE(g.pnl[4][2], 0.0); // the unshocked centre
        for (const auto& row : g.pnl)
            for (size_t j = 1; j < row.size(); ++j)
                QVERIFY(row[j] > row[j - 1]); // long vega: P&L rises with vol
        std::atomic<bool> cancel{true};
        QVERIFY(sc::pnl_grid(s, m, spots, vols, 0, 0, &cancel).cancelled);
        // Time decay of a long straddle loses money, a crash with a vol spike makes money.
        QVERIFY(sc::evaluate(s, m, {0, 0, 0, 7}).pnl < 0);
        QVERIFY(sc::evaluate(s, m, {-20, 25, 0, 0}).pnl > 0);
        QVERIFY(sc::stress_presets().size() >= 10);
        const sc::Cone c = sc::probability_cone(100, 0.03, 0.0, 0.2, 90, 31);
        QCOMPARE(int(c.days.size()), 31);
        QVERIFY(near(c.p50.front(), 100, 1e-12));
        const double t = 90 / 365.0;
        QVERIFY(near(c.p84.back() / c.p50.back(), std::exp(0.2 * std::sqrt(t)), 1e-12));
        QVERIFY(c.p2.back() < c.p16.back() && c.p16.back() < c.p50.back() && c.p84.back() < c.p98.back());
    }

    // ── Rates ────────────────────────────────────────────────────────────────
    void bootstrap() {
        // A flat 5 % par curve (semi-annual) is a flat 5 % bond-equivalent zero curve.
        const std::vector<double> t = {0.25, 0.5, 1, 2, 5, 10, 30};
        const std::vector<double> y(t.size(), 5.0);
        const rates::ZeroCurve z = rates::bootstrap_par_curve(t, y);
        for (double T : {0.25, 1.0, 3.0, 7.5, 10.0, 30.0})
            QVERIFY2(near(z.rate_at(T), 2 * std::log(1.025), 1e-10), qPrintable(QString::number(z.rate_at(T), 'g', 12)));
        // [Hull] ch. 4: every par bond on the grid reprices to par off the bootstrapped curve.
        const mk::TreasuryCurve tc = mk::parse_treasury_curve(fixture("treasury_curve_sample.json"));
        QVERIFY(tc.ok());
        const rates::ZeroCurve zc = mk::zero_curve(tc);
        const double c10 = tc.yields[tc.tenors.indexOf(10.0)] / 100;
        double pv = 0;
        for (int k = 1; k <= 20; ++k)
            pv += 0.5 * c10 * zc.df(0.5 * k);
        pv += zc.df(10.0);
        QVERIFY2(near(pv, 1.0, 1e-10), qPrintable(QString::number(pv, 'g', 15)));
        QVERIFY(rates::forward_rate(zc, 1, 2) > 0);
    }

    void swaps_fras_caps() {
        const mk::TreasuryCurve tc = mk::parse_treasury_curve(fixture("treasury_curve_sample.json"));
        const rates::ZeroCurve z = mk::zero_curve(tc);
        QVERIFY(!z.empty());
        // A swap at its own par rate is worth nothing; DV01 of payer and receiver are opposite.
        const rates::SwapResult par = rates::swap_on_curve(z, 0.0, 5.0, 0.0, 2, 1e6, true);
        const rates::SwapResult at = rates::swap_on_curve(z, 0.0, 5.0, par.par_rate, 2, 1e6, true);
        QVERIFY2(std::fabs(at.pv) < 1e-6, qPrintable(QString::number(at.pv)));
        QVERIFY(at.dv01 > 0); // the payer of fixed gains when rates rise
        const rates::SwapResult rec = rates::swap_on_curve(z, 0.0, 5.0, par.par_rate, 2, 1e6, false);
        QVERIFY(near(rec.dv01, -at.dv01, 1e-9));
        QCOMPARE(at.payments, 10);
        // FRA struck at its forward is worth nothing.
        double f = 0;
        rates::fra_value(z, 0.5, 1.0, 0.0, 1e6, true, &f);
        QVERIFY(near(rates::fra_value(z, 0.5, 1.0, f, 1e6, true), 0.0, 1e-9));
        // Cap − floor = payer swap at the strike (forward start: every period is still an option).
        const double K = 0.045;
        const rates::CapResult cap = rates::cap_floor(z, 1.0, 4.0, K, 0.25, 4, 1e6, true);
        const rates::CapResult flo = rates::cap_floor(z, 1.0, 4.0, K, 0.25, 4, 1e6, false);
        const rates::SwapResult sw = rates::swap_on_curve(z, 1.0, 4.0, K, 4, 1e6, true);
        QVERIFY2(near(cap.value - flo.value, sw.pv, 1e-6), qPrintable(QStringLiteral("%1 vs %2").arg(cap.value - flo.value).arg(sw.pv)));
        QCOMPARE(int(cap.caplet_values.size()), 16);
        // Spot start: the first (already fixed) period is not an option.
        QCOMPARE(int(rates::cap_floor(z, 0.0, 2.0, K, 0.25, 4, 1e6, true).caplet_values.size()), 7);
        // Bachelier caps: positive and finite, also at a zero strike.
        QVERIFY(rates::cap_floor(z, 0.0, 2.0, 0.0, 0.01, 4, 1e6, true, true).value > 0);
    }

    // ── Fixtures: parsers and the chain analysis ─────────────────────────────
    void parse_cboe_chain_fixture() {
        const QByteArray raw = fixture("spy_chain_cboe_sample.json");
        QVERIFY2(!raw.isEmpty(), "tests/data/derivatives/spy_chain_cboe_sample.json not found");
        mk::FreeChain c = mk::parse_free_chain(raw);
        QVERIFY(c.ok());
        QCOMPARE(c.source, QStringLiteral("cboe"));
        QCOMPARE(c.symbol, QStringLiteral("SPY"));
        QCOMPARE(c.expiries.size(), 3);
        QCOMPARE(c.quotes.size(), 602);
        QVERIFY(near(c.spot, 740.29, 1e-9));
        // 16:00 New York in November (EST, UTC−5) = 21:00 UTC.
        QCOMPARE(c.quote_time, QDateTime(QDate(2026, 11, 16), QTime(21, 0), QTimeZone::UTC));
        for (int e = 0; e < c.expiries.size(); ++e) {
            QVERIFY(c.count(e) > 0);
            for (int i = c.expiries[e].begin + 1; i < c.expiries[e].end; ++i)
                QVERIFY(c.quotes[i - 1].strike <= c.quotes[i].strike);
        }
        const mk::TreasuryCurve tc = mk::parse_treasury_curve(fixture("treasury_curve_sample.json"));
        mk::analyse_chain(c, mk::zero_curve(tc), 0.04);
        QVERIFY(c.analysed);
        int with_iv = 0, quoted = 0;
        for (const auto& q : c.quotes) {
            if (q.quoted()) {
                ++quoted;
                with_iv += std::isfinite(q.iv) ? 1 : 0;
            }
        }
        QVERIFY2(with_iv > 0.8 * quoted, qPrintable(QStringLiteral("%1 of %2").arg(with_iv).arg(quoted)));
        for (const mk::FreeExpiry& x : c.expiries) {
            QVERIFY(x.forward_from_parity);
            QVERIFY(std::fabs(x.forward / c.spot - 1) < 0.02);
            QVERIFY(near(x.years, x.days / 365.0, 1.5 / 365.0));
            QVERIFY(x.rate > 0.03 && x.rate < 0.06);
            QVERIFY2(x.smile.atm_iv > 0.05 && x.smile.atm_iv < 0.6, qPrintable(QString::number(x.smile.atm_iv)));
            QVERIFY(x.smile_points >= 10);
            QVERIFY2(x.svi.ok && x.svi.rmse_vol < 0.02, qPrintable(QString::fromStdString(x.svi.message)));
            QVERIFY(x.call_oi > 0 && x.put_oi > 0 && std::isfinite(x.max_pain));
            QVERIFY(std::isfinite(x.straddle) && x.straddle > 0);
        }
        // Local IV close to the feed's own IV near the money (both from the same quotes).
        for (const auto& q : c.quotes) {
            const auto& x = c.expiries[q.expiry];
            if (std::isfinite(q.iv) && std::isfinite(q.feed_iv) && std::fabs(q.strike / x.forward - 1) < 0.02)
                QVERIFY2(std::fabs(q.iv - q.feed_iv) < 0.03, qPrintable(QStringLiteral("K=%1 %2 vs %3").arg(q.strike).arg(q.iv).arg(q.feed_iv)));
        }
        QCOMPARE(c.surface_iv.size(), mk::surface_rows().size());
        QCOMPARE(c.surface_iv.first().size(), 3);
        double mid = 0, iv = 0;
        QVERIFY(mk::find_quote(c, 1, c.expiries[1].atm_strike, true, &mid, &iv) && mid > 0 && iv > 0);
        QVERIFY(mk::strikes(c, 0).size() > 10);
    }

    void parse_yahoo_and_failures() {
        mk::FreeChain y = mk::parse_free_chain(fixture("aapl_chain_yahoo_sample.json"));
        QVERIFY(y.ok());
        QCOMPARE(y.source, QStringLiteral("yahoo"));
        QCOMPARE(y.partial_loaded, 1);
        QVERIFY(y.partial_total > 1);
        QCOMPARE(y.quote_time.timeSpec(), Qt::UTC);
        mk::analyse_chain(y, {}, 0.04);
        QVERIFY(y.analysed);
        const mk::FreeChain none = mk::parse_free_chain(fixture("chain_no_options.json"));
        QCOMPARE(none.status, QStringLiteral("no_options"));
        QVERIFY(!none.message.isEmpty());
        QCOMPARE(mk::parse_free_chain("{not json").status, QStringLiteral("parse_error"));
        QCOMPARE(mk::parse_treasury_curve("[]").status, QStringLiteral("parse_error"));
        const mk::TreasuryCurve tc = mk::parse_treasury_curve(fixture("treasury_curve_sample.json"));
        QCOMPARE(tc.date, QDate(2026, 11, 16));
        QCOMPARE(tc.tenors.size(), 14);
        QVERIFY(near(mk::zero_curve(tc).rate_at(1.0), rates::bey_to_continuous(4.62), 1e-12));
    }

    // ── D6: swaptions, key-rate DV01, bonds off the curve, cost of carry ─────
    // Reference values recomputed independently in Python (scipy) before being typed
    // here: D:\ft-scratch\deriv2\rates\refs.py.
    void swaption_hull_and_parity() {
        // [Hull] "Swaptions" worked example (OFOD 8th ed. Ex. 28.5): curve flat 6 % (cont.),
        // 5-year option into a 3-year payer swap at 6.2 % semi-annual, 20 % vol, 100m:
        // about $2.07 million (Python: 2,070,981.70; F = 6.0909 %, A = 2.003558).
        rates::ZeroCurve flat;
        flat.t = {0.25, 30.0};
        flat.r = {0.06, 0.06};
        const rates::SwaptionResult p = rates::swaption_on_curve(flat, 5.0, 3.0, 0.062, 0.20, 2, 100e6, true, false);
        QVERIFY(p.ok);
        QCOMPARE(p.payments, 6);
        QVERIFY2(near(p.annuity, 2.003558, 1e-6), qPrintable(QString::number(p.annuity, 'g', 10)));
        QVERIFY(near(p.forward_rate, 2.0 * (std::exp(0.03) - 1.0), 1e-12));
        QVERIFY2(near(p.premium, 2070981.70, 1.0), qPrintable(QString::number(p.premium, 'f', 2)));
        const rates::SwaptionResult r = rates::swaption_on_curve(flat, 5.0, 3.0, 0.062, 0.20, 2, 100e6, false, false);
        QVERIFY2(near(r.premium, 2289556.24, 1.0), qPrintable(QString::number(r.premium, 'f', 2)));
        // Payer − receiver = the forward-starting payer swap at the strike.
        const double fwd_swap = rates::swap_pv(flat, 5.0, 3.0, 0.062, 2, 100e6, true);
        QVERIFY2(near(p.premium - r.premium, fwd_swap, 1e-3), qPrintable(QStringLiteral("%1 vs %2").arg(p.premium - r.premium).arg(fwd_swap)));
        // Rates up: the payer gains, the receiver loses; both have positive vega.
        QVERIFY(p.dv01 > 0 && r.dv01 < 0);
        QVERIFY(p.vega > 0 && r.vega > 0);
        QVERIFY(p.delta_bp > 0 && r.delta_bp < 0);
        // Bachelier ATM at σn = F·σB agrees with Black to O(σ²T/24): Python 0.208 %.
        const double F = p.forward_rate;
        const rates::SwaptionResult bk = rates::swaption_on_curve(flat, 5.0, 3.0, F, 0.10, 2, 1.0, true, false);
        const rates::SwaptionResult bc = rates::swaption_on_curve(flat, 5.0, 3.0, F, 0.10 * F, 2, 1.0, true, true);
        QVERIFY(bk.ok && bc.ok);
        QVERIFY2(near(bc.premium / bk.premium - 1.0, 0.00208, 2e-4), qPrintable(QString::number(bc.premium / bk.premium - 1.0)));
        // Black has no price for a negative strike; Bachelier does.
        const rates::SwaptionResult neg = rates::swaption_on_curve(flat, 1.0, 2.0, -0.001, 0.2, 2, 1e6, false, false);
        QVERIFY(!neg.ok && !neg.message.empty());
        QVERIFY(rates::swaption_on_curve(flat, 1.0, 2.0, -0.001, 0.008, 2, 1e6, false, true).premium > 0);
    }

    void key_rate_buckets() {
        const mk::TreasuryCurve tc = mk::parse_treasury_curve(fixture("treasury_curve_sample.json"));
        QVERIFY(tc.ok());
        const std::vector<double> ten(tc.tenors.begin(), tc.tenors.end());
        const std::vector<double> par(tc.yields.begin(), tc.yields.end());
        const rates::ZeroCurve z = rates::bootstrap_par_curve(ten, par);
        // A 5-year payer swap at its par rate.
        const double k = rates::swap_on_curve(z, 0.0, 5.0, 0.0, 2, 1e6, true).par_rate;
        const rates::KeyRateResult kr = rates::key_rate_dv01(
            ten, par, [k](const rates::ZeroCurve& c) { return rates::swap_pv(c, 0.0, 5.0, k, 2, 1e6, true); });
        QCOMPARE(int(kr.dv01.size()), int(ten.size()));
        QVERIFY(!kr.cancelled);
        QVERIFY(std::fabs(kr.base_pv) < 1e-6);
        QVERIFY(kr.parallel > 0); // the payer of fixed gains when rates rise
        QVERIFY2(near(kr.total, kr.parallel, 0.01 * std::fabs(kr.parallel)),
                 qPrintable(QStringLiteral("%1 vs %2").arg(kr.total).arg(kr.parallel)));
        // Par-curve DV01 is close to the zero-curve parallel DV01 of swap_on_curve.
        const double zero_dv01 = rates::swap_on_curve(z, 0.0, 5.0, k, 2, 1e6, true).dv01;
        QVERIFY2(near(kr.parallel, zero_dv01, 0.1 * std::fabs(zero_dv01)),
                 qPrintable(QStringLiteral("%1 vs %2").arg(kr.parallel).arg(zero_dv01)));
        // Nothing beyond the 7-year point moves a 5-year swap; the 5-year bucket dominates.
        double beyond = 0, at5 = 0;
        for (size_t i = 0; i < kr.tenors.size(); ++i) {
            if (kr.tenors[i] > 7.0 + 1e-9)
                beyond += std::fabs(kr.dv01[i]);
            if (std::fabs(kr.tenors[i] - 5.0) < 1e-9)
                at5 = kr.dv01[i];
        }
        QVERIFY2(beyond < 1e-6 * std::fabs(kr.parallel) + 1e-9, qPrintable(QString::number(beyond)));
        QVERIFY(at5 > 0.8 * kr.parallel);
        // A 10-year bond loses when rates rise; buckets add up to the parallel move.
        const rates::BondSpec b{4.5, 2, 10.0, 100.0};
        const rates::KeyRateResult kb = rates::key_rate_dv01(
            ten, par, [b](const rates::ZeroCurve& c) { return rates::bond_on_curve(c, b).dirty * 1e4; });
        QVERIFY(kb.parallel < 0);
        QVERIFY(near(kb.total, kb.parallel, 0.01 * std::fabs(kb.parallel)));
        // Cancelled before the first bump.
        std::atomic<bool> stop{true};
        QVERIFY(rates::key_rate_dv01(ten, par, [b](const rates::ZeroCurve& c) { return rates::bond_on_curve(c, b).dirty; },
                                     &stop)
                    .cancelled);
    }

    void bonds_z_spread_asw() {
        rates::ZeroCurve flat;
        flat.t = {0.25, 30.0};
        flat.r = {0.05, 0.05};
        const rates::BondSpec b{6.0, 2, 5.0, 100.0};
        const rates::BondCurveResult at0 = rates::bond_on_curve(flat, b);
        QVERIFY(at0.ok);
        QCOMPARE(at0.payments, 10);
        QVERIFY2(near(at0.dirty, 104.093568, 1e-6), qPrintable(QString::number(at0.dirty, 'f', 8))); // Python
        QVERIFY(near(at0.accrued, 0.0, 1e-12));
        QVERIFY(at0.duration > 4.0 && at0.duration < 5.0 && at0.convexity > 0);
        const rates::BondCurveResult at50 = rates::bond_on_curve(flat, b, 0.005);
        QVERIFY(near(at50.dirty, 101.826719, 1e-6));
        // The Z-spread of the curve's own price is zero; a price at Z = 50 bp solves back to 50 bp.
        const rates::SpreadSolve z0 = rates::z_spread(flat, b, at0.dirty);
        QVERIFY(z0.ok && std::fabs(z0.spread) < 1e-10);
        const rates::SpreadSolve z50 = rates::z_spread(flat, b, at50.dirty);
        QVERIFY2(z50.ok && near(z50.spread, 0.005, 1e-10), qPrintable(QString::number(z50.spread, 'g', 12)));
        // Par-par ASW: positive for a bond cheap to the curve (Python 51.89 bp), negative when rich.
        QVERIFY2(near(rates::asset_swap_spread(flat, b, at50.dirty), 0.0051886, 2e-6),
                 qPrintable(QString::number(rates::asset_swap_spread(flat, b, at50.dirty), 'g', 10)));
        QVERIFY(rates::asset_swap_spread(flat, b, at0.dirty + 1.0) < 0);
        // Accrued since the previous (rolled) coupon date: 4.8 years left → 0.2 years accrued.
        const rates::BondCurveResult odd = rates::bond_on_curve(flat, rates::BondSpec{6.0, 2, 4.8, 100.0});
        QVERIFY(near(odd.accrued, 1.2, 1e-9) && near(odd.clean, odd.dirty - 1.2, 1e-12));
        // No spread reprices a negative price; the failure is a sentence, not NaN.
        const rates::SpreadSolve bad = rates::z_spread(flat, b, -5.0);
        QVERIFY(!bad.ok && !bad.message.empty());
    }

    void futures_carry_hull() {
        // [Hull] ch. 5: 10-month forward on a stock at 50, r = 8 %, dividends 0.75 at 3, 6 and
        // 9 months: PV(I) = 2.162, F = 51.14 (Python 2.16206448, 51.13584001).
        rates::CarryInputs s;
        s.spot = 50;
        s.years = 10.0 / 12.0;
        s.rate = 0.08;
        s.income = {{0.25, 0.75}, {0.5, 0.75}, {0.75, 0.75}};
        rates::CarryResult r = rates::futures_carry(s);
        QVERIFY(r.ok && !r.has_market);
        QVERIFY(near(r.pv_income, 2.1620644845, 1e-9));
        QVERIFY(near(r.fair, 51.1358400107, 1e-9));
        // Implied repo of a market price of 51.50 (Python: 8.8515 % continuous, 9.1862 % simple).
        s.market_future = 51.50;
        r = rates::futures_carry(s);
        QVERIFY(r.has_market);
        QVERIFY(near(r.basis, 51.50 - 51.1358400107, 1e-9));
        QVERIFY(near(r.implied_repo, 0.0885154235, 1e-9));
        QVERIFY(near(r.implied_repo_simple, 0.0918617690, 1e-9));
        // At the fair price the implied repo is the financing rate itself.
        s.market_future = r.fair;
        QVERIFY(near(rates::futures_carry(s).implied_repo, 0.08, 1e-12));
        // [Hull] index, 6 months, dividend yield 3 %, r = 8 %: 307.59; the implied yield is 3 %.
        rates::CarryInputs ix;
        ix.spot = 300;
        ix.years = 0.5;
        ix.rate = 0.08;
        ix.income_yield = 0.03;
        const rates::CarryResult ri = rates::futures_carry(ix);
        QVERIFY(near(ri.fair, 307.5945361573, 1e-9));
        ix.market_future = ri.fair;
        QVERIFY(near(rates::futures_carry(ix).implied_yield, 0.03, 1e-12));
        // [Hull] gold: storage 2 per ounce paid at year end (negative income), S = 450, r = 7 %: 484.63.
        rates::CarryInputs g;
        g.spot = 450;
        g.years = 1.0;
        g.rate = 0.07;
        g.income = {{1.0, -2.0}};
        QVERIFY(near(rates::futures_carry(g).fair, 484.6286815644, 1e-9));
        // Plain failures.
        rates::CarryInputs bad;
        bad.spot = 0;
        QVERIFY(!rates::futures_carry(bad).ok && !rates::futures_carry(bad).message.empty());
    }
};

QTEST_GUILESS_MAIN(TestDerivAnalytics)
#include "tst_deriv_analytics.moc"
