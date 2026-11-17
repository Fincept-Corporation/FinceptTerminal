"""
Strategy catalogue of the "zipline" engine: 24 zipline-style algorithms (initialize / handle_data,
zl_api) - the 23 of v1, each now the rule its name says, plus buy_and_hold.

    init, handle = build('sma_crossover', {'fastPeriod': 20, 'slowPeriod': 50})
    decisions = zl_api.run_algorithm(init, handle, bars, params)

Every indicator is a full-history line from zl_api (Fincept indicator table, OHLC where the
indicator needs it - v1 fed CCI / Williams %R / Keltner / ATR / ADX closes only, audit Z10).
A rule states the target it wants on each bar (order_target_percent: 1 = long, 0 = flat; no order
while its inputs are undefined); the executor enters only when flat and exits only when holding,
which gives each rule its hysteresis. When an exit and an entry condition hold on the same bar the
exit wins (base.simulator's convention).

SIGNAL ALGORITHMS (long / flat per symbol; base.simulator signals mode)
  sma_crossover       long while SMA(fast) > SMA(slow)                       (state rule)
  ema_crossover       long while EMA(fast) > EMA(slow)  (EMAs SMA-seeded)     (state rule)
  macd                long while the MACD line > its signal line            (state rule)
  macd_zero_cross     enter when the MACD line crosses above 0, exit when it crosses below 0 (the
                      MACD line is EMA(fast) - EMA(slow), so this is that EMA pair's cross; v1's
                      "histogram zero cross" was the macd rule again)
  adx_trend           long while Wilder ADX > threshold and the close > its SMA(adxPeriod)
  triple_ma           enter when fast > medium > slow, exit when fast < medium
  keltner_channel     enter on a close above EMA(period) + k x ATR(period), exit below the EMA
  bollinger_bands     enter on a close below the lower band, exit above the UPPER band (bands =
                      SMA +- k population std, Bollinger's definition)
  mean_reversion      z-score of the close (sample std): enter z < -threshold, exit z >= exitZ
                      (default 0 = back at the mean). v1 exited at +threshold with the same bands
                      as bollinger_bands, so the two returned identical trades (Z17).
  rsi                 Wilder RSI: enter below oversold, exit above overbought  (level rule)
  stochastic          %D (SMA of %K) below oversold -> enter, above overbought -> exit
  williams_r          %R below oversold (default -80) -> enter, above overbought (-20) -> exit
  cci                 CCI below -threshold -> enter, above +threshold -> exit
  momentum            ROC over N bars above +threshold (fraction: 0.02 = 2 %) -> enter, below
                      -threshold -> exit
  breakout            close above the PREVIOUS N-bar high -> enter, below the previous N-bar low ->
                      exit (Donchian)
  volatility_breakout close above SMA(N) + k x ATR(N) -> enter, below SMA(N) -> exit
  rsi_macd            enter when RSI < rsiLower while the MACD line > 0 (a dip in an uptrend);
                      exit when RSI > rsiUpper or the MACD line < 0 (v1 hard-coded 40 / 60)
  macd_adx            long while the MACD line > 0 and ADX > adxThreshold (v1 hard-coded 25)
  bollinger_rsi       enter on a close below the lower band with RSI < rsiOversold; exit on a close
                      above the middle band or RSI > rsiOverbought
  obv_trend           long while OBV > its N-bar SMA
  scheduled_rebalance schedule_function(month_start): on the first bar of each month, long if the
                      return over momentumWindow bars is positive, else flat (per symbol)
  buy_and_hold        zipline's tutorial algorithm: order_target_percent(asset, 1) once, on the
                      symbol's first tradable bar
ALLOCATION ALGORITHMS (target weights; base.simulator weights mode; trades = position episodes)
  pipeline            a Pipeline-style factor screen: every rebalanceDays bars rank the symbols by
                      their lookback-bar return, keep the topN with a positive return, equal weight
                      (cash when none qualifies). v1 used one symbol and never ranked (Z17); with one
                      symbol the ranking is moot and only the screen applies (stated in a warning).
  dual_momentum       Antonacci: on each rebalance (monthly by default) pick the topN symbols by
                      return over relativePeriod bars (relative momentum) and hold those whose return
                      over absolutePeriod bars is > 0 (absolute momentum), equal weight, else cash.
                      Lookbacks in BARS (v1 compared a 12-BAR return with itself: a 12-day ROC rule).
                      With one symbol only absolute momentum applies (stated in a warning).
"""

from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Tuple

