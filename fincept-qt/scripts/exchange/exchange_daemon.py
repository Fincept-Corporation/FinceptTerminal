"""
Persistent exchange daemon — long-running Python worker process.

Keeps ccxt loaded and exchange instances warm. Accepts JSON-RPC requests
on stdin, returns JSON responses on stdout. Eliminates 600-1200ms Python
startup overhead per request (interpreter + ccxt import + exchange init).

Protocol:
  - C++ writes one JSON line to stdin per request
  - Python writes one JSON line to stdout per response
  - Each request has an "id" field for correlation; replies can arrive in any
    order (requests for different exchanges run in parallel, see below)
  - The daemon runs until stdin is closed or SIGTERM is received

Request format:
  {"id":"req_1", "method":"fetch_ticker", "exchange":"kraken", "args":{"symbol":"BTC/USDT"},
   "timeout_ms":15000}
  "timeout_ms" (optional) is how long the caller waits for the reply. A request
  that has waited in its queue past that budget is answered EXPIRED without
  being sent — and an order-changing request is only sent while enough of the
  budget is left for its reply to arrive in time. That is checked again right
  before every HTTP request the account-changing call makes (ccxt's own
  pre-calls, rate-limit sleeps and market loading included).

Response format:
  {"id":"req_1", "success":true, "data":{...}}
  {"id":"req_1", "success":false, "error":"...", "code":"EXCHANGE_ERROR"}
  Account-changing methods (place_order, cancel_order, set_leverage,
  set_margin_mode) also carry "outcome" — "accepted" / "rejected" (definitely
  not done: refused before sending, or a definitive exchange rejection) /
  "unknown" (it may have happened: never re-send, look it up).

Orders: the app first calls prepare_order (nothing is sent) and shows the user
its result — the exact market, the size in the exchange's own units, the
price, the parameters and a confirm_hash. place_order rebuilds the same
request from the same inputs, refuses if it no longer hashes the same (the
user would be confirming something else), and only then sends it.

Threading: one worker thread per exchange id. A ccxt exchange object is not
thread-safe, so every request for one exchange (public and authenticated
client, credentials included) runs on that exchange's worker, in arrival
order; a stalled exchange only delays its own requests. stdout writes are
serialised by a lock. ping and list_exchange_ids touch no exchange and are
answered on the reader thread.

Supported methods:
  fetch_ticker, fetch_tickers, fetch_orderbook, fetch_ohlcv, fetch_markets,
  fetch_trades, fetch_funding_rate, fetch_open_interest, fetch_mark_price,
  fetch_balance, fetch_positions, fetch_open_orders, fetch_my_trades,
  fetch_trading_fees, prepare_order, place_order, find_order, cancel_order,
  set_leverage, set_margin_mode, fetch_leverage_info, warm,
  list_exchange_ids, set_credentials, ping
"""

import hashlib
import json
import math
import os
import queue
import sys
import signal
import threading
import time
import uuid

# The pipe to the terminal carries UTF-8 both ways. Without this, a Windows pipe
# uses the ANSI code page: a reply holding a CJK market symbol (Binance, MEXC,
# Bitget list some) raised UnicodeEncodeError and was never written, so the
# caller waited out its whole budget. Must run before anything is written.
for _stream in (sys.stdin, sys.stdout):
    try:
        _stream.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

# Ensure this directory is on path for exchange_client imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ccxt
from exchange_client import (load_cached_market_data, save_markets_cache, get_default_type,
                             apply_cached_markets, install_get_retry,
                             resolve_market, market_descriptor, market_kind, order_capabilities,
                             client_order_id, same_client_order_id, credential_fingerprint,
                             account_label, scrub_message, OrderRefused, USD_QUOTES,
                             PREFERRED_USD_QUOTES, MARKET_SLIPPAGE, PERPS_VENUES, stop_params,
                             order_size_limits)

# Kept for scripts that read them from here.
_USD_QUOTES = USD_QUOTES
_PREFERRED_USD_QUOTES = PREFERRED_USD_QUOTES


