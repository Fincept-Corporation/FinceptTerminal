"""
Zipline Strategy Implementations

Each strategy is a pair of (initialize, handle_data) callables that follow
Zipline's event-driven API. Strategies accept a `params` dict and return
the two functions.

Also provides standalone indicator calculation functions used by
calculate_indicator() in zipline_provider.py.

Usage:
    init_fn, handle_fn = get_strategy('sma_crossover', {'fastPeriod': 10, 'slowPeriod': 20})
    results = zipline.run_algorithm(..., initialize=init_fn, handle_data=handle_fn)
"""

import sys
import numpy as np
from typing import Dict, Any, Callable, Tuple, List, Optional


# ============================================================================
# Strategy registry
# ============================================================================

_STRATEGY_REGISTRY: Dict[str, Dict[str, Any]] = {}


def _register(strategy_id: str, category: str, name: str, description: str,
              params_spec: List[Dict[str, Any]]):
    """Decorator factory to register a strategy builder function."""
    def decorator(fn):
        _STRATEGY_REGISTRY[strategy_id] = {
            'id': strategy_id,
            'name': name,
            'category': category,
            'description': description,
            'params': params_spec,
            'builder': fn,
        }
        return fn
    return decorator


def get_strategy(strategy_type: str, params: Dict[str, Any]) -> Tuple[Callable, Callable]:
    """
    Look up a strategy by type and return (initialize_fn, handle_data_fn).

    The returned functions use Zipline's API:
      - initialize(context) — called once at start
      - handle_data(context, data) — called on every bar
    """
    entry = _STRATEGY_REGISTRY.get(strategy_type)
    if entry is None:
        raise ValueError(f'Unknown strategy: {strategy_type}. '
                         f'Available: {sorted(_STRATEGY_REGISTRY.keys())}')
    return entry['builder'](params)


def get_strategy_catalog() -> Dict[str, Any]:
    """Return full catalog of strategies grouped by category."""
    catalog: Dict[str, list] = {}
    for sid, info in _STRATEGY_REGISTRY.items():
        cat = info['category']
        if cat not in catalog:
            catalog[cat] = []
        catalog[cat].append({
            'id': info['id'],
            'name': info['name'],
            'description': info['description'],
            'params': info['params'],
        })
    return catalog


# ============================================================================
# Helper: rolling calculations (numpy-only, no TA-lib dependency)
# These are also exported for use by calculate_indicator() in the provider.
# ============================================================================

def _rolling_mean(series, window):
    """Simple rolling mean using numpy cumsum trick."""
    arr = np.asarray(series, dtype=float)
    cumsum = np.cumsum(np.insert(arr, 0, 0))
    result = np.full_like(arr, np.nan)
    result[window - 1:] = (cumsum[window:] - cumsum[:-window]) / window
    return result


def _rolling_std(series, window):
    """Rolling standard deviation."""
    arr = np.asarray(series, dtype=float)
    result = np.full_like(arr, np.nan)
    for i in range(window - 1, len(arr)):
        result[i] = np.std(arr[i - window + 1:i + 1], ddof=1)
    return result


def _ema(series, span):
    """Exponential moving average."""
    arr = np.asarray(series, dtype=float)
    alpha = 2.0 / (span + 1)
    result = np.full_like(arr, np.nan)
    result[0] = arr[0]
    for i in range(1, len(arr)):
        result[i] = alpha * arr[i] + (1 - alpha) * result[i - 1]
    return result


def _rsi(series, period=14):
    """Relative Strength Index."""
    arr = np.asarray(series, dtype=float)
    deltas = np.diff(arr)
    gains = np.where(deltas > 0, deltas, 0.0)
    losses = np.where(deltas < 0, -deltas, 0.0)

    avg_gain = np.full(len(arr), np.nan)
    avg_loss = np.full(len(arr), np.nan)
    rsi_vals = np.full(len(arr), np.nan)

    if len(gains) < period:
        return rsi_vals

    avg_gain[period] = np.mean(gains[:period])
    avg_loss[period] = np.mean(losses[:period])

    for i in range(period + 1, len(arr)):
        avg_gain[i] = (avg_gain[i - 1] * (period - 1) + gains[i - 1]) / period
        avg_loss[i] = (avg_loss[i - 1] * (period - 1) + losses[i - 1]) / period

    for i in range(period, len(arr)):
        if avg_loss[i] == 0:
            rsi_vals[i] = 100.0
        else:
            rs = avg_gain[i] / avg_loss[i]
            rsi_vals[i] = 100.0 - (100.0 / (1.0 + rs))

    return rsi_vals


