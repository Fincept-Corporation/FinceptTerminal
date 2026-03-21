
"""Portfolio Config Module
===============================

Portfolio management configuration

===== DATA SOURCES REQUIRED =====
INPUT:
  - Portfolio holdings and transaction history
  - Asset price data and market returns
  - Benchmark indices and market data
  - Investment policy statements and constraints
  - Risk tolerance and preference parameters

OUTPUT:
  - Portfolio performance metrics and attribution
  - Risk analysis and diversification metrics
  - Rebalancing recommendations and optimization
  - Portfolio analytics reports and visualizations
  - Investment strategy recommendations

PARAMETERS:
  - optimization_method: Portfolio optimization method (default: 'mean_variance')
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - rebalance_frequency: Portfolio rebalancing frequency (default: 'quarterly')
  - max_weight: Maximum single asset weight (default: 0.10)
  - benchmark: Portfolio benchmark index (default: 'market_index')
"""

from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple, Union
from enum import Enum
import numpy as np


# Mathematical Constants
class MathConstants:
    """Mathematical constants used across analytics"""
    TRADING_DAYS_YEAR = 252
    BUSINESS_DAYS_YEAR = 252
    CALENDAR_DAYS_YEAR = 365
    SQRT_TRADING_DAYS = np.sqrt(TRADING_DAYS_YEAR)

    # Risk-free rate assumptions
    DEFAULT_RISK_FREE_RATE = 0.03  # 3% annual

    # Optimization parameters
    MAX_ITERATIONS = 10000
    CONVERGENCE_TOLERANCE = 1e-8

    # Statistical defaults
    CONFIDENCE_LEVELS = [0.90, 0.95, 0.99]
    DEFAULT_CONFIDENCE = 0.95


# Asset Classes
class AssetClass(Enum):
    """Major asset classes for portfolio construction"""
    EQUITY = "equity"
    FIXED_INCOME = "fixed_income"
    COMMODITIES = "commodities"
    REAL_ESTATE = "real_estate"
    CASH = "cash"
    ALTERNATIVES = "alternatives"
    CRYPTO = "crypto"


# Risk Metrics
class RiskMetric(Enum):
    """Risk measurement types"""
    STANDARD_DEVIATION = "standard_deviation"
    VARIANCE = "variance"
    VaR = "value_at_risk"
    CVaR = "conditional_value_at_risk"
    BETA = "beta"
    TRACKING_ERROR = "tracking_error"
    DOWNSIDE_DEVIATION = "downside_deviation"


# Performance Metrics
class PerformanceMetric(Enum):
    """Performance measurement types"""
    SHARPE_RATIO = "sharpe_ratio"
    TREYNOR_RATIO = "treynor_ratio"
    INFORMATION_RATIO = "information_ratio"
    JENSEN_ALPHA = "jensen_alpha"
    M_SQUARED = "m_squared"
    SORTINO_RATIO = "sortino_ratio"


# Data Validation Schemas
@dataclass
class DataSchema:
    """Data validation requirements"""
    required_columns: List[str]
    optional_columns: List[str]
    date_columns: List[str]
    numeric_columns: List[str]
    min_observations: int
    max_missing_ratio: float


# Portfolio Analytics Parameters
@dataclass
class PortfolioParameters:
    """Portfolio analytics configuration"""
    min_weight: float = 0.0
    max_weight: float = 1.0
    weight_sum_tolerance: float = 1e-6
    return_frequency: str = "daily"  # daily, weekly, monthly, annual
    risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE

    # Efficient frontier parameters
    num_frontier_points: int = 100
    min_return_percentile: float = 0.05
    max_return_percentile: float = 0.95


# Risk Management Parameters
@dataclass
class RiskParameters:
    """Risk management configuration"""
    var_confidence_levels: List[float] = None
    var_holding_period: int = 1  # days
    monte_carlo_simulations: int = 10000
    historical_window: int = 252  # trading days

    # Stress testing
    stress_scenarios: Dict[str, float] = None

    def __post_init__(self):
        if self.var_confidence_levels is None:
            self.var_confidence_levels = [0.95, 0.99]
        if self.stress_scenarios is None:
            self.stress_scenarios = {
                "market_crash": -0.20,
                "mild_stress": -0.10,
                "extreme_stress": -0.30
            }


