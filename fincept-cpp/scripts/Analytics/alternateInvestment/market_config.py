"""
Market Configuration Module

Global market-specific parameters for alternative investment analytics.
Supports multiple regions with different trading calendars, tax regimes, and regulatory frameworks.
"""

from decimal import Decimal
from typing import Dict, Any
from dataclasses import dataclass
from enum import Enum


class MarketRegion(Enum):
    """Global market regions"""
    # Americas
    UNITED_STATES = "US"
    CANADA = "CA"
    BRAZIL = "BR"
    MEXICO = "MX"
    CHILE = "CL"
    ARGENTINA = "AR"

    # Europe
    UNITED_KINGDOM = "GB"
    GERMANY = "DE"
    FRANCE = "FR"
    SWITZERLAND = "CH"
    NETHERLANDS = "NL"
    SPAIN = "ES"
    ITALY = "IT"
    SWEDEN = "SE"
    NORWAY = "NO"

    # Asia Pacific
    JAPAN = "JP"
    CHINA = "CN"
    HONG_KONG = "HK"
    SINGAPORE = "SG"
    SOUTH_KOREA = "KR"
    INDIA = "IN"
    AUSTRALIA = "AU"
    NEW_ZEALAND = "NZ"
    TAIWAN = "TW"
    THAILAND = "TH"
    MALAYSIA = "MY"
    INDONESIA = "ID"
    PHILIPPINES = "PH"
    VIETNAM = "VN"

    # Middle East & Africa
    UAE = "AE"
    SAUDI_ARABIA = "SA"
    QATAR = "QA"
    KUWAIT = "KW"
    BAHRAIN = "BH"
    ISRAEL = "IL"
    TURKEY = "TR"
    SOUTH_AFRICA = "ZA"
    EGYPT = "EG"
    MOROCCO = "MA"
    NIGERIA = "NG"
    KENYA = "KE"

    # Global/Multi-Region
    GLOBAL = "GLOBAL"
    EMERGING_MARKETS = "EM"
    DEVELOPED_MARKETS = "DM"


@dataclass
class MarketParameters:
    """Market-specific parameters for analytics"""

    # Basic Info
    region: MarketRegion
    currency: str

    # Trading Calendar
    business_days_per_year: int
    holidays_per_year: int

    # Tax & Regulatory
    corporate_tax_rate: Decimal
    capital_gains_tax_rate: Decimal
    withholding_tax_rate: Decimal
    real_estate_depreciation_years: int
    vat_gst_rate: Decimal

    # Market Characteristics
    typical_risk_free_rate: Decimal
    equity_risk_premium: Decimal
    real_estate_cap_rate_range: tuple  # (min, max)
    commodity_storage_cost: Decimal

    # Regulatory Framework
    reporting_standard: str  # "IFRS", "US_GAAP", "LOCAL"
    regulatory_authority: str
    requires_registration: bool

    # Market Access
    foreign_ownership_allowed: bool
    foreign_ownership_limit: Decimal  # 1.0 = 100%
    capital_controls: bool

    # Trading Infrastructure
    settlement_cycle: int  # T+N days
    market_maker_system: bool
    derivatives_market: bool


# ============================================================================
# MARKET CONFIGURATIONS
# ============================================================================

