"""
fincept_yahoo.py - the ONE way the bundled scripts reach Yahoo Finance.

Import (the terminal puts the scripts folder on PYTHONPATH; from a sub-folder add it first):

    import os, sys
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # sub-folder only
    import fincept_yahoo as fy

Public API (stable; every call is safe from threads; nothing is printed to stdout):

    yf = fy.yf()
        yfinance, configured: one shared HTTP session per process with the cookie + crumb
        persisted on disk (no handshake per script start), a cross-process request limiter,
        back-off on HTTP 429 / 999 / empty crumbs, a circuit breaker, and yfinance's tz/cookie
        cache under the data root. Importing this module installs all of that for every
        yfinance call in the process (also plain `yf.Ticker(...)` calls).

    rows = fy.quotes(["AAPL", "EURUSD=X"])
        {symbol as given: Yahoo v7 quote row (raw Yahoo keys) + "_fetched" (epoch s) +
        "_served" ("network"|"memory"|"disk"|"stale" = an older row while Yahoo throttles)}. Batched (50 per request), cached per freshness
        class: 15 s while the instrument's market is open, until the next session open while it
        is closed (memory + on-disk snapshot shared by every process). Unknown symbols are absent.

    df = fy.download(["AAPL", "MSFT"], period="1y", interval="1d", auto_adjust=True, actions=False,
                     group_by="column")
        A drop-in for yf.download() (same frame shape; start/end/period/interval/auto_adjust/
        actions/group_by/multi_level_index) served from the shared on-disk bar cache: a longer
        range is downloaded once and shorter ones are sliced locally; a later call fetches only
        the missing tail. Symbols with no data are all-NaN columns (as yf.download does).

    df = fy.ticker_history("AAPL", period="5y", interval="1d", auto_adjust=True, actions=True)
        A drop-in for yf.Ticker(sym).history() from the same bar cache (exchange-tz index).

    res = fy.bars(["SPY"], "2020-01-01", "2024-12-31", interval="1d", adjust="split+dividend")
        The bar cache itself (Analytics/backtesting/base/price_store.BarsResult: frames,
        provenance, errors, network_calls).

    info = fy.info("AAPL")
        yf.Ticker(sym).info as a plain dict, fetched at most once a day per symbol and shared
        by every script and process (disk JSON). {} when Yahoo has nothing for the symbol.

    value = fy.cached("holders", "AAPL", lambda: fetch(...), max_age=86400)
        Any JSON-able payload, cached on disk per (kind, key) for max_age seconds; concurrent
        identical requests (threads or processes) wait for the one fetch. Return
        fy.Partial(value, retry_s) for a partial answer (cached, retried after retry_s) and
        raise to cache nothing.

    hits = fy.search("apple", limit=10)
        Yahoo symbol search hits ({"symbol","name","exchange","type","currency","industry"}),
        cached for a day.

    rate = fy.fx_rate("EUR", "USD")
        Spot rate from the quote cache (EURUSD=X), None when unknown. 1.0 only for the same
        currency - a missing rate is never 1.0.

    fy.status()   {"breaker_open", "retry_at", "message", "backoff_until"}
    fy.stats(reset=False)   this process's counters (requests sent by kind, served from
                  memory / disk, merged, waited, rejected, HTTP 429s)
    fy.YahooRateLimited   raised INSTEAD of a request while Yahoo throttles us (circuit
                  breaker open or the local limit cannot be met in time); a subclass of
                  yfinance's YFRateLimitError, .retry_at = epoch seconds, str() = user text.

Freshness classes (seconds; env overrides in brackets):
    live quote (market open, crypto)    15            [FINCEPT_YAHOO_QUOTE_LIVE_TTL_S]
    quote, market closed                until the next session open (cap 4 days); re-checked
                                        every 15 min inside the expected session hours
                                        (lunch break / holiday)  [FINCEPT_YAHOO_CLOSED_RECHECK_S]
    intraday bars                       one bar (60 s .. 15 min)
    daily+ bars                         market open: 900 [FINCEPT_PRICE_CACHE_TTL_S];
                                        closed: until the next open once fetched after the close
    fundamentals / profile (Ticker.info) 86400        [FINCEPT_YAHOO_INFO_TTL_S]
    statements                          86400 (a partial answer: its missing frames after 3600)
    news                                600
    symbol search / resolve probes      86400 / 7 days
Limiter / back-off / breaker (shared by every process through a locked state file):
    FINCEPT_YAHOO_RPS 4, FINCEPT_YAHOO_RPM 100 (requests per second / minute),
    FINCEPT_YAHOO_MAX_WAIT_S 90 (8 in the yfinance daemon): longer waits fail with
    YahooRateLimited instead of blocking; back-off after a 429: 5 s doubling to 120 s with
    +-20 % jitter [FINCEPT_YAHOO_BACKOFF_BASE_S / _MAX_S]; circuit breaker after 3 throttled
    answers in a row [FINCEPT_YAHOO_BREAKER_AFTER]: no Yahoo traffic for 300 s, doubling per
    trip within an hour up to 1800 s [FINCEPT_YAHOO_BREAKER_COOLDOWN_S / _MAX_S].
    FINCEPT_YAHOO_OFFLINE=1: never touch the network (cache only).

Files (all under fincept_paths.cache_dir("yahoo") = <data root>/cache/scripts/yahoo):
    yfinance/          yfinance's tz + cookie databases
    session.json       the crumb (with a hash of the cookie it belongs to), 7 days max
    state.json/.lock   limiter, back-off and breaker state shared by all processes
    quotes.json        quote snapshot (newest 3000 symbols)
    json/<kind>/       cached payloads (info, statements, holders, search, ...), capped at
                       FINCEPT_YAHOO_CACHE_MAX_MB (256 MB) by least-recent use
    bars:              the backtesting price store, <data root>/cache/scripts/backtesting/prices
"""

from __future__ import annotations

import atexit
import collections
import contextlib
import hashlib
import json
import math
import os
import random
import re
import sys
import threading
import time
from datetime import date, datetime, timedelta, timezone

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.append(_HERE)

import fincept_paths  # noqa: E402


def _env_float(name, default):
    try:
        return float(os.environ.get(name, default))
    except (TypeError, ValueError):
        return float(default)


RPS = max(1.0, _env_float("FINCEPT_YAHOO_RPS", 4))
RPM = max(5.0, _env_float("FINCEPT_YAHOO_RPM", 100))
MAX_WAIT_S = max(0.0, _env_float("FINCEPT_YAHOO_MAX_WAIT_S", 90))
BACKOFF_BASE_S = max(0.5, _env_float("FINCEPT_YAHOO_BACKOFF_BASE_S", 5))
BACKOFF_MAX_S = max(BACKOFF_BASE_S, _env_float("FINCEPT_YAHOO_BACKOFF_MAX_S", 120))
BREAKER_AFTER = max(1, int(_env_float("FINCEPT_YAHOO_BREAKER_AFTER", 3)))
BREAKER_COOLDOWN_S = max(10.0, _env_float("FINCEPT_YAHOO_BREAKER_COOLDOWN_S", 300))
BREAKER_MAX_S = max(BREAKER_COOLDOWN_S, _env_float("FINCEPT_YAHOO_BREAKER_MAX_S", 1800))
QUOTE_LIVE_TTL_S = max(1.0, _env_float("FINCEPT_YAHOO_QUOTE_LIVE_TTL_S", 15))
CLOSED_RECHECK_S = max(60.0, _env_float("FINCEPT_YAHOO_CLOSED_RECHECK_S", 900))
CLOSED_MAX_S = 4 * 86400
INFO_TTL_S = max(60.0, _env_float("FINCEPT_YAHOO_INFO_TTL_S", 86400))
DAILY_OPEN_TTL_S = max(30.0, _env_float("FINCEPT_PRICE_CACHE_TTL_S", 900))
CRUMB_MAX_AGE_S = max(3600.0, _env_float("FINCEPT_YAHOO_CRUMB_MAX_AGE_S", 7 * 86400))
CACHE_MAX_BYTES = max(16.0, _env_float("FINCEPT_YAHOO_CACHE_MAX_MB", 256)) * 1024 * 1024
OFFLINE = os.environ.get("FINCEPT_YAHOO_OFFLINE", "") not in ("", "0", "false", "no")
NET_TIMEOUT = _env_float("FINCEPT_YF_TIMEOUT", 15)

_V7_URL = "https://query1.finance.yahoo.com/v7/finance/quote"
_SPARK_URL = "https://query1.finance.yahoo.com/v7/finance/spark"
_SEARCH_URL = "https://query2.finance.yahoo.com/v1/finance/search"
_V7_CHUNK = 50
_QUOTE_MEM_MAX = 5000
_QUOTE_DISK_MAX = 3000
_JSON_MEM_MAX = 512
_JSON_MEM_MAX_BYTES = 64 * 1024 * 1024


def _diag(msg):
    try:
        sys.stderr.write("[fincept_yahoo] " + msg + "\n")
    except Exception:
        pass


def cache_root():
    """<data root>/cache/scripts/yahoo (created on demand)."""
    return fincept_paths.cache_dir("yahoo")


def configure(max_wait_s=None):
    """Process-wide knobs. The yfinance daemon sets a short max wait: it serves the UI and
    must answer from cache rather than sit out a long limiter wait."""
    global MAX_WAIT_S
    if max_wait_s is not None:
        MAX_WAIT_S = max(0.0, float(max_wait_s))


