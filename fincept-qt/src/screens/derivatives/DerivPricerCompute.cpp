// DerivPricerPage — calculation (local C++) and the result area: headline tiles, a
// detail strip, the method note and a chart on the shared chart kit, plus what the
// COPY / CSV / PNG / ADD TO REPORT toolbar exports.

#include "screens/derivatives/DerivPricerPage.h"

#include "core/logging/Logger.h"
#include "services/options/DerivativesPricing.h"
#include "services/options/ExoticPricing.h"
#include "services/options/OptionModels.h"
#include "services/quantlib/QuantLibAccess.h"
#include "services/quantlib/QuantLibClient.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::deriv {

namespace md = services::options::models;
namespace ex = services::options::exotic;
namespace dp = services::options::derivatives;

namespace {

// Keys and rounding are the screen's long-standing result contract (Python round()
// semantics), so every figure of the original calculators reads as it always has.
double pc_round(double v, int decimals) {
    return dp::round_half_even(v, decimals);
}

QJsonObject pc_greeks_json(const services::options::pricing::BsmGreeks& g) {
    QJsonObject o;
    o["delta"] = pc_round(g.delta, 6);
    o["gamma"] = pc_round(g.gamma, 6);
    o["theta"] = pc_round(g.theta, 6);
    o["vega"] = pc_round(g.vega * 0.01, 6);
    o["rho"] = pc_round(g.rho * 0.01, 6);
    return o;
}

QJsonArray pc_points(const QVector<QPointF>& pts) {
    QJsonArray a;
    for (const QPointF& p : pts)
        a.append(QJsonArray{p.x(), p.y()});
    return a;
}

QVector<QPointF> pc_points_from(const QJsonArray& a) {
    QVector<QPointF> pts;
    pts.reserve(a.size());
    for (const QJsonValue& v : a) {
        const QJsonArray xy = v.toArray();
        if (xy.size() == 2)
            pts.append(QPointF(xy.at(0).toDouble(), xy.at(1).toDouble()));
    }
    return pts;
}

QJsonObject pc_chart(const QString& title, const QString& x_title, const QString& curve_name,
                     const QVector<QPointF>& curve, const QString& ref_name, const QVector<QPointF>& ref,
                     const QPointF& marker, const QString& marker_name) {
    QJsonObject c;
    c["title"] = title;
    c["x_title"] = x_title;
    c["curve_name"] = curve_name;
    c["curve"] = pc_points(curve);
    c["ref_name"] = ref_name;
    c["ref"] = pc_points(ref);
    c["marker"] = QJsonArray{marker.x(), marker.y()};
    c["marker_name"] = marker_name;
    return c;
}

QJsonArray pc_array(const QVector<double>& v) {
    QJsonArray a;
    for (double d : v)
        a.append(std::isfinite(d) ? d : 0.0);
    return a;
}

QVector<double> pc_vec(const QJsonArray& a) {
    QVector<double> v;
    v.reserve(a.size());
    for (const QJsonValue& x : a)
        v.append(x.toDouble());
    return v;
}

struct PcOutcome {
    QJsonObject json;
    QString error;
};

template <typename T, typename ToJson>
PcOutcome pc_outcome(const Result<T>& r, ToJson&& to_json) {
    if (r.is_err())
        return {QJsonObject{}, QString::fromStdString(r.error())};
    return {to_json(r.value()), QString()};
}

int pc_freq(const QComboBox* c) {
    return c->currentIndex() == 0 ? 1 : c->currentIndex() == 1 ? 2 : 4;
}

} // namespace

// ── Calculation ──────────────────────────────────────────────────────────────

