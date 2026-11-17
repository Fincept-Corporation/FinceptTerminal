"""
Strategy library of the vectorbt engine (23 catalogue strategies).

Each SIGNAL strategy is a builder(ta, params) -> (entries, exits): boolean numpy arrays over one
symbol's own bars, decided on each bar's CLOSE (base.simulator fills them at the next open by
default). Indicators come from vbt_indicators.TA (memoised, causal, NaN during warm-up, so no rule
fires before its indicators exist). Crossovers are warm-up gated (base.signals): the first bar on
which both lines exist is not a cross.

ALLOCATION strategies (rebalance_periodic) return target weights instead and run in
base.simulator's weights mode (a real rebalance of one portfolio with shared cash).

Rules (what each strategy does; levels are inclusive of the catalogue defaults):
  sma_crossover / ema_crossover  enter when the fast average crosses above the slow one, exit on
                                 the cross below.
  macd               MACD line crosses its signal line (above = enter, below = exit).
  adx_trend          enter while ADX > threshold and +DI > -DI; exit when -DI > +DI with ADX >
                     threshold, or ADX < 0.7 x threshold (trend gone).
  keltner_breakout   enter on a close above the upper Keltner band, exit on a close below the middle.
  triple_ma          enter while EMA fast > medium > slow, exit when fast < medium.
  mean_reversion     z-score crosses below -z (enter) / above +z/2 (exit).
  bollinger_bands    enter on a close below the lower band, exit on a close above the upper band.
  rsi                Wilder RSI crosses below oversold (enter) / above overbought (exit).
  stochastic         %K crosses above %D while %K < oversold (enter); %K crosses below %D while
                     %K > overbought (exit).
  momentum           enter while ROC(lookback) > threshold %, exit while ROC < -threshold %.
  dual_momentum      enter while short and long ROC > threshold %, exit when the short ROC < -threshold.
  breakout           Donchian: close above the previous n-bar high (enter) / below the previous
                     n-bar low (exit).
  volatility_breakout close > previous close + k x ATR (enter) / < previous close - k x ATR (exit).
  rsi_macd           enter when the MACD histogram turns positive and the RSI was below oversold
                     within the last `lookback` bars; exit when RSI > overbought or the histogram
                     turns negative. (v1 required RSI < oversold and histogram > 0 on the SAME bar,
                     which never happened on SPY in ten years.)
  macd_adx           enter while MACD > signal and ADX > threshold, exit when MACD < signal.
  bollinger_rsi      enter on close < lower band and RSI < oversold; exit on close > upper band or
                     RSI > overbought.
  williams_r         enter while %R < oversold, exit while %R > overbought.
  cci                enter while CCI < lower, exit while CCI > upper.
  obv_trend          enter while OBV > its n-bar SMA, exit while below.
  custom_combo       2-3 indicator conditions (comboConfig from the screen) combined with AND / OR
                     (exit conditions are OR-ed); without a comboConfig: RSI(14) below 35 AND close
                     above SMA(200) (buy pullbacks in an uptrend), exit on RSI above 70 OR a close
                     below SMA(200). For sma / ema rows the indicator value is the close's distance
                     from the average in percent (threshold 0 = above it; v1 compared the average's
                     price level with the threshold, so "SMA above 0" was always true).
  buy_and_hold       enter on the first bar, never exit (equal slots across symbols, no rebalance).
  rebalance_periodic target weights (equal, or the request's `weights`) set every
                     `rebalancePeriod` bars - or 'weekly' / 'monthly' / 'quarterly' / 'yearly'
                     (first bar of each calendar period) - and traded to at the next fill.
"""

from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Tuple

import numpy as np

import vbt_indicators as I
from vbt_indicators import TA, cross_above, cross_below, edge, gt, lt, prev

Signals = Tuple[np.ndarray, np.ndarray]


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
# Signal builders
# ════════════════════════════════════════════════════════════════════════════

def _sma_crossover(ta: TA, p) -> Signals:
    f, s = _pos('fastPeriod', _i(p, 'fastPeriod', 10)), _pos('slowPeriod', _i(p, 'slowPeriod', 20))
    a, b = ta.sma(f), ta.sma(s)
    return cross_above(a, b), cross_below(a, b)


