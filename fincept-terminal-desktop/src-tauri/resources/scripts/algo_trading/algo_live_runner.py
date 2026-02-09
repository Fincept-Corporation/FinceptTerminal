"""
Algo Live Runner

Long-running Python process spawned by Rust for strategy deployment.
Reads candle_cache, evaluates conditions, generates trade signals.

Usage:
    python algo_live_runner.py --deploy-id <id> --strategy-id <id> --symbol <sym>
        --provider <prov> --mode paper|live --timeframe <tf> --quantity <qty> --db <path>
"""

import json
import sys
import argparse
import sqlite3
import time
import os
import signal
import uuid
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from condition_evaluator import load_candles_from_db, evaluate_condition_group

# Graceful shutdown
running = True

def signal_handler(sig, frame):
    global running
    running = False

signal.signal(signal.SIGTERM, signal_handler)
signal.signal(signal.SIGINT, signal_handler)


TIMEFRAME_SECONDS = {
    '1m': 60, '3m': 180, '5m': 300, '10m': 600, '15m': 900,
    '30m': 1800, '1h': 3600, '4h': 14400, '1d': 86400,
}


def load_strategy(db_path: str, strategy_id: str) -> dict:
    """Load strategy conditions from algo_strategies table."""
    conn = sqlite3.connect(db_path)
    row = conn.execute(
        "SELECT entry_conditions, exit_conditions, entry_logic, exit_logic, "
        "stop_loss, take_profit, trailing_stop, trailing_stop_type "
        "FROM algo_strategies WHERE id = ?",
        (strategy_id,)
    ).fetchone()
    conn.close()

    if not row:
        return None

    return {
        'entry_conditions': json.loads(row[0]),
        'exit_conditions': json.loads(row[1]),
        'entry_logic': row[2],
        'exit_logic': row[3],
        'stop_loss': row[4],
        'take_profit': row[5],
        'trailing_stop': row[6],
        'trailing_stop_type': row[7],
    }


def update_deployment_status(db_path: str, deploy_id: str, status: str, error: str = None):
    """Update deployment status in DB."""
    conn = sqlite3.connect(db_path)
    if error:
        conn.execute(
            "UPDATE algo_deployments SET status = ?, error_message = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (status, error, deploy_id)
        )
    else:
        conn.execute(
            "UPDATE algo_deployments SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (status, deploy_id)
        )
    conn.commit()
    conn.close()


def update_metrics(db_path: str, deploy_id: str, metrics: dict):
    """Update algo_metrics table."""
    conn = sqlite3.connect(db_path)
    conn.execute(
        """INSERT INTO algo_metrics (deployment_id, total_pnl, unrealized_pnl, total_trades,
           win_rate, max_drawdown, current_position_qty, current_position_side,
           current_position_entry, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
           ON CONFLICT(deployment_id) DO UPDATE SET
              total_pnl = excluded.total_pnl,
              unrealized_pnl = excluded.unrealized_pnl,
              total_trades = excluded.total_trades,
              win_rate = excluded.win_rate,
              max_drawdown = excluded.max_drawdown,
              current_position_qty = excluded.current_position_qty,
              current_position_side = excluded.current_position_side,
              current_position_entry = excluded.current_position_entry,
              updated_at = CURRENT_TIMESTAMP""",
        (deploy_id, metrics.get('total_pnl', 0), metrics.get('unrealized_pnl', 0),
         metrics.get('total_trades', 0), metrics.get('win_rate', 0),
         metrics.get('max_drawdown', 0), metrics.get('current_position_qty', 0),
         metrics.get('current_position_side', ''), metrics.get('current_position_entry', 0))
    )
    conn.commit()
    conn.close()


def record_trade(db_path: str, deploy_id: str, symbol: str, side: str,
                 quantity: float, price: float, pnl: float, reason: str):
    """Record a trade in algo_trades table."""
    trade_id = f"trade-{uuid.uuid4()}"
    conn = sqlite3.connect(db_path)
    conn.execute(
        "INSERT INTO algo_trades (id, deployment_id, symbol, side, quantity, price, pnl, signal_reason) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        (trade_id, deploy_id, symbol, side, quantity, price, pnl, reason)
    )
    conn.commit()
    conn.close()
    return trade_id


