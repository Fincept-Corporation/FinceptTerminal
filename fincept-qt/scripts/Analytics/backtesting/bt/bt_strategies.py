"""
Strategy library of the "bt" engine (20 catalogue strategies). Nothing here imports the bt
library: signal strategies never need it, and allocation strategies are only DESCRIBED here
(the algo stack bt_algos builds from the description).

TWO KINDS (catalogue `kind`, and what runs - every result says it)
  signals     long / flat (short with allowShort) rules on each symbol's OWN bars, decided on a
              bar's close; executed by base.simulator's signals mode (an entry takes the symbol's
              slot of the equity, positions are held until the exit rule fires). The bt library is
              not used for these: it would only rebalance a 0/1 weight every bar (3.5 s of algo
              stack per 10-year run) without adding anything to the rule.
  allocation  the bt library's algo stack DECIDES target weights on its schedule (bt.algos
              RunDaily / RunWeekly / RunMonthly / RunQuarterly / RunYearly / RunEveryNPeriods /
              RunOnce, SelectAll, SelectN, WeighEqually + the Fincept bar-lookback algos of
              bt_algos that call ffn's weighting functions); base.simulator's weights mode (or bt's
              own Backtest for fillTiming next_close) executes them.

Lookbacks are in BARS (rolling windows on the bars), not calendar days (v1 used
pd.DateOffset(days=n): "lookback 20" was ~14 trading days - audit T3).

SIGNAL RULES (indicators computed once on the full history, causal, NaN during warm-up so no
rule fires before its inputs exist; entries / exits are conditions - the simulator enters only
when flat and exits only when holding, which gives each rule its hysteresis)
  sma_crossover    long while SMA(fast) > SMA(slow) (state rule: bt's SelectWhere convention -
                   the first bar with both averages and fast above enters)
  ema_crossover    long while EMA(fast) > EMA(slow) (EMAs seeded with the SMA of their first bars)
  macd             long while the MACD line > its signal line
  adx_trend        long while Wilder ADX > threshold and +DI > -DI; out when either fails
  mean_reversion   z-score of the close vs its N-bar mean (sample std): enter z < -threshold, exit
                   when z >= exitZ (default 0 = back at the mean)
  bollinger_bands  enter on a close below the lower band, exit on a close above the UPPER band
                   (bands = N-bar mean +- k x population std, Bollinger's definition). The entry
                   equals mean_reversion's at the same window / threshold; the exit differs (upper
                   band vs the mean), so the two never return the same trades (audit T0.2).
  rsi              Wilder RSI: enter below oversold, exit above OVERBOUGHT (v1 ignored overbought)
  stochastic       %K crosses above %D while %K < oversold (enter); %K crosses below %D while
                   %K > overbought (exit). %K over kPeriod high/low, %D = SMA(dPeriod) of %K.
  breakout         Donchian: close above the previous N-bar HIGH (enter) / below the previous
                   N-bar LOW (exit)
  momentum         rate of change over N bars: enter when ROC > threshold, exit when ROC <
                   -threshold (threshold is a fraction: 0.02 = 2 %)
ALLOCATION STACKS (bt algos; schedule = rebalancePeriod: daily | weekly | monthly | quarterly |
yearly, or a number of bars)
  buy_and_hold     RunOnce, SelectAll, WeighEqually (equal weights once, never rebalanced)
  equal_weight     schedule, SelectAll, WeighEqually
  inv_vol          schedule, SelectAll, WeighInvVolBars(lookback)            (1 / std of returns)
  mean_var         schedule, SelectAll, WeighMeanVarBars(lookback)           (ffn max-Sharpe, Ledoit-Wolf)
  min_var          schedule, SelectAll, WeighMinVarBars(lookback)            (minimum variance, Ledoit-Wolf)
  risk_parity      schedule, SelectAll, WeighERCBars(lookback)               (ffn equal risk contribution)
  target_vol       schedule, SelectAll, WeighEqually, TargetVolBars(target %, lookback) - scaled to the
                   target volatility, capped at 100 % invested (no leverage)
  momentum_topn    schedule, SelectAll, StatTotalReturnBars(lookback), SelectN(topN), WeighEqually
  momentum_inv_vol schedule, SelectAll, StatTotalReturnBars(lookback), SelectN(topN), WeighInvVolBars(lookback)
  dual_momentum    schedule, SelectAll, StatTotalReturnBars(relativePeriod), SelectN(topN),
                   AbsoluteMomentumBars(absolutePeriod, > 0), WeighEqually - relative momentum picks
                   the leaders, absolute momentum keeps only those with a positive return over
                   absolutePeriod (else cash): Antonacci's dual momentum. (momentum above is a
                   time-series signal on each symbol - a different strategy.)
"""

