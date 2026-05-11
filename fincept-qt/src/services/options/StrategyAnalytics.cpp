#include "services/options/StrategyAnalytics.h"

#include "services/options/OptionPricing.h"

#include <QDate>
#include <QHash>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::services::options::analytics {

using fincept::trading::InstrumentType;

namespace {

/// Days-to-expiry for a leg, actual/365. Floors at 1 day so expiry-day
/// strategies don't collapse the target curve into intrinsic immediately.
int days_to_expiry(const QString& expiry) {
    QDate exp = QDate::fromString(expiry, "dd-MMM-yy");
    if (!exp.isValid())
        exp = QDate::fromString(expiry, "yyyy-MM-dd");
    if (!exp.isValid())
        return 1;
    const int d = QDate::currentDate().daysTo(exp);
    return d < 0 ? 0 : d;
}

double t_years(int days) {
    return std::max(days, 0) / 365.0;
}

/// Per-leg P/L at a given spot — intrinsic at expiry. Returns (pnl, signed_units).
double leg_pnl_expiry(const StrategyLeg& leg, double S) {
    const double signed_units = double(leg.lots) * double(leg.lot_size);
    double intrinsic = 0;
    if (leg.type == InstrumentType::CE)
        intrinsic = std::max(S - leg.strike, 0.0);
    else if (leg.type == InstrumentType::PE)
        intrinsic = std::max(leg.strike - S, 0.0);
    // signed_units * (intrinsic - entry_price)
    //   buy:  paid premium up front; profit when intrinsic > entry
    //   sell: received premium; profit when intrinsic < entry
    return signed_units * (intrinsic - leg.entry_price);
}

/// Per-leg P/L at a given spot, target date — BSM-priced.
double leg_pnl_target(const StrategyLeg& leg, double S, int days_remaining,
                      double r, double sigma_fallback) {
    const double signed_units = double(leg.lots) * double(leg.lot_size);
    if (days_remaining <= 0)
        return leg_pnl_expiry(leg, S);
    const double t = days_remaining / 365.0;
    const double sigma = (leg.iv_at_entry > 0) ? leg.iv_at_entry : sigma_fallback;
    double price = 0;
    if (leg.type == InstrumentType::CE)
        price = pricing::bs_call(S, leg.strike, t, r, sigma);
    else if (leg.type == InstrumentType::PE)
        price = pricing::bs_put(S, leg.strike, t, r, sigma);
    return signed_units * (price - leg.entry_price);
}

/// Net call lots — drives upside-tail unbounded detection.
double net_call_lots(const Strategy& s) {
    double n = 0;
    for (const auto& leg : s.legs) {
        if (!leg.is_active)
            continue;
        if (leg.type == InstrumentType::CE)
            n += double(leg.lots) * double(leg.lot_size);
    }
    return n;
}

/// Net put lots — drives downside-tail behaviour (capped at S=0 always,
/// but still useful when computing left-tail POP regions).
[[maybe_unused]] double net_put_lots(const Strategy& s) {
    double n = 0;
    for (const auto& leg : s.legs) {
        if (!leg.is_active)
            continue;
        if (leg.type == InstrumentType::PE)
            n += double(leg.lots) * double(leg.lot_size);
    }
    return n;
}

}  // namespace

// ── Public API ──────────────────────────────────────────────────────────────

double net_premium(const Strategy& s) {
    double net = 0;
    for (const auto& leg : s.legs) {
        if (!leg.is_active)
            continue;
        net += double(leg.lots) * double(leg.lot_size) * leg.entry_price;
    }
    return net;
}

