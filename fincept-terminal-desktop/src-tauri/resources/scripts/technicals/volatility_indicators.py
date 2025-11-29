"""
Volatility Indicators Module
Provides all volatility-based technical indicators from the ta library
"""

import pandas as pd
from ta.volatility import (
    AverageTrueRange,
    BollingerBands,
    KeltnerChannel,
    DonchianChannel,
    UlcerIndex,
)


def calculate_atr(df, window=14, fillna=False):
    """
    Calculate Average True Range (ATR)

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window: Period for ATR calculation (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with ATR values
    """
    indicator = AverageTrueRange(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window=window,
        fillna=fillna
    )
    return indicator.average_true_range()


def calculate_bollinger_bands(df, window=20, window_dev=2, fillna=False):
    """
    Calculate Bollinger Bands

    Args:
        df: DataFrame with 'close' column
        window: Period for moving average (default: 20)
        window_dev: Standard deviation multiplier (default: 2)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'bb_mavg', 'bb_hband', 'bb_lband', 'bb_pband', 'bb_wband',
        'bb_hband_indicator', 'bb_lband_indicator' Series
    """
    indicator = BollingerBands(
        close=df['close'],
        window=window,
        window_dev=window_dev,
        fillna=fillna
    )
    return {
        'bb_mavg': indicator.bollinger_mavg(),
        'bb_hband': indicator.bollinger_hband(),
        'bb_lband': indicator.bollinger_lband(),
        'bb_pband': indicator.bollinger_pband(),
        'bb_wband': indicator.bollinger_wband(),
        'bb_hband_indicator': indicator.bollinger_hband_indicator(),
        'bb_lband_indicator': indicator.bollinger_lband_indicator()
    }


def calculate_keltner_channel(df, window=20, window_atr=10, fillna=False, original_version=True):
    """
    Calculate Keltner Channel

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window: Period for EMA (default: 20)
        window_atr: Period for ATR (default: 10)
        fillna: Fill NaN values (default: False)
        original_version: Use original version (default: True)

    Returns:
        Dict with 'kc_mavg', 'kc_hband', 'kc_lband', 'kc_pband', 'kc_wband',
        'kc_hband_indicator', 'kc_lband_indicator' Series
    """
    indicator = KeltnerChannel(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window=window,
        window_atr=window_atr,
        fillna=fillna,
        original_version=original_version
    )
    return {
        'kc_mavg': indicator.keltner_channel_mband(),
        'kc_hband': indicator.keltner_channel_hband(),
        'kc_lband': indicator.keltner_channel_lband(),
        'kc_pband': indicator.keltner_channel_pband(),
        'kc_wband': indicator.keltner_channel_wband(),
        'kc_hband_indicator': indicator.keltner_channel_hband_indicator(),
        'kc_lband_indicator': indicator.keltner_channel_lband_indicator()
    }


def calculate_donchian_channel(df, window=20, offset=0, fillna=False):
    """
    Calculate Donchian Channel

    Args:
        df: DataFrame with 'high', 'low', 'close' columns
        window: Period for channel (default: 20)
        offset: Offset period (default: 0)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'dc_hband', 'dc_lband', 'dc_mband', 'dc_pband', 'dc_wband' Series
    """
    indicator = DonchianChannel(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        window=window,
        offset=offset,
        fillna=fillna
    )
    return {
        'dc_hband': indicator.donchian_channel_hband(),
        'dc_lband': indicator.donchian_channel_lband(),
        'dc_mband': indicator.donchian_channel_mband(),
        'dc_pband': indicator.donchian_channel_pband(),
        'dc_wband': indicator.donchian_channel_wband()
    }


def calculate_ulcer_index(df, window=14, fillna=False):
    """
    Calculate Ulcer Index

    Args:
        df: DataFrame with 'close' column
        window: Period for Ulcer Index calculation (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with Ulcer Index values
    """
    indicator = UlcerIndex(
        close=df['close'],
        window=window,
        fillna=fillna
    )
    return indicator.ulcer_index()


def calculate_all_volatility_indicators(df, **kwargs):
    """
    Calculate all volatility indicators at once

    Args:
        df: DataFrame with required columns (high, low, close)
        **kwargs: Optional parameters for individual indicators

    Returns:
        DataFrame with all volatility indicators
    """
    result_df = df.copy()

    # ATR
    result_df['atr'] = calculate_atr(df, **kwargs.get('atr', {}))

    # Bollinger Bands
    bb = calculate_bollinger_bands(df, **kwargs.get('bollinger_bands', {}))
    result_df['bb_mavg'] = bb['bb_mavg']
    result_df['bb_hband'] = bb['bb_hband']
    result_df['bb_lband'] = bb['bb_lband']
    result_df['bb_pband'] = bb['bb_pband']
    result_df['bb_wband'] = bb['bb_wband']
    result_df['bb_hband_indicator'] = bb['bb_hband_indicator']
    result_df['bb_lband_indicator'] = bb['bb_lband_indicator']

    # Keltner Channel
    kc = calculate_keltner_channel(df, **kwargs.get('keltner_channel', {}))
    result_df['kc_mavg'] = kc['kc_mavg']
    result_df['kc_hband'] = kc['kc_hband']
    result_df['kc_lband'] = kc['kc_lband']
    result_df['kc_pband'] = kc['kc_pband']
    result_df['kc_wband'] = kc['kc_wband']
    result_df['kc_hband_indicator'] = kc['kc_hband_indicator']
    result_df['kc_lband_indicator'] = kc['kc_lband_indicator']

    # Donchian Channel
    dc = calculate_donchian_channel(df, **kwargs.get('donchian_channel', {}))
    result_df['dc_hband'] = dc['dc_hband']
    result_df['dc_lband'] = dc['dc_lband']
    result_df['dc_mband'] = dc['dc_mband']
    result_df['dc_pband'] = dc['dc_pband']
    result_df['dc_wband'] = dc['dc_wband']

    # Ulcer Index
    result_df['ui'] = calculate_ulcer_index(df, **kwargs.get('ulcer_index', {}))

    return result_df
