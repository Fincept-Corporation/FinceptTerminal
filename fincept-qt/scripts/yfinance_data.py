"""
YFinance Data Fetcher
Fetches real-time stock quotes and historical data using yfinance
Returns JSON output for Qt/C++ integration
"""

import contextlib
import io
import logging
import math
import os
import sys
import json
import re
import threading
import time
import pandas as pd
from datetime import datetime

# Safe JSON output boundary. `json.dumps` defaults to allow_nan=True and emits
# the bare tokens NaN / Infinity / -Infinity, none of which are valid JSON.
# QJsonDocument::fromJson() on the C++ side does not repair them — it returns a
# *null document*, so the host throws away the ENTIRE payload, not just the
# offending field. One halted bar inside a 250-row series is enough to blank a
# whole chart with no error shown. Every print/frame boundary in this file goes
# through fincept_json.
try:
    import fincept_json
except ImportError:  # defensive: scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

# Every Yahoo request of this file goes through fincept_yahoo: one session per
# process with the cookie + crumb persisted on disk, the cross-process limiter,
# back-off and circuit breaker, and the shared caches (quotes per market session,
# the on-disk bar cache for every history, fundamentals once a day). `yf` is the
# configured yfinance module.
import fincept_yahoo as fy  # noqa: E402

yf = fy.yf()

# Per-HTTP-request budget for every yfinance call reachable from the daemon.
# run_daemon() is a strictly serial read -> dispatch -> write loop, so a single
# unbounded Yahoo request blocks every request queued behind it — quotes,
# sparklines, history, search and financials all stall together. yfinance's own
# default is 10s but it is applied per underlying HTTP call and silently
# inherited; pin it explicitly so the budget is visible and tunable.
# Override with FINCEPT_YF_TIMEOUT (seconds).
try:
    _NET_TIMEOUT = float(os.environ.get("FINCEPT_YF_TIMEOUT", "15"))
except (TypeError, ValueError):
    _NET_TIMEOUT = 15.0


def _price_digits(value):
    """Decimal places that keep a price's significant digits.

    A flat 2 dp suits most equities but destroys FX rates (EURUSD 1.0812 ->
    1.08, a 12-pip error) and low-priced crypto (SHIB 0.0000123 -> 0.0, DOGE
    0.0987 -> 0.1). Scale with magnitude instead: 2 dp from 1000 up, 4 dp from
    1, and ~5 significant figures below 1. Keys and types are unchanged; prices
    simply stop being truncated.
    """
    try:
        a = abs(float(value))
    except (TypeError, ValueError):
        return 2
    if not math.isfinite(a) or a == 0 or a >= 1000:
        return 2
    if a >= 1:
        return 4
    return min(12, 4 - int(math.floor(math.log10(a))))


def _px(value, digits=None):
    """round() a price to `digits`, or to _price_digits(value) when omitted."""
    v = float(value)
    return round(v, _price_digits(v) if digits is None else digits)


def _num(value, digits=None):
    """`round(float(value), digits)`, or None when the cell is NaN / NA / None.

    `digits=None` (the default; every caller passes a price) picks the
    precision from the value's magnitude — see _price_digits.

    yfinance returns NaN cells routinely: halted sessions, missing intraday
    bars, thinly-traded tickers, futures roll gaps. `round(float(nan), 2)` is
    still nan and nan is not valid JSON (see the fincept_json note above), so
    an unguarded conversion voids the whole response.
    """
    if value is None:
        return None
    try:
        if pd.isna(value):
            return None
        return _px(value, digits)
    except (TypeError, ValueError):
        return None


def _int(value, default=None):
    """`int(value)`, or `default` when the cell is NaN / NA / None.

    Distinct from _num because `int(nan)` does not merely produce a bad number,
    it raises ValueError — which is how one missing volume cell used to abort
    the whole loop and turn an entire price series into {"error": ...}.
    """
    if value is None:
        return default
    try:
        if pd.isna(value):
            return default
        return int(value)
    except (TypeError, ValueError):
        return default


# ── Failure reporting ────────────────────────────────────────────────────────
#
# A fetch that yields nothing is an error, never an empty success. `[]` / `{}`
# reach the host as "loaded, no rows": it paints a blank chart or table, caches
# the emptiness, and the user never learns why. Failures are returned as
#
#     {"error": "<short user-facing reason>", "symbol": <sym>, "code": <code>}
#
# PythonRunner turns any top-level "error" into a failed result carrying that
# text. `code` is the stable, machine-readable class the C++ side branches on:
#
#   not_found     Yahoo says the symbol is unknown or delisted
#   no_data       the symbol exists but Yahoo has no rows for this request
#   rate_limited  HTTP 429 / "Too Many Requests"
#   network       connect / DNS / TLS / proxy / timeout failure, or Yahoo down
#   unknown       anything else

_RATE_LIMIT_RE = re.compile(r"too many requests|rate[ -]?limit", re.I)
_HTTP_STATUS_RE = re.compile(
    r"\bhttp(?:\s+error)?\s*:?\s*([1-5]\d\d)\b|status[_ ]?code\s*=?\s*([1-5]\d\d)\b", re.I)
_NETWORK_RE = re.compile(
    r"curl: \(\d+\)|failed to perform|could not connect|failed to connect|no connection could be made"
    r"|connection (?:refused|reset|aborted|error|timed out|closed)|timed out|\btime ?out\b"
    r"|could not resolve|name resolution|getaddrinfo|nodename nor servname|name or service not known"
    r"|network is unreachable|no route to host|\bproxy\b|\bssl\b|\btls\b|certificate|handshake"
    r"|remote end closed|remotedisconnected|max retries exceeded|urlopen error|eof occurred"
    r"|empty reply|recv failure|send failure|currently down", re.I)
_NOT_FOUND_RE = re.compile(
    r"symbol may be delisted|quote not found|no timezone found|not found for symbol"
    r"|no data found for symbol|invalid ticker|unknown symbol", re.I)
_NO_DATA_RE = re.compile(
    r"no price data found|is invalid, must be one of|invalid period|data doesn't exist"
    r"|not available for|no data", re.I)
# The part of a YFPricesMissingError that is Yahoo's own answer, e.g.
#   (Yahoo error = "No data found, symbol may be delisted")
_YAHOO_DESC_RE = re.compile(r'yahoo error = "([^"]*)"', re.I)
_CURL_DETAIL_RE = re.compile(r"curl: \((\d+)\)\s*(.*?)(?:\.?\s*See https?://\S+.*)?$", re.I | re.S)
_REPR_WRAP_RE = re.compile(r"""^\w+\((['"])(.*)\1\)$""", re.S)
_TICKER_PREFIX_RE = re.compile(r"\$\S+?:\s")
# Exception classes that always mean the request never got a usable answer:
# curl_cffi's transport errors plus the stdlib ones (urllib is used by search).
# curl_cffi's HTTPError is deliberately absent — its status code decides.
_NETWORK_EXC_NAMES = frozenset((
    "ConnectionError", "ConnectTimeout", "ReadTimeout", "Timeout", "ProxyError", "SSLError",
    "CertificateVerifyError", "DNSError", "URLError", "timeout", "gaierror", "TimeoutError",
    "ConnectionRefusedError", "ConnectionResetError", "ConnectionAbortedError",
    "RemoteDisconnected", "SSLCertVerificationError", "SSLEOFError",
))
_TRANSIENT_CODES = frozenset(("network", "rate_limited"))


def _failure_code(text, exc=None):
    """Code for one piece of failure text (or None when nothing matches)."""
    names = {c.__name__ for c in type(exc).__mro__} if exc is not None else set()
    if "YFRateLimitError" in names or _RATE_LIMIT_RE.search(text):
        return "rate_limited"
    m = _HTTP_STATUS_RE.search(text)
    status = int(m.group(1) or m.group(2)) if m else None
    if status == 429:
        return "rate_limited"
    if status == 404:
        return "not_found"
    if status == 407:
        return "network"  # proxy authentication required
    if status is not None and status >= 400:
        return "unknown"
    if names & _NETWORK_EXC_NAMES or _NETWORK_RE.search(text):
        return "network"
    if _NOT_FOUND_RE.search(text):
        return "not_found"
    if "YFInvalidPeriodError" in names or _NO_DATA_RE.search(text):
        return "no_data"
    return None


def _strip_symbol(text, symbol):
    """Remove the ticker from `text` before pattern matching, so a symbol such as
    SSL or TLS cannot read as a network error."""
    text = _TICKER_PREFIX_RE.sub(" ", text)
    for s in {str(symbol), str(symbol).upper()} if symbol else ():
        text = re.sub(r"(?<![A-Za-z0-9])" + re.escape(s) + r"(?![A-Za-z0-9])", " ", text)
    return text


def _short_text(text, limit=160):
    """One readable line of an error: repr() wrapper, ticker prefix and URL tail removed."""
    text = str(text or "").strip()
    m = _REPR_WRAP_RE.match(text)
    if m:
        text = m.group(2)
    m = re.match(r"^<urlopen error (.*)>$", text, re.S)  # urllib's URLError
    if m:
        text = m.group(1)
    text = _TICKER_PREFIX_RE.sub("", text)
    text = re.sub(r"\s*See https?://\S+.*$", "", text, flags=re.S)
    text = " ".join(text.split())
    return text if len(text) <= limit else text[:limit - 3].rstrip() + "..."


def _rate_limit_extra():
    """{"retry_after_s": n} while fincept_yahoo's circuit breaker holds Yahoo traffic back
    (the host uses it as its retry hint), else {}."""
    try:
        st = fy.status()
        if st.get("breaker_open") and st.get("retry_at"):
            return {"retry_after_s": max(1, int(st["retry_at"] - time.time()))}
    except Exception:
        pass
    return {}


def _failure_message(code, text, symbol=None, no_data_msg=None):
    if code == "rate_limited":
        st = fy.status()
        if st.get("breaker_open") and st.get("message"):
            return st["message"]  # "... showing saved data, retry at HH:MM"
        m = re.search(r"retry at \d\d:\d\d", str(text or ""))
        if m:
            return f"Yahoo Finance is rate limiting requests - showing saved data, {m.group(0)}"
        return "Yahoo Finance is rate limiting requests (HTTP 429 Too Many Requests); retry in a minute"
    if code == "network":
        if re.search(r"currently down", text, re.I):
            return "Yahoo Finance is temporarily unavailable (the service reports it is down)"
        m = _CURL_DETAIL_RE.search(text)
        if m:
            detail = m.group(2).strip().rstrip("'\").").strip()
            return f"Cannot reach Yahoo Finance: {_short_text(detail, 140)} (curl error {m.group(1)})"
        return f"Cannot reach Yahoo Finance: {_short_text(text, 140)}"
    if code == "not_found":
        return (f"Symbol '{symbol}' not found on Yahoo Finance (unknown or delisted)" if symbol
                else "Symbol not found on Yahoo Finance (unknown or delisted)")
    if code == "no_data":
        base = no_data_msg or (f"Yahoo Finance returned no data for {symbol}" if symbol
                               else "Yahoo Finance returned no data")
        m = _YAHOO_DESC_RE.search(text or "")
        return f"{base} ({_short_text(m.group(1), 120)})" if m else base
    where = f" for {symbol}" if symbol else ""
    m = _HTTP_STATUS_RE.search(text or "")
    if m:
        return f"Yahoo Finance request failed{where}: HTTP {m.group(1) or m.group(2)}"
    return f"Yahoo Finance request failed{where}: {_short_text(text) or 'no details'}"


def _classify_failure(err=None, symbol=None, hints=None, default="unknown", no_data_msg=None):
    """Map a failure to (code, short user-facing message).

    `err` is an exception or yfinance's per-symbol error text (yfinance.shared._ERRORS).
    `hints` are yfinance ERROR log lines captured during the call (see _yf_quiet):
    yfinance swallows some failures — a timezone lookup that could not connect
    resurfaces as "possibly delisted; no timezone found", an HTTP error inside
    Ticker.info only reaches the log — so the log can carry the real cause.
    `default` is the code used when nothing recognisable is found.
    `no_data_msg` replaces the generic message for code "no_data".
    """
    exc = err if isinstance(err, BaseException) else None
    if exc is not None:
        raw = str(exc).strip() or type(exc).__name__
        text = f"{type(exc).__name__}: {raw}"
    else:
        raw = text = str(err or "").strip()
    code = _failure_code(_strip_symbol(text, symbol), exc) if text else None
    msg_text = raw

    hint_text = " | ".join(str(h) for h in (hints or []) if h)
    if hint_text:
        hint_code = _failure_code(_strip_symbol(hint_text, symbol))
        explicit = bool(_YAHOO_DESC_RE.search(text)) or "quote not found" in text.lower()
        if hint_code in _TRANSIENT_CODES and code not in _TRANSIENT_CODES and not explicit:
            code, msg_text = hint_code, hint_text
        elif code is None and hint_code:
            code, msg_text = hint_code, hint_text
    code = code or default
    return code, _failure_message(code, msg_text, symbol, no_data_msg)


def _failure(symbol, code, message, **extra):
    """The error object every fetch returns instead of an empty container."""
    out = {"error": message, "symbol": symbol, "code": code}
    if code == "rate_limited":
        out.update(_rate_limit_extra())
    out.update(extra)
    return out


def _error_result(err, symbol, hints=None, default="unknown", no_data_msg=None):
    code, message = _classify_failure(err, symbol, hints, default, no_data_msg)
    return _failure(symbol, code, message)


_CODE_PRIORITY = ("rate_limited", "network", "unknown", "no_data", "not_found")


def _summarize_failures(errors):
    """_summarize_failures_raw() plus "retry_after_s" while the circuit breaker is open."""
    out = _summarize_failures_raw(errors)
    if out.get("code") == "rate_limited":
        out.update(_rate_limit_extra())
    return out


def _summarize_failures_raw(errors):
    """One {"code", "message"} for a request in which every symbol failed.

    `errors` is {symbol: {"code", "message"}}. A transport-level cause (rate
    limit, network) wins over per-symbol causes because it is what the user
    must act on.
    """
    items = [e for e in errors.values() if isinstance(e, dict)]
    if not items:
        return {"code": "unknown", "message": "Yahoo Finance returned no data"}
    if len(items) == 1:
        return {"code": items[0].get("code", "unknown"), "message": items[0].get("message", "")}
    codes = [i.get("code") for i in items]
    code = next((c for c in _CODE_PRIORITY if c in codes), codes[0] or "unknown")
    first = next(i for i in items if i.get("code") == code)
    if code in ("rate_limited", "network", "unknown"):
        return {"code": code, "message": first.get("message", "")}
    if all(c == "not_found" for c in codes):
        return {"code": code, "message": f"None of the {len(items)} requested symbols were found on Yahoo Finance"}
    return {"code": code, "message": f"Yahoo Finance returned no data for any of the {len(items)} requested symbols"}


class _YFLogCollector(logging.Handler):
    """Collects yfinance ERROR lines for _classify_failure and echoes every
    WARNING+ line to stderr, exactly as logging's last-resort handler did before
    this handler existed (the daemon host logs stderr; stdout stays pure JSON)."""

    def __init__(self, sink, echo):
        super().__init__(level=logging.WARNING)
        self.sink = sink
        self.echo = echo

    def emit(self, record):
        try:
            msg = record.getMessage()
        except Exception:
            return
        if record.levelno >= logging.ERROR and self.sink is not None:
            self.sink.append(msg)
        if self.echo:
            try:
                sys.stderr.write(msg + "\n")
            except Exception:
                pass


@contextlib.contextmanager
def _yf_quiet(log_sink=None):
    """Around yfinance calls: swallow stray stdout (it would corrupt the JSON the
    host parses) and append yfinance's ERROR log lines to `log_sink`."""
    yf_logger = logging.getLogger("yfinance")
    echo = not yf_logger.handlers and not logging.getLogger().handlers
    handler = _YFLogCollector(log_sink, echo)
    yf_logger.addHandler(handler)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            yield log_sink
    finally:
        yf_logger.removeHandler(handler)


class _YFThreadLogCollector(_YFLogCollector):
    """_YFLogCollector for a thread pool: ERROR lines are kept as (thread ident,
    text) so each pooled fetch classifies its failure from its own lines only
    (_pooled_hints), never another thread's."""

    def emit(self, record):
        try:
            msg = record.getMessage()
        except Exception:
            return
        if record.levelno >= logging.ERROR:
            self.sink.append((record.thread, msg))
        if self.echo:
            try:
                sys.stderr.write(msg + "\n")
            except Exception:
                pass


@contextlib.contextmanager
def _yf_quiet_pool():
    """_yf_quiet for a thread pool, entered ONCE around the whole pool on the
    calling thread: redirecting sys.stdout from several threads races and can
    leave it pointing at a buffer (the final JSON print would then be lost).
    Yields the shared log sink for _pooled_hints."""
    sink = []
    yf_logger = logging.getLogger("yfinance")
    echo = not yf_logger.handlers and not logging.getLogger().handlers
    handler = _YFThreadLogCollector(sink, echo)
    yf_logger.addHandler(handler)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            yield sink
    finally:
        yf_logger.removeHandler(handler)


def _pooled_hints(sink):
    """A zero-arg callable returning the yfinance ERROR lines the CURRENT thread
    logged into `sink` from now on (pass to _classify_failure as hints)."""
    me = threading.get_ident()
    start = len(sink)
    return lambda: [m for (tid, m) in sink[start:] if tid == me]


