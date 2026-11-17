"""
Strategy library of the Backtesting.py engine: the 27 catalogue strategies (signal builders and
their specs). Nothing here imports the backtesting library (get_strategies stays cheap); the
backtesting.py Strategy class that executes the signals is btp_executor.SignalExecutor.

HOW A CATALOGUE STRATEGY RUNS
  Each strategy is a vectorised, causal signal builder(ta, params) -> signals over one symbol's
  bars (the value at bar t uses bars <= t only; NaN warm-up compares False, crossovers need both
  lines valid on the previous bar too). btp_executor.SignalExecutor - a module-level
  backtesting.py Strategy (importable / picklable, so optimisation can use worker processes) -
  reads the signal of the decision bar in Strategy.next() and trades through the library's own
  API (buy / sell / position.close / Trade.sl): the library's event loop, broker, order matching,
  whole-share sizing, contingent stop-loss / take-profit orders and equity accounting run every
  strategy. This is the library's documented SignalStrategy pattern (backtesting.lib):
  indicators are precomputed in init(), decisions happen bar by bar in next().

DECISIONS (on the close of decision bar d; the fill follows the run's fill timing)
  long  + exit   -> close the long (and open a short when shorting is allowed and no entry)
  long  + flat   -> close
  short + entry  -> cover (and buy when there is no exit signal)
  short + flat   -> cover
  flat  + entry (no exit) -> buy;  flat + exit (no entry) -> short (only when allowed)
  'entry' / 'exit' are level or event signals as each rule below says; 'flat' closes any position.

RULES (defaults in the catalogue; RSI here is the SIMPLE-AVERAGE (Cutler) RSI - mean gain / mean
loss over the period - not Wilder's smoothed RSI the vectorbt engine uses)
  buy_and_hold       buy on the first bar, never sell.
  sma_crossover      fast SMA crosses above the slow SMA (enter) / below (exit).
  ema_crossover      same with EMAs (seeded with the SMA of the first n bars).
  macd               MACD line crosses its signal line (above = enter, below = exit).
  macd_zero_cross    MACD LINE crosses the zero line (above = enter, below = exit). The MACD line is
                     fast EMA - slow EMA, so these are the events of an EMA crossover with the same
                     two periods (stated in the catalogue; the defaults differ: 12/26 vs 10/20).
  adx_trend          ADX > threshold: long while +DI > -DI, exit while -DI > +DI; ADX <= threshold:
                     flat.
  keltner_breakout   close above the upper Keltner band (EMA +/- multiplier x ATR) = enter, close
                     below the lower band = exit.
  triple_ma          SMA fast > medium > slow = enter; fast < medium < slow = exit.
  ichimoku           Tenkan-sen crosses above Kijun-sen (enter) / below (exit).
  psar               close above the Parabolic SAR = enter, below = exit.
  trend_momentum     close > SMA and RSI > threshold = enter; close < SMA and RSI < threshold = exit.
  mean_reversion     z-score < -Z = enter, > +Z = exit, |z| < 0.5 = flat.
  bollinger_bands    close below the lower band = enter, above the upper band = exit.
  rsi                RSI < oversold = enter, RSI > overbought = exit (level rule: in the market from
                     the first oversold close until the first overbought close).
  stochastic         %K crosses above %D below oversold = enter; %K crosses below %D above
                     overbought = exit.
  williams_r         %R < oversold = enter, %R > overbought = exit.
  momentum           rate of change over the lookback > threshold % = enter, < -threshold % = exit.
  dual_momentum      short AND long rate of change > threshold % = enter; short ROC < -threshold %
                     = exit.
  cci                CCI < lower = enter, CCI > upper = exit (two independent levels).
  breakout           Donchian: close above the previous N-bar high = enter, below the previous
                     N-bar low = exit; optional protective stop atrStop x ATR below the entry.
  volatility_breakout  ATR > K x its `lookback`-bar average (volatility expansion): an up bar
                     (close > open) = enter, a down bar = exit.
  rsi_macd           MACD histogram turns positive while RSI was below oversold within `lookback`
                     bars = enter; RSI > overbought or the histogram turns negative = exit.
  macd_adx           MACD crosses above its signal with ADX > threshold = enter; MACD crosses below
                     its signal = exit.
  bollinger_rsi      close < lower band and RSI < oversold = enter; close > upper band and RSI >
                     overbought = exit.
  obv_trend          on-balance volume above its N-bar SMA = enter, below = exit.
  mfi                money flow index < oversold = enter, > overbought = exit.
  atr_trailing_stop  close above its SMA = enter (exit below it); a trailing stop multiplier x ATR
                     below the close is raised at every close (backtesting.py contingent SL order).
"""

from __future__ import annotations

import math
from typing import Any, Callable, Dict, List, Optional

import numpy as np

from base import signals as S

Signals = Dict[str, Any]


# ════════════════════════════════════════════════════════════════════════════
# Parameters
# ════════════════════════════════════════════════════════════════════════════

def _num(params, key, default, *alts):
    for k in (key,) + alts:
        if k in params and params[k] is not None and params[k] != '':
            try:
                v = float(params[k])
            except (TypeError, ValueError):
                raise ValueError(f'parameter {k} must be a number, not {params[k]!r}')
            if not math.isfinite(v):
                raise ValueError(f'parameter {k} must be a finite number')
            return v
    return float(default)


def _i(params, key, default, *alts) -> int:
    v = int(round(_num(params, key, default, *alts)))
    return v


def _pos(name, v):
    if v < 1:
        raise ValueError(f'{name} must be at least 1 (got {v})')
    return v


def _f(params, key, default, *alts) -> float:
    return _num(params, key, default, *alts)


