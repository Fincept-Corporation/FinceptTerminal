# pmdarima Wrapper

Comprehensive Python wrapper for the pmdarima library, providing automatic ARIMA modeling, forecasting, and time series analysis tools.

## Overview

This wrapper provides complete coverage of pmdarima with 28 functions organized into 4 modules:

- **ARIMA Models**: AutoARIMA, ARIMA fitting and forecasting (5 functions)
- **Preprocessing**: Box-Cox, Log transforms, Date/Fourier features (6 functions)
- **Model Selection**: Train/test split, cross-validation (4 functions)
- **Utils**: ACF, PACF, decomposition, differencing, metrics (9 functions)

## Installation

```bash
pip install pmdarima==2.1.1
```

## Module Structure

```
pmdarima_wrapper/
├── __init__.py              # Main exports
├── arima.py                 # ARIMA/AutoARIMA models (5 functions)
├── preprocessing.py         # Data transformations (6 functions)
├── model_selection.py       # Cross-validation (4 functions)
├── utils.py                 # Utility functions (9 functions)
└── README.md                # This file
```

## Quick Start

### AutoARIMA - Automatic Parameter Selection

```python
from pmdarima_wrapper import fit_auto_arima, forecast_auto_arima

y = [10, 15, 13, 18, 22, 20, 25, 28, 30, 32]

result = fit_auto_arima(y, seasonal=False)
print(f"Best order: {result['order']}, AIC: {result['aic']:.2f}")

forecast = forecast_auto_arima(y, n_periods=5)
print(f"Forecast: {forecast['forecast']}")
print(f"Confidence intervals: [{forecast['conf_int_lower']}, {forecast['conf_int_upper']}]")
```

### ARIMA Forecasting

```python
from pmdarima_wrapper import fit_arima, forecast_arima

result = forecast_arima(y, order=(1, 1, 1), n_periods=5)
print(f"Forecast: {result['forecast']}")
```

### Preprocessing - Box-Cox Transform

```python
from pmdarima_wrapper import apply_boxcox_transform, inverse_boxcox_transform

transform_result = apply_boxcox_transform(y)
print(f"Lambda: {transform_result['lambda']}")
print(f"Transformed: {transform_result['transformed']}")

inv_result = inverse_boxcox_transform(transform_result['transformed'], transform_result['lambda'])
print(f"Original: {inv_result['original']}")
```

### ACF and PACF

```python
from pmdarima_wrapper import calculate_acf, calculate_pacf

acf = calculate_acf(y, nlags=20)
print(f"ACF: {acf['acf']}")

pacf = calculate_pacf(y, nlags=20)
print(f"PACF: {pacf['pacf']}")
```

### Time Series Decomposition

```python
from pmdarima_wrapper import decompose_timeseries

decomp = decompose_timeseries(y, type='additive', m=12)
print(f"Trend: {decomp['trend']}")
print(f"Seasonal: {decomp['seasonal']}")
print(f"Residual: {decomp['resid']}")
```

### Cross-Validation

```python
from pmdarima_wrapper import split_train_test, cross_validate_arima

split = split_train_test(y, test_size=0.2)
print(f"Train: {split['train_size']}, Test: {split['test_size']}")

cv_result = cross_validate_arima(y, order=(1, 1, 1), cv_splits=3)
print(f"Mean CV score: {cv_result['mean_score']:.4f}")
```

## Function Reference

### ARIMA Models (arima.py)

| Function | Description |
|----------|-------------|
| `fit_auto_arima` | Automatic ARIMA parameter selection |
| `fit_arima` | Fit ARIMA with specified parameters |
| `forecast_auto_arima` | AutoARIMA with forecasting |
| `forecast_arima` | ARIMA forecasting |
| `update_arima` | Update model with new data |

### Preprocessing (preprocessing.py)

| Function | Description |
|----------|-------------|
| `apply_boxcox_transform` | Box-Cox transformation |
| `inverse_boxcox_transform` | Inverse Box-Cox |
| `apply_log_transform` | Logarithmic transformation |
| `inverse_log_transform` | Inverse log transform |
| `create_date_features` | Extract date features (day, month, year, etc.) |
| `create_fourier_features` | Fourier terms for seasonality |

### Model Selection (model_selection.py)

| Function | Description |
|----------|-------------|
| `split_train_test` | Train/test split for time series |
| `cross_validate_arima` | Cross-validate ARIMA model |
| `rolling_forecast_cv` | Rolling window cross-validation |
| `sliding_window_cv` | Sliding window cross-validation |

### Utils (utils.py)

| Function | Description |
|----------|-------------|
| `calculate_acf` | Autocorrelation function |
| `calculate_pacf` | Partial autocorrelation function |
| `decompose_timeseries` | Trend/seasonal decomposition |
| `difference_series` | Difference time series |
| `inverse_difference` | Inverse differencing |
| `smape_metric` | Symmetric MAPE metric |
| `check_endogenous` | Validate endogenous variable |
| `create_c_array` | Create array (R-style) |

## Key Features

- **AutoARIMA**: Automatically finds best (p,d,q) parameters
- **Seasonality**: Supports seasonal ARIMA models
- **Exogenous Variables**: Include external regressors
- **Transformations**: Stabilize variance with Box-Cox/Log
- **Feature Engineering**: Date and Fourier features
- **Model Validation**: Rolling and sliding window CV
- **Metrics**: SMAPE for forecast evaluation

## Testing

```bash
python arima.py
python preprocessing.py
python model_selection.py
python utils.py
```

## Version

- **pmdarima**: 2.1.1
- **Wrapper Version**: 1.0.0
- **Total Functions**: 28
- **Coverage**: Complete
- **Last Updated**: 2026-01-23

## License

MIT License - Same as Fincept Terminal
