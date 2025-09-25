"""
Fixed Income Analytics - Yield Curves Module
CFA Level I & II Compliant Yield Curve Construction and Analysis
"""

from typing import List, Dict, Optional, Tuple
from decimal import Decimal
from datetime import date
from models import YieldCurve, Bond, MarketData
from utils import MathUtils, DateUtils, ValidationUtils, cache_calculation
from config import Currency, CompoundingFrequency, ERROR_TOLERANCE


class SpotCurve:
    """Zero-coupon (spot) yield curve implementation"""

    def __init__(self, curve: YieldCurve):
        ValidationUtils.validate_date_order(date.today(), curve.curve_date, "Today", "Curve date")
        self.curve = curve
        self._validate_curve()

    def _validate_curve(self):
        """Validate curve data"""
        if len(self.curve.maturities) != len(self.curve.rates):
            raise ValueError("Maturities and rates must have same length")

        # Check ascending order of maturities
        for i in range(1, len(self.curve.maturities)):
            if self.curve.maturities[i] <= self.curve.maturities[i - 1]:
                raise ValueError("Maturities must be in ascending order")

        # Validate all rates
        for rate in self.curve.rates:
            ValidationUtils.validate_yield(rate, "Spot rate")

    @cache_calculation
    def get_rate(self, maturity: Decimal) -> Decimal:
        """Get spot rate for given maturity using interpolation"""
        ValidationUtils.validate_positive(maturity, "Maturity")

        # Exact match
        if maturity in self.curve.maturities:
            index = self.curve.maturities.index(maturity)
            return self.curve.rates[index]

        # Extrapolation for very short or long maturities
        if maturity < self.curve.maturities[0]:
            return self.curve.rates[0]  # Flat extrapolation

        if maturity > self.curve.maturities[-1]:
            return self.curve.rates[-1]  # Flat extrapolation

        # Linear interpolation
        for i in range(len(self.curve.maturities) - 1):
            if self.curve.maturities[i] <= maturity <= self.curve.maturities[i + 1]:
                return MathUtils.linear_interpolation(
                    maturity,
                    self.curve.maturities[i], self.curve.rates[i],
                    self.curve.maturities[i + 1], self.curve.rates[i + 1]
                )

        return self.curve.rates[0]  # Fallback

    def get_discount_factor(self, maturity: Decimal) -> Decimal:
        """Calculate discount factor for given maturity"""
        spot_rate = self.get_rate(maturity)
        return Decimal('1') / ((Decimal('1') + spot_rate) ** maturity)

    def forward_rate(self, start_maturity: Decimal, end_maturity: Decimal) -> Decimal:
        """Calculate forward rate between two maturities"""
        ValidationUtils.validate_positive(start_maturity, "Start maturity")
        ValidationUtils.validate_positive(end_maturity, "End maturity")

        if end_maturity <= start_maturity:
            raise ValueError("End maturity must be greater than start maturity")

        r1 = self.get_rate(start_maturity)
        r2 = self.get_rate(end_maturity)

        # Forward rate formula: (1 + r2)^t2 = (1 + r1)^t1 * (1 + f)^(t2-t1)
        numerator = (Decimal('1') + r2) ** end_maturity
        denominator = (Decimal('1') + r1) ** start_maturity
        time_diff = end_maturity - start_maturity

        forward_factor = numerator / denominator
        return forward_factor ** (Decimal('1') / time_diff) - Decimal('1')

    def shift_curve(self, shift_amount: Decimal, shift_type: str = "parallel") -> 'SpotCurve':
        """Apply parallel or non-parallel shifts to the curve"""
        new_rates = []

        if shift_type == "parallel":
            new_rates = [rate + shift_amount for rate in self.curve.rates]
        elif shift_type == "steepening":
            # Steepening: short rates decrease, long rates increase
            for i, rate in enumerate(self.curve.rates):
                maturity = self.curve.maturities[i]
                # Linear scaling based on maturity
                shift = shift_amount * (maturity / self.curve.maturities[-1])
                new_rates.append(rate + shift)
        elif shift_type == "flattening":
            # Flattening: opposite of steepening
            for i, rate in enumerate(self.curve.rates):
                maturity = self.curve.maturities[i]
                shift = -shift_amount * (maturity / self.curve.maturities[-1])
                new_rates.append(rate + shift)
        else:
            raise ValueError("Invalid shift type. Use 'parallel', 'steepening', or 'flattening'")

        # Validate new rates
        for rate in new_rates:
            ValidationUtils.validate_yield(rate, "Shifted rate")

        new_curve = YieldCurve(
            curve_date=self.curve.curve_date,
            maturities=self.curve.maturities.copy(),
            rates=new_rates,
            currency=self.curve.currency,
            curve_type="spot"
        )

        return SpotCurve(new_curve)