# ════════════════════════════════════════════════════════════════════════════
# Indicators (numpy, causal, NaN during warm-up; memoised per symbol by TA)
# ════════════════════════════════════════════════════════════════════════════

def _gt(a, b):
    with np.errstate(invalid='ignore'):
        return np.asarray(np.greater(a, b), dtype=bool)


def _lt(a, b):
    with np.errstate(invalid='ignore'):
        return np.asarray(np.less(a, b), dtype=bool)


def _valid(*arrs):
    ok = np.ones(len(arrs[0]), dtype=bool)
    for a in arrs:
        ok &= np.isfinite(a)
    return ok


def ema_valid(v: np.ndarray, n: int) -> np.ndarray:
    """EMA (alpha 2/(n+1)) seeded with the SMA of the first n VALID values; NaN before. Works on
    series with a NaN warm-up (e.g. the MACD line)."""
    v = np.asarray(v, dtype=float)
    out = np.full(len(v), np.nan)
    ok = np.flatnonzero(np.isfinite(v))
    if not len(ok) or n < 1:
        return out
    k = int(ok[0])
    tail = v[k:]
    if len(tail) < n or not np.isfinite(tail[:n]).all():
        return out
    if np.isfinite(tail).all():
        out[k:] = S.ema(tail, n)
        return out
    import pandas as pd                       # gaps later in the series: carry the EMA over them
    seed = float(np.mean(tail[:n]))
    s = pd.Series(np.concatenate([[seed], tail[n:]]))
    out[k + n - 1:] = s.ewm(span=n, adjust=False, ignore_na=True).mean().to_numpy()
    return out


def rolling_std(v: np.ndarray, n: int) -> np.ndarray:
    import pandas as pd
    return pd.Series(v).rolling(n).std(ddof=1).to_numpy()


def rolling_max(v: np.ndarray, n: int) -> np.ndarray:
    import pandas as pd
    return pd.Series(v).rolling(n).max().to_numpy()


def rolling_min(v: np.ndarray, n: int) -> np.ndarray:
    import pandas as pd
    return pd.Series(v).rolling(n).min().to_numpy()


def shift1(v: np.ndarray) -> np.ndarray:
    out = np.full(len(v), np.nan)
    out[1:] = v[:-1]
    return out