def create_order_signal(db_path: str, deploy_id: str, symbol: str, side: str,
                        quantity: float, order_type: str = 'MARKET', price: float = None):
    """Write an order signal for Rust to execute (live mode)."""
    signal_id = f"signal-{uuid.uuid4()}"
    conn = sqlite3.connect(db_path)
    conn.execute(
        "INSERT INTO algo_order_signals (id, deployment_id, symbol, side, quantity, order_type, price, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 'pending')",
        (signal_id, deploy_id, symbol, side, quantity, order_type, price)
    )
    conn.commit()
    conn.close()
    return signal_id


def get_current_price(db_path: str, symbol: str) -> float:
    """Get latest price from strategy_price_cache."""
    conn = sqlite3.connect(db_path)
    row = conn.execute(
        "SELECT price FROM strategy_price_cache WHERE symbol = ?",
        (symbol,)
    ).fetchone()
    conn.close()
    return row[0] if row else 0.0


def check_risk_management(current_price: float, position: dict, strategy: dict) -> str:
    """Check stop-loss, take-profit, trailing stop. Returns 'exit' reason or None."""
    if not position or position['qty'] == 0:
        return None

    entry_price = position['entry']
    side = position['side']

    if side == 'BUY':
        pnl_pct = (current_price - entry_price) / entry_price * 100
    else:
        pnl_pct = (entry_price - current_price) / entry_price * 100

    # Stop loss
    sl = strategy.get('stop_loss')
    if sl and pnl_pct <= -abs(sl):
        return f'stop_loss_hit ({pnl_pct:.2f}%)'

    # Take profit
    tp = strategy.get('take_profit')
    if tp and pnl_pct >= abs(tp):
        return f'take_profit_hit ({pnl_pct:.2f}%)'

    # Trailing stop
    ts = strategy.get('trailing_stop')
    if ts and position.get('max_pnl_pct', 0) > 0:
        if position['max_pnl_pct'] - pnl_pct >= abs(ts):
            return f'trailing_stop_hit ({pnl_pct:.2f}%)'

    return None


