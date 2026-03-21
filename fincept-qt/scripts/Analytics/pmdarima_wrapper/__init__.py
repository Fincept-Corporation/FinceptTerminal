"""
pmdarima Wrapper for Fincept Terminal

Complete wrapper for pmdarima library covering:
- AutoARIMA for automatic parameter selection
- ARIMA modeling and forecasting
- Preprocessing (Box-Cox, Log transforms, Date/Fourier features)
- Cross-validation (Rolling, Sliding Window)
- Utility functions (ACF, PACF, Decomposition, Differencing)
- Metrics (SMAPE)
"""

from .arima import (
    fit_auto_arima,
    fit_arima,
    forecast_auto_arima,
    forecast_arima,
    update_arima
)

from .preprocessing import (
    apply_boxcox_transform,
    inverse_boxcox_transform,
    apply_log_transform,
    inverse_log_transform,
    create_date_features,
    create_fourier_features
)

from .model_selection import (
    split_train_test,
    cross_validate_arima,
    rolling_forecast_cv,
    sliding_window_cv
)

from .utils import (
    calculate_acf,
    calculate_pacf,
    decompose_timeseries,
    difference_series,
    inverse_difference,
    smape_metric,
    check_endogenous,
    create_c_array
)

__all__ = [
    # ARIMA models
    'fit_auto_arima',
    'fit_arima',
    'forecast_auto_arima',
    'forecast_arima',
    'update_arima',
    # Preprocessing
    'apply_boxcox_transform',
    'inverse_boxcox_transform',
    'apply_log_transform',
    'inverse_log_transform',
    'create_date_features',
    'create_fourier_features',
    # Model selection
    'split_train_test',
    'cross_validate_arima',
    'rolling_forecast_cv',
    'sliding_window_cv',
    # Utils
    'calculate_acf',
    'calculate_pacf',
    'decompose_timeseries',
    'difference_series',
    'inverse_difference',
    'smape_metric',
    'check_endogenous',
    'create_c_array'
]