QJsonObject DerivPricerPage::compute_view(const QString& view, QString* error) const {
    QJsonObject result;
    QJsonObject chart;
    auto take = [&result, error](const PcOutcome& o) {
        if (!o.error.isEmpty()) {
            *error = o.error;
            return false;
        }
        result = o.json;
        return true;
    };

    if (view == QLatin1String("equity"))
        return compute_equity(error);
    if (view == QLatin1String("equity_iv"))
        return compute_equity_iv(error);

    if (view == QLatin1String("bond")) {
        const int freq = pc_freq(bond_freq_);
        const double ytm = bond_ytm_->value();
        const auto priced = dp::bond_price_from_ytm(bond_issue_->date(), bond_settle_->date(), bond_maturity_->date(),
                                                    bond_coupon_->value(), ytm, freq);
        if (!take(pc_outcome(priced, [ytm](const dp::BondPrice& b) {
                QJsonObject o;
                o["clean_price"] = pc_round(b.clean_price, 4);
                o["dirty_price"] = pc_round(b.dirty_price, 4);
                o["accrued_interest"] = pc_round(b.accrued_interest, 4);
                o["duration"] = pc_round(b.macaulay_duration, 4);
                o["modified_duration"] = pc_round(b.modified_duration, 4);
                o["convexity"] = pc_round(b.convexity, 4);
                o["remaining_coupons"] = b.remaining_coupons;
                o["next_coupon_date"] = b.next_coupon_date.toString(Qt::ISODate);
                o["ytm"] = pc_round(ytm, 4);
                return o;
            })))
            return {};
        // Clean price across yield ± 300 bp: the slope is duration, the bend convexity.
        const dp::BondPrice& b0 = priced.value();
        QVector<QPointF> curve, est;
        for (int k = -12; k <= 12; ++k) {
            const double y = ytm + 0.25 * k;
            const auto b = dp::bond_price_from_ytm(bond_issue_->date(), bond_settle_->date(), bond_maturity_->date(),
                                                   bond_coupon_->value(), y, freq);
            if (!b.is_ok())
                continue;
            curve.append({y, b.value().clean_price});
            est.append({y, b0.clean_price - b0.modified_duration * b0.dirty_price * (y - ytm) / 100.0});
        }
        chart = pc_chart(tr("CLEAN PRICE VS YIELD"), tr("Yield %"), tr("Clean price"), curve, tr("Duration estimate"),
                         est, QPointF(ytm, b0.clean_price), tr("Input yield"));
    } else if (view == QLatin1String("bond_ytm")) {
        if (!take(pc_outcome(dp::bond_ytm_from_clean_price(ytm_issue_->date(), ytm_settle_->date(), ytm_maturity_->date(),
                                                           ytm_coupon_->value(), bond_clean_price_->value(),
                                                           pc_freq(ytm_freq_)),
                             [](double y) {
                                 QJsonObject o;
                                 o["ytm"] = pc_round(y, 6);
                                 return o;
                             })))
            return {};
    } else if (view == QLatin1String("fx")) {
        const double spot = fx_spot_->value(), notional = fx_notional_->value(), strike = fx_strike_->value();
        const bool call = fx_type_->currentIndex() == 0;
        const auto priced = dp::fx_option(spot, strike, fx_time_->value(), fx_dom_->value(), fx_for_->value(),
                                          fx_vol_->value(), call, notional);
        if (!take(pc_outcome(priced, [spot, notional](const dp::FxOptionValue& v) {
                const auto& g = v.greeks;
                QJsonObject desk; // whole-notional desk units, from the unrounded per-unit Greeks
                desk["delta_foreign"] = pc_round(g.delta * notional, 2);
                desk["gamma_1pct_foreign"] = pc_round(g.gamma * spot * 0.01 * notional, 2);
                desk["vega_domestic"] = pc_round(g.vega * 0.01 * notional, 2);
                desk["theta_domestic"] = pc_round(g.theta * notional, 2);
                desk["rho_domestic"] = pc_round(g.rho * 0.01 * notional, 2);
                QJsonObject o;
                o["price"] = pc_round(v.premium, 4);
                o["price_per_unit"] = pc_round(v.premium_per_unit, 6);
                o["premium_pct_foreign"] = pc_round(v.premium_pct_foreign, 4);
                o["notional"] = notional;
                o["greeks"] = pc_greeks_json(g);
                o["greeks_notional"] = desk;
                return o;
            })))
            return {};
        result["currency"] = fx_ccy_->currentText();
        if (spot > 0) {
            QVector<QPointF> curve, intrinsic;
            for (int k = 0; k <= 40; ++k) {
                const double s = spot * (0.7 + 0.6 * k / 40.0);
                const auto v = dp::fx_option(s, strike, fx_time_->value(), fx_dom_->value(), fx_for_->value(),
                                             fx_vol_->value(), call, notional);
                if (!v.is_ok())
                    continue;
                curve.append({s, v.value().premium_per_unit});
                intrinsic.append({s, std::max(0.0, call ? s - strike : strike - s)});
            }
            chart = pc_chart(tr("PREMIUM PER UNIT VS SPOT"), tr("Spot"), tr("Premium"), curve, tr("Intrinsic value"),
                             intrinsic, QPointF(spot, priced.value().premium_per_unit), tr("Current spot"));
        }
    } else if (view == QLatin1String("swap")) {
        const int freq = pc_freq(swap_freq_);
        const double notional = swap_notional_->value(), disc = swap_disc_->value();
        const auto priced = dp::swap_value(swap_eff_->date(), swap_mat_->date(), swap_fixed_->value(), freq, notional, disc);
        if (!take(pc_outcome(priced, [notional](const dp::SwapValue& v) {
                QJsonObject o;
                o["swap_value"] = pc_round(v.value, 2);
                o["fixed_leg_pv"] = pc_round(v.fixed_leg_pv, 2);
                o["floating_leg_pv"] = pc_round(v.floating_leg_pv, 2);
                o["par_swap_rate"] = pc_round(v.par_rate_pct, 4);
                o["notional"] = notional;
                o["tenor_years"] = pc_round(v.tenor_years, 2);
                o["payments"] = v.payments;
                return o;
            })))
            return {};
        result["currency"] = swap_ccy_->currentText();
        QVector<QPointF> curve, zero;
        for (int k = -20; k <= 20; ++k) {
            const double r = disc + 0.1 * k;
            const auto v = dp::swap_value(swap_eff_->date(), swap_mat_->date(), swap_fixed_->value(), freq, notional, r);
            if (v.is_ok())
                curve.append({r, v.value().value});
        }
        if (curve.size() >= 2)
            zero = {QPointF(curve.first().x(), 0.0), QPointF(curve.last().x(), 0.0)};
        chart = pc_chart(tr("SWAP VALUE VS DISCOUNT RATE"), tr("Flat rate %"), tr("Swap value"), curve, tr("Zero value"),
                         zero, QPointF(disc, priced.value().value), tr("Input rate"));
    } else if (view == QLatin1String("cds")) {
        const double notional = cds_notional_->value(), coupon = cds_coupon_->value(), rate = cds_rate_->value();
        const double spread = cds_spread_->value();
        const auto priced = dp::cds_value(cds_val_->date(), cds_mat_->date(), cds_recovery_->value(), notional, spread,
                                          rate, coupon);
        if (!take(pc_outcome(priced, [notional, coupon, rate](const dp::CdsValue& v) {
                QJsonObject o;
                o["upfront_value"] = pc_round(v.upfront, 2);
                o["upfront_points"] = pc_round(v.upfront_points, 4);
                o["premium_leg_pv"] = pc_round(v.premium_leg_pv, 2);
                o["protection_leg_pv"] = pc_round(v.protection_leg_pv, 2);
                o["breakeven_spread_bps"] = pc_round(v.breakeven_spread_bps, 2);
                o["risky_pv01"] = pc_round(v.risky_pv01, 2);
                o["hazard_rate"] = pc_round(v.hazard_rate_pct, 4);
                o["survival_probability"] = pc_round(v.survival_pct, 2);
                o["notional"] = notional;
                o["coupon_bps"] = pc_round(coupon, 4);
                o["risk_free_rate"] = pc_round(rate, 4);
                o["premium_payments"] = v.premium_payments;
                o["tenor_years"] = pc_round(v.tenor_years, 4);
                return o;
            })))
            return {};
        result["currency"] = cds_ccy_->currentText();
        const double base = std::max({spread, coupon, 20.0});
        QVector<QPointF> curve, zero;
        for (int k = 0; k <= 40; ++k) {
            const double s = base * (0.05 + 1.95 * k / 40.0);
            const auto v = dp::cds_value(cds_val_->date(), cds_mat_->date(), cds_recovery_->value(), notional, s, rate,
                                         coupon);
            if (v.is_ok())
                curve.append({s, v.value().upfront});
        }
        if (curve.size() >= 2)
            zero = {QPointF(curve.first().x(), 0.0), QPointF(curve.last().x(), 0.0)};
        chart = pc_chart(tr("VALUE VS MARKET SPREAD"), tr("Spread bp"), tr("CDS value"), curve, tr("Zero value"), zero,
                         QPointF(spread, priced.value().upfront), tr("Input spread"));
    } else {
        *error = tr("Unknown calculator");
        return {};
    }
    QJsonObject payload;
    payload["result"] = result;
    if (!chart.isEmpty())
        payload["chart"] = chart;
    return payload;
}

