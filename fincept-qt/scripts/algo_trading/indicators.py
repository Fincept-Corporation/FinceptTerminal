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
    calculate_rsi, calculate_stochastic, calculate_williams_r, calculate_roc,
    calculate_stoch_rsi, calculate_tsi, calculate_ultimate_oscillator,
    calculate_awesome_oscillator, calculate_kama, calculate_ppo, calculate_pvo
)
from technicals.trend_indicators import (
    calculate_sma, calculate_ema, calculate_wma, calculate_macd, calculate_cci,
    calculate_adx, calculate_ichimoku, calculate_psar, calculate_aroon,
    calculate_stc, calculate_kst, calculate_vortex, calculate_dpo,
    calculate_trix, calculate_mass_index
)
from technicals.volatility_indicators import (
    calculate_atr, calculate_bollinger_bands, calculate_keltner_channel, calculate_donchian_channel
)
from technicals.volume_indicators import (
    calculate_obv, calculate_cmf, calculate_mfi, calculate_force_index,
    calculate_adi, calculate_eom, calculate_vpt, calculate_nvi
)


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

    # ── Stock Attributes ──────────────────────────────────────────────
    if name == 'CLOSE' or name == 'PRICE':
        return {'value': df['close']}

    elif name == 'OPEN':
        return {'value': df['open']}

    elif name == 'HIGH':
        return {'value': df['high']}

    elif name == 'LOW':
        return {'value': df['low']}

    elif name == 'VOLUME':
        return {'value': df['volume']}

    elif name == 'CHANGE_PCT':
        pct = df['close'].pct_change() * 100
        return {'value': pct.fillna(0)}

    elif name == 'VWAP':
        typical_price = (df['high'] + df['low'] + df['close']) / 3
        cumulative_tp_vol = (typical_price * df['volume']).cumsum()
        cumulative_vol = df['volume'].cumsum()
        vwap = cumulative_tp_vol / cumulative_vol
        return {'value': vwap}

    elif name == 'HL2':
        return {'value': (df['high'] + df['low']) / 2}

    elif name == 'HLC3':
        return {'value': (df['high'] + df['low'] + df['close']) / 3}

    elif name == 'OHLC4':
        return {'value': (df['open'] + df['high'] + df['low'] + df['close']) / 4}

    # Heikin-Ashi candles
    elif name == 'HA_CLOSE':
        return {'value': (df['open'] + df['high'] + df['low'] + df['close']) / 4}

    elif name == 'HA_OPEN':
        ha_close = (df['open'] + df['high'] + df['low'] + df['close']) / 4
        ha_open = pd.Series(index=df.index, dtype=float)
        ha_open.iloc[0] = (df['open'].iloc[0] + df['close'].iloc[0]) / 2
        for i in range(1, len(df)):
            ha_open.iloc[i] = (ha_open.iloc[i - 1] + ha_close.iloc[i - 1]) / 2
        return {'value': ha_open}

    elif name == 'HA_HIGH':
        ha_close = (df['open'] + df['high'] + df['low'] + df['close']) / 4
        ha_open = pd.Series(index=df.index, dtype=float)
        ha_open.iloc[0] = (df['open'].iloc[0] + df['close'].iloc[0]) / 2
        for i in range(1, len(df)):
            ha_open.iloc[i] = (ha_open.iloc[i - 1] + ha_close.iloc[i - 1]) / 2
        return {'value': pd.concat([df['high'], ha_open, ha_close], axis=1).max(axis=1)}

    elif name == 'HA_LOW':
        ha_close = (df['open'] + df['high'] + df['low'] + df['close']) / 4
        ha_open = pd.Series(index=df.index, dtype=float)
        ha_open.iloc[0] = (df['open'].iloc[0] + df['close'].iloc[0]) / 2
        for i in range(1, len(df)):
            ha_open.iloc[i] = (ha_open.iloc[i - 1] + ha_close.iloc[i - 1]) / 2
        return {'value': pd.concat([df['low'], ha_open, ha_close], axis=1).min(axis=1)}

    # ── Moving Averages ───────────────────────────────────────────────
    elif name == 'SMA':
        period = params.get('period', 20)
        series = calculate_sma(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'EMA':
        period = params.get('period', 12)
        series = calculate_ema(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'WMA':
        period = params.get('period', 9)
        series = calculate_wma(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'KAMA':
        period = params.get('period', 10)
        pow1 = params.get('pow1', 2)
        pow2 = params.get('pow2', 30)
        series = calculate_kama(df, window=period, pow1=pow1, pow2=pow2, fillna=True)
        return {'value': series}

    elif name == 'TEMA':
        period = params.get('period', 20)
        ema1 = df['close'].ewm(span=period, adjust=False).mean()
        ema2 = ema1.ewm(span=period, adjust=False).mean()
        ema3 = ema2.ewm(span=period, adjust=False).mean()
        return {'value': 3 * ema1 - 3 * ema2 + ema3}

    elif name == 'DEMA':
        period = params.get('period', 20)
        ema1 = df['close'].ewm(span=period, adjust=False).mean()
        ema2 = ema1.ewm(span=period, adjust=False).mean()
        return {'value': 2 * ema1 - ema2}

    elif name == 'HMA':
        period = params.get('period', 14)
        half_period = max(int(period / 2), 1)
        sqrt_period = max(int(period ** 0.5), 1)
        wma_half = df['close'].rolling(half_period).apply(
            lambda x: np.average(x, weights=range(1, half_period + 1)), raw=True
        )
        wma_full = df['close'].rolling(period).apply(
            lambda x: np.average(x, weights=range(1, period + 1)), raw=True
        )
        diff = 2 * wma_half - wma_full
        hma = diff.rolling(sqrt_period).apply(
            lambda x: np.average(x, weights=range(1, sqrt_period + 1)), raw=True
        )
        return {'value': hma.bfill()}

    elif name == 'RMA':
        period = params.get('period', 14)
        rma = df['close'].ewm(alpha=1.0 / period, adjust=False).mean()
        return {'value': rma}

    elif name == 'TMA':
        period = params.get('period', 20)
        sma1 = df['close'].rolling(window=period).mean()
        tma = sma1.rolling(window=period).mean()
        return {'value': tma.bfill()}

    elif name == 'VWMA':
        period = params.get('period', 20)
        vwma = (df['close'] * df['volume']).rolling(period).sum() / df['volume'].rolling(period).sum()
        return {'value': vwma}

    # ── Momentum ──────────────────────────────────────────────────────
    elif name == 'RSI':
        period = params.get('period', 14)
        series = calculate_rsi(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'STOCHASTIC' or name == 'STOCH':
        period = params.get('period', 14)
        smooth = params.get('smooth', 3)
        result = calculate_stochastic(df, window=period, smooth_window=smooth, fillna=True)
        return {
            'k': result['stoch_k'],
            'd': result['stoch_d'],
        }

    elif name == 'STOCH_RSI':
        period = params.get('period', 14)
        smooth1 = params.get('smooth1', 3)
        smooth2 = params.get('smooth2', 3)
        result = calculate_stoch_rsi(df, window=period, smooth1=smooth1, smooth2=smooth2, fillna=True)
        return {
            'stoch_rsi': result['stoch_rsi'],
            'k': result['stoch_rsi_k'],
            'd': result['stoch_rsi_d'],
        }

    elif name == 'WILLIAMS_R' or name == 'WILLR':
        period = params.get('period', 14)
        series = calculate_williams_r(df, lbp=period, fillna=True)
        return {'value': series}

    elif name == 'ROC' or name == 'MOMENTUM':
        period = params.get('period', 12)
        series = calculate_roc(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'TSI':
        window_slow = params.get('window_slow', 25)
        window_fast = params.get('window_fast', 13)
        series = calculate_tsi(df, window_slow=window_slow, window_fast=window_fast, fillna=True)
        return {'value': series}

    elif name == 'ULTIMATE_OSCILLATOR':
        w1 = params.get('window1', 7)
        w2 = params.get('window2', 14)
        w3 = params.get('window3', 28)
        series = calculate_ultimate_oscillator(df, window1=w1, window2=w2, window3=w3, fillna=True)
        return {'value': series}

    elif name == 'AWESOME_OSCILLATOR':
        w1 = params.get('window1', 5)
        w2 = params.get('window2', 34)
        series = calculate_awesome_oscillator(df, window1=w1, window2=w2, fillna=True)
        return {'value': series}

    elif name == 'PPO':
        fast = params.get('fast', 12)
        slow = params.get('slow', 26)
        signal = params.get('signal', 9)
        result = calculate_ppo(df, window_slow=slow, window_fast=fast, window_sign=signal, fillna=True)
        return {
            'ppo': result['ppo'],
            'ppo_signal': result['ppo_signal'],
            'ppo_hist': result['ppo_hist'],
        }

    elif name == 'PVO':
        window_slow = params.get('window_slow', 26)
        window_fast = params.get('window_fast', 12)
        window_sign = params.get('window_sign', 9)
        result = calculate_pvo(df, window_slow=window_slow, window_fast=window_fast, window_sign=window_sign, fillna=True)
        return {
            'pvo': result['pvo'],
            'pvo_signal': result['pvo_signal'],
            'pvo_hist': result['pvo_hist'],
        }

    # ── Trend ─────────────────────────────────────────────────────────
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

    elif name == 'ICHIMOKU':
        w1 = params.get('window1', 9)
        w2 = params.get('window2', 26)
        w3 = params.get('window3', 52)
        result = calculate_ichimoku(df, window1=w1, window2=w2, window3=w3, fillna=True)
        return {
            'conversion': result['ichimoku_conversion'],
            'base': result['ichimoku_base'],
            'span_a': result['ichimoku_a'],
            'span_b': result['ichimoku_b'],
        }

    elif name == 'PSAR':
        step = params.get('step', 0.02)
        max_step = params.get('max_step', 0.2)
        result = calculate_psar(df, step=step, max_step=max_step, fillna=True)
        return {
            'psar': result['psar'],
            'psar_up': result['psar_up'],
            'psar_down': result['psar_down'],
        }

    elif name == 'AROON':
        period = params.get('period', 25)
        result = calculate_aroon(df, window=period, fillna=True)
        return {
            'aroon_up': result['aroon_up'],
            'aroon_down': result['aroon_down'],
            'aroon_indicator': result['aroon_indicator'],
        }

    elif name == 'STC':
        window_slow = params.get('window_slow', 50)
        window_fast = params.get('window_fast', 23)
        cycle = params.get('cycle', 10)
        series = calculate_stc(df, window_slow=window_slow, window_fast=window_fast, cycle=cycle, fillna=True)
        return {'value': series}

    elif name == 'KST':
        nsig = params.get('nsig', 9)
        result = calculate_kst(df, nsig=nsig, fillna=True)
        return {
            'kst': result['kst'],
            'kst_signal': result['kst_signal'],
        }

    elif name == 'VORTEX':
        period = params.get('period', 14)
        result = calculate_vortex(df, window=period, fillna=True)
        return {
            'vortex_pos': result['vortex_pos'],
            'vortex_neg': result['vortex_neg'],
        }

    elif name == 'DPO':
        period = params.get('period', 20)
        series = calculate_dpo(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'TRIX':
        period = params.get('period', 15)
        series = calculate_trix(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'MASS_INDEX':
        window_fast = params.get('window_fast', 9)
        window_slow = params.get('window_slow', 25)
        series = calculate_mass_index(df, window_fast=window_fast, window_slow=window_slow, fillna=True)
        return {'value': series}

    # ── Volatility ────────────────────────────────────────────────────
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

    # ── Volume ────────────────────────────────────────────────────────
    elif name == 'OBV':
        series = calculate_obv(df, fillna=True)
        return {'value': series}

    elif name == 'CMF':
        period = params.get('period', 20)
        series = calculate_cmf(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'MFI':
        period = params.get('period', 14)
        series = calculate_mfi(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'FORCE_INDEX' or name == 'FI':
        period = params.get('period', 13)
        series = calculate_force_index(df, window=period, fillna=True)
        return {'value': series}

    elif name == 'ADI':
        series = calculate_adi(df, fillna=True)
        return {'value': series}

    elif name == 'EOM':
        period = params.get('period', 14)
        result = calculate_eom(df, window=period, fillna=True)
        return {
            'value': result['eom'],
            'signal': result['eom_signal'],
        }

    elif name == 'VPT':
        series = calculate_vpt(df, fillna=True)
        return {'value': series}

    elif name == 'NVI':
        series = calculate_nvi(df, fillna=True)
        return {'value': series}

    # ── Pivot Points ──────────────────────────────────────────────────
    elif name == 'PIVOT':
        pivot_type = int(params.get('type', 0))  # 0=Standard, 1=Fibonacci, 2=Woodie, 3=Camarilla
        # Use previous bar's HLC for pivot calculation
        h = float(df['high'].iloc[-2]) if len(df) > 1 else float(df['high'].iloc[-1])
        l = float(df['low'].iloc[-2]) if len(df) > 1 else float(df['low'].iloc[-1])
        c = float(df['close'].iloc[-2]) if len(df) > 1 else float(df['close'].iloc[-1])

        if pivot_type == 1:  # Fibonacci
            pp = (h + l + c) / 3
            r1 = pp + 0.382 * (h - l)
            s1 = pp - 0.382 * (h - l)
            r2 = pp + 0.618 * (h - l)
            s2 = pp - 0.618 * (h - l)
            r3 = pp + 1.0 * (h - l)
            s3 = pp - 1.0 * (h - l)
        elif pivot_type == 2:  # Woodie
            pp = (h + l + 2 * c) / 4
            r1 = 2 * pp - l
            s1 = 2 * pp - h
            r2 = pp + (h - l)
            s2 = pp - (h - l)
            r3 = h + 2 * (pp - l)
            s3 = l - 2 * (h - pp)
        elif pivot_type == 3:  # Camarilla
            pp = (h + l + c) / 3
            r1 = c + 1.1 * (h - l) / 12
            s1 = c - 1.1 * (h - l) / 12
            r2 = c + 1.1 * (h - l) / 6
            s2 = c - 1.1 * (h - l) / 6
            r3 = c + 1.1 * (h - l) / 4
            s3 = c - 1.1 * (h - l) / 4
        else:  # Standard
            pp = (h + l + c) / 3
            r1 = 2 * pp - l
            s1 = 2 * pp - h
            r2 = pp + (h - l)
            s2 = pp - (h - l)
            r3 = h + 2 * (pp - l)
            s3 = l - 2 * (h - pp)

        length = len(df)
        return {
            'pp': pd.Series([pp] * length, index=df.index),
            'r1': pd.Series([r1] * length, index=df.index),
            'r2': pd.Series([r2] * length, index=df.index),
            'r3': pd.Series([r3] * length, index=df.index),
            's1': pd.Series([s1] * length, index=df.index),
            's2': pd.Series([s2] * length, index=df.index),
            's3': pd.Series([s3] * length, index=df.index),
        }

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


def get_value_at_offset(name: str, df: pd.DataFrame, params: dict, field: str = 'value', offset: int = 0) -> float:
    """Get value of an indicator at N bars ago (offset=0 is latest, offset=1 is previous, etc.)."""
    result = compute_indicator(name, df, params)
    if field not in result:
        raise ValueError(f"Indicator {name} has no field '{field}'. Available: {list(result.keys())}")
    series = result[field]
    idx = -(offset + 1)
    if abs(idx) > len(series):
        return float('nan')
    val = series.iloc[idx]
    return float(val) if not pd.isna(val) else float('nan')
