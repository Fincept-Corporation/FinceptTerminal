"""
WebSocket streaming bridge — long-running Python subprocess.
Connects to exchange via ccxt.pro WebSocket and streams JSON lines to stdout.
C++ reads stdout line-by-line in a background thread.

Usage: python ws_stream.py <exchange_id> <symbol> [extra_symbols...]
Example: python ws_stream.py binance BTC/USDT ETH/USDT SOL/USDT

Each line is a JSON object with a "type" field:
  {"type":"ticker",    "symbol":"BTC/USDT", "last":70500.0, ...}
  {"type":"orderbook", "symbol":"BTC/USDT", "bids":[[p,a],...], "asks":[[p,a],...], ...}
  {"type":"ohlcv",     "symbol":"BTC/USDT", "timeframe":"1m", "candle":{...}}
  {"type":"trade",     "symbol":"BTC/USDT", "side":"buy", "price":..., "amount":...}
  {"type":"error",     "message":"..."}
  {"type":"status",    "connected":true, "exchange":"binance", "symbols":[...]}
"""

import asyncio
import json
import os
import sys
import signal
import time
import traceback

# Suppress Python package warnings — they go to stdout via some environments
# and corrupt the JSON line protocol the C++ side reads.
import warnings
warnings.filterwarnings("ignore")

MAX_CONSECUTIVE_ERRORS = 10

# Try orjson for ~3-5x faster JSON serialization (C extension).
try:
    import orjson as _json_mod
    def _dumps(obj):
        return _json_mod.dumps(obj, default=str).decode()
except ImportError:
    def _dumps(obj):
        return json.dumps(obj, default=str, separators=(",", ":"))

_stdout_write = sys.stdout.write
_stdout_flush  = sys.stdout.flush

# Dedicated executor for the blocking sys.stdin.readline() in the stdin command
# loop. It is NOT asyncio's default executor on purpose: a readline() parked at
# SIGTERM time would otherwise make asyncio.run()'s shutdown_default_executor()
# hang forever joining the worker thread. Keeping it in our own executor leaves the
# default executor empty (so shutdown is instant); the parked worker is then
# stepped over by the os._exit() in __main__ instead of being joined.
from concurrent.futures import ThreadPoolExecutor as _ThreadPoolExecutor
_stdin_executor = _ThreadPoolExecutor(max_workers=1, thread_name_prefix="stdin-reader")


def emit(obj):
    """Write a JSON line to stdout and flush immediately."""
    try:
        _stdout_write(_dumps(obj) + "\n")
        _stdout_flush()
    except Exception:
        pass


# ── Ticker helpers ────────────────────────────────────────────────────────────

def _make_ticker_msg(ticker, last_override=None):
    """Build a ticker dict from a ccxt ticker object."""
    last = last_override if last_override is not None else ticker.get("last")
    return {
        "type":         "ticker",
        "symbol":       ticker.get("symbol") or ticker.get("Symbol"),
        "last":         last,
        "bid":          ticker.get("bid"),
        "ask":          ticker.get("ask"),
        "high":         ticker.get("high"),
        "low":          ticker.get("low"),
        "open":         ticker.get("open"),
        "close":        ticker.get("close"),
        "change":       ticker.get("change"),
        "percentage":   ticker.get("percentage"),
        "base_volume":  ticker.get("baseVolume"),
        "quote_volume": ticker.get("quoteVolume"),
        "timestamp":    ticker.get("timestamp"),
    }


# Dedup: skip emitting if last price is identical and <100ms since last emit.
_last_ticker_emit  = {}   # symbol -> (last_price, monotonic_time)
_TICKER_MIN_INTERVAL = 0.1  # seconds

def _should_emit_ticker(symbol, last_price):
    now  = time.monotonic()
    prev = _last_ticker_emit.get(symbol)
    if prev is not None:
        prev_price, prev_time = prev
        if prev_price == last_price and (now - prev_time) < _TICKER_MIN_INTERVAL:
            return False
    _last_ticker_emit[symbol] = (last_price, now)
    return True


# ── Symbol normalization ──────────────────────────────────────────────────────

