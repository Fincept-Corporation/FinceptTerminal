"""
Economics Analytics Core Framework
==================================

Comprehensive foundation for economic analysis providing base classes, validation utilities, mathematical functions, and data containers. Implements CFA Institute standard methodologies with high-precision decimal arithmetic for reliable economic calculations.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Economic time series data (GDP, inflation, interest rates)
  - Exchange rate data and currency information
  - Balance of payments and capital flow statistics
  - Price level indices and inflation measures
  - Central bank policy data and monetary indicators
  - Trade statistics and international transaction data

OUTPUT:
  - Validated economic data containers with metadata
  - High-precision calculation results and analytics
  - Standardized economic indicators and metrics
  - Data quality assessments and validation reports
  - Mathematical calculations for economic modeling
  - Configuration settings and global constants

PARAMETERS:
  - precision: Decimal precision for calculations - default: 8
  - base_currency: Base currency for analysis - default: 'USD'
  - data_validation_enabled: Enable input validation - default: True
  - error_tolerance: Numerical error tolerance - default: 1e-6
  - default_confidence_interval: Default CI for calculations - default: 0.95
  - cache_enabled: Enable result caching - default: True
  - currency_code: ISO 4217 currency code
  - exchange_rate: Foreign exchange rate value
  - interest_rate: Annual interest rate (can be negative)
  - inflation_rate: Annual inflation rate
  - gdp_value: Gross Domestic Product value
  - time_period: Time period in years
"""

import re
import logging
from abc import ABC, abstractmethod
from decimal import Decimal, getcontext, ROUND_HALF_UP
from typing import Any, Dict, List, Optional, Union, Tuple
from datetime import datetime, date
import pandas as pd
import numpy as np

# Set high precision for financial calculations
getcontext().prec = 28

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class EconomicsError(Exception):
    """Base exception for economics module"""
    pass


class ValidationError(EconomicsError):
    """Data validation errors"""
    pass


class CalculationError(EconomicsError):
    """Mathematical calculation errors"""
    pass


class DataError(EconomicsError):
    """Data sourcing and formatting errors"""
    pass


class EconomicsBase(ABC):
    """
    Abstract base class for all economics analysis components.
    Ensures consistent interface and precision across modules.
    """

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        self.precision = precision
        self.base_currency = base_currency
        self.validator = DataValidator()
        self._results_cache = {}

    def to_decimal(self, value: Union[float, int, str]) -> Decimal:
        """Convert value to high-precision Decimal"""
        try:
            return Decimal(str(value)).quantize(
                Decimal('0.' + '0' * self.precision),
                rounding=ROUND_HALF_UP
            )
        except Exception as e:
            raise CalculationError(f"Cannot convert {value} to Decimal: {e}")

    def validate_inputs(self, **kwargs) -> bool:
        """Validate input parameters"""
        return self.validator.validate_parameters(**kwargs)

    @abstractmethod
    def calculate(self, *args, **kwargs) -> Dict[str, Any]:
        """Main calculation method - must be implemented by subclasses"""
        pass

    def get_metadata(self) -> Dict[str, Any]:
        """Return component metadata"""
        return {
            'class': self.__class__.__name__,
            'precision': self.precision,
            'base_currency': self.base_currency,
            'timestamp': datetime.now().isoformat()
        }


