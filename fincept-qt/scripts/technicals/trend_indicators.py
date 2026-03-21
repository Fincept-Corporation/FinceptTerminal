"""
Trend Indicators Module
Provides all trend-based technical indicators from the ta library
"""

import pandas as pd
from ta.trend import (
    SMAIndicator,
    EMAIndicator,
    WMAIndicator,
    MACD,
    TRIXIndicator,
    MassIndex,
    IchimokuIndicator,
    KSTIndicator,
    DPOIndicator,
    CCIIndicator,
    ADXIndicator,
    VortexIndicator,
    PSARIndicator,
    STCIndicator,
    AroonIndicator,
)


def calculate_sma(df, window=20, fillna=False):
    """
    Calculate Simple Moving Average (SMA)

    Args:
        df: DataFrame with 'close' column
        window: Period for SMA calculation (default: 20)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with SMA values
    """
    indicator = SMAIndicator(close=df['close'], window=window, fillna=fillna)
    return indicator.sma_indicator()


def calculate_ema(df, window=12, fillna=False):
    """
    Calculate Exponential Moving Average (EMA)

    Args:
        df: DataFrame with 'close' column
        window: Period for EMA calculation (default: 12)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with EMA values
    """
    indicator = EMAIndicator(close=df['close'], window=window, fillna=fillna)
    return indicator.ema_indicator()


def calculate_wma(df, window=9, fillna=False):
    """
    Calculate Weighted Moving Average (WMA)

    Args:
        df: DataFrame with 'close' column
        window: Period for WMA calculation (default: 9)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with WMA values
    """
    indicator = WMAIndicator(close=df['close'], window=window, fillna=fillna)
    return indicator.wma()


def calculate_macd(df, window_slow=26, window_fast=12, window_sign=9, fillna=False):
    """
    Calculate Moving Average Convergence Divergence (MACD)

    Args:
        df: DataFrame with 'close' column
        window_slow: Slow period (default: 26)
        window_fast: Fast period (default: 12)
        window_sign: Signal period (default: 9)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'macd', 'macd_signal', and 'macd_diff' Series
    """
    indicator = MACD(
        close=df['close'],
        window_slow=window_slow,
        window_fast=window_fast,
        window_sign=window_sign,
        fillna=fillna
    )
    return {
        'macd': indicator.macd(),
        'macd_signal': indicator.macd_signal(),
        'macd_diff': indicator.macd_diff()
    }


def calculate_trix(df, window=15, fillna=False):
    """
    Calculate Trix (TRIX)

    Args:
        df: DataFrame with 'close' column
        window: Period for triple EMA (default: 15)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with TRIX values
    """
    indicator = TRIXIndicator(close=df['close'], window=window, fillna=fillna)
    return indicator.trix()


def calculate_mass_index(df, window_fast=9, window_slow=25, fillna=False):
    """
    Calculate Mass Index (MI)

    Args:
        df: DataFrame with 'high', 'low' columns
        window_fast: Fast period (default: 9)
        window_slow: Slow period (default: 25)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with Mass Index values
    """
    indicator = MassIndex(
        high=df['high'],
        low=df['low'],
        window_fast=window_fast,
        window_slow=window_slow,
        fillna=fillna
    )
    return indicator.mass_index()


def calculate_ichimoku(df, window1=9, window2=26, window3=52, visual=False, fillna=False):
    """
    Calculate Ichimoku Kinko Hyo

    Args:
        df: DataFrame with 'high', 'low' columns
        window1: Tenkan period (default: 9)
        window2: Kijun period (default: 26)
        window3: Senkou period (default: 52)
        visual: Visual mode (default: False)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'ichimoku_conversion', 'ichimoku_base', 'ichimoku_a', 'ichimoku_b' Series
    """
    indicator = IchimokuIndicator(
        high=df['high'],
        low=df['low'],
        window1=window1,
        window2=window2,
        window3=window3,
        visual=visual,
        fillna=fillna
    )
    return {
        'ichimoku_conversion': indicator.ichimoku_conversion_line(),
        'ichimoku_base': indicator.ichimoku_base_line(),
        'ichimoku_a': indicator.ichimoku_a(),
        'ichimoku_b': indicator.ichimoku_b()
    }