def resolve_symbol(exchange, requested, exchange_id):
    """
    Resolve a requested symbol to the exchange's actual unified symbol.

    Strategy (in order):
      1. Exact match — already valid.
      2. Base/quote match on the exchange's default market type.
      3. Base/quote match on any market type (e.g. BTC/USDT → BTC/USDC:USDC on Hyperliquid).
      4. Base match only — pick the highest-volume market for that base.

    Returns the resolved symbol string, or the original if nothing matches
    (ccxt will raise a meaningful error at subscription time).
    """
    markets = exchange.markets
    if not markets:
        return requested

    # 1. Exact match
    if requested in markets:
        return requested

    # Parse base/quote from the request (handle "BASE/QUOTE" and "BASE/QUOTE:SETTLE")
    parts = requested.replace(":USDT", "").replace(":USDC", "").replace(":USD", "").split("/")
    base  = parts[0].upper() if parts else ""
    quote = parts[1].upper() if len(parts) > 1 else ""

    default_type = exchange.describe().get("options", {}).get("defaultType", "spot")

    # 2. Exact base/quote on default market type
    candidates = [
        m for m in markets.values()
        if m.get("base", "").upper() == base
        and m.get("quote", "").upper() == quote
        and m.get("type") == default_type
        and m.get("active", True)
    ]
    if candidates:
        # Prefer linear (USDT-margined) perps over inverse
        candidates.sort(key=lambda m: (not m.get("linear", False), m.get("symbol", "")))
        return candidates[0]["symbol"]

    # 3. Base/quote match on any type
    candidates = [
        m for m in markets.values()
        if m.get("base", "").upper() == base
        and m.get("quote", "").upper() == quote
        and m.get("active", True)
    ]
    if candidates:
        candidates.sort(key=lambda m: (not m.get("linear", False), m.get("symbol", "")))
        return candidates[0]["symbol"]

    # 4. Base match only — find any active market for this base asset
    candidates = [
        m for m in markets.values()
        if m.get("base", "").upper() == base
        and m.get("type") == default_type
        and m.get("active", True)
    ]
    if candidates:
        # Prefer USDT quote, then USDC, then anything
        def quote_rank(m):
            q = m.get("quote", "")
            return (0 if q == "USDT" else 1 if q == "USDC" else 2, m.get("symbol", ""))
        candidates.sort(key=quote_rank)
        resolved = candidates[0]["symbol"]
        emit({"type": "info", "message":
              f"Symbol '{requested}' not found on {exchange_id}, using '{resolved}'"})
        return resolved

    # Nothing found — return None so the caller can skip this symbol
    emit({"type": "info", "message":
          f"Symbol '{requested}' has no matching market on {exchange_id} — skipping"})
    return None


# ── Per-exchange capability detection ────────────────────────────────────────

def get_exchange_caps(exchange):
    has = exchange.describe().get("has", {})
    return {
        "watch_ticker":    bool(has.get("watchTicker")),
        "watch_tickers":   bool(has.get("watchTickers")),
        "watch_ohlcv":     bool(has.get("watchOHLCV")),
        "watch_trades":    bool(has.get("watchTrades")),
        "watch_orderbook": bool(has.get("watchOrderBook")),
    }


def safe_ob_limit(exchange_id):
    """
    Return a safe orderbook depth limit for each exchange.
    Exchanges that whitelist specific values will raise NotSupported for others.
    Return None to use the exchange default (always safe).
    """
    strict = {
        # Kraken only accepts 10/25/100/500/1000 — None uses the default channel
        "kraken":            None,
        "krakenfutures":     None,
        # Binance accepts most values; 20 is a safe common choice
        "binance":           20,
        "binanceusdm":       20,
        "binancecoinm":      20,
        "binanceus":         20,
        # Bybit WS watch_order_book only accepts 1/50/200/1000 — 50 is a safe default
        "bybit":             50,
        # OKX uses its own channel selector; None picks 'books' (400 levels)
        "okx":               None,
        "okxus":             None,
        # Coinbase Advanced has its own protocol
        "coinbase":          None,
        "coinbaseadvanced":  None,
        "coinbaseexchange":  None,
        # HTX/Huobi
        "htx":               20,
        "huobi":             20,
    }
    return strict.get(exchange_id, 10)


