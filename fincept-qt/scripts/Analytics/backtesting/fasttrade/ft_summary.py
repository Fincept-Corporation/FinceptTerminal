"""
Fast-Trade Summary & Metrics Module

Complete wrapper for fast_trade.build_summary:

Core Summary:
- build_summary(): Generate full performance summary from backtest DataFrame

Return Metrics:
- calculate_return_perc(): Total return percentage from trade log
- calculate_buy_and_hold_perc(): Buy-and-hold benchmark return
- calculate_market_adjusted_returns(): Alpha vs buy-and-hold

Risk Metrics:
- calculate_risk_metrics(): Sharpe, Sortino, Calmar ratios
- calculate_shape_ratio(): Sharpe ratio calculation
- calculate_drawdown_metrics(): Max drawdown, duration, recovery

Trade Analysis:
- create_trade_log(): Build trade log DataFrame from backtest results
- calculate_effective_trades(): Count effective (profitable) trades
- calculate_trade_quality(): Win rate, avg win/loss, expectancy
- calculate_trade_streaks(): Consecutive win/loss streaks
- summarize_trades(): Aggregate trade statistics
- summarize_trade_perc(): Trade percentage breakdown
- summarize_time_held(): Average holding period analysis

Position & Exposure:
- calculate_position_metrics(): Long/short/flat time breakdown
- calculate_market_exposure(): Percentage of time in market

Time Analysis:
- calculate_time_analysis(): Time-based performance breakdown
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional, Tuple
from datetime import datetime


# ============================================================================
# Core Summary
# ============================================================================

def build_summary(
    df: pd.DataFrame,
    performance_start_time: Optional[float] = None
) -> Dict[str, Any]:
    """
    Generate full performance summary from a backtest DataFrame.

    This is the main summary function. Takes the result DataFrame from
    run_backtest and computes all performance metrics.

    Args:
        df: Backtest result DataFrame (with 'close', 'total', 'action' columns)
        performance_start_time: Unix timestamp of backtest start (for timing)

    Returns:
        dict with all performance metrics:
            - return_perc: Total return %
            - sharpe_ratio: Annualized Sharpe
            - sortino_ratio: Annualized Sortino
            - calmar_ratio: Calmar ratio
            - max_drawdown: Maximum drawdown %
            - num_trades: Total number of trades
            - win_perc: Win percentage
            - loss_perc: Loss percentage
            - avg_win: Average winning trade %
            - avg_loss: Average losing trade %
            - max_win: Largest winning trade %
            - max_loss: Largest losing trade %
            - avg_trade: Average trade return %
            - expectancy: Expected return per trade
            - starting_balance: Initial capital
            - ending_balance: Final capital
            - total_comission: Total commissions paid
            - trade_streaks: Consecutive win/loss data
            - buy_and_hold_perc: Benchmark return
            - market_adjusted_return: Alpha vs benchmark
            - total_days: Number of trading days
            - time_held: Holding period stats
            - trade_quality: Quality metrics
            - ... and more
    """
    try:
        from fast_trade.build_summary import build_summary as ft_summary
        return ft_summary(df, performance_start_time)
    except ImportError:
        raise ImportError("fast-trade not installed. Run: pip install fast-trade")


# ============================================================================
# Return Metrics
# ============================================================================

def calculate_return_perc(trade_log_df: pd.DataFrame) -> float:
    """
    Calculate total return percentage from trade log.

    Args:
        trade_log_df: DataFrame with trade entries and exits

    Returns:
        Total return as percentage (e.g. 15.5 for 15.5%)
    """
    try:
        from fast_trade.build_summary import calculate_return_perc as ft_return
        return ft_return(trade_log_df)
    except ImportError:
        if trade_log_df.empty:
            return 0.0
        if 'pnl' in trade_log_df.columns:
            return trade_log_df['pnl'].sum()
        return 0.0


def calculate_buy_and_hold_perc(df: pd.DataFrame) -> float:
    """
    Calculate buy-and-hold benchmark return.

    Simply compares first close to last close.

    Args:
        df: OHLCV DataFrame

    Returns:
        Buy-and-hold return percentage
    """
    try:
        from fast_trade.build_summary import calculate_buy_and_hold_perc as ft_bnh
        return ft_bnh(df)
    except ImportError:
        if df.empty or 'close' not in df.columns:
            return 0.0
        first = df['close'].iloc[0]
        last = df['close'].iloc[-1]
        if first == 0:
            return 0.0
        return ((last - first) / first) * 100


def calculate_market_adjusted_returns(
    df: pd.DataFrame,
    return_perc: float,
    buy_and_hold_perc: float
) -> float:
    """
    Calculate market-adjusted returns (alpha).

    Strategy return minus buy-and-hold return.

    Args:
        df: Backtest DataFrame
        return_perc: Strategy return %
        buy_and_hold_perc: Buy-and-hold return %

    Returns:
        Market-adjusted return percentage
    """
    try:
        from fast_trade.build_summary import calculate_market_adjusted_returns as ft_adj
        return ft_adj(df, return_perc, buy_and_hold_perc)
    except ImportError:
        return return_perc - buy_and_hold_perc


# ============================================================================
# Risk Metrics
# ============================================================================

def calculate_risk_metrics(df: pd.DataFrame) -> Dict[str, float]:
    """
    Calculate risk-adjusted performance metrics.

    Args:
        df: Backtest DataFrame with 'total' (equity) column

    Returns:
        dict with 'sharpe_ratio', 'sortino_ratio', 'calmar_ratio'
    """
    try:
        from fast_trade.build_summary import calculate_risk_metrics as ft_risk
        return ft_risk(df)
    except ImportError:
        result = {'sharpe_ratio': 0.0, 'sortino_ratio': 0.0, 'calmar_ratio': 0.0}
        if df.empty or 'total' not in df.columns:
            return result

        returns = df['total'].pct_change().dropna()
        if len(returns) == 0 or returns.std() == 0:
            return result

        # Sharpe (annualized, assuming daily)
        result['sharpe_ratio'] = (returns.mean() / returns.std()) * np.sqrt(252)

        # Sortino (downside only)
        downside = returns[returns < 0]
        if len(downside) > 0 and downside.std() > 0:
            result['sortino_ratio'] = (returns.mean() / downside.std()) * np.sqrt(252)

        # Calmar
        dd = calculate_drawdown_metrics(df)
        max_dd = abs(dd.get('max_drawdown', 0))
        if max_dd > 0:
            ann_return = returns.mean() * 252
            result['calmar_ratio'] = ann_return / max_dd

        return result


def calculate_shape_ratio(df: pd.DataFrame) -> float:
    """
    Calculate Sharpe ratio.

    Args:
        df: Backtest DataFrame

    Returns:
        Sharpe ratio (annualized)
    """
    try:
        from fast_trade.build_summary import calculate_shape_ratio as ft_sharpe
        return ft_sharpe(df)
    except ImportError:
        metrics = calculate_risk_metrics(df)
        return metrics.get('sharpe_ratio', 0.0)


def calculate_drawdown_metrics(df: pd.DataFrame) -> Dict[str, Any]:
    """
    Calculate drawdown metrics.

    Args:
        df: Backtest DataFrame with 'total' column

    Returns:
        dict with:
            - max_drawdown: Maximum drawdown (negative %)
            - max_drawdown_duration: Bars in longest drawdown
            - avg_drawdown: Average drawdown
            - drawdown_series: Full drawdown series
    """
    try:
        from fast_trade.build_summary import calculate_drawdown_metrics as ft_dd
        return ft_dd(df)
    except ImportError:
        result = {
            'max_drawdown': 0.0,
            'max_drawdown_duration': 0,
            'avg_drawdown': 0.0,
        }
        if df.empty or 'total' not in df.columns:
            return result

        equity = df['total']
        running_max = equity.cummax()
        drawdown = (equity - running_max) / running_max

        result['max_drawdown'] = drawdown.min()
        result['avg_drawdown'] = drawdown[drawdown < 0].mean() if (drawdown < 0).any() else 0.0

        # Duration: count bars in drawdown
        in_dd = drawdown < 0
        groups = (in_dd != in_dd.shift()).cumsum()
        dd_groups = groups[in_dd]
        if len(dd_groups) > 0:
            result['max_drawdown_duration'] = dd_groups.value_counts().max()

        return result


# ============================================================================
# Trade Analysis
# ============================================================================

def create_trade_log(df: pd.DataFrame) -> pd.DataFrame:
    """
    Build trade log DataFrame from backtest results.

    Extracts entry/exit pairs from the action column.

    Args:
        df: Backtest result DataFrame with 'action' column

    Returns:
        DataFrame with trade entries: action, close, amount, pnl, etc.
    """
    try:
        from fast_trade.build_summary import create_trade_log as ft_log
        return ft_log(df)
    except ImportError:
        if df.empty or 'action' not in df.columns:
            return pd.DataFrame()
        trades = df[df['action'].isin(['e', 'x'])].copy()
        return trades


def calculate_effective_trades(
    df: pd.DataFrame,
    trade_log_df: pd.DataFrame
) -> int:
    """
    Count effective (completed round-trip) trades.

    Args:
        df: Full backtest DataFrame
        trade_log_df: Trade log DataFrame

    Returns:
        Number of completed trades
    """
    try:
        from fast_trade.build_summary import calculate_effective_trades as ft_eff
        return ft_eff(df, trade_log_df)
    except ImportError:
        if trade_log_df.empty:
            return 0
        entries = len(trade_log_df[trade_log_df['action'] == 'e'])
        exits = len(trade_log_df[trade_log_df['action'] == 'x'])
        return min(entries, exits)


def calculate_trade_quality(trade_log_df: pd.DataFrame) -> Dict[str, Any]:
    """
    Calculate trade quality metrics.

    Args:
        trade_log_df: Trade log DataFrame

    Returns:
        dict with:
            - win_rate: Percentage of winning trades
            - avg_win: Average winning trade return
            - avg_loss: Average losing trade return
            - max_win: Largest winning trade
            - max_loss: Largest losing trade
            - expectancy: Expected return per trade
            - payoff_ratio: avg_win / avg_loss
    """
    try:
        from fast_trade.build_summary import calculate_trade_quality as ft_quality
        return ft_quality(trade_log_df)
    except ImportError:
        result = {
            'win_rate': 0.0,
            'avg_win': 0.0,
            'avg_loss': 0.0,
            'max_win': 0.0,
            'max_loss': 0.0,
            'expectancy': 0.0,
            'payoff_ratio': 0.0,
        }
        if trade_log_df.empty or 'pnl' not in trade_log_df.columns:
            return result

        exits = trade_log_df[trade_log_df['action'] == 'x']
        if exits.empty:
            return result

        wins = exits[exits['pnl'] > 0]
        losses = exits[exits['pnl'] <= 0]

        total = len(exits)
        result['win_rate'] = (len(wins) / total * 100) if total > 0 else 0.0
        result['avg_win'] = wins['pnl'].mean() if len(wins) > 0 else 0.0
        result['avg_loss'] = losses['pnl'].mean() if len(losses) > 0 else 0.0
        result['max_win'] = wins['pnl'].max() if len(wins) > 0 else 0.0
        result['max_loss'] = losses['pnl'].min() if len(losses) > 0 else 0.0
        result['expectancy'] = exits['pnl'].mean() if total > 0 else 0.0

        if result['avg_loss'] != 0:
            result['payoff_ratio'] = abs(result['avg_win'] / result['avg_loss'])

        return result


def calculate_trade_streaks(trade_log_df: pd.DataFrame) -> Dict[str, int]:
    """
    Calculate consecutive win/loss streaks.

    Args:
        trade_log_df: Trade log DataFrame

    Returns:
        dict with:
            - max_win_streak: Longest consecutive wins
            - max_loss_streak: Longest consecutive losses
            - current_streak: Current streak (positive=wins, negative=losses)
    """
    try:
        from fast_trade.build_summary import calculate_trade_streaks as ft_streaks
        return ft_streaks(trade_log_df)
    except ImportError:
        result = {'max_win_streak': 0, 'max_loss_streak': 0, 'current_streak': 0}
        if trade_log_df.empty or 'pnl' not in trade_log_df.columns:
            return result

        exits = trade_log_df[trade_log_df['action'] == 'x']
        if exits.empty:
            return result

        wins_losses = (exits['pnl'] > 0).astype(int).values
        max_win = max_loss = current = 0

        for wl in wins_losses:
            if wl == 1:
                current = max(1, current + 1)
                max_win = max(max_win, current)
            else:
                current = min(-1, current - 1)
                max_loss = max(max_loss, abs(current))

        result['max_win_streak'] = max_win
        result['max_loss_streak'] = max_loss
        result['current_streak'] = current
        return result


def summarize_trades(
    trades: pd.DataFrame,
    total_trades: int
) -> Dict[str, Any]:
    """
    Aggregate trade statistics.

    Args:
        trades: Trade DataFrame
        total_trades: Total number of trades

    Returns:
        Summary dict with trade count, averages, totals
    """
    try:
        from fast_trade.build_summary import summarize_trades as ft_sum
        return ft_sum(trades, total_trades)
    except ImportError:
        return {
            'total_trades': total_trades,
            'trades_count': len(trades),
        }


def summarize_trade_perc(trade_log_df: pd.DataFrame) -> Dict[str, float]:
    """
    Summarize trade percentage breakdown.

    Args:
        trade_log_df: Trade log DataFrame

    Returns:
        dict with win_perc, loss_perc, even_perc
    """
    try:
        from fast_trade.build_summary import summarize_trade_perc as ft_perc
        return ft_perc(trade_log_df)
    except ImportError:
        if trade_log_df.empty:
            return {'win_perc': 0.0, 'loss_perc': 0.0, 'even_perc': 0.0}

        exits = trade_log_df[trade_log_df['action'] == 'x']
        total = len(exits)
        if total == 0:
            return {'win_perc': 0.0, 'loss_perc': 0.0, 'even_perc': 0.0}

        wins = len(exits[exits.get('pnl', 0) > 0])
        losses = len(exits[exits.get('pnl', 0) < 0])
        even = total - wins - losses

        return {
            'win_perc': wins / total * 100,
            'loss_perc': losses / total * 100,
            'even_perc': even / total * 100,
        }


def summarize_time_held(trade_log_df: pd.DataFrame) -> Dict[str, Any]:
    """
    Analyze holding periods of trades.

    Args:
        trade_log_df: Trade log DataFrame

    Returns:
        dict with avg_holding_period, min, max, median
    """
    try:
        from fast_trade.build_summary import summarize_time_held as ft_time
        return ft_time(trade_log_df)
    except ImportError:
        return {
            'avg_holding_period': 0,
            'min_holding_period': 0,
            'max_holding_period': 0,
        }


# ============================================================================
# Position & Exposure
# ============================================================================

def calculate_position_metrics(df: pd.DataFrame) -> Dict[str, float]:
    """
    Calculate position metrics (time in long/short/flat).

    Args:
        df: Backtest DataFrame

    Returns:
        dict with long_pct, short_pct, flat_pct
    """
    try:
        from fast_trade.build_summary import calculate_position_metrics as ft_pos
        return ft_pos(df)
    except ImportError:
        return {'long_pct': 0.0, 'short_pct': 0.0, 'flat_pct': 100.0}


def calculate_market_exposure(df: pd.DataFrame) -> float:
    """
    Calculate percentage of time in market.

    Args:
        df: Backtest DataFrame

    Returns:
        Market exposure as percentage (0-100)
    """
    try:
        from fast_trade.build_summary import calculate_market_exposure as ft_exp
        return ft_exp(df)
    except ImportError:
        if df.empty or 'action' not in df.columns:
            return 0.0
        in_market = df['action'].isin(['e', 'h']).sum()
        return (in_market / len(df)) * 100


# ============================================================================
# Time Analysis
# ============================================================================

def calculate_time_analysis(df: pd.DataFrame) -> Dict[str, Any]:
    """
    Time-based performance breakdown.

    Args:
        df: Backtest DataFrame

    Returns:
        dict with total_days, total_bars, bars_per_day, etc.
    """
    try:
        from fast_trade.build_summary import calculate_time_analysis as ft_time
        return ft_time(df)
    except ImportError:
        if df.empty:
            return {'total_days': 0, 'total_bars': 0}

        total_bars = len(df)
        if hasattr(df.index, 'min') and hasattr(df.index, 'max'):
            total_days = (df.index.max() - df.index.min()).days
        else:
            total_days = 0

        return {
            'total_days': total_days,
            'total_bars': total_bars,
        }