QJsonObject DerivPricerPage::compute_equity(QString* error) const {
    const double S = opt_spot_->value(), K = opt_strike_->value(), T = opt_time_->value();
    const double r = opt_rate_->value() / 100.0, vol = opt_vol_->value() / 100.0;
    const bool call = opt_type_->currentIndex() == 0;
    const int payoff = opt_payoff_->currentIndex();
    const int model = payoff == 0 ? opt_model_->currentIndex() : 0;
    const bool american = payoff == 0 && opt_exercise_->currentIndex() == 1;
    const bool on_forward = payoff == 0 && (model == 3 || model == 4);
    const bool discrete =
        !on_forward && payoff == 0 && (model == 0 || model == 1 || model == 2) && opt_div_mode_->currentIndex() == 1;
    // Barone-Adesi-Whaley with discrete cash dividends: no such form exists — the tree prices it.
    const bool baw_tree = model == 2 && discrete;
    const double avg_so_far = opt_avg_so_far_->value(), avg_elapsed = opt_avg_elapsed_->value();
    const double q = (discrete || on_forward) ? 0.0 : opt_div_->value() / 100.0;
    QString div_error; // a dividend row that cannot be read is an input problem, never silently dropped
    const std::vector<md::CashDividend> divs = discrete ? dividends(&div_error) : std::vector<md::CashDividend>{};
    const double sigma_n = opt_normal_vol_->value();
    const int steps = opt_steps_->value();
    const double cash = opt_cash_->value(), H = opt_barrier_->value(), rebate = opt_rebate_->value();
    const double F2 = opt_f2_->value(), vol2 = opt_vol2_->value() / 100.0, rho = opt_rho_->value();
    const int bk = opt_barrier_kind_->currentIndex();
    const ex::BarrierKind bkind = bk == 0   ? ex::BarrierKind::DownOut
                                  : bk == 1 ? ex::BarrierKind::DownIn
                                  : bk == 2 ? ex::BarrierKind::UpOut
                                            : ex::BarrierKind::UpIn;

    if (!(T > 0)) {
        *error = tr("Time to expiry must be positive.");
        return {};
    }
    if (model != 4 && !(S > 0)) {
        *error = tr("The spot (or forward) price must be positive for this model; Bachelier accepts zero and negative forwards.");
        return {};
    }
    if (model != 4 && payoff != 6 && !(K > 0)) {
        *error = tr("The strike must be positive for this model.");
        return {};
    }
    if (payoff == 6 && !(F2 + K > 0)) {
        *error = tr("Kirk's approximation needs the second forward plus the strike to be positive.");
        return {};
    }
    if (payoff == 4 && (avg_elapsed > 0) != (avg_so_far > 0)) {
        *error = tr("For an average that has already started, give both the average so far and the years already "
                    "averaged (set both to 0 when the averaging starts today).");
        return {};
    }
    if (!div_error.isEmpty()) {
        *error = div_error;
        return {};
    }
    if (discrete && md::pv_dividends(divs, T, r) >= S) {
        *error = tr("The dividends are worth more than the spot price - check the amounts.");
        return {};
    }

    // Price as a function of spot, time, volatility and rate (the bumps and sweeps use it).
    auto price_of = [=](double s, double t, double v, double rr, int tree_steps) -> double {
        switch (payoff) {
            case 1:
                return ex::digital_cash_or_nothing(call, s, K, t, rr, q, v, cash);
            case 2:
                return ex::digital_asset_or_nothing(call, s, K, t, rr, q, v);
            case 3:
                return ex::barrier(bkind, call, s, K, H, rebate, t, rr, q, v);
            case 4:
                return ex::asian_arithmetic_tw_seasoned(call, s, K, t, rr, q, v, avg_so_far, avg_elapsed);
            case 5:
                return ex::asian_geometric(call, s, K, t, rr, q, v);
            case 6:
                return ex::spread_kirk(call, s, F2, K, t, rr, v, vol2, rho);
            default:
                break;
        }
        switch (model) {
            case 1:
                return md::crr_price(call, american, s, K, t, rr, q, v, tree_steps, divs).price;
            case 2:
                return baw_tree ? md::crr_price(call, true, s, K, t, rr, 0.0, v, tree_steps, divs).price
                                : md::baw_price(call, s, K, t, rr, q, v);
            case 3:
                return md::black76_price(call, s, K, t, rr, v);
            case 4:
                return md::bachelier_price(call, s, K, t, rr, v);
            default:
                return discrete ? md::bsm_price(call, s - md::pv_dividends(divs, t, rr), K, t, rr, 0.0, v)
                                : md::bsm_price(call, s, K, t, rr, q, v);
        }
    };
    const double v0 = model == 4 ? sigma_n : vol;
    const bool analytic = payoff == 0 && (model == 0 || model == 3 || model == 4);

    // Greeks: analytic where the model has them, else the tree's / finite differences.
    auto greeks_at = [&](double s, int tree_steps) {
        md::Greeks g;
        if (analytic) {
            if (model == 3)
                g = md::black76_full(call, s, K, T, r, vol);
            else if (model == 4)
                g = md::bachelier_full(call, s, K, T, r, sigma_n);
            else
                g = md::bsm_full(call, discrete ? s - md::pv_dividends(divs, T, r) : s, K, T, r, q, vol);
            return g;
        }
        g.price = price_of(s, T, v0, r, tree_steps);
        const double hs = std::max(std::fabs(s) * 1e-3, 1e-6);
        const double up = price_of(s + hs, T, v0, r, tree_steps), dn = price_of(s - hs, T, v0, r, tree_steps);
        g.delta = (up - dn) / (2 * hs);
        g.gamma = (up - 2 * g.price + dn) / (hs * hs);
        if (model == 1 && payoff == 0) {
            const md::TreeResult tr0 = md::crr_price(call, american, s, K, T, r, q, v0, tree_steps, divs);
            g.price = tr0.price;
            g.delta = tr0.delta;
            g.gamma = tr0.gamma;
            g.theta = tr0.theta;
        }
        const double hv = std::max(v0 * 1e-2, 1e-4);
        g.vega = (price_of(s, T, v0 + hv, r, tree_steps) - price_of(s, T, std::max(v0 - hv, 1e-6), r, tree_steps)) /
                 (v0 + hv - std::max(v0 - hv, 1e-6));
        if (!(model == 1 && payoff == 0)) {
            const double dt = std::min(1.0 / 365.0, 0.5 * T);
            g.theta = (price_of(s, T - dt, v0, r, tree_steps) - g.price) / dt;
        }
        g.rho = (price_of(s, T, v0, r + 1e-4, tree_steps) - price_of(s, T, v0, r - 1e-4, tree_steps)) / 2e-4;
        g.lambda = std::fabs(g.price) > 1e-12 ? g.delta * s / g.price : 0.0;
        g.valid = std::isfinite(g.price);
        return g;
    };

    const md::Greeks g = greeks_at(S, steps);
    if (!g.valid || !std::isfinite(g.price)) {
        *error = tr("The model could not price these inputs - check the volatility and the dates.");
        return {};
    }
    const double units = opt_qty_->value() * opt_mult_->value();
    const double df = std::exp(-r * T);
    const double intrinsic = payoff == 0 ? (on_forward ? df : 1.0) * std::max(call ? S - K : K - S, 0.0) : 0.0;
    double early = 0;
    if (american && (model == 1 || baw_tree))
        early = md::crr_price(call, true, S, K, T, r, q, vol, steps, divs).early_exercise_premium;
    else if (american && model == 2)
        early = std::max(0.0, g.price - md::bsm_price(call, S, K, T, r, q, vol));

    static const char* const kModels[] = {QT_TR_NOOP("Black-Scholes-Merton"), QT_TR_NOOP("Binomial (CRR)"),
                                          QT_TR_NOOP("Barone-Adesi-Whaley"), QT_TR_NOOP("Black-76"),
                                          QT_TR_NOOP("Bachelier")};
    QJsonObject res;
    res["kind"] = QStringLiteral("equity_v2");
    res["price"] = g.price;
    res["model"] = baw_tree       ? tr("Binomial (CRR) in place of Barone-Adesi-Whaley (discrete dividends)")
                   : payoff == 0  ? tr(kModels[model])
                                  : opt_payoff_->currentText();
    res["baw_tree"] = baw_tree;
    if (payoff == 4 && avg_elapsed > 0)
        res["asian_seasoned"] = true;
    res["exercise"] = american ? tr("American") : tr("European");
    res["call"] = call;
    res["currency"] = market_ccy_;
    res["intrinsic"] = intrinsic;
    res["time_value"] = g.price - intrinsic;
    res["early_premium"] = early;
    res["american"] = american;
    res["analytic"] = analytic;
    res["normal_model"] = model == 4 && payoff == 0;
    QJsonObject gr;
    gr["delta"] = g.delta;
    gr["gamma"] = g.gamma;
    gr["vega_pt"] = g.vega / 100.0;
    gr["theta_day"] = g.theta / 365.0;
    gr["rho_pct"] = g.rho / 100.0;
    gr["lambda"] = g.lambda;
    res["greeks"] = gr;
    if (analytic) {
        QJsonObject h;
        h["vanna"] = g.vanna / 100.0;  // Δ per vol point
        h["volga"] = g.volga / 1e4;    // vega (per pt) per vol point
        h["charm_day"] = g.charm / 365.0;
        h["speed"] = g.speed;
        h["colour_day"] = g.colour / 365.0;
        h["zomma"] = g.zomma / 100.0;
        h["ultima"] = g.ultima / 1e6;
        h["veta_day"] = g.veta / 100.0 / 365.0;
        h["dual_delta"] = g.dual_delta;
        h["dual_gamma"] = g.dual_gamma;
        res["higher"] = h;
    }
    QJsonObject dol;
    dol["units"] = units;
    dol["value"] = g.price * units;
    dol["delta"] = g.delta * S * units;
    dol["gamma_1pct"] = 0.5 * g.gamma * (0.01 * S) * (0.01 * S) * units;
    dol["vega_pt"] = g.vega / 100.0 * units;
    dol["theta_day"] = g.theta / 365.0 * units;
    res["dollar"] = dol;

    // Sweeps: value and Greeks against spot (±30 %), value against the time left.
    const int sweep_steps = std::min(steps, 200);
    QVector<double> xs, vals, intr, dl, gm, vg, th, va, vo, ch;
    for (int k = 0; k <= 40; ++k) {
        const double s = model == 4 ? S + (std::max(std::fabs(S), 1.0) * 0.6) * (k / 40.0 - 0.5)
                                    : S * (0.7 + 0.6 * k / 40.0);
        const md::Greeks gk = greeks_at(s, sweep_steps);
        xs.append(s);
        vals.append(gk.price);
        intr.append(payoff == 0 ? (on_forward ? df : 1.0) * std::max(call ? s - K : K - s, 0.0) : std::nan(""));
        dl.append(gk.delta);
        gm.append(gk.gamma);
        vg.append(gk.vega / 100.0);
        th.append(gk.theta / 365.0);
        va.append(gk.vanna / 100.0);
        vo.append(gk.volga / 1e4);
        ch.append(gk.charm / 365.0);
    }
    QVector<double> tx, tv;
    for (int k = 0; k <= 24; ++k) {
        const double t = T * (1.0 - k / 24.0);
        tx.append(t);
        tv.append(t > 1e-6 ? price_of(S, t, v0, r, sweep_steps)
                           : (payoff == 0 ? std::max(call ? S - K : K - S, 0.0) : price_of(S, 1e-6, v0, r, sweep_steps)));
    }
    QJsonObject c;
    c["kind"] = QStringLiteral("equity_v2");
    c["x"] = pc_array(xs);
    c["value"] = pc_array(vals);
    if (payoff == 0)
        c["intrinsic"] = pc_array(intr);
    c["delta"] = pc_array(dl);
    c["gamma"] = pc_array(gm);
    c["vega"] = pc_array(vg);
    c["theta"] = pc_array(th);
    if (analytic) {
        c["vanna"] = pc_array(va);
        c["volga"] = pc_array(vo);
        c["charm"] = pc_array(ch);
    }
    c["t_x"] = pc_array(tx);
    c["t_value"] = pc_array(tv);
    c["spot"] = S;
    c["price"] = g.price;
    c["years"] = T;
    QJsonObject payload;
    payload["result"] = res;
    payload["chart"] = c;
    return payload;
}

