"""
Alternative Investments Configuration Module
============================================

Centralized configuration management for alternative investments analytics platform.
Provides constants, validation rules, enums, and parameter settings compliant
with CFA Institute standards for alternative investment analysis across all
asset classes including private equity, real estate, hedge funds, and digital assets.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Configuration files (JSON/YAML) for asset parameters
  - Validation rule definitions for data quality checks
  - Risk parameters and tolerance settings
  - Market data source configurations
  - Currency and exchange rate settings

OUTPUT:
  - Standardized configuration objects
  - Validated parameters for analytics calculations
  - Asset class specific settings and constraints
  - Risk management configuration parameters
  - Data source connection settings

PARAMETERS:
  - default_currency: Base currency for calculations (default: 'USD')
  - decimal_precision: Precision for financial calculations (default: 8)
  - max_lookback_years: Maximum historical data lookback (default: 20)
  - min_data_quality_score: Minimum data quality threshold (default: 0.8)
  - risk_free_rate: Default risk-free rate (default: 0.02)
  - confidence_levels: VaR confidence levels (default: [0.90, 0.95, 0.99])
"""

from decimal import Decimal, getcontext
from typing import Dict, List, Any, Optional
from dataclasses import dataclass
from enum import Enum
import logging

# Set high precision for financial calculations
getcontext().prec = 28


# ============================================================================
# CALCULATION CONSTANTS
# ============================================================================

class Constants:
    """Mathematical and financial constants"""
    DAYS_IN_YEAR = Decimal('365.25')
    BUSINESS_DAYS_IN_YEAR = Decimal('252')
    MONTHS_IN_YEAR = Decimal('12')
    QUARTERS_IN_YEAR = Decimal('4')
    BASIS_POINTS = Decimal('10000')
    PERCENT = Decimal('100')

    # Risk-free rates (update as needed)
    DEFAULT_RISK_FREE_RATE = Decimal('0.03')  # 3%

    # Alternative investment specific
    PE_TYPICAL_FUND_LIFE = 10  # years
    RE_DEPRECIATION_YEARS = 39  # US commercial real estate
    COMMODITY_STORAGE_COST_TYPICAL = Decimal('0.02')  # 2%


# ============================================================================
# ENUMS AND CLASSIFICATIONS
# ============================================================================

class AssetClass(Enum):
    """Alternative investment asset classes"""
    PRIVATE_EQUITY = "private_equity"
    PRIVATE_DEBT = "private_debt"
    REAL_ESTATE = "real_estate"
    REIT = "reit"
    INFRASTRUCTURE = "infrastructure"
    COMMODITIES = "commodities"
    TIMBERLAND = "timberland"
    FARMLAND = "farmland"
    RAW_LAND = "raw_land"
    HEDGE_FUND = "hedge_fund"
    DIGITAL_ASSETS = "digital_assets"


class InvestmentMethod(Enum):
    """Investment access methods"""
    DIRECT = "direct"
    CO_INVESTMENT = "co_investment"
    FUND = "fund"


class HedgeFundStrategy(Enum):
    """Hedge fund strategy classifications"""
    # Equity Related
    LONG_SHORT_EQUITY = "long_short_equity"
    EQUITY_MARKET_NEUTRAL = "equity_market_neutral"
    DEDICATED_SHORT_BIAS = "dedicated_short_bias"

    # Event Driven
    MERGER_ARBITRAGE = "merger_arbitrage"
    DISTRESSED_SECURITIES = "distressed_securities"
    ACTIVIST = "activist"
    SPECIAL_SITUATIONS = "special_situations"

    # Relative Value
    FIXED_INCOME_ARBITRAGE = "fixed_income_arbitrage"
    CONVERTIBLE_ARBITRAGE = "convertible_arbitrage"
    ASSET_BACKED_SECURITIES = "asset_backed_securities"
    VOLATILITY_ARBITRAGE = "volatility_arbitrage"

    # Opportunistic
    GLOBAL_MACRO = "global_macro"
    CTA_MANAGED_FUTURES = "cta_managed_futures"

    # Specialist
    REINSURANCE = "reinsurance"
    STRUCTURED_CREDIT = "structured_credit"

    # Multi-Manager
    MULTI_STRATEGY = "multi_strategy"
    FUND_OF_FUNDS = "fund_of_funds"


