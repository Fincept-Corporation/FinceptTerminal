#!/usr/bin/env python3
"""
Fincept Terminal - Live Strategy Runner
Manages strategy deployment for paper/live trading with SQLite persistence.
All state is stored in SQLite only - no in-memory state in Rust.

Price data comes from the main app's strategy_price_cache table (written by
the WebSocket router on every tick). Falls back to yfinance if no cached price.

Commands:
  deploy <deploy_id> <strategy_id> <params_json> --db <path> --strategies-dir <path> --app-db <path>
  stop <deploy_id> --db <path>
  stop_all <asset_type> --db <path>
  list <asset_type> --db <path>
  check_duplicate <strategy_id> <symbol> --db <path>
  update <deploy_id> <params_json> --db <path>
"""

import sys
import os
import json
import sqlite3
import time
import signal
from datetime import datetime, timezone

# ============================================================================
# SQLite Schema & DB Helpers
# ============================================================================

SCHEMA = """
CREATE TABLE IF NOT EXISTS deployed_strategies (
    deploy_id TEXT PRIMARY KEY,
    strategy_id TEXT NOT NULL,
    strategy_name TEXT NOT NULL,
    symbol TEXT NOT NULL,
    asset_type TEXT NOT NULL DEFAULT 'equity',
    mode TEXT NOT NULL DEFAULT 'paper',
    status TEXT NOT NULL DEFAULT 'starting',
    params TEXT,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    stopped_at TEXT,
    pid INTEGER
);

CREATE TABLE IF NOT EXISTS strategy_trades (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    deploy_id TEXT NOT NULL,
    symbol TEXT NOT NULL,
    side TEXT NOT NULL,
    quantity REAL NOT NULL,
    price REAL NOT NULL,
    timestamp TEXT NOT NULL,
    order_id TEXT,
    pnl REAL DEFAULT 0,
    FOREIGN KEY (deploy_id) REFERENCES deployed_strategies(deploy_id)
);

CREATE TABLE IF NOT EXISTS strategy_metrics (
    deploy_id TEXT PRIMARY KEY,
    total_pnl REAL DEFAULT 0,
    total_trades INTEGER DEFAULT 0,
    win_rate REAL DEFAULT 0,
    max_drawdown REAL DEFAULT 0,
    sharpe_ratio REAL DEFAULT 0,
    current_position_qty REAL DEFAULT 0,
    current_position_side TEXT DEFAULT '',
    current_position_entry REAL DEFAULT 0,
    updated_at TEXT NOT NULL,
    FOREIGN KEY (deploy_id) REFERENCES deployed_strategies(deploy_id)
);
"""


def get_db(db_path: str) -> sqlite3.Connection:
    """Open SQLite connection and ensure schema exists."""
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    conn.executescript(SCHEMA)
    return conn


# ============================================================================
# Deploy Command - Main strategy execution loop
# ============================================================================

