"""Shared test helpers: a fake Yahoo fetcher for the price store, module-level (picklable) evaluate
functions for the process-pool tests, and small builders."""
import json
import os
import sys
import time
from pathlib import Path

import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
BT_DIR = HERE.parent
if str(BT_DIR) not in sys.path:
    sys.path.insert(0, str(BT_DIR))

from base.price_store import FetchResult, PriceDataError  # noqa: E402

FIX = HERE / 'fixtures'


def yahoo_layout(adj: pd.DataFrame, tz: str = 'America/New_York', dividends=None, splits=None) -> pd.DataFrame:
    """An adjusted OHLCV frame dressed up as Yahoo history(auto_adjust=False, actions=True): no
    corporate actions, Adj Close = Close, exchange-local midnight timestamps."""
    out = adj[['Open', 'High', 'Low', 'Close', 'Volume']].copy()
    out['Adj Close'] = out['Close']
    out['Dividends'] = 0.0 if dividends is None else dividends
    out['Stock Splits'] = 0.0 if splits is None else splits
    idx = pd.DatetimeIndex(out.index)
    out.index = idx.tz_localize(tz) if tz else idx
    return out


def raw_to_yahoo(raw: pd.DataFrame, tz: str = 'America/New_York') -> pd.DataFrame:
    """Store-style RAW rows (open..close raw, adj_close, volume, dividends, splits) -> Yahoo's
    split-adjusted view as of the last row."""
    spl = raw['splits'].replace(0, 1.0).fillna(1.0).to_numpy()
    after = np.ones(len(spl))
    after[:-1] = np.cumprod(spl[::-1])[::-1][1:]
    v = {c: raw[c].to_numpy(dtype=float) for c in raw.columns}
    out = pd.DataFrame({'Open': v['open'] / after, 'High': v['high'] / after, 'Low': v['low'] / after,
                        'Close': v['close'] / after, 'Adj Close': v['adj_close'], 'Volume': v['volume'] * after,
                        'Dividends': v['dividends'] / after, 'Stock Splits': np.where(spl != 1.0, spl, 0.0)},
                       index=pd.DatetimeIndex(raw.index).tz_localize(tz))
    return out


class FakeYahoo:
    """Callable fetcher(symbol, interval, start, end) -> FetchResult over in-memory Yahoo-layout
    frames. `today` caps what is 'available'. Every call is logged (and appended to `log_file`
    when given, for multi-process tests)."""

    def __init__(self, frames, today=None, delay=0.0, log_file=None, meta=None):
        """today: the fake 'now' (also give the store clock=fake.clock so its freshness rules agree)."""
        self.frames = frames
        self.today = pd.Timestamp(today) if today is not None else None
        self.delay = delay
        self.log_file = log_file
        self.calls = []
        self.meta = meta or {}

    def clock(self):
        from datetime import timezone
        if self.today is None:
            return pd.Timestamp.now(tz='UTC').to_pydatetime()
        return (self.today + pd.Timedelta(hours=23)).to_pydatetime().replace(tzinfo=timezone.utc)

    def __call__(self, symbol, interval, start, end):
        self.calls.append((symbol, str(start), str(end)))
        if self.log_file:
            with open(self.log_file, 'a', encoding='utf-8') as f:
                f.write(json.dumps([symbol, str(start), os.getpid()]) + '\n')
        if self.delay:
            time.sleep(self.delay)
        if symbol not in self.frames:
            raise PriceDataError(symbol, 'UNKNOWN_SYMBOL', f'Yahoo Finance has no symbol {symbol}.')
        f = self.frames[symbol]
        naive = f.index.tz_localize(None) if f.index.tz is not None else f.index
        m = (naive >= pd.Timestamp(start)) & (naive <= pd.Timestamp(end))
        if self.today is not None:
            m &= naive <= self.today
        out = f[m]
        if len(out) == 0:
            raise PriceDataError(symbol, 'NO_DATA_IN_RANGE', f'{symbol} has no bars from {start}.')
        return FetchResult(out.copy(), dict(self.meta.get(symbol, {})))


def child_get_bars(root, symbols, start, end, log_file, delay):
    """Run in a spawned process: get bars through a PriceStore on `root` with a FakeYahoo over the
    SPY fixture, return a digest of what was served."""
    from base.price_store import PriceStore
    spy = pd.read_csv(FIX / 'SPY_1d.csv', index_col=0, parse_dates=[0])
    frames = {s: yahoo_layout(spy * (1.0 + 0.01 * i)) for i, s in enumerate(['AAA', 'BBB', 'CCC'])}
    store = PriceStore(root=Path(root), fetcher=FakeYahoo(frames, delay=delay, log_file=log_file), ttl_s=3600,
                       offline=False)
    res = store.get(symbols, start, end, progress=False)
    return {s: (len(f), float(f['Close'].sum())) for s, f in res.frames.items()}, \
        {s: str(e) for s, e in res.errors.items()}


# ── pool evaluate (module level, picklable) ────────────────────────────────

def sma_context(n_bars=None):
    spy = pd.read_csv(FIX / 'SPY_1d.csv', index_col=0, parse_dates=[0])
    if n_bars:
        spy = spy.iloc[:n_bars]
    return {'open': spy['Open'].to_numpy(), 'high': spy['High'].to_numpy(), 'low': spy['Low'].to_numpy(),
            'close': spy['Close'].to_numpy(), 'dates': spy.index}


def sma_evaluate(params, context, window):
    """SMA cross on the SPY fixture through base.simulator (python backend for determinism across
    processes); window = None or {'dataStart', 'scoreStart', 'end'}."""
    from base import signals as S
    from base.costs import CostModel
    from base.simulator import Simulator, panel_from_arrays
    a, s, e = (0, 0, len(context['close'])) if window is None else (window['dataStart'], window['scoreStart'],
                                                                     window['end'])
    sl = slice(a, e)
    c = context['close'][sl]
    f, w = S.sma(c, int(params['fastPeriod'])), S.sma(c, int(params['slowPeriod']))
    ent, ex = S.crossover(f, w), S.crossunder(f, w)
    p = panel_from_arrays(c, context['open'][sl], context['high'][sl], context['low'][sl],
                          dates=context['dates'][sl], symbols=['SPY'])
    sim = Simulator(p, costs=CostModel(commission_value=0.001, slippage_value=0.0005), backend='python')
    res = sim.run(entries=ent, exits=ex, start=s - a)
    r = res.returns[s - a:]
    trades = len([t for t in res.fill_arrays['qty'] if t > 0])
    return {'returns': r, 'trades': trades}


def periodic_evaluate(params, context, window):
    """Returns with the SAME statistics in every window (a fixed 20-bar pattern): walk-forward IS and
    OOS behaviour are identical by construction; the parameter scales the pattern."""
    n = context['n']
    a, s, e = (0, 0, n) if window is None else (window['dataStart'], window['scoreStart'], window['end'])
    base = np.array([0.004, -0.002, 0.003, -0.001, 0.002, -0.003, 0.005, -0.002, 0.001, -0.001] * 2)
    t = np.arange(s, e)
    return {'returns': base[t % 20] * float(params['k']), 'trades': 1}


def sleepy_evaluate(params, context, window):
    time.sleep(context.get('sleep', 0.05))
    return {'returns': np.full(50, 0.001 * params['k'])}