MARKET_CONFIGS: Dict[MarketRegion, MarketParameters] = {

    # ========== AMERICAS ==========

    MarketRegion.UNITED_STATES: MarketParameters(
        region=MarketRegion.UNITED_STATES,
        currency="USD",
        business_days_per_year=252,
        holidays_per_year=10,
        corporate_tax_rate=Decimal('0.21'),
        capital_gains_tax_rate=Decimal('0.20'),
        withholding_tax_rate=Decimal('0.30'),
        real_estate_depreciation_years=39,  # Commercial
        vat_gst_rate=Decimal('0.00'),  # No federal VAT
        typical_risk_free_rate=Decimal('0.04'),
        equity_risk_premium=Decimal('0.06'),
        real_estate_cap_rate_range=(Decimal('0.04'), Decimal('0.10')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="US_GAAP",
        regulatory_authority="SEC",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.CANADA: MarketParameters(
        region=MarketRegion.CANADA,
        currency="CAD",
        business_days_per_year=252,
        holidays_per_year=9,
        corporate_tax_rate=Decimal('0.26'),
        capital_gains_tax_rate=Decimal('0.13'),  # 50% inclusion rate
        withholding_tax_rate=Decimal('0.25'),
        real_estate_depreciation_years=40,
        vat_gst_rate=Decimal('0.05'),  # GST
        typical_risk_free_rate=Decimal('0.035'),
        equity_risk_premium=Decimal('0.055'),
        real_estate_cap_rate_range=(Decimal('0.045'), Decimal('0.095')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="CSA",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    # ========== EUROPE ==========

    MarketRegion.UNITED_KINGDOM: MarketParameters(
        region=MarketRegion.UNITED_KINGDOM,
        currency="GBP",
        business_days_per_year=252,
        holidays_per_year=8,
        corporate_tax_rate=Decimal('0.25'),
        capital_gains_tax_rate=Decimal('0.20'),
        withholding_tax_rate=Decimal('0.20'),
        real_estate_depreciation_years=50,
        vat_gst_rate=Decimal('0.20'),
        typical_risk_free_rate=Decimal('0.04'),
        equity_risk_premium=Decimal('0.055'),
        real_estate_cap_rate_range=(Decimal('0.04'), Decimal('0.09')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="FCA",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.GERMANY: MarketParameters(
        region=MarketRegion.GERMANY,
        currency="EUR",
        business_days_per_year=252,
        holidays_per_year=9,
        corporate_tax_rate=Decimal('0.30'),  # Including trade tax
        capital_gains_tax_rate=Decimal('0.26'),  # Abgeltungsteuer
        withholding_tax_rate=Decimal('0.26'),
        real_estate_depreciation_years=50,
        vat_gst_rate=Decimal('0.19'),
        typical_risk_free_rate=Decimal('0.03'),
        equity_risk_premium=Decimal('0.055'),
        real_estate_cap_rate_range=(Decimal('0.035'), Decimal('0.085')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="BaFin",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    # ========== ASIA PACIFIC ==========

    MarketRegion.JAPAN: MarketParameters(
        region=MarketRegion.JAPAN,
        currency="JPY",
        business_days_per_year=245,  # Fewer than US
        holidays_per_year=15,
        corporate_tax_rate=Decimal('0.30'),
        capital_gains_tax_rate=Decimal('0.20'),
        withholding_tax_rate=Decimal('0.20'),
        real_estate_depreciation_years=47,
        vat_gst_rate=Decimal('0.10'),  # Consumption tax
        typical_risk_free_rate=Decimal('0.005'),  # Very low
        equity_risk_premium=Decimal('0.06'),
        real_estate_cap_rate_range=(Decimal('0.03'), Decimal('0.06')),
        commodity_storage_cost=Decimal('0.025'),
        reporting_standard="IFRS",
        regulatory_authority="FSA",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.CHINA: MarketParameters(
        region=MarketRegion.CHINA,
        currency="CNY",
        business_days_per_year=242,
        holidays_per_year=11,
        corporate_tax_rate=Decimal('0.25'),
        capital_gains_tax_rate=Decimal('0.20'),
        withholding_tax_rate=Decimal('0.10'),
        real_estate_depreciation_years=70,  # Land lease
        vat_gst_rate=Decimal('0.13'),
        typical_risk_free_rate=Decimal('0.03'),
        equity_risk_premium=Decimal('0.08'),
        real_estate_cap_rate_range=(Decimal('0.04'), Decimal('0.08')),
        commodity_storage_cost=Decimal('0.025'),
        reporting_standard="LOCAL",
        regulatory_authority="CSRC",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('0.30'),  # QFII limits
        capital_controls=True,
        settlement_cycle=1,
        market_maker_system=False,
        derivatives_market=True
    ),

    MarketRegion.HONG_KONG: MarketParameters(
        region=MarketRegion.HONG_KONG,
        currency="HKD",
        business_days_per_year=248,
        holidays_per_year=17,
        corporate_tax_rate=Decimal('0.165'),  # Low tax
        capital_gains_tax_rate=Decimal('0.00'),  # No CGT!
        withholding_tax_rate=Decimal('0.00'),
        real_estate_depreciation_years=50,
        vat_gst_rate=Decimal('0.00'),  # No VAT
        typical_risk_free_rate=Decimal('0.03'),
        equity_risk_premium=Decimal('0.065'),
        real_estate_cap_rate_range=(Decimal('0.025'), Decimal('0.05')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="SFC",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.SINGAPORE: MarketParameters(
        region=MarketRegion.SINGAPORE,
        currency="SGD",
        business_days_per_year=250,
        holidays_per_year=11,
        corporate_tax_rate=Decimal('0.17'),
        capital_gains_tax_rate=Decimal('0.00'),  # No CGT
        withholding_tax_rate=Decimal('0.15'),
        real_estate_depreciation_years=60,
        vat_gst_rate=Decimal('0.09'),  # GST
        typical_risk_free_rate=Decimal('0.03'),
        equity_risk_premium=Decimal('0.06'),
        real_estate_cap_rate_range=(Decimal('0.03'), Decimal('0.06')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="MAS",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.INDIA: MarketParameters(
        region=MarketRegion.INDIA,
        currency="INR",
        business_days_per_year=250,
        holidays_per_year=14,
        corporate_tax_rate=Decimal('0.25'),
        capital_gains_tax_rate=Decimal('0.10'),  # Long-term
        withholding_tax_rate=Decimal('0.20'),
        real_estate_depreciation_years=60,
        vat_gst_rate=Decimal('0.18'),  # GST
        typical_risk_free_rate=Decimal('0.06'),
        equity_risk_premium=Decimal('0.08'),
        real_estate_cap_rate_range=(Decimal('0.06'), Decimal('0.12')),
        commodity_storage_cost=Decimal('0.03'),
        reporting_standard="IFRS",
        regulatory_authority="SEBI",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('0.74'),  # FDI limits
        capital_controls=True,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.AUSTRALIA: MarketParameters(
        region=MarketRegion.AUSTRALIA,
        currency="AUD",
        business_days_per_year=252,
        holidays_per_year=9,
        corporate_tax_rate=Decimal('0.30'),
        capital_gains_tax_rate=Decimal('0.24'),  # 50% discount
        withholding_tax_rate=Decimal('0.30'),
        real_estate_depreciation_years=40,
        vat_gst_rate=Decimal('0.10'),  # GST
        typical_risk_free_rate=Decimal('0.035'),
        equity_risk_premium=Decimal('0.06'),
        real_estate_cap_rate_range=(Decimal('0.045'), Decimal('0.09')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="ASIC",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    # ========== MIDDLE EAST & AFRICA ==========

    MarketRegion.UAE: MarketParameters(
        region=MarketRegion.UAE,
        currency="AED",
        business_days_per_year=250,  # Friday/Saturday weekend
        holidays_per_year=12,
        corporate_tax_rate=Decimal('0.09'),  # New CT from 2023
        capital_gains_tax_rate=Decimal('0.00'),
        withholding_tax_rate=Decimal('0.00'),
        real_estate_depreciation_years=50,
        vat_gst_rate=Decimal('0.05'),
        typical_risk_free_rate=Decimal('0.04'),
        equity_risk_premium=Decimal('0.07'),
        real_estate_cap_rate_range=(Decimal('0.05'), Decimal('0.09')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="SCA",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),  # 100% since 2020
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.SAUDI_ARABIA: MarketParameters(
        region=MarketRegion.SAUDI_ARABIA,
        currency="SAR",
        business_days_per_year=250,
        holidays_per_year=12,
        corporate_tax_rate=Decimal('0.20'),
        capital_gains_tax_rate=Decimal('0.00'),
        withholding_tax_rate=Decimal('0.05'),
        real_estate_depreciation_years=50,
        vat_gst_rate=Decimal('0.15'),
        typical_risk_free_rate=Decimal('0.045'),
        equity_risk_premium=Decimal('0.07'),
        real_estate_cap_rate_range=(Decimal('0.06'), Decimal('0.10')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="CMA",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),

    MarketRegion.SOUTH_AFRICA: MarketParameters(
        region=MarketRegion.SOUTH_AFRICA,
        currency="ZAR",
        business_days_per_year=252,
        holidays_per_year=12,
        corporate_tax_rate=Decimal('0.27'),
        capital_gains_tax_rate=Decimal('0.18'),
        withholding_tax_rate=Decimal('0.20'),
        real_estate_depreciation_years=40,
        vat_gst_rate=Decimal('0.15'),
        typical_risk_free_rate=Decimal('0.07'),
        equity_risk_premium=Decimal('0.08'),
        real_estate_cap_rate_range=(Decimal('0.08'), Decimal('0.13')),
        commodity_storage_cost=Decimal('0.025'),
        reporting_standard="IFRS",
        regulatory_authority="FSB",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=True,
        settlement_cycle=3,
        market_maker_system=True,
        derivatives_market=True
    ),

    # ========== GLOBAL ==========

    MarketRegion.GLOBAL: MarketParameters(
        region=MarketRegion.GLOBAL,
        currency="USD",  # Default to USD for global
        business_days_per_year=252,
        holidays_per_year=10,
        corporate_tax_rate=Decimal('0.25'),  # Average
        capital_gains_tax_rate=Decimal('0.15'),  # Average
        withholding_tax_rate=Decimal('0.15'),
        real_estate_depreciation_years=40,
        vat_gst_rate=Decimal('0.15'),
        typical_risk_free_rate=Decimal('0.03'),
        equity_risk_premium=Decimal('0.06'),
        real_estate_cap_rate_range=(Decimal('0.04'), Decimal('0.10')),
        commodity_storage_cost=Decimal('0.02'),
        reporting_standard="IFRS",
        regulatory_authority="MULTIPLE",
        requires_registration=True,
        foreign_ownership_allowed=True,
        foreign_ownership_limit=Decimal('1.0'),
        capital_controls=False,
        settlement_cycle=2,
        market_maker_system=True,
        derivatives_market=True
    ),
}


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def get_market_config(region: MarketRegion) -> MarketParameters:
    """
    Get market configuration for a specific region

    Args:
        region: MarketRegion enum

    Returns:
        MarketParameters for that region
    """
    return MARKET_CONFIGS.get(region, MARKET_CONFIGS[MarketRegion.GLOBAL])


def get_market_by_currency(currency: str) -> MarketParameters:
    """
    Get market configuration by currency code

    Args:
        currency: ISO currency code (e.g., 'USD', 'EUR', 'JPY')

    Returns:
        MarketParameters for that currency's primary market
    """
    currency_to_region = {
        'USD': MarketRegion.UNITED_STATES,
        'CAD': MarketRegion.CANADA,
        'GBP': MarketRegion.UNITED_KINGDOM,
        'EUR': MarketRegion.GERMANY,  # Or use EU aggregate
        'JPY': MarketRegion.JAPAN,
        'CNY': MarketRegion.CHINA,
        'HKD': MarketRegion.HONG_KONG,
        'SGD': MarketRegion.SINGAPORE,
        'INR': MarketRegion.INDIA,
        'AUD': MarketRegion.AUSTRALIA,
        'AED': MarketRegion.UAE,
        'SAR': MarketRegion.SAUDI_ARABIA,
        'ZAR': MarketRegion.SOUTH_AFRICA,
    }

    region = currency_to_region.get(currency, MarketRegion.GLOBAL)
    return get_market_config(region)


def list_supported_markets() -> Dict[str, Dict[str, Any]]:
    """
    List all supported markets with key information

    Returns:
        Dictionary of market info
    """
    markets = {}
    for region, config in MARKET_CONFIGS.items():
        markets[region.value] = {
            'currency': config.currency,
            'business_days': config.business_days_per_year,
            'corporate_tax': float(config.corporate_tax_rate),
            'capital_gains_tax': float(config.capital_gains_tax_rate),
            'regulatory_authority': config.regulatory_authority,
            'foreign_ownership_limit': float(config.foreign_ownership_limit)
        }
    return markets


# Export main components
__all__ = [
    'MarketRegion',
    'MarketParameters',
    'MARKET_CONFIGS',
    'get_market_config',
    'get_market_by_currency',
    'list_supported_markets'
]