from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Tuple

import numpy as np

Signals = Tuple[np.ndarray, np.ndarray]


# ════════════════════════════════════════════════════════════════════════════
# Parameter helpers
# ════════════════════════════════════════════════════════════════════════════

def _i(params, key, default, *alts) -> int:
    for k in (key,) + alts:
        if k in params and params[k] is not None:
            try:
                return int(round(float(params[k])))
            except (TypeError, ValueError):
                raise ValueError(f'parameter {k} must be a number, not {params[k]!r}')
    return int(default)


def _f(params, key, default, *alts) -> float:
    for k in (key,) + alts:
        if k in params and params[k] is not None:
            try:
                return float(params[k])
            except (TypeError, ValueError):
                raise ValueError(f'parameter {k} must be a number, not {params[k]!r}')
    return float(default)


def _pos(name, v) -> int:
    if v < 1:
        raise ValueError(f'{name} must be at least 1 (got {v})')
    return v


# ════════════════════════════════════════════════════════════════════════════
# Indicators (numpy / pandas, vectorised, causal)
# ════════════════════════════════════════════════════════════════════════════

def sma(x, n: int) -> np.ndarray:
    from base.signals import sma as _sma
    return _sma(x, n)


def ema_valid(x, n: int) -> np.ndarray:
    """EMA (alpha 2/(n+1)) seeded with the SMA of the first n VALID values (works on a series with a
    NaN warm-up, e.g. the MACD line)."""
    from base.signals import ema as _ema
    v = np.asarray(x, dtype=float)
    out = np.full(len(v), np.nan)
    ok = np.flatnonzero(np.isfinite(v))
    if not len(ok):
        return out
    f = int(ok[0])
    out[f:] = _ema(v[f:], n)
    return out


def rolling_std(x, n: int, ddof: int = 1) -> np.ndarray:
    import pandas as pd
    return pd.Series(np.asarray(x, dtype=float)).rolling(int(n)).std(ddof=ddof).to_numpy()


def rolling_max(x, n: int) -> np.ndarray:
    import pandas as pd
    return pd.Series(np.asarray(x, dtype=float)).rolling(int(n)).max().to_numpy()


def rolling_min(x, n: int) -> np.ndarray:
    import pandas as pd
    return pd.Series(np.asarray(x, dtype=float)).rolling(int(n)).min().to_numpy()


def prev(x) -> np.ndarray:
    v = np.asarray(x, dtype=float)
    out = np.full(len(v), np.nan)
    out[1:] = v[:-1]
    return out


def wilder(x, n: int) -> np.ndarray:
    """Wilder's smoothing (RMA, alpha 1/n) seeded with the mean of the first n valid values."""
    import pandas as pd
    v = np.asarray(x, dtype=float)
    out = np.full(len(v), np.nan)
    ok = np.flatnonzero(np.isfinite(v))
    if len(ok) < n:
        return out
    f = int(ok[0])
    seed = float(np.mean(v[f:f + n]))
    tail = v[f + n:]
    s = pd.Series(np.concatenate([[seed], tail])).ewm(alpha=1.0 / n, adjust=False).mean().to_numpy()
    out[f + n - 1:] = s
    return out


def rsi(close, n: int) -> np.ndarray:
    from base.signals import rsi_wilder
    return rsi_wilder(close, n)


def macd(close, fast: int, slow: int, signal: int) -> Dict[str, np.ndarray]:
    from base.signals import ema as _ema
    line = _ema(close, fast) - _ema(close, slow)
    sig = ema_valid(line, signal)
    return {'macd': line, 'signal': sig, 'histogram': line - sig}


