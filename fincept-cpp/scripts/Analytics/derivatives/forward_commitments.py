"""
Forward Commitments Analytics Module
===================================

Comprehensive pricing and valuation framework for forward commitments including forwards, futures, and swaps. Implements CFA Institute standard carry arbitrage models and pricing methodologies for various derivative types.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Spot prices for underlying assets
  - Risk-free interest rate curves and yields
  - Dividend yields and payment schedules
  - Storage costs and convenience yields for commodities
  - Repo rates and borrowing costs for financing
  - Fixed income instrument details and coupon schedules
  - Currency exchange rates for cross-currency swaps

OUTPUT:
  - Forward and futures contract valuations
  - Interest rate swap pricing and par rate calculations
  - Currency swap fair value assessments
  - Equity swap payment calculations
  - Carry arbitrage opportunity analysis
  - Forward rate agreement (FRA) valuations

PARAMETERS:
  - spot_price: Current spot price of underlying
  - contract_price: Forward contract price
  - risk_free_rate: Risk-free interest rate
  - dividend_yield: Continuous dividend yield - default: 0.0
  - storage_cost: Storage cost rate - default: 0.0
  - convenience_yield: Convenience yield - default: 0.0
  - notional: Contract notional amount
  - day_count: Day count convention - default: DayCountConvention.ACT_365
  - currency: Contract currency - default: "USD"
  - payment_frequency: Swap payment frequency - default: 0.25 (quarterly)
"""

import numpy as np
from typing import Optional, List, Dict, Tuple
from datetime import datetime, date
from dataclasses import dataclass
import logging

from .core import (
    ForwardCommitment, DerivativeType, UnderlyingType, DayCountConvention,
    MarketData, PricingResult, PricingEngine, ValidationError, ModelValidator,
    Constants, calculate_time_fraction
)
from .market_data import MarketDataManager, CurveData, YieldCurvePoint

logger = logging.getLogger(__name__)


@dataclass
class CarryModel:
    """Carry arbitrage model parameters"""
    spot_price: float
    risk_free_rate: float
    dividend_yield: float = 0.0
    storage_cost: float = 0.0
    convenience_yield: float = 0.0
    repo_rate: Optional[float] = None
    borrow_cost: Optional[float] = None

    def __post_init__(self):
        ModelValidator.validate_positive(self.spot_price, "spot_price")
        ModelValidator.validate_rate(self.risk_free_rate, "risk_free_rate")
        ModelValidator.validate_non_negative(self.dividend_yield, "dividend_yield")
        ModelValidator.validate_non_negative(self.storage_cost, "storage_cost")
        ModelValidator.validate_non_negative(self.convenience_yield, "convenience_yield")

    @property
    def net_carry_rate(self) -> float:
        """Calculate net carry rate"""
        carry_rate = self.risk_free_rate - self.dividend_yield + self.storage_cost - self.convenience_yield

        if self.repo_rate is not None:
            carry_rate = self.repo_rate - self.dividend_yield + self.storage_cost - self.convenience_yield

        if self.borrow_cost is not None:
            carry_rate += self.borrow_cost

        return carry_rate


class EquityForward(ForwardCommitment):
    """Equity forward contract implementation"""

    def __init__(self,
                 underlying_symbol: str,
                 expiry_date: datetime,
                 contract_price: float,
                 notional: float = 1.0,
                 day_count: DayCountConvention = DayCountConvention.ACT_365):
        super().__init__(
            DerivativeType.FORWARD,
            UnderlyingType.EQUITY,
            expiry_date,
            contract_price,
            notional,
            day_count
        )
        self.underlying_symbol = underlying_symbol

    def calculate_payoff(self, spot_price: float) -> float:
        """Calculate payoff at expiration"""
        return self.notional * (spot_price - self.contract_price)

    def fair_value(self, market_data: MarketData) -> PricingResult:
        """Calculate fair value using carry arbitrage model"""
        time_to_expiry = self.time_to_expiry()

        # Carry arbitrage model: F = S * e^((r-q)*T)
        carry_rate = market_data.risk_free_rate - market_data.dividend_yield
        theoretical_forward_price = market_data.spot_price * np.exp(carry_rate * time_to_expiry)

        # Value = (F_market - F_theoretical) * e^(-r*T) * notional
        discount_factor = np.exp(-market_data.risk_free_rate * time_to_expiry)
        fair_value = (self.contract_price - theoretical_forward_price) * discount_factor * self.notional

        return PricingResult(
            fair_value=fair_value,
            calculation_details={
                "theoretical_forward_price": theoretical_forward_price,
                "market_forward_price": self.contract_price,
                "carry_rate": carry_rate,
                "discount_factor": discount_factor,
                "time_to_expiry": time_to_expiry
            }
        )