QJsonObject DerivPricerPage::compute_equity_iv(QString* error) const {
    const double price = iv_price_->value();
    if (price <= 0) {
        *error = tr("Enter the option's market price — implied volatility is solved from it.");
        return {};
    }
    const double S = iv_spot_->value(), K = iv_strike_->value(), T = iv_time_->value();
    const double r = iv_rate_->value() / 100.0, q = iv_div_->value() / 100.0;
    const bool call = iv_type_->currentIndex() == 0;
    const int model = iv_model_->currentIndex();
    md::VolSolve s;
    switch (model) {
        case 1:
            s = md::american_implied_vol(price, md::AmericanModel::Binomial, call, S, K, T, r, q, 400);
            break;
        case 2:
            s = md::american_implied_vol(price, md::AmericanModel::BaroneAdesiWhaley, call, S, K, T, r, q);
            break;
        case 3:
            s = md::black76_implied_vol(price, call, S, K, T, r);
            break;
        case 4:
            s = md::bachelier_implied_vol(price, call, S, K, T, r);
            break;
        default:
            s = md::bsm_implied_vol(price, call, S, K, T, r, q);
            break;
    }
    if (s.status != md::SolveStatus::Ok) {
        // Plain language, never NaN: say why no volatility reproduces the price.
        QString why;
        switch (s.status) {
            case md::SolveStatus::BelowIntrinsic:
                why = tr("the price is below the option's intrinsic value, so no volatility reproduces it");
                break;
            case md::SolveStatus::AboveMaximum:
                why = tr("the price is above the most the option can be worth, so no volatility reproduces it");
                break;
            case md::SolveStatus::InvalidInput:
                why = tr("spot, strike and time to expiry must be positive");
                break;
            default:
                why = tr("no volatility in the search range reproduces the price");
                break;
        }
        *error = tr("No implied volatility: %1.").arg(why);
        return {};
    }
    QJsonObject o;
    if (model == 4) {
        o["normal_vol"] = s.sigma;
    } else {
        o["implied_volatility"] = pc_round(s.sigma * 100, 4);
    }
    o["iv_model"] = iv_model_->currentText();
    QJsonObject payload;
    payload["result"] = o;
    return payload;
}

// ── Results ──────────────────────────────────────────────────────────────────

void DerivPricerPage::clear_results() {
    if (!results_body_ || !results_body_->layout())
        return;
    auto* layout = results_body_->layout();
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        if (QWidget* w = item->widget()) {
            w->hide(); // off screen now; deleted on the next event-loop turn
            w->deleteLater();
        }
        delete item;
    }
    last_chart_ = nullptr;
    chart_slot_ = nullptr;
    chart_pick_ = nullptr;
}

void DerivPricerPage::render_payload(const QJsonValue& payload) {
    const QJsonObject o = payload.toObject();
    display_results(o.value(QStringLiteral("result")).toObject(), o.value(QStringLiteral("chart")).toObject());
}