class TA:
    """Memoised indicators of one symbol's OHLCV arrays."""

    def __init__(self, o, h, l, c, v):
        self.c = np.asarray(c, dtype=float)
        self.o = np.asarray(o if o is not None else c, dtype=float)
        self.h = np.asarray(h if h is not None else c, dtype=float)
        self.l = np.asarray(l if l is not None else c, dtype=float)
        self.v = np.asarray(v if v is not None else np.zeros(len(self.c)), dtype=float)
        self.n = len(self.c)
        self._m: Dict[tuple, Any] = {}

    @classmethod
    def from_frame(cls, f):
        g = (lambda k: f[k].to_numpy(dtype=float) if k in f else None)
        return cls(g('Open'), g('High'), g('Low'), f['Close'].to_numpy(dtype=float), g('Volume'))

    def memo(self, key, fn):
        r = self._m.get(key)
        if r is None:
            if len(self._m) > 2048:
                self._m.clear()
            r = self._m[key] = fn()
        return r

    def sma(self, n):
        return self.memo(('sma', n), lambda: S.sma(self.c, n))

    def ema(self, n):
        return self.memo(('ema', n), lambda: ema_valid(self.c, n))

    def rsi(self, n):
        return self.memo(('rsi', n), lambda: S.rsi_simple(self.c, n))

    def macd(self, f, s, g):
        def calc():
            line = self.ema(f) - self.ema(s)
            sig = ema_valid(line, g)
            return {'macd': line, 'signal': sig, 'histogram': line - sig}
        return self.memo(('macd', f, s, g), calc)

    def bbands(self, n, k):
        def calc():
            m = self.sma(n)
            sd = self.memo(('std', n), lambda: rolling_std(self.c, n))
            return {'upper': m + k * sd, 'lower': m - k * sd, 'middle': m}
        return self.memo(('bb', n, k), calc)

    def zscore(self, n):
        def calc():
            m = self.sma(n)
            sd = self.memo(('std', n), lambda: rolling_std(self.c, n))
            with np.errstate(divide='ignore', invalid='ignore'):
                return np.where(sd > 0, (self.c - m) / sd, np.nan)
        return self.memo(('z', n), calc)

    def tr(self):
        def calc():
            pc = shift1(self.c)
            with np.errstate(invalid='ignore'):
                t = np.fmax(self.h - self.l, np.fmax(np.abs(self.h - pc), np.abs(self.l - pc)))
            return t
        return self.memo(('tr',), calc)

    def atr(self, n):
        return self.memo(('atr', n), lambda: S.sma(np.where(np.isfinite(self.tr()), self.tr(), 0.0), n))

    def dmi(self, n):
        """ADX / +DI / -DI with exponential smoothing (span n) - this engine's v1 definition."""
        def calc():
            import pandas as pd
            h, l = pd.Series(self.h), pd.Series(self.l)
            up, dn = h - h.shift(1), l.shift(1) - l
            plus_dm = up.where(up > dn, 0.0).clip(lower=0.0)
            minus_dm = dn.where(dn > up, 0.0).clip(lower=0.0)
            atr_v = pd.Series(self.tr()).ewm(span=n, adjust=False).mean()
            atr_safe = atr_v.replace(0, np.nan)
            pdi = 100.0 * plus_dm.ewm(span=n, adjust=False).mean() / atr_safe
            mdi = 100.0 * minus_dm.ewm(span=n, adjust=False).mean() / atr_safe
            dx = (pdi - mdi).abs() / (pdi + mdi).replace(0, np.nan) * 100.0
            adx = dx.ewm(span=n, adjust=False).mean()
            warm = np.arange(self.n) < 2 * n - 1        # smoothing needs ~2n bars before ADX means anything
            a = adx.to_numpy()
            a[warm] = np.nan
            return {'adx': a, 'plus_di': pdi.to_numpy(), 'minus_di': mdi.to_numpy()}
        return self.memo(('dmi', n), calc)

    def stoch(self, k, d):
        def calc():
            lo, hi = rolling_min(self.l, k), rolling_max(self.h, k)
            rng = hi - lo
            with np.errstate(divide='ignore', invalid='ignore'):
                kk = np.where(rng > 0, (self.c - lo) / rng * 100.0, np.nan)
            return {'k': kk, 'd': S.sma(kk, d)}
        return self.memo(('stoch', k, d), calc)

    def williams_r(self, n):
        def calc():
            lo, hi = rolling_min(self.l, n), rolling_max(self.h, n)
            rng = hi - lo
            with np.errstate(divide='ignore', invalid='ignore'):
                return np.where(rng > 0, -100.0 * (hi - self.c) / rng, np.nan)
        return self.memo(('wr', n), calc)

    def cci(self, n):
        def calc():
            tp = (self.h + self.l + self.c) / 3.0
            out = np.full(self.n, np.nan)
            if self.n < n:
                return out
            w = np.lib.stride_tricks.sliding_window_view(tp, n)
            m = w.mean(axis=1)
            mad = np.abs(w - m[:, None]).mean(axis=1)
            with np.errstate(divide='ignore', invalid='ignore'):
                out[n - 1:] = np.where(mad > 0, (tp[n - 1:] - m) / (0.015 * mad), np.nan)
            return out
        return self.memo(('cci', n), calc)

    def obv(self):
        def calc():
            d = np.sign(np.diff(self.c, prepend=self.c[0]))
            d[0] = 0.0
            return np.cumsum(np.nan_to_num(self.v) * np.nan_to_num(d))
        return self.memo(('obv',), calc)

    def mfi(self, n):
        def calc():
            tp = (self.h + self.l + self.c) / 3.0
            rmf = tp * np.nan_to_num(self.v)
            ptp = shift1(tp)
            pos = S.sma(np.where(_gt(tp, ptp), rmf, 0.0), n) * n
            neg = S.sma(np.where(_lt(tp, ptp), rmf, 0.0), n) * n
            with np.errstate(divide='ignore', invalid='ignore'):
                return np.where(neg > 0, 100.0 - 100.0 / (1.0 + pos / neg), np.where(pos > 0, 100.0, np.nan))
        return self.memo(('mfi', n), calc)

    def midpoint(self, n):
        return self.memo(('mid', n), lambda: (rolling_max(self.h, n) + rolling_min(self.l, n)) / 2.0)

    def roc(self, n):
        def calc():
            out = np.full(self.n, np.nan)
            if self.n > n:
                with np.errstate(divide='ignore', invalid='ignore'):
                    out[n:] = self.c[n:] / self.c[:-n] - 1.0
            return out
        return self.memo(('roc', n), calc)

    def psar(self, step, mx):
        def calc():
            h, l = self.h, self.l
            n = self.n
            out = np.full(n, np.nan)
            if n < 2:
                return out
            af, bull = step, True
            hp, lp = h[0], l[0]
            out[0] = l[0]
            for i in range(1, n):
                prev = out[i - 1]
                if bull:
                    sar = prev + af * (hp - prev)
                    sar = min(sar, l[i - 1], l[i - 2] if i >= 2 else l[i - 1])
                    if l[i] < sar:
                        bull, sar, lp, af = False, hp, l[i], step
                    elif h[i] > hp:
                        hp, af = h[i], min(af + step, mx)
                else:
                    sar = prev + af * (lp - prev)
                    sar = max(sar, h[i - 1], h[i - 2] if i >= 2 else h[i - 1])
                    if h[i] > sar:
                        bull, sar, hp, af = True, lp, h[i], step
                    elif l[i] < lp:
                        lp, af = l[i], min(af + step, mx)
                out[i] = sar
            out[0] = np.nan
            return out
        return self.memo(('psar', step, mx), calc)

    def realised_vol(self, n, ppy):
        """Annualised std of close-to-close returns over the n returns ending at each bar."""
        def calc():
            import pandas as pd
            r = pd.Series(self.c).pct_change()
            return (r.rolling(n).std(ddof=1) * math.sqrt(ppy)).to_numpy()
        return self.memo(('vol', n, ppy), calc)


# ════════════════════════════════════════════════════════════════════════════
# Signal builders: (ta, params) -> {'e', 'x', 'f'?, 'sl'?, 'tr'?}
# ════════════════════════════════════════════════════════════════════════════

def _sig(e, x, f=None, sl=None, tr=None) -> Signals:
    return {'e': np.asarray(e, dtype=bool), 'x': np.asarray(x, dtype=bool),
            'f': None if f is None else np.asarray(f, dtype=bool), 'sl': sl, 'tr': tr}


def _buy_and_hold(ta: TA, p) -> Signals:
    e = np.zeros(ta.n, dtype=bool)
    if ta.n:
        e[0] = True
    return _sig(e, np.zeros(ta.n, dtype=bool))


def _sma_crossover(ta, p):
    a, b = ta.sma(_pos('fastPeriod', _i(p, 'fastPeriod', 10))), ta.sma(_pos('slowPeriod', _i(p, 'slowPeriod', 20)))
    return _sig(S.crossover(a, b), S.crossunder(a, b))


