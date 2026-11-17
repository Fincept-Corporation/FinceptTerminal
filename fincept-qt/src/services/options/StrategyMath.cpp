#include "services/options/StrategyMath.h"

#include "services/options/OptionModels.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::options::strategy {

namespace {

constexpr double kInvSqrt2 = 0.70710678118654752440;
constexpr double kInvSqrt2Pi = 0.39894228040143267794;

inline double sm_cdf(double x) {
    return 0.5 * std::erfc(-x * kInvSqrt2);
}

bool sm_is_option(const Leg& l) {
    return l.kind != LegKind::Underlying;
}

double sm_horizon(const Spec& spec) {
    return first_expiry(spec);
}

/// Exact-zero bisection of f on [a, b] where f(a)·f(b) < 0.
template <typename F>
double sm_bisect(F&& f, double a, double b, double fa) {
    for (int i = 0; i < 80; ++i) {
        const double c = 0.5 * (a + b);
        const double fc = f(c);
        if (fc == 0)
            return c;
        if ((fc < 0) == (fa < 0)) {
            a = c;
            fa = fc;
        } else {
            b = c;
        }
    }
    return 0.5 * (a + b);
}

} // namespace

double leg_units(const Leg& leg, double multiplier) {
    return sm_is_option(leg) ? leg.quantity * multiplier : leg.quantity;
}

double leg_value(const Leg& leg, double S, double t, double r, double q, double vol_shift) {
    if (leg.kind == LegKind::Underlying)
        return S;
    const bool call = leg.kind == LegKind::Call;
    const double tau = leg.expiry_years - t;
    if (tau <= 1e-9 || !(S > 0)) {
        if (!(S > 0)) // the underlying at zero: calls are worthless, puts worth the discounted strike
            return call ? 0.0 : leg.strike * std::exp(-r * std::max(tau, 0.0));
        return std::max(call ? S - leg.strike : leg.strike - S, 0.0);
    }
    const double sigma = std::max(leg.iv + vol_shift, 1e-4);
    return models::bsm_price(call, S, leg.strike, tau, r, q, sigma);
}

double net_premium(const Spec& spec) {
    double net = 0;
    for (const Leg& l : spec.legs)
        if (l.active)
            net += leg_units(l, spec.multiplier) * l.entry_price;
    return net;
}

double first_expiry(const Spec& spec) {
    double first = 0;
    bool any = false;
    for (const Leg& l : spec.legs) {
        if (!l.active || !sm_is_option(l))
            continue;
        if (!any || l.expiry_years < first)
            first = l.expiry_years;
        any = true;
    }
    return any ? std::max(first, 0.0) : 0.0;
}

double pnl_at(const Spec& spec, const Market& m, double S, double t, double vol_shift) {
    double pnl = 0;
    for (const Leg& l : spec.legs)
        if (l.active)
            pnl += leg_units(l, spec.multiplier) * (leg_value(l, S, t, m.r, m.q, vol_shift) - l.entry_price);
    return pnl;
}

double position_value(const Spec& spec, const Market& m, double S, double t, double vol_shift, double rate_shift) {
    double v = 0;
    for (const Leg& l : spec.legs)
        if (l.active)
            v += leg_units(l, spec.multiplier) * leg_value(l, S, t, m.r + rate_shift, m.q, vol_shift);
    return v;
}

std::vector<PayoffPoint> payoff_curve(const Spec& spec, const Market& m, double s_lo, double s_hi, int n,
                                      double t_target) {
    std::vector<PayoffPoint> out;
    n = std::max(n, 2);
    if (!(s_hi > s_lo))
        return out;
    const double h = sm_horizon(spec);
    const double tt = std::clamp(t_target, 0.0, h);
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double S = s_lo + (s_hi - s_lo) * i / (n - 1);
        out.push_back({S, pnl_at(spec, m, S, h), pnl_at(spec, m, S, tt)});
    }
    return out;
}

