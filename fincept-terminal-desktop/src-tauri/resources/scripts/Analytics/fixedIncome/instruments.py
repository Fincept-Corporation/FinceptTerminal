
Fixed Income Instruments Module
=================================

Fixed income instrument definitions

===== DATA SOURCES REQUIRED =====
INPUT:
  - Bond prospectuses and issue documentation
  - Market price data and yield curve information
  - Credit ratings and default recovery rates
  - Interest rate benchmarks and swap curves
  - Macroeconomic data affecting fixed income markets

OUTPUT:
  - Bond valuation models and price calculations
  - Yield curve analysis and forward rate calculations
  - Credit risk assessment and spread analysis
  - Duration and convexity risk metrics
  - Portfolio optimization and immunization strategies

PARAMETERS:
  - yield_curve_method: Yield curve modeling method (default: 'nelson_siegel')
  - credit_spread: Credit spread assumption (default: 0.02)
  - recovery_rate: Default recovery rate (default: 0.40)
  - convexity_adjustment: Convexity adjustment factor (default: 1.0)
  - benchmark_curve: Benchmark yield curve (default: 'treasury')


from typing import List, Optional, Dict, Tuple
from decimal import Decimal
from datetime import date, timedelta
from models import (
    Bond, CallableBond, PutableBond, FloatingRateNote, ConvertibleBond,
    CashFlow, CallableFeature, PutableFeature, ValidationError
)
from config import (
    DayCountConvention, CompoundingFrequency, BusinessDayConvention,
    Currency, BondType, get_market_convention
)


