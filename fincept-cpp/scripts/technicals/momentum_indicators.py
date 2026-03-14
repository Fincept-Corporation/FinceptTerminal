"""
Momentum Indicators Module
Provides all momentum-based technical indicators from the ta library
"""

import pandas as pd
from ta.momentum import (
    RSIIndicator,
    StochasticOscillator,
    StochRSIIndicator,
    WilliamsRIndicator,
    AwesomeOscillatorIndicator,
    KAMAIndicator,
    ROCIndicator,
    TSIIndicator,
    UltimateOscillator,
    PercentagePriceOscillator,
    PercentageVolumeOscillator,
)


def calculate_rsi(df, window=14, fillna=False):
    """
    Calculate Relative Strength Index (RSI)

    Args:
        df: DataFrame with 'close' column
        window: Period for RSI calculation (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with RSI values
    """
    indicator = RSIIndicator(close=df['close'], window=window, fillna=fillna)
    return indicator.rsi()


def calculate_stochastic(df, window=14, smooth_window=3, fillna=False):
    """
    Calculate Stochastic Oscillator (%K and %D)

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window: Period for stochastic calculation (default: 14)
        smooth_window: Smoothing period for %D (default: 3)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'stoch_k' and 'stoch_d' Series
    """
    indicator = StochasticOscillator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window=window,
        smooth_window=smooth_window,
        fillna=fillna
    )
    return {
        'stoch_k': indicator.stoch(),
        'stoch_d': indicator.stoch_signal()
    }


def calculate_stoch_rsi(df, window=14, smooth1=3, smooth2=3, fillna=False):
    """
    Calculate Stochastic RSI

    Args:
        df: DataFrame with 'close' column
        window: RSI period (default: 14)
        smooth1: First smoothing period (default: 3)
        smooth2: Second smoothing period (default: 3)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'stoch_rsi', 'stoch_rsi_k', and 'stoch_rsi_d' Series
    """
    indicator = StochRSIIndicator(
        close=df['close'],
        window=window,
        smooth1=smooth1,
        smooth2=smooth2,
        fillna=fillna
    )
    return {
        'stoch_rsi': indicator.stochrsi(),
        'stoch_rsi_k': indicator.stochrsi_k(),
        'stoch_rsi_d': indicator.stochrsi_d()
    }


def calculate_williams_r(df, lbp=14, fillna=False):
    """
    Calculate Williams %R

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        lbp: Lookback period (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with Williams %R values
    """
    indicator = WilliamsRIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        lbp=lbp,
        fillna=fillna
    )
    return indicator.williams_r()


def calculate_awesome_oscillator(df, window1=5, window2=34, fillna=False):
    """
    Calculate Awesome Oscillator

    Args:
        df: DataFrame with 'high', 'low' columns
        window1: Short period (default: 5)
        window2: Long period (default: 34)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with Awesome Oscillator values
    """
    indicator = AwesomeOscillatorIndicator(
        high=df['high'],
        low=df['low'],
        window1=window1,
        window2=window2,
        fillna=fillna
    )
    return indicator.awesome_oscillator()


def calculate_kama(df, window=10, pow1=2, pow2=30, fillna=False):
    """
    Calculate Kaufman's Adaptive Moving Average (KAMA)

    Args:
        df: DataFrame with 'close' column
        window: Period for efficiency ratio (default: 10)
        pow1: Fast EMA constant (default: 2)
        pow2: Slow EMA constant (default: 30)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with KAMA values
    """
    indicator = KAMAIndicator(
        close=df['close'],
        window=window,
        pow1=pow1,
        pow2=pow2,
        fillna=fillna
    )
    return indicator.kama()


def calculate_roc(df, window=12, fillna=False):
    """
    Calculate Rate of Change (ROC)

    Args:
        df: DataFrame with 'close' column
        window: Period for ROC calculation (default: 12)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with ROC values
    """
    indicator = ROCIndicator(close=df['close'], window=window, fillna=fillna)
    return indicator.roc()


def calculate_tsi(df, window_slow=25, window_fast=13, fillna=False):
    """
    Calculate True Strength Index (TSI)

    Args:
        df: DataFrame with 'close' column
        window_slow: Slow period (default: 25)
        window_fast: Fast period (default: 13)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with TSI values
    """
    indicator = TSIIndicator(
        close=df['close'],
        window_slow=window_slow,
        window_fast=window_fast,
        fillna=fillna
    )
    return indicator.tsi()


