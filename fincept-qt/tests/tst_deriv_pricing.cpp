// Unit tests for the Derivatives tab's pricing maths (pure C++, no Qt in the units):
//   src/services/options/OptionModels.{h,cpp}  — BSM Greeks to third order, Black-76,
//       Bachelier, CRR binomial (discrete dividends), Barone-Adesi-Whaley, IV solvers
//   src/services/options/ExoticPricing.{h,cpp} — digitals, barriers, Asians, Kirk, Margrabe
//
// Published reference values are cited next to each vector. Every cited value was also
// recomputed independently (Python / SciPy) before it was written down here, so a
// failure means the C++ is wrong, not the book.
//   [Haug]  E.G. Haug (2007), The Complete Guide to Option Pricing Formulas, 2nd ed.
//   [Hull]  J.C. Hull, Options, Futures and Other Derivatives (8th-10th ed.)
// Identities (put-call parity, in + out = vanilla, model limits) and central finite
// differences check everything that has no published number.

#include "services/options/ExoticPricing.h"
#include "services/options/OptionModels.h"

#include <QtTest>

#include <cmath>
#include <random>
#include <vector>

using namespace fincept::services::options;
namespace md = fincept::services::options::models;
namespace ex = fincept::services::options::exotic;

namespace {

bool close_abs(double a, double b, double tol) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tol;
}

bool close_rel(double a, double b, double rel, double abs_floor = 1e-9) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= std::max(rel * std::fabs(b), abs_floor);
}

struct P {
    bool call;
    double S, K, T, r, q, v;
};

const std::vector<P>& grid_cases() {
    static const std::vector<P> k = {
        {true, 100, 100, 1.0, 0.05, 0.02, 0.25},  {false, 100, 100, 1.0, 0.05, 0.02, 0.25},
        {true, 90, 110, 0.5, 0.03, 0.00, 0.40},   {false, 90, 110, 0.5, 0.03, 0.00, 0.40},
        {true, 120, 100, 2.0, 0.01, 0.03, 0.15},  {false, 120, 100, 2.0, 0.01, 0.03, 0.15},
        {true, 50, 48, 0.1, 0.07, 0.01, 0.60},    {false, 50, 52, 0.08, -0.005, 0.0, 0.30},
    };
    return k;
}

} // namespace

class TestDerivPricing : public QObject {
    Q_OBJECT

  private slots:
    // ── Published closed-form values ─────────────────────────────────────────
    void bsm_published_values() {
        // [Hull] Example (ch. 15 BSM): S=42, K=40, r=10%, σ=20%, T=0.5 → call 4.76, put 0.81.
        QVERIFY(close_abs(md::bsm_price(true, 42, 40, 0.5, 0.10, 0.0, 0.20), 4.76, 0.005));
        QVERIFY(close_abs(md::bsm_price(false, 42, 40, 0.5, 0.10, 0.0, 0.20), 0.81, 0.005));
        // [Haug] §1.1.1 generalised BSM: S=60, K=65, T=0.25, r=8%, b=8%, σ=30% → call 2.1334.
        QVERIFY(close_abs(md::bsm_price(true, 60, 65, 0.25, 0.08, 0.0, 0.30), 2.1334, 5e-5));
        // [Haug] §1.1.3 Merton (1973), continuous dividend: S=100, K=95, T=0.5, r=10%, q=5%, σ=20% → put 2.4648.
        QVERIFY(close_abs(md::bsm_price(false, 100, 95, 0.5, 0.10, 0.05, 0.20), 2.4648, 5e-5));
        // [Haug] §1.1.6 Garman-Kohlhagen: S=1.56, K=1.60, T=0.5, r=6%, rf=8%, σ=12% → call 0.0291.
        QVERIFY(close_abs(md::bsm_price(true, 1.56, 1.60, 0.5, 0.06, 0.08, 0.12), 0.0291, 5e-5));
        // [Haug] §1.1.4 Black (1976): F=19, K=19, T=0.75, r=10%, σ=28% → put 1.7011 (= call at the money).
        QVERIFY(close_abs(md::black76_price(false, 19, 19, 0.75, 0.10, 0.28), 1.7011, 5e-5));
        QVERIFY(close_abs(md::black76_price(true, 19, 19, 0.75, 0.10, 0.28), 1.7011, 5e-5));
    }