def cmd_deploy(deploy_id: str, strategy_id: str, params_json: str, db_path: str, strategies_dir: str, app_db_path: str = ""):
    """Deploy a strategy as a long-running process."""
    params = json.loads(params_json)
    strategy_name = params.get("strategy_name", strategy_id)
    symbol = params.get("symbol", "")
    asset_type = params.get("asset_type", "equity")
    mode = params.get("mode", "paper")
    timeframe = params.get("timeframe", "1d")
    quantity = float(params.get("quantity", 1))

    # Risk params (user-defined, not hardcoded)
    stop_loss = float(params.get("stop_loss", 0) or 0)
    take_profit = float(params.get("take_profit", 0) or 0)
    max_daily_loss = float(params.get("max_daily_loss", 0) or 0)

    now = datetime.now(timezone.utc).isoformat()
    pid = os.getpid()

    # Persist to SQLite - status starts as "waiting" (waits for conditions)
    conn = get_db(db_path)
    conn.execute(
        """INSERT OR REPLACE INTO deployed_strategies
           (deploy_id, strategy_id, strategy_name, symbol, asset_type, mode, status, params, created_at, updated_at, pid)
           VALUES (?, ?, ?, ?, ?, ?, 'waiting', ?, ?, ?, ?)""",
        (deploy_id, strategy_id, strategy_name, symbol, asset_type, mode, params_json, now, now, pid)
    )
    conn.execute(
        """INSERT OR REPLACE INTO strategy_metrics
           (deploy_id, total_pnl, total_trades, win_rate, max_drawdown, sharpe_ratio,
            current_position_qty, current_position_side, current_position_entry, updated_at)
           VALUES (?, 0, 0, 0, 0, 0, 0, '', 0, ?)""",
        (deploy_id, now)
    )
    conn.commit()

    # Load the strategy class
    try:
        if strategies_dir and strategies_dir not in sys.path:
            sys.path.insert(0, strategies_dir)

        # Find strategy file from registry
        registry_path = os.path.join(strategies_dir, "_registry.py") if strategies_dir else ""
        strategy_path = None

        if os.path.exists(registry_path):
            with open(registry_path, "r") as f:
                for line in f:
                    if strategy_id in line and '"path":' in line:
                        start = line.index('"path":') + len('"path":')
                        rest = line[start:].strip().strip('"').rstrip('",}')
                        strategy_path = rest
                        break

        if not strategy_path:
            _update_status(conn, deploy_id, "error", f"Strategy {strategy_id} not found in registry")
            conn.close()
            return

        full_path = os.path.join(strategies_dir, strategy_path)
        if not os.path.exists(full_path):
            _update_status(conn, deploy_id, "error", f"Strategy file not found: {strategy_path}")
            conn.close()
            return

        # Load strategy module
        with open(full_path, "r") as f:
            source = f.read()

        module_globals = {}
        exec(compile(source, full_path, "exec"), module_globals)

        # Find the QCAlgorithm subclass
        algo_class = None
        for name, obj in module_globals.items():
            if isinstance(obj, type) and name != "QCAlgorithm":
                try:
                    from fincept_engine.algorithm import QCAlgorithm
                    if issubclass(obj, QCAlgorithm):
                        algo_class = obj
                        break
                except ImportError:
                    pass

        if not algo_class:
            _update_status(conn, deploy_id, "error", "No QCAlgorithm subclass found in strategy")
            conn.close()
            return

        # Instantiate and initialize
        algo = algo_class()
        algo.set_cash(float(params.get("initial_cash", 100000)))

        # Inject user params via the parameter system
        algo.set_parameters({
            "symbol": symbol,
            "quantity": str(quantity),
            "mode": mode,
            "timeframe": timeframe,
        })

        if symbol:
            algo._live_symbol = symbol
            algo._live_quantity = quantity

        algo.initialize()

    except Exception as e:
        _update_status(conn, deploy_id, "error", str(e))
        conn.close()
        return

    # Graceful shutdown
    running = True
    def signal_handler(signum, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, signal_handler)
    signal.signal(signal.SIGINT, signal_handler)

    # ---- Main loop ----
    total_pnl = 0.0
    total_trades = 0
    wins = 0
    peak_equity = float(params.get("initial_cash", 100000))
    max_drawdown = 0.0
    current_position = {"qty": 0, "side": "", "entry": 0.0}
    daily_pnl = 0.0
    has_first_signal = False  # Track when we move from "waiting" to "running"

    try:
        while running:
            try:
                # Re-read status and params from DB (allows live updates + stop detection)
                row = conn.execute(
                    "SELECT status, params FROM deployed_strategies WHERE deploy_id = ?",
                    (deploy_id,)
                ).fetchone()
                if not row or row["status"] in ("stopped", "error"):
                    break

                # Dynamic param reload - user can change params while running
                if row["params"]:
                    try:
                        live_params = json.loads(row["params"])
                        stop_loss = float(live_params.get("stop_loss", 0) or 0)
                        take_profit = float(live_params.get("take_profit", 0) or 0)
                        max_daily_loss = float(live_params.get("max_daily_loss", 0) or 0)
                        quantity = float(live_params.get("quantity", quantity) or quantity)
                        timeframe = live_params.get("timeframe", timeframe)
                    except (json.JSONDecodeError, ValueError):
                        pass

                # Fetch current market data from WebSocket cache (main app DB), fallback to yfinance
                market_data = _fetch_price(symbol, asset_type, app_db_path)
                if market_data is None:
                    time.sleep(5)
                    continue

                current_price = market_data["price"]

                # Feed full OHLCV data to algorithm as a TradeBar
                try:
                    from fincept_engine.types import TradeBar, Symbol as FSymbol, Slice
                    from fincept_engine.enums import SecurityType, Market as FMarket

                    sec_type = SecurityType.EQUITY if asset_type == "equity" else SecurityType.CRYPTO
                    market = FMarket.USA if asset_type == "equity" else FMarket.GDAX

                    sym = FSymbol.create(symbol, sec_type, market)
                    bar = TradeBar(
                        time=datetime.now(timezone.utc),
                        symbol=sym,
                        open=market_data["open"],
                        high=market_data["high"],
                        low=market_data["low"],
                        close=current_price,
                        volume=market_data["volume"]
                    )

                    if hasattr(algo, "on_data"):
                        slice_data = Slice(datetime.now(timezone.utc), {symbol: bar})
                        algo.on_data(slice_data)
                except Exception:
                    pass

                # Check for pending orders from the algorithm
                pending_orders = getattr(algo, "_pending_orders", [])

                # Transition from "waiting" to "running" on first signal
                if pending_orders and not has_first_signal:
                    has_first_signal = True
                    _update_status(conn, deploy_id, "running")

                for order in pending_orders:
                    side = "buy" if order.get("quantity", 0) > 0 else "sell"
                    qty = abs(order.get("quantity", 0))
                    if qty == 0:
                        qty = quantity

                    trade_pnl = 0.0
                    if current_position["qty"] > 0 and side == "sell" and current_position["side"] == "long":
                        trade_pnl = (current_price - current_position["entry"]) * min(qty, current_position["qty"])
                    elif current_position["qty"] > 0 and current_position["side"] == "short" and side == "buy":
                        trade_pnl = (current_position["entry"] - current_price) * min(qty, current_position["qty"])

                    # Record trade
                    trade_time = datetime.now(timezone.utc).isoformat()
                    conn.execute(
                        """INSERT INTO strategy_trades
                           (deploy_id, symbol, side, quantity, price, timestamp, pnl)
                           VALUES (?, ?, ?, ?, ?, ?, ?)""",
                        (deploy_id, symbol, side, qty, current_price, trade_time, trade_pnl)
                    )

                    total_trades += 1
                    total_pnl += trade_pnl
                    daily_pnl += trade_pnl
                    if trade_pnl > 0:
                        wins += 1

                    # Update position tracking
                    if side == "buy":
                        if current_position["side"] == "long":
                            # Add to long position
                            total_qty = current_position["qty"] + qty
                            avg_entry = (current_position["entry"] * current_position["qty"] + current_price * qty) / total_qty
                            current_position = {"qty": total_qty, "side": "long", "entry": avg_entry}
                        else:
                            current_position = {"qty": qty, "side": "long", "entry": current_price}
                    elif side == "sell":
                        if current_position["side"] == "long":
                            remaining = current_position["qty"] - qty
                            if remaining <= 0:
                                current_position = {"qty": 0, "side": "", "entry": 0.0}
                            else:
                                current_position["qty"] = remaining
                        else:
                            current_position = {"qty": qty, "side": "short", "entry": current_price}

                # Clear pending orders
                if hasattr(algo, "_pending_orders"):
                    algo._pending_orders = []

                # Calculate unrealized PnL
                unrealized = 0.0
                if current_position["qty"] > 0 and current_position["entry"] > 0:
                    if current_position["side"] == "long":
                        unrealized = (current_price - current_position["entry"]) * current_position["qty"]
                    else:
                        unrealized = (current_position["entry"] - current_price) * current_position["qty"]

                # Drawdown calculation
                equity = float(params.get("initial_cash", 100000)) + total_pnl + unrealized
                if equity > peak_equity:
                    peak_equity = equity
                dd = (peak_equity - equity) / peak_equity if peak_equity > 0 else 0
                if dd > max_drawdown:
                    max_drawdown = dd

                # Risk checks (user-defined, dynamic)
                if stop_loss > 0 and unrealized < -stop_loss:
                    _update_status(conn, deploy_id, "stopped", "Stop loss hit")
                    break
                if take_profit > 0 and unrealized > take_profit:
                    _update_status(conn, deploy_id, "stopped", "Take profit hit")
                    break
                if max_daily_loss > 0 and daily_pnl < -max_daily_loss:
                    _update_status(conn, deploy_id, "stopped", "Max daily loss hit")
                    break

                # Update metrics in DB
                win_rate = (wins / total_trades * 100) if total_trades > 0 else 0
                now = datetime.now(timezone.utc).isoformat()
                conn.execute(
                    """UPDATE strategy_metrics SET
                       total_pnl = ?, total_trades = ?, win_rate = ?,
                       max_drawdown = ?, current_position_qty = ?,
                       current_position_side = ?, current_position_entry = ?,
                       updated_at = ?
                       WHERE deploy_id = ?""",
                    (total_pnl + unrealized, total_trades, win_rate,
                     max_drawdown * 100, current_position["qty"],
                     current_position["side"], current_position["entry"],
                     now, deploy_id)
                )
                conn.execute(
                    "UPDATE deployed_strategies SET updated_at = ? WHERE deploy_id = ?",
                    (now, deploy_id)
                )
                conn.commit()

                # Sleep based on timeframe
                sleep_map = {
                    "1s": 1, "5s": 5, "10s": 10, "30s": 30,
                    "1m": 60, "5m": 300, "15m": 900, "1h": 3600,
                    "4h": 14400, "1d": 60, "1w": 300,
                }
                time.sleep(sleep_map.get(timeframe, 60))

            except Exception:
                conn.execute(
                    "UPDATE deployed_strategies SET updated_at = ? WHERE deploy_id = ?",
                    (datetime.now(timezone.utc).isoformat(), deploy_id)
                )
                conn.commit()
                time.sleep(5)

    except Exception as e:
        _update_status(conn, deploy_id, "error", str(e))
    finally:
        if conn:
            _update_status(conn, deploy_id, "stopped")
            conn.close()


