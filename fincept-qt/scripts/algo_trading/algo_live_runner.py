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
import logging
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from condition_evaluator import load_candles_from_db, evaluate_condition_group

# ── Heavy debug logging ──────────────────────────────────────────────
LOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'algo_runner_debug.log')
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[
        logging.FileHandler(LOG_FILE, mode='a', encoding='utf-8'),
        logging.StreamHandler(sys.stderr),
    ],
)
log = logging.getLogger('algo_runner')

# Graceful shutdown
running = True

def signal_handler(sig, frame):
    global running
    running = False

signal.signal(signal.SIGTERM, signal_handler)
signal.signal(signal.SIGINT, signal_handler)


TIMEFRAME_SECONDS = {
    'live': 1, '1m': 60, '3m': 180, '5m': 300, '10m': 600, '15m': 900,
    '30m': 1800, '1h': 3600, '4h': 14400, '1d': 86400,
}


MAX_ORDER_VALUE_WARNING = 1_000_000  # Warn if order value exceeds this


def _db_execute_with_retry(conn, sql, params=(), retries=3, commit=True):
    """Execute a DB statement with retry logic for 'database is locked' errors."""
    for attempt in range(retries):
        try:
            conn.execute(sql, params)
            if commit:
                conn.commit()
            return
        except sqlite3.OperationalError as e:
            if 'locked' in str(e).lower() and attempt < retries - 1:
                log.warning(f"  DB locked, retry {attempt + 1}/{retries}...")
                time.sleep(0.1 * (attempt + 1))
            else:
                raise


def open_db_connection(db_path: str):
    """Open a shared DB connection with WAL mode for concurrent access."""
    conn = sqlite3.connect(db_path, timeout=10)
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA busy_timeout=5000")
    return conn


def load_strategy(conn, strategy_id: str) -> dict:
    """Load strategy conditions from algo_strategies table."""
    row = conn.execute(
        "SELECT entry_conditions, exit_conditions, entry_logic, exit_logic, "
        "stop_loss, take_profit, trailing_stop, trailing_stop_type "
        "FROM algo_strategies WHERE id = ?",
        (strategy_id,)
    ).fetchone()

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