    void greeks_published_values() {
        // [Haug] §2.1 delta, futures option (b=0): S=105, K=100, T=0.5, r=10%, σ=36% → call 0.5946, put −0.3566.
        QVERIFY(close_abs(md::black76_full(true, 105, 100, 0.5, 0.10, 0.36).delta, 0.5946, 5e-5));
        QVERIFY(close_abs(md::black76_full(false, 105, 100, 0.5, 0.10, 0.36).delta, -0.3566, 5e-5));
        // [Haug] §2.2 gamma: S=55, K=60, T=0.75, r=b=10%, σ=30% → 0.0278.
        QVERIFY(close_abs(md::bsm_full(true, 55, 60, 0.75, 0.10, 0.0, 0.30).gamma, 0.0278, 5e-5));
        // [Haug] §2.3 DdeltaDvol (vanna): S=90, K=80, T=0.25, r=b=5%, σ=20% → −1.0008.
        QVERIFY(close_abs(md::bsm_full(true, 90, 80, 0.25, 0.05, 0.0, 0.20).vanna, -1.0008, 5e-5));
        // [Haug] §2.4 theta (per year): put S=430, K=405, T=0.0833, r=7%, b=2% (q=5%), σ=20% → −31.1924.
        QVERIFY(close_abs(md::bsm_full(false, 430, 405, 0.0833, 0.07, 0.05, 0.20).theta, -31.1924, 5e-4));
        // [Haug] §2.5 rho: call S=72, K=75, T=1, r=b=9%, σ=19% → 38.7325.
        QVERIFY(close_abs(md::bsm_full(true, 72, 75, 1.0, 0.09, 0.0, 0.19).rho, 38.7325, 5e-4));
    }

    // ── Every Greek against central finite differences of the price ──────────
    void greeks_match_finite_differences() {
        for (const P& c : grid_cases()) {
            const md::Greeks g = md::bsm_full(c.call, c.S, c.K, c.T, c.r, c.q, c.v);
            QVERIFY(g.valid);
            auto V = [&](double S, double K, double T, double r, double q, double v) {
                return md::bsm_price(c.call, S, K, T, r, q, v);
            };
            auto G = [&](double S, double T, double v) { return md::bsm_full(c.call, S, c.K, T, c.r, c.q, v); };
            const double hS = c.S * 1e-4, hv = 1e-5, hT = 1e-6, hr = 1e-6, hK = c.K * 1e-4;
            const double d_S = (V(c.S + hS, c.K, c.T, c.r, c.q, c.v) - V(c.S - hS, c.K, c.T, c.r, c.q, c.v)) / (2 * hS);
            QVERIFY2(close_rel(g.delta, d_S, 1e-6, 1e-8), "delta");
            const double gm = (V(c.S + hS, c.K, c.T, c.r, c.q, c.v) - 2 * g.price + V(c.S - hS, c.K, c.T, c.r, c.q, c.v)) /
                              (hS * hS);
            QVERIFY2(close_rel(g.gamma, gm, 1e-4, 1e-7), "gamma");
            QVERIFY2(close_rel(g.vega, (V(c.S, c.K, c.T, c.r, c.q, c.v + hv) - V(c.S, c.K, c.T, c.r, c.q, c.v - hv)) / (2 * hv),
                               1e-6, 1e-7),
                     "vega");
            QVERIFY2(close_rel(g.theta, -(V(c.S, c.K, c.T + hT, c.r, c.q, c.v) - V(c.S, c.K, c.T - hT, c.r, c.q, c.v)) / (2 * hT),
                               1e-5, 1e-6),
                     "theta");
            QVERIFY2(close_rel(g.rho, (V(c.S, c.K, c.T, c.r + hr, c.q, c.v) - V(c.S, c.K, c.T, c.r - hr, c.q, c.v)) / (2 * hr),
                               1e-6, 1e-6),
                     "rho");
            QVERIFY2(close_rel(g.phi, (V(c.S, c.K, c.T, c.r, c.q + hr, c.v) - V(c.S, c.K, c.T, c.r, c.q - hr, c.v)) / (2 * hr),
                               1e-6, 1e-6),
                     "phi");
            QVERIFY2(close_rel(g.dual_delta, (V(c.S, c.K + hK, c.T, c.r, c.q, c.v) - V(c.S, c.K - hK, c.T, c.r, c.q, c.v)) / (2 * hK),
                               1e-6, 1e-8),
                     "dual delta");
            QVERIFY2(close_rel(g.dual_gamma,
                               (V(c.S, c.K + hK, c.T, c.r, c.q, c.v) - 2 * g.price + V(c.S, c.K - hK, c.T, c.r, c.q, c.v)) / (hK * hK),
                               1e-4, 1e-7),
                     "dual gamma");
            // Cross and higher-order Greeks from the analytical lower-order ones.
            QVERIFY2(close_rel(g.vanna, (G(c.S, c.T, c.v + hv).delta - G(c.S, c.T, c.v - hv).delta) / (2 * hv), 1e-5, 1e-7), "vanna");
            QVERIFY2(close_rel(g.vanna, (G(c.S + hS, c.T, c.v).vega - G(c.S - hS, c.T, c.v).vega) / (2 * hS), 1e-5, 1e-7), "vanna (vega/S)");
            QVERIFY2(close_rel(g.volga, (G(c.S, c.T, c.v + hv).vega - G(c.S, c.T, c.v - hv).vega) / (2 * hv), 1e-5, 1e-6), "volga");
            QVERIFY2(close_rel(g.charm, -(G(c.S, c.T + hT, c.v).delta - G(c.S, c.T - hT, c.v).delta) / (2 * hT), 1e-4, 1e-6), "charm");
            QVERIFY2(close_rel(g.veta, -(G(c.S, c.T + hT, c.v).vega - G(c.S, c.T - hT, c.v).vega) / (2 * hT), 1e-4, 1e-5), "veta");
            QVERIFY2(close_rel(g.speed, (G(c.S + hS, c.T, c.v).gamma - G(c.S - hS, c.T, c.v).gamma) / (2 * hS), 1e-4, 1e-9), "speed");
            QVERIFY2(close_rel(g.colour, -(G(c.S, c.T + hT, c.v).gamma - G(c.S, c.T - hT, c.v).gamma) / (2 * hT), 1e-4, 1e-7), "colour");
            QVERIFY2(close_rel(g.zomma, (G(c.S, c.T, c.v + hv).gamma - G(c.S, c.T, c.v - hv).gamma) / (2 * hv), 1e-4, 1e-7), "zomma");
            QVERIFY2(close_rel(g.ultima, (G(c.S, c.T, c.v + hv).volga - G(c.S, c.T, c.v - hv).volga) / (2 * hv), 1e-4, 1e-4), "ultima");
            if (g.price > 1e-8)
                QVERIFY(close_rel(g.lambda, g.delta * c.S / g.price, 1e-12));
        }
    }

