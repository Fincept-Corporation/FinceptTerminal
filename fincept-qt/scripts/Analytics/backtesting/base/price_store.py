"""
Price store: ONE on-disk cache of OHLCV bars for every engine and command.

    from base.price_store import get_bars
    res = get_bars(['SPY', 'RELIANCE.NS'], '2015-01-01', '2024-12-31', interval='1d',
                   adjust='split+dividend')
    res.frames['SPY']        # DataFrame[Open, High, Low, Close, Volume], tz-naive session dates
    res.provenance['SPY']    # bars, firstBar, lastBar, currency, source, cacheAsOf, fromCache ...
    res.errors               # {symbol: PriceDataError} for symbols that could not be served
    res.network_calls        # 0 when the cache was warm

WHY ONE STORE. Engines and the normaliser used to download the same bars 2-8 times per run
and once per optimisation combination (zipline, bt). Warm: zero downloads. Cold: one batch
for all missing symbols. Never one per combination: optimisers load bars once and hand them
to their workers.

LAYOUT   <data root>/cache/scripts/backtesting/prices/v1/<interval>/<SYMBOL>.npz  (data root and cache
  folder from scripts/fincept_paths.py via base.paths.cache_root; + .json copy of the
  metadata for people, + .lock). numpy savez, loaded with allow_pickle=False (no pickle, no
  pyarrow - pyarrow is not installed). Arrays:
    t           int64 ns, tz-NAIVE exchange wall time (daily bars: the exchange SESSION DATE;
                Yahoo's exchange-local midnight is kept as that date - no UTC shift)
    open/high/low/close   RAW traded prices (split-unadjusted, rebuilt from Yahoo's
                split-adjusted bars and the split events, as ai_quant_lab/qlib_datasets.py does)
    adj_close   Yahoo's split+dividend adjusted close
    volume      raw shares; dividends: cash per share (raw) on the ex-date; splits: ratio, 1 = none
    meta        JSON string (authoritative metadata, written atomically with the arrays)
  One file per (symbol, interval) holds both series: the adjustment is chosen at read time:
    'split+dividend'  OHLC x adj_close / close (what yfinance auto_adjust=True returns);
                      volume split-adjusted
    'split'           OHLC / (product of later splits); volume x that product
    'none'            raw OHLC and volume, plus Dividends and Splits columns (corporate actions
                      for base.simulator's raw-price mode)
  The column semantics match qlib_datasets' Yahoo cache (open/high/low/close raw, adj_close,
  volume, dividends, splits) so the two can be converted; a separate file format was needed
  for intraday intervals, lossless floats (that cache is 6-dp CSV) and atomic, locked writes.

FRESHNESS. An entry records the date range asked of Yahoo and when. A request is served
  from the cache when its start is covered (or the symbol's history starts later - listing)
  and its end is before the last cached bar, or the fetch happened after the requested end
  date was over everywhere (end + 1 day + 14 h UTC), or the entry is younger than
  FINCEPT_PRICE_CACHE_TTL_S (default 900 s = 15 min). Otherwise only the missing TAIL is
  fetched (from the last cached bar - 7 days, for an overlap check): raw closes on the
  overlap must agree within 2e-6 and the adjusted-close ratio must be constant (a new
  dividend / split re-scales Yahoo's adjusted history by one factor - applied to the cached
  rows); anything else refetches the whole symbol. A start earlier than the cached range
  refetches the whole symbol (rare). The last bar is flagged provisional when it was fetched
  before its day was over everywhere; the next update replaces it.

CONCURRENCY. Writers take a per-symbol lock file (msvcrt.locking on Windows, fcntl.flock on
  POSIX; released by the OS if the process dies), re-check the entry, fetch, then write
  temp + os.replace (retried while a reader holds the file on Windows). Readers never lock:
  they read the whole file in one call and retry on a torn / replaced file. Corrupt entries
  are logged and refetched (an error offline).

SIZE CAP. FINCEPT_PRICE_CACHE_MAX_MB (default 1024): after a write the least recently used
  entries (file mtime, touched on every read) are deleted until the cache is under 90 %.

  PriceStore.ttl_for() and fetch_start_for() are policy hooks: scripts/fincept_yahoo.py's store
  (charts, quotes' bar fallback, analytics downloads) uses the same files with a market-session
  freshness and a long first download, so every later range is a local slice.

YAHOO. yfinance is imported only on a miss (~0.9 s saved per warm run), configured by
  scripts/fincept_yahoo.py (one session with the cookie + crumb persisted, a limiter / back-off /
  circuit breaker shared by every process). All Yahoo calls in a process go through one lock,
  30 s timeout, a pause between requests and back-off on HTTP 429 (5, 15, 45 s; none while the
  shared circuit breaker is open). The chart metadata comes with the bars (no second request). Errors are per symbol and plain: unknown symbol; no bars in the range (with
  the dates the symbol does have); intraday range limits (1m: 30 days, <1h: 60 days, 1h: 730
  days); rate limited; network. Tests set FINCEPT_PRICE_CACHE_OFFLINE=1 (a miss is an error)
  or inject a fetcher: PriceStore(fetcher=fn).
"""

from __future__ import annotations

import io
import json
import math
import os
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from datetime import date, datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, List, Optional, Sequence, Tuple

import numpy as np

from base.paths import cache_root

STORE_VERSION = 'v1'
ADJUSTMENTS = ('split+dividend', 'split', 'none')
INTERVALS = ('1m', '2m', '5m', '15m', '30m', '60m', '90m', '1h', '4h', '1d', '5d', '1wk', '1mo', '3mo')
_DAILY = ('1d', '5d', '1wk', '1mo', '3mo')
# Yahoo history limits for intraday bars (days back from today).
_INTRADAY_LIMIT_DAYS = {'1m': 30, '2m': 60, '5m': 60, '15m': 60, '30m': 60, '60m': 730, '90m': 60, '1h': 730}
_TAIL_OVERLAP_DAYS = {'1d': 7, '5d': 21, '1wk': 21, '1mo': 70, '3mo': 200}
_PERIOD_DAYS = {'1d': 1, '5d': 7, '1wk': 7, '1mo': 31, '3mo': 92}
_MISSING_RECHECK_S = 24 * 3600
_RATE_LIMIT_DELAYS = (5.0, 15.0, 45.0)
_PAUSE_S = 0.15
_YF_TIMEOUT_S = 20
_LOCK_TIMEOUT_S = 180.0
_WIN_RESERVED = {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(10)), *(f'LPT{i}' for i in range(10))}

_yf_lock = threading.RLock()


def _log(msg: str) -> None:
    try:
        print(f'[price_store] {msg}', file=sys.stderr, flush=True)
    except Exception:
        pass


# ════════════════════════════════════════════════════════════════════════════
# Errors
# ════════════════════════════════════════════════════════════════════════════