std::vector<double> breakevens(const Spec& spec, const Market& m, double s_lo, double s_hi) {
    std::vector<double> out;
    if (!(s_hi > s_lo))
        return out;
    const double h = sm_horizon(spec);
    auto f = [&](double S) { return pnl_at(spec, m, S, h); };
    constexpr int kSamples = 2000;
    double prev_s = s_lo;
    double prev_f = f(s_lo);
    if (prev_f == 0)
        out.push_back(s_lo);
    for (int i = 1; i <= kSamples; ++i) {
        const double S = s_lo + (s_hi - s_lo) * i / kSamples;
        const double v = f(S);
        if (v == 0) {
            if (out.empty() || std::fabs(out.back() - S) > 1e-9)
                out.push_back(S);
        } else if (prev_f != 0 && (v < 0) != (prev_f < 0)) {
            out.push_back(sm_bisect(f, prev_s, S, prev_f));
        }
        prev_s = S;
        prev_f = v;
    }
    return out;
}

Extremes extremes(const Spec& spec, const Market& m) {
    Extremes ex;
    const double h = sm_horizon(spec);
    double k_max = std::max(m.spot, 1e-9);
    for (const Leg& l : spec.legs)
        if (l.active && sm_is_option(l))
            k_max = std::max(k_max, l.strike);
    const double s_hi = 3.0 * k_max;
    std::vector<double> grid;
    constexpr int kSamples = 3000;
    grid.reserve(kSamples + spec.legs.size() + 2);
    for (int i = 0; i <= kSamples; ++i)
        grid.push_back(s_hi * i / kSamples);
    for (const Leg& l : spec.legs)
        if (l.active && sm_is_option(l) && l.strike > 0)
            grid.push_back(l.strike); // the kinks of the expiry payoff
    bool first = true;
    for (double S : grid) {
        const double v = pnl_at(spec, m, S, h);
        if (first || v > ex.max_profit) {
            ex.max_profit = v;
            ex.spot_at_max_profit = S;
        }
        if (first || v < ex.max_loss) {
            ex.max_loss = v;
            ex.spot_at_max_loss = S;
        }
        first = false;
    }
    // Upside tail: the P&L keeps moving at the position's terminal slope.
    const double v1 = pnl_at(spec, m, s_hi, h);
    const double v2 = pnl_at(spec, m, 2.0 * s_hi, h);
    const double slope = (v2 - v1) / s_hi;
    double scale = 0;
    for (const Leg& l : spec.legs)
        if (l.active)
            scale += std::fabs(leg_units(l, spec.multiplier));
    const double tiny = 1e-6 * std::max(scale, 1.0);
    if (slope > tiny)
        ex.profit_unbounded = true;
    else if (slope < -tiny)
        ex.loss_unbounded = true;
    ex.max_loss = std::min(ex.max_loss, 0.0);
    return ex;
}

Probability probability_of_profit(const Spec& spec, const Market& m, double sigma) {
    Probability out;
    const double h = sm_horizon(spec);
    if (!(h > 0) || !(sigma > 0) || !(m.spot > 0))
        return out;
    const double sd = sigma * std::sqrt(h);
    const double mu = std::log(m.spot) + (m.r - m.q - 0.5 * sigma * sigma) * h;
    auto spot_at = [&](double z) { return std::exp(mu + sd * z); };
    auto f = [&](double z) { return pnl_at(spec, m, spot_at(z), h); };
    constexpr int kSamples = 1600;
    constexpr double kZ = 8.0;
    double prev_z = -kZ;
    double prev_f = f(prev_z);
    double seg_start = -kZ;
    bool positive = prev_f > 0;
    double pop = 0;
    double expected = 0;
    double prev_density_val = prev_f * kInvSqrt2Pi * std::exp(-0.5 * prev_z * prev_z);
    const double dz = 2 * kZ / kSamples;
    for (int i = 1; i <= kSamples; ++i) {
        const double z = -kZ + dz * i;
        const double v = f(z);
        const double dens_val = v * kInvSqrt2Pi * std::exp(-0.5 * z * z);
        expected += 0.5 * (prev_density_val + dens_val) * dz; // trapezoid
        prev_density_val = dens_val;
        if ((v > 0) != positive) {
            const double root = sm_bisect(f, prev_z, z, prev_f);
            if (positive)
                pop += sm_cdf(root) - sm_cdf(seg_start);
            seg_start = root;
            positive = v > 0;
        }
        prev_z = z;
        prev_f = v;
    }
    if (positive)
        pop += sm_cdf(kZ) - sm_cdf(seg_start);
    // The tails beyond ±8σ carry ~1e-15 probability: decided by their sign at ±8σ.
    out.pop = std::clamp(pop, 0.0, 1.0);
    out.expected_pnl = expected;
    out.valid = std::isfinite(out.pop);
    return out;
}

