
__all__ = [
    'smooth_lowess', 'smooth_convolution', 'smooth_spectral',
    'smooth_polynomial', 'smooth_spline', 'smooth_gaussian',
    'smooth_exponential', 'smooth_kalman', 'smooth_binner', 'smooth_decompose',
    'get_sigma_intervals', 'get_confidence_intervals',
    'get_prediction_intervals', 'detect_outliers_sigma'
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "smooth_lowess": ("smoothers", "smooth_lowess"),
    "smooth_convolution": ("smoothers", "smooth_convolution"),
    "smooth_spectral": ("smoothers", "smooth_spectral"),
    "smooth_polynomial": ("smoothers", "smooth_polynomial"),
    "smooth_spline": ("smoothers", "smooth_spline"),
    "smooth_gaussian": ("smoothers", "smooth_gaussian"),
    "smooth_exponential": ("smoothers", "smooth_exponential"),
    "smooth_kalman": ("smoothers", "smooth_kalman"),
    "smooth_binner": ("smoothers", "smooth_binner"),
    "smooth_decompose": ("smoothers", "smooth_decompose"),
    "get_sigma_intervals": ("intervals", "get_sigma_intervals"),
    "get_confidence_intervals": ("intervals", "get_confidence_intervals"),
    "get_prediction_intervals": ("intervals", "get_prediction_intervals"),
    "detect_outliers_sigma": ("intervals", "detect_outliers_sigma"),
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