def calculate_kst(df, roc1=10, roc2=15, roc3=20, roc4=30, window1=10, window2=10, window3=10, window4=15, nsig=9, fillna=False):
    """
    Calculate KST Oscillator (Know Sure Thing)

    Args:
        df: DataFrame with 'close' column
        roc1-4: ROC periods (defaults: 10, 15, 20, 30)
        window1-4: SMA windows (defaults: 10, 10, 10, 15)
        nsig: Signal line period (default: 9)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'kst' and 'kst_signal' Series
    """
    indicator = KSTIndicator(
        close=df['close'],
        roc1=roc1, roc2=roc2, roc3=roc3, roc4=roc4,
        window1=window1, window2=window2, window3=window3, window4=window4,
        nsig=nsig,
        fillna=fillna
    )
    return {
        'kst': indicator.kst(),
        'kst_signal': indicator.kst_sig()
    }


def calculate_dpo(df, window=20, fillna=False):
    """
    Calculate Detrended Price Oscillator (DPO)

    Args:
        df: DataFrame with 'close' column
        window: Period for calculation (default: 20)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with DPO values
    """
    indicator = DPOIndicator(close=df['close'], window=window, fillna=fillna)
    return indicator.dpo()


def calculate_cci(df, window=20, constant=0.015, fillna=False):
    """
    Calculate Commodity Channel Index (CCI)

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window: Period for CCI calculation (default: 20)
        constant: Constant value (default: 0.015)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with CCI values
    """
    indicator = CCIIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window=window,
        constant=constant,
        fillna=fillna
    )
    return indicator.cci()


def calculate_adx(df, window=14, fillna=False):
    """
    Calculate Average Directional Movement Index (ADX)

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window: Period for ADX calculation (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'adx', 'adx_pos', and 'adx_neg' Series
    """
    indicator = ADXIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window=window,
        fillna=fillna
    )
    return {
        'adx': indicator.adx(),
        'adx_pos': indicator.adx_pos(),
        'adx_neg': indicator.adx_neg()
    }


def calculate_vortex(df, window=14, fillna=False):
    """
    Calculate Vortex Indicator (VI)

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window: Period for VI calculation (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'vortex_pos' and 'vortex_neg' Series
    """
    indicator = VortexIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window=window,
        fillna=fillna
    )
    return {
        'vortex_pos': indicator.vortex_indicator_pos(),
        'vortex_neg': indicator.vortex_indicator_neg()
    }


def calculate_psar(df, step=0.02, max_step=0.2, fillna=False):
    """
    Calculate Parabolic Stop and Reverse (PSAR)

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        step: Acceleration factor step (default: 0.02)
        max_step: Maximum acceleration factor (default: 0.2)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'psar', 'psar_up', 'psar_down', 'psar_up_indicator', 'psar_down_indicator' Series
    """
    indicator = PSARIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        step=step,
        max_step=max_step,
        fillna=fillna
    )
    return {
        'psar': indicator.psar(),
        'psar_up': indicator.psar_up(),
        'psar_down': indicator.psar_down(),
        'psar_up_indicator': indicator.psar_up_indicator(),
        'psar_down_indicator': indicator.psar_down_indicator()
    }


def calculate_stc(df, window_slow=50, window_fast=23, cycle=10, smooth1=3, smooth2=3, fillna=False):
    """
    Calculate Schaff Trend Cycle (STC)

    Args:
        df: DataFrame with 'close' column
        window_slow: Slow period (default: 50)
        window_fast: Fast period (default: 23)
        cycle: Cycle period (default: 10)
        smooth1: First smoothing (default: 3)
        smooth2: Second smoothing (default: 3)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with STC values
    """
    indicator = STCIndicator(
        close=df['close'],
        window_slow=window_slow,
        window_fast=window_fast,
        cycle=cycle,
        smooth1=smooth1,
        smooth2=smooth2,
        fillna=fillna
    )
    return indicator.stc()


