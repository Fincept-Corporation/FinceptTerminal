"""
Backtesting.py Indicators Module

Full technical indicator coverage using pandas/numpy.
Each function takes a DataFrame with OHLCV columns and returns computed values.
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional


# ============================================================================
# Trend Indicators
# ============================================================================

def sma(close: pd.Series, period: int = 20) -> pd.Series:
    """Simple Moving Average"""
    return close.rolling(window=period).mean()


def ema(close: pd.Series, period: int = 20) -> pd.Series:
    """Exponential Moving Average"""
    return close.ewm(span=period, adjust=False).mean()


def dema(close: pd.Series, period: int = 20) -> pd.Series:
    """Double Exponential Moving Average"""
    e = ema(close, period)
    return 2 * e - ema(e, period)


def tema(close: pd.Series, period: int = 20) -> pd.Series:
    """Triple Exponential Moving Average"""
    e1 = ema(close, period)
    e2 = ema(e1, period)
    e3 = ema(e2, period)
    return 3 * e1 - 3 * e2 + e3


def wma(close: pd.Series, period: int = 20) -> pd.Series:
    """Weighted Moving Average"""
    weights = np.arange(1, period + 1, dtype=float)
    return close.rolling(window=period).apply(lambda x: np.dot(x, weights) / weights.sum(), raw=True)


def hma(close: pd.Series, period: int = 20) -> pd.Series:
    """Hull Moving Average"""
    half = int(period / 2)
    sqrt_p = int(np.sqrt(period))
    wmaf = wma(close, half)
    wmas = wma(close, period)
    diff = 2 * wmaf - wmas
    return wma(diff, sqrt_p)


# ============================================================================
# Momentum Indicators
# ============================================================================

def rsi(close: pd.Series, period: int = 14) -> pd.Series:
    """Relative Strength Index"""
    delta = close.diff()
    gain = delta.where(delta > 0, 0.0).rolling(window=period).mean()
    loss = (-delta.where(delta < 0, 0.0)).rolling(window=period).mean()
    rs = gain / loss.replace(0, np.nan)
    return 100 - (100 / (1 + rs))


def stochastic(high: pd.Series, low: pd.Series, close: pd.Series,
               k_period: int = 14, d_period: int = 3) -> Dict[str, pd.Series]:
    """Stochastic Oscillator - returns %K and %D"""
    lowest = low.rolling(window=k_period).min()
    highest = high.rolling(window=k_period).max()
    denom = (highest - lowest).replace(0, np.nan)
    k = ((close - lowest) / denom) * 100
    d = k.rolling(window=d_period).mean()
    return {'k': k, 'd': d}


def stochrsi(close: pd.Series, rsi_period: int = 14, stoch_period: int = 14) -> pd.Series:
    """Stochastic RSI"""
    r = rsi(close, rsi_period)
    lowest = r.rolling(window=stoch_period).min()
    highest = r.rolling(window=stoch_period).max()
    denom = (highest - lowest).replace(0, np.nan)
    return ((r - lowest) / denom) * 100


def macd(close: pd.Series, fast: int = 12, slow: int = 26,
         signal: int = 9) -> Dict[str, pd.Series]:
    """MACD - returns macd line, signal line, histogram"""
    fast_ema = ema(close, fast)
    slow_ema = ema(close, slow)
    macd_line = fast_ema - slow_ema
    signal_line = ema(macd_line, signal)
    histogram = macd_line - signal_line
    return {'macd': macd_line, 'signal': signal_line, 'histogram': histogram}


def momentum(close: pd.Series, period: int = 10) -> pd.Series:
    """Momentum (Rate of Change)"""
    return close.pct_change(periods=period) * 100


def roc(close: pd.Series, period: int = 10) -> pd.Series:
    """Rate of Change"""
    return ((close - close.shift(period)) / close.shift(period).replace(0, np.nan)) * 100


def williams_r(high: pd.Series, low: pd.Series, close: pd.Series,
               period: int = 14) -> pd.Series:
    """Williams %R"""
    highest = high.rolling(window=period).max()
    lowest = low.rolling(window=period).min()
    denom = (highest - lowest).replace(0, np.nan)
    return -100 * (highest - close) / denom


def cci(high: pd.Series, low: pd.Series, close: pd.Series,
        period: int = 20) -> pd.Series:
    """Commodity Channel Index"""
    tp = (high + low + close) / 3.0
    sma_tp = tp.rolling(window=period).mean()
    mad = tp.rolling(window=period).apply(lambda x: np.abs(x - x.mean()).mean(), raw=True)
    return (tp - sma_tp) / (0.015 * mad.replace(0, np.nan))


def tsi(close: pd.Series, long_period: int = 25, short_period: int = 13) -> pd.Series:
    """True Strength Index"""
    diff = close.diff()
    double_smoothed = diff.ewm(span=long_period, adjust=False).mean().ewm(span=short_period, adjust=False).mean()
    double_smoothed_abs = diff.abs().ewm(span=long_period, adjust=False).mean().ewm(span=short_period, adjust=False).mean()
    return 100 * double_smoothed / double_smoothed_abs.replace(0, np.nan)


def uo(high: pd.Series, low: pd.Series, close: pd.Series,
       s: int = 7, m: int = 14, l: int = 28) -> pd.Series:
    """Ultimate Oscillator"""
    prev_close = close.shift(1)
    bp = close - pd.concat([low, prev_close], axis=1).min(axis=1)
    tr = pd.concat([high, prev_close], axis=1).max(axis=1) - pd.concat([low, prev_close], axis=1).min(axis=1)
    avg_s = bp.rolling(s).sum() / tr.rolling(s).sum().replace(0, np.nan)
    avg_m = bp.rolling(m).sum() / tr.rolling(m).sum().replace(0, np.nan)
    avg_l = bp.rolling(l).sum() / tr.rolling(l).sum().replace(0, np.nan)
    return 100 * (4 * avg_s + 2 * avg_m + avg_l) / 7


# ============================================================================
# Volatility Indicators
# ============================================================================

def atr(high: pd.Series, low: pd.Series, close: pd.Series,
        period: int = 14) -> pd.Series:
    """Average True Range"""
    prev_close = close.shift(1)
    tr1 = high - low
    tr2 = (high - prev_close).abs()
    tr3 = (low - prev_close).abs()
    tr = pd.concat([tr1, tr2, tr3], axis=1).max(axis=1)
    return tr.rolling(window=period).mean()


def true_range(high: pd.Series, low: pd.Series, close: pd.Series) -> pd.Series:
    """True Range"""
    prev_close = close.shift(1)
    tr1 = high - low
    tr2 = (high - prev_close).abs()
    tr3 = (low - prev_close).abs()
    return pd.concat([tr1, tr2, tr3], axis=1).max(axis=1)


def bollinger_bands(close: pd.Series, period: int = 20,
                    std_dev: float = 2.0) -> Dict[str, pd.Series]:
    """Bollinger Bands - returns upper, middle, lower, bandwidth, percent_b"""
    middle = sma(close, period)
    std = close.rolling(window=period).std()
    upper = middle + std_dev * std
    lower = middle - std_dev * std
    bandwidth = ((upper - lower) / middle.replace(0, np.nan)) * 100
    percent_b = (close - lower) / (upper - lower).replace(0, np.nan)
    return {
        'upper': upper, 'middle': middle, 'lower': lower,
        'bandwidth': bandwidth, 'percent_b': percent_b
    }


def keltner_channel(high: pd.Series, low: pd.Series, close: pd.Series,
                    ema_period: int = 20, atr_period: int = 10,
                    multiplier: float = 2.0) -> Dict[str, pd.Series]:
    """Keltner Channel - returns upper, middle, lower"""
    middle = ema(close, ema_period)
    a = atr(high, low, close, atr_period)
    upper = middle + multiplier * a
    lower = middle - multiplier * a
    return {'upper': upper, 'middle': middle, 'lower': lower}


def donchian_channel(high: pd.Series, low: pd.Series,
                     period: int = 20) -> Dict[str, pd.Series]:
    """Donchian Channel - returns upper, lower, middle"""
    upper = high.rolling(window=period).max()
    lower = low.rolling(window=period).min()
    middle = (upper + lower) / 2
    return {'upper': upper, 'lower': lower, 'middle': middle}


def moving_std(close: pd.Series, period: int = 20) -> pd.Series:
    """Moving Standard Deviation"""
    return close.rolling(window=period).std()


def zscore(close: pd.Series, period: int = 20) -> pd.Series:
    """Z-Score"""
    mean = close.rolling(window=period).mean()
    std = close.rolling(window=period).std().replace(0, np.nan)
    return (close - mean) / std


# ============================================================================
# Volume Indicators
# ============================================================================

def obv(close: pd.Series, volume: pd.Series) -> pd.Series:
    """On-Balance Volume"""
    direction = np.sign(close.diff())
    direction.iloc[0] = 0
    return (volume * direction).cumsum()


def vwap(high: pd.Series, low: pd.Series, close: pd.Series,
         volume: pd.Series) -> pd.Series:
    """Volume Weighted Average Price"""
    tp = (high + low + close) / 3.0
    return (tp * volume).cumsum() / volume.cumsum().replace(0, np.nan)


def mfi(high: pd.Series, low: pd.Series, close: pd.Series,
        volume: pd.Series, period: int = 14) -> pd.Series:
    """Money Flow Index"""
    tp = (high + low + close) / 3.0
    rmf = tp * volume
    positive_flow = rmf.where(tp > tp.shift(1), 0.0).rolling(period).sum()
    negative_flow = rmf.where(tp < tp.shift(1), 0.0).rolling(period).sum()
    ratio = positive_flow / negative_flow.replace(0, np.nan)
    return 100 - (100 / (1 + ratio))


def chaikin_mf(high: pd.Series, low: pd.Series, close: pd.Series,
               volume: pd.Series, period: int = 20) -> pd.Series:
    """Chaikin Money Flow"""
    hl_range = (high - low).replace(0, np.nan)
    clv = ((close - low) - (high - close)) / hl_range
    return (clv * volume).rolling(period).sum() / volume.rolling(period).sum().replace(0, np.nan)


def adl(high: pd.Series, low: pd.Series, close: pd.Series,
        volume: pd.Series) -> pd.Series:
    """Accumulation/Distribution Line"""
    hl_range = (high - low).replace(0, np.nan)
    clv = ((close - low) - (high - close)) / hl_range
    return (clv * volume).cumsum()


# ============================================================================
# Trend Strength Indicators
# ============================================================================

def adx(high: pd.Series, low: pd.Series, close: pd.Series,
        period: int = 14) -> Dict[str, pd.Series]:
    """Average Directional Index - returns adx, plus_di, minus_di"""
    prev_high = high.shift(1)
    prev_low = low.shift(1)
    prev_close = close.shift(1)

    tr = pd.concat([
        high - low,
        (high - prev_close).abs(),
        (low - prev_close).abs()
    ], axis=1).max(axis=1)

    plus_dm = (high - prev_high).where((high - prev_high) > (prev_low - low), 0.0).clip(lower=0)
    minus_dm = (prev_low - low).where((prev_low - low) > (high - prev_high), 0.0).clip(lower=0)

    atr_val = tr.ewm(span=period, adjust=False).mean()
    plus_di = 100 * (plus_dm.ewm(span=period, adjust=False).mean() / atr_val.replace(0, np.nan))
    minus_di = 100 * (minus_dm.ewm(span=period, adjust=False).mean() / atr_val.replace(0, np.nan))

    dx = (abs(plus_di - minus_di) / (plus_di + minus_di).replace(0, np.nan)) * 100
    adx_val = dx.ewm(span=period, adjust=False).mean()

    return {'adx': adx_val, 'plus_di': plus_di, 'minus_di': minus_di}


def ichimoku(high: pd.Series, low: pd.Series,
             tenkan: int = 9, kijun: int = 26,
             senkou_b: int = 52) -> Dict[str, pd.Series]:
    """Ichimoku Cloud - returns tenkan_sen, kijun_sen, senkou_a, senkou_b, chikou"""
    tenkan_sen = (high.rolling(tenkan).max() + low.rolling(tenkan).min()) / 2
    kijun_sen = (high.rolling(kijun).max() + low.rolling(kijun).min()) / 2
    senkou_a = ((tenkan_sen + kijun_sen) / 2).shift(kijun)
    senkou_b_val = ((high.rolling(senkou_b).max() + low.rolling(senkou_b).min()) / 2).shift(kijun)
    chikou = high.shift(-kijun)
    return {
        'tenkan_sen': tenkan_sen, 'kijun_sen': kijun_sen,
        'senkou_a': senkou_a, 'senkou_b': senkou_b_val, 'chikou': chikou
    }


def psar(high: pd.Series, low: pd.Series,
         af_step: float = 0.02, af_max: float = 0.2) -> pd.Series:
    """Parabolic SAR"""
    length = len(high)
    psar_vals = np.zeros(length)
    af = af_step
    bull = True
    ep = low.iloc[0]
    hp = high.iloc[0]
    lp = low.iloc[0]
    psar_vals[0] = high.iloc[0]

    for i in range(1, length):
        if bull:
            psar_vals[i] = psar_vals[i - 1] + af * (hp - psar_vals[i - 1])
            psar_vals[i] = min(psar_vals[i], low.iloc[i - 1])
            if i >= 2:
                psar_vals[i] = min(psar_vals[i], low.iloc[i - 2])
            if low.iloc[i] < psar_vals[i]:
                bull = False
                psar_vals[i] = hp
                lp = low.iloc[i]
                af = af_step
            else:
                if high.iloc[i] > hp:
                    hp = high.iloc[i]
                    af = min(af + af_step, af_max)
        else:
            psar_vals[i] = psar_vals[i - 1] + af * (lp - psar_vals[i - 1])
            psar_vals[i] = max(psar_vals[i], high.iloc[i - 1])
            if i >= 2:
                psar_vals[i] = max(psar_vals[i], high.iloc[i - 2])
            if high.iloc[i] > psar_vals[i]:
                bull = True
                psar_vals[i] = lp
                hp = high.iloc[i]
                af = af_step
            else:
                if low.iloc[i] < lp:
                    lp = low.iloc[i]
                    af = min(af + af_step, af_max)

    return pd.Series(psar_vals, index=high.index)


# ============================================================================
# Indicator Catalog
# ============================================================================

INDICATOR_CATALOG = {
    # Trend
    'sma': {'label': 'SMA', 'category': 'Trend', 'params': ['period']},
    'ema': {'label': 'EMA', 'category': 'Trend', 'params': ['period']},
    'dema': {'label': 'DEMA', 'category': 'Trend', 'params': ['period']},
    'tema': {'label': 'TEMA', 'category': 'Trend', 'params': ['period']},
    'wma': {'label': 'WMA', 'category': 'Trend', 'params': ['period']},
    'hma': {'label': 'HMA', 'category': 'Trend', 'params': ['period']},
    'bbands': {'label': 'Bollinger Bands', 'category': 'Trend', 'params': ['period', 'std_dev']},
    'keltner': {'label': 'Keltner Channel', 'category': 'Trend', 'params': ['ema_period', 'atr_period', 'multiplier']},
    'donchian': {'label': 'Donchian Channel', 'category': 'Trend', 'params': ['period']},
    'ichimoku': {'label': 'Ichimoku Cloud', 'category': 'Trend', 'params': ['tenkan', 'kijun', 'senkou_b']},
    'psar': {'label': 'Parabolic SAR', 'category': 'Trend', 'params': ['af_step', 'af_max']},
    # Momentum
    'rsi': {'label': 'RSI', 'category': 'Momentum', 'params': ['period']},
    'stoch': {'label': 'Stochastic', 'category': 'Momentum', 'params': ['k_period', 'd_period']},
    'stochrsi': {'label': 'Stochastic RSI', 'category': 'Momentum', 'params': ['rsi_period', 'stoch_period']},
    'macd': {'label': 'MACD', 'category': 'Momentum', 'params': ['fast', 'slow', 'signal']},
    'momentum': {'label': 'Momentum', 'category': 'Momentum', 'params': ['period']},
    'roc': {'label': 'Rate of Change', 'category': 'Momentum', 'params': ['period']},
    'williams_r': {'label': 'Williams %R', 'category': 'Momentum', 'params': ['period']},
    'cci': {'label': 'CCI', 'category': 'Momentum', 'params': ['period']},
    'tsi': {'label': 'True Strength Index', 'category': 'Momentum', 'params': ['long_period', 'short_period']},
    'uo': {'label': 'Ultimate Oscillator', 'category': 'Momentum', 'params': ['s', 'm', 'l']},
    # Volatility
    'atr': {'label': 'ATR', 'category': 'Volatility', 'params': ['period']},
    'tr': {'label': 'True Range', 'category': 'Volatility', 'params': []},
    'mstd': {'label': 'Moving Std Dev', 'category': 'Volatility', 'params': ['period']},
    'zscore': {'label': 'Z-Score', 'category': 'Volatility', 'params': ['period']},
    # Volume
    'obv': {'label': 'On-Balance Volume', 'category': 'Volume', 'params': []},
    'vwap': {'label': 'VWAP', 'category': 'Volume', 'params': []},
    'mfi': {'label': 'Money Flow Index', 'category': 'Volume', 'params': ['period']},
    'cmf': {'label': 'Chaikin Money Flow', 'category': 'Volume', 'params': ['period']},
    'adl': {'label': 'Accumulation/Distribution', 'category': 'Volume', 'params': []},
    # Trend Strength
    'adx': {'label': 'ADX', 'category': 'Trend Strength', 'params': ['period']},
}


def calculate(indicator_type: str, data: pd.DataFrame,
              params: Dict[str, Any] = None) -> Dict[str, Any]:
    """
    Universal indicator calculator.

    Args:
        indicator_type: Indicator key from INDICATOR_CATALOG
        data: DataFrame with Open, High, Low, Close, Volume columns
        params: Indicator parameters

    Returns:
        Dict with 'values' (list of {date, value} or {date, ...multi-values})
    """
    params = params or {}
    close = data['Close']
    high = data.get('High', close)
    low = data.get('Low', close)
    volume = data.get('Volume', pd.Series(0, index=close.index))

    ind = indicator_type.lower()
    result_series = None
    result_multi = None

    # --- Trend ---
    if ind == 'sma':
        result_series = sma(close, params.get('period', 20))
    elif ind == 'ema':
        result_series = ema(close, params.get('period', 20))
    elif ind == 'dema':
        result_series = dema(close, params.get('period', 20))
    elif ind == 'tema':
        result_series = tema(close, params.get('period', 20))
    elif ind == 'wma':
        result_series = wma(close, params.get('period', 20))
    elif ind == 'hma':
        result_series = hma(close, params.get('period', 20))
    elif ind in ('bbands', 'bollinger_bands', 'bollinger'):
        result_multi = bollinger_bands(close, params.get('period', 20), params.get('std_dev', 2.0))
    elif ind in ('keltner', 'kc'):
        result_multi = keltner_channel(high, low, close,
                                       params.get('ema_period', 20),
                                       params.get('atr_period', 10),
                                       params.get('multiplier', 2.0))
    elif ind == 'donchian':
        result_multi = donchian_channel(high, low, params.get('period', 20))
    elif ind == 'ichimoku':
        result_multi = ichimoku(high, low,
                                params.get('tenkan', 9),
                                params.get('kijun', 26),
                                params.get('senkou_b', 52))
    elif ind == 'psar':
        result_series = psar(high, low, params.get('af_step', 0.02), params.get('af_max', 0.2))

    # --- Momentum ---
    elif ind == 'rsi':
        result_series = rsi(close, params.get('period', 14))
    elif ind in ('stoch', 'stochastic'):
        result_multi = stochastic(high, low, close,
                                  params.get('k_period', 14),
                                  params.get('d_period', 3))
    elif ind == 'stochrsi':
        result_series = stochrsi(close, params.get('rsi_period', 14), params.get('stoch_period', 14))
    elif ind == 'macd':
        result_multi = macd(close, params.get('fast', 12), params.get('slow', 26), params.get('signal', 9))
    elif ind in ('momentum', 'mom'):
        result_series = momentum(close, params.get('period', 10))
    elif ind == 'roc':
        result_series = roc(close, params.get('period', 10))
    elif ind == 'williams_r':
        result_series = williams_r(high, low, close, params.get('period', 14))
    elif ind == 'cci':
        result_series = cci(high, low, close, params.get('period', 20))
    elif ind == 'tsi':
        result_series = tsi(close, params.get('long_period', 25), params.get('short_period', 13))
    elif ind == 'uo':
        result_series = uo(high, low, close, params.get('s', 7), params.get('m', 14), params.get('l', 28))

    # --- Volatility ---
    elif ind == 'atr':
        result_series = atr(high, low, close, params.get('period', 14))
    elif ind == 'tr':
        result_series = true_range(high, low, close)
    elif ind in ('mstd', 'moving_std'):
        result_series = moving_std(close, params.get('period', 20))
    elif ind == 'zscore':
        result_series = zscore(close, params.get('period', 20))

    # --- Volume ---
    elif ind == 'obv':
        result_series = obv(close, volume)
    elif ind == 'vwap':
        result_series = vwap(high, low, close, volume)
    elif ind == 'mfi':
        result_series = mfi(high, low, close, volume, params.get('period', 14))
    elif ind in ('cmf', 'chaikin'):
        result_series = chaikin_mf(high, low, close, volume, params.get('period', 20))
    elif ind == 'adl':
        result_series = adl(high, low, close, volume)

    # --- Trend Strength ---
    elif ind == 'adx':
        result_multi = adx(high, low, close, params.get('period', 14))

    else:
        return {'success': False, 'error': f'Unknown indicator: {indicator_type}'}

    # Format output
    values = []
    if result_series is not None:
        for idx, val in result_series.items():
            if pd.notna(val):
                values.append({'date': str(idx), 'value': float(val)})
    elif result_multi is not None:
        for idx in data.index:
            point = {'date': str(idx)}
            all_nan = True
            for key, series in result_multi.items():
                v = series.get(idx, np.nan) if hasattr(series, 'get') else (series.loc[idx] if idx in series.index else np.nan)
                if pd.notna(v):
                    point[key] = float(v)
                    all_nan = False
                else:
                    point[key] = None
            if not all_nan:
                values.append(point)

    return {
        'success': True,
        'indicator': indicator_type,
        'values': values,
        'count': len(values)
    }


def get_catalog() -> List[Dict[str, Any]]:
    """Return full indicator catalog for frontend"""
    catalog = []
    for key, info in INDICATOR_CATALOG.items():
        catalog.append({
            'id': key,
            'label': info['label'],
            'category': info['category'],
            'params': info['params'],
        })
    return catalog
