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


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "fit_auto_arima": ("arima", "fit_auto_arima"),
    "fit_arima": ("arima", "fit_arima"),
    "forecast_auto_arima": ("arima", "forecast_auto_arima"),
    "forecast_arima": ("arima", "forecast_arima"),
    "update_arima": ("arima", "update_arima"),
    "apply_boxcox_transform": ("preprocessing", "apply_boxcox_transform"),
    "inverse_boxcox_transform": ("preprocessing", "inverse_boxcox_transform"),
    "apply_log_transform": ("preprocessing", "apply_log_transform"),
    "inverse_log_transform": ("preprocessing", "inverse_log_transform"),
    "create_date_features": ("preprocessing", "create_date_features"),
    "create_fourier_features": ("preprocessing", "create_fourier_features"),
    "split_train_test": ("model_selection", "split_train_test"),
    "cross_validate_arima": ("model_selection", "cross_validate_arima"),
    "rolling_forecast_cv": ("model_selection", "rolling_forecast_cv"),
    "sliding_window_cv": ("model_selection", "sliding_window_cv"),
    "calculate_acf": ("utils", "calculate_acf"),
    "calculate_pacf": ("utils", "calculate_pacf"),
    "decompose_timeseries": ("utils", "decompose_timeseries"),
    "difference_series": ("utils", "difference_series"),
    "inverse_difference": ("utils", "inverse_difference"),
    "smape_metric": ("utils", "smape_metric"),
    "check_endogenous": ("utils", "check_endogenous"),
    "create_c_array": ("utils", "create_c_array"),
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
