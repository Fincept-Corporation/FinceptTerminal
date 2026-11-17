"""
Vectorised indicators and signal helpers shared by the engines.

Every indicator is computed ONCE on the full history (no per-bar recomputation on a
truncated window - zipline's Z2 defect) and is causal: the value at bar t uses bars <= t only.
Signals are decided on bar t's close; base.simulator fills them at t+1's open by default.

    fast, slow = sma(close, 20), sma(close, 50)
    entries = crossover(fast, slow); exits = crossunder(fast, slow)
    rsi = rsi_wilder(close, 14)

Warm-up: a crossover needs a valid value on the previous bar too (`valid.shift(1)` gating), so
the first bar on which both averages exist is NOT a cross (vectorbt treated it as one).
"""

from __future__ import annotations

import numpy as np


def _a(x) -> np.ndarray:
    if hasattr(x, 'to_numpy'):
        x = x.to_numpy()
    return np.asarray(x, dtype=float)


def _shift1(a: np.ndarray, fill) -> np.ndarray:
    """a shifted down one bar along axis 0 (works for (n,) and (n, k))."""
    out = np.empty_like(a)
    out[:1] = fill
    out[1:] = a[:-1]
    return out


def _columnwise(fn, x, *args):
    """Apply a 1-D indicator to every column of a (n, k) array."""
    v = _a(x)
    if v.ndim == 1:
        return fn(v, *args)
    return np.column_stack([fn(v[:, j], *args) for j in range(v.shape[1])]) if v.shape[1] else v.copy()


def sma(x, n: int) -> np.ndarray:
    """Simple moving average (NaN for the first n-1 bars), O(len) with a cumulative sum."""
    v = _a(x)
    n = int(n)
    out = np.full(v.shape, np.nan)
    if n <= 0 or len(v) < n:
        return out
    c = np.cumsum(np.insert(np.where(np.isfinite(v), v, 0.0), 0, 0.0, axis=0), axis=0)
    out[n - 1:] = (c[n:] - c[:-n]) / n
    # Windows containing a NaN are NaN.
    bad = np.cumsum(np.insert((~np.isfinite(v)).astype(np.int64), 0, 0, axis=0), axis=0)
    has_nan = (bad[n:] - bad[:-n]) > 0
    out[n - 1:][has_nan] = np.nan
    return out


def ema(x, n: int, adjust: bool = False) -> np.ndarray:
    """Exponential moving average with alpha = 2/(n+1), seeded with the SMA of the first n bars
    (pandas ewm(span=n, adjust=False) after an SMA seed); NaN during warm-up. (n,) or (n, k)."""
    if _a(x).ndim == 2:
        return _columnwise(ema, x, n, adjust)
    import pandas as pd
    v = _a(x)
    n = int(n)
    out = np.full(len(v), np.nan)
    if n <= 0 or len(v) < n:
        return out
    seed = float(np.mean(v[:n]))
    s = pd.Series(np.concatenate([[seed], v[n:]]))
    e = s.ewm(span=n, adjust=adjust).mean().to_numpy()
    out[n - 1:] = e
    return out


def rsi_wilder(close, n: int = 14) -> np.ndarray:
    """Classic Wilder RSI: seed = simple mean of the first n changes, then recursive smoothing
    with alpha = 1/n. First value at bar n. Computed with pandas ewm (vectorised). (n,) or (n, k)."""
    if _a(close).ndim == 2:
        return _columnwise(rsi_wilder, close, n)
    import pandas as pd
    c = _a(close)
    n = int(n)
    out = np.full(len(c), np.nan)
    if len(c) <= n:
        return out
    d = np.diff(c)
    up = np.clip(d, 0, None)
    dn = np.clip(-d, 0, None)
    au0, ad0 = up[:n].mean(), dn[:n].mean()
    au = pd.Series(np.concatenate([[au0], up[n:]])).ewm(alpha=1.0 / n, adjust=False).mean().to_numpy()
    ad = pd.Series(np.concatenate([[ad0], dn[n:]])).ewm(alpha=1.0 / n, adjust=False).mean().to_numpy()
    with np.errstate(divide='ignore', invalid='ignore'):
        rs = au / ad
        r = np.where(ad == 0, 100.0, 100.0 - 100.0 / (1.0 + rs))
    out[n:] = r
    return out


def rsi_simple(close, n: int = 14) -> np.ndarray:
    """Cutler's RSI: simple moving averages of gains and losses (backtesting.py's flavour)."""
    c = _a(close)
    if c.ndim == 2:
        return _columnwise(rsi_simple, c, n)
    d = np.concatenate([[np.nan], np.diff(c)])
    up = sma(np.clip(d, 0, None), n)
    dn = sma(np.clip(-d, 0, None), n)
    with np.errstate(divide='ignore', invalid='ignore'):
        return np.where(dn == 0, np.where(np.isfinite(up), 100.0, np.nan), 100.0 - 100.0 / (1.0 + up / dn))


def crossover(a, b) -> np.ndarray:
    """True on bar t when a > b on t and a <= b on t-1, both bars valid (warm-up gated)."""
    a, b = np.broadcast_arrays(_a(a), _a(b))
    valid = np.isfinite(a) & np.isfinite(b)
    above = valid & (a > b)
    return above & _shift1(valid, False) & ~_shift1(above, False)


def crossunder(a, b) -> np.ndarray:
    a, b = np.broadcast_arrays(_a(a), _a(b))
    valid = np.isfinite(a) & np.isfinite(b)
    below = valid & (a < b)
    return below & _shift1(valid, False) & ~_shift1(below, False)


def cross_above_level(x, level: float) -> np.ndarray:
    """True on bar t when x > level on t and x <= level on t-1 (both valid)."""
    v = _a(x)
    return crossover(v, np.full(v.shape, float(level)))


def cross_below_level(x, level: float) -> np.ndarray:
    v = _a(x)
    return crossunder(v, np.full(v.shape, float(level)))


def state_to_events(state) -> tuple:
    """(entries, exits) from a boolean 'in the market' state series (enter when it turns on,
    exit when it turns off)."""
    s = np.asarray(state, dtype=bool)
    prev = _shift1(s, False)
    return s & ~prev, ~s & prev
