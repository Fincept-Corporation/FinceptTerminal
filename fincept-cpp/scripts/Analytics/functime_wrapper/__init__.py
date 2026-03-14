"""
functime Wrapper for Fincept Terminal

Complete wrapper for functime library covering:
- Forecasting models (Linear, Lasso, Ridge, ElasticNet, KNN, LightGBM + Auto versions)
- Feature extraction (Calendar and holiday effects)
- Preprocessing (Box-Cox, differencing, scaling, imputation, lags, rolling)
- Cross-validation (Train/test split, expanding/sliding windows)
- Metrics (MAE, MAPE, MASE, MSE, RMSE, RMSSE, SMAPE, overforecast, underforecast)
- Offsets (Frequency to seasonal period conversion)
"""

from .forecasting import (
    fit_linear_model,
    forecast_linear_model,
    fit_lasso,
    forecast_lasso,
    fit_ridge,
    forecast_ridge,
    fit_elasticnet,
    forecast_elasticnet,
    fit_knn,
    forecast_knn,
    fit_lightgbm,
    forecast_lightgbm,
    auto_linear_model,
    auto_lasso,
    auto_ridge,
    auto_elasticnet,
    auto_knn,
    auto_lightgbm
)

from .feature_extraction import (
    create_calendar_effects,
    create_holiday_effects,
    create_future_calendar_effects,
    create_future_holiday_effects
)

from .preprocessing import (
    apply_boxcox,
    find_boxcox_normmax,
    coerce_data_types,
    difference_data,
    impute_missing,
    create_lags,
    reindex_panel_data,
    resample_data,
    create_rolling_features,
    scale_data,
    zero_pad_data
)

from .cross_validation import (
    split_train_test,
    create_expanding_window_splits,
    create_sliding_window_splits
)

from .metrics import (
    calculate_mae,
    calculate_mape,
    calculate_mase,
    calculate_mse,
    calculate_rmse,
    calculate_rmsse,
    calculate_smape,
    calculate_overforecast,
    calculate_underforecast
)

from .offsets import (
    frequency_to_seasonal_period
)

__all__ = [
    # Forecasting models
    'fit_linear_model',
    'forecast_linear_model',
    'fit_lasso',
    'forecast_lasso',
    'fit_ridge',
    'forecast_ridge',
    'fit_elasticnet',
    'forecast_elasticnet',
    'fit_knn',
    'forecast_knn',
    'fit_lightgbm',
    'forecast_lightgbm',
    # Auto models
    'auto_linear_model',
    'auto_lasso',
    'auto_ridge',
    'auto_elasticnet',
    'auto_knn',
    'auto_lightgbm',
    # Feature extraction
    'create_calendar_effects',
    'create_holiday_effects',
    'create_future_calendar_effects',
    'create_future_holiday_effects',
    # Preprocessing
    'apply_boxcox',
    'find_boxcox_normmax',
    'coerce_data_types',
    'difference_data',
    'impute_missing',
    'create_lags',
    'reindex_panel_data',
    'resample_data',
    'create_rolling_features',
    'scale_data',
    'zero_pad_data',
    # Cross validation
    'split_train_test',
    'create_expanding_window_splits',
    'create_sliding_window_splits',
    # Metrics
    'calculate_mae',
    'calculate_mape',
    'calculate_mase',
    'calculate_mse',
    'calculate_rmse',
    'calculate_rmsse',
    'calculate_smape',
    'calculate_overforecast',
    'calculate_underforecast',
    # Offsets
    'frequency_to_seasonal_period'
]