class PriceDataError(Exception):
    """A symbol could not be served. `code`: UNKNOWN_SYMBOL | NO_DATA_IN_RANGE | RANGE_LIMIT |
    RATE_LIMITED | NETWORK | OFFLINE_MISS | CORRUPT | BAD_REQUEST. str(e) is a plain sentence."""

    def __init__(self, symbol: str, code: str, message: str, first_available: Optional[str] = None,
                 last_available: Optional[str] = None):
        super().__init__(message)
        self.symbol = symbol
        self.code = code
        self.first_available = first_available
        self.last_available = last_available

    def to_dict(self) -> Dict[str, Any]:
        return {'symbol': self.symbol, 'code': self.code, 'message': str(self),
                'firstAvailable': self.first_available, 'lastAvailable': self.last_available}


# ════════════════════════════════════════════════════════════════════════════
# Small helpers
# ════════════════════════════════════════════════════════════════════════════

def normalize_symbol(sym: str) -> str:
    """'  spy ' -> 'SPY'; broker style 'NSE:RELIANCE' -> 'RELIANCE.NS', 'BSE:X' -> 'X.BO'."""
    s = str(sym or '').strip().upper()
    if ':' in s:
        ex, _, tk = s.partition(':')
        if ex == 'NSE' and tk and not tk.endswith('.NS'):
            return tk + '.NS'
        if ex == 'BSE' and tk and not tk.endswith('.BO'):
            return tk + '.BO'
        return tk or ex
    return s


def safe_file_stem(sym: str) -> str:
    s = re.sub(r'[^A-Z0-9.\-^=]', lambda m: '_%02X' % ord(m.group(0)), sym.upper())
    return ('_' + s) if s.split('.')[0] in _WIN_RESERVED else s


def normalize_interval(interval: Optional[str]) -> str:
    iv = str(interval or '1d').strip().lower()
    iv = {'d': '1d', 'day': '1d', 'daily': '1d', '1day': '1d', 'w': '1wk', 'week': '1wk', 'weekly': '1wk',
          '1w': '1wk', 'm': '1mo', 'month': '1mo', 'monthly': '1mo', '1mth': '1mo', '60min': '60m',
          '1hour': '1h', 'hour': '1h'}.get(iv, iv)
    if iv not in INTERVALS:
        raise ValueError(f'unsupported interval {interval!r}; use one of {", ".join(INTERVALS)}')
    return iv


def is_intraday(interval: str) -> bool:
    return normalize_interval(interval) not in _DAILY


def _to_date(v) -> date:
    if isinstance(v, datetime):
        return v.date()
    if isinstance(v, date):
        return v
    s = str(v).strip()[:10]
    return date.fromisoformat(s)


def _utcnow() -> datetime:
    return datetime.now(timezone.utc)


_CCY_SUFFIX = {
    '.NS': 'INR', '.BO': 'INR', '.T': 'JPY', '.L': 'GBp', '.IL': 'USD', '.HK': 'HKD', '.TO': 'CAD', '.V': 'CAD',
    '.CN': 'CAD', '.NE': 'CAD', '.AX': 'AUD', '.NZ': 'NZD', '.DE': 'EUR', '.F': 'EUR', '.BE': 'EUR', '.DU': 'EUR',
    '.HM': 'EUR', '.HA': 'EUR', '.MU': 'EUR', '.SG': 'EUR', '.PA': 'EUR', '.AS': 'EUR', '.MI': 'EUR', '.MC': 'EUR',
    '.BR': 'EUR', '.VI': 'EUR', '.HE': 'EUR', '.LS': 'EUR', '.IR': 'EUR', '.SW': 'CHF', '.SS': 'CNY', '.SZ': 'CNY',
    '.KS': 'KRW', '.KQ': 'KRW', '.TW': 'TWD', '.TWO': 'TWD', '.SA': 'BRL', '.MX': 'MXN', '.SI': 'SGD',
    '.ST': 'SEK', '.OL': 'NOK', '.CO': 'DKK', '.JK': 'IDR', '.KL': 'MYR', '.IS': 'TRY', '.TA': 'ILA',
    '.JO': 'ZAc', '.BK': 'THB', '.WA': 'PLN', '.SR': 'SAR', '.QA': 'QAR',
}
_CCY_INDEX = {'^NSEI': 'INR', '^BSESN': 'INR', '^N225': 'JPY', '^FTSE': 'GBP', '^GDAXI': 'EUR', '^FCHI': 'EUR',
              '^STOXX50E': 'EUR', '^HSI': 'HKD', '^GSPTSE': 'CAD', '^AXJO': 'AUD', '^KS11': 'KRW',
              '^TWII': 'TWD', '^BVSP': 'BRL', '^MXX': 'MXN', '^SSMI': 'CHF', '^STI': 'SGD', '000001.SS': 'CNY'}


def currency_for_symbol(sym: str) -> Optional[str]:
    """Trading currency inferred from the Yahoo symbol (suffix / known index); None if unsure.
    Crypto pairs: the quote currency ('BTC-USD' -> USD). Plain US tickers -> USD."""
    s = normalize_symbol(sym)
    if s in _CCY_INDEX:
        return _CCY_INDEX[s]
    m = re.match(r'^[A-Z0-9]+-([A-Z]{3,4})$', s)
    if m:
        return m.group(1)[:3] if m.group(1) != 'USDT' else 'USD'
    if s.endswith('=X'):
        return s[3:6] if len(s) >= 8 else None
    for suf, ccy in sorted(_CCY_SUFFIX.items(), key=lambda kv: -len(kv[0])):
        if s.endswith(suf):
            return ccy
    if s.startswith('^'):
        return 'USD' if s in ('^GSPC', '^DJI', '^IXIC', '^RUT', '^VIX', '^NDX', '^IRX', '^TNX', '^FVX') else None
    if re.match(r'^[A-Z][A-Z0-9.\-]{0,9}$', s) and '.' not in s:
        return 'USD'
    return None


# ════════════════════════════════════════════════════════════════════════════
# File lock (Windows + POSIX, no dependency)
# ════════════════════════════════════════════════════════════════════════════