def _yf_history(ticker, **kwargs):
    """ticker.history() that raises the real cause of a failure.

    With yfinance's default raise_errors=False a refused connection, a TLS
    failure and an unknown ticker all come back as an empty frame plus the same
    "possibly delisted; no price data found" log line, so the cause is lost.
    raise_errors=True re-raises the curl/HTTP exception instead. It also turns a
    failed auto_adjust into an exception where the lenient mode logs it and
    returns unadjusted bars; that one case is retried leniently so successful
    output stays exactly what it was.
    """
    try:
        return ticker.history(raise_errors=True, **kwargs)
    except TypeError as e:
        if "raise_errors" not in str(e):
            raise
        return ticker.history(**kwargs)  # yfinance without raise_errors
    except Exception as e:
        if "adjust failed" in str(e):
            return ticker.history(raise_errors=False, **kwargs)
        raise


def _yf_download_errors():
    """Per-symbol failure text recorded by the last yf.download() call:
    {SYMBOL (upper case, as yfinance keys it): text}. Empty when this yfinance
    has no shared._ERRORS."""
    try:
        from yfinance import shared as _yf_shared
        errs = getattr(_yf_shared, "_ERRORS", None)
        if isinstance(errs, dict):
            return {str(k).upper(): str(v) for k, v in errs.items()}
    except Exception:
        pass
    return {}


def get_quote(symbol):
    """Quote for one symbol: the batched v7 quote row (one shared, cached request - see
    _batch_quotes_detailed) plus Yahoo's top of book. Ticker.info + a 1-day history used to cost
    three to four requests per call for the same fields."""
    no_data_msg = f"No quote data for {symbol}"
    if not symbol:
        return _failure(symbol, "not_found", "No symbol given")
    try:
        quotes, errors, total = _batch_quotes_detailed([symbol])
    except Exception as e:
        return _error_result(e, symbol, no_data_msg=no_data_msg)
    if quotes:
        q = dict(quotes[0])
        row = _V7_LAST_ROWS.get(str(symbol).upper()) or {}
        q["bid"] = _num(row.get("bid")) if _positive(row.get("bid")) else None
        q["ask"] = _num(row.get("ask")) if _positive(row.get("ask")) else None
        q.setdefault("market_time", None)
        q.setdefault("market_state", None)
        q.setdefault("quote_type", None)
        if not q.get("exchange"):
            q["exchange"] = _text(row.get("exchange")) or ""
        return q
    err = errors.get(symbol) or total or {}
    if err.get("code") in ("rate_limited", "network"):
        return _failure(symbol, err.get("code"), err.get("message") or no_data_msg)
    return _get_quote_legacy(symbol)


def _get_quote_legacy(symbol):
    """The pre-cache single quote (Ticker.info + 1-day history): only for a symbol neither the
    v7 quote nor the bar cache could price."""
    logs = []
    no_data_msg = f"No quote data for {symbol}"
    try:
        ticker = yf.Ticker(symbol)
        with _yf_quiet(logs):
            info = fy.info(symbol)  # shared daily cache; {} = Yahoo has no such symbol
            if not info:
                return _error_result(None, symbol, logs, default="not_found", no_data_msg=no_data_msg)
            hist = _yf_history(ticker, period="1d", timeout=_NET_TIMEOUT)

        if hist is None or hist.empty:
            return _error_result(None, symbol, logs, default="no_data", no_data_msg=no_data_msg)

        current_price = hist['Close'].iloc[-1]
        previous_close = info.get('previousClose', current_price)
        change = current_price - previous_close
        change_percent = (change / previous_close) * 100 if previous_close else 0

        d = _price_digits(current_price)
        # Ticker.info already carries the v7 quote keys, so the extended fields
        # (currency, exchange names, timezone, session prices, averages) cost no
        # extra request here. Keys set explicitly below win over these.
        quote_data = _quote_meta(info, symbol)
        quote_data.update({
            "symbol": symbol,
            "price": _px(current_price, d),
            "change": _px(change, d),
            "change_percent": round(float(change_percent), 2),
            "volume": int(hist['Volume'].iloc[-1]) if not hist['Volume'].empty else None,
            "high": _px(hist['High'].iloc[-1], d) if not hist['High'].empty else None,
            "low": _px(hist['Low'].iloc[-1], d) if not hist['Low'].empty else None,
            "open": _px(hist['Open'].iloc[-1], d) if not hist['Open'].empty else None,
            "previous_close": _px(previous_close, d),
            "timestamp": int(datetime.now().timestamp()),  # when this was fetched, not market time
            "exchange": info.get('exchange', ''),
            # "As of": the session the price belongs to, plus Yahoo's own last-trade time
            # (epoch seconds) and session state from the same info call (no extra request).
            "market_date": _market_date(hist.index[-1]),
            "market_time": int(float(info['regularMarketTime'])) if _positive(info.get('regularMarketTime')) else None,
            "market_state": _market_state(info.get('marketState')),
            # Top of book from the same info call (no extra request). Yahoo reports 0 or nothing
            # when a side is not quoted; that is "no quote", never a price.
            "bid": _num(info.get('bid')) if _positive(info.get('bid')) else None,
            "ask": _num(info.get('ask')) if _positive(info.get('ask')) else None,
            "quote_type": quote_data.get("quote_type"),
            "price_digits": _display_digits(current_price, quote_data.get("quote_type"),
                                            quote_data.pop("price_hint", None), symbol),
        })

        return quote_data
    except Exception as e:
        return _error_result(e, symbol, logs, no_data_msg=no_data_msg)


def _market_date(ts):
    """'YYYY-MM-DD' of a price bar's index timestamp, in the exchange's own calendar
    (daily bars are stamped at the session's local midnight), or None."""
    try:
        return pd.Timestamp(ts).strftime('%Y-%m-%d')
    except Exception:
        return None


def _positive(value):
    """True for a finite number > 0."""
    try:
        v = float(value)
    except (TypeError, ValueError):
        return False
    return math.isfinite(v) and v > 0


def _finite(value):
    """float(value) when it is a finite number, else None. Booleans are not numbers."""
    if value is None or isinstance(value, bool):
        return None
    try:
        v = float(value)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


def _pos(value):
    """float(value) when it is a finite number > 0, else None. For prices, volumes
    and counts, where Yahoo writes 0 for "not reported" (FX volume, fund averages)."""
    v = _finite(value)
    return v if v is not None and v > 0 else None


def _epoch(value):
    """Whole epoch seconds from a positive timestamp, else None."""
    v = _pos(value)
    return int(v) if v is not None else None


def _first(*values):
    """The first value that is not None (0 and 0.0 are values)."""
    return next((v for v in values if v is not None), None)


def _text(value):
    """A stripped non-empty string, else None ("N/A" style placeholders included)."""
    s = str(value).strip() if value is not None else ""
    return s if s and s.upper() not in ("N/A", "NONE", "NULL") else None


# ── Quote metadata (QuoteData extension fields) ──────────────────────────────
#
# Batched quotes are priced from Yahoo's v7 quote endpoint, one request per
# _V7_QUOTE_CHUNK symbols (the same endpoint Ticker.info reads), through
# fincept_yahoo's quote cache: a row stays valid 15 s while its market trades and
# until the next session open while it is closed, in memory and in an on-disk
# snapshot shared by every process. The same answer supplies currency, exchange,
# session state, pre/post prices, averages and the display name. Symbols it does
# not price go through daily bars of the shared bar cache, which keep no
# per-symbol metadata; when v7 fails altogether those rows go out without the
# extension fields, except the static ones remembered in _quote_static_cache.

_V7_QUOTE_URL = "https://query1.finance.yahoo.com/v7/finance/quote"
_V7_QUOTE_CHUNK = 50

# A v7 quote whose regularMarketTime is older than this (or missing) is not a
# current price: Yahoo keeps a dead ticker's last quote for years, while the
# daily-bar path only ever priced from the last 5 trading days (period="5d").
_V7_PRIMARY_MAX_AGE_S = 10 * 86400

# USD-base FX pairs have two Yahoo spellings for one instrument (JPY=X and
# USDJPY=X); asked for both in one request, v7 answers only one of them.
_USD_FX_PAIR_RE = re.compile(r"^(USD)?([A-Z]{3})=X$")

# Yahoo marketState -> the four states the host shows. PREPRE / POSTPOST are
# the closed hours before pre-market and after post-market.
_MARKET_STATES = {
    "PRE": "PRE", "REGULAR": "REGULAR", "POST": "POST", "CLOSED": "CLOSED",
    "PREPRE": "CLOSED", "POSTPOST": "CLOSED",
}

# Per-instrument facts that do not change between quotes, remembered from the
# last v7 answer for the daemon's lifetime so one failed metadata request does
# not blank a row's currency, exchange or name.
_QUOTE_STATIC_KEYS = ("currency", "exchange", "exchange_name", "exchange_timezone", "quote_type",
                      "exchange_delay_min", "price_hint", "name")
_quote_static_cache = {}
_QUOTE_STATIC_CACHE_MAX = 5000


def _market_state(raw):
    """PRE / REGULAR / POST / CLOSED for a Yahoo marketState, None when unknown."""
    return _MARKET_STATES.get(str(raw or "").strip().upper())


def _guess_quote_type(symbol):
    """quoteType implied by Yahoo's ticker syntax — used only to pick display
    digits when Yahoo did not say; never reported as the quote_type."""
    s = str(symbol or "").upper()
    if s.endswith("=X"):
        return "CURRENCY"
    if s.endswith("=F"):
        return "FUTURE"
    if s.startswith("^"):
        return "INDEX"
    return None


def _display_digits(price, quote_type=None, price_hint=None, symbol=None):
    """Decimals to DISPLAY `price` with (QuoteData.price_digits).

    Yahoo's priceHint when it gives one (2 for most equities, 4 for FX), else 4
    for FX and 2 otherwise. Below 1 at least four significant digits are kept
    whatever the hint says: Yahoo hints 5 for SHIB-USD at 0.00000393. Distinct
    from _price_digits, which is the (finer) precision prices are rounded to.
    """
    hint = _finite(price_hint)
    if hint is not None and 0 <= hint <= 12 and hint == int(hint):
        digits = int(hint)
    else:
        qt = str(quote_type or _guess_quote_type(symbol) or "").upper()
        digits = 4 if qt == "CURRENCY" else 2
    a = _finite(price)
    if a is not None and 0 < abs(a) < 1:
        digits = max(digits, min(12, 3 - int(math.floor(math.log10(abs(a))))))
    return digits


def _quote_meta(row, symbol=None):
    """The QuoteData extension fields read from a Yahoo v7 quote row — or a
    Ticker.info dict, which carries the same keys. Absent values are omitted,
    never zero-filled. "price_hint" is internal (consumed by _display_digits)."""
    if not isinstance(row, dict):
        return {}
    qt = _text(row.get("quoteType"))
    delay = _finite(row.get("exchangeDataDelayedBy"))
    hint = _finite(row.get("priceHint"))
    fields = {
        # longName over the often truncated shortName; futures lose the contract
        # month ("Gold Dec 26" -> "Gold"). See _clean_display_name.
        "name": _text(_clean_display_name(str(symbol or row.get("symbol") or ""), row)),
        "currency": _text(row.get("currency")),          # as reported: "GBp" stays "GBp"
        "exchange": _text(row.get("exchange")),          # code: NMS, LSE, JPX
        "exchange_name": _text(row.get("fullExchangeName")),  # NasdaqGS, LSE, Tokyo
        "exchange_timezone": _text(row.get("exchangeTimezoneName")),  # IANA id
        "quote_type": qt.upper() if qt else None,
        "exchange_delay_min": int(delay) if delay is not None and delay >= 0 else None,
        "price_hint": int(hint) if hint is not None and 0 <= hint <= 12 else None,
        "market_state": _market_state(row.get("marketState")),
        "market_time": _epoch(row.get("regularMarketTime")),
        "pre_market_price": _num(_pos(row.get("preMarketPrice"))),
        "pre_market_change_percent": _finite(row.get("preMarketChangePercent")),
        "pre_market_time": _epoch(row.get("preMarketTime")),
        "post_market_price": _num(_pos(row.get("postMarketPrice"))),
        "post_market_change_percent": _finite(row.get("postMarketChangePercent")),
        "post_market_time": _epoch(row.get("postMarketTime")),
        # 3-month average daily volume (Ticker.info names it averageVolume).
        "average_volume": _first(_pos(row.get("averageDailyVolume3Month")), _pos(row.get("averageVolume"))),
        "average_volume_10d": _first(_pos(row.get("averageDailyVolume10Day")),
                                     _pos(row.get("averageVolume10days"))),
        "fifty_two_week_high": _num(_pos(row.get("fiftyTwoWeekHigh"))),
        "fifty_two_week_low": _num(_pos(row.get("fiftyTwoWeekLow"))),
        "fifty_day_average": _num(_pos(row.get("fiftyDayAverage"))),
        "two_hundred_day_average": _num(_pos(row.get("twoHundredDayAverage"))),
    }
    for k in ("pre_market_change_percent", "post_market_change_percent"):
        if fields[k] is not None:
            fields[k] = round(fields[k], 4)
    return {k: v for k, v in fields.items() if v is not None}


# The newest v7 rows handed out, by symbol (get_quote reads its bid / ask here).
_V7_LAST_ROWS = {}


def _v7_quote_rows(symbols):
    """{SYMBOL (upper case): Yahoo v7 quote row} for `symbols`, through fincept_yahoo's
    quote cache: a row still fresh for its market (15 s while it trades, until the next
    session open while it is closed) costs no request; the rest go out in one request per
    50 symbols. Each row carries "_fetched" (epoch s Yahoo answered) and "_served"
    ("network" | "memory" | "disk" | "stale"). Never raises: a symbol whose request failed is absent
    (or answered from an older cached row while Yahoo throttles)."""
    syms = list(dict.fromkeys(str(s) for s in symbols if s))
    if not syms:
        return {}
    try:
        with _yf_quiet():
            ents = fy.quote_entries(syms)
    except Exception:
        return {}
    rows = {}
    for sym, e in ents.items():
        row = dict(e["row"])
        row["_fetched"] = int(e["t"])
        row["_served"] = e.get("src") or "network"
        rows[sym] = row
    if len(_V7_LAST_ROWS) > _QUOTE_STATIC_CACHE_MAX:
        _V7_LAST_ROWS.clear()
    _V7_LAST_ROWS.update(rows)
    return rows


def _v7_session_quote(symbol, row):
    """A batched-quote row built from a Yahoo v7 quote row's regular-session
    fields, or None when it carries no usable price.

    price, previous_close, change, change_percent and market_date all describe
    one session: regularMarketPrice against regularMarketPreviousClose, dated by
    regularMarketTime in the exchange's time zone. The daily bars cannot promise
    that: for hours after the close of Asian venues the newest bar is missing
    (indices) or has a volume but no prices (NSE/BSE), so a bar-priced row shows
    the previous session. Used as the primary price (_v7_primary_quote) and by
    the daily-bar fallback in _bar_quotes."""
    if not isinstance(row, dict):
        return None
    price = _pos(row.get("regularMarketPrice"))
    if price is None:
        return None
    prev = _pos(row.get("regularMarketPreviousClose"))
    change = _finite(row.get("regularMarketChange"))
    pct = _finite(row.get("regularMarketChangePercent"))
    if prev is not None:
        change = price - prev
        pct = (change / prev) * 100
    d = _price_digits(price)
    vol = _finite(row.get("regularMarketVolume"))
    market_date = ""
    ts = _epoch(row.get("regularMarketTime"))
    if ts:
        try:
            stamp = pd.Timestamp(ts, unit="s", tz="UTC")
            tz_name = _text(row.get("exchangeTimezoneName"))
            if tz_name:
                stamp = stamp.tz_convert(tz_name)
            market_date = stamp.strftime("%Y-%m-%d")
        except Exception:
            market_date = ""
    return {
        "symbol": symbol,
        "price": _px(price, d),
        "change": _px(change, d) if change is not None else None,
        "change_percent": round(pct, 2) if pct is not None else None,
        "volume": int(vol) if vol is not None and vol >= 0 else 0,
        "high": _px(_pos(row.get("regularMarketDayHigh")), d) if _pos(row.get("regularMarketDayHigh")) else None,
        "low": _px(_pos(row.get("regularMarketDayLow")), d) if _pos(row.get("regularMarketDayLow")) else None,
        "open": _px(_pos(row.get("regularMarketOpen")), d) if _pos(row.get("regularMarketOpen")) else None,
        "previous_close": _px(prev, d) if prev is not None else None,
        # When Yahoo answered for this row (a cached row keeps its fetch time - it never
        # looks fresher than it is), and where it came from this time.
        "timestamp": int(row.get("_fetched") or datetime.now().timestamp()),
        "served_from": row.get("_served") or "network",
        "exchange": "",
        "market_date": market_date,
    }


def _v7_primary_quote(symbol, row, now):
    """_v7_session_quote(symbol, row) when the row is a current quote, else None
    (the caller then prices `symbol` from daily bars): its regularMarketTime must
    be no older than _V7_PRIMARY_MAX_AGE_S before `now` (epoch seconds)."""
    ts = _epoch(row.get("regularMarketTime")) if isinstance(row, dict) else None
    if ts is None or now - ts > _V7_PRIMARY_MAX_AGE_S:
        return None
    return _v7_session_quote(symbol, row)