def _ema_crossover(ta, p):
    a, b = ta.ema(_pos('fastPeriod', _i(p, 'fastPeriod', 10))), ta.ema(_pos('slowPeriod', _i(p, 'slowPeriod', 20)))
    return _sig(S.crossover(a, b), S.crossunder(a, b))


def _macd_parts(ta, p):
    return ta.macd(_pos('fastPeriod', _i(p, 'fastPeriod', 12)), _pos('slowPeriod', _i(p, 'slowPeriod', 26)),
                   _pos('signalPeriod', _i(p, 'signalPeriod', 9)))


def _macd(ta, p):
    m = _macd_parts(ta, p)
    return _sig(S.crossover(m['macd'], m['signal']), S.crossunder(m['macd'], m['signal']))


def _macd_zero_cross(ta, p):
    line = ta.ema(_pos('fastPeriod', _i(p, 'fastPeriod', 12))) - ta.ema(_pos('slowPeriod', _i(p, 'slowPeriod', 26)))
    zero = np.zeros(ta.n)
    return _sig(S.crossover(line, zero), S.crossunder(line, zero))


def _adx_trend(ta, p):
    d = ta.dmi(_pos('period', _i(p, 'period', 14, 'adxPeriod')))
    th = _f(p, 'threshold', 25, 'adxThreshold')
    a, pdi, mdi = d['adx'], d['plus_di'], d['minus_di']
    strong = _gt(a, th)
    return _sig(strong & _gt(pdi, mdi), strong & _gt(mdi, pdi), np.isfinite(a) & ~strong)


def _keltner_breakout(ta, p):
    n = _pos('period', _i(p, 'period', 20, 'emaPeriod'))
    an = _pos('atrPeriod', _i(p, 'atrPeriod', 10))
    k = _f(p, 'multiplier', 2.0, 'atrMultiplier')
    mid, a = ta.ema(n), ta.atr(an)
    return _sig(_gt(ta.c, mid + k * a), _lt(ta.c, mid - k * a))


def _triple_ma(ta, p):
    f = ta.sma(_pos('fastPeriod', _i(p, 'fastPeriod', 5)))
    m = ta.sma(_pos('mediumPeriod', _i(p, 'mediumPeriod', 20)))
    s = ta.sma(_pos('slowPeriod', _i(p, 'slowPeriod', 50)))
    return _sig(_gt(f, m) & _gt(m, s), _lt(f, m) & _lt(m, s))


def _ichimoku(ta, p):
    t = ta.midpoint(_pos('tenkanPeriod', _i(p, 'tenkanPeriod', 9)))
    k = ta.midpoint(_pos('kijunPeriod', _i(p, 'kijunPeriod', 26)))
    return _sig(S.crossover(t, k), S.crossunder(t, k))


def _psar(ta, p):
    step, mx = _f(p, 'afStep', 0.02), _f(p, 'afMax', 0.2)
    if not (0 < step <= mx):
        raise ValueError('psar needs 0 < afStep <= afMax')
    sar = ta.psar(step, mx)
    return _sig(_gt(ta.c, sar), _lt(ta.c, sar))


def _trend_momentum(ta, p):
    ma = ta.sma(_pos('maPeriod', _i(p, 'maPeriod', 50)))
    r = ta.rsi(_pos('rsiPeriod', _i(p, 'rsiPeriod', 14, 'momentumPeriod')))
    th = _f(p, 'rsiThreshold', 50)
    return _sig(_gt(ta.c, ma) & _gt(r, th), _lt(ta.c, ma) & _lt(r, th))


def _mean_reversion(ta, p):
    z = ta.zscore(_pos('period', _i(p, 'period', 20, 'window')))
    th = _f(p, 'zThreshold', 2.0, 'threshold')
    with np.errstate(invalid='ignore'):
        flat = np.isfinite(z) & (np.abs(z) < 0.5)
    return _sig(_lt(z, -th), _gt(z, th), flat)


def _bollinger_bands(ta, p):
    bb = ta.bbands(_pos('period', _i(p, 'period', 20)), _f(p, 'stdDev', 2.0))
    return _sig(_lt(ta.c, bb['lower']), _gt(ta.c, bb['upper']))


def _rsi(ta, p):
    r = ta.rsi(_pos('period', _i(p, 'period', 14)))
    return _sig(_lt(r, _f(p, 'oversold', 30)), _gt(r, _f(p, 'overbought', 70)))


def _stochastic(ta, p):
    st = ta.stoch(_pos('kPeriod', _i(p, 'kPeriod', 14)), _pos('dPeriod', _i(p, 'dPeriod', 3)))
    k, d = st['k'], st['d']
    return _sig(S.crossover(k, d) & _lt(k, _f(p, 'oversold', 20)), S.crossunder(k, d) & _gt(k, _f(p, 'overbought', 80)))


def _williams_r(ta, p):
    w = ta.williams_r(_pos('period', _i(p, 'period', 14)))
    return _sig(_lt(w, _f(p, 'oversold', -80)), _gt(w, _f(p, 'overbought', -20)))


def _momentum(ta, p):
    roc = ta.roc(_pos('lookback', _i(p, 'lookback', 20, 'period')))
    th = _f(p, 'threshold', 0.0) / 100.0
    return _sig(_gt(roc, th), _lt(roc, -th))


def _dual_momentum(ta, p):
    s = ta.roc(_pos('shortLookback', _i(p, 'shortLookback', 20)))
    lg = ta.roc(_pos('longLookback', _i(p, 'longLookback', 60)))
    th = _f(p, 'threshold', 0.0) / 100.0
    return _sig(_gt(s, th) & _gt(lg, th), _lt(s, -th))