def _ema_crossover(ta: TA, p) -> Signals:
    f, s = _pos('fastPeriod', _i(p, 'fastPeriod', 10)), _pos('slowPeriod', _i(p, 'slowPeriod', 20))
    a, b = ta.ema(f), ta.ema(s)
    return cross_above(a, b), cross_below(a, b)


def _macd(ta: TA, p) -> Signals:
    m = ta.macd(_i(p, 'fastPeriod', 12), _i(p, 'slowPeriod', 26), _i(p, 'signalPeriod', 9))
    return cross_above(m['macd'], m['signal']), cross_below(m['macd'], m['signal'])


def _adx_trend(ta: TA, p) -> Signals:
    d = ta.adx(_pos('period', _i(p, 'period', 14)))
    th = _f(p, 'threshold', 25)
    a, pdi, mdi = d['adx'], d['plus_di'], d['minus_di']
    ent = gt(a, th) & gt(pdi, mdi)
    ex = (gt(a, th) & gt(mdi, pdi)) | lt(a, th * 0.7)
    return ent, ex


def _keltner_breakout(ta: TA, p) -> Signals:
    k = ta.keltner(_i(p, 'period', 20, 'emaPeriod'), _i(p, 'atrPeriod', 10), _f(p, 'multiplier', 2.0))
    return gt(ta.c, k['upper']), lt(ta.c, k['middle'])


def _triple_ma(ta: TA, p) -> Signals:
    f, m, s = ta.ema(_i(p, 'fastPeriod', 5)), ta.ema(_i(p, 'mediumPeriod', 20)), ta.ema(_i(p, 'slowPeriod', 50))
    return gt(f, m) & gt(m, s), lt(f, m)


def _mean_reversion(ta: TA, p) -> Signals:
    z = ta.zscore(_pos('period', _i(p, 'period', 20)))
    th = _f(p, 'zThreshold', 2.0, 'z_threshold')
    return cross_below(z, -th), cross_above(z, th * 0.5)


def _bollinger_bands(ta: TA, p) -> Signals:
    bb = ta.bbands(_pos('period', _i(p, 'period', 20)), _f(p, 'stdDev', 2.0))
    return lt(ta.c, bb['lower']), gt(ta.c, bb['upper'])


def _rsi(ta: TA, p) -> Signals:
    r = ta.rsi(_pos('period', _i(p, 'period', 14)))
    return cross_below(r, _f(p, 'oversold', 30)), cross_above(r, _f(p, 'overbought', 70))


def _stochastic(ta: TA, p) -> Signals:
    st = ta.stoch(_i(p, 'kPeriod', 14, 'k_period'), _i(p, 'dPeriod', 3, 'd_period'))
    k, d = st['k'], st['d']
    return cross_above(k, d) & lt(k, _f(p, 'oversold', 20)), cross_below(k, d) & gt(k, _f(p, 'overbought', 80))


def _momentum(ta: TA, p) -> Signals:
    roc = ta.momentum(_pos('lookback', _i(p, 'lookback', 20, 'period')))
    th = _f(p, 'threshold', 0.0) / 100.0
    return gt(roc, th), lt(roc, -th)


def _dual_momentum(ta: TA, p) -> Signals:
    s = ta.momentum(_pos('shortLookback', _i(p, 'shortLookback', 20)))
    lg = ta.momentum(_pos('longLookback', _i(p, 'longLookback', 60)))
    th = _f(p, 'threshold', 0.0) / 100.0
    return gt(s, th) & gt(lg, th), lt(s, -th)


def _breakout(ta: TA, p) -> Signals:
    dc = ta.donchian(_pos('period', _i(p, 'period', 20)))
    return gt(ta.c, prev(dc['upper'])), lt(ta.c, prev(dc['lower']))


def _volatility_breakout(ta: TA, p) -> Signals:
    a = ta.atr(_pos('period', _i(p, 'period', 20)))
    k = _f(p, 'kMultiplier', 1.5, 'atr_mult')
    pc = prev(ta.c)
    return gt(ta.c, pc + k * a), lt(ta.c, pc - k * a)