QVector<PayoffPoint> compute_payoff(const Strategy& s, const PayoffComputeOptions& opts) {
    QVector<PayoffPoint> out;
    if (s.legs.isEmpty())
        return out;

    // Resolve sample bounds — auto = ±30% of current_spot if not specified.
    double lo = opts.spot_min;
    double hi = opts.spot_max;
    if (lo <= 0 || hi <= 0 || hi <= lo) {
        const double anchor = opts.current_spot > 0 ? opts.current_spot : s.legs.first().strike;
        lo = anchor * 0.70;
        hi = anchor * 1.30;
    }
    const int n = std::max(opts.n_points, 2);
    const double step = (hi - lo) / (n - 1);

    // Pick the strategy's nearest-expiry leg as the BSM time-to-expiry anchor.
    // All single-expiry templates share a date; for hand-edited mixed-expiry
    // strategies we use the leg-specific expiry inside leg_pnl_target.
    int dte_strategy = 0;
    for (const auto& leg : s.legs) {
        const int d = days_to_expiry(leg.expiry);
        if (dte_strategy == 0 || (d > 0 && d < dte_strategy))
            dte_strategy = d;
    }
    const int days_target_capped = std::clamp(opts.days_to_target, 0, dte_strategy);

    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        const double S = lo + step * i;
        PayoffPoint p;
        p.spot = S;
        p.pnl_expiry = 0;
        p.pnl_target = 0;
        for (const auto& leg : s.legs) {
            if (!leg.is_active)
                continue;
            p.pnl_expiry += leg_pnl_expiry(leg, S);
            const int leg_dte = days_to_expiry(leg.expiry);
            const int leg_remaining = std::max(0, leg_dte - days_target_capped);
            p.pnl_target += leg_pnl_target(leg, S, leg_remaining, opts.risk_free_rate, opts.fallback_iv);
        }
        out.append(p);
    }
    return out;
}

QVector<double> compute_breakevens(const QVector<PayoffPoint>& curve) {
    QVector<double> bes;
    if (curve.size() < 2)
        return bes;
    for (int i = 0; i + 1 < curve.size(); ++i) {
        const double a = curve[i].pnl_expiry;
        const double b = curve[i + 1].pnl_expiry;
        if (a == 0) {
            bes.append(curve[i].spot);
            continue;
        }
        if ((a < 0 && b > 0) || (a > 0 && b < 0)) {
            // Linear interpolation: zero crossing between samples.
            const double t = a / (a - b);
            bes.append(curve[i].spot + t * (curve[i + 1].spot - curve[i].spot));
        }
    }
    std::sort(bes.begin(), bes.end());
    bes.erase(std::unique(bes.begin(), bes.end(),
                          [](double a, double b) { return std::abs(a - b) < 1e-3; }),
              bes.end());
    return bes;
}

MaxPnL compute_max_pnl(const QVector<PayoffPoint>& curve, const Strategy& s) {
    MaxPnL m;
    if (curve.isEmpty())
        return m;
    m.max_profit = -std::numeric_limits<double>::infinity();
    m.max_loss = std::numeric_limits<double>::infinity();
    for (const auto& p : curve) {
        if (p.pnl_expiry > m.max_profit)
            m.max_profit = p.pnl_expiry;
        if (p.pnl_expiry < m.max_loss)
            m.max_loss = p.pnl_expiry;
    }
    const double net_calls = net_call_lots(s);
    if (net_calls > 0) {
        m.profit_unbounded = true;
        m.max_profit = std::numeric_limits<double>::infinity();
    } else if (net_calls < 0) {
        m.loss_unbounded = true;
        m.max_loss = -std::numeric_limits<double>::infinity();
    }
    return m;
}

double compute_pop(const Strategy& s, double current_spot, double t, double r, double sigma) {
    if (s.legs.isEmpty() || current_spot <= 0 || t <= 0 || sigma <= 0)
        return 0.0;

    // Build a finely sampled curve over a wide range (±5σ on the GBM
    // price distribution) so every relevant profit/loss region is covered.
    const double sigma_t = sigma * std::sqrt(t);
    const double mu = std::log(current_spot) + (r - 0.5 * sigma * sigma) * t;
    const double lo = std::exp(mu - 5.0 * sigma_t);
    const double hi = std::exp(mu + 5.0 * sigma_t);

    PayoffComputeOptions opts;
    opts.spot_min = std::min(lo, current_spot * 0.30);
    opts.spot_max = std::max(hi, current_spot * 3.00);
    opts.n_points = 1001;
    opts.current_spot = current_spot;
    opts.risk_free_rate = r;
    opts.fallback_iv = sigma;
    QVector<PayoffPoint> curve = compute_payoff(s, opts);
    if (curve.size() < 2)
        return 0.0;

    auto ln_cdf = [&](double x) {
        if (x <= 0)
            return 0.0;
        return pricing::normal_cdf((std::log(x) - mu) / sigma_t);
    };

    // Sweep through curve, summing P(profitable interval).
    double pop = 0.0;
    for (int i = 0; i + 1 < curve.size(); ++i) {
        const double mid_pnl = 0.5 * (curve[i].pnl_expiry + curve[i + 1].pnl_expiry);
        if (mid_pnl > 0) {
            pop += ln_cdf(curve[i + 1].spot) - ln_cdf(curve[i].spot);
        }
    }
    return std::clamp(pop, 0.0, 1.0);
}