PositionGreeks greeks(const Spec& spec, const Market& m) {
    PositionGreeks g;
    for (const Leg& l : spec.legs) {
        if (!l.active)
            continue;
        const double u = leg_units(l, spec.multiplier);
        if (l.kind == LegKind::Underlying) {
            g.delta += u;
            continue;
        }
        if (!(l.expiry_years > 0) || !(m.spot > 0))
            continue;
        const models::Greeks lg = models::bsm_full(l.kind == LegKind::Call, m.spot, l.strike, l.expiry_years, m.r,
                                                   m.q, std::max(l.iv, 1e-4));
        if (!lg.valid)
            continue;
        g.delta += u * lg.delta;
        g.gamma += u * lg.gamma;
        g.theta_day += u * lg.theta / 365.0;
        g.vega_pt += u * lg.vega / 100.0;
        g.rho_pct += u * lg.rho / 100.0;
    }
    g.dollar_delta = g.delta * m.spot;
    return g;
}

MarginEstimate margin_estimate(const Spec& spec, const Market& m, bool broad_index) {
    MarginEstimate out;
    const double S = m.spot;
    const double mult = spec.multiplier;
    double stock_shares = 0;
    Spec opts;
    opts.multiplier = mult;
    for (const Leg& l : spec.legs) {
        if (!l.active)
            continue;
        if (l.kind == LegKind::Underlying)
            stock_shares += l.quantity;
        else
            opts.legs.push_back(l);
    }
    const double stock_req = 0.5 * std::fabs(stock_shares) * S;
    double opt_req = 0;
    std::string method;
    if (!opts.legs.empty()) {
        // Coverage first: short calls are covered by long calls or long stock, short puts by
        // long puts or short stock (Reg-T pairs them into spreads / covered positions).
        double short_calls = 0, long_calls = 0, short_puts = 0, long_puts = 0, long_prem = 0;
        for (const Leg& l : opts.legs) {
            if (l.kind == LegKind::Call)
                (l.quantity < 0 ? short_calls : long_calls) += std::fabs(l.quantity);
            else
                (l.quantity < 0 ? short_puts : long_puts) += std::fabs(l.quantity);
            if (l.quantity > 0)
                long_prem += std::fabs(l.quantity) * l.entry_price * mult;
        }
        const double cover_c = long_calls + std::max(stock_shares, 0.0) / mult;
        const double cover_p = long_puts + std::max(-stock_shares, 0.0) / mult;
        const double frac_c = short_calls > 0 ? std::max(0.0, short_calls - cover_c) / short_calls : 0.0;
        const double frac_p = short_puts > 0 ? std::max(0.0, short_puts - cover_p) / short_puts : 0.0;
        if (frac_c <= 0 && frac_p <= 0) {
            const Extremes ex = extremes(opts, m);
            if (!ex.loss_unbounded) {
                opt_req = std::max(0.0, -ex.max_loss); // spreads: the most the position can lose
                method = "defined risk: maximum loss at expiry";
            } else {
                opt_req = long_prem; // short calls covered by the stock: only long premiums are paid
                method = "covered by the stock; long options paid in full";
            }
        } else {
            const double pct = broad_index ? 0.15 : 0.20;
            double call_req = 0, put_req = 0, call_prem = 0, put_prem = 0;
            for (const Leg& l : opts.legs) {
                if (l.quantity >= 0)
                    continue;
                const double n = std::fabs(l.quantity);
                if (l.kind == LegKind::Call) {
                    const double otm = std::max(l.strike - S, 0.0);
                    call_req += n * frac_c * (l.entry_price + std::max(pct * S - otm, 0.10 * S)) * mult;
                    call_prem += n * frac_c * l.entry_price * mult;
                } else {
                    const double otm = std::max(S - l.strike, 0.0);
                    put_req += n * frac_p * (l.entry_price + std::max(pct * S - otm, 0.10 * l.strike)) * mult;
                    put_prem += n * frac_p * l.entry_price * mult;
                }
            }
            if (call_req > 0 && put_req > 0) // short straddle / strangle: the larger side + the other's premium
                opt_req = std::max(call_req + put_prem, put_req + call_prem);
            else
                opt_req = call_req + put_req;
            opt_req += long_prem;
            method = broad_index ? "uncovered short options: 15 % / 10 % rule" : "uncovered short options: 20 % / 10 % rule";
        }
    }
    if (stock_shares != 0)
        method += method.empty() ? "stock at 50 % (Reg-T initial)" : "; stock at 50 % (Reg-T initial)";
    out.requirement = opt_req + stock_req;
    out.method = method;
    return out;
}