def adx(high, low, close, n: int) -> Dict[str, np.ndarray]:
    """Wilder's ADX / +DI / -DI."""
    h, lo, c = (np.asarray(a, dtype=float) for a in (high, low, close))
    pc = prev(c)
    tr = np.fmax(h - lo, np.fmax(np.abs(h - pc), np.abs(lo - pc)))
    tr[0] = np.nan
    up = h - prev(h)
    dn = prev(lo) - lo
    pdm = np.where((up > dn) & (up > 0), up, 0.0)
    mdm = np.where((dn > up) & (dn > 0), dn, 0.0)
    pdm[0] = mdm[0] = np.nan
    atr = wilder(tr, n)
    with np.errstate(divide='ignore', invalid='ignore'):
        pdi = 100.0 * wilder(pdm, n) / atr
        mdi = 100.0 * wilder(mdm, n) / atr
        dx = 100.0 * np.abs(pdi - mdi) / (pdi + mdi)
    dx = np.where(np.isfinite(dx), dx, np.nan)
    return {'adx': wilder(dx, n), 'plus_di': pdi, 'minus_di': mdi}


def stochastic(high, low, close, k: int, d: int) -> Dict[str, np.ndarray]:
    hh, ll = rolling_max(high, k), rolling_min(low, k)
    c = np.asarray(close, dtype=float)
    with np.errstate(divide='ignore', invalid='ignore'):
        kk = np.where(hh - ll > 0, 100.0 * (c - ll) / (hh - ll), 50.0)
    kk = np.where(np.isfinite(hh) & np.isfinite(ll), kk, np.nan)
    return {'k': kk, 'd': sma(kk, d)}


def roc(close, n: int) -> np.ndarray:
    c = np.asarray(close, dtype=float)
    out = np.full(len(c), np.nan)
    if len(c) > n:
        with np.errstate(divide='ignore', invalid='ignore'):
            out[n:] = c[n:] / c[:-n] - 1.0
    return out


def _gt(a, b) -> np.ndarray:
    a, b = np.broadcast_arrays(np.asarray(a, dtype=float), np.asarray(b, dtype=float))
    with np.errstate(invalid='ignore'):
        return np.isfinite(a) & np.isfinite(b) & (a > b)


def _lt(a, b) -> np.ndarray:
    a, b = np.broadcast_arrays(np.asarray(a, dtype=float), np.asarray(b, dtype=float))
    with np.errstate(invalid='ignore'):
        return np.isfinite(a) & np.isfinite(b) & (a < b)


def _ge(a, b) -> np.ndarray:
    a, b = np.broadcast_arrays(np.asarray(a, dtype=float), np.asarray(b, dtype=float))
    with np.errstate(invalid='ignore'):
        return np.isfinite(a) & np.isfinite(b) & (a >= b)


def _state(cond: np.ndarray, valid: np.ndarray) -> Signals:
    """A 'long while cond' rule: enter while it holds, exit once it fails (on valid bars)."""
    c = np.asarray(cond, dtype=bool)
    return c, np.asarray(valid, dtype=bool) & ~c


# ════════════════════════════════════════════════════════════════════════════
# Signal builders: (o, h, l, c, v, params) -> (entries, exits)
# ════════════════════════════════════════════════════════════════════════════

class Bars:
    """One symbol's OHLCV arrays (its own session dates) with a small indicator memo."""

    def __init__(self, o, h, l, c, v=None):
        self.c = np.asarray(c, dtype=float)
        self.o = self.c if o is None else np.asarray(o, dtype=float)
        self.h = self.c if h is None else np.asarray(h, dtype=float)
        self.l = self.c if l is None else np.asarray(l, dtype=float)
        self.v = np.zeros(len(self.c)) if v is None else np.asarray(v, dtype=float)
        self.n = len(self.c)
        self._memo: Dict[tuple, Any] = {}

    def memo(self, key, fn):
        hit = self._memo.get(key)
        if hit is None:
            hit = fn()
            if len(self._memo) > 512:
                self._memo.clear()
            self._memo[key] = hit
        return hit


def _sma_crossover(b: Bars, p) -> Signals:
    f, s = _pos('fastPeriod', _i(p, 'fastPeriod', 10)), _pos('slowPeriod', _i(p, 'slowPeriod', 20))
    a = b.memo(('sma', f), lambda: sma(b.c, f))
    z = b.memo(('sma', s), lambda: sma(b.c, s))
    return _state(_gt(a, z), np.isfinite(a) & np.isfinite(z))


def _ema_crossover(b: Bars, p) -> Signals:
    from base.signals import ema as _ema
    f, s = _pos('fastPeriod', _i(p, 'fastPeriod', 10)), _pos('slowPeriod', _i(p, 'slowPeriod', 20))
    a = b.memo(('ema', f), lambda: _ema(b.c, f))
    z = b.memo(('ema', s), lambda: _ema(b.c, s))
    return _state(_gt(a, z), np.isfinite(a) & np.isfinite(z))


