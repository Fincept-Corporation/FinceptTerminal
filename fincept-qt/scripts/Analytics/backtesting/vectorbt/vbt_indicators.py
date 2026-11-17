"""
Indicators of the vectorbt engine: ONE id table shared by get_indicators, calculate_indicator,
indicator_signals, indicator_param_sweep and the strategies (audit T6).

Every indicator is computed once on a symbol's full history, vectorised, and is causal (the
value at bar t uses bars <= t only). Warm-up bars are NaN - never a filled-in 0 / 50 that a
signal rule could trigger on. Definitions (the usual ones, TA-Lib conventions):
  sma        simple moving average of the close
  ema        exponential average, alpha 2/(n+1), seeded with the SMA of the first n bars
  rsi        Wilder RSI (seed = mean of the first n changes, then Wilder smoothing)
  macd       EMA(fast) - EMA(slow); signal = EMA(macd, signal); histogram = macd - signal
  stochastic %K = 100 (C - LL_n) / (HH_n - LL_n) on highs / lows; %D = SMA(%K, d)
  williams_r -100 (HH_n - C) / (HH_n - LL_n)
  cci        (TP - SMA_n(TP)) / (0.015 x mean absolute deviation_n), TP = (H + L + C) / 3
  atr        Wilder average of the true range
  adx        Wilder +DI / -DI / ADX
  bbands     SMA_n +- k x population standard deviation_n (+ bandwidth, %B)
  keltner    EMA_n(close) +- k x ATR_m
  donchian   highest high / lowest low of the last n bars (+ middle)
  mstd       population standard deviation of the close over n bars
  zscore     (C - SMA_n) / population std_n
  momentum   rate of change C_t / C_{t-n} - 1 (fraction)
  obv        on-balance volume (cumulative signed volume)
  vwap       volume-weighted average price anchored at the first bar of the period

TA  - a per-symbol accessor with memoised indicators (each distinct (indicator, parameters)
      is computed once per symbol; optimisation workers keep one TA per symbol for all trials).
INDICATORS - the id table: name, category, params (id / name / type / default / min / max /
      step), outputs, primary output, signal support (threshold levels), sweep parameter.
"""

from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Tuple

import numpy as np

# ════════════════════════════════════════════════════════════════════════════
# Array helpers (1-D float arrays; NaN = not yet defined)
# ════════════════════════════════════════════════════════════════════════════


def _a(x) -> np.ndarray:
    if hasattr(x, 'to_numpy'):
        x = x.to_numpy()
    return np.asarray(x, dtype=float).reshape(-1)


def _first_valid(v: np.ndarray) -> int:
    ok = np.isfinite(v)
    return int(np.argmax(ok)) if ok.any() else len(v)


def rolling_sum(v: np.ndarray, n: int) -> np.ndarray:
    v = _a(v)
    n = int(n)
    out = np.full(len(v), np.nan)
    if n <= 0 or len(v) < n:
        return out
    bad = ~np.isfinite(v)
    c = np.concatenate([[0.0], np.cumsum(np.where(bad, 0.0, v))])
    b = np.concatenate([[0], np.cumsum(bad.astype(np.int64))])
    s = c[n:] - c[:-n]
    s[(b[n:] - b[:-n]) > 0] = np.nan
    out[n - 1:] = s
    return out


def sma(x, n: int) -> np.ndarray:
    return rolling_sum(x, n) / float(n) if int(n) > 0 else np.full(len(_a(x)), np.nan)


def rolling_std(x, n: int, ddof: int = 0) -> np.ndarray:
    v = _a(x)
    n = int(n)
    if n <= ddof:
        return np.full(len(v), np.nan)
    m = sma(v, n)
    m2 = sma(v * v, n)
    var = np.maximum(m2 - m * m, 0.0) * (n / (n - ddof))
    return np.sqrt(var)


def _windows(v: np.ndarray, n: int):
    from numpy.lib.stride_tricks import sliding_window_view
    return sliding_window_view(v, n)


