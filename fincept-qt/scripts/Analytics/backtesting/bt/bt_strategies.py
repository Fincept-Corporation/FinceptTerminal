"""
BT Strategy Implementations

Portfolio-level strategies using bt's composable algo pipeline.
Each strategy returns a bt.Strategy object (or a builder function
that creates one given data).

Also provides numpy-based indicator helper functions shared with
the provider for calculate_indicator().

Strategies unique to bt:
- Portfolio allocation: equal weight, inverse vol, mean-var, risk parity,
  target vol, min variance
- Momentum selection: top-N momentum, momentum + inverse vol
- Trend/mean-reversion: SMA/EMA crossover, RSI, Bollinger bands, z-score
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


def get_strategy(strategy_type: str, params: Dict[str, Any]):
    """
    Look up a strategy by type and return a builder function.

    The builder function signature: build(data, name=None) -> bt.Strategy
    It creates a bt.Strategy with the appropriate algo pipeline.

    If bt is not installed, returns a fallback that does numpy simulation.
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
    result = np.empty_like(arr)
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
            rsi_vals[i] = 100.0 - 100.0 / (1.0 + rs)

    return rsi_vals


def _macd_calc(series, fast=12, slow=26, signal=9):
    """MACD calculation returning (macd_line, signal_line, histogram)."""
    arr = np.asarray(series, dtype=float)
    fast_ema = _ema(arr, fast)
    slow_ema = _ema(arr, slow)
    macd_line = fast_ema - slow_ema
    signal_line = _ema(macd_line, signal)
    histogram = macd_line - signal_line
    return macd_line, signal_line, histogram


def _zscore_calc(series, window):
    """Rolling z-score."""
    arr = np.asarray(series, dtype=float)
    mean = _rolling_mean(arr, window)
    std = _rolling_std(arr, window)
    result = np.full_like(arr, np.nan)
    valid = ~np.isnan(mean) & ~np.isnan(std) & (std > 0)
    result[valid] = (arr[valid] - mean[valid]) / std[valid]
    return result


def _bollinger_bands(series, period=20, std_dev=2.0):
    """Bollinger Bands returning (upper, middle, lower)."""
    arr = np.asarray(series, dtype=float)
    middle = _rolling_mean(arr, period)
    std = _rolling_std(arr, period)
    upper = middle + std_dev * std
    lower = middle - std_dev * std
    return upper, middle, lower


def _atr(high, low, close, period=14):
    """Average True Range."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    tr = np.maximum(h - l, np.maximum(np.abs(h - np.roll(c, 1)),
                                       np.abs(l - np.roll(c, 1))))
    tr[0] = h[0] - l[0]
    return _rolling_mean(tr, period)


def _adx_calc(high, low, close, period=14):
    """ADX calculation."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    n = len(c)

    up_move = np.diff(h, prepend=h[0])
    down_move = -np.diff(l, prepend=l[0])
    plus_dm = np.where((up_move > down_move) & (up_move > 0), up_move, 0.0)
    minus_dm = np.where((down_move > up_move) & (down_move > 0), down_move, 0.0)

    atr_vals = _atr(h, l, c, period)
    smooth_plus = _ema(plus_dm, period)
    smooth_minus = _ema(minus_dm, period)

    plus_di = np.where(atr_vals > 0, 100 * smooth_plus / atr_vals, 0.0)
    minus_di = np.where(atr_vals > 0, 100 * smooth_minus / atr_vals, 0.0)

    dx = np.where((plus_di + minus_di) > 0,
                  100 * np.abs(plus_di - minus_di) / (plus_di + minus_di), 0.0)
    adx = _ema(dx, period)
    return adx, plus_di, minus_di


def _momentum_calc(series, period=12):
    """Rate of change momentum."""
    arr = np.asarray(series, dtype=float)
    result = np.full_like(arr, np.nan)
    result[period:] = arr[period:] / arr[:-period] - 1.0
    return result


def _stochastic_calc(high, low, close, k_period=14, d_period=3):
    """Stochastic oscillator."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    n = len(c)
    k_vals = np.full(n, np.nan)
    for i in range(k_period - 1, n):
        hh = np.max(h[i - k_period + 1:i + 1])
        ll = np.min(l[i - k_period + 1:i + 1])
        if hh - ll > 0:
            k_vals[i] = 100 * (c[i] - ll) / (hh - ll)
        else:
            k_vals[i] = 50.0
    d_vals = _rolling_mean(k_vals, d_period)
    return k_vals, d_vals


def _williams_r_calc(high, low, close, period=14):
    """Williams %R oscillator."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    n = len(c)
    wr = np.full(n, np.nan)
    for i in range(period - 1, n):
        hh = np.max(h[i - period + 1:i + 1])
        ll = np.min(l[i - period + 1:i + 1])
        if hh - ll > 0:
            wr[i] = -100 * (hh - c[i]) / (hh - ll)
        else:
            wr[i] = -50.0
    return wr


