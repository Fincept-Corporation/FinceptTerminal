
"""Shared constants, validators and CLI plumbing for Analytics/portfolioManagement (library, no CLI).

Conventions used by every module here: returns are simple per-period (daily) fractions; annualisation
uses 252 periods (mean x 252, stdev x sqrt(252)); the default risk-free rate is 3 % a year; VaR / ES are
reported as POSITIVE loss fractions at the stated confidence level (0.95 = the 5 % left tail).
Every CLI prints exactly one JSON object on stdout ({"error": "<message>"} and exit 1 on failure).
"""

from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple, Union
from enum import Enum
import json
import os
import sys

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
    """Validate covariance matrix properties (square, symmetric, finite, positive semi-definite)"""
    cov_matrix = np.atleast_2d(cov_matrix)
    if cov_matrix.shape[0] != cov_matrix.shape[1]:
        return False
    if not np.all(np.isfinite(cov_matrix)):
        return False
    if not np.allclose(cov_matrix, cov_matrix.T):
        return False
    eigenvals = np.linalg.eigvalsh(cov_matrix)
    return bool(np.all(eigenvals >= -1e-8))  # Allow small numerical errors


# ============================================================================
# CLI plumbing shared by the six command-line modules
# ============================================================================

class CliError(ValueError):
    """A bad request: reported as {"error": <message>} with exit code 1, no traceback."""


def _fincept_json():
    try:
        import fincept_json
    except ImportError:  # scripts/ not on sys.path: it is three levels up from this file
        sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        import fincept_json
    return fincept_json


def _json_default(obj):
    if isinstance(obj, Enum):
        return obj.value
    return str(obj)


def cli_emit(obj) -> None:
    """Print one JSON line (NaN / Infinity -> null, Enum -> its value)."""
    _fincept_json().emit(obj, default=_json_default)


class CliArgs:
    """Arguments after the command, in either of the two accepted forms:

    * positional (legacy):  <command> <arg1> <arg2> ...   ("null" or a missing arg = use the default)
    * one JSON object:      <command> '{"<name1>": ..., "<name2>": ...}'
      recognised when it is the only argument, is an object and contains the FIRST name of the
      command's argument list (argmap). Missing keys mean "use the default" - the old bridge turned
      them into the string 'null', which crashed float('null') / json.loads('null').get(...).
    """

    def __init__(self, argv: List[str], argmap: Dict[str, List[str]]):
        self.command = argv[1] if len(argv) > 1 else None
        raw = list(argv[2:])
        self.object = None          # the whole JSON object in the object form
        self._items = [(s, True) for s in raw]
        names = argmap.get(self.command) or []
        if names and len(raw) == 1:
            try:
                obj = json.loads(raw[0])
            except ValueError:
                obj = None
            if isinstance(obj, dict) and names[0] in obj:
                self.object = obj
                self._items = [(obj.get(n), False) for n in names]

    def _get(self, i):
        if i >= len(self._items):
            return None, False
        value, is_raw = self._items[i]
        if is_raw and (value is None or value.strip() in ("", "null", "None")):
            return None, False
        return value, is_raw

    def get_json(self, i: int, name: str, default=None):
        value, is_raw = self._get(i)
        if value is None:
            return default
        if is_raw:
            try:
                return json.loads(value)
            except ValueError:
                raise CliError(f"{name} is not valid JSON")
        return value

    def get_float(self, i: int, name: str, default=None):
        value, _ = self._get(i)
        if value is None:
            return default
        try:
            out = float(value)
        except (TypeError, ValueError):
            raise CliError(f"{name} must be a number (got {value!r})")
        if not np.isfinite(out):
            raise CliError(f"{name} must be a finite number")
        return out

    def get_str(self, i: int, name: str, default=None):
        value, _ = self._get(i)
        if value is None:
            return default
        return value if isinstance(value, str) else json.dumps(value)


def return_series(values, name: str, min_len: int = 2) -> np.ndarray:
    """A 1-D array of finite floats, or CliError with a plain message."""
    if values is None:
        raise CliError(f"{name} is required (a list of periodic returns as fractions, e.g. 0.012)")
    if isinstance(values, dict):
        raise CliError(f"{name} must be a list of numbers, not an object")
    try:
        arr = np.asarray(values, dtype=float)
    except (TypeError, ValueError):
        raise CliError(f"{name} must contain only numbers")
    if arr.ndim != 1:
        raise CliError(f"{name} must be a flat list of numbers")
    if not np.all(np.isfinite(arr)):
        raise CliError(f"{name} contains missing or non-finite values")
    if arr.size < min_len:
        raise CliError(f"{name} needs at least {min_len} observations (got {arr.size})")
    return arr


def run_cli(handlers: Dict, argmap: Dict[str, List[str]]) -> None:
    """Dispatch sys.argv to handlers[command](CliArgs) and print exactly one JSON object.

    A handler returns a dict; a dict with an "error" key exits 1. Unexpected exceptions are reported
    as {"error": ...} with the traceback on stderr only.
    """
    commands = ", ".join(sorted(handlers))
    code = 0
    try:
        if len(sys.argv) < 2:
            raise CliError(f"No command specified. Commands: {commands}")
        args = CliArgs(sys.argv, argmap)
        handler = handlers.get(args.command)
        if handler is None:
            raise CliError(f"Unknown command: {args.command}. Commands: {commands}")
        result = handler(args)
        if isinstance(result, dict) and "error" in result:
            code = 1
    except CliError as exc:
        result, code = {"error": str(exc)}, 1
    except ValueError as exc:  # the libraries raise ValueError with a plain message for bad input
        import traceback
        traceback.print_exc(file=sys.stderr)
        result, code = {"error": str(exc) or type(exc).__name__}, 1
    except Exception as exc:  # noqa: BLE001 - every failure must still be one JSON object
        import traceback
        traceback.print_exc(file=sys.stderr)
        result, code = {"error": f"{type(exc).__name__}: {exc}"}, 1
    cli_emit(result)
    sys.exit(code)