class ParCurve:
    """Par yield curve implementation"""

    def __init__(self, spot_curve: SpotCurve):
        self.spot_curve = spot_curve
        self.curve_date = spot_curve.curve.curve_date
        self.currency = spot_curve.curve.currency

    @cache_calculation
    def get_par_rate(self, maturity: Decimal,
                     frequency: CompoundingFrequency = CompoundingFrequency.SEMI_ANNUAL) -> Decimal:
        """Calculate par rate for given maturity"""
        ValidationUtils.validate_positive(maturity, "Maturity")

        if frequency == CompoundingFrequency.CONTINUOUS:
            # For continuous compounding, par rate equals spot rate
            return self.spot_curve.get_rate(maturity)

        # Calculate discount factors for all payment dates
        payments_per_year = frequency.value
        payment_interval = Decimal('1') / Decimal(str(payments_per_year))

        # Generate payment times
        payment_times = []
        current_time = payment_interval
        while current_time <= maturity:
            payment_times.append(current_time)
            current_time += payment_interval

        # Ensure maturity is included
        if not payment_times or payment_times[-1] != maturity:
            payment_times.append(maturity)

        # Calculate sum of discount factors
        discount_sum = Decimal('0')
        for time in payment_times:
            discount_sum += self.spot_curve.get_discount_factor(time)

        # Final discount factor
        final_discount = self.spot_curve.get_discount_factor(maturity)

        # Par rate formula: (1 - final_discount) / discount_sum
        par_rate = (Decimal('1') - final_discount) / discount_sum

        return par_rate

    def build_par_curve(self, maturities: List[Decimal],
                        frequency: CompoundingFrequency = CompoundingFrequency.SEMI_ANNUAL) -> YieldCurve:
        """Build complete par curve for given maturities"""
        par_rates = []

        for maturity in maturities:
            par_rate = self.get_par_rate(maturity, frequency)
            par_rates.append(par_rate)

        return YieldCurve(
            curve_date=self.curve_date,
            maturities=maturities,
            rates=par_rates,
            currency=self.currency,
            curve_type="par"
        )


class ForwardCurve:
    """Forward rate curve implementation"""

    def __init__(self, spot_curve: SpotCurve):
        self.spot_curve = spot_curve
        self.curve_date = spot_curve.curve.curve_date
        self.currency = spot_curve.curve.currency

    @cache_calculation
    def get_forward_rate(self, start_time: Decimal, end_time: Decimal) -> Decimal:
        """Calculate forward rate between two time points"""
        return self.spot_curve.forward_rate(start_time, end_time)

    def build_forward_curve(self, start_maturity: Decimal,
                            end_maturities: List[Decimal]) -> YieldCurve:
        """Build forward curve from start maturity to various end maturities"""
        forward_rates = []

        for end_maturity in end_maturities:
            if end_maturity <= start_maturity:
                raise ValueError(f"End maturity {end_maturity} must be greater than start maturity {start_maturity}")

            forward_rate = self.get_forward_rate(start_maturity, end_maturity)
            forward_rates.append(forward_rate)

        return YieldCurve(
            curve_date=self.curve_date,
            maturities=end_maturities,
            rates=forward_rates,
            currency=self.currency,
            curve_type="forward"
        )

    def instantaneous_forward_rate(self, maturity: Decimal, delta: Decimal = Decimal('0.001')) -> Decimal:
        """Calculate instantaneous forward rate at given maturity"""
        if maturity <= delta:
            return self.spot_curve.get_rate(delta)

        # Approximate derivative using finite difference
        r1 = self.spot_curve.get_rate(maturity - delta / 2)
        r2 = self.spot_curve.get_rate(maturity + delta / 2)

        # Instantaneous forward = d/dt[t * r(t)]
        return r2 + maturity * (r2 - r1) / delta