def _cci_calc(high, low, close, period=20):
    """Commodity Channel Index."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    tp = (h + l + c) / 3.0
    tp_ma = _rolling_mean(tp, period)
    n = len(c)
    mad = np.full(n, np.nan)
    for i in range(period - 1, n):
        window = tp[i - period + 1:i + 1]
        mad[i] = np.mean(np.abs(window - np.mean(window)))
    result = np.full(n, np.nan)
    valid = ~np.isnan(tp_ma) & ~np.isnan(mad) & (mad > 0)
    result[valid] = (tp[valid] - tp_ma[valid]) / (0.015 * mad[valid])
    return result


def _obv_calc(close, volume):
    """On-Balance Volume."""
    c = np.asarray(close, dtype=float)
    v = np.asarray(volume, dtype=float)
    obv = np.zeros(len(c))
    for i in range(1, len(c)):
        if c[i] > c[i - 1]:
            obv[i] = obv[i - 1] + v[i]
        elif c[i] < c[i - 1]:
            obv[i] = obv[i - 1] - v[i]
        else:
            obv[i] = obv[i - 1]
    return obv


def _donchian_calc(high, low, period=20):
    """Donchian channels returning (upper, lower)."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    n = len(h)
    upper = np.full(n, np.nan)
    lower = np.full(n, np.nan)
    for i in range(period - 1, n):
        upper[i] = np.max(h[i - period + 1:i + 1])
        lower[i] = np.min(l[i - period + 1:i + 1])
    return upper, lower


def _keltner_calc(high, low, close, period=20, atr_mult=2.0):
    """Keltner channels returning (upper, middle, lower)."""
    c = np.asarray(close, dtype=float)
    middle = _ema(c, period)
    atr_vals = _atr(high, low, close, period)
    upper = middle + atr_mult * atr_vals
    lower = middle - atr_mult * atr_vals
    return upper, middle, lower


def _vwap_calc(high, low, close, volume):
    """Volume Weighted Average Price."""
    h = np.asarray(high, dtype=float)
    l = np.asarray(low, dtype=float)
    c = np.asarray(close, dtype=float)
    v = np.asarray(volume, dtype=float)
    tp = (h + l + c) / 3.0
    cum_tpv = np.cumsum(tp * v)
    cum_v = np.cumsum(v)
    return np.where(cum_v > 0, cum_tpv / cum_v, 0.0)


# ============================================================================
# bt algo helpers — try to import bt, fall back gracefully
# ============================================================================

_BT_AVAILABLE = False
try:
    import bt as _bt
    _BT_AVAILABLE = True
except ImportError:
    _bt = None


def _make_bt_strategy(name, algos, data):
    """Create a bt.Strategy + bt.Backtest if bt is available."""
    if not _BT_AVAILABLE:
        return None, None
    strategy = _bt.Strategy(name, algos)
    return strategy, data


# ============================================================================
# Portfolio Allocation Strategies (unique to bt)
# ============================================================================

@_register('equal_weight', 'portfolio', 'Equal Weight',
           'Equal-weight allocation across all assets, rebalanced periodically',
           [{'name': 'rebalancePeriod', 'label': 'Rebalance Period', 'default': 'monthly'}])