def _fetch_price(symbol: str, asset_type: str, app_db_path: str = ""):
    """Fetch current market data from WebSocket price cache (main app DB).

    The Rust WebSocket router writes every tick to strategy_price_cache in the
    main fincept_terminal.db. This function reads from that table so strategies
    use the same real-time data as the rest of the app.

    Returns a dict with full OHLCV + bid/ask data:
      {"price", "open", "high", "low", "volume", "bid", "ask", "change_percent"}
    Or None if no data available.

    Falls back to yfinance HTTP if no cached price is found (e.g. WebSocket not
    connected or symbol not subscribed).
    """
    def _row_to_dict(row):
        """Convert a cache row to a market data dict."""
        return {
            "price": float(row["price"]),
            "open": float(row["open"] or row["price"]),
            "high": float(row["high"] or row["price"]),
            "low": float(row["low"] or row["price"]),
            "volume": float(row["volume"] or 0),
            "bid": float(row["bid"] or 0),
            "ask": float(row["ask"] or 0),
            "change_percent": float(row["change_percent"] or 0),
        }

    # 1. Try reading from WebSocket price cache in main app DB (normalized lookup)
    if app_db_path and os.path.exists(app_db_path):
        try:
            cache_conn = sqlite3.connect(app_db_path, timeout=2)
            cache_conn.row_factory = sqlite3.Row
            # Normalize symbol for lookup: strip slashes/dashes/underscores, uppercase
            normalized = symbol.replace("/", "").replace("-", "").replace("_", "").upper()
            row = cache_conn.execute(
                """SELECT price, bid, ask, volume, high, low, open, change_percent, updated_at
                   FROM strategy_price_cache
                   WHERE UPPER(REPLACE(REPLACE(REPLACE(symbol, '/', ''), '-', ''), '_', '')) = ?
                   LIMIT 1""",
                (normalized,)
            ).fetchone()
            cache_conn.close()

            if row and row["price"] and row["price"] > 0:
                # Check staleness: if updated_at is a unix timestamp, ensure it's recent
                updated_at = row["updated_at"]
                if updated_at and isinstance(updated_at, (int, float)):
                    age_seconds = time.time() - (updated_at / 1000 if updated_at > 1e12 else updated_at)
                    # Accept if less than 5 minutes old
                    if age_seconds < 300:
                        return _row_to_dict(row)
                else:
                    return _row_to_dict(row)
        except Exception:
            pass

    # 2. Fallback: try without normalization (exact match)
    if app_db_path and os.path.exists(app_db_path):
        try:
            cache_conn = sqlite3.connect(app_db_path, timeout=2)
            cache_conn.row_factory = sqlite3.Row
            row = cache_conn.execute(
                """SELECT price, bid, ask, volume, high, low, open, change_percent, updated_at
                   FROM strategy_price_cache WHERE symbol = ? LIMIT 1""",
                (symbol,)
            ).fetchone()
            cache_conn.close()

            if row and row["price"] and row["price"] > 0:
                return _row_to_dict(row)
        except Exception:
            pass

    # 3. Final fallback: yfinance HTTP (slow, rate-limited)
    try:
        import yfinance as yf
        ticker = yf.Ticker(symbol)
        info = ticker.fast_info
        price = getattr(info, "last_price", None)
        if price is None:
            hist = ticker.history(period="1d")
            if not hist.empty:
                price = float(hist["Close"].iloc[-1])
        if price:
            return {"price": price, "open": price, "high": price, "low": price,
                    "volume": 0, "bid": 0, "ask": 0, "change_percent": 0}
        return None
    except Exception:
        return None