// ── Templates ────────────────────────────────────────────────────────────────

const std::vector<Template>& templates() {
    using K = LegKind;
    static const std::vector<Template> kTemplates = {
        {"long_call", "Long call", "Bullish, limited risk", {{K::Call, 1, 0, 0}}},
        {"long_put", "Long put", "Bearish, limited risk", {{K::Put, 1, 0, 0}}},
        {"bull_call_spread", "Bull call spread (debit vertical)", "Moderately bullish", {{K::Call, 1, 0, 0}, {K::Call, -1, 1, 0}}},
        {"bear_put_spread", "Bear put spread (debit vertical)", "Moderately bearish", {{K::Put, 1, 0, 0}, {K::Put, -1, -1, 0}}},
        {"bull_put_spread", "Bull put spread (credit vertical)", "Neutral to bullish", {{K::Put, -1, 0, 0}, {K::Put, 1, -1, 0}}},
        {"bear_call_spread", "Bear call spread (credit vertical)", "Neutral to bearish", {{K::Call, -1, 0, 0}, {K::Call, 1, 1, 0}}},
        {"long_straddle", "Long straddle", "Big move either way", {{K::Call, 1, 0, 0}, {K::Put, 1, 0, 0}}},
        {"short_straddle", "Short straddle", "Range-bound, unlimited risk", {{K::Call, -1, 0, 0}, {K::Put, -1, 0, 0}}},
        {"long_strangle", "Long strangle", "Big move either way, cheaper", {{K::Call, 1, 1, 0}, {K::Put, 1, -1, 0}}},
        {"short_strangle", "Short strangle", "Range-bound, unlimited risk", {{K::Call, -1, 1, 0}, {K::Put, -1, -1, 0}}},
        {"long_call_butterfly", "Long call butterfly", "Pinned near the middle strike", {{K::Call, 1, -1, 0}, {K::Call, -2, 0, 0}, {K::Call, 1, 1, 0}}},
        {"iron_butterfly", "Iron butterfly", "Pinned near the money, defined risk", {{K::Put, 1, -1, 0}, {K::Put, -1, 0, 0}, {K::Call, -1, 0, 0}, {K::Call, 1, 1, 0}}},
        {"iron_condor", "Iron condor", "Range-bound, defined risk", {{K::Put, 1, -2, 0}, {K::Put, -1, -1, 0}, {K::Call, -1, 1, 0}, {K::Call, 1, 2, 0}}},
        {"call_calendar", "Call calendar", "Neutral near term, vol up", {{K::Call, -1, 0, 0}, {K::Call, 1, 0, 1}}},
        {"put_calendar", "Put calendar", "Neutral near term, vol up", {{K::Put, -1, 0, 0}, {K::Put, 1, 0, 1}}},
        {"call_diagonal", "Call diagonal", "Mildly bullish over time", {{K::Call, -1, 1, 0}, {K::Call, 1, 0, 1}}},
        {"call_ratio_spread", "Call ratio spread (1x2)", "Mildly bullish, risk above", {{K::Call, 1, 0, 0}, {K::Call, -2, 1, 0}}},
        {"put_ratio_spread", "Put ratio spread (1x2)", "Mildly bearish, risk below", {{K::Put, 1, 0, 0}, {K::Put, -2, -1, 0}}},
        {"collar", "Collar", "Own the stock, capped both ways", {{K::Underlying, 1, 0, 0}, {K::Put, 1, -1, 0}, {K::Call, -1, 1, 0}}},
        {"covered_call", "Covered call", "Own the stock, earn premium", {{K::Underlying, 1, 0, 0}, {K::Call, -1, 1, 0}}},
        {"protective_put", "Protective put", "Own the stock, insured", {{K::Underlying, 1, 0, 0}, {K::Put, 1, -1, 0}}},
        {"cash_secured_put", "Cash-secured put", "Willing to buy lower", {{K::Put, -1, -1, 0}}},
    };
    return kTemplates;
}