def _cci(ta, p):
    c = ta.cci(_pos('period', _i(p, 'period', 20)))
    return _sig(_lt(c, _f(p, 'lower', -100)), _gt(c, _f(p, 'upper', 100)))


def _breakout(ta, p):
    n = _pos('period', _i(p, 'period', 20))
    up, lo = shift1(rolling_max(ta.h, n)), shift1(rolling_min(ta.l, n))
    k = _f(p, 'atrStop', 1.5, 'atrMult')
    sl = None
    if k > 0:
        a = ta.atr(n)
        sl = np.where(np.isfinite(a) & (a > 0), k * a, np.nan)
    return _sig(_gt(ta.c, up), _lt(ta.c, lo), sl=sl)


def _volatility_breakout(ta, p):
    a = ta.atr(_pos('period', _i(p, 'period', 20, 'atrPeriod')))
    k = _f(p, 'kMultiplier', 1.5, 'atrMultiplier', 'atrMult')
    look = _pos('lookback', _i(p, 'lookback', 20))
    expand = _gt(a, k * S.sma(a, look))
    return _sig(expand & _gt(ta.c, ta.o), expand & _lt(ta.c, ta.o))


def _rsi_macd(ta, p):
    r = ta.rsi(_pos('rsiPeriod', _i(p, 'rsiPeriod', 14)))
    h = _macd_parts(ta, p)['histogram']
    look = _pos('lookback', _i(p, 'lookback', 5))
    os_, ob = _f(p, 'oversold', 30), _f(p, 'overbought', 70)
    was_oversold = _lt(rolling_min(np.where(np.isfinite(r), r, np.inf), look), os_)
    zero = np.zeros(ta.n)
    return _sig(S.crossover(h, zero) & was_oversold, _gt(r, ob) | S.crossunder(h, zero))


def _macd_adx(ta, p):
    m = _macd_parts(ta, p)
    a = ta.dmi(_pos('adxPeriod', _i(p, 'adxPeriod', 14)))['adx']
    th = _f(p, 'adxThreshold', 25)
    return _sig(S.crossover(m['macd'], m['signal']) & _gt(a, th), S.crossunder(m['macd'], m['signal']))


def _bollinger_rsi(ta, p):
    bb = ta.bbands(_pos('bbPeriod', _i(p, 'bbPeriod', 20, 'period')), _f(p, 'stdDev', 2.0))
    r = ta.rsi(_pos('rsiPeriod', _i(p, 'rsiPeriod', 14)))
    return _sig(_lt(ta.c, bb['lower']) & _lt(r, _f(p, 'oversold', 30)),
                _gt(ta.c, bb['upper']) & _gt(r, _f(p, 'overbought', 70)))


def _obv_trend(ta, p):
    n = _pos('period', _i(p, 'period', 20, 'maPeriod'))
    o = ta.obv()
    m = ta.memo(('obv_sma', n), lambda: S.sma(o, n))
    return _sig(_gt(o, m), _lt(o, m))


def _mfi(ta, p):
    m = ta.mfi(_pos('period', _i(p, 'period', 14)))
    return _sig(_lt(m, _f(p, 'oversold', 20)), _gt(m, _f(p, 'overbought', 80)))


def _atr_trailing_stop(ta, p):
    ma = ta.sma(_pos('maPeriod', _i(p, 'maPeriod', 20)))
    a = ta.atr(_pos('atrPeriod', _i(p, 'atrPeriod', 14)))
    k = _f(p, 'multiplier', 3.0, 'atrMultiplier')
    if k <= 0:
        raise ValueError('atr_trailing_stop needs a positive multiplier')
    dist = np.where(np.isfinite(a) & (a > 0), k * a, np.nan)
    return _sig(_gt(ta.c, ma), _lt(ta.c, ma), tr=dist)


BUILDERS: Dict[str, Callable[[TA, Dict[str, Any]], Signals]] = {
    'buy_and_hold': _buy_and_hold, 'sma_crossover': _sma_crossover, 'ema_crossover': _ema_crossover,
    'macd': _macd, 'macd_zero_cross': _macd_zero_cross, 'adx_trend': _adx_trend,
    'keltner_breakout': _keltner_breakout, 'triple_ma': _triple_ma, 'ichimoku': _ichimoku, 'psar': _psar,
    'trend_momentum': _trend_momentum, 'mean_reversion': _mean_reversion, 'bollinger_bands': _bollinger_bands,
    'rsi': _rsi, 'stochastic': _stochastic, 'williams_r': _williams_r, 'momentum': _momentum,
    'dual_momentum': _dual_momentum, 'cci': _cci, 'breakout': _breakout, 'volatility_breakout': _volatility_breakout,
    'rsi_macd': _rsi_macd, 'macd_adx': _macd_adx, 'bollinger_rsi': _bollinger_rsi, 'obv_trend': _obv_trend,
    'mfi': _mfi, 'atr_trailing_stop': _atr_trailing_stop,
}
ALIASES = {'buy_hold': 'buy_and_hold', 'buyandhold': 'buy_and_hold', 'sma': 'sma_crossover',
           'ema': 'ema_crossover', 'macd_crossover': 'macd', 'donchian_breakout': 'breakout',
           'parabolic_sar': 'psar', 'money_flow_index': 'mfi'}
USES_RSI = ('rsi', 'trend_momentum', 'rsi_macd', 'bollinger_rsi')


def resolve(strategy_type: Any) -> str:
    s = str(strategy_type or 'sma_crossover').strip().lower()
    s = ALIASES.get(s, s)
    if s not in BUILDERS:
        raise ValueError(f'Unknown strategy {strategy_type!r} for Backtesting.py. Available: '
                         f'{", ".join(sorted(BUILDERS))}.')
    return s