class DataValidator:
    """
    Comprehensive data validation for economics calculations.
    Ensures data quality and CFA-compliant input standards.
    """

    def __init__(self):
        self.currency_codes = {
            'USD', 'EUR', 'GBP', 'JPY', 'CHF', 'AUD', 'CAD', 'NZD',
            'SEK', 'NOK', 'DKK', 'CNY', 'INR', 'BRL', 'RUB', 'ZAR',
            'MXN', 'SGD', 'HKD', 'KRW', 'TRY', 'PLN', 'CZK', 'HUF'
        }

    def validate_currency_code(self, code: str) -> bool:
        """Validate ISO currency code"""
        if not isinstance(code, str) or len(code) != 3:
            raise ValidationError(f"Invalid currency code format: {code}")
        if code.upper() not in self.currency_codes:
            raise ValidationError(f"Unsupported currency code: {code}")
        return True

    def validate_exchange_rate(self, rate: Union[float, Decimal]) -> bool:
        """Validate exchange rate values"""
        rate = Decimal(str(rate)) if not isinstance(rate, Decimal) else rate
        if rate <= 0:
            raise ValidationError(f"Exchange rate must be positive: {rate}")
        if rate > Decimal('1000000'):
            raise ValidationError(f"Exchange rate seems unrealistic: {rate}")
        return True

    def validate_interest_rate(self, rate: Union[float, Decimal]) -> bool:
        """Validate interest rate (can be negative)"""
        rate = Decimal(str(rate)) if not isinstance(rate, Decimal) else rate
        if rate < Decimal('-0.10') or rate > Decimal('1.0'):
            raise ValidationError(f"Interest rate outside reasonable range: {rate}")
        return True

    def validate_time_period(self, period: Union[int, float]) -> bool:
        """Validate time periods in years"""
        if not isinstance(period, (int, float)) or period <= 0:
            raise ValidationError(f"Time period must be positive: {period}")
        if period > 100:
            raise ValidationError(f"Time period seems unrealistic: {period}")
        return True

    def validate_gdp_data(self, gdp: Union[float, Decimal]) -> bool:
        """Validate GDP values"""
        gdp = Decimal(str(gdp)) if not isinstance(gdp, Decimal) else gdp
        if gdp <= 0:
            raise ValidationError(f"GDP must be positive: {gdp}")
        return True

    def validate_inflation_rate(self, rate: Union[float, Decimal]) -> bool:
        """Validate inflation rates"""
        rate = Decimal(str(rate)) if not isinstance(rate, Decimal) else rate
        if rate < Decimal('-0.5') or rate > Decimal('2.0'):
            raise ValidationError(f"Inflation rate outside normal range: {rate}")
        return True

    def validate_date_format(self, date_input: Union[str, datetime, date]) -> datetime:
        """Validate and convert date inputs"""
        if isinstance(date_input, datetime):
            return date_input
        elif isinstance(date_input, date):
            return datetime.combine(date_input, datetime.min.time())
        elif isinstance(date_input, str):
            try:
                return datetime.strptime(date_input, '%Y-%m-%d')
            except ValueError:
                try:
                    return datetime.strptime(date_input, '%Y/%m/%d')
                except ValueError:
                    raise ValidationError(f"Invalid date format: {date_input}")
        else:
            raise ValidationError(f"Unsupported date type: {type(date_input)}")

    def validate_percentage(self, value: Union[float, Decimal]) -> bool:
        """Validate percentage values (0-100 or 0-1)"""
        value = Decimal(str(value)) if not isinstance(value, Decimal) else value
        if value < 0 or value > 100:
            raise ValidationError(f"Percentage outside valid range: {value}")
        return True

    def validate_dataframe(self, df: pd.DataFrame, required_columns: List[str]) -> bool:
        """Validate pandas DataFrame structure"""
        if not isinstance(df, pd.DataFrame):
            raise ValidationError("Input must be a pandas DataFrame")

        missing_cols = set(required_columns) - set(df.columns)
        if missing_cols:
            raise ValidationError(f"Missing required columns: {missing_cols}")

        if df.empty:
            raise ValidationError("DataFrame cannot be empty")

        return True

    def validate_bid_ask_spread(self, bid: Decimal, ask: Decimal) -> bool:
        """Validate bid-ask spread"""
        if bid >= ask:
            raise ValidationError(f"Bid ({bid}) must be less than ask ({ask})")

        spread = (ask - bid) / bid
        if spread > Decimal('0.1'):  # 10% spread seems excessive
            raise ValidationError(f"Bid-ask spread too wide: {spread:.4f}")

        return True

    def validate_parameters(self, **kwargs) -> bool:
        """Validate multiple parameters based on their types"""
        validators = {
            'currency': self.validate_currency_code,
            'exchange_rate': self.validate_exchange_rate,
            'interest_rate': self.validate_interest_rate,
            'time_period': self.validate_time_period,
            'gdp': self.validate_gdp_data,
            'inflation': self.validate_inflation_rate,
            'percentage': self.validate_percentage,
            'date': self.validate_date_format
        }

        for param_name, param_value in kwargs.items():
            # Extract parameter type from name
            param_type = None
            for validator_type in validators.keys():
                if validator_type in param_name.lower():
                    param_type = validator_type
                    break

            if param_type and param_value is not None:
                validators[param_type](param_value)

        return True


