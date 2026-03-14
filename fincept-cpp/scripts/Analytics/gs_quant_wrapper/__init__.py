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
from .datetime_utils import (
    DateTimeUtils,
    DateTimeConfig
)

# Timeseries - unified class
from .timeseries_analytics import (
    TimeseriesAnalytics,
    TimeseriesConfig
)

# Timeseries - sub-modules
from . import ts_math_statistics
from . import ts_returns_performance
from . import ts_risk_measures
from . import ts_technical_indicators
from . import ts_data_transforms
from . import ts_portfolio_analytics

# Instrument wrapper
from .instrument_wrapper import (
    InstrumentFactory,
    InstrumentConfig,
    EquitySpecs,
    BondSpecs,
    OptionSpecs,
    SwapSpecs
)

# Risk analytics
from .risk_analytics import (
    RiskAnalytics,
    RiskConfig,
    MarketShock
)

# Backtest analytics
from .backtest_analytics import (
    BacktestEngine,
    BacktestConfig,
    Trade,
    Position
)

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