def build_signals(strategy_id: str, ta: TA, params: Dict[str, Any]) -> Signals:
    sig = BUILDERS[strategy_id](ta, params or {})
    act = sig['e'] | sig['x']
    if sig['f'] is not None:
        act = act | sig['f']
    sig['act'] = act
    return sig


def mask_before(sig: Signals, start: int) -> Signals:
    """A copy whose decisions before bar `start` are dropped (walk-forward warm-up bars)."""
    if start <= 0:
        return sig
    out = dict(sig)
    for k in ('e', 'x', 'f', 'act'):
        if out.get(k) is not None:
            a = out[k].copy()
            a[:start] = False
            out[k] = a
    return out


def slice_signals(sig: Signals, a: int, e: int) -> Signals:
    return {k: (v[a:e] if isinstance(v, np.ndarray) else v) for k, v in sig.items()}


def default_warmup(strategy_id: str, params: Dict[str, Any]) -> int:
    """Bars the strategy's indicators need before its first possible signal."""
    p = params or {}
    look = [int(round(float(v))) for k, v in p.items() if isinstance(v, (int, float)) and not isinstance(v, bool)
            and any(w in k.lower() for w in ('period', 'lookback', 'window')) and float(v) >= 1]
    base = max(look) if look else 0
    if strategy_id in ('macd', 'rsi_macd', 'macd_adx'):
        base = max(base, _i(p, 'slowPeriod', 26) + _i(p, 'signalPeriod', 9))
    if strategy_id in ('macd_zero_cross',):
        base = max(base, _i(p, 'slowPeriod', 26))
    if strategy_id in ('adx_trend', 'macd_adx'):
        base = max(base, 2 * _i(p, 'adxPeriod', 14, 'period'))
    if strategy_id == 'volatility_breakout':
        base = _i(p, 'period', 20) + _i(p, 'lookback', 20)
    if strategy_id == 'atr_trailing_stop':
        base = max(_i(p, 'maPeriod', 20), _i(p, 'atrPeriod', 14))
    return int(base)


# ════════════════════════════════════════════════════════════════════════════
# Catalogue (get_strategies) - same spec shape as the vectorbt engine
# ════════════════════════════════════════════════════════════════════════════

def _p(pid, name, default, lo, hi, step=1, typ='int', desc=None):
    d = {'id': pid, 'name': name, 'type': typ, 'default': default, 'min': lo, 'max': hi, 'step': step}
    if desc:
        d['description'] = desc
    return d


_FAST_SLOW = [{'lt': ['fastPeriod', 'slowPeriod'], 'message': 'The fast period must be shorter than the slow period.'}]
_OS_OB = [{'lt': ['oversold', 'overbought'], 'message': 'Oversold must be below overbought.'}]
_RSI_NOTE = ' RSI = simple-average (Cutler) RSI: mean gain / mean loss over the period (not Wilder smoothing).'

