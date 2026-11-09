"""
GS-Quant Wrapper Library
========================

Comprehensive Python wrapper for Goldman Sachs' gs-quant library providing
FREE offline functionality for quantitative finance.

Modules (all work offline without GS API):
- ts_math_statistics: Math operations, logical ops & statistics (40 functions)
- ts_returns_performance: Returns & performance (11 functions)
- ts_risk_measures: Risk, volatility, VaR & swap measures (19 functions)
- ts_technical_indicators: Technical indicators (10 functions)
- ts_data_transforms: Data transforms & utilities (39 functions)
- ts_portfolio_analytics: Portfolio analytics, baskets & simulation (38 functions)
- risk_analytics: Risk management & Greeks (81 functions)

Total Timeseries Coverage: 157 FREE functions (of 216 free in gs_quant)
CLI: gs_quant_service.py (the AI Quant Lab "GS Quant" panel's backend).

Note: All functions work offline with your own data. No GS API required.
"""

# Timeseries - sub-modules
from . import ts_math_statistics
from . import ts_returns_performance
from . import ts_risk_measures
from . import ts_technical_indicators
from . import ts_data_transforms
from . import ts_portfolio_analytics

__version__ = '1.3.0'
__author__ = 'Fincept Corporation'

__all__ = [
    # Timeseries - sub-modules
    'ts_math_statistics',
    'ts_returns_performance',
    'ts_risk_measures',
    'ts_technical_indicators',
    'ts_data_transforms',
    'ts_portfolio_analytics',

    # Risk
    'RiskAnalytics',
    'RiskConfig',
    'MarketShock',
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "RiskAnalytics": ("risk_analytics", "RiskAnalytics"),
    "RiskConfig": ("risk_analytics", "RiskConfig"),
    "MarketShock": ("risk_analytics", "MarketShock"),
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