OptionGreeks combined_greeks(const Strategy& s, const OptionChain& chain) {
    OptionGreeks combined;
    combined.valid = true;
    if (s.legs.isEmpty() || chain.rows.isEmpty())
        return combined;

    // Two-tier lookup: token-keyed hash (O(1), correct for live-instantiated
    // strategies) and a (strike, side) fallback. The fallback rescues
    // strategies that were saved/loaded — the persisted leg.token may not
    // exist in today's chain (different broker session, different expiry
    // ladder), but matching by strike+type still pulls live Greeks for the
    // equivalent leg.
    QHash<qint64, OptionGreeks> by_token;
    QHash<QPair<double, fincept::trading::InstrumentType>, OptionGreeks> by_strike_type;
    by_token.reserve(chain.rows.size() * 2);
    by_strike_type.reserve(chain.rows.size() * 2);
    for (const auto& row : chain.rows) {
        if (row.ce_token != 0 && row.ce_greeks.valid) {
            by_token.insert(row.ce_token, row.ce_greeks);
            by_strike_type.insert({row.strike, fincept::trading::InstrumentType::CE}, row.ce_greeks);
        }
        if (row.pe_token != 0 && row.pe_greeks.valid) {
            by_token.insert(row.pe_token, row.pe_greeks);
            by_strike_type.insert({row.strike, fincept::trading::InstrumentType::PE}, row.pe_greeks);
        }
    }

    for (const auto& leg : s.legs) {
        if (!leg.is_active)
            continue;
        const OptionGreeks* g = nullptr;
        if (leg.instrument_token != 0) {
            auto it = by_token.constFind(leg.instrument_token);
            if (it != by_token.constEnd())
                g = &it.value();
        }
        if (!g) {
            // Token miss — fall back to (strike, type). Helpful for loaded
            // strategies whose tokens were captured in an earlier session.
            auto it = by_strike_type.constFind({leg.strike, leg.type});
            if (it != by_strike_type.constEnd())
                g = &it.value();
        }
        if (!g)
            continue;  // Greeks genuinely not available for this leg — skip
        const double signed_units = double(leg.lots) * double(leg.lot_size);
        combined.delta += signed_units * g->delta;
        combined.gamma += signed_units * g->gamma;
        combined.theta += signed_units * g->theta;
        combined.vega += signed_units * g->vega;
        combined.rho += signed_units * g->rho;
    }
    return combined;
}

StrategyAnalytics compute_all(const Strategy& s, const OptionChain& chain,
                              const PayoffComputeOptions& opts) {
    StrategyAnalytics out;
    if (s.legs.isEmpty())
        return out;

    PayoffComputeOptions o = opts;
    if (o.current_spot <= 0)
        o.current_spot = chain.spot;

    const QVector<PayoffPoint> curve = compute_payoff(s, o);
    const MaxPnL pnl = compute_max_pnl(curve, s);
    const QVector<double> bes = compute_breakevens(curve);

    // Pick a t for POP — use the strategy's nearest-leg expiry.
    int dte = 0;
    for (const auto& leg : s.legs) {
        const int d = days_to_expiry(leg.expiry);
        if (dte == 0 || (d > 0 && d < dte))
            dte = d;
    }
    const double sigma_for_pop = (o.fallback_iv > 0) ? o.fallback_iv : 0.20;

    out.combined = combined_greeks(s, chain);
    out.max_profit = pnl.max_profit;
    out.max_loss = pnl.max_loss;
    out.breakevens = bes;
    out.pop = compute_pop(s, o.current_spot, t_years(dte), o.risk_free_rate, sigma_for_pop);
    out.premium_paid = net_premium(s);
    out.margin_required = 0;       // Phase 6 wires IBroker::get_basket_margins
    out.margin_estimated = false;
    return out;
}

} // namespace fincept::services::options::analytics