# ════════════════════════════════════════════════════════════════════════════
# Counters
# ════════════════════════════════════════════════════════════════════════════

_stats_lock = threading.Lock()
_stats = {"sent": {}, "served_memory": 0, "served_disk": 0, "merged": 0, "waited": 0, "rejected": 0,
          "http_429": 0, "errors": 0}
_stats_since = time.time()


def _count(key, n=1, kind=None):
    with _stats_lock:
        if kind is not None:
            _stats["sent"][kind] = _stats["sent"].get(kind, 0) + n
        else:
            _stats[key] = _stats.get(key, 0) + n


def stats(reset=False):
    """This process's counters since start (or the previous reset)."""
    global _stats_since
    with _stats_lock:
        out = {"window_s": int(time.time() - _stats_since), "sent": sum(_stats["sent"].values()),
               "sent_by_kind": dict(_stats["sent"])}
        for k, v in _stats.items():
            if k != "sent":
                out[k] = v
        if reset:
            _stats["sent"] = {}
            for k in list(_stats):
                if k != "sent":
                    _stats[k] = 0
            _stats_since = time.time()
    st = _read_state_quiet()
    out["breaker_open"] = bool(st.get("breaker_until", 0) > time.time())
    out["breaker_until"] = int(st.get("breaker_until", 0) or 0)
    out["all_processes_sent_total"] = int(st.get("sent_total", 0) or 0)
    out["all_processes_throttled_total"] = int(st.get("throttled_total", 0) or 0)
    return out


# ════════════════════════════════════════════════════════════════════════════
# Exceptions
# ════════════════════════════════════════════════════════════════════════════

def _rate_limit_base():
    try:
        from yfinance.exceptions import YFRateLimitError
        return YFRateLimitError
    except Exception:
        return Exception


class YahooRateLimited(_rate_limit_base()):
    """No request was sent: Yahoo is throttling us (breaker open) or the local limit could
    not be met in time. retry_at: epoch seconds. A YFRateLimitError, so existing failure
    classification ("rate_limited") applies."""

    def __init__(self, retry_at, breaker=True):
        self.retry_at = float(retry_at or time.time())
        self.breaker = bool(breaker)
        hhmm = datetime.fromtimestamp(self.retry_at).strftime("%H:%M")
        if breaker:
            text = (f"Yahoo Finance is rate limiting requests (Too Many Requests) - showing saved data, "
                    f"retry at {hhmm}")
        else:
            text = f"Too many Yahoo Finance requests at once - showing saved data, retry at {hhmm}"
        Exception.__init__(self, text)

    @property
    def retry_after_s(self):
        return max(1, int(math.ceil(self.retry_at - time.time())))


class Partial:
    """Return value for cached(): a usable but incomplete answer, retried after retry_s."""

    def __init__(self, value, retry_s=3600):
        self.value = value
        self.retry_s = float(retry_s)


class Negative:
    """Return value for cached(): "Yahoo has nothing for this key" (cached for ttl_s)."""

    def __init__(self, message="", ttl_s=3600):
        self.message = message
        self.ttl_s = float(ttl_s)


class NotAvailable(Exception):
    """cached() raises this for a key negatively cached by a Negative return."""


# ════════════════════════════════════════════════════════════════════════════
# Cross-process file lock (no numpy, no dependency)
# ════════════════════════════════════════════════════════════════════════════

class _FileLock:
    """Exclusive lock on a file, between processes AND threads of this process (re-entrant
    within a thread). The OS releases it if the process dies."""

    _thread_locks = {}   # path -> RLock (threads of this process)
    _held = {}           # path -> (thread ident, depth, fd)
    _tl_guard = threading.Lock()

    def __init__(self, path, timeout=30.0):
        self.path = str(path)
        self.timeout = float(timeout)
        with _FileLock._tl_guard:
            self.tlock = _FileLock._thread_locks.setdefault(self.path, threading.RLock())

    def acquire(self):
        deadline = time.monotonic() + self.timeout
        if not self.tlock.acquire(timeout=max(0.0, self.timeout)):
            return False
        me = threading.get_ident()
        with _FileLock._tl_guard:
            held = _FileLock._held.get(self.path)
            if held and held[0] == me:
                _FileLock._held[self.path] = (me, held[1] + 1, held[2])
                return True
        try:
            os.makedirs(os.path.dirname(self.path), exist_ok=True)
            fd = os.open(self.path, os.O_RDWR | os.O_CREAT, 0o666)
        except OSError:
            self.tlock.release()
            return False
        while True:
            try:
                if os.name == "nt":
                    import msvcrt
                    os.lseek(fd, 0, os.SEEK_SET)
                    msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                with _FileLock._tl_guard:
                    _FileLock._held[self.path] = (me, 1, fd)
                return True
            except OSError:
                if time.monotonic() >= deadline:
                    os.close(fd)
                    self.tlock.release()
                    return False
                time.sleep(0.02)

    def release(self):
        with _FileLock._tl_guard:
            me, depth, fd = _FileLock._held.get(self.path, (None, 0, None))
            if depth > 1:
                _FileLock._held[self.path] = (me, depth - 1, fd)
                self.tlock.release()
                return
            _FileLock._held.pop(self.path, None)
        try:
            if fd is not None:
                if os.name == "nt":
                    import msvcrt
                    os.lseek(fd, 0, os.SEEK_SET)
                    msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(fd, fcntl.LOCK_UN)
        except OSError:
            pass
        finally:
            if fd is not None:
                os.close(fd)
            self.tlock.release()

    def __enter__(self):
        if not self.acquire():
            raise TimeoutError("could not lock " + self.path)
        return self

    def __exit__(self, *exc):
        self.release()


def _atomic_write(path, text):
    try:
        fincept_paths.atomic_write_text(path, text)
        return True
    except Exception as e:  # disk full / permissions: the cache is optional
        _diag(f"could not write {os.path.basename(str(path))}: {type(e).__name__}")
        return False