def rolling_max(x, n: int) -> np.ndarray:
    v = _a(x)
    n = int(n)
    out = np.full(len(v), np.nan)
    if n <= 0 or len(v) < n:
        return out
    out[n - 1:] = np.max(_windows(v, n), axis=1)   # NaN inside a window -> NaN
    return out


def rolling_min(x, n: int) -> np.ndarray:
    v = _a(x)
    n = int(n)
    out = np.full(len(v), np.nan)
    if n <= 0 or len(v) < n:
        return out
    out[n - 1:] = np.min(_windows(v, n), axis=1)
    return out


def _recursive(v: np.ndarray, n: int, alpha: float) -> np.ndarray:
    """Exponential smoothing seeded with the SMA of the first n valid values; NaN before."""
    import pandas as pd
    v = _a(v)
    out = np.full(len(v), np.nan)
    f = _first_valid(v)
    if n <= 0 or len(v) - f < n:
        return out
    seed = float(np.mean(v[f:f + n]))
    s = pd.Series(np.concatenate([[seed], v[f + n:]]))
    out[f + n - 1:] = s.ewm(alpha=alpha, adjust=False, ignore_na=True).mean().to_numpy()
    return out


def ema(x, n: int) -> np.ndarray:
    n = int(n)
    return _recursive(x, n, 2.0 / (n + 1.0))


def wilder(x, n: int) -> np.ndarray:
    n = int(n)
    return _recursive(x, n, 1.0 / n)


def rsi(close, n: int = 14) -> np.ndarray:
    """Wilder RSI (identical to base.signals.rsi_wilder)."""
    from base.signals import rsi_wilder
    return rsi_wilder(_a(close), int(n))


def true_range(high, low, close) -> np.ndarray:
    h, lo, c = _a(high), _a(low), _a(close)
    pc = np.concatenate([[np.nan], c[:-1]])
    tr = np.nanmax(np.vstack([h - lo, np.abs(h - pc), np.abs(lo - pc)]), axis=0)
    tr[0] = h[0] - lo[0]
    return tr


def atr(high, low, close, n: int = 14) -> np.ndarray:
    return wilder(true_range(high, low, close), int(n))


def macd(close, fast: int = 12, slow: int = 26, signal: int = 9) -> Dict[str, np.ndarray]:
    fast, slow, signal = int(fast), int(slow), int(signal)
    if fast >= slow:
        raise ValueError(f'MACD needs a fast period ({fast}) shorter than the slow period ({slow})')
    line = ema(close, fast) - ema(close, slow)
    sig = ema(line, signal)
    return {'macd': line, 'signal': sig, 'histogram': line - sig}


def stochastic(high, low, close, k: int = 14, d: int = 3) -> Dict[str, np.ndarray]:
    hh, ll = rolling_max(high, k), rolling_min(low, k)
    den = hh - ll
    c = _a(close)
    with np.errstate(divide='ignore', invalid='ignore'):
        pk = np.where(den > 0, 100.0 * (c - ll) / den, np.where(np.isfinite(den), 50.0, np.nan))
    return {'k': pk, 'd': sma(pk, d)}


def williams_r(high, low, close, n: int = 14) -> np.ndarray:
    hh, ll = rolling_max(high, n), rolling_min(low, n)
    den = hh - ll
    c = _a(close)
    with np.errstate(divide='ignore', invalid='ignore'):
        return np.where(den > 0, -100.0 * (hh - c) / den, np.where(np.isfinite(den), -50.0, np.nan))


def cci(high, low, close, n: int = 20) -> np.ndarray:
    tp = (_a(high) + _a(low) + _a(close)) / 3.0
    n = int(n)
    out = np.full(len(tp), np.nan)
    if n <= 0 or len(tp) < n:
        return out
    w = _windows(tp, n)
    m = w.mean(axis=1)
    md = np.abs(w - m[:, None]).mean(axis=1)
    with np.errstate(divide='ignore', invalid='ignore'):
        out[n - 1:] = np.where(md > 0, (tp[n - 1:] - m) / (0.015 * md), 0.0)
    return out