class FileLock:
    """Exclusive inter-process lock on `path` (created, never deleted). Released by the OS when
    the process dies. Re-entrant within one process per path."""

    _held: Dict[str, int] = {}
    _held_lock = threading.Lock()

    def __init__(self, path: Path, timeout: float = _LOCK_TIMEOUT_S, poll: float = 0.05):
        self.path = str(path)
        self.timeout = timeout
        self.poll = poll
        self.fd: Optional[int] = None
        self._reentrant = False

    def acquire(self, blocking: bool = True) -> bool:
        with FileLock._held_lock:
            if self.path in FileLock._held:
                FileLock._held[self.path] += 1
                self._reentrant = True
                return True
        Path(self.path).parent.mkdir(parents=True, exist_ok=True)
        fd = os.open(self.path, os.O_RDWR | os.O_CREAT, 0o666)
        deadline = time.monotonic() + (self.timeout if blocking else 0.0)
        while True:
            try:
                if os.name == 'nt':
                    import msvcrt
                    os.lseek(fd, 0, os.SEEK_SET)
                    msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                self.fd = fd
                with FileLock._held_lock:
                    FileLock._held[self.path] = 1
                return True
            except OSError:
                if time.monotonic() >= deadline:
                    os.close(fd)
                    return False
                time.sleep(self.poll)

    def release(self) -> None:
        with FileLock._held_lock:
            cnt = FileLock._held.get(self.path, 0)
            if cnt > 1:
                FileLock._held[self.path] = cnt - 1
                return
            FileLock._held.pop(self.path, None)
        if self.fd is None:
            return
        try:
            if os.name == 'nt':
                import msvcrt
                os.lseek(self.fd, 0, os.SEEK_SET)
                msvcrt.locking(self.fd, msvcrt.LK_UNLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.fd, fcntl.LOCK_UN)
        except OSError:
            pass
        finally:
            os.close(self.fd)
            self.fd = None

    def __enter__(self):
        if not self.acquire():
            raise TimeoutError(f'could not lock {self.path} within {self.timeout:.0f} s')
        return self

    def __exit__(self, *exc):
        self.release()


def _atomic_write_bytes(path: Path, data: bytes, retries: int = 40) -> None:
    tmp = path.with_name(f'{path.name}.{os.getpid()}.{threading.get_ident()}.tmp')
    with open(tmp, 'wb') as f:
        f.write(data)
        f.flush()
        try:
            os.fsync(f.fileno())
        except OSError:
            pass
    for i in range(retries):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            # Windows: a reader has the target open for a moment.
            time.sleep(0.025 * (i + 1))
    try:
        os.remove(tmp)
    finally:
        raise PermissionError(f'could not replace {path} (held open by another process)')


# ════════════════════════════════════════════════════════════════════════════
# Entries
# ════════════════════════════════════════════════════════════════════════════

_COLS = ('open', 'high', 'low', 'close', 'adj_close', 'volume', 'dividends', 'splits')


@dataclass
class Entry:
    symbol: str
    interval: str
    t: np.ndarray                  # int64 ns
    cols: Dict[str, np.ndarray]
    meta: Dict[str, Any]

    @property
    def rows(self) -> int:
        return len(self.t)

    def first_bar(self):
        return _ts(self.t[0]) if len(self.t) else None

    def last_bar(self):
        return _ts(self.t[-1]) if len(self.t) else None


def _ts(ns):
    import pandas as pd
    return pd.Timestamp(int(ns))


def _fmt_bar(ts, intraday: bool) -> str:
    return ts.strftime('%Y-%m-%d %H:%M:%S') if intraday else ts.strftime('%Y-%m-%d')


def _encode_entry(e: Entry) -> bytes:
    buf = io.BytesIO()
    arrays = {'t': np.asarray(e.t, dtype=np.int64)}
    for c in _COLS:
        arrays[c] = np.asarray(e.cols[c], dtype=np.float64)
    arrays['meta'] = np.array(json.dumps(e.meta, sort_keys=True))
    np.savez(buf, **arrays)
    return buf.getvalue()


def _decode_entry(symbol: str, interval: str, raw: bytes) -> Entry:
    with np.load(io.BytesIO(raw), allow_pickle=False) as z:
        meta = json.loads(str(z['meta']))
        t = z['t'].astype(np.int64)
        cols = {c: z[c].astype(np.float64) for c in _COLS}
    n = len(t)
    if any(len(v) != n for v in cols.values()):
        raise ValueError('column lengths differ')
    if n and np.any(np.diff(t) <= 0):
        raise ValueError('timestamps not strictly increasing')
    if meta.get('rows') is not None and int(meta['rows']) != n:
        raise ValueError('row count does not match metadata')
    return Entry(symbol, interval, t, cols, meta)


# ════════════════════════════════════════════════════════════════════════════
# Yahoo fetcher
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class FetchResult:
    frame: Any                         # DataFrame indexed by tz-naive wall time, Yahoo raw-layout columns
    meta: Dict[str, Any] = field(default_factory=dict)