def _enrich_quotes(results, meta_rows=None):
    """Add the QuoteData extension fields to batched quote rows in place: one v7
    request per chunk (skipped when the caller already fetched `meta_rows`),
    static facts from _quote_static_cache when it fails, and price_digits on
    every row."""
    if not results:
        return
    if meta_rows is None:
        try:
            meta_rows = _v7_quote_rows([r.get("symbol") for r in results])
        except Exception:
            meta_rows = {}
    for r in results:
        sym = str(r.get("symbol") or "")
        key = sym.upper()
        meta = _quote_meta(meta_rows.get(key), sym)
        if meta:
            if len(_quote_static_cache) >= _QUOTE_STATIC_CACHE_MAX:
                _quote_static_cache.clear()
            _quote_static_cache[key] = {k: meta[k] for k in _QUOTE_STATIC_KEYS if k in meta}
        else:
            meta = dict(_quote_static_cache.get(key) or {})
        hint = meta.pop("price_hint", None)
        for k, v in meta.items():
            if r.get(k) in (None, ""):  # the row's own values (price-path facts) win
                r[k] = v
        r["price_digits"] = _display_digits(r.get("price"), r.get("quote_type"), hint, sym)

def get_historical(symbol, start_date, end_date, interval='1d'):
    """Fetch historical data for a symbol with specified interval

    Args:
        symbol: Stock symbol (e.g., 'AAPL')
        start_date: Start date (YYYY-MM-DD)
        end_date: End date (YYYY-MM-DD)
        interval: Data interval - valid values: 1m, 2m, 5m, 15m, 30m, 60m, 90m, 1h, 1d, 5d, 1wk, 1mo, 3mo
                  Note: Intraday data (< 1d) is only available for last 60 days

    Returns the bar list, or {"error", "symbol", "code"} when Yahoo has no rows
    (see "Failure reporting" above) — never an empty list.
    """
    logs = []
    no_data_msg = f"No price data for {symbol} ({start_date} to {end_date}, interval={interval})"
    try:
        # From the shared on-disk bar cache (end exclusive, as Ticker.history).
        try:
            from datetime import timedelta as _td
            end_incl = (datetime.strptime(str(end_date)[:10], "%Y-%m-%d") - _td(days=1)).date() \
                if end_date else None
            with _yf_quiet(logs):
                hist, _prov, _meta = _cached_history(symbol, interval=interval, start=start_date, end=end_incl)
        except Exception as e:
            failed = _store_failure(e, symbol, no_data_msg)
            if failed is not None:
                return failed
            raise

        if hist is None or hist.empty:
            return _error_result(None, symbol, logs, default="no_data", no_data_msg=no_data_msg)

        historical_data = []
        for index, row in hist.iterrows():
            # Guard every conversion the way get_batch_quotes already does — a
            # single NaN bar must not take the whole series with it.
            close = _num(row['Close'])
            if close is None:
                continue  # a bar with no close is not a price point
            historical_data.append({
                "symbol": symbol,
                "timestamp": int(index.timestamp()),
                "open": _num(row['Open']),
                "high": _num(row['High']),
                "low": _num(row['Low']),
                "close": close,
                "volume": _int(row['Volume'], 0),
                "adj_close": close
            })

        if not historical_data:
            return _failure(symbol, "no_data", no_data_msg)
        return historical_data
    except Exception as e:
        return _error_result(e, symbol, logs, no_data_msg=no_data_msg)

def get_historical_price(symbol, target_date):
    """Fetch closing price for a specific date

    Args:
        symbol: Stock symbol (e.g., 'AAPL')
        target_date: Target date (YYYY-MM-DD)

    Returns:
        dict with price info or error ({"found": False, "error", "symbol", "code"})
    """
    logs = []
    no_data_msg = f"No price data for {symbol} around {target_date}"
    try:
        from datetime import timedelta

        # Parse target date
        target = datetime.strptime(target_date, '%Y-%m-%d')

        # Fetch a range around the target date (to handle weekends/holidays)
        start = target - timedelta(days=5)
        end = target + timedelta(days=1)

        try:
            with _yf_quiet(logs):
                hist, _prov, _meta = _cached_history(symbol, interval='1d', start=start.date(), end=target.date())
        except Exception as e:
            failed = _store_failure(e, symbol, no_data_msg)
            if failed is None:
                raise
            return {"found": False, "error": failed["error"], "symbol": symbol, "code": failed["code"]}

        if hist is None or hist.empty:
            code, msg = _classify_failure(None, symbol, logs, default="no_data", no_data_msg=no_data_msg)
            return {"found": False, "error": msg, "symbol": symbol, "code": code}

        # Find the closest date <= target_date
        closest_date = None
        closest_price = None

        for index, row in hist.iterrows():
            idx_date = index.to_pydatetime().replace(tzinfo=None)
            if idx_date.date() <= target.date():
                closest_date = idx_date
                closest_price = round(float(row['Close']), 2)

        if closest_price is None:
            # If no date before or on target, take the first available
            first_row = hist.iloc[0]
            closest_date = hist.index[0].to_pydatetime()
            closest_price = round(float(first_row['Close']), 2)

        return {
            "found": True,
            "symbol": symbol,
            "price": closest_price,
            "date": closest_date.strftime('%Y-%m-%d'),
            "requested_date": target_date
        }
    except Exception as e:
        code, msg = _classify_failure(e, symbol, logs, no_data_msg=no_data_msg)
        return {"found": False, "error": msg, "symbol": symbol, "code": code}

def _diag(message):
    """One diagnostic line on stderr (stdout is the JSON result channel; never data)."""
    try:
        sys.stderr.write(f"[yfinance_data] {message}\n")
        sys.stderr.flush()
    except Exception:
        pass


def _fx_rate(from_ccy, to_ccy):
    """Spot rate to convert from_ccy amounts into to_ccy (e.g. EUR -> USD)."""
    if not from_ccy or not to_ccy or from_ccy == to_ccy:
        return 1.0
    try:
        # The batched, cached v7 quote of the pair (fast_info.lastPrice downloaded a whole
        # year of daily bars for one number).
        rate = fy.fx_rate(from_ccy, to_ccy)
        if rate and rate > 0:
            return float(rate)
    except Exception as exc:
        # Cross-currency ratios are then left out of an otherwise successful payload.
        _diag(f"WARNING FX rate lookup failed pair={from_ccy}{to_ccy} error={type(exc).__name__}")
    return None


def _statement_equity(ticker, symbol):
    """Shareholders' equity from the latest balance sheet (statement currency), cached for a
    day per symbol with the rest of the fundamentals; None when unknown."""
    def fetch():
        bs = ticker.balance_sheet
        for row in ("Stockholders Equity", "Common Stock Equity", "Total Equity Gross Minority Interest"):
            if bs is not None and row in bs.index:
                val = bs.loc[row].dropna()
                if len(val):
                    return float(val.iloc[0])
        return None
    try:
        return fy.cached("equity", symbol, fetch, max_age=fy.INFO_TTL_S)
    except Exception:
        return None


def _cross_currency_ratios(ticker, info, fx_rates=None):
    """Recompute price-vs-statement ratios when the stock trades in one currency and
    reports in another (ASML: USD price, EUR statements; TSM: USD ADR, TWD statements).

    Yahoo divides the trading-currency price by statement-currency figures, which gives
    a P/B of 1,430 for ASML and a P/S of 0.44 for TSM. Here the statement figures are
    converted to the trading currency first, and book value comes from the balance
    sheet. Returns {} when the currencies match or the FX rate is unavailable, so the
    caller keeps Yahoo's values.

    `fx_rates` (optional dict) memoises rates across one batch (get_batch_info).
    """
    trade_ccy = info.get("currency")
    fin_ccy = info.get("financialCurrency")
    if not trade_ccy or not fin_ccy or trade_ccy == fin_ccy:
        return {}
    if fx_rates is not None:
        pair = (fin_ccy, trade_ccy)
        if pair not in fx_rates:
            fx_rates[pair] = _fx_rate(fin_ccy, trade_ccy)
        fx = fx_rates[pair]
    else:
        fx = _fx_rate(fin_ccy, trade_ccy)
    if not fx:
        return {}
    out = {"fx_rate_statement_to_price": fx}
    mcap = info.get("marketCap")
    shares = info.get("sharesOutstanding")
    revenue = info.get("totalRevenue")
    ebitda = info.get("ebitda")
    cash = info.get("totalCash") or 0
    debt = info.get("totalDebt") or 0
    equity = _statement_equity(ticker, str(info.get("symbol") or getattr(ticker, "ticker", "") or ""))
    if equity and equity > 0:
        # Company-level ratio (market cap / equity) so ADRs that bundle several
        # ordinary shares (TSM: 1 ADR = 5 shares) are not off by the ADR ratio.
        if mcap:
            out["priceToBook"] = mcap / (equity * fx)
        if shares:
            out["bookValue"] = equity * fx / shares
    if mcap:
        ev = mcap + (debt - cash) * fx
        out["enterpriseValue"] = ev
        if revenue:
            out["priceToSalesTrailing12Months"] = mcap / (revenue * fx)
            out["enterpriseToRevenue"] = ev / (revenue * fx)
        if ebitda:
            out["enterpriseToEbitda"] = ev / (ebitda * fx)
    return out


_INFO_NAME_KEYS = ("longName", "shortName", "displayName")
_INFO_PRICE_KEYS = ("currentPrice", "regularMarketPrice", "previousClose", "regularMarketPreviousClose",
                    "regularMarketOpen", "open", "dayHigh", "dayLow", "bid", "ask", "navPrice",
                    "fiftyTwoWeekHigh", "fiftyTwoWeekLow", "marketCap")


def _info_has_data(info):
    """True when a Ticker.info dict describes an instrument Yahoo actually has.

    For an unknown symbol yfinance does not raise: quoteSummary answers 404
    (logged, swallowed) and .info is {'trailingPegRatio': None}. A dead listing
    such as UNI-USD gives {'quoteType': 'NONE', 'symbol': ...} and nothing else.
    Real but sparse instruments — indices (^GSPC), FX (EURUSD=X), futures (GC=F,
    no longName), crypto (BTC-USD) — always carry a name or a price, so "no
    name AND no price" is the not-found test.
    """
    if not info:
        return False
    if any(str(info.get(k) or "").strip() for k in _INFO_NAME_KEYS):
        return True
    return any(_positive(info.get(k)) for k in _INFO_PRICE_KEYS)


def get_info(symbol):
    """Fetch company info for a symbol"""
    logs = []
    try:
        ticker = yf.Ticker(symbol)
        with _yf_quiet(logs):
            info = dict(fy.info(symbol))  # one fetch per symbol per day, shared by every caller
        if not _info_has_data(info):
            return _error_result(None, symbol, logs, default="not_found")
        info.update(_cross_currency_ratios(ticker, info))

        # Extract comprehensive information - many more fields available from yfinance
        info_data = {
            "symbol": symbol,
            "company_name": info.get('longName', info.get('shortName', 'N/A')),
            "sector": info.get('sector', 'N/A'),
            "industry": info.get('industry', 'N/A'),
            "market_cap": info.get('marketCap'),
            "pe_ratio": info.get('trailingPE'),
            "forward_pe": info.get('forwardPE'),
            "dividend_yield": info.get('dividendYield'),
            "beta": info.get('beta'),
            "fifty_two_week_high": info.get('fiftyTwoWeekHigh'),
            "fifty_two_week_low": info.get('fiftyTwoWeekLow'),
            "average_volume": info.get('averageVolume'),
            "description": info.get('longBusinessSummary', 'N/A'),
            "website": info.get('website', 'N/A'),
            "country": info.get('country', 'N/A'),
            "currency": info.get('currency', 'USD'),
            "financial_currency": info.get('financialCurrency'),
            "exchange": info.get('exchange', 'N/A'),
            "employees": info.get('fullTimeEmployees'),
            # Additional comprehensive metrics
            "current_price": info.get('currentPrice'),
            "target_high_price": info.get('targetHighPrice'),
            "target_low_price": info.get('targetLowPrice'),
            "target_mean_price": info.get('targetMeanPrice'),
            "recommendation_mean": info.get('recommendationMean'),
            "recommendation_key": info.get('recommendationKey'),
            "number_of_analyst_opinions": info.get('numberOfAnalystOpinions'),
            "total_cash": info.get('totalCash'),
            "total_debt": info.get('totalDebt'),
            "total_revenue": info.get('totalRevenue'),
            "revenue_per_share": info.get('revenuePerShare'),
            "return_on_assets": info.get('returnOnAssets'),
            "return_on_equity": info.get('returnOnEquity'),
            "gross_profits": info.get('grossProfits'),
            "free_cashflow": info.get('freeCashflow'),
            "operating_cashflow": info.get('operatingCashflow'),
            "earnings_growth": info.get('earningsGrowth'),
            "revenue_growth": info.get('revenueGrowth'),
            "gross_margins": info.get('grossMargins'),
            "operating_margins": info.get('operatingMargins'),
            "ebitda_margins": info.get('ebitdaMargins'),
            "profit_margins": info.get('profitMargins'),
            "book_value": info.get('bookValue'),
            "price_to_book": info.get('priceToBook'),
            "enterprise_value": info.get('enterpriseValue'),
            "enterprise_to_revenue": info.get('enterpriseToRevenue'),
            "enterprise_to_ebitda": info.get('enterpriseToEbitda'),
            "shares_outstanding": info.get('sharesOutstanding'),
            "float_shares": info.get('floatShares'),
            "held_percent_insiders": info.get('heldPercentInsiders'),
            "held_percent_institutions": info.get('heldPercentInstitutions'),
            "short_ratio": info.get('shortRatio'),
            "short_percent_of_float": info.get('shortPercentOfFloat'),
            "peg_ratio": info.get('trailingPegRatio'),
            "total_assets": info.get('totalAssets'),
            "total_liabilities": info.get('totalLiab', info.get('totalLiabilities')),
            "book_value_total": (info.get('bookValue') or 0) * (info.get('sharesOutstanding') or 0) if info.get('bookValue') and info.get('sharesOutstanding') else None,
            "timestamp": int(datetime.now().timestamp())
        }

        return info_data
    except Exception as e:
        return _error_result(e, symbol, logs)

def _no_statements_error(ticker, symbol, logs):
    """Why get_financials found nothing.

    yfinance swallows every statement-fetch failure (refused connection, 404,
    429 alike) and hands back empty frames, so the reason has to be asked
    separately: one chart request tells "unknown symbol", "Yahoo unreachable"
    and "instrument files no statements" (index, FX, crypto, most ETFs) apart.
    Only paid on this failure path.
    """
    base = f"No financial statements available for {symbol}"
    try:
        with _yf_quiet(logs):
            hist = _yf_history(ticker, period="5d", interval="1d", timeout=_NET_TIMEOUT)
        if hist is not None and not hist.empty:
            return _failure(symbol, "no_data", base)
        code, msg = _classify_failure(None, symbol, logs, default="no_data")
    except Exception as e:
        code, msg = _classify_failure(e, symbol, logs)
    if code == "no_data":
        return _failure(symbol, code, base)
    return _failure(symbol, code, f"{base}: {msg}")


class _FinancialsUncached(Exception):
    def __init__(self, payload):
        super().__init__(str(payload.get("error") or ""))
        self.payload = payload


def get_financials(symbol):
    """Financial statements for a symbol, fetched at most once a day per symbol (statements
    change quarterly) and shared by every caller through fincept_yahoo's disk cache. A payload
    whose optional quarterly / TTM frames failed is cached as partial: after an hour only those
    frames are asked again. "No statements" (index, FX, most ETFs) is remembered for 6 hours;
    rate-limit / network failures are never cached (an older payload is served instead)."""
    def fetch():
        prev = fy.peek("financials", symbol)
        payload = _get_financials_uncached(symbol, prev if isinstance(prev, dict) else None)
        if "error" in payload:
            if payload.get("code") in ("no_data", "not_found"):
                return fy.Partial(payload, 6 * 3600)
            raise _FinancialsUncached(payload)
        if payload.get("_missing"):
            return fy.Partial(payload, 3600)
        return payload
    try:
        out = fy.cached("financials", symbol, fetch, max_age=fy.INFO_TTL_S)
    except _FinancialsUncached as e:
        return e.payload
    except Exception as e:
        code, msg = _classify_failure(e, symbol)
        return _failure(symbol, code, f"No financial statements available for {symbol}: {msg}")
    out = dict(out) if isinstance(out, dict) else {}
    out.pop("_missing", None)
    return out


# Optional statement frames -> where they land in the financials payload.
_OPTIONAL_FRAMES = {
    "quarterly_income_stmt": ("quarterly", "income_statement", "quarterly_income_statement"),
    "quarterly_balance_sheet": ("quarterly", "balance_sheet", "quarterly_balance_sheet"),
    "quarterly_cashflow": ("quarterly", "cash_flow", "quarterly_cash_flow"),
    "ttm_income_stmt": ("ttm", "income_statement", None),
    "ttm_cashflow": ("ttm", "cash_flow", None),
}