    // ── Identities and limits ────────────────────────────────────────────────
    void put_call_parity_everywhere() {
        const double Ss[] = {1e-3, 50, 100, 200, 1e5};
        const double Ts[] = {1e-9, 1.0 / 365, 0.25, 1.0, 10.0};
        const double vols[] = {0.0, 1e-8, 0.05, 0.3, 2.0};
        for (double S : Ss)
            for (double T : Ts)
                for (double v : vols) {
                    const double K = 100, r = 0.04, q = 0.01;
                    const double c = md::bsm_price(true, S, K, T, r, q, v);
                    const double p = md::bsm_price(false, S, K, T, r, q, v);
                    QVERIFY(std::isfinite(c) && std::isfinite(p) && c >= 0 && p >= 0);
                    const double fwd = S * std::exp(-q * T) - K * std::exp(-r * T);
                    QVERIFY2(close_abs(c - p, fwd, 1e-9 * std::max({S, K, 1.0})), "C - P = S e^-qT - K e^-rT");
                }
    }

    void limits_are_finite_and_correct() {
        // T → 0: intrinsic value, delta 0/±1, no NaN anywhere.
        const md::Greeks a = md::bsm_full(true, 110, 100, 0.0, 0.05, 0.0, 0.3);
        QVERIFY(a.valid && close_abs(a.price, 10, 1e-12) && close_abs(a.delta, 1, 1e-12) && a.gamma == 0);
        const md::Greeks b = md::bsm_full(false, 110, 100, 1e-14, 0.05, 0.0, 0.3);
        QVERIFY(b.valid && close_abs(b.price, 0, 1e-9) && std::isfinite(b.gamma) && std::isfinite(b.vanna));
        // σ → 0: discounted forward intrinsic.
        const md::Greeks c = md::bsm_full(true, 100, 90, 1.0, 0.05, 0.02, 0.0);
        QVERIFY(c.valid && close_abs(c.price, 100 * std::exp(-0.02) - 90 * std::exp(-0.05), 1e-12));
        // Deep in / out of the money: finite, bounded by the no-arbitrage limits.
        for (double S : {1e-6, 1e-2, 1e6}) {
            const md::Greeks g = md::bsm_full(true, S, 100, 1.0, 0.05, 0.0, 0.2);
            QVERIFY(g.valid && std::isfinite(g.price) && std::isfinite(g.gamma) && std::isfinite(g.speed) &&
                    std::isfinite(g.ultima) && std::isfinite(g.lambda));
            QVERIFY(g.price <= S + 1e-9 && g.delta >= 0 && g.delta <= 1);
        }
        // Invalid inputs are flagged, not NaN.
        QVERIFY(!md::bsm_full(true, -1, 100, 1, 0.05, 0, 0.2).valid);
        QVERIFY(!md::bsm_full(true, 100, 100, 1, 0.05, 0, std::nan("")).valid);
    }

    void black76_is_bsm_on_the_forward() {
        for (double K : {80.0, 100.0, 125.0}) {
            const md::Greeks b = md::black76_full(true, 100, K, 0.7, 0.03, 0.22);
            const md::Greeks s = md::bsm_full(true, 100, K, 0.7, 0.03, 0.03, 0.22);
            QVERIFY(close_rel(b.price, s.price, 1e-14) && close_rel(b.delta, s.delta, 1e-14) &&
                    close_rel(b.gamma, s.gamma, 1e-14));
            QVERIFY(close_rel(b.rho, -0.7 * b.price, 1e-14)); // the forward is held fixed
        }
    }