def _macd(b: Bars, p) -> Signals:
    f, s, g = (_pos(k, _i(p, k, d)) for k, d in (('fastPeriod', 12), ('slowPeriod', 26), ('signalPeriod', 9)))
    m = b.memo(('macd', f, s, g), lambda: macd(b.c, f, s, g))
    return _state(_gt(m['macd'], m['signal']), np.isfinite(m['macd']) & np.isfinite(m['signal']))


def _adx_trend(b: Bars, p) -> Signals:
    n = _pos('adxPeriod', _i(p, 'adxPeriod', 14, 'period'))
    th = _f(p, 'adxThreshold', 25, 'threshold')
    d = b.memo(('adx', n), lambda: adx(b.h, b.l, b.c, n))
    valid = np.isfinite(d['adx']) & np.isfinite(d['plus_di']) & np.isfinite(d['minus_di'])
    return _state(_gt(d['adx'], th) & _gt(d['plus_di'], d['minus_di']), valid)


def _zscore(b: Bars, n: int) -> np.ndarray:
    def calc():
        m, sd = sma(b.c, n), rolling_std(b.c, n, ddof=1)
        with np.errstate(divide='ignore', invalid='ignore'):
            z = (b.c - m) / sd
        return np.where(np.isfinite(z), z, np.nan)
    return b.memo(('z', n), calc)


def _mean_reversion(b: Bars, p) -> Signals:
    n = _pos('window', _i(p, 'window', 20, 'period'))
    th = _f(p, 'threshold', 2.0, 'zThreshold')
    ex = _f(p, 'exitZ', 0.0)
    z = _zscore(b, n)
    return _lt(z, -th), _ge(z, ex)


def _bollinger_bands(b: Bars, p) -> Signals:
    n = _pos('period', _i(p, 'period', 20))
    k = _f(p, 'stdDev', 2.0)

    def bands():
        m, sd = sma(b.c, n), rolling_std(b.c, n, ddof=0)
        return m - k * sd, m + k * sd
    lo, up = b.memo(('bb', n, k), bands)
    return _lt(b.c, lo), _gt(b.c, up)


def _rsi(b: Bars, p) -> Signals:
    n = _pos('period', _i(p, 'period', 14))
    r = b.memo(('rsi', n), lambda: rsi(b.c, n))
    return _lt(r, _f(p, 'oversold', 30)), _gt(r, _f(p, 'overbought', 70))


def _stochastic(b: Bars, p) -> Signals:
    from base.signals import crossover, crossunder
    k, d = _pos('kPeriod', _i(p, 'kPeriod', 14)), _pos('dPeriod', _i(p, 'dPeriod', 3))
    st = b.memo(('stoch', k, d), lambda: stochastic(b.h, b.l, b.c, k, d))
    kk, dd = st['k'], st['d']
    return (crossover(kk, dd) & _lt(kk, _f(p, 'oversold', 20)),
            crossunder(kk, dd) & _gt(kk, _f(p, 'overbought', 80)))


def _breakout(b: Bars, p) -> Signals:
    n = _pos('period', _i(p, 'period', 20))
    up = b.memo(('dc_up', n), lambda: prev(rolling_max(b.h, n)))
    lo = b.memo(('dc_lo', n), lambda: prev(rolling_min(b.l, n)))
    return _gt(b.c, up), _lt(b.c, lo)


def _momentum(b: Bars, p) -> Signals:
    n = _pos('period', _i(p, 'period', 12, 'lookback'))
    th = _f(p, 'threshold', 0.02)
    r = b.memo(('roc', n), lambda: roc(b.c, n))
    return _gt(r, th), _lt(r, -th)


BUILDERS: Dict[str, Callable[[Bars, Dict[str, Any]], Signals]] = {
    'sma_crossover': _sma_crossover, 'ema_crossover': _ema_crossover, 'macd': _macd, 'adx_trend': _adx_trend,
    'mean_reversion': _mean_reversion, 'bollinger_bands': _bollinger_bands, 'rsi': _rsi,
    'stochastic': _stochastic, 'breakout': _breakout, 'momentum': _momentum,
}

# ════════════════════════════════════════════════════════════════════════════
# Allocation stacks (descriptions; bt_algos builds the bt algos)
# ════════════════════════════════════════════════════════════════════════════