# Data Provider Configuration
@dataclass
class DataProviderConfig:
    """Data provider settings"""
    provider_name: str
    api_key: Optional[str] = None
    base_url: Optional[str] = None
    rate_limit: int = 1000  # requests per hour
    timeout: int = 30  # seconds
    retry_attempts: int = 3
    cache_duration: int = 3600  # seconds


# Behavioral Finance Parameters
@dataclass
class BehavioralParameters:
    """Behavioral finance configuration"""
    bias_types: List[str] = None
    risk_aversion_levels: Dict[str, float] = None
    utility_function_type: str = "power"  # power, exponential, log

    def __post_init__(self):
        if self.bias_types is None:
            self.bias_types = [
                "overconfidence", "anchoring", "loss_aversion",
                "mental_accounting", "herding", "confirmation_bias"
            ]
        if self.risk_aversion_levels is None:
            self.risk_aversion_levels = {
                "low": 1.0,
                "moderate": 3.0,
                "high": 5.0,
                "very_high": 10.0
            }


# Economics Analysis Parameters
@dataclass
class EconomicsParameters:
    """Economics and markets configuration"""
    business_cycle_indicators: List[str] = None
    yield_curve_maturities: List[int] = None  # years
    credit_rating_categories: List[str] = None

    def __post_init__(self):
        if self.business_cycle_indicators is None:
            self.business_cycle_indicators = [
                "gdp_growth", "unemployment_rate", "inflation_rate",
                "yield_curve_slope", "credit_spreads", "equity_volatility"
            ]
        if self.yield_curve_maturities is None:
            self.yield_curve_maturities = [0.25, 0.5, 1, 2, 5, 10, 30]
        if self.credit_rating_categories is None:
            self.credit_rating_categories = [
                "AAA", "AA", "A", "BBB", "BB", "B", "CCC", "CC", "C", "D"
            ]


# Default Configurations
DEFAULT_PORTFOLIO_PARAMS = PortfolioParameters()
DEFAULT_RISK_PARAMS = RiskParameters()
DEFAULT_BEHAVIORAL_PARAMS = BehavioralParameters()
DEFAULT_ECONOMICS_PARAMS = EconomicsParameters()

# Data Schemas
PRICE_DATA_SCHEMA = DataSchema(
    required_columns=["date", "close"],
    optional_columns=["open", "high", "low", "volume", "adjusted_close"],
    date_columns=["date"],
    numeric_columns=["open", "high", "low", "close", "volume", "adjusted_close"],
    min_observations=30,
    max_missing_ratio=0.05
)

RETURN_DATA_SCHEMA = DataSchema(
    required_columns=["date", "return"],
    optional_columns=["excess_return", "benchmark_return"],
    date_columns=["date"],
    numeric_columns=["return", "excess_return", "benchmark_return"],
    min_observations=30,
    max_missing_ratio=0.02
)

PORTFOLIO_DATA_SCHEMA = DataSchema(
    required_columns=["asset", "weight"],
    optional_columns=["expected_return", "volatility", "beta"],
    date_columns=[],
    numeric_columns=["weight", "expected_return", "volatility", "beta"],
    min_observations=2,
    max_missing_ratio=0.0
)

# Error Messages
ERROR_MESSAGES = {
    "insufficient_data": "Insufficient data points for analysis",
    "invalid_weights": "Portfolio weights must sum to 1.0",
    "negative_variance": "Negative variance detected in calculations",
    "singular_matrix": "Covariance matrix is singular",
    "optimization_failed": "Portfolio optimization failed to converge",
    "invalid_date_range": "Invalid date range specified",
    "missing_risk_free_rate": "Risk-free rate not specified",
    "invalid_confidence_level": "Confidence level must be between 0 and 1"
}


# Validation Functions
def validate_weights(weights: Union[List, np.ndarray], tolerance: float = 1e-6) -> bool:
    """Validate portfolio weights sum to 1.0"""
    return abs(np.sum(weights) - 1.0) <= tolerance


def validate_returns(returns: Union[List, np.ndarray]) -> bool:
    """Validate return data"""
    returns_array = np.array(returns)
    return not np.any(np.isnan(returns_array)) and len(returns_array) >= 2


def validate_covariance_matrix(cov_matrix: np.ndarray) -> bool:
    """Validate covariance matrix properties"""
    if cov_matrix.shape[0] != cov_matrix.shape[1]:
        return False
    if not np.allclose(cov_matrix, cov_matrix.T):
        return False
    eigenvals = np.linalg.eigvals(cov_matrix)
    return np.all(eigenvals >= -1e-8)  # Allow small numerical errors