# ── Individual watchers ───────────────────────────────────────────────────────

async def watch_ticker(exchange, symbol):
    """Stream ticker updates. ccxt.pro awaits the next WS push — no polling."""
    errors = 0
    while True:
        try:
            ticker = await exchange.watch_ticker(symbol)
            errors = 0
            last = ticker.get("last")
            if last and _should_emit_ticker(symbol, last):
                emit(_make_ticker_msg(ticker))
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "ticker", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                emit({"type": "error", "source": "ticker", "symbol": symbol,
                      "message": f"Stopping after {errors} consecutive errors"})
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_orderbook(exchange, exchange_id, symbol, ticker_cache):
    """
    Stream orderbook updates.
    Also derives a fast mid-price ticker from best_bid/best_ask — this provides
    sub-second price updates on exchanges where watch_ticker is slow (e.g. Kraken
    spot pushes ~1 ticker/5s but orderbook updates arrive every ~100ms).
    """
    limit  = safe_ob_limit(exchange_id)
    errors = 0
    while True:
        try:
            ob     = await exchange.watch_order_book(symbol, limit)
            errors = 0
            bids   = [[b[0], b[1]] for b in ob["bids"][:10]]
            asks   = [[a[0], a[1]] for a in ob["asks"][:10]]
            best_bid  = bids[0][0] if bids else 0
            best_ask  = asks[0][0] if asks else 0
            spread    = best_ask - best_bid if best_bid and best_ask else 0
            spread_pct = (spread / best_ask * 100) if best_ask else 0
            mid_price = (best_bid + best_ask) / 2 if best_bid and best_ask else 0

            emit({
                "type":       "orderbook",
                "symbol":     symbol,
                "bids":       bids,
                "asks":       asks,
                "best_bid":   best_bid,
                "best_ask":   best_ask,
                "spread":     spread,
                "spread_pct": spread_pct,
            })

            # Emit a derived ticker from orderbook mid-price.
            # This is the fast price path — fires on every OB update.
            # Merge with the last known ticker fields (high/low/volume etc.)
            if mid_price > 0 and _should_emit_ticker(symbol + ":ob", mid_price):
                cached = ticker_cache.get(symbol, {})
                emit({
                    "type":         "ticker",
                    "symbol":       symbol,
                    "last":         mid_price,
                    "bid":          best_bid,
                    "ask":          best_ask,
                    "high":         cached.get("high"),
                    "low":          cached.get("low"),
                    "open":         cached.get("open"),
                    "close":        cached.get("close"),
                    "change":       cached.get("change"),
                    "percentage":   cached.get("percentage"),
                    "base_volume":  cached.get("baseVolume"),
                    "quote_volume": cached.get("quoteVolume"),
                    "timestamp":    None,
                })

        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "orderbook", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                emit({"type": "error", "source": "orderbook", "symbol": symbol,
                      "message": f"Stopping after {errors} consecutive errors"})
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_ticker_and_cache(exchange, symbol, ticker_cache):
    """
    Watch ticker and keep ticker_cache updated with high/low/volume/etc.
    The OB watcher uses this cache to enrich its fast mid-price tickers.
    """
    errors = 0
    while True:
        try:
            ticker = await exchange.watch_ticker(symbol)
            errors = 0
            # Update cache with latest full ticker data
            ticker_cache[symbol] = ticker
            last = ticker.get("last")
            if last and _should_emit_ticker(symbol, last):
                emit(_make_ticker_msg(ticker))
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "ticker", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_ohlcv(exchange, symbol, timeframe="1m"):
    """Stream OHLCV candle updates — emits only the current (latest) candle."""
    errors = 0
    while True:
        try:
            candles = await exchange.watch_ohlcv(symbol, timeframe)
            errors  = 0
            if candles:
                c = candles[-1]
                # Coerce all OHLCV values to float — some exchanges return strings
                try:
                    o, h, l, cl, v = (float(c[i]) if c[i] is not None else 0.0
                                      for i in range(1, 6))
                except (TypeError, ValueError, IndexError):
                    continue
                if o > 0 and h > 0 and l > 0 and cl > 0 and l <= h:
                    emit({
                        "type":      "ohlcv",
                        "symbol":    symbol,
                        "timeframe": timeframe,
                        "interval":  timeframe,
                        "candle": {
                            "timestamp": int(c[0]) if c[0] is not None else 0,
                            "open":   o,
                            "high":   h,
                            "low":    l,
                            "close":  cl,
                            "volume": v,
                        },
                    })
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "ohlcv", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_trades_stream(exchange, symbol):
    """Stream public trade prints — each trade also provides a fast last price."""
    errors = 0
    while True:
        try:
            trades = await exchange.watch_trades(symbol)
            errors = 0
            for t in trades:
                price = t.get("price")
                if price and _should_emit_ticker(symbol + ":trade", price):
                    # Fast price path from trade stream
                    pass  # let the trade message carry the price — C++ uses it
                emit({
                    "type":      "trade",
                    "symbol":    symbol,
                    "id":        t.get("id"),
                    "timestamp": t.get("timestamp"),
                    "side":      t.get("side"),
                    "price":     price,
                    "amount":    t.get("amount"),
                    "cost":      t.get("cost"),
                })
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "trades", "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_tickers_batch(exchange, symbols):
    """Batch ticker for watchlist symbols (exchanges supporting watchTickers).
    On repeated errors with the full set, progressively drops bad symbols
    rather than dying — ensures the valid symbols keep streaming.
    """
    active = list(symbols)
    errors = 0
    while active:
        try:
            tickers = await exchange.watch_tickers(active)
            errors  = 0
            for symbol, ticker in tickers.items():
                last = ticker.get("last")
                if last and _should_emit_ticker(symbol, last):
                    emit(_make_ticker_msg(ticker))
        except Exception as e:
            errors += 1
            err_str = str(e)
            if errors <= 3:
                emit({"type": "error", "source": "tickers", "message": err_str})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                # Try to identify and drop the bad symbol, then reset error count
                # so the remaining valid symbols keep streaming.
                bad = None
                for sym in active:
                    if sym in err_str:
                        bad = sym
                        break
                if bad:
                    emit({"type": "info",
                          "message": f"Dropping bad symbol from batch: {bad}"})
                    active.remove(bad)
                    errors = 0
                    continue
                # Cannot identify which symbol is bad — give up
                emit({"type": "error", "source": "tickers",
                      "message": f"Too many errors, stopping batch ticker watcher"})
                return
            await asyncio.sleep(min(errors * 2, 30))