def _atr(high, low, close, period=14):
    """Average True Range."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    tr = np.maximum(h[1:] - l[1:],
                    np.maximum(np.abs(h[1:] - c[:-1]),
                               np.abs(l[1:] - c[:-1])))
    tr = np.insert(tr, 0, h[0] - l[0])
    atr_vals = np.full_like(tr, np.nan)
    atr_vals[period - 1] = np.mean(tr[:period])
    for i in range(period, len(tr)):
        atr_vals[i] = (atr_vals[i - 1] * (period - 1) + tr[i]) / period
    return atr_vals


# ============================================================================
# Standalone indicator functions (used by calculate_indicator)
# ============================================================================

def _macd_calc(close, fast_period=12, slow_period=26, signal_period=9):
    """Calculate MACD line, signal line, and histogram."""
    arr = np.asarray(close, dtype=float)
    fast_ema = _ema(arr, fast_period)
    slow_ema = _ema(arr, slow_period)
    macd_line = fast_ema - slow_ema
    # Signal line: EMA of MACD (handle NaN)
    valid_macd = macd_line.copy()
    valid_macd[np.isnan(valid_macd)] = 0.0
    signal_line = _ema(valid_macd, signal_period)
    histogram = macd_line - signal_line
    return macd_line, signal_line, histogram


def _stochastic_calc(high, low, close, k_period=14, d_period=3):
    """Calculate Stochastic %K and %D."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    k_vals = np.full_like(c, np.nan)

    for i in range(k_period - 1, len(c)):
        hh = h[i - k_period + 1:i + 1].max()
        ll = l[i - k_period + 1:i + 1].min()
        if hh - ll < 1e-8:
            k_vals[i] = 50.0
        else:
            k_vals[i] = 100.0 * (c[i] - ll) / (hh - ll)

    d_vals = _rolling_mean(np.nan_to_num(k_vals, nan=50.0), d_period)
    return k_vals, d_vals


def _williams_r_calc(high, low, close, period=14):
    """Williams %R."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    result = np.full_like(c, np.nan)

    for i in range(period - 1, len(c)):
        hh = h[i - period + 1:i + 1].max()
        ll = l[i - period + 1:i + 1].min()
        if hh - ll < 1e-8:
            result[i] = -50.0
        else:
            result[i] = -100.0 * (hh - c[i]) / (hh - ll)
    return result


def _cci_calc(high, low, close, period=20):
    """Commodity Channel Index."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    tp = (h + l + c) / 3.0
    result = np.full_like(c, np.nan)

    for i in range(period - 1, len(c)):
        window = tp[i - period + 1:i + 1]
        mean = window.mean()
        mad = np.mean(np.abs(window - mean))
        if mad < 1e-8:
            result[i] = 0.0
        else:
            result[i] = (tp[i] - mean) / (0.015 * mad)
    return result


def _obv_calc(close, volume):
    """On-Balance Volume."""
    c = np.asarray(close, dtype=float)
    v = np.asarray(volume, dtype=float)
    obv = np.zeros_like(c)
    obv[0] = v[0]
    for i in range(1, len(c)):
        if c[i] > c[i - 1]:
            obv[i] = obv[i - 1] + v[i]
        elif c[i] < c[i - 1]:
            obv[i] = obv[i - 1] - v[i]
        else:
            obv[i] = obv[i - 1]
    return obv