    void bachelier_identities() {
        // Put-call parity C − P = e^{−rT}(F − K), also for negative rates / forwards.
        for (double F : {-0.005, 0.01, 100.0})
            for (double K : {-0.01, 0.0025, 100.0}) {
                const double c = md::bachelier_price(true, F, K, 2.0, 0.03, 0.01 * std::max(1.0, std::fabs(F)));
                const double p = md::bachelier_price(false, F, K, 2.0, 0.03, 0.01 * std::max(1.0, std::fabs(F)));
                QVERIFY(close_abs(c - p, std::exp(-0.06) * (F - K), 1e-12 * std::max(1.0, std::fabs(F))));
            }
        // At the money: e^{−rT}·σ√T/√(2π) exactly.
        QVERIFY(close_rel(md::bachelier_price(true, 100, 100, 0.5, 0.02, 12.0),
                          std::exp(-0.01) * 12.0 * std::sqrt(0.5) / std::sqrt(2 * 3.14159265358979323846), 1e-13));
        // Small vol: Bachelier with σ_N = F·σ_B agrees with Black (relative gap ≈ σ²T/24).
        const double black = md::black76_price(true, 100, 100, 1.0, 0.0, 0.001);
        const double bach = md::bachelier_price(true, 100, 100, 1.0, 0.0, 0.1);
        QVERIFY(close_rel(bach, black, 1e-6));
        // Greeks vs finite differences.
        const md::Greeks g = md::bachelier_full(false, 1.5, 1.7, 0.8, 0.02, 0.4);
        const double h = 1e-5;
        QVERIFY(close_rel(g.delta, (md::bachelier_price(false, 1.5 + h, 1.7, 0.8, 0.02, 0.4) -
                                    md::bachelier_price(false, 1.5 - h, 1.7, 0.8, 0.02, 0.4)) / (2 * h), 1e-6));
        QVERIFY(close_rel(g.vega, (md::bachelier_price(false, 1.5, 1.7, 0.8, 0.02, 0.4 + h) -
                                   md::bachelier_price(false, 1.5, 1.7, 0.8, 0.02, 0.4 - h)) / (2 * h), 1e-6));
        QVERIFY(close_rel(g.theta, -(md::bachelier_price(false, 1.5, 1.7, 0.8 + h, 0.02, 0.4) -
                                     md::bachelier_price(false, 1.5, 1.7, 0.8 - h, 0.02, 0.4)) / (2 * h), 1e-5));
    }

    // ── Implied volatility: round trips at the wings, "no solution", never NaN ──
    void implied_vol_round_trips() {
        const double Ts[] = {1.0 / 365, 30.0 / 365, 1.0, 5.0};
        const double vols[] = {0.05, 0.2, 0.8, 2.5};
        for (double T : Ts)
            for (double v : vols)
                for (double z : {-3.0, -1.0, 0.0, 1.0, 3.0}) {
                    const double F = 100;
                    const double K = F * std::exp(z * v * std::sqrt(T));
                    for (bool call : {true, false}) {
                        const double px = md::black76_price(call, F, K, T, 0.03, v);
                        const md::VolSolve s = md::black76_implied_vol(px, call, F, K, T, 0.03);
                        // Below ~1e-12 of the forward a price carries no recoverable volatility.
                        if (px < 1e-10 * F)
                            continue;
                        QVERIFY2(s.status == md::SolveStatus::Ok, "black76 IV solves");
                        QVERIFY2(close_rel(s.sigma, v, 1e-6), "black76 IV round trip");
                        const double nv = v * F; // normal vol of similar size
                        const double pn = md::bachelier_price(call, F, K, T, 0.03, nv);
                        const md::VolSolve sn = md::bachelier_implied_vol(pn, call, F, K, T, 0.03);
                        // Only the time value carries the volatility: when it is lost below double
                        // precision of the price (strikes 10+ normal deviations away) σ = 0 is the answer.
                        const double time_value = pn - std::exp(-0.03 * T) * std::max(call ? F - K : K - F, 0.0);
                        if (time_value > 1e-9 * std::max(pn, 1.0)) {
                            QVERIFY2(sn.status == md::SolveStatus::Ok, "bachelier IV solves");
                            QVERIFY2(close_rel(sn.sigma, nv, 1e-6), "bachelier IV round trip");
                        }
                    }
                }
        // No solution is reported, never NaN.
        md::VolSolve below = md::bsm_implied_vol(5.0, true, 120, 100, 1, 0.05, 0.0); // < intrinsic
        QCOMPARE(below.status, md::SolveStatus::BelowIntrinsic);
        md::VolSolve above = md::bsm_implied_vol(130.0, true, 120, 100, 1, 0.05, 0.0); // > S
        QCOMPARE(above.status, md::SolveStatus::AboveMaximum);
        md::VolSolve bad = md::bsm_implied_vol(5.0, true, -1, 100, 1, 0.05, 0.0);
        QCOMPARE(bad.status, md::SolveStatus::InvalidInput);
        QCOMPARE(md::bachelier_implied_vol(-1.0, true, 100, 90, 1, 0.0).status, md::SolveStatus::BelowIntrinsic);
        // Generic solver: brackets outward, reports AboveMaximum when the model saturates.
        const md::VolSolve sat = md::solve_increasing([](double s) { return 1.0 - std::exp(-s); }, 2.0, 0.0, 1.0, 50.0);
        QCOMPARE(sat.status, md::SolveStatus::AboveMaximum);
    }

