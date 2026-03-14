"""
Others Indicators Module
Provides miscellaneous technical indicators from the ta library
"""

import pandas as pd
from ta.others import (
    DailyReturnIndicator,
    DailyLogReturnIndicator,
    CumulativeReturnIndicator,
)


def calculate_daily_return(df, fillna=False):
    """
    Calculate Daily Return (DR)

    Args:
        df: DataFrame with 'close' column
        fillna: Fill NaN values (default: False)

    Returns:
        Series with daily return values
    """
    indicator = DailyReturnIndicator(close=df['close'], fillna=fillna)
    return indicator.daily_return()


def calculate_daily_log_return(df, fillna=False):
    """
    Calculate Daily Log Return (DLR)

    Args:
        df: DataFrame with 'close' column
        fillna: Fill NaN values (default: False)

    Returns:
        Series with daily log return values
    """
    indicator = DailyLogReturnIndicator(close=df['close'], fillna=fillna)
    return indicator.daily_log_return()


def calculate_cumulative_return(df, fillna=False):
    """
    Calculate Cumulative Return (CR)

    Args:
        df: DataFrame with 'close' column
        fillna: Fill NaN values (default: False)

    Returns:
        Series with cumulative return values
    """
    indicator = CumulativeReturnIndicator(close=df['close'], fillna=fillna)
    return indicator.cumulative_return()


def calculate_all_others_indicators(df, **kwargs):
    """
    Calculate all other indicators at once

    Args:
        df: DataFrame with required columns (close)
        **kwargs: Optional parameters for individual indicators

    Returns:
        DataFrame with all other indicators
    """
    result_df = df.copy()

    # Daily Return
    result_df['daily_return'] = calculate_daily_return(df, **kwargs.get('daily_return', {}))

    # Daily Log Return
    result_df['daily_log_return'] = calculate_daily_log_return(df, **kwargs.get('daily_log_return', {}))

    # Cumulative Return
    result_df['cumulative_return'] = calculate_cumulative_return(df, **kwargs.get('cumulative_return', {}))

    return result_df
