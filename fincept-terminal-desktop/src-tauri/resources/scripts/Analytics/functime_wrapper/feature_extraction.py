import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
from functime.feature_extraction import (
    add_calendar_effects, add_holiday_effects,
    make_future_calendar_effects, make_future_holiday_effects
)

def create_calendar_effects(
    y: pl.DataFrame,
    freq: str,
    country_codes: Optional[List[str]] = None
) -> Dict[str, Any]:
    """Add calendar effects (day of week, month, etc.)"""
    result = add_calendar_effects(y, freq=freq, country_codes=country_codes)

    return {
        'data': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }

def create_holiday_effects(
    y: pl.DataFrame,
    country_codes: List[str],
    freq: str = 'D'
) -> Dict[str, Any]:
    """Add holiday indicator features"""
    result = add_holiday_effects(y, country_codes=country_codes, freq=freq)

    return {
        'data': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }

def create_future_calendar_effects(
    fh: int,
    freq: str,
    time_col: str = 'time',
    country_codes: Optional[List[str]] = None
) -> Dict[str, Any]:
    """Create calendar effects for future dates"""
    result = make_future_calendar_effects(
        fh=fh,
        freq=freq,
        time_col=time_col,
        country_codes=country_codes
    )

    return {
        'data': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }

def create_future_holiday_effects(
    fh: int,
    country_codes: List[str],
    freq: str = 'D',
    time_col: str = 'time'
) -> Dict[str, Any]:
    """Create holiday effects for future dates"""
    result = make_future_holiday_effects(
        fh=fh,
        country_codes=country_codes,
        freq=freq,
        time_col=time_col
    )

    return {
        'data': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }

def main():
    print("Testing functime feature_extraction wrapper")

    # Create sample panel data
    df = pl.DataFrame({
        'entity_id': ['A'] * 7,
        'time': pl.datetime_range(
            start=pl.datetime(2020, 1, 1),
            end=pl.datetime(2020, 1, 7),
            interval='1d',
            eager=True
        ).to_list(),
        'value': [10.0, 15.0, 20.0, 18.0, 22.0, 25.0, 30.0]
    })

    # Test calendar effects
    calendar_result = create_calendar_effects(df, freq='1d')
    print("Calendar effects columns: {}".format(len(calendar_result['columns'])))

    # Test future calendar effects
    future_calendar = create_future_calendar_effects(fh=5, freq='1d')
    print("Future calendar shape: {}".format(future_calendar['shape']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