def _rsi_macd(ta: TA, p) -> Signals:
    r = ta.rsi(_pos('rsiPeriod', _i(p, 'rsiPeriod', 14, 'period')))
    m = ta.macd(_i(p, 'fastPeriod', 12), _i(p, 'slowPeriod', 26), _i(p, 'signalPeriod', 9))
    look = _pos('lookback', _i(p, 'lookback', 5))
    os_, ob = _f(p, 'oversold', 30), _f(p, 'overbought', 70)
    was_oversold = I.rolling_min(np.where(np.isfinite(r), r, np.inf), look) < os_
    h = m['histogram']
    return cross_above(h, 0.0) & was_oversold, gt(r, ob) | cross_below(h, 0.0)


def _macd_adx(ta: TA, p) -> Signals:
    m = ta.macd(_i(p, 'fastPeriod', 12), _i(p, 'slowPeriod', 26), _i(p, 'signalPeriod', 9))
    a = ta.adx(_pos('adxPeriod', _i(p, 'adxPeriod', 14)))['adx']
    th = _f(p, 'adxThreshold', 25, 'threshold')
    bull = gt(m['macd'], m['signal'])
    return bull & gt(a, th), lt(m['macd'], m['signal'])


def _bollinger_rsi(ta: TA, p) -> Signals:
    bb = ta.bbands(_pos('bbPeriod', _i(p, 'bbPeriod', 20, 'period')), _f(p, 'stdDev', 2.0))
    r = ta.rsi(_pos('rsiPeriod', _i(p, 'rsiPeriod', 14)))
    return (lt(ta.c, bb['lower']) & lt(r, _f(p, 'oversold', 30)),
            gt(ta.c, bb['upper']) | gt(r, _f(p, 'overbought', 70)))


def _williams_r(ta: TA, p) -> Signals:
    w = ta.williams_r(_pos('period', _i(p, 'period', 14)))
    return lt(w, _f(p, 'oversold', -80)), gt(w, _f(p, 'overbought', -20))


def _cci(ta: TA, p) -> Signals:
    c = ta.cci(_pos('period', _i(p, 'period', 20)))
    return lt(c, _f(p, 'lower', -100)), gt(c, _f(p, 'upper', 100))


def _obv_trend(ta: TA, p) -> Signals:
    n = _pos('period', _i(p, 'period', 20))
    o = ta.obv()
    m = ta.memo(('obv_sma', n), lambda: I.sma(o, n))
    return gt(o, m), lt(o, m)


DEFAULT_COMBO = {'logic': 'and', 'indicators': [
    {'type': 'rsi', 'period': 14, 'condition': 'below', 'threshold': 35, 'exitCondition': 'above', 'exitThreshold': 70},
    {'type': 'sma', 'period': 200, 'condition': 'above', 'threshold': 0, 'exitCondition': 'below',
     'exitThreshold': 0},
]}
COMBO_TYPES = ('rsi', 'sma', 'ema', 'macd_hist', 'bbands_pctb', 'stochastic', 'atr')