from zl_api import date_rules

Builder = Callable[[Dict[str, Any]], Tuple[Optional[Callable], Optional[Callable]]]


# ════════════════════════════════════════════════════════════════════════════
# Parameter helpers
# ════════════════════════════════════════════════════════════════════════════

def _i(params, key, default) -> int:
    v = params.get(key, default)
    if v is None:
        v = default
    try:
        out = int(round(float(v)))
    except (TypeError, ValueError):
        raise ValueError(f'parameter {key} must be a number, not {v!r}')
    if out < 1:
        raise ValueError(f'parameter {key} must be at least 1 (got {v})')
    return out


def _f(params, key, default) -> float:
    v = params.get(key, default)
    if v is None:
        v = default
    try:
        return float(v)
    except (TypeError, ValueError):
        raise ValueError(f'parameter {key} must be a number, not {v!r}')


def _p(pid, name, default, lo, hi, step=1, typ='int', desc=None, choices=None) -> Dict[str, Any]:
    d = {'id': pid, 'name': name, 'label': name, 'type': typ, 'default': default, 'min': lo, 'max': hi, 'step': step}
    if choices is not None:
        d['choices'] = list(choices)
    if desc:
        d['description'] = desc
    return d


def _lt(a, b, msg) -> Dict[str, Any]:
    return {'lt': [a, b], 'message': msg}


_FS = [_lt('fastPeriod', 'slowPeriod', 'The fast period must be shorter than the slow period.')]
_MACD_FS = [_lt('macdFast', 'macdSlow', 'The MACD fast period must be shorter than the slow period.')]
_REBAL = _p('rebalancePeriod', 'Rebalance', 'monthly', None, None, None, 'choice',
            'First bar of each period (schedule_function date rule); a number = every N bars',
            ('daily', 'weekly', 'monthly', 'quarterly', 'yearly'))

STRATEGIES: Dict[str, Dict[str, Any]] = {}


def _algo(sid: str, name: str, category: str, description: str, params: List[Dict[str, Any]], *,
          kind: str = 'signals', constraints: Optional[List[Dict[str, Any]]] = None):
    def deco(fn: Builder) -> Builder:
        STRATEGIES[sid] = {'type': sid, 'name': name, 'category': category, 'description': description,
                           'parameters': params, 'kind': kind, 'constraints': list(constraints or []), 'build': fn}
        return fn
    return deco


def _long_flat(entry: bool, exit_: bool, context, asset) -> None:
    """Exit wins when both hold (base.simulator's convention); nothing when neither."""
    if exit_:
        context.order_target_percent(asset, 0.0)
    elif entry:
        context.order_target_percent(asset, 1.0)


# ════════════════════════════════════════════════════════════════════════════
# Trend
# ════════════════════════════════════════════════════════════════════════════

@_algo('sma_crossover', 'SMA Crossover', 'trend', 'Long while the fast SMA is above the slow SMA (state rule)',
       [_p('fastPeriod', 'Fast Period', 10, 2, 200), _p('slowPeriod', 'Slow Period', 20, 5, 400)], constraints=_FS)
def _sma_crossover(p):
    fast, slow = _i(p, 'fastPeriod', 10), _i(p, 'slowPeriod', 20)

    def initialize(context):
        context.fast = context.indicator('sma', fast)
        context.slow = context.indicator('sma', slow)

    def handle_data(context, data):
        for a in context.assets:
            f, s = data.current(a, context.fast), data.current(a, context.slow)
            if f == f and s == s:
                context.order_target_percent(a, 1.0 if f > s else 0.0)
    return initialize, handle_data


@_algo('ema_crossover', 'EMA Crossover', 'trend',
       'Long while the fast EMA is above the slow EMA (EMAs seeded with the SMA of their first bars)',
       [_p('fastPeriod', 'Fast Period', 10, 2, 200), _p('slowPeriod', 'Slow Period', 20, 5, 400)], constraints=_FS)
def _ema_crossover(p):
    fast, slow = _i(p, 'fastPeriod', 10), _i(p, 'slowPeriod', 20)

    def initialize(context):
        context.fast = context.indicator('ema', fast)
        context.slow = context.indicator('ema', slow)

    def handle_data(context, data):
        for a in context.assets:
            f, s = data.current(a, context.fast), data.current(a, context.slow)
            if f == f and s == s:
                context.order_target_percent(a, 1.0 if f > s else 0.0)
    return initialize, handle_data


