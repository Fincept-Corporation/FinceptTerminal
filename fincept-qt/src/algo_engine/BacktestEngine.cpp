// src/algo_engine/BacktestEngine.cpp
#include "algo_engine/BacktestEngine.h"

#include "algo_engine/ConditionEvaluator.h"

#include <algorithm>
#include <cmath>

namespace fincept::algo {

namespace {

// Bars required before any evaluation (indicator warm-up). Matches the old
// Python engine's WARMUP_BARS so results are comparable in shape.
constexpr int kWarmupBars = 50;

// Trailing window passed to the indicator/condition evaluator each bar. Bounds
// per-bar cost to O(window) instead of O(i) — 500 bars is ample warm-up for any
// indicator in the catalog (largest typical period ~200).
constexpr int kEvalWindow = 500;

double round_to(double v, int decimals) {
    double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

// Bars per year for timeframe-aware Sharpe annualisation (mirrors the old engine).
double bars_per_year(const QString& tf) {
    if (tf == "1m")  return 252.0 * 390.0;
    if (tf == "3m")  return 252.0 * 130.0;
    if (tf == "5m")  return 252.0 * 78.0;
    if (tf == "15m") return 252.0 * 26.0;
    if (tf == "30m") return 252.0 * 13.0;
    if (tf == "1h")  return 252.0 * 6.5;
    if (tf == "4h")  return 252.0 * 1.625;
    if (tf == "1d")  return 252.0;
    return 252.0;
}

} // namespace

QJsonObject BacktestEngine::run(const QVector<OhlcvCandle>& candles,
                                const QJsonArray& entry_conditions, const QString& entry_logic,
                                const QJsonArray& exit_conditions, const QString& exit_logic,
                                double stop_loss_pct, double take_profit_pct, double trailing_stop_pct,
                                double initial_capital, const QString& timeframe) {
    const int n = candles.size();
    if (n < kWarmupBars + 10) {
        QJsonObject err;
        err["success"] = false;
        err["error"] = QString("Insufficient data: %1 candles (need at least %2)")
                           .arg(n)
                           .arg(kWarmupBars + 10);
        return err;
    }

    double cash = initial_capital;
    bool in_pos = false;
    double entry_price = 0.0;
    int entry_bar = 0;
    long long shares = 0;
    double highest = 0.0; // high-watermark for trailing stop (long)

    // Signals latched on the close of bar i, executed at the open of bar i+1.
    bool entry_signal = false;
    bool exit_signal = false;

    QJsonArray trades;
    QVector<double> equity_curve;
    equity_curve.reserve(n - kWarmupBars);
    double peak_equity = initial_capital;
    double max_dd = 0.0;

    auto close_trade = [&](double exit_price, const char* reason, int exit_bar) {
        const double pnl = (exit_price - entry_price) * static_cast<double>(shares);
        const double pnl_pct = entry_price > 0 ? (exit_price - entry_price) / entry_price * 100.0 : 0.0;
        cash += static_cast<double>(shares) * exit_price;
        QJsonObject t;
        t["entry_bar"] = entry_bar;
        t["exit_bar"] = exit_bar;
        t["entry_price"] = round_to(entry_price, 2);
        t["exit_price"] = round_to(exit_price, 2);
        t["shares"] = static_cast<double>(shares);
        t["pnl"] = round_to(pnl, 2);
        t["pnl_pct"] = round_to(pnl_pct, 2);
        t["reason"] = QString::fromLatin1(reason);
        t["bars_held"] = exit_bar - entry_bar;
        trades.append(t);
        in_pos = false;
        shares = 0;
    };

    for (int i = kWarmupBars; i < n; ++i) {
        const OhlcvCandle& bar = candles[i];

        // ── 1. Execute pending signal fills at THIS bar's open ──────────────
        if (!in_pos && entry_signal) {
            const double px = bar.open;
            const long long qty = px > 0 ? static_cast<long long>(std::floor(cash / px)) : 0;
            if (qty > 0) {
                in_pos = true;
                entry_price = px;
                entry_bar = i;
                shares = qty;
                cash -= static_cast<double>(shares) * px;
                highest = px;
            }
            entry_signal = false;
        } else if (in_pos && exit_signal) {
            close_trade(bar.open, "exit_signal", i);
            exit_signal = false;
        }

        // ── 2. Intrabar stop-loss / take-profit on THIS bar ─────────────────
        if (in_pos) {
            highest = std::max(highest, bar.high);

            bool have_stop = false;
            double stop_price = 0.0;
            if (stop_loss_pct > 0) {
                stop_price = entry_price * (1.0 - stop_loss_pct / 100.0);
                have_stop = true;
            }
            if (trailing_stop_pct > 0) {
                const double trail = highest * (1.0 - trailing_stop_pct / 100.0);
                stop_price = have_stop ? std::max(stop_price, trail) : trail;
                have_stop = true;
            }
            const double tp_price = take_profit_pct > 0 ? entry_price * (1.0 + take_profit_pct / 100.0) : 0.0;

            // Stop checked first (conservative when both touched in one bar).
            if (have_stop && bar.low <= stop_price) {
                close_trade(stop_price, "stop_loss", i);
            } else if (take_profit_pct > 0 && bar.high >= tp_price) {
                close_trade(tp_price, "take_profit", i);
            }
        }

        // ── 3. Evaluate conditions on close of bar i → latch for next bar ───
        const int start = std::max(0, i - kEvalWindow + 1);
        const QVector<OhlcvCandle> window = candles.mid(start, i - start + 1);
        if (!in_pos && !entry_signal && !entry_conditions.isEmpty()) {
            if (ConditionEvaluator::evaluate_group(entry_conditions, entry_logic, window).triggered)
                entry_signal = true;
        } else if (in_pos && !exit_signal && !exit_conditions.isEmpty()) {
            if (ConditionEvaluator::evaluate_group(exit_conditions, exit_logic, window).triggered)
                exit_signal = true;
        }

        // ── 4. Mark-to-market equity on close ───────────────────────────────
        const double equity = cash + (in_pos ? static_cast<double>(shares) * bar.close : 0.0);
        equity_curve.append(equity);
        if (equity > peak_equity)
            peak_equity = equity;
        const double dd = peak_equity > 0 ? (peak_equity - equity) / peak_equity * 100.0 : 0.0;
        if (dd > max_dd)
            max_dd = dd;
    }

    // Close any open position at the last bar's close.
    if (in_pos)
        close_trade(candles[n - 1].close, "end_of_data", n - 1);

    // ── Metrics ─────────────────────────────────────────────────────────────
    const int total_trades = trades.size();
    const double final_value = cash;
    const double total_return = final_value - initial_capital;
    const double total_return_pct = initial_capital > 0 ? total_return / initial_capital * 100.0 : 0.0;

    QJsonObject out;
    out["success"] = true;

    if (total_trades == 0) {
        out["total_trades"] = 0;
        out["winning_trades"] = 0;
        out["losing_trades"] = 0;
        out["win_rate"] = 0.0;
        out["total_return"] = round_to(total_return_pct, 2);
        out["total_return_abs"] = round_to(total_return, 2);
        out["final_value"] = round_to(final_value, 2);
        out["max_drawdown"] = round_to(max_dd, 2);
        out["avg_pnl"] = 0.0;
        out["avg_bars_held"] = 0.0;
        out["profit_factor"] = 0.0;
        out["sharpe_ratio"] = 0.0;
        out["sharpe"] = 0.0;
        out["equity_curve"] = QJsonArray();
        out["trades"] = trades;
        return out;
    }

    int wins = 0;
    double gross_profit = 0.0, gross_loss = 0.0, sum_pnl = 0.0;
    long long sum_bars_held = 0;
    for (const auto& tv : trades) {
        const QJsonObject t = tv.toObject();
        const double pnl = t.value("pnl").toDouble();
        sum_pnl += pnl;
        sum_bars_held += t.value("bars_held").toInt();
        if (pnl > 0) {
            ++wins;
            gross_profit += pnl;
        } else {
            gross_loss += std::abs(pnl);
        }
    }
    const int losses = total_trades - wins;
    const double win_rate = static_cast<double>(wins) / total_trades * 100.0;
    const double avg_pnl = sum_pnl / total_trades;
    const double avg_bars_held = static_cast<double>(sum_bars_held) / total_trades;

    double profit_factor;
    if (gross_loss > 0)
        profit_factor = gross_profit / gross_loss;
    else
        profit_factor = gross_profit > 0 ? 999.99 : 0.0;
    if (profit_factor > 999.99)
        profit_factor = 999.99;

    // Sharpe from equity-curve point-to-point returns, annualised by timeframe.
    double sharpe = 0.0;
    if (equity_curve.size() > 1) {
        QVector<double> rets;
        rets.reserve(equity_curve.size() - 1);
        for (int i = 1; i < equity_curve.size(); ++i) {
            if (equity_curve[i - 1] != 0.0)
                rets.append((equity_curve[i] - equity_curve[i - 1]) / equity_curve[i - 1]);
        }
        if (!rets.isEmpty()) {
            double mean = 0.0;
            for (double r : rets)
                mean += r;
            mean /= rets.size();
            double var = 0.0;
            for (double r : rets)
                var += (r - mean) * (r - mean);
            var /= rets.size();
            const double sd = std::sqrt(var);
            if (sd > 0.0)
                sharpe = (mean / sd) * std::sqrt(bars_per_year(timeframe));
        }
    }

    // Downsample equity curve to <= 500 points (keep last point).
    QJsonArray equity_out;
    if (equity_curve.size() > 500) {
        const int step = equity_curve.size() / 500;
        for (int i = 0; i < equity_curve.size(); i += step)
            equity_out.append(round_to(equity_curve[i], 2));
        if (equity_out.isEmpty() ||
            equity_out.last().toDouble() != round_to(equity_curve.last(), 2))
            equity_out.append(round_to(equity_curve.last(), 2));
    } else {
        for (double e : equity_curve)
            equity_out.append(round_to(e, 2));
    }

    out["total_trades"] = total_trades;
    out["winning_trades"] = wins;
    out["losing_trades"] = losses;
    out["win_rate"] = round_to(win_rate, 1);
    out["total_return"] = round_to(total_return_pct, 2); // panel reads as percent
    out["total_return_abs"] = round_to(total_return, 2);
    out["final_value"] = round_to(final_value, 2);
    out["max_drawdown"] = round_to(max_dd, 2);
    out["avg_pnl"] = round_to(avg_pnl, 2);
    out["avg_bars_held"] = round_to(avg_bars_held, 1);
    out["profit_factor"] = round_to(profit_factor, 2);
    out["sharpe_ratio"] = round_to(sharpe, 3); // panel reads sharpe_ratio
    out["sharpe"] = round_to(sharpe, 3);       // alias
    out["equity_curve"] = equity_out;
    out["trades"] = trades;
    return out;
}

} // namespace fincept::algo
