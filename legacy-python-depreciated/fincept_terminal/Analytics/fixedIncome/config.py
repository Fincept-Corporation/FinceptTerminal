"""
Fixed Income Analytics - Configuration Module
CFA Level I & II Compliant Configuration System
"""

from enum import Enum
from decimal import Decimal, getcontext
from typing import Dict, List, Optional, Union
import datetime

# Set high precision for financial calculations
getcontext().prec = 28


class DayCountConvention(Enum):
    """Day count conventions used in fixed income markets"""
    ACTUAL_360 = "Actual/360"
    ACTUAL_365 = "Actual/365"
    ACTUAL_ACTUAL = "Actual/Actual"
    THIRTY_360 = "30/360"
    THIRTY_360_EUROPEAN = "30E/360"
    ACTUAL_365_FIXED = "Actual/365 Fixed"
    NL_365 = "NL/365"


class CompoundingFrequency(Enum):
    """Compounding frequency conventions"""
    CONTINUOUS = 0
    ANNUAL = 1
    SEMI_ANNUAL = 2
    QUARTERLY = 4
    MONTHLY = 12
    DAILY = 365


class BusinessDayConvention(Enum):
    """Business day adjustment conventions"""
    FOLLOWING = "Following"
    MODIFIED_FOLLOWING = "Modified Following"
    PRECEDING = "Preceding"
    MODIFIED_PRECEDING = "Modified Preceding"
    UNADJUSTED = "Unadjusted"


class Currency(Enum):
    """Major currencies for fixed income markets"""
    USD = "USD"
    EUR = "EUR"
    GBP = "GBP"
    JPY = "JPY"
    CHF = "CHF"
    CAD = "CAD"
    AUD = "AUD"
    CNY = "CNY"


class CreditRating(Enum):
    """Standard credit rating scale (S&P equivalent)"""
    AAA = "AAA"
    AA_PLUS = "AA+"
    AA = "AA"
    AA_MINUS = "AA-"
    A_PLUS = "A+"
    A = "A"
    A_MINUS = "A-"
    BBB_PLUS = "BBB+"
    BBB = "BBB"
    BBB_MINUS = "BBB-"
    BB_PLUS = "BB+"
    BB = "BB"
    BB_MINUS = "BB-"
    B_PLUS = "B+"
    B = "B"
    B_MINUS = "B-"
    CCC_PLUS = "CCC+"
    CCC = "CCC"
    CCC_MINUS = "CCC-"
    CC = "CC"
    C = "C"
    D = "D"


class BondType(Enum):
    """Bond instrument types"""
    FIXED_RATE = "Fixed Rate"
    FLOATING_RATE = "Floating Rate"
    ZERO_COUPON = "Zero Coupon"
    CALLABLE = "Callable"
    PUTABLE = "Putable"
    CONVERTIBLE = "Convertible"
    INFLATION_LINKED = "Inflation Linked"
    PERPETUAL = "Perpetual"


class SecuritizationType(Enum):
    """Securitized product types"""
    MORTGAGE_PASS_THROUGH = "Mortgage Pass-Through"
    CMO = "Collateralized Mortgage Obligation"
    ABS = "Asset-Backed Security"
    CDO = "Collateralized Debt Obligation"
    COVERED_BOND = "Covered Bond"


# Market Conventions by Currency
MARKET_CONVENTIONS = {
    Currency.USD: {
        'day_count': DayCountConvention.THIRTY_360,
        'settlement_days': 3,
        'compounding': CompoundingFrequency.SEMI_ANNUAL,
        'business_day_convention': BusinessDayConvention.MODIFIED_FOLLOWING
    },
    Currency.EUR: {
        'day_count': DayCountConvention.ACTUAL_ACTUAL,
        'settlement_days': 3,
        'compounding': CompoundingFrequency.ANNUAL,
        'business_day_convention': BusinessDayConvention.MODIFIED_FOLLOWING
    },
    Currency.GBP: {
        'day_count': DayCountConvention.ACTUAL_365,
        'settlement_days': 3,
        'compounding': CompoundingFrequency.SEMI_ANNUAL,
        'business_day_convention': BusinessDayConvention.MODIFIED_FOLLOWING
    },
    Currency.JPY: {
        'day_count': DayCountConvention.ACTUAL_365,
        'settlement_days': 3,
        'compounding': CompoundingFrequency.SEMI_ANNUAL,
        'business_day_convention': BusinessDayConvention.MODIFIED_FOLLOWING
    }
}

# Numerical Precision Settings
PRECISION_CONFIG = {
    'decimal_places': 8,
    'price_precision': 6,
    'yield_precision': 4,
    'spread_precision': 1,
    'duration_precision': 4,
    'convergence_tolerance': Decimal('1e-10'),
    'max_iterations': 1000
}

# Default Calculation Parameters
DEFAULT_PARAMS = {
    'yield_calculation_method': 'newton_raphson',
    'interpolation_method': 'cubic_spline',
    'volatility_model': 'black_scholes',
    'credit_model': 'merton',
    'monte_carlo_paths': 10000,
    'binomial_steps': 100
}

# Credit Rating Numerical Mapping (for calculations)
RATING_SCORES = {
    CreditRating.AAA: 1,
    CreditRating.AA_PLUS: 2,
    CreditRating.AA: 3,
    CreditRating.AA_MINUS: 4,
    CreditRating.A_PLUS: 5,
    CreditRating.A: 6,
    CreditRating.A_MINUS: 7,
    CreditRating.BBB_PLUS: 8,
    CreditRating.BBB: 9,
    CreditRating.BBB_MINUS: 10,
    CreditRating.BB_PLUS: 11,
    CreditRating.BB: 12,
    CreditRating.BB_MINUS: 13,
    CreditRating.B_PLUS: 14,
    CreditRating.B: 15,
    CreditRating.B_MINUS: 16,
    CreditRating.CCC_PLUS: 17,
    CreditRating.CCC: 18,
    CreditRating.CCC_MINUS: 19,
    CreditRating.CC: 20,
    CreditRating.C: 21,
    CreditRating.D: 22
}

