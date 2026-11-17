// src/algo_engine/BacktestEngine.cpp
#include "algo_engine/BacktestEngine.h"

#include "algo_engine/ConditionEvaluator.h"
#include "algo_engine/PerformanceMetrics.h"
#include "core/logging/Logger.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <limits>

// Result keys (all money in the instrument's quote currency, all *_pct / returns
// in percent):
//   identity     success, symbol, venue, currency, timeframe, side, bar_seconds,
//                periods_per_year, ratio_basis ("daily" | "bar"), ratio_periods_per_year,
//                risk_free_rate, start_time, end_time, bars
//   returns      initial_capital, final_value, total_return, total_return_abs,
//                gross_return, cagr, cagr_extrapolated, benchmark_return,
//                benchmark_cagr, benchmark_max_drawdown, excess_return
//   risk         ann_volatility, sharpe_ratio (alias sharpe), sortino, calmar,
//                max_drawdown, max_drawdown_abs, max_dd_peak_time, max_dd_trough_time,
//                max_dd_recovery_time, max_dd_duration_bars, max_dd_duration_days,
//                exposure_pct, turnover, turnover_annual
//   costs        costs_applied, cost_model, total_fees, total_slippage,
//                total_borrow, total_costs, skipped_entries
//   trades       PerformanceMetrics::trade_stats_json keys + expectancy_r
//   series       equity_time, equity_curve, benchmark_curve, drawdown_curve,
//                monthly_returns [{month "yyyy-MM", return}], return_samples,
//                return_samples_kind ("daily" | "bar"), bars {t,o,h,l,c,v}
//   trades[]     index, side, entry_time, exit_time, entry_bar, exit_bar,
//                entry_ref, entry_price, exit_ref, exit_price, shares, gross_pnl,
//                fees, slippage, borrow, pnl (net), pnl_pct, mae_pct, mfe_pct,
//                bars_held, reason
//   warnings     [string] — translated notes the report shows prominently