PERIODS = ('daily', 'weekly', 'monthly', 'quarterly', 'yearly')
_PERIOD_ALIASES = {'day': 'daily', 'd': 'daily', 'week': 'weekly', 'w': 'weekly', 'month': 'monthly', 'm': 'monthly',
                   'quarter': 'quarterly', 'q': 'quarterly', 'year': 'yearly', 'annual': 'yearly',
                   'annually': 'yearly', 'y': 'yearly'}


def parse_period(value: Any):
    """rebalancePeriod -> 'daily' | 'weekly' | 'monthly' | 'quarterly' | 'yearly' or an int number of
    bars (a number sent by an older screen's spin box is honoured as bars)."""
    if value is None or (isinstance(value, str) and not value.strip()):
        return 'monthly'
    if isinstance(value, str):
        s = value.strip().lower()
        s = _PERIOD_ALIASES.get(s, s)
        if s in PERIODS:
            return s
        try:
            value = float(s)
        except ValueError:
            raise ValueError(f'rebalancePeriod must be one of {", ".join(PERIODS)} or a number of bars '
                             f'(got {value!r})')
    try:
        n = int(round(float(value)))
    except (TypeError, ValueError):
        raise ValueError(f'rebalancePeriod must be one of {", ".join(PERIODS)} or a number of bars (got {value!r})')
    if n < 1:
        raise ValueError('rebalancePeriod must be at least 1 bar')
    return n


def describe_period(per) -> str:
    return f'every {per} bars' if isinstance(per, int) else per


def allocation_stack(sid: str, p: Dict[str, Any]) -> Dict[str, Any]:
    """{'schedule': period | 'once', 'steps': [(algo, kwargs)...], 'lookback': bars or 0} - the bt algo stack
    of an allocation strategy (bt_algos.build_stack turns it into bt algos)."""
    if sid == 'buy_and_hold':
        return {'schedule': 'once', 'steps': [('SelectAll', {}), ('WeighEqually', {})], 'lookback': 0}
    per = parse_period(p.get('rebalancePeriod', 'monthly'))
    if sid == 'equal_weight':
        steps = [('SelectAll', {}), ('WeighEqually', {})]
        look = 0
    elif sid in ('inv_vol', 'mean_var', 'min_var', 'risk_parity'):
        look = _pos('lookback', _i(p, 'lookback', 20 if sid == 'inv_vol' else 60))
        algo = {'inv_vol': 'WeighInvVolBars', 'mean_var': 'WeighMeanVarBars', 'min_var': 'WeighMinVarBars',
                'risk_parity': 'WeighERCBars'}[sid]
        steps = [('SelectAll', {}), (algo, {'lookback': look})]
    elif sid == 'target_vol':
        look = _pos('lookback', _i(p, 'lookback', 20))
        tv = _f(p, 'targetVol', 10.0)
        if not tv > 0:
            raise ValueError('targetVol must be a positive percentage')
        steps = [('SelectAll', {}), ('WeighEqually', {}), ('TargetVolBars', {'target': tv / 100.0, 'lookback': look})]
    elif sid in ('momentum_topn', 'momentum_inv_vol'):
        look = _pos('lookback', _i(p, 'lookback', 60))
        n = _pos('topN', _i(p, 'topN', 2))
        last = ('WeighEqually', {}) if sid == 'momentum_topn' else ('WeighInvVolBars', {'lookback': look})
        steps = [('SelectAll', {}), ('StatTotalReturnBars', {'lookback': look}), ('SelectN', {'n': n}), last]
    elif sid == 'dual_momentum':
        rel = _pos('relativePeriod', _i(p, 'relativePeriod', 252))
        ab = _pos('absolutePeriod', _i(p, 'absolutePeriod', 252))
        n = _pos('topN', _i(p, 'topN', 1))
        look = max(rel, ab)
        steps = [('SelectAll', {}), ('StatTotalReturnBars', {'lookback': rel}), ('SelectN', {'n': n}),
                 ('AbsoluteMomentumBars', {'lookback': ab, 'threshold': 0.0}), ('WeighEqually', {})]
    else:
        raise ValueError(f'{sid} is not an allocation strategy')
    return {'schedule': per, 'steps': steps, 'lookback': look}


def describe_stack(stack: Dict[str, Any]) -> str:
    sch = stack['schedule']
    sched = 'RunOnce' if sch == 'once' else (f'RunEveryNPeriods({sch})' if isinstance(sch, int) else
                                            {'daily': 'RunDaily', 'weekly': 'RunWeekly', 'monthly': 'RunMonthly',
                                             'quarterly': 'RunQuarterly', 'yearly': 'RunYearly'}[sch] + '()')
    parts = [sched] + [f'{a}({", ".join(f"{k}={v}" for k, v in kw.items())})' for a, kw in stack['steps']]
    return ' > '.join(parts)