class CommoditySector(Enum):
    """Commodity sector classifications"""
    ENERGY = "energy"
    METALS = "metals"
    AGRICULTURE = "agriculture"
    LIVESTOCK = "livestock"


class RealEstateType(Enum):
    """Real estate property types"""
    OFFICE = "office"
    RETAIL = "retail"
    INDUSTRIAL = "industrial"
    MULTIFAMILY = "multifamily"
    HOTEL = "hotel"
    MIXED_USE = "mixed_use"
    LAND = "land"


# ============================================================================
# DATA SCHEMAS
# ============================================================================

@dataclass
class AssetParameters:
    """Standard parameters for alternative investments"""
    asset_class: AssetClass
    ticker: Optional[str] = None
    name: Optional[str] = None
    currency: str = "USD"
    inception_date: Optional[str] = None
    management_fee: Optional[Decimal] = None
    performance_fee: Optional[Decimal] = None
    hurdle_rate: Optional[Decimal] = None
    high_water_mark: bool = True
    lock_up_period: Optional[int] = None  # months
    redemption_frequency: Optional[str] = None
    minimum_investment: Optional[Decimal] = None


@dataclass
class MarketData:
    """Standardized market data structure"""
    timestamp: str
    price: Decimal
    volume: Optional[Decimal] = None
    bid: Optional[Decimal] = None
    ask: Optional[Decimal] = None
    high: Optional[Decimal] = None
    low: Optional[Decimal] = None
    open: Optional[Decimal] = None
    close: Optional[Decimal] = None


@dataclass
class CashFlow:
    """Cash flow data structure"""
    date: str
    amount: Decimal
    cf_type: str  # 'inflow', 'outflow', 'distribution', 'capital_call'
    description: Optional[str] = None


@dataclass
class Performance:
    """Performance metrics structure"""
    period: str
    total_return: Decimal
    annualized_return: Optional[Decimal] = None
    volatility: Optional[Decimal] = None
    sharpe_ratio: Optional[Decimal] = None
    max_drawdown: Optional[Decimal] = None
    benchmark_return: Optional[Decimal] = None
    alpha: Optional[Decimal] = None
    beta: Optional[Decimal] = None


# ============================================================================
# CONFIGURATION SETTINGS
# ============================================================================