def _complete_partial_financials(symbol, prev):
    """`prev` (a payload whose optional frames failed) with only those frames fetched again."""
    import copy
    out = copy.deepcopy(prev)
    ticker = yf.Ticker(symbol)
    still = []
    for attr in prev.get("_missing") or []:
        dest = _OPTIONAL_FRAMES.get(attr)
        if dest is None:
            continue
        try:
            with _yf_quiet():
                df = getattr(ticker, attr)
        except Exception as exc:
            if _failure_code(str(exc), exc) == "rate_limited":
                raise
            still.append(attr)
            continue
        table = {}
        if df is not None and not df.empty:
            for col in df.columns:
                table[str(col)] = {str(i): (float(v) if isinstance(v, (int, float)) else str(v))
                                   for i, v in df[col].items() if pd.notna(v)}
        out.setdefault(dest[0], {})[dest[1]] = table
        if dest[2]:
            out.setdefault("row_order", {})[dest[2]] = [str(i) for i in df.index] if df is not None and not df.empty else []
    out["_missing"] = still
    if not still:
        out.pop("_missing", None)
    return out


def _get_financials_uncached(symbol, prev=None):
    """Fetch financial statements for a symbol.

    {"error", "symbol", "code"} when all three statements are empty — an empty
    payload used to be cached (and stored as last-known-good) by the host.
    """
    if prev and prev.get("_missing") and not prev.get("error"):
        try:
            return _complete_partial_financials(symbol, prev)
        except Exception as e:
            code, msg = _classify_failure(e, symbol)
            return _failure(symbol, code, f"No financial statements available for {symbol}: {msg}")
    logs = []
    missing = []
    try:
        ticker = yf.Ticker(symbol)

        # Get income statement, balance sheet, and cash flow
        with _yf_quiet(logs):
            income_stmt = ticker.financials
            balance_sheet = ticker.balance_sheet
            cash_flow = ticker.cashflow

        def dataframe_to_dict(df):
            """Convert pandas DataFrame to dict with cleaned values"""
            if df is None or df.empty:
                return {}
            result = {}
            for col in df.columns:
                result[str(col)] = {}
                for idx in df.index:
                    value = df.loc[idx, col]
                    if pd.notna(value):
                        result[str(col)][str(idx)] = float(value) if isinstance(value, (int, float)) else str(value)
            return result

        def row_order(df):
            """Line items in statement order (df.index) — the per-period dicts lose it on the
            host, whose JSON object sorts keys alphabetically."""
            if df is None or df.empty:
                return []
            return [str(idx) for idx in df.index]

        financials_data = {
            "symbol": symbol,
            "income_statement": dataframe_to_dict(income_stmt),
            "balance_sheet": dataframe_to_dict(balance_sheet),
            "cash_flow": dataframe_to_dict(cash_flow),
            "row_order": {
                "income_statement": row_order(income_stmt),
                "balance_sheet": row_order(balance_sheet),
                "cash_flow": row_order(cash_flow),
            },
            "timestamp": int(datetime.now().timestamp())
        }

        if not (financials_data["income_statement"] or financials_data["balance_sheet"]
                or financials_data["cash_flow"]):
            return _no_statements_error(ticker, symbol, logs)

        # Quarterly statements and Yahoo's trailing-twelve-month columns (Equity Research
        # ANNUAL / QUARTERLY / TTM toggle). Optional: a listing that files no quarters
        # (half-yearly reporters such as most of the LSE) keeps its annual statements.
        def optional_frame(attr):
            try:
                with _yf_quiet(logs):
                    return getattr(ticker, attr)
            except Exception as exc:
                _diag(f"WARNING Statement frame could not be fetched symbol={symbol} frame={attr} "
                      f"error={type(exc).__name__}")
                missing.append(attr)
                return None

        q_income = optional_frame("quarterly_income_stmt")
        q_balance = optional_frame("quarterly_balance_sheet")
        q_cash = optional_frame("quarterly_cashflow")
        ttm_income = optional_frame("ttm_income_stmt")
        ttm_cash = optional_frame("ttm_cashflow")
        financials_data["quarterly"] = {
            "income_statement": dataframe_to_dict(q_income),
            "balance_sheet": dataframe_to_dict(q_balance),
            "cash_flow": dataframe_to_dict(q_cash),
        }
        financials_data["ttm"] = {
            "income_statement": dataframe_to_dict(ttm_income),
            "cash_flow": dataframe_to_dict(ttm_cash),
        }
        financials_data["row_order"]["quarterly_income_statement"] = row_order(q_income)
        financials_data["row_order"]["quarterly_balance_sheet"] = row_order(q_balance)
        financials_data["row_order"]["quarterly_cash_flow"] = row_order(q_cash)
        if missing:
            financials_data["_missing"] = missing  # internal: get_financials retries only these
        return financials_data
    except Exception as e:
        code, msg = _classify_failure(e, symbol, logs)
        return _failure(symbol, code, f"No financial statements available for {symbol}: {msg}")

def _download_miss(symbol, dl_errors, no_data_msg):
    """{"code", "message"} for a symbol yf.download() returned no rows for,
    from the per-symbol reason yfinance recorded (shared._ERRORS) when there is one."""
    text = dl_errors.get(str(symbol).upper())
    if not text:
        return {"code": "no_data", "message": no_data_msg}
    code, message = _classify_failure(text, symbol, no_data_msg=no_data_msg)
    return {"code": code, "message": message}


def _bar_quotes(symbols, v7_rows, miss):
    """Quote rows priced from yf.download() daily bars, for the symbols the v7
    quote did not price. Returns (rows, complete): `complete` is True when the
    download raised and the rows come from get_quote() one by one, already
    carrying their extension fields. Every symbol left without a row is
    reported through miss(symbol, {"code", "message"}). `v7_rows` is the v7
    answer the caller already has; no further v7 request is made."""
    try:
        # Daily bars from the shared on-disk bar cache (fincept_yahoo.download): a symbol
        # priced here a minute ago, or one Yahoo does not know (remembered for a day), costs
        # no request; yf.download asked Yahoo once per symbol on every refresh.
        # 5 sessions guarantee at least 2 trading days for futures/commodities.
        with _yf_quiet():
            data = fy.download(symbols, period="5d", group_by='ticker', auto_adjust=True)
        dl_errors = _yf_download_errors()
    except Exception as e:
        code, message = _classify_failure(e)
        for symbol in symbols:
            miss(symbol, {"code": code, "message": message})
        return [], False

    def no_quote(symbol):
        return _download_miss(symbol, dl_errors, f"No recent price data for {symbol}")

    if data is None or data.empty:
        for symbol in symbols:
            miss(symbol, no_quote(symbol))
        return [], False

    results = []
    # Symbols whose newest daily bar has no prices yet (see _v7_session_quote):
    # symbol -> index of its bar-priced row in `results`, or None when the
    # download had no priced bar at all for it.
    unpriced = {}
    for symbol in symbols:
        try:
            if not isinstance(data.columns, pd.MultiIndex):
                # Flat columns (rare): use directly
                hist = data
            else:
                # MultiIndex columns — detect (Ticker, Price) vs (Price, Ticker)
                level0 = data.columns.get_level_values(0).unique().tolist()
                level1 = data.columns.get_level_values(1).unique().tolist()
                if symbol in level0:
                    # (Ticker, Price) ordering
                    hist = data[symbol]
                elif symbol in level1:
                    # (Price, Ticker) ordering
                    hist = data.xs(symbol, axis=1, level=1)
                else:
                    miss(symbol, no_quote(symbol))
                    continue

            if hist.empty or hist.dropna(how='all').empty:
                miss(symbol, no_quote(symbol))
                continue

            hist = hist.dropna(how='all')
            if pd.isna(hist['Close'].iloc[-1]):
                # Newest bar carries volume but no prices. Price the row from the
                # last complete bar for now; the quote endpoint replaces it below.
                priced = hist[hist['Close'].notna()]
                if priced.empty:
                    unpriced[symbol] = None
                    continue
                unpriced[symbol] = len(results)
                hist = priced
            raw_price = hist['Close'].iloc[-1]
            current_price = float(raw_price)
            # Use previous trading day close for accurate daily change.
            # With period="5d" we always have >= 2 rows for normally-traded instruments.
            raw_prev = hist['Close'].iloc[-2] if len(hist) >= 2 else raw_price
            previous_close = float(raw_prev) if not pd.isna(raw_prev) else current_price
            change = current_price - previous_close
            change_percent = (change / previous_close) * 100 if previous_close else 0

            d = _price_digits(current_price)
            row = {
                "symbol": symbol,
                "price": _px(current_price, d),
                "change": _px(change, d),
                "change_percent": round(change_percent, 2),
                "volume": int(hist['Volume'].iloc[-1]) if not pd.isna(hist['Volume'].iloc[-1]) else 0,
                "high": _px(hist['High'].iloc[-1], d) if not pd.isna(hist['High'].iloc[-1]) else None,
                "low": _px(hist['Low'].iloc[-1], d) if not pd.isna(hist['Low'].iloc[-1]) else None,
                "open": _px(hist['Open'].iloc[-1], d) if not pd.isna(hist['Open'].iloc[-1]) else None,
                "previous_close": _px(previous_close, d),
                # when the bars were fetched (bar cache), not market time
                "timestamp": int(fy.bar_fetched(symbol) or datetime.now().timestamp()),
                "exchange": "",
                # Trading session the price belongs to (the last daily bar's date, in
                # the exchange's calendar) — "as of" for a closed market's last price.
                "market_date": _market_date(hist.index[-1]),
            }
            # A v7 quote of a later session than the newest priced bar wins: the
            # bar row would show the previous session's price and change.
            v7 = _v7_session_quote(symbol, v7_rows.get(str(symbol).upper()))
            if v7 is not None and v7["market_date"] > (row["market_date"] or ""):
                row = v7
            results.append(row)
        except Exception as e:
            code, message = _classify_failure(e, symbol)
            miss(symbol, {"code": code, "message": message})
            continue

    # Price the symbols the daily bars could not from the v7 rows already fetched.
    for symbol, at in unpriced.items():
        row = _v7_session_quote(symbol, v7_rows.get(str(symbol).upper()))
        if row is not None:
            if at is None:
                results.append(row)
            else:
                results[at] = row
        elif at is None:
            miss(symbol, {"code": "no_data", "message": f"No recent closing price for {symbol}"})
        # else: keep the row priced from the last complete bar (its
        # market_date says which session that is).
    return results, False


def _batch_quotes_detailed(symbols):
    """get_batch_quotes(), plus why every symbol without a quote has none.

    Prices come from Yahoo's v7 quote endpoint (one request per _V7_QUOTE_CHUNK
    symbols, see _v7_session_quote); only the symbols it gives no current price
    for go through the daily-bar download (_bar_quotes). The same v7 answer
    then supplies the extension fields of every row.

    Returns (quotes, errors, total):
      quotes  the list get_batch_quotes() has always returned, in request order.
              Each row has symbol (as requested), price, change, change_percent,
              volume, high, low, open, previous_close, timestamp (fetch time),
              exchange, market_date (the price's session, exchange calendar),
              price_digits, and — when Yahoo's v7 quote answered (or, for the
              static ones, an earlier answer is remembered) — name, currency,
              exchange_name, exchange_timezone, exchange_delay_min, quote_type,
              market_state (PRE/REGULAR/POST/CLOSED), market_time,
              pre_/post_market_price, pre_/post_market_change_percent,
              pre_/post_market_time, average_volume (3-month), average_volume_10d,
              fifty_two_week_high, fifty_two_week_low, fifty_day_average,
              two_hundred_day_average.
              A key that Yahoo did not supply is absent or null, never 0
              (volume is 0 when the source reports none).
      errors  {requested_symbol: {"code", "message"}} for each symbol with no quote
      total   None, or {"code", "message"} when a non-empty request produced no
              quote at all (whole fetch failed / returned nothing)
    """
    errors = {}

    def miss(symbol, entry):
        errors.setdefault(symbol, entry)

    try:
        v7_rows = _v7_quote_rows(symbols)
    except Exception:
        v7_rows = {}
    for symbol in symbols:  # the other spelling of a USD-base FX pair (_USD_FX_PAIR_RE)
        key = str(symbol).upper()
        m = _USD_FX_PAIR_RE.match(key)
        if m and key not in v7_rows:
            alias = (m.group(2) if m.group(1) else "USD" + m.group(2)) + "=X"
            if alias in v7_rows:
                v7_rows[key] = v7_rows[alias]

    now = time.time()
    rows = {}
    for symbol in symbols:
        if symbol not in rows:
            row = _v7_primary_quote(symbol, v7_rows.get(str(symbol).upper()), now)
            if row is not None:
                rows[symbol] = row

    # Daily bars for the rest: no v7 row or price, a stale one, or v7 failed.
    complete = ()
    rest = [s for s in dict.fromkeys(symbols) if s not in rows]
    if rest:
        bar_rows, done = _bar_quotes(rest, v7_rows, miss)
        complete = {id(r) for r in bar_rows} if done else ()
        for row in bar_rows:
            rows.setdefault(row.get("symbol"), row)

    results = [rows[s] for s in symbols if s in rows]
    # Currency, exchange, name, session state, pre/post prices, averages,
    # 52-week range and display digits, from the same v7 rows.
    _enrich_quotes([r for r in results if id(r) not in complete], v7_rows)

    extra = None
    for e in errors.values():
        if isinstance(e, dict) and e.get("code") == "rate_limited":
            extra = _rate_limit_extra() if extra is None else extra
            e.update(extra)
    total = _summarize_failures(errors) if (symbols and not results) else None
    return results, errors, total


def get_batch_quotes(symbols):
    """Fetch quotes for multiple symbols at once (see _batch_quotes_detailed).

    Always a list — standalone callers (MarketDataService::flush_batch) depend on
    that; a symbol with no quote is simply absent. get_batch_all reports why.
    """
    return _batch_quotes_detailed(symbols)[0]

# Contract-month suffixes on futures names, in the two shapes Yahoo uses:
#   "Gold Jun 26" / "Crude Oil Jul 26"     (space-separated)
#   "Lumber Futures,Jan-2027"               (comma + Mon-YYYY, e.g. LBR=F)
# We strip the suffix so the panel shows "Gold" / "Lumber Futures".
_FUTURES_MONTH_RE = re.compile(
    r'(?:\s+(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\s+\d{2,4}'
    r'|\s*,\s*(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)-\d{2,4})$')

def _clean_display_name(symbol, info):
    """Pick the most human-readable name yfinance offers for `symbol`.

    Preference: longName (fuller, e.g. "Invesco QQQ Trust") over shortName
    (often truncated to ~30 chars). Falls back to shortName, then "".
    Futures (`=F`) have no longName and a month-stamped shortName
    ("Gold Jun 26") — strip the contract month so panels show "Gold".
    """
    name = (info.get('longName') or info.get('shortName') or '').strip()
    if not name:
        return ''
    if symbol.endswith('=F'):
        name = _FUTURES_MONTH_RE.sub('', name).strip()
    return name

def get_quote_names(symbols):
    """Resolve display names + quote currency for symbols.

    Returns {"names": {symbol: name}, "currencies": {symbol: code}}. Both are
    static per symbol, so the C++ caller caches the result and only asks for
    symbols it hasn't resolved yet. Read from the batched v7 quote rows (one
    shared, cached request per 50 symbols; a symbol quoted anywhere recently costs
    nothing) - they carry longName / shortName, currency and quoteType, so the
    per-symbol Ticker.info (three requests each) is not needed. Symbols Yahoo
    can't name are simply omitted (caller keeps the raw ticker / no prefix).

    Currency is omitted for forex pairs (quoteType CURRENCY — the price is a
    rate, not a monetary amount) and for indices/yields (quoteType INDEX —
    e.g. a currency symbol on the S&P level or the 10Y yield is misleading),
    so callers only ever prefix a currency where it's meaningful.

    When nothing could be resolved because the lookups themselves failed
    (network down, rate limited) the result also carries "error" and "code":
    an empty success makes the host mark every symbol "unresolvable" for the
    session, while a failure lets it ask again on a later refresh.
    """
    names = {}
    currencies = {}
    unique = list(dict.fromkeys(s for s in symbols if s))
    if not unique:
        return {"names": names, "currencies": currencies}
    try:
        with _yf_quiet():
            rows = fy.quotes(unique)
    except Exception as e:
        code, message = _classify_failure(e)
        if code != "not_found":
            return {"error": message, "code": code, "names": {}, "currencies": {}, **_rate_limit_extra()}
        rows = {}
    for symbol in unique:
        row = rows.get(symbol)
        if not row:
            continue
        name = _clean_display_name(symbol, row)
        if name:
            names[symbol] = name
        quote_type = (row.get("quoteType") or "").upper()
        currency = row.get("currency")
        if currency and quote_type not in ("CURRENCY", "INDEX"):
            currencies[symbol] = currency
    if not names and not currencies:
        err = fy.last_quote_error()
        if err is not None:
            code, message = _classify_failure(err)
            if code != "not_found":
                return {"error": message, "code": code, "names": {}, "currencies": {}, **_rate_limit_extra()}
    return {"names": names, "currencies": currencies}

# Sparklines change slowly (5 days of hourly closes): one answer serves every caller for
# this long (the hub's own TTL is 10 minutes).
_SPARK_MAX_AGE_S = 300