def calculate_aroon(df, window=25, fillna=False):
    """
    Calculate Aroon Indicator

    Args:
        df: DataFrame with 'high', 'low' columns
        window: Period for Aroon calculation (default: 25)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'aroon_up', 'aroon_down', and 'aroon_indicator' Series
    """
    indicator = AroonIndicator(high=df['high'], low=df['low'], window=window, fillna=fillna)
    return {
        'aroon_up': indicator.aroon_up(),
        'aroon_down': indicator.aroon_down(),
        'aroon_indicator': indicator.aroon_indicator()
    }


def calculate_all_trend_indicators(df, **kwargs):
    """
    Calculate all trend indicators at once

    Args:
        df: DataFrame with required columns (high, low, close)
        **kwargs: Optional parameters for individual indicators

    Returns:
        DataFrame with all trend indicators
    """
    result_df = df.copy()

    # SMA
    result_df['sma_20'] = calculate_sma(df, **kwargs.get('sma', {}))

    # EMA
    result_df['ema_12'] = calculate_ema(df, **kwargs.get('ema', {}))

    # WMA
    result_df['wma_9'] = calculate_wma(df, **kwargs.get('wma', {}))

    # MACD
    macd = calculate_macd(df, **kwargs.get('macd', {}))
    result_df['macd'] = macd['macd']
    result_df['macd_signal'] = macd['macd_signal']
    result_df['macd_diff'] = macd['macd_diff']

    # TRIX
    result_df['trix'] = calculate_trix(df, **kwargs.get('trix', {}))

    # Mass Index
    result_df['mass_index'] = calculate_mass_index(df, **kwargs.get('mass_index', {}))

    # Ichimoku
    ichimoku = calculate_ichimoku(df, **kwargs.get('ichimoku', {}))
    result_df['ichimoku_conversion'] = ichimoku['ichimoku_conversion']
    result_df['ichimoku_base'] = ichimoku['ichimoku_base']
    result_df['ichimoku_a'] = ichimoku['ichimoku_a']
    result_df['ichimoku_b'] = ichimoku['ichimoku_b']

    # KST
    kst = calculate_kst(df, **kwargs.get('kst', {}))
    result_df['kst'] = kst['kst']
    result_df['kst_signal'] = kst['kst_signal']

    # DPO
    result_df['dpo'] = calculate_dpo(df, **kwargs.get('dpo', {}))

    # CCI
    result_df['cci'] = calculate_cci(df, **kwargs.get('cci', {}))

    # ADX
    adx = calculate_adx(df, **kwargs.get('adx', {}))
    result_df['adx'] = adx['adx']
    result_df['adx_pos'] = adx['adx_pos']
    result_df['adx_neg'] = adx['adx_neg']

    # Vortex
    vortex = calculate_vortex(df, **kwargs.get('vortex', {}))
    result_df['vortex_pos'] = vortex['vortex_pos']
    result_df['vortex_neg'] = vortex['vortex_neg']

    # PSAR
    psar = calculate_psar(df, **kwargs.get('psar', {}))
    result_df['psar'] = psar['psar']
    result_df['psar_up'] = psar['psar_up']
    result_df['psar_down'] = psar['psar_down']
    result_df['psar_up_indicator'] = psar['psar_up_indicator']
    result_df['psar_down_indicator'] = psar['psar_down_indicator']

    # STC
    result_df['stc'] = calculate_stc(df, **kwargs.get('stc', {}))

    # Aroon
    aroon = calculate_aroon(df, **kwargs.get('aroon', {}))
    result_df['aroon_up'] = aroon['aroon_up']
    result_df['aroon_down'] = aroon['aroon_down']
    result_df['aroon_indicator'] = aroon['aroon_indicator']

    return result_df
