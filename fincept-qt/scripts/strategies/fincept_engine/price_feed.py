# ============================================================================
# Fincept Terminal - Strategy Engine: market data from the shared price store
#
# Every price the engine reads - the strategy's symbols, history(), indicator warm-up,
# a universe's candidate pool, ^VIX / ^IRX for option pricing, continuous futures,
# splits and dividends - comes from base.price_store (scripts/Analytics/backtesting/base),
# the one on-disk OHLCV cache every backtesting engine shares:
#   * exchange SESSION dates (no UTC shift), split and dividend data stored once per
#     symbol, the adjustment chosen at read time ('split+dividend', 'split', 'none');
#   * warm: zero Yahoo requests; cold: one request per missing symbol; the store's own
#     freshness rules decide when a symbol's tail is refreshed;
#   * this module adds a per-process memo (a symbol read once is served from memory to
#     every later request inside the loaded range - optimise / walk-forward trials and
#     repeated history() calls do not touch the disk again) and a ledger of what was
#     served (provenance of the run: bars, first / last bar, cache time, downloads).
#
# The engine's own formats are kept (end dates EXCLUSIVE, as the engine has always
# called its fetchers):
#   daily_dicts(tickers, start, end_excl, mode)  -> {ticker: [{time, open, high, low, close,
#                                                   volume(, adj_close)}]}  mode 'adj' | 'raw'
#   bars(tickers, start, end_excl, interval, mode) -> {ticker: {'bars': [[time, o, h, l, c, v(, adj)]],
#                                                   'dividends': [[date, amount]], 'splits': [[date, ratio]]}}
#   actions(ticker, start, end_excl=None)          -> {'dividends': [[date, amount]] (per share in today's
#                                                   split-adjusted units, Yahoo's convention),
#                                                   'dividends_raw': [[date, cash as paid]], 'splits': [[date, ratio]]}
# mode 'adj': OHLC split- and dividend-adjusted. mode 'raw': OHLC split-adjusted only (relative to
# the store's latest data), the adjusted close as the 7th column / 'adj_close'; bars(..., 'raw')
# lists every split from `start` up to the store's last bar (not only inside the window) so a
# caller can turn split-adjusted prices back into prices as traded without a second request.
# Tickers the store cannot serve are absent from the result (the reason is in feed.errors).
# ============================================================================

import os
import sys
import threading
from datetime import datetime, date, timedelta
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional

_BT_DIR = str(Path(__file__).resolve().parents[2] / 'Analytics' / 'backtesting')  # fincept_engine -> strategies -> scripts


def _ensure_base():
    """`base` (the backtesting core) importable: the engine also runs outside the provider (tests)."""
    if _BT_DIR not in sys.path:
        sys.path.append(_BT_DIR)


def _ps():
    _ensure_base()
    from base import price_store
    return price_store


def _d(v) -> date:
    if isinstance(v, datetime):
        return v.date()
    if isinstance(v, date):
        return v
    return date.fromisoformat(str(v).strip()[:10])


_ADJUST = {'adj': 'split+dividend', 'raw': 'split'}


