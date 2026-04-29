
__all__ = [
    'forecast_feedforward', 'forecast_deepar', 'forecast_tft', 'forecast_wavenet',
    'forecast_dlinear', 'forecast_patchtst', 'forecast_tide', 'forecast_lagtst', 'forecast_deepnpts',
    'predict_seasonal_naive', 'predict_mean', 'predict_constant',
    'evaluate_forecasts'
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "forecast_feedforward": ("forecasters", "forecast_feedforward"),
    "forecast_deepar": ("forecasters", "forecast_deepar"),
    "forecast_tft": ("forecasters", "forecast_tft"),
    "forecast_wavenet": ("forecasters", "forecast_wavenet"),
    "forecast_dlinear": ("forecasters", "forecast_dlinear"),
    "forecast_patchtst": ("forecasters", "forecast_patchtst"),
    "forecast_tide": ("forecasters", "forecast_tide"),
    "forecast_lagtst": ("forecasters", "forecast_lagtst"),
    "forecast_deepnpts": ("forecasters", "forecast_deepnpts"),
    "predict_seasonal_naive": ("predictors", "predict_seasonal_naive"),
    "predict_mean": ("predictors", "predict_mean"),
    "predict_constant": ("predictors", "predict_constant"),
    "evaluate_forecasts": ("evaluation", "evaluate_forecasts"),
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