class InterestRateForward(ForwardCommitment):
    """Interest rate forward contract (FRA - Forward Rate Agreement)"""

    def __init__(self,
                 start_date: datetime,
                 end_date: datetime,
                 contract_rate: float,
                 notional: float = 1000000,  # $1M standard
                 day_count: DayCountConvention = DayCountConvention.ACT_360,
                 currency: str = "USD"):

        super().__init__(
            DerivativeType.FORWARD,
            UnderlyingType.INTEREST_RATE,
            end_date,
            contract_rate,
            notional,
            day_count
        )
        self.start_date = start_date
        self.currency = currency

        if start_date >= end_date:
            raise ValidationError("Start date must be before end date")

    def calculate_payoff(self, market_rate: float) -> float:
        """Calculate FRA payoff at settlement"""
        period_length = calculate_time_fraction(self.start_date, self.expiry_date, self.day_count)
        rate_diff = market_rate - self.contract_price

        # FRA payoff discounted to settlement date
        payoff = (rate_diff * period_length * self.notional) / (1 + market_rate * period_length)
        return payoff

    def fair_value(self, market_data: MarketData) -> PricingResult:
        """Calculate FRA fair value using forward rates"""
        # Get yield curve from market data manager
        data_manager = MarketDataManager()
        yield_curve = data_manager.primary_provider.get_yield_curve(self.currency)

        # Calculate forward rate
        t1 = calculate_time_fraction(datetime.now(), self.start_date, self.day_count)
        t2 = calculate_time_fraction(datetime.now(), self.expiry_date, self.day_count)

        r1 = yield_curve.interpolate_rate(t1)
        r2 = yield_curve.interpolate_rate(t2)

        # Forward rate formula: F = ((1 + r2*T2) / (1 + r1*T1) - 1) / (T2 - T1)
        if self.day_count == DayCountConvention.ACT_360:
            forward_rate = ((1 + r2 * t2) / (1 + r1 * t1) - 1) / (t2 - t1)
        else:
            # For continuous compounding
            forward_rate = (r2 * t2 - r1 * t1) / (t2 - t1)

        # FRA value
        period_length = calculate_time_fraction(self.start_date, self.expiry_date, self.day_count)
        rate_diff = forward_rate - self.contract_price
        discount_factor = np.exp(-r1 * t1)

        fair_value = (rate_diff * period_length * self.notional * discount_factor) / (1 + forward_rate * period_length)

        return PricingResult(
            fair_value=fair_value,
            calculation_details={
                "forward_rate": forward_rate,
                "contract_rate": self.contract_price,
                "period_length": period_length,
                "t1": t1,
                "t2": t2,
                "r1": r1,
                "r2": r2,
                "discount_factor": discount_factor
            }
        )


class FixedIncomeForward(ForwardCommitment):
    """Fixed income forward contract"""

    def __init__(self,
                 bond_details: Dict,
                 expiry_date: datetime,
                 contract_price: float,
                 notional: float = 100,  # Par value
                 day_count: DayCountConvention = DayCountConvention.ACT_365):

        super().__init__(
            DerivativeType.FORWARD,
            UnderlyingType.BOND,
            expiry_date,
            contract_price,
            notional,
            day_count
        )
        self.bond_details = bond_details
        self.coupon_rate = bond_details.get("coupon_rate", 0.0)
        self.face_value = bond_details.get("face_value", 100)
        self.maturity_date = bond_details.get("maturity_date")

    def calculate_payoff(self, bond_price: float) -> float:
        """Calculate payoff at expiration"""
        return self.notional * (bond_price - self.contract_price)

    def fair_value(self, market_data: MarketData) -> PricingResult:
        """Calculate fair value with accrued interest consideration"""
        time_to_expiry = self.time_to_expiry()

        # Calculate present value of coupons between now and forward expiry
        coupon_pv = self._calculate_coupon_pv(market_data.risk_free_rate, time_to_expiry)

        # Forward price: F = (S - PV_coupons) * e^(r*T)
        adjusted_spot = market_data.spot_price - coupon_pv
        theoretical_forward_price = adjusted_spot * np.exp(market_data.risk_free_rate * time_to_expiry)

        discount_factor = np.exp(-market_data.risk_free_rate * time_to_expiry)
        fair_value = (self.contract_price - theoretical_forward_price) * discount_factor * self.notional

        return PricingResult(
            fair_value=fair_value,
            calculation_details={
                "theoretical_forward_price": theoretical_forward_price,
                "coupon_pv": coupon_pv,
                "adjusted_spot_price": adjusted_spot,
                "time_to_expiry": time_to_expiry
            }
        )

    def _calculate_coupon_pv(self, risk_free_rate: float, time_to_expiry: float) -> float:
        """Calculate present value of coupons paid during forward period"""
        # Simplified: assume semi-annual coupons
        annual_coupon = self.coupon_rate * self.face_value
        semi_annual_coupon = annual_coupon / 2

        coupon_pv = 0.0
        coupon_frequency = 0.5  # Semi-annual

        # Calculate PV of coupons paid before forward expiry
        for i in range(1, int(time_to_expiry / coupon_frequency) + 1):
            coupon_time = i * coupon_frequency
            if coupon_time <= time_to_expiry:
                coupon_pv += semi_annual_coupon * np.exp(-risk_free_rate * coupon_time)

        return coupon_pv


