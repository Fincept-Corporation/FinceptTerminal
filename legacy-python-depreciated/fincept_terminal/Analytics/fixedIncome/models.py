"""
Fixed Income Analytics - Data Models Module
CFA Level I & II Compliant Data Structures
"""

from dataclasses import dataclass, field
from typing import List, Dict, Optional, Union, Tuple
from decimal import Decimal
from datetime import datetime, date
from config import (
    DayCountConvention, CompoundingFrequency, BusinessDayConvention,
    Currency, CreditRating, BondType, SecuritizationType,
    VALIDATION_RULES, ValidationError
)


@dataclass
class CashFlow:
    """Represents a single cash flow"""
    date: date
    amount: Decimal
    type: str = "coupon"  # coupon, principal, fee

    def __post_init__(self):
        if self.amount < 0:
            raise ValidationError("Cash flow amount cannot be negative")


@dataclass
class YieldCurve:
    """Yield curve data structure"""
    curve_date: date
    maturities: List[Decimal]  # Years to maturity
    rates: List[Decimal]  # Zero-coupon rates
    currency: Currency = Currency.USD
    curve_type: str = "spot"  # spot, forward, par, swap

    def __post_init__(self):
        if len(self.maturities) != len(self.rates):
            raise ValidationError("Maturities and rates must have same length")

        if any(rate < VALIDATION_RULES['min_yield'] for rate in self.rates):
            raise ValidationError("Yield below minimum threshold")

        if any(rate > VALIDATION_RULES['max_yield'] for rate in self.rates):
            raise ValidationError("Yield above maximum threshold")


@dataclass
class CreditSpread:
    """Credit spread data structure"""
    rating: CreditRating
    maturity: Decimal
    spread: Decimal  # Basis points over benchmark
    sector: Optional[str] = None

    def __post_init__(self):
        if self.spread < 0:
            raise ValidationError("Credit spread cannot be negative")


@dataclass
class Bond:
    """Base bond data model"""
    # Identification
    isin: str
    cusip: Optional[str] = None
    ticker: Optional[str] = None

    # Basic characteristics
    issue_date: date
    maturity_date: date
    face_value: Decimal = Decimal('100')
    currency: Currency = Currency.USD

    # Coupon details
    coupon_rate: Decimal = Decimal('0')
    coupon_frequency: CompoundingFrequency = CompoundingFrequency.SEMI_ANNUAL
    day_count_convention: DayCountConvention = DayCountConvention.THIRTY_360

    # Issuer information
    issuer_name: str = ""
    issuer_rating: Optional[CreditRating] = None
    sector: Optional[str] = None
    country: Optional[str] = None

    # Market data
    current_price: Optional[Decimal] = None
    current_yield: Optional[Decimal] = None

    # Bond type and features
    bond_type: BondType = BondType.FIXED_RATE
    callable: bool = False
    putable: bool = False

    # Settlement
    settlement_days: int = 3
    business_day_convention: BusinessDayConvention = BusinessDayConvention.MODIFIED_FOLLOWING

    def __post_init__(self):
        self._validate()

    def _validate(self):
        """Validate bond parameters"""
        if self.maturity_date <= self.issue_date:
            raise ValidationError("Maturity date must be after issue date")

        if self.coupon_rate < VALIDATION_RULES['min_coupon_rate']:
            raise ValidationError("Coupon rate below minimum")

        if self.coupon_rate > VALIDATION_RULES['max_coupon_rate']:
            raise ValidationError("Coupon rate above maximum")

        if self.current_price and (
                self.current_price < VALIDATION_RULES['min_price'] or
                self.current_price > VALIDATION_RULES['max_price']
        ):
            raise ValidationError("Bond price outside valid range")

    @property
    def time_to_maturity(self) -> Decimal:
        """Calculate time to maturity in years"""
        today = date.today()
        days_to_maturity = (self.maturity_date - today).days
        return Decimal(days_to_maturity) / Decimal('365.25')

    @property
    def is_zero_coupon(self) -> bool:
        """Check if bond is zero coupon"""
        return self.coupon_rate == 0 or self.bond_type == BondType.ZERO_COUPON


@dataclass
class CallableFeature:
    """Callable bond feature"""
    call_date: date
    call_price: Decimal
    call_type: str = "american"  # american, european, bermudan

    def __post_init__(self):
        if self.call_price <= 0:
            raise ValidationError("Call price must be positive")