class PriceFeed:
    """Price-store reads with a per-process memo and a ledger of what was served."""

    def __init__(self, store=None):
        self._store = store
        self._memo: Dict[tuple, tuple] = {}     # (symbol, interval, adjust) -> (start, end, frame, provenance)
        self._lock = threading.RLock()
        self.ledger: Dict[str, Dict[str, Any]] = {}
        self.errors: Dict[str, str] = {}
        self.network_calls = 0
        self.requests = 0

    # ---------------------------------------------------------------- bookkeeping
    def begin_run(self):
        """Start a new ledger (the memo stays: the bars do not change within a process)."""
        with self._lock:
            self.ledger = {}
            self.errors = {}
            self.network_calls = 0
            self.requests = 0

    def store(self):
        return self._store if self._store is not None else _ps().default_store()

    # ---------------------------------------------------------------- frames
    def frames(self, tickers: Iterable[str], start, end, interval: str = '1d',
               adjust: str = 'split+dividend') -> Dict[str, Any]:
        """{ticker: DataFrame[Open, High, Low, Close, Volume(, Dividends, Splits)]} for [start, end]
        (both INCLUSIVE dates), from the memo or the price store."""
        ps = _ps()
        s, e = _d(start), _d(end)
        if e < s:
            return {}
        out: Dict[str, Any] = {}
        need: List[str] = []
        with self._lock:
            for t in dict.fromkeys(str(x).strip() for x in tickers):
                if not t:
                    continue
                key = (ps.normalize_symbol(t), interval, adjust)
                m = self._memo.get(key)
                if m is not None and m[0] <= s and e <= m[1]:
                    out[t] = _window(m[2], s, e)
                    self._note(t, m[3], out[t])
                else:
                    need.append(t)
            if not need:
                return out
            self.requests += 1
            res = ps.get_bars(need, s, e, interval=interval, adjust=adjust, progress=False, store=self._store)
            self.network_calls += int(res.network_calls or 0)
            for t in need:
                ns = ps.normalize_symbol(t)
                f = res.frames.get(ns)
                if f is None or len(f) == 0:
                    err = res.errors.get(ns)
                    self.errors[t] = str(err) if err is not None else 'not loaded'
                    continue
                prov = dict(res.provenance.get(ns) or {})
                key = (ns, interval, adjust)
                old = self._memo.get(key)
                if old is None or (s <= old[0] and old[1] <= e):
                    self._memo[key] = (s, e, f, prov)
                out[t] = f
                self.errors.pop(t, None)
                self._note(t, prov, f)
        return out

    def _note(self, t, prov, frame):
        """Ledger: the widest window served per ticker."""
        if frame is None or len(frame) == 0:
            return
        first, last = frame.index[0], frame.index[-1]
        rec = self.ledger.get(t)
        if rec is None:
            rec = self.ledger[t] = {'symbol': t, 'source': prov.get('source', 'yahoo'),
                                    'currency': prov.get('currency'), 'cacheAsOf': prov.get('cacheAsOf'),
                                    'fromCache': prov.get('fromCache'), 'stale': prov.get('stale', False),
                                    'lastBarProvisional': prov.get('lastBarProvisional', False),
                                    'firstAvailable': prov.get('firstAvailable'),
                                    'listingKnown': prov.get('listingKnown'), '_first': first, '_last': last,
                                    'adjustments': set()}
        rec['_first'] = min(rec['_first'], first)
        rec['_last'] = max(rec['_last'], last)
        if prov.get('adjustment'):
            rec['adjustments'].add(prov['adjustment'])
        if prov.get('fromCache') is False:
            rec['fromCache'] = False

    def provenance(self) -> Dict[str, Dict[str, Any]]:
        """{ticker: {source, currency, cacheAsOf, fromCache, loadedFrom, loadedTo, adjustments}}."""
        out = {}
        for t, r in self.ledger.items():
            d = {k: v for k, v in r.items() if not k.startswith('_') and k != 'adjustments'}
            d['loadedFrom'] = r['_first'].strftime('%Y-%m-%d')
            d['loadedTo'] = r['_last'].strftime('%Y-%m-%d')
            d['adjustments'] = sorted(r['adjustments'])
            out[t] = d
        return out

    # ---------------------------------------------------------------- engine formats
    def daily_dicts(self, tickers: Iterable[str], start, end_exclusive, mode: str = 'adj') -> Dict[str, List[Dict]]:
        """The runner's bar format (see module doc); end EXCLUSIVE."""
        s, e = _d(start), _d(end_exclusive) - timedelta(days=1)
        tickers = [str(x).strip() for x in tickers if str(x).strip()]
        if mode == 'raw':
            spl = self.frames(tickers, s, e, '1d', 'split')
            adj = self.frames(list(spl), s, e, '1d', 'split+dividend')
        else:
            spl = self.frames(tickers, s, e, '1d', 'split+dividend')
            adj = None
        out = {}
        for t, f in spl.items():
            a = adj.get(t) if adj is not None else None
            out[t] = _rows(f, a)
        return out

    def bars(self, tickers: Iterable[str], start, end_exclusive, interval: str = '1d',
             mode: str = 'adj') -> Dict[str, Dict[str, list]]:
        """yahoo_cache.fetch_bars format (see module doc); end EXCLUSIVE."""
        ps = _ps()
        iv = ps.normalize_interval(interval)
        intraday = ps.is_intraday(iv)
        s, e = _d(start), _d(end_exclusive) - timedelta(days=1)
        tickers = [str(x).strip() for x in tickers if str(x).strip()]
        main = self.frames(tickers, s, e, iv, _ADJUST.get(mode, 'split+dividend'))
        adj = self.frames(list(main), s, e, iv, 'split+dividend') if mode == 'raw' else None
        fmt = '%Y-%m-%d %H:%M:%S' if intraday else '%Y-%m-%d'
        out = {}
        for t, f in main.items():
            keys = [x.strftime(fmt) for x in f.index]
            cols = [f[c].to_numpy(dtype=float).tolist() for c in ('Open', 'High', 'Low', 'Close', 'Volume')]
            if adj is not None and t in adj:
                cols.append(adj[t]['Close'].reindex(f.index).to_numpy(dtype=float).tolist())
            rows = [list(r) for r in zip(keys, *cols)]
            # actions come from the daily entry (splits and dividends are stored once per symbol)
            win = self.actions(t, s, e + timedelta(days=1))
            splits = self.actions(t, s, None)['splits'] if mode == 'raw' else win['splits']
            out[t] = {'bars': rows, 'dividends': win['dividends'], 'splits': splits}
        return out

    def actions(self, ticker: str, start, end_exclusive=None) -> Dict[str, list]:
        """Dividends (split basis and as paid) and splits in [start, end_exclusive); end None: up to the
        store's last bar (what turns split-adjusted bars back into prices as traded)."""
        ps = _ps()
        s = _d(start)
        e_excl = _d(end_exclusive) if end_exclusive is not None else None
        ns = ps.normalize_symbol(ticker)
        try:
            ent = self.store().read_entry(ns, '1d', touch=False)
        except Exception:
            ent = None
        covers = ent is not None and len(ent.t) and (
            str(ent.meta.get('historyStart') or ent.meta.get('firstBar') or '9999')[:10] <= s.isoformat()
            or ent.meta.get('listingKnown'))
        if not covers:
            hi = (e_excl - timedelta(days=1)) if e_excl is not None else s + timedelta(days=7)
            self.frames([ticker], s, max(s, hi), '1d', 'none')
            try:
                ent = self.store().read_entry(ns, '1d', touch=False)
            except Exception:
                ent = None
        if ent is None or not len(ent.t):
            return {'dividends': [], 'dividends_raw': [], 'splits': []}
        import numpy as np
        days = ent.t.astype('datetime64[ns]').astype('datetime64[D]')
        spl = ent.cols['splits']
        after = np.ones(len(spl))
        if len(spl) > 1:
            after[:-1] = np.cumprod(spl[::-1])[::-1][1:]
        m = days >= np.datetime64(s.isoformat(), 'D')
        if e_excl is not None:
            m &= days < np.datetime64(e_excl.isoformat(), 'D')
        dv = ent.cols['dividends']
        divs, divs_raw, splits = [], [], []
        for i in np.nonzero(m)[0]:
            day = str(days[i])
            if dv[i] and np.isfinite(dv[i]):
                divs.append([day, float(dv[i] / after[i])])
                divs_raw.append([day, float(dv[i])])
            if spl[i] > 0 and spl[i] != 1.0:
                splits.append([day, float(spl[i])])
        return {'dividends': divs, 'dividends_raw': divs_raw, 'splits': splits}


