"""
Economics analytics package (CFA-curriculum economics: FX, parity, growth, cycles, policy,
capital flows, trade) plus statistics/forecasting helpers.

Every public class is resolved lazily (PEP 562, see _LAZY_ATTRS at the bottom): importing the
package costs nothing until a class is used, so a caller that needs one Decimal calculator does
not pay for scipy / statsmodels / matplotlib / seaborn / requests, which only analytics_engine,
reporting and data_handler need. `from Analytics.economics import X` and attribute access work
as before.

Command-line entry points in this directory (each prints one JSON object):
  trade_geopolitics.py, business_cycle.py, equity_risk_premium.py
"""

__version__ = "1.0.0"
__author__ = "Fincept Corporation"
__email__ = "support@fincept.in"

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


# Quick access functions for common operations. They import their class explicitly: the
# package's classes are lazy module attributes, which a global-name lookup inside this module
# would not trigger.
def analyze_currency_arbitrage(currency_data, base_currency='USD'):
    """Quick triangular arbitrage analysis (quotes keyed 'BASE/QUOTE' -> {'bid','ask'})"""
    from .currency_analysis import ArbitrageDetector
    return ArbitrageDetector().detect_triangular_arbitrage(currency_data, base_currency)


def calculate_gdp_growth(historical_data, forecast_assumptions=None):
    """Quick growth-accounting forecast of potential GDP growth
    (g_Y = g_A + alpha*g_K + (1-alpha)*g_L), see GrowthAnalyzer.forecast_potential_gdp"""
    from .growth_analysis import GrowthAnalyzer
    return GrowthAnalyzer().forecast_potential_gdp(historical_data or {}, forecast_assumptions or {})


def assess_policy_impact(policy_data, policy_type='fiscal'):
    """Quick policy assessment: fiscal -> fiscal stance from spending/tax changes
    (FiscalPolicyAnalyzer 'policy_stance'); monetary -> monetary tools analysis
    (MonetaryPolicyAnalyzer 'tools_analysis')."""
    if policy_type == 'fiscal':
        from .policy_analysis import FiscalPolicyAnalyzer
        return FiscalPolicyAnalyzer().calculate('policy_stance', fiscal_indicators=policy_data or {})
    from .policy_analysis import MonetaryPolicyAnalyzer
    return MonetaryPolicyAnalyzer().calculate('tools_analysis', policy_data=policy_data or {})


def detect_business_cycle_phase(economic_indicators):
    """Quick business cycle phase detection (needs at least 3 of the six indicators)"""
    from .market_cycles import BusinessCycleAnalyzer
    return BusinessCycleAnalyzer().detect_cycle_phase(economic_indicators)


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
    from .config import EconomicsConfig
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


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Every public class is imported on first access. This keeps `import Analytics.economics`
# cheap (it used to import scipy, statsmodels, matplotlib, seaborn and requests eagerly,
# ~5 s), and modules with an `if __name__ == "__main__":` block are not put in sys.modules
# before `python -m` re-executes them (RuntimeWarning "found in sys.modules").
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    # core
    "EconomicsBase": ("core", "EconomicsBase"),
    "DataValidator": ("core", "DataValidator"),
    "EconomicsError": ("core", "EconomicsError"),
    "ValidationError": ("core", "ValidationError"),
    "CalculationError": ("core", "CalculationError"),
    "DataError": ("core", "DataError"),
    # analysis modules
    "CurrencyAnalyzer": ("currency_analysis", "CurrencyAnalyzer"),
    "SpotForwardAnalyzer": ("currency_analysis", "SpotForwardAnalyzer"),
    "ArbitrageDetector": ("currency_analysis", "ArbitrageDetector"),
    "ParityAnalyzer": ("currency_analysis", "ParityAnalyzer"),
    "CarryTradeAnalyzer": ("currency_analysis", "CarryTradeAnalyzer"),
    "ExchangeCalculator": ("exchange_calculations", "ExchangeCalculator"),
    "CrossRateCalculator": ("exchange_calculations", "CrossRateCalculator"),
    "ForwardCalculator": ("exchange_calculations", "ForwardCalculator"),
    "GrowthAnalyzer": ("growth_analysis", "GrowthAnalyzer"),
    "ProductivityAnalyzer": ("growth_analysis", "ProductivityAnalyzer"),
    "ConvergenceAnalyzer": ("growth_analysis", "ConvergenceAnalyzer"),
    "DemographicAnalyzer": ("growth_analysis", "DemographicAnalyzer"),
    "BusinessCycleAnalyzer": ("market_cycles", "BusinessCycleAnalyzer"),
    "MarketStructureAnalyzer": ("market_cycles", "MarketStructureAnalyzer"),
    "CreditCycleAnalyzer": ("market_cycles", "CreditCycleAnalyzer"),
    "FiscalPolicyAnalyzer": ("policy_analysis", "FiscalPolicyAnalyzer"),
    "MonetaryPolicyAnalyzer": ("policy_analysis", "MonetaryPolicyAnalyzer"),
    "CentralBankAnalyzer": ("policy_analysis", "CentralBankAnalyzer"),
    "TradeAnalyzer": ("trade_geopolitics", "TradeAnalyzer"),
    "GeopoliticalRiskAnalyzer": ("trade_geopolitics", "GeopoliticalRiskAnalyzer"),
    "TradingBlocAnalyzer": ("trade_geopolitics", "TradingBlocAnalyzer"),
    "CapitalFlowAnalyzer": ("capital_flows", "CapitalFlowAnalyzer"),
    "FXMarketAnalyzer": ("capital_flows", "FXMarketAnalyzer"),
    "ExchangeRegimeAnalyzer": ("capital_flows", "ExchangeRegimeAnalyzer"),
    # infrastructure (heavy optional dependencies live here)
    "DataHandler": ("data_handler", "DataHandler"),
    "DataProvider": ("data_handler", "DataProvider"),
    "ManualDataInput": ("data_handler", "ManualDataInput"),
    "StatisticalAnalyzer": ("analytics_engine", "StatisticalAnalyzer"),
    "ForecastingEngine": ("analytics_engine", "ForecastingEngine"),
    "ScenarioAnalyzer": ("analytics_engine", "ScenarioAnalyzer"),
    "ReportGenerator": ("reporting", "ReportGenerator"),
    "VisualizationEngine": ("reporting", "VisualizationEngine"),
    "ExportManager": ("reporting", "ExportManager"),
    "EconomicsConfig": ("config", "EconomicsConfig"),
    "DataSources": ("config", "DataSources"),
    "CalculationPrecision": ("config", "CalculationPrecision"),
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
