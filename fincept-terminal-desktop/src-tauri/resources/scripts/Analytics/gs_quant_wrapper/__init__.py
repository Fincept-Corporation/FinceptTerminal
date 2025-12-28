"""
GS-Quant Wrapper Library
========================

Comprehensive Python wrapper for Goldman Sachs' gs-quant library providing
983 functions across 6 major modules for quantitative finance.

Modules:
- datetime_utils: Date/time calculations (21 functions)
- timeseries_analytics: Time series analysis (338 functions)
- instrument_wrapper: Financial instruments (365+ classes)
- risk_analytics: Risk management (81 functions)
- markets_wrapper: Market data access (21 functions)
- backtest_analytics: Strategy backtesting (10 functions)

Total Coverage: 836+ functions and classes

Usage:
    from gs_quant_wrapper import TimeseriesAnalytics, InstrumentFactory, RiskAnalytics

    # Time series analysis
    ts = TimeseriesAnalytics()
    returns = ts.calculate_returns(prices)
    sharpe = ts.calculate_sharpe_ratio(returns)

    # Instrument creation
    factory = InstrumentFactory()
    option = factory.create_equity_option('AAPL', 150, '2025-12-19', 'Call')

    # Risk analytics
    risk = RiskAnalytics()
    greeks = risk.calculate_all_greeks('Call', 100, 100, 0.25, 0.20)
    var = risk.calculate_parametric_var(1_000_000, returns)

Authentication:
    Most functions work offline without GS API authentication.
    Real market data and pricing require GS API credentials.
"""

# DateTime utilities
from .datetime_utils import (
    DateTimeUtils,
    DateTimeConfig
)

# Timeseries analytics
from .timeseries_analytics import (
    TimeseriesAnalytics,
    TimeseriesConfig
)

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

# Markets wrapper
from .markets_wrapper import (
    MarketsManager,
    MarketConfig,
    MarketDataRequest,
    PricingScenario
)

# Backtest analytics
from .backtest_analytics import (
    BacktestEngine,
    BacktestConfig,
    Trade,
    Position
)

__version__ = '1.0.0'
__author__ = 'Fincept Corporation'

__all__ = [
    # DateTime
    'DateTimeUtils',
    'DateTimeConfig',

    # Timeseries
    'TimeseriesAnalytics',
    'TimeseriesConfig',

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

    # Markets
    'MarketsManager',
    'MarketConfig',
    'MarketDataRequest',
    'PricingScenario',

    # Backtesting
    'BacktestEngine',
    'BacktestConfig',
    'Trade',
    'Position',
]