    void american_implied_vol_round_trip() {
        const double px = md::crr_price(false, true, 100, 105, 0.75, 0.05, 0.01, 0.33, 400).price;
        const md::VolSolve s = md::american_implied_vol(px, md::AmericanModel::Binomial, false, 100, 105, 0.75, 0.05,
                                                        0.01, 400);
        QCOMPARE(s.status, md::SolveStatus::Ok);
        QVERIFY(close_abs(s.sigma, 0.33, 1e-5));
        const double pb = md::baw_price(false, 100, 105, 0.75, 0.05, 0.01, 0.33);
        const md::VolSolve sb = md::american_implied_vol(pb, md::AmericanModel::BaroneAdesiWhaley, false, 100, 105,
                                                         0.75, 0.05, 0.01);
        QCOMPARE(sb.status, md::SolveStatus::Ok);
        QVERIFY(close_abs(sb.sigma, 0.33, 1e-6));
    }

    // ── American exercise ────────────────────────────────────────────────────
    void crr_matches_hull_american_put() {
        // [Hull] ch. 21 (Basic numerical procedures), American put S=50, K=50, r=10%, σ=40%,
        // T=5 months: 4.49 with 5 steps; 4.263 (30), 4.272 (50), 4.278 (100), 4.283 (500).
        const double T = 5.0 / 12.0;
        QVERIFY(close_abs(md::crr_price(false, true, 50, 50, T, 0.10, 0.0, 0.40, 5).price, 4.49, 0.005));
        QVERIFY(close_abs(md::crr_price(false, true, 50, 50, T, 0.10, 0.0, 0.40, 30).price, 4.263, 5e-4));
        QVERIFY(close_abs(md::crr_price(false, true, 50, 50, T, 0.10, 0.0, 0.40, 50).price, 4.272, 5e-4));
        QVERIFY(close_abs(md::crr_price(false, true, 50, 50, T, 0.10, 0.0, 0.40, 100).price, 4.278, 5e-4));
        QVERIFY(close_abs(md::crr_price(false, true, 50, 50, T, 0.10, 0.0, 0.40, 500).price, 4.283, 5e-4));
    }

    void american_bounds_and_models() {
        for (const P& c : grid_cases()) {
            const double euro = md::bsm_price(c.call, c.S, c.K, c.T, c.r, c.q, c.v);
            const md::TreeResult am = md::crr_price(c.call, true, c.S, c.K, c.T, c.r, c.q, c.v, 1500);
            const md::TreeResult eu = md::crr_price(c.call, false, c.S, c.K, c.T, c.r, c.q, c.v, 1500);
            QVERIFY(am.valid && eu.valid);
            QVERIFY2(close_abs(eu.price, euro, 0.01), "European tree converges to BSM");
            QVERIFY2(am.price >= eu.price - 1e-12, "American >= European");
            QVERIFY2(am.price >= std::max(c.call ? c.S - c.K : c.K - c.S, 0.0) - 1e-12, "American >= intrinsic");
            const double baw = md::baw_price(c.call, c.S, c.K, c.T, c.r, c.q, c.v);
            QVERIFY2(baw >= euro - 1e-12, "BAW >= European");
            // BAW is an approximation: within a few cents (≤ 1 % here) of a fine tree.
            QVERIFY2(close_abs(baw, am.price, std::max(0.03, 0.01 * am.price)), "BAW close to the binomial tree");
        }
        // No dividends: an American call is never exercised early (exactly European on the same tree).
        const md::TreeResult call = md::crr_price(true, true, 100, 95, 1.0, 0.05, 0.0, 0.25, 800);
        QVERIFY(close_abs(call.early_exercise_premium, 0.0, 1e-12));
        QVERIFY(close_abs(md::baw_price(true, 100, 95, 1.0, 0.05, 0.0, 0.25),
                          md::bsm_price(true, 100, 95, 1.0, 0.05, 0.0, 0.25), 1e-12));
        // Tree delta / gamma / theta near BSM for a European option.
        const md::TreeResult eu = md::crr_price(true, false, 100, 100, 1.0, 0.05, 0.02, 0.25, 2000);
        const md::Greeks bs = md::bsm_full(true, 100, 100, 1.0, 0.05, 0.02, 0.25);
        QVERIFY(close_abs(eu.delta, bs.delta, 2e-3) && close_abs(eu.gamma, bs.gamma, 1e-3) &&
                close_abs(eu.theta, bs.theta, 0.05));
    }