def _update_status(conn, deploy_id: str, status: str, error: str = None):
    """Update deployment status in DB."""
    now = datetime.now(timezone.utc).isoformat()
    if status == "stopped":
        conn.execute(
            "UPDATE deployed_strategies SET status = ?, stopped_at = ?, updated_at = ? WHERE deploy_id = ?",
            (status, now, now, deploy_id)
        )
    elif error:
        conn.execute(
            "UPDATE deployed_strategies SET status = ?, updated_at = ? WHERE deploy_id = ?",
            (status, now, deploy_id)
        )
        # Store error in a separate way that doesn't break params JSON
        try:
            row = conn.execute("SELECT params FROM deployed_strategies WHERE deploy_id = ?", (deploy_id,)).fetchone()
            if row and row["params"]:
                p = json.loads(row["params"])
                p["_error"] = error
                conn.execute("UPDATE deployed_strategies SET params = ? WHERE deploy_id = ?", (json.dumps(p), deploy_id))
        except Exception:
            pass
    else:
        conn.execute(
            "UPDATE deployed_strategies SET status = ?, updated_at = ? WHERE deploy_id = ?",
            (status, now, deploy_id)
        )
    conn.commit()


# ============================================================================
# Stop Command - reads PID from DB, kills process, updates status
# ============================================================================

