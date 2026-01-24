"""
VBT Strategy Library

15+ strategies leveraging VectorBT's vectorized computation:

Trend Following:
  1. SMA Crossover
  2. EMA Crossover
  3. MACD Crossover
  4. ADX Trend Filter
  5. Keltner Channel Breakout
  6. Triple MA (3 moving averages)

Mean Reversion:
  7. Z-Score Mean Reversion
  8. Bollinger Band Mean Reversion
  9. RSI Mean Reversion
  10. Stochastic Mean Reversion

Momentum:
  11. Momentum (ROC)
  12. Dual Momentum (absolute + relative)

Breakout:
  13. Donchian Channel Breakout
  14. Volatility Breakout (ATR-based)

Multi-Indicator:
  15. RSI + MACD Confluence
  16. MACD + ADX Filter
  17. Bollinger + RSI Combo

Custom:
  18. Custom Code (user-provided Python)
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Tuple, Optional

from . import vbt_indicators as ind


def build_strategy_signals(
    vbt,
    strategy_type: str,
    close_series: pd.Series,
    parameters: Dict[str, Any],
    high_series: Optional[pd.Series] = None,
    low_series: Optional[pd.Series] = None,
    volume_series: Optional[pd.Series] = None,
) -> Tuple[pd.Series, pd.Series]:
    """
    Build entry/exit signals for any strategy type.

    Args:
        vbt: vectorbt module
        strategy_type: Strategy identifier string
        close_series: Close price series
        parameters: Strategy parameters dict
        high_series: High prices (optional, approximated if not provided)
        low_series: Low prices (optional, approximated if not provided)
        volume_series: Volume data (optional)

    Returns:
        (entries, exits) tuple of boolean pd.Series
    """
    close_np = close_series.values.astype(float)
    index = close_series.index

    # Approximate OHLC if not provided
    if high_series is None:
        high_np = close_np * 1.005
    else:
        high_np = high_series.values.astype(float)

    if low_series is None:
        low_np = close_np * 0.995
    else:
        low_np = low_series.values.astype(float)

    volume_np = volume_series.values.astype(float) if volume_series is not None else np.ones(len(close_np))

    # Dispatch to strategy builder
    strategy_builders = {
        'sma_crossover': _sma_crossover,
        'ema_crossover': _ema_crossover,
        'macd': _macd_strategy,
        'macd_crossover': _macd_strategy,
        'rsi': _rsi_strategy,
        'bollinger_bands': _bollinger_bands,
        'mean_reversion': _mean_reversion,
        'momentum': _momentum,
        'breakout': _breakout,
        'donchian_breakout': _breakout,
        'stochastic': _stochastic,
        'adx_trend': _adx_trend,
        'keltner_breakout': _keltner_breakout,
        'triple_ma': _triple_ma,
        'volatility_breakout': _volatility_breakout,
        'dual_momentum': _dual_momentum,
        'rsi_macd': _rsi_macd_confluence,
        'macd_adx': _macd_adx_filter,
        'bollinger_rsi': _bollinger_rsi_combo,
        'williams_r': _williams_r_strategy,
        'cci': _cci_strategy,
        'obv_trend': _obv_trend,
    }

    builder = strategy_builders.get(strategy_type)
    if builder is None:
        raise ValueError(
            f'Unknown strategy type: {strategy_type}. '
            f'Available: {", ".join(sorted(strategy_builders.keys()))}'
        )

    return builder(vbt, close_np, high_np, low_np, volume_np, index, parameters)


# ============================================================================
# Trend Following Strategies
# ============================================================================

def _sma_crossover(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """SMA Crossover: Fast SMA crosses Slow SMA."""
    fast_period = int(params.get('fastPeriod', 10))
    slow_period = int(params.get('slowPeriod', 20))

    fast_ma = ind.calculate_ma(vbt, close, fast_period, ewm=False)
    slow_ma = ind.calculate_ma(vbt, close, slow_period, ewm=False)

    return ind.generate_crossover_signals(fast_ma, slow_ma, index)


def _ema_crossover(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """EMA Crossover: Fast EMA crosses Slow EMA."""
    fast_period = int(params.get('fastPeriod', 10))
    slow_period = int(params.get('slowPeriod', 20))

    fast_ma = ind.calculate_ma(vbt, close, fast_period, ewm=True)
    slow_ma = ind.calculate_ma(vbt, close, slow_period, ewm=True)

    return ind.generate_crossover_signals(fast_ma, slow_ma, index)


def _macd_strategy(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """MACD Crossover: MACD line crosses Signal line."""
    fast = int(params.get('fastPeriod', 12))
    slow = int(params.get('slowPeriod', 26))
    signal = int(params.get('signalPeriod', 9))

    macd_data = ind.calculate_macd(vbt, close, fast, slow, signal)
    return ind.generate_crossover_signals(macd_data['macd'], macd_data['signal'], index)


def _adx_trend(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """ADX Trend: Trade with trend when ADX > threshold."""
    period = int(params.get('period', 14))
    threshold = float(params.get('threshold', 25))

    adx_data = ind.calculate_adx(close, high, low, period)
    adx = adx_data['adx']
    plus_di = adx_data['plus_di']
    minus_di = adx_data['minus_di']

    # Entry: ADX > threshold AND +DI > -DI (uptrend)
    entries = pd.Series((adx > threshold) & (plus_di > minus_di), index=index)
    # Exit: ADX > threshold AND -DI > +DI (downtrend) OR ADX < threshold (no trend)
    exits = pd.Series(
        ((adx > threshold) & (minus_di > plus_di)) | (adx < threshold * 0.7),
        index=index
    )
    return entries, exits


def _keltner_breakout(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Keltner Channel Breakout: Enter on upper break, exit on middle cross."""
    ema_period = int(params.get('emaPeriod', params.get('period', 20)))
    atr_period = int(params.get('atrPeriod', 10))
    multiplier = float(params.get('multiplier', 2.0))

    keltner = ind.calculate_keltner(high, low, close, ema_period, atr_period, multiplier)

    entries = pd.Series(close > keltner['upper'], index=index)
    exits = pd.Series(close < keltner['middle'], index=index)
    return entries, exits