# ════════════════════════════════════════════════════════════════════════════
# Registry
# ════════════════════════════════════════════════════════════════════════════

ALLOCATION = ('buy_and_hold', 'equal_weight', 'inv_vol', 'mean_var', 'min_var', 'risk_parity', 'target_vol',
              'momentum_topn', 'momentum_inv_vol', 'dual_momentum')
# cross-sectional stacks: with one symbol they would hold it 100 % (= buy_and_hold's result under another
# name - audit T0.1 / T0.2), so they refuse to run on fewer symbols (INPUT error naming the reason)
MIN_SYMBOLS = {'equal_weight': 2, 'inv_vol': 2, 'mean_var': 2, 'min_var': 2, 'risk_parity': 2, 'momentum_topn': 2,
               'momentum_inv_vol': 2}
ALIASES = {'sma': 'sma_crossover', 'ema': 'ema_crossover', 'macd_crossover': 'macd', 'bollinger': 'bollinger_bands',
           'zscore': 'mean_reversion', 'donchian_breakout': 'breakout', 'buy_hold': 'buy_and_hold',
           'buyandhold': 'buy_and_hold', 'inverse_volatility': 'inv_vol', 'erc': 'risk_parity',
           'min_variance': 'min_var', 'mean_variance': 'mean_var', 'rebalance_periodic': 'equal_weight'}


def resolve(strategy_type: Any) -> str:
    s = str(strategy_type or 'sma_crossover').strip().lower()
    s = ALIASES.get(s, s)
    if s not in BUILDERS and s not in ALLOCATION:
        raise ValueError(f'Unknown strategy {strategy_type!r}. Available: '
                         f'{", ".join(sorted(set(BUILDERS) | set(ALLOCATION)))}.')
    return s


def kind_of(sid: str) -> str:
    return 'allocation' if sid in ALLOCATION else 'signals'


def build_signals(sid: str, bars: Bars, params: Dict[str, Any]) -> Signals:
    e, x = BUILDERS[sid](bars, params or {})
    return np.asarray(e, dtype=bool), np.asarray(x, dtype=bool)


def default_warmup(sid: str, params: Dict[str, Any]) -> int:
    """Bars a strategy's inputs need before its first possible decision (provenance, walk-forward
    warm-up)."""
    p = params or {}
    if sid in ALLOCATION:
        try:
            return int(allocation_stack(sid, p)['lookback'])
        except ValueError:
            return 0
    if sid == 'macd':
        return _i(p, 'slowPeriod', 26) + _i(p, 'signalPeriod', 9)
    if sid == 'adx_trend':
        return 2 * _i(p, 'adxPeriod', 14, 'period')
    if sid == 'stochastic':
        return _i(p, 'kPeriod', 14) + _i(p, 'dPeriod', 3)
    look = [int(round(float(v))) for k, v in p.items() if isinstance(v, (int, float)) and not isinstance(v, bool)
            and any(w in k.lower() for w in ('period', 'lookback', 'window'))]
    return int(max(look)) if look else 0


# ════════════════════════════════════════════════════════════════════════════
# Catalogue (get_strategies)
# ════════════════════════════════════════════════════════════════════════════

def _p(pid, name, default, lo, hi, step=1, typ='int', desc=None, choices=None):
    d = {'id': pid, 'name': name, 'label': name, 'type': typ, 'default': default, 'min': lo, 'max': hi, 'step': step}
    if choices is not None:
        d['choices'] = list(choices)
    if desc:
        d['description'] = desc
    return d


_REBAL = _p('rebalancePeriod', 'Rebalance', 'monthly', None, None, None, 'choice',
            'When bt\'s schedule algo fires (first bar of each period); a number = every N bars', PERIODS)
_FAST_SLOW = [{'lt': ['fastPeriod', 'slowPeriod'], 'message': 'The fast period must be shorter than the slow period.'}]
_SIM = 'base.simulator (signals mode) - the bt library is not used for signal strategies'
_BTS = 'bt library algo stack decides target weights; base.simulator executes them (bt itself at next_close)'

