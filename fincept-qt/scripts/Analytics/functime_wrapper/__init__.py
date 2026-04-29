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


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "fit_linear_model": ("forecasting", "fit_linear_model"),
    "forecast_linear_model": ("forecasting", "forecast_linear_model"),
    "fit_lasso": ("forecasting", "fit_lasso"),
    "forecast_lasso": ("forecasting", "forecast_lasso"),
    "fit_ridge": ("forecasting", "fit_ridge"),
    "forecast_ridge": ("forecasting", "forecast_ridge"),
    "fit_elasticnet": ("forecasting", "fit_elasticnet"),
    "forecast_elasticnet": ("forecasting", "forecast_elasticnet"),
    "fit_knn": ("forecasting", "fit_knn"),
    "forecast_knn": ("forecasting", "forecast_knn"),
    "fit_lightgbm": ("forecasting", "fit_lightgbm"),
    "forecast_lightgbm": ("forecasting", "forecast_lightgbm"),
    "auto_linear_model": ("forecasting", "auto_linear_model"),
    "auto_lasso": ("forecasting", "auto_lasso"),
    "auto_ridge": ("forecasting", "auto_ridge"),
    "auto_elasticnet": ("forecasting", "auto_elasticnet"),
    "auto_knn": ("forecasting", "auto_knn"),
    "auto_lightgbm": ("forecasting", "auto_lightgbm"),
    "create_calendar_effects": ("feature_extraction", "create_calendar_effects"),
    "create_holiday_effects": ("feature_extraction", "create_holiday_effects"),
    "create_future_calendar_effects": ("feature_extraction", "create_future_calendar_effects"),
    "create_future_holiday_effects": ("feature_extraction", "create_future_holiday_effects"),
    "apply_boxcox": ("preprocessing", "apply_boxcox"),
    "find_boxcox_normmax": ("preprocessing", "find_boxcox_normmax"),
    "coerce_data_types": ("preprocessing", "coerce_data_types"),
    "difference_data": ("preprocessing", "difference_data"),
    "impute_missing": ("preprocessing", "impute_missing"),
    "create_lags": ("preprocessing", "create_lags"),
    "reindex_panel_data": ("preprocessing", "reindex_panel_data"),
    "resample_data": ("preprocessing", "resample_data"),
    "create_rolling_features": ("preprocessing", "create_rolling_features"),
    "scale_data": ("preprocessing", "scale_data"),
    "zero_pad_data": ("preprocessing", "zero_pad_data"),
    "split_train_test": ("cross_validation", "split_train_test"),
    "create_expanding_window_splits": ("cross_validation", "create_expanding_window_splits"),
    "create_sliding_window_splits": ("cross_validation", "create_sliding_window_splits"),
    "calculate_mae": ("metrics", "calculate_mae"),
    "calculate_mape": ("metrics", "calculate_mape"),
    "calculate_mase": ("metrics", "calculate_mase"),
    "calculate_mse": ("metrics", "calculate_mse"),
    "calculate_rmse": ("metrics", "calculate_rmse"),
    "calculate_rmsse": ("metrics", "calculate_rmsse"),
    "calculate_smape": ("metrics", "calculate_smape"),
    "calculate_overforecast": ("metrics", "calculate_overforecast"),
    "calculate_underforecast": ("metrics", "calculate_underforecast"),
    "frequency_to_seasonal_period": ("offsets", "frequency_to_seasonal_period"),
}


def __getattr__(name: str):  # PEP 562
    target = _LAZY_ATTRS.get(name)
    if target is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    submodule, original_name = target
    import importlib
    mod = importlib.import_module(f".{submodule}", __name__)
    value = getattr(mod, original_name)
    globals()[name] = value  # cache for subsequent access
    return value


def __dir__() -> list[str]:
    return sorted(set(globals()) | set(_LAZY_ATTRS))