QWidget* DerivPricerPage::make_equity_chart(QWidget* parent, int which) const {
    const QJsonObject& c = last_charts_;
    const QVector<double> x = pc_vec(c.value(QStringLiteral("x")).toArray());
    auto pts = [&x](const QVector<double>& y) {
        QVector<QPointF> p;
        for (int i = 0; i < x.size() && i < y.size(); ++i)
            p.append({x[i], y[i]});
        return p;
    };
    const double spot = c.value(QStringLiteral("spot")).toDouble();
    static const char* const kKeys[] = {"value", "delta", "gamma", "vega", "theta", "vanna", "volga", "charm"};
    static const char* const kNames[] = {QT_TR_NOOP("Option value"), QT_TR_NOOP("Delta"), QT_TR_NOOP("Gamma"),
                                         QT_TR_NOOP("Vega per vol point"), QT_TR_NOOP("Theta per day"),
                                         QT_TR_NOOP("Vanna"), QT_TR_NOOP("Volga"), QT_TR_NOOP("Charm per day")};
    if (which == 8) { // value against the time left
        const QVector<double> tx = pc_vec(c.value(QStringLiteral("t_x")).toArray());
        const QVector<double> tv = pc_vec(c.value(QStringLiteral("t_value")).toArray());
        QVector<QPointF> p;
        for (int i = 0; i < tx.size() && i < tv.size(); ++i)
            p.append({tx[i], tv[i]});
        std::sort(p.begin(), p.end(), [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });
        return make_xy_chart(parent, tr("OPTION VALUE VS TIME TO EXPIRY"), tr("Years"), {{tr("Option value"), p}},
                             {{c.value(QStringLiteral("years")).toDouble(), c.value(QStringLiteral("price")).toDouble(),
                               tr("Now")}});
    }
    which = std::clamp(which, 0, 7);
    const QVector<double> y = pc_vec(c.value(QLatin1String(kKeys[which])).toArray());
    if (y.isEmpty())
        return make_xy_chart(parent, tr("OPTION VALUE VS SPOT"), tr("Spot"),
                             {{tr("Option value"), pts(pc_vec(c.value(QStringLiteral("value")).toArray()))}});
    QVector<LineSpec> lines = {{tr(kNames[which]), pts(y)}};
    QVector<std::tuple<double, double, QString>> marks;
    if (which == 0) {
        const QVector<double> intr = pc_vec(c.value(QStringLiteral("intrinsic")).toArray());
        if (!intr.isEmpty())
            lines.append({tr("Intrinsic value"), pts(intr), true, -1, true});
        marks.append({spot, c.value(QStringLiteral("price")).toDouble(), tr("Current spot")});
    }
    const QString title = which == 0 ? tr("OPTION VALUE VS SPOT") : tr("%1 VS SPOT").arg(tr(kNames[which]).toUpper());
    return make_xy_chart(parent, title, tr("Spot"), lines, marks);
}