@dataclass
class PutableFeature:
    """Putable bond feature"""
    put_date: date
    put_price: Decimal
    put_type: str = "european"

    def __post_init__(self):
        if self.put_price <= 0:
            raise ValidationError("Put price must be positive")


@dataclass
class CallableBond(Bond):
    """Callable bond model"""
    call_schedule: List[CallableFeature] = field(default_factory=list)

    def __post_init__(self):
        super().__post_init__()
        self.callable = True
        self.bond_type = BondType.CALLABLE


@dataclass
class PutableBond(Bond):
    """Putable bond model"""
    put_schedule: List[PutableFeature] = field(default_factory=list)

    def __post_init__(self):
        super().__post_init__()
        self.putable = True
        self.bond_type = BondType.PUTABLE


@dataclass
class FloatingRateNote(Bond):
    """Floating rate note model"""
    reference_rate: str = "LIBOR"  # LIBOR, SOFR, EURIBOR, etc.
    spread: Decimal = Decimal('0')  # Spread over reference rate
    reset_frequency: CompoundingFrequency = CompoundingFrequency.QUARTERLY
    rate_cap: Optional[Decimal] = None
    rate_floor: Optional[Decimal] = None

    def __post_init__(self):
        super().__post_init__()
        self.bond_type = BondType.FLOATING_RATE
        self.coupon_rate = Decimal('0')  # Will be set by reference rate


@dataclass
class ConvertibleBond(Bond):
    """Convertible bond model"""
    conversion_ratio: Decimal = Decimal('1')
    conversion_price: Decimal = Decimal('100')
    underlying_stock_price: Optional[Decimal] = None
    stock_volatility: Optional[Decimal] = None
    dividend_yield: Optional[Decimal] = None

    def __post_init__(self):
        super().__post_init__()
        self.bond_type = BondType.CONVERTIBLE

    @property
    def conversion_value(self) -> Optional[Decimal]:
        """Calculate conversion value"""
        if self.underlying_stock_price:
            return self.conversion_ratio * self.underlying_stock_price
        return None


@dataclass
class SecuritizedBond:
    """Base securitized product model"""
    # Identification
    cusip: str
    pool_number: Optional[str] = None

    # Structure details
    securitization_type: SecuritizationType = SecuritizationType.ABS
    original_balance: Decimal = Decimal('0')
    current_balance: Decimal = Decimal('0')

    # Underlying assets
    collateral_type: str = ""
    weighted_average_maturity: Optional[Decimal] = None
    weighted_average_coupon: Optional[Decimal] = None

    # Credit enhancement
    credit_enhancement_level: Decimal = Decimal('0')
    enhancement_type: str = ""  # subordination, overcollateralization, insurance

    # Prepayment
    prepayment_speed: Optional[Decimal] = None  # CPR or PSA

    def __post_init__(self):
        if self.current_balance > self.original_balance:
            raise ValidationError("Current balance cannot exceed original balance")


@dataclass
class MortgagePassThrough(SecuritizedBond):
    """Mortgage pass-through security"""
    agency: str = ""  # GNMA, FNMA, FHLMC
    pool_type: str = ""  # single-family, multi-family

    def __post_init__(self):
        super().__post_init__()
        self.securitization_type = SecuritizationType.MORTGAGE_PASS_THROUGH


@dataclass
class AssetBackedSecurity(SecuritizedBond):
    """Asset-backed security"""
    asset_class: str = ""  # auto loans, credit cards, student loans
    geographic_concentration: Optional[str] = None

    def __post_init__(self):
        super().__post_init__()
        self.securitization_type = SecuritizationType.ABS


@dataclass
class CreditDefaultSwap:
    """Credit default swap model"""
    # Contract details
    reference_entity: str
    notional_amount: Decimal
    maturity_date: date

    # Pricing
    spread: Decimal  # Basis points per annum
    upfront_payment: Decimal = Decimal('0')
    recovery_rate: Decimal = Decimal('0.40')  # 40% default assumption

    # Contract specifications
    settlement_type: str = "physical"  # physical, cash
    credit_events: List[str] = field(default_factory=lambda: ["bankruptcy", "failure_to_pay"])

    def __post_init__(self):
        if self.spread < 0:
            raise ValidationError("CDS spread cannot be negative")

        if not (0 <= self.recovery_rate <= 1):
            raise ValidationError("Recovery rate must be between 0 and 1")


