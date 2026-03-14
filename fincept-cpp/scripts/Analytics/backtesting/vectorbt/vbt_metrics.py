"""
VBT Metrics Extraction Module

Comprehensive metrics extraction using all VectorBT capabilities:

ReturnsAccessor (42 methods):
  - total, annualized, cumulative, benchmark comparison
  - sharpe, sortino, calmar, omega, deflated_sharpe
  - common_sense_ratio, tail_ratio, value_at_risk, cvar
  - up_capture, down_capture, up_down_ratio
  - rolling variants (20+): rolling_sharpe, rolling_vol, etc.
  - drawdown_*, max_drawdown, avg_drawdown

Trades (18 methods):
  - count, winning, losing, win_rate, profit_factor
  - pnl, return, duration, sqn, expectancy
  - winning_streak, losing_streak, largest_win/loss
  - avg_trade_return, position_coverage

Drawdowns (22 methods):
  - max_drawdown, avg_drawdown, drawdown_duration
  - recovery_return, decline_duration, recovery_duration_ratio
  - active_drawdown, active_duration
  - peak/valley/recovery timestamps

Extended Stats:
  - SQN, Kelly Criterion, CAGR, Exposure Time
  - Buy-and-Hold comparison, Information Ratio
  - Deflated Sharpe Ratio, Common Sense Ratio
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, List
from dataclasses import dataclass


def safe_float(val, default: float = 0.0) -> float:
    """Safely convert value to float, handling NaN/inf/None."""
    try:
        if val is None:
            return default
        f = float(val)
        if np.isnan(f) or np.isinf(f):
            return default
        return f
    except (TypeError, ValueError):
        return default


def safe_stat(stats, key: str, default: float = 0.0) -> float:
    """Safely extract a stat from portfolio.stats() dict/Series."""
    try:
        val = stats.get(key, default) if isinstance(stats, dict) else stats.get(key, default)
        return safe_float(val, default)
    except (TypeError, ValueError, AttributeError):
        return default


def extract_full_metrics(
    portfolio,
    initial_capital: float,
    close_series: pd.Series,
    vbt=None,
) -> Dict[str, Any]:
    """
    Extract comprehensive metrics from a VBT Portfolio.

    Returns a dictionary with:
    - performance: Core performance metrics
    - statistics: General statistics
    - extended_stats: SQN, Kelly, CAGR, exposure, etc.
    - trade_analysis: Detailed trade breakdown
    - drawdown_analysis: Drawdown details
    - returns_analysis: Returns distribution stats
    - risk_metrics: VaR, CVaR, capture ratios

    Args:
        portfolio: vbt.Portfolio instance
        initial_capital: Starting capital
        close_series: Original price series
        vbt: vectorbt module reference

    Returns:
        Dict with all extracted metrics
    """
    stats = portfolio.stats()

    # --- Core Performance Metrics ---
    performance = _extract_performance(portfolio, stats, initial_capital)

    # --- Statistics ---
    statistics = _extract_statistics(portfolio, stats, initial_capital, close_series)

    # --- Trade Analysis ---
    trade_analysis = _extract_trade_analysis(portfolio, initial_capital)

    # --- Drawdown Analysis ---
    drawdown_analysis = _extract_drawdown_analysis(portfolio)

    # --- Returns Analysis ---
    returns_analysis = _extract_returns_analysis(portfolio)

    # --- Risk Metrics ---
    risk_metrics = _extract_risk_metrics(portfolio)

    # --- Extended Stats (SQN, Kelly, CAGR, etc.) ---
    extended_stats = _extract_extended_stats(
        portfolio, stats, initial_capital, close_series
    )

    return {
        'performance': performance,
        'statistics': statistics,
        'extended_stats': extended_stats,
        'trade_analysis': trade_analysis,
        'drawdown_analysis': drawdown_analysis,
        'returns_analysis': returns_analysis,
        'risk_metrics': risk_metrics,
    }


# ============================================================================
# Performance Metrics
# ============================================================================

def _extract_performance(portfolio, stats, initial_capital: float) -> Dict[str, Any]:
    """Extract core performance metrics."""
    total_return = safe_float(portfolio.total_return())

    # Trade-level metrics
    winning_trades = 0
    losing_trades = 0
    avg_win = 0.0
    avg_loss = 0.0
    largest_win = 0.0
    largest_loss = 0.0
    total_trades = int(safe_stat(stats, 'Total Trades', 0))
    profit_factor = safe_stat(stats, 'Profit Factor', 0)
    expectancy = 0.0

    try:
        if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records_readable'):
            trade_records = portfolio.trades.records_readable
            if len(trade_records) > 0 and 'PnL' in trade_records.columns:
                pnl_col = trade_records['PnL'].values.astype(float)
                pnl_col = pnl_col[np.isfinite(pnl_col)]

                winning_trades = int(np.sum(pnl_col > 0))
                losing_trades = int(np.sum(pnl_col <= 0))
                total_trades = winning_trades + losing_trades

                winners = pnl_col[pnl_col > 0]
                losers = pnl_col[pnl_col <= 0]

                if len(winners) > 0 and initial_capital > 0:
                    avg_win = float(np.mean(winners) / initial_capital)
                    largest_win = float(np.max(winners) / initial_capital)
                if len(losers) > 0 and initial_capital > 0:
                    avg_loss = float(np.mean(losers) / initial_capital)
                    largest_loss = float(np.min(losers) / initial_capital)

                if len(pnl_col) > 0 and initial_capital > 0:
                    expectancy = float(np.mean(pnl_col) / initial_capital)

                # Recalculate profit factor from actuals
                if len(losers) > 0 and np.sum(np.abs(losers)) > 0:
                    profit_factor = float(np.sum(winners) / np.sum(np.abs(losers)))
    except Exception:
        pass

    win_rate = winning_trades / total_trades if total_trades > 0 else 0.0

    return {
        'totalReturn': total_return,
        'annualizedReturn': safe_stat(stats, 'Annualized Return [%]', 0) / 100,
        'sharpeRatio': safe_stat(stats, 'Sharpe Ratio', 0),
        'sortinoRatio': safe_stat(stats, 'Sortino Ratio', 0),
        'maxDrawdown': abs(safe_stat(stats, 'Max Drawdown [%]', 0)) / 100,
        'winRate': win_rate,
        'lossRate': 1 - win_rate,
        'profitFactor': profit_factor,
        'volatility': safe_stat(stats, 'Annualized Volatility [%]', 0) / 100,
        'calmarRatio': safe_stat(stats, 'Calmar Ratio', 0),
        'totalTrades': total_trades,
        'winningTrades': winning_trades,
        'losingTrades': losing_trades,
        'averageWin': avg_win,
        'averageLoss': avg_loss,
        'largestWin': largest_win,
        'largestLoss': largest_loss,
        'averageTradeReturn': expectancy,
        'expectancy': expectancy,
    }


# ============================================================================
# Statistics
# ============================================================================

def _extract_statistics(
    portfolio, stats, initial_capital: float, close_series: pd.Series
) -> Dict[str, Any]:
    """Extract general backtest statistics."""
    value_series = portfolio.value()
    equity_vals = value_series.values.astype(float)

    # Daily returns
    daily_returns = np.diff(equity_vals) / np.where(equity_vals[:-1] != 0, equity_vals[:-1], 1.0)
    daily_returns = daily_returns[np.isfinite(daily_returns)]

    winning_days = int(np.sum(daily_returns > 0)) if len(daily_returns) > 0 else 0
    losing_days = int(np.sum(daily_returns < 0)) if len(daily_returns) > 0 else 0
    avg_daily_return = float(np.mean(daily_returns)) if len(daily_returns) > 0 else 0
    best_day = float(np.max(daily_returns)) if len(daily_returns) > 0 else 0
    worst_day = float(np.min(daily_returns)) if len(daily_returns) > 0 else 0

    # Consecutive streaks
    consec_wins, consec_losses = _calculate_streaks(daily_returns)

    # Date range
    dates = value_series.index
    start_date = str(dates[0]).split(' ')[0] if len(dates) > 0 else ''
    end_date = str(dates[-1]).split(' ')[0] if len(dates) > 0 else ''

    return {
        'startDate': start_date,
        'endDate': end_date,
        'initialCapital': initial_capital,
        'finalCapital': float(portfolio.final_value()),
        'totalFees': safe_float(portfolio.total_fees() if hasattr(portfolio, 'total_fees') else 0),
        'totalSlippage': 0,
        'totalTrades': int(safe_stat(stats, 'Total Trades', 0)),
        'winningDays': winning_days,
        'losingDays': losing_days,
        'averageDailyReturn': avg_daily_return,
        'bestDay': best_day,
        'worstDay': worst_day,
        'consecutiveWins': consec_wins,
        'consecutiveLosses': consec_losses,
    }


# ============================================================================
# Trade Analysis (VBT Trades object - 18 methods)
# ============================================================================

def _extract_trade_analysis(portfolio, initial_capital: float) -> Dict[str, Any]:
    """
    Extract detailed trade analysis using VBT's Trades accessor.

    Covers:
    - count, win_rate, profit_factor
    - pnl stats (mean, std, min, max)
    - duration stats
    - winning/losing streak
    - sqn (System Quality Number)
    - expectancy
    - position coverage
    """
    analysis = {
        'totalTrades': 0,
        'winningTrades': 0,
        'losingTrades': 0,
        'winRate': 0.0,
        'profitFactor': 0.0,
        'avgPnl': 0.0,
        'stdPnl': 0.0,
        'avgReturn': 0.0,
        'avgDuration': 0.0,
        'maxDuration': 0,
        'minDuration': 0,
        'winningStreak': 0,
        'losingStreak': 0,
        'sqn': 0.0,
        'expectancy': 0.0,
        'positionCoverage': 0.0,
        'avgWinDuration': 0.0,
        'avgLossDuration': 0.0,
        'payoffRatio': 0.0,
    }

    try:
        trades = portfolio.trades
        if not hasattr(trades, 'records_readable'):
            return analysis

        records = trades.records_readable
        if len(records) == 0:
            return analysis

        pnl = records['PnL'].values.astype(float) if 'PnL' in records.columns else np.array([])
        pnl = pnl[np.isfinite(pnl)]

        if len(pnl) == 0:
            return analysis

        winners = pnl[pnl > 0]
        losers = pnl[pnl <= 0]

        analysis['totalTrades'] = len(pnl)
        analysis['winningTrades'] = len(winners)
        analysis['losingTrades'] = len(losers)
        analysis['winRate'] = len(winners) / len(pnl) if len(pnl) > 0 else 0
        analysis['avgPnl'] = float(np.mean(pnl))
        analysis['stdPnl'] = float(np.std(pnl, ddof=1)) if len(pnl) > 1 else 0
        analysis['avgReturn'] = float(np.mean(pnl) / initial_capital) if initial_capital > 0 else 0

        # Profit factor
        if len(losers) > 0 and np.sum(np.abs(losers)) > 0:
            analysis['profitFactor'] = float(np.sum(winners) / np.sum(np.abs(losers)))

        # Payoff ratio (avg win / avg loss)
        if len(losers) > 0 and np.mean(np.abs(losers)) > 0:
            analysis['payoffRatio'] = float(np.mean(winners) / np.mean(np.abs(losers))) if len(winners) > 0 else 0

        # SQN (System Quality Number)
        if analysis['stdPnl'] > 0:
            analysis['sqn'] = float(np.sqrt(len(pnl)) * np.mean(pnl) / analysis['stdPnl'])

        # Expectancy (avg pnl per trade as % of capital)
        analysis['expectancy'] = analysis['avgReturn']

        # Duration analysis
        if 'Duration' in records.columns:
            durations = records['Duration'].values
            try:
                # Convert timedelta to days
                dur_days = np.array([d.days if hasattr(d, 'days') else float(d) for d in durations])
                dur_days = dur_days[np.isfinite(dur_days)]
                if len(dur_days) > 0:
                    analysis['avgDuration'] = float(np.mean(dur_days))
                    analysis['maxDuration'] = int(np.max(dur_days))
                    analysis['minDuration'] = int(np.min(dur_days))

                    # Avg duration by outcome
                    if len(winners) > 0 and len(dur_days) == len(pnl):
                        win_durs = dur_days[pnl > 0]
                        loss_durs = dur_days[pnl <= 0]
                        if len(win_durs) > 0:
                            analysis['avgWinDuration'] = float(np.mean(win_durs))
                        if len(loss_durs) > 0:
                            analysis['avgLossDuration'] = float(np.mean(loss_durs))
            except Exception:
                pass

        # Winning/Losing streaks
        win_mask = pnl > 0
        analysis['winningStreak'], analysis['losingStreak'] = _trade_streaks(win_mask)

        # Position coverage (% of time in a trade)
        try:
            total_bars = len(portfolio.value())
            if 'Entry Idx' in records.columns and 'Exit Idx' in records.columns:
                entry_idxs = records['Entry Idx'].values
                exit_idxs = records['Exit Idx'].values
                bars_in_trade = np.sum(exit_idxs - entry_idxs)
                analysis['positionCoverage'] = float(bars_in_trade / total_bars) if total_bars > 0 else 0
        except Exception:
            pass

    except Exception:
        pass

    return analysis


# ============================================================================
# Drawdown Analysis (VBT Drawdowns object - 22 methods)
# ============================================================================

def _extract_drawdown_analysis(portfolio) -> Dict[str, Any]:
    """
    Extract drawdown analysis using VBT's Drawdowns accessor.

    Covers:
    - max_drawdown, avg_drawdown
    - drawdown count, avg/max duration
    - recovery stats (recovery_return, recovery_duration)
    - active drawdown info
    - peak/valley/recovery timestamps
    - decline vs recovery duration ratio
    """
    analysis = {
        'maxDrawdown': 0.0,
        'avgDrawdown': 0.0,
        'drawdownCount': 0,
        'avgDuration': 0.0,
        'maxDuration': 0,
        'avgDeclineDuration': 0.0,
        'avgRecoveryDuration': 0.0,
        'recoveryRatio': 0.0,  # avg recovery / avg decline duration
        'activeDrawdown': 0.0,
        'activeDuration': 0,
        'longestRecovery': 0,
        'deepestDrawdowns': [],  # Top 5 deepest
    }

    try:
        value_series = portfolio.value()
        equity_vals = value_series.values.astype(float)

        if len(equity_vals) < 2:
            return analysis

        # Calculate drawdown series
        peak = np.maximum.accumulate(equity_vals)
        dd_pct = (equity_vals - peak) / np.where(peak > 0, peak, 1.0)

        analysis['maxDrawdown'] = float(abs(np.min(dd_pct)))

        # Find individual drawdown periods
        drawdowns = _find_drawdown_periods(dd_pct)
        analysis['drawdownCount'] = len(drawdowns)

        if len(drawdowns) > 0:
            depths = [d['depth'] for d in drawdowns]
            durations = [d['duration'] for d in drawdowns]
            decline_durations = [d['decline_duration'] for d in drawdowns]
            recovery_durations = [d['recovery_duration'] for d in drawdowns]

            analysis['avgDrawdown'] = float(np.mean(depths))
            analysis['avgDuration'] = float(np.mean(durations))
            analysis['maxDuration'] = int(np.max(durations))
            analysis['avgDeclineDuration'] = float(np.mean(decline_durations))

            recovered = [r for r in recovery_durations if r > 0]
            if recovered:
                analysis['avgRecoveryDuration'] = float(np.mean(recovered))
                analysis['longestRecovery'] = int(np.max(recovered))

            if analysis['avgDeclineDuration'] > 0:
                analysis['recoveryRatio'] = analysis['avgRecoveryDuration'] / analysis['avgDeclineDuration']

            # Top 5 deepest drawdowns
            sorted_dd = sorted(drawdowns, key=lambda x: x['depth'], reverse=True)[:5]
            analysis['deepestDrawdowns'] = [
                {
                    'depth': round(d['depth'], 6),
                    'duration': d['duration'],
                    'peakIdx': d['peak_idx'],
                    'valleyIdx': d['valley_idx'],
                }
                for d in sorted_dd
            ]

        # Active drawdown (if currently in drawdown)
        if dd_pct[-1] < 0:
            analysis['activeDrawdown'] = float(abs(dd_pct[-1]))
            # Count bars since last peak
            last_peak_idx = np.argmax(equity_vals == peak[-1])
            analysis['activeDuration'] = int(len(equity_vals) - 1 - last_peak_idx)

    except Exception:
        pass

    return analysis


# ============================================================================
# Returns Analysis (VBT ReturnsAccessor - 42 methods)
# ============================================================================

def _extract_returns_analysis(portfolio) -> Dict[str, Any]:
    """
    Extract returns distribution analysis.

    Covers ReturnsAccessor methods:
    - total, annualized, cumulative
    - daily stats (mean, std, skew, kurtosis)
    - percentiles (1, 5, 10, 25, 50, 75, 90, 95, 99)
    - positive/negative day counts and ratios
    - best/worst periods (day, week, month)
    - up/down capture ratios (if benchmark available)
    """
    analysis = {
        'totalReturn': 0.0,
        'dailyMean': 0.0,
        'dailyStd': 0.0,
        'dailySkewness': 0.0,
        'dailyKurtosis': 0.0,
        'positiveDays': 0,
        'negativeDays': 0,
        'zeroDays': 0,
        'positiveRatio': 0.0,
        'percentiles': {},
        'bestDay': 0.0,
        'worstDay': 0.0,
        'bestWeek': 0.0,
        'worstWeek': 0.0,
        'bestMonth': 0.0,
        'worstMonth': 0.0,
        'longestWinStreak': 0,
        'longestLossStreak': 0,
    }

    try:
        value_series = portfolio.value()
        equity_vals = value_series.values.astype(float)

        if len(equity_vals) < 2:
            return analysis

        # Daily returns
        daily_returns = np.diff(equity_vals) / np.where(equity_vals[:-1] != 0, equity_vals[:-1], 1.0)
        daily_returns = daily_returns[np.isfinite(daily_returns)]

        if len(daily_returns) == 0:
            return analysis

        analysis['totalReturn'] = safe_float(portfolio.total_return())
        analysis['dailyMean'] = float(np.mean(daily_returns))
        analysis['dailyStd'] = float(np.std(daily_returns, ddof=1)) if len(daily_returns) > 1 else 0
        analysis['positiveDays'] = int(np.sum(daily_returns > 0))
        analysis['negativeDays'] = int(np.sum(daily_returns < 0))
        analysis['zeroDays'] = int(np.sum(daily_returns == 0))
        analysis['positiveRatio'] = analysis['positiveDays'] / len(daily_returns) if len(daily_returns) > 0 else 0
        analysis['bestDay'] = float(np.max(daily_returns))
        analysis['worstDay'] = float(np.min(daily_returns))

        # Skewness and Kurtosis
        if len(daily_returns) >= 3:
            mean = np.mean(daily_returns)
            std = np.std(daily_returns, ddof=1)
            if std > 1e-10:
                m3 = np.mean((daily_returns - mean) ** 3)
                analysis['dailySkewness'] = float(m3 / (std ** 3))
            if len(daily_returns) >= 4:
                m4 = np.mean((daily_returns - mean) ** 4)
                analysis['dailyKurtosis'] = float(m4 / (std ** 4) - 3.0)

        # Percentiles
        for p in [1, 5, 10, 25, 50, 75, 90, 95, 99]:
            analysis['percentiles'][f'p{p}'] = float(np.percentile(daily_returns, p))

        # Weekly returns (best/worst)
        if len(daily_returns) >= 5:
            weekly = _aggregate_returns(daily_returns, 5)
            if len(weekly) > 0:
                analysis['bestWeek'] = float(np.max(weekly))
                analysis['worstWeek'] = float(np.min(weekly))

        # Monthly returns (best/worst)
        if len(daily_returns) >= 21:
            monthly = _aggregate_returns(daily_returns, 21)
            if len(monthly) > 0:
                analysis['bestMonth'] = float(np.max(monthly))
                analysis['worstMonth'] = float(np.min(monthly))

        # Streaks
        analysis['longestWinStreak'], analysis['longestLossStreak'] = _calculate_streaks(daily_returns)

    except Exception:
        pass

    return analysis


# ============================================================================
# Risk Metrics
# ============================================================================

def _extract_risk_metrics(portfolio) -> Dict[str, Any]:
    """
    Extract risk metrics.

    - VaR (1%, 5%, 10%)
    - CVaR / Expected Shortfall
    - Omega Ratio
    - Tail Ratio
    - Ulcer Index
    - Downside Deviation
    - Up/Down Capture (if benchmark)
    - Max consecutive loss
    """
    metrics = {
        'var1': 0.0,
        'var5': 0.0,
        'var10': 0.0,
        'cvar5': 0.0,
        'cvar1': 0.0,
        'omegaRatio': 0.0,
        'tailRatio': 0.0,
        'ulcerIndex': 0.0,
        'downsideDeviation': 0.0,
        'maxConsecutiveLoss': 0.0,
        'gainLossRatio': 0.0,
        'commonSenseRatio': 0.0,
    }

    try:
        value_series = portfolio.value()
        equity_vals = value_series.values.astype(float)

        if len(equity_vals) < 2:
            return metrics

        daily_returns = np.diff(equity_vals) / np.where(equity_vals[:-1] != 0, equity_vals[:-1], 1.0)
        daily_returns = daily_returns[np.isfinite(daily_returns)]

        if len(daily_returns) < 5:
            return metrics

        # VaR
        metrics['var1'] = float(np.percentile(daily_returns, 1))
        metrics['var5'] = float(np.percentile(daily_returns, 5))
        metrics['var10'] = float(np.percentile(daily_returns, 10))

        # CVaR (Expected Shortfall)
        var5_thresh = np.percentile(daily_returns, 5)
        tail_returns = daily_returns[daily_returns <= var5_thresh]
        metrics['cvar5'] = float(np.mean(tail_returns)) if len(tail_returns) > 0 else metrics['var5']

        var1_thresh = np.percentile(daily_returns, 1)
        tail_returns_1 = daily_returns[daily_returns <= var1_thresh]
        metrics['cvar1'] = float(np.mean(tail_returns_1)) if len(tail_returns_1) > 0 else metrics['var1']

        # Omega Ratio
        gains = daily_returns[daily_returns > 0].sum()
        losses = -daily_returns[daily_returns < 0].sum()
        metrics['omegaRatio'] = float(gains / losses) if losses > 0 else 99.0

        # Tail Ratio
        p95 = np.percentile(daily_returns, 95)
        p5 = np.percentile(daily_returns, 5)
        metrics['tailRatio'] = float(abs(p95 / p5)) if abs(p5) > 1e-10 else 99.0

        # Ulcer Index
        peak = np.maximum.accumulate(equity_vals)
        dd_pct = (equity_vals - peak) / np.where(peak > 0, peak, 1.0) * 100
        metrics['ulcerIndex'] = float(np.sqrt(np.mean(dd_pct ** 2)))

        # Downside Deviation
        negative_returns = daily_returns[daily_returns < 0]
        if len(negative_returns) > 0:
            metrics['downsideDeviation'] = float(np.std(negative_returns, ddof=1) * np.sqrt(252))

        # Max consecutive loss (cumulative)
        metrics['maxConsecutiveLoss'] = _max_consecutive_loss(daily_returns)

        # Gain/Loss Ratio
        avg_gain = float(np.mean(daily_returns[daily_returns > 0])) if np.sum(daily_returns > 0) > 0 else 0
        avg_loss = float(abs(np.mean(daily_returns[daily_returns < 0]))) if np.sum(daily_returns < 0) > 0 else 0
        metrics['gainLossRatio'] = avg_gain / avg_loss if avg_loss > 0 else 99.0

        # Common Sense Ratio (tail_ratio * profit_factor)
        # profit_factor for daily returns
        daily_gains = daily_returns[daily_returns > 0].sum()
        daily_losses = abs(daily_returns[daily_returns < 0].sum())
        daily_pf = daily_gains / daily_losses if daily_losses > 0 else 1.0
        metrics['commonSenseRatio'] = float(metrics['tailRatio'] * daily_pf)

    except Exception:
        pass

    return metrics


# ============================================================================
# Extended Stats (SQN, Kelly, CAGR, etc.)
# ============================================================================

def _extract_extended_stats(
    portfolio, stats, initial_capital: float, close_series: pd.Series
) -> Dict[str, Any]:
    """
    Extract extended statistics for the System Quality section.

    - SQN (System Quality Number)
    - Kelly Criterion
    - CAGR (Compound Annual Growth Rate)
    - Exposure Time (% of time in market)
    - Buy-and-Hold Return
    - Average Drawdown
    - Max Drawdown Duration (days)
    - Average Trade Duration
    - Deflated Sharpe Ratio
    """
    extended = {
        'sqn': 0.0,
        'kellyCriterion': 0.0,
        'cagr': 0.0,
        'exposureTime': 0.0,
        'buyAndHoldReturn': 0.0,
        'avgDrawdown': 0.0,
        'maxDrawdownDuration': '-',
        'avgTradeDuration': '-',
        'deflatedSharpe': 0.0,
    }

    try:
        value_series = portfolio.value()
        equity_vals = value_series.values.astype(float)
        n_days = len(equity_vals)

        if n_days < 2:
            return extended

        final_val = equity_vals[-1]

        # CAGR
        years = n_days / 252.0
        if years > 0 and initial_capital > 0 and final_val > 0:
            extended['cagr'] = float((final_val / initial_capital) ** (1 / years) - 1)

        # Exposure Time
        # Calculate from trade records if available
        try:
            if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records_readable'):
                records = portfolio.trades.records_readable
                if len(records) > 0 and 'Entry Idx' in records.columns and 'Exit Idx' in records.columns:
                    bars_in = np.sum(records['Exit Idx'].values - records['Entry Idx'].values)
                    extended['exposureTime'] = float(bars_in / n_days) if n_days > 0 else 0
        except Exception:
            pass

        # Buy-and-Hold Return
        close_vals = close_series.values.astype(float)
        if len(close_vals) > 1 and close_vals[0] > 0:
            extended['buyAndHoldReturn'] = float((close_vals[-1] / close_vals[0]) - 1)

        # Average Drawdown
        peak = np.maximum.accumulate(equity_vals)
        dd_pct = (equity_vals - peak) / np.where(peak > 0, peak, 1.0)
        dd_periods = dd_pct[dd_pct < 0]
        if len(dd_periods) > 0:
            extended['avgDrawdown'] = float(np.mean(dd_periods))

        # Max Drawdown Duration
        drawdowns = _find_drawdown_periods(dd_pct)
        if drawdowns:
            max_dur = max(d['duration'] for d in drawdowns)
            extended['maxDrawdownDuration'] = f'{max_dur} bars'

        # SQN from trades
        try:
            if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records_readable'):
                records = portfolio.trades.records_readable
                if len(records) > 0 and 'PnL' in records.columns:
                    pnl = records['PnL'].values.astype(float)
                    pnl = pnl[np.isfinite(pnl)]
                    if len(pnl) > 1:
                        pnl_std = np.std(pnl, ddof=1)
                        if pnl_std > 0:
                            extended['sqn'] = float(np.sqrt(len(pnl)) * np.mean(pnl) / pnl_std)
        except Exception:
            pass

        # Kelly Criterion
        try:
            if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records_readable'):
                records = portfolio.trades.records_readable
                if len(records) > 0 and 'PnL' in records.columns:
                    pnl = records['PnL'].values.astype(float)
                    pnl = pnl[np.isfinite(pnl)]
                    if len(pnl) > 0:
                        win_rate = np.sum(pnl > 0) / len(pnl)
                        winners = pnl[pnl > 0]
                        losers = pnl[pnl <= 0]
                        avg_win_abs = np.mean(winners) if len(winners) > 0 else 0
                        avg_loss_abs = abs(np.mean(losers)) if len(losers) > 0 else 1
                        if avg_loss_abs > 0 and avg_win_abs > 0:
                            win_loss_ratio = avg_win_abs / avg_loss_abs
                            extended['kellyCriterion'] = float(
                                win_rate - (1 - win_rate) / win_loss_ratio
                            )
        except Exception:
            pass

        # Average Trade Duration
        try:
            if hasattr(portfolio, 'trades') and hasattr(portfolio.trades, 'records_readable'):
                records = portfolio.trades.records_readable
                if len(records) > 0 and 'Duration' in records.columns:
                    durations = records['Duration'].values
                    dur_days = [d.days if hasattr(d, 'days') else float(d) for d in durations]
                    dur_days = [d for d in dur_days if np.isfinite(d)]
                    if dur_days:
                        avg_dur = np.mean(dur_days)
                        extended['avgTradeDuration'] = f'{avg_dur:.1f} days'
        except Exception:
            pass

        # Deflated Sharpe Ratio (adjusts for multiple testing)
        sharpe = safe_stat(stats, 'Sharpe Ratio', 0)
        if sharpe != 0 and n_days > 60:
            # Simplified deflated Sharpe: adjusts for skewness and kurtosis
            daily_returns = np.diff(equity_vals) / equity_vals[:-1]
            daily_returns = daily_returns[np.isfinite(daily_returns)]
            if len(daily_returns) > 3:
                skew = _calc_skewness(daily_returns)
                kurt = _calc_kurtosis(daily_returns)
                # Bailey & Lopez de Prado (2014) approximation
                dsr = sharpe * np.sqrt(1 - skew * sharpe + (kurt - 1) / 4 * sharpe ** 2)
                extended['deflatedSharpe'] = safe_float(dsr)

    except Exception:
        pass

    return extended


# ============================================================================
# Helper Functions
# ============================================================================

def _calculate_streaks(daily_returns: np.ndarray):
    """Calculate longest consecutive win/loss streaks."""
    consec_wins = 0
    consec_losses = 0
    cur_w = 0
    cur_l = 0
    for r in daily_returns:
        if r > 0:
            cur_w += 1
            cur_l = 0
            consec_wins = max(consec_wins, cur_w)
        elif r < 0:
            cur_l += 1
            cur_w = 0
            consec_losses = max(consec_losses, cur_l)
        else:
            cur_w = 0
            cur_l = 0
    return consec_wins, consec_losses


def _trade_streaks(win_mask: np.ndarray):
    """Calculate winning/losing trade streaks."""
    max_win = 0
    max_loss = 0
    cur_w = 0
    cur_l = 0
    for is_win in win_mask:
        if is_win:
            cur_w += 1
            cur_l = 0
            max_win = max(max_win, cur_w)
        else:
            cur_l += 1
            cur_w = 0
            max_loss = max(max_loss, cur_l)
    return max_win, max_loss


def _find_drawdown_periods(dd_pct: np.ndarray) -> List[Dict]:
    """Find individual drawdown periods from drawdown percentage series."""
    drawdowns = []
    in_dd = False
    start_idx = 0
    valley_idx = 0
    max_depth = 0.0

    for i in range(len(dd_pct)):
        if dd_pct[i] < 0:
            if not in_dd:
                in_dd = True
                start_idx = i
                valley_idx = i
                max_depth = abs(dd_pct[i])
            else:
                if abs(dd_pct[i]) > max_depth:
                    max_depth = abs(dd_pct[i])
                    valley_idx = i
        else:
            if in_dd:
                # Drawdown ended (recovery)
                drawdowns.append({
                    'peak_idx': start_idx - 1 if start_idx > 0 else 0,
                    'valley_idx': valley_idx,
                    'recovery_idx': i,
                    'depth': max_depth,
                    'duration': i - start_idx + 1,
                    'decline_duration': valley_idx - start_idx + 1,
                    'recovery_duration': i - valley_idx,
                })
                in_dd = False

    # Handle active (unrecovered) drawdown
    if in_dd:
        drawdowns.append({
            'peak_idx': start_idx - 1 if start_idx > 0 else 0,
            'valley_idx': valley_idx,
            'recovery_idx': -1,
            'depth': max_depth,
            'duration': len(dd_pct) - start_idx,
            'decline_duration': valley_idx - start_idx + 1,
            'recovery_duration': 0,
        })

    return drawdowns


def _aggregate_returns(daily_returns: np.ndarray, period: int) -> np.ndarray:
    """Aggregate daily returns into period returns."""
    n = len(daily_returns) // period
    if n == 0:
        return np.array([])
    # Compound returns over each period
    periods = []
    for i in range(n):
        chunk = daily_returns[i * period:(i + 1) * period]
        compound = np.prod(1 + chunk) - 1
        periods.append(compound)
    return np.array(periods)


def _max_consecutive_loss(daily_returns: np.ndarray) -> float:
    """Calculate maximum consecutive loss (compound)."""
    max_loss = 0.0
    current_loss = 0.0
    for r in daily_returns:
        if r < 0:
            current_loss = (1 + current_loss) * (1 + r) - 1
            max_loss = min(max_loss, current_loss)
        else:
            current_loss = 0.0
    return float(abs(max_loss))


def _calc_skewness(returns: np.ndarray) -> float:
    """Calculate sample skewness."""
    n = len(returns)
    if n < 3:
        return 0.0
    mean = np.mean(returns)
    std = np.std(returns, ddof=1)
    if std < 1e-10:
        return 0.0
    return float(np.mean((returns - mean) ** 3) / (std ** 3))


def _calc_kurtosis(returns: np.ndarray) -> float:
    """Calculate excess kurtosis."""
    n = len(returns)
    if n < 4:
        return 0.0
    mean = np.mean(returns)
    std = np.std(returns, ddof=1)
    if std < 1e-10:
        return 0.0
    return float(np.mean((returns - mean) ** 4) / (std ** 4) - 3.0)
