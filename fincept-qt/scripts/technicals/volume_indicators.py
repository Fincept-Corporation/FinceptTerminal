"""
Volume Indicators Module
Provides all volume-based technical indicators from the ta library
"""

import pandas as pd
from ta.volume import (
    AccDistIndexIndicator,
    OnBalanceVolumeIndicator,
    ChaikinMoneyFlowIndicator,
    ForceIndexIndicator,
    EaseOfMovementIndicator,
    VolumePriceTrendIndicator,
    NegativeVolumeIndexIndicator,
    VolumeWeightedAveragePrice,
    MFIIndicator,
)


def calculate_adi(df, fillna=False):
    """
    Calculate Accumulation/Distribution Index (ADI)

    Args:
        df: DataFrame with 'high', 'low', 'close', 'volume' columns
        fillna: Fill NaN values (default: False)

    Returns:
        Series with ADI values
    """
    indicator = AccDistIndexIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        volume=df['volume'],
        fillna=fillna
    )
    return indicator.acc_dist_index()


def calculate_obv(df, fillna=False):
    """
    Calculate On-Balance Volume (OBV)

    Args:
        df: DataFrame with 'close', 'volume' columns
        fillna: Fill NaN values (default: False)

    Returns:
        Series with OBV values
    """
    indicator = OnBalanceVolumeIndicator(
        close=df['close'],
        volume=df['volume'],
        fillna=fillna
    )
    return indicator.on_balance_volume()


def calculate_cmf(df, window=20, fillna=False):
    """
    Calculate Chaikin Money Flow (CMF)

    Args:
        df: DataFrame with 'high', 'low', 'close', 'volume' columns
        window: Period for CMF calculation (default: 20)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with CMF values
    """
    indicator = ChaikinMoneyFlowIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        volume=df['volume'],
        window=window,
        fillna=fillna
    )
    return indicator.chaikin_money_flow()


def calculate_force_index(df, window=13, fillna=False):
    """
    Calculate Force Index (FI)

    Args:
        df: DataFrame with 'close', 'volume' columns
        window: Period for exponential smoothing (default: 13)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with Force Index values
    """
    indicator = ForceIndexIndicator(
        close=df['close'],
        volume=df['volume'],
        window=window,
        fillna=fillna
    )
    return indicator.force_index()


def calculate_eom(df, window=14, fillna=False):
    """
    Calculate Ease of Movement (EoM)

    Args:
        df: DataFrame with 'high', 'low', 'volume' columns
        window: Period for SMA (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Dict with 'eom' and 'eom_signal' Series
    """
    indicator = EaseOfMovementIndicator(
        high=df['high'],
        low=df['low'],
        volume=df['volume'],
        window=window,
        fillna=fillna
    )
    return {
        'eom': indicator.ease_of_movement(),
        'eom_signal': indicator.sma_ease_of_movement()
    }


def calculate_vpt(df, fillna=False):
    """
    Calculate Volume-Price Trend (VPT)

    Args:
        df: DataFrame with 'close', 'volume' columns
        fillna: Fill NaN values (default: False)

    Returns:
        Series with VPT values
    """
    indicator = VolumePriceTrendIndicator(
        close=df['close'],
        volume=df['volume'],
        fillna=fillna
    )
    return indicator.volume_price_trend()


def calculate_nvi(df, fillna=False):
    """
    Calculate Negative Volume Index (NVI)

    Args:
        df: DataFrame with 'close', 'volume' columns
        fillna: Fill NaN values (default: False)

    Returns:
        Series with NVI values
    """
    indicator = NegativeVolumeIndexIndicator(
        close=df['close'],
        volume=df['volume'],
        fillna=fillna
    )
    return indicator.negative_volume_index()


def calculate_vwap(df, window=14, fillna=False):
    """
    Calculate Volume Weighted Average Price (VWAP)

    Args:
        df: DataFrame with 'high', 'low', 'close', 'volume' columns
        window: Period for VWAP calculation (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with VWAP values
    """
    indicator = VolumeWeightedAveragePrice(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        volume=df['volume'],
        window=window,
        fillna=fillna
    )
    return indicator.volume_weighted_average_price()


def calculate_mfi(df, window=14, fillna=False):
    """
    Calculate Money Flow Index (MFI)

    Args:
        df: DataFrame with 'high', 'low', 'close', 'volume' columns
        window: Period for MFI calculation (default: 14)
        fillna: Fill NaN values (default: False)

    Returns:
        Series with MFI values
    """
    indicator = MFIIndicator(
        high=df['high'],
        low=df['low'],
        close=df['close'],
        volume=df['volume'],
        window=window,
        fillna=fillna
    )
    return indicator.money_flow_index()


def calculate_all_volume_indicators(df, **kwargs):
    """
    Calculate all volume indicators at once

    Args:
        df: DataFrame with required columns (high, low, close, volume)
        **kwargs: Optional parameters for individual indicators

    Returns:
        DataFrame with all volume indicators
    """
    result_df = df.copy()

    # ADI
    result_df['adi'] = calculate_adi(df, **kwargs.get('adi', {}))

    # OBV
    result_df['obv'] = calculate_obv(df, **kwargs.get('obv', {}))

    # CMF
    result_df['cmf'] = calculate_cmf(df, **kwargs.get('cmf', {}))

    # Force Index
    result_df['fi'] = calculate_force_index(df, **kwargs.get('force_index', {}))

    # Ease of Movement
    eom = calculate_eom(df, **kwargs.get('eom', {}))
    result_df['eom'] = eom['eom']
    result_df['eom_signal'] = eom['eom_signal']

    # VPT
    result_df['vpt'] = calculate_vpt(df, **kwargs.get('vpt', {}))

    # NVI
    result_df['nvi'] = calculate_nvi(df, **kwargs.get('nvi', {}))

    # VWAP
    result_df['vwap'] = calculate_vwap(df, **kwargs.get('vwap', {}))

    # MFI
    result_df['mfi'] = calculate_mfi(df, **kwargs.get('mfi', {}))

    return result_df