def _adx_calc(high, low, close, period=14):
    """Average Directional Index (simplified)."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    n = len(c)

    # True Range
    tr = np.zeros(n)
    tr[0] = h[0] - l[0]
    for i in range(1, n):
        tr[i] = max(h[i] - l[i], abs(h[i] - c[i - 1]), abs(l[i] - c[i - 1]))

    # +DM / -DM
    plus_dm = np.zeros(n)
    minus_dm = np.zeros(n)
    for i in range(1, n):
        up = h[i] - h[i - 1]
        down = l[i - 1] - l[i]
        plus_dm[i] = up if (up > down and up > 0) else 0
        minus_dm[i] = down if (down > up and down > 0) else 0

    # Smoothed TR, +DM, -DM
    atr_s = np.zeros(n)
    pdm_s = np.zeros(n)
    mdm_s = np.zeros(n)

    if n <= period:
        return np.full(n, np.nan)

    atr_s[period] = np.sum(tr[1:period + 1])
    pdm_s[period] = np.sum(plus_dm[1:period + 1])
    mdm_s[period] = np.sum(minus_dm[1:period + 1])

    for i in range(period + 1, n):
        atr_s[i] = atr_s[i - 1] - atr_s[i - 1] / period + tr[i]
        pdm_s[i] = pdm_s[i - 1] - pdm_s[i - 1] / period + plus_dm[i]
        mdm_s[i] = mdm_s[i - 1] - mdm_s[i - 1] / period + minus_dm[i]

    # +DI / -DI
    plus_di = np.full(n, np.nan)
    minus_di = np.full(n, np.nan)
    dx = np.full(n, np.nan)

    for i in range(period, n):
        if atr_s[i] > 0:
            plus_di[i] = 100 * pdm_s[i] / atr_s[i]
            minus_di[i] = 100 * mdm_s[i] / atr_s[i]
            di_sum = plus_di[i] + minus_di[i]
            dx[i] = 100 * abs(plus_di[i] - minus_di[i]) / di_sum if di_sum > 0 else 0

    # ADX = smoothed DX
    adx = np.full(n, np.nan)
    start_idx = 2 * period
    if start_idx < n:
        valid_dx = [d for d in dx[period:start_idx] if not np.isnan(d)]
        if valid_dx:
            adx[start_idx - 1] = np.mean(valid_dx)
            for i in range(start_idx, n):
                if not np.isnan(dx[i]) and not np.isnan(adx[i - 1]):
                    adx[i] = (adx[i - 1] * (period - 1) + dx[i]) / period

    return adx


def _keltner_calc(high, low, close, period=20, multiplier=2.0):
    """Keltner Channel (EMA +/- multiplier * ATR)."""
    c = np.asarray(close, dtype=float)
    middle = _ema(c, period)
    atr_vals = _atr(high, low, close, period)
    upper = middle + multiplier * atr_vals
    lower = middle - multiplier * atr_vals
    return upper, middle, lower


def _donchian_calc(high, low, period=20):
    """Donchian Channel (rolling high/low)."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    upper = np.full_like(h, np.nan)
    lower = np.full_like(l, np.nan)
    for i in range(period - 1, len(h)):
        upper[i] = h[i - period + 1:i + 1].max()
        lower[i] = l[i - period + 1:i + 1].min()
    return upper, lower


def _momentum_calc(close, period=12):
    """Momentum / Rate of Change."""
    c = np.asarray(close, dtype=float)
    result = np.full_like(c, np.nan)
    for i in range(period, len(c)):
        if c[i - period] != 0:
            result[i] = (c[i] / c[i - period]) - 1.0
    return result


def _zscore_calc(close, period=20):
    """Z-Score of price."""
    c = np.asarray(close, dtype=float)
    result = np.full_like(c, np.nan)
    for i in range(period - 1, len(c)):
        window = c[i - period + 1:i + 1]
        mean = window.mean()
        std = window.std(ddof=1)
        if std > 1e-8:
            result[i] = (c[i] - mean) / std
    return result