class SwapCurve:
    """Interest rate swap curve implementation"""

    def __init__(self, market_data: Dict[str, Decimal], currency: Currency = Currency.USD):
        """
        Initialize with market swap rates
        market_data: Dict with tenor -> rate (e.g., {'2Y': 0.025, '5Y': 0.035})
        """
        self.market_data = market_data
        self.currency = currency
        self.curve_date = date.today()
        self._tenors = []
        self._rates = []
        self._maturities = []
        self._parse_market_data()

    def _parse_market_data(self):
        """Parse market data into tenors, maturities, and rates"""
        for tenor, rate in self.market_data.items():
            self._tenors.append(tenor)
            self._rates.append(rate)

            # Convert tenor to maturity in years
            if tenor.endswith('M'):
                months = int(tenor[:-1])
                maturity = Decimal(months) / Decimal('12')
            elif tenor.endswith('Y'):
                years = int(tenor[:-1])
                maturity = Decimal(years)
            else:
                raise ValueError(f"Invalid tenor format: {tenor}")

            self._maturities.append(maturity)

        # Sort by maturity
        sorted_data = sorted(zip(self._maturities, self._rates, self._tenors))
        self._maturities, self._rates, self._tenors = zip(*sorted_data)
        self._maturities = list(self._maturities)
        self._rates = list(self._rates)
        self._tenors = list(self._tenors)

    def get_swap_rate(self, maturity: Decimal) -> Decimal:
        """Get swap rate for given maturity using interpolation"""
        ValidationUtils.validate_positive(maturity, "Maturity")

        # Exact match
        if maturity in self._maturities:
            index = self._maturities.index(maturity)
            return self._rates[index]

        # Interpolation
        if maturity < self._maturities[0]:
            return self._rates[0]

        if maturity > self._maturities[-1]:
            return self._rates[-1]

        for i in range(len(self._maturities) - 1):
            if self._maturities[i] <= maturity <= self._maturities[i + 1]:
                return MathUtils.linear_interpolation(
                    maturity,
                    self._maturities[i], self._rates[i],
                    self._maturities[i + 1], self._rates[i + 1]
                )

        return self._rates[0]

    def swap_spread(self, maturity: Decimal, treasury_curve: SpotCurve) -> Decimal:
        """Calculate swap spread over treasury curve"""
        swap_rate = self.get_swap_rate(maturity)
        treasury_rate = treasury_curve.get_rate(maturity)
        return swap_rate - treasury_rate

    def to_yield_curve(self) -> YieldCurve:
        """Convert to YieldCurve object"""
        return YieldCurve(
            curve_date=self.curve_date,
            maturities=self._maturities,
            rates=self._rates,
            currency=self.currency,
            curve_type="swap"
        )


class BootstrappingEngine:
    """Bootstrap yield curves from market instruments"""

    @staticmethod
    def bootstrap_from_bonds(bonds: List[Bond], prices: List[Decimal],
                             settlement_date: Optional[date] = None) -> SpotCurve:
        """Bootstrap spot curve from bond prices"""
        if len(bonds) != len(prices):
            raise ValueError("Number of bonds must equal number of prices")

        if settlement_date is None:
            settlement_date = date.today()

        # Sort bonds by maturity
        bond_price_pairs = list(zip(bonds, prices))
        bond_price_pairs.sort(key=lambda x: x[0].maturity_date)

        maturities = []
        spot_rates = []

        for bond, price in bond_price_pairs:
            # Calculate time to maturity
            maturity = DateUtils.calculate_day_count_fraction(
                settlement_date, bond.maturity_date, bond.day_count_convention
            )

            if bond.is_zero_coupon:
                # For zero-coupon bonds, directly calculate spot rate
                # price = face_value / (1 + r)^t
                # r = (face_value / price)^(1/t) - 1
                spot_rate = (bond.face_value / price) ** (Decimal('1') / maturity) - Decimal('1')
            else:
                # For coupon bonds, solve iteratively
                spot_rate = BootstrappingEngine._solve_coupon_bond_rate(
                    bond, price, maturity, maturities, spot_rates
                )

            maturities.append(maturity)
            spot_rates.append(spot_rate)

        # Create yield curve
        yield_curve = YieldCurve(
            curve_date=settlement_date,
            maturities=maturities,
            rates=spot_rates,
            currency=bonds[0].currency if bonds else Currency.USD,
            curve_type="spot"
        )

        return SpotCurve(yield_curve)

    @staticmethod
    def _solve_coupon_bond_rate(bond: Bond, price: Decimal, maturity: Decimal,
                                existing_maturities: List[Decimal], existing_rates: List[Decimal]) -> Decimal:
        """Solve for spot rate of coupon bond using existing curve"""
        from instruments import BondInstrument

        # Create temporary spot curve with existing rates
        if existing_maturities:
            temp_curve = YieldCurve(
                curve_date=date.today(),
                maturities=existing_maturities,
                rates=existing_rates,
                currency=bond.currency,
                curve_type="spot"
            )
            temp_spot_curve = SpotCurve(temp_curve)
        else:
            temp_spot_curve = None

        # Generate cash flows
        bond_instrument = BondInstrument(bond)
        cash_flows = bond_instrument.generate_cash_flows()

        # Define function to solve: PV of cash flows - market price = 0
        def pv_function(rate: Decimal) -> Decimal:
            total_pv = Decimal('0')

            for cf in cash_flows:
                cf_maturity = DateUtils.calculate_day_count_fraction(
                    date.today(), cf.date, bond.day_count_convention
                )

                if cf_maturity < maturity:
                    # Use existing curve for earlier cash flows
                    if temp_spot_curve:
                        discount_rate = temp_spot_curve.get_rate(cf_maturity)
                    else:
                        discount_rate = rate  # Use current rate if no existing curve
                else:
                    # Use the rate we're solving for
                    discount_rate = rate

                pv = cf.amount / ((Decimal('1') + discount_rate) ** cf_maturity)
                total_pv += pv

            return total_pv - price

        # Derivative for Newton-Raphson
        def pv_derivative(rate: Decimal) -> Decimal:
            total_derivative = Decimal('0')

            for cf in cash_flows:
                cf_maturity = DateUtils.calculate_day_count_fraction(
                    date.today(), cf.date, bond.day_count_convention
                )

                if cf_maturity >= maturity:  # Only consider cash flows at this maturity
                    derivative = -cf_maturity * cf.amount / ((Decimal('1') + rate) ** (cf_maturity + Decimal('1')))
                    total_derivative += derivative

            return total_derivative

        # Solve using Newton-Raphson
        try:
            initial_guess = Decimal('0.05')  # 5% initial guess
            spot_rate = MathUtils.newton_raphson(pv_function, pv_derivative, initial_guess)
            ValidationUtils.validate_yield(spot_rate, "Bootstrapped spot rate")
            return spot_rate
        except:
            # Fallback to bisection method
            return MathUtils.bisection_method(pv_function, Decimal('0.001'), Decimal('0.50'))