class BondInstrument:
    """Base bond instrument class with core functionality"""

    def __init__(self, bond: Bond):
        self.bond = bond
        self._cash_flows = None

    def generate_cash_flows(self, settlement_date: Optional[date] = None) -> List[CashFlow]:
        """Generate bond cash flows from settlement date to maturity"""
        if settlement_date is None:
            settlement_date = date.today()

        cash_flows = []

        # For zero coupon bonds, only final payment
        if self.bond.is_zero_coupon:
            cash_flows.append(CashFlow(
                date=self.bond.maturity_date,
                amount=self.bond.face_value,
                type="principal"
            ))
            return cash_flows

        # Generate coupon payment dates
        coupon_dates = self._generate_coupon_dates(settlement_date)

        # Generate coupon payments
        coupon_amount = self._calculate_coupon_amount()

        for coupon_date in coupon_dates:
            if coupon_date > settlement_date:
                cash_flows.append(CashFlow(
                    date=coupon_date,
                    amount=coupon_amount,
                    type="coupon"
                ))

        # Add principal repayment at maturity
        if cash_flows:  # If there are coupon payments
            cash_flows[-1].amount += self.bond.face_value
            cash_flows[-1].type = "coupon_and_principal"
        else:  # If no future coupons, just principal
            cash_flows.append(CashFlow(
                date=self.bond.maturity_date,
                amount=self.bond.face_value,
                type="principal"
            ))

        self._cash_flows = cash_flows
        return cash_flows

    def _generate_coupon_dates(self, start_date: date) -> List[date]:
        """Generate coupon payment dates"""
        dates = []
        frequency = self.bond.coupon_frequency.value

        if frequency == 0:  # Continuous compounding - treat as annual
            frequency = 1

        # Calculate months between payments
        months_between = 12 // frequency

        # Start from maturity and work backwards
        current_date = self.bond.maturity_date

        while current_date > self.bond.issue_date:
            dates.append(current_date)
            # Subtract months
            if current_date.month <= months_between:
                new_month = 12 + current_date.month - months_between
                new_year = current_date.year - 1
            else:
                new_month = current_date.month - months_between
                new_year = current_date.year

            try:
                current_date = current_date.replace(year=new_year, month=new_month)
            except ValueError:
                # Handle end-of-month dates
                if new_month == 2 and current_date.day > 28:
                    current_date = current_date.replace(year=new_year, month=new_month, day=28)
                else:
                    current_date = current_date.replace(year=new_year, month=new_month, day=1)
                    current_date = current_date.replace(day=min(current_date.day,
                                                                self._days_in_month(new_year, new_month)))

        # Reverse to get chronological order
        dates.reverse()
        return dates

    def _days_in_month(self, year: int, month: int) -> int:
        """Get number of days in a month"""
        if month == 12:
            next_month = date(year + 1, 1, 1)
        else:
            next_month = date(year, month + 1, 1)
        this_month = date(year, month, 1)
        return (next_month - this_month).days

    def _calculate_coupon_amount(self) -> Decimal:
        """Calculate coupon payment amount"""
        annual_coupon = self.bond.face_value * self.bond.coupon_rate
        frequency = self.bond.coupon_frequency.value

        if frequency == 0:  # Continuous compounding
            return annual_coupon

        return annual_coupon / Decimal(frequency)

    def accrued_interest(self, settlement_date: date) -> Decimal:
        """Calculate accrued interest from last coupon date"""
        if self.bond.is_zero_coupon:
            return Decimal('0')

        coupon_dates = self._generate_coupon_dates(self.bond.issue_date)

        # Find last coupon date before settlement
        last_coupon_date = self.bond.issue_date
        for coupon_date in coupon_dates:
            if coupon_date <= settlement_date:
                last_coupon_date = coupon_date
            else:
                break

        # Calculate accrual period
        days_accrued = self._calculate_days(last_coupon_date, settlement_date)
        days_in_period = self._calculate_days_in_coupon_period(last_coupon_date)

        coupon_amount = self._calculate_coupon_amount()

        return coupon_amount * (Decimal(days_accrued) / Decimal(days_in_period))

    def _calculate_days(self, start_date: date, end_date: date) -> int:
        """Calculate days between dates based on day count convention"""
        if self.bond.day_count_convention == DayCountConvention.ACTUAL_360:
            return (end_date - start_date).days
        elif self.bond.day_count_convention == DayCountConvention.ACTUAL_365:
            return (end_date - start_date).days
        elif self.bond.day_count_convention == DayCountConvention.ACTUAL_ACTUAL:
            return (end_date - start_date).days
        elif self.bond.day_count_convention == DayCountConvention.THIRTY_360:
            return self._thirty_360_days(start_date, end_date)
        else:
            return (end_date - start_date).days

    def _thirty_360_days(self, start_date: date, end_date: date) -> int:
        """Calculate days using 30/360 convention"""
        d1 = min(start_date.day, 30)
        d2 = min(end_date.day, 30) if d1 == 30 else end_date.day

        return (360 * (end_date.year - start_date.year) +
                30 * (end_date.month - start_date.month) +
                (d2 - d1))

    def _calculate_days_in_coupon_period(self, coupon_date: date) -> int:
        """Calculate days in coupon period"""
        frequency = self.bond.coupon_frequency.value
        if frequency == 0:
            frequency = 1

        if self.bond.day_count_convention == DayCountConvention.THIRTY_360:
            return 360 // frequency
        else:
            return 365 // frequency  # Approximation

    def time_to_maturity(self, settlement_date: Optional[date] = None) -> Decimal:
        """Calculate time to maturity in years"""
        if settlement_date is None:
            settlement_date = date.today()

        days = (self.bond.maturity_date - settlement_date).days

        if self.bond.day_count_convention == DayCountConvention.ACTUAL_365:
            return Decimal(days) / Decimal('365')
        elif self.bond.day_count_convention == DayCountConvention.ACTUAL_360:
            return Decimal(days) / Decimal('360')
        else:
            return Decimal(days) / Decimal('365.25')  # Default