class YahooFetcher:
    """Ticker.history(auto_adjust=False, actions=True) per symbol, serialised, paced, with
    back-off. Returns the bars rebuilt to RAW prices (see module doc)."""

    def __init__(self):
        self.calls = 0

    def _yf(self):
        # scripts/fincept_yahoo.py: the process's one Yahoo session (persisted cookie + crumb,
        # cross-process limiter, back-off, circuit breaker, yfinance cache under the data root).
        try:
            from base.paths import _fincept_paths
            _fincept_paths()  # puts the scripts folder on sys.path
            import fincept_yahoo
            return fincept_yahoo.yf()
        except ImportError:
            import yfinance as yf
            try:
                yf.set_tz_cache_location(str(cache_root('yfinance')))
            except Exception:
                pass
            return yf

    @staticmethod
    def _chart_meta(tk) -> Dict[str, Any]:
        """The chart metadata the history() call just stored. Ticker.get_history_metadata()
        sends a SECOND request (a 5-day hourly chart) for its trading periods - never needed."""
        try:
            hm = getattr(tk._lazy_load_price_history(), '_history_metadata', None)
            return hm if isinstance(hm, dict) else {}
        except Exception:
            return {}

    def __call__(self, symbol: str, interval: str, start: date, end: date) -> FetchResult:
        yf = self._yf()
        yiv = '1h' if interval == '4h' else interval
        last_exc = None
        for attempt in range(len(_RATE_LIMIT_DELAYS) + 1):
            with _yf_lock:
                self.calls += 1
                try:
                    tk = yf.Ticker(symbol)
                    df = tk.history(start=start.isoformat(), end=(end + timedelta(days=1)).isoformat(),
                                    interval=yiv, auto_adjust=False, actions=True, raise_errors=True,
                                    timeout=_YF_TIMEOUT_S)
                    meta = {}
                    try:
                        hm = self._chart_meta(tk)
                        meta = {'currency': hm.get('currency'), 'exchangeTz': hm.get('exchangeTimezoneName'),
                                'firstTradeDate': _epoch_date(hm.get('firstTradeDate')),
                                'instrumentType': hm.get('instrumentType'), 'exchange': hm.get('exchangeName'),
                                'name': hm.get('longName') or hm.get('shortName'),
                                'regularMarketTime': hm.get('regularMarketTime')}
                    except Exception:
                        pass
                    time.sleep(_PAUSE_S)
                    return FetchResult(df, meta)
                except Exception as exc:  # classified below
                    last_exc = exc
                    time.sleep(_PAUSE_S)
            if type(last_exc).__name__ == 'YahooRateLimited':
                break  # no request was sent: the shared circuit breaker / limiter decides when
            if _is_rate_limit(last_exc) and attempt < len(_RATE_LIMIT_DELAYS):
                _log(f'{symbol}: Yahoo rate limit, retrying in {_RATE_LIMIT_DELAYS[attempt]:.0f} s')
                time.sleep(_RATE_LIMIT_DELAYS[attempt])
                continue
            if _is_network(last_exc) and attempt < 1:
                time.sleep(2.0)
                continue
            break
        raise self._classify(symbol, interval, start, end, last_exc)

    def _classify(self, symbol, interval, start, end, e) -> PriceDataError:
        msg = str(e or '')
        low = msg.lower()
        if _is_rate_limit(e):
            return PriceDataError(symbol, 'RATE_LIMITED', f'Yahoo Finance is rate-limiting requests for {symbol}; '
                                                          f'try again in a minute.')
        if 'no timezone found' in low or 'symbol may be delisted' in low or 'tickermissing' in low \
                or 'not found' in low or 'quote not found' in low:
            return PriceDataError(symbol, 'UNKNOWN_SYMBOL',
                                  f'Yahoo Finance has no symbol {symbol}. Check the ticker and its exchange suffix '
                                  f'(e.g. RELIANCE.NS, 7203.T, VOD.L).')
        if 'no price data found' in low or 'no data found' in low or 'pricesmissing' in low or 'delisted' in low:
            first, last = self._probe_range(symbol)
            if first and _to_date(first) > end:
                return PriceDataError(symbol, 'NO_DATA_IN_RANGE',
                                      f'{symbol} has no bars between {start} and {end}: its history on Yahoo '
                                      f'starts {first}.', first, last)
            if last and _to_date(last) < start:
                return PriceDataError(symbol, 'NO_DATA_IN_RANGE',
                                      f'{symbol} has no bars between {start} and {end}: its history on Yahoo '
                                      f'ends {last} (delisted or renamed?).', first, last)
            if first is None and last is None:
                return PriceDataError(symbol, 'UNKNOWN_SYMBOL',
                                      f'Yahoo Finance returned no data for {symbol}. Check the ticker and its '
                                      f'exchange suffix (e.g. RELIANCE.NS).')
            return PriceDataError(symbol, 'NO_DATA_IN_RANGE',
                                  f'{symbol} has no {interval} bars between {start} and {end}.', first, last)
        if _is_network(e):
            return PriceDataError(symbol, 'NETWORK', f'Could not reach Yahoo Finance for {symbol} '
                                                     f'({msg[:120] or type(e).__name__}). Check the connection.')
        return PriceDataError(symbol, 'NETWORK', f'Yahoo Finance request for {symbol} failed: {msg[:160]}')

    def _probe_range(self, symbol) -> Tuple[Optional[str], Optional[str]]:
        """(first trade date, last bar date) of a symbol from a 5-day probe (only after a miss)."""
        try:
            yf = self._yf()
            with _yf_lock:
                self.calls += 1
                tk = yf.Ticker(symbol)
                df = tk.history(period='5d', interval='1d', timeout=_YF_TIMEOUT_S)
                hm = self._chart_meta(tk)
            first = _epoch_date(hm.get('firstTradeDate'))
            last = None
            if df is not None and len(df):
                last = str(df.index[-1])[:10]
            return first, last
        except Exception:
            return None, None


def _epoch_date(v) -> Optional[str]:
    try:
        if v is None:
            return None
        if hasattr(v, 'strftime'):
            return v.strftime('%Y-%m-%d')
        return datetime.fromtimestamp(int(v), tz=timezone.utc).strftime('%Y-%m-%d')
    except Exception:
        return None


def _is_rate_limit(e) -> bool:
    if e is None:
        return False
    if type(e).__name__ == 'YFRateLimitError':
        return True
    m = str(e)
    return 'Too Many Requests' in m or ' 429' in m or 'Rate limited' in m


def _is_network(e) -> bool:
    if e is None:
        return False
    name = type(e).__name__.lower()
    m = str(e).lower()
    return any(s in name for s in ('connection', 'timeout', 'curl', 'ssl')) or \
        any(s in m for s in ('timed out', 'timeout', 'connection', 'name resolution', 'could not resolve', 'curl:'))


def yahoo_frame_to_raw(df, interval: str) -> Tuple[np.ndarray, Dict[str, np.ndarray]]:
    """Yahoo history (auto_adjust=False, actions=True) -> (t ns tz-naive, raw columns).

    Yahoo's Open/High/Low/Close/Volume/Dividends are split-adjusted up to the fetch; the raw
    value multiplies by the product of the splits AFTER the bar inside the same response
    (which runs to today, so it holds every later split). Exchange-local timestamps keep
    their wall time: tz_localize(None), never tz_convert('UTC') (that put RELIANCE.NS bars
    on the previous day)."""
    import pandas as pd
    idx = pd.DatetimeIndex(df.index)
    if idx.tz is not None:
        idx = idx.tz_localize(None)
    if not is_intraday(interval):
        idx = idx.normalize()

    def col(name, default=np.nan):
        if name in df.columns:
            return pd.to_numeric(pd.Series(df[name].values), errors='coerce').to_numpy(dtype=float)
        return np.full(len(df), default)
    splits = col('Stock Splits', 0.0)
    splits = np.where(np.isfinite(splits) & (splits > 0), splits, 1.0)
    after = np.ones(len(splits))
    if len(splits) > 1:
        after[:-1] = np.cumprod(splits[::-1])[::-1][1:]
    close = col('Close')
    adj = col('Adj Close')
    adj = np.where(np.isfinite(adj), adj, close)
    out = {
        'open': col('Open') * after, 'high': col('High') * after, 'low': col('Low') * after, 'close': close * after,
        'adj_close': adj, 'volume': col('Volume', 0.0) / after,
        'dividends': np.nan_to_num(col('Dividends', 0.0)) * after, 'splits': splits,
    }
    t = idx.asi8.astype(np.int64)
    ok = np.isfinite(out['close']) & (out['close'] > 0)
    order = np.argsort(t[ok], kind='stable')
    t2 = t[ok][order]
    cols = {k: v[ok][order] for k, v in out.items()}
    if len(t2) > 1:
        keep = np.concatenate([t2[1:] != t2[:-1], [True]])  # duplicates: keep the last
        t2 = t2[keep]
        cols = {k: v[keep] for k, v in cols.items()}
    return t2, cols


# ════════════════════════════════════════════════════════════════════════════
# Store
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class BarsResult:
    frames: Dict[str, Any] = field(default_factory=dict)
    provenance: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    errors: Dict[str, PriceDataError] = field(default_factory=dict)
    network_calls: int = 0
    interval: str = '1d'
    adjust: str = 'split+dividend'

    def raise_first_error(self) -> None:
        if self.errors:
            raise next(iter(self.errors.values()))