    void discrete_dividends() {
        // Escrowed model: a European option is BSM on S − PV(dividends).
        const std::vector<md::CashDividend> divs = {{0.25, 1.5}, {0.75, 1.5}};
        const double pv = md::pv_dividends(divs, 1.0, 0.04);
        QVERIFY(close_abs(pv, 1.5 * std::exp(-0.01) + 1.5 * std::exp(-0.03), 1e-12));
        const md::TreeResult eu = md::crr_price(true, false, 100, 100, 1.0, 0.04, 0.0, 0.3, 2000, divs);
        QVERIFY(close_abs(eu.price, md::bsm_price(true, 100 - pv, 100, 1.0, 0.04, 0.0, 0.3), 0.01));
        // A large dividend just before expiry makes early exercise of a deep call worthwhile.
        const std::vector<md::CashDividend> big = {{0.9, 8.0}};
        const md::TreeResult am = md::crr_price(true, true, 120, 100, 1.0, 0.04, 0.0, 0.2, 800, big);
        QVERIFY(am.early_exercise_premium > 0.5);
    }

    // ── Digitals ─────────────────────────────────────────────────────────────
    void digitals() {
        // [Haug] §4.19.2 cash-or-nothing: S=100, K=80, T=0.75, r=6%, b=0 (q=6%), σ=35%, cash 10 → put 2.6710.
        QVERIFY(close_abs(ex::digital_cash_or_nothing(false, 100, 80, 0.75, 0.06, 0.06, 0.35, 10), 2.6710, 5e-5));
        // [Haug] §4.19.3 asset-or-nothing: S=70, K=65, T=0.5, r=7%, b=2% (q=5%), σ=27% → put 20.2069.
        QVERIFY(close_abs(ex::digital_asset_or_nothing(false, 70, 65, 0.5, 0.07, 0.05, 0.27), 20.2069, 5e-5));
        // Identities: cash call + put = discounted cash; asset call + put = S e^{−qT};
        // asset-or-nothing call − K·cash-or-nothing(1) call = the vanilla call.
        const double S = 103, K = 97, T = 0.6, r = 0.03, q = 0.01, v = 0.27;
        QVERIFY(close_abs(ex::digital_cash_or_nothing(true, S, K, T, r, q, v, 1) +
                              ex::digital_cash_or_nothing(false, S, K, T, r, q, v, 1),
                          std::exp(-r * T), 1e-14));
        QVERIFY(close_abs(ex::digital_asset_or_nothing(true, S, K, T, r, q, v) +
                              ex::digital_asset_or_nothing(false, S, K, T, r, q, v),
                          S * std::exp(-q * T), 1e-12));
        QVERIFY(close_abs(ex::digital_asset_or_nothing(true, S, K, T, r, q, v) -
                              K * ex::digital_cash_or_nothing(true, S, K, T, r, q, v, 1),
                          md::bsm_price(true, S, K, T, r, q, v), 1e-12));
        QVERIFY(close_abs(ex::digital_cash_or_nothing(true, 105, 100, 0.0, 0.05, 0, 0.2, 7), 7.0, 1e-12));
    }

    // ── Barriers: Haug Table 4-13 (Reiner-Rubinstein), S=100, rebate 3, T=0.5, r=8%, q=4%, σ=25% ──
    void barrier_haug_table() {
        struct B {
            ex::BarrierKind kind;
            bool call;
            double H, K, expected;
        };
        using BK = ex::BarrierKind;
        const B cases[] = {
            {BK::DownOut, true, 95, 90, 9.0246},  {BK::DownOut, true, 95, 100, 6.7924},  {BK::DownOut, true, 95, 110, 4.8759},
            {BK::UpOut, true, 105, 90, 2.6789},   {BK::UpOut, true, 105, 100, 2.3580},   {BK::UpOut, true, 105, 110, 2.3453},
            {BK::DownIn, true, 95, 90, 7.7627},   {BK::DownIn, true, 95, 100, 4.0109},   {BK::DownIn, true, 95, 110, 2.0576},
            {BK::UpIn, true, 105, 90, 14.1112},   {BK::UpIn, true, 105, 100, 8.4482},    {BK::UpIn, true, 105, 110, 4.5910},
            {BK::DownOut, false, 95, 90, 2.2798}, {BK::DownOut, false, 95, 100, 2.2947}, {BK::DownOut, false, 95, 110, 2.6252},
            {BK::UpOut, false, 105, 90, 3.7760},  {BK::UpOut, false, 105, 100, 5.4932},  {BK::UpOut, false, 105, 110, 7.5187},
            {BK::DownIn, false, 95, 90, 2.9586},  {BK::DownIn, false, 95, 100, 6.5677},  {BK::DownIn, false, 95, 110, 11.9752},
            {BK::UpIn, false, 105, 90, 1.4653},   {BK::UpIn, false, 105, 100, 3.3721},   {BK::UpIn, false, 105, 110, 7.0846},
        };
        for (const B& b : cases) {
            const double v = ex::barrier(b.kind, b.call, 100, b.K, b.H, 3.0, 0.5, 0.08, 0.04, 0.25);
            QVERIFY2(close_abs(v, b.expected, 1.5e-4), qPrintable(QStringLiteral("barrier H=%1 K=%2 got %3 want %4")
                                                                       .arg(b.H).arg(b.K).arg(v, 0, 'f', 6).arg(b.expected)));
        }
    }