@dataclass
class Portfolio:
    """Fixed income portfolio model"""
    name: str
    holdings: List[Tuple[Union[Bond, SecuritizedBond], Decimal]] = field(default_factory=list)  # (instrument, quantity)
    cash_position: Decimal = Decimal('0')
    base_currency: Currency = Currency.USD

    def add_holding(self, instrument: Union[Bond, SecuritizedBond], quantity: Decimal):
        """Add holding to portfolio"""
        if quantity <= 0:
            raise ValidationError("Quantity must be positive")
        self.holdings.append((instrument, quantity))

    def remove_holding(self, instrument: Union[Bond, SecuritizedBond]):
        """Remove holding from portfolio"""
        self.holdings = [(inst, qty) for inst, qty in self.holdings if inst != instrument]

    @property
    def total_positions(self) -> int:
        """Get total number of positions"""
        return len(self.holdings)

    @property
    def total_face_value(self) -> Decimal:
        """Calculate total face value of portfolio"""
        total = Decimal('0')
        for instrument, quantity in self.holdings:
            if hasattr(instrument, 'face_value'):
                total += instrument.face_value * quantity
            elif hasattr(instrument, 'current_balance'):
                total += instrument.current_balance * quantity
        return total


@dataclass
class MarketData:
    """Market data container"""
    date: date
    yield_curves: Dict[Currency, YieldCurve] = field(default_factory=dict)
    credit_spreads: Dict[str, List[CreditSpread]] = field(default_factory=dict)  # sector -> spreads
    exchange_rates: Dict[str, Decimal] = field(default_factory=dict)  # currency pair -> rate
    volatilities: Dict[str, Decimal] = field(default_factory=dict)  # instrument -> volatility

    def add_yield_curve(self, currency: Currency, curve: YieldCurve):
        """Add yield curve for currency"""
        self.yield_curves[currency] = curve

    def get_yield_curve(self, currency: Currency) -> Optional[YieldCurve]:
        """Get yield curve for currency"""
        return self.yield_curves.get(currency)

    def add_credit_spreads(self, sector: str, spreads: List[CreditSpread]):
        """Add credit spreads for sector"""
        self.credit_spreads[sector] = spreads

    def get_credit_spreads(self, sector: str) -> List[CreditSpread]:
        """Get credit spreads for sector"""
        return self.credit_spreads.get(sector, [])


# Factory functions for creating common instruments
def create_treasury_bond(maturity_years: Decimal, coupon_rate: Decimal,
                         currency: Currency = Currency.USD) -> Bond:
    """Create a treasury bond"""
    today = date.today()
    maturity_date = date(today.year + int(maturity_years), today.month, today.day)

    return Bond(
        isin=f"US{today.strftime('%Y%m%d')}{maturity_years}",
        issue_date=today,
        maturity_date=maturity_date,
        coupon_rate=coupon_rate,
        currency=currency,
        issuer_name="U.S. Treasury",
        issuer_rating=CreditRating.AAA,
        sector="Government"
    )


def create_corporate_bond(issuer: str, maturity_years: Decimal,
                          coupon_rate: Decimal, rating: CreditRating) -> Bond:
    """Create a corporate bond"""
    today = date.today()
    maturity_date = date(today.year + int(maturity_years), today.month, today.day)

    return Bond(
        isin=f"CORP{today.strftime('%Y%m%d')}{maturity_years}",
        issue_date=today,
        maturity_date=maturity_date,
        coupon_rate=coupon_rate,
        issuer_name=issuer,
        issuer_rating=rating,
        sector="Corporate"
    )


def create_zero_coupon_bond(maturity_years: Decimal, currency: Currency = Currency.USD) -> Bond:
    """Create a zero coupon bond"""
    today = date.today()
    maturity_date = date(today.year + int(maturity_years), today.month, today.day)

    return Bond(
        isin=f"ZERO{today.strftime('%Y%m%d')}{maturity_years}",
        issue_date=today,
        maturity_date=maturity_date,
        coupon_rate=Decimal('0'),
        currency=currency,
        bond_type=BondType.ZERO_COUPON
    )


class ValidationError(Exception):
    """Custom validation error"""
    pass