def adx(high, low, close, n: int = 14) -> Dict[str, np.ndarray]:
    h, lo = _a(high), _a(low)
    up = np.concatenate([[np.nan], np.diff(h)])
    dn = np.concatenate([[np.nan], -np.diff(lo)])
    pdm = np.where((up > dn) & (up > 0), up, 0.0)
    mdm = np.where((dn > up) & (dn > 0), dn, 0.0)
    pdm[0] = mdm[0] = np.nan
    tr = true_range(h, lo, close)
    tr[0] = np.nan
    a = wilder(tr, n)
    with np.errstate(divide='ignore', invalid='ignore'):
        pdi = 100.0 * wilder(pdm, n) / a
        mdi = 100.0 * wilder(mdm, n) / a
        s = pdi + mdi
        dx = np.where(s > 0, 100.0 * np.abs(pdi - mdi) / s, 0.0)
    dx = np.where(np.isfinite(pdi) & np.isfinite(mdi), dx, np.nan)
    return {'adx': wilder(dx, n), 'plus_di': pdi, 'minus_di': mdi}


def bbands(close, n: int = 20, k: float = 2.0) -> Dict[str, np.ndarray]:
    c = _a(close)
    mid = sma(c, n)
    sd = rolling_std(c, n, ddof=0)
    up, lo = mid + float(k) * sd, mid - float(k) * sd
    with np.errstate(divide='ignore', invalid='ignore'):
        bw = np.where(mid > 0, (up - lo) / mid, np.nan)
        pb = np.where(up > lo, (c - lo) / (up - lo), 0.5)
    pb = np.where(np.isfinite(mid), pb, np.nan)
    return {'upper': up, 'middle': mid, 'lower': lo, 'bandwidth': bw, 'percent_b': pb}


def keltner(high, low, close, n: int = 20, atr_n: int = 10, mult: float = 2.0) -> Dict[str, np.ndarray]:
    mid = ema(close, n)
    r = atr(high, low, close, atr_n)
    return {'upper': mid + float(mult) * r, 'middle': mid, 'lower': mid - float(mult) * r}


def donchian(high, low, n: int = 20) -> Dict[str, np.ndarray]:
    up, lo = rolling_max(high, n), rolling_min(low, n)
    return {'upper': up, 'lower': lo, 'middle': (up + lo) / 2.0}


def mstd(close, n: int = 20) -> np.ndarray:
    return rolling_std(close, n, ddof=0)


def zscore(close, n: int = 20) -> np.ndarray:
    c = _a(close)
    sd = rolling_std(c, n, ddof=0)
    with np.errstate(divide='ignore', invalid='ignore'):
        return np.where(sd > 0, (c - sma(c, n)) / sd, np.where(np.isfinite(sd), 0.0, np.nan))


def momentum(close, n: int = 20) -> np.ndarray:
    c = _a(close)
    n = int(n)
    out = np.full(len(c), np.nan)
    if 0 < n < len(c):
        with np.errstate(divide='ignore', invalid='ignore'):
            out[n:] = c[n:] / c[:-n] - 1.0
    return out


def obv(close, volume) -> np.ndarray:
    c, v = _a(close), np.nan_to_num(_a(volume))
    d = np.sign(np.concatenate([[0.0], np.diff(c)]))
    return np.cumsum(np.nan_to_num(d) * v)


def vwap(high, low, close, volume) -> np.ndarray:
    tp = (_a(high) + _a(low) + _a(close)) / 3.0
    v = np.nan_to_num(_a(volume))
    cv = np.cumsum(v)
    with np.errstate(divide='ignore', invalid='ignore'):
        return np.where(cv > 0, np.cumsum(tp * v) / cv, np.nan)


def realised_vol(close, n: int = 20, ppy: float = 252.0) -> np.ndarray:
    """Annualised std of the last n close-to-close returns, known at each bar's close."""
    c = _a(close)
    r = np.concatenate([[np.nan], c[1:] / c[:-1] - 1.0])
    return rolling_std(r, n, ddof=1) * np.sqrt(ppy)


# ════════════════════════════════════════════════════════════════════════════
# Per-symbol memoised accessor
# ════════════════════════════════════════════════════════════════════════════

