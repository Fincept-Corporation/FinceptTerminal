"""
Derivatives Core Analytics Module
===============================

Core framework for derivatives analytics providing foundational classes, data structures, and validation utilities. Implements CFA Institute standard methodologies for derivative pricing, risk measurement, and portfolio management.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Market data including spot prices, interest rates, dividend yields
  - Volatility surfaces and option pricing parameters
  - Derivative instrument specifications (strike, expiry, type)
  - Day count conventions and calendar data
  - Interest rate curves and yield curves
  - Corporate actions and event data

OUTPUT:
  - Standardized derivative instrument representations
  - Market data validation and processing
  - Pricing result containers and calculations
  - Time calculations using various day count conventions
  - Interest rate conversion utilities
  - Model validation and error handling

PARAMETERS:
  - spot_price: Current spot price of underlying asset
  - risk_free_rate: Risk-free interest rate
  - dividend_yield: Dividend yield for the underlying
  - volatility: Volatility parameter for pricing models
  - time_to_expiry: Time to expiration in years
  - strike_price: Strike price for options
  - notional: Contract notional amount - default: 1.0
  - day_count: Day count convention - default: DayCountConvention.ACT_365
  - from_compounding: Source rate compounding method
  - to_compounding: Target rate compounding method
  - frequency: Compounding frequency for discrete rates
"""

from abc import ABC, abstractmethod
from enum import Enum
from dataclasses import dataclass
from typing import Optional, Union, Dict, Any, List
from datetime import datetime, date
import numpy as np
import logging

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class DerivativeType(Enum):
    """Classification of derivative instruments"""
    FORWARD = "forward"
    FUTURE = "future"
    SWAP = "swap"
    OPTION = "option"
    CREDIT_DERIVATIVE = "credit_derivative"


class OptionType(Enum):
    """Option contract types"""
    CALL = "call"
    PUT = "put"


class Position(Enum):
    """Trading position direction"""
    LONG = "long"
    SHORT = "short"


class ExerciseStyle(Enum):
    """Option exercise styles"""
    EUROPEAN = "european"
    AMERICAN = "american"
    BERMUDAN = "bermudan"


class UnderlyingType(Enum):
    """Types of underlying assets"""
    EQUITY = "equity"
    BOND = "bond"
    COMMODITY = "commodity"
    CURRENCY = "currency"
    INTEREST_RATE = "interest_rate"
    INDEX = "index"


class DayCountConvention(Enum):
    """Day count conventions for financial calculations"""
    ACT_360 = "ACT/360"
    ACT_365 = "ACT/365"
    THIRTY_360 = "30/360"
    ACT_ACT = "ACT/ACT"


@dataclass
class MarketData:
    """Market data container for derivative pricing"""
    spot_price: float
    risk_free_rate: float
    dividend_yield: float = 0.0
    volatility: float = 0.0
    time_to_expiry: float = 0.0
    strike_price: Optional[float] = None
    forward_price: Optional[float] = None

    def __post_init__(self):
        """Validate market data inputs"""
        if self.spot_price <= 0:
            raise ValueError("Spot price must be positive")
        if self.volatility < 0:
            raise ValueError("Volatility cannot be negative")
        if self.time_to_expiry < 0:
            raise ValueError("Time to expiry cannot be negative")


@dataclass
class PricingResult:
    """Container for derivative pricing results"""
    fair_value: float
    intrinsic_value: Optional[float] = None
    time_value: Optional[float] = None
    greeks: Optional[Dict[str, float]] = None
    confidence_interval: Optional[tuple] = None
    calculation_details: Optional[Dict[str, Any]] = None

    def __post_init__(self):
        """Calculate derived values"""
        if self.intrinsic_value is not None and self.time_value is None:
            self.time_value = self.fair_value - self.intrinsic_value


class ValidationError(Exception):
    """Custom exception for validation errors"""
    pass


class PricingError(Exception):
    """Custom exception for pricing calculation errors"""
    pass