def _vwap_calc(high, low, close, volume):
    """Volume Weighted Average Price (cumulative)."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    v = np.asarray(volume, dtype=float)
    tp = (h + l + c) / 3.0
    cum_tpv = np.cumsum(tp * v)
    cum_v = np.cumsum(v)
    vwap = np.where(cum_v > 0, cum_tpv / cum_v, np.nan)
    return vwap


# ============================================================================
# Trend Following Strategies
# ============================================================================

@_register('sma_crossover', 'trend', 'SMA Crossover',
           'Fast SMA crosses Slow SMA',
           [{'name': 'fastPeriod', 'default': 10}, {'name': 'slowPeriod', 'default': 20}])
def _sma_crossover(params):
    fast = int(params.get('fastPeriod', 10))
    slow = int(params.get('slowPeriod', 20))

    def initialize(context):
        context.fast = fast
        context.slow = slow
        context.history_len = slow + 2
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < context.slow:
            return
        fast_ma = prices[-context.fast:].mean()
        slow_ma = prices[-context.slow:].mean()
        if fast_ma > slow_ma and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif fast_ma < slow_ma and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('ema_crossover', 'trend', 'EMA Crossover',
           'Fast EMA crosses Slow EMA',
           [{'name': 'fastPeriod', 'default': 10}, {'name': 'slowPeriod', 'default': 20}])
def _ema_crossover(params):
    fast = int(params.get('fastPeriod', 10))
    slow = int(params.get('slowPeriod', 20))

    def initialize(context):
        context.fast = fast
        context.slow = slow
        context.history_len = slow + 10
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < context.slow:
            return
        arr = np.asarray(prices)
        fast_ema = _ema(arr, context.fast)[-1]
        slow_ema = _ema(arr, context.slow)[-1]
        if fast_ema > slow_ema and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif fast_ema < slow_ema and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('macd', 'trend', 'MACD Crossover',
           'MACD line crosses Signal line',
           [{'name': 'fastPeriod', 'default': 12},
            {'name': 'slowPeriod', 'default': 26},
            {'name': 'signalPeriod', 'default': 9}])
def _macd(params):
    fast_p = int(params.get('fastPeriod', 12))
    slow_p = int(params.get('slowPeriod', 26))
    signal_p = int(params.get('signalPeriod', 9))

    def initialize(context):
        context.history_len = slow_p + signal_p + 10
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < context.history_len - 5:
            return
        arr = np.asarray(prices)
        macd_line, signal_line, _ = _macd_calc(arr, fast_p, slow_p, signal_p)
        if len(signal_line) < 2:
            return
        if not np.isnan(signal_line[-1]) and not np.isnan(macd_line[-1]):
            if macd_line[-1] > signal_line[-1] and not context.invested:
                context.order_target_percent(context.asset, 1.0)
                context.invested = True
            elif macd_line[-1] < signal_line[-1] and context.invested:
                context.order_target_percent(context.asset, 0.0)
                context.invested = False

    return initialize, handle_data


@_register('adx_trend', 'trend', 'ADX Trend Filter',
           'ADX-based trend following',
           [{'name': 'adxPeriod', 'default': 14}, {'name': 'adxThreshold', 'default': 25}])
def _adx_trend(params):
    period = int(params.get('adxPeriod', 14))
    threshold = float(params.get('adxThreshold', 25))

    def initialize(context):
        context.history_len = period * 3
        context.invested = False

    def handle_data(context, data):
        close = data.history(context.asset, 'close', context.history_len, '1d')
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        if len(close) < context.history_len - 2:
            return

        c = np.asarray(close)
        h = np.asarray(high)
        l = np.asarray(low)

        adx_vals = _adx_calc(h, l, c, period)
        sma = _rolling_mean(c, period)
        if np.isnan(sma[-1]):
            return

        adx_val = adx_vals[-1] if not np.isnan(adx_vals[-1]) else 0
        trending = adx_val > threshold

        if trending and c[-1] > sma[-1] and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif (not trending or c[-1] < sma[-1]) and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('triple_ma', 'trend', 'Triple MA',
           'Three moving average system',
           [{'name': 'fastPeriod', 'default': 10},
            {'name': 'mediumPeriod', 'default': 20},
            {'name': 'slowPeriod', 'default': 50}])
def _triple_ma(params):
    fast = int(params.get('fastPeriod', 10))
    med = int(params.get('mediumPeriod', 20))
    slow = int(params.get('slowPeriod', 50))

    def initialize(context):
        context.history_len = slow + 2
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < slow:
            return
        fast_ma = prices[-fast:].mean()
        med_ma = prices[-med:].mean()
        slow_ma = prices[-slow:].mean()
        if fast_ma > med_ma > slow_ma and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif fast_ma < med_ma and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('keltner_channel', 'trend', 'Keltner Channel',
           'Keltner channel trend system',
           [{'name': 'period', 'default': 20},
            {'name': 'atrMultiplier', 'default': 2.0}])
def _keltner_channel_strategy(params):
    period = int(params.get('period', 20))
    multiplier = float(params.get('atrMultiplier', 2.0))

    def initialize(context):
        context.history_len = period + 10
        context.invested = False

    def handle_data(context, data):
        close = data.history(context.asset, 'close', context.history_len, '1d')
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        if len(close) < period + 2:
            return

        c = np.asarray(close)
        h = np.asarray(high)
        l = np.asarray(low)
        upper, middle, lower = _keltner_calc(h, l, c, period, multiplier)
        current = c[-1]

        if np.isnan(upper[-1]) or np.isnan(lower[-1]):
            return

        if current > upper[-1] and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif current < middle[-1] and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


# ============================================================================
# Mean Reversion Strategies
# ============================================================================

@_register('bollinger_bands', 'meanReversion', 'Bollinger Bands',
           'Bollinger band mean reversion',
           [{'name': 'period', 'default': 20},
            {'name': 'stdDev', 'default': 2.0}])
def _bollinger_bands(params):
    period = int(params.get('period', 20))
    num_std = float(params.get('stdDev', 2.0))

    def initialize(context):
        context.history_len = period + 2
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < period:
            return
        window = np.asarray(prices[-period:])
        mean = window.mean()
        std = window.std(ddof=1)
        upper = mean + num_std * std
        lower = mean - num_std * std
        current = prices.iloc[-1]

        if current < lower and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif current > upper and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('rsi', 'meanReversion', 'RSI Mean Reversion',
           'RSI oversold/overbought',
           [{'name': 'period', 'default': 14},
            {'name': 'oversold', 'default': 30},
            {'name': 'overbought', 'default': 70}])
def _rsi_strategy(params):
    period = int(params.get('period', 14))
    oversold = float(params.get('oversold', 30))
    overbought = float(params.get('overbought', 70))

    def initialize(context):
        context.history_len = period + 10
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < period + 2:
            return
        rsi_vals = _rsi(np.asarray(prices), period)
        current_rsi = rsi_vals[-1]
        if np.isnan(current_rsi):
            return

        if current_rsi < oversold and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif current_rsi > overbought and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('mean_reversion', 'meanReversion', 'Z-Score Reversion',
           'Z-score mean reversion',
           [{'name': 'window', 'default': 20},
            {'name': 'threshold', 'default': 2.0}])
def _zscore_reversion(params):
    window = int(params.get('window', 20))
    threshold = float(params.get('threshold', 2.0))

    def initialize(context):
        context.history_len = window + 2
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < window:
            return
        arr = np.asarray(prices[-window:])
        mean = arr.mean()
        std = arr.std(ddof=1)
        if std < 1e-8:
            return
        zscore = (prices.iloc[-1] - mean) / std

        if zscore < -threshold and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif zscore > threshold and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('stochastic', 'meanReversion', 'Stochastic',
           'Stochastic oscillator',
           [{'name': 'kPeriod', 'default': 14},
            {'name': 'dPeriod', 'default': 3},
            {'name': 'oversold', 'default': 20},
            {'name': 'overbought', 'default': 80}])
def _stochastic(params):
    k_period = int(params.get('kPeriod', 14))
    d_period = int(params.get('dPeriod', 3))
    oversold = float(params.get('oversold', 20))
    overbought = float(params.get('overbought', 80))

    def initialize(context):
        context.history_len = k_period + d_period + 5
        context.invested = False

    def handle_data(context, data):
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        close = data.history(context.asset, 'close', context.history_len, '1d')
        if len(close) < k_period + d_period:
            return

        h_arr = np.asarray(high)
        l_arr = np.asarray(low)
        c_arr = np.asarray(close)
        k_vals, d_vals = _stochastic_calc(h_arr, l_arr, c_arr, k_period, d_period)
        d_val = d_vals[-1] if not np.isnan(d_vals[-1]) else 50.0

        if d_val < oversold and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif d_val > overbought and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('williams_r', 'meanReversion', 'Williams %R',
           'Williams %R oscillator',
           [{'name': 'period', 'default': 14},
            {'name': 'oversold', 'default': -80},
            {'name': 'overbought', 'default': -20}])
def _williams_r_strategy(params):
    period = int(params.get('period', 14))
    oversold = float(params.get('oversold', -80))
    overbought = float(params.get('overbought', -20))

    def initialize(context):
        context.history_len = period + 5
        context.invested = False

    def handle_data(context, data):
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        close = data.history(context.asset, 'close', context.history_len, '1d')
        if len(close) < period:
            return

        wr = _williams_r_calc(np.asarray(high), np.asarray(low), np.asarray(close), period)
        val = wr[-1]
        if np.isnan(val):
            return

        if val < oversold and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif val > overbought and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('cci', 'meanReversion', 'CCI',
           'Commodity Channel Index',
           [{'name': 'period', 'default': 20},
            {'name': 'threshold', 'default': 100}])
def _cci_strategy(params):
    period = int(params.get('period', 20))
    threshold = float(params.get('threshold', 100))

    def initialize(context):
        context.history_len = period + 5
        context.invested = False

    def handle_data(context, data):
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        close = data.history(context.asset, 'close', context.history_len, '1d')
        if len(close) < period:
            return

        cci_vals = _cci_calc(np.asarray(high), np.asarray(low), np.asarray(close), period)
        val = cci_vals[-1]
        if np.isnan(val):
            return

        if val < -threshold and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif val > threshold and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


# ============================================================================
# Momentum Strategies
# ============================================================================

@_register('momentum', 'momentum', 'Momentum (ROC)',
           'Rate of change momentum',
           [{'name': 'period', 'default': 12},
            {'name': 'threshold', 'default': 0.02}])
def _momentum(params):
    period = int(params.get('period', 12))
    threshold = float(params.get('threshold', 0.02))

    def initialize(context):
        context.history_len = period + 2
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < period + 1:
            return
        roc = (prices.iloc[-1] / prices.iloc[-period - 1]) - 1.0

        if roc > threshold and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif roc < -threshold and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('dual_momentum', 'momentum', 'Dual Momentum',
           'Absolute + relative momentum',
           [{'name': 'absolutePeriod', 'default': 12},
            {'name': 'relativePeriod', 'default': 12}])
def _dual_momentum(params):
    abs_period = int(params.get('absolutePeriod', 12))
    rel_period = int(params.get('relativePeriod', 12))
    lookback = max(abs_period, rel_period)

    def initialize(context):
        context.history_len = lookback * 22 + 5  # ~months to trading days
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < lookback + 1:
            return
        abs_return = (prices.iloc[-1] / prices.iloc[-abs_period - 1]) - 1.0
        rel_return = (prices.iloc[-1] / prices.iloc[-rel_period - 1]) - 1.0

        if abs_return > 0 and rel_return > 0 and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif (abs_return < 0 or rel_return < 0) and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('macd_zero_cross', 'momentum', 'MACD Zero Cross',
           'MACD histogram zero-line crossover',
           [{'name': 'fastPeriod', 'default': 12},
            {'name': 'slowPeriod', 'default': 26},
            {'name': 'signalPeriod', 'default': 9}])
def _macd_zero_cross(params):
    fast_p = int(params.get('fastPeriod', 12))
    slow_p = int(params.get('slowPeriod', 26))
    signal_p = int(params.get('signalPeriod', 9))

    def initialize(context):
        context.history_len = slow_p + signal_p + 10
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < context.history_len - 5:
            return
        arr = np.asarray(prices)
        _, _, histogram = _macd_calc(arr, fast_p, slow_p, signal_p)
        if len(histogram) < 2 or np.isnan(histogram[-1]) or np.isnan(histogram[-2]):
            return

        # Zero-line crossover on histogram
        if histogram[-2] <= 0 < histogram[-1] and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif histogram[-2] >= 0 > histogram[-1] and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


# ============================================================================
# Breakout Strategies
# ============================================================================

@_register('breakout', 'breakout', 'Donchian Breakout',
           'Donchian channel breakout',
           [{'name': 'period', 'default': 20}])
def _donchian_breakout(params):
    period = int(params.get('period', 20))

    def initialize(context):
        context.history_len = period + 2
        context.invested = False

    def handle_data(context, data):
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        close = data.history(context.asset, 'close', context.history_len, '1d')
        if len(close) < period:
            return

        upper = np.asarray(high[-period:]).max()
        lower = np.asarray(low[-period:]).min()
        current = close.iloc[-1]

        if current >= upper and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif current <= lower and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('volatility_breakout', 'breakout', 'Volatility Breakout',
           'ATR-based volatility breakout',
           [{'name': 'atrPeriod', 'default': 14},
            {'name': 'atrMultiplier', 'default': 2.0}])
def _volatility_breakout(params):
    period = int(params.get('atrPeriod', 14))
    multiplier = float(params.get('atrMultiplier', 2.0))

    def initialize(context):
        context.history_len = period + 10
        context.invested = False

    def handle_data(context, data):
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        close = data.history(context.asset, 'close', context.history_len, '1d')
        if len(close) < period + 2:
            return

        atr_vals = _atr(np.asarray(high), np.asarray(low), np.asarray(close), period)
        if np.isnan(atr_vals[-1]):
            return

        sma = np.asarray(close[-period:]).mean()
        upper = sma + multiplier * atr_vals[-1]
        current = close.iloc[-1]

        if current > upper and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif current < sma and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


# ============================================================================
# Multi-Indicator Strategies
# ============================================================================

@_register('rsi_macd', 'multiIndicator', 'RSI + MACD',
           'RSI and MACD confluence',
           [{'name': 'rsiPeriod', 'default': 14},
            {'name': 'macdFast', 'default': 12},
            {'name': 'macdSlow', 'default': 26}])
def _rsi_macd(params):
    rsi_p = int(params.get('rsiPeriod', 14))
    macd_fast = int(params.get('macdFast', 12))
    macd_slow = int(params.get('macdSlow', 26))

    def initialize(context):
        context.history_len = max(rsi_p, macd_slow) + 20
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < context.history_len - 5:
            return

        arr = np.asarray(prices)
        rsi_vals = _rsi(arr, rsi_p)
        macd_line, _, _ = _macd_calc(arr, macd_fast, macd_slow, 9)
        current_rsi = rsi_vals[-1]
        macd_val = macd_line[-1]

        if np.isnan(current_rsi) or np.isnan(macd_val):
            return

        if current_rsi < 40 and macd_val > 0 and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif (current_rsi > 60 or macd_val < 0) and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('macd_adx', 'multiIndicator', 'MACD + ADX Filter',
           'MACD with ADX trend filter',
           [{'name': 'macdFast', 'default': 12},
            {'name': 'macdSlow', 'default': 26},
            {'name': 'adxPeriod', 'default': 14}])
def _macd_adx(params):
    macd_fast = int(params.get('macdFast', 12))
    macd_slow = int(params.get('macdSlow', 26))
    adx_period = int(params.get('adxPeriod', 14))

    def initialize(context):
        context.history_len = max(macd_slow, adx_period) + 20
        context.invested = False

    def handle_data(context, data):
        close = data.history(context.asset, 'close', context.history_len, '1d')
        high = data.history(context.asset, 'high', context.history_len, '1d')
        low = data.history(context.asset, 'low', context.history_len, '1d')
        if len(close) < context.history_len - 5:
            return

        c = np.asarray(close)
        macd_line, _, _ = _macd_calc(c, macd_fast, macd_slow, 9)
        macd_val = macd_line[-1]

        adx_vals = _adx_calc(np.asarray(high), np.asarray(low), c, adx_period)
        if np.isnan(macd_val):
            return
        adx_val = adx_vals[-1] if not np.isnan(adx_vals[-1]) else 0
        trending = adx_val > 25

        if macd_val > 0 and trending and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif (macd_val < 0 or not trending) and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('bollinger_rsi', 'multiIndicator', 'Bollinger + RSI',
           'Bollinger bands with RSI',
           [{'name': 'bbPeriod', 'default': 20},
            {'name': 'rsiPeriod', 'default': 14}])
def _bollinger_rsi(params):
    bb_period = int(params.get('bbPeriod', 20))
    rsi_period = int(params.get('rsiPeriod', 14))

    def initialize(context):
        context.history_len = max(bb_period, rsi_period) + 10
        context.invested = False

    def handle_data(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < context.history_len - 5:
            return

        arr = np.asarray(prices)
        rsi_vals = _rsi(arr, rsi_period)
        window = arr[-bb_period:]
        mean = window.mean()
        std = window.std(ddof=1)
        lower = mean - 2 * std
        current = arr[-1]
        current_rsi = rsi_vals[-1]

        if np.isnan(current_rsi):
            return

        if current < lower and current_rsi < 30 and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif (current > mean or current_rsi > 70) and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


# ============================================================================
# Volume-Based Strategies
# ============================================================================

@_register('obv_trend', 'other', 'OBV Trend',
           'On-Balance Volume trend',
           [{'name': 'maPeriod', 'default': 20}])
def _obv_trend_strategy(params):
    ma_period = int(params.get('maPeriod', 20))

    def initialize(context):
        context.history_len = ma_period + 10
        context.invested = False

    def handle_data(context, data):
        close = data.history(context.asset, 'close', context.history_len, '1d')
        volume = data.history(context.asset, 'volume', context.history_len, '1d')
        if len(close) < ma_period + 2:
            return

        obv = _obv_calc(np.asarray(close), np.asarray(volume))
        obv_ma = _rolling_mean(obv, ma_period)

        if np.isnan(obv_ma[-1]):
            return

        if obv[-1] > obv_ma[-1] and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif obv[-1] < obv_ma[-1] and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


# ============================================================================
# Pipeline Strategy (Zipline-unique)
# ============================================================================

@_register('pipeline', 'pipeline', 'Custom Pipeline',
           'Zipline Pipeline API strategy - factor-based screening and ranking',
           [{'name': 'topN', 'default': 5},
            {'name': 'rebalanceDays', 'default': 21}])
def _pipeline_strategy(params):
    """
    Uses a simple factor-based approach (mimics Pipeline concepts).
    Since actual Pipeline requires ingested data bundles, this implements
    a simplified momentum-ranking approach.
    """
    top_n = int(params.get('topN', 5))
    rebalance_days = int(params.get('rebalanceDays', 21))

    def initialize(context):
        context.history_len = 63  # ~3 months
        context.day_count = 0
        context.invested = False

    def handle_data(context, data):
        context.day_count += 1
        if context.day_count % rebalance_days != 0:
            return

        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < 21:
            return

        # Simple momentum factor: 1-month return
        mom_1m = (prices.iloc[-1] / prices.iloc[-21]) - 1.0

        if mom_1m > 0 and not context.invested:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        elif mom_1m < 0 and context.invested:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    return initialize, handle_data


@_register('scheduled_rebalance', 'pipeline', 'Scheduled Rebalance',
           'Monthly rebalance using schedule_function',
           [{'name': 'momentumWindow', 'default': 63}])
def _scheduled_rebalance(params):
    """Strategy that demonstrates schedule_function usage."""
    window = int(params.get('momentumWindow', 63))

    def initialize(context):
        context.history_len = window + 5
        context.invested = False
        # Schedule monthly rebalance
        context.schedule_function(
            _rebalance,
            date_rule={'type': 'month_start', 'days_offset': 0},
            time_rule={'type': 'market_open', 'minutes': 30},
        )

    def _rebalance(context, data):
        prices = data.history(context.asset, 'close', context.history_len, '1d')
        if len(prices) < window:
            return
        ret = (prices.iloc[-1] / prices.iloc[-window]) - 1.0
        if ret > 0:
            context.order_target_percent(context.asset, 1.0)
            context.invested = True
        else:
            context.order_target_percent(context.asset, 0.0)
            context.invested = False

    def handle_data(context, data):
        pass  # All logic in scheduled function

    return initialize, handle_data
