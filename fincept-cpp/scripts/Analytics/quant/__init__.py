"""
Quantitative Analytics Module
============================

CFA-level quantitative analysis for financial modeling and analysis.

Modules:
- base_calculator: Base class for all quantitative calculators
- data_validator: Data validation and quality control
- exceptions: Custom exception classes
- quant_modules_3042: Advanced quantitative analyzer
- rate_calculations: Interest rate and yield calculations
"""

from .exceptions import (
    QuantAnalyticsError,
    DataValidationError,
    InsufficientDataError,
    CalculationError,
    ConvergenceError,
    ModelFitError,
    ConfigurationError,
    DependencyError,
    handle_calculation_error,
    safe_calculation
)

from .base_calculator import (
    BaseCalculator,
    CalculatorFactory,
    CalculationResult,
    validate_inputs,
    cache_result,
    timing_decorator
)

from .data_validator import (
    DataValidator,
    DataQualityReport,
    validate_returns_series,
    validate_positive_number,
    validate_probability
)

__all__ = [
    'QuantAnalyticsError',
    'DataValidationError',
    'InsufficientDataError',
    'CalculationError',
    'ConvergenceError',
    'ModelFitError',
    'ConfigurationError',
    'DependencyError',
    'handle_calculation_error',
    'safe_calculation',
    'BaseCalculator',
    'CalculatorFactory',
    'CalculationResult',
    'validate_inputs',
    'cache_result',
    'timing_decorator',
    'DataValidator',
    'DataQualityReport',
    'validate_returns_series',
    'validate_positive_number',
    'validate_probability'
]