    void barrier_parity_and_breach() {
        // Without a rebate, knock-in + knock-out = the vanilla option.
        for (bool call : {true, false})
            for (double K : {90.0, 100.0, 110.0}) {
                const double van = md::bsm_price(call, 100, K, 0.5, 0.08, 0.04, 0.25);
                const double dn = ex::barrier(ex::BarrierKind::DownIn, call, 100, K, 95, 0, 0.5, 0.08, 0.04, 0.25) +
                                  ex::barrier(ex::BarrierKind::DownOut, call, 100, K, 95, 0, 0.5, 0.08, 0.04, 0.25);
                const double up = ex::barrier(ex::BarrierKind::UpIn, call, 100, K, 105, 0, 0.5, 0.08, 0.04, 0.25) +
                                  ex::barrier(ex::BarrierKind::UpOut, call, 100, K, 105, 0, 0.5, 0.08, 0.04, 0.25);
                QVERIFY(close_abs(dn, van, 1e-10) && close_abs(up, van, 1e-10));
            }
        // Already breached: knock-out pays the rebate, knock-in is the vanilla.
        QVERIFY(close_abs(ex::barrier(ex::BarrierKind::DownOut, true, 94, 100, 95, 3, 0.5, 0.08, 0.04, 0.25), 3.0, 1e-12));
        QVERIFY(close_abs(ex::barrier(ex::BarrierKind::DownIn, true, 94, 100, 95, 3, 0.5, 0.08, 0.04, 0.25),
                          md::bsm_price(true, 94, 100, 0.5, 0.08, 0.04, 0.25), 1e-12));
        // σ → 0 and T → 0 stay finite.
        QVERIFY(std::isfinite(ex::barrier(ex::BarrierKind::UpOut, true, 100, 100, 120, 1, 1.0, 0.05, 0.0, 0.0)));
        QVERIFY(std::isfinite(ex::barrier(ex::BarrierKind::UpIn, false, 100, 100, 120, 1, 0.0, 0.05, 0.0, 0.3)));
    }

    // ── Asians ───────────────────────────────────────────────────────────────
    void asian_options() {
        // [Haug] §4.20.1 geometric average (Kemna-Vorst): S=80, K=85, T=0.25, r=5%, b=8% (q=−3%), σ=20% → put 4.6922.
        QVERIFY(close_abs(ex::asian_geometric(false, 80, 85, 0.25, 0.05, -0.03, 0.20), 4.6922, 5e-5));
        // Turnbull-Wakeman arithmetic: above the geometric call (AM ≥ GM), and close to Monte Carlo.
        const double S = 100, K = 100, T = 1.0, r = 0.05, q = 0.02, v = 0.25;
        const double tw = ex::asian_arithmetic_tw(true, S, K, T, r, q, v);
        QVERIFY(tw > ex::asian_geometric(true, S, K, T, r, q, v));
        std::mt19937_64 rng(20261117);
        std::normal_distribution<double> nd(0.0, 1.0);
        const int paths = 40000, steps = 252;
        const double dt = T / steps, drift = (r - q - 0.5 * v * v) * dt, vol = v * std::sqrt(dt);
        double sum = 0, sum2 = 0;
        for (int p = 0; p < paths; ++p) {
            double s1 = S, s2 = S, a1 = 0, a2 = 0; // antithetic pair
            for (int i = 0; i < steps; ++i) {
                const double z = nd(rng);
                s1 *= std::exp(drift + vol * z);
                s2 *= std::exp(drift - vol * z);
                a1 += s1;
                a2 += s2;
            }
            const double pay = 0.5 * (std::max(a1 / steps - K, 0.0) + std::max(a2 / steps - K, 0.0));
            sum += pay;
            sum2 += pay * pay;
        }
        const double mean = sum / paths, se = std::sqrt((sum2 / paths - mean * mean) / paths);
        const double mc = std::exp(-r * T) * mean;
        QVERIFY2(std::fabs(tw - mc) < 3 * se + 0.01 * mc,
                 qPrintable(QStringLiteral("TW %1 vs MC %2 (se %3)").arg(tw).arg(mc).arg(se)));
        QVERIFY(std::isfinite(ex::asian_arithmetic_tw(true, 100, 100, 1.0, 0.03, 0.03, 0.2))); // b = 0 branch
        QVERIFY(std::isfinite(ex::asian_arithmetic_tw(true, 100, 100, 1.0, 0.03, 0.0, 0.0)));  // σ = 0
    }