class CalculationUtils:
    """
    Utility functions for common economic calculations.
    Provides mathematical precision and error handling.
    """

    @staticmethod
    def compound_growth_rate(initial: Decimal, final: Decimal, periods: Decimal) -> Decimal:
        """Calculate compound annual growth rate"""
        if initial <= 0 or final <= 0 or periods <= 0:
            raise CalculationError("All values must be positive for CAGR calculation")

        return (final / initial) ** (Decimal('1') / periods) - Decimal('1')

    @staticmethod
    def present_value(future_value: Decimal, rate: Decimal, periods: Decimal) -> Decimal:
        """Calculate present value"""
        if periods <= 0:
            raise CalculationError("Periods must be positive")

        return future_value / ((Decimal('1') + rate) ** periods)

    @staticmethod
    def future_value(present_value: Decimal, rate: Decimal, periods: Decimal) -> Decimal:
        """Calculate future value"""
        if periods <= 0:
            raise CalculationError("Periods must be positive")

        return present_value * ((Decimal('1') + rate) ** periods)

    @staticmethod
    def effective_rate(nominal_rate: Decimal, compounding_frequency: int) -> Decimal:
        """Calculate effective annual rate"""
        if compounding_frequency <= 0:
            raise CalculationError("Compounding frequency must be positive")

        return (Decimal('1') + nominal_rate / Decimal(str(compounding_frequency))) ** Decimal(
            str(compounding_frequency)) - Decimal('1')

    @staticmethod
    def geometric_mean(values: List[Decimal]) -> Decimal:
        """Calculate geometric mean"""
        if not values or any(v <= 0 for v in values):
            raise CalculationError("All values must be positive for geometric mean")

        product = Decimal('1')
        for value in values:
            product *= value

        return product ** (Decimal('1') / Decimal(str(len(values))))

    @staticmethod
    def standard_deviation(values: List[Decimal]) -> Decimal:
        """Calculate sample standard deviation"""
        if len(values) < 2:
            raise CalculationError("At least 2 values required for standard deviation")

        mean = sum(values) / Decimal(str(len(values)))
        variance = sum((x - mean) ** 2 for x in values) / Decimal(str(len(values) - 1))

        return variance.sqrt()


class DataContainer:
    """
    Container for economic data with validation and metadata.
    Ensures data integrity throughout calculations.
    """

    def __init__(self, data: Dict[str, Any], data_type: str, timestamp: Optional[datetime] = None):
        self.data = data
        self.data_type = data_type
        self.timestamp = timestamp or datetime.now()
        self.validator = DataValidator()
        self._validate_data()

    def _validate_data(self):
        """Validate data based on type"""
        validation_rules = {
            'currency': ['currency_code', 'exchange_rate'],
            'gdp': ['gdp_value', 'country_code'],
            'interest_rate': ['rate_value', 'currency'],
            'inflation': ['inflation_rate', 'period']
        }

        if self.data_type in validation_rules:
            required_fields = validation_rules[self.data_type]
            for field in required_fields:
                if field not in self.data:
                    raise ValidationError(f"Missing required field for {self.data_type}: {field}")

    def get_value(self, key: str, default: Any = None) -> Any:
        """Get value with optional default"""
        return self.data.get(key, default)

    def update_value(self, key: str, value: Any):
        """Update value with validation"""
        self.data[key] = value
        self._validate_data()

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary with metadata"""
        return {
            'data': self.data,
            'type': self.data_type,
            'timestamp': self.timestamp.isoformat(),
            'validated': True
        }


# Global configuration
class EconomicsConfig:
    """Global configuration for economics module"""

    def __init__(self):
        self.precision = 8
        self.base_currency = 'USD'
        self.data_validation_enabled = True
        self.error_tolerance = Decimal('1e-6')
        self.default_confidence_interval = Decimal('0.95')
        self.cache_enabled = True
        self.logging_level = logging.INFO

    def update_config(self, **kwargs):
        """Update configuration parameters"""
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)
            else:
                logger.warning(f"Unknown configuration parameter: {key}")

    def to_dict(self) -> Dict[str, Any]:
        """Convert configuration to dictionary"""
        return {
            'precision': self.precision,
            'base_currency': self.base_currency,
            'data_validation_enabled': self.data_validation_enabled,
            'error_tolerance': str(self.error_tolerance),
            'default_confidence_interval': str(self.default_confidence_interval),
            'cache_enabled': self.cache_enabled,
            'logging_level': self.logging_level
        }


# Module constants
ECONOMIC_CONSTANTS = {
    'DAYS_PER_YEAR': Decimal('365'),
    'BUSINESS_DAYS_PER_YEAR': Decimal('252'),
    'MONTHS_PER_YEAR': Decimal('12'),
    'QUARTERS_PER_YEAR': Decimal('4'),
    'BASIS_POINTS': Decimal('10000'),
    'PERCENTAGE': Decimal('100')
}

# Export global configuration instance
config = EconomicsConfig()