CATALOG: List[Dict[str, Any]] = [
    {'type': 'buy_and_hold', 'name': 'Buy & Hold', 'category': 'Benchmark',
     'description': 'Buy on the first bar with the sizing setting and hold to the end (the canonical baseline)',
     'parameters': []},
    {'type': 'sma_crossover', 'name': 'SMA Crossover', 'category': 'Trend',
     'description': 'Fast SMA crosses above the slow SMA (enter) / below it (exit); a real cross is needed',
     'parameters': [_p('fastPeriod', 'Fast Period', 10, 2, 100), _p('slowPeriod', 'Slow Period', 20, 5, 200)],
     'constraints': _FAST_SLOW},
    {'type': 'ema_crossover', 'name': 'EMA Crossover', 'category': 'Trend',
     'description': 'Fast EMA crosses above / below the slow EMA (EMAs seeded with the SMA of their first N bars)',
     'parameters': [_p('fastPeriod', 'Fast Period', 10, 2, 100), _p('slowPeriod', 'Slow Period', 20, 5, 200)],
     'constraints': _FAST_SLOW},
    {'type': 'macd', 'name': 'MACD Crossover', 'category': 'Trend',
     'description': 'MACD line crosses above its signal line (enter) / below it (exit)',
     'parameters': [_p('fastPeriod', 'Fast Period', 12, 2, 50), _p('slowPeriod', 'Slow Period', 26, 10, 100),
                    _p('signalPeriod', 'Signal Period', 9, 2, 30)], 'constraints': _FAST_SLOW},
    {'type': 'macd_zero_cross', 'name': 'MACD Zero Cross', 'category': 'Trend',
     'description': 'MACD line (fast EMA - slow EMA) crosses above zero (enter) / below zero (exit) - the trend '
                    'change of the MACD centre line (the same events as an EMA crossover with these two periods); '
                    '"MACD Crossover" trades the MACD line against its signal line instead',
     'parameters': [_p('fastPeriod', 'Fast', 12, 2, 50), _p('slowPeriod', 'Slow', 26, 10, 100)],
     'constraints': _FAST_SLOW},
    {'type': 'adx_trend', 'name': 'ADX Trend', 'category': 'Trend',
     'description': 'While ADX > threshold: long when +DI > -DI, out when -DI > +DI; flat while ADX <= threshold',
     'parameters': [_p('period', 'ADX Period', 14, 5, 50), _p('threshold', 'ADX Threshold', 25, 10, 50)]},
    {'type': 'keltner_breakout', 'name': 'Keltner Breakout', 'category': 'Trend',
     'description': 'Close above the upper Keltner band (EMA + multiplier x ATR) enters, below the lower band exits',
     'parameters': [_p('period', 'EMA Period', 20, 5, 100), _p('atrPeriod', 'ATR Period', 10, 5, 50),
                    _p('multiplier', 'Multiplier', 2.0, 0.5, 5.0, 0.1, 'float')]},
    {'type': 'triple_ma', 'name': 'Triple MA', 'category': 'Trend',
     'description': 'Long when SMA fast > medium > slow, out when fast < medium < slow',
     'parameters': [_p('fastPeriod', 'Fast', 5, 2, 20), _p('mediumPeriod', 'Medium', 20, 10, 50),
                    _p('slowPeriod', 'Slow', 50, 30, 200)],
     'constraints': [{'lt': ['fastPeriod', 'mediumPeriod'], 'message': 'Fast must be shorter than medium.'},
                     {'lt': ['mediumPeriod', 'slowPeriod'], 'message': 'Medium must be shorter than slow.'}]},
    {'type': 'ichimoku', 'name': 'Ichimoku Cloud', 'category': 'Trend',
     'description': 'Tenkan-sen crosses above Kijun-sen (enter) / below it (exit)',
     'parameters': [_p('tenkanPeriod', 'Tenkan', 9, 2, 30), _p('kijunPeriod', 'Kijun', 26, 5, 100)],
     'constraints': [{'lt': ['tenkanPeriod', 'kijunPeriod'], 'message': 'Tenkan must be shorter than Kijun.'}]},
    {'type': 'psar', 'name': 'Parabolic SAR', 'category': 'Trend',
     'description': 'Close above the Parabolic SAR enters, below it exits',
     'parameters': [_p('afStep', 'AF Step', 0.02, 0.005, 0.1, 0.005, 'float'),
                    _p('afMax', 'AF Max', 0.2, 0.05, 0.5, 0.01, 'float')],
     'constraints': [{'lt': ['afStep', 'afMax'], 'message': 'The AF step must be below the AF maximum.'}]},
    {'type': 'trend_momentum', 'name': 'Trend + Momentum', 'category': 'Trend',
     'description': 'Close above its SMA with RSI above the threshold enters; close below the SMA with RSI below '
                    'the threshold exits.' + _RSI_NOTE,
     'parameters': [_p('maPeriod', 'MA Period', 50, 10, 200), _p('rsiPeriod', 'RSI Period', 14, 5, 50),
                    _p('rsiThreshold', 'RSI Threshold', 50, 30, 70)]},
    {'type': 'mean_reversion', 'name': 'Z-Score Mean Reversion', 'category': 'Mean Reversion',
     'description': 'z-score of the close below -Z enters, above +Z exits, back inside +/-0.5 goes flat',
     'parameters': [_p('period', 'Lookback', 20, 5, 100), _p('zThreshold', 'Z Threshold', 2.0, 0.5, 4.0, 0.1, 'float')]},
    {'type': 'bollinger_bands', 'name': 'Bollinger Bands', 'category': 'Mean Reversion',
     'description': 'Close below the lower band enters, close above the upper band exits',
     'parameters': [_p('period', 'Period', 20, 5, 100), _p('stdDev', 'Std Dev', 2.0, 0.5, 4.0, 0.1, 'float')]},
    {'type': 'rsi', 'name': 'RSI', 'category': 'Mean Reversion',
     'description': 'RSI below oversold enters, RSI above overbought exits (level rule: long from the first '
                    'oversold close until the first overbought close).' + _RSI_NOTE,
     'parameters': [_p('period', 'Period', 14, 2, 50), _p('oversold', 'Oversold', 30, 10, 40),
                    _p('overbought', 'Overbought', 70, 60, 95)], 'constraints': _OS_OB},
    {'type': 'stochastic', 'name': 'Stochastic', 'category': 'Mean Reversion',
     'description': '%K crosses above %D below the oversold level (enter) / below %D above overbought (exit)',
     'parameters': [_p('kPeriod', '%K Period', 14, 5, 30), _p('dPeriod', '%D Period', 3, 2, 10),
                    _p('oversold', 'Oversold', 20, 5, 30), _p('overbought', 'Overbought', 80, 70, 95)],
     'constraints': _OS_OB},
    {'type': 'williams_r', 'name': 'Williams %R', 'category': 'Mean Reversion',
     'description': '%R below oversold enters, above overbought exits',
     'parameters': [_p('period', 'Period', 14, 5, 50), _p('oversold', 'Oversold', -80, -95, -60),
                    _p('overbought', 'Overbought', -20, -40, -5)], 'constraints': _OS_OB},
    {'type': 'momentum', 'name': 'Momentum', 'category': 'Momentum',
     'description': 'Rate of change over the lookback above +threshold % enters, below -threshold % exits',
     'parameters': [_p('lookback', 'Lookback', 20, 5, 100),
                    _p('threshold', 'Threshold %', 0.0, 0.0, 20.0, 0.5, 'float')]},
    {'type': 'dual_momentum', 'name': 'Dual Momentum', 'category': 'Momentum',
     'description': 'Short- AND long-lookback rate of change above the threshold % enter; the short one below '
                    '-threshold % exits',
     'parameters': [_p('shortLookback', 'Short Lookback', 20, 5, 60),
                    _p('longLookback', 'Long Lookback', 60, 30, 200),
                    _p('threshold', 'Threshold %', 0.0, 0.0, 10.0, 0.5, 'float')],
     'constraints': [{'lt': ['shortLookback', 'longLookback'], 'message': 'The short lookback must be shorter.'}]},
    {'type': 'cci', 'name': 'CCI', 'category': 'Momentum',
     'description': 'CCI below the lower level enters, above the upper level exits',
     'parameters': [_p('period', 'Period', 20, 5, 50), _p('lower', 'Lower', -100, -200, -50),
                    _p('upper', 'Upper', 100, 50, 200)],
     'constraints': [{'lt': ['lower', 'upper'], 'message': 'The lower level must be below the upper level.'}]},
    {'type': 'breakout', 'name': 'Donchian Breakout', 'category': 'Breakout',
     'description': 'Close above the previous N-bar high enters, below the previous N-bar low exits; a protective '
                    'stop ATR Stop x ATR from the entry fill (0 = none)',
     'parameters': [_p('period', 'Channel Period', 20, 5, 100),
                    _p('atrStop', 'ATR Stop', 1.5, 0.0, 6.0, 0.1, 'float')]},
    {'type': 'volatility_breakout', 'name': 'Volatility Breakout', 'category': 'Breakout',
     'description': 'When ATR exceeds K x its lookback average: an up bar enters, a down bar exits',
     'parameters': [_p('period', 'ATR Period', 20, 5, 50),
                    _p('kMultiplier', 'K Multiplier', 1.5, 0.5, 4.0, 0.1, 'float'),
                    _p('lookback', 'ATR Average Lookback', 20, 5, 100)]},
    {'type': 'rsi_macd', 'name': 'RSI + MACD', 'category': 'Multi-Indicator',
     'description': 'MACD histogram turns positive while RSI was below oversold within the lookback (enter); '
                    'RSI above overbought or the histogram turning negative exits.' + _RSI_NOTE,
     'parameters': [_p('rsiPeriod', 'RSI Period', 14, 5, 30), _p('oversold', 'Oversold', 30, 15, 40),
                    _p('overbought', 'Overbought', 70, 60, 90), _p('fastPeriod', 'MACD Fast', 12, 5, 30),
                    _p('slowPeriod', 'MACD Slow', 26, 15, 50), _p('signalPeriod', 'MACD Signal', 9, 3, 20),
                    _p('lookback', 'Oversold Lookback', 5, 1, 30)], 'constraints': _FAST_SLOW + _OS_OB},
    {'type': 'macd_adx', 'name': 'MACD + ADX', 'category': 'Multi-Indicator',
     'description': 'MACD crosses above its signal with ADX above the threshold (enter); MACD crosses below its '
                    'signal (exit)',
     'parameters': [_p('fastPeriod', 'MACD Fast', 12, 5, 30), _p('slowPeriod', 'MACD Slow', 26, 15, 50),
                    _p('signalPeriod', 'MACD Signal', 9, 3, 20), _p('adxPeriod', 'ADX Period', 14, 5, 30),
                    _p('adxThreshold', 'ADX Threshold', 25, 15, 50)], 'constraints': _FAST_SLOW},
    {'type': 'bollinger_rsi', 'name': 'Bollinger + RSI', 'category': 'Multi-Indicator',
     'description': 'Close below the lower band with RSI below oversold enters; close above the upper band with '
                    'RSI above overbought exits.' + _RSI_NOTE,
     'parameters': [_p('bbPeriod', 'BB Period', 20, 5, 50), _p('stdDev', 'Std Dev', 2.0, 1.0, 4.0, 0.1, 'float'),
                    _p('rsiPeriod', 'RSI Period', 14, 5, 30), _p('oversold', 'Oversold', 30, 15, 40),
                    _p('overbought', 'Overbought', 70, 60, 90)], 'constraints': _OS_OB},
    {'type': 'obv_trend', 'name': 'OBV Trend', 'category': 'Volume',
     'description': 'On-balance volume above its N-bar SMA enters, below it exits',
     'parameters': [_p('period', 'OBV SMA Period', 20, 5, 100)]},
    {'type': 'mfi', 'name': 'Money Flow Index', 'category': 'Volume',
     'description': 'Money flow index below oversold enters, above overbought exits',
     'parameters': [_p('period', 'Period', 14, 5, 50), _p('oversold', 'Oversold', 20, 5, 35),
                    _p('overbought', 'Overbought', 80, 65, 95)], 'constraints': _OS_OB},
    {'type': 'atr_trailing_stop', 'name': 'ATR Trailing Stop', 'category': 'Risk',
     'description': 'Close above its SMA enters (below exits); a stop multiplier x ATR below the close is raised at '
                    'every close and executed intrabar by backtesting.py',
     'parameters': [_p('atrPeriod', 'ATR Period', 14, 5, 50),
                    _p('multiplier', 'Multiplier', 3.0, 0.5, 6.0, 0.1, 'float'),
                    _p('maPeriod', 'MA Period', 20, 5, 200)]},
]
_BY_ID = {c['type']: c for c in CATALOG}


def catalog_entry(strategy_id: str) -> Optional[Dict[str, Any]]:
    return _BY_ID.get(strategy_id)


def default_params(strategy_id: str) -> Dict[str, Any]:
    c = _BY_ID.get(strategy_id) or {}
    return {p['id']: p['default'] for p in c.get('parameters', [])}


def constraint_ok(strategy_id: str, params: Dict[str, Any]) -> bool:
    for c in (_BY_ID.get(strategy_id) or {}).get('constraints', []):
        a, b = c['lt']
        try:
            if not float(params[a]) < float(params[b]):
                return False
        except (KeyError, TypeError, ValueError):
            continue
    return True


def get_strategy_catalog() -> List[Dict[str, Any]]:
    out = []
    for s in CATALOG:
        d = dict(s)
        d['id'] = s['type']
        d['kind'] = 'signals'
        d['runnable'] = True
        d['rsiFlavour'] = 'simple' if s['type'] in USES_RSI else None
        d['parameters'] = [dict(p) for p in s['parameters']]
        d['constraints'] = [dict(c) for c in s.get('constraints', [])]
        out.append(d)
    return out