    void asian_seasoned() {
        // [Haug] §4.20.2: averaging already started. Values recomputed independently in Python
        // (D:\ft-scratch\deriv2\tools\asianref.py: own Turnbull-Wakeman + strike adjustment) and
        // checked against a 400,000-path antithetic Monte Carlo of the continuous average:
        //   call S=100 K=100 T=0.5 left, r=5%, q=2%, σ=25%, 0.5 y elapsed at A=105: TW 3.66413122 (MC 3.6532 ± 0.0064)
        //   put  same, A=95:                                                          TW 3.24774340 (MC 3.2536 ± 0.0054)
        //   call K=100, 0.75 y elapsed at A=250, T=0.25 left: K* < 0, exact forward 111.1953198726; put 0.
        QVERIFY(close_abs(ex::asian_arithmetic_tw_seasoned(true, 100, 100, 0.5, 0.05, 0.02, 0.25, 105, 0.5), 3.66413122, 1e-7));
        QVERIFY(close_abs(ex::asian_arithmetic_tw_seasoned(false, 100, 100, 0.5, 0.05, 0.02, 0.25, 95, 0.5), 3.24774340, 1e-7));
        QVERIFY(close_abs(ex::asian_arithmetic_tw_seasoned(true, 100, 100, 0.25, 0.05, 0.02, 0.25, 250, 0.75),
                          111.1953198726, 1e-8));
        QCOMPARE(ex::asian_arithmetic_tw_seasoned(false, 100, 100, 0.25, 0.05, 0.02, 0.25, 250, 0.75), 0.0);
        // Nothing elapsed: the plain Turnbull-Wakeman value; averaging over: the payoff on the average.
        QCOMPARE(ex::asian_arithmetic_tw_seasoned(true, 100, 100, 0.5, 0.05, 0.02, 0.25, 105, 0.0),
                 ex::asian_arithmetic_tw(true, 100, 100, 0.5, 0.05, 0.02, 0.25));
        QCOMPARE(ex::asian_arithmetic_tw_seasoned(true, 100, 100, 0.0, 0.05, 0.02, 0.25, 104, 1.0), 4.0);
        // A higher observed average is worth more to a call, less to a put.
        QVERIFY(ex::asian_arithmetic_tw_seasoned(true, 100, 100, 0.5, 0.05, 0.02, 0.25, 110, 0.5) >
                ex::asian_arithmetic_tw_seasoned(true, 100, 100, 0.5, 0.05, 0.02, 0.25, 100, 0.5));
        QVERIFY(ex::asian_arithmetic_tw_seasoned(false, 100, 100, 0.5, 0.05, 0.02, 0.25, 110, 0.5) <
                ex::asian_arithmetic_tw_seasoned(false, 100, 100, 0.5, 0.05, 0.02, 0.25, 100, 0.5));
    }

    // ── Spread / exchange ────────────────────────────────────────────────────
    void spread_and_exchange() {
        // Kirk is exact for K = 0, where it is Margrabe's exchange option on forwards.
        const double F1 = 110, F2 = 100, T = 0.75, r = 0.04, s1 = 0.3, s2 = 0.25, rho = 0.6;
        const double kirk0 = ex::spread_kirk(true, F1, F2, 0.0, T, r, s1, s2, rho);
        const double marg = ex::exchange_margrabe(F1, F2, T, r, r, s1, s2, rho);
        QVERIFY(close_abs(kirk0, marg, 1e-10));
        // K > 0: close to Monte Carlo of the two correlated lognormal forwards.
        std::mt19937_64 rng(42);
        std::normal_distribution<double> nd(0.0, 1.0);
        const double K = 5;
        double sum = 0;
        const int n = 200000;
        for (int i = 0; i < n; ++i) {
            const double z1 = nd(rng), z2 = rho * z1 + std::sqrt(1 - rho * rho) * nd(rng);
            const double f1 = F1 * std::exp(-0.5 * s1 * s1 * T + s1 * std::sqrt(T) * z1);
            const double f2 = F2 * std::exp(-0.5 * s2 * s2 * T + s2 * std::sqrt(T) * z2);
            sum += std::max(f1 - f2 - K, 0.0);
        }
        const double mc = std::exp(-r * T) * sum / n;
        const double kirk = ex::spread_kirk(true, F1, F2, K, T, r, s1, s2, rho);
        QVERIFY2(std::fabs(kirk - mc) < 0.01 * mc + 0.03, qPrintable(QStringLiteral("Kirk %1 vs MC %2").arg(kirk).arg(mc)));
        // Put-call parity of the spread: C − P = e^{−rT}(F1 − F2 − K).
        const double put = ex::spread_kirk(false, F1, F2, K, T, r, s1, s2, rho);
        QVERIFY(close_abs(kirk - put, std::exp(-r * T) * (F1 - F2 - K), 1e-10));
    }
};

QTEST_APPLESS_MAIN(TestDerivPricing)
#include "tst_deriv_pricing.moc"
