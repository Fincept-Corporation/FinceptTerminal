# functime Wrapper

Comprehensive Python wrapper for the functime library, providing machine learning forecasting models and time series utilities built on Polars for blazing-fast performance.

## Overview

This wrapper provides complete coverage of functime with 40 functions organized into 6 modules:

- **Forecasting**: 18 forecasting models (Linear, Lasso, Ridge, ElasticNet, KNN, LightGBM + Auto versions)
- **Feature Extraction**: 4 calendar and holiday feature functions
- **Preprocessing**: 11 data transformation functions
- **Cross Validation**: 3 train/test split methods
- **Metrics**: 9 forecast accuracy metrics
- **Offsets**: 1 frequency conversion utility

## Installation

```bash
pip install functime==0.1.10
```

## Module Structure

```
functime_wrapper/
├── __init__.py              # Main exports
├── forecasting.py           # ML forecasting models (18 functions)
├── feature_extraction.py    # Calendar/holiday features (4 functions)
├── preprocessing.py         # Data transformations (11 functions)
├── cross_validation.py      # CV splits (3 functions)
├── metrics.py               # Accuracy metrics (9 functions)
├── offsets.py               # Frequency utilities (1 function)
└── README.md                # This file
```

## Quick Start

### Forecasting - Linear Models

```python
from functime_wrapper import forecast_linear_model, forecast_lasso, forecast_ridge
import polars as pl

# Create panel data
df = pl.DataFrame({
    'entity_id': ['A'] * 10,
    'time': pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 1, 10),
        interval='1d',
        eager=True
    ).to_list(),
    'value': [10.0, 12.0, 15.0, 14.0, 18.0, 20.0, 22.0, 21.0, 25.0, 28.0]
})

# Linear Model
linear_forecast = forecast_linear_model(df, fh=3, freq='1d')
print(f"Linear forecast: {linear_forecast['forecast']}")

# Lasso (L1 regularization)
lasso_forecast = forecast_lasso(df, fh=3, freq='1d', alpha=0.1)
print(f"Lasso forecast: {lasso_forecast['forecast']}")

# Ridge (L2 regularization)
ridge_forecast = forecast_ridge(df, fh=3, freq='1d', alpha=0.1)
print(f"Ridge forecast: {ridge_forecast['forecast']}")
```

### Forecasting - Auto Models (with hyperparameter tuning)

```python
from functime_wrapper import auto_lasso, auto_ridge, auto_elasticnet

# Auto-tune Lasso
auto_result = auto_lasso(df, fh=3, freq='1d')
print(f"Best params: {auto_result['best_params']}")
print(f"Forecast: {auto_result['forecast']}")

# Auto-tune Ridge
ridge_result = auto_ridge(df, fh=3, freq='1d')
print(f"Forecast: {ridge_result['forecast']}")

# Auto-tune ElasticNet
elastic_result = auto_elasticnet(df, fh=3, freq='1d')
print(f"Forecast: {elastic_result['forecast']}")
```

### Preprocessing

```python
from functime_wrapper import (
    apply_boxcox, scale_data, difference_data,
    create_lags, create_rolling_features
)

# Box-Cox transformation
boxcox_result = apply_boxcox(df, lmbda=0.5)
print(f"Transformed: {boxcox_result['transformed']}")

# Scaling
scaled = scale_data(df, method='standard')
print(f"Scaled: {scaled['scaled']}")

# Differencing
diff_result = difference_data(df, order=1)
print(f"Differenced: {diff_result['differenced']}")

# Lags
lags_result = create_lags(df, lags=[1, 2, 3])
print(f"Lagged features: {lags_result['columns']}")

# Rolling features
rolling_result = create_rolling_features(df, window_sizes=[3, 7], stats=['mean', 'std'])
print(f"Rolling features: {rolling_result['columns']}")
```

### Feature Extraction

```python
from functime_wrapper import (
    create_calendar_effects,
    create_holiday_effects,
    create_future_calendar_effects
)

# Calendar effects (day of week, month, etc.)
calendar = create_calendar_effects(df, freq='1d')
print(f"Calendar features: {calendar['columns']}")

# Holiday effects
holidays = create_holiday_effects(df, country_codes=['US'], freq='D')
print(f"Holiday features: {holidays['columns']}")

# Future calendar effects
future_calendar = create_future_calendar_effects(fh=7, freq='1d')
print(f"Future calendar: {future_calendar['data']}")
```

### Cross Validation

```python
from functime_wrapper import (
    split_train_test,
    create_expanding_window_splits,
    create_sliding_window_splits
)

# Simple train/test split
split = split_train_test(df, test_size=2)
print(f"Train: {split['train_shape']}, Test: {split['test_shape']}")

# Expanding window CV
expanding = create_expanding_window_splits(df, test_size=1, n_splits=3)
print(f"Number of splits: {expanding['n_splits']}")

# Sliding window CV
sliding = create_sliding_window_splits(df, train_size=5, test_size=1, n_splits=3)
print(f"Number of splits: {sliding['n_splits']}")
```

### Metrics