def get_batch_sparklines(symbols, period="5d", interval="1h"):
    """Fetch close price series for multiple symbols — used for blotter sparklines.
    Returns: {"AAPL": [170.1, 171.3, ...], "MSFT": [...], ...}
    A request that yields no series at all returns {"error": "<reason>"}.

    Yahoo's spark endpoint answers 20 symbols per request (yf.download asked once per
    symbol), cached in memory for _SPARK_MAX_AGE_S. Symbols it has no series for, or every
    symbol when it fails for a reason other than a rate limit, come from the shared bar cache.
    """
    symbols = [s for s in dict.fromkeys(symbols or []) if s]
    result = {}
    failures = {}
    rest = symbols
    try:
        with _yf_quiet():
            result = fy.spark(symbols, period, interval, max_age=_SPARK_MAX_AGE_S)
        rest = [s for s in symbols if s not in result]
    except Exception as e:
        code, message = _classify_failure(e)
        if code == "rate_limited":
            return {"error": message, **_rate_limit_extra()}
        _diag(f"WARNING spark request failed, using the bar cache error={type(e).__name__}")
    for symbol in rest:
        try:
            with _yf_quiet():
                hist, _prov, _meta = _cached_history(symbol, period=period, interval=interval)
            closes = hist["Close"].dropna()
            if closes.empty:
                failures[symbol] = {"code": "no_data", "message": f"No sparkline data for {symbol}"}
                continue
            result[symbol] = [round(float(v), 4) for v in closes.tolist()]
        except Exception as exc:
            failed = _store_failure(exc, symbol, f"No sparkline data for {symbol}")
            if failed is None:
                _diag(f"WARNING Sparkline skipped symbol={symbol} error={type(exc).__name__}")
                failed = {"code": "unknown", "error": f"No sparkline data for {symbol}"}
            failures[symbol] = {"code": failed.get("code"), "message": failed.get("error")}
            if failed.get("code") == "rate_limited":
                break
    if symbols and not result:
        summary = _summarize_failures(failures) if failures else {"message": "No data"}
        out = {"error": summary.get("message") or "No data"}
        if summary.get("retry_after_s"):
            out["retry_after_s"] = summary["retry_after_s"]
        return out
    return result


def get_batch_all(payload):
    """Unified batch fetcher — returns quotes + sparklines + histories in a single call.

    Called by MarketDataService::refresh() so one DataHub refresh tick spawns a single
    Python process rather than three (quote + sparkline + history). Shares the same
    cached paths as the individual endpoints (quote cache, spark cache, bar cache) —
    no behaviour change per family, just fewer process spawns and requests.

    payload = {
      "quotes":     [sym, ...],                         # optional
      "sparklines": [sym, ...],                         # optional, 5d/1h
      "histories":  [{"symbol","period","interval"}, ...],  # optional
    }

    Returns:
      {
        "quotes":     [<quote dicts from get_batch_quotes>, ...],   # keys: _batch_quotes_detailed
        "quote_errors": {sym: {"code", "message"}, ...},   # every requested quote
                                                          # symbol with no quote
        "quotes_error": "<reason>",       # only when the whole quote fetch failed /
        "quotes_error_code": "<code>",    # returned nothing for a non-empty request
        "sparklines": {sym: [close, ...], ...}   or {"error": "<reason>"},
        "histories":  [{"symbol","period","interval","points":[...]
                        [, "error": "<reason>", "code": "<code>"]}],
      }
    A history entry carries "error"/"code" whenever its points are empty or its
    fetch failed ("points" is then []). Codes: see "Failure reporting" above.
    Any family absent from `payload` is omitted from the result.
    """
    out = {}

    quote_syms = payload.get("quotes") or []
    if quote_syms:
        quotes, quote_errors, quotes_total = _batch_quotes_detailed(quote_syms)
        out["quotes"] = quotes
        out["quote_errors"] = quote_errors
        if quotes_total:
            out["quotes_error"] = quotes_total["message"]
            out["quotes_error_code"] = quotes_total["code"]
            if quotes_total.get("retry_after_s"):
                out["quotes_retry_after_s"] = quotes_total["retry_after_s"]

    spark_syms = payload.get("sparklines") or []
    if spark_syms:
        out["sparklines"] = get_batch_sparklines(spark_syms)

    hist_reqs = payload.get("histories") or []
    if hist_reqs:
        histories = []
        for req in hist_reqs:
            req = req if isinstance(req, dict) else {}
            try:
                sym = req.get("symbol")
                period = req.get("period", "6mo")
                interval = req.get("interval", "1d")
                if not sym:
                    continue
                points = get_historical_period(sym, period, interval)
                entry = {
                    "symbol": sym,
                    "period": period,
                    "interval": interval,
                    "points": points if isinstance(points, list) else [],
                }
                if not entry["points"]:
                    # get_historical_period's {"error","symbol","code"} used to be
                    # dropped here, leaving an empty series that looked like success.
                    failed = points if isinstance(points, dict) else {}
                    entry["error"] = failed.get("error") or \
                        f"No price data for {sym} (period={period}, interval={interval})"
                    entry["code"] = failed.get("code") or "no_data"
                histories.append(entry)
            except Exception as e:
                code, message = _classify_failure(e, req.get("symbol"))
                histories.append({
                    "symbol": req.get("symbol", ""),
                    "period": req.get("period", ""),
                    "interval": req.get("interval", ""),
                    "points": [],
                    "error": message,
                    "code": code,
                })
        out["histories"] = histories

    return out


def get_company_profile(symbol):
    """Get company profile data formatted for peer comparison"""
    logs = []
    try:
        ticker = yf.Ticker(symbol)
        with _yf_quiet(logs):
            info = fy.info(symbol)  # shared daily cache

        if not info or 'symbol' not in info or not _info_has_data(info):
            return _error_result(None, symbol, logs, default="not_found")

        # Format data to match FMP structure
        profile = {
            "symbol": info.get("symbol", symbol),
            "companyName": info.get("longName", info.get("shortName", "")),
            "sector": info.get("sector", ""),
            "industry": info.get("industry", ""),
            "website": info.get("website", ""),
            "description": info.get("longBusinessSummary", ""),
            "exchange": info.get("exchange", ""),
            "country": info.get("country", ""),
            "city": info.get("city", ""),
            "address": info.get("address1", ""),
            "phone": info.get("phone", ""),
            "marketCap": info.get("marketCap", 0),
            "employees": info.get("fullTimeEmployees", 0),
            "currency": info.get("currency", "USD"),
            "beta": info.get("beta", 0),
            "price": info.get("currentPrice", info.get("regularMarketPrice", 0)),
            "changes": info.get("regularMarketChangePercent", 0),
        }

        return profile
    except Exception as e:
        return _error_result(e, symbol, logs)

def get_financial_ratios(symbol):
    """Get financial ratios formatted for peer comparison"""
    logs = []
    try:
        ticker = yf.Ticker(symbol)
        with _yf_quiet(logs):
            info = fy.info(symbol)  # shared daily cache

        if not info or 'symbol' not in info or not _info_has_data(info):
            return _error_result(None, symbol, logs, default="not_found")
        info = dict(info)
        info.update(_cross_currency_ratios(ticker, info))

        # Calculate free cash flow per share
        free_cashflow = info.get("freeCashflow", 0)
        shares_outstanding = info.get("sharesOutstanding", 1)
        fcf_per_share = free_cashflow / shares_outstanding if shares_outstanding else 0

        ratios = {
            "symbol": symbol,
            "peRatio": info.get("trailingPE", 0),
            "forwardPE": info.get("forwardPE", 0),
            "priceToBook": info.get("priceToBook", 0),
            "priceToSales": info.get("priceToSalesTrailing12Months", 0),
            "pegRatio": info.get("trailingPegRatio", 0),
            "debtToEquity": info.get("debtToEquity", 0),
            "returnOnEquity": info.get("returnOnEquity", 0),
            "returnOnAssets": info.get("returnOnAssets", 0),
            "profitMargin": info.get("profitMargins", 0),
            "operatingMargin": info.get("operatingMargins", 0),
            "grossMargin": info.get("grossMargins", 0),
            "currentRatio": info.get("currentRatio", 0),
            "quickRatio": info.get("quickRatio", 0),
            "dividendYield": info.get("dividendYield", 0),
            "revenuePerShare": info.get("revenuePerShare", 0),
            "bookValuePerShare": info.get("bookValue", 0),
            "freeCashFlowPerShare": fcf_per_share,
            # Growth is what justifies a premium multiple — the peer table compares it
            # side by side with P/E and P/S (fractions, latest quarter YoY).
            "revenueGrowth": info.get("revenueGrowth", 0),
            "earningsGrowth": info.get("earningsGrowth", 0),
            "enterpriseToEbitda": info.get("enterpriseToEbitda", 0),
        }

        return ratios
    except Exception as e:
        return _error_result(e, symbol, logs)

# ── Batched fundamentals (InfoData) ──────────────────────────────────────────
#
# One process answers for many symbols: Ticker.info per symbol in a bounded
# thread pool, each symbol failing on its own. Feeds MarketDataService's
# `market:info:<SYM>` topic (hours-long TTL), so list screens get sector, P/E,
# yield, beta, ... without one Python process per row. Run as a subprocess by
# the host, not on the serial daemon: a 40-symbol batch takes seconds and would
# stall every quote refresh queued behind it.

try:
    _INFO_MAX_WORKERS = max(1, min(16, int(os.environ.get("FINCEPT_INFO_WORKERS", "6"))))
except (TypeError, ValueError):
    _INFO_MAX_WORKERS = 6


def _info_date(epoch_value, tz_name=None):
    """'YYYY-MM-DD' of an epoch-seconds timestamp, or None.

    Pass the exchange's timezone for a real point in time (earnings: AAPL
    21:00Z is 16:00 New York; 7203.T 06:30Z is 15:30 Tokyo). Leave it out for
    Yahoo's date-only fields (exDividendDate), which are stamped at midnight
    UTC of the date itself — converting those to New York moves them a day back.
    """
    ts = _epoch(epoch_value)
    if ts is None:
        return None
    try:
        t = pd.Timestamp(ts, unit="s", tz="UTC")
        if tz_name:
            try:
                t = t.tz_convert(tz_name)
            except Exception:
                pass
        return t.strftime("%Y-%m-%d")
    except Exception:
        return None


def _next_earnings_epoch(info, now=None):
    """Epoch seconds of the next scheduled earnings report (start of Yahoo's
    window), counting one reported in the last 12 hours as "today"; None when
    nothing upcoming is known. earningsTimestamp is often the LAST report."""
    now = time.time() if now is None else now
    candidates = [_epoch(info.get(k)) for k in
                  ("earningsTimestampStart", "earningsTimestamp", "earningsCallTimestampStart")]
    upcoming = [c for c in candidates if c is not None and c >= now - 12 * 3600]
    return min(upcoming) if upcoming else None


def _info_row(symbol, info):
    """One batch_info row from a Ticker.info dict. Keys match InfoData; a value
    Yahoo did not report is omitted (never "N/A", never 0)."""
    tz = _text(info.get("exchangeTimezoneName"))

    def num(k):
        return _finite(info.get(k))

    def pos(k):
        return _pos(info.get(k))

    qt = _text(info.get("quoteType"))
    rec_key = _text(info.get("recommendationKey"))
    dy = num("dividendYield")
    row = {
        "symbol": symbol,
        "name": _clean_display_name(symbol, info) or None,
        "sector": _text(info.get("sector")),
        "industry": _text(info.get("industry")),
        "country": _text(info.get("country")),
        "currency": _text(info.get("currency")),                    # as reported ("GBp")
        "financial_currency": _text(info.get("financialCurrency")),  # statements' currency
        "quote_type": qt.upper() if qt else None,
        "exchange": _text(info.get("exchange")),
        "exchange_name": _text(info.get("fullExchangeName")),
        "market_cap": pos("marketCap"),
        "pe_ratio": num("trailingPE"),
        "forward_pe": num("forwardPE"),
        "price_to_book": num("priceToBook"),
        "price_to_sales": num("priceToSalesTrailing12Months"),
        "peg_ratio": _first(num("trailingPegRatio"), num("pegRatio")),
        "enterprise_value": num("enterpriseValue"),
        "ev_to_ebitda": num("enterpriseToEbitda"),
        # yfinance 0.2.66 reports dividendYield in percent (0.3 = 0.3 %, checked
        # against AAPL Nov 2026); the row carries a fraction like every other ratio.
        "dividend_yield": round(dy / 100.0, 8) if dy is not None else None,
        "dividend_rate": num("dividendRate"),
        "payout_ratio": num("payoutRatio"),
        "ex_dividend_date": _info_date(info.get("exDividendDate")),  # date-only field: UTC midnight
        "earnings_date": _info_date(_next_earnings_epoch(info), tz),  # a real time: exchange's day
        "beta": num("beta"),
        "fifty_two_week_high": pos("fiftyTwoWeekHigh"),
        "fifty_two_week_low": pos("fiftyTwoWeekLow"),
        "average_volume": _first(pos("averageVolume"), pos("averageDailyVolume3Month")),
        "average_volume_10d": _first(pos("averageVolume10days"), pos("averageDailyVolume10Day")),
        "shares_outstanding": pos("sharesOutstanding"),
        "float_shares": pos("floatShares"),
        "short_ratio": num("shortRatio"),
        "short_percent_float": num("shortPercentOfFloat"),
        "revenue_per_share": num("revenuePerShare"),
        "trailing_eps": _first(num("trailingEps"), num("epsTrailingTwelveMonths")),
        "forward_eps": _first(num("forwardEps"), num("epsForward")),
        "roe": num("returnOnEquity"),
        "profit_margin": num("profitMargins"),
        "gross_margin": num("grossMargins"),
        "operating_margin": num("operatingMargins"),
        "debt_to_equity": num("debtToEquity"),  # Yahoo's percent: 150 = 1.5x
        "current_ratio": num("currentRatio"),
        "revenue_growth": num("revenueGrowth"),
        "earnings_growth": num("earningsGrowth"),
        "target_mean_price": pos("targetMeanPrice"),
        "recommendation_mean": num("recommendationMean"),
        "recommendation_key": rec_key.lower() if rec_key else None,
    }
    return {k: v for k, v in row.items() if v is not None}


def _batch_info_one(symbol, state, logs):
    """(symbol, row or None, {"code","message"} or None) for one symbol. Runs on
    a pool thread inside _yf_quiet_pool(); `logs` is its sink."""
    if state["rate_limited"]:
        # Another symbol was just throttled; asking again only extends it.
        return symbol, None, {"code": "rate_limited", "message": _failure_message("rate_limited", "", symbol)}
    own_hints = _pooled_hints(logs)
    try:
        ticker = yf.Ticker(symbol)
        info = dict(fy.info(symbol))  # shared daily cache: a symbol any screen loaded today is free
    except Exception as e:
        code, message = _classify_failure(e, symbol, own_hints())
        if code == "rate_limited":
            state["rate_limited"] = True
        return symbol, None, {"code": code, "message": message}
    if not _info_has_data(info):
        # yfinance swallows quoteSummary failures (404 for an unknown ticker,
        # but also connect errors): the log lines tell them apart.
        code, message = _classify_failure(None, symbol, own_hints(), default="not_found")
        if code == "rate_limited":
            state["rate_limited"] = True
        return symbol, None, {"code": code, "message": message}
    try:
        # Same price-vs-statement currency correction as get_info / financial_ratios,
        # so P/B, P/S and EV ratios agree with fetch_info(). Only costs requests
        # for cross-currency listings; FX rates are shared across the batch.
        info.update(_cross_currency_ratios(ticker, info, state["fx"]))
    except Exception:
        pass
    return symbol, _info_row(symbol, info), None


def get_batch_info(symbols):
    """Fundamentals for many symbols in one process (InfoData / market:info:*).

    Returns {"info": {symbol: row}, "info_errors": {symbol: {"code", "message"}}}
    where each row is _info_row()'s keys. Symbols are keyed exactly as passed.
    Every requested symbol appears in exactly one of the two maps. When none
    succeeded the result also carries top-level "error" and "code" (a fetch that
    yields nothing is an error, never an empty success); the host still reads
    the per-symbol reasons from "info_errors".

    Concurrency is bounded by FINCEPT_INFO_WORKERS (default 6). After a rate
    limit the remaining symbols are not requested: they report rate_limited.
    """
    from concurrent.futures import ThreadPoolExecutor
    unique = list(dict.fromkeys(str(s) for s in symbols if s and str(s).strip()))
    out = {"info": {}, "info_errors": {}}
    if not unique:
        return out
    state = {"rate_limited": False, "fx": {}}
    try:
        # One batched quote request for the whole list: each Ticker.info below then reads
        # its v7 part from the quote cache instead of one request per symbol.
        with _yf_quiet():
            fy.quote_entries(unique)
    except Exception:
        pass
    with _yf_quiet_pool() as logs:
        with ThreadPoolExecutor(max_workers=min(_INFO_MAX_WORKERS, len(unique))) as pool:
            results = list(pool.map(lambda s: _batch_info_one(s, state, logs), unique))
    for symbol, row, err in results:
        if row:
            out["info"][symbol] = row
        else:
            out["info_errors"][symbol] = err or {"code": "unknown", "message": f"No data for {symbol}"}
    if not out["info"]:
        summary = _summarize_failures(out["info_errors"])
        out["error"] = summary["message"]
        out["code"] = summary["code"]
    return out