const Template* find_template(const std::string& id) {
    for (const Template& t : templates())
        if (t.id == id)
            return &t;
    return nullptr;
}

std::vector<Leg> instantiate(const Template& tpl, const ChainAccess& chain, int expiry_index, double atm_ref,
                             int width, int quantity, double multiplier, std::string* error) {
    std::vector<Leg> legs;
    auto fail = [&](const std::string& why) {
        if (error)
            *error = why;
        return std::vector<Leg>{};
    };
    if (!chain.strikes || !chain.years || !chain.quote || expiry_index < 0 || expiry_index >= chain.expiry_count)
        return fail("choose an expiry first");
    width = std::max(width, 1);
    quantity = std::max(quantity, 1);
    for (const LegRecipe& r : tpl.legs) {
        Leg leg;
        leg.kind = r.kind;
        if (r.kind == LegKind::Underlying) {
            leg.quantity = r.ratio * quantity * multiplier;
            leg.entry_price = atm_ref;
            legs.push_back(leg);
            continue;
        }
        const int e = expiry_index + r.expiry_slot;
        if (e >= chain.expiry_count)
            return fail("this strategy needs a later expiry than the one chosen");
        const std::vector<double> strikes = chain.strikes(e);
        if (strikes.empty())
            return fail("no strikes are listed for that expiry");
        size_t atm = 0;
        for (size_t i = 1; i < strikes.size(); ++i)
            if (std::fabs(strikes[i] - atm_ref) < std::fabs(strikes[atm] - atm_ref))
                atm = i;
        const long idx = static_cast<long>(atm) + static_cast<long>(r.strike_steps) * width;
        if (idx < 0 || idx >= static_cast<long>(strikes.size()))
            return fail("not enough strikes listed for this width - use a smaller width");
        leg.strike = strikes[static_cast<size_t>(idx)];
        leg.expiry_years = chain.years(e);
        leg.expiry_index = e;
        leg.quantity = r.ratio * quantity;
        double mid = 0, iv = 0;
        const bool have = chain.quote(e, leg.strike, r.kind == LegKind::Call, &mid, &iv);
        if (!(iv > 0))
            iv = 0;
        if (have && mid > 0) {
            leg.entry_price = mid;
        } else if (iv > 0) {
            leg.entry_price = models::bsm_price(r.kind == LegKind::Call, atm_ref, leg.strike, leg.expiry_years, 0.0,
                                                0.0, iv);
        } else {
            return fail("no usable quote for one of the legs - pick another expiry or width");
        }
        leg.iv = iv > 0 ? iv : 0.2;
        legs.push_back(leg);
    }
    if (error)
        error->clear();
    return legs;
}

} // namespace fincept::services::options::strategy