def calculate_ultimate_oscillator(df, window1=7, window2=14, window3=28, weight1=4.0, weight2=2.0, weight3=1.0, fillna=False):
    """
    Calculate Ultimate Oscillator

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window1: Short period (default: 7)
        window2: Medium period (default: 14)
        window3: Long period (default: 28)
        weight1: Weight for short period (default: 4.0)
        weight2: Weight for medium period (default: 2.0)
        weight3: Weight for long period (default: 1.0)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with Ultimate Oscillator values
    """
    indicator = UltimateOscillator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window1=window1,
        window2=window2,
        window3=window3,
        weight1=weight1,
        weight2=weight2,
        weight3=weight3,
        fillna=fillna
    )
    return indicator.ultimate_oscillator()


def calculate_ppo(df, window_slow=26, window_fast=12, window_sign=9, fillna=False):
    """
    Calculate Percentage Price Oscillator (PPO)

    Args:
        df: DataFrame with 'close' column
        window_slow: Slow period (default: 26)
        window_fast: Fast period (default: 12)
        window_sign: Signal period (default: 9)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'ppo', 'ppo_signal', and 'ppo_hist' Series
    """
    indicator = PercentagePriceOscillator(
        close=df['close'],
        window_slow=window_slow,
        window_fast=window_fast,
        window_sign=window_sign,
        fillna=fillna
    )
    return {
        'ppo': indicator.ppo(),
        'ppo_signal': indicator.ppo_signal(),
        'ppo_hist': indicator.ppo_hist()
    }


def calculate_pvo(df, window_slow=26, window_fast=12, window_sign=9, fillna=False):
    """
    Calculate Percentage Volume Oscillator (PVO)

    Args:
        df: DataFrame with 'volume' column
        window_slow: Slow period (default: 26)
        window_fast: Fast period (default: 12)
        window_sign: Signal period (default: 9)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'pvo', 'pvo_signal', and 'pvo_hist' Series
    """
    indicator = PercentageVolumeOscillator(
        volume=df['volume'],
        window_slow=window_slow,
        window_fast=window_fast,
        window_sign=window_sign,
        fillna=fillna
    )
    return {
        'pvo': indicator.pvo(),
        'pvo_signal': indicator.pvo_signal(),
        'pvo_hist': indicator.pvo_hist()
    }


def calculate_all_momentum_indicators(df, **kwargs):
    """
    Calculate all momentum indicators at once

    Args:
        df: DataFrame with required columns (high, low, close, volume)
        **kwargs: Optional parameters for individual indicators

    Returns:
        DataFrame with all momentum indicators
    """
    result_df = df.copy()

    # RSI
    result_df['rsi'] = calculate_rsi(df, **kwargs.get('rsi', {}))

    # Stochastic Oscillator
    stoch = calculate_stochastic(df, **kwargs.get('stochastic', {}))
    result_df['stoch_k'] = stoch['stoch_k']
    result_df['stoch_d'] = stoch['stoch_d']

    # Stochastic RSI
    stoch_rsi = calculate_stoch_rsi(df, **kwargs.get('stoch_rsi', {}))
    result_df['stoch_rsi'] = stoch_rsi['stoch_rsi']
    result_df['stoch_rsi_k'] = stoch_rsi['stoch_rsi_k']
    result_df['stoch_rsi_d'] = stoch_rsi['stoch_rsi_d']

    # Williams %R
    result_df['williams_r'] = calculate_williams_r(df, **kwargs.get('williams_r', {}))

    # Awesome Oscillator
    result_df['ao'] = calculate_awesome_oscillator(df, **kwargs.get('awesome_oscillator', {}))

    # KAMA
    result_df['kama'] = calculate_kama(df, **kwargs.get('kama', {}))

    # ROC
    result_df['roc'] = calculate_roc(df, **kwargs.get('roc', {}))

    # TSI
    result_df['tsi'] = calculate_tsi(df, **kwargs.get('tsi', {}))

    # Ultimate Oscillator
    result_df['uo'] = calculate_ultimate_oscillator(df, **kwargs.get('ultimate_oscillator', {}))

    # PPO
    ppo = calculate_ppo(df, **kwargs.get('ppo', {}))
    result_df['ppo'] = ppo['ppo']
    result_df['ppo_signal'] = ppo['ppo_signal']
    result_df['ppo_hist'] = ppo['ppo_hist']

    # PVO
    if 'volume' in df.columns:
        pvo = calculate_pvo(df, **kwargs.get('pvo', {}))
        result_df['pvo'] = pvo['pvo']
        result_df['pvo_signal'] = pvo['pvo_signal']
        result_df['pvo_hist'] = pvo['pvo_hist']

    return result_df