def _finite(obj):
    """Copy of `obj` with every NaN / +-Infinity float replaced by None."""
    if isinstance(obj, float):
        return obj if math.isfinite(obj) else None
    if isinstance(obj, dict):
        return {k: _finite(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_finite(v) for v in obj]
    return obj


def _dumps_std(obj):
    # allow_nan=False after _finite(): a NaN that slipped through raises here
    # instead of reaching the C++ parser as an invalid token.
    return json.dumps(_finite(obj), default=str, separators=(",", ":"), allow_nan=False)


# orjson is faster and already writes NaN / Infinity as null. It refuses
# non-string dict keys (allowed by the option) and integers beyond 64 bits; the
# standard encoder handles those.
try:
    import orjson

    def _dumps(obj):
        try:
            return orjson.dumps(obj, default=str, option=orjson.OPT_NON_STR_KEYS).decode()
        except TypeError:
            return _dumps_std(obj)
except ImportError:
    _dumps = _dumps_std


_out_lock = threading.RLock()


def _write_line(text):
    with _out_lock:
        sys.stdout.write(text)
        sys.stdout.write("\n")
        sys.stdout.flush()


def _diag(message):
    try:
        sys.stderr.write(f"[exchange_daemon] {message}\n")
        sys.stderr.flush()
    except Exception:
        pass


def _respond_unwritable(req_id, message, extra=None):
    """A reply could not be serialised or written: answer the id with an
    ASCII-only error line (json.dumps escapes every non-ASCII character), so the
    caller learns at once instead of waiting out its timeout. For an order the
    outcome is then unknown — it may have been placed."""
    resp = {"id": req_id, "success": False, "error": str(message)[:500], "code": "ENCODE_ERROR"}
    if extra and extra.get("outcome"):
        resp["outcome"] = "unknown"
    line = json.dumps(resp, default=str)
    try:
        with _out_lock:
            # The leading newline ends any partial line the failed write left
            # behind; the reader skips the empty line.
            sys.stdout.write("\n" + line + "\n")
            sys.stdout.flush()
    except Exception as e:
        _diag(f"could not write a reply for id {req_id!r}: {e}")


def _respond(req_id, success, data=None, error=None, code=None, extra=None):
    """Write a JSON response line to stdout (any thread). `extra` keys are added
    at the top level of the reply (e.g. "outcome" on a failed order)."""
    resp = {"id": req_id, "success": success}
    if data is not None:
        resp["data"] = data
    if error is not None:
        resp["error"] = error
    if code is not None:
        resp["code"] = code
    if extra:
        for k, v in extra.items():
            resp.setdefault(k, v)
    try:
        line = _dumps(resp)
    except Exception as e:
        _respond_unwritable(req_id, f"reply could not be encoded: {e}",
                            extra or ({"outcome": "x"} if isinstance(data, dict) and "outcome" in data else None))
        return
    try:
        _write_line(line)
    except Exception as e:
        _respond_unwritable(req_id, f"reply could not be written: {e}",
                            extra or ({"outcome": "x"} if isinstance(data, dict) and "outcome" in data else None))


class _InvalidArgs(ValueError):
    """A request is missing an argument or has one of the wrong type."""


class _DeadlineExpired(Exception):
    """The caller's budget ran out before an account-changing request could be
    sent; it was NOT sent."""


def _arg(args, key):
    value = args.get(key)
    if value is None or (isinstance(value, str) and not value.strip()):
        raise _InvalidArgs(f"missing required argument '{key}'")
    return value


def _require_cap(req_id, ex, cap, exchange_id, *alternatives):
    if not any(ex.has.get(c) for c in (cap,) + alternatives):
        extra = {"outcome": "rejected", "sent": False} \
            if getattr(_call_state, "method", None) in _ACCOUNT_CHANGING else None
        _respond(req_id, False, error=f"{exchange_id} does not support {cap}", code="NOT_SUPPORTED", extra=extra)
        return False
    return True


# ── Per-call state (thread-local: one worker thread per exchange) ───────────
# expires_at   monotonic deadline of the caller (None = none given)
# armed        True inside a send window: the span in which an account-changing
#              request may leave the process
# deadline     no HTTP request may START after this while armed
# mutating     account-changing HTTP requests started inside the window
_call_state = threading.local()


def _read_only_request(exchange_id, url, method):
    """A request that cannot change the account (Hyperliquid reads are POSTs
    to /info; everything else reads with GET)."""
    m = (method or "GET").upper()
    if m in ("GET", "HEAD"):
        return True
    return exchange_id == "hyperliquid" and str(url).split("?")[0].rstrip("/").endswith("/info")


def _install_send_guard(exchange, exchange_id):
    """Wrap exchange.fetch: inside a send window, refuse to START any HTTP
    request after the deadline (so nothing account-changing leaves after the
    caller gave up) and count the requests that could change the account."""
    inner = exchange.fetch

    def fetch(url, method="GET", headers=None, body=None):
        if getattr(_call_state, "armed", False):
            deadline = getattr(_call_state, "deadline", None)
            if deadline is not None and time.monotonic() > deadline:
                raise _DeadlineExpired(f"{exchange_id}: the request ran out of time before it could be "
                                       f"sent and was not sent")
            if not _read_only_request(exchange_id, url, method):
                _call_state.mutating = getattr(_call_state, "mutating", 0) + 1
        return inner(url, method, headers, body)

    exchange.fetch = fetch


class _SendWindow:
    """with _SendWindow(headroom): ...  — the account-changing call runs inside.
    No request starts once fewer than `headroom` seconds of the caller's budget
    are left; the HTTP timeout is raised to `http_timeout_ms` for the call."""

    def __init__(self, exchange, headroom, http_timeout_ms=None):
        self.exchange = exchange
        self.headroom = headroom
        self.http_timeout_ms = http_timeout_ms
        self.saved_timeout = None

    def __enter__(self):
        expires_at = getattr(_call_state, "expires_at", None)
        deadline = None if expires_at is None else expires_at - self.headroom
        if deadline is not None and time.monotonic() > deadline:
            raise _DeadlineExpired(f"{self.exchange.id}: the request ran out of time before it could be "
                                   f"sent and was not sent")
        _call_state.deadline = deadline
        _call_state.mutating = 0
        _call_state.armed = True
        if self.http_timeout_ms:
            self.saved_timeout = self.exchange.timeout
            self.exchange.timeout = self.http_timeout_ms
        return self

    def __exit__(self, *exc):
        _call_state.armed = False
        _call_state.deadline = None
        if self.saved_timeout is not None:
            self.exchange.timeout = self.saved_timeout
        return False


# ── Exchange instance pool ──────────────────────────────────────────────────
# Keyed by (exchange_id, has_credentials) — reuse across requests.
# ccxt instances with credentials differ from public ones. Only the worker
# thread of `exchange_id` reads or writes the entries for that exchange.

_exchange_pool = {}      # key -> ccxt.Exchange
_credentials = {}        # exchange_id -> dict

# Fields of a set_credentials payload that carry an actual credential.
_CREDENTIAL_FIELDS = ("api_key", "secret", "password", "wallet_address", "private_key")

# Per-request HTTP timeout. 4.5 s with up to 3 attempts on a hung GET
# (install_get_retry) stays inside the C++ caller's 15 s budget. An order POST
# gets longer (see _ORDER_HTTP_TIMEOUT_MS): a slow but successful order reply
# would otherwise be reported "not confirmed".
_HTTP_TIMEOUT_MS = 4500
_ORDER_HTTP_TIMEOUT_MS = 10000


def _exchange_options(exchange_id):
    options = {"defaultType": get_default_type(exchange_id)}
    if exchange_id == "hyperliquid":
        # ccxt signs and sends account actions of its own before the first
        # order — approveBuilderFee (ccxt's builder address, 0.01 % max fee) and
        # setReferrer "CCXT1" — and then attaches that builder fee to every
        # order. The user asked for none of it. refSet=True makes set_ref() a
        # no-op. No option stops the builder: builderFee=False still approves a
        # 0 % builder and attaches {"b": ..., "f": 0} (hyperliquid.py
        # handle_builder_fee_approval / create_orders_request), so approval is
        # also overridden on the instance (_disable_hyperliquid_builder) and
        # approvedBuilderFee stays False — the order action then carries no
        # "builder" field at all.
        options.update({"refSet": True, "builderFee": False, "approvedBuilderFee": False,
                        "defaultSlippage": float(MARKET_SLIPPAGE)})
    return options


def _disable_hyperliquid_builder(exchange):
    exchange.handle_builder_fee_approval = lambda *a, **k: True
    exchange.options["approvedBuilderFee"] = False
    exchange.options["refSet"] = True
    exchange.options["builderFee"] = False


def _get_exchange(exchange_id, need_auth=False):
    """Get or create a cached exchange instance."""
    if need_auth and exchange_id not in _credentials:
        # Never build an unauthenticated client for a private call: after the
        # user CLEARs their keys every authenticated request must fail here,
        # with a message that says why, instead of reaching the exchange.
        raise ccxt.AuthenticationError(f"{exchange_id}: no API credentials configured")
    key = (exchange_id, need_auth)
    if key in _exchange_pool:
        return _exchange_pool[key]

    if exchange_id not in ccxt.exchanges:
        raise _InvalidArgs(f"Unknown exchange: {exchange_id}")

    config = {
        "enableRateLimit": True,
        "timeout": _HTTP_TIMEOUT_MS,
        "options": _exchange_options(exchange_id),
    }

    if need_auth and exchange_id in _credentials:
        creds = _credentials[exchange_id]
        if creds.get("api_key"):
            config["apiKey"] = creds["api_key"]
        if creds.get("secret"):
            config["secret"] = creds["secret"]
        if creds.get("password"):
            config["password"] = creds["password"]
        if creds.get("wallet_address"):
            config["walletAddress"] = creds["wallet_address"]
        if creds.get("private_key"):
            config["privateKey"] = creds["private_key"]

    exchange_class = getattr(ccxt, exchange_id)
    exchange = exchange_class(config)
    if exchange_id == "hyperliquid":
        _disable_hyperliquid_builder(exchange)
    _install_send_guard(exchange, exchange_id)   # innermost: checked on every attempt
    install_get_retry(exchange)

    # Fresh disk cache → no network call before the first request. set_markets()
    # rebuilds ccxt's indexes (markets_by_id, symbols, currencies).
    markets, currencies = load_cached_market_data(exchange_id)
    if markets and apply_cached_markets(exchange, markets, currencies):
        exchange._fincept_markets_stale = False
    # Otherwise load now, through _ensure_markets, so the result is cached and an
    # unreachable exchange can fall back to a stale list. Left to ccxt, the
    # first fetch_* would load markets itself and do neither.
    _ensure_markets(exchange, exchange_id)

    _exchange_pool[key] = exchange
    return exchange


def _ensure_markets(exchange, exchange_id):
    """Load markets if not yet loaded."""
    if not exchange.markets:
        try:
            exchange.load_markets()
            exchange._fincept_markets_stale = False
            save_markets_cache(exchange_id, exchange.markets, exchange.currencies)
        except Exception:
            # Exchange unreachable: a stale market list still lets every
            # display call proceed. Orders refuse it (_prepare_order): contract
            # sizes and precision must be current.
            markets, currencies = load_cached_market_data(exchange_id, allow_stale=True)
            if not (markets and apply_cached_markets(exchange, markets, currencies)):
                raise
            exchange._fincept_markets_stale = True


# ── Symbol resolution (one rule set: exchange_client.resolve_market) ────────

def _resolve_market_symbol(ex, symbol, exchange_id):
    """The market a symbol is SHOWN from (exact, or a labelled substitute), or
    None when there is none — the same answer ws_stream.resolve_symbol gives."""
    res = resolve_market(getattr(ex, "markets", None) or {}, symbol, exchange_id)
    return res["symbol"]


def _market_symbol(ex, exchange_id, symbol):
    """Resolve the app's symbol for a display call. Empty / None passes through
    unchanged so account-wide requests (all positions, all open orders) stay
    account-wide. No market at all: BadSymbol with the reason and the choices."""
    if not symbol:
        return symbol
    _ensure_markets(ex, exchange_id)
    res = resolve_market(ex.markets or {}, symbol, exchange_id)
    if res["symbol"] is None:
        alts = res["alternatives"]
        raise ccxt.BadSymbol(res["reason"] + (f" (listed: {', '.join(alts)})" if alts else ""))
    return res["symbol"]


def _exact_symbol(ex, exchange_id, symbol):
    """The market an account-changing call may act on: only an exact, active
    market. Never a substitute — the user must choose the market explicitly."""
    _ensure_markets(ex, exchange_id)
    res = resolve_market(ex.markets or {}, symbol, exchange_id)
    if res["match"] != "exact":
        alts = res["alternatives"]
        raise OrderRefused(res["reason"] + (f". Choose one of: {', '.join(alts)}" if alts else ""), alts)
    return res["symbol"]


# ── Method handlers ─────────────────────────────────────────────────────────

def _handle_ping(req_id, exchange_id, args):
    _respond(req_id, True, {"pong": True, "time": time.time()})


def _handle_set_credentials(req_id, exchange_id, args):
    """Store credentials in memory for this session — or drop them.

    An args object with no credential field set (the app sends {} when the
    user CLEARs their keys) removes this exchange's credentials, so the
    authenticated client below cannot be rebuilt from them.
    """
    has_creds = isinstance(args, dict) and any(args.get(k) for k in _CREDENTIAL_FIELDS)
    if has_creds:
        _credentials[exchange_id] = args
    else:
        _credentials.pop(exchange_id, None)
    # Invalidate any existing authenticated exchange instance
    _exchange_pool.pop((exchange_id, True), None)
    _respond(req_id, True, {"stored": has_creds, "cleared": not has_creds})


def _ticker_row(t, requested, market_symbol):
    """One ticker reply. "symbol" is the symbol the caller asked for — the app
    keys its caches by it — and "market_symbol" the market actually quoted
    (they differ where the app's pair is resolved, e.g. Hyperliquid
    "BTC/USDT" -> "BTC/USDC:USDC")."""
    opened, change, percentage = t.get("open"), t.get("change"), t.get("percentage")
    # Hyperliquid reports the previous day's close but no open / change.
    last, prev = _to_float(t.get("last")), _to_float(t.get("previousClose"))
    if percentage is None and last and prev:
        opened = opened if opened is not None else prev
        change = last - prev
        percentage = change / prev * 100.0
    return {
        "symbol": requested,
        "market_symbol": market_symbol,
        "last": t.get("last"),
        "bid": t.get("bid"),
        "ask": t.get("ask"),
        "high": t.get("high"),
        "low": t.get("low"),
        "open": opened,
        "close": t.get("close"),
        "change": change,
        "percentage": percentage,
        "baseVolume": t.get("baseVolume"),
        "quoteVolume": t.get("quoteVolume"),
        "timestamp": t.get("timestamp"),
    }


def _fetch_tickers_by_type(ex, exchange_id, market_symbols):
    """ex.fetch_tickers(market_symbols), one call per market type on Hyperliquid.

    ccxt builds Hyperliquid tickers from its market loader: without a "type" it
    loads spot, perps and every HIP-3 dex — 12 s for fetch_tickers and 24 s for
    the emulated fetch_ticker from this host, past the caller's 15 s budget —
    and with one only that list (~1 s)."""
    if exchange_id != "hyperliquid":
        return ex.fetch_tickers(market_symbols)
    markets = ex.markets or {}
    groups = {}
    for s in market_symbols:
        groups.setdefault((markets.get(s) or {}).get("type"), []).append(s)
    result = {}
    for mtype, group in groups.items():
        params = {"type": mtype} if mtype in ("spot", "swap") else {}
        result.update(ex.fetch_tickers(group, params))
    return result


def _fetch_one_ticker(ex, exchange_id, symbol):
    if exchange_id == "hyperliquid":
        ticker = _fetch_tickers_by_type(ex, exchange_id, [symbol]).get(symbol)
        if not ticker:
            raise ccxt.BadSymbol(f"hyperliquid returned no ticker for {symbol}")
        return ticker
    return ex.fetch_ticker(symbol)


def _handle_fetch_ticker(req_id, exchange_id, args):
    requested = _arg(args, "symbol")
    ex = _get_exchange(exchange_id)
    symbol = _market_symbol(ex, exchange_id, requested)
    _respond(req_id, True, _ticker_row(_fetch_one_ticker(ex, exchange_id, symbol), requested, symbol))


def _handle_fetch_tickers(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbols = args.get("symbols") or []
    if not isinstance(symbols, list):
        raise _InvalidArgs("'symbols' must be a list")
    if not symbols:
        tickers = ex.fetch_tickers(None, {"type": get_default_type(exchange_id)}) \
            if exchange_id == "hyperliquid" else ex.fetch_tickers()
        rows = [_ticker_row(t, t.get("symbol"), t.get("symbol")) for t in tickers.values()]
        _respond(req_id, True, {"tickers": rows})
        return

    # Resolve each requested symbol to a listed market and leave out the ones
    # with none: one unknown symbol used to fail the whole batch (ccxt raises
    # BadSymbol for the list), so the watchlist got no prices at all.
    _ensure_markets(ex, exchange_id)
    markets = ex.markets or {}
    wanted = {}   # requested -> market symbol
    missing = []
    for s in symbols:
        if not isinstance(s, str) or not s.strip():
            continue
        m = resolve_market(markets, s, exchange_id)["symbol"]
        if m in markets:
            wanted[s] = m
        else:
            missing.append(s)
    tickers = _fetch_tickers_by_type(ex, exchange_id, sorted(set(wanted.values()))) if wanted else {}
    rows = []
    for requested, m in wanted.items():
        t = tickers.get(m)
        if t:
            rows.append(_ticker_row(t, requested, m))
        else:
            missing.append(requested)
    _respond(req_id, True, {"tickers": rows, "missing": missing})


def _handle_fetch_orderbook(req_id, exchange_id, args):
    requested = _arg(args, "symbol")
    ex = _get_exchange(exchange_id)
    symbol = _market_symbol(ex, exchange_id, requested)
    limit = int(args.get("limit") or 20)
    ob = ex.fetch_order_book(symbol, limit)
    bids = [[b[0], b[1]] for b in ob.get("bids", [])[:limit]]
    asks = [[a[0], a[1]] for a in ob.get("asks", [])[:limit]]
    best_bid = bids[0][0] if bids else 0
    best_ask = asks[0][0] if asks else 0
    spread = best_ask - best_bid if best_bid and best_ask else 0
    spread_pct = (spread / best_ask * 100) if best_ask else 0
    _respond(req_id, True, {
        "symbol": requested,
        "market_symbol": symbol,
        "bids": bids, "asks": asks,
        "best_bid": best_bid, "best_ask": best_ask,
        "spread": spread, "spread_pct": spread_pct,
    })


def _handle_fetch_ohlcv(req_id, exchange_id, args):
    requested = _arg(args, "symbol")
    ex = _get_exchange(exchange_id)
    symbol = _market_symbol(ex, exchange_id, requested)
    timeframe = args.get("timeframe", "1h")
    limit = args.get("limit", 100)
    candles_raw = ex.fetch_ohlcv(symbol, timeframe, limit=limit)
    candles = []
    for c in candles_raw:
        candles.append({
            "timestamp": c[0], "open": c[1], "high": c[2],
            "low": c[3], "close": c[4], "volume": c[5],
        })
    _respond(req_id, True, {"candles": candles, "market_symbol": symbol})


def _market_row(m):
    limits = m.get("limits") or {}
    return {
        "symbol": m["symbol"],
        "base": m.get("base"), "quote": m.get("quote"), "settle": m.get("settle"),
        "type": m.get("type"), "active": m.get("active"),
        "maker": m.get("maker"), "taker": m.get("taker"),
        "minAmount": (limits.get("amount") or {}).get("min"),
        "minCost":   (limits.get("cost") or {}).get("min"),
    }


def _handle_fetch_markets(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    _ensure_markets(ex, exchange_id)

    # {"symbol": X}: the market X resolves to (the same resolve_market the
    # stream and the order path use) with how it matched, why, the choices, its
    # contract facts and what the ticket may send there. Empty "markets" when
    # there is none — "resolution" then says why.
    single = args.get("symbol")
    if single:
        res = resolve_market(ex.markets or {}, single, exchange_id)
        m = (ex.markets or {}).get(res["symbol"]) if res["symbol"] else None
        rows = []
        if m:
            row = _market_row(m)
            row["requested"] = single
            row.update({k: res[k] for k in ("match", "tradable", "reason", "alternatives")})
            row["market"] = market_descriptor(m)
            row["capabilities"] = order_capabilities(ex, exchange_id, m)
            row["markets_stale"] = bool(getattr(ex, "_fincept_markets_stale", False))
            # The ticket sizes in base coins: its minimum in coins too.
            mn, mx, min_base = order_size_limits(exchange_id, m)
            row["minBaseAmount"] = min_base
            row["minOrderAmount"] = mn
            row["maxOrderAmount"] = mx
            rows.append(row)
        _respond(req_id, True, {"markets": rows, "count": len(rows), "resolution": res})
        return

    # {"symbols": [...]}: how each one resolves (the watchlist labels rows that
    # are shown from a related market, and rows with no market at all).
    many = args.get("symbols")
    if isinstance(many, list):
        out = []
        for s in many:
            if isinstance(s, str) and s.strip():
                r = resolve_market(ex.markets or {}, s, exchange_id)
                out.append({k: r[k] for k in ("requested", "symbol", "match", "tradable", "reason")})
        _respond(req_id, True, {"resolutions": out})
        return

    market_type = args.get("type")
    query       = (args.get("query") or "").upper().strip()  # server-side filter
    limit       = int(args.get("limit") or 0)                # 0 = no limit
    markets = []
    for sym, m in ex.markets.items():
        if market_type and m.get("type") != market_type:
            continue
        if not m.get("active", True):
            continue
        if query:
            sym_upper  = (m.get("symbol") or "").upper()
            base_upper = (m.get("base") or "").upper()
            if query not in sym_upper and query not in base_upper:
                continue
        markets.append(_market_row(m))
        if limit and len(markets) >= limit:
            break
    _respond(req_id, True, {"markets": markets, "count": len(markets)})


def _handle_fetch_trades(req_id, exchange_id, args):
    requested = _arg(args, "symbol")
    if exchange_id == "hyperliquid":
        # ccxt's hyperliquid fetchTrades reads an ACCOUNT's fills (it needs a
        # wallet address); the venue has no public trade history over REST.
        # The live stream carries the public prints.
        _respond(req_id, False, code="NOT_SUPPORTED",
                 error="hyperliquid has no public trade history over REST; trades arrive on the live stream")
        return
    ex = _get_exchange(exchange_id)
    if not _require_cap(req_id, ex, "fetchTrades", exchange_id):
        return
    symbol = _market_symbol(ex, exchange_id, requested)
    limit = int(args.get("limit") or 50)
    trades = ex.fetch_trades(symbol, limit=limit)
    result = []
    for t in trades:
        result.append({
            "id": t.get("id"), "symbol": requested,
            "side": t.get("side"), "price": t.get("price"),
            "amount": t.get("amount"), "cost": t.get("cost"),
            "timestamp": t.get("timestamp"),
        })
    _respond(req_id, True, {"trades": result})


def _perp_symbol(ex, symbol):
    """The linear perpetual of a spot pair ("BTC/USDT" -> "BTC/USDT:USDT") when
    the venue lists it, else None. Ported from the one-shot
    fetch_funding_rate.py / fetch_open_interest.py (to_perp_symbol)."""
    markets = ex.markets or {}
    if not symbol or "/" not in symbol or ":" in symbol:
        return None
    current = markets.get(symbol) or {}
    if current.get("contract") or current.get("swap"):
        return None
    quote = symbol.split("/", 1)[1].strip()
    candidate = f"{symbol}:{quote}"
    m = markets.get(candidate)
    if m and m.get("swap") and m.get("active", True) is not False:
        return candidate
    return None


def _contract_symbol(ex, exchange_id, args):
    """Market for a funding / open-interest / mark-price request. With
    "derive_perp": true a spot pair is replaced by its linear perpetual when the
    venue lists one (off by default: the screen shows "--" for a spot pair)."""
    symbol = _market_symbol(ex, exchange_id, _arg(args, "symbol"))
    if args.get("derive_perp"):
        perp = _perp_symbol(ex, symbol)
        if perp:
            return perp
    return symbol


def _handle_fetch_funding_rate(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    # ccxt emulates fetchFundingRate from fetchFundingRates (Hyperliquid has
    # only the latter); checking the single-symbol flag alone refused it there.
    if not _require_cap(req_id, ex, 'fetchFundingRate', exchange_id, 'fetchFundingRates'):
        return
    symbol = _contract_symbol(ex, exchange_id, args)
    fr = ex.fetch_funding_rate(symbol)
    _respond(req_id, True, {
        "symbol": fr.get("symbol"),
        "funding_rate": fr.get("fundingRate"),
        "mark_price": fr.get("markPrice"),
        "index_price": fr.get("indexPrice"),
        "funding_timestamp": fr.get("fundingTimestamp"),
        "next_funding_timestamp": fr.get("nextFundingTimestamp"),
    })


def _to_float(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    return f if math.isfinite(f) and f > 0 else None


def _handle_fetch_open_interest(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    if not _require_cap(req_id, ex, 'fetchOpenInterest', exchange_id):
        return
    symbol = _contract_symbol(ex, exchange_id, args)
    oi = ex.fetch_open_interest(symbol)
    # ccxt 4.x reports openInterestAmount (contracts / base) and
    # openInterestValue (quote); the old "openInterest" key does not exist.
    amount = oi.get("openInterestAmount")
    if amount is None:
        amount = oi.get("openInterest")
    value = oi.get("openInterestValue")
    if value is None and amount is not None:
        # Hyperliquid gives only the amount; its raw payload carries the mark.
        info = oi.get("info") if isinstance(oi.get("info"), dict) else {}
        mark = _to_float(info.get("markPx")) or _to_float(info.get("markPrice"))
        amount_f = _to_float(amount)
        if mark and amount_f:
            value = amount_f * mark
    _respond(req_id, True, {
        "symbol": oi.get("symbol"),
        "open_interest": amount,
        "open_interest_value": value,
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
    _respond(req_id, True, {"balances": non_zero, "exchange": exchange_id})


def _handle_warm(req_id, exchange_id, args):
    """Build the authenticated client and load its markets now (the app calls
    this when LIVE is armed), so no order waits on a 20 s market load."""
    ex = _get_exchange(exchange_id, need_auth=True)
    _ensure_markets(ex, exchange_id)
    _respond(req_id, True, {"exchange": exchange_id, "markets": len(ex.markets or {}),
                            "markets_stale": bool(getattr(ex, "_fincept_markets_stale", False)),
                            "account_id": credential_fingerprint(_credentials.get(exchange_id)),
                            "account": account_label(_credentials.get(exchange_id))})


# ── Orders ──────────────────────────────────────────────────────────────────

_ORDER_TYPES = {"market": "market", "limit": "limit", "stop": "stop", "stop_market": "stop",
                "stop_limit": "stop_limit", "stop_loss_limit": "stop_limit"}


def _positive(value, label):
    try:
        f = float(value)
    except (TypeError, ValueError):
        raise OrderRefused(f"{label} must be a number")
    if not math.isfinite(f) or f <= 0:
        raise OrderRefused(f"{label} must be greater than zero")
    return f


def _fmt(v):
    """Plain decimal text for a float (no exponent)."""
    s = f"{v:.12f}".rstrip("0").rstrip(".")
    return s if s else "0"


def _price_on_grid(ex, symbol, value, label):
    """`value` as the exchange's price text, only when it already lies on the
    price grid — the price sent is exactly the price the user typed. Off-grid:
    refused with the nearest valid price (never rounded silently)."""
    v = _positive(value, label)
    try:
        s = ex.price_to_precision(symbol, v)
    except ccxt.InvalidOrder:
        raise OrderRefused(f"{label} {_fmt(v)} is below the smallest price step of {symbol}")
    if abs(float(s) - v) > abs(v) * 1e-9:
        raise OrderRefused(f"{label} {_fmt(v)} is not a valid price step for {symbol} on {ex.id}; "
                           f"the nearest valid price is {s}")
    return s


def _reference_price(ex, exchange_id, symbol):
    """Last price of `symbol` fetched from the exchange now (the stream's price
    may belong to another instrument; this cannot)."""
    t = _fetch_one_ticker(ex, exchange_id, symbol) or {}
    last = _to_float(t.get("last")) or _to_float(t.get("close"))
    bid, ask = _to_float(t.get("bid")), _to_float(t.get("ask"))
    if last:
        return last
    if bid and ask:
        return (bid + ask) / 2.0
    return None


def _market_buy_needs_price(ex, market):
    """ccxt sends a spot market BUY as a quote amount (amount x price) here."""
    return market_kind(market) == "spot" and ex.options.get("createMarketBuyOrderRequiresPrice") is True


def _confirm_hash(exchange_id, account_id, create):
    blob = json.dumps({"exchange": exchange_id, "account_id": account_id, "create_order": create},
                      sort_keys=True, separators=(",", ":"), default=str)
    return hashlib.sha256(blob.encode("utf-8")).hexdigest()


def _prepare_order(ex, exchange_id, args, ref_price=None):
    """Everything an order would send, decided and checked without sending.
    Raises OrderRefused for anything that cannot be made provably right."""
    requested = str(_arg(args, "symbol")).strip()
    side = str(_arg(args, "side")).strip().lower()
    if side not in ("buy", "sell"):
        raise OrderRefused(f"side must be buy or sell, not {side!r}")
    otype = _ORDER_TYPES.get(str(_arg(args, "type")).strip().lower())
    if not otype:
        raise OrderRefused(f"order type {args.get('type')!r} is not supported")
    qty = _positive(_arg(args, "amount"), "quantity")
    unit = (args.get("amount_unit") or "base").lower()
    if unit not in ("base", "contracts"):
        raise OrderRefused("amount_unit must be base or contracts")
    reduce_only = bool(args.get("reduce_only"))
    has_sl = args.get("sl") not in (None, 0, 0.0, "", "0")
    has_tp = args.get("tp") not in (None, 0, 0.0, "", "0")

    if getattr(ex, "_fincept_markets_stale", False):
        raise OrderRefused(f"{exchange_id} could not be reached to refresh its market list, and the "
                           f"saved one is older than a day — contract sizes and price steps may have "
                           f"changed, so no order is sent. Try again when the exchange is reachable.")
    _ensure_markets(ex, exchange_id)
    res = resolve_market(ex.markets or {}, requested, exchange_id)
    if res["match"] != "exact":
        alts = res["alternatives"]
        raise OrderRefused(res["reason"] + (f". Choose one of: {', '.join(alts)}" if alts else ""), alts)
    if not res["tradable"]:
        raise OrderRefused(res["reason"])
    symbol = res["symbol"]
    m = ex.markets[symbol]
    kind = market_kind(m)
    caps = order_capabilities(ex, exchange_id, m)
    if not caps["orders"]:
        raise OrderRefused(caps["orders_reason"])
    if otype == "stop" and not caps["stop"]:
        raise OrderRefused(caps["stop_reason"])
    if otype == "stop_limit" and not caps["stop_limit"]:
        raise OrderRefused(caps["stop_limit_reason"])
    if (has_sl or has_tp):
        if not caps["sl_tp"]:
            raise OrderRefused(caps["sl_tp_reason"])
        if otype not in caps["sl_tp_types"]:
            allowed = " or ".join(t.upper() for t in caps["sl_tp_types"])
            raise OrderRefused(f"on {exchange_id} {'spot' if kind == 'spot' else 'contracts'} stop-loss / "
                               f"take-profit can only be attached to a {allowed} entry")
        if reduce_only:
            raise OrderRefused("a reduce-only order cannot carry stop-loss / take-profit legs")
    if reduce_only and kind == "spot":
        raise OrderRefused("reduce-only applies to contracts, not to spot markets")
    if unit == "contracts" and kind == "spot":
        raise OrderRefused("a spot order is sized in coins, not contracts")

    price_s = _price_on_grid(ex, symbol, args.get("price"), "limit price") \
        if otype in ("limit", "stop_limit") else None
    stop_s = _price_on_grid(ex, symbol, args.get("stop_price"), "stop price") \
        if otype in ("stop", "stop_limit") else None
    sl_s = _price_on_grid(ex, symbol, args.get("sl"), "stop-loss price") if has_sl else None
    tp_s = _price_on_grid(ex, symbol, args.get("tp"), "take-profit price") if has_tp else None

    ref = _to_float(ref_price) if ref_price is not None else _reference_price(ex, exchange_id, symbol)
    if not ref:
        raise OrderRefused(f"no current price for {symbol} from {exchange_id}; nothing was sent")

    warnings = []
    if stop_s is not None:
        stop_f = float(stop_s)
        if side == "sell" and not stop_f < ref:
            raise OrderRefused(f"a SELL stop triggers when the price falls to it: {stop_s} must be below the "
                               f"current price {_fmt(ref)} (use a LIMIT sell above the market)")
        if side == "buy" and not stop_f > ref:
            raise OrderRefused(f"a BUY stop triggers when the price rises to it: {stop_s} must be above the "
                               f"current price {_fmt(ref)} (use a LIMIT buy below the market)")
        if otype == "stop_limit":
            if side == "sell" and float(price_s) > stop_f:
                warnings.append(f"the limit {price_s} is above the stop {stop_s}: after the stop triggers "
                                f"the sell may not fill")
            if side == "buy" and float(price_s) < stop_f:
                warnings.append(f"the limit {price_s} is below the stop {stop_s}: after the stop triggers "
                                f"the buy may not fill")

    entry = float(price_s) if price_s is not None else ref
    if sl_s is not None:
        if side == "buy" and not float(sl_s) < entry:
            raise OrderRefused(f"the stop-loss {sl_s} of a BUY must be below the entry price {_fmt(entry)}")
        if side == "sell" and not float(sl_s) > entry:
            raise OrderRefused(f"the stop-loss {sl_s} of a SELL must be above the entry price {_fmt(entry)}")
    if tp_s is not None:
        if side == "buy" and not float(tp_s) > entry:
            raise OrderRefused(f"the take-profit {tp_s} of a BUY must be above the entry price {_fmt(entry)}")
        if side == "sell" and not float(tp_s) < entry:
            raise OrderRefused(f"the take-profit {tp_s} of a SELL must be below the entry price {_fmt(entry)}")

    # ── Size: base coins -> the exchange's own unit ─────────────────────────
    cs = 1.0
    size_px = float(price_s) if price_s is not None else (float(stop_s) if stop_s is not None else ref)
    if kind == "spot":
        amount = qty
    else:
        cs = float(m.get("contractSize"))
        if m.get("inverse"):
            amount = qty if unit == "contracts" else qty * size_px / cs
        else:
            amount = qty if unit == "contracts" else qty / cs
    try:
        amount_s = ex.amount_to_precision(symbol, amount)   # truncates: never larger than asked
    except ccxt.InvalidOrder:
        step = (m.get("precision") or {}).get("amount")
        raise OrderRefused(f"{_fmt(qty)} {m.get('base') if unit == 'base' else 'contracts'} is smaller than "
                           f"the smallest order step of {symbol} ({step} "
                           f"{'contracts' if kind != 'spot' else m.get('base')})")
    amount_f = float(amount_s)
    if not amount_f > 0:
        raise OrderRefused(f"the order size rounds to zero on {symbol}")
    if kind == "spot":
        base_amount = amount_f
        notional = amount_f * size_px
        notional_ccy = m.get("quote")
    elif m.get("inverse"):
        notional = amount_f * cs
        notional_ccy = m.get("quote")
        base_amount = notional / size_px
    else:
        base_amount = amount_f * cs
        notional = base_amount * size_px
        notional_ccy = m.get("quote")
    wanted_base = qty if unit == "base" else None
    if wanted_base is not None and kind != "spot" and m.get("inverse"):
        pass  # inverse: the coin amount depends on the price; the USD size is what is fixed
    elif wanted_base is not None and abs(base_amount - wanted_base) > max(wanted_base, 1e-18) * 1e-9:
        warnings.append(f"size rounded down to the exchange's step: {_fmt(base_amount)} {m.get('base')} "
                        f"instead of {_fmt(wanted_base)}")

    desc = market_descriptor(m)
    unit_label = "contracts" if kind != "spot" else m.get("base")
    min_amt, max_amt, _min_base = order_size_limits(exchange_id, m)
    if min_amt and amount_f < min_amt * (1 - 1e-9):
        raise OrderRefused(f"{_fmt(amount_f)} {unit_label} is below the exchange minimum of "
                           f"{_fmt(min_amt)} {unit_label} for {symbol}")
    if max_amt and amount_f > max_amt * (1 + 1e-9):
        raise OrderRefused(f"{_fmt(amount_f)} {unit_label} is above the exchange maximum of "
                           f"{_fmt(max_amt)} {unit_label} for {symbol}")
    if desc["min_cost"] and notional < desc["min_cost"] * (1 - 1e-9):
        raise OrderRefused(f"order value {_fmt(notional)} {notional_ccy} is below the exchange minimum of "
                           f"{_fmt(desc['min_cost'])} {notional_ccy} for {symbol}")
    if desc["max_cost"] and notional > desc["max_cost"] * (1 + 1e-9):
        raise OrderRefused(f"order value {_fmt(notional)} {notional_ccy} is above the exchange maximum of "
                           f"{_fmt(desc['max_cost'])} {notional_ccy} for {symbol}")

    # A limit far through the market fills at once as a taker.
    if otype == "limit":
        through = (float(price_s) - ref) / ref if side == "buy" else (ref - float(price_s)) / ref
        if through > 0.03:
            warnings.append(f"the limit is {through * 100:.1f}% through the current price {_fmt(ref)}: it will "
                            f"fill at once, at up to that price")

    # ── The exact ccxt call ─────────────────────────────────────────────────
    ccxt_type = "market" if otype in ("market", "stop") else "limit"
    order_price = price_s
    params = {}
    if stop_s is not None:
        sp = stop_params(exchange_id, m, otype, side, stop_s)
        if sp is None:
            raise OrderRefused(caps[otype + "_reason"] or f"{otype} orders are not supported on {exchange_id}")
        params.update(sp)
    margin_mode = None
    if caps.get("margin_mode_per_order"):
        # The margin mode is part of each order here (OKX tdMode, Bitget /
        # KuCoin marginMode): send the one the ticket shows, never ccxt's default.
        margin_mode = str(args.get("margin_mode") or "").lower()
        if margin_mode not in ("cross", "isolated"):
            raise OrderRefused(f"{exchange_id} contract orders carry a margin mode: choose cross or isolated")
        params["marginMode"] = margin_mode
        if exchange_id == "mexc" and margin_mode == "isolated":
            # MEXC isolated orders carry their leverage (ccxt requires it).
            params["leverage"] = _leverage_arg(args, "mexc isolated-margin orders carry a leverage")
    leverage_sent = params.get("leverage")
    if exchange_id == "kucoin" and kind != "spot" and not ex.is_uta_enabled():
        # Classic-account KuCoin futures orders carry their own leverage, and
        # ccxt sends 1x unless told — send the leverage the ticket shows.
        # (Unified-account orders carry none.)
        params["leverage"] = leverage_sent = _leverage_arg(args, "kucoin futures orders carry a leverage")
    if sl_s is not None:
        params["stopLoss"] = _sltp_leg(exchange_id, sl_s)
    if tp_s is not None:
        params["takeProfit"] = _sltp_leg(exchange_id, tp_s)
    if reduce_only:
        params["reduceOnly"] = True
    market_bound = None
    spend = None
    if ccxt_type == "market" and exchange_id == "hyperliquid":
        # Hyperliquid has no market orders: ccxt sends an IOC limit at the
        # reference price moved by the slippage — the worst price accepted.
        anchor = float(stop_s) if stop_s is not None else ref
        order_price = ex.price_to_precision(symbol, anchor)
        params["slippage"] = MARKET_SLIPPAGE
        factor = 1 + float(MARKET_SLIPPAGE) if side == "buy" else 1 - float(MARKET_SLIPPAGE)
        market_bound = ex.price_to_precision(symbol, float(order_price) * factor)
    elif ccxt_type == "market" and side == "buy" and _market_buy_needs_price(ex, m):
        # ccxt sends this market BUY as a spend of amount x price in the quote.
        order_price = _fmt(ref)
        spend = amount_f * ref

    token = str(args.get("client_order_token") or uuid.uuid4().hex)
    cid_key, cid = client_order_id(exchange_id, m, token)
    if exchange_id == "hyperliquid" and (sl_s is not None or tp_s is not None):
        # ccxt copies the cloid onto the stop-loss / take-profit legs, giving
        # three orders one cloid; send none rather than risk the legs failing.
        cid_key = cid = None
    if cid_key:
        params[cid_key] = cid

    create = {"symbol": symbol, "type": ccxt_type, "side": side, "amount": amount_s,
              "price": order_price, "params": params}
    creds = _credentials.get(exchange_id)
    account_id = credential_fingerprint(creds)
    return {
        "exchange": exchange_id,
        "account_id": account_id,
        "account": account_label(creds),
        "requested": requested,
        "market": desc,
        "side": side, "type": otype,
        "qty": qty, "qty_unit": unit,
        "amount": amount_f, "amount_text": amount_s, "amount_unit": unit_label,
        "contract_size": cs if kind != "spot" else None,
        "base_amount": base_amount, "base_ccy": m.get("base"),
        "price": price_s, "stop_price": stop_s, "sl": sl_s, "tp": tp_s, "reduce_only": reduce_only,
        "margin_mode": margin_mode, "leverage_sent": leverage_sent,
        "ref_price": ref, "notional": notional, "notional_ccy": notional_ccy,
        "market_bound": market_bound, "slippage": MARKET_SLIPPAGE if market_bound else None,
        "spend": spend,
        "client_order_token": token, "client_order_id": cid,
        "create_order": create,
        "confirm_hash": _confirm_hash(exchange_id, account_id, create),
        "warnings": warnings,
    }


def _leverage_arg(args, why):
    try:
        lev = int(float(args.get("leverage") or 0))
    except (TypeError, ValueError):
        lev = 0
    if lev < 1:
        raise OrderRefused(f"{why}; none was given")
    return lev


def _create_order_args(create):
    """Positional arguments for ccxt create_order from a prepared order. The
    confirmed block keeps the exchange's exact text; ccxt takes numbers (KuCoin
    contracts compare `amount < 1`), and float(text) re-rounds to the same text."""
    price = create["price"]
    return (create["symbol"], create["type"], create["side"], float(create["amount"]),
            float(price) if price is not None else None, json.loads(json.dumps(create["params"])))


def _sltp_leg(exchange_id, trigger):
    """One attached stop-loss / take-profit leg, closing at market once triggered."""
    leg = {"triggerPrice": trigger}
    if exchange_id == "hyperliquid":
        leg["type"] = "market"   # ccxt's default there is a limit at the trigger price
    return leg


def _handle_prepare_order(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    _respond(req_id, True, _prepare_order(ex, exchange_id, args))


def _order_reply(exchange_id, prepared, order):
    """What the exchange reported, as plain values, checked against what was
    sent. Always encodable (strings / numbers / None only)."""
    def plain(v):
        if v is None or isinstance(v, (bool, int, str)):
            return v
        if isinstance(v, float):
            return v if math.isfinite(v) else None
        return str(v)

    order = order or {}
    oid = order.get("id")
    create = prepared["create_order"]
    mismatch = []
    rs = order.get("symbol")
    if rs and rs != create["symbol"]:
        mismatch.append(f"market {rs} (sent {create['symbol']})")
    rside = order.get("side")
    if rside and str(rside).lower() != create["side"]:
        mismatch.append(f"side {rside} (sent {create['side']})")
    ramount = _to_float(order.get("amount"))
    if ramount and abs(ramount - prepared["amount"]) > prepared["amount"] * 1e-6:
        mismatch.append(f"size {ramount:g} (sent {create['amount']})")
    fee = order.get("fee") if isinstance(order.get("fee"), dict) else {}
    return {
        "outcome": "accepted" if oid not in (None, "") else "unknown",
        "exchange": exchange_id,
        "account_id": prepared["account_id"],
        "id": plain(oid),
        "client_order_id": plain(order.get("clientOrderId") or prepared["client_order_id"]),
        "symbol": plain(rs or create["symbol"]),
        "side": plain(rside or create["side"]),
        "type": plain(order.get("type") or create["type"]),
        "amount": plain(order.get("amount")),
        "price": plain(order.get("price")),
        "status": plain(order.get("status")),
        "filled": plain(order.get("filled")),
        "remaining": plain(order.get("remaining")),
        "average": plain(order.get("average")),
        "cost": plain(order.get("cost")),
        "fee": plain(fee.get("cost")), "fee_currency": plain(fee.get("currency")),
        "timestamp": plain(order.get("timestamp")),
        "sent": {"symbol": create["symbol"], "side": create["side"], "amount": create["amount"],
                 "price": create["price"], "type": create["type"]},
        "mismatch": mismatch,
    }


# Headroom (s) an account-changing request needs left in the caller's budget
# before it is sent: the HTTP timeout of that request plus a margin.
_ORDER_HEADROOM_S = _ORDER_HTTP_TIMEOUT_MS / 1000.0 + 1.0


def _handle_place_order(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    confirm = args.get("confirm_hash")
    if not confirm:
        raise OrderRefused("place_order needs the confirm_hash of the prepared order the user confirmed")
    account_id = credential_fingerprint(_credentials.get(exchange_id))
    if args.get("account_id") is not None and args.get("account_id") != account_id:
        raise OrderRefused(f"the {exchange_id} API key changed after the order was confirmed; nothing was sent")
    ref = args.get("ref_price")
    if ref is None:
        raise OrderRefused("place_order needs the ref_price of the prepared order")
    prepared = _prepare_order(ex, exchange_id, args, ref_price=ref)
    if prepared["confirm_hash"] != confirm:
        raise OrderRefused("the order no longer matches what was confirmed (market data or the API key "
                           "changed); nothing was sent — review it again")
    create = prepared["create_order"]
    _diag(f"order {exchange_id} {create['side']} {create['amount']} {create['symbol']} {create['type']}"
          f"{' @ ' + str(create['price']) if create['price'] else ''}")
    with _SendWindow(ex, _ORDER_HEADROOM_S, _ORDER_HTTP_TIMEOUT_MS):
        order = ex.create_order(*_create_order_args(create))
    reply = _order_reply(exchange_id, prepared, order)
    reply["prepared"] = {k: prepared[k] for k in ("market", "amount_text", "amount_unit", "base_amount",
                                                  "notional", "notional_ccy", "create_order", "warnings")}
    _respond(req_id, True, reply)


def _handle_find_order(req_id, exchange_id, args):
    """Look an order up by the client order id derived from the ticket's token
    (after an unknown outcome). Read-only; never re-sends anything."""
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = _exact_symbol(ex, exchange_id, _arg(args, "symbol"))
    m = ex.markets[symbol]
    _, cid = client_order_id(exchange_id, m, _arg(args, "client_order_token"))
    if cid is None:
        _respond(req_id, True, {"found": False, "supported": False,
                                "reason": f"{exchange_id} orders carry no client order id here"})
        return
    searched = []
    for label, fetch in (("open", lambda: ex.fetch_open_orders(symbol)),
                         ("closed", lambda: ex.fetch_closed_orders(symbol, None, 50)
                          if ex.has.get("fetchClosedOrders") else None)):
        try:
            orders = fetch()
        except ccxt.NotSupported:
            orders = None
        if orders is None:
            continue
        searched.append(label)
        for o in orders:
            if same_client_order_id(exchange_id, cid, o.get("clientOrderId")):
                _respond(req_id, True, {"found": True, "where": label, "client_order_id": cid,
                                        "order": {"id": o.get("id"), "status": o.get("status"),
                                                  "symbol": o.get("symbol"), "side": o.get("side"),
                                                  "amount": o.get("amount"), "filled": o.get("filled"),
                                                  "price": o.get("price")}})
                return
    _respond(req_id, True, {"found": False, "supported": True, "client_order_id": cid, "searched": searched})


def _handle_cancel_order(req_id, exchange_id, args):
    order_id = _arg(args, "order_id")
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = args.get("symbol")
    if symbol:
        symbol = _exact_symbol(ex, exchange_id, symbol)
    with _SendWindow(ex, _ACCOUNT_CHANGE_HEADROOM_S):
        result = ex.cancel_order(order_id, symbol) or {}
    status = result.get("status")
    _respond(req_id, True, {
        "outcome": "accepted",
        "id": result.get("id") or order_id,
        "symbol": result.get("symbol") or symbol,
        # Most venues return no status for a cancel; do not invent "canceled".
        "status": status,
        "exchange": exchange_id,
    })


def _handle_fetch_positions(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    if not _require_cap(req_id, ex, 'fetchPositions', exchange_id):
        return
    symbol = _market_symbol(ex, exchange_id, args.get("symbol"))
    positions = ex.fetch_positions([symbol] if symbol else None)
    # ccxt uses None for an unknown field: `abs(p.get("contracts", 0))` raised
    # TypeError on a present-but-None value and blanked the whole Positions tab.
    result = [p for p in positions if abs(p.get("contracts") or 0) > 0]
    _respond(req_id, True, {"positions": result, "exchange": exchange_id})


def _handle_fetch_open_orders(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = _market_symbol(ex, exchange_id, args.get("symbol"))
    orders = ex.fetch_open_orders(symbol)
    _respond(req_id, True, {"orders": orders, "exchange": exchange_id})


def _handle_fetch_mark_price(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id)
    symbol = _contract_symbol(ex, exchange_id, args)
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


def _trade_row(t):
    # ccxt sets "fee": None on many venues (and fee["cost"] / ["currency"] may be
    # None too): `t.get("fee", {}).get(...)` raised AttributeError and the whole
    # MY TRADES fetch failed.
    fee = t.get("fee") or {}
    return {
        "id": t.get("id"),
        "order": t.get("order"),
        "symbol": t.get("symbol"),
        "side": t.get("side"),
        "price": t.get("price"),
        "amount": t.get("amount"),
        "cost": t.get("cost"),
        "fee": fee.get("cost") or 0,
        "fee_currency": fee.get("currency") or "",
        "timestamp": t.get("timestamp"),
    }


def _handle_fetch_my_trades(req_id, exchange_id, args):
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol = _market_symbol(ex, exchange_id, args.get("symbol"))
    limit = int(args.get("limit", 50))
    trades = ex.fetch_my_trades(symbol, limit=limit)
    _respond(req_id, True, {
        "trades": [_trade_row(t) for t in trades],
        "count": len(trades),
    })


def _market_fee_row(ex, market_symbol, requested):
    """The venue's published default maker/taker for one market (ccxt fills these
    from load_markets). Used when there is no account fee endpoint or no API key;
    tagged source="market" so the UI can say it may not be the account's tier."""
    m = (getattr(ex, "markets", None) or {}).get(market_symbol) or {}
    if m.get("maker") is None and m.get("taker") is None:
        return None
    return {
        "symbol": requested,
        "maker": m.get("maker"),
        "taker": m.get("taker"),
        "percentage": True,
        "source": "market",
    }


def _handle_fetch_trading_fees(req_id, exchange_id, args):
    # The account fee endpoints are private. The old code asked the PUBLIC client,
    # which failed with an authentication error on every supported venue, so the
    # FEES tab and the MAKER / TAKER card never populated even with valid keys.
    try:
        auth_ex = _get_exchange(exchange_id, need_auth=True)
    except ccxt.AuthenticationError:
        auth_ex = None  # no API key configured — only the published schedule is available
    ex = auth_ex or _get_exchange(exchange_id)
    symbol = args.get("symbol")
    if symbol:
        market_symbol = _market_symbol(ex, exchange_id, symbol)
        fee_error = None
        if auth_ex is not None and auth_ex.has.get("fetchTradingFee"):
            try:
                fee = auth_ex.fetch_trading_fee(market_symbol) or {}
                if fee.get("maker") is not None or fee.get("taker") is not None:
                    _respond(req_id, True, {
                        "symbol": symbol,
                        "maker": fee.get("maker"),
                        "taker": fee.get("taker"),
                        "percentage": fee.get("percentage", True),
                        "source": "account",
                    })
                    return
            except Exception as e:  # fall back to the published schedule below
                fee_error = e
        row = _market_fee_row(ex, market_symbol, symbol)
        if row is not None:
            _respond(req_id, True, row)
            return
        if fee_error is not None:
            raise fee_error
        _respond(req_id, False, error=f"{exchange_id} reports no fee schedule for {symbol}",
                 code="NOT_SUPPORTED")
    else:
        if not _require_cap(req_id, ex, 'fetchTradingFees', exchange_id):
            return
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


def _contract_market(ex, exchange_id, requested):
    symbol = _exact_symbol(ex, exchange_id, requested)
    m = ex.markets[symbol]
    if market_kind(m) not in ("swap", "future"):
        raise OrderRefused(f"{symbol} is a {market_kind(m)} market: leverage and margin mode apply to "
                           f"contracts only")
    return symbol, m


def _handle_fetch_leverage_info(req_id, exchange_id, args):
    """Current leverage / margin mode for a contract market, where the
    exchange can report them (None where it cannot). Read-only."""
    ex = _get_exchange(exchange_id, need_auth=True)
    symbol, m = _contract_market(ex, exchange_id, _arg(args, "symbol"))
    lev = mode = None
    if ex.has.get("fetchLeverage"):
        try:
            info = ex.fetch_leverage(symbol) or {}
            lev = info.get("longLeverage") or info.get("shortLeverage")
            mode = info.get("marginMode")
        except Exception as e:
            _diag(f"fetch_leverage {exchange_id} {symbol}: {scrub_message(e, _credentials.get(exchange_id))}")
    if mode is None and ex.has.get("fetchMarginMode"):
        try:
            mode = (ex.fetch_margin_mode(symbol) or {}).get("marginMode")
        except Exception as e:
            _diag(f"fetch_margin_mode {exchange_id} {symbol}: {scrub_message(e, _credentials.get(exchange_id))}")
    _respond(req_id, True, {"symbol": symbol, "leverage": lev, "margin_mode": mode, "exchange": exchange_id,
                            "margin_mode_scope": order_capabilities(ex, exchange_id, m)["margin_mode_scope"]})


def _handle_set_leverage(req_id, exchange_id, args):
    leverage = int(_arg(args, "leverage"))
    if leverage < 1:
        raise OrderRefused("leverage must be at least 1")
    ex = _get_exchange(exchange_id, need_auth=True)
    if not _require_cap(req_id, ex, 'setLeverage', exchange_id):
        return
    symbol, _m = _contract_market(ex, exchange_id, _arg(args, "symbol"))
    # ccxt picks the contract API from the market itself; a stray
    # "defaultType" param used to travel into the exchange request.
    with _SendWindow(ex, _ACCOUNT_CHANGE_HEADROOM_S):
        result = ex.set_leverage(leverage, symbol)
    _respond(req_id, True, {"outcome": "accepted", "symbol": symbol, "leverage": leverage,
                            "exchange": exchange_id, "result": result})


def _handle_set_margin_mode(req_id, exchange_id, args):
    mode = str(_arg(args, "mode")).lower()  # "cross" or "isolated"
    if mode not in ("cross", "isolated"):
        raise OrderRefused("margin mode must be cross or isolated")
    ex = _get_exchange(exchange_id, need_auth=True)
    if not _require_cap(req_id, ex, 'setMarginMode', exchange_id):
        return
    symbol, m = _contract_market(ex, exchange_id, _arg(args, "symbol"))
    params = {}
    if exchange_id in ("okx", "hyperliquid"):
        # Both set the mode together with a leverage (ccxt requires it).
        lev = args.get("leverage")
        if not lev:
            raise OrderRefused(f"{exchange_id} sets the margin mode together with a leverage; none was given")
        params["leverage"] = int(lev)
    with _SendWindow(ex, _ACCOUNT_CHANGE_HEADROOM_S):
        result = ex.set_margin_mode(mode, symbol, params)
    _respond(req_id, True, {"outcome": "accepted", "symbol": symbol, "margin_mode": mode,
                            "exchange": exchange_id,
                            "scope": order_capabilities(ex, exchange_id, m)["margin_mode_scope"],
                            "result": result})


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
    "warm": _handle_warm,
    "prepare_order": _handle_prepare_order,
    "place_order": _handle_place_order,
    "find_order": _handle_find_order,
    "cancel_order": _handle_cancel_order,
    "fetch_positions": _handle_fetch_positions,
    "fetch_open_orders": _handle_fetch_open_orders,
    "fetch_mark_price": _handle_fetch_mark_price,
    "fetch_my_trades": _handle_fetch_my_trades,
    "fetch_trading_fees": _handle_fetch_trading_fees,
    "fetch_leverage_info": _handle_fetch_leverage_info,
    "set_leverage": _handle_set_leverage,
    "set_margin_mode": _handle_set_margin_mode,
}

# Answered on the reader thread: they touch no exchange instance.
_INLINE_METHODS = {"ping", "list_exchange_ids"}

# Requests that change the account. One is sent only while at least its
# headroom of the caller's budget is left, so its reply normally arrives before
# the caller gives up; later it is answered EXPIRED and never sent.
_ACCOUNT_CHANGING = {"place_order", "cancel_order", "set_leverage", "set_margin_mode"}
_ACCOUNT_CHANGE_HEADROOM_S = _HTTP_TIMEOUT_MS / 1000.0 + 1.0


def _headroom(method):
    if method == "place_order":
        return _ORDER_HEADROOM_S
    return _ACCOUNT_CHANGE_HEADROOM_S if method in _ACCOUNT_CHANGING else 0.0


# ── Error normalization (matches exchange_client.py patterns) ───────────────
# Most specific class first: ExchangeNotAvailable and RequestTimeout derive
# from NetworkError, OrderNotFound from InvalidOrder, so with the base class
# listed first the specific codes were unreachable.

_CCXT_ERROR_MAP = (
    (_InvalidArgs, "INVALID_ARGS"),
    (OrderRefused, "ORDER_REFUSED"),
    (_DeadlineExpired, "EXPIRED"),
    (ccxt.AuthenticationError, "AUTH_ERROR"),
    (ccxt.InsufficientFunds, "INSUFFICIENT_FUNDS"),
    (ccxt.OrderNotFound, "ORDER_NOT_FOUND"),
    (ccxt.DuplicateOrderId, "DUPLICATE_ORDER"),
    (ccxt.InvalidOrder, "INVALID_ORDER"),
    (ccxt.BadSymbol, "BAD_SYMBOL"),
    (ccxt.NotSupported, "NOT_SUPPORTED"),
    (ccxt.RateLimitExceeded, "RATE_LIMIT"),
    # The request may or may not have reached the exchange (an order may exist).
    (ccxt.RequestTimeout, "REQUEST_TIMEOUT"),
    (ccxt.ExchangeNotAvailable, "EXCHANGE_UNAVAILABLE"),
    (ccxt.NetworkError, "NETWORK_ERROR"),
    (ccxt.ExchangeError, "EXCHANGE_ERROR"),
)

# Exchange errors that say the request was refused before anything was done.
_DEFINITIVE_REJECTIONS = (ccxt.AuthenticationError, ccxt.InsufficientFunds, ccxt.InvalidOrder,
                          ccxt.BadRequest, ccxt.OperationRejected, ccxt.NotSupported)


def _classify_error(exc):
    """Map a ccxt exception to an error code."""
    for exc_type, code in _CCXT_ERROR_MAP:
        if isinstance(exc, exc_type):
            return code
    return "UNKNOWN_ERROR"


def _failure_outcome(exc, sent):
    """For an account-changing request that raised: "rejected" when it surely
    did nothing (never sent, or a definitive exchange rejection), else
    "unknown" (it may have happened)."""
    if not sent:
        return "rejected"
    if isinstance(exc, ccxt.DuplicateOrderId):
        return "unknown"   # an order with this client id already exists
    if isinstance(exc, _DEFINITIVE_REJECTIONS):
        return "rejected"
    return "unknown"


def _execute(req_id, handler, method, exchange_id, args, expires_at):
    """Run one request and always answer it."""
    account_changing = method in _ACCOUNT_CHANGING
    if expires_at is not None and method != "set_credentials":
        if time.monotonic() + _headroom(method) > expires_at:
            _respond(req_id, False, code="EXPIRED",
                     error=f"{exchange_id}: the request waited too long behind other {exchange_id} "
                           f"requests and was not sent",
                     extra={"outcome": "rejected", "sent": False} if account_changing else None)
            return
    _call_state.expires_at = expires_at
    _call_state.method = method
    _call_state.armed = False
    _call_state.mutating = 0
    try:
        handler(req_id, exchange_id, args)
    except Exception as e:
        code = _classify_error(e)
        message = str(e) if isinstance(e, (ccxt.BaseError, _InvalidArgs, OrderRefused, _DeadlineExpired)) \
            else f"{type(e).__name__}: {e}"
        message = scrub_message(message, _credentials.get(exchange_id))
        extra = None
        if account_changing:
            sent = getattr(_call_state, "mutating", 0) > 0
            extra = {"outcome": _failure_outcome(e, sent), "sent": sent}
        if isinstance(e, OrderRefused) and e.alternatives:
            extra = dict(extra or {})
            extra["alternatives"] = list(e.alternatives)
        _respond(req_id, False, error=message, code=code, extra=extra)
    finally:
        _call_state.expires_at = None
        _call_state.armed = False
        _call_state.deadline = None


class _ExchangeWorker:
    """The one thread that talks to one exchange (see the module docstring)."""

    def __init__(self, exchange_id):
        self.queue = queue.Queue()
        self.thread = threading.Thread(target=self._run, name=f"exchange-{exchange_id}", daemon=True)
        self.thread.start()

    def _run(self):
        while True:
            item = self.queue.get()
            if item is None:
                return
            try:
                _execute(*item)
            except Exception as e:  # _execute answers everything; never lose the worker
                _diag(f"worker error: {e}")


_workers = {}  # exchange_id -> _ExchangeWorker; reader thread only
_stdin_closed = threading.Event()


def _worker_for(exchange_id):
    w = _workers.get(exchange_id)
    if w is None:
        w = _ExchangeWorker(exchange_id)
        _workers[exchange_id] = w
    return w


# ── Main event loop ─────────────────────────────────────────────────────────

def _dispatch(line, received):
    req_id = None
    try:
        req = json.loads(line)
        if not isinstance(req, dict):
            raise _InvalidArgs("a request must be a JSON object")
        req_id = req.get("id", "__unknown__")
        method = req.get("method")
        exchange_id = req.get("exchange") or ""
        args = req.get("args")
        if args is None:
            args = {}
        if not isinstance(args, dict):
            raise _InvalidArgs("'args' must be a JSON object")

        handler = _METHODS.get(method)
        if not handler:
            _respond(req_id, False, error=f"Unknown method: {method}", code="INVALID_METHOD")
            return

        expires_at = None
        timeout_ms = req.get("timeout_ms")
        if isinstance(timeout_ms, (int, float)) and not isinstance(timeout_ms, bool) and timeout_ms > 0:
            expires_at = received + timeout_ms / 1000.0

        if method in _INLINE_METHODS:
            _execute(req_id, handler, method, exchange_id, args, None)
            return
        if exchange_id not in ccxt.exchanges:
            _respond(req_id, False, error=f"Unknown exchange: {exchange_id}", code="INVALID_ARGS")
            return
        _worker_for(exchange_id).queue.put((req_id, handler, method, exchange_id, args, expires_at))

    except json.JSONDecodeError as e:
        _respond(req_id or "__parse_error__", False,
                 error=f"JSON parse error: {e}", code="PARSE_ERROR")
    except Exception as e:
        _respond(req_id or "__unknown__", False, error=str(e), code=_classify_error(e))


def _drop_queued_account_changes():
    """stdin closed (the app is exiting): an order or cancel still queued would
    be sent with nobody left to see the result. Answer those EXPIRED instead."""
    for w in _workers.values():
        kept = []
        while True:
            try:
                item = w.queue.get_nowait()
            except queue.Empty:
                break
            if item is not None and item[2] in _ACCOUNT_CHANGING:
                _respond(item[0], False, code="EXPIRED", error="the app closed before this request was sent",
                         extra={"outcome": "rejected", "sent": False})
            else:
                kept.append(item)
        for item in kept:
            w.queue.put(item)


def main():
    # Signal readiness
    _respond("__init__", True, {"status": "ready", "pid": os.getpid()})

    for line in sys.stdin:
        line = line.strip()
        if line:
            _dispatch(line, time.monotonic())

    # stdin closed: answer what is already queued (each request still honours
    # its own expiry; account changes are not sent), then exit. Bounded so a
    # wedged exchange cannot keep the process alive.
    _stdin_closed.set()
    _drop_queued_account_changes()
    for w in _workers.values():
        w.queue.put(None)
    deadline = time.monotonic() + 60.0
    for w in _workers.values():
        w.thread.join(timeout=max(0.0, deadline - time.monotonic()))


def run_cli(method, exchange_id, args, credentials=None, emit=True):
    """Run one daemon handler from a one-shot script (scripts/exchange/*.py) —
    the same code the app uses, so the scripts inherit every rule above. The
    reply is written to stdout in the daemon's own shape (unless emit=False).
    Returns (exit status, reply dict)."""
    if credentials and any(credentials.get(k) for k in _CREDENTIAL_FIELDS):
        _credentials[exchange_id] = credentials
    replies = []
    real = _respond

    def capture(req_id, success, data=None, error=None, code=None, extra=None):
        resp = {"id": req_id, "success": success}
        if data is not None:
            resp["data"] = data
        if error is not None:
            resp["error"] = error
        if code is not None:
            resp["code"] = code
        for k, v in (extra or {}).items():
            resp.setdefault(k, v)
        replies.append(resp)
        if emit:
            real(req_id, success, data, error, code, extra)

    globals()["_respond"] = capture
    try:
        handler = _METHODS.get(method)
        if handler is None:
            capture("cli", False, error=f"Unknown method: {method}", code="INVALID_METHOD")
        elif method not in _INLINE_METHODS and exchange_id not in ccxt.exchanges:
            capture("cli", False, error=f"Unknown exchange: {exchange_id}", code="INVALID_ARGS")
        else:
            _execute("cli", handler, method, exchange_id, args, None)
    finally:
        globals()["_respond"] = real
    reply = replies[-1] if replies else {"id": "cli", "success": False, "error": "no reply", "code": "UNKNOWN_ERROR"}
    return (0 if reply.get("success") else 1), reply


def cli_usage_error(usage):
    """The daemon's reply shape for a bad command line."""
    _respond("cli", False, error=f"Usage: {usage}", code="INVALID_ARGS")
    return 2


def cli_credentials():
    """Credentials JSON from stdin (the scripts never take them on argv)."""
    data = ""
    try:
        data = sys.stdin.read()
        return json.loads(data) if data.strip() else {}
    except (ValueError, OSError) as exc:
        # The script then runs unauthenticated and the exchange answers with an auth
        # error that hides this cause. Size only — never the credentials.
        _diag(f"ERROR Credentials on stdin are not valid JSON bytes={len(data)} error={type(exc).__name__}")
        return {}


if __name__ == "__main__":
    try:
        signal.signal(signal.SIGTERM, lambda s, f: sys.exit(0))
    except Exception:
        pass

    try:
        main()
    except (KeyboardInterrupt, EOFError):
        pass