def _triple_ma(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Triple MA: Fast > Medium > Slow for entry, reverse for exit."""
    fast = int(params.get('fastPeriod', 5))
    medium = int(params.get('mediumPeriod', 20))
    slow = int(params.get('slowPeriod', 50))

    fast_ma = ind.calculate_ma(vbt, close, fast, ewm=True)
    med_ma = ind.calculate_ma(vbt, close, medium, ewm=True)
    slow_ma = ind.calculate_ma(vbt, close, slow, ewm=True)

    # Entry: fast > medium > slow (strong uptrend)
    entries = pd.Series(
        (fast_ma > med_ma) & (med_ma > slow_ma),
        index=index
    )
    # Exit: fast < medium (trend weakening)
    exits = pd.Series(fast_ma < med_ma, index=index)
    return entries, exits


# ============================================================================
# Mean Reversion Strategies
# ============================================================================

def _mean_reversion(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Z-Score Mean Reversion: Buy when oversold, sell when overbought."""
    period = int(params.get('period', 20))
    z_threshold = float(params.get('zThreshold', params.get('z_threshold', 2.0)))

    zscore = ind.calculate_zscore(close, period)
    return ind.generate_mean_reversion_signals(zscore, z_threshold, z_threshold * 0.5, index)


def _bollinger_bands(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Bollinger Band Mean Reversion: Buy at lower, sell at upper."""
    period = int(params.get('period', 20))
    std_dev = float(params.get('stdDev', 2.0))

    bb = ind.calculate_bbands(vbt, close, period, std_dev)

    entries = pd.Series(close < bb['lower'], index=index)
    exits = pd.Series(close > bb['upper'], index=index)
    return entries, exits


def _rsi_strategy(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """RSI Mean Reversion: Buy oversold, sell overbought."""
    period = int(params.get('period', 14))
    oversold = float(params.get('oversold', 30))
    overbought = float(params.get('overbought', 70))

    rsi = ind.calculate_rsi(vbt, close, period)
    return ind.generate_threshold_signals(rsi, oversold, overbought, index)


def _stochastic(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Stochastic Oscillator: %K/%D crossover with oversold/overbought zones."""
    k_period = int(params.get('kPeriod', params.get('k_period', 14)))
    d_period = int(params.get('dPeriod', params.get('d_period', 3)))
    oversold = float(params.get('oversold', 20))
    overbought = float(params.get('overbought', 80))

    stoch = ind.calculate_stoch(vbt, high, low, close, k_period, d_period)
    k = stoch['k']
    d = stoch['d']

    # Entry: %K < oversold AND %K crosses above %D
    entries = pd.Series((k < oversold) & (k > d), index=index)
    # Exit: %K > overbought AND %K crosses below %D
    exits = pd.Series((k > overbought) & (k < d), index=index)
    return entries, exits


# ============================================================================
# Momentum Strategies
# ============================================================================

def _momentum(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Momentum (ROC): Buy strong positive momentum, sell on reversal."""
    lookback = int(params.get('lookback', 20))
    threshold = float(params.get('threshold', 0.0)) / 100.0

    roc = ind.calculate_momentum(close, lookback)

    entries = pd.Series(roc > threshold, index=index)
    exits = pd.Series(roc < -threshold, index=index)
    return entries, exits


def _dual_momentum(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """
    Dual Momentum: Combines absolute and relative momentum.

    Entry: Both short-term and long-term momentum positive
    Exit: Either momentum turns negative
    """
    short_lookback = int(params.get('shortLookback', 20))
    long_lookback = int(params.get('longLookback', 60))
    threshold = float(params.get('threshold', 0.0)) / 100.0

    short_mom = ind.calculate_momentum(close, short_lookback)
    long_mom = ind.calculate_momentum(close, long_lookback)

    # Entry: Both timeframes show positive momentum
    entries = pd.Series(
        (short_mom > threshold) & (long_mom > threshold),
        index=index
    )
    # Exit: Short-term momentum reverses
    exits = pd.Series(short_mom < -threshold, index=index)
    return entries, exits


# ============================================================================
# Breakout Strategies
# ============================================================================

def _breakout(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Donchian Channel Breakout: Buy on upper channel break."""
    period = int(params.get('period', 20))

    donchian = ind.calculate_donchian(close, period)

    entries, exits = ind.generate_breakout_signals(
        close, donchian['upper'], donchian['lower'], index
    )
    return entries, exits


def _volatility_breakout(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """
    Volatility Breakout: Enter when price moves > K * ATR from previous close.

    Popular in Korean/Japanese markets (Larry Williams-style).
    """
    period = int(params.get('period', 20))
    k_mult = float(params.get('kMultiplier', params.get('atr_mult', 1.5)))

    atr = ind.calculate_atr(vbt, high, low, close, period)

    # Entry: Close > prev_close + K * ATR
    prev_close = np.roll(close, 1)
    prev_close[0] = close[0]
    upper_band = prev_close + k_mult * atr
    lower_band = prev_close - k_mult * atr

    entries = pd.Series(close > upper_band, index=index)
    exits = pd.Series(close < lower_band, index=index)
    return entries, exits


# ============================================================================
# Multi-Indicator Strategies
# ============================================================================

def _rsi_macd_confluence(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """
    RSI + MACD Confluence: Only trade when both agree.

    Entry: RSI < oversold AND MACD histogram turns positive
    Exit: RSI > overbought OR MACD histogram turns negative
    """
    rsi_period = int(params.get('rsiPeriod', params.get('period', 14)))
    oversold = float(params.get('oversold', 30))
    overbought = float(params.get('overbought', 70))
    fast = int(params.get('fastPeriod', 12))
    slow = int(params.get('slowPeriod', 26))
    signal = int(params.get('signalPeriod', 9))

    rsi = ind.calculate_rsi(vbt, close, rsi_period)
    macd_data = ind.calculate_macd(vbt, close, fast, slow, signal)
    histogram = macd_data['histogram']

    # Entry: RSI oversold + MACD histogram positive (momentum turning up)
    entries = pd.Series((rsi < oversold) & (histogram > 0), index=index)
    # Exit: RSI overbought OR MACD histogram negative
    exits = pd.Series((rsi > overbought) | (histogram < 0), index=index)
    return entries, exits


def _macd_adx_filter(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """
    MACD + ADX Filter: MACD crossover only when ADX confirms trend.

    Entry: MACD > Signal AND ADX > threshold
    Exit: MACD < Signal
    """
    fast = int(params.get('fastPeriod', 12))
    slow = int(params.get('slowPeriod', 26))
    signal = int(params.get('signalPeriod', 9))
    adx_period = int(params.get('adxPeriod', 14))
    adx_threshold = float(params.get('adxThreshold', params.get('threshold', 25)))

    macd_data = ind.calculate_macd(vbt, close, fast, slow, signal)
    adx_data = ind.calculate_adx(close, high, low, adx_period)

    macd_bullish = macd_data['macd'] > macd_data['signal']
    trend_strong = adx_data['adx'] > adx_threshold

    entries = pd.Series(macd_bullish & trend_strong, index=index)
    exits = pd.Series(~macd_bullish, index=index)
    return entries, exits


def _bollinger_rsi_combo(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """
    Bollinger Bands + RSI Combo: BB mean reversion confirmed by RSI.

    Entry: Price < lower BB AND RSI < oversold
    Exit: Price > upper BB OR RSI > overbought
    """
    bb_period = int(params.get('bbPeriod', params.get('period', 20)))
    bb_std = float(params.get('stdDev', 2.0))
    rsi_period = int(params.get('rsiPeriod', 14))
    oversold = float(params.get('oversold', 30))
    overbought = float(params.get('overbought', 70))

    bb = ind.calculate_bbands(vbt, close, bb_period, bb_std)
    rsi = ind.calculate_rsi(vbt, close, rsi_period)

    entries = pd.Series((close < bb['lower']) & (rsi < oversold), index=index)
    exits = pd.Series((close > bb['upper']) | (rsi > overbought), index=index)
    return entries, exits


# ============================================================================
# Additional Strategies
# ============================================================================

def _williams_r_strategy(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """Williams %R: Buy oversold, sell overbought."""
    period = int(params.get('period', 14))
    oversold = float(params.get('oversold', -80))
    overbought = float(params.get('overbought', -20))

    wr = ind.calculate_williams_r(high, low, close, period)

    entries = pd.Series(wr < oversold, index=index)
    exits = pd.Series(wr > overbought, index=index)
    return entries, exits


def _cci_strategy(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """CCI Strategy: Buy on CCI crossing above -100, sell on crossing below +100."""
    period = int(params.get('period', 20))
    lower = float(params.get('lower', -100))
    upper = float(params.get('upper', 100))

    cci = ind.calculate_cci(high, low, close, period)

    entries = pd.Series(cci < lower, index=index)
    exits = pd.Series(cci > upper, index=index)
    return entries, exits


def _obv_trend(vbt, close, high, low, volume, index, params) -> Tuple[pd.Series, pd.Series]:
    """
    OBV Trend: Trade based on OBV trend confirmation.

    Entry: OBV SMA crosses up (volume confirming price trend)
    Exit: OBV SMA crosses down
    """
    period = int(params.get('period', 20))

    obv = ind.calculate_obv(vbt, close, volume)
    obv_sma = pd.Series(obv).rolling(period).mean().values

    entries = pd.Series(obv > obv_sma, index=index)
    exits = pd.Series(obv < obv_sma, index=index)
    return entries, exits


# ============================================================================
# Strategy Catalog (for frontend)
# ============================================================================

def get_strategy_catalog() -> list:
    """
    Return list of all available strategies with their parameters.

    Used by frontend to populate strategy selection UI.
    """
    return [
        {
            'type': 'sma_crossover',
            'name': 'SMA Crossover',
            'category': 'Trend',
            'description': 'Fast SMA crosses Slow SMA',
            'parameters': [
                {'id': 'fastPeriod', 'name': 'Fast Period', 'default': 10, 'min': 2, 'max': 100},
                {'id': 'slowPeriod', 'name': 'Slow Period', 'default': 20, 'min': 5, 'max': 200},
            ],
        },
        {
            'type': 'ema_crossover',
            'name': 'EMA Crossover',
            'category': 'Trend',
            'description': 'Fast EMA crosses Slow EMA',
            'parameters': [
                {'id': 'fastPeriod', 'name': 'Fast Period', 'default': 10, 'min': 2, 'max': 100},
                {'id': 'slowPeriod', 'name': 'Slow Period', 'default': 20, 'min': 5, 'max': 200},
            ],
        },
        {
            'type': 'macd',
            'name': 'MACD Crossover',
            'category': 'Trend',
            'description': 'MACD line crosses Signal line',
            'parameters': [
                {'id': 'fastPeriod', 'name': 'Fast Period', 'default': 12, 'min': 2, 'max': 50},
                {'id': 'slowPeriod', 'name': 'Slow Period', 'default': 26, 'min': 10, 'max': 100},
                {'id': 'signalPeriod', 'name': 'Signal Period', 'default': 9, 'min': 2, 'max': 30},
            ],
        },
        {
            'type': 'adx_trend',
            'name': 'ADX Trend',
            'category': 'Trend',
            'description': 'Trade with trend when ADX confirms strength',
            'parameters': [
                {'id': 'period', 'name': 'ADX Period', 'default': 14, 'min': 5, 'max': 50},
                {'id': 'threshold', 'name': 'ADX Threshold', 'default': 25, 'min': 10, 'max': 50},
            ],
        },
        {
            'type': 'keltner_breakout',
            'name': 'Keltner Breakout',
            'category': 'Trend',
            'description': 'Keltner Channel upper band breakout',
            'parameters': [
                {'id': 'period', 'name': 'EMA Period', 'default': 20, 'min': 5, 'max': 100},
                {'id': 'atrPeriod', 'name': 'ATR Period', 'default': 10, 'min': 5, 'max': 50},
                {'id': 'multiplier', 'name': 'Multiplier', 'default': 2.0, 'min': 0.5, 'max': 5.0},
            ],
        },
        {
            'type': 'triple_ma',
            'name': 'Triple MA',
            'category': 'Trend',
            'description': 'Three moving averages alignment',
            'parameters': [
                {'id': 'fastPeriod', 'name': 'Fast', 'default': 5, 'min': 2, 'max': 20},
                {'id': 'mediumPeriod', 'name': 'Medium', 'default': 20, 'min': 10, 'max': 50},
                {'id': 'slowPeriod', 'name': 'Slow', 'default': 50, 'min': 30, 'max': 200},
            ],
        },
        {
            'type': 'mean_reversion',
            'name': 'Z-Score Mean Reversion',
            'category': 'Mean Reversion',
            'description': 'Buy oversold (low Z-score), sell overbought',
            'parameters': [
                {'id': 'period', 'name': 'Lookback', 'default': 20, 'min': 5, 'max': 100},
                {'id': 'zThreshold', 'name': 'Z Threshold', 'default': 2.0, 'min': 0.5, 'max': 4.0},
            ],
        },
        {
            'type': 'bollinger_bands',
            'name': 'Bollinger Bands',
            'category': 'Mean Reversion',
            'description': 'Buy at lower band, sell at upper band',
            'parameters': [
                {'id': 'period', 'name': 'Period', 'default': 20, 'min': 5, 'max': 100},
                {'id': 'stdDev', 'name': 'Std Dev', 'default': 2.0, 'min': 0.5, 'max': 4.0},
            ],
        },
        {
            'type': 'rsi',
            'name': 'RSI',
            'category': 'Mean Reversion',
            'description': 'Buy oversold RSI, sell overbought',
            'parameters': [
                {'id': 'period', 'name': 'Period', 'default': 14, 'min': 2, 'max': 50},
                {'id': 'oversold', 'name': 'Oversold', 'default': 30, 'min': 10, 'max': 40},
                {'id': 'overbought', 'name': 'Overbought', 'default': 70, 'min': 60, 'max': 95},
            ],
        },
        {
            'type': 'stochastic',
            'name': 'Stochastic',
            'category': 'Mean Reversion',
            'description': '%K/%D crossover in oversold/overbought zones',
            'parameters': [
                {'id': 'kPeriod', 'name': '%K Period', 'default': 14, 'min': 5, 'max': 30},
                {'id': 'dPeriod', 'name': '%D Period', 'default': 3, 'min': 2, 'max': 10},
                {'id': 'oversold', 'name': 'Oversold', 'default': 20, 'min': 5, 'max': 30},
                {'id': 'overbought', 'name': 'Overbought', 'default': 80, 'min': 70, 'max': 95},
            ],
        },
        {
            'type': 'momentum',
            'name': 'Momentum',
            'category': 'Momentum',
            'description': 'Buy strong positive ROC, sell on reversal',
            'parameters': [
                {'id': 'lookback', 'name': 'Lookback', 'default': 20, 'min': 5, 'max': 100},
                {'id': 'threshold', 'name': 'Threshold %', 'default': 0, 'min': 0, 'max': 20},
            ],
        },
        {
            'type': 'dual_momentum',
            'name': 'Dual Momentum',
            'category': 'Momentum',
            'description': 'Both short and long-term momentum must agree',
            'parameters': [
                {'id': 'shortLookback', 'name': 'Short Lookback', 'default': 20, 'min': 5, 'max': 60},
                {'id': 'longLookback', 'name': 'Long Lookback', 'default': 60, 'min': 30, 'max': 200},
                {'id': 'threshold', 'name': 'Threshold %', 'default': 0, 'min': 0, 'max': 10},
            ],
        },
        {
            'type': 'breakout',
            'name': 'Donchian Breakout',
            'category': 'Breakout',
            'description': 'Buy on N-bar high breakout, sell on N-bar low',
            'parameters': [
                {'id': 'period', 'name': 'Channel Period', 'default': 20, 'min': 5, 'max': 100},
            ],
        },
        {
            'type': 'volatility_breakout',
            'name': 'Volatility Breakout',
            'category': 'Breakout',
            'description': 'Enter when price moves K*ATR from previous close',
            'parameters': [
                {'id': 'period', 'name': 'ATR Period', 'default': 20, 'min': 5, 'max': 50},
                {'id': 'kMultiplier', 'name': 'K Multiplier', 'default': 1.5, 'min': 0.5, 'max': 4.0},
            ],
        },
        {
            'type': 'rsi_macd',
            'name': 'RSI + MACD',
            'category': 'Multi-Indicator',
            'description': 'RSI oversold confirmed by MACD momentum',
            'parameters': [
                {'id': 'rsiPeriod', 'name': 'RSI Period', 'default': 14, 'min': 5, 'max': 30},
                {'id': 'oversold', 'name': 'Oversold', 'default': 30, 'min': 15, 'max': 40},
                {'id': 'overbought', 'name': 'Overbought', 'default': 70, 'min': 60, 'max': 90},
                {'id': 'fastPeriod', 'name': 'MACD Fast', 'default': 12, 'min': 5, 'max': 30},
                {'id': 'slowPeriod', 'name': 'MACD Slow', 'default': 26, 'min': 15, 'max': 50},
                {'id': 'signalPeriod', 'name': 'MACD Signal', 'default': 9, 'min': 3, 'max': 20},
            ],
        },
        {
            'type': 'macd_adx',
            'name': 'MACD + ADX',
            'category': 'Multi-Indicator',
            'description': 'MACD crossover filtered by ADX trend strength',
            'parameters': [
                {'id': 'fastPeriod', 'name': 'MACD Fast', 'default': 12, 'min': 5, 'max': 30},
                {'id': 'slowPeriod', 'name': 'MACD Slow', 'default': 26, 'min': 15, 'max': 50},
                {'id': 'signalPeriod', 'name': 'MACD Signal', 'default': 9, 'min': 3, 'max': 20},
                {'id': 'adxPeriod', 'name': 'ADX Period', 'default': 14, 'min': 5, 'max': 30},
                {'id': 'adxThreshold', 'name': 'ADX Threshold', 'default': 25, 'min': 15, 'max': 50},
            ],
        },
        {
            'type': 'bollinger_rsi',
            'name': 'Bollinger + RSI',
            'category': 'Multi-Indicator',
            'description': 'Bollinger Band reversion confirmed by RSI',
            'parameters': [
                {'id': 'bbPeriod', 'name': 'BB Period', 'default': 20, 'min': 5, 'max': 50},
                {'id': 'stdDev', 'name': 'Std Dev', 'default': 2.0, 'min': 1.0, 'max': 4.0},
                {'id': 'rsiPeriod', 'name': 'RSI Period', 'default': 14, 'min': 5, 'max': 30},
                {'id': 'oversold', 'name': 'Oversold', 'default': 30, 'min': 15, 'max': 40},
                {'id': 'overbought', 'name': 'Overbought', 'default': 70, 'min': 60, 'max': 90},
            ],
        },
        {
            'type': 'williams_r',
            'name': 'Williams %R',
            'category': 'Mean Reversion',
            'description': 'Williams Percent Range oscillator',
            'parameters': [
                {'id': 'period', 'name': 'Period', 'default': 14, 'min': 5, 'max': 50},
                {'id': 'oversold', 'name': 'Oversold', 'default': -80, 'min': -95, 'max': -60},
                {'id': 'overbought', 'name': 'Overbought', 'default': -20, 'min': -40, 'max': -5},
            ],
        },
        {
            'type': 'cci',
            'name': 'CCI',
            'category': 'Momentum',
            'description': 'Commodity Channel Index extremes',
            'parameters': [
                {'id': 'period', 'name': 'Period', 'default': 20, 'min': 5, 'max': 50},
                {'id': 'lower', 'name': 'Lower', 'default': -100, 'min': -200, 'max': -50},
                {'id': 'upper', 'name': 'Upper', 'default': 100, 'min': 50, 'max': 200},
            ],
        },
        {
            'type': 'obv_trend',
            'name': 'OBV Trend',
            'category': 'Volume',
            'description': 'On-Balance Volume trend confirmation',
            'parameters': [
                {'id': 'period', 'name': 'OBV SMA Period', 'default': 20, 'min': 5, 'max': 100},
            ],
        },
    ]
