"""
Economic Init Module
=============================

Init analysis and calculation tools

===== DATA SOURCES REQUIRED =====
INPUT:
  - Macroeconomic time series data from official sources
  - Central bank policy statements and interest rate data
  - International trade and balance of payments statistics
  - Market indicators and sentiment measures
  - Demographic and structural economic data

OUTPUT:
  - Economic trend analysis and forecasts
  - Policy impact assessment and scenario modeling
  - Market cycle identification and timing analysis
  - Cross-country economic comparisons and rankings
  - Investment recommendations based on economic outlook

PARAMETERS:
  - forecast_horizon: Economic forecast horizon (default: 12 months)
  - confidence_level: Confidence level for predictions (default: 0.90)
  - base_currency: Base currency for analysis (default: 'USD')
  - seasonal_adjustment: Seasonal adjustment method (default: true)
  - lookback_period: Historical analysis period (default: 10 years)
"""

from .core import (
    EconomicsBase,
    DataValidator,
    EconomicsError,
    ValidationError,
    CalculationError,
    DataError
)

from .currency_analysis import (
    CurrencyAnalyzer,
    SpotForwardAnalyzer,
    ArbitrageDetector,
    ParityAnalyzer,
    CarryTradeAnalyzer
)

from .exchange_calculations import (
    ExchangeCalculator,
    CrossRateCalculator,
    ForwardCalculator
)

from .growth_analysis import (
    GrowthAnalyzer,
    ProductivityAnalyzer,
    ConvergenceAnalyzer,
    DemographicAnalyzer
)

from .market_cycles import (
    BusinessCycleAnalyzer,
    MarketStructureAnalyzer,
    CreditCycleAnalyzer
)

from .policy_analysis import (
    FiscalPolicyAnalyzer,
    MonetaryPolicyAnalyzer,
    CentralBankAnalyzer
)

from .trade_geopolitics import (
    TradeAnalyzer,
    GeopoliticalRiskAnalyzer,
    TradingBlocAnalyzer
)

from .capital_flows import (
    CapitalFlowAnalyzer,
    FXMarketAnalyzer,
    ExchangeRegimeAnalyzer
)

from .data_handler import (
    DataHandler,
    DataProvider,
    ManualDataInput
)

from .analytics_engine import (
    StatisticalAnalyzer,
    ForecastingEngine,
    ScenarioAnalyzer
)

from .reporting import (
    ReportGenerator,
    VisualizationEngine,
    ExportManager
)

from .config import (
    EconomicsConfig,
    DataSources,
    CalculationPrecision
)

__version__ = "1.0.0"
__author__ = "Fincept Corporation"
__email__ = "dev@fincept.com"

# Module metadata
__all__ = [
    # Core components
    'EconomicsBase', 'DataValidator', 'EconomicsError', 'ValidationError',
    'CalculationError', 'DataError',

    # Analysis modules
    'CurrencyAnalyzer', 'SpotForwardAnalyzer', 'ArbitrageDetector',
    'ParityAnalyzer', 'CarryTradeAnalyzer', 'ExchangeCalculator',
    'CrossRateCalculator', 'ForwardCalculator', 'GrowthAnalyzer',
    'ProductivityAnalyzer', 'ConvergenceAnalyzer', 'DemographicAnalyzer',
    'BusinessCycleAnalyzer', 'MarketStructureAnalyzer', 'CreditCycleAnalyzer',
    'FiscalPolicyAnalyzer', 'MonetaryPolicyAnalyzer', 'CentralBankAnalyzer',
    'TradeAnalyzer', 'GeopoliticalRiskAnalyzer', 'TradingBlocAnalyzer',
    'CapitalFlowAnalyzer', 'FXMarketAnalyzer', 'ExchangeRegimeAnalyzer',

    # Infrastructure
    'DataHandler', 'DataProvider', 'ManualDataInput',
    'StatisticalAnalyzer', 'ForecastingEngine', 'ScenarioAnalyzer',
    'ReportGenerator', 'VisualizationEngine', 'ExportManager',
    'EconomicsConfig', 'DataSources', 'CalculationPrecision'
]


# Quick access functions for common operations
def analyze_currency_arbitrage(currency_data, base_currency='USD'):
    """Quick triangular arbitrage analysis"""
    detector = ArbitrageDetector()
    return detector.detect_triangular_arbitrage(currency_data, base_currency)


def calculate_gdp_growth(gdp_data, method='solow'):
    """Quick GDP growth decomposition"""
    analyzer = GrowthAnalyzer()
    return analyzer.decompose_growth(gdp_data, method)


def assess_policy_impact(policy_data, policy_type='fiscal'):
    """Quick policy impact assessment"""
    if policy_type == 'fiscal':
        analyzer = FiscalPolicyAnalyzer()
    else:
        analyzer = MonetaryPolicyAnalyzer()
    return analyzer.assess_impact(policy_data)


def detect_business_cycle_phase(economic_indicators):
    """Quick business cycle phase detection"""
    analyzer = BusinessCycleAnalyzer()
    return analyzer.detect_phase(economic_indicators)


# Module configuration
DEFAULT_CONFIG = {
    'precision': 8,
    'base_currency': 'USD',
    'data_validation': True,
    'error_tolerance': 1e-6,
    'default_forecast_periods': 12,
    'confidence_interval': 0.95
}


def configure_module(**kwargs):
    """Configure module-wide settings"""
    config = EconomicsConfig()
    for key, value in kwargs.items():
        if hasattr(config, key):
            setattr(config, key, value)
    return config


# Version information
def get_version_info():
    """Return detailed version information"""
    return {
        'version': __version__,
        'author': __author__,
        'email': __email__,
        'description': 'CFA-based Economics Analysis Module for FinceptTerminal',
        'modules': len(__all__),
        'cfa_compliance': 'Level I & II'
    }