CATALOG: List[Dict[str, Any]] = [
    # ── allocation (bt algo stack) ─────────────────────────────────────────
    {'type': 'buy_and_hold', 'name': 'Buy & Hold', 'category': 'portfolio',
     'description': 'bt RunOnce + SelectAll + WeighEqually: equal weights on the first decision, never rebalanced',
     'parameters': []},
    {'type': 'equal_weight', 'name': 'Equal Weight', 'category': 'portfolio',
     'description': 'Equal weights across all symbols, rebalanced on bt\'s schedule (monthly by default; needs 2+ '
                    'symbols - on one symbol it would be buy and hold)',
     'parameters': [_REBAL]},
    {'type': 'inv_vol', 'name': 'Inverse Volatility', 'category': 'portfolio',
     'description': 'Weights proportional to 1 / volatility of the last N bars (needs 2+ symbols)',
     'parameters': [_p('lookback', 'Lookback (bars)', 20, 5, 252), _REBAL]},
    {'type': 'mean_var', 'name': 'Mean-Variance (max Sharpe)', 'category': 'portfolio',
     'description': 'Markowitz tangency weights (ffn calc_mean_var_weights, Ledoit-Wolf covariance, long only) '
                    'from the last N bars (needs 2+ symbols)',
     'parameters': [_p('lookback', 'Lookback (bars)', 60, 20, 504), _REBAL]},
    {'type': 'min_var', 'name': 'Minimum Variance', 'category': 'portfolio',
     'description': 'Long-only minimum-variance weights (Ledoit-Wolf covariance of the last N bars; no return '
                    'estimate) (needs 2+ symbols)',
     'parameters': [_p('lookback', 'Lookback (bars)', 60, 20, 504), _REBAL]},
    {'type': 'risk_parity', 'name': 'Risk Parity (ERC)', 'category': 'portfolio',
     'description': 'Equal risk contribution weights (ffn calc_erc_weights, Ledoit-Wolf) from the last N bars '
                    '(needs 2+ symbols)',
     'parameters': [_p('lookback', 'Lookback (bars)', 60, 20, 504), _REBAL]},
    {'type': 'target_vol', 'name': 'Target Volatility', 'category': 'portfolio',
     'description': 'Equal weights scaled so the portfolio\'s N-bar volatility meets the target; capped at 100 % '
                    'invested (the rest stays in cash)',
     'parameters': [_p('targetVol', 'Target Vol (%)', 10.0, 1.0, 50.0, 0.5, 'float'),
                    _p('lookback', 'Lookback (bars)', 20, 5, 252), _REBAL]},
    {'type': 'momentum_topn', 'name': 'Momentum Top-N', 'category': 'momentum',
     'description': 'Hold the N symbols with the highest N-bar total return, equal weight (needs 2+ symbols)',
     'parameters': [_p('topN', 'Top N', 2, 1, 50), _p('lookback', 'Lookback (bars)', 60, 5, 504), _REBAL]},
    {'type': 'momentum_inv_vol', 'name': 'Momentum + Inv Vol', 'category': 'momentum',
     'description': 'Top N by N-bar total return, weighted by inverse volatility (needs 2+ symbols)',
     'parameters': [_p('topN', 'Top N', 2, 1, 50), _p('lookback', 'Lookback (bars)', 60, 5, 504), _REBAL]},
    {'type': 'dual_momentum', 'name': 'Dual Momentum', 'category': 'momentum',
     'description': 'Relative momentum picks the top N symbols by return over the relative period; absolute '
                    'momentum keeps only those with a positive return over the absolute period (else cash)',
     'parameters': [_p('relativePeriod', 'Relative Period (bars)', 252, 20, 504),
                    _p('absolutePeriod', 'Absolute Period (bars)', 252, 20, 504), _p('topN', 'Top N', 1, 1, 50),
                    _REBAL]},
    # ── signals (base.simulator) ───────────────────────────────────────────
    {'type': 'momentum', 'name': 'Momentum (ROC)', 'category': 'momentum',
     'description': 'Long when the N-bar rate of change > threshold, out when it falls below -threshold',
     'parameters': [_p('period', 'Period (bars)', 12, 2, 252),
                    _p('threshold', 'Threshold (fraction)', 0.02, 0.0, 0.2, 0.005, 'float', '0.02 = 2 %')]},
    {'type': 'sma_crossover', 'name': 'SMA Crossover', 'category': 'trend',
     'description': 'Long while the fast SMA is above the slow SMA',
     'parameters': [_p('fastPeriod', 'Fast Period', 10, 2, 100), _p('slowPeriod', 'Slow Period', 20, 5, 300)],
     'constraints': _FAST_SLOW},
    {'type': 'ema_crossover', 'name': 'EMA Crossover', 'category': 'trend',
     'description': 'Long while the fast EMA is above the slow EMA',
     'parameters': [_p('fastPeriod', 'Fast Period', 10, 2, 100), _p('slowPeriod', 'Slow Period', 20, 5, 300)],
     'constraints': _FAST_SLOW},
    {'type': 'macd', 'name': 'MACD', 'category': 'trend', 'description': 'Long while the MACD line is above its signal line',
     'parameters': [_p('fastPeriod', 'Fast Period', 12, 2, 50), _p('slowPeriod', 'Slow Period', 26, 10, 100),
                    _p('signalPeriod', 'Signal Period', 9, 2, 50)], 'constraints': _FAST_SLOW},
    {'type': 'adx_trend', 'name': 'ADX Trend', 'category': 'trend',
     'description': 'Long while Wilder ADX > threshold and +DI > -DI',
     'parameters': [_p('adxPeriod', 'ADX Period', 14, 5, 50), _p('adxThreshold', 'ADX Threshold', 25.0, 10.0, 50.0, 1.0,
                                                                   'float')]},
    {'type': 'breakout', 'name': 'Donchian Breakout', 'category': 'breakout',
     'description': 'Enter on a close above the previous N-bar high, exit on a close below the previous N-bar low',
     'parameters': [_p('period', 'Channel Period', 20, 5, 200)]},
    {'type': 'mean_reversion', 'name': 'Z-Score Reversion', 'category': 'meanReversion',
     'description': 'Enter when the z-score of the close < -threshold, exit when it is back at exitZ (0 = the mean)',
     'parameters': [_p('window', 'Window', 20, 5, 200), _p('threshold', 'Z Threshold', 2.0, 0.5, 4.0, 0.1, 'float'),
                    _p('exitZ', 'Exit Z', 0.0, -2.0, 3.0, 0.1, 'float')]},
    {'type': 'bollinger_bands', 'name': 'Bollinger Bands', 'category': 'meanReversion',
     'description': 'Enter on a close below the lower band, exit on a close above the upper band',
     'parameters': [_p('period', 'Period', 20, 5, 200), _p('stdDev', 'Std Dev', 2.0, 0.5, 4.0, 0.1, 'float')]},
    {'type': 'rsi', 'name': 'RSI Mean Reversion', 'category': 'meanReversion',
     'description': 'Wilder RSI: enter below oversold, exit above overbought',
     'parameters': [_p('period', 'Period', 14, 2, 50), _p('oversold', 'Oversold', 30.0, 5.0, 50.0, 1.0, 'float'),
                    _p('overbought', 'Overbought', 70.0, 50.0, 95.0, 1.0, 'float')],
     'constraints': [{'lt': ['oversold', 'overbought'], 'message': 'Oversold must be below overbought.'}]},
    {'type': 'stochastic', 'name': 'Stochastic', 'category': 'meanReversion',
     'description': '%K crosses above %D below the oversold level (enter) / below %D above overbought (exit)',
     'parameters': [_p('kPeriod', '%K Period', 14, 3, 50), _p('dPeriod', '%D Period', 3, 1, 20),
                    _p('oversold', 'Oversold', 20.0, 5.0, 45.0, 1.0, 'float'),
                    _p('overbought', 'Overbought', 80.0, 55.0, 95.0, 1.0, 'float')],
     'constraints': [{'lt': ['oversold', 'overbought'], 'message': 'Oversold must be below overbought.'}]},
]


def catalog_entry(sid: str) -> Optional[Dict[str, Any]]:
    return next((c for c in CATALOG if c['type'] == sid), None)


def defaults(sid: str) -> Dict[str, Any]:
    c = catalog_entry(sid)
    return {p['id']: p['default'] for p in (c or {}).get('parameters', [])}


def get_strategy_catalog() -> List[Dict[str, Any]]:
    out = []
    for s in CATALOG:
        d = dict(s)
        d['id'] = s['type']
        d['kind'] = kind_of(s['type'])
        d['implementation'] = _BTS if d['kind'] == 'allocation' else _SIM
        d['minSymbols'] = MIN_SYMBOLS.get(s['type'], 1)
        d.setdefault('constraints', [])
        d['parameters'] = [dict(p) for p in s['parameters']]
        d['params'] = d['parameters']
        out.append(d)
    return out