void DerivPricerPage::display_results(const QJsonObject& result, const QJsonObject& chart) {
    clear_results();
    if (!results_body_)
        return;
    last_rows_.clear();
    last_result_ = result;
    last_charts_ = chart;
    last_title_ = instrument_name(active_);
    // Built hidden under its final parent, shown once complete.
    auto* body = new QWidget(results_body_);
    body->hide();
    auto* vl = new QVBoxLayout(body);
    vl->setContentsMargins(10, 10, 10, 10);
    vl->setSpacing(8);

    ui::KpiStrip* strip = nullptr;
    bool compact = false;
    auto begin_strip = [&](bool small) {
        strip = new ui::KpiStrip(body);
        compact = small;
        vl->addWidget(strip);
    };
    auto tile = [&](const QString& label, const QString& value, const QString& sub = QString()) {
        ui::StatTile* t = strip->add_tile(label);
        t->set_value(value);
        t->set_compact(compact);
        if (!sub.isEmpty())
            t->set_sub_label(sub);
        last_rows_.append({sub.isEmpty() ? label : label + QStringLiteral(" (") + sub + QLatin1Char(')'), value});
        return t;
    };
    auto signed_tile = [&](const QString& label, const QString& value, double v, const QString& sub) {
        ui::StatTile* t = tile(label, value, sub);
        t->set_tone(v > 0 ? ui::StatTile::Tone::Positive : v < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral);
        return t;
    };
    auto rv = [&result](const char* key, int decimals) { return num(result.value(QLatin1String(key)).toDouble(), decimals); };
    auto gv = [](const QJsonObject& o, const char* key, int decimals) {
        return num(o.value(QLatin1String(key)).toDouble(), decimals);
    };
    // Money in the calculator's own currency (results remembered before it was stored: USD).
    const QString res_ccy = result.value(QStringLiteral("currency")).toString(QStringLiteral("USD"));
    auto rm = [&result, &res_ccy](const char* key) { return money(result.value(QLatin1String(key)).toDouble(), res_ccy); };
    auto gm = [&res_ccy](const QJsonObject& o, const char* key) { return money(o.value(QLatin1String(key)).toDouble(), res_ccy); };
    QString note;
    QWidget* chart_widget = nullptr;

    if (result.value(QStringLiteral("kind")).toString() == QLatin1String("equity_v2")) {
        const QJsonObject g = result.value(QStringLiteral("greeks")).toObject();
        const QString ccy = result.value(QStringLiteral("currency")).toString(QStringLiteral("USD"));
        const bool normal = result.value(QStringLiteral("normal_model")).toBool();
        last_title_ = tr("%1 · %2 %3").arg(result.value(QStringLiteral("model")).toString(),
                                           result.value(QStringLiteral("exercise")).toString(),
                                           result.value(QStringLiteral("call")).toBool() ? tr("call") : tr("put"));
        begin_strip(false);
        tile(tr("OPTION PRICE"), rv("price", 4), tr("per share · %1").arg(result.value(QStringLiteral("model")).toString()));
        tile(tr("DELTA"), gv(g, "delta", 4), normal ? tr("per 1.0 forward") : tr("per 1.0 spot"));
        tile(tr("GAMMA"), gv(g, "gamma", 6), tr("per 1.0 spot"));
        tile(tr("VEGA"), gv(g, "vega_pt", 4), normal ? tr("per 1.0 normal vol") : tr("per vol point"));
        begin_strip(true);
        tile(tr("THETA"), gv(g, "theta_day", 4), tr("per calendar day"));
        tile(tr("RHO"), gv(g, "rho_pct", 4), tr("per 1% rate"));
        tile(tr("INTRINSIC"), rv("intrinsic", 4));
        tile(tr("TIME VALUE"), rv("time_value", 4));
        if (result.value(QStringLiteral("american")).toBool())
            tile(tr("EARLY-EXERCISE PREMIUM"), rv("early_premium", 4), tr("American − European"));
        tile(tr("ELASTICITY"), gv(g, "lambda", 2), tr("% per 1% spot"));
        const QJsonObject h = result.value(QStringLiteral("higher")).toObject();
        if (!h.isEmpty() && !normal) {
            begin_strip(true);
            tile(tr("VANNA"), gv(h, "vanna", 5), tr("delta per vol pt"));
            tile(tr("VOLGA"), gv(h, "volga", 5), tr("vega per vol pt"));
            tile(tr("CHARM"), gv(h, "charm_day", 5), tr("delta per day"));
            tile(tr("SPEED"), gv(h, "speed", 6), tr("gamma per 1.0 spot"));
            tile(tr("COLOUR"), gv(h, "colour_day", 6), tr("gamma per day"));
            tile(tr("ZOMMA"), gv(h, "zomma", 6), tr("gamma per vol pt"));
            tile(tr("ULTIMA"), gv(h, "ultima", 6), tr("volga per vol pt"));
            tile(tr("VETA"), gv(h, "veta_day", 5), tr("vega per day"));
            tile(tr("DUAL DELTA"), gv(h, "dual_delta", 4), tr("per 1.0 strike"));
        }
        const QJsonObject d = result.value(QStringLiteral("dollar")).toObject();
        begin_strip(true);
        const double units = d.value(QStringLiteral("units")).toDouble();
        signed_tile(tr("POSITION VALUE"), money(d.value(QStringLiteral("value")).toDouble(), ccy),
                    d.value(QStringLiteral("value")).toDouble(), tr("%1 units").arg(num(units, 0)));
        tile(tr("DOLLAR DELTA"), money(d.value(QStringLiteral("delta")).toDouble(), ccy), tr("delta × spot × units"));
        tile(tr("GAMMA P&L, 1% MOVE"), money(d.value(QStringLiteral("gamma_1pct")).toDouble(), ccy));
        tile(tr("VEGA"), money(d.value(QStringLiteral("vega_pt")).toDouble(), ccy), tr("per vol point"));
        tile(tr("THETA"), money(d.value(QStringLiteral("theta_day")).toDouble(), ccy), tr("per day"));

        // Chart picker: value / any Greek against spot, or value against time.
        auto* row = new QWidget(body);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* cap = new QLabel(tr("CHART"), row);
        cap->setObjectName(QStringLiteral("derivLabel"));
        auto* pick = make_combo(row, {tr("Value vs spot"), tr("Delta vs spot"), tr("Gamma vs spot"), tr("Vega vs spot"),
                                      tr("Theta vs spot"), tr("Vanna vs spot"), tr("Volga vs spot"), tr("Charm vs spot"),
                                      tr("Value vs time to expiry")});
        const bool has_higher = chart.contains(QStringLiteral("vanna"));
        if (!has_higher) {
            for (int i = 5; i <= 7; ++i)
                pick->setItemData(i, 0, Qt::UserRole - 1); // disabled: no closed form for this model
        }
        int view = chart_view_;
        if (!has_higher && view >= 5 && view <= 7)
            view = 0;
        pick->setCurrentIndex(view);
        cap->setBuddy(pick);
        rl->addWidget(cap);
        rl->addWidget(pick);
        rl->addStretch(1);
        vl->addWidget(row);
        auto* slot = new QWidget(body);
        auto* sl = new QVBoxLayout(slot);
        sl->setContentsMargins(0, 0, 0, 0);
        chart_widget = make_equity_chart(slot, view);
        sl->addWidget(chart_widget);
        vl->addWidget(slot, 1); // the chart takes the panel's spare height
        chart_slot_ = slot;
        chart_pick_ = pick;
        connect(pick, &QComboBox::currentIndexChanged, this, [this](int i) {
            chart_view_ = i;
            emit state_changed();
            if (!chart_slot_)
                return;
            auto* lay = chart_slot_->layout();
            while (lay->count() > 0) {
                auto* it = lay->takeAt(0);
                if (QWidget* w = it->widget()) {
                    w->hide();
                    w->deleteLater();
                }
                delete it;
            }
            QWidget* c = make_equity_chart(chart_slot_, i);
            lay->addWidget(c);
            last_chart_ = c;
        });

        note = result.value(QStringLiteral("american")).toBool()
                   ? tr("American exercise. Theta per calendar day, vega per volatility point, rho per 1% rate. "
                        "Dollar Greeks are for the position (contracts × multiplier).")
                   : tr("European exercise. Theta per calendar day, vega per volatility point, rho per 1% rate. "
                        "Higher-order Greeks are closed form where the model has them. Dollar Greeks are for the "
                        "position (contracts × multiplier).");
        if (result.value(QStringLiteral("baw_tree")).toBool())
            note += QLatin1Char(' ') +
                    tr("Barone-Adesi-Whaley has no form for discrete cash dividends: this price comes from the binomial "
                       "tree (escrowed dividends) instead.");
        if (result.value(QStringLiteral("asian_seasoned")).toBool())
            note += QLatin1Char(' ') + tr("Averaging already started: the remaining average is priced on the adjusted "
                                          "strike (Haug §4.20.2).");
        // Optional cross-check in QuantLib: never runs by itself, labelled with what it costs.
        QString ql_ep, ql_caveat;
        QJsonObject ql_body;
        double ql_scale = 1.0;
        if (quantlib_route(&ql_ep, &ql_body, &ql_scale, &ql_caveat)) {
            auto* qrow = new QWidget(body);
            auto* ql = new QHBoxLayout(qrow);
            ql->setContentsMargins(0, 0, 0, 0);
            const QString ep = ql_ep;
            const auto cost = services::quantlib_cost(services::quantlib_endpoint_tier(ep), services::quantlib_paid_plan());
            const QString cost_text = cost == services::QuantLibCost::NeedsPro    ? tr("needs a Pro plan")
                                      : cost == services::QuantLibCost::OneCredit ? tr("1 credit")
                                                                                  : tr("included in your plan");
            auto* qb = make_button(qrow, tr("CROSS-CHECK IN QUANTLIB · %1").arg(cost_text.toUpper()), false);
            qb->setProperty("fincept_agent_risk", "spend"); // agents: a QuantLib cross-check uses Fincept API credits
            qb->setToolTip(tr("Optional: price the same option with the QuantLib cloud (%1) and show the difference. "
                              "Your local result does not depend on it.%2")
                               .arg(ep, ql_caveat.isEmpty() ? QString() : QLatin1Char(' ') + ql_caveat));
            auto* qout = new QLabel(qrow);
            qout->setObjectName(QStringLiteral("derivNote"));
            qout->setWordWrap(true);
            ql->addWidget(qb);
            ql->addWidget(qout, 1);
            vl->addWidget(qrow);
            connect(qb, &QPushButton::clicked, this, [this, qb, qout] { run_quantlib_cross_check(qb, qout); });
        }
    } else if (result.contains(QStringLiteral("clean_price"))) {
        begin_strip(false);
        tile(tr("CLEAN PRICE"), rv("clean_price", 4), tr("per 100 face"));
        tile(tr("DIRTY PRICE"), rv("dirty_price", 4), tr("per 100 face"));
        tile(tr("YIELD TO MATURITY"), rv("ytm", 4) + QLatin1Char('%'), tr("coupon-frequency compounding"));
        tile(tr("MODIFIED DURATION"), rv("modified_duration", 4), tr("years"));
        begin_strip(true);
        tile(tr("ACCRUED"), rv("accrued_interest", 4), tr("per 100 face"));
        tile(tr("MACAULAY DURATION"), rv("duration", 4), tr("years"));
        tile(tr("CONVEXITY"), rv("convexity", 4), tr("years²"));
        if (result.contains(QStringLiteral("next_coupon_date"))) {
            tile(tr("NEXT COUPON"), result.value(QStringLiteral("next_coupon_date")).toString());
            tile(tr("COUPONS REMAINING"), QString::number(result.value(QStringLiteral("remaining_coupons")).toInt()));
        }
        note = tr("Price from a yield compounded at the coupon frequency; accrued interest actual/actual within "
                  "the coupon period. A 1bp yield rise changes the dirty price by about −modified duration × "
                  "dirty price × 0.0001.");
    } else if (result.contains(QStringLiteral("ytm"))) {
        begin_strip(false);
        tile(tr("YIELD TO MATURITY"), rv("ytm", 6) + QLatin1Char('%'), tr("solved from the clean price"));
        note = tr("Compounded at the coupon frequency, solved from the clean price.");
    } else if (result.contains(QStringLiteral("price")) && result.contains(QStringLiteral("notional"))) {
        begin_strip(false);
        tile(tr("PREMIUM"), rm("price"), tr("whole notional"));
        tile(tr("PER 1 FOREIGN UNIT"), rv("price_per_unit", 6), tr("domestic ccy"));
        tile(tr("% OF FOREIGN NOTIONAL"), rv("premium_pct_foreign", 4) + QLatin1Char('%'));
        const QJsonObject gn = result.value(QStringLiteral("greeks_notional")).toObject();
        if (!gn.isEmpty()) {
            begin_strip(true);
            tile(tr("DELTA"), gv(gn, "delta_foreign", 0), tr("foreign ccy"));
            tile(tr("GAMMA"), gv(gn, "gamma_1pct_foreign", 0), tr("foreign ccy / 1% spot"));
            tile(tr("VEGA"), gm(gn, "vega_domestic"), tr("per 1 vol pt"));
            tile(tr("THETA"), gm(gn, "theta_domestic"), tr("per day"));
            tile(tr("RHO (DOMESTIC)"), gm(gn, "rho_domestic"), tr("per 1% rate"));
        }
        const QJsonObject g = result.value(QStringLiteral("greeks")).toObject();
        note = tr("Garman-Kohlhagen, continuous rates. Per 1 unit of foreign notional: delta %1, gamma %2 (per 1.0 "
                  "spot move), vega %3, theta %4, rho %5.")
                   .arg(gv(g, "delta", 6), gv(g, "gamma", 6), gv(g, "vega", 6), gv(g, "theta", 6), gv(g, "rho", 6));
    } else if (result.contains(QStringLiteral("price")) && result.contains(QStringLiteral("greeks"))) {
        // A remembered result of the earlier calculator (re-priced on show).
        const QJsonObject greeks = result.value(QStringLiteral("greeks")).toObject();
        begin_strip(false);
        tile(tr("OPTION PRICE"), rv("price", 6), tr("per share, European"));
        tile(tr("DELTA"), gv(greeks, "delta", 6), tr("per 1.0 spot"));
        note = tr("Black-Scholes-Merton, European exercise.");
    } else if (result.contains(QStringLiteral("implied_volatility")) || result.contains(QStringLiteral("normal_vol"))) {
        begin_strip(false);
        if (result.contains(QStringLiteral("normal_vol")))
            tile(tr("IMPLIED NORMAL VOL"), rv("normal_vol", 4), tr("price units per year^0.5"));
        else
            tile(tr("IMPLIED VOLATILITY"), rv("implied_volatility", 4) + QLatin1Char('%'), tr("annualised"));
        note = tr("The volatility that reprices the market option price under %1 (bracketed Brent / rational solver; "
                  "no solution is reported as such).")
                   .arg(result.value(QStringLiteral("iv_model")).toString(tr("Black-Scholes-Merton")));
    } else if (result.contains(QStringLiteral("swap_value"))) {
        const double value = result.value(QStringLiteral("swap_value")).toDouble();
        begin_strip(false);
        signed_tile(tr("SWAP VALUE"), rm("swap_value"), value, tr("to the payer of fixed"));
        tile(tr("PAR SWAP RATE"), rv("par_swap_rate", 4) + QLatin1Char('%'));
        begin_strip(true);
        tile(tr("FIXED LEG PV"), rm("fixed_leg_pv"));
        tile(tr("FLOATING LEG PV"), rm("floating_leg_pv"));
        tile(tr("TENOR"), rv("tenor_years", 2), tr("years"));
        tile(tr("PAYMENTS"), QString::number(result.value(QStringLiteral("payments")).toInt()));
        note = tr("Valued at the effective date to the PAYER of fixed / receiver of floating, on a flat "
                  "continuously-compounded curve. Positive = the fixed rate is below the par rate.");
    } else if (result.contains(QStringLiteral("upfront_value"))) {
        const double value = result.value(QStringLiteral("upfront_value")).toDouble();
        begin_strip(false);
        signed_tile(tr("VALUE TO PROTECTION BUYER"), rm("upfront_value"), value, tr("protection − premium leg"));
        tile(tr("UPFRONT"), rv("upfront_points", 4), tr("points per 100"));
        tile(tr("PAR SPREAD"), rv("breakeven_spread_bps", 2) + QStringLiteral(" bp"));
        begin_strip(true);
        tile(tr("PREMIUM LEG PV"), rm("premium_leg_pv"));
        tile(tr("PROTECTION LEG PV"), rm("protection_leg_pv"));
        tile(tr("RISKY PV01"), rm("risky_pv01"), tr("per 1bp"));
        tile(tr("HAZARD RATE"), rv("hazard_rate", 4) + QLatin1Char('%'), tr("per year"));
        tile(tr("SURVIVAL TO MATURITY"), rv("survival_probability", 2) + QLatin1Char('%'));
        tile(tr("PREMIUM PAYMENTS"), QString::number(result.value(QStringLiteral("premium_payments")).toInt()));
        note = tr("Flat hazard rate calibrated to the market spread; quarterly premiums with accrual on default; "
                  "flat continuously-compounded discounting. Positive value = the protection buyer pays upfront.");
    } else {
        LOG_WARN("Derivatives", "Unrecognised result keys: " + result.keys().join(QStringLiteral(", ")));
        note = tr("Unrecognised result: press CALCULATE to price it again.");
    }

    // The generic sweep chart of the other calculators.
    if (!chart_widget && !chart.isEmpty() && chart.contains(QStringLiteral("curve"))) {
        const QJsonArray m = chart.value(QStringLiteral("marker")).toArray();
        QVector<LineSpec> lines = {{chart.value(QStringLiteral("curve_name")).toString(),
                                    pc_points_from(chart.value(QStringLiteral("curve")).toArray())}};
        const QVector<QPointF> ref = pc_points_from(chart.value(QStringLiteral("ref")).toArray());
        if (!ref.isEmpty())
            lines.append({chart.value(QStringLiteral("ref_name")).toString(), ref, true, -1, true});
        QVector<std::tuple<double, double, QString>> marks;
        if (m.size() == 2)
            marks.append({m.at(0).toDouble(), m.at(1).toDouble(), chart.value(QStringLiteral("marker_name")).toString()});
        chart_widget = make_xy_chart(body, chart.value(QStringLiteral("title")).toString(),
                                     chart.value(QStringLiteral("x_title")).toString(), lines, marks);
        vl->addWidget(chart_widget, 1); // the chart takes the panel's spare height
    }
    last_chart_ = chart_widget;
    if (!note.isEmpty()) {
        auto* n = make_note(body, note);
        n->setObjectName(QStringLiteral("derivResultNote"));
        vl->addWidget(n);
    }
    if (!chart_widget)
        vl->addStretch(1); // no chart: the tiles keep their height, the spare room stays below
    last_note_ = note;
    results_body_->layout()->addWidget(body);
    body->show();
}