def cmd_stop(deploy_id: str, db_path: str):
    """Stop a deployed strategy by reading PID from DB and killing it."""
    conn = get_db(db_path)

    # Read PID from DB
    row = conn.execute(
        "SELECT pid FROM deployed_strategies WHERE deploy_id = ?",
        (deploy_id,)
    ).fetchone()

    if row and row["pid"]:
        try:
            os.kill(row["pid"], signal.SIGTERM)
        except (ProcessLookupError, PermissionError, OSError):
            pass

    _update_status(conn, deploy_id, "stopped")
    conn.close()
    print(json.dumps({"success": True, "deploy_id": deploy_id}))


# ============================================================================
# Stop All Command
# ============================================================================

def cmd_stop_all(asset_type: str, db_path: str):
    """Stop all strategies of a given asset type."""
    conn = get_db(db_path)
    now = datetime.now(timezone.utc).isoformat()
    cursor = conn.execute(
        "SELECT deploy_id, pid FROM deployed_strategies WHERE asset_type = ? AND status IN ('running', 'waiting', 'starting')",
        (asset_type,)
    )
    stopped = []
    for row in cursor.fetchall():
        conn.execute(
            "UPDATE deployed_strategies SET status = 'stopped', stopped_at = ?, updated_at = ? WHERE deploy_id = ?",
            (now, now, row["deploy_id"])
        )
        stopped.append(row["deploy_id"])
        if row["pid"]:
            try:
                os.kill(row["pid"], signal.SIGTERM)
            except (ProcessLookupError, PermissionError, OSError):
                pass

    conn.commit()
    conn.close()
    print(json.dumps({"success": True, "stopped": stopped, "count": len(stopped)}))


# ============================================================================
# List Command
# ============================================================================

def cmd_list(asset_type: str, db_path: str):
    """List deployed strategies with metrics."""
    conn = get_db(db_path)
    cursor = conn.execute(
        """SELECT d.*, m.total_pnl, m.total_trades, m.win_rate, m.max_drawdown,
                  m.sharpe_ratio, m.current_position_qty, m.current_position_side,
                  m.current_position_entry
           FROM deployed_strategies d
           LEFT JOIN strategy_metrics m ON d.deploy_id = m.deploy_id
           WHERE d.asset_type = ? AND d.status IN ('running', 'starting', 'waiting', 'error')
           ORDER BY d.created_at DESC""",
        (asset_type,)
    )
    results = []
    for row in cursor.fetchall():
        item = dict(row)
        # Check if process is still alive
        if item.get("pid") and item["status"] in ("running", "waiting"):
            try:
                os.kill(item["pid"], 0)
            except (ProcessLookupError, OSError):
                item["status"] = "stopped"
                conn.execute(
                    "UPDATE deployed_strategies SET status = 'stopped', updated_at = ? WHERE deploy_id = ?",
                    (datetime.now(timezone.utc).isoformat(), item["deploy_id"])
                )
        results.append(item)

    conn.commit()
    conn.close()
    print(json.dumps({"success": True, "data": results, "source": "sqlite"}))