class InterestRateSwap:
    """Interest Rate Swap implementation"""

    def __init__(self,
                 notional: float,
                 fixed_rate: float,
                 floating_rate_index: str,
                 start_date: datetime,
                 end_date: datetime,
                 payment_frequency: float = 0.25,  # Quarterly
                 day_count: DayCountConvention = DayCountConvention.ACT_360,
                 currency: str = "USD"):

        self.notional = notional
        self.fixed_rate = fixed_rate
        self.floating_rate_index = floating_rate_index
        self.start_date = start_date
        self.end_date = end_date
        self.payment_frequency = payment_frequency
        self.day_count = day_count
        self.currency = currency

        ModelValidator.validate_positive(notional, "notional")
        ModelValidator.validate_rate(fixed_rate, "fixed_rate")

    def fair_value(self, yield_curve: CurveData, pay_fixed: bool = True) -> PricingResult:
        """Calculate swap fair value using yield curve"""
        payment_dates = self._generate_payment_dates()

        # Calculate fixed leg PV
        fixed_leg_pv = 0.0
        for payment_date in payment_dates:
            time_to_payment = calculate_time_fraction(datetime.now(), payment_date, self.day_count)
            discount_rate = yield_curve.interpolate_rate(time_to_payment)
            discount_factor = np.exp(-discount_rate * time_to_payment)

            period_length = self.payment_frequency
            fixed_payment = self.fixed_rate * period_length * self.notional
            fixed_leg_pv += fixed_payment * discount_factor

        # Calculate floating leg PV (simplified)
        floating_leg_pv = self.notional * (1 - np.exp(-yield_curve.interpolate_rate(
            calculate_time_fraction(datetime.now(), self.end_date, self.day_count)
        ) * calculate_time_fraction(datetime.now(), self.end_date, self.day_count)))

        # Swap value depends on position
        if pay_fixed:
            fair_value = floating_leg_pv - fixed_leg_pv
        else:
            fair_value = fixed_leg_pv - floating_leg_pv

        return PricingResult(
            fair_value=fair_value,
            calculation_details={
                "fixed_leg_pv": fixed_leg_pv,
                "floating_leg_pv": floating_leg_pv,
                "pay_fixed": pay_fixed,
                "payment_dates": len(payment_dates)
            }
        )

    def _generate_payment_dates(self) -> List[datetime]:
        """Generate payment dates for swap"""
        payment_dates = []
        current_date = self.start_date

        while current_date < self.end_date:
            # Add payment frequency in years converted to days
            days_to_add = int(self.payment_frequency * 365.25)
            next_date = current_date.replace(day=current_date.day + days_to_add)

            # Simplified date handling - in production use proper business day calendar
            if next_date <= self.end_date:
                payment_dates.append(next_date)
            current_date = next_date

        return payment_dates

    def par_rate(self, yield_curve: CurveData) -> float:
        """Calculate par swap rate (market swap rate)"""
        payment_dates = self._generate_payment_dates()

        # Calculate annuity factor (sum of discount factors)
        annuity_factor = 0.0
        for payment_date in payment_dates:
            time_to_payment = calculate_time_fraction(datetime.now(), payment_date, self.day_count)
            discount_rate = yield_curve.interpolate_rate(time_to_payment)
            discount_factor = np.exp(-discount_rate * time_to_payment)
            annuity_factor += discount_factor * self.payment_frequency

        # Par rate = (1 - final_discount_factor) / annuity_factor
        final_time = calculate_time_fraction(datetime.now(), self.end_date, self.day_count)
        final_discount_factor = np.exp(-yield_curve.interpolate_rate(final_time) * final_time)

        par_rate = (1 - final_discount_factor) / annuity_factor
        return par_rate