# Investment Grade Threshold
INVESTMENT_GRADE_THRESHOLD = RATING_SCORES[CreditRating.BBB_MINUS]

# Standard Treasury Maturities (in years)
TREASURY_MATURITIES = [
    Decimal('0.25'),  # 3 months
    Decimal('0.5'),  # 6 months
    Decimal('1'),  # 1 year
    Decimal('2'),  # 2 years
    Decimal('3'),  # 3 years
    Decimal('5'),  # 5 years
    Decimal('7'),  # 7 years
    Decimal('10'),  # 10 years
    Decimal('20'),  # 20 years
    Decimal('30')  # 30 years
]

# Swap Curve Standard Tenors
SWAP_TENORS = [
    '1M', '3M', '6M', '1Y', '2Y', '3Y', '4Y', '5Y',
    '7Y', '10Y', '12Y', '15Y', '20Y', '25Y', '30Y'
]

# Credit Default Swap Standard Tenors
CDS_TENORS = ['6M', '1Y', '2Y', '3Y', '4Y', '5Y', '7Y', '10Y']

# Holidays (basic implementation - can be extended)
US_HOLIDAYS = [
    # New Year's Day, Martin Luther King Jr. Day, Presidents Day,
    # Memorial Day, Independence Day, Labor Day, Columbus Day,
    # Veterans Day, Thanksgiving, Christmas Day
]

# Data Provider Configuration
DATA_PROVIDERS = {
    'bloomberg': {
        'base_url': 'https://api.bloomberg.com',
        'fields': ['PX_LAST', 'YLD_YTM_MID', 'DUR_ADJ_MID'],
        'rate_limit': 1000  # requests per hour
    },
    'refinitiv': {
        'base_url': 'https://api.refinitiv.com',
        'fields': ['TR.PriceClose', 'TR.YieldToMaturity'],
        'rate_limit': 500
    },
    'fred': {
        'base_url': 'https://api.stlouisfed.org/fred',
        'series': ['DGS1MO', 'DGS3MO', 'DGS6MO', 'DGS1', 'DGS2', 'DGS5', 'DGS10', 'DGS30'],
        'rate_limit': 120
    },
    'yahoo': {
        'base_url': 'https://query1.finance.yahoo.com',
        'rate_limit': 2000
    }
}

# Error Tolerance Levels
ERROR_TOLERANCE = {
    'low': Decimal('1e-6'),
    'medium': Decimal('1e-8'),
    'high': Decimal('1e-10'),
    'ultra_high': Decimal('1e-12')
}

# Validation Rules
VALIDATION_RULES = {
    'min_yield': Decimal('-0.10'),  # -10% minimum yield
    'max_yield': Decimal('1.00'),  # 100% maximum yield
    'min_price': Decimal('0.01'),  # Minimum bond price
    'max_price': Decimal('10.00'),  # Maximum bond price (1000%)
    'min_maturity_days': 1,
    'max_maturity_years': 100,
    'min_coupon_rate': Decimal('0.00'),
    'max_coupon_rate': Decimal('1.00')
}

# Model Parameters
MODEL_PARAMS = {
    'black_scholes': {
        'risk_free_rate': Decimal('0.02'),
        'volatility': Decimal('0.20')
    },
    'vasicek': {
        'mean_reversion_speed': Decimal('0.1'),
        'long_term_mean': Decimal('0.05'),
        'volatility': Decimal('0.01')
    },
    'hull_white': {
        'mean_reversion_speed': Decimal('0.1'),
        'volatility': Decimal('0.01')
    }
}


def get_market_convention(currency: Currency) -> Dict:
    """Get market conventions for a specific currency"""
    return MARKET_CONVENTIONS.get(currency, MARKET_CONVENTIONS[Currency.USD])


def is_investment_grade(rating: CreditRating) -> bool:
    """Check if a credit rating is investment grade"""
    return RATING_SCORES[rating] <= INVESTMENT_GRADE_THRESHOLD


def get_precision(calc_type: str) -> int:
    """Get precision for specific calculation type"""
    precision_map = {
        'price': PRECISION_CONFIG['price_precision'],
        'yield': PRECISION_CONFIG['yield_precision'],
        'spread': PRECISION_CONFIG['spread_precision'],
        'duration': PRECISION_CONFIG['duration_precision']
    }
    return precision_map.get(calc_type, PRECISION_CONFIG['decimal_places'])


def get_tolerance(level: str = 'medium') -> Decimal:
    """Get error tolerance for calculations"""
    return ERROR_TOLERANCE.get(level, ERROR_TOLERANCE['medium'])


class ConfigurationError(Exception):
    """Custom exception for configuration errors"""
    pass


def validate_configuration():
    """Validate configuration settings on module load"""
    if getcontext().prec < 20:
        raise ConfigurationError("Decimal precision too low for financial calculations")

    # Validate that all required configurations are present
    required_configs = ['MARKET_CONVENTIONS', 'PRECISION_CONFIG', 'DEFAULT_PARAMS']
    for config in required_configs:
        if config not in globals():
            raise ConfigurationError(f"Missing required configuration: {config}")


# Run validation on import
validate_configuration()