def _read_json(path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def _dumps(obj):
    try:
        import fincept_json
        return fincept_json.dumps(obj)
    except Exception:
        return json.dumps(obj, default=str)


# ════════════════════════════════════════════════════════════════════════════
# Limiter, back-off and circuit breaker (state shared by every process)
# ════════════════════════════════════════════════════════════════════════════

def _state_path():
    return os.path.join(cache_root(), "state.json")


def _state_lock():
    return _FileLock(os.path.join(cache_root(), "state.lock"), timeout=10.0)


def _read_state_quiet():
    st = _read_json(_state_path())
    return st if isinstance(st, dict) else {}


@contextlib.contextmanager
def _state():
    """The shared limiter state, read-modify-written under the state lock."""
    lock = _state_lock()
    got = lock.acquire()
    st = _read_state_quiet() if got else {}
    before = json.dumps(st, sort_keys=True)
    try:
        yield st
    finally:
        try:
            if got and json.dumps(st, sort_keys=True) != before:
                tmp = _state_path() + ".%d.%d.tmp" % (os.getpid(), threading.get_ident())
                with open(tmp, "w", encoding="utf-8") as f:
                    json.dump(st, f)
                for i in range(20):
                    try:
                        os.replace(tmp, _state_path())
                        break
                    except PermissionError:
                        time.sleep(0.01 * (i + 1))
                else:
                    try:
                        os.remove(tmp)
                    except OSError:
                        pass
        finally:
            if got:
                lock.release()


def status():
    """{"breaker_open", "retry_at", "message", "backoff_until"} - for UIs and scripts."""
    st = _read_state_quiet()
    now = time.time()
    until = float(st.get("breaker_until", 0) or 0)
    out = {"breaker_open": until > now, "retry_at": int(until) if until > now else 0,
           "backoff_until": int(st.get("backoff_until", 0) or 0), "message": ""}
    if until > now:
        out["message"] = str(YahooRateLimited(until, True))
    return out


def _acquire(kind):
    """Wait for a request slot (or raise YahooRateLimited). Never holds the lock while sleeping."""
    if OFFLINE:
        raise YahooRateLimited(time.time() + 3600, breaker=False)
    deadline = time.time() + MAX_WAIT_S
    waited = False
    while True:
        with _state() as st:
            now = time.time()
            until = float(st.get("breaker_until", 0) or 0)
            if until > now:
                _count("rejected")
                raise YahooRateLimited(until, True)
            win = sorted(t for t in (st.get("win") or []) if isinstance(t, (int, float)) and t > now - 60)
            wait = max(0.0, float(st.get("backoff_until", 0) or 0) - now)
            last_sec = [t for t in win if t > now - 1]
            if len(last_sec) >= int(RPS):
                wait = max(wait, last_sec[len(last_sec) - int(RPS)] + 1.0 - now)
            if len(win) >= RPM:
                wait = max(wait, win[len(win) - int(RPM)] + 60.0 - now)
            if wait <= 0:
                win.append(now)
                st["win"] = win[-int(RPM) - 5:]
                st["sent_total"] = int(st.get("sent_total", 0) or 0) + 1
                if waited:
                    _count("waited")
                return
            if now + wait > deadline:
                _count("rejected")
                raise YahooRateLimited(now + wait, breaker=float(st.get("backoff_until", 0) or 0) > now)
        waited = True
        time.sleep(min(wait, 1.0) + random.uniform(0, 0.05))


def _on_throttled(kind):
    _count("http_429")
    with _state() as st:
        now = time.time()
        streak = int(st.get("streak", 0) or 0) + 1
        st["streak"] = streak
        st["throttled_total"] = int(st.get("throttled_total", 0) or 0) + 1
        delay = min(BACKOFF_BASE_S * (2 ** (streak - 1)), BACKOFF_MAX_S) * random.uniform(0.8, 1.2)
        st["backoff_until"] = max(float(st.get("backoff_until", 0) or 0), now + delay)
        if streak >= BREAKER_AFTER:
            trips = int(st.get("trips", 0) or 0)
            if now - float(st.get("last_trip", 0) or 0) > 3600:
                trips = 0
            trips += 1
            cool = min(BREAKER_COOLDOWN_S * (2 ** (trips - 1)), BREAKER_MAX_S)
            st["trips"] = trips
            st["last_trip"] = now
            st["breaker_until"] = now + cool
            st["streak"] = 0
            _diag(f"Yahoo is rate limiting: no Yahoo requests for {int(cool)} s (trip {trips})")


def _on_ok():
    st = _read_state_quiet()
    if int(st.get("streak", 0) or 0):
        with _state() as st2:
            st2["streak"] = 0


def _kind_of(url):
    u = str(url).lower()
    for key, kind in (("getcrumb", "crumb"), ("fc.yahoo.com", "cookie"), ("consent", "consent"),
                      ("/v7/finance/quote", "quote"), ("/finance/spark", "spark"), ("/finance/chart", "chart"),
                      ("quotesummary", "summary"), ("timeseries", "timeseries"), ("/finance/search", "search"),
                      ("/finance/screener", "screener"), ("/xhr/", "news"), ("visualization", "calendar")):
        if key in u:
            return kind
    return "other"


def _guarded(orig):
    """Wrap a curl_cffi Session.request bound method: limiter before, back-off after."""

    def request(method, url, *args, **kwargs):
        kind = _kind_of(url)
        _acquire(kind)
        _count(None, 1, kind)
        try:
            resp = orig(method, url, *args, **kwargs)
        except Exception:
            _count("errors")
            raise
        code = getattr(resp, "status_code", 0) or 0
        throttled = code in (429, 999)
        if not throttled and kind == "crumb" and code < 400:
            try:
                text = (resp.text or "").strip()
            except Exception:
                text = ""
            throttled = (not text) or "too many requests" in text.lower()
        if throttled:
            _on_throttled(kind)
        elif code < 400 or code == 404:
            _on_ok()
        return resp

    request._fy_guarded = True
    return request


# ════════════════════════════════════════════════════════════════════════════
# Session: yfinance configured once per process
# ════════════════════════════════════════════════════════════════════════════

_install_lock = threading.RLock()
_installed = False
_crumb_saved = None


def _session_file():
    return os.path.join(cache_root(), "session.json")


def _cookie_hash(data):
    """sha1 of Yahoo's A3 cookie value in the session (the crumb belongs to that cookie)."""
    v = getattr(getattr(data, "_cookie", None), "value", None)
    if not v:
        try:
            for dom, paths in data._session.cookies.jar._cookies.items():
                if "yahoo" in dom and "consent" not in dom:
                    a3 = (paths.get("/") or {}).get("A3")
                    if a3 is not None and a3.value:
                        v = a3.value
                        break
        except Exception:
            v = None
    if not v:
        return None
    return hashlib.sha1(str(v).encode("utf-8")).hexdigest()


def _restore_crumb(data):
    """Reuse the crumb an earlier process obtained for the same persisted cookie."""
    global _crumb_saved
    saved = _read_json(_session_file())
    if not isinstance(saved, dict) or not saved.get("crumb"):
        return
    if time.time() - float(saved.get("ts", 0) or 0) > CRUMB_MAX_AGE_S:
        return
    strategy = saved.get("strategy") or "basic"
    try:
        with data._cookie_lock:
            if data._crumb is not None:
                return
            if strategy != data._cookie_strategy:
                return
            if not data._load_cookie_curlCffi():
                return
            if _cookie_hash(data) != saved.get("cookie"):
                data._cookie = None
                return
            if strategy == "csrf":
                data._cookie = True
            data._crumb = saved["crumb"]
            _crumb_saved = saved["crumb"]
    except Exception as e:
        _diag(f"saved crumb not reused: {type(e).__name__}")


def _save_crumb(data, crumb, strategy):
    global _crumb_saved
    if not crumb or crumb == _crumb_saved:
        return
    _crumb_saved = crumb
    h = _cookie_hash(data) if strategy == "basic" else "csrf"
    if not h:
        return
    _atomic_write(_session_file(), json.dumps({"crumb": crumb, "strategy": strategy, "cookie": h,
                                               "ts": time.time()}))


def _forget_crumb():
    global _crumb_saved
    _crumb_saved = None
    try:
        os.remove(_session_file())
    except OSError:
        pass


def _make_request(self, url, request_method, body=None, params=None, timeout=30):
    """yfinance 0.2.66 YfData._make_request, with three changes:
      * only 401 / 403 (a rejected cookie or crumb) switch the cookie strategy and retry once -
        yfinance retried EVERY 4xx/5xx that way, so an unknown symbol (404) or a 429 cost up to
        five more requests (consent page, consent post, consent copy, crumb, retry);
      * a 429 is raised at once (no retry: it only extends the throttle);
      * a crumb that works is persisted for the next process (session.json)."""
    from yfinance.exceptions import YFRateLimitError
    if params is None:
        params = {}
    if "crumb" in params:
        raise Exception("Don't manually add 'crumb' to params dict, let data.py handle it")
    crumb, strategy = self._get_cookie_and_crumb()
    request_args = {"url": url, "params": {**params, **({"crumb": crumb} if crumb is not None else {})},
                    "timeout": timeout}
    if body:
        request_args["json"] = body
    response = request_method(**request_args)
    code = response.status_code
    if code == 429:
        raise YFRateLimitError()
    if code in (401, 403):
        _forget_crumb()
        self._set_cookie_strategy("csrf" if strategy == "basic" else "basic")
        crumb, strategy = self._get_cookie_and_crumb(timeout)
        request_args["params"]["crumb"] = crumb
        response = request_method(**request_args)
        if response.status_code == 429:
            raise YFRateLimitError()
    if response.status_code < 400 and crumb:
        _save_crumb(self, crumb, strategy)
    return response


def _install():
    global _installed
    import yfinance as yf_mod
    try:
        yf_mod.set_tz_cache_location(str(fincept_paths.cache_dir("yahoo", "yfinance")))
    except Exception as e:
        _diag(f"yfinance cache folder not set: {type(e).__name__}")
    from yfinance.data import YfData
    if getattr(YfData, "_make_request", None) is not None and not getattr(YfData._make_request, "_fy", False):
        _make_request._fy = True
        YfData._make_request = _make_request
    data = YfData()
    sess = data._session
    if not getattr(getattr(sess, "request", None), "_fy_guarded", False):
        sess.request = _guarded(sess.request)
    _restore_crumb(data)
    _patch_info_quote()
    _installed = True


def _patch_info_quote():
    """Ticker.info asks the v7 quote endpoint for its symbol on top of quoteSummary: answer
    that part from the quote cache (the same raw row, batched and shared with every other
    quote consumer) so a cold info costs one request less."""
    try:
        from yfinance.scrapers import quote as yq
    except Exception:
        return
    orig = getattr(yq.Quote, "_fetch_additional_info", None)
    if orig is None or getattr(orig, "_fy", False):
        return

    def _fetch_additional_info(self):
        try:
            sym = str(getattr(self, "_symbol", "") or "")
            e = quote_entries([sym]).get(sym.upper()) if sym else None
            if e is not None:
                return {"quoteResponse": {"result": [dict(e["row"])], "error": None}}
            if sym and _quote_miss.get(sym.upper(), 0) > time.time():
                return {"quoteResponse": {"result": [], "error": None}}  # what v7 answers for it
        except Exception:
            pass
        return orig(self)

    _fetch_additional_info._fy = True
    yq.Quote._fetch_additional_info = _fetch_additional_info


def session():
    """yfinance's shared YfData (cookie, crumb, guarded HTTP session) for this process."""
    if not _installed:
        with _install_lock:
            if not _installed:
                _install()
    from yfinance.data import YfData
    data = YfData()
    sess = data._session
    if not getattr(getattr(sess, "request", None), "_fy_guarded", False):  # someone replaced it
        with _install_lock:
            sess = data._session
            if not getattr(getattr(sess, "request", None), "_fy_guarded", False):
                sess.request = _guarded(sess.request)
    return data


def yf():
    """The yfinance module, configured (see the module docstring)."""
    session()
    import yfinance as yf_mod
    return yf_mod


def get_json(url, params=None, timeout=None):
    """GET a Yahoo JSON endpoint through the shared session (crumb added). Raises on HTTP errors."""
    return session().get_raw_json(url, params=params or {}, timeout=timeout or NET_TIMEOUT)


# ════════════════════════════════════════════════════════════════════════════
# Exchange sessions (estimates from the exchange time zone Yahoo reports)
# ════════════════════════════════════════════════════════════════════════════

# IANA zone -> (first state change of the day: pre-open / open, last: close / post-close),
# local "HH:MM", and the weekend (Python weekday numbers, Mon = 0). Holidays are unknown here:
# a closed answer inside these hours is re-checked every CLOSED_RECHECK_S.
_SAT_SUN = (5, 6)
_FRI_SAT = (4, 5)
_SESSIONS = {
    "America/New_York": ("04:00", "20:00", _SAT_SUN), "America/Chicago": ("03:00", "19:00", _SAT_SUN),
    "America/Toronto": ("09:30", "16:00", _SAT_SUN), "America/Sao_Paulo": ("10:00", "18:00", _SAT_SUN),
    "America/Mexico_City": ("08:30", "15:00", _SAT_SUN), "America/Santiago": ("09:30", "16:00", _SAT_SUN),
    "America/Argentina/Buenos_Aires": ("11:00", "17:00", _SAT_SUN), "America/Lima": ("09:00", "16:00", _SAT_SUN),
    "Europe/London": ("07:50", "16:35", _SAT_SUN), "Europe/Dublin": ("08:00", "16:30", _SAT_SUN),
    "Europe/Berlin": ("08:00", "22:00", _SAT_SUN), "Europe/Paris": ("09:00", "17:35", _SAT_SUN),
    "Europe/Amsterdam": ("09:00", "17:35", _SAT_SUN), "Europe/Brussels": ("09:00", "17:35", _SAT_SUN),
    "Europe/Lisbon": ("08:00", "16:35", _SAT_SUN), "Europe/Madrid": ("09:00", "17:35", _SAT_SUN),
    "Europe/Rome": ("09:00", "17:35", _SAT_SUN), "Europe/Milan": ("09:00", "17:35", _SAT_SUN),
    "Europe/Zurich": ("09:00", "17:30", _SAT_SUN), "Europe/Vienna": ("09:00", "17:35", _SAT_SUN),
    "Europe/Stockholm": ("09:00", "17:30", _SAT_SUN), "Europe/Oslo": ("09:00", "16:25", _SAT_SUN),
    "Europe/Copenhagen": ("09:00", "17:00", _SAT_SUN), "Europe/Helsinki": ("10:00", "18:30", _SAT_SUN),
    "Europe/Warsaw": ("08:30", "17:05", _SAT_SUN), "Europe/Prague": ("09:00", "16:25", _SAT_SUN),
    "Europe/Athens": ("10:00", "17:20", _SAT_SUN), "Europe/Istanbul": ("09:40", "18:10", _SAT_SUN),
    "Europe/Moscow": ("09:50", "18:50", _SAT_SUN), "Europe/Budapest": ("09:00", "17:10", _SAT_SUN),
    "Asia/Kolkata": ("09:00", "16:00", _SAT_SUN), "Asia/Calcutta": ("09:00", "16:00", _SAT_SUN),
    "Asia/Tokyo": ("09:00", "15:30", _SAT_SUN), "Asia/Hong_Kong": ("09:00", "16:10", _SAT_SUN),
    "Asia/Shanghai": ("09:15", "15:00", _SAT_SUN), "Asia/Seoul": ("08:30", "15:30", _SAT_SUN),
    "Asia/Taipei": ("08:30", "13:30", _SAT_SUN), "Asia/Singapore": ("08:30", "17:16", _SAT_SUN),
    "Asia/Jakarta": ("08:45", "16:15", _SAT_SUN), "Asia/Bangkok": ("09:30", "16:40", _SAT_SUN),
    "Asia/Kuala_Lumpur": ("08:30", "17:00", _SAT_SUN), "Asia/Manila": ("09:00", "15:00", _SAT_SUN),
    "Asia/Karachi": ("09:15", "15:30", _SAT_SUN), "Asia/Ho_Chi_Minh": ("09:00", "15:00", _SAT_SUN),
    "Asia/Riyadh": ("09:30", "15:20", _FRI_SAT), "Asia/Dubai": ("09:30", "15:00", _SAT_SUN),
    "Asia/Qatar": ("09:00", "13:30", _FRI_SAT), "Asia/Jerusalem": ("09:30", "17:30", _SAT_SUN),
    "Australia/Sydney": ("10:00", "16:15", _SAT_SUN), "Pacific/Auckland": ("10:00", "16:45", _SAT_SUN),
    "Africa/Johannesburg": ("08:30", "17:00", _SAT_SUN), "Africa/Cairo": ("10:00", "14:30", _FRI_SAT),
    "Africa/Lagos": ("09:30", "14:30", _SAT_SUN),
}
# US indices / funds print only in the regular session (no pre-market state).
_NY_REGULAR = ("09:30", "16:00", _SAT_SUN)


def _zone(tz_name):
    try:
        from zoneinfo import ZoneInfo
        return ZoneInfo(str(tz_name))
    except Exception:
        return None


def _hm(s):
    h, m = s.split(":")
    return int(h), int(m)


def _schedule(tz_name, quote_type):
    qt = str(quote_type or "").upper()
    if qt in ("INDEX", "MUTUALFUND") and tz_name == "America/New_York":
        return _NY_REGULAR
    return _SESSIONS.get(str(tz_name or ""))


def _at(day, hm, tz):
    h, m = _hm(hm)
    return datetime(day.year, day.month, day.day, h, m, tzinfo=tz).timestamp()


def next_open(tz_name, quote_type, t):
    """Estimated epoch of the next session start after `t` for an instrument quoted in the
    IANA zone tz_name (None when unknown). FX: Sunday 17:00 New York; futures: 18:00 New York
    (Sunday for the weekend, otherwise after the daily break)."""
    qt = str(quote_type or "").upper()
    if qt in ("CRYPTOCURRENCY",):
        return t
    if qt in ("CURRENCY", "FUTURE"):
        ny = _zone("America/New_York")
        if ny is None:
            return None
        local = datetime.fromtimestamp(t, ny)
        hm = "17:00" if qt == "CURRENCY" else "18:00"
        for i in range(0, 8):
            d = (local + timedelta(days=i)).date()
            wd = d.weekday()
            if wd in (4, 5):  # FX and futures do not open on Friday / Saturday evening
                continue
            ts = _at(d, hm, ny)
            if ts > t:
                return ts
        return None
    sched = _schedule(tz_name, qt)
    tz = _zone(tz_name)
    if sched is None or tz is None:
        return None
    o, _c, weekend = sched
    local = datetime.fromtimestamp(t, tz)
    for i in range(0, 10):
        d = (local + timedelta(days=i)).date()
        if d.weekday() in weekend:
            continue
        ts = _at(d, o, tz)
        if ts > t:
            return ts
    return None


def in_session_hours(tz_name, quote_type, t):
    """True / False when `t` is inside / outside the expected session hours, None if unknown."""
    qt = str(quote_type or "").upper()
    if qt == "CRYPTOCURRENCY":
        return True
    if qt in ("CURRENCY", "FUTURE"):
        ny = _zone("America/New_York")
        if ny is None:
            return None
        local = datetime.fromtimestamp(t, ny)
        wd, hh = local.weekday(), local.hour + local.minute / 60.0
        if wd == 5:
            return False
        if wd == 4 and hh >= 17:
            return False
        if wd == 6 and hh < (17 if qt == "CURRENCY" else 18):
            return False
        if qt == "FUTURE" and 17 <= hh < 18:
            return False
        return True
    sched = _schedule(tz_name, qt)
    tz = _zone(tz_name)
    if sched is None or tz is None:
        return None
    o, c, weekend = sched
    local = datetime.fromtimestamp(t, tz)
    if local.date().weekday() in weekend:
        return False
    return _at(local.date(), o, tz) <= t < _at(local.date(), c, tz)


def last_close(tz_name, quote_type, t):
    """Estimated epoch of the most recent session end at or before `t` (None when unknown)."""
    qt = str(quote_type or "").upper()
    if qt in ("CRYPTOCURRENCY", "CURRENCY", "FUTURE"):
        return None
    sched = _schedule(tz_name, qt)
    tz = _zone(tz_name)
    if sched is None or tz is None:
        return None
    _o, c, weekend = sched
    local = datetime.fromtimestamp(t, tz)
    for i in range(0, 10):
        d = (local - timedelta(days=i)).date()
        if d.weekday() in weekend:
            continue
        ts = _at(d, c, tz)
        if ts <= t:
            return ts
    return None


def quote_expires(row, fetched):
    """Epoch after which a cached v7 quote row must be fetched again."""
    qt = str(row.get("quoteType") or "").upper()
    state = str(row.get("marketState") or "").upper()
    if qt == "CRYPTOCURRENCY" or state in ("REGULAR", "PRE", "POST", ""):
        return fetched + QUOTE_LIVE_TTL_S
    tz = row.get("exchangeTimezoneName")
    # Just after the close a delayed feed (15-20 min on most non-US venues) still moves the
    # last price: keep re-checking every minute until delay + 20 min after the last trade.
    try:
        delay_min = max(0.0, float(row.get("exchangeDataDelayedBy") or 0))
        last_trade = float(row.get("regularMarketTime") or 0)
    except (TypeError, ValueError):
        delay_min, last_trade = 0.0, 0.0
    if last_trade and fetched - last_trade < (delay_min + 20) * 60:
        return fetched + max(QUOTE_LIVE_TTL_S, 60)
    inside = in_session_hours(tz, qt, fetched)
    if inside:  # closed while it should trade: lunch break, holiday, late open
        return fetched + CLOSED_RECHECK_S
    nxt = next_open(tz, qt, fetched)
    if nxt is None:
        return fetched + CLOSED_RECHECK_S
    return min(max(nxt, fetched + QUOTE_LIVE_TTL_S), fetched + CLOSED_MAX_S)


# ════════════════════════════════════════════════════════════════════════════
# JSON payload cache (memory LRU + one file per key, shared by every process)
# ════════════════════════════════════════════════════════════════════════════

_json_mem = collections.OrderedDict()   # (kind, key) -> (entry dict, approx bytes)
_json_mem_bytes = 0
_json_lock = threading.RLock()
_json_writes = 0
_inflight = {}                          # (kind, key) -> threading.Event
_inflight_lock = threading.Lock()


def _safe_name(key):
    s = re.sub(r"[^A-Za-z0-9.\-^=_]", lambda m: "_%02X" % ord(m.group(0)), str(key).upper())[:120]
    if s.split(".")[0] in ("CON", "PRN", "AUX", "NUL") or re.fullmatch(r"(COM|LPT)\d", s.split(".")[0]):
        s = "_" + s
    if len(str(key)) > 100:
        s += "_" + hashlib.sha1(str(key).encode("utf-8")).hexdigest()[:10]
    return s


def _json_path(kind, key):
    return os.path.join(fincept_paths.cache_dir("yahoo", "json", kind), _safe_name(key) + ".json")


def _mem_put(k, entry, size):
    global _json_mem_bytes
    with _json_lock:
        old = _json_mem.pop(k, None)
        if old:
            _json_mem_bytes -= old[1]
        _json_mem[k] = (entry, size)
        _json_mem_bytes += size
        while _json_mem and (len(_json_mem) > _JSON_MEM_MAX or _json_mem_bytes > _JSON_MEM_MAX_BYTES):
            _, (_e, sz) = _json_mem.popitem(last=False)
            _json_mem_bytes -= sz


def _entry_fresh(entry, max_age, now):
    if not isinstance(entry, dict):
        return False
    age = now - float(entry.get("fetched", 0) or 0)
    if entry.get("negative"):
        return age < float(entry.get("ttl", 3600))
    if entry.get("partial"):
        return age < min(float(entry.get("retry", 3600)), max_age)
    return age < max_age


def _get_entry(kind, key, max_age):
    """(entry, source) of a fresh cached entry, else (None or stale entry, None)."""
    now = time.time()
    k = (kind, str(key).upper())
    with _json_lock:
        hit = _json_mem.get(k)
        if hit and _entry_fresh(hit[0], max_age, now):
            _json_mem.move_to_end(k)
            return hit[0], "memory"
    p = _json_path(kind, key)
    entry = _read_json(p)
    if isinstance(entry, dict):
        try:
            size = os.path.getsize(p)
        except OSError:
            size = 1024
        _mem_put(k, entry, size)
        if _entry_fresh(entry, max_age, now):
            try:
                os.utime(p, None)  # least-recent-use clock for the size cap
            except OSError:
                pass
            return entry, "disk"
        return entry, None
    return None, None


def _put_entry(kind, key, entry):
    global _json_writes
    text = _dumps(entry)
    p = _json_path(kind, key)
    _atomic_write(p, text)
    _mem_put((kind, str(key).upper()), entry, len(text))
    _json_writes += 1
    if _json_writes % 200 == 1:
        _enforce_json_cap()


def _enforce_json_cap():
    root = os.path.join(cache_root(), "json")
    files = []
    total = 0
    for dirpath, _dirs, names in os.walk(root):
        for n in names:
            p = os.path.join(dirpath, n)
            try:
                st = os.stat(p)
            except OSError:
                continue
            files.append((st.st_mtime, p, st.st_size))
            total += st.st_size
    if total <= CACHE_MAX_BYTES:
        return
    files.sort()
    target = 0.9 * CACHE_MAX_BYTES
    for _m, p, sz in files:
        if total <= target:
            break
        try:
            os.remove(p)
            total -= sz
        except OSError:
            pass


def recall(kind, key, max_age):
    """The cached value for (kind, key) if younger than max_age seconds, else None."""
    entry, src = _get_entry(kind, key, max_age)
    if src and isinstance(entry, dict) and not entry.get("negative"):
        _count("served_memory" if src == "memory" else "served_disk")
        return entry.get("value")
    return None


def remember(kind, key, value):
    """Store a JSON-able value for (kind, key) now (read back with recall / peek / cached)."""
    _put_entry(kind, key, {"fetched": time.time(), "value": value})


def peek(kind, key):
    """The cached value for (kind, key) whatever its age, or None (for "show saved data")."""
    entry, _src = _get_entry(kind, key, float("inf"))
    if isinstance(entry, dict) and not entry.get("negative"):
        return entry.get("value")
    return None


def cached(kind, key, fetch, max_age=INFO_TTL_S, stale_on_error=True):
    """fetch() at most once per max_age per (kind, key) across threads and processes. Keys are
    case-insensitive (hash a key whose case matters).

    fetch() returns the value (JSON-able), Partial(value, retry_s) or Negative(msg, ttl_s).
    Exceptions propagate and nothing is cached; with stale_on_error an older cached value is
    returned instead when the failure is a rate limit / network error. A key negatively
    cached raises NotAvailable(message)."""
    entry, src = _get_entry(kind, key, max_age)
    if src:
        _count("served_memory" if src == "memory" else "served_disk")
        if entry.get("negative"):
            raise NotAvailable(entry.get("message") or "")
        return entry.get("value")
    k = (kind, str(key).upper())
    # Coalesce identical requests: threads of this process wait on an event, other
    # processes on the key's lock file; both re-check the cache afterwards.
    with _inflight_lock:
        ev = _inflight.get(k)
        owner = ev is None
        if owner:
            ev = threading.Event()
            _inflight[k] = ev
    if not owner:
        ev.wait(timeout=120)
        entry, src = _get_entry(kind, key, max_age)
        if src:
            _count("merged")
            if entry.get("negative"):
                raise NotAvailable(entry.get("message") or "")
            return entry.get("value")
        return cached(kind, key, fetch, max_age, stale_on_error)
    try:
        lock = _FileLock(_json_path(kind, key) + ".lock", timeout=120)
        got = lock.acquire()
        try:
            entry2, src2 = _get_entry(kind, key, max_age)
            if src2:
                _count("merged")
                if entry2.get("negative"):
                    raise NotAvailable(entry2.get("message") or "")
                return entry2.get("value")
            stale = entry2 if entry2 is not None else entry
            try:
                value = fetch()
            except NotAvailable:
                raise
            except Exception as e:
                if stale_on_error and isinstance(stale, dict) and not stale.get("negative") \
                        and _transient(e):
                    _count("served_disk")
                    return stale.get("value")
                raise
            now = time.time()
            if isinstance(value, Negative):
                _put_entry(kind, key, {"fetched": now, "negative": True, "ttl": value.ttl_s,
                                       "message": value.message})
                raise NotAvailable(value.message)
            if isinstance(value, Partial):
                _put_entry(kind, key, {"fetched": now, "partial": True, "retry": value.retry_s,
                                       "value": value.value})
                return value.value
            _put_entry(kind, key, {"fetched": now, "value": value})
            return value
        finally:
            if got:
                lock.release()
    finally:
        with _inflight_lock:
            _inflight.pop(k, None)
        ev.set()


def _transient(e):
    if isinstance(e, YahooRateLimited):
        return True
    names = {c.__name__ for c in type(e).__mro__}
    if names & {"YFRateLimitError", "ConnectionError", "Timeout", "ReadTimeout", "ConnectTimeout",
                "RequestException", "CurlError", "SSLError", "ProxyError", "TimeoutError", "OSError"}:
        return True
    text = str(e).lower()
    return any(s in text for s in ("too many requests", "rate limit", "timed out", "timeout", "connection",
                                   "curl: (", "could not resolve", "temporarily unavailable"))


# ════════════════════════════════════════════════════════════════════════════
# Quotes (Yahoo v7, batched)
# ════════════════════════════════════════════════════════════════════════════

_quote_lock = threading.RLock()
_quote_mem = {}            # SYMBOL -> {"row": dict, "t": fetched, "src": "network"|"disk"}
_quote_miss = {}           # SYMBOL -> epoch until which "v7 does not know it" holds
_quote_disk_loaded = False
_quote_dirty = False
_quote_last_flush = 0.0
_USD_FX_PAIR_RE = re.compile(r"^(USD)?([A-Z]{3})=X$")


def _quotes_path():
    return os.path.join(cache_root(), "quotes.json")


def _load_quote_snapshot():
    global _quote_disk_loaded
    if _quote_disk_loaded:
        return
    _quote_disk_loaded = True
    snap = _read_json(_quotes_path())
    if not isinstance(snap, dict):
        return
    for sym, e in (snap.get("rows") or {}).items():
        if isinstance(e, dict) and isinstance(e.get("row"), dict):
            cur = _quote_mem.get(sym)
            if cur is None or float(cur.get("t", 0)) < float(e.get("t", 0) or 0):
                _quote_mem[sym] = {"row": e["row"], "t": float(e.get("t", 0) or 0), "src": "disk"}
    now = time.time()
    for sym, until in (snap.get("miss") or {}).items():
        try:
            if float(until) > now:
                _quote_miss[sym] = float(until)
        except (TypeError, ValueError):
            pass


def flush_quotes(force=False):
    """Write the quote snapshot (merged with other processes' rows, newest wins)."""
    global _quote_dirty, _quote_last_flush
    with _quote_lock:
        if not _quote_dirty or (not force and time.time() - _quote_last_flush < 20):
            return
        mine = {s: {"row": e["row"], "t": e["t"]} for s, e in _quote_mem.items()}
        miss = dict(_quote_miss)
        _quote_dirty = False
        _quote_last_flush = time.time()
    try:
        with _FileLock(_quotes_path() + ".lock", timeout=10):
            snap = _read_json(_quotes_path())
            rows = (snap.get("rows") if isinstance(snap, dict) else None) or {}
            for s, e in mine.items():
                cur = rows.get(s)
                if not isinstance(cur, dict) or float(cur.get("t", 0) or 0) < e["t"]:
                    rows[s] = e
            if len(rows) > _QUOTE_DISK_MAX:
                keep = sorted(rows.items(), key=lambda kv: -float(kv[1].get("t", 0) or 0))[:_QUOTE_DISK_MAX]
                rows = dict(keep)
            now = time.time()
            old_miss = (snap.get("miss") if isinstance(snap, dict) else None) or {}
            merged_miss = {s: u for s, u in {**old_miss, **miss}.items()
                           if isinstance(u, (int, float)) and u > now}
            _atomic_write(_quotes_path(), _dumps({"v": 1, "rows": rows, "miss": merged_miss}))
    except Exception as e:
        _diag(f"quote snapshot not written: {type(e).__name__}")


atexit.register(lambda: flush_quotes(force=True))


def _alias(sym):
    m = _USD_FX_PAIR_RE.match(sym)
    if not m:
        return None
    return (m.group(2) if m.group(1) else "USD" + m.group(2)) + "=X"


_tls = threading.local()


def last_quote_error():
    """The exception that made the calling thread's last quote_entries() / quotes() call miss
    rows (a failed request), or None when every needed request succeeded."""
    return getattr(_tls, "quote_error", None)


_tz_known = set()      # symbols whose time zone yfinance's tz cache already holds


def _tz_cache():
    if not _installed:
        return None
    try:
        from yfinance import cache as ycache
        return ycache.get_tz_cache()
    except Exception:
        return None


def _seed_tz(rows):
    """Store the exchange time zone of quoted symbols in yfinance's tz cache: its first history
    download of a symbol otherwise sends a separate 1-day chart request just to learn it."""
    c = None
    for row in rows:
        sym = str(row.get("symbol") or "").upper()
        tz = row.get("exchangeTimezoneName")
        if not sym or not tz or sym in _tz_known:
            continue
        if c is None:
            c = _tz_cache()
            if c is None:
                return
        try:
            if not c.lookup(sym):
                c.store(sym, tz)
            _tz_known.add(sym)
        except Exception:
            return


def ensure_tz(symbols):
    """Make sure yfinance knows the time zone of `symbols` before their first history download:
    the ones its tz cache lacks are quoted in ONE batched request (whose rows carry the zone)
    instead of one 1-day chart probe each."""
    session()  # the tz cache must be opened in OUR folder, never yfinance's default one
    c = _tz_cache()
    if c is None:
        return
    missing = []
    for s in symbols:
        u = str(s or "").strip().upper()
        if not u or u in _tz_known:
            continue
        try:
            if c.lookup(u):
                _tz_known.add(u)
                continue
        except Exception:
            return
        missing.append(u)
    if missing:
        try:
            quote_entries(missing)
        except Exception:
            pass


def quote_entries(symbols, refresh=False):
    """{SYMBOL (upper): {"row", "t", "src"}} for the symbols Yahoo's v7 quote knows - from the
    cache when fresh, else in batched requests. Raises nothing for a failed chunk (those symbols
    are absent, or served from an older cached row when Yahoo throttles / is unreachable);
    `refresh` skips the freshness check for live rows only (closed markets still answer from cache)."""
    syms = list(dict.fromkeys(str(s).strip().upper() for s in symbols if s and str(s).strip()))
    out = {}
    _tls.quote_error = None
    if not syms:
        return out
    global _quote_dirty
    now = time.time()
    need = []
    with _quote_lock:
        _load_quote_snapshot()
        for s in syms:
            e = _quote_mem.get(s)
            if e is not None:
                exp = quote_expires(e["row"], e["t"])
                live = exp - e["t"] <= QUOTE_LIVE_TTL_S + 1
                if now < exp and not (refresh and live):
                    out[s] = dict(e, src="disk" if e.get("src") == "disk" else "memory")
                    _count("served_memory" if e.get("src") != "disk" else "served_disk")
                    continue
            if _quote_miss.get(s, 0) > now and not refresh:
                _count("served_memory")
                continue
            need.append(s)
    if not need:
        return out
    failed = False
    session()
    for i in range(0, len(need), _V7_CHUNK):
        chunk = need[i:i + _V7_CHUNK]
        try:
            res = get_json(_V7_URL, {"symbols": ",".join(chunk), "formatted": "false"})
        except Exception as e:
            failed = True
            _tls.quote_error = e
            if _transient(e):
                for s in need[i:]:  # serve older rows rather than nothing
                    old = _quote_mem.get(s)
                    if old is not None:
                        out[s] = dict(old, src="stale")  # its "t" still says how old it is
                break
            continue
        result = ((res or {}).get("quoteResponse") or {}).get("result") or []
        _seed_tz([r for r in result if isinstance(r, dict)])
        t = time.time()
        got = set()
        with _quote_lock:
            for row in result:
                if isinstance(row, dict) and row.get("symbol"):
                    s = str(row["symbol"]).upper()
                    e = {"row": row, "t": t, "src": "network"}
                    _quote_mem[s] = e
                    got.add(s)
                    if s in chunk:
                        out[s] = e
            for s in chunk:
                if s not in got:
                    a = _alias(s)
                    if a and a in got:
                        _quote_mem[s] = _quote_mem[a]  # the other spelling of a USD-base FX pair
                        out[s] = _quote_mem[a]
                    else:
                        _quote_miss[s] = t + 900  # unknown to v7: ask again in 15 min
            if len(_quote_mem) > _QUOTE_MEM_MAX:
                for s, _e in sorted(_quote_mem.items(), key=lambda kv: kv[1]["t"])[:len(_quote_mem) - _QUOTE_MEM_MAX]:
                    _quote_mem.pop(s, None)
            _quote_dirty = True
    if not failed:
        flush_quotes()
    return out


def quotes(symbols, refresh=False):
    """{symbol as given: v7 row + "_fetched" + "_served"} (see the module docstring)."""
    ents = quote_entries(symbols, refresh=refresh)
    out = {}
    for s in symbols:
        e = ents.get(str(s).strip().upper())
        if e is None:
            a = _alias(str(s).strip().upper())
            e = ents.get(a) if a else None
        if e is not None:
            row = dict(e["row"])
            row["_fetched"] = int(e["t"])
            row["_served"] = e.get("src") or "network"
            out[s] = row
    return out


def fx_rate(base, quote):
    """Spot rate converting `base` amounts into `quote` (EUR -> USD: EURUSD=X), from the quote
    cache. None when unknown - never 1.0 unless the currencies are the same."""
    b, q = str(base or "").strip().upper(), str(quote or "").strip().upper()
    if not b or not q:
        return None
    if b == q:
        return 1.0
    sym = f"{b}{q}=X"
    row = quotes([sym]).get(sym)
    try:
        px = float((row or {}).get("regularMarketPrice"))
        if math.isfinite(px) and px > 0:
            return px
    except (TypeError, ValueError):
        pass
    return None


# ════════════════════════════════════════════════════════════════════════════
# Fundamentals
# ════════════════════════════════════════════════════════════════════════════

def _info_has_data(info):
    if not info:
        return False
    if any(str(info.get(k) or "").strip() for k in ("longName", "shortName", "displayName")):
        return True
    for k in ("currentPrice", "regularMarketPrice", "previousClose", "marketCap", "navPrice"):
        try:
            if float(info.get(k)) > 0:
                return True
        except (TypeError, ValueError):
            continue
    return False


@contextlib.contextmanager
def _yf_errors():
    """Collect the ERROR lines yfinance logs on THIS thread during the block."""
    import logging
    me = threading.get_ident()
    sink = []

    class _H(logging.Handler):
        def emit(self, record):
            if record.levelno >= logging.ERROR and record.thread == me:
                try:
                    sink.append(record.getMessage())
                except Exception:
                    pass

    h = _H(level=logging.ERROR)
    lg = logging.getLogger("yfinance")
    lg.addHandler(h)
    try:
        yield sink
    finally:
        lg.removeHandler(h)


def info(symbol, max_age=None):
    """yf.Ticker(symbol).info as a dict, fetched at most once per INFO_TTL_S per symbol by any
    script / process. {} when Yahoo has no such instrument (remembered for an hour). Raises on
    rate limits / network errors when nothing is cached (an older copy is returned instead)."""
    sym = str(symbol or "").strip()
    if not sym:
        return {}

    def fetch():
        yfm = yf()
        with _yf_errors() as errs:
            val = dict(yfm.Ticker(sym).info or {})
        if not _info_has_data(val):
            # yfinance swallows HTTP errors here: only a "not found" answer is remembered.
            if any(re.search(r"\b5\d\d\b|timed? ?out|curl|connection", e, re.I) for e in errs):
                raise ConnectionError(f"Yahoo Finance did not answer for {sym}: {errs[-1][:120]}")
            import logging
            if not logging.getLogger("yfinance").isEnabledFor(logging.ERROR):
                # Someone muted yfinance's log: a failed request cannot be told from "not
                # found", so the empty answer is remembered only briefly.
                return Negative(f"Yahoo Finance has no data for {sym}", 300)
            return Negative(f"Yahoo Finance has no data for {sym}", 3600)
        return val

    try:
        return cached("info", sym, fetch, max_age=INFO_TTL_S if max_age is None else max_age)
    except NotAvailable:
        return {}


# ════════════════════════════════════════════════════════════════════════════
# Search
# ════════════════════════════════════════════════════════════════════════════

def search_raw(query, limit=10, news_count=0, timeout=5.0):
    """Yahoo's search endpoint (raw JSON), cached for a day per (query, limit, news)."""
    q = " ".join(str(query or "").split())
    key = f"{q}|{int(limit)}|{int(news_count)}"

    def fetch():
        session()
        params = {"q": q, "quotesCount": int(limit), "newsCount": int(news_count), "enableFuzzyQuery": "false",
                  "quotesQueryId": "tss_match_phrase_query"}
        if news_count:
            params["newsQueryId"] = "news_cie_vespa"
        r = session()._session.get(_SEARCH_URL, params=params, timeout=timeout)
        if r.status_code == 429:
            from yfinance.exceptions import YFRateLimitError
            raise YFRateLimitError()
        r.raise_for_status()
        return r.json()

    return cached("search", key, fetch, max_age=86400 if not news_count else 600)


def search(query, limit=10):
    """Search hits: [{"symbol","name","exchange","type","currency","industry"}] (cached a day)."""
    data = search_raw(query, limit) or {}
    out = []
    for q in data.get("quotes", []) or []:
        sym = q.get("symbol", "")
        if not sym:
            continue
        out.append({"symbol": sym, "name": q.get("longname") or q.get("shortname", ""),
                    "exchange": q.get("exchange", ""), "type": q.get("quoteType", "EQUITY"),
                    "currency": q.get("currency", "USD"), "industry": q.get("industry", "")})
    return out


# ════════════════════════════════════════════════════════════════════════════
# Bars: the shared on-disk bar cache (backtesting price store)
# ════════════════════════════════════════════════════════════════════════════

_store_obj = None
_store_lock = threading.Lock()
_BAR_SECONDS = {"1m": 60, "2m": 120, "5m": 300, "15m": 900, "30m": 1800, "60m": 3600, "90m": 5400,
                "1h": 3600, "4h": 3600}
# How far back the FIRST download of a symbol reaches, so later longer ranges are slices:
# daily bars 10 years (one request either way); intraday as far as Yahoo serves in one go.
_MIN_SPAN_DAYS = {"1d": 3660, "5d": 3660, "1wk": 3660, "1mo": 7320, "3mo": 7320,
                  "1m": 7, "2m": 59, "5m": 59, "15m": 59, "30m": 59, "90m": 59, "60m": 365, "1h": 365, "4h": 365}
try:
    _MIN_SPAN_SCALE = float(os.environ.get("FINCEPT_YAHOO_HISTORY_SPAN_SCALE", "1"))
except (TypeError, ValueError):
    _MIN_SPAN_SCALE = 1.0


def _price_store_module():
    bt = os.path.join(_HERE, "Analytics", "backtesting")
    if bt not in sys.path:
        sys.path.append(bt)
    from base import price_store
    return price_store


_bar_tls = threading.local()


def bar_ttl(meta, interval, now=None):
    """Seconds a cached bar entry stays fresh (the price store's freshness hook). A caller's
    max_age (bars / download / ticker_history / frame) only ever lengthens it."""
    floor = getattr(_bar_tls, "max_age", None) or 0
    return max(floor, _bar_ttl_policy(meta, interval, now))


@contextlib.contextmanager
def _bar_max_age(max_age):
    prev = getattr(_bar_tls, "max_age", None)
    if max_age is not None:
        _bar_tls.max_age = max_age
    try:
        yield
    finally:
        _bar_tls.max_age = prev


def _bar_ttl_policy(meta, interval, now=None):
    now = time.time() if now is None else now
    iv = str(interval)
    fetched = float(meta.get("fetchedTs", 0) or 0)
    tz = meta.get("exchangeTz")
    qt = meta.get("instrumentType")
    inside = in_session_hours(tz, qt, now)
    if iv in _BAR_SECONDS:
        live = min(max(_BAR_SECONDS[iv], 60), 900)
    else:
        live = DAILY_OPEN_TTL_S
    if inside is None or inside:
        return live
    # Closed now: fresh until the next open once it was fetched after the last session end
    # (+30 min for Yahoo to settle the final bar).
    lc = last_close(tz, qt, now)
    if lc is None:  # FX / futures weekend
        return max(live, now - fetched + 60) if fetched and not in_session_hours(tz, qt, fetched) else live
    if fetched >= lc + 1800:
        nxt = next_open(tz, qt, now) or (now + CLOSED_RECHECK_S)
        return max(live, nxt - fetched)
    return live


def store():
    """The process's price store (shared files with backtesting), with this module's freshness."""
    global _store_obj
    if _store_obj is not None:
        return _store_obj
    with _store_lock:
        if _store_obj is None:
            ps = _price_store_module()

            class _Store(ps.PriceStore):
                def ttl_for(self, meta, interval):
                    return bar_ttl(meta, interval)

                def fetch_start_for(self, symbol, interval, start):
                    span = _MIN_SPAN_DAYS.get(interval)
                    if not span:
                        return start
                    floor = self.clock().date() - timedelta(days=int(span * _MIN_SPAN_SCALE))
                    return min(start, floor)

            _store_obj = _Store()
    return _store_obj


def bars(symbols, start, end, interval="1d", adjust="split+dividend", refresh=False, max_age=None):
    """price_store.get_bars through this module's store (see the module docstring).
    max_age (seconds): accept cached bars up to this old even while the market trades (e.g.
    86400 for one-year analytics that do not need today's moving bar)."""
    session()
    if isinstance(symbols, str):
        symbols = [symbols]
    ensure_tz(symbols)
    with _bar_max_age(max_age):
        return store().get(list(symbols), start, end, interval=interval, adjust=adjust, refresh=refresh,
                           progress=False)


def bar_fetched(symbol, interval="1d"):
    """Epoch seconds the cached bars of `symbol` were last fetched from Yahoo, or None."""
    try:
        ps = _price_store_module()
        e = store().read_entry(ps.normalize_symbol(symbol), interval, touch=False)
        return float(e.meta.get("fetchedTs")) if e is not None and e.meta.get("fetchedTs") else None
    except Exception:
        return None


_PERIOD_RE = re.compile(r"^(\d+)(d|wk|mo|y)$")


def period_start(period, interval="1d", today=None):
    """(start date, session count) for a Yahoo period string: '5d' counts trading sessions
    (count is set, start reaches back far enough), 'Nwk'/'Nmo'/'Ny' are calendar spans,
    'ytd' Jan 1, 'max' 1900-01-01."""
    today = today or date.today()
    p = str(period or "1mo").strip().lower()
    if p == "max":
        return date(1900, 1, 1), None
    if p == "ytd":
        return date(today.year, 1, 1), None
    m = _PERIOD_RE.match(p)
    if not m:
        raise ValueError(f"invalid period {period!r}")
    n, unit = int(m.group(1)), m.group(2)
    if unit == "d":
        return today - timedelta(days=int(n * 7 / 5) + 10), n
    if unit == "wk":
        return today - timedelta(days=7 * n), None
    if unit == "mo":
        y, mo = today.year, today.month - n
        while mo <= 0:
            mo += 12
            y -= 1
        day = min(today.day, 28)
        return date(y, mo, day), None
    return date(today.year - n, today.month, min(today.day, 28)), None


def _localize(idx, tz_name):
    """tz-naive exchange wall times -> tz-aware (exchange zone; UTC when unknown)."""
    import pandas as pd
    tz = tz_name or "UTC"
    try:
        return idx.tz_localize(tz, ambiguous="NaT", nonexistent="shift_forward")
    except Exception:
        return idx.tz_localize("UTC")


def frame(symbol, period=None, interval="1d", start=None, end=None, adjust="split+dividend", max_age=None):
    """(DataFrame, provenance) of one symbol from the bar cache: tz-naive exchange wall-time
    index; columns Open High Low Close Volume (+ Dividends Splits for adjust='none').
    Raises price_store.PriceDataError."""
    ps = _price_store_module()
    today = datetime.now(timezone.utc).date()
    sessions = None
    if start is None:
        s, sessions = period_start(period or "1mo", interval, today)
    else:
        s = start if isinstance(start, date) else date.fromisoformat(str(start)[:10])
    e = today if end is None else (end if isinstance(end, date) else date.fromisoformat(str(end)[:10]))
    if e < s:
        e = s
    res = bars([symbol], s, e, interval=interval, adjust=adjust, max_age=max_age)
    sym = ps.normalize_symbol(symbol)
    if sym in res.errors:
        raise res.errors[sym]
    df = res.frames[sym]
    prov = res.provenance[sym]
    if sessions:
        days = df.index.normalize().unique()
        if len(days) > sessions:
            df = df[df.index >= days[-sessions]]
    return df, prov


def ticker_history(symbol, period=None, interval="1d", start=None, end=None, auto_adjust=True, actions=True,
                   max_age=None, **_ignored):
    """A drop-in for yf.Ticker(symbol).history(...) from the bar cache. Empty frame when Yahoo
    has nothing (yfinance's lenient behaviour); rate limits / network errors raise. max_age:
    see bars()."""
    with _bar_max_age(max_age):
        return _ticker_history(symbol, period, interval, start, end, auto_adjust, actions)


def _ticker_history(symbol, period, interval, start, end, auto_adjust, actions):
    import pandas as pd
    ps = _price_store_module()
    if start is not None and end is not None:
        end = (pd.Timestamp(end) - pd.Timedelta(days=1)).date()  # yfinance's end is exclusive
    try:
        if auto_adjust:
            df, prov = frame(symbol, period, interval, start, end, "split+dividend")
            out = df[["Open", "High", "Low", "Close", "Volume"]].copy()
        else:
            df, prov = frame(symbol, period, interval, start, end, "split")
            adj, _p = frame(symbol, period, interval, start, end, "split+dividend")
            out = df[["Open", "High", "Low", "Close"]].copy()
            out["Adj Close"] = adj["Close"].reindex(out.index)
            out["Volume"] = df["Volume"]
        if actions:
            raw, _p2 = frame(symbol, period, interval, start, end, "none")
            div, spl = _split_adjusted_actions(raw)
            out["Dividends"] = div.reindex(out.index).fillna(0.0)
            out["Stock Splits"] = spl.reindex(out.index).fillna(0.0)
    except ps.PriceDataError as e:
        if e.code in ("RATE_LIMITED", "NETWORK"):
            raise
        return pd.DataFrame(columns=["Open", "High", "Low", "Close", "Volume"])
    idx = _localize(out.index, prov.get("exchangeTz"))
    out.index = idx
    out = out[~out.index.isna()]
    out.index.name = "Datetime" if ps.is_intraday(interval) else "Date"
    return out


def _split_adjusted_actions(raw):
    """(dividends per share in today's split-adjusted units, split ratio with 0 = none) from an
    adjust='none' frame (yfinance's Dividends / Stock Splits convention)."""
    import numpy as np
    spl = raw["Splits"].astype(float).to_numpy()
    after = np.ones(len(spl))
    if len(spl) > 1:
        after[:-1] = np.cumprod(spl[::-1])[::-1][1:]
    div = raw["Dividends"].astype(float) / after
    splits = raw["Splits"].astype(float).where(raw["Splits"].astype(float) != 1.0, 0.0)
    return div, splits


def download(tickers, start=None, end=None, period=None, interval="1d", auto_adjust=True, actions=False,
             group_by="column", multi_level_index=True, max_age=None, **_ignored):
    """A drop-in for yf.download() from the bar cache (see the module docstring). max_age: see
    bars(). Symbols yfinance's tz cache does not know yet are quoted in one batched request
    first (their rows carry the exchange time zone), saving a tz probe per symbol."""
    if isinstance(tickers, str):
        tickers = tickers.replace(",", " ").split()
    ensure_tz(tickers)
    with _bar_max_age(max_age):
        return _download(tickers, start, end, period, interval, auto_adjust, actions, group_by,
                         multi_level_index)


def _download(tickers, start, end, period, interval, auto_adjust, actions, group_by, multi_level_index):
    import pandas as pd
    ps = _price_store_module()
    if isinstance(tickers, str):
        tickers = tickers.replace(",", " ").split()
    tickers = list(dict.fromkeys(str(t).strip() for t in tickers if str(t).strip()))
    if period is None and start is None:
        period = "1mo"
    if start is not None and end is not None:
        end = (pd.Timestamp(end) - pd.Timedelta(days=1)).date()  # exclusive, as yf.download
    errors = {}
    frames = {}
    intraday = ps.is_intraday(interval)
    for t in tickers:
        try:
            if auto_adjust:
                df, prov = frame(t, period, interval, start, end, "split+dividend")
                f = df[["Open", "High", "Low", "Close", "Volume"]].copy()
            else:
                df, prov = frame(t, period, interval, start, end, "split")
                adj, _p = frame(t, period, interval, start, end, "split+dividend")
                f = df[["Open", "High", "Low", "Close"]].copy()
                f["Adj Close"] = adj["Close"].reindex(f.index)
                f["Volume"] = df["Volume"]
            if actions:
                raw, _p2 = frame(t, period, interval, start, end, "none")
                div, spl = _split_adjusted_actions(raw)
                f["Dividends"] = div.reindex(f.index).fillna(0.0)
                f["Stock Splits"] = spl.reindex(f.index).fillna(0.0)
            if intraday:
                f.index = _localize(f.index, prov.get("exchangeTz")).tz_convert("UTC")
                f = f[~f.index.isna()]
            frames[t] = f
        except ps.PriceDataError as e:
            # yfinance's own wording, so callers that classify shared._ERRORS keep working.
            prefix = {"UNKNOWN_SYMBOL": "Unknown symbol (possibly delisted; no price data found): ",
                      "NO_DATA_IN_RANGE": "No price data found: ", "RANGE_LIMIT": "No price data found: ",
                      "RATE_LIMITED": "Too Many Requests. Rate limited. ",
                      "NETWORK": "Connection error: ", "OFFLINE_MISS": "Connection error: "}.get(e.code, "")
            errors[t.upper()] = prefix + str(e)
        except ValueError as e:
            errors[t.upper()] = "No price data found: " + str(e)
    try:  # yf.download reports per-symbol failures here; callers read it
        from yfinance import shared as _sh
        _sh._ERRORS = dict(errors)
    except Exception:
        pass
    fields = ["Open", "High", "Low", "Close", "Volume"] if auto_adjust else \
        ["Open", "High", "Low", "Close", "Adj Close", "Volume"]
    if actions:
        fields += ["Dividends", "Stock Splits"]
    for t in tickers:
        if t not in frames:
            frames[t] = pd.DataFrame(columns=fields, dtype=float)
    if not any(len(f) for f in frames.values()):
        return pd.DataFrame()
    parts = []
    for t in tickers:
        f = frames[t].reindex(columns=fields)
        f.columns = pd.MultiIndex.from_tuples([(t, c) for c in f.columns], names=["Ticker", "Price"])
        parts.append(f)
    out = pd.concat(parts, axis=1).sort_index()
    out.index.name = "Datetime" if intraday else "Date"
    if group_by != "ticker":
        out = out.swaplevel(0, 1, axis=1)
        out.columns.names = ["Price", "Ticker"]
        order = [c for c in sorted(fields) if c in fields]
        out = out.reindex(columns=pd.MultiIndex.from_product([order, tickers], names=["Price", "Ticker"]))
    if not multi_level_index and len(tickers) == 1:
        out.columns = out.columns.droplevel(0 if group_by == "ticker" else 1)
    return out


_spark_mem = collections.OrderedDict()  # (SYMBOL, range, interval) -> (fetched, closes)
_SPARK_MEM_MAX = 2000


def spark(symbols, range_="5d", interval="1h", max_age=300):
    """{symbol: [close, ...]} from Yahoo's spark endpoint (20 symbols per request), cached in
    memory max_age seconds per symbol. Symbols Yahoo has no series for are absent. Raises
    when a request fails (the caller falls back to the bar cache)."""
    syms = list(dict.fromkeys(str(s).strip() for s in symbols if s and str(s).strip()))
    out = {}
    need = []
    now = time.time()
    with _json_lock:
        for s in syms:
            hit = _spark_mem.get((s.upper(), range_, interval))
            if hit and now - hit[0] < max_age:
                _count("served_memory")
                out[s] = hit[1]
            else:
                need.append(s)
    for i in range(0, len(need), 20):
        chunk = need[i:i + 20]
        res = get_json(_SPARK_URL, {"symbols": ",".join(chunk), "range": range_, "interval": interval,
                                    "indicators": "close", "includeTimestamps": "false",
                                    "includePrePost": "false"})
        by_sym = {}
        for item in ((res or {}).get("spark") or {}).get("result") or []:
            try:
                resp = (item.get("response") or [None])[0] or {}
                closes = (((resp.get("indicators") or {}).get("quote") or [{}])[0] or {}).get("close") or []
                vals = [round(float(v), 4) for v in closes if v is not None and math.isfinite(float(v))]
                if vals:
                    by_sym[str(item.get("symbol") or "").upper()] = vals
            except Exception:
                continue
        t = time.time()
        with _json_lock:
            for s in chunk:
                vals = by_sym.get(s.upper())
                if vals:
                    out[s] = vals
                    _spark_mem[(s.upper(), range_, interval)] = (t, vals)
            while len(_spark_mem) > _SPARK_MEM_MAX:
                _spark_mem.popitem(last=False)
    return out