```python
from functime_wrapper import (
    calculate_mae, calculate_rmse, calculate_smape,
    calculate_mase, calculate_overforecast
)

y_true = df  # Actual values
y_pred = df  # Predicted values (with same structure)

# MAE
mae = calculate_mae(y_true, y_pred)
print(f"MAE: {mae['mean_mae']}")

# RMSE
rmse = calculate_rmse(y_true, y_pred)
print(f"RMSE: {rmse['mean_rmse']}")

# SMAPE
smape = calculate_smape(y_true, y_pred)
print(f"SMAPE: {smape['mean_smape']}")

# MASE (requires training data)
y_train = df
mase = calculate_mase(y_true, y_pred, y_train, sp=1)
print(f"MASE: {mase['mean_mase']}")

# Overforecast percentage
over = calculate_overforecast(y_true, y_pred)
print(f"Overforecast: {over['mean_overforecast']}")
```

## Function Reference

### Forecasting (forecasting.py)

| Function | Description |
|----------|-------------|
| `fit_linear_model` | Fit Linear Regression forecaster |
| `forecast_linear_model` | Fit and forecast with Linear Regression |
| `fit_lasso` | Fit Lasso (L1) forecaster |
| `forecast_lasso` | Fit and forecast with Lasso |
| `fit_ridge` | Fit Ridge (L2) forecaster |
| `forecast_ridge` | Fit and forecast with Ridge |
| `fit_elasticnet` | Fit ElasticNet forecaster |
| `forecast_elasticnet` | Fit and forecast with ElasticNet |
| `fit_knn` | Fit K-Nearest Neighbors forecaster |
| `forecast_knn` | Fit and forecast with KNN |
| `fit_lightgbm` | Fit LightGBM forecaster |
| `forecast_lightgbm` | Fit and forecast with LightGBM |
| `auto_linear_model` | Auto-tune Linear Model |
| `auto_lasso` | Auto-tune Lasso |
| `auto_ridge` | Auto-tune Ridge |
| `auto_elasticnet` | Auto-tune ElasticNet |
| `auto_knn` | Auto-tune KNN |
| `auto_lightgbm` | Auto-tune LightGBM |

### Feature Extraction (feature_extraction.py)

| Function | Description |
|----------|-------------|
| `create_calendar_effects` | Add calendar features (day, month, year, etc.) |
| `create_holiday_effects` | Add holiday indicators |
| `create_future_calendar_effects` | Create calendar features for future dates |
| `create_future_holiday_effects` | Create holiday features for future dates |

### Preprocessing (preprocessing.py)

| Function | Description |
|----------|-------------|
| `apply_boxcox` | Apply Box-Cox transformation |
| `find_boxcox_normmax` | Find optimal Box-Cox lambda |
| `coerce_data_types` | Coerce to functime dtypes |
| `difference_data` | Difference panel data |
| `impute_missing` | Impute missing values |
| `create_lags` | Create lagged features |
| `reindex_panel_data` | Reindex to specified frequency |
| `resample_data` | Resample to different frequency |
| `create_rolling_features` | Create rolling window features |
| `scale_data` | Scale panel data |
| `zero_pad_data` | Zero-pad panel data |

### Cross Validation (cross_validation.py)

| Function | Description |
|----------|-------------|
| `split_train_test` | Split into train and test |
| `create_expanding_window_splits` | Expanding window CV |
| `create_sliding_window_splits` | Sliding window CV |

### Metrics (metrics.py)

| Function | Description |
|----------|-------------|
| `calculate_mae` | Mean Absolute Error |
| `calculate_mape` | Mean Absolute Percentage Error |
| `calculate_mase` | Mean Absolute Scaled Error |
| `calculate_mse` | Mean Squared Error |
| `calculate_rmse` | Root Mean Squared Error |
| `calculate_rmsse` | Root Mean Squared Scaled Error |
| `calculate_smape` | Symmetric MAPE |
| `calculate_overforecast` | Overforecast percentage |
| `calculate_underforecast` | Underforecast percentage |

### Offsets (offsets.py)

| Function | Description |
|----------|-------------|
| `frequency_to_seasonal_period` | Convert frequency to seasonal period |

## Key Features

- **Polars-based**: Blazing fast performance using Polars DataFrames
- **Panel Data**: Native support for multi-entity time series
- **ML Forecasting**: 6 model types (Linear, Lasso, Ridge, ElasticNet, KNN, LightGBM)
- **Auto-tuning**: Automatic hyperparameter optimization
- **Feature Engineering**: Calendar, holiday, lag, and rolling features
- **Transformations**: Box-Cox, scaling, differencing, imputation
- **Cross-validation**: Expanding and sliding window methods
- **Comprehensive Metrics**: 9 forecast accuracy measures
- **Exogenous Variables**: Support for external regressors

## Testing

```bash
python forecasting.py
python preprocessing.py
python feature_extraction.py
python cross_validation.py
python metrics.py
python offsets.py
```

## Version

- **functime**: 0.1.10
- **Wrapper Version**: 1.0.0
- **Total Functions**: 40
- **Coverage**: Complete
- **Last Updated**: 2025-12-29

## Notes

- All functions work with Polars DataFrames (not Pandas)
- Panel data requires 'entity_id', 'time', and 'value' columns
- Frequencies: '1d' (daily), '1w' (weekly), '1mo' (monthly), '1q' (quarterly), '1y' (yearly)
- Auto models use FLAML for hyperparameter tuning

## License

MIT License - Same as Fincept Terminal