def main():
    parser = argparse.ArgumentParser(description='Algo Live Runner')
    parser.add_argument('--deploy-id', required=True)
    parser.add_argument('--strategy-id', required=True)
    parser.add_argument('--symbol', required=True)
    parser.add_argument('--provider', default='')
    parser.add_argument('--mode', default='paper', choices=['paper', 'live'])
    parser.add_argument('--timeframe', default='5m')
    parser.add_argument('--quantity', type=float, default=1.0)
    parser.add_argument('--db', required=True)
    args = parser.parse_args()

    sys.stderr.write(f"[algo_runner] Starting deployment {args.deploy_id} for {args.symbol}\n")

    # Load strategy
    strategy = load_strategy(args.db, args.strategy_id)
    if not strategy:
        update_deployment_status(args.db, args.deploy_id, 'error', 'Strategy not found')
        sys.exit(1)

    update_deployment_status(args.db, args.deploy_id, 'running')

    # Inject logic operators into condition lists
    entry_conds = strategy['entry_conditions']
    exit_conds = strategy['exit_conditions']

    # State
    position = {'qty': 0, 'side': '', 'entry': 0, 'max_pnl_pct': 0}
    total_pnl = 0.0
    total_trades = 0
    winning_trades = 0
    max_drawdown = 0.0
    peak_pnl = 0.0
    last_candle_time = 0

    interval = TIMEFRAME_SECONDS.get(args.timeframe, 300)
    check_interval = max(interval // 2, 5)  # check at half the timeframe interval, min 5s

    while running:
        try:
            # Load candles
            df = load_candles_from_db(args.db, args.symbol, args.timeframe, limit=200)
            if df.empty or len(df) < 10:
                time.sleep(check_interval)
                continue

            # Check if we have new data
            newest_time = int(df['open_time'].iloc[-1]) if 'open_time' in df.columns else 0
            if newest_time == last_candle_time:
                time.sleep(check_interval)
                continue
            last_candle_time = newest_time

            current_price = get_current_price(args.db, args.symbol)
            if current_price <= 0:
                current_price = float(df['close'].iloc[-1])

            # Check risk management first
            if position['qty'] != 0:
                # Update max P&L for trailing stop
                if position['side'] == 'BUY':
                    pnl_pct = (current_price - position['entry']) / position['entry'] * 100
                else:
                    pnl_pct = (position['entry'] - current_price) / position['entry'] * 100
                position['max_pnl_pct'] = max(position.get('max_pnl_pct', 0), pnl_pct)

                risk_exit = check_risk_management(current_price, position, strategy)
                if risk_exit:
                    # Close position
                    pnl = (current_price - position['entry']) * position['qty'] if position['side'] == 'BUY' \
                        else (position['entry'] - current_price) * position['qty']

                    if args.mode == 'paper':
                        record_trade(args.db, args.deploy_id, args.symbol, 'SELL' if position['side'] == 'BUY' else 'BUY',
                                     abs(position['qty']), current_price, pnl, risk_exit)
                    else:
                        create_order_signal(args.db, args.deploy_id, args.symbol,
                                            'SELL' if position['side'] == 'BUY' else 'BUY',
                                            abs(position['qty']))

                    total_pnl += pnl
                    total_trades += 1
                    if pnl > 0:
                        winning_trades += 1
                    position = {'qty': 0, 'side': '', 'entry': 0, 'max_pnl_pct': 0}

            # Evaluate conditions
            if position['qty'] == 0:
                # No position: check entry conditions
                entry_result = evaluate_condition_group(entry_conds, df)
                if entry_result['result']:
                    # Enter position
                    position = {
                        'qty': args.quantity,
                        'side': 'BUY',
                        'entry': current_price,
                        'max_pnl_pct': 0,
                    }

                    if args.mode == 'paper':
                        record_trade(args.db, args.deploy_id, args.symbol, 'BUY',
                                     args.quantity, current_price, 0,
                                     f'entry_signal: {json.dumps([d.get("indicator", "") for d in entry_result.get("details", [])])}')
                    else:
                        create_order_signal(args.db, args.deploy_id, args.symbol, 'BUY', args.quantity)

            else:
                # In position: check exit conditions
                exit_result = evaluate_condition_group(exit_conds, df)
                if exit_result['result']:
                    # Exit position
                    pnl = (current_price - position['entry']) * position['qty'] if position['side'] == 'BUY' \
                        else (position['entry'] - current_price) * position['qty']

                    if args.mode == 'paper':
                        record_trade(args.db, args.deploy_id, args.symbol,
                                     'SELL' if position['side'] == 'BUY' else 'BUY',
                                     abs(position['qty']), current_price, pnl,
                                     f'exit_signal: {json.dumps([d.get("indicator", "") for d in exit_result.get("details", [])])}')
                    else:
                        create_order_signal(args.db, args.deploy_id, args.symbol,
                                            'SELL' if position['side'] == 'BUY' else 'BUY',
                                            abs(position['qty']))

                    total_pnl += pnl
                    total_trades += 1
                    if pnl > 0:
                        winning_trades += 1
                    position = {'qty': 0, 'side': '', 'entry': 0, 'max_pnl_pct': 0}

            # Update metrics
            unrealized = 0
            if position['qty'] != 0:
                if position['side'] == 'BUY':
                    unrealized = (current_price - position['entry']) * position['qty']
                else:
                    unrealized = (position['entry'] - current_price) * position['qty']

            # Track drawdown
            current_equity = total_pnl + unrealized
            if current_equity > peak_pnl:
                peak_pnl = current_equity
            dd = peak_pnl - current_equity
            if dd > max_drawdown:
                max_drawdown = dd

            win_rate = (winning_trades / total_trades * 100) if total_trades > 0 else 0

            update_metrics(args.db, args.deploy_id, {
                'total_pnl': total_pnl,
                'unrealized_pnl': unrealized,
                'total_trades': total_trades,
                'win_rate': win_rate,
                'max_drawdown': max_drawdown,
                'current_position_qty': position['qty'],
                'current_position_side': position['side'],
                'current_position_entry': position['entry'],
            })

            time.sleep(check_interval)

        except Exception as e:
            sys.stderr.write(f"[algo_runner] Error: {e}\n")
            time.sleep(check_interval)

    # Shutdown: update status
    update_deployment_status(args.db, args.deploy_id, 'stopped')
    sys.stderr.write(f"[algo_runner] Deployment {args.deploy_id} stopped\n")


if __name__ == '__main__':
    main()