class PriceStore:
    def __init__(self, root: Optional[Path] = None, fetcher: Optional[Callable] = None,
                 ttl_s: Optional[float] = None, max_mb: Optional[float] = None, offline: Optional[bool] = None,
                 clock: Optional[Callable[[], datetime]] = None):
        """root: store directory (default default_root()). fetcher(symbol, interval,
        start, end) -> FetchResult (default YahooFetcher). clock() -> aware UTC datetime (tests)."""
        self.root = Path(root) if root else default_root()
        self.clock = clock or _utcnow
        self._fetcher = fetcher
        self.ttl_s = float(ttl_s if ttl_s is not None else os.environ.get('FINCEPT_PRICE_CACHE_TTL_S', 900))
        self.max_bytes = float(max_mb if max_mb is not None else os.environ.get('FINCEPT_PRICE_CACHE_MAX_MB', 1024)) \
            * 1024 * 1024
        env_off = os.environ.get('FINCEPT_PRICE_CACHE_OFFLINE', '') not in ('', '0', 'false', 'no')
        self.offline = env_off if offline is None else bool(offline)
        self.network_calls = 0

    @property
    def fetcher(self):
        if self._fetcher is None:
            self._fetcher = YahooFetcher()
        return self._fetcher

    # ── paths ──────────────────────────────────────────────────────────────
    def _dir(self, interval: str) -> Path:
        d = self.root / interval
        d.mkdir(parents=True, exist_ok=True)
        return d

    def path(self, symbol: str, interval: str) -> Path:
        return self._dir(interval) / f'{safe_file_stem(symbol)}.npz'

    # ── read ───────────────────────────────────────────────────────────────
    def read_entry(self, symbol: str, interval: str, touch: bool = True) -> Optional[Entry]:
        """The cached entry, None when absent. Raises ValueError on a corrupt file."""
        p = self.path(symbol, interval)
        last_err = None
        for attempt in range(8):
            try:
                with open(p, 'rb') as f:
                    raw = f.read()
            except FileNotFoundError:
                return None
            except PermissionError as e:  # being replaced right now (Windows)
                last_err = e
                time.sleep(0.02 * (attempt + 1))
                continue
            try:
                e = _decode_entry(symbol, interval, raw)
            except Exception as ex:  # torn read or corruption: retry once, then report
                last_err = ex
                time.sleep(0.02 * (attempt + 1))
                continue
            if touch:
                try:
                    os.utime(p, None)
                except OSError:
                    pass
            return e
        raise ValueError(f'cache entry {p.name} is unreadable: {last_err}')

    def _read_missing_marker(self, symbol: str, interval: str) -> Optional[Dict[str, Any]]:
        p = self.path(symbol, interval).with_suffix('.missing.json')
        try:
            m = json.loads(p.read_text(encoding='utf-8'))
        except Exception:
            return None
        if time.time() - float(m.get('checkedTs', 0)) > _MISSING_RECHECK_S:
            return None
        return m

    # ── coverage ───────────────────────────────────────────────────────────
    def _needs(self, e: Optional[Entry], start: date, end: date, interval: str, refresh: bool) -> Optional[str]:
        """None when the entry serves [start, end]; else 'full' or 'tail' (see FRESHNESS)."""
        if e is None or e.rows == 0:
            return 'full'
        m = e.meta
        hist_start = _to_date(m.get('historyStart') or m.get('firstBar'))
        if start < hist_start and not m.get('listingKnown'):
            return 'full'          # never asked Yahoo for bars that early
        if refresh:
            return 'tail'
        last_day = _to_date(m.get('lastBar'))
        if end < last_day or (end == last_day and not m.get('lastBarProvisional') and not is_intraday(interval)):
            return None
        fetched = datetime.fromtimestamp(float(m.get('fetchedTs', 0)), tz=timezone.utc)
        end_over = datetime(end.year, end.month, end.day, tzinfo=timezone.utc) + timedelta(days=1, hours=14)
        if fetched >= end_over:
            return None            # fetched after the requested range was over everywhere
        if (self.clock() - fetched).total_seconds() < self.ttl_for(m, interval):
            return None            # fresh enough (FINCEPT_PRICE_CACHE_TTL_S, or the subclass's policy)
        return 'tail'

    # ── policy hooks (scripts/fincept_yahoo.py's store overrides both) ─────
    def ttl_for(self, meta: Dict[str, Any], interval: str) -> float:
        """Seconds an entry fetched up to today stays fresh. Default: FINCEPT_PRICE_CACHE_TTL_S."""
        return self.ttl_s

    def fetch_start_for(self, symbol: str, interval: str, start: date) -> date:
        """First day a FULL download of `symbol` asks Yahoo for. Default: the requested start.
        Reaching further back once makes every later, longer request a local slice."""
        return start

    # ── fetch + merge ──────────────────────────────────────────────────────
    def _fetch(self, symbol: str, interval: str, start: date) -> Tuple[np.ndarray, Dict[str, np.ndarray], Dict]:
        if self.offline:
            raise PriceDataError(symbol, 'OFFLINE_MISS', f'{symbol} ({interval}) is not in the price cache and the '
                                                         f'cache is offline (FINCEPT_PRICE_CACHE_OFFLINE).')
        today = self.clock().date()
        end = today + timedelta(days=1)
        if is_intraday(interval):
            lim = _INTRADAY_LIMIT_DAYS.get('1h' if interval == '4h' else interval)
            if lim:
                earliest = today - timedelta(days=lim - 1)
                if start < earliest:
                    start = earliest
        self.network_calls += 1
        fr = self.fetcher(symbol, interval, start, end)
        if fr is None or fr.frame is None or len(fr.frame) == 0:
            raise PriceDataError(symbol, 'NO_DATA_IN_RANGE', f'Yahoo Finance returned no {interval} bars for '
                                                             f'{symbol} from {start}.')
        t, cols = yahoo_frame_to_raw(fr.frame, interval)
        if len(t) == 0:
            raise PriceDataError(symbol, 'NO_DATA_IN_RANGE', f'Yahoo Finance returned no usable {interval} bars '
                                                             f'for {symbol} from {start}.')
        return t, cols, dict(fr.meta or {})

    def _make_meta(self, symbol, interval, t, hist_start: date, fmeta: Dict, listing_known: bool) -> Dict[str, Any]:
        now = self.clock()
        last = _ts(t[-1])
        intraday = is_intraday(interval)
        period = _PERIOD_DAYS.get(interval, 1)
        last_day_over = datetime(last.year, last.month, last.day, tzinfo=timezone.utc) + timedelta(days=period, hours=14)
        return {
            'symbol': symbol, 'interval': interval, 'source': 'yahoo', 'version': STORE_VERSION,
            'historyStart': hist_start.isoformat(), 'listingKnown': bool(listing_known),
            'firstBar': _fmt_bar(_ts(t[0]), intraday), 'lastBar': _fmt_bar(last, intraday), 'rows': int(len(t)),
            'fetchedAt': now.strftime('%Y-%m-%dT%H:%M:%SZ'), 'fetchedTs': now.timestamp(),
            'lastBarProvisional': bool(now < last_day_over),
            'currency': fmeta.get('currency') or currency_for_symbol(symbol),
            'currencySource': 'yahoo' if fmeta.get('currency') else 'symbol suffix',
            'exchangeTz': fmeta.get('exchangeTz'), 'firstTradeDate': fmeta.get('firstTradeDate'),
            'instrumentType': fmeta.get('instrumentType'), 'exchange': fmeta.get('exchange'),
            'name': fmeta.get('name'), 'regularMarketTime': fmeta.get('regularMarketTime'),
        }

    def _update(self, symbol: str, interval: str, start: date, kind: str, e: Optional[Entry]) -> Entry:
        """Fetch and write under the symbol's lock (caller holds it)."""
        if kind == 'tail' and e is not None and e.rows > 0:
            overlap = _TAIL_OVERLAP_DAYS.get(interval, 2)
            last = e.last_bar()
            t_new, c_new, fmeta = self._fetch(symbol, interval, (last - _pd_days(overlap)).date())
            merged = _merge_tail(e, t_new, c_new)
            if merged is not None:
                t, cols, ratio = merged
                meta = self._make_meta(symbol, interval, t, _to_date(e.meta['historyStart']),
                                       {**{k: e.meta.get(k) for k in ('currency', 'exchangeTz', 'firstTradeDate',
                                                                     'instrumentType', 'exchange', 'name')},
                                        **{k: v for k, v in fmeta.items() if v is not None}},
                                       bool(e.meta.get('listingKnown')))
                if abs(ratio - 1.0) > 1e-9:
                    meta['adjRescaled'] = ratio
                ent = Entry(symbol, interval, t, cols, meta)
                self._write(ent)
                return ent
            _log(f'{symbol}: cached history disagrees with Yahoo on overlapping bars; downloading it again')
            start = min(start, _to_date(e.meta['historyStart']))
        start = self.fetch_start_for(symbol, interval, start)
        hist_start = start if e is None else min(start, _to_date(e.meta.get('historyStart') or start))
        t, cols, fmeta = self._fetch(symbol, interval, hist_start)
        first = _ts(t[0]).date()
        listing_known = (first - hist_start).days > 7 and not is_intraday(interval)
        meta = self._make_meta(symbol, interval, t, hist_start, fmeta, listing_known)
        ent = Entry(symbol, interval, t, cols, meta)
        self._write(ent)
        return ent

    def _write(self, e: Entry) -> None:
        p = self.path(e.symbol, e.interval)
        _atomic_write_bytes(p, _encode_entry(e))
        try:
            _atomic_write_bytes(p.with_suffix('.json'), json.dumps(e.meta, indent=1, sort_keys=True).encode('utf-8'))
        except Exception:
            pass
        mp = p.with_suffix('.missing.json')
        if mp.exists():
            try:
                mp.unlink()
            except OSError:
                pass

    def _write_missing(self, symbol: str, interval: str, err: PriceDataError) -> None:
        if err.code not in ('UNKNOWN_SYMBOL',):
            return
        p = self.path(symbol, interval).with_suffix('.missing.json')
        try:
            _atomic_write_bytes(p, json.dumps({'symbol': symbol, 'code': err.code, 'message': str(err),
                                               'checkedTs': time.time()}).encode('utf-8'))
        except Exception:
            pass

    # ── public ─────────────────────────────────────────────────────────────
    def get(self, symbols: Sequence[str], start, end, interval: str = '1d', adjust: str = 'split+dividend',
            refresh: bool = False, progress: bool = True) -> BarsResult:
        """Bars of every symbol in [start, end] (both inclusive). See module doc."""
        iv = normalize_interval(interval)
        if adjust not in ADJUSTMENTS:
            raise ValueError(f'adjust must be one of {", ".join(ADJUSTMENTS)}, not {adjust!r}')
        s_date, e_date = _to_date(start), _to_date(end)
        if e_date < s_date:
            raise ValueError(f'end date {e_date} is before start date {s_date}')
        store_iv = '1h' if iv == '4h' else iv
        syms: List[str] = []
        for s in symbols:
            ns = normalize_symbol(s)
            if ns and ns not in syms:
                syms.append(ns)
        res = BarsResult(interval=iv, adjust=adjust)
        calls0 = self.network_calls
        prog = None
        if progress:
            from base.progress import Progress
            prog = Progress('data', total=len(syms), message=f'Loading {len(syms)} symbol(s)')
        # Intraday window check (plain error, no request)
        if is_intraday(iv):
            lim = _INTRADAY_LIMIT_DAYS.get(store_iv)
            today = self.clock().date()
            if lim and s_date < today - timedelta(days=lim - 1):
                earliest = today - timedelta(days=lim - 1)
                if e_date < earliest:
                    for s in syms:
                        res.errors[s] = PriceDataError(
                            s, 'RANGE_LIMIT', f'Yahoo keeps {iv} bars for {lim} days only; the earliest start is '
                                              f'{earliest}. Use a later period or daily bars.')
                    return res
        entries: Dict[str, Entry] = {}
        todo: List[Tuple[str, str]] = []
        for s in syms:
            if not refresh:
                miss = self._read_missing_marker(s, store_iv)
                if miss:
                    res.errors[s] = PriceDataError(s, miss.get('code', 'UNKNOWN_SYMBOL'), miss.get('message', ''))
                    continue
            try:
                e = self.read_entry(s, store_iv)
            except ValueError as ex:
                _log(f'{s}: {ex}; it will be downloaded again')
                e = None
            need = self._needs(e, s_date, e_date, store_iv, refresh)
            if need is None:
                entries[s] = e
            else:
                todo.append((s, need))
        # Cold / stale symbols: one batch, locks taken in a fixed order. Circuit breaker: after two
        # consecutive network failures the rest of the batch fails fast (offline machine) - a stale
        # cached copy is still served.
        net_fail = 0
        for s, need in sorted(todo):
            if net_fail >= 2:
                try:
                    e_old = self.read_entry(s, store_iv)
                except ValueError:
                    e_old = None
                if e_old is not None and e_old.rows and need == 'tail':
                    entries[s] = e_old
                    e_old.meta = dict(e_old.meta, stale=True, staleReason='Yahoo Finance unreachable')
                else:
                    res.errors[s] = PriceDataError(s, 'NETWORK', f'Could not reach Yahoo Finance for {s} (the previous '
                                                                 f'requests failed too). Check the connection.')
                continue
            lock = FileLock(self.path(s, store_iv).with_suffix('.lock'))
            if not lock.acquire():
                res.errors[s] = PriceDataError(s, 'NETWORK', f'{s}: another process has been downloading it for '
                                                             f'{_LOCK_TIMEOUT_S:.0f} s; try again.')
                continue
            try:
                try:
                    e = self.read_entry(s, store_iv)
                except ValueError:
                    e = None
                just_refreshed = e is not None and (self.clock().timestamp() - float(e.meta.get('fetchedTs', 0))) < 5.0
                need2 = self._needs(e, s_date, e_date, store_iv, refresh and not just_refreshed)
                if need2 is None:
                    entries[s] = e
                    continue
                try:
                    entries[s] = self._update(s, store_iv, s_date, need2, e)
                    net_fail = 0
                except PriceDataError as pe:
                    net_fail = net_fail + 1 if pe.code == 'NETWORK' else 0
                    if e is not None and e.rows and pe.code in ('NETWORK', 'RATE_LIMITED', 'OFFLINE_MISS') \
                            and need2 == 'tail':
                        entries[s] = e  # serve what we have; provenance says how old it is
                        e.meta = dict(e.meta, stale=True, staleReason=str(pe))
                    else:
                        res.errors[s] = pe
                        self._write_missing(s, store_iv, pe)
                except Exception as ex:  # network library failures
                    net_fail += 1
                    if e is not None and e.rows and need2 == 'tail':
                        entries[s] = e
                        e.meta = dict(e.meta, stale=True, staleReason=str(ex)[:160])
                    else:
                        res.errors[s] = PriceDataError(s, 'NETWORK', f'Could not load {s}: {str(ex)[:160]}')
            finally:
                lock.release()
            if prog:
                prog.update(len(entries) + len(res.errors), f'Loaded {s}')
        for s in syms:
            if s in entries:
                try:
                    frame, prov = _slice(entries[s], s_date, e_date, iv, adjust)
                except PriceDataError as pe:
                    res.errors[s] = pe
                    continue
                prov['fromCache'] = not any(x == s for x, _ in todo)
                res.frames[s] = frame
                res.provenance[s] = prov
        res.network_calls = self.network_calls - calls0
        if prog:
            prog.finish(f'{len(res.frames)} of {len(syms)} symbol(s) loaded'
                        + (f', {res.network_calls} download(s)' if res.network_calls else ' from the cache'))
        if todo:
            self.enforce_size_cap()
        return res

    # ── maintenance ────────────────────────────────────────────────────────
    def entries(self) -> List[Path]:
        if not self.root.exists():
            return []
        return [p for p in self.root.glob('*/*.npz')]

    def size_bytes(self) -> int:
        tot = 0
        for p in self.root.glob('*/*'):
            try:
                tot += p.stat().st_size
            except OSError:
                pass
        return tot

    def enforce_size_cap(self) -> int:
        """Delete least-recently-used entries until the cache is under 90 % of the cap. Returns
        the number of entries removed. Entries locked by another process are skipped."""
        total = self.size_bytes()
        if total <= self.max_bytes:
            return 0
        target = 0.9 * self.max_bytes
        files = []
        for p in self.entries():
            try:
                st = p.stat()
                files.append((st.st_mtime, p, st.st_size))
            except OSError:
                pass
        files.sort()
        removed = 0
        for _, p, size in files:
            if total <= target:
                break
            lock = FileLock(p.with_suffix('.lock'), timeout=0)
            if not lock.acquire(blocking=False):
                continue
            try:
                for q in (p, p.with_suffix('.json')):
                    try:
                        sz = q.stat().st_size
                        q.unlink()
                        total -= sz
                    except OSError:
                        pass
                removed += 1
            finally:
                lock.release()
        if removed:
            _log(f'size cap: removed {removed} least recently used entr{"y" if removed == 1 else "ies"}')
        return removed