class CallableBondInstrument(BondInstrument):
    """Callable bond instrument with embedded call option"""

    def __init__(self, callable_bond: CallableBond):
        super().__init__(callable_bond)
        self.callable_bond = callable_bond

    def is_callable_on_date(self, check_date: date) -> bool:
        """Check if bond is callable on a specific date"""
        for call_feature in self.callable_bond.call_schedule:
            if call_feature.call_date == check_date:
                return True
        return False

    def get_call_price(self, call_date: date) -> Optional[Decimal]:
        """Get call price for a specific date"""
        for call_feature in self.callable_bond.call_schedule:
            if call_feature.call_date == call_date:
                return call_feature.call_price
        return None

    def next_call_date(self, from_date: Optional[date] = None) -> Optional[date]:
        """Get next call date after specified date"""
        if from_date is None:
            from_date = date.today()

        next_call = None
        for call_feature in self.callable_bond.call_schedule:
            if call_feature.call_date > from_date:
                if next_call is None or call_feature.call_date < next_call:
                    next_call = call_feature.call_date

        return next_call

    def effective_maturity(self, yield_to_call: Decimal, yield_to_maturity: Decimal) -> date:
        """Determine effective maturity based on yield comparison"""
        next_call = self.next_call_date()

        if next_call and yield_to_call < yield_to_maturity:
            return next_call
        else:
            return self.bond.maturity_date


class PutableBondInstrument(BondInstrument):
    """Putable bond instrument with embedded put option"""

    def __init__(self, putable_bond: PutableBond):
        super().__init__(putable_bond)
        self.putable_bond = putable_bond

    def is_putable_on_date(self, check_date: date) -> bool:
        """Check if bond is putable on a specific date"""
        for put_feature in self.putable_bond.put_schedule:
            if put_feature.put_date == check_date:
                return True
        return False

    def get_put_price(self, put_date: date) -> Optional[Decimal]:
        """Get put price for a specific date"""
        for put_feature in self.putable_bond.put_schedule:
            if put_feature.put_date == put_date:
                return put_feature.put_price
        return None

    def next_put_date(self, from_date: Optional[date] = None) -> Optional[date]:
        """Get next put date after specified date"""
        if from_date is None:
            from_date = date.today()

        next_put = None
        for put_feature in self.putable_bond.put_schedule:
            if put_feature.put_date > from_date:
                if next_put is None or put_feature.put_date < next_put:
                    next_put = put_feature.put_date

        return next_put


class FloatingRateNoteInstrument(BondInstrument):
    """Floating rate note instrument"""

    def __init__(self, frn: FloatingRateNote):
        super().__init__(frn)
        self.frn = frn
        self._current_reference_rate = Decimal('0')

    def set_reference_rate(self, rate: Decimal):
        """Set current reference rate"""
        self._current_reference_rate = rate

    def current_coupon_rate(self) -> Decimal:
        """Calculate current coupon rate"""
        base_rate = self._current_reference_rate + self.frn.spread

        # Apply cap and floor if specified
        if self.frn.rate_cap and base_rate > self.frn.rate_cap:
            return self.frn.rate_cap

        if self.frn.rate_floor and base_rate < self.frn.rate_floor:
            return self.frn.rate_floor

        return base_rate

    def generate_cash_flows_with_rate_path(self, rate_path: List[Tuple[date, Decimal]]) -> List[CashFlow]:
        """Generate cash flows given a path of reference rates"""
        cash_flows = []
        coupon_dates = self._generate_coupon_dates(date.today())

        for i, coupon_date in enumerate(coupon_dates):
            if coupon_date > date.today():
                # Find applicable rate for this period
                applicable_rate = self._find_rate_for_period(rate_path, coupon_date)

                # Calculate coupon amount
                coupon_rate = applicable_rate + self.frn.spread

                # Apply caps and floors
                if self.frn.rate_cap:
                    coupon_rate = min(coupon_rate, self.frn.rate_cap)
                if self.frn.rate_floor:
                    coupon_rate = max(coupon_rate, self.frn.rate_floor)

                # Calculate payment amount
                coupon_amount = self.frn.face_value * coupon_rate / Decimal(self.frn.coupon_frequency.value)

                cash_flows.append(CashFlow(
                    date=coupon_date,
                    amount=coupon_amount,
                    type="coupon"
                ))

        # Add principal at maturity
        if cash_flows:
            cash_flows[-1].amount += self.frn.face_value
            cash_flows[-1].type = "coupon_and_principal"

        return cash_flows

    def _find_rate_for_period(self, rate_path: List[Tuple[date, Decimal]], period_end: date) -> Decimal:
        """Find the reference rate applicable for a coupon period"""
        applicable_rate = Decimal('0')

        for rate_date, rate in rate_path:
            if rate_date <= period_end:
                applicable_rate = rate
            else:
                break

        return applicable_rate