class TA:
    """Indicators of ONE symbol's bars, each (indicator, parameters) computed once."""

    def __init__(self, open_, high, low, close, volume=None):
        self.c = _a(close)
        self.o = _a(open_) if open_ is not None else self.c
        self.h = _a(high) if high is not None else np.maximum(self.o, self.c)
        self.l = _a(low) if low is not None else np.minimum(self.o, self.c)
        self.v = _a(volume) if volume is not None else np.zeros(len(self.c))
        self.n = len(self.c)
        self._cache: Dict[tuple, Any] = {}

    def memo(self, key: tuple, fn: Callable[[], Any]):
        """Value of fn() computed once per key (key: a hashable tuple naming indicator + parameters)."""
        r = self._cache.get(key)
        if r is None:
            r = fn()
            self._cache[key] = r
        return r

    _m = memo

    def sma(self, n):
        return self._m(('sma', int(n)), lambda: sma(self.c, n))

    def ema(self, n):
        return self._m(('ema', int(n)), lambda: ema(self.c, n))

    def rsi(self, n):
        return self._m(('rsi', int(n)), lambda: rsi(self.c, n))

    def macd(self, f, s, g):
        return self._m(('macd', int(f), int(s), int(g)), lambda: macd(self.c, f, s, g))

    def stoch(self, k, d):
        return self._m(('stoch', int(k), int(d)), lambda: stochastic(self.h, self.l, self.c, k, d))

    def williams_r(self, n):
        return self._m(('wr', int(n)), lambda: williams_r(self.h, self.l, self.c, n))

    def cci(self, n):
        return self._m(('cci', int(n)), lambda: cci(self.h, self.l, self.c, n))

    def atr(self, n):
        return self._m(('atr', int(n)), lambda: atr(self.h, self.l, self.c, n))

    def adx(self, n):
        return self._m(('adx', int(n)), lambda: adx(self.h, self.l, self.c, n))

    def bbands(self, n, k):
        return self._m(('bb', int(n), float(k)), lambda: bbands(self.c, n, k))

    def keltner(self, n, an, m):
        return self._m(('kc', int(n), int(an), float(m)), lambda: keltner(self.h, self.l, self.c, n, an, m))

    def donchian(self, n):
        return self._m(('dc', int(n)), lambda: donchian(self.h, self.l, n))

    def mstd(self, n):
        return self._m(('mstd', int(n)), lambda: mstd(self.c, n))

    def zscore(self, n):
        return self._m(('z', int(n)), lambda: zscore(self.c, n))

    def momentum(self, n):
        return self._m(('mom', int(n)), lambda: momentum(self.c, n))

    def obv(self):
        return self._m(('obv',), lambda: obv(self.c, self.v))

    def vwap(self):
        return self._m(('vwap',), lambda: vwap(self.h, self.l, self.c, self.v))

    def vol(self, n, ppy):
        return self._m(('vol', int(n), float(ppy)), lambda: realised_vol(self.c, n, ppy))


# ════════════════════════════════════════════════════════════════════════════
# The id table
# ════════════════════════════════════════════════════════════════════════════

def _p(pid, name, default, lo, hi, step=1, typ='int'):
    return {'id': pid, 'name': name, 'type': typ, 'default': default, 'min': lo, 'max': hi, 'step': step}


_PERIOD = _p('period', 'Period', 14, 2, 400)

