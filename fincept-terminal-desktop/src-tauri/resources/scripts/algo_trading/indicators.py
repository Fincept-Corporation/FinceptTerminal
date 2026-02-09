"""
Unified Indicator Wrapper for Algo Trading

Single entry point: compute_indicator(name, df, params) -> dict of field_name: series
Reuses existing technicals library (ta-based indicators).
"""

import pandas as pd
import numpy as np
import sys
import os

# Add parent to path so we can import technicals
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from technicals.momentum_indicators import (
    calculate_rsi, calculate_stochastic, calculate_williams_r, calculate_roc
)
from technicals.trend_indicators import (
    calculate_sma, calculate_ema, calculate_macd, calculate_cci, calculate_adx
)
from technicals.volatility_indicators import (
    calculate_atr, calculate_bollinger_bands, calculate_keltner_channel, calculate_donchian_channel
)
from technicals.volume_indicators import calculate_obv


def compute_indicator(name: str, df: pd.DataFrame, params: dict) -> dict:
    """
    Compute a technical indicator and return its fields as a dict of Series.

    Args:
        name: Indicator name (case-insensitive), e.g. 'RSI', 'MACD', 'SMA'
        df: DataFrame with columns: open, high, low, close, volume
        params: Indicator parameters, e.g. {'period': 14}

    Returns:
        Dict mapping field names to pd.Series, e.g. {'value': Series([...])}
    """
    name = name.upper()

    if name == 'RSI':
        period = params.get('period', 14)
        series = calculate_rsi(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'SMA':
        period = params.get('period', 20)
        series = calculate_sma(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'EMA':
        period = params.get('period', 12)
        series = calculate_ema(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'MACD':
        fast = params.get('fast', 12)
        slow = params.get('slow', 26)
        signal = params.get('signal', 9)
        result = calculate_macd(df, window_fast=fast, window_slow=slow, window_sign=signal, fillna=True)
        return {
            'line': result['macd'],
            'signal_line': result['macd_signal'],
            'histogram': result['macd_diff'],
        }

    elif name == 'BOLLINGER' or name == 'BB':
        period = params.get('period', 20)
        std_dev = params.get('std_dev', 2)
        result = calculate_bollinger_bands(df, window=period, window_dev=std_dev, fillna=True)
        # Calculate %B
        upper = result['bb_upper']
        lower = result['bb_lower']
        close = df['close']
        pct_b = (close - lower) / (upper - lower)
        return {
            'upper': upper,
            'middle': result['bb_middle'],
            'lower': lower,
            'pct_b': pct_b,
        }

    elif name == 'ATR':
        period = params.get('period', 14)
        series = calculate_atr(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'ADX':
        period = params.get('period', 14)
        result = calculate_adx(df, window=period, fillna=True)
        return {
            'value': result['adx'],
            'plus_di': result['adx_pos'],
            'minus_di': result['adx_neg'],
        }

    elif name == 'CCI':
        period = params.get('period', 20)
        series = calculate_cci(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'OBV':
        series = calculate_obv(df, fillna=True)
        return {'value': series}

    elif name == 'WILLIAMS_R' or name == 'WILLR':
        period = params.get('period', 14)
        series = calculate_williams_r(df, lbp=period, fillna=True)
        return {'value': series}

    elif name == 'STOCHASTIC' or name == 'STOCH':
        period = params.get('period', 14)
        smooth = params.get('smooth', 3)
        result = calculate_stochastic(df, window=period, smooth_window=smooth, fillna=True)
        return {
            'k': result['stoch_k'],
            'd': result['stoch_d'],
        }

    elif name == 'ROC' or name == 'MOMENTUM':
        period = params.get('period', 12)
        series = calculate_roc(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'KELTNER':
        period = params.get('period', 20)
        result = calculate_keltner_channel(df, window=period, fillna=True)
        return {
            'upper': result['kc_upper'],
            'middle': result['kc_middle'],
            'lower': result['kc_lower'],
        }

    elif name == 'DONCHIAN':
        period = params.get('period', 20)
        result = calculate_donchian_channel(df, window=period, fillna=True)
        return {
            'upper': result['dc_upper'],
            'middle': result['dc_middle'],
            'lower': result['dc_lower'],
        }

    elif name == 'SUPERTREND':
        period = params.get('period', 10)
        multiplier = params.get('multiplier', 3.0)
        atr = calculate_atr(df, window=period, fillna=True)
        hl2 = (df['high'] + df['low']) / 2
        upper_band = hl2 + multiplier * atr
        lower_band = hl2 - multiplier * atr
        supertrend = pd.Series(index=df.index, dtype=float)
        direction = pd.Series(1, index=df.index, dtype=int)
        supertrend.iloc[0] = lower_band.iloc[0]
        for i in range(1, len(df)):
            if df['close'].iloc[i] > upper_band.iloc[i - 1]:
                direction.iloc[i] = 1
            elif df['close'].iloc[i] < lower_band.iloc[i - 1]:
                direction.iloc[i] = -1
            else:
                direction.iloc[i] = direction.iloc[i - 1]
            if direction.iloc[i] == 1:
                supertrend.iloc[i] = max(lower_band.iloc[i], supertrend.iloc[i - 1]) if direction.iloc[i - 1] == 1 else lower_band.iloc[i]
            else:
                supertrend.iloc[i] = min(upper_band.iloc[i], supertrend.iloc[i - 1]) if direction.iloc[i - 1] == -1 else upper_band.iloc[i]
        return {
            'value': supertrend,
            'direction': direction,
        }

    elif name == 'VWAP':
        typical_price = (df['high'] + df['low'] + df['close']) / 3
        cumulative_tp_vol = (typical_price * df['volume']).cumsum()
        cumulative_vol = df['volume'].cumsum()
        vwap = cumulative_tp_vol / cumulative_vol
        return {'value': vwap}

    elif name == 'PRICE':
        return {'value': df['close']}

    elif name == 'VOLUME':
        return {'value': df['volume']}

    elif name == 'CHANGE_PCT':
        pct = df['close'].pct_change() * 100
        return {'value': pct.fillna(0)}

    else:
        raise ValueError(f"Unknown indicator: {name}")


def get_latest_value(name: str, df: pd.DataFrame, params: dict, field: str = 'value') -> float:
    """Get the most recent value of an indicator field."""
    result = compute_indicator(name, df, params)
    if field not in result:
        raise ValueError(f"Indicator {name} has no field '{field}'. Available: {list(result.keys())}")
    series = result[field]
    val = series.iloc[-1]
    if pd.isna(val):
        return float('nan')
    return float(val)


def get_last_n_values(name: str, df: pd.DataFrame, params: dict, field: str = 'value', n: int = 2) -> list:
    """Get the last N values of an indicator field (for crossing detection)."""
    result = compute_indicator(name, df, params)
    if field not in result:
        raise ValueError(f"Indicator {name} has no field '{field}'. Available: {list(result.keys())}")
    series = result[field]
    values = series.iloc[-n:].tolist()
    return [float(v) if not pd.isna(v) else float('nan') for v in values]