async def watch_ticker_individual(exchange, symbol):
    """Fallback per-symbol ticker for exchanges without watchTickers (e.g. htx)."""
    errors = 0
    while True:
        try:
            ticker = await exchange.watch_ticker(symbol)
            errors = 0
            last = ticker.get("last")
            if last and _should_emit_ticker(symbol, last):
                emit(_make_ticker_msg(ticker))
        except Exception as e:
            errors += 1
            if errors <= 3:
                emit({"type": "error", "source": "ticker_individual",
                      "symbol": symbol, "message": str(e)})
            if errors >= MAX_CONSECUTIVE_ERRORS:
                return
            await asyncio.sleep(min(errors * 2, 30))


# ── Watcher supervision ────────────────────────────────────────────────────────

# A per-symbol/channel watcher loop returns after MAX_CONSECUTIVE_ERRORS consecutive
# errors. Without supervision that channel is dead for the life of the process even
# after a transient (~1 min) outage clears. supervise() re-runs the watcher after a
# capped backoff so streams self-heal, while staying safe against busy-looping:
# a watcher that dies immediately every time is retried with an increasing delay
# (up to RESPAWN_MAX_DELAY), and the delay resets once a watcher has run a while.

RESPAWN_BASE_DELAY = 5      # seconds — first respawn backoff
RESPAWN_MAX_DELAY  = 60     # seconds — cap so we never sleep too long
RESPAWN_RESET_AFTER = 120   # seconds — a watcher that ran this long resets backoff