def _pd_days(n):
    import pandas as pd
    return pd.Timedelta(days=n)


def _merge_tail(e: Entry, t_new: np.ndarray, c_new: Dict[str, np.ndarray]):
    """(t, cols, adj_ratio) of the cached rows + the fresh tail, or None when the overlap
    disagrees or is empty (the caller then refetches the whole symbol).

    Raw closes on the overlap must agree within 2e-6 (they are what traded); Yahoo's adjusted
    closes may differ by ONE constant factor (a dividend / split after the cached rows
    re-scales the whole earlier history) which is applied to the cached adj_close."""
    if len(t_new) == 0:
        return e.t, e.cols, 1.0
    common, i_old, i_new = np.intersect1d(e.t, t_new, return_indices=True)
    if e.meta.get('lastBarProvisional') and len(common) and common[-1] == e.t[-1]:
        common, i_old, i_new = common[:-1], i_old[:-1], i_new[:-1]
    if len(common) == 0:
        return None
    oc, nc = e.cols['close'][i_old], c_new['close'][i_new]
    if np.any(~np.isfinite(nc / oc)) or np.any(np.abs(nc / oc - 1.0) > 2e-6):
        return None
    ra = c_new['adj_close'][i_new] / e.cols['adj_close'][i_old]
    if np.any(~np.isfinite(ra)):
        return None
    r = float(np.median(ra))
    if np.any(np.abs(ra / r - 1.0) > 5e-6):
        return None
    keep = e.t < t_new[0]
    t = np.concatenate([e.t[keep], t_new])
    cols = {c: np.concatenate([e.cols[c][keep] * (r if c == 'adj_close' else 1.0), c_new[c]]) for c in _COLS}
    return t, cols, r