# compute(ta, params) -> 1-D array or {output: array}
INDICATORS: Dict[str, Dict[str, Any]] = {
    'sma': {'name': 'SMA', 'category': 'Trend', 'description': 'Simple moving average of the close',
            'params': [_p('period', 'Period', 20, 2, 400)], 'scale': 'price',
            'compute': lambda ta, p: ta.sma(p['period'])},
    'ema': {'name': 'EMA', 'category': 'Trend', 'description': 'Exponential moving average (SMA-seeded)',
            'params': [_p('period', 'Period', 20, 2, 400)], 'scale': 'price',
            'compute': lambda ta, p: ta.ema(p['period'])},
    'macd': {'name': 'MACD', 'category': 'Trend', 'description': 'MACD line, signal line, histogram',
             'params': [_p('fastPeriod', 'Fast Period', 12, 2, 100), _p('slowPeriod', 'Slow Period', 26, 3, 200),
                        _p('signalPeriod', 'Signal Period', 9, 2, 50)],
             'outputs': ['macd', 'signal', 'histogram'], 'primary': 'macd', 'scale': 'oscillator',
             'levels': (0.0, 0.0), 'periodParam': 'fastPeriod',
             'compute': lambda ta, p: ta.macd(p['fastPeriod'], p['slowPeriod'], p['signalPeriod'])},
    'adx': {'name': 'ADX', 'category': 'Trend', 'description': 'Average Directional Index with +DI / -DI (Wilder)',
            'params': [_p('period', 'Period', 14, 2, 100)], 'outputs': ['adx', 'plus_di', 'minus_di'],
            'primary': 'adx', 'scale': 'oscillator', 'levels': (20.0, 25.0),
            'compute': lambda ta, p: ta.adx(p['period'])},
    'keltner': {'name': 'Keltner', 'category': 'Trend', 'description': 'Keltner channels: EMA +- k x ATR',
                'params': [_p('period', 'EMA Period', 20, 2, 200), _p('atrPeriod', 'ATR Period', 10, 2, 100),
                           _p('multiplier', 'Multiplier', 2.0, 0.5, 5.0, 0.1, 'float')],
                'outputs': ['upper', 'middle', 'lower'], 'primary': 'middle', 'scale': 'price', 'channel': True,
                'compute': lambda ta, p: ta.keltner(p['period'], p['atrPeriod'], p['multiplier'])},
    'vwap': {'name': 'VWAP', 'category': 'Trend', 'description': 'Volume-weighted average price, anchored at the '
                                                                 'first bar of the period',
             'params': [], 'scale': 'price', 'compute': lambda ta, p: ta.vwap()},
    'rsi': {'name': 'RSI', 'category': 'Momentum', 'description': 'Relative Strength Index (Wilder)',
            'params': [_p('period', 'Period', 14, 2, 100)], 'scale': 'oscillator', 'levels': (30.0, 70.0),
            'compute': lambda ta, p: ta.rsi(p['period'])},
    'stochastic': {'name': 'Stochastic', 'category': 'Momentum', 'description': 'Stochastic oscillator %K / %D',
                   'params': [_p('kPeriod', '%K Period', 14, 2, 100), _p('dPeriod', '%D Period', 3, 1, 50)],
                   'outputs': ['k', 'd'], 'primary': 'k', 'scale': 'oscillator', 'levels': (20.0, 80.0),
                   'periodParam': 'kPeriod', 'compute': lambda ta, p: ta.stoch(p['kPeriod'], p['dPeriod'])},
    'momentum': {'name': 'Momentum', 'category': 'Momentum', 'description': 'Rate of change over n bars (fraction)',
                 'params': [_p('period', 'Lookback', 20, 1, 400)], 'scale': 'oscillator', 'levels': (-0.05, 0.05),
                 'compute': lambda ta, p: ta.momentum(p['period'])},
    'williams_r': {'name': 'Williams %R', 'category': 'Momentum', 'description': 'Williams percent range (-100..0)',
                   'params': [_p('period', 'Period', 14, 2, 100)], 'scale': 'oscillator', 'levels': (-80.0, -20.0),
                   'compute': lambda ta, p: ta.williams_r(p['period'])},
    'cci': {'name': 'CCI', 'category': 'Momentum', 'description': 'Commodity Channel Index',
            'params': [_p('period', 'Period', 20, 2, 200)], 'scale': 'oscillator', 'levels': (-100.0, 100.0),
            'compute': lambda ta, p: ta.cci(p['period'])},
    'bbands': {'name': 'Bollinger Bands', 'category': 'Volatility',
               'description': 'SMA +- k population standard deviations, bandwidth, %B',
               'params': [_p('period', 'Period', 20, 2, 400), _p('stdDev', 'Std Dev', 2.0, 0.5, 5.0, 0.1, 'float')],
               'outputs': ['upper', 'middle', 'lower', 'bandwidth', 'percent_b'], 'primary': 'middle',
               'scale': 'price', 'channel': True,
               'compute': lambda ta, p: ta.bbands(p['period'], p['stdDev'])},
    'atr': {'name': 'ATR', 'category': 'Volatility', 'description': 'Average True Range (Wilder)',
            'params': [_p('period', 'Period', 14, 2, 200)], 'scale': 'volatility',
            'compute': lambda ta, p: ta.atr(p['period'])},
    'mstd': {'name': 'MSTD', 'category': 'Volatility', 'description': 'Moving standard deviation of the close',
             'params': [_p('period', 'Period', 20, 2, 400)], 'scale': 'volatility',
             'compute': lambda ta, p: ta.mstd(p['period'])},
    'donchian': {'name': 'Donchian', 'category': 'Volatility', 'description': 'Highest high / lowest low channel',
                 'params': [_p('period', 'Period', 20, 2, 400)], 'outputs': ['upper', 'lower', 'middle'],
                 'primary': 'middle', 'scale': 'price', 'channel': True,
                 'compute': lambda ta, p: ta.donchian(p['period'])},
    'obv': {'name': 'OBV', 'category': 'Volume', 'description': 'On-balance volume', 'params': [], 'scale': 'volume',
            'compute': lambda ta, p: ta.obv()},
    'zscore': {'name': 'Z-Score', 'category': 'Statistical', 'description': 'Standard deviations from the moving mean',
               'params': [_p('period', 'Period', 20, 2, 400)], 'scale': 'oscillator', 'levels': (-2.0, 2.0),
               'compute': lambda ta, p: ta.zscore(p['period'])},
}