class CurveAnalysis:
    """Yield curve analysis and metrics"""

    @staticmethod
    def curve_slope(curve: SpotCurve, short_maturity: Decimal = Decimal('2'),
                    long_maturity: Decimal = Decimal('10')) -> Decimal:
        """Calculate curve slope between two maturities"""
        short_rate = curve.get_rate(short_maturity)
        long_rate = curve.get_rate(long_maturity)
        return long_rate - short_rate

    @staticmethod
    def curve_curvature(curve: SpotCurve, short_maturity: Decimal = Decimal('2'),
                        medium_maturity: Decimal = Decimal('5'),
                        long_maturity: Decimal = Decimal('10')) -> Decimal:
        """Calculate curve curvature (butterfly)"""
        short_rate = curve.get_rate(short_maturity)
        medium_rate = curve.get_rate(medium_maturity)
        long_rate = curve.get_rate(long_maturity)

        # Curvature = 2 * medium - short - long
        return Decimal('2') * medium_rate - short_rate - long_rate

    @staticmethod
    def level_slope_curvature_decomposition(curve: SpotCurve) -> Tuple[Decimal, Decimal, Decimal]:
        """Decompose curve into level, slope, and curvature factors"""
        # Use 2Y, 5Y, 10Y points for decomposition
        r2 = curve.get_rate(Decimal('2'))
        r5 = curve.get_rate(Decimal('5'))
        r10 = curve.get_rate(Decimal('10'))

        # Level: average of rates
        level = (r2 + r5 + r10) / Decimal('3')

        # Slope: 10Y - 2Y
        slope = r10 - r2

        # Curvature: butterfly
        curvature = Decimal('2') * r5 - r2 - r10

        return level, slope, curvature

    @staticmethod
    def duration_matched_curve_shift(curve: SpotCurve, target_duration: Decimal,
                                     shift_amount: Decimal) -> SpotCurve:
        """Apply duration-matched curve shift"""
        # Simplified implementation - apply parallel shift scaled by duration
        duration_scaling = target_duration / Decimal('5')  # Normalize to 5-year duration
        scaled_shift = shift_amount * duration_scaling

        return curve.shift_curve(scaled_shift, "parallel")


# Factory functions for common curve constructions
def create_flat_curve(rate: Decimal, maturities: List[Decimal],
                      currency: Currency = Currency.USD) -> SpotCurve:
    """Create flat yield curve with constant rate"""
    rates = [rate] * len(maturities)

    yield_curve = YieldCurve(
        curve_date=date.today(),
        maturities=maturities,
        rates=rates,
        currency=currency,
        curve_type="spot"
    )

    return SpotCurve(yield_curve)


def create_upward_sloping_curve(short_rate: Decimal, long_rate: Decimal,
                                maturities: List[Decimal],
                                currency: Currency = Currency.USD) -> SpotCurve:
    """Create upward sloping yield curve"""
    if not maturities:
        raise ValueError("Maturities list cannot be empty")

    min_maturity = min(maturities)
    max_maturity = max(maturities)

    rates = []
    for maturity in maturities:
        # Linear interpolation between short and long rates
        rate = MathUtils.linear_interpolation(
            maturity, min_maturity, short_rate, max_maturity, long_rate
        )
        rates.append(rate)

    yield_curve = YieldCurve(
        curve_date=date.today(),
        maturities=maturities,
        rates=rates,
        currency=currency,
        curve_type="spot"
    )

    return SpotCurve(yield_curve)