bool DerivPricerPage::quantlib_route(QString* endpoint, QJsonObject* body, double* scale, QString* caveat) const {
    // Routes of the QuantLib cloud that price what this card prices (src/screens/quantlib route list;
    // request fields from the server's OpenAPI schema). Discrete dividends and the Asian / spread
    // payoffs have no matching route.
    const int payoff = opt_payoff_->currentIndex();
    const int model = opt_model_->currentIndex();
    const bool call = opt_type_->currentIndex() == 0;
    const bool american = payoff == 0 && opt_exercise_->currentIndex() == 1;
    const bool discrete = opt_div_mode_->currentIndex() == 1;
    *scale = 1.0;
    caveat->clear();
    QJsonObject b;
    b["strike"] = opt_strike_->value();
    b["risk_free_rate"] = opt_rate_->value() / 100.0;
    b["time_to_maturity"] = opt_time_->value();
    auto spot_fields = [&] {
        b["spot"] = opt_spot_->value();
        b["dividend_yield"] = opt_div_->value() / 100.0;
        b["volatility"] = opt_vol_->value() / 100.0;
    };
    if (payoff == 0) {
        if (discrete && model != 3 && model != 4)
            return false;
        b["option_type"] = call ? QStringLiteral("call") : QStringLiteral("put");
        if (model == 3) {
            b["forward"] = opt_spot_->value();
            b["volatility"] = opt_vol_->value() / 100.0;
            *endpoint = QStringLiteral("pricing/black76/greeks-full");
        } else if (model == 4) {
            b["forward"] = opt_spot_->value();
            b["normal_volatility"] = opt_normal_vol_->value();
            *endpoint = QStringLiteral("pricing/bachelier/greeks-full");
        } else if (model == 1 || american) {
            spot_fields();
            b["n_steps"] = opt_steps_->value();
            b["method"] = QStringLiteral("CRR");
            *endpoint = american ? QStringLiteral("pricing/binomial/american") : QStringLiteral("pricing/binomial/european");
            if (model == 2)
                *caveat = tr("QuantLib prices it on a binomial tree, not with Barone-Adesi-Whaley: expect a small gap.");
        } else {
            spot_fields();
            *endpoint = QStringLiteral("pricing/bs/greeks-full");
        }
    } else if (payoff == 1 || payoff == 2) {
        spot_fields();
        *endpoint = payoff == 1 ? (call ? QStringLiteral("pricing/bs/digital-call") : QStringLiteral("pricing/bs/digital-put"))
                                : (call ? QStringLiteral("pricing/bs/asset-or-nothing-call")
                                        : QStringLiteral("pricing/bs/asset-or-nothing-put"));
        if (payoff == 1) {
            *scale = opt_cash_->value(); // the route pays 1
            *caveat = tr("The route's digital pays 1; its price is multiplied by your cash payout.");
        }
    } else if (payoff == 3) {
        spot_fields();
        const int bk = opt_barrier_kind_->currentIndex();
        b["option_type"] = call ? QStringLiteral("call") : QStringLiteral("put");
        b["barrier"] = opt_barrier_->value();
        b["is_knock_in"] = bk == 1 || bk == 3;
        b["is_down"] = bk == 0 || bk == 1;
        b["rebate"] = opt_rebate_->value();
        b["n_steps"] = opt_steps_->value();
        b["method"] = QStringLiteral("CRR");
        *endpoint = QStringLiteral("pricing/binomial/barrier");
        *caveat = tr("QuantLib checks the barrier on a binomial tree (at each step), the local price monitors it "
                     "continuously: a gap of some cents is expected.");
    } else {
        return false;
    }
    *body = b;
    return true;
}