# Aliases accepted from callers (the screen, older payloads). Anything else is an error.
ALIASES = {'ma': 'sma', 'stoch': 'stochastic', 'bb': 'bbands', 'bollinger': 'bbands', 'roc': 'momentum',
           'willr': 'williams_r', 'williams': 'williams_r', 'z_score': 'zscore', 'std': 'mstd'}

SIGNAL_MODES = ('crossover_signals', 'threshold_signals', 'breakout_signals', 'mean_reversion_signals',
                'signal_filter')
_MODE_ALIASES = {'crossover': 'crossover_signals', 'threshold': 'threshold_signals', 'breakout': 'breakout_signals',
                 'mean_reversion': 'mean_reversion_signals', 'filter': 'signal_filter'}
FILTER_INDICATORS = ('adx', 'atr', 'mstd', 'zscore', 'rsi', 'cci', 'williams_r', 'stochastic', 'momentum')


def resolve_id(indicator: Any) -> str:
    """Canonical indicator id; ValueError for an unknown id (never a silent substitute)."""
    key = str(indicator or '').strip().lower()
    key = ALIASES.get(key, key)
    if key not in INDICATORS:
        raise ValueError(f'Unknown indicator {indicator!r}. Available: {", ".join(sorted(INDICATORS))}.')
    return key


def resolve_mode(mode: Any) -> str:
    m = str(mode or 'crossover_signals').strip().lower()
    m = _MODE_ALIASES.get(m, m)
    if m not in SIGNAL_MODES:
        raise ValueError(f'Unknown indicator signal mode {mode!r}. Available: {", ".join(SIGNAL_MODES)}.')
    return m


def supports_threshold(ind_id: str) -> bool:
    return INDICATORS[ind_id].get('scale') == 'oscillator'


def sweep_param(ind_id: str) -> Optional[str]:
    spec = INDICATORS[ind_id]
    if not spec['params']:
        return None
    return spec.get('periodParam') or spec['params'][0]['id']


