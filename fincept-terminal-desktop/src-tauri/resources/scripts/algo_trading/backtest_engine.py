"""
Backtest Engine — Walk-forward strategy backtester

Fetches historical data via yfinance, walks through candles bar-by-bar,
evaluates entry/exit conditions at each bar, and computes performance metrics.

Usage:
    python backtest_engine.py \
        --symbol RELIANCE.NS \
        --entry-conditions '[{"indicator":"RSI","params":{"period":14},"field":"value","operator":"crosses_below","value":30}]' \
        --exit-conditions '[{"indicator":"RSI","params":{"period":14},"field":"value","operator":"crosses_above","value":70}]' \
        --timeframe 1d \
        --period 1y \
        --stop-loss 3 \
        --take-profit 5 \
        --initial-capital 100000

Output:
    JSON with trades, equity curve, and performance metrics
"""

import json
import sys
import argparse
import math
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from condition_evaluator import evaluate_condition_group

# Minimum candles needed before we start evaluating (for indicator warmup)
WARMUP_BARS = 50


def fetch_historical_data(symbol: str, period: str, interval: str):
    """Fetch OHLCV data via yfinance."""
    try:
        import yfinance as yf
        ticker = yf.Ticker(symbol)
        df = ticker.history(period=period, interval=interval)
        if df.empty:
            return None
        # Normalize column names
        df.columns = [c.lower() for c in df.columns]
        # Ensure we have needed columns
        for col in ['open', 'high', 'low', 'close', 'volume']:
            if col not in df.columns:
                return None
        df = df[['open', 'high', 'low', 'close', 'volume']].reset_index(drop=True)
        return df
    except Exception as e:
        print(json.dumps({'success': False, 'error': f'Failed to fetch data: {e}'}), flush=True)
        sys.exit(1)


def run_backtest(
    df,
    entry_conditions: list,
    exit_conditions: list,
    stop_loss_pct: float = 0,
    take_profit_pct: float = 0,
    initial_capital: float = 100000,
):
    """
    Walk forward through candle data and generate trades.

    Returns dict with trades, equity curve, and metrics.
    """
    trades = []
    equity_curve = []
    in_position = False
    entry_price = 0.0
    entry_bar = 0
    capital = initial_capital
    peak_capital = initial_capital
    max_drawdown = 0.0

    for i in range(WARMUP_BARS, len(df)):
        # Use candles up to and including bar i
        window = df.iloc[:i + 1].copy()
        current_price = window['close'].iloc[-1]
        current_bar = i

        if not in_position:
            # Check entry conditions
            result = evaluate_condition_group(entry_conditions, window)
            if result['result']:
                in_position = True
                entry_price = current_price
                entry_bar = current_bar
        else:
            # Check risk management first
            pnl_pct = ((current_price - entry_price) / entry_price) * 100

            # Stop loss
            if stop_loss_pct > 0 and pnl_pct <= -stop_loss_pct:
                exit_price = current_price
                pnl = exit_price - entry_price
                capital += pnl
                trades.append({
                    'entry_bar': entry_bar,
                    'exit_bar': current_bar,
                    'entry_price': round(entry_price, 2),
                    'exit_price': round(exit_price, 2),
                    'pnl': round(pnl, 2),
                    'pnl_pct': round(pnl_pct, 2),
                    'reason': 'stop_loss',
                    'bars_held': current_bar - entry_bar,
                })
                in_position = False
                continue

            # Take profit
            if take_profit_pct > 0 and pnl_pct >= take_profit_pct:
                exit_price = current_price
                pnl = exit_price - entry_price
                capital += pnl
                trades.append({
                    'entry_bar': entry_bar,
                    'exit_bar': current_bar,
                    'entry_price': round(entry_price, 2),
                    'exit_price': round(exit_price, 2),
                    'pnl': round(pnl, 2),
                    'pnl_pct': round(pnl_pct, 2),
                    'reason': 'take_profit',
                    'bars_held': current_bar - entry_bar,
                })
                in_position = False
                continue

            # Check exit conditions
            result = evaluate_condition_group(exit_conditions, window)
            if result['result']:
                exit_price = current_price
                pnl = exit_price - entry_price
                capital += pnl
                trades.append({
                    'entry_bar': entry_bar,
                    'exit_bar': current_bar,
                    'entry_price': round(entry_price, 2),
                    'exit_price': round(exit_price, 2),
                    'pnl': round(pnl, 2),
                    'pnl_pct': round(pnl_pct, 2),
                    'reason': 'exit_signal',
                    'bars_held': current_bar - entry_bar,
                })
                in_position = False

        # Track equity curve
        unrealized = (current_price - entry_price) if in_position else 0
        current_equity = capital + unrealized
        equity_curve.append(round(current_equity, 2))

        if current_equity > peak_capital:
            peak_capital = current_equity
        dd = ((peak_capital - current_equity) / peak_capital) * 100 if peak_capital > 0 else 0
        if dd > max_drawdown:
            max_drawdown = dd

    # Close any open position at last bar
    if in_position and len(df) > 0:
        exit_price = df['close'].iloc[-1]
        pnl = exit_price - entry_price
        pnl_pct = ((exit_price - entry_price) / entry_price) * 100
        capital += pnl
        trades.append({
            'entry_bar': entry_bar,
            'exit_bar': len(df) - 1,
            'entry_price': round(entry_price, 2),
            'exit_price': round(exit_price, 2),
            'pnl': round(pnl, 2),
            'pnl_pct': round(pnl_pct, 2),
            'reason': 'end_of_data',
            'bars_held': len(df) - 1 - entry_bar,
        })

    # Compute metrics
    total_trades = len(trades)
    if total_trades == 0:
        return {
            'trades': [],
            'equity_curve': equity_curve,
            'metrics': {
                'total_trades': 0,
                'win_rate': 0,
                'total_return': 0,
                'total_return_pct': 0,
                'max_drawdown': round(max_drawdown, 2),
                'avg_pnl': 0,
                'avg_bars_held': 0,
                'profit_factor': 0,
                'sharpe': 0,
            },
        }

    wins = [t for t in trades if t['pnl'] > 0]
    losses = [t for t in trades if t['pnl'] <= 0]
    win_rate = (len(wins) / total_trades) * 100

    total_return = capital - initial_capital
    total_return_pct = (total_return / initial_capital) * 100

    avg_pnl = sum(t['pnl'] for t in trades) / total_trades
    avg_bars_held = sum(t['bars_held'] for t in trades) / total_trades

    gross_profit = sum(t['pnl'] for t in wins) if wins else 0
    gross_loss = abs(sum(t['pnl'] for t in losses)) if losses else 0
    profit_factor = (gross_profit / gross_loss) if gross_loss > 0 else float('inf') if gross_profit > 0 else 0

    # Simple Sharpe approximation (annualized using daily returns)
    if len(equity_curve) > 1:
        returns = []
        for i in range(1, len(equity_curve)):
            if equity_curve[i - 1] != 0:
                returns.append((equity_curve[i] - equity_curve[i - 1]) / equity_curve[i - 1])
        if returns:
            mean_ret = sum(returns) / len(returns)
            std_ret = (sum((r - mean_ret) ** 2 for r in returns) / len(returns)) ** 0.5
            sharpe = (mean_ret / std_ret) * (252 ** 0.5) if std_ret > 0 else 0
        else:
            sharpe = 0
    else:
        sharpe = 0

    # Cap profit_factor for JSON serialization
    if math.isinf(profit_factor):
        profit_factor = 999.99

    return {
        'trades': trades,
        'equity_curve': equity_curve[-100:],  # Last 100 points for chart
        'metrics': {
            'total_trades': total_trades,
            'winning_trades': len(wins),
            'losing_trades': len(losses),
            'win_rate': round(win_rate, 1),
            'total_return': round(total_return, 2),
            'total_return_pct': round(total_return_pct, 2),
            'max_drawdown': round(max_drawdown, 2),
            'avg_pnl': round(avg_pnl, 2),
            'avg_bars_held': round(avg_bars_held, 1),
            'profit_factor': round(profit_factor, 2),
            'sharpe': round(sharpe, 2),
        },
    }


