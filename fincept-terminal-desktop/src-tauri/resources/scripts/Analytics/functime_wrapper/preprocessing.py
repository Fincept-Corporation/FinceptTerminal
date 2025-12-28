import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
from functime.preprocessing import (
    boxcox, boxcox_normmax, coerce_dtypes, diff, impute,
    lag, reindex_panel, resample, roll, scale, zero_pad
)

def apply_boxcox(
    y: pl.DataFrame,
    lmbda: Optional[float] = None
) -> Dict[str, Any]:
    """Apply Box-Cox transformation to panel data"""
    result = boxcox(y, lmbda=lmbda)

    return {
        'transformed': result.to_dicts(),
        'shape': result.shape
    }

def find_boxcox_normmax(
    y: pl.DataFrame,
    method: str = 'mle'
) -> Dict[str, Any]:
    """Find optimal Box-Cox lambda parameter"""
    lmbda = boxcox_normmax(y, method=method)

    return {
        'lambda': float(lmbda)
    }

def coerce_data_types(
    y: pl.DataFrame
) -> Dict[str, Any]:
    """Coerce DataFrame to functime-compatible dtypes"""
    result = coerce_dtypes(y)

    return {
        'data': result.to_dicts(),
        'dtypes': {col: str(dtype) for col, dtype in zip(result.columns, result.dtypes)}
    }

def difference_data(
    y: pl.DataFrame,
    order: int = 1
) -> Dict[str, Any]:
    """Difference panel data"""
    result = diff(y, order=order)

    return {
        'differenced': result.to_dicts(),
        'shape': result.shape
    }

def impute_missing(
    y: pl.DataFrame,
    method: str = 'forward'
) -> Dict[str, Any]:
    """Impute missing values in panel data"""
    result = impute(y, method=method)

    return {
        'imputed': result.to_dicts(),
        'shape': result.shape
    }

def create_lags(
    y: pl.DataFrame,
    lags: Union[int, List[int]]
) -> Dict[str, Any]:
    """Create lagged features"""
    result = lag(y, lags=lags)

    return {
        'lagged': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }

def reindex_panel_data(
    y: pl.DataFrame,
    freq: str
) -> Dict[str, Any]:
    """Reindex panel data to specified frequency"""
    result = reindex_panel(y, freq=freq)

    return {
        'reindexed': result.to_dicts(),
        'shape': result.shape
    }

def resample_data(
    y: pl.DataFrame,
    freq: str,
    agg_method: str = 'sum'
) -> Dict[str, Any]:
    """Resample time series to different frequency"""
    result = resample(y, freq=freq, agg_method=agg_method)

    return {
        'resampled': result.to_dicts(),
        'shape': result.shape
    }

def create_rolling_features(
    y: pl.DataFrame,
    window_sizes: Union[int, List[int]],
    stats: List[str] = ['mean']
) -> Dict[str, Any]:
    """Create rolling window features"""
    result = roll(y, window_sizes=window_sizes, stats=stats)

    return {
        'rolling': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }

def scale_data(
    y: pl.DataFrame,
    method: str = 'standard'
) -> Dict[str, Any]:
    """Scale panel data"""
    result = scale(y, method=method)

    return {
        'scaled': result.to_dicts(),
        'shape': result.shape
    }

def zero_pad_data(
    y: pl.DataFrame,
    pad_length: int
) -> Dict[str, Any]:
    """Zero-pad panel data"""
    result = zero_pad(y, pad_length=pad_length)

    return {
        'padded': result.to_dicts(),
        'shape': result.shape
    }

def main():
    print("Testing functime preprocessing wrapper")

    # Create sample panel data
    dates_a = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 1, 3),
        interval='1d',
        eager=True
    ).to_list()
    dates_b = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 1, 3),
        interval='1d',
        eager=True
    ).to_list()

    df = pl.DataFrame({
        'entity_id': ['A', 'A', 'A', 'B', 'B', 'B'],
        'time': dates_a + dates_b,
        'value': [10.0, 15.0, 20.0, 12.0, 18.0, 24.0]
    })

    # Test coerce_dtypes
    coerce_result = coerce_data_types(df)
    print("Coerced dtypes: {}".format(len(coerce_result['dtypes'])))

    # Test scale
    scale_result = scale_data(df, method='standard')
    print("Scaled data shape: {}".format(scale_result['shape']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