@_algo('macd', 'MACD Crossover', 'trend', 'Long while the MACD line is above its signal line',
       [_p('fastPeriod', 'Fast Period', 12, 2, 100), _p('slowPeriod', 'Slow Period', 26, 5, 200),
        _p('signalPeriod', 'Signal Period', 9, 2, 50)], constraints=_FS)
def _macd(p):
    f, s, g = _i(p, 'fastPeriod', 12), _i(p, 'slowPeriod', 26), _i(p, 'signalPeriod', 9)

    def initialize(context):
        context.line = context.indicator('macd', f, s, g, output='macd')
        context.signal = context.indicator('macd', f, s, g, output='signal')

    def handle_data(context, data):
        for a in context.assets:
            m, sg = data.current(a, context.line), data.current(a, context.signal)
            if m == m and sg == sg:
                context.order_target_percent(a, 1.0 if m > sg else 0.0)
    return initialize, handle_data


@_algo('adx_trend', 'ADX Trend Filter', 'trend',
       'Long while Wilder ADX (high / low / close) is above the threshold and the close is above its SMA over the '
       'ADX period',
       [_p('adxPeriod', 'ADX Period', 14, 2, 100), _p('adxThreshold', 'ADX Threshold', 25.0, 5.0, 60.0, 1.0, 'float')])
def _adx_trend(p):
    n, th = _i(p, 'adxPeriod', 14), _f(p, 'adxThreshold', 25.0)

    def initialize(context):
        context.adx = context.indicator('adx', n, output='adx')
        context.ma = context.indicator('sma', n)

    def handle_data(context, data):
        for a in context.assets:
            x, m = data.current(a, context.adx), data.current(a, context.ma)
            if x == x and m == m:
                context.order_target_percent(a, 1.0 if (x > th and data.current(a, 'close') > m) else 0.0)
    return initialize, handle_data


@_algo('triple_ma', 'Triple MA', 'trend', 'Enter when fast SMA > medium SMA > slow SMA, exit when fast < medium',
       [_p('fastPeriod', 'Fast Period', 10, 2, 200), _p('mediumPeriod', 'Medium Period', 20, 3, 300),
        _p('slowPeriod', 'Slow Period', 50, 5, 400)],
       constraints=[_lt('fastPeriod', 'mediumPeriod', 'The fast period must be shorter than the medium period.'),
                    _lt('mediumPeriod', 'slowPeriod', 'The medium period must be shorter than the slow period.')])
def _triple_ma(p):
    f, m, s = _i(p, 'fastPeriod', 10), _i(p, 'mediumPeriod', 20), _i(p, 'slowPeriod', 50)

    def initialize(context):
        context.f = context.indicator('sma', f)
        context.m = context.indicator('sma', m)
        context.s = context.indicator('sma', s)

    def handle_data(context, data):
        for a in context.assets:
            x, y, z = data.current(a, context.f), data.current(a, context.m), data.current(a, context.s)
            if x == x and y == y and z == z:
                _long_flat(x > y > z, x < y, context, a)
    return initialize, handle_data


@_algo('keltner_channel', 'Keltner Channel', 'trend',
       'Enter on a close above EMA(period) + k x ATR(period) (Wilder ATR on high / low / close), exit on a close '
       'below the EMA',
       [_p('period', 'Period', 20, 2, 200), _p('atrMultiplier', 'ATR Multiplier', 2.0, 0.5, 5.0, 0.1, 'float')])
def _keltner(p):
    n, k = _i(p, 'period', 20), _f(p, 'atrMultiplier', 2.0)

    def initialize(context):
        context.up = context.indicator('keltner', n, n, k, output='upper')
        context.mid = context.indicator('keltner', n, n, k, output='middle')

    def handle_data(context, data):
        for a in context.assets:
            u, m = data.current(a, context.up), data.current(a, context.mid)
            if u == u and m == m:
                c = data.current(a, 'close')
                _long_flat(c > u, c < m, context, a)
    return initialize, handle_data


# ════════════════════════════════════════════════════════════════════════════
# Mean reversion
# ════════════════════════════════════════════════════════════════════════════

@_algo('bollinger_bands', 'Bollinger Bands', 'meanReversion',
       'Enter on a close below the lower band, exit on a close above the upper band (SMA +- k population std)',
       [_p('period', 'Period', 20, 2, 400), _p('stdDev', 'Std Dev', 2.0, 0.5, 5.0, 0.1, 'float')])
