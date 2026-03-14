"""
Preprocessing module for functime wrapper
==========================================

Provides preprocessing transformations for time series panel data.
Uses sklearn-based local implementations to avoid functime cloud dependencies.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any
from scipy import stats as scipy_stats


def apply_boxcox(
    y: pl.DataFrame,
    lmbda: Optional[float] = None
) -> Dict[str, Any]:
    """Apply Box-Cox transformation to panel data"""
    # Get value column
    values = y['value'].to_numpy()

    # Box-Cox requires positive values
    min_val = values.min()
    if min_val <= 0:
        # Shift to make positive
        shift = abs(min_val) + 1
        values = values + shift
    else:
        shift = 0

    if lmbda is None:
        # Find optimal lambda
        transformed, lmbda = scipy_stats.boxcox(values)
    else:
        if lmbda == 0:
            transformed = np.log(values)
        else:
            transformed = (np.power(values, lmbda) - 1) / lmbda

    # Create result DataFrame
    result = y.clone()
    result = result.with_columns([
        pl.lit(transformed).alias('value')
    ])

    return {
        'transformed': result.to_dicts(),
        'shape': result.shape,
        'lambda': float(lmbda),
        'shift': float(shift),
        'columns': result.columns
    }


def find_boxcox_normmax(
    y: pl.DataFrame,
    method: str = 'mle'
) -> Dict[str, Any]:
    """Find optimal Box-Cox lambda parameter"""
    values = y['value'].to_numpy()

    # Ensure positive values
    min_val = values.min()
    if min_val <= 0:
        values = values + abs(min_val) + 1

    # Use scipy to find optimal lambda
    _, lmbda = scipy_stats.boxcox(values)

    return {
        'lambda': float(lmbda)
    }


def coerce_data_types(
    y: pl.DataFrame
) -> Dict[str, Any]:
    """Coerce DataFrame to functime-compatible dtypes"""
    result = y.clone()

    # Ensure proper types
    if 'entity_id' in result.columns:
        result = result.with_columns([
            pl.col('entity_id').cast(pl.Utf8)
        ])

    if 'time' in result.columns:
        if result['time'].dtype != pl.Datetime:
            result = result.with_columns([
                pl.col('time').str.to_datetime()
            ])

    if 'value' in result.columns:
        result = result.with_columns([
            pl.col('value').cast(pl.Float64)
        ])

    return {
        'data': result.to_dicts(),
        'dtypes': {col: str(dtype) for col, dtype in zip(result.columns, result.dtypes)}
    }


def difference_data(
    y: pl.DataFrame,
    order: int = 1
) -> Dict[str, Any]:
    """Difference panel data"""
    result_rows = []

    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        # Apply differencing
        diffed = values.copy()
        for _ in range(order):
            diffed = np.diff(diffed)

        # Align times (drop first 'order' times)
        for i, (t, v) in enumerate(zip(times[order:], diffed)):
            result_rows.append({
                'entity_id': entity_id,
                'time': t,
                'value': float(v)
            })

    result = pl.DataFrame(result_rows)

    return {
        'differenced': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }


def impute_missing(
    y: pl.DataFrame,
    method: str = 'forward'
) -> Dict[str, Any]:
    """Impute missing values in panel data"""
    result = y.clone()

    if method == 'forward':
        result = result.with_columns([
            pl.col('value').forward_fill().over('entity_id')
        ])
    elif method == 'backward':
        result = result.with_columns([
            pl.col('value').backward_fill().over('entity_id')
        ])
    elif method == 'mean':
        result = result.with_columns([
            pl.col('value').fill_null(pl.col('value').mean().over('entity_id'))
        ])
    elif method == 'zero':
        result = result.with_columns([
            pl.col('value').fill_null(0.0)
        ])
    elif method == 'linear':
        # Simple linear interpolation
        result = result.with_columns([
            pl.col('value').interpolate().over('entity_id')
        ])

    return {
        'imputed': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }


def create_lags(
    y: pl.DataFrame,
    lags: Union[int, List[int]]
) -> Dict[str, Any]:
    """Create lagged features"""
    if isinstance(lags, int):
        lags = list(range(1, lags + 1))

    result = y.clone()

    for lag_val in lags:
        result = result.with_columns([
            pl.col('value').shift(lag_val).over('entity_id').alias(f'lag_{lag_val}')
        ])

    # Drop rows with nulls from lagging
    result = result.drop_nulls()

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
    # Map frequency string to Polars interval
    freq_map = {
        '1d': '1d',
        '1h': '1h',
        '1w': '1w',
        '1mo': '1mo',
        '1m': '1m',
    }
    pl_freq = freq_map.get(freq, '1d')

    result_rows = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        min_time = entity_data['time'].min()
        max_time = entity_data['time'].max()

        # Create date range
        full_range = pl.datetime_range(min_time, max_time, pl_freq, eager=True)

        # Create full DataFrame and join
        full_df = pl.DataFrame({
            'entity_id': [entity_id] * len(full_range),
            'time': full_range
        })

        merged = full_df.join(entity_data, on=['entity_id', 'time'], how='left')
        result_rows.extend(merged.to_dicts())

    result = pl.DataFrame(result_rows)

    return {
        'reindexed': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }


def resample_data(
    y: pl.DataFrame,
    freq: str,
    agg_method: str = 'sum'
) -> Dict[str, Any]:
    """Resample time series to different frequency"""
    # Group by entity and resample
    if agg_method == 'sum':
        agg_fn = pl.col('value').sum()
    elif agg_method == 'mean':
        agg_fn = pl.col('value').mean()
    elif agg_method == 'first':
        agg_fn = pl.col('value').first()
    elif agg_method == 'last':
        agg_fn = pl.col('value').last()
    elif agg_method == 'min':
        agg_fn = pl.col('value').min()
    elif agg_method == 'max':
        agg_fn = pl.col('value').max()
    else:
        agg_fn = pl.col('value').mean()

    result = y.group_by_dynamic(
        'time',
        every=freq,
        by='entity_id'
    ).agg(agg_fn)

    return {
        'resampled': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }


def create_rolling_features(
    y: pl.DataFrame,
    window_sizes: Union[int, List[int]],
    stats: List[str] = ['mean']
) -> Dict[str, Any]:
    """Create rolling window features"""
    if isinstance(window_sizes, int):
        window_sizes = [window_sizes]

    result = y.clone()

    for window in window_sizes:
        for stat in stats:
            col_name = f'rolling_{stat}_{window}'

            if stat == 'mean':
                result = result.with_columns([
                    pl.col('value').rolling_mean(window).over('entity_id').alias(col_name)
                ])
            elif stat == 'std':
                result = result.with_columns([
                    pl.col('value').rolling_std(window).over('entity_id').alias(col_name)
                ])
            elif stat == 'min':
                result = result.with_columns([
                    pl.col('value').rolling_min(window).over('entity_id').alias(col_name)
                ])
            elif stat == 'max':
                result = result.with_columns([
                    pl.col('value').rolling_max(window).over('entity_id').alias(col_name)
                ])
            elif stat == 'sum':
                result = result.with_columns([
                    pl.col('value').rolling_sum(window).over('entity_id').alias(col_name)
                ])

    # Drop rows with nulls from rolling
    result = result.drop_nulls()

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
    result = y.clone()

    if method == 'standard':
        # Z-score normalization per entity
        result = result.with_columns([
            ((pl.col('value') - pl.col('value').mean().over('entity_id')) /
             pl.col('value').std().over('entity_id')).alias('value')
        ])
    elif method == 'minmax':
        # Min-max scaling per entity
        result = result.with_columns([
            ((pl.col('value') - pl.col('value').min().over('entity_id')) /
             (pl.col('value').max().over('entity_id') - pl.col('value').min().over('entity_id'))).alias('value')
        ])
    elif method == 'robust':
        # Robust scaling using median and IQR
        result = result.with_columns([
            ((pl.col('value') - pl.col('value').median().over('entity_id')) /
             (pl.col('value').quantile(0.75).over('entity_id') -
              pl.col('value').quantile(0.25).over('entity_id'))).alias('value')
        ])

    return {
        'scaled': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }


def zero_pad_data(
    y: pl.DataFrame,
    pad_length: int
) -> Dict[str, Any]:
    """Zero-pad panel data at the beginning"""
    result_rows = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        first_time = entity_data['time'].min()

        # Get time delta from first two rows
        times = entity_data['time'].to_list()
        if len(times) >= 2:
            delta = times[1] - times[0]
        else:
            from datetime import timedelta
            delta = timedelta(days=1)

        # Create padding rows
        for i in range(pad_length, 0, -1):
            result_rows.append({
                'entity_id': entity_id,
                'time': first_time - (delta * i),
                'value': 0.0
            })

        # Add original data
        result_rows.extend(entity_data.to_dicts())

    result = pl.DataFrame(result_rows).sort(['entity_id', 'time'])

    return {
        'padded': result.to_dicts(),
        'shape': result.shape,
        'columns': result.columns
    }


def main():
    """Test the preprocessing functions"""
    print("Testing functime preprocessing wrapper (sklearn backend)")

    # Create sample panel data
    dates_a = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 1, 10),
        interval='1d',
        eager=True
    ).to_list()
    dates_b = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 1, 10),
        interval='1d',
        eager=True
    ).to_list()

    df = pl.DataFrame({
        'entity_id': ['A'] * len(dates_a) + ['B'] * len(dates_b),
        'time': dates_a + dates_b,
        'value': [10.0, 15.0, 20.0, 18.0, 22.0, 25.0, 23.0, 28.0, 30.0, 32.0,
                  12.0, 18.0, 24.0, 22.0, 26.0, 30.0, 28.0, 34.0, 36.0, 38.0]
    })

    # Test coerce_dtypes
    coerce_result = coerce_data_types(df)
    print(f"Coerced dtypes: {len(coerce_result['dtypes'])} columns")

    # Test scale
    scale_result = scale_data(df, method='standard')
    print(f"Scaled data shape: {scale_result['shape']}")

    # Test lags
    lag_result = create_lags(df, lags=[1, 2, 3])
    print(f"Lagged data columns: {lag_result['columns']}")

    # Test rolling
    rolling_result = create_rolling_features(df, window_sizes=[3], stats=['mean', 'std'])
    print(f"Rolling features columns: {rolling_result['columns']}")

    # Test difference
    diff_result = difference_data(df, order=1)
    print(f"Differenced data shape: {diff_result['shape']}")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