class CurrencySwap:
    """Currency Swap implementation"""

    def __init__(self,
                 notional_domestic: float,
                 notional_foreign: float,
                 fixed_rate_domestic: float,
                 fixed_rate_foreign: float,
                 start_date: datetime,
                 end_date: datetime,
                 domestic_currency: str = "USD",
                 foreign_currency: str = "EUR",
                 payment_frequency: float = 0.5):  # Semi-annual

        self.notional_domestic = notional_domestic
        self.notional_foreign = notional_foreign
        self.fixed_rate_domestic = fixed_rate_domestic
        self.fixed_rate_foreign = fixed_rate_foreign
        self.start_date = start_date
        self.end_date = end_date
        self.domestic_currency = domestic_currency
        self.foreign_currency = foreign_currency
        self.payment_frequency = payment_frequency

        ModelValidator.validate_positive(notional_domestic, "domestic_notional")
        ModelValidator.validate_positive(notional_foreign, "foreign_notional")

    def fair_value(self,
                   domestic_curve: CurveData,
                   foreign_curve: CurveData,
                   fx_rate: float) -> PricingResult:
        """Calculate currency swap fair value"""

        # Calculate domestic leg PV
        domestic_leg_pv = self._calculate_leg_pv(
            self.notional_domestic,
            self.fixed_rate_domestic,
            domestic_curve
        )

        # Calculate foreign leg PV in foreign currency
        foreign_leg_pv_foreign = self._calculate_leg_pv(
            self.notional_foreign,
            self.fixed_rate_foreign,
            foreign_curve
        )

        # Convert foreign leg to domestic currency
        foreign_leg_pv_domestic = foreign_leg_pv_foreign * fx_rate

        # Swap value = Foreign leg PV - Domestic leg PV
        fair_value = foreign_leg_pv_domestic - domestic_leg_pv

        return PricingResult(
            fair_value=fair_value,
            calculation_details={
                "domestic_leg_pv": domestic_leg_pv,
                "foreign_leg_pv_foreign": foreign_leg_pv_foreign,
                "foreign_leg_pv_domestic": foreign_leg_pv_domestic,
                "fx_rate": fx_rate
            }
        )

    def _calculate_leg_pv(self, notional: float, fixed_rate: float, yield_curve: CurveData) -> float:
        """Calculate present value of one leg"""
        total_time = calculate_time_fraction(self.start_date, self.end_date, DayCountConvention.ACT_365)
        num_payments = int(total_time / self.payment_frequency)

        leg_pv = 0.0
        for i in range(1, num_payments + 1):
            payment_time = i * self.payment_frequency
            discount_rate = yield_curve.interpolate_rate(payment_time)
            discount_factor = np.exp(-discount_rate * payment_time)

            coupon_payment = fixed_rate * self.payment_frequency * notional
            leg_pv += coupon_payment * discount_factor

        # Add principal repayment at maturity
        final_discount_rate = yield_curve.interpolate_rate(total_time)
        final_discount_factor = np.exp(-final_discount_rate * total_time)
        leg_pv += notional * final_discount_factor

        return leg_pv


class EquitySwap:
    """Equity Swap implementation"""

    def __init__(self,
                 notional: float,
                 equity_leg_return: str,  # "total_return" or "price_return"
                 fixed_rate: Optional[float] = None,
                 floating_rate_spread: float = 0.0,
                 start_date: datetime = None,
                 end_date: datetime = None,
                 payment_frequency: float = 0.25):  # Quarterly

        self.notional = notional
        self.equity_leg_return = equity_leg_return
        self.fixed_rate = fixed_rate
        self.floating_rate_spread = floating_rate_spread
        self.start_date = start_date or datetime.now()
        self.end_date = end_date
        self.payment_frequency = payment_frequency

        ModelValidator.validate_positive(notional, "notional")

    def calculate_equity_leg_payment(self,
                                     initial_price: float,
                                     final_price: float,
                                     dividends: float = 0.0) -> float:
        """Calculate equity leg payment"""
        price_return = (final_price - initial_price) / initial_price

        if self.equity_leg_return == "total_return":
            total_return = price_return + dividends / initial_price
            return self.notional * total_return
        else:  # price_return only
            return self.notional * price_return

    def calculate_fixed_leg_payment(self, period_length: float) -> float:
        """Calculate fixed leg payment"""
        if self.fixed_rate is None:
            raise ValueError("Fixed rate not specified for equity swap")
        return self.notional * self.fixed_rate * period_length

    def fair_value(self, market_data: MarketData, expected_equity_return: float) -> PricingResult:
        """Calculate equity swap fair value"""
        time_to_expiry = calculate_time_fraction(self.start_date, self.end_date, DayCountConvention.ACT_365)

        # Expected equity leg PV
        expected_equity_pv = self.notional * expected_equity_return * np.exp(
            -market_data.risk_free_rate * time_to_expiry)

        # Fixed leg PV
        if self.fixed_rate is not None:
            total_fixed_payments = self.fixed_rate * time_to_expiry * self.notional
            fixed_leg_pv = total_fixed_payments * np.exp(-market_data.risk_free_rate * time_to_expiry)
        else:
            # Floating leg approximation
            fixed_leg_pv = self.notional * (market_data.risk_free_rate + self.floating_rate_spread) * time_to_expiry
            fixed_leg_pv *= np.exp(-market_data.risk_free_rate * time_to_expiry)

        fair_value = expected_equity_pv - fixed_leg_pv

        return PricingResult(
            fair_value=fair_value,
            calculation_details={
                "expected_equity_pv": expected_equity_pv,
                "fixed_leg_pv": fixed_leg_pv,
                "expected_equity_return": expected_equity_return,
                "time_to_expiry": time_to_expiry
            }
        )