async def supervise(name, factory):
    """Run a watcher coroutine forever, respawning it with backoff if it exits.

    `factory` is a zero-arg callable returning a fresh coroutine each time, so we
    can recreate the awaitable after one finishes (a coroutine cannot be re-awaited).
    """
    delay = RESPAWN_BASE_DELAY
    while True:
        started = time.monotonic()
        try:
            await factory()
        except asyncio.CancelledError:
            raise
        except Exception as e:
            emit({"type": "error", "source": "supervisor", "channel": name,
                  "message": f"watcher crashed: {e}"})
        # Watcher returned (gave up after consecutive errors) or crashed.
        ran_for = time.monotonic() - started
        if ran_for >= RESPAWN_RESET_AFTER:
            delay = RESPAWN_BASE_DELAY  # it was healthy for a while — reset backoff
        emit({"type": "info", "source": "supervisor", "channel": name,
              "message": f"respawning in {delay}s"})
        await asyncio.sleep(delay)
        delay = min(delay * 2, RESPAWN_MAX_DELAY)


# ── Primary-symbol task factory ────────────────────────────────────────────────

def make_primary_tasks(exchange, exchange_id, caps, resolved_primary, timeframe, ticker_cache):
    """Create the supervise()-wrapped watcher tasks for the PRIMARY symbol.

    Returns a list of asyncio.Task. The list is laid out as:
      [ (ticker_cache and/or orderbook and/or ticker), (ohlcv), (trades) ]
    so callers can cancel/recreate just the OHLCV task. The OHLCV task (if any)
    is tagged with task.ws_kind == "ohlcv" so it can be located unambiguously.

    `timeframe` is the OHLCV timeframe (e.g. "1m", "5m") — NOT hardcoded.
    """
    tasks = []

    # ── Price / orderbook / ticker-cache ──────────────────────────────────────
    if caps["watch_ticker"] and caps["watch_orderbook"]:
        # Run both: OB gives fast mid-price, ticker watcher keeps cache fresh
        tasks.append(asyncio.create_task(supervise(
            f"ticker_cache:{resolved_primary}",
            lambda: watch_ticker_and_cache(exchange, resolved_primary, ticker_cache))))
        tasks.append(asyncio.create_task(supervise(
            f"orderbook:{resolved_primary}",
            lambda: watch_orderbook(exchange, exchange_id, resolved_primary, ticker_cache))))
    elif caps["watch_orderbook"]:
        tasks.append(asyncio.create_task(supervise(
            f"orderbook:{resolved_primary}",
            lambda: watch_orderbook(exchange, exchange_id, resolved_primary, ticker_cache))))
    elif caps["watch_ticker"]:
        tasks.append(asyncio.create_task(supervise(
            f"ticker:{resolved_primary}",
            lambda: watch_ticker(exchange, resolved_primary))))

    # ── OHLCV (tagged so it can be recreated alone on a timeframe change) ──────
    if caps["watch_ohlcv"]:
        ohlcv_task = asyncio.create_task(supervise(
            f"ohlcv:{resolved_primary}",
            lambda: watch_ohlcv(exchange, resolved_primary, timeframe)))
        ohlcv_task.ws_kind = "ohlcv"
        tasks.append(ohlcv_task)

    # ── Trades ────────────────────────────────────────────────────────────────
    if caps["watch_trades"]:
        tasks.append(asyncio.create_task(supervise(
            f"trades:{resolved_primary}",
            lambda: watch_trades_stream(exchange, resolved_primary))))

    return tasks


async def _cancel_tasks(tasks):
    """Cancel the given tasks and await them so no 'pending' warnings remain."""
    for t in tasks:
        t.cancel()
    if tasks:
        await asyncio.gather(*tasks, return_exceptions=True)