class DerivativeInstrument(ABC):
    """
    Abstract base class for all derivative instruments.
    Implements common interface following CFA curriculum structure.
    """

    def __init__(self,
                 derivative_type: DerivativeType,
                 underlying_type: UnderlyingType,
                 expiry_date: Union[datetime, date],
                 notional: float = 1.0,
                 day_count: DayCountConvention = DayCountConvention.ACT_365):

        self.derivative_type = derivative_type
        self.underlying_type = underlying_type
        self.expiry_date = expiry_date
        self.notional = notional
        self.day_count = day_count
        self.creation_date = datetime.now()

        self._validate_inputs()

    def _validate_inputs(self):
        """Validate instrument parameters"""
        if self.notional <= 0:
            raise ValidationError("Notional amount must be positive")

        if isinstance(self.expiry_date, date):
            self.expiry_date = datetime.combine(self.expiry_date, datetime.min.time())

        if self.expiry_date <= self.creation_date:
            raise ValidationError("Expiry date must be in the future")

    @abstractmethod
    def calculate_payoff(self, spot_price: float) -> float:
        """Calculate payoff at expiration given spot price"""
        pass

    @abstractmethod
    def fair_value(self, market_data: MarketData) -> PricingResult:
        """Calculate fair value using appropriate pricing model"""
        pass

    def time_to_expiry(self, valuation_date: Optional[datetime] = None) -> float:
        """Calculate time to expiry in years"""
        if valuation_date is None:
            valuation_date = datetime.now()

        time_diff = self.expiry_date - valuation_date

        if self.day_count == DayCountConvention.ACT_365:
            return time_diff.total_seconds() / (365.25 * 24 * 3600)
        elif self.day_count == DayCountConvention.ACT_360:
            return time_diff.total_seconds() / (360 * 24 * 3600)
        elif self.day_count == DayCountConvention.THIRTY_360:
            return time_diff.days / 360
        else:  # ACT_ACT
            return time_diff.total_seconds() / (365.25 * 24 * 3600)

    def is_expired(self, valuation_date: Optional[datetime] = None) -> bool:
        """Check if derivative has expired"""
        if valuation_date is None:
            valuation_date = datetime.now()
        return valuation_date >= self.expiry_date

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(type={self.derivative_type.value}, expiry={self.expiry_date})"


class ForwardCommitment(DerivativeInstrument):
    """Base class for forward commitments (forwards, futures, swaps)"""

    def __init__(self,
                 derivative_type: DerivativeType,
                 underlying_type: UnderlyingType,
                 expiry_date: Union[datetime, date],
                 contract_price: float,
                 notional: float = 1.0,
                 day_count: DayCountConvention = DayCountConvention.ACT_365):
        super().__init__(derivative_type, underlying_type, expiry_date, notional, day_count)
        self.contract_price = contract_price

        if contract_price <= 0:
            raise ValidationError("Contract price must be positive")


class ContingentClaim(DerivativeInstrument):
    """Base class for contingent claims (options)"""

    def __init__(self,
                 option_type: OptionType,
                 underlying_type: UnderlyingType,
                 expiry_date: Union[datetime, date],
                 strike_price: float,
                 exercise_style: ExerciseStyle = ExerciseStyle.EUROPEAN,
                 notional: float = 1.0,
                 day_count: DayCountConvention = DayCountConvention.ACT_365):

        super().__init__(DerivativeType.OPTION, underlying_type, expiry_date, notional, day_count)
        self.option_type = option_type
        self.strike_price = strike_price
        self.exercise_style = exercise_style

        if strike_price <= 0:
            raise ValidationError("Strike price must be positive")

    def moneyness(self, spot_price: float) -> str:
        """Determine option moneyness"""
        if self.option_type == OptionType.CALL:
            if spot_price > self.strike_price:
                return "ITM"  # In-the-money
            elif spot_price == self.strike_price:
                return "ATM"  # At-the-money
            else:
                return "OTM"  # Out-of-the-money
        else:  # PUT
            if spot_price < self.strike_price:
                return "ITM"
            elif spot_price == self.strike_price:
                return "ATM"
            else:
                return "OTM"

    def intrinsic_value(self, spot_price: float) -> float:
        """Calculate intrinsic value of option"""
        if self.option_type == OptionType.CALL:
            return max(0, spot_price - self.strike_price)
        else:  # PUT
            return max(0, self.strike_price - spot_price)


class PricingEngine(ABC):
    """Abstract base class for pricing engines"""

    @abstractmethod
    def price(self, instrument: DerivativeInstrument, market_data: MarketData) -> PricingResult:
        """Price derivative instrument"""
        pass

    @abstractmethod
    def validate_inputs(self, instrument: DerivativeInstrument, market_data: MarketData) -> bool:
        """Validate inputs for pricing"""
        pass