def _build_equal_weight(params):
    """Equal-weight portfolio strategy."""
    def build(data, name='equal_weight'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('inv_vol', 'portfolio', 'Inverse Volatility',
           'Weight assets inversely proportional to their volatility',
           [{'name': 'lookback', 'label': 'Lookback (days)', 'default': 20}])
def _build_inv_vol(params):
    """Inverse volatility weighted portfolio."""
    lookback = int(params.get('lookback', 20))

    def build(data, name='inv_vol'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectAll(),
                _bt.algos.WeighInvVol(lookback=lookback),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('mean_var', 'portfolio', 'Mean-Variance',
           'Mean-variance optimized portfolio (Markowitz)',
           [{'name': 'lookback', 'label': 'Lookback (days)', 'default': 60}])
def _build_mean_var(params):
    """Mean-variance optimized portfolio."""
    lookback = int(params.get('lookback', 60))

    def build(data, name='mean_var'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectAll(),
                _bt.algos.WeighMeanVar(lookback=lookback),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('risk_parity', 'portfolio', 'Risk Parity',
           'Equal risk contribution (ERC) portfolio',
           [{'name': 'lookback', 'label': 'Lookback (days)', 'default': 60}])
def _build_risk_parity(params):
    """Risk parity (equal risk contribution) portfolio."""
    lookback = int(params.get('lookback', 60))

    def build(data, name='risk_parity'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectAll(),
                _bt.algos.WeighERC(lookback=lookback),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('target_vol', 'portfolio', 'Target Volatility',
           'Target a specific portfolio volatility level',
           [{'name': 'targetVol', 'label': 'Target Vol (%)', 'default': 10, 'min': 1, 'max': 50},
            {'name': 'lookback', 'label': 'Lookback (days)', 'default': 20}])
def _build_target_vol(params):
    """Target volatility portfolio."""
    target = float(params.get('targetVol', 10)) / 100.0
    lookback = int(params.get('lookback', 20))

    def build(data, name='target_vol'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectAll(),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('min_var', 'portfolio', 'Minimum Variance',
           'Minimum variance portfolio (lowest risk)',
           [{'name': 'lookback', 'label': 'Lookback (days)', 'default': 60}])
def _build_min_var(params):
    """Minimum variance portfolio."""
    lookback = int(params.get('lookback', 60))

    def build(data, name='min_var'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectAll(),
                _bt.algos.WeighMeanVar(lookback=lookback),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# Momentum Selection Strategies
# ============================================================================

@_register('momentum_topn', 'momentum', 'Momentum Top-N',
           'Select top N assets by momentum, equal weight',
           [{'name': 'topN', 'label': 'Top N', 'default': 5, 'min': 1, 'max': 50},
            {'name': 'lookback', 'label': 'Momentum Lookback', 'default': 60}])
def _build_momentum_topn(params):
    """Momentum top-N selection strategy."""
    top_n = int(params.get('topN', 5))
    lookback = int(params.get('lookback', 60))

    def build(data, name='momentum_topn'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectMomentum(n=top_n, lookback=pd.DateOffset(days=lookback)),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('momentum_inv_vol', 'momentum', 'Momentum + Inv Vol',
           'Select top N by momentum, weight by inverse volatility',
           [{'name': 'topN', 'label': 'Top N', 'default': 5, 'min': 1, 'max': 50},
            {'name': 'lookback', 'label': 'Lookback', 'default': 60}])
def _build_momentum_inv_vol(params):
    """Momentum selection with inverse volatility weighting."""
    top_n = int(params.get('topN', 5))
    lookback = int(params.get('lookback', 60))

    def build(data, name='momentum_inv_vol'):
        if _BT_AVAILABLE:
            algos = [
                _bt.algos.RunMonthly(),
                _bt.algos.SelectMomentum(n=top_n, lookback=pd.DateOffset(days=lookback)),
                _bt.algos.WeighInvVol(lookback=lookback),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('momentum', 'momentum', 'Momentum (ROC)',
           'Rate of change momentum',
           [{'name': 'period', 'label': 'Period', 'default': 12, 'min': 5, 'max': 50},
            {'name': 'threshold', 'label': 'Threshold', 'default': 0.02, 'min': 0.01, 'max': 0.1}])
def _build_momentum(params):
    """Simple momentum (rate of change) strategy."""
    period = int(params.get('period', 12))
    threshold = float(params.get('threshold', 0.02))

    def build(data, name='momentum'):
        # Uses numpy fallback for signal-based strategies
        return None
    return build


@_register('dual_momentum', 'momentum', 'Dual Momentum',
           'Absolute + relative momentum',
           [{'name': 'absolutePeriod', 'label': 'Absolute Period', 'default': 12, 'min': 3, 'max': 24},
            {'name': 'relativePeriod', 'label': 'Relative Period', 'default': 12, 'min': 3, 'max': 24}])
def _build_dual_momentum(params):
    """Dual momentum strategy."""
    def build(data, name='dual_momentum'):
        return None
    return build


# ============================================================================
# Trend Following Strategies
# ============================================================================

@_register('sma_crossover', 'trend', 'SMA Crossover',
           'Fast SMA crosses Slow SMA',
           [{'name': 'fastPeriod', 'label': 'Fast Period', 'default': 10, 'min': 2, 'max': 100},
            {'name': 'slowPeriod', 'label': 'Slow Period', 'default': 20, 'min': 5, 'max': 200}])
def _build_sma_crossover(params):
    """SMA crossover strategy."""
    fast = int(params.get('fastPeriod', 10))
    slow = int(params.get('slowPeriod', 20))

    def build(data, name='sma_crossover'):
        if _BT_AVAILABLE:
            # Create signal: 1 when fast > slow, 0 otherwise
            import pandas as pd
            fast_ma = data.rolling(fast).mean()
            slow_ma = data.rolling(slow).mean()
            signal = (fast_ma > slow_ma).astype(float)
            algos = [
                _bt.algos.RunDaily(),
                _bt.algos.SelectWhere(signal),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('ema_crossover', 'trend', 'EMA Crossover',
           'Fast EMA crosses Slow EMA',
           [{'name': 'fastPeriod', 'label': 'Fast Period', 'default': 10, 'min': 2, 'max': 100},
            {'name': 'slowPeriod', 'label': 'Slow Period', 'default': 20, 'min': 5, 'max': 200}])
def _build_ema_crossover(params):
    """EMA crossover strategy."""
    fast = int(params.get('fastPeriod', 10))
    slow = int(params.get('slowPeriod', 20))

    def build(data, name='ema_crossover'):
        if _BT_AVAILABLE:
            import pandas as pd
            fast_ema = data.ewm(span=fast).mean()
            slow_ema = data.ewm(span=slow).mean()
            signal = (fast_ema > slow_ema).astype(float)
            algos = [
                _bt.algos.RunDaily(),
                _bt.algos.SelectWhere(signal),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('macd', 'trend', 'MACD Crossover',
           'MACD line crosses Signal line',
           [{'name': 'fastPeriod', 'label': 'Fast Period', 'default': 12, 'min': 2, 'max': 50},
            {'name': 'slowPeriod', 'label': 'Slow Period', 'default': 26, 'min': 10, 'max': 100},
            {'name': 'signalPeriod', 'label': 'Signal Period', 'default': 9, 'min': 2, 'max': 50}])
def _build_macd(params):
    """MACD crossover strategy."""
    fast = int(params.get('fastPeriod', 12))
    slow = int(params.get('slowPeriod', 26))
    sig = int(params.get('signalPeriod', 9))

    def build(data, name='macd'):
        if _BT_AVAILABLE:
            import pandas as pd
            fast_ema = data.ewm(span=fast).mean()
            slow_ema = data.ewm(span=slow).mean()
            macd_line = fast_ema - slow_ema
            signal_line = macd_line.ewm(span=sig).mean()
            signal = (macd_line > signal_line).astype(float)
            algos = [
                _bt.algos.RunDaily(),
                _bt.algos.SelectWhere(signal),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('adx_trend', 'trend', 'ADX Trend Filter',
           'ADX-based trend following',
           [{'name': 'adxPeriod', 'label': 'ADX Period', 'default': 14, 'min': 5, 'max': 50},
            {'name': 'adxThreshold', 'label': 'ADX Threshold', 'default': 25, 'min': 10, 'max': 50}])
def _build_adx_trend(params):
    """ADX trend filter strategy."""
    def build(data, name='adx_trend'):
        return None  # Uses numpy fallback (needs OHLC)
    return build


# ============================================================================
# Mean Reversion Strategies
# ============================================================================

@_register('mean_reversion', 'meanReversion', 'Z-Score Reversion',
           'Z-score mean reversion',
           [{'name': 'window', 'label': 'Window', 'default': 20, 'min': 5, 'max': 100},
            {'name': 'threshold', 'label': 'Z-Score Threshold', 'default': 2.0, 'min': 0.5, 'max': 5.0}])
def _build_mean_reversion(params):
    """Z-score mean reversion strategy."""
    window = int(params.get('window', 20))
    threshold = float(params.get('threshold', 2.0))

    def build(data, name='mean_reversion'):
        if _BT_AVAILABLE:
            import pandas as pd
            mean = data.rolling(window).mean()
            std = data.rolling(window).std()
            zscore = (data - mean) / std
            signal = (zscore < -threshold).astype(float)
            algos = [
                _bt.algos.RunDaily(),
                _bt.algos.SelectWhere(signal),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('bollinger_bands', 'meanReversion', 'Bollinger Bands',
           'Bollinger band mean reversion',
           [{'name': 'period', 'label': 'Period', 'default': 20, 'min': 5, 'max': 100},
            {'name': 'stdDev', 'label': 'Std Dev', 'default': 2.0, 'min': 1.0, 'max': 4.0}])
def _build_bollinger_bands(params):
    """Bollinger band mean reversion strategy."""
    period = int(params.get('period', 20))
    std_dev = float(params.get('stdDev', 2.0))

    def build(data, name='bollinger_bands'):
        if _BT_AVAILABLE:
            import pandas as pd
            mean = data.rolling(period).mean()
            std = data.rolling(period).std()
            lower = mean - std_dev * std
            signal = (data < lower).astype(float)
            algos = [
                _bt.algos.RunDaily(),
                _bt.algos.SelectWhere(signal),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


@_register('rsi', 'meanReversion', 'RSI Mean Reversion',
           'RSI oversold/overbought',
           [{'name': 'period', 'label': 'Period', 'default': 14, 'min': 2, 'max': 50},
            {'name': 'oversold', 'label': 'Oversold', 'default': 30, 'min': 10, 'max': 40},
            {'name': 'overbought', 'label': 'Overbought', 'default': 70, 'min': 60, 'max': 90}])
def _build_rsi(params):
    """RSI mean reversion strategy."""
    period = int(params.get('period', 14))
    oversold = float(params.get('oversold', 30))

    def build(data, name='rsi'):
        # Uses numpy fallback for RSI calculation
        return None
    return build


@_register('stochastic', 'meanReversion', 'Stochastic',
           'Stochastic oscillator',
           [{'name': 'kPeriod', 'label': 'K Period', 'default': 14, 'min': 5, 'max': 50},
            {'name': 'dPeriod', 'label': 'D Period', 'default': 3, 'min': 2, 'max': 20},
            {'name': 'oversold', 'label': 'Oversold', 'default': 20, 'min': 10, 'max': 30},
            {'name': 'overbought', 'label': 'Overbought', 'default': 80, 'min': 70, 'max': 90}])
def _build_stochastic(params):
    """Stochastic oscillator strategy."""
    def build(data, name='stochastic'):
        return None  # Needs OHLC, uses numpy fallback
    return build


# ============================================================================
# Breakout Strategies
# ============================================================================

@_register('breakout', 'breakout', 'Donchian Breakout',
           'Donchian channel breakout',
           [{'name': 'period', 'label': 'Period', 'default': 20, 'min': 5, 'max': 100}])
def _build_breakout(params):
    """Donchian breakout strategy."""
    period = int(params.get('period', 20))

    def build(data, name='breakout'):
        if _BT_AVAILABLE:
            import pandas as pd
            upper = data.rolling(period).max()
            signal = (data >= upper).astype(float)
            algos = [
                _bt.algos.RunDaily(),
                _bt.algos.SelectWhere(signal),
                _bt.algos.WeighEqually(),
                _bt.algos.Rebalance(),
            ]
            return _bt.Strategy(name, algos)
        return None
    return build


# ============================================================================
# Indicator catalog for get_indicators command
# ============================================================================

INDICATOR_CATALOG = {
    'trend': [
        {'id': 'ma', 'name': 'Moving Average', 'params': ['period', 'ewm']},
        {'id': 'ema', 'name': 'EMA', 'params': ['period']},
        {'id': 'macd', 'name': 'MACD', 'params': ['fast', 'slow', 'signal']},
        {'id': 'adx', 'name': 'ADX', 'params': ['period']},
        {'id': 'keltner', 'name': 'Keltner Channels', 'params': ['period', 'atrMult']},
        {'id': 'donchian', 'name': 'Donchian Channels', 'params': ['period']},
    ],
    'momentum': [
        {'id': 'rsi', 'name': 'RSI', 'params': ['period']},
        {'id': 'stoch', 'name': 'Stochastic', 'params': ['kPeriod', 'dPeriod']},
        {'id': 'momentum', 'name': 'Momentum (ROC)', 'params': ['period']},
        {'id': 'williams_r', 'name': 'Williams %R', 'params': ['period']},
        {'id': 'cci', 'name': 'CCI', 'params': ['period']},
    ],
    'volatility': [
        {'id': 'bbands', 'name': 'Bollinger Bands', 'params': ['period', 'alpha']},
        {'id': 'atr', 'name': 'ATR', 'params': ['period']},
        {'id': 'mstd', 'name': 'Moving Std Dev', 'params': ['period']},
        {'id': 'zscore', 'name': 'Z-Score', 'params': ['window']},
    ],
    'volume': [
        {'id': 'obv', 'name': 'OBV', 'params': []},
        {'id': 'vwap', 'name': 'VWAP', 'params': []},
    ],
}