async def stdin_command_loop(exchange, exchange_id, caps, ticker_cache, state):
    """Read newline-delimited JSON commands from stdin and re-point the primary
    stream WITHOUT restarting the process. Watchlist ticker tasks are untouched.

    Commands (malformed / unknown are ignored safely):
      {"cmd":"set_primary","symbol":"<unified pair>"}
      {"cmd":"set_timeframe","timeframe":"<tf>"}

    `state` is a dict carrying mutable shared state:
      state["primary"]   -> current resolved primary symbol
      state["timeframe"] -> current OHLCV timeframe
      state["tasks"]     -> list of current primary asyncio.Task

    Reads run on a DEDICATED single-thread executor (not the default one).
    sys.stdin.readline() blocks until a line or EOF, so on SIGTERM the worker stays
    parked — if it lived in asyncio's *default* executor, asyncio.run()'s
    shutdown_default_executor() would hang forever joining it. Keeping it out of the
    default executor lets shutdown complete; the parked worker is then stepped over
    by the os._exit() in __main__ instead of being joined.
    """
    loop = asyncio.get_event_loop()
    while True:
        line = await loop.run_in_executor(_stdin_executor, sys.stdin.readline)
        if line == "":
            break  # EOF — parent closed our stdin
        line = line.strip()
        if not line:
            continue
        try:
            cmd = json.loads(line)
        except Exception:
            continue  # malformed JSON — ignore
        if not isinstance(cmd, dict):
            continue

        action = cmd.get("cmd")

        # ── Re-point the primary symbol ───────────────────────────────────────
        if action == "set_primary":
            requested = cmd.get("symbol")
            if not requested or not isinstance(requested, str):
                continue
            try:
                resolved = resolve_symbol(exchange, requested, exchange_id)
            except Exception as e:
                emit({"type": "error", "source": "set_primary", "message": str(e)})
                continue
            if not resolved:
                continue  # no matching market — resolve_symbol already emitted info
            # Emit the same symbol_map shape the startup path uses so C++ remaps.
            if resolved != requested:
                emit({"type": "symbol_map", "map": {resolved: requested}})
            # Cancel the old primary tasks and spin up fresh ones at current tf.
            old = state["tasks"]
            state["tasks"] = make_primary_tasks(
                exchange, exchange_id, caps, resolved, state["timeframe"], ticker_cache)
            state["primary"] = resolved
            await _cancel_tasks(old)

        # ── Change the OHLCV timeframe only ───────────────────────────────────
        elif action == "set_timeframe":
            tf = cmd.get("timeframe")
            if not tf or not isinstance(tf, str):
                continue
            state["timeframe"] = tf
            # Recreate ONLY the ohlcv task; leave orderbook/trades/ticker running.
            if not caps["watch_ohlcv"]:
                continue
            old_ohlcv = [t for t in state["tasks"] if getattr(t, "ws_kind", None) == "ohlcv"]
            kept = [t for t in state["tasks"] if getattr(t, "ws_kind", None) != "ohlcv"]
            new_ohlcv = asyncio.create_task(supervise(
                f"ohlcv:{state['primary']}",
                lambda: watch_ohlcv(exchange, state["primary"], tf)))
            new_ohlcv.ws_kind = "ohlcv"
            state["tasks"] = kept + [new_ohlcv]
            await _cancel_tasks(old_ohlcv)

        # Unknown command — ignore silently.


# ── Main ──────────────────────────────────────────────────────────────────────