def _slice(e: Entry, start: date, end: date, interval: str, adjust: str):
    """DataFrame of the requested window + provenance."""
    import pandas as pd
    intraday = is_intraday(interval)
    t = e.t
    lo = np.datetime64(start.isoformat()).astype('datetime64[ns]').astype(np.int64)
    hi = np.datetime64((end + timedelta(days=1)).isoformat()).astype('datetime64[ns]').astype(np.int64)
    m = (t >= lo) & (t < hi)
    sym = e.symbol
    if not m.any():
        first = _fmt_bar(_ts(t[0]), intraday) if len(t) else None
        last = _fmt_bar(_ts(t[-1]), intraday) if len(t) else None
        if first and _to_date(first) > end:
            msg = f'{sym} has no bars between {start} and {end}: its history starts {first}.'
        elif last and _to_date(last) < start:
            msg = f'{sym} has no bars between {start} and {end}: its history ends {last}.'
        else:
            msg = f'{sym} has no {interval} bars between {start} and {end}.'
        raise PriceDataError(sym, 'NO_DATA_IN_RANGE', msg, first, last)
    c = {k: v[m] for k, v in e.cols.items()}
    idx = pd.DatetimeIndex(t[m])
    # product of splits strictly AFTER each bar, over the whole cached history (to the last fetch)
    spl_all = e.cols['splits']
    after_all = np.ones(len(spl_all))
    if len(spl_all) > 1:
        after_all[:-1] = np.cumprod(spl_all[::-1])[::-1][1:]
    after = after_all[m]
    if adjust == 'split+dividend':
        f = c['adj_close'] / c['close']
        df = pd.DataFrame({'Open': c['open'] * f, 'High': c['high'] * f, 'Low': c['low'] * f, 'Close': c['adj_close'],
                           'Volume': c['volume'] * after}, index=idx)
    elif adjust == 'split':
        df = pd.DataFrame({'Open': c['open'] / after, 'High': c['high'] / after, 'Low': c['low'] / after,
                           'Close': c['close'] / after, 'Volume': c['volume'] * after}, index=idx)
    else:
        df = pd.DataFrame({'Open': c['open'], 'High': c['high'], 'Low': c['low'], 'Close': c['close'],
                           'Volume': c['volume'], 'Dividends': c['dividends'], 'Splits': c['splits']}, index=idx)
    if interval == '4h':
        df = _resample_4h(df)
    meta = e.meta
    first_bar = _fmt_bar(df.index[0], intraday)
    last_bar = _fmt_bar(df.index[-1], intraday)
    provisional = bool(meta.get('lastBarProvisional')) and df.index[-1] == _ts(e.t[-1])
    prov = {
        'symbol': sym, 'source': meta.get('source', 'yahoo'), 'interval': interval, 'adjustment': adjust,
        'bars': int(len(df)), 'firstBar': first_bar, 'lastBar': last_bar,
        'requestedStart': start.isoformat(), 'requestedEnd': end.isoformat(),
        'currency': meta.get('currency') or currency_for_symbol(sym), 'currencySource': meta.get('currencySource'),
        'exchangeTz': meta.get('exchangeTz'), 'cacheAsOf': meta.get('fetchedAt'), 'missingBars': None,
        'lastBarProvisional': provisional, 'stale': bool(meta.get('stale')),
        'firstAvailable': meta.get('firstBar'), 'listingKnown': bool(meta.get('listingKnown')),
    }
    if meta.get('staleReason'):
        prov['staleReason'] = meta['staleReason']
    return df, prov