def _bollinger(p):
    n, k = _i(p, 'period', 20), _f(p, 'stdDev', 2.0)

    def initialize(context):
        context.lo = context.indicator('bbands', n, k, output='lower')
        context.up = context.indicator('bbands', n, k, output='upper')

    def handle_data(context, data):
        for a in context.assets:
            lo, up = data.current(a, context.lo), data.current(a, context.up)
            if lo == lo and up == up:
                c = data.current(a, 'close')
                _long_flat(c < lo, c > up, context, a)
    return initialize, handle_data


@_algo('rsi', 'RSI Mean Reversion', 'meanReversion', 'Wilder RSI: enter below oversold, exit above overbought',
       [_p('period', 'Period', 14, 2, 100), _p('oversold', 'Oversold', 30.0, 1.0, 50.0, 1.0, 'float'),
        _p('overbought', 'Overbought', 70.0, 50.0, 99.0, 1.0, 'float')],
       constraints=[_lt('oversold', 'overbought', 'Oversold must be below overbought.')])
def _rsi(p):
    n, lo, hi = _i(p, 'period', 14), _f(p, 'oversold', 30.0), _f(p, 'overbought', 70.0)

    def initialize(context):
        context.rsi = context.indicator('rsi', n)

    def handle_data(context, data):
        for a in context.assets:
            r = data.current(a, context.rsi)
            if r == r:
                _long_flat(r < lo, r > hi, context, a)
    return initialize, handle_data


@_algo('mean_reversion', 'Z-Score Reversion', 'meanReversion',
       'Z-score of the close vs its N-bar mean (sample std): enter below -threshold, exit when it is back at exitZ '
       '(0 = the mean)',
       [_p('window', 'Window', 20, 3, 400), _p('threshold', 'Z Threshold', 2.0, 0.1, 5.0, 0.1, 'float'),
        _p('exitZ', 'Exit Z', 0.0, -3.0, 3.0, 0.1, 'float')])
def _zscore(p):
    n, th, ex = _i(p, 'window', 20), _f(p, 'threshold', 2.0), _f(p, 'exitZ', 0.0)

    def initialize(context):
        context.z = context.indicator('zscore_sample', n)

    def handle_data(context, data):
        for a in context.assets:
            z = data.current(a, context.z)
            if z == z:
                _long_flat(z < -th, z >= ex, context, a)
    return initialize, handle_data


@_algo('stochastic', 'Stochastic', 'meanReversion',
       '%D (SMA of %K over high / low) below oversold -> enter, above overbought -> exit',
       [_p('kPeriod', '%K Period', 14, 2, 100), _p('dPeriod', '%D Period', 3, 1, 50),
        _p('oversold', 'Oversold', 20.0, 1.0, 50.0, 1.0, 'float'),
        _p('overbought', 'Overbought', 80.0, 50.0, 99.0, 1.0, 'float')],
       constraints=[_lt('oversold', 'overbought', 'Oversold must be below overbought.')])
def _stochastic(p):
    k, d, lo, hi = _i(p, 'kPeriod', 14), _i(p, 'dPeriod', 3), _f(p, 'oversold', 20.0), _f(p, 'overbought', 80.0)

    def initialize(context):
        context.d = context.indicator('stochastic', k, d, output='d')

    def handle_data(context, data):
        for a in context.assets:
            v = data.current(a, context.d)
            if v == v:
                _long_flat(v < lo, v > hi, context, a)
    return initialize, handle_data


@_algo('williams_r', 'Williams %R', 'meanReversion',
       'Williams %R (-100..0, high / low): enter below oversold, exit above overbought',
       [_p('period', 'Period', 14, 2, 100), _p('oversold', 'Oversold', -80.0, -100.0, 0.0, 1.0, 'float'),
        _p('overbought', 'Overbought', -20.0, -100.0, 0.0, 1.0, 'float')],
       constraints=[_lt('oversold', 'overbought', 'Oversold must be below overbought.')])
def _williams(p):
    n, lo, hi = _i(p, 'period', 14), _f(p, 'oversold', -80.0), _f(p, 'overbought', -20.0)

    def initialize(context):
        context.wr = context.indicator('williams_r', n)

    def handle_data(context, data):
        for a in context.assets:
            v = data.current(a, context.wr)
            if v == v:
                _long_flat(v < lo, v > hi, context, a)
    return initialize, handle_data


@_algo('cci', 'CCI', 'meanReversion',
       'Commodity Channel Index (typical price of high / low / close): enter below -threshold, exit above +threshold',
       [_p('period', 'Period', 20, 2, 200), _p('threshold', 'Threshold', 100.0, 10.0, 300.0, 5.0, 'float')])