async def main():
    if len(sys.argv) < 3:
        emit({"type": "error",
              "message": "Usage: ws_stream.py <exchange_id> <symbol> [symbols...]"})
        sys.exit(1)

    exchange_id    = sys.argv[1]
    primary_symbol = sys.argv[2]
    all_symbols    = sys.argv[2:]

    # Graceful shutdown on SIGTERM/SIGINT: cancel THIS coroutine so the finally
    # block below tears every watcher down cleanly (cancel + await) before closing
    # the exchange. Doing sys.exit() from a signal handler while the event loop is
    # in select() unwinds the loop mid-flight and makes ccxt.pro's WS read callbacks
    # dump CancelledError tracebacks to stdout — handling it inside the loop avoids
    # that entirely. add_signal_handler is POSIX-only; on Windows we fall back to
    # the plain signal handler installed in __main__.
    main_task = asyncio.current_task()
    loop = asyncio.get_event_loop()
    for sig in (getattr(signal, "SIGTERM", None), getattr(signal, "SIGINT", None)):
        if sig is None:
            continue
        try:
            loop.add_signal_handler(sig, main_task.cancel)
        except (NotImplementedError, RuntimeError, ValueError):
            pass  # not supported on this platform/loop — __main__ handler covers it

    try:
        import ccxt.pro as ccxtpro
    except ImportError:
        emit({"type": "error",
              "message": "ccxt.pro not available — install with: pip install ccxt"})
        sys.exit(1)

    exchange_class = getattr(ccxtpro, exchange_id, None)
    if not exchange_class:
        emit({"type": "error",
              "message": f"Exchange '{exchange_id}' not found in ccxt.pro"})
        sys.exit(1)

    # Hyperliquid is a perps DEX — its primary market type is "swap".
    # All other exchanges default to "spot".
    default_type = "swap" if exchange_id in ("hyperliquid",) else "spot"
    exchange = exchange_class({
        "enableRateLimit": True,
        "timeout":         30000,
        "options":         {"defaultType": default_type},
    })

    # Windows DNS fix: aiohttp's default resolver (aiodns/pycares) cannot read
    # the system DNS config on Windows, so EVERY ccxt.pro REST + WS call fails
    # with ExchangeNotAvailable ("Could not contact DNS servers") — even though
    # the synchronous REST daemon (which uses requests' system resolver) works
    # fine. Force aiohttp's ThreadedResolver (same getaddrinfo path) so all
    # daemon exchanges resolve. ccxt.pro reuses this session for the WS upgrade
    # too. Gated to Windows so Linux/macOS keep the faster native resolver.
    if sys.platform == "win32":
        import aiohttp
        exchange.session = aiohttp.ClientSession(
            connector=aiohttp.TCPConnector(resolver=aiohttp.ThreadedResolver(), ttl_dns_cache=300),
            trust_env=True,
        )

    # IMPORTANT: Always call load_markets() — do NOT restore from a disk cache by
    # assigning exchange.markets directly. That leaves ccxt.pro's internal indexes
    # (currencies, safeMarket lookups) incomplete, causing "NoneType has no len()"
    # on the first watch_* call.
    MAX_RETRIES = 5
    for attempt in range(1, MAX_RETRIES + 1):
        try:
            emit({"type": "status", "connected": False, "exchange": exchange_id,
                  "symbols": all_symbols,
                  "message": f"Loading markets (attempt {attempt}/{MAX_RETRIES})..."})
            await exchange.load_markets()
            emit({"type": "status", "connected": True,
                  "exchange": exchange_id, "symbols": all_symbols})
            break
        except Exception as e:
            emit({"type": "error",
                  "message": f"load_markets attempt {attempt} failed: {e}"})
            if attempt < MAX_RETRIES:
                delay = min(attempt * 3, 15)
                await asyncio.sleep(delay)
    else:
        emit({"type": "status", "connected": False, "exchange": exchange_id,
              "symbols": all_symbols, "message": "load_markets failed — giving up"})
        await exchange.close()
        sys.exit(1)

    caps = get_exchange_caps(exchange)

    # Resolve all requested symbols to valid unified symbols for this exchange.
    # resolve_symbol() returns None when a symbol has no matching market — skip those.
    # Build a reverse map: exchange_symbol → requested_symbol so C++ can remap tickers.
    original_symbols = list(all_symbols)
    primary_symbol   = resolve_symbol(exchange, primary_symbol, exchange_id) or primary_symbol
    resolved         = [resolve_symbol(exchange, s, exchange_id) for s in original_symbols]

    # symbol_map: {exchange_symbol: requested_symbol} — only for remapped symbols
    symbol_map = {}
    seen = set()
    all_symbols = []
    for orig, res in zip(original_symbols, resolved):
        if res and res not in seen:
            seen.add(res)
            all_symbols.append(res)
            if res != orig:
                symbol_map[res] = orig  # e.g. "BTC/USDC:USDC" → "BTC/USDT"
    if primary_symbol not in seen:
        all_symbols.insert(0, primary_symbol)

    # Emit the symbol map so C++ can remap incoming tickers to the originally
    # requested symbol names (important for exchanges like Hyperliquid where
    # BTC/USDT resolves to BTC/USDC:USDC).
    if symbol_map:
        emit({"type": "symbol_map", "map": symbol_map})

    # Shared ticker cache — OB watcher reads this to enrich fast mid-price tickers
    ticker_cache = {}

    # Mutable state shared with the stdin command handler so a running stream can
    # re-point its primary symbol / OHLCV timeframe without restarting the process.
    state = {
        "primary":   primary_symbol,
        "timeframe": "1m",
        "tasks":     [],   # current PRIMARY tasks (replaced on set_primary/set_timeframe)
    }

    watchlist_tasks = []
    stdin_task      = None

    try:
        # Each watcher is wrapped in supervise() so a channel that dies after a
        # transient outage (MAX_CONSECUTIVE_ERRORS) is respawned with backoff
        # instead of staying dead for the life of the process.

        # ── Primary symbol (via the shared factory) ───────────────────────────
        state["tasks"] = make_primary_tasks(
            exchange, exchange_id, caps, state["primary"], state["timeframe"], ticker_cache)

        # ── Watchlist symbols (excluding primary) ─────────────────────────────
        # These stay connected for the whole life of the process — set_primary /
        # set_timeframe never touch them.
        other_symbols = [s for s in all_symbols if s != primary_symbol]
        if other_symbols:
            if caps["watch_tickers"]:
                watchlist_tasks.append(asyncio.create_task(supervise(
                    "tickers_batch",
                    lambda: watch_tickers_batch(exchange, other_symbols))))
            elif caps["watch_ticker"]:
                for sym in other_symbols:
                    # Bind sym per-iteration via default arg to avoid late-binding closure bug
                    watchlist_tasks.append(asyncio.create_task(supervise(
                        f"ticker_individual:{sym}",
                        lambda s=sym: watch_ticker_individual(exchange, s))))

        if not state["tasks"] and not watchlist_tasks:
            emit({"type": "error",
                  "message": f"Exchange '{exchange_id}' has no usable watch methods"})
            await exchange.close()
            sys.exit(1)

        # The stdin loop runs until EOF (parent closes our stdin). Watchlist tasks
        # run forever via supervise(). Primary tasks live in state["tasks"] and are
        # swapped out on commands; they are NOT awaited here so cancelling them does
        # not tear down the process. We await the stdin loop + watchlist tasks; the
        # process stays alive as long as any of these is running.
        stdin_task = asyncio.create_task(
            stdin_command_loop(exchange, exchange_id, caps, ticker_cache, state))
        await asyncio.gather(stdin_task, *watchlist_tasks)

    except asyncio.CancelledError:
        pass
    except KeyboardInterrupt:
        pass
    except Exception as e:
        emit({"type": "error", "message": f"Fatal: {e}"})
        traceback.print_exc(file=sys.stderr)
    finally:
        # Tear down every still-running task (primary + watchlist + stdin) cleanly
        # so there are no "Task was destroyed but it is pending" warnings on shutdown.
        teardown = list(state["tasks"]) + watchlist_tasks
        if stdin_task is not None:
            teardown.append(stdin_task)
        await _cancel_tasks(teardown)
        emit({"type": "status", "connected": False,
              "exchange": exchange_id, "symbols": all_symbols})
        await exchange.close()


if __name__ == "__main__":
    # Fallback SIGTERM handler for platforms where loop.add_signal_handler() is
    # unavailable (Windows). On POSIX, main() installs an asyncio signal handler
    # that supersedes this one, giving a clean cancel-and-await shutdown instead of
    # raising SystemExit from inside the event loop's select().
    try:
        signal.signal(signal.SIGTERM, lambda s, f: sys.exit(0))
    except Exception:
        pass
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
    # main() shut down cleanly (flushing its final status line as it went). The
    # stdin reader runs in a default-executor worker thread that may still be parked
    # in a blocking sys.stdin.readline(); that non-daemon thread would otherwise
    # block interpreter exit until the parent closes our stdin. os._exit() bypasses
    # the thread join (and atexit) — safe here because every emit() already flushed.
    sys.stdout.flush()
    os._exit(0)