class ModelValidator:
    """Validation utilities for derivative models"""

    @staticmethod
    def validate_probability(prob: float) -> bool:
        """Validate probability is between 0 and 1"""
        return 0 <= prob <= 1

    @staticmethod
    def validate_positive(value: float, name: str) -> bool:
        """Validate value is positive"""
        if value <= 0:
            raise ValidationError(f"{name} must be positive, got {value}")
        return True

    @staticmethod
    def validate_non_negative(value: float, name: str) -> bool:
        """Validate value is non-negative"""
        if value < 0:
            raise ValidationError(f"{name} cannot be negative, got {value}")
        return True

    @staticmethod
    def validate_rate(rate: float, name: str) -> bool:
        """Validate interest rate (can be negative in modern markets)"""
        if abs(rate) > 1.0:  # More than 100% is suspicious
            logger.warning(f"{name} is unusually high: {rate * 100:.2f}%")
        return True

    @staticmethod
    def validate_volatility(vol: float) -> bool:
        """Validate volatility parameter"""
        if vol < 0:
            raise ValidationError(f"Volatility cannot be negative, got {vol}")
        if vol > 5.0:  # 500% volatility is extreme
            logger.warning(f"Volatility is extremely high: {vol * 100:.2f}%")
        return True


class Constants:
    """Mathematical and financial constants"""

    # Numerical precision
    EPSILON = 1e-10
    MAX_ITERATIONS = 10000

    # Financial constants
    TRADING_DAYS_PER_YEAR = 252
    CALENDAR_DAYS_PER_YEAR = 365.25

    # Default model parameters
    DEFAULT_RISK_FREE_RATE = 0.02
    DEFAULT_VOLATILITY = 0.20
    DEFAULT_DIVIDEND_YIELD = 0.0

    # Greeks calculation parameters
    BUMP_SIZE = 0.01  # 1% for delta, gamma calculations
    VOL_BUMP = 0.01  # 1% for vega calculations
    TIME_BUMP = 1 / 365  # 1 day for theta calculations


def calculate_time_fraction(start_date: datetime,
                            end_date: datetime,
                            day_count: DayCountConvention = DayCountConvention.ACT_365) -> float:
    """
    Calculate time fraction between dates using specified day count convention

    Args:
        start_date: Start date
        end_date: End date
        day_count: Day count convention

    Returns:
        Time fraction in years
    """
    if end_date <= start_date:
        return 0.0

    time_diff = end_date - start_date

    if day_count == DayCountConvention.ACT_365:
        return time_diff.total_seconds() / (365.25 * 24 * 3600)
    elif day_count == DayCountConvention.ACT_360:
        return time_diff.total_seconds() / (360 * 24 * 3600)
    elif day_count == DayCountConvention.THIRTY_360:
        return time_diff.days / 360
    else:  # ACT_ACT
        return time_diff.total_seconds() / (365.25 * 24 * 3600)


def risk_free_rate_converter(rate: float,
                             from_compounding: str = "continuous",
                             to_compounding: str = "continuous",
                             frequency: int = 1) -> float:
    """
    Convert between different interest rate compounding conventions

    Args:
        rate: Input interest rate
        from_compounding: Source compounding ('continuous', 'annual', 'discrete')
        to_compounding: Target compounding ('continuous', 'annual', 'discrete')
        frequency: Compounding frequency for discrete rates

    Returns:
        Converted interest rate
    """
    # Convert to continuous first
    if from_compounding == "continuous":
        continuous_rate = rate
    elif from_compounding == "annual":
        continuous_rate = np.log(1 + rate)
    elif from_compounding == "discrete":
        continuous_rate = frequency * np.log(1 + rate / frequency)
    else:
        raise ValueError(f"Unknown compounding type: {from_compounding}")

    # Convert from continuous to target
    if to_compounding == "continuous":
        return continuous_rate
    elif to_compounding == "annual":
        return np.exp(continuous_rate) - 1
    elif to_compounding == "discrete":
        return frequency * (np.exp(continuous_rate / frequency) - 1)
    else:
        raise ValueError(f"Unknown compounding type: {to_compounding}")


# Export main classes and functions
__all__ = [
    'DerivativeType', 'OptionType', 'Position', 'ExerciseStyle', 'UnderlyingType',
    'DayCountConvention', 'MarketData', 'PricingResult', 'ValidationError', 'PricingError',
    'DerivativeInstrument', 'ForwardCommitment', 'ContingentClaim', 'PricingEngine',
    'ModelValidator', 'Constants', 'calculate_time_fraction', 'risk_free_rate_converter'
]