def _cci(p):
    n, th = _i(p, 'period', 20), _f(p, 'threshold', 100.0)

    def initialize(context):
        context.cci = context.indicator('cci', n)

    def handle_data(context, data):
        for a in context.assets:
            v = data.current(a, context.cci)
            if v == v:
                _long_flat(v < -th, v > th, context, a)
    return initialize, handle_data


# ════════════════════════════════════════════════════════════════════════════
# Momentum
# ════════════════════════════════════════════════════════════════════════════

@_algo('momentum', 'Momentum (ROC)', 'momentum',
       'Rate of change over N bars: enter above +threshold, exit below -threshold (threshold is a fraction: 0.02 = 2 %)',
       [_p('period', 'Period (bars)', 12, 1, 400),
        _p('threshold', 'Threshold (fraction)', 0.02, 0.0, 0.5, 0.005, 'float', '0.02 = 2 %')])
def _momentum(p):
    n, th = _i(p, 'period', 12), _f(p, 'threshold', 0.02)

    def initialize(context):
        context.roc = context.indicator('momentum', n)

    def handle_data(context, data):
        for a in context.assets:
            v = data.current(a, context.roc)
            if v == v:
                _long_flat(v > th, v < -th, context, a)
    return initialize, handle_data


@_algo('dual_momentum', 'Dual Momentum', 'momentum',
       'Antonacci dual momentum: on each rebalance pick the top N symbols by return over the relative period, hold '
       'those whose return over the absolute period is positive (equal weight), else cash. Lookbacks in bars; with '
       'one symbol only absolute momentum applies',
       [_p('relativePeriod', 'Relative Period (bars)', 252, 5, 504),
        _p('absolutePeriod', 'Absolute Period (bars)', 252, 5, 504), _p('topN', 'Top N', 1, 1, 50), _REBAL],
       kind='allocation')
def _dual_momentum(p):
    rel_n, abs_n, top = _i(p, 'relativePeriod', 252), _i(p, 'absolutePeriod', 252), _i(p, 'topN', 1)
    rule = date_rules.from_period(p.get('rebalancePeriod', 'monthly'))

    def initialize(context):
        context.rel = context.indicator('momentum', rel_n)
        context.abs = context.indicator('momentum', abs_n)
        context.schedule_function(rebalance, rule)

    def rebalance(context, data):
        ranked = []
        for a in context.assets:
            r, b = data.current(a, context.rel), data.current(a, context.abs)
            if r == r and b == b and data.can_trade(a):
                ranked.append((r, b, a))
        if not ranked:
            return
        ranked.sort(key=lambda x: -x[0])
        hold = [a for r, b, a in ranked[:top] if b > 0]
        w = 1.0 / len(hold) if hold else 0.0
        for r, b, a in ranked:
            context.order_target_percent(a, w if a in hold else 0.0)
    return initialize, None


@_algo('macd_zero_cross', 'MACD Zero Cross', 'momentum',
       'Enter when the MACD line crosses above zero, exit when it crosses below (the MACD line is EMA(fast) - '
       'EMA(slow), so this is the cross of those two EMAs)',
       [_p('fastPeriod', 'Fast Period', 12, 2, 100), _p('slowPeriod', 'Slow Period', 26, 5, 200)], constraints=_FS)
def _macd_zero(p):
    f, s = _i(p, 'fastPeriod', 12), _i(p, 'slowPeriod', 26)

    def initialize(context):
        context.line = context.indicator('macd', f, s, 9, output='macd')
        context.prev = context.indicator('macd', f, s, 9, output='macd', lag=1)

    def handle_data(context, data):
        for a in context.assets:
            m, q = data.current(a, context.line), data.current(a, context.prev)
            if m == m and q == q:
                _long_flat(q <= 0.0 < m, q >= 0.0 > m, context, a)
    return initialize, handle_data


# ════════════════════════════════════════════════════════════════════════════
# Breakout
# ════════════════════════════════════════════════════════════════════════════

@_algo('breakout', 'Donchian Breakout', 'breakout',
       'Enter on a close above the PREVIOUS N-bar high, exit on a close below the previous N-bar low',
       [_p('period', 'Channel Period', 20, 2, 400)])