namespace fincept::algo {

namespace {

// Trailing window passed to the indicator/condition evaluator each bar. Bounds
// per-bar cost to O(window) instead of O(i) — 500 bars is ample warm-up for any
// indicator in the catalog (largest typical period ~200).
constexpr int kBtEvalWindow = 500;
constexpr int kBtAtrPeriod = 14;
// Below this span CAGR / Calmar extrapolate a few weeks to a year.
constexpr double kBtMinDaysForAnnual = 60.0;

double bt_round(double v, int decimals) {
    if (!std::isfinite(v))
        return v;
    const double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

QJsonValue bt_num(double v, int decimals) {
    return std::isfinite(v) ? QJsonValue(bt_round(v, decimals)) : QJsonValue(QJsonValue::Null);
}

QString bt_tr(const char* text) {
    return QCoreApplication::translate("BacktestEngine", text);
}

// Wilder ATR, aligned with the candles (NaN before it is defined).
QVector<double> bt_atr_series(const QVector<OhlcvCandle>& c, int period) {
    const int n = c.size();
    QVector<double> out(n, std::numeric_limits<double>::quiet_NaN());
    if (n <= period)
        return out;
    double sum = 0;
    for (int i = 1; i <= period; ++i) {
        const double tr = std::max({c[i].high - c[i].low, std::abs(c[i].high - c[i - 1].close),
                                    std::abs(c[i].low - c[i - 1].close)});
        sum += tr;
    }
    out[period] = sum / period;
    for (int i = period + 1; i < n; ++i) {
        const double tr = std::max({c[i].high - c[i].low, std::abs(c[i].high - c[i - 1].close),
                                    std::abs(c[i].low - c[i - 1].close)});
        out[i] = (out[i - 1] * (period - 1) + tr) / period;
    }
    return out;
}

struct BtPosition {
    bool open = false;
    bool is_long = true;
    double qty = 0;
    double entry_ref = 0;
    double entry_px = 0;
    double entry_fees = 0;
    double entry_slip = 0;
    qint64 entry_ms = 0;
    int entry_bar = 0;
    double hi = 0; // trailing-stop extreme (long: highest, short: lowest) before the current bar
    double lo = 0;
    double best = 0;  // most favourable price reached
    double worst = 0; // most adverse price reached
};

} // namespace

QJsonObject BacktestEngine::run(const QVector<OhlcvCandle>& candles, const QJsonArray& entry_conditions,
                                const QString& entry_logic, const QJsonArray& exit_conditions,
                                const QString& exit_logic, double stop_loss_pct, double take_profit_pct,
                                double trailing_stop_pct, double initial_capital, const QString& timeframe,
                                double position_size_pct) {
    BacktestConfig cfg;
    cfg.entry_conditions = entry_conditions;
    cfg.entry_logic = entry_logic;
    cfg.exit_conditions = exit_conditions;
    cfg.exit_logic = exit_logic;
    cfg.stop_loss_pct = stop_loss_pct;
    cfg.take_profit_pct = take_profit_pct;
    cfg.trailing_stop_pct = trailing_stop_pct;
    cfg.initial_capital = initial_capital;
    cfg.timeframe = timeframe;
    cfg.position_size_pct = position_size_pct;
    return run(candles, cfg);
}

QJsonObject BacktestEngine::run(const QVector<OhlcvCandle>& candles, const BacktestConfig& cfg) {
    const int n = candles.size();
    const int warm = std::max(1, cfg.warmup_bars);
    if (n < warm + 10) {
        QJsonObject err;
        err["success"] = false;
        err["error"] =
            bt_tr("Insufficient data: %1 bars (need at least %2 — %3 for indicator warm-up plus 10 to trade).")
                .arg(n)
                .arg(warm + 10)
                .arg(warm);
        return err;
    }
    if (!(cfg.initial_capital > 0)) {
        QJsonObject err;
        err["success"] = false;
        err["error"] = bt_tr("Initial capital must be positive.");
        return err;
    }

    const bool short_side = cfg.side.compare(QStringLiteral("SELL"), Qt::CaseInsensitive) == 0;
    const double size_frac = std::clamp(cfg.position_size_pct, 1.0, 100.0) / 100.0;
    const CostModel& costs = cfg.costs;
    const int bar_secs = timeframe_seconds(timeframe_from_string(cfg.timeframe));
    const bool daily_bars = bar_secs >= 86400;
    const int64_t bar_ms = static_cast<int64_t>(bar_secs) * 1000;

    LOG_DEBUG("Backtest", QString("run: %1 bars tf=%2 side=%3 entry=%4 exit=%5 sl=%6 tp=%7 trail=%8 size=%9% costs=%10")
                              .arg(n)
                              .arg(cfg.timeframe, short_side ? QStringLiteral("SHORT") : QStringLiteral("LONG"))
                              .arg(cfg.entry_conditions.size())
                              .arg(cfg.exit_conditions.size())
                              .arg(cfg.stop_loss_pct)
                              .arg(cfg.take_profit_pct)
                              .arg(cfg.trailing_stop_pct)
                              .arg(cfg.position_size_pct)
                              .arg(costs.preset));

    const QVector<double> atr = bt_atr_series(candles, kBtAtrPeriod);
    auto atr_before = [&atr](int i) { return i > 0 ? atr[i - 1] : std::numeric_limits<double>::quiet_NaN(); };
    // Equity is marked on the bar CLOSE; intraday points are stamped at the bar's
    // end so the curve does not lead the price by one bar. Daily bars keep their
    // date stamp (the open of the trading day).
    auto mark_time = [&](const OhlcvCandle& b) -> qint64 {
        if (daily_bars || bar_ms <= 0)
            return b.open_time;
        const int64_t end = b.close_time > b.open_time && b.close_time <= b.open_time + 2 * bar_ms
                                ? b.close_time
                                : b.open_time + bar_ms;
        return end;
    };

    // ── State ────────────────────────────────────────────────────────────────
    double cash = cfg.initial_capital;
    BtPosition pos;
    bool entry_signal = false;
    bool exit_signal = false;
    int skipped_entries = 0;
    double skip_px = 0, skip_budget = 0; // first skipped entry, for the warning

    double total_fees = 0, total_slip = 0, total_borrow = 0, traded_notional = 0;
    int bars_in_market = 0;
    int entry_eval_count = 0, entry_true_count = 0, entry_err_count = 0;
    QString last_entry_err;

    QJsonArray trades_json;
    QVector<perf::ClosedTrade> closed;
    QVector<perf::Point> curve;
    QVector<double> bench_curve;
    curve.reserve(n - warm);
    bench_curve.reserve(n - warm);

    // Buy & hold benchmark: bought at the first evaluated bar's open with the same
    // costs (fractional, so lot rounding does not distort it), sold at the end.
    double bench_qty = 0, bench_cash = 0;
    {
        const OhlcvCandle& b0 = candles[warm];
        const double px = execution::slipped_price(costs, FillSide::Buy, b0.open, atr_before(warm));
        if (px > 0) {
            const double fee = execution::fees(costs, FillSide::Buy, cfg.initial_capital / px, px).total();
            bench_qty = std::max(0.0, (cfg.initial_capital - fee) / px);
            bench_cash = cfg.initial_capital - fee - bench_qty * px;
        }
    }

    auto close_position = [&](double ref, bool slip, const QString& reason, int bar_idx, double atr_v) {
        const FillSide fs = pos.is_long ? FillSide::Sell : FillSide::Buy;
        const Fill f = execution::fill(costs, fs, ref, pos.qty, atr_v, slip);
        const qint64 exit_ms = candles[bar_idx].open_time;
        double borrow = 0;
        if (!pos.is_long) {
            const double days = std::max(0.0, (exit_ms - pos.entry_ms) / 86400000.0);
            borrow = execution::borrow_cost(costs, pos.entry_px * pos.qty, days);
        }
        const double exit_fees = f.fees.total();
        if (pos.is_long)
            cash += f.notional - exit_fees;
        else
            cash -= f.notional + exit_fees + borrow;

        const double dir = pos.is_long ? 1.0 : -1.0;
        const double gross = (f.ref_price - pos.entry_ref) * pos.qty * dir;
        const double net = (f.price - pos.entry_px) * pos.qty * dir - pos.entry_fees - exit_fees - borrow;
        const double fees_all = pos.entry_fees + exit_fees;
        const double slip_all = pos.entry_slip + f.slippage_cost;
        const double entry_notional = pos.entry_px * pos.qty;
        // Excursions bounded by the exit fill (the part of the exit bar after it is not held).
        const double best = pos.is_long ? std::max(pos.best, f.price) : std::min(pos.best, f.price);
        const double worst = pos.is_long ? std::min(pos.worst, f.price) : std::max(pos.worst, f.price);
        const double mae = pos.entry_px > 0 ? (worst - pos.entry_px) / pos.entry_px * 100.0 * dir : 0.0;
        const double mfe = pos.entry_px > 0 ? (best - pos.entry_px) / pos.entry_px * 100.0 * dir : 0.0;

        total_fees += exit_fees;
        total_slip += f.slippage_cost;
        total_borrow += borrow;
        traded_notional += f.notional;

        perf::ClosedTrade ct;
        ct.entry_ms = pos.entry_ms;
        ct.exit_ms = exit_ms;
        ct.net_pnl = net;
        ct.gross_pnl = gross;
        ct.fees = fees_all;
        ct.ret_pct = entry_notional > 0 ? net / entry_notional * 100.0 : 0.0;
        ct.bars_held = bar_idx - pos.entry_bar;
        ct.mae_pct = std::min(0.0, mae);
        ct.mfe_pct = std::max(0.0, mfe);
        closed.append(ct);

        QJsonObject t;
        t["index"] = closed.size();
        t["side"] = pos.is_long ? QStringLiteral("LONG") : QStringLiteral("SHORT");
        t["entry_time"] = static_cast<double>(pos.entry_ms);
        t["exit_time"] = static_cast<double>(exit_ms);
        t["entry_bar"] = pos.entry_bar;
        t["exit_bar"] = bar_idx;
        t["entry_ref"] = bt_round(pos.entry_ref, 6);
        t["entry_price"] = bt_round(pos.entry_px, 6);
        t["exit_ref"] = bt_round(f.ref_price, 6);
        t["exit_price"] = bt_round(f.price, 6);
        t["shares"] = pos.qty;
        t["gross_pnl"] = bt_round(gross, 2);
        t["fees"] = bt_round(fees_all, 2);
        t["slippage"] = bt_round(slip_all, 2);
        t["borrow"] = bt_round(borrow, 2);
        t["pnl"] = bt_round(net, 2);
        t["pnl_pct"] = bt_round(ct.ret_pct, 3);
        t["mae_pct"] = bt_round(ct.mae_pct, 3);
        t["mfe_pct"] = bt_round(ct.mfe_pct, 3);
        t["bars_held"] = ct.bars_held;
        t["reason"] = reason;
        trades_json.append(t);
        pos = BtPosition{};
    };

    const int total_steps = n - warm;
    int last_progress = -1;

    for (int i = warm; i < n; ++i) {
        if (cfg.cancel && cfg.cancel->load(std::memory_order_relaxed)) {
            QJsonObject err;
            err["success"] = false;
            err["cancelled"] = true;
            err["error"] = bt_tr("Backtest cancelled.");
            return err;
        }
        if (cfg.progress) {
            const int pct = static_cast<int>(100.0 * (i - warm) / std::max(1, total_steps));
            if (pct != last_progress && (pct - last_progress >= 2 || pct == 100)) {
                last_progress = pct;
                cfg.progress(pct);
            }
        }

        const OhlcvCandle& bar = candles[i];
        const double atr_v = atr_before(i);

        // ── 1. Pending signal fills at THIS bar's open ──────────────────────
        if (!pos.open && entry_signal) {
            entry_signal = false;
            const FillSide fs = short_side ? FillSide::Sell : FillSide::Buy;
            const double px = execution::slipped_price(costs, fs, bar.open, atr_v);
            const double budget = cash * size_frac;
            const double qty = execution::max_affordable_qty(costs, fs, budget, px);
            if (qty <= 0) {
                if (skipped_entries++ == 0) {
                    skip_px = px;
                    skip_budget = budget;
                }
            } else {
                const Fill f = execution::fill(costs, fs, bar.open, qty, atr_v, true);
                pos.open = true;
                pos.is_long = !short_side;
                pos.qty = f.qty;
                pos.entry_ref = f.ref_price;
                pos.entry_px = f.price;
                pos.entry_fees = f.fees.total();
                pos.entry_slip = f.slippage_cost;
                pos.entry_ms = bar.open_time;
                pos.entry_bar = i;
                pos.hi = pos.lo = f.price;
                pos.best = pos.worst = f.price;
                if (pos.is_long)
                    cash -= f.notional + f.fees.total();
                else
                    cash += f.notional - f.fees.total();
                total_fees += f.fees.total();
                total_slip += f.slippage_cost;
                traded_notional += f.notional;
            }
        } else if (pos.open && exit_signal) {
            exit_signal = false;
            close_position(bar.open, true, QStringLiteral("exit_signal"), i, atr_v);
        }

        // ── 2. Resting stop / target orders against THIS bar's range ────────
        if (pos.open) {
            const bool lng = pos.is_long;
            bool have_stop = false;
            double stop_level = 0;
            bool trailing_binds = false;
            if (cfg.stop_loss_pct > 0) {
                stop_level = lng ? pos.entry_px * (1.0 - cfg.stop_loss_pct / 100.0)
                                 : pos.entry_px * (1.0 + cfg.stop_loss_pct / 100.0);
                have_stop = true;
            }
            if (cfg.trailing_stop_pct > 0) {
                const double trail = lng ? pos.hi * (1.0 - cfg.trailing_stop_pct / 100.0)
                                         : pos.lo * (1.0 + cfg.trailing_stop_pct / 100.0);
                if (!have_stop || (lng ? trail > stop_level : trail < stop_level)) {
                    stop_level = trail;
                    trailing_binds = true;
                }
                have_stop = true;
            }
            const bool have_tp = cfg.take_profit_pct > 0;
            const double tp_level = lng ? pos.entry_px * (1.0 + cfg.take_profit_pct / 100.0)
                                        : pos.entry_px * (1.0 - cfg.take_profit_pct / 100.0);

            // Stop checked first (conservative when both are touched in one bar).
            const bool stop_hit = have_stop && (lng ? bar.low <= stop_level : bar.high >= stop_level);
            const bool tp_hit = have_tp && (lng ? bar.high >= tp_level : bar.low <= tp_level);
            if (stop_hit) {
                // A bar that gaps through the stop fills at its open, not the (better) level.
                // (The exit bar's path before the fill is unknown: excursions are bounded by
                // the fill itself in close_position.)
                const double ref = lng ? std::min(stop_level, bar.open) : std::max(stop_level, bar.open);
                close_position(ref, true, trailing_binds ? QStringLiteral("trailing_stop") : QStringLiteral("stop_loss"),
                               i, atr_v);
            } else if (tp_hit) {
                const double ref = lng ? std::max(tp_level, bar.open) : std::min(tp_level, bar.open);
                close_position(ref, false, QStringLiteral("take_profit"), i, atr_v);
            } else {
                // The whole bar was held: update excursions and the trailing extreme.
                if (lng) {
                    pos.best = std::max(pos.best, bar.high);
                    pos.worst = std::min(pos.worst, bar.low);
                } else {
                    pos.best = std::min(pos.best, bar.low);
                    pos.worst = std::max(pos.worst, bar.high);
                }
                pos.hi = std::max(pos.hi, bar.high);
                pos.lo = std::min(pos.lo, bar.low);
            }
        }

        // ── 3. Evaluate conditions on the close of bar i → latch for i+1 ────
        const int start = std::max(0, i - kBtEvalWindow + 1);
        const QVector<OhlcvCandle> window = candles.mid(start, i - start + 1);
        if (!pos.open && !entry_signal && !cfg.entry_conditions.isEmpty() && i < n - 1) {
            const auto g = ConditionEvaluator::evaluate_group(cfg.entry_conditions, cfg.entry_logic, window);
            ++entry_eval_count;
            if (g.triggered) {
                entry_signal = true;
                ++entry_true_count;
            }
            for (const auto& d : g.details)
                if (!d.error.isEmpty()) {
                    ++entry_err_count;
                    if (last_entry_err.isEmpty())
                        last_entry_err = d.error;
                }
        } else if (pos.open && !exit_signal && !cfg.exit_conditions.isEmpty() && i < n - 1) {
            if (ConditionEvaluator::evaluate_group(cfg.exit_conditions, cfg.exit_logic, window).triggered)
                exit_signal = true;
        }

        // ── 4. Mark to market on the close ──────────────────────────────────
        double equity = cash;
        if (pos.open) {
            equity += pos.is_long ? pos.qty * bar.close : -pos.qty * bar.close;
            ++bars_in_market;
        }
        perf::Point pt;
        pt.t_ms = mark_time(bar);
        pt.v = equity;
        pt.day = bar.session_date;
        curve.append(pt);
        bench_curve.append(bench_cash + bench_qty * bar.close);
    }

    // Close a position still open at the last close (costs included).
    bool closed_at_end = false;
    if (pos.open) {
        closed_at_end = true;
        close_position(candles[n - 1].close, true, QStringLiteral("end_of_data"), n - 1, atr[n - 1]);
        curve.last().v = cash;
    }
    if (cfg.progress)
        cfg.progress(100);

    LOG_DEBUG("Backtest", QString("done: evalBars=%1 entryTrue=%2 entryErr=%3 trades=%4 skipped=%5 lastErr='%6'")
                              .arg(entry_eval_count)
                              .arg(entry_true_count)
                              .arg(entry_err_count)
                              .arg(closed.size())
                              .arg(skipped_entries)
                              .arg(last_entry_err));

    // ── Metrics (shared definitions, full-resolution curve) ─────────────────
    const double final_value = cash;
    const double net_profit = final_value - cfg.initial_capital;
    const double total_costs = total_fees + total_slip + total_borrow;
    const double ppy = cfg.periods_per_year > 0 ? cfg.periods_per_year : perf::estimate_periods_per_year(curve);
    const auto rets = perf::period_returns(curve);
    // Sharpe / Sortino / volatility: daily returns (× √trading days) for an intraday run of at
    // least 20 trading days, the bar returns otherwise (see PerformanceMetrics::ratio_basis).
    const perf::RatioBasis basis = perf::ratio_basis(curve, !daily_bars, ppy, cfg.trading_days_per_year);
    const auto rat = perf::ratios(basis.returns, basis.periods_per_year, cfg.risk_free_pct);
    const auto dd = perf::drawdown(curve);
    const auto ts = perf::trade_stats(closed);
    const qint64 t0 = candles[warm].open_time;
    const qint64 t1 = curve.isEmpty() ? t0 : curve.last().t_ms;
    const double span_days = (t1 - t0) / 86400000.0;
    const double cagr = perf::cagr_pct(cfg.initial_capital, final_value, t0, t1);
    const double calmar = (std::isfinite(cagr) && dd.max_pct > 0) ? cagr / dd.max_pct : 0.0;

    double bench_final = bench_curve.isEmpty() ? cfg.initial_capital : bench_curve.last();
    if (bench_qty > 0)
        bench_final -= execution::fees(costs, FillSide::Sell, bench_qty, candles[n - 1].close).total();
    QVector<perf::Point> bench_pts;
    bench_pts.reserve(bench_curve.size());
    for (int j = 0; j < bench_curve.size(); ++j)
        bench_pts.append({curve[j].t_ms, bench_curve[j], curve[j].day});
    const double bench_ret = (bench_final / cfg.initial_capital - 1.0) * 100.0;
    const double bench_cagr = perf::cagr_pct(cfg.initial_capital, bench_final, t0, t1);
    const auto bench_dd = perf::drawdown(bench_pts);

    double mean_equity = 0;
    for (const auto& p : curve)
        mean_equity += p.v;
    mean_equity = curve.isEmpty() ? cfg.initial_capital : mean_equity / curve.size();
    const double turnover = mean_equity > 0 ? traded_notional / mean_equity : 0.0;
    const double years = perf::years_between(t0, t1);

    QJsonObject out;
    out["success"] = true;
    out["symbol"] = cfg.symbol;
    out["venue"] = cfg.venue;
    out["currency"] = cfg.currency;
    out["timeframe"] = cfg.timeframe;
    out["side"] = short_side ? QStringLiteral("SHORT") : QStringLiteral("LONG");
    out["bar_seconds"] = bar_secs;
    out["periods_per_year"] = bt_round(ppy, 2);
    out["ratio_basis"] = basis.daily ? QStringLiteral("daily") : QStringLiteral("bar");
    out["ratio_periods_per_year"] = bt_round(basis.periods_per_year, 2);
    out["risk_free_rate"] = cfg.risk_free_pct;
    out["start_time"] = static_cast<double>(t0);
    out["end_time"] = static_cast<double>(t1);
    out["bars"] = static_cast<int>(curve.size());

    out["initial_capital"] = cfg.initial_capital;
    out["final_value"] = bt_round(final_value, 2);
    out["total_return"] = bt_round(net_profit / cfg.initial_capital * 100.0, 3);
    out["total_return_abs"] = bt_round(net_profit, 2);
    out["gross_return"] = bt_round((net_profit + total_costs) / cfg.initial_capital * 100.0, 3);
    out["gross_profit_abs"] = bt_round(net_profit + total_costs, 2);
    out["cagr"] = bt_num(cagr, 3);
    out["cagr_extrapolated"] = span_days < kBtMinDaysForAnnual;
    out["benchmark_return"] = bt_round(bench_ret, 3);
    out["benchmark_cagr"] = bt_num(bench_cagr, 3);
    out["benchmark_max_drawdown"] = bt_round(bench_dd.max_pct, 3);
    out["excess_return"] = bt_round(net_profit / cfg.initial_capital * 100.0 - bench_ret, 3);

    out["ann_volatility"] = bt_round(rat.ann_vol_pct, 3);
    out["sharpe_ratio"] = bt_round(rat.sharpe, 3);
    out["sharpe"] = bt_round(rat.sharpe, 3);
    out["sortino"] = bt_round(rat.sortino, 3);
    out["calmar"] = bt_round(calmar, 3);
    out["max_drawdown"] = bt_round(dd.max_pct, 3);
    out["max_drawdown_abs"] = bt_round(dd.max_abs, 2);
    out["max_dd_peak_time"] = dd.peak >= 0 ? static_cast<double>(curve[dd.peak].t_ms) : 0.0;
    out["max_dd_trough_time"] = dd.trough >= 0 ? static_cast<double>(curve[dd.trough].t_ms) : 0.0;
    out["max_dd_recovery_time"] = dd.recovery >= 0 ? static_cast<double>(curve[dd.recovery].t_ms) : 0.0;
    out["max_dd_duration_bars"] = dd.longest_points;
    out["max_dd_duration_days"] = bt_round(dd.longest_days, 2);
    out["exposure_pct"] = bt_round(curve.isEmpty() ? 0.0 : 100.0 * bars_in_market / curve.size(), 2);
    out["turnover"] = bt_round(turnover, 3);
    out["turnover_annual"] = bt_round(years > 0 ? turnover / years : 0.0, 3);

    out["costs_applied"] = !costs.is_zero();
    out["cost_model"] = costs.to_json();
    out["total_fees"] = bt_round(total_fees, 2);
    out["total_slippage"] = bt_round(total_slip, 2);
    out["total_borrow"] = bt_round(total_borrow, 2);
    out["total_costs"] = bt_round(total_costs, 2);
    out["skipped_entries"] = skipped_entries;

    const QJsonObject tsj = perf::trade_stats_json(ts);
    for (auto it = tsj.begin(); it != tsj.end(); ++it)
        out[it.key()] = it.value();
    out["expectancy_r"] = bt_round(ts.avg_loss > 0 ? ts.expectancy / ts.avg_loss : 0.0, 3);

    // ── Series ───────────────────────────────────────────────────────────────
    const QVector<double> uw = perf::underwater_pct(curve);
    QJsonArray eq_t, eq_v, bench_v, dd_v;
    for (int j = 0; j < curve.size(); ++j) {
        eq_t.append(static_cast<double>(curve[j].t_ms));
        eq_v.append(bt_round(curve[j].v, 2));
        bench_v.append(bt_round(bench_curve[j], 2));
        dd_v.append(bt_round(uw[j], 3));
    }
    out["equity_time"] = eq_t;
    out["equity_curve"] = eq_v;
    out["benchmark_curve"] = bench_v;
    out["drawdown_curve"] = dd_v;

    QJsonArray monthly;
    for (const auto& m : perf::monthly_returns(curve, cfg.initial_capital)) {
        QJsonObject mo;
        mo["month"] = QStringLiteral("%1-%2").arg(m.first / 100, 4, 10, QLatin1Char('0')).arg(m.first % 100, 2, 10,
                                                                                               QLatin1Char('0'));
        mo["return"] = bt_round(m.second, 3);
        monthly.append(mo);
    }
    out["monthly_returns"] = monthly;

    // Return distribution: daily returns when there are enough days, else bar returns.
    const auto daily = perf::daily_close(curve);
    QJsonArray samples;
    if (!daily_bars && daily.size() >= 20) {
        for (const double r : perf::period_returns(daily))
            samples.append(bt_round(r * 100.0, 4));
        out["return_samples_kind"] = QStringLiteral("daily");
    } else {
        for (const double r : rets)
            samples.append(bt_round(r * 100.0, 4));
        out["return_samples_kind"] = daily_bars ? QStringLiteral("daily") : QStringLiteral("bar");
    }
    out["return_samples"] = samples;

    if (cfg.include_bars) {
        QJsonArray bt, bo, bh, bl, bc, bv;
        for (int i = warm; i < n; ++i) {
            const auto& c = candles[i];
            bt.append(static_cast<double>(c.open_time));
            bo.append(c.open);
            bh.append(c.high);
            bl.append(c.low);
            bc.append(c.close);
            bv.append(c.volume);
        }
        QJsonObject bars;
        bars["t"] = bt;
        bars["o"] = bo;
        bars["h"] = bh;
        bars["l"] = bl;
        bars["c"] = bc;
        bars["v"] = bv;
        out["price_bars"] = bars;
    }
    out["trades"] = trades_json;

    // ── Warnings the report must show ───────────────────────────────────────
    QJsonArray warnings;
    if (costs.is_zero())
        warnings.append(bt_tr("No costs applied: results are GROSS (no commission, taxes or slippage) and overstate "
                              "what a live account would earn."));
    if (skipped_entries > 0)
        warnings.append(bt_tr("%1 entry signal(s) could not be filled: one lot (%2 units) at %3 costs more than the %4 "
                              "allocated (capital × position size). Raise the capital or the position size, or "
                              "lower the lot size.")
                            .arg(skipped_entries)
                            .arg(costs.lot_size)
                            .arg(skip_px, 0, 'f', 2)
                            .arg(skip_budget, 0, 'f', 2));
    if (closed.isEmpty() && entry_true_count == 0)
        warnings.append(entry_err_count > 0
                            ? bt_tr("The entry rule never fired; it reported errors such as: %1").arg(last_entry_err)
                            : bt_tr("The entry rule never fired over this period."));
    if (closed_at_end)
        warnings.append(bt_tr("A position still open on the last bar was closed at its close (costs included)."));
    if (span_days < kBtMinDaysForAnnual)
        warnings.append(bt_tr("The tested period is %1 days: CAGR and Calmar extrapolate it to a year.")
                            .arg(span_days, 0, 'f', 0));
    if (daily_bars && (cfg.stop_loss_pct > 0 || cfg.trailing_stop_pct > 0) && cfg.take_profit_pct > 0)
        warnings.append(bt_tr("Daily bars cannot tell whether the stop or the target was touched first; a bar that "
                              "touches both is counted as a stop."));
    out["warnings"] = warnings;
    return out;
}

} // namespace fincept::algo