class CarryArbitrageCalculator:
    """Carry arbitrage model calculations"""

    @staticmethod
    def forward_price_no_income(spot: float, risk_free_rate: float, time_to_expiry: float) -> float:
        """Forward price with no income from underlying"""
        return spot * np.exp(risk_free_rate * time_to_expiry)

    @staticmethod
    def forward_price_with_yield(spot: float, risk_free_rate: float, yield_rate: float, time_to_expiry: float) -> float:
        """Forward price with continuous yield from underlying"""
        return spot * np.exp((risk_free_rate - yield_rate) * time_to_expiry)

    @staticmethod
    def forward_price_with_discrete_income(spot: float, risk_free_rate: float, income_pv: float,
                                           time_to_expiry: float) -> float:
        """Forward price with discrete income payments"""
        return (spot - income_pv) * np.exp(risk_free_rate * time_to_expiry)

    @staticmethod
    def forward_price_with_storage_cost(spot: float, risk_free_rate: float, storage_cost_rate: float,
                                        time_to_expiry: float) -> float:
        """Forward price with storage costs"""
        return spot * np.exp((risk_free_rate + storage_cost_rate) * time_to_expiry)

    @staticmethod
    def forward_price_commodity(spot: float, risk_free_rate: float, storage_cost_rate: float, convenience_yield: float,
                                time_to_expiry: float) -> float:
        """Forward price for commodities with storage costs and convenience yield"""
        net_cost = risk_free_rate + storage_cost_rate - convenience_yield
        return spot * np.exp(net_cost * time_to_expiry)

    @staticmethod
    def arbitrage_profit(forward_market_price: float, forward_theoretical_price: float, risk_free_rate: float,
                         time_to_expiry: float) -> float:
        """Calculate arbitrage profit"""
        price_diff = forward_market_price - forward_theoretical_price
        return price_diff * np.exp(-risk_free_rate * time_to_expiry)


class ForwardCommitmentPricingEngine(PricingEngine):
    """Unified pricing engine for forward commitments"""

    def __init__(self):
        self.carry_calculator = CarryArbitrageCalculator()

    def price(self, instrument: ForwardCommitment, market_data: MarketData) -> PricingResult:
        """Price forward commitment based on type"""
        if not self.validate_inputs(instrument, market_data):
            raise ValidationError("Invalid inputs for forward commitment pricing")

        if isinstance(instrument, EquityForward):
            return instrument.fair_value(market_data)
        elif isinstance(instrument, InterestRateForward):
            return instrument.fair_value(market_data)
        elif isinstance(instrument, FixedIncomeForward):
            return instrument.fair_value(market_data)
        else:
            raise ValueError(f"Unsupported forward commitment type: {type(instrument)}")

    def validate_inputs(self, instrument: ForwardCommitment, market_data: MarketData) -> bool:
        """Validate inputs for forward commitment pricing"""
        try:
            ModelValidator.validate_positive(market_data.spot_price, "spot_price")
            ModelValidator.validate_rate(market_data.risk_free_rate, "risk_free_rate")
            ModelValidator.validate_non_negative(market_data.dividend_yield, "dividend_yield")

            if instrument.is_expired():
                logger.warning("Forward commitment has expired")
                return False

            return True
        except ValidationError:
            return False


# Export main classes
__all__ = [
    'CarryModel', 'EquityForward', 'InterestRateForward', 'FixedIncomeForward',
    'InterestRateSwap', 'CurrencySwap', 'EquitySwap', 'CarryArbitrageCalculator',
    'ForwardCommitmentPricingEngine'
]