def _breakout(p):
    n = _i(p, 'period', 20)

    def initialize(context):
        context.up = context.indicator('donchian', n, output='upper', lag=1)
        context.lo = context.indicator('donchian', n, output='lower', lag=1)

    def handle_data(context, data):
        for a in context.assets:
            u, lo = data.current(a, context.up), data.current(a, context.lo)
            if u == u and lo == lo:
                c = data.current(a, 'close')
                _long_flat(c > u, c < lo, context, a)
    return initialize, handle_data


@_algo('volatility_breakout', 'Volatility Breakout', 'breakout',
       'Enter on a close above SMA(N) + k x ATR(N) (Wilder ATR on high / low / close), exit on a close below SMA(N)',
       [_p('atrPeriod', 'ATR Period', 14, 2, 200), _p('atrMultiplier', 'ATR Multiplier', 2.0, 0.5, 5.0, 0.1, 'float')])
def _vol_breakout(p):
    n, k = _i(p, 'atrPeriod', 14), _f(p, 'atrMultiplier', 2.0)

    def initialize(context):
        context.ma = context.indicator('sma', n)
        context.atr = context.indicator('atr', n)

    def handle_data(context, data):
        for a in context.assets:
            m, r = data.current(a, context.ma), data.current(a, context.atr)
            if m == m and r == r:
                c = data.current(a, 'close')
                _long_flat(c > m + k * r, c < m, context, a)
    return initialize, handle_data


# ════════════════════════════════════════════════════════════════════════════
# Multi-indicator
# ════════════════════════════════════════════════════════════════════════════

@_algo('rsi_macd', 'RSI + MACD', 'multiIndicator',
       'Enter when RSI is below the lower level while the MACD line is above 0 (a dip in an uptrend); exit when RSI '
       'is above the upper level or the MACD line falls below 0',
       [_p('rsiPeriod', 'RSI Period', 14, 2, 100), _p('macdFast', 'MACD Fast', 12, 2, 100),
        _p('macdSlow', 'MACD Slow', 26, 5, 200), _p('rsiLower', 'Buy below RSI', 40.0, 1.0, 60.0, 1.0, 'float'),
        _p('rsiUpper', 'Sell above RSI', 60.0, 40.0, 99.0, 1.0, 'float')],
       constraints=_MACD_FS + [_lt('rsiLower', 'rsiUpper', 'The buy level must be below the sell level.')])
def _rsi_macd(p):
    rn, f, s = _i(p, 'rsiPeriod', 14), _i(p, 'macdFast', 12), _i(p, 'macdSlow', 26)
    lo, hi = _f(p, 'rsiLower', 40.0), _f(p, 'rsiUpper', 60.0)

    def initialize(context):
        context.rsi = context.indicator('rsi', rn)
        context.line = context.indicator('macd', f, s, 9, output='macd')

    def handle_data(context, data):
        for a in context.assets:
            r, m = data.current(a, context.rsi), data.current(a, context.line)
            if r == r and m == m:
                _long_flat(r < lo and m > 0.0, r > hi or m < 0.0, context, a)
    return initialize, handle_data


@_algo('macd_adx', 'MACD + ADX Filter', 'multiIndicator',
       'Long while the MACD line is above 0 and Wilder ADX is above the threshold (a trend strong enough to follow)',
       [_p('macdFast', 'MACD Fast', 12, 2, 100), _p('macdSlow', 'MACD Slow', 26, 5, 200),
        _p('adxPeriod', 'ADX Period', 14, 2, 100), _p('adxThreshold', 'ADX Threshold', 25.0, 5.0, 60.0, 1.0, 'float')],
       constraints=_MACD_FS)
def _macd_adx(p):
    f, s, n, th = _i(p, 'macdFast', 12), _i(p, 'macdSlow', 26), _i(p, 'adxPeriod', 14), _f(p, 'adxThreshold', 25.0)

    def initialize(context):
        context.line = context.indicator('macd', f, s, 9, output='macd')
        context.adx = context.indicator('adx', n, output='adx')

    def handle_data(context, data):
        for a in context.assets:
            m, x = data.current(a, context.line), data.current(a, context.adx)
            if m == m and x == x:
                context.order_target_percent(a, 1.0 if (m > 0.0 and x > th) else 0.0)
    return initialize, handle_data