def _collect_per_symbol(symbols, fetch):
    """List of fetch(symbol) successes. When symbols were asked for and none
    succeeded, {"error", "code"} instead of an empty list (an empty peer table
    with no reason is the blank-screen symptom)."""
    results, failures = [], {}
    for symbol in symbols:
        item = fetch(symbol)
        if item and "error" not in item:
            results.append(item)
        else:
            item = item if isinstance(item, dict) else {}
            failures[symbol] = {"code": item.get("code") or "unknown",
                                "message": item.get("error") or f"No data for {symbol}"}
    if failures and not results:
        summary = _summarize_failures(failures)
        return {"error": summary["message"], "code": summary["code"]}
    return results

def get_multiple_profiles(symbols):
    """Get company profiles for multiple symbols"""
    return _collect_per_symbol(symbols, get_company_profile)

def get_multiple_ratios(symbols):
    """Get financial ratios for multiple symbols"""
    return _collect_per_symbol(symbols, get_financial_ratios)

def _yahoo_search_api(query_str, limit=20, timeout=5):
    """Hits from Yahoo Finance's search/autocomplete endpoint (fast, no auth).
    Raises on any transport, HTTP or decoding failure — an empty list means
    Yahoo answered and matched nothing."""
    # Through the shared session (limiter / breaker) and cached for a day per query.
    data = fy.search_raw(query_str, limit, 0, timeout=timeout) or {}
    results = []
    for q in data.get("quotes", []):
        symbol = q.get("symbol", "")
        if not symbol:
            continue
        results.append({
            "symbol": symbol,
            "name": q.get("longname") or q.get("shortname", ""),
            "exchange": q.get("exchange", ""),
            "type": q.get("quoteType", "EQUITY"),
            "currency": q.get("currency", "USD"),
            "sector": "",
            "industry": q.get("industry", "")
        })
    return results


def search_symbols(query, limit=20):
    """
    Search for stock symbols using yfinance's search API.
    Fast — uses Yahoo Finance's search endpoint instead of brute-force .info calls.

    When the search endpoint fails and the exact-ticker fallback finds nothing,
    the result carries "error" and "code" rather than posing as "0 matches".
    """
    try:
        results = []
        query_str = query.strip()
        if not query_str:
            return {"results": [], "query": query, "count": 0}

        try:
            results = _yahoo_search_api(query_str, limit)
        except Exception as search_err:
            # Fallback: try the exact symbol as a yfinance Ticker
            query_upper = query_str.upper()
            suffixes = ["", ".NS", ".BO"]
            for suffix in suffixes:
                candidate = query_upper + suffix
                try:
                    info = fy.info(candidate)
                    if info.get("longName") or info.get("shortName"):
                        results.append({
                            "symbol": candidate,
                            "name": info.get("longName", info.get("shortName", "")),
                            "exchange": info.get("exchange", ""),
                            "type": info.get("quoteType", "EQUITY"),
                            "currency": info.get("currency", "USD"),
                            "sector": info.get("sector", ""),
                            "industry": info.get("industry", "")
                        })
                        break
                except Exception:
                    continue
            if not results:
                code, message = _classify_failure(search_err)
                return {"results": [], "query": query, "count": 0, "error": message, "code": code}

        return {"results": results, "query": query, "count": len(results)}

    except Exception as e:
        code, message = _classify_failure(e)
        return {"error": message, "query": query, "results": [], "code": code}

# Ticker news is shared by every caller for this long (Equity Research's own cache is shorter).
_NEWS_TTL_S = 600


def get_news(symbol, count=20):
    """News articles for a symbol, cached _NEWS_TTL_S per (symbol, count); a failed fetch is
    never cached."""
    key = f"{symbol}|{int(count or 20)}"
    hit = fy.recall("news", key, _NEWS_TTL_S)
    if isinstance(hit, dict) and hit.get("articles") is not None:
        return hit
    out = _get_news_uncached(symbol, count)
    if isinstance(out, dict) and "error" not in out:
        try:
            fy.remember("news", key, out)
        except Exception:
            pass
    return out


def _get_news_uncached(symbol, count=20):
    """Fetch news articles for a symbol using yfinance"""
    try:
        import re
        # Ticker.news posts to an endpoint Yahoo retired (HTTP 404, Nov 2026): after one such
        # answer go straight to the search API for a day instead of paying for the 404 each time.
        if fy.recall("flag", "news_ncp_dead", 86400):
            raise RuntimeError("Ticker.news endpoint unavailable")
        ticker = yf.Ticker(symbol)
        try:
            raw_news = ticker.news
        except Exception as e:
            if "currently down" in str(e).lower() or "404" in str(e):
                try:
                    fy.remember("flag", "news_ncp_dead", True)
                except Exception:
                    pass
            raise
        if not raw_news:
            return {"articles": [], "symbol": symbol}

        articles = []
        for item in raw_news[:count]:
            content = item.get("content", {})
            title = content.get("title", "")
            if not title:
                continue
            # Clean HTML from summary
            summary = content.get("summary", "")
            summary = re.sub(r'<[^>]+>', '', summary)
            pub_date = content.get("pubDate", "")
            provider = content.get("provider", {})
            publisher = provider.get("displayName", "")
            # Get URL
            url = ""
            click_url = content.get("clickThroughUrl", {})
            if click_url:
                url = click_url.get("url", "")
            if not url:
                canonical = content.get("canonicalUrl", {})
                if canonical:
                    url = canonical.get("url", "")

            articles.append({
                "title": title,
                "description": summary,
                "url": url,
                "publisher": publisher,
                "published_date": pub_date
            })

        return {"articles": articles, "symbol": symbol}
    except Exception as e:
        # Ticker.news posts to finance.yahoo.com/xhr/ncp, which answers HTTP 404 (Nov 2026):
        # yfinance then raises "YAHOO! FINANCE IS CURRENTLY DOWN" for every symbol. Yahoo's
        # search API still serves ticker news, so fall back to it before reporting the error.
        try:
            return {"articles": _search_news(symbol, count), "symbol": symbol, "source": "yahoo_search"}
        except Exception as fallback_err:
            # The fallback's failure is the informative one: the primary endpoint
            # fails for every symbol with the misleading "Yahoo is down" text.
            code, message = _classify_failure(fallback_err, symbol)
            return {"error": message, "symbol": symbol, "articles": [], "code": code}