def main():
    parser = argparse.ArgumentParser(description='Backtest Engine')
    parser.add_argument('--symbol', required=True, help='yfinance-compatible ticker (e.g. RELIANCE.NS)')
    parser.add_argument('--entry-conditions', required=True, help='JSON entry conditions')
    parser.add_argument('--exit-conditions', required=True, help='JSON exit conditions')
    parser.add_argument('--timeframe', default='1d', help='Candle interval (1d, 1h, 5m, etc.)')
    parser.add_argument('--period', default='1y', help='History period (1mo, 3mo, 6mo, 1y, 2y, 5y)')
    parser.add_argument('--stop-loss', type=float, default=0, help='Stop loss percentage')
    parser.add_argument('--take-profit', type=float, default=0, help='Take profit percentage')
    parser.add_argument('--initial-capital', type=float, default=100000, help='Starting capital')
    args = parser.parse_args()

    try:
        entry_conditions = json.loads(args.entry_conditions)
        exit_conditions = json.loads(args.exit_conditions)
    except json.JSONDecodeError as e:
        print(json.dumps({'success': False, 'error': f'Invalid conditions JSON: {e}'}))
        sys.exit(1)

    # Fetch historical data
    df = fetch_historical_data(args.symbol, args.period, args.timeframe)
    if df is None or df.empty:
        print(json.dumps({
            'success': False,
            'error': f'No historical data available for {args.symbol}'
        }))
        sys.exit(0)

    if len(df) < WARMUP_BARS + 10:
        print(json.dumps({
            'success': False,
            'error': f'Insufficient data ({len(df)} bars, need at least {WARMUP_BARS + 10})'
        }))
        sys.exit(0)

    # Run backtest
    result = run_backtest(
        df=df,
        entry_conditions=entry_conditions,
        exit_conditions=exit_conditions,
        stop_loss_pct=args.stop_loss,
        take_profit_pct=args.take_profit,
        initial_capital=args.initial_capital,
    )

    print(json.dumps({
        'success': True,
        'symbol': args.symbol,
        'timeframe': args.timeframe,
        'period': args.period,
        'total_bars': len(df),
        **result,
    }))


if __name__ == '__main__':
    main()