def resolve_params(ind_id: str, given: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """Defaults + the caller's values (int params rounded). A generic 'period' sets the primary
    period parameter of indicators whose period has another name (macd fast, stochastic %K)."""
    spec = INDICATORS[ind_id]
    out = {p['id']: p['default'] for p in spec['params']}
    g = dict(given or {})
    alias = {'k_period': 'kPeriod', 'd_period': 'dPeriod', 'fast_period': 'fastPeriod', 'slow_period': 'slowPeriod',
             'signal_period': 'signalPeriod', 'fast': 'fastPeriod', 'slow': 'slowPeriod', 'signal': 'signalPeriod',
             'std_dev': 'stdDev', 'alpha': 'stdDev', 'atr_period': 'atrPeriod', 'lookback': 'period',
             'ema_period': 'period', 'emaPeriod': 'period'}
    for k, v in list(g.items()):
        if k in alias and alias[k] not in g:
            g[alias[k]] = v
    if 'period' in g and spec.get('periodParam') and spec['periodParam'] not in g:
        g[spec['periodParam']] = g['period']
    for p in spec['params']:
        if p['id'] in g and g[p['id']] is not None:
            try:
                v = float(g[p['id']])
            except (TypeError, ValueError):
                raise ValueError(f'{spec["name"]} parameter {p["id"]} must be a number, not {g[p["id"]]!r}')
            out[p['id']] = int(round(v)) if p['type'] == 'int' else v
            if p['type'] == 'int' and out[p['id']] < 1:
                raise ValueError(f'{spec["name"]} parameter {p["id"]} must be at least 1')
    return out


def compute(ind_id: str, ta: TA, params: Optional[Dict[str, Any]] = None):
    """(values, params used): values is an array or {output: array}."""
    pid = resolve_id(ind_id)
    p = resolve_params(pid, params)
    return INDICATORS[pid]['compute'](ta, p), p


def primary_line(ind_id: str, values) -> np.ndarray:
    if isinstance(values, dict):
        return values[INDICATORS[ind_id].get('primary') or next(iter(values))]
    return values


def catalog() -> Dict[str, List[Dict[str, Any]]]:
    """get_indicators payload: {category: [{id, name, description, params, outputs, signalModes,
    thresholdSignals, defaultLevels, sweepable, sweepParam}]}."""
    out: Dict[str, List[Dict[str, Any]]] = {}
    for iid, s in INDICATORS.items():
        thr = supports_threshold(iid)
        modes = ['crossover_signals', 'breakout_signals', 'mean_reversion_signals']
        if thr:
            modes += ['threshold_signals', 'signal_filter']
        out.setdefault(s['category'], []).append({
            'id': iid, 'name': s['name'], 'description': s['description'],
            'params': [dict(p) for p in s['params']], 'outputs': list(s.get('outputs') or ['value']),
            'signalModes': modes, 'thresholdSignals': thr,
            'defaultLevels': list(s['levels']) if s.get('levels') else None,
            'filterable': iid in FILTER_INDICATORS,
            'sweepable': sweep_param(iid) is not None, 'sweepParam': sweep_param(iid),
        })
    return out


# ════════════════════════════════════════════════════════════════════════════
# Signal helpers (bar t close decisions; warm-up NaN never triggers)
# ════════════════════════════════════════════════════════════════════════════

def _shift(a: np.ndarray, fill) -> np.ndarray:
    out = np.empty_like(a)
    out[:1] = fill
    out[1:] = a[:-1]
    return out


def _line(b, n: int) -> np.ndarray:
    return np.full(n, float(b)) if np.ndim(b) == 0 else _a(b)


def cross_above(a, b) -> np.ndarray:
    """a > b on t and a <= b on t-1, both bars valid (base.signals.crossover, warm-up gated)."""
    from base.signals import crossover
    x = _a(a)
    return crossover(x, _line(b, len(x)))


def cross_below(a, b) -> np.ndarray:
    from base.signals import crossunder
    x = _a(a)
    return crossunder(x, _line(b, len(x)))


def edge(state) -> np.ndarray:
    """True on the first bar of each run of `state` (state turns on)."""
    s = np.asarray(state, dtype=bool)
    return s & ~_shift(s, False)


def gt(a, b) -> np.ndarray:
    with np.errstate(invalid='ignore'):
        return np.asarray(_a(a) > (b if np.isscalar(b) else _a(b)), dtype=bool)


def lt(a, b) -> np.ndarray:
    with np.errstate(invalid='ignore'):
        return np.asarray(_a(a) < (b if np.isscalar(b) else _a(b)), dtype=bool)


def prev(a) -> np.ndarray:
    return _shift(_a(a), np.nan)