@_algo('bollinger_rsi', 'Bollinger + RSI', 'multiIndicator',
       'Enter on a close below the lower Bollinger band with RSI below oversold; exit on a close above the middle band '
       'or RSI above overbought',
       [_p('bbPeriod', 'BB Period', 20, 2, 400), _p('stdDev', 'Std Dev', 2.0, 0.5, 5.0, 0.1, 'float'),
        _p('rsiPeriod', 'RSI Period', 14, 2, 100), _p('rsiOversold', 'RSI Oversold', 30.0, 1.0, 50.0, 1.0, 'float'),
        _p('rsiOverbought', 'RSI Overbought', 70.0, 50.0, 99.0, 1.0, 'float')],
       constraints=[_lt('rsiOversold', 'rsiOverbought', 'Oversold must be below overbought.')])
def _bollinger_rsi(p):
    n, k, rn = _i(p, 'bbPeriod', 20), _f(p, 'stdDev', 2.0), _i(p, 'rsiPeriod', 14)
    lo, hi = _f(p, 'rsiOversold', 30.0), _f(p, 'rsiOverbought', 70.0)

    def initialize(context):
        context.lower = context.indicator('bbands', n, k, output='lower')
        context.mid = context.indicator('bbands', n, k, output='middle')
        context.rsi = context.indicator('rsi', rn)

    def handle_data(context, data):
        for a in context.assets:
            b, m, r = data.current(a, context.lower), data.current(a, context.mid), data.current(a, context.rsi)
            if b == b and m == m and r == r:
                c = data.current(a, 'close')
                _long_flat(c < b and r < lo, c > m or r > hi, context, a)
    return initialize, handle_data


# ════════════════════════════════════════════════════════════════════════════
# Volume / scheduled / pipeline
# ════════════════════════════════════════════════════════════════════════════

@_algo('obv_trend', 'OBV Trend', 'other', 'Long while on-balance volume is above its N-bar SMA',
       [_p('maPeriod', 'MA Period', 20, 2, 400)])
def _obv_trend(p):
    n = _i(p, 'maPeriod', 20)

    def initialize(context):
        context.obv = context.indicator('obv')
        context.ma = context.indicator('obv_sma', n)

    def handle_data(context, data):
        for a in context.assets:
            o, m = data.current(a, context.obv), data.current(a, context.ma)
            if o == o and m == m:
                _long_flat(o > m, o < m, context, a)
    return initialize, handle_data


@_algo('scheduled_rebalance', 'Scheduled Rebalance', 'pipeline',
       'schedule_function(month_start): on the first bar of each month hold each symbol whose return over the momentum '
       'window is positive, else stay flat',
       [_p('momentumWindow', 'Momentum Window (bars)', 63, 2, 504)])
def _scheduled(p):
    n = _i(p, 'momentumWindow', 63)

    def initialize(context):
        context.mom = context.indicator('momentum', n)
        context.schedule_function(rebalance, date_rules.month_start())

    def rebalance(context, data):
        for a in context.assets:
            v = data.current(a, context.mom)
            if v == v:
                context.order_target_percent(a, 1.0 if v > 0.0 else 0.0)
    return initialize, None


@_algo('pipeline', 'Custom Pipeline', 'pipeline',
       'Pipeline-style factor screen: every N bars rank the symbols by their lookback return, keep the top N with a '
       'positive return, equal weight (cash when none qualifies). Needs 2+ symbols to rank',
       [_p('topN', 'Top N', 3, 1, 50, desc='v1 default 5 selected every symbol of a typical 1-5 symbol run'),
        _p('rebalanceDays', 'Rebalance Every (bars)', 21, 1, 252),
        _p('lookback', 'Factor Lookback (bars)', 21, 2, 504)], kind='allocation')
def _pipeline(p):
    top, every, look = _i(p, 'topN', 3), _i(p, 'rebalanceDays', 21), _i(p, 'lookback', 21)

    def initialize(context):
        context.factor = context.indicator('momentum', look)
        context.schedule_function(rebalance, date_rules.every_n_bars(every))

    def rebalance(context, data):
        scores = []
        for a in context.assets:
            v = data.current(a, context.factor)
            if v == v and data.can_trade(a):
                scores.append((v, a))
        if not scores:
            return
        scores.sort(key=lambda x: -x[0])
        longs = [a for v, a in scores[:top] if v > 0.0]
        w = 1.0 / len(longs) if longs else 0.0
        for v, a in scores:
            context.order_target_percent(a, w if a in longs else 0.0)
    return initialize, None


@_algo('buy_and_hold', 'Buy & Hold', 'benchmark',
       'zipline\'s tutorial algorithm: order_target_percent(asset, 1) once on each symbol\'s first tradable bar and '
       'hold (with several symbols each gets an equal slot)', [])