def _search_news(symbol, count):
    """News for `symbol` from Yahoo's search API, keeping only the articles Yahoo tags with that
    ticker (the search itself is fuzzy: "RELIANCE" also finds Reliance Steel stories)."""
    import io, contextlib
    from datetime import timezone
    wanted = str(symbol or "").strip().upper()
    if not wanted:
        raise ValueError("Empty ticker name")
    with contextlib.redirect_stdout(io.StringIO()):
        # Same search endpoint as yf.Search, through the shared session and its cache.
        items = (fy.search_raw(wanted, limit=1, news_count=max(int(count), 1) * 2) or {}).get("news") or []
    articles = []
    for item in items:
        related = [str(t).upper() for t in (item.get("relatedTickers") or [])]
        if wanted not in related or not item.get("title"):
            continue
        published = item.get("providerPublishTime")
        articles.append({
            "title": item.get("title", ""),
            "description": "",  # the search API carries no summary
            "url": item.get("link", ""),
            "publisher": item.get("publisher", ""),
            "published_date": datetime.fromtimestamp(published, timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            if isinstance(published, (int, float)) else ""
        })
        if len(articles) >= int(count):
            break
    return articles

def _candidate_yf_symbols(symbol):
    """Generate yfinance ticker candidates for a portfolio symbol.

    Heuristic — try the symbol as-given first, then common non-US exchange
    suffixes. We can't ask the portfolio for currency here, so we just iterate
    until something returns data. Order matters: most likely first.
    """
    s = symbol.strip().upper()
    if not s:
        return []
    # Already has an exchange suffix or is an index/crypto/forex pair → trust it.
    if '.' in s or s.startswith('^') or '-' in s or '=' in s:
        return [s]
    # Bare ticker — try US first, then Toronto, NSE, BSE, LSE, ASX, Hong Kong.
    return [s, f"{s}.TO", f"{s}.NS", f"{s}.BO", f"{s}.L", f"{s}.AX", f"{s}.HK"]


def _resolve_for_history(symbols, period):
    """Resolve every input symbol to a yfinance ticker that returns data
    in the given period, AND return the close-price series for each.

    Returns: dict[input_symbol] -> (resolved_symbol, closes_series).
    Symbols that fail every candidate are omitted.

    Candidates (the symbol as given, then exchange-suffix variants for a bare
    ticker) are checked against ONE batched quote request (cached), so only the
    listing that exists is downloaded - not seven variants per symbol. The closes
    come from the shared on-disk bar cache.
    """
    candidates_per_symbol = {s: _candidate_yf_symbols(s) for s in symbols}
    all_candidates = list(dict.fromkeys(c for variants in candidates_per_symbol.values() for c in variants))
    if not all_candidates:
        return {}
    try:
        with _yf_quiet():
            known = fy.quotes(all_candidates)
    except Exception:
        known = {}

    def closes_for(ticker):
        try:
            with _yf_quiet():
                hist, _prov, _meta = _cached_history(ticker, period=period, interval="1d")
            ser = hist["Close"].dropna()
            ser.index = ser.index.tz_localize(None).normalize()
            return ser
        except Exception as exc:
            if type(exc).__name__ != "PriceDataError":
                _diag(f"WARNING Close series unavailable ticker={ticker} error={type(exc).__name__}")
            return None

    resolved = {}
    for s, variants in candidates_per_symbol.items():
        listed = [v for v in variants if v in known]
        # A listing the quote endpoint knows first; without any answer (quote request
        # failed) the variants are tried in order through the bar cache.
        for v in (listed or (variants if not known else variants[:1])):
            ser = closes_for(v)
            if ser is not None and not ser.empty:
                resolved[s] = (v, ser)
                break
    return resolved


def get_portfolio_nav_history(positions, period='6mo'):
    """Reconstruct a daily NAV time series from current positions.

    positions: list of {"symbol": str, "quantity": float} dicts.
    Returns: {"dates": [...], "navs": [...], "resolved": {sym: yf_sym},
              "missing": [sym, ...]}.

    Each input symbol is resolved to a yfinance ticker via _resolve_for_history,
    which tries common exchange suffixes (.TO, .NS, .BO, .L, .AX, .HK) when the
    bare symbol returns no data. Missing closes are forward-filled per symbol so
    a symbol that listed mid-period doesn't drop the whole row.

    NAV on each date = sum(close_i * quantity_i) over all resolved symbols.

    NOTE: this back-projects NAV using CURRENT quantity at every date — it
    does not account for buys/sells inside the window. That's acceptable for
    risk metrics (Beta, MDD) which measure basket variability, not realised P&L.
    """
    try:
        if not positions:
            return {"dates": [], "navs": []}
        symbols = [p["symbol"] for p in positions if p.get("symbol")]
        qty_map = {p["symbol"]: float(p.get("quantity", 0)) for p in positions if p.get("symbol")}
        if not symbols:
            return {"dates": [], "navs": []}

        resolved = _resolve_for_history(symbols, period)
        if not resolved:
            return {"dates": [], "navs": [], "error": "no historical data for any symbol",
                    "missing": symbols}

        nav_series = None
        for sym, (yf_sym, closes) in resolved.items():
            contrib = closes.ffill() * qty_map[sym]
            if nav_series is None:
                nav_series = contrib
            else:
                nav_series = nav_series.add(contrib, fill_value=0)

        if nav_series is None or nav_series.empty:
            return {"dates": [], "navs": [], "error": "no usable closes",
                    "missing": [s for s in symbols if s not in resolved]}

        nav_series = nav_series.dropna()
        dates = [idx.strftime("%Y-%m-%d") for idx in nav_series.index]
        navs = [float(v) for v in nav_series.values]
        return {
            "dates": dates,
            "navs": navs,
            "resolved": {s: yf for s, (yf, _) in resolved.items()},
            "missing": [s for s in symbols if s not in resolved],
        }
    except Exception as e:
        return {"dates": [], "navs": [], "error": str(e)}


def get_portfolio_nav_history_replay(transactions, base_currency=None, track_cash=False,
                                     instrument_currencies=None):
    """Daily NAV reconstructed by replaying the ledger — delegates to
    portfolio_metrics.nav_replay, the one implementation the Portfolio service uses
    (PortfolioService::backfill_history runs `portfolio_metrics.py nav_replay`).

    It replays every row type (BUY / SELL / SPLIT / DIVIDEND / cash rows with fees,
    taxes, per-row currency and trade FX) over RAW closes (split-adjusted closes
    multiplied back by later split factors — consistent with the ledger's own trade
    prices; dividends come from the ledger, not from adjusted closes), converts
    every holding into the base currency with daily FX closes, applies splits found
    in the market data that the ledger does not record (reported in "warnings"), and
    never outputs a day on which a held position has no price / FX.

    transactions: list of {"date", "symbol", "type", "quantity", "price", optional
                  "fees", "taxes", "currency", "fx_rate", "created_at"}.
    base_currency: ISO code; default: the single quoted currency of the traded
                  instruments, else USD (legacy callers passed no currency).
    Returns: {"dates", "navs" (base), "costs" (base), "missing", "warnings",
              "splits_detected", "base_currency"} or {"error", ...}.
    """
    try:
        import os as _os
        import sys as _sys
        _here = _os.path.dirname(_os.path.abspath(__file__))
        if _here not in _sys.path:
            _sys.path.append(_here)
        import portfolio_metrics as _pm

        if not transactions:
            return {"dates": [], "navs": [], "costs": []}
        if not base_currency:
            ccys = {_pm.major_ccy(_pm.ccy_for_symbol(t.get("symbol")))[0]
                    for t in transactions if t.get("symbol")}
            ccys.discard("")
            base_currency = ccys.pop() if len(ccys) == 1 else "USD"
        return _pm.nav_replay({"transactions": transactions, "base_currency": base_currency,
                               "track_cash": bool(track_cash),
                               "instrument_currencies": instrument_currencies or {}})
    except SystemExit:
        return {"dates": [], "navs": [], "costs": [], "error": "price download failed"}
    except Exception as e:
        return {"dates": [], "navs": [], "costs": [], "error": str(e)}


# Daily/weekly/monthly bars Yahoo leaves broken, repaired from its 60m bars. BSE
# (.BO) daily/weekly series are near-empty (weekly can even raise "'Dividends'
# events are out-of-range"), and for hours after the close on Asian venues the
# newest daily/weekly bar has NaN OHLC (indices: no row at all), so the chart's
# last candle was the previous session. 60m bars are grouped in the exchange time
# zone and keyed like yfinance: 00:00 of the day, of the week's Monday, of the 1st.
# interval -> (calendar days per bar, 60m window that covers the newest bar)
_BARS_FROM_60M = {"1d": (1.4, "5d"), "1wk": (7, "1mo"), "1mo": (30.4, "3mo")}
# Calendar days of 60m bars that cover the newest daily / weekly / monthly bar.
_SHORT_60M_DAYS = {"1d": 10, "1wk": 35, "1mo": 95}

# price_store error codes -> this file's failure codes ("Failure reporting" above).
_STORE_CODES = {"UNKNOWN_SYMBOL": "not_found", "NO_DATA_IN_RANGE": "no_data", "RANGE_LIMIT": "no_data",
                "RATE_LIMITED": "rate_limited", "NETWORK": "network", "OFFLINE_MISS": "network",
                "CORRUPT": "unknown", "BAD_REQUEST": "no_data"}


def _cached_history(symbol, period=None, interval="1d", start=None, end=None):
    """(DataFrame indexed in the exchange's time zone, provenance, cache-entry metadata) of one
    symbol from the shared bar cache, split+dividend adjusted (what Ticker.history's default
    auto_adjust returns). Raises price_store.PriceDataError / ValueError."""
    df, prov = fy.frame(symbol, period=period, interval=interval, start=start, end=end,
                        adjust="split+dividend")
    tz = prov.get("exchangeTz")
    out = df[["Open", "High", "Low", "Close", "Volume"]].copy()
    out.index = fy._localize(out.index, tz)
    out = out[~out.index.isna()]
    meta = {}
    try:
        ps = fy._price_store_module()
        e = fy.store().read_entry(ps.normalize_symbol(symbol), "1h" if interval == "4h" else interval, touch=False)
        meta = dict(e.meta) if e is not None else {}
    except Exception:
        meta = {}
    meta.setdefault("exchangeTz", tz)
    return out, prov, meta


def _store_failure(exc, symbol, no_data_msg):
    """The {"error", "symbol", "code"} object for a bar-cache failure, or None when `exc` is
    not one (a bug: the caller re-raises it)."""
    if isinstance(exc, ValueError):
        return _failure(symbol, "no_data", f"{no_data_msg} ({_short_text(str(exc), 120)})")
    code = getattr(exc, "code", None)
    if code is None or type(exc).__name__ != "PriceDataError":
        if _failure_code(str(exc), exc) in ("rate_limited", "network"):
            c, m = _classify_failure(exc, symbol)
            return _failure(symbol, c, m)
        return None
    ours = _STORE_CODES.get(code, "unknown")
    if ours in ("rate_limited", "not_found"):
        return _failure(symbol, ours, _failure_message(ours, str(exc), symbol, no_data_msg))
    if ours == "network" and "rate" in str(exc).lower():
        return _failure(symbol, "rate_limited", _failure_message("rate_limited", str(exc), symbol))
    return _failure(symbol, ours, str(exc) or no_data_msg)


def _repair_from_cached_60m(symbol, hist, period, interval, meta):
    """Daily/weekly/monthly bars Yahoo leaves broken, repaired from 60m bars of the same bar
    cache. BSE (.BO) daily/weekly series are near-empty, and for hours after the close on Asian
    venues the newest daily/weekly bar has NaN OHLC (dropped by the cache), so the chart's last
    candle was the previous session. 60m bars are grouped in the exchange time zone and keyed
    like yfinance: 00:00 of the day, of the week's Monday, of the 1st. Returns `hist` (None
    after a failed request) unchanged when nothing is missing or 60m bars do not help."""
    def key(ix):  # naive exchange-local DatetimeIndex -> start of its bar
        day = ix.normalize()
        return day - pd.to_timedelta({"1wk": day.dayofweek, "1mo": day.day - 1}.get(interval, 0), unit="D")

    try:
        step, _short = _BARS_FROM_60M[interval]
        ok = 0 if hist is None else int(hist["Close"].notna().sum())
        m = re.fullmatch(r"(\d+)(d|wk|mo|y)", str(period))
        days = (int(m.group(1)) * {"d": 1, "wk": 7, "mo": 30, "y": 365}[m.group(2)] if m
                else datetime.now().timetuple().tm_yday if period == "ytd" else 99999)
        rebuild = ok < min(5, days / step / 3)
        if not rebuild:
            last = hist.index[-1]
            rmt = _epoch((meta or {}).get("regularMarketTime"))
            if not rmt:
                return hist
            now = pd.Timestamp(rmt, unit="s", tz="UTC").tz_convert(hist.index.tz)
            if not key(pd.DatetimeIndex([now.tz_localize(None)]))[0] > last.tz_localize(None):
                return hist  # newest bar present: no 60m bars needed
        span = min(days, 729) if rebuild else _SHORT_60M_DAYS[interval]
        from datetime import timedelta as _td
        with _yf_quiet():
            h, _p, _m = _cached_history(symbol, interval="60m", start=(datetime.now() - _td(days=span)).date())
        h = h.dropna(subset=["Close"])
        if h.empty:
            return hist
        agg = h.groupby(key(h.index.tz_localize(None))).agg(
            Open=("Open", "first"), High=("High", "max"), Low=("Low", "min"),
            Close=("Close", "last"), Volume=("Volume", "sum"))
        agg.index = agg.index.tz_localize(h.index.tz)
        if rebuild:
            agg = agg if interval == "1d" else agg.iloc[1:]  # range start cuts the first week/month
            return agg if len(agg) > ok else hist
        new = agg[agg.index > last]
        if new.empty:
            return hist
        vol = hist["Volume"].reindex(new.index)  # Yahoo's own session volume when it has one
        return pd.concat([hist.loc[:last], new.assign(Volume=vol.where(vol > 0, new["Volume"]))])
    except Exception:
        return hist


def get_historical_period(symbol, period='6mo', interval='1d'):
    """Fetch historical data using a period string (e.g., '1mo', '6mo', '1y', '5y')

    Returns the bar list, or {"error", "symbol", "code"} when Yahoo has no rows
    (see "Failure reporting" above) — never an empty list.
    """
    logs = []
    no_data_msg = f"No price data for {symbol} (period={period}, interval={interval})"
    try:
        # One on-disk bar cache per symbol + interval (fincept_yahoo / price_store): the first
        # request downloads a long span once, every period is a local slice of it, and later
        # requests fetch only the missing tail when the market has moved on.
        try:
            with _yf_quiet(logs):
                hist, _prov, meta = _cached_history(symbol, period=period, interval=interval)
        except Exception as e:
            failed = _store_failure(e, symbol, no_data_msg)
            if failed is None:
                raise
            if interval not in _BARS_FROM_60M or failed.get("code") not in ("no_data",):
                return failed
            hist, meta = None, {}
        if interval in _BARS_FROM_60M:
            hist = _repair_from_cached_60m(symbol, hist, period, interval, meta)

        if hist is None or hist.empty:
            return _error_result(None, symbol, logs, default="no_data", no_data_msg=no_data_msg)

        historical_data = []
        for index, row in hist.iterrows():
            # Same NaN guard as get_historical — this is the chart path, where
            # one halted bar previously blanked the entire series.
            close = _num(row['Close'])
            if close is None:
                continue
            historical_data.append({
                "timestamp": int(index.timestamp()),
                "open": _num(row['Open']),
                "high": _num(row['High']),
                "low": _num(row['Low']),
                "close": close,
                "volume": _int(row['Volume'], 0)
            })

        if not historical_data:
            return _failure(symbol, "no_data", no_data_msg)
        return historical_data
    except Exception as e:
        return _error_result(e, symbol, logs, no_data_msg=no_data_msg)

# ── Symbol resolution ────────────────────────────────────────────────────────

# Whole-call budget for resolve_symbol (seconds). Every attempt is a network
# round trip, so attempts run in parallel phases and stop at this deadline.
# Override with FINCEPT_RESOLVE_BUDGET.
try:
    _RESOLVE_BUDGET_S = float(os.environ.get("FINCEPT_RESOLVE_BUDGET", "20"))
except (TypeError, ValueError):
    _RESOLVE_BUDGET_S = 20.0

# US class shares typed the way exchanges print them (BRK.B, BF.B, HEI.A);
# Yahoo spells them with a dash (BRK-B). One-letter Yahoo exchange suffixes
# (.L London, .F Frankfurt, .V TSX Venture, .T Tokyo) look the same, so for
# those letters the as-typed form is preferred and the dash form tried second.
_CLASS_SHARE_RE = re.compile(r"^([A-Z]{1,5})\.([A-Z])$")
_ONE_LETTER_EXCHANGE_SUFFIXES = frozenset("LFVT")
# India guesses for a bare token. They rank below the typed text and below
# Yahoo's own ranking of listings with that root (BMW -> BMW.DE, not BMW.BO),
# but are fetched in the first parallel round so using them costs no extra trip.
_RESOLVE_SUFFIXES = (".NS", ".BO")
# Last resort for a bare token that nothing above placed: one parallel round,
# so widening this list costs no extra wall time.
_RESOLVE_WIDE_SUFFIXES = (".L", ".TO", ".AX", ".DE", ".PA")
_RESOLVE_CONFIDENT_TYPES = ("EQUITY", "ETF", "INDEX")
_RESOLVE_MAX_CANDIDATES = 8
# Most search hits sharing the typed root that are verified (in one parallel round).
_RESOLVE_MAX_ROOT_PROBES = 4
# Words that do not identify a company, ignored when comparing typed text to a name.
_NAME_NOISE_WORDS = frozenset((
    "inc", "incorporated", "corp", "corporation", "co", "company", "companies", "ltd", "limited",
    "plc", "llc", "lp", "sa", "ag", "nv", "se", "spa", "oyj", "asa", "ab", "bhd", "tbk", "the",
    "com", "group", "holdings", "holding", "class", "ordinary", "shares", "adr", "ads", "new",
))


def _chart_meta(ticker):
    """Chart metadata from the history() call just made on `ticker` (exchange
    code, names). Ticker.history_metadata would issue a second, intraday request
    to fill in trading periods, so read what the first call already stored."""
    try:
        meta = getattr(ticker._lazy_load_price_history(), "_history_metadata", None)
        return meta if isinstance(meta, dict) else {}
    except Exception:
        return {}


class _ProbeUncached(Exception):
    def __init__(self, result):
        super().__init__(result.get("message") or "")
        self.result = result


def _probe_symbol(sym, timeout):
    """One chart request: does Yahoo have recent bars for `sym`?
    {"ok": True, "exchange", "name"} or {"ok": False, "code", "message"}.
    Remembered on disk (symbol metadata class): a hit for a week, "no such symbol" for
    6 hours; network / rate-limit failures are not remembered."""
    def fetch():
        r = _probe_symbol_uncached(sym, timeout)
        if r.get("ok"):
            return r
        if r.get("code") in ("not_found", "no_data"):
            return fy.Partial(r, 6 * 3600)
        raise _ProbeUncached(r)
    try:
        return fy.cached("probe", sym, fetch, max_age=7 * 86400, stale_on_error=False)
    except _ProbeUncached as e:
        return e.result
    except Exception as e:
        code, message = _classify_failure(e, sym)
        return {"ok": False, "code": code, "message": message}


def _probe_symbol_uncached(sym, timeout):
    try:
        ticker = yf.Ticker(sym)
        hist = _yf_history(ticker, period="1mo", interval="1d", timeout=timeout)
        if hist is not None and not hist.empty and not hist["Close"].dropna().empty:
            meta = _chart_meta(ticker)
            return {"ok": True, "exchange": str(meta.get("exchangeName") or ""),
                    "name": str(meta.get("longName") or meta.get("shortName") or "")}
        return {"ok": False, "code": "no_data", "message": f"No recent price data for {sym}"}
    except Exception as e:
        code, message = _classify_failure(e, sym)
        return {"ok": False, "code": code, "message": message}


def _run_parallel(jobs, deadline):
    """Run {key: zero-arg callable} concurrently and return {key: result or
    exception} for every job that finished before `deadline` (time.monotonic()).
    Stragglers are abandoned, not awaited: each carries its own network timeout."""
    from concurrent.futures import ThreadPoolExecutor, wait
    if not jobs:
        return {}
    pool = ThreadPoolExecutor(max_workers=len(jobs))
    try:
        futures = {pool.submit(fn): key for key, fn in jobs.items()}
        wait(list(futures), timeout=max(0.0, deadline - time.monotonic()))
        out = {}
        for fut, key in futures.items():
            if fut.done():
                try:
                    out[key] = fut.result()
                except Exception as e:
                    out[key] = e
        return out
    finally:
        try:
            pool.shutdown(wait=False, cancel_futures=True)
        except TypeError:  # Python < 3.9
            pool.shutdown(wait=False)


def _name_words(text):
    return [w for w in re.sub(r"[^a-z0-9]+", " ", str(text or "").lower()).split()
            if w not in _NAME_NOISE_WORDS]


def _name_match_score(typed_words, name):
    """Share of the name's identifying words the typed words cover, when the
    typed words are its leading words ("apple" -> "Apple Inc." = 1.0,
    "meta" -> "Meta Platforms" = 0.5, "bank" -> "Bank of America" = 0.33); else 0."""
    words = _name_words(name)
    if not typed_words or not words or words[:len(typed_words)] != typed_words:
        return 0.0
    return len(typed_words) / len(words)


def _root_match_hits(typed, hits):
    """Search hits whose symbol, or its root before an exchange suffix, is the
    typed text (case-insensitive; BRK.B also matches BRK-B), in Yahoo's own
    ranking order. Options are skipped. RELIANCE -> [RELIANCE.NS, ...],
    BMW -> [BMW.DE, BMW.BO, BMW.F, ...]."""
    typed_up = str(typed or "").strip().upper()
    wanted = {typed_up, typed_up.replace(".", "-")}
    out = []
    for h in hits or []:
        sym = str(h.get("symbol") or "").upper()
        if not sym or str(h.get("type") or "").upper() == "OPTION":
            continue
        if sym in wanted or sym.split(".", 1)[0] in wanted:
            out.append(h)
    return out


def _confident_name_hit(typed, hits):
    """The top equity/ETF/index search hit when its name plainly is what was typed, else None.

    Confident means the typed words lead its name, cover at least half of it,
    and match it strictly better than any other equity/ETF/index hit with a
    different name ("apple" picks Apple Inc. over Apple Hospitality REIT;
    "general" picks nothing).
    """
    allowed = [h for h in hits or []
               if str(h.get("type") or "").upper() in _RESOLVE_CONFIDENT_TYPES and h.get("symbol")]
    if not allowed:
        return None
    top = allowed[0]
    typed_words = _name_words(typed)
    score = _name_match_score(typed_words, top.get("name"))
    if score < 0.5:
        return None
    top_name = _name_words(top.get("name"))
    for other in allowed[1:]:
        if _name_words(other.get("name")) != top_name and \
                _name_match_score(typed_words, other.get("name")) >= score:
            return None
    return top


def _search_candidates(hits):
    """Up to _RESOLVE_MAX_CANDIDATES search hits for the UI to offer (options omitted)."""
    out = []
    for h in hits or []:
        if str(h.get("type") or "").upper() == "OPTION" or not h.get("symbol"):
            continue
        out.append({"symbol": h.get("symbol", ""), "name": h.get("name") or "",
                    "exchange": h.get("exchange") or "", "type": h.get("type") or ""})
        if len(out) >= _RESOLVE_MAX_CANDIDATES:
            break
    return out


def resolve_symbol(symbol, budget_s=None):
    """
    Resolve what a user typed to a working yfinance symbol.

    Handles a bare ticker (RELIANCE -> RELIANCE.NS, BMW -> BMW.DE, VOD), a US
    class share typed with a dot (BRK.B -> BRK-B), a symbol already in Yahoo
    form (^GSPC, EURUSD=X, GC=F, BTC-USD) and a company name ("apple" -> AAPL).

    Preference order — the audience is global, so a bare ticker must not
    quietly become an Indian listing:
      1. the text exactly as typed (or the dash spelling of a class share),
         when Yahoo has it: US/primary listing, index, FX, crypto, futures;
      2. a search hit whose symbol root is the typed text, in Yahoo's own
         ranking (RELIANCE -> RELIANCE.NS, BMW -> BMW.DE, HSBA -> HSBA.L);
      3. the .NS / .BO guesses;
      4. the top equity/ETF/index search hit when its name is a confident
         match for the typed text ("apple" -> AAPL);
      5. for a bare token, the wider suffix list _RESOLVE_WIDE_SUFFIXES.
    The first round fetches 1, 3 and the search together, so most inputs need
    one round trip; 2, 4 and 5 add one parallel round each only when reached.
    Everything is bounded by budget_s (default _RESOLVE_BUDGET_S). A candidate
    counts as found only when Yahoo returns recent bars for it.

    Returns a dict with:
      - resolved_symbol: the working yfinance symbol (e.g., "PIDILITIND.NS");
        the upper-cased input when nothing was found
      - original_symbol: what the user typed, upper-cased (e.g., "PIDILITIND")
      - exchange: Yahoo exchange code (e.g., "NSI", "NMS"), "" when not found
      - found: bool indicating if any variant returned data
      - candidates: [{"symbol","name","exchange","type"}, ...] up to 8 search
        hits for the UI to offer ([] when the search failed or matched nothing)
      - when found: "name" and "method" ("direct" | "normalized" | "suffix" | "search")
      - when not found: "code" and "message"; and when Yahoo could not be
        reached at all (code network / rate_limited / unknown) also "error",
        so the host reports a failed lookup rather than "no such symbol"
    """
    text = " ".join(str(symbol or "").split())
    s = text.upper()
    result = {"resolved_symbol": s, "original_symbol": s, "exchange": "", "found": False, "candidates": []}
    if not s:
        result.update({"code": "not_found", "message": "No symbol entered"})
        return result

    deadline = time.monotonic() + (budget_s if budget_s else _RESOLVE_BUDGET_S)

    def call_timeout():
        return max(1.0, min(_NET_TIMEOUT, deadline - time.monotonic() - 0.5))

    def resolved(sym, method, probe, hits):
        listed = next((h for h in hits if str(h.get("symbol", "")).upper() == sym.upper()), {})
        result.update({"resolved_symbol": sym,
                       "exchange": probe.get("exchange") or listed.get("exchange") or "",
                       "found": True,
                       "name": probe.get("name") or listed.get("name") or "",
                       "method": method})
        return result

    def probe_round(candidates):
        timeout = call_timeout()
        jobs = {c: (lambda c=c: _probe_symbol(c, timeout)) for c in candidates}
        done = _run_parallel(jobs, deadline)
        return {c: (r if isinstance(r, dict) else {"ok": False, "code": "unknown", "message": str(r)})
                for c, r in done.items()}

    # Step 1 candidates (the typed text), and the step 3 guesses fetched alongside.
    m = _CLASS_SHARE_RE.match(s)
    bare = bool(re.fullmatch(r"[A-Z0-9&]{1,20}", s))
    if m:
        dash = f"{m.group(1)}-{m.group(2)}"
        exact = [(s, "direct"), (dash, "normalized")] if m.group(2) in _ONE_LETTER_EXCHANGE_SUFFIXES \
            else [(dash, "normalized"), (s, "direct")]
        query = dash
    elif " " in s:
        exact, query = [], text  # a name: search only
    else:
        exact, query = [(s, "direct")], text  # bare ticker, or already Yahoo form (^GSPC, EURUSD=X)
    guesses = [s + suf for suf in _RESOLVE_SUFFIXES] if bare else []

    with _yf_quiet():
        timeout = call_timeout()
        first_round = [c for c, _ in exact] + guesses
        jobs = {("probe", c): (lambda c=c: _probe_symbol(c, timeout)) for c in first_round}
        jobs[("search",)] = lambda: _yahoo_search_api(query, 10, min(5.0, timeout))
        done = _run_parallel(jobs, deadline)

        search = done.get(("search",))
        hits = search if isinstance(search, list) else []
        result["candidates"] = _search_candidates(hits)
        probes = {}  # upper-cased symbol -> probe result
        for c in first_round:
            r = done.get(("probe", c))
            probes[c.upper()] = r if isinstance(r, dict) else \
                {"ok": False, "code": "network", "message": "Symbol lookup timed out"}

        # 1. The text exactly as typed.
        for c, method in exact:
            if probes[c.upper()].get("ok"):
                return resolved(c, method, probes[c.upper()], hits)

        # Did Yahoo answer anything at all? If not, report the failure instead of
        # "not found" — the symbol may well exist.
        answered = isinstance(search, list) or any(
            p.get("code") in ("not_found", "no_data") for p in probes.values())
        if not answered:
            failures = {c: {"code": p["code"], "message": p["message"]} for c, p in probes.items()}
            if isinstance(search, BaseException):
                code, message = _classify_failure(search)
                failures["(search)"] = {"code": code, "message": message}
            elif ("search",) not in done:
                failures["(search)"] = {"code": "network", "message": "Symbol search timed out"}
            summary = _summarize_failures(failures)
            code = summary["code"] if summary["code"] not in ("not_found", "no_data") else "unknown"
            result.update({"code": code, "message": summary["message"], "error": summary["message"]})
            return result

        def verify(symbols):
            """Probe the not-yet-probed symbols in one parallel round (budget permitting)."""
            todo = [x for x in symbols if x.upper() not in probes]
            if todo and deadline - time.monotonic() > 1.0:
                for x, r in probe_round(todo).items():
                    probes[x.upper()] = r

        # 2. Listings sharing the typed root, in Yahoo's ranking. A guess from
        #    round one (RELIANCE.NS) is reused; only unknown ones cost a round.
        root_syms = [str(h["symbol"]) for h in _root_match_hits(s, hits)][:_RESOLVE_MAX_ROOT_PROBES]
        first_open = next((x for x in root_syms if probes.get(x.upper(), {}).get("ok")
                           or x.upper() not in probes), None)
        if first_open and first_open.upper() not in probes:
            verify(root_syms)
        for x in root_syms:
            if probes.get(x.upper(), {}).get("ok"):
                return resolved(x, "search", probes[x.upper()], hits)

        # 3. The .NS / .BO guesses (already fetched in round one).
        for g in guesses:
            if probes.get(g.upper(), {}).get("ok"):
                return resolved(g, "suffix", probes[g.upper()], hits)

        # 4. The top search hit, when its name is a confident match.
        hit = _confident_name_hit(text, hits)
        if hit:
            hs = str(hit["symbol"])
            verify([hs])
            if probes.get(hs.upper(), {}).get("ok"):
                return resolved(hs, "search", probes[hs.upper()], hits)

        # 5. Wider exchange suffixes for a bare token; a variant Yahoo's search
        #    listed goes first, then the fixed order.
        if bare and deadline - time.monotonic() > 1.0:
            listed = [str(h.get("symbol", "")).upper() for h in hits]
            fixed = [s + suf for suf in _RESOLVE_WIDE_SUFFIXES]
            wide = sorted(fixed, key=lambda c: (0, listed.index(c)) if c in listed else (1, fixed.index(c)))
            results = probe_round(wide)
            for c in wide:
                if results.get(c, {}).get("ok"):
                    return resolved(c, "suffix", results[c], hits)

    message = f"No Yahoo Finance symbol matches '{text}'"
    if result["candidates"]:
        message += "; closest matches: " + ", ".join(c["symbol"] for c in result["candidates"][:3])
    result.update({"code": "not_found", "message": message})
    return result


def main(args=None):
    # Support both worker pool (args parameter) and subprocess/CLI (sys.argv)
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        return fincept_json.dumps({"error": "Usage: python yfinance_data.py <command> <args>"})

    # yfinance prints some failures to stdout (failed downloads in several releases, financial
    # statement parse errors in 0.2.66); stdout carries exactly one JSON line, written below.
    with fincept_json.stdout_to_stderr():
        result = _cli_result(args)

    # Return JSON for worker pool, print for subprocess/CLI
    # IMPORTANT: Do NOT use indent=2 here. The host subprocess parser
    # looks for the last line starting with '{' or '[' to extract JSON.
    # Pretty-printed JSON puts '{' alone on the first line, breaking parsing.
    # fincept_json.emit == print(dumps(...)) with NaN/Infinity scrubbed.
    return fincept_json.emit(result)


def _cli_result(args):
    """The result object of one CLI command (main() prints it)."""
    command = args[0]

    if command == "quote":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py quote <symbol>"}
        else:
            symbol = args[1]
            result = get_quote(symbol)

    elif command == "batch_quotes":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_quotes <symbol1> <symbol2> ..."}
        else:
            symbols = args[1:]
            result = get_batch_quotes(symbols)

    elif command == "historical":
        if len(args) < 4:
            result = {"error": "Usage: python yfinance_data.py historical <symbol> <start_date> <end_date> [interval]"}
        else:
            symbol = args[1]
            start_date = args[2]
            end_date = args[3]
            interval = args[4] if len(args) > 4 else '1d'
            result = get_historical(symbol, start_date, end_date, interval)

    elif command == "info":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py info <symbol>"}
        else:
            symbol = args[1]
            result = get_info(symbol)

    elif command == "financials":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py financials <symbol>"}
        else:
            symbol = args[1]
            result = get_financials(symbol)

    elif command == "company_profile":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py company_profile <symbol>"}
        else:
            symbol = args[1]
            result = get_company_profile(symbol)

    elif command == "financial_ratios":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py financial_ratios <symbol>"}
        else:
            symbol = args[1]
            result = get_financial_ratios(symbol)

    elif command == "multiple_profiles":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py multiple_profiles <symbol1,symbol2,...>"}
        else:
            symbols = args[1].split(",")
            result = get_multiple_profiles(symbols)

    elif command == "multiple_ratios":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py multiple_ratios <symbol1,symbol2,...>"}
        else:
            symbols = args[1].split(",")
            result = get_multiple_ratios(symbols)

    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py search <query> [limit]"}
        else:
            query = args[1]
            limit = int(args[2]) if len(args) > 2 else 50
            result = search_symbols(query, limit)

    elif command == "news":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py news <symbol> [count]"}
        else:
            symbol = args[1]
            count = int(args[2]) if len(args) > 2 else 20
            result = get_news(symbol, count)

    elif command == "portfolio_nav_history":
        # Args layout: <period> <sym1> <qty1> <sym2> <qty2> ...
        # Period defaults to 1y if first arg looks like a symbol.
        if len(args) < 3:
            result = {"error": "Usage: yfinance_data.py portfolio_nav_history <period> <sym1> <qty1> ..."}
        else:
            period = args[1]
            tail = args[2:]
            if len(tail) % 2 != 0:
                result = {"error": "symbol/quantity arguments must be paired"}
            else:
                positions = []
                for i in range(0, len(tail), 2):
                    try:
                        positions.append({"symbol": tail[i], "quantity": float(tail[i + 1])})
                    except ValueError:
                        result = {"error": f"invalid quantity for {tail[i]}"}
                        positions = None
                        break
                if positions is not None:
                    result = get_portfolio_nav_history(positions, period)

    elif command == "portfolio_nav_history_replay":
        # Args layout: <transactions_json_path>
        # The JSON file contains a list of {date, symbol, type, quantity, price}
        # objects. We replay them over time to compute true historical NAV
        # and cost basis instead of back-projecting today's positions.
        if len(args) < 2:
            result = {"error": "Usage: yfinance_data.py portfolio_nav_history_replay <transactions_json_path>"}
        else:
            try:
                with open(args[1], "r", encoding="utf-8") as f:
                    txns = json.load(f)
                if not isinstance(txns, list):
                    result = {"error": "transactions JSON must be a list"}
                else:
                    result = get_portfolio_nav_history_replay(txns)
            except Exception as e:
                result = {"error": f"failed to read transactions JSON: {e}"}

    elif command == "historical_period":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py historical_period <symbol> [period] [interval]"}
        else:
            symbol = args[1]
            period = args[2] if len(args) > 2 else '6mo'
            interval = args[3] if len(args) > 3 else '1d'
            result = get_historical_period(symbol, period, interval)

    elif command == "batch_sparklines":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_sparklines <sym1> <sym2> ..."}
        else:
            symbols = args[1:]
            result = get_batch_sparklines(symbols)

    elif command == "quote_names":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py quote_names <sym1> <sym2> ..."}
        else:
            symbols = args[1:]
            result = get_quote_names(symbols)

    elif command == "batch_info":
        # Fundamentals for many symbols in one process — MarketDataService's
        # market:info:* producer. See get_batch_info().
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_info <sym1> <sym2> ..."}
        else:
            result = get_batch_info(args[1:])

    elif command == "batch_all":
        # Unified hub-refresh endpoint: payload is a single JSON blob on argv[1]
        # (or @tempfile via PythonRunner's arg-spill) describing quotes + sparklines
        # + histories to fetch. One process, one yfinance warm-up, three family
        # results. See MarketDataService::refresh().
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_all <json_payload>"}
        else:
            raw = args[1]
            # Handle PythonRunner arg-spill: args > ~8KB land as "@/tmp/…" and
            # must be read back. Payloads for large symbol lists easily exceed
            # that threshold on Windows (32KB cmdline limit).
            if raw.startswith("@"):
                path = raw[1:]
                try:
                    with open(path, "r", encoding="utf-8") as f:
                        raw = f.read()
                    try:
                        import os as _os
                        _os.remove(path)
                    except Exception:
                        pass
                except Exception as e:
                    result = {"error": f"batch_all: failed to read spill file: {e}"}
                    raw = None
            if raw is not None:
                try:
                    payload = json.loads(raw)
                    result = get_batch_all(payload)
                except json.JSONDecodeError as e:
                    result = {"error": f"batch_all: invalid JSON payload: {e}"}

    elif command == "resolve_symbol":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py resolve_symbol <symbol or company name>"}
        else:
            # Joined so an unquoted company name ("apple inc") arrives whole.
            symbol = " ".join(args[1:])
            result = resolve_symbol(symbol)

    elif command == "historical_price":
        # Get closing price for a specific date
        if len(args) < 3:
            result = {"error": "Usage: python yfinance_data.py historical_price <symbol> <date>"}
        else:
            symbol = args[1]
            target_date = args[2]  # Format: YYYY-MM-DD
            result = get_historical_price(symbol, target_date)

    else:
        result = {"error": f"Unknown command: {command}"}

    return result

# ── Daemon mode ──────────────────────────────────────────────────────────────
#
# Long-lived worker invoked by PythonWorker on the C++ side. Reads length-prefixed
# JSON request frames from stdin, writes length-prefixed JSON response frames to
# stdout. Keeps yfinance + pandas loaded so MarketDataService refreshes don't pay
# the 2–3s import cost on every spawn.
#
# Frame format (stdin + stdout, network byte order):
#   [4 bytes big-endian uint32 length N] [N bytes UTF-8 JSON]
#
# Request JSON:  {"id": <int>, "action": "batch_all"|"batch_quotes"|..., "payload": {...}}
# Response JSON: {"id": <int>, "ok": true, "result": <any>}  or
#                {"id": <int>, "ok": false, "error": "<msg>"}
#
# Special action "shutdown" causes a clean exit. EOF on stdin also exits.

def _daemon_read_frame(stream):
    """Read one length-prefixed frame. Returns bytes payload, or None on EOF."""
    header = b""
    while len(header) < 4:
        chunk = stream.read(4 - len(header))
        if not chunk:
            return None
        header += chunk
    n = int.from_bytes(header, byteorder="big", signed=False)
    if n == 0 or n > 64 * 1024 * 1024:  # sanity: cap frames at 64 MB
        return None
    buf = b""
    while len(buf) < n:
        chunk = stream.read(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def _daemon_write_frame(stream, data_bytes):
    """Write one length-prefixed frame. `data_bytes` must be bytes."""
    n = len(data_bytes)
    stream.write(n.to_bytes(4, byteorder="big", signed=False))
    stream.write(data_bytes)
    stream.flush()


def _daemon_dispatch(action, payload):
    """Run one action and return the raw result object (not wrapped).

    Mirrors the CLI dispatch in main() — every action a C++ caller might pass
    through PythonRunner is wired here so the daemon can handle it without the
    ~3s subprocess + import cost.
    """
    p = payload or {}
    if action == "yahoo_stats":
        # Counters of this daemon's Yahoo traffic since the previous reset (no network):
        # requests sent (by kind), answers served from memory / disk, merged waits, limiter
        # waits / rejections, HTTP 429s, breaker state, all processes' total.
        return fy.stats(reset=bool(p.get("reset")))
    if action == "batch_all":
        return get_batch_all(p)
    if action == "batch_quotes":
        return get_batch_quotes(p.get("symbols") or [])
    if action == "batch_sparklines":
        return get_batch_sparklines(p.get("symbols") or [])
    if action == "historical_period":
        return get_historical_period(
            p.get("symbol"), p.get("period", "6mo"), p.get("interval", "1d"))
    if action == "historical":
        return get_historical(
            p.get("symbol"), p.get("start_date"), p.get("end_date"), p.get("interval", "1d"))
    if action == "quote":
        return get_quote(p.get("symbol"))
    if action == "info":
        return get_info(p.get("symbol"))
    if action == "news":
        return get_news(p.get("symbol"), p.get("count", 20))
    if action == "financials":
        return get_financials(p.get("symbol"))
    if action == "company_profile":
        return get_company_profile(p.get("symbol"))
    if action == "financial_ratios":
        return get_financial_ratios(p.get("symbol"))
    if action == "multiple_ratios":
        return get_multiple_ratios(p.get("symbols") or [])
    if action == "multiple_profiles":
        return get_multiple_profiles(p.get("symbols") or [])
    if action == "batch_info":
        # Wired for completeness. MarketDataService runs batch_info as a
        # subprocess on purpose: it takes seconds and this loop is serial.
        return get_batch_info(p.get("symbols") or [])
    if action == "quote_names":
        # Wired for the runner's daemon routing (one cached quote batch now, no Ticker.info).
        return get_quote_names(p.get("symbols") or [])
    if action == "historical_price":
        return get_historical_price(p.get("symbol"), p.get("date") or p.get("target_date"))
    if action == "search":
        return search_symbols(p.get("query") or "", int(p.get("limit", 50)))
    if action == "resolve_symbol":
        # payload {"symbol": "<what the user typed>"}; "query" accepted as an alias.
        return resolve_symbol(p.get("symbol") or p.get("query") or "")
    if action == "portfolio_nav_history":
        return get_portfolio_nav_history(p.get("positions") or [], p.get("period", "1y"))
    if action == "portfolio_nav_history_replay":
        return get_portfolio_nav_history_replay(p.get("transactions") or [])
    return {"error": f"Unknown action: {action}"}


def run_daemon():
    """Main daemon loop — read frame, dispatch, write frame, repeat."""
    import io
    # The daemon serves the UI one request at a time: a long limiter wait would stall every
    # request queued behind it, so it answers from cache / fails fast instead.
    fy.configure(max_wait_s=float(os.environ.get("FINCEPT_YAHOO_DAEMON_MAX_WAIT_S", "8")))
    # Use the raw binary stdio streams; we do our own framing.
    stdin = sys.stdin.buffer
    stdout = sys.stdout.buffer
    # Ready marker so the C++ host knows imports are done and the worker is
    # ready to accept requests. Uses the same framing.
    try:
        ready = fincept_json.dumps_bytes({"ready": True, "pid": os.getpid()})
        _daemon_write_frame(stdout, ready)
    except Exception:
        pass

    while True:
        frame = _daemon_read_frame(stdin)
        if frame is None:
            break  # EOF or bad frame — exit
        try:
            req = json.loads(frame.decode("utf-8"))
        except Exception as e:
            err = {"id": 0, "ok": False, "error": f"bad request JSON: {e}"}
            _daemon_write_frame(stdout, fincept_json.dumps_bytes(err))
            continue

        req_id = req.get("id", 0)
        action = req.get("action", "")
        if action == "shutdown":
            resp = {"id": req_id, "ok": True, "result": {"shutdown": True}}
            _daemon_write_frame(stdout, fincept_json.dumps_bytes(resp))
            break

        try:
            # A library line printed to sys.stdout would land between two frames on the same
            # file descriptor and corrupt the protocol: it goes to stderr. (Frames are written
            # through `stdout`, the binary stream taken above, which this does not touch.)
            with fincept_json.stdout_to_stderr():
                result = _daemon_dispatch(action, req.get("payload"))
            resp = {"id": req_id, "ok": True, "result": result}
        except Exception as e:
            resp = {"id": req_id, "ok": False, "error": str(e)}

        # Encode before writing. Every request MUST get exactly one response
        # frame: the host correlates by id and a request that never gets a
        # frame back only ends when its deadline sweep fires. So if the result
        # itself can't be serialised, downgrade to an error frame for that id
        # rather than dropping it (or killing the loop).
        try:
            payload_bytes = fincept_json.dumps_bytes(resp)
        except Exception as e:
            payload_bytes = fincept_json.dumps_bytes(
                {"id": req_id, "ok": False, "error": f"unserialisable result for {action}: {e}"})
        try:
            _daemon_write_frame(stdout, payload_bytes)
        except Exception:
            break  # stdout is gone — the host will notice the exit
        try:
            fy.flush_quotes()  # quote snapshot for the next start (at most every 20 s)
        except Exception:
            pass


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--daemon":
        run_daemon()
    else:
        main()
