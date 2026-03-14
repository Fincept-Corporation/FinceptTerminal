"""
Zipline Metrics Extraction

Extracts performance metrics, trade records, and equity curves from
Zipline's result DataFrame returned by run_algorithm().
"""

import sys
import math
import numpy as np
import pandas as pd
from typing import Dict, Any, List, Optional
from datetime import datetime


def extract_performance(
    results: pd.DataFrame,
    initial_capital: float,
) -> Dict[str, Any]:
    """
    Extract PerformanceMetrics from Zipline result DataFrame.

    Zipline returns a DataFrame indexed by date with columns like:
    portfolio_value, returns, positions, transactions, orders,
    starting_cash, ending_cash, etc.
    """
    if results is None or results.empty:
        return _empty_metrics()

    try:
        portfolio_values = results['portfolio_value'].values
        daily_returns = results['returns'].values

        final_value = float(portfolio_values[-1])
        total_return = (final_value / initial_capital) - 1.0

        # Annualized return
        n_days = len(daily_returns)
        n_years = n_days / 252.0
        if n_years > 0 and final_value > 0:
            annualized_return = (final_value / initial_capital) ** (1.0 / n_years) - 1.0
        else:
            annualized_return = 0.0

        # Volatility
        volatility = float(np.std(daily_returns, ddof=1) * np.sqrt(252)) if n_days > 1 else 0.0

        # Sharpe ratio (assume 0 risk-free rate)
        mean_ret = float(np.mean(daily_returns))
        std_ret = float(np.std(daily_returns, ddof=1)) if n_days > 1 else 1e-8
        sharpe = (mean_ret * 252) / (std_ret * np.sqrt(252)) if std_ret > 1e-8 else 0.0

        # Sortino ratio
        downside = daily_returns[daily_returns < 0]
        downside_std = float(np.std(downside, ddof=1) * np.sqrt(252)) if len(downside) > 1 else 1e-8
        sortino = (mean_ret * 252) / downside_std if downside_std > 1e-8 else 0.0

        # Max drawdown
        cumulative = np.cumprod(1 + daily_returns)
        running_max = np.maximum.accumulate(cumulative)
        drawdowns = (cumulative - running_max) / running_max
        max_drawdown = float(np.min(drawdowns)) if len(drawdowns) > 0 else 0.0

        # Calmar ratio
        calmar = annualized_return / abs(max_drawdown) if abs(max_drawdown) > 1e-8 else 0.0

        # Max drawdown duration
        dd_duration = _max_drawdown_duration(drawdowns)

        # Trade statistics from transactions
        trades = _extract_trades_from_results(results, initial_capital)
        total_trades = len(trades)
        winning_trades = sum(1 for t in trades if (t.get('pnl') or 0) > 0)
        losing_trades = sum(1 for t in trades if (t.get('pnl') or 0) < 0)
        win_rate = winning_trades / total_trades if total_trades > 0 else 0.0
        loss_rate = losing_trades / total_trades if total_trades > 0 else 0.0

        wins = [t.get('pnl', 0) for t in trades if (t.get('pnl') or 0) > 0]
        losses = [t.get('pnl', 0) for t in trades if (t.get('pnl') or 0) < 0]

        average_win = float(np.mean(wins)) if wins else 0.0
        average_loss = float(np.mean(losses)) if losses else 0.0
        largest_win = float(max(wins)) if wins else 0.0
        largest_loss = float(min(losses)) if losses else 0.0

        all_pnl = [t.get('pnl', 0) for t in trades]
        average_trade_return = float(np.mean(all_pnl)) if all_pnl else 0.0
        expectancy = win_rate * average_win + loss_rate * average_loss if total_trades > 0 else 0.0

        # Profit factor
        gross_profit = sum(wins) if wins else 0.0
        gross_loss = abs(sum(losses)) if losses else 1e-8
        profit_factor = gross_profit / gross_loss if gross_loss > 1e-8 else 0.0

        return {
            'total_return': _safe_float(total_return),
            'annualized_return': _safe_float(annualized_return),
            'sharpe_ratio': _safe_float(sharpe),
            'sortino_ratio': _safe_float(sortino),
            'max_drawdown': _safe_float(max_drawdown),
            'win_rate': _safe_float(win_rate),
            'loss_rate': _safe_float(loss_rate),
            'profit_factor': _safe_float(profit_factor),
            'volatility': _safe_float(volatility),
            'calmar_ratio': _safe_float(calmar),
            'total_trades': total_trades,
            'winning_trades': winning_trades,
            'losing_trades': losing_trades,
            'average_win': _safe_float(average_win),
            'average_loss': _safe_float(average_loss),
            'largest_win': _safe_float(largest_win),
            'largest_loss': _safe_float(largest_loss),
            'average_trade_return': _safe_float(average_trade_return),
            'expectancy': _safe_float(expectancy),
            'max_drawdown_duration': dd_duration,
        }

    except Exception as e:
        print(f'[ZL-METRICS] Error extracting metrics: {e}', file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
        return _empty_metrics()


def extract_equity_curve(
    results: pd.DataFrame,
    initial_capital: float,
) -> List[Dict[str, Any]]:
    """Extract equity curve data from Zipline results."""
    if results is None or results.empty:
        return []

    equity = []
    portfolio_values = results['portfolio_value'].values
    daily_returns = results['returns'].values

    cumulative = np.cumprod(1 + daily_returns)
    running_max = np.maximum.accumulate(cumulative)
    drawdowns = (cumulative - running_max) / running_max

    for i, (idx, row) in enumerate(results.iterrows()):
        dt = idx
        if hasattr(dt, 'strftime'):
            date_str = dt.strftime('%Y-%m-%d')
        else:
            date_str = str(dt)

        equity.append({
            'date': date_str,
            'equity': _safe_float(portfolio_values[i]),
            'returns': _safe_float(daily_returns[i]),
            'drawdown': _safe_float(drawdowns[i]) if i < len(drawdowns) else 0.0,
        })

    return equity


def extract_statistics(
    results: pd.DataFrame,
    initial_capital: float,
    trades: List[Dict[str, Any]],
) -> Dict[str, Any]:
    """Extract BacktestStatistics from Zipline results."""
    if results is None or results.empty:
        return _empty_statistics(initial_capital)

    portfolio_values = results['portfolio_value'].values
    daily_returns = results['returns'].values

    start_date = results.index[0]
    end_date = results.index[-1]

    if hasattr(start_date, 'strftime'):
        start_str = start_date.strftime('%Y-%m-%d')
        end_str = end_date.strftime('%Y-%m-%d')
    else:
        start_str = str(start_date)
        end_str = str(end_date)

    winning_days = int(np.sum(daily_returns > 0))
    losing_days = int(np.sum(daily_returns < 0))
    avg_daily_return = float(np.mean(daily_returns))
    best_day = float(np.max(daily_returns)) if len(daily_returns) > 0 else 0.0
    worst_day = float(np.min(daily_returns)) if len(daily_returns) > 0 else 0.0

    # Consecutive wins/losses
    cons_wins, cons_losses = _consecutive_streaks(daily_returns)

    return {
        'start_date': start_str,
        'end_date': end_str,
        'initial_capital': initial_capital,
        'final_capital': _safe_float(portfolio_values[-1]),
        'total_fees': 0.0,  # Zipline handles fees internally
        'total_slippage': 0.0,
        'total_trades': len(trades),
        'winning_days': winning_days,
        'losing_days': losing_days,
        'average_daily_return': _safe_float(avg_daily_return),
        'best_day': _safe_float(best_day),
        'worst_day': _safe_float(worst_day),
        'consecutive_wins': cons_wins,
        'consecutive_losses': cons_losses,
    }


def _extract_trades_from_results(
    results: pd.DataFrame,
    initial_capital: float,
) -> List[Dict[str, Any]]:
    """
    Extract trade records from Zipline's transactions column.

    Zipline stores transactions as a list of dicts per day in the
    'transactions' column. We pair buys/sells into round-trip trades.
    """
    trades = []
    if 'transactions' not in results.columns:
        return trades

    open_positions: Dict[str, Dict[str, Any]] = {}
    trade_id = 0

    for idx, row in results.iterrows():
        txns = row.get('transactions', [])
        if not isinstance(txns, list) or len(txns) == 0:
            continue

        dt = idx
        date_str = dt.strftime('%Y-%m-%d') if hasattr(dt, 'strftime') else str(dt)

        for txn in txns:
            if not isinstance(txn, dict):
                continue

            sid = str(txn.get('sid', txn.get('symbol', 'UNKNOWN')))
            amount = txn.get('amount', 0)
            price = txn.get('price', 0)
            commission = txn.get('commission', 0)

            if amount > 0:
                # Buy / open long
                open_positions[sid] = {
                    'entry_date': date_str,
                    'entry_price': float(price),
                    'quantity': float(amount),
                    'commission': float(commission or 0),
                }
            elif amount < 0 and sid in open_positions:
                # Sell / close position
                entry = open_positions.pop(sid)
                trade_id += 1
                exit_price = float(price)
                entry_price = entry['entry_price']
                qty = entry['quantity']
                pnl = (exit_price - entry_price) * qty - entry['commission'] - float(commission or 0)

                entry_dt = datetime.strptime(entry['entry_date'], '%Y-%m-%d')
                exit_dt = datetime.strptime(date_str, '%Y-%m-%d')
                holding = (exit_dt - entry_dt).days

                trades.append({
                    'id': str(trade_id),
                    'symbol': sid,
                    'entry_date': entry['entry_date'],
                    'exit_date': date_str,
                    'side': 'long',
                    'quantity': qty,
                    'entry_price': entry_price,
                    'exit_price': exit_price,
                    'pnl': _safe_float(pnl),
                    'pnl_percent': _safe_float(pnl / (entry_price * qty)) if entry_price * qty > 0 else 0.0,
                    'holding_period': holding,
                    'commission': entry['commission'] + float(commission or 0),
                    'slippage': 0.0,
                    'exit_reason': 'signal',
                })

    return trades


def _max_drawdown_duration(drawdowns: np.ndarray) -> int:
    """Calculate max drawdown duration in days."""
    if len(drawdowns) == 0:
        return 0
    in_drawdown = drawdowns < -1e-8
    max_dur = 0
    current = 0
    for dd in in_drawdown:
        if dd:
            current += 1
            max_dur = max(max_dur, current)
        else:
            current = 0
    return max_dur


def _consecutive_streaks(returns: np.ndarray):
    """Calculate max consecutive winning and losing days."""
    max_wins = 0
    max_losses = 0
    cur_wins = 0
    cur_losses = 0

    for r in returns:
        if r > 0:
            cur_wins += 1
            max_wins = max(max_wins, cur_wins)
            cur_losses = 0
        elif r < 0:
            cur_losses += 1
            max_losses = max(max_losses, cur_losses)
            cur_wins = 0
        else:
            cur_wins = 0
            cur_losses = 0

    return max_wins, max_losses


def _safe_float(val) -> float:
    """Convert to float, replacing NaN/Inf with 0."""
    try:
        f = float(val)
        if math.isnan(f) or math.isinf(f):
            return 0.0
        return f
    except (TypeError, ValueError):
        return 0.0


def _empty_metrics() -> Dict[str, Any]:
    """Return zeroed performance metrics."""
    return {
        'total_return': 0.0,
        'annualized_return': 0.0,
        'sharpe_ratio': 0.0,
        'sortino_ratio': 0.0,
        'max_drawdown': 0.0,
        'win_rate': 0.0,
        'loss_rate': 0.0,
        'profit_factor': 0.0,
        'volatility': 0.0,
        'calmar_ratio': 0.0,
        'total_trades': 0,
        'winning_trades': 0,
        'losing_trades': 0,
        'average_win': 0.0,
        'average_loss': 0.0,
        'largest_win': 0.0,
        'largest_loss': 0.0,
        'average_trade_return': 0.0,
        'expectancy': 0.0,
        'max_drawdown_duration': 0,
    }


def _empty_statistics(initial_capital: float) -> Dict[str, Any]:
    """Return empty statistics."""
    return {
        'start_date': '',
        'end_date': '',
        'initial_capital': initial_capital,
        'final_capital': initial_capital,
        'total_fees': 0.0,
        'total_slippage': 0.0,
        'total_trades': 0,
        'winning_days': 0,
        'losing_days': 0,
        'average_daily_return': 0.0,
        'best_day': 0.0,
        'worst_day': 0.0,
        'consecutive_wins': 0,
        'consecutive_losses': 0,
    }