def _window(frame, s: date, e: date):
    import pandas as pd
    return frame.loc[(frame.index >= pd.Timestamp(s)) & (frame.index < pd.Timestamp(e) + pd.Timedelta(days=1))]


def _rows(f, adj=None) -> List[Dict]:
    """DataFrame -> [{time 'YYYY-MM-DD 00:00:00', open, high, low, close, volume(, adj_close)}]; each row
    also carries the parsed session date under '_dt' (BarSeries skips the string parsing)."""
    idx = f.index
    dts = idx.to_pydatetime()
    keys = idx.strftime('%Y-%m-%d 00:00:00')
    o, h, l, c, v = (f[x].to_numpy(dtype=float).tolist() for x in ('Open', 'High', 'Low', 'Close', 'Volume'))
    if adj is not None:
        a = adj['Close'].reindex(idx).to_numpy(dtype=float).tolist()
        return [{'time': keys[i], 'open': o[i], 'high': h[i], 'low': l[i], 'close': c[i], 'volume': v[i],
                 'adj_close': a[i], '_dt': dts[i]} for i in range(len(keys))]
    return [{'time': keys[i], 'open': o[i], 'high': h[i], 'low': l[i], 'close': c[i], 'volume': v[i], '_dt': dts[i]}
            for i in range(len(keys))]


_FEED: Optional[PriceFeed] = None
_FEED_LOCK = threading.Lock()


def default_feed() -> PriceFeed:
    """The process-wide feed (one memo per process; a pool worker builds its own)."""
    global _FEED
    with _FEED_LOCK:
        if _FEED is None:
            _FEED = PriceFeed()
        return _FEED


def set_default_store(store) -> PriceFeed:
    """Tests: serve the engine from `store` (a base.price_store.PriceStore) with a fresh memo."""
    global _FEED
    with _FEED_LOCK:
        _FEED = PriceFeed(store)
        return _FEED