void DerivPricerPage::run_quantlib_cross_check(QPushButton* button, QLabel* out) {
    QString endpoint, caveat;
    QJsonObject body;
    double scale = 1.0;
    if (!quantlib_route(&endpoint, &body, &scale, &caveat)) {
        out->setText(tr("QuantLib has no route for these inputs."));
        return;
    }
    const double local = last_result_.value(QStringLiteral("price")).toDouble();
    button->setEnabled(false);
    out->setText(tr("Asking QuantLib…"));
    QPointer<QPushButton> b(button);
    QPointer<QLabel> o(out);
    LOG_INFO("Derivatives", QStringLiteral("QuantLib cross-check requested route=%1").arg(endpoint));
    services::QuantLibClient::instance().call(
        endpoint, body, [b, o, local, scale](mcp::ToolResult r) {
            if (b)
                b->setEnabled(true);
            if (!o)
                return;
            if (!r.success) {
                const auto f = services::classify_quantlib_failure(r);
                o->setText(f.kind == services::QuantLibFailureKind::NeedsPro
                               ? DerivPricerPage::tr("This cross-check needs a Pro plan - your local result is unaffected.")
                               : DerivPricerPage::tr("QuantLib did not answer (%1) - your local result is unaffected.")
                                     .arg(f.message));
                return;
            }
            // Find the price in whatever shape the route answers.
            double ql = std::nan("");
            const QJsonObject d = r.data.toObject();
            for (const char* k : {"price", "npv", "value", "option_price"}) {
                if (d.value(QLatin1String(k)).isDouble()) {
                    ql = d.value(QLatin1String(k)).toDouble();
                    break;
                }
            }
            if (!std::isfinite(ql)) {
                o->setText(DerivPricerPage::tr("QuantLib answered without a price field - your local result is unaffected."));
                return;
            }
            ql *= scale;
            o->setText(DerivPricerPage::tr("QuantLib price %1 · local %2 · difference %3")
                           .arg(num(ql, 6), num(local, 6), signed_num(ql - local, 6)));
        });
}

// ── Export ───────────────────────────────────────────────────────────────────

ui::ChartExport::Table DerivPricerPage::result_table() const {
    ui::ChartExport::Table t;
    t.headers = {tr("Figure"), tr("Value")};
    for (const auto& [k, v] : last_rows_)
        t.rows.append({k, v});
    return t;
}

report::ReportPayload DerivPricerPage::report_payload() const {
    report::ReportPayload p;
    p.title = tr("Derivatives pricer: %1").arg(last_title_);
    p.source = QStringLiteral("Derivatives");
    p.as_of = pane_ ? pane_->as_of() : QDateTime::currentDateTime();
    p.kpi_title = last_title_;
    for (const auto& [k, v] : last_rows_.mid(0, 12)) {
        report::PayloadKpi kpi;
        kpi.label = k;
        kpi.value = v;
        p.kpis.append(kpi);
    }
    report::PayloadTable table;
    table.title = tr("All figures");
    table.columns = {tr("Figure"), tr("Value")};
    for (const auto& [k, v] : last_rows_)
        table.rows.append({k, v});
    p.tables.append(table);
    if (last_chart_) {
        report::PayloadChart c;
        c.title = last_title_;
        c.image = png_bytes(ui::ChartExport::grab(last_chart_));
        if (!c.image.isEmpty())
            p.charts.append(c);
    }
    // Inputs echo: the visible card's numeric inputs.
    if (active_ == 1) {
        p.inputs = {{tr("Spot / forward"), num(opt_spot_->value(), 4)},
                    {tr("Strike"), num(opt_strike_->value(), 4)},
                    {tr("Years to expiry"), num(opt_time_->value(), 4)},
                    {tr("Volatility"), pct_points(opt_vol_->value())},
                    {tr("Rate (cont.)"), pct_points(opt_rate_->value())},
                    {tr("Dividend yield"), pct_points(opt_div_->value())},
                    {tr("Model"), opt_model_->currentText()},
                    {tr("Payoff"), opt_payoff_->currentText()}};
    }
    p.notes = last_note_;
    return p;
}

} // namespace fincept::screens::deriv