def _buy_and_hold(p):
    def initialize(context):
        context.ordered = set()

    def handle_data(context, data):
        if len(context.ordered) == len(context.assets):
            return
        for a in context.assets:
            if a.sid not in context.ordered and data.can_trade(a):
                context.order_target_percent(a, 1.0)
                context.ordered.add(a.sid)
    return initialize, handle_data


# ════════════════════════════════════════════════════════════════════════════
# Catalogue access
# ════════════════════════════════════════════════════════════════════════════

ALIASES = {'sma': 'sma_crossover', 'ema': 'ema_crossover', 'macd_crossover': 'macd', 'bollinger': 'bollinger_bands',
           'zscore': 'mean_reversion', 'z_score': 'mean_reversion', 'donchian_breakout': 'breakout',
           'buy_hold': 'buy_and_hold', 'buyandhold': 'buy_and_hold', 'keltner': 'keltner_channel',
           'williams': 'williams_r', 'roc': 'momentum'}
# a ranking across symbols needs >= 2 symbols to rank; with one symbol it still runs (stated in a warning)
RANKS_SYMBOLS = ('pipeline', 'dual_momentum')


def resolve(strategy_type: Any) -> str:
    s = str(strategy_type or 'sma_crossover').strip().lower()
    s = ALIASES.get(s, s)
    if s not in STRATEGIES:
        raise ValueError(f'Unknown strategy {strategy_type!r}. Available: {", ".join(sorted(STRATEGIES))}.')
    return s


def kind_of(sid: str) -> str:
    return STRATEGIES[sid]['kind']


def catalog_entry(sid: str) -> Optional[Dict[str, Any]]:
    return STRATEGIES.get(sid)


def defaults(sid: str) -> Dict[str, Any]:
    return {p['id']: p['default'] for p in STRATEGIES[sid]['parameters']}


def build(sid: str, params: Dict[str, Any]):
    """(initialize, handle_data) of a catalogue algorithm (ValueError for a bad parameter)."""
    return STRATEGIES[sid]['build'](params or {})


def check_constraints(sid: str, params: Dict[str, Any]) -> Optional[str]:
    """The message of the first catalogue constraint the parameters break, else None."""
    for c in STRATEGIES[sid]['constraints']:
        a, b = c['lt']
        try:
            if not float(params[a]) < float(params[b]):
                return f"{c['message']} ({a} {params[a]} / {b} {params[b]})"
        except (KeyError, TypeError, ValueError):
            continue
    return None


def default_warmup(sid: str, params: Dict[str, Any]) -> int:
    """Bars an algorithm's indicators need before its first possible decision (provenance, walk-forward
    warm-up)."""
    p = params or {}

    def g(k, d):
        try:
            return int(round(float(p.get(k, d))))
        except (TypeError, ValueError):
            return int(d)
    if sid == 'macd':
        return g('slowPeriod', 26) + g('signalPeriod', 9)
    if sid in ('adx_trend',):
        return 2 * g('adxPeriod', 14)
    if sid == 'macd_adx':
        return max(g('macdSlow', 26), 2 * g('adxPeriod', 14))
    if sid == 'stochastic':
        return g('kPeriod', 14) + g('dPeriod', 3)
    if sid == 'breakout':
        return g('period', 20) + 1
    if sid == 'macd_zero_cross':
        return g('slowPeriod', 26) + 1
    look = [int(round(float(v))) for k, v in p.items() if isinstance(v, (int, float)) and not isinstance(v, bool)
            and any(w in k.lower() for w in ('period', 'lookback', 'window', 'slow')) and float(v) >= 1]
    return int(max(look)) if look else 0


_IMPL = {'signals': 'zipline-style algorithm (initialize / handle_data on full-history indicators) decides; '
                    'base.simulator executes (signals mode) - zipline-reloaded is not used',
         'allocation': 'zipline-style algorithm (schedule_function + order_target_percent weights) decides; '
                       'base.simulator executes (weights mode) - zipline-reloaded is not used'}


def get_strategy_catalog() -> List[Dict[str, Any]]:
    out = []
    for sid, s in STRATEGIES.items():
        d = {k: v for k, v in s.items() if k != 'build'}
        d['id'] = sid
        d['implementation'] = _IMPL[s['kind']]
        d['minSymbols'] = 1
        d['parameters'] = [dict(x) for x in s['parameters']]
        d['params'] = d['parameters']
        d['constraints'] = [dict(c) for c in s['constraints']]
        if sid in RANKS_SYMBOLS:
            d['ranksSymbols'] = True
        out.append(d)
    return out