# ============================================================================
# Check Duplicate Command
# ============================================================================

def cmd_check_duplicate(strategy_id: str, symbol: str, db_path: str):
    """Check if a strategy+symbol combo is already deployed."""
    conn = get_db(db_path)
    row = conn.execute(
        """SELECT deploy_id FROM deployed_strategies
           WHERE strategy_id = ? AND symbol = ? AND status IN ('running', 'waiting', 'starting')
           LIMIT 1""",
        (strategy_id, symbol)
    ).fetchone()
    conn.close()
    is_dup = row is not None
    print(json.dumps({"success": True, "is_duplicate": is_dup, "existing_deploy_id": row["deploy_id"] if is_dup else None}))


# ============================================================================
# Update Command - update params of a running strategy
# ============================================================================

def cmd_update(deploy_id: str, params_json: str, db_path: str):
    """Update parameters of a deployed strategy. Runner picks up changes on next loop."""
    conn = get_db(db_path)

    # Merge new params with existing
    row = conn.execute(
        "SELECT params, status FROM deployed_strategies WHERE deploy_id = ?",
        (deploy_id,)
    ).fetchone()

    if not row:
        conn.close()
        print(json.dumps({"success": False, "error": "Strategy not found"}))
        return

    if row["status"] not in ("running", "waiting"):
        conn.close()
        print(json.dumps({"success": False, "error": f"Cannot update strategy in '{row['status']}' state"}))
        return

    try:
        existing = json.loads(row["params"]) if row["params"] else {}
        updates = json.loads(params_json)
        existing.update(updates)
        now = datetime.now(timezone.utc).isoformat()
        conn.execute(
            "UPDATE deployed_strategies SET params = ?, updated_at = ? WHERE deploy_id = ?",
            (json.dumps(existing), now, deploy_id)
        )
        conn.commit()
    except Exception as e:
        conn.close()
        print(json.dumps({"success": False, "error": str(e)}))
        return

    conn.close()
    print(json.dumps({"success": True, "deploy_id": deploy_id}))


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command provided"}))
        sys.exit(1)

    command = sys.argv[1]

    # Parse --db, --strategies-dir, and --app-db flags
    db_path = None
    strategies_dir = None
    app_db_path = ""
    args = sys.argv[2:]
    positional = []

    i = 0
    while i < len(args):
        if args[i] == "--db" and i + 1 < len(args):
            db_path = args[i + 1]
            i += 2
        elif args[i] == "--strategies-dir" and i + 1 < len(args):
            strategies_dir = args[i + 1]
            i += 2
        elif args[i] == "--app-db" and i + 1 < len(args):
            app_db_path = args[i + 1]
            i += 2
        else:
            positional.append(args[i])
            i += 1

    if not db_path:
        print(json.dumps({"success": False, "error": "--db path is required"}))
        sys.exit(1)

    if command == "deploy":
        if len(positional) < 3:
            print(json.dumps({"success": False, "error": "Usage: deploy <deploy_id> <strategy_id> <params_json>"}))
            sys.exit(1)
        cmd_deploy(positional[0], positional[1], positional[2], db_path, strategies_dir or "", app_db_path)

    elif command == "stop":
        if len(positional) < 1:
            print(json.dumps({"success": False, "error": "Usage: stop <deploy_id>"}))
            sys.exit(1)
        cmd_stop(positional[0], db_path)

    elif command == "stop_all":
        if len(positional) < 1:
            print(json.dumps({"success": False, "error": "Usage: stop_all <asset_type>"}))
            sys.exit(1)
        cmd_stop_all(positional[0], db_path)

    elif command == "list":
        if len(positional) < 1:
            print(json.dumps({"success": False, "error": "Usage: list <asset_type>"}))
            sys.exit(1)
        cmd_list(positional[0], db_path)

    elif command == "check_duplicate":
        if len(positional) < 2:
            print(json.dumps({"success": False, "error": "Usage: check_duplicate <strategy_id> <symbol>"}))
            sys.exit(1)
        cmd_check_duplicate(positional[0], positional[1], db_path)

    elif command == "update":
        if len(positional) < 2:
            print(json.dumps({"success": False, "error": "Usage: update <deploy_id> <params_json>"}))
            sys.exit(1)
        cmd_update(positional[0], positional[1], db_path)

    else:
        print(json.dumps({"success": False, "error": f"Unknown command: {command}"}))
        sys.exit(1)


if __name__ == "__main__":
    main()
