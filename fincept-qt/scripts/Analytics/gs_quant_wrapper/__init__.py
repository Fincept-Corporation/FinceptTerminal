"""
GS-Quant Wrapper Library
========================

Comprehensive Python wrapper for Goldman Sachs' gs-quant library providing
FREE offline functionality for quantitative finance.

Modules (all work offline without GS API):
- datetime_utils: Date/time calculations (21 functions)
- timeseries_analytics: Unified timeseries interface (legacy class)
- ts_math_statistics: Math operations, logical ops & statistics (40 functions)
- ts_returns_performance: Returns & performance (11 functions)
- ts_risk_measures: Risk, volatility, VaR & swap measures (19 functions)
- ts_technical_indicators: Technical indicators (10 functions)
- ts_data_transforms: Data transforms & utilities (39 functions)
- ts_portfolio_analytics: Portfolio analytics, baskets & simulation (38 functions)
- instrument_wrapper: Financial instrument definitions (365+ classes)
- risk_analytics: Risk management & Greeks (81 functions)
- backtest_analytics: Strategy backtesting (10 functions)

Total Timeseries Coverage: 157 FREE functions (of 216 free in gs_quant)
Total Coverage: 634+ functions and classes

Note: All functions work offline with your own data. No GS API required.
"""

# DateTime utilities

# Timeseries - unified class

# Timeseries - sub-modules
from . import ts_math_statistics
from . import ts_returns_performance
from . import ts_risk_measures
from . import ts_technical_indicators
from . import ts_data_transforms
from . import ts_portfolio_analytics

# Instrument wrapper

# Risk analytics

# Backtest analytics

__version__ = '1.3.0'
__author__ = 'Fincept Corporation'

__all__ = [
    # DateTime
    'DateTimeUtils',
    'DateTimeConfig',

    # Timeseries - class
    'TimeseriesAnalytics',
    'TimeseriesConfig',

    # Timeseries - sub-modules
    'ts_math_statistics',
    'ts_returns_performance',
    'ts_risk_measures',
    'ts_technical_indicators',
    'ts_data_transforms',
    'ts_portfolio_analytics',

    # Instruments
    'InstrumentFactory',
    'InstrumentConfig',
    'EquitySpecs',
    'BondSpecs',
    'OptionSpecs',
    'SwapSpecs',

    # Risk
    'RiskAnalytics',
    'RiskConfig',
    'MarketShock',

    # Backtesting
    'BacktestEngine',
    'BacktestConfig',
    'Trade',
    'Position',
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "DateTimeUtils": ("datetime_utils", "DateTimeUtils"),
    "DateTimeConfig": ("datetime_utils", "DateTimeConfig"),
    "TimeseriesAnalytics": ("timeseries_analytics", "TimeseriesAnalytics"),
    "TimeseriesConfig": ("timeseries_analytics", "TimeseriesConfig"),
    "InstrumentFactory": ("instrument_wrapper", "InstrumentFactory"),
    "InstrumentConfig": ("instrument_wrapper", "InstrumentConfig"),
    "EquitySpecs": ("instrument_wrapper", "EquitySpecs"),
    "BondSpecs": ("instrument_wrapper", "BondSpecs"),
    "OptionSpecs": ("instrument_wrapper", "OptionSpecs"),
    "SwapSpecs": ("instrument_wrapper", "SwapSpecs"),
    "RiskAnalytics": ("risk_analytics", "RiskAnalytics"),
    "RiskConfig": ("risk_analytics", "RiskConfig"),
    "MarketShock": ("risk_analytics", "MarketShock"),
    "BacktestEngine": ("backtest_analytics", "BacktestEngine"),
    "BacktestConfig": ("backtest_analytics", "BacktestConfig"),
    "Trade": ("backtest_analytics", "Trade"),
    "Position": ("backtest_analytics", "Position"),
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