def _combo_values(ta: TA, typ: str, n: int) -> np.ndarray:
    if typ == 'rsi':
        return ta.rsi(n)
    if typ in ('sma', 'ema'):
        ma = ta.sma(n) if typ == 'sma' else ta.ema(n)
        with np.errstate(divide='ignore', invalid='ignore'):
            return (ta.c / ma - 1.0) * 100.0
    if typ == 'macd_hist':
        return ta.macd(max(1, n // 2), max(2, n), 9)['histogram']
    if typ == 'bbands_pctb':
        return ta.bbands(n, 2.0)['percent_b'] * 100.0
    if typ == 'stochastic':
        return ta.stoch(n, 3)['k']
    if typ == 'atr':
        return ta.atr(n)
    raise ValueError(f'custom combo indicator {typ!r} is not supported; choose one of {", ".join(COMBO_TYPES)}')


def _condition(v: np.ndarray, cond: str, th: float) -> np.ndarray:
    c = str(cond or 'above').lower()
    if c == 'above':
        return gt(v, th)
    if c == 'below':
        return lt(v, th)
    if c == 'cross_up':
        return cross_above(v, th)
    if c == 'cross_down':
        return cross_below(v, th)
    raise ValueError(f'custom combo condition {cond!r} is not one of above, below, cross_up, cross_down')


def _custom_combo(ta: TA, p) -> Signals:
    cfg = p.get('comboConfig') if isinstance(p.get('comboConfig'), dict) else None
    cfg = cfg or DEFAULT_COMBO
    rows = [r for r in (cfg.get('indicators') or []) if isinstance(r, dict)]
    if len(rows) < 1:
        raise ValueError('custom combo needs at least one indicator row in comboConfig.indicators')
    logic = str(cfg.get('logic') or 'and').lower()
    ents, exs = [], []
    for r in rows:
        typ = str(r.get('type') or 'rsi').lower()
        n = _pos('period', int(round(float(r.get('period') or 14))))
        v = _combo_values(ta, typ, n)
        ents.append(_condition(v, r.get('condition', 'below'), float(r.get('threshold', 30))))
        exs.append(_condition(v, r.get('exitCondition', 'above'), float(r.get('exitThreshold', 70))))
    ent = ents[0].copy()
    for e in ents[1:]:
        ent = (ent | e) if logic == 'or' else (ent & e)
    ex = exs[0].copy()
    for e in exs[1:]:
        ex = ex | e
    return ent, ex


def _buy_and_hold(ta: TA, p) -> Signals:
    ent = np.zeros(ta.n, dtype=bool)
    ex = np.zeros(ta.n, dtype=bool)
    if ta.n:
        ent[0] = True
    return ent, ex


BUILDERS: Dict[str, Callable[[TA, Dict[str, Any]], Signals]] = {
    'sma_crossover': _sma_crossover, 'ema_crossover': _ema_crossover, 'macd': _macd, 'macd_crossover': _macd,
    'adx_trend': _adx_trend, 'keltner_breakout': _keltner_breakout, 'triple_ma': _triple_ma,
    'mean_reversion': _mean_reversion, 'bollinger_bands': _bollinger_bands, 'rsi': _rsi, 'stochastic': _stochastic,
    'momentum': _momentum, 'dual_momentum': _dual_momentum, 'breakout': _breakout, 'donchian_breakout': _breakout,
    'volatility_breakout': _volatility_breakout, 'rsi_macd': _rsi_macd, 'macd_adx': _macd_adx,
    'bollinger_rsi': _bollinger_rsi, 'williams_r': _williams_r, 'cci': _cci, 'obv_trend': _obv_trend,
    'custom_combo': _custom_combo, 'buy_and_hold': _buy_and_hold,
}
ALLOCATION = ('rebalance_periodic',)
ALIASES = {'equal_weight': 'rebalance_periodic', 'rebalance': 'rebalance_periodic', 'buy_hold': 'buy_and_hold',
           'buyandhold': 'buy_and_hold', 'sma': 'sma_crossover', 'ema': 'ema_crossover'}


def resolve(strategy_type: Any) -> str:
    s = str(strategy_type or 'sma_crossover').strip().lower()
    s = ALIASES.get(s, s)
    if s not in BUILDERS and s not in ALLOCATION:
        raise ValueError(f'Unknown strategy {strategy_type!r}. Available: '
                         f'{", ".join(sorted(set(BUILDERS) | set(ALLOCATION)))}.')
    return s


def kind_of(strategy_id: str) -> str:
    return 'allocation' if strategy_id in ALLOCATION else 'signals'


def build_signals(strategy_id: str, ta: TA, params: Dict[str, Any]) -> Signals:
    e, x = BUILDERS[strategy_id](ta, params or {})
    return np.asarray(e, dtype=bool), np.asarray(x, dtype=bool)


# ── allocation ───────────────────────────────────────────────────────────────

_CAL = {'weekly': 'W', 'week': 'W', 'monthly': 'M', 'month': 'M', 'quarterly': 'Q', 'quarter': 'Q',
        'yearly': 'Y', 'annual': 'Y', 'annually': 'Y', 'year': 'Y'}


def rebalance_bars(dates, period: Any) -> np.ndarray:
    """Decision bars of a periodic rebalance: every `period` bars from the first bar, or the first
    bar of each calendar week / month / quarter / year for a named period."""
    import pandas as pd
    idx = pd.DatetimeIndex(dates)
    n = len(idx)
    flag = np.zeros(n, dtype=bool)
    if n == 0:
        return flag
    if isinstance(period, str) and period.strip().lower() in _CAL:
        per = idx.to_period(_CAL[period.strip().lower()])
        flag[0] = True
        flag[1:] = per[1:] != per[:-1]
        return flag
    try:
        k = int(round(float(period)))
    except (TypeError, ValueError):
        raise ValueError(f'rebalancePeriod must be a number of bars or one of weekly, monthly, quarterly, yearly '
                         f'(got {period!r})')
    if k < 1:
        raise ValueError('rebalancePeriod must be at least 1 bar')
    flag[::k] = True
    return flag


def default_warmup(strategy_id: str, params: Dict[str, Any]) -> int:
    """Bars the strategy's indicators need before its first possible signal (for provenance and
    walk-forward warm-up)."""
    p = params or {}
    look = [int(round(float(v))) for k, v in p.items() if isinstance(v, (int, float)) and not isinstance(v, bool)
            and any(w in k.lower() for w in ('period', 'lookback', 'window'))]
    base = max(look) if look else 0
    if strategy_id in ('macd', 'macd_crossover', 'rsi_macd', 'macd_adx'):
        base = _i(p, 'slowPeriod', 26) + _i(p, 'signalPeriod', 9)
    if strategy_id in ('adx_trend', 'macd_adx'):
        base = max(base, 2 * _i(p, 'adxPeriod', 14, 'period'))
    if strategy_id == 'custom_combo':
        cfg = p.get('comboConfig') if isinstance(p.get('comboConfig'), dict) else DEFAULT_COMBO
        base = max([int(r.get('period') or 14) for r in cfg.get('indicators') or [{}]] + [0])
    return int(base)


# ════════════════════════════════════════════════════════════════════════════
# Catalogue (get_strategies)
# ════════════════════════════════════════════════════════════════════════════

def _p(pid, name, default, lo, hi, step=1, typ='int', desc=None):
    d = {'id': pid, 'name': name, 'type': typ, 'default': default, 'min': lo, 'max': hi, 'step': step}
    if desc:
        d['description'] = desc
    return d


_FAST_SLOW = [{'lt': ['fastPeriod', 'slowPeriod'], 'message': 'The fast period must be shorter than the slow period.'}]

CATALOG: List[Dict[str, Any]] = [
    {'type': 'sma_crossover', 'name': 'SMA Crossover', 'category': 'Trend',
     'description': 'Fast SMA crosses above / below the slow SMA',
     'parameters': [_p('fastPeriod', 'Fast Period', 10, 2, 100), _p('slowPeriod', 'Slow Period', 20, 5, 200)],
     'constraints': _FAST_SLOW},
    {'type': 'ema_crossover', 'name': 'EMA Crossover', 'category': 'Trend',
     'description': 'Fast EMA crosses above / below the slow EMA',
     'parameters': [_p('fastPeriod', 'Fast Period', 10, 2, 100), _p('slowPeriod', 'Slow Period', 20, 5, 200)],
     'constraints': _FAST_SLOW},
    {'type': 'macd', 'name': 'MACD Crossover', 'category': 'Trend', 'description': 'MACD line crosses its signal line',
     'parameters': [_p('fastPeriod', 'Fast Period', 12, 2, 50), _p('slowPeriod', 'Slow Period', 26, 10, 100),
                    _p('signalPeriod', 'Signal Period', 9, 2, 30)], 'constraints': _FAST_SLOW},
    {'type': 'adx_trend', 'name': 'ADX Trend', 'category': 'Trend',
     'description': 'Long while ADX > threshold and +DI > -DI; out when -DI leads or ADX < 0.7 x threshold',
     'parameters': [_p('period', 'ADX Period', 14, 5, 50), _p('threshold', 'ADX Threshold', 25, 10, 50)]},
    {'type': 'keltner_breakout', 'name': 'Keltner Breakout', 'category': 'Trend',
     'description': 'Enter on a close above the upper Keltner band, exit below the middle (EMA)',
     'parameters': [_p('period', 'EMA Period', 20, 5, 100), _p('atrPeriod', 'ATR Period', 10, 5, 50),
                    _p('multiplier', 'Multiplier', 2.0, 0.5, 5.0, 0.1, 'float')]},
    {'type': 'triple_ma', 'name': 'Triple MA', 'category': 'Trend',
     'description': 'Long while EMA fast > medium > slow, exit when fast < medium',
     'parameters': [_p('fastPeriod', 'Fast', 5, 2, 20), _p('mediumPeriod', 'Medium', 20, 10, 50),
                    _p('slowPeriod', 'Slow', 50, 30, 200)],
     'constraints': [{'lt': ['fastPeriod', 'mediumPeriod'], 'message': 'Fast must be shorter than medium.'},
                     {'lt': ['mediumPeriod', 'slowPeriod'], 'message': 'Medium must be shorter than slow.'}]},
    {'type': 'mean_reversion', 'name': 'Z-Score Mean Reversion', 'category': 'Mean Reversion',
     'description': 'Enter when the z-score crosses below -Z, exit when it crosses above +Z/2',
     'parameters': [_p('period', 'Lookback', 20, 5, 100), _p('zThreshold', 'Z Threshold', 2.0, 0.5, 4.0, 0.1, 'float')]},
    {'type': 'bollinger_bands', 'name': 'Bollinger Bands', 'category': 'Mean Reversion',
     'description': 'Enter on a close below the lower band, exit on a close above the upper band',
     'parameters': [_p('period', 'Period', 20, 5, 100), _p('stdDev', 'Std Dev', 2.0, 0.5, 4.0, 0.1, 'float')]},
    {'type': 'rsi', 'name': 'RSI', 'category': 'Mean Reversion',
     'description': 'Wilder RSI crosses below oversold (enter) / above overbought (exit)',
     'parameters': [_p('period', 'Period', 14, 2, 50), _p('oversold', 'Oversold', 30, 10, 40),
                    _p('overbought', 'Overbought', 70, 60, 95)],
     'constraints': [{'lt': ['oversold', 'overbought'], 'message': 'Oversold must be below overbought.'}]},
    {'type': 'stochastic', 'name': 'Stochastic', 'category': 'Mean Reversion',
     'description': '%K crosses above %D below the oversold level (enter) / below %D above overbought (exit)',
     'parameters': [_p('kPeriod', '%K Period', 14, 5, 30), _p('dPeriod', '%D Period', 3, 2, 10),
                    _p('oversold', 'Oversold', 20, 5, 30), _p('overbought', 'Overbought', 80, 70, 95)]},
    {'type': 'momentum', 'name': 'Momentum', 'category': 'Momentum',
     'description': 'Long while the rate of change > threshold %, out while < -threshold %',
     'parameters': [_p('lookback', 'Lookback', 20, 5, 100), _p('threshold', 'Threshold %', 0.0, 0.0, 20.0, 0.5,
                                                                'float')]},
    {'type': 'dual_momentum', 'name': 'Dual Momentum', 'category': 'Momentum',
     'description': 'Long while short- and long-term momentum are both above the threshold %',
     'parameters': [_p('shortLookback', 'Short Lookback', 20, 5, 60), _p('longLookback', 'Long Lookback', 60, 30, 200),
                    _p('threshold', 'Threshold %', 0.0, 0.0, 10.0, 0.5, 'float')],
     'constraints': [{'lt': ['shortLookback', 'longLookback'], 'message': 'The short lookback must be shorter.'}]},
    {'type': 'breakout', 'name': 'Donchian Breakout', 'category': 'Breakout',
     'description': 'Enter on a close above the previous N-bar high, exit below the previous N-bar low',
     'parameters': [_p('period', 'Channel Period', 20, 5, 100)]},
    {'type': 'volatility_breakout', 'name': 'Volatility Breakout', 'category': 'Breakout',
     'description': 'Enter when the close moves K x ATR above the previous close, exit K x ATR below',
     'parameters': [_p('period', 'ATR Period', 20, 5, 50), _p('kMultiplier', 'K Multiplier', 1.5, 0.5, 4.0, 0.1,
                                                               'float')]},
    {'type': 'rsi_macd', 'name': 'RSI + MACD', 'category': 'Multi-Indicator',
     'description': 'MACD histogram turns positive after RSI was oversold within the lookback; exit on '
                    'overbought RSI or a negative histogram',
     'parameters': [_p('rsiPeriod', 'RSI Period', 14, 5, 30), _p('oversold', 'Oversold', 30, 15, 40),
                    _p('overbought', 'Overbought', 70, 60, 90), _p('fastPeriod', 'MACD Fast', 12, 5, 30),
                    _p('slowPeriod', 'MACD Slow', 26, 15, 50), _p('signalPeriod', 'MACD Signal', 9, 3, 20),
                    _p('lookback', 'Oversold Lookback', 5, 1, 30)], 'constraints': _FAST_SLOW},
    {'type': 'macd_adx', 'name': 'MACD + ADX', 'category': 'Multi-Indicator',
     'description': 'Long while MACD > signal and ADX > threshold; exit when MACD < signal',
     'parameters': [_p('fastPeriod', 'MACD Fast', 12, 5, 30), _p('slowPeriod', 'MACD Slow', 26, 15, 50),
                    _p('signalPeriod', 'MACD Signal', 9, 3, 20), _p('adxPeriod', 'ADX Period', 14, 5, 30),
                    _p('adxThreshold', 'ADX Threshold', 25, 15, 50)], 'constraints': _FAST_SLOW},
    {'type': 'bollinger_rsi', 'name': 'Bollinger + RSI', 'category': 'Multi-Indicator',
     'description': 'Close below the lower band with RSI oversold; exit above the upper band or RSI overbought',
     'parameters': [_p('bbPeriod', 'BB Period', 20, 5, 50), _p('stdDev', 'Std Dev', 2.0, 1.0, 4.0, 0.1, 'float'),
                    _p('rsiPeriod', 'RSI Period', 14, 5, 30), _p('oversold', 'Oversold', 30, 15, 40),
                    _p('overbought', 'Overbought', 70, 60, 90)]},
    {'type': 'williams_r', 'name': 'Williams %R', 'category': 'Mean Reversion',
     'description': 'Long while %R < oversold, out while %R > overbought',
     'parameters': [_p('period', 'Period', 14, 5, 50), _p('oversold', 'Oversold', -80, -95, -60),
                    _p('overbought', 'Overbought', -20, -40, -5)]},
    {'type': 'cci', 'name': 'CCI', 'category': 'Momentum',
     'description': 'Long while CCI < lower, out while CCI > upper',
     'parameters': [_p('period', 'Period', 20, 5, 50), _p('lower', 'Lower', -100, -200, -50),
                    _p('upper', 'Upper', 100, 50, 200)]},
    {'type': 'obv_trend', 'name': 'OBV Trend', 'category': 'Volume',
     'description': 'Long while on-balance volume is above its N-bar average',
     'parameters': [_p('period', 'OBV SMA Period', 20, 5, 100)]},
    {'type': 'custom_combo', 'name': 'Custom Combo', 'category': 'Multi-Indicator',
     'description': 'Combine 2-3 indicator conditions with AND / OR (default: RSI(14) < 35 and close above '
                    'SMA(200); exit RSI > 70 or close below SMA(200)). sma / ema rows compare the close\'s % '
                    'distance from the average with the threshold.', 'parameters': []},
    {'type': 'buy_and_hold', 'name': 'Buy & Hold', 'category': 'Portfolio',
     'description': 'Buy on the first bar and hold (several symbols: equal slots, never rebalanced)',
     'parameters': []},
    {'type': 'rebalance_periodic', 'name': 'Periodic Rebalance', 'category': 'Portfolio',
     'description': 'Hold every symbol at its target weight (equal, or the portfolio weights) and rebalance '
                    'every N bars', 'kind': 'allocation',
     'parameters': [_p('rebalancePeriod', 'Rebalance Every (bars)', 63, 1, 252)]},
]


def get_strategy_catalog() -> List[Dict[str, Any]]:
    out = []
    for s in CATALOG:
        d = dict(s)
        d['id'] = s['type']
        d.setdefault('kind', kind_of(s['type']))
        # every catalogue entry runs on this engine (no package or symbol-count gate); the screen reads
        # supported / runnable / reason uniformly across engines (reason is set when either is false)
        d['runnable'] = True
        d['supported'] = True
        d['reason'] = None
        d['parameters'] = [dict(p) for p in s['parameters']]
        out.append(d)
    return out
