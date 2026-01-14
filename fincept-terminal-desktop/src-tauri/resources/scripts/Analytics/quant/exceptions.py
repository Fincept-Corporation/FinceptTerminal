
"""Quantitative Exceptions Module
=================================

Custom exception classes for quantitative analysis

===== DATA SOURCES REQUIRED =====
INPUT:
  - High-frequency market data and price series
  - Order book data and market microstructure
  - Alternative data sources and sentiment indicators
  - Economic data and market fundamentals
  - Historical factor returns and premiums

OUTPUT:
  - Quantitative trading signals and strategies
  - Factor model implementations and analysis
  - Risk models and portfolio construction methods
  - Backtest results and performance attribution
  - Alpha generation and research insights

PARAMETERS:
  - factor_model: Factor model type (default: 'fama_french_5')
  - lookback_period: Historical lookback window (default: 252 days)
  - rebalance_frequency: Strategy rebalancing frequency (default: 'monthly')
  - universe_size: Investment universe size (default: 1000)
  - risk_model: Risk model for portfolio construction (default: 'barra')
"""


from typing import Optional, Any
from functools import wraps


class QuantAnalyticsError(Exception):
    """Base exception for all quantitative analytics errors."""

    def __init__(self, message: str, error_code: Optional[str] = None):
        self.message = message
        self.error_code = error_code
        super().__init__(self.message)

    def to_dict(self) -> dict:
        return {
            'error': self.message,
            'error_code': self.error_code,
            'error_type': self.__class__.__name__
        }


class DataValidationError(QuantAnalyticsError):
    """Raised when input data fails validation checks."""

    def __init__(self, message: str, field_name: Optional[str] = None):
        self.field_name = field_name
        super().__init__(
            message=f"{field_name}: {message}" if field_name else message,
            error_code="DATA_VALIDATION_ERROR"
        )


class InsufficientDataError(QuantAnalyticsError):
    """Raised when there is not enough data for the calculation."""

    def __init__(self, required: int, provided: int, calculation: Optional[str] = None):
        self.required = required
        self.provided = provided
        self.calculation = calculation
        message = f"Insufficient data: need {required} observations, got {provided}"
        if calculation:
            message = f"{calculation}: {message}"
        super().__init__(message=message, error_code="INSUFFICIENT_DATA")


class CalculationError(QuantAnalyticsError):
    """Raised when a calculation fails."""

    def __init__(self, message: str, calculation_type: Optional[str] = None):
        self.calculation_type = calculation_type
        super().__init__(
            message=f"{calculation_type}: {message}" if calculation_type else message,
            error_code="CALCULATION_ERROR"
        )


class ConvergenceError(QuantAnalyticsError):
    """Raised when an optimization or iterative method fails to converge."""

    def __init__(self, message: str, iterations: Optional[int] = None):
        self.iterations = iterations
        if iterations:
            message = f"{message} (after {iterations} iterations)"
        super().__init__(message=message, error_code="CONVERGENCE_ERROR")


class ModelFitError(QuantAnalyticsError):
    """Raised when a statistical model fails to fit."""

    def __init__(self, message: str, model_type: Optional[str] = None):
        self.model_type = model_type
        super().__init__(
            message=f"{model_type}: {message}" if model_type else message,
            error_code="MODEL_FIT_ERROR"
        )


class ConfigurationError(QuantAnalyticsError):
    """Raised when configuration parameters are invalid."""

    def __init__(self, message: str, parameter: Optional[str] = None):
        self.parameter = parameter
        super().__init__(
            message=f"Invalid parameter '{parameter}': {message}" if parameter else message,
            error_code="CONFIGURATION_ERROR"
        )


class DependencyError(QuantAnalyticsError):
    """Raised when a required dependency is not available."""

    def __init__(self, dependency: str, message: Optional[str] = None):
        self.dependency = dependency
        msg = f"Required dependency '{dependency}' not available"
        if message:
            msg = f"{msg}: {message}"
        super().__init__(message=msg, error_code="DEPENDENCY_ERROR")


def handle_calculation_error(func):
    """Decorator to handle calculation errors gracefully."""
    @wraps(func)
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except DataValidationError:
            raise
        except InsufficientDataError:
            raise
        except CalculationError:
            raise
        except ConvergenceError:
            raise
        except ModelFitError:
            raise
        except Exception as e:
            func_name = getattr(func, '__name__', 'unknown')
            raise CalculationError(
                message=str(e),
                calculation_type=func_name
            ) from e
    return wrapper


def safe_calculation(default_value: Any = None):
    """Decorator that returns a default value on error instead of raising."""
    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except Exception:
                return default_value
        return wrapper
    return decorator