class ConvertibleBondInstrument(BondInstrument):
    """Convertible bond instrument"""

    def __init__(self, convertible: ConvertibleBond):
        super().__init__(convertible)
        self.convertible = convertible

    def conversion_value(self, stock_price: Optional[Decimal] = None) -> Decimal:
        """Calculate conversion value"""
        if stock_price is None:
            stock_price = self.convertible.underlying_stock_price

        if stock_price is None:
            raise ValidationError("Stock price required for conversion value calculation")

        return self.convertible.conversion_ratio * stock_price

    def conversion_premium(self, bond_price: Decimal, stock_price: Optional[Decimal] = None) -> Decimal:
        """Calculate conversion premium"""
        conv_value = self.conversion_value(stock_price)
        return bond_price - conv_value

    def conversion_premium_percentage(self, bond_price: Decimal, stock_price: Optional[Decimal] = None) -> Decimal:
        """Calculate conversion premium as percentage"""
        conv_value = self.conversion_value(stock_price)
        if conv_value == 0:
            return Decimal('0')

        premium = self.conversion_premium(bond_price, stock_price)
        return premium / conv_value * Decimal('100')

    def payback_period(self, bond_price: Decimal, dividend_yield: Optional[Decimal] = None) -> Optional[Decimal]:
        """Calculate payback period in years"""
        if dividend_yield is None:
            dividend_yield = self.convertible.dividend_yield

        if dividend_yield is None or dividend_yield == 0:
            return None

        coupon_yield = (self.convertible.coupon_rate * self.convertible.face_value) / bond_price
        yield_advantage = coupon_yield - dividend_yield

        if yield_advantage <= 0:
            return None

        premium_pct = self.conversion_premium_percentage(bond_price) / Decimal('100')
        return premium_pct / yield_advantage

    def delta(self, bond_price: Decimal, stock_price: Decimal) -> Decimal:
        """Calculate convertible bond delta (sensitivity to stock price)"""
        # Simplified delta calculation
        conv_value = self.conversion_value(stock_price)

        if bond_price <= conv_value:
            return self.convertible.conversion_ratio
        else:
            # Approximate delta for out-of-the-money convertible
            return self.convertible.conversion_ratio * (conv_value / bond_price)


# Factory functions for instrument creation
def create_bond_instrument(bond: Bond) -> BondInstrument:
    """Factory function to create appropriate instrument type"""
    if isinstance(bond, CallableBond):
        return CallableBondInstrument(bond)
    elif isinstance(bond, PutableBond):
        return PutableBondInstrument(bond)
    elif isinstance(bond, FloatingRateNote):
        return FloatingRateNoteInstrument(bond)
    elif isinstance(bond, ConvertibleBond):
        return ConvertibleBondInstrument(bond)
    else:
        return BondInstrument(bond)


def create_callable_bond_with_schedule(base_bond: Bond,
                                       call_schedule: List[Tuple[date, Decimal]]) -> CallableBondInstrument:
    """Create callable bond with call schedule"""
    call_features = [CallableFeature(call_date=date, call_price=price) for date, price in call_schedule]

    callable_bond = CallableBond(
        isin=base_bond.isin,
        cusip=base_bond.cusip,
        ticker=base_bond.ticker,
        issue_date=base_bond.issue_date,
        maturity_date=base_bond.maturity_date,
        face_value=base_bond.face_value,
        currency=base_bond.currency,
        coupon_rate=base_bond.coupon_rate,
        coupon_frequency=base_bond.coupon_frequency,
        day_count_convention=base_bond.day_count_convention,
        issuer_name=base_bond.issuer_name,
        issuer_rating=base_bond.issuer_rating,
        call_schedule=call_features
    )

    return CallableBondInstrument(callable_bond)