def update_deployment_status(conn, deploy_id: str, status: str, error: str = None):
    """Update deployment status in DB."""
    if error:
        _db_execute_with_retry(conn,
            "UPDATE algo_deployments SET status = ?, error_message = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (status, error, deploy_id))
    else:
        _db_execute_with_retry(conn,
            "UPDATE algo_deployments SET status = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (status, deploy_id))


def update_metrics(conn, deploy_id: str, metrics: dict):
    """Update algo_metrics table."""
    _db_execute_with_retry(conn,
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
         metrics.get('current_position_side', ''), metrics.get('current_position_entry', 0)))


def record_trade(conn, deploy_id: str, symbol: str, side: str,
                 quantity: float, price: float, pnl: float, reason: str):
    """Record a trade in algo_trades table."""
    trade_id = f"trade-{uuid.uuid4()}"
    _db_execute_with_retry(conn,
        "INSERT INTO algo_trades (id, deployment_id, symbol, side, quantity, price, pnl, signal_reason) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        (trade_id, deploy_id, symbol, side, quantity, price, pnl, reason))
    return trade_id


def create_order_signal(conn, deploy_id: str, symbol: str, side: str,
                        quantity: float, order_type: str = 'MARKET', price: float = None):
    """Write an order signal for Rust to execute (live mode)."""
    signal_id = f"signal-{uuid.uuid4()}"
    _db_execute_with_retry(conn,
        "INSERT INTO algo_order_signals (id, deployment_id, symbol, side, quantity, order_type, price, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, 'pending')",
        (signal_id, deploy_id, symbol, side, quantity, order_type, price))
    return signal_id


def _normalize_symbol(s: str) -> str:
    """Strip slashes, dashes, underscores and uppercase for matching."""
    return s.replace('/', '').replace('-', '').replace('_', '').replace(':', '').upper()


def get_current_price(conn, symbol: str) -> float:
    """Get latest price from strategy_price_cache.

    Tries exact match first, then normalized match to handle symbol format
    differences (e.g., BTC/USD vs BTCUSD, NSE:RELIANCE vs RELIANCE).
    """
    # Try exact match first
    row = conn.execute(
        "SELECT price FROM strategy_price_cache WHERE symbol = ? ORDER BY updated_at DESC LIMIT 1",
        (symbol,)
    ).fetchone()
    if row:
        log.debug(f"  get_current_price({symbol}) = {row[0]} (exact match)")
        return row[0]

    # Try normalized match: load all symbols and compare normalized forms
    norm = _normalize_symbol(symbol)
    rows = conn.execute(
        "SELECT symbol, price FROM strategy_price_cache ORDER BY updated_at DESC"
    ).fetchall()

    log.debug(f"  get_current_price({symbol}) exact miss. Normalized={norm}. Cache has {len(rows)} entries: {[(s, p) for s, p in rows[:10]]}")

    for (cached_sym, price) in rows:
        cached_norm = _normalize_symbol(cached_sym)
        if cached_norm == norm or norm.startswith(cached_norm) or cached_norm.startswith(norm):
            log.debug(f"  get_current_price({symbol}) = {price} (normalized/prefix match via '{cached_sym}')")
            return price

    log.warning(f"  get_current_price({symbol}) = 0.0 (NO MATCH in price cache)")
    return 0.0


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

    log.info("=" * 70)
    log.info("ALGO LIVE RUNNER STARTING")
    log.info(f"  deploy_id  = {args.deploy_id}")
    log.info(f"  strategy_id= {args.strategy_id}")
    log.info(f"  symbol     = {args.symbol}")
    log.info(f"  provider   = {args.provider}")
    log.info(f"  mode       = {args.mode}")
    log.info(f"  timeframe  = {args.timeframe}")
    log.info(f"  quantity   = {args.quantity}")
    log.info(f"  db         = {args.db}")
    log.info(f"  pid        = {os.getpid()}")
    log.info("=" * 70)

    # Open shared DB connection (WAL mode for concurrent Rust access)
    conn = open_db_connection(args.db)
    log.info("Shared DB connection opened (WAL mode)")

    # Load strategy
    strategy = load_strategy(conn, args.strategy_id)
    if not strategy:
        log.error(f"Strategy {args.strategy_id} NOT FOUND in DB!")
        update_deployment_status(conn, args.deploy_id, 'error', 'Strategy not found')
        conn.close()
        sys.exit(1)

    log.info(f"Strategy loaded OK. Entry conditions: {json.dumps(strategy['entry_conditions'], indent=2)}")
    log.info(f"Exit conditions: {json.dumps(strategy['exit_conditions'], indent=2)}")
    log.info(f"Risk: SL={strategy.get('stop_loss')}, TP={strategy.get('take_profit')}, TS={strategy.get('trailing_stop')}")

    update_deployment_status(conn, args.deploy_id, 'running')

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
    loop_count = 0

    is_live = args.timeframe == 'live'
    interval = 1 if is_live else TIMEFRAME_SECONDS.get(args.timeframe, 300)
    check_interval = 1 if is_live else max(interval // 2, 5)
    log.info(f"Timeframe interval={interval}s, check_interval={check_interval}s, live_mode={is_live}")

    while running:
        loop_count += 1
        try:
            # Load candles (live mode needs fewer rows since each row is a tick)
            candle_limit = 50 if is_live else 200
            df = load_candles_from_db(args.db, args.symbol, args.timeframe, limit=candle_limit)
            if df.empty or len(df) < 2:
                if loop_count <= 5 or loop_count % 20 == 0:
                    log.warning(f"[loop#{loop_count}] Candle data insufficient: {len(df)} rows (need >=2) for {args.symbol}@{args.timeframe}")
                    # Dump what symbols exist in candle_cache
                    try:
                        syms = conn.execute("SELECT DISTINCT symbol, timeframe, COUNT(*) FROM candle_cache GROUP BY symbol, timeframe").fetchall()
                        log.warning(f"  candle_cache contents: {syms}")
                        # Also check strategy_price_cache for recent price updates
                        prices = conn.execute("SELECT symbol, price, updated_at FROM strategy_price_cache ORDER BY updated_at DESC LIMIT 10").fetchall()
                        log.warning(f"  strategy_price_cache recent entries: {prices}")
                    except Exception as dbg_e:
                        log.warning(f"  could not read cache tables: {dbg_e}")
                time.sleep(check_interval)
                continue

            # Check if we have new data
            newest_time = int(df['open_time'].iloc[-1]) if 'open_time' in df.columns else 0
            if not is_live and newest_time == last_candle_time:
                if loop_count <= 3 or loop_count % 60 == 0:
                    log.debug(f"[loop#{loop_count}] No new candle (last={last_candle_time}), {len(df)} candles loaded. Sleeping {check_interval}s")
                time.sleep(check_interval)
                continue
            last_candle_time = newest_time
            if is_live:
                if loop_count <= 3 or loop_count % 100 == 0:
                    log.info(f"[loop#{loop_count}] LIVE tick eval, {len(df)} ticks, latest_price={df['close'].iloc[-1]}")
            else:
                log.info(f"[loop#{loop_count}] NEW candle detected! open_time={newest_time}, total candles={len(df)}")
            log.debug(f"  Last 3 candles: {df[['open_time','open','high','low','close','volume']].tail(3).to_string()}")

            current_price = get_current_price(conn, args.symbol)
            price_source = "price_cache"
            if current_price <= 0:
                current_price = float(df['close'].iloc[-1])
                price_source = "last_candle_close"
            log.info(f"  Current price = {current_price} (source: {price_source})")

            # Check risk management first
            if position['qty'] != 0:
                # Update max P&L for trailing stop
                if position['side'] == 'BUY':
                    pnl_pct = (current_price - position['entry']) / position['entry'] * 100
                else:
                    pnl_pct = (position['entry'] - current_price) / position['entry'] * 100
                position['max_pnl_pct'] = max(position.get('max_pnl_pct', 0), pnl_pct)
                log.debug(f"  Position: {position['side']} qty={position['qty']} entry={position['entry']} pnl%={pnl_pct:.2f}%")

                risk_exit = check_risk_management(current_price, position, strategy)
                if risk_exit:
                    log.info(f"  RISK EXIT TRIGGERED: {risk_exit}")
                    # Close position - use abs(qty) since qty is always stored positive
                    qty = abs(position['qty'])
                    pnl = (current_price - position['entry']) * qty if position['side'] == 'BUY' \
                        else (position['entry'] - current_price) * qty

                    if args.mode == 'paper':
                        record_trade(conn, args.deploy_id, args.symbol, 'SELL' if position['side'] == 'BUY' else 'BUY',
                                     abs(position['qty']), current_price, pnl, risk_exit)
                    else:
                        create_order_signal(conn, args.deploy_id, args.symbol,
                                            'SELL' if position['side'] == 'BUY' else 'BUY',
                                            abs(position['qty']))

                    total_pnl += pnl
                    total_trades += 1
                    if pnl > 0:
                        winning_trades += 1
                    position = {'qty': 0, 'side': '', 'entry': 0, 'max_pnl_pct': 0}

            # Evaluate conditions
            verbose_log = not is_live or loop_count <= 3 or loop_count % 100 == 0
            if position['qty'] == 0:
                # No position: check entry conditions
                entry_result = evaluate_condition_group(entry_conds, df)
                if verbose_log:
                    log.info(f"  ENTRY eval => result={entry_result['result']}, logic={entry_result.get('logic','AND')}")
                    for i, d in enumerate(entry_result.get('details', [])):
                        log.info(f"    cond[{i}]: {d.get('indicator','')} {d.get('operator','')} target={d.get('target','')} "
                                 f"computed={d.get('computed_value','')} met={d.get('met',False)} "
                                 f"{'ERROR: '+d.get('error','') if d.get('error') else ''}")
                if entry_result['result']:
                    log.info(f"  >>> ENTRY SIGNAL FIRED! BUY {args.quantity} @ {current_price}")
                    # Enter position
                    position = {
                        'qty': args.quantity,
                        'side': 'BUY',
                        'entry': current_price,
                        'max_pnl_pct': 0,
                    }

                    # Position size warning (#18)
                    order_value = args.quantity * current_price
                    if order_value > MAX_ORDER_VALUE_WARNING:
                        log.warning(f"  HIGH ORDER VALUE: {args.quantity} x {current_price} = {order_value:.2f} (exceeds {MAX_ORDER_VALUE_WARNING})")

                    if args.mode == 'paper':
                        record_trade(conn, args.deploy_id, args.symbol, 'BUY',
                                     args.quantity, current_price, 0,
                                     f'entry_signal: {json.dumps([d.get("indicator", "") for d in entry_result.get("details", [])])}')
                    else:
                        create_order_signal(conn, args.deploy_id, args.symbol, 'BUY', args.quantity)

            else:
                # In position: check exit conditions
                exit_result = evaluate_condition_group(exit_conds, df)
                if verbose_log:
                    log.info(f"  EXIT eval => result={exit_result['result']}, logic={exit_result.get('logic','AND')}")
                    for i, d in enumerate(exit_result.get('details', [])):
                        log.info(f"    cond[{i}]: {d.get('indicator','')} {d.get('operator','')} target={d.get('target','')} "
                                 f"computed={d.get('computed_value','')} met={d.get('met',False)} "
                                 f"{'ERROR: '+d.get('error','') if d.get('error') else ''}")
                if exit_result['result']:
                    log.info(f"  >>> EXIT SIGNAL FIRED!")
                    # Exit position - use abs(qty) since qty is always stored positive
                    qty = abs(position['qty'])
                    pnl = (current_price - position['entry']) * qty if position['side'] == 'BUY' \
                        else (position['entry'] - current_price) * qty

                    if args.mode == 'paper':
                        record_trade(conn, args.deploy_id, args.symbol,
                                     'SELL' if position['side'] == 'BUY' else 'BUY',
                                     abs(position['qty']), current_price, pnl,
                                     f'exit_signal: {json.dumps([d.get("indicator", "") for d in exit_result.get("details", [])])}')
                    else:
                        create_order_signal(conn, args.deploy_id, args.symbol,
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

            update_metrics(conn, args.deploy_id, {
                'total_pnl': total_pnl,
                'unrealized_pnl': unrealized,
                'total_trades': total_trades,
                'win_rate': win_rate,
                'max_drawdown': max_drawdown,
                'current_position_qty': position['qty'],
                'current_position_side': position['side'],
                'current_position_entry': position['entry'],
            })

            if loop_count <= 5 or loop_count % 30 == 0:
                log.info(f"  Metrics: pnl={total_pnl:.2f} unrealized={unrealized:.2f} trades={total_trades} win%={win_rate:.1f} dd={max_drawdown:.2f}")

            time.sleep(check_interval)

        except Exception as e:
            log.error(f"[loop#{loop_count}] EXCEPTION: {e}", exc_info=True)
            time.sleep(check_interval)

    # Shutdown: update status and close DB connection
    update_deployment_status(conn, args.deploy_id, 'stopped')
    conn.close()
    log.info(f"Deployment {args.deploy_id} STOPPED. Total trades={total_trades}, PnL={total_pnl:.2f}")


if __name__ == '__main__':
    main()