def _resample_4h(df):
    agg = {'Open': 'first', 'High': 'max', 'Low': 'min', 'Close': 'last', 'Volume': 'sum'}
    for c in ('Dividends', 'Splits'):
        if c in df.columns:
            agg[c] = 'sum' if c == 'Dividends' else 'prod'
    out = df.resample('4h', origin='start_day', label='left').agg(agg)
    return out.dropna(subset=['Close'])


# ════════════════════════════════════════════════════════════════════════════
# Module-level convenience
# ════════════════════════════════════════════════════════════════════════════

_default_store: Optional[PriceStore] = None


def default_root() -> Path:
    """The store directory: <data_root>/cache/scripts/backtesting/prices/<STORE_VERSION>, through
    scripts/fincept_paths.py (base.paths.cache_root) - never the home or temp folder."""
    return cache_root('prices') / STORE_VERSION


def default_store() -> PriceStore:
    global _default_store
    if _default_store is None or _default_store.root != default_root():
        _default_store = PriceStore()
    return _default_store


def get_bars(symbols: Sequence[str], start, end, interval: str = '1d', adjust: str = 'split+dividend',
             refresh: bool = False, progress: bool = True, store: Optional[PriceStore] = None) -> BarsResult:
    """See module doc. Never raises for a symbol problem: look at result.errors."""
    return (store or default_store()).get(symbols, start, end, interval=interval, adjust=adjust,
                                         refresh=refresh, progress=progress)


def get_frame(symbol: str, start, end, interval: str = '1d', adjust: str = 'split+dividend',
              store: Optional[PriceStore] = None):
    """One symbol's DataFrame; raises PriceDataError with a plain message."""
    res = get_bars([symbol], start, end, interval=interval, adjust=adjust, store=store, progress=False)
    s = normalize_symbol(symbol)
    if s in res.errors:
        raise res.errors[s]
    return res.frames[s]


def risk_free_series(dates, symbol: str = '^IRX', store: Optional[PriceStore] = None,
                     min_coverage: float = 0.9) -> Tuple[Optional[np.ndarray], Dict[str, Any]]:
    """Annual risk-free yield (fraction) for each date, from a Yahoo yield series (default the
    13-week US T-bill, ^IRX, quoted in percent). Forward-filled onto `dates` (yield of the last
    session on or before each date). (None, info) when unavailable or covering < min_coverage."""
    import pandas as pd
    idx = pd.DatetimeIndex(dates)
    if idx.tz is not None:
        idx = idx.tz_localize(None)
    info: Dict[str, Any] = {'symbol': symbol, 'source': 'yahoo'}
    if len(idx) == 0:
        return None, dict(info, reason='no dates')
    start = (idx[0] - pd.Timedelta(days=10)).date()
    end = idx[-1].date()
    try:
        res = get_bars([symbol], start, end, interval='1d', adjust='split', store=store, progress=False)
    except Exception as e:
        return None, dict(info, reason=str(e)[:160])
    s = normalize_symbol(symbol)
    if s in res.errors or s not in res.frames:
        return None, dict(info, reason=str(res.errors.get(s, 'not loaded')))
    y = res.frames[s]['Close'].astype(float) / 100.0
    y = y[~y.index.duplicated(keep='last')]
    al = y.reindex(y.index.union(idx.normalize())).ffill().reindex(idx.normalize())
    cov = float(np.isfinite(al.values).mean())
    info.update({'coverage': cov, 'cacheAsOf': res.provenance[s].get('cacheAsOf'),
                 'networkCalls': res.network_calls})
    if cov < min_coverage:
        return None, dict(info, reason=f'{symbol} covers {cov:.0%} of the bars')
    vals = al.values.astype(float)
    vals = pd.Series(vals).ffill().bfill().to_numpy()
    info['annualMean'] = float(np.nanmean(vals))
    return vals, info


def get_actions(symbol: str, start, end, basis: str = 'raw', store: Optional[PriceStore] = None):
    """Dividends and splits of one symbol in [start, end] as a DataFrame[Dividends, Splits] (rows
    with an action only). basis 'raw': cash per share as paid at the time; 'split': per share in
    today's split-adjusted units (Yahoo's convention, e.g. AAPL's 2020-08-07 dividend 0.205
    instead of 0.82). Raises PriceDataError."""
    import pandas as pd
    if basis not in ('raw', 'split'):
        raise ValueError("basis must be 'raw' or 'split'")
    raw = get_frame(symbol, start, end, adjust='none', store=store)
    out = raw[['Dividends', 'Splits']].copy()
    if basis == 'split':
        st = store or default_store()
        e = st.read_entry(normalize_symbol(symbol), '1d')
        spl = e.cols['splits']
        after = np.ones(len(spl))
        if len(spl) > 1:
            after[:-1] = np.cumprod(spl[::-1])[::-1][1:]
        a = pd.Series(after, index=pd.DatetimeIndex(e.t)).reindex(out.index).fillna(1.0)
        out['Dividends'] = out['Dividends'] / a
    return out[(out['Dividends'] != 0) | (out['Splits'] != 1.0)]