class Config:
    """Main configuration class"""

    # Data validation settings
    PRICE_TOLERANCE = Decimal('0.0001')  # 1 basis point
    MAX_LEVERAGE = Decimal('10.0')
    MIN_PRICE = Decimal('0.0001')

    # Performance calculation settings
    ANNUALIZATION_FACTOR = Constants.DAYS_IN_YEAR
    RISK_FREE_RATE = Constants.DEFAULT_RISK_FREE_RATE

    # Alternative investment specific settings
    PE_IRR_TOLERANCE = Decimal('0.000001')
    PE_IRR_MAX_ITERATIONS = 1000

    # Real estate settings
    RE_CAP_RATE_MIN = Decimal('0.01')  # 1%
    RE_CAP_RATE_MAX = Decimal('0.20')  # 20%

    # Commodity settings
    COMMODITY_ROLL_DAYS = 5  # Days before expiry to roll

    # Hedge fund settings
    HF_HIGH_WATER_MARK_DEFAULT = True
    HF_HURDLE_RATE_DEFAULT = Decimal('0.08')  # 8%

    # Digital assets settings
    CRYPTO_VOLATILITY_FLOOR = Decimal('0.10')  # 10% minimum volatility

    # Portfolio settings
    MAX_CONCENTRATION = Decimal('0.50')  # 50% max in single asset
    MIN_WEIGHT = Decimal('0.001')  # 0.1% minimum weight

    # Reporting settings
    DECIMAL_PLACES = 4
    PERCENTAGE_DECIMAL_PLACES = 2

    @classmethod
    def get_asset_defaults(cls, asset_class: AssetClass) -> Dict[str, Any]:
        """Get default parameters for asset class"""
        defaults = {
            AssetClass.PRIVATE_EQUITY: {
                'management_fee': Decimal('0.02'),  # 2%
                'performance_fee': Decimal('0.20'),  # 20%
                'lock_up_period': 120,  # 10 years
                'minimum_investment': Decimal('1000000')  # $1M
            },
            AssetClass.PRIVATE_DEBT: {
                'management_fee': Decimal('0.015'),  # 1.5%
                'performance_fee': Decimal('0.10'),  # 10%
                'lock_up_period': 60,  # 5 years
                'minimum_investment': Decimal('250000')  # $250K
            },
            AssetClass.REAL_ESTATE: {
                'management_fee': Decimal('0.01'),  # 1%
                'performance_fee': Decimal('0.15'),  # 15%
                'minimum_investment': Decimal('50000')  # $50K
            },
            AssetClass.HEDGE_FUND: {
                'management_fee': Decimal('0.02'),  # 2%
                'performance_fee': Decimal('0.20'),  # 20%
                'hurdle_rate': Decimal('0.08'),  # 8%
                'high_water_mark': True,
                'minimum_investment': Decimal('100000')  # $100K
            },
            AssetClass.COMMODITIES: {
                'management_fee': Decimal('0.005'),  # 0.5%
                'minimum_investment': Decimal('10000')  # $10K
            },
            AssetClass.DIGITAL_ASSETS: {
                'management_fee': Decimal('0.01'),  # 1%
                'minimum_investment': Decimal('1000')  # $1K
            }
        }
        return defaults.get(asset_class, {})


# ============================================================================
# LOGGING CONFIGURATION
# ============================================================================

def setup_logging(level: str = "INFO") -> logging.Logger:
    """Setup logging for the analytics module"""
    logging.basicConfig(
        level=getattr(logging, level.upper()),
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )

    logger = logging.getLogger('alternative_investments')
    return logger


# ============================================================================
# VALIDATION RULES
# ============================================================================

class ValidationRules:
    """Validation rules for different asset classes"""

    @staticmethod
    def validate_performance_fee(fee: Decimal, asset_class: AssetClass) -> bool:
        """Validate performance fee ranges"""
        if asset_class == AssetClass.PRIVATE_EQUITY:
            return Decimal('0.15') <= fee <= Decimal('0.30')  # 15-30%
        elif asset_class == AssetClass.HEDGE_FUND:
            return Decimal('0.10') <= fee <= Decimal('0.50')  # 10-50%
        elif asset_class == AssetClass.REAL_ESTATE:
            return Decimal('0.05') <= fee <= Decimal('0.25')  # 5-25%
        return True

    @staticmethod
    def validate_management_fee(fee: Decimal, asset_class: AssetClass) -> bool:
        """Validate management fee ranges"""
        return Decimal('0.001') <= fee <= Decimal('0.05')  # 0.1-5%

    @staticmethod
    def validate_return(return_value: Decimal) -> bool:
        """Validate return values"""
        return Decimal('-0.99') <= return_value <= Decimal('10.0')  # -99% to 1000%

    @staticmethod
    def validate_volatility(vol: Decimal) -> bool:
        """Validate volatility values"""
        return Decimal('0.001') <= vol <= Decimal('5.0')  # 0.1% to 500%


# Export main components
__all__ = [
    'Constants', 'AssetClass', 'InvestmentMethod', 'HedgeFundStrategy',
    'CommoditySector', 'RealEstateType', 'AssetParameters', 'MarketData',
    'CashFlow', 'Performance', 'Config', 'ValidationRules', 'setup_logging'
]