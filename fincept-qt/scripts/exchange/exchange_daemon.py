"""
Persistent exchange daemon — long-running Python worker process.

Keeps ccxt loaded and exchange instances warm. Accepts JSON-RPC requests
on stdin, returns JSON responses on stdout. Eliminates 600-1200ms Python
startup overhead per request (interpreter + ccxt import + exchange init).

Protocol:
  - C++ writes one JSON line to stdin per request
  - Python writes one JSON line to stdout per response
  - Each request has an "id" field for correlation
  - The daemon runs until stdin is closed or SIGTERM is received

Request format:
  {"id":"req_1", "method":"fetch_ticker", "exchange":"kraken", "args":{"symbol":"BTC/USDT"}}

Response format:
  {"id":"req_1", "success":true, "data":{...}}
  {"id":"req_1", "success":false, "error":"...", "code":"EXCHANGE_ERROR"}

Supported methods:
  fetch_ticker, fetch_tickers, fetch_orderbook, fetch_ohlcv, fetch_markets,
  fetch_trades, fetch_funding_rate, fetch_open_interest,
  fetch_balance, fetch_positions, fetch_open_orders,
  place_order, cancel_order, list_exchange_ids,
  set_credentials, ping
"""

import json
import os
import sys
import signal
import time
import traceback

# Ensure this directory is on path for exchange_client imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ccxt
from exchange_client import load_cached_markets, save_markets_cache, get_default_type

# Try orjson for faster serialization
try:
    import orjson

    def _dumps(obj):
        return orjson.dumps(obj, default=str).decode()
except ImportError:
    def _dumps(obj):
        return json.dumps(obj, default=str, separators=(",", ":"))

# Direct stdout access for speed
_stdout_write = sys.stdout.write
_stdout_flush = sys.stdout.flush


def _respond(req_id, success, data=None, error=None, code=None):
    """Write a JSON response line to stdout."""
    resp = {"id": req_id, "success": success}
    if data is not None:
        resp["data"] = data
    if error is not None:
        resp["error"] = error
    if code is not None:
        resp["code"] = code
    try:
        _stdout_write(_dumps(resp))
        _stdout_write("\n")
        _stdout_flush()
    except Exception:
        pass


# ── Exchange instance pool ──────────────────────────────────────────────────
# Keyed by (exchange_id, has_credentials) — reuse across requests.
# ccxt instances with credentials differ from public ones.

_exchange_pool = {}      # key -> ccxt.Exchange
_credentials = {}        # exchange_id -> dict


def _get_exchange(exchange_id, need_auth=False):
    """Get or create a cached exchange instance."""
    key = (exchange_id, need_auth)
    if key in _exchange_pool:
        return _exchange_pool[key]

    if exchange_id not in ccxt.exchanges:
        raise ValueError(f"Unknown exchange: {exchange_id}")

    config = {
        "enableRateLimit": True,
        "timeout": 15000,
        "options": {"defaultType": get_default_type(exchange_id)},
    }

    if need_auth and exchange_id in _credentials:
        creds = _credentials[exchange_id]
        if creds.get("api_key"):
            config["apiKey"] = creds["api_key"]
        if creds.get("secret"):
            config["secret"] = creds["secret"]
        if creds.get("password"):
            config["password"] = creds["password"]

    exchange_class = getattr(ccxt, exchange_id)
    exchange = exchange_class(config)

    # Try to load markets from disk cache (avoids 3-10s network call).
    # Use load_markets(reload=False) with a pre-populated cache file — this lets
    # ccxt rebuild all internal indexes (markets_by_id, currencies, ids) correctly
    # rather than assigning exchange.markets directly which leaves indexes incomplete.
    cached = load_cached_markets(exchange_id)
    if cached is not None:
        try:
            exchange.markets = cached
            exchange.set_markets(cached)
        except Exception:
            pass  # Fall through — will load on demand if needed

    _exchange_pool[key] = exchange
    return exchange


def _ensure_markets(exchange, exchange_id):
    """Load markets if not yet loaded."""
    if not exchange.markets:
        exchange.load_markets()
        save_markets_cache(exchange_id, exchange.markets)


# ── Method handlers ─────────────────────────────────────────────────────────

def _handle_ping(req_id, exchange_id, args):
    _respond(req_id, True, {"pong": True, "time": time.time()})


def _handle_set_credentials(req_id, exchange_id, args):
    """Store credentials in memory for this session."""
    _credentials[exchange_id] = args
    # Invalidate any existing authenticated exchange instance
    key = (exchange_id, True)
    if key in _exchange_pool:
        del _exchange_pool[key]
    _respond(req_id, True, {"stored": True})


def _handle_fetch_ticker(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    ticker = ex.fetch_ticker(args["symbol"])
    _respond(req_id, True, {
        "symbol": ticker.get("symbol"),
        "last": ticker.get("last"),
        "bid": ticker.get("bid"),
        "ask": ticker.get("ask"),
        "high": ticker.get("high"),
        "low": ticker.get("low"),
        "open": ticker.get("open"),
        "close": ticker.get("close"),
        "change": ticker.get("change"),
        "percentage": ticker.get("percentage"),
        "baseVolume": ticker.get("baseVolume"),
        "quoteVolume": ticker.get("quoteVolume"),
        "timestamp": ticker.get("timestamp"),
    })


def _handle_fetch_tickers(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbols = args.get("symbols", [])
    tickers = ex.fetch_tickers(symbols) if symbols else ex.fetch_tickers()
    result = []
    for sym, t in tickers.items():
        result.append({
            "symbol": t.get("symbol"),
            "last": t.get("last"),
            "bid": t.get("bid"),
            "ask": t.get("ask"),
            "high": t.get("high"),
            "low": t.get("low"),
            "open": t.get("open"),
            "close": t.get("close"),
            "change": t.get("change"),
            "percentage": t.get("percentage"),
            "baseVolume": t.get("baseVolume"),
            "quoteVolume": t.get("quoteVolume"),
            "timestamp": t.get("timestamp"),
        })
    _respond(req_id, True, {"tickers": result})


def _handle_fetch_orderbook(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    limit = args.get("limit", 20)
    ob = ex.fetch_order_book(args["symbol"], limit)
    bids = [[b[0], b[1]] for b in ob.get("bids", [])[:limit]]
    asks = [[a[0], a[1]] for a in ob.get("asks", [])[:limit]]
    best_bid = bids[0][0] if bids else 0
    best_ask = asks[0][0] if asks else 0
    spread = best_ask - best_bid if best_bid and best_ask else 0
    spread_pct = (spread / best_ask * 100) if best_ask else 0
    _respond(req_id, True, {
        "symbol": args["symbol"],
        "bids": bids, "asks": asks,
        "best_bid": best_bid, "best_ask": best_ask,
        "spread": spread, "spread_pct": spread_pct,
    })


def _handle_fetch_ohlcv(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbol = args["symbol"]
    timeframe = args.get("timeframe", "1h")
    limit = args.get("limit", 100)
    candles_raw = ex.fetch_ohlcv(symbol, timeframe, limit=limit)
    candles = []
    for c in candles_raw:
        candles.append({
            "timestamp": c[0], "open": c[1], "high": c[2],
            "low": c[3], "close": c[4], "volume": c[5],
        })
    _respond(req_id, True, {"candles": candles})


def _handle_fetch_markets(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    _ensure_markets(ex, exchange_id)
    market_type = args.get("type")
    query       = args.get("query", "").upper().strip()  # server-side filter
    limit       = int(args.get("limit", 0))              # 0 = no limit
    markets = []
    for sym, m in ex.markets.items():
        if market_type and m.get("type") != market_type:
            continue
        if not m.get("active", True):
            continue
        if query:
            sym_upper  = m.get("symbol", "").upper()
            base_upper = m.get("base", "").upper()
            if query not in sym_upper and query not in base_upper:
                continue
        limits = m.get("limits", {})
        markets.append({
            "symbol": m["symbol"],
            "base": m.get("base"), "quote": m.get("quote"),
            "type": m.get("type"), "active": m.get("active"),
            "maker": m.get("maker"), "taker": m.get("taker"),
            "minAmount": limits.get("amount", {}).get("min"),
            "minCost":   limits.get("cost", {}).get("min"),
        })
        if limit and len(markets) >= limit:
            break
    _respond(req_id, True, {"markets": markets, "count": len(markets)})


def _handle_fetch_trades(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbol = args["symbol"]
    limit = args.get("limit", 50)
    trades = ex.fetch_trades(symbol, limit=limit)
    result = []
    for t in trades:
        result.append({
            "id": t.get("id"), "symbol": t.get("symbol"),
            "side": t.get("side"), "price": t.get("price"),
            "amount": t.get("amount"), "cost": t.get("cost"),
            "timestamp": t.get("timestamp"),
        })
    _respond(req_id, True, {"trades": result})


def _handle_fetch_funding_rate(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbol = args["symbol"]
    fr = ex.fetch_funding_rate(symbol)
    _respond(req_id, True, {
        "symbol": fr.get("symbol"),
        "funding_rate": fr.get("fundingRate"),
        "mark_price": fr.get("markPrice"),
        "index_price": fr.get("indexPrice"),
        "funding_timestamp": fr.get("fundingTimestamp"),
        "next_funding_timestamp": fr.get("nextFundingTimestamp"),
    })


def _handle_fetch_open_interest(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbol = args["symbol"]
    oi = ex.fetch_open_interest(symbol)
    _respond(req_id, True, {
        "symbol": oi.get("symbol"),
        "open_interest": oi.get("openInterest"),
        "open_interest_value": oi.get("openInterestValue"),
        "timestamp": oi.get("timestamp"),
    })


def _handle_list_exchange_ids(req_id, exchange_id, args):
    _respond(req_id, True, {"exchanges": ccxt.exchanges})


# ── Authenticated handlers ──────────────────────────────────────────────────

def _handle_fetch_balance(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    bal = ex.fetch_balance()
    # Filter out zero balances
    non_zero = {}
    for currency, data in bal.get("total", {}).items():
        if data and float(data) > 0:
            non_zero[currency] = {
                "free": bal.get("free", {}).get(currency, 0),
                "used": bal.get("used", {}).get(currency, 0),
                "total": data,
            }
    _respond(req_id, True, {"balances": non_zero})


def _handle_place_order(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    _ensure_markets(ex, exchange_id)
    order = ex.create_order(
        args["symbol"], args["type"], args["side"],
        args["amount"], args.get("price"),
    )
    _respond(req_id, True, {
        "id": order.get("id"),
        "symbol": order.get("symbol"),
        "side": order.get("side"),
        "type": order.get("type"),
        "amount": order.get("amount"),
        "price": order.get("price"),
        "status": order.get("status"),
        "filled": order.get("filled"),
        "remaining": order.get("remaining"),
        "cost": order.get("cost"),
        "average": order.get("average"),
        "fee": order.get("fee"),
        "timestamp": order.get("timestamp"),
        "exchange": exchange_id,
    })


def _handle_cancel_order(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    result = ex.cancel_order(args["order_id"], args.get("symbol"))
    _respond(req_id, True, {
        "id": result.get("id", args["order_id"]),
        "symbol": result.get("symbol", args.get("symbol")),
        "status": result.get("status", "canceled"),
        "exchange": exchange_id,
    })


def _handle_fetch_positions(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = args.get("symbol")
    positions = ex.fetch_positions([symbol] if symbol else None)
    result = [p for p in positions if abs(p.get("contracts", 0)) > 0]
    _respond(req_id, True, {"positions": result})


def _handle_fetch_open_orders(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = args.get("symbol")
    orders = ex.fetch_open_orders(symbol)
    _respond(req_id, True, {"orders": orders})


def _handle_fetch_mark_price(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbol = args["symbol"]
    try:
        params = {"defaultType": "swap"}
        rate = ex.fetch_funding_rate(symbol, params=params)
        _respond(req_id, True, {
            "symbol": symbol,
            "mark_price": rate.get("markPrice"),
            "index_price": rate.get("indexPrice"),
            "timestamp": rate.get("timestamp"),
        })
    except Exception as e:
        _respond(req_id, False, error=f"Mark price not available: {e}", code="NOT_SUPPORTED")


def _handle_fetch_my_trades(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = args.get("symbol")
    limit = int(args.get("limit", 50))
    trades = ex.fetch_my_trades(symbol, limit=limit)
    _respond(req_id, True, {
        "trades": [
            {
                "id": t.get("id"),
                "order": t.get("order"),
                "symbol": t.get("symbol"),
                "side": t.get("side"),
                "price": t.get("price"),
                "amount": t.get("amount"),
                "cost": t.get("cost"),
                "fee": t.get("fee", {}).get("cost", 0),
                "fee_currency": t.get("fee", {}).get("currency", ""),
                "timestamp": t.get("timestamp"),
            }
            for t in trades
        ],
        "count": len(trades),
    })


def _handle_fetch_trading_fees(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbol = args.get("symbol")
    if symbol:
        fee = ex.fetch_trading_fee(symbol)
        _respond(req_id, True, {
            "symbol": symbol,
            "maker": fee.get("maker"),
            "taker": fee.get("taker"),
            "percentage": fee.get("percentage", True),
        })
    else:
        fees = ex.fetch_trading_fees()
        result = []
        for sym, fee in list(fees.items())[:50]:
            if isinstance(fee, dict) and "maker" in fee:
                result.append({
                    "symbol": sym,
                    "maker": fee.get("maker"),
                    "taker": fee.get("taker"),
                })
        _respond(req_id, True, {"fees": result, "count": len(result)})


def _handle_set_leverage(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = args["symbol"]
    leverage = int(args["leverage"])
    # Pass defaultType via params so we don't mutate the shared pooled instance
    result = ex.set_leverage(leverage, symbol, params={"defaultType": "swap"})
    _respond(req_id, True, {"symbol": symbol, "leverage": leverage, "result": result})


def _handle_set_margin_mode(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = args["symbol"]
    mode = args["mode"]  # "cross" or "isolated"
    result = ex.set_margin_mode(mode, symbol, params={"defaultType": "swap"})
    _respond(req_id, True, {"symbol": symbol, "margin_mode": mode, "result": result})


# ── Method dispatch table ───────────────────────────────────────────────────

_METHODS = {
    "ping": _handle_ping,
    "set_credentials": _handle_set_credentials,
    "fetch_ticker": _handle_fetch_ticker,
    "fetch_tickers": _handle_fetch_tickers,
    "fetch_orderbook": _handle_fetch_orderbook,
    "fetch_ohlcv": _handle_fetch_ohlcv,
    "fetch_markets": _handle_fetch_markets,
    "fetch_trades": _handle_fetch_trades,
    "fetch_funding_rate": _handle_fetch_funding_rate,
    "fetch_open_interest": _handle_fetch_open_interest,
    "list_exchange_ids": _handle_list_exchange_ids,
    "fetch_balance": _handle_fetch_balance,
    "place_order": _handle_place_order,
    "cancel_order": _handle_cancel_order,
    "fetch_positions": _handle_fetch_positions,
    "fetch_open_orders": _handle_fetch_open_orders,
    "fetch_mark_price": _handle_fetch_mark_price,
    "fetch_my_trades": _handle_fetch_my_trades,
    "fetch_trading_fees": _handle_fetch_trading_fees,
    "set_leverage": _handle_set_leverage,
    "set_margin_mode": _handle_set_margin_mode,
}


# ── Error normalization (matches exchange_client.py patterns) ───────────────

_CCXT_ERROR_MAP = {
    ccxt.AuthenticationError: "AUTH_ERROR",
    ccxt.InsufficientFunds: "INSUFFICIENT_FUNDS",
    ccxt.InvalidOrder: "INVALID_ORDER",
    ccxt.OrderNotFound: "ORDER_NOT_FOUND",
    ccxt.RateLimitExceeded: "RATE_LIMIT",
    ccxt.NetworkError: "NETWORK_ERROR",
    ccxt.ExchangeNotAvailable: "EXCHANGE_UNAVAILABLE",
    ccxt.ExchangeError: "EXCHANGE_ERROR",
}


def _classify_error(exc):
    """Map a ccxt exception to an error code."""
    for exc_type, code in _CCXT_ERROR_MAP.items():
        if isinstance(exc, exc_type):
            return code
    return "UNKNOWN_ERROR"


# ── Main event loop ─────────────────────────────────────────────────────────

def main():
    # Signal readiness
    _respond("__init__", True, {"status": "ready", "pid": os.getpid()})

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        req_id = None
        try:
            req = json.loads(line)
            req_id = req.get("id", "__unknown__")
            method = req.get("method")
            exchange_id = req.get("exchange", "")
            args = req.get("args", {})

            handler = _METHODS.get(method)
            if not handler:
                _respond(req_id, False, error=f"Unknown method: {method}", code="INVALID_METHOD")
                continue

            handler(req_id, exchange_id, args)

        except json.JSONDecodeError as e:
            _respond(req_id or "__parse_error__", False,
                     error=f"JSON parse error: {e}", code="PARSE_ERROR")
        except Exception as e:
            code = _classify_error(e)
            _respond(req_id or "__unknown__", False,
                     error=str(e), code=code)


if __name__ == "__main__":
    try:
        signal.signal(signal.SIGTERM, lambda s, f: sys.exit(0))
    except Exception:
        pass

    try:
        main()
    except (KeyboardInterrupt, EOFError):
        pass
