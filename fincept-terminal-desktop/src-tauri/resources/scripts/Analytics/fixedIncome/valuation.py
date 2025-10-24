
Fixed Income Valuation Module
=================================

Fixed income security valuation

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


from typing import List, Dict, Optional, Tuple, Callable
from decimal import Decimal
from datetime import date
import random
from models import Bond, CallableBond, PutableBond, ConvertibleBond, CashFlow
from instruments import BondInstrument, create_bond_instrument
from yield_curves import SpotCurve
from utils import MathUtils, DateUtils, ValidationUtils, cache_calculation, black_scholes_call_price
from config import CompoundingFrequency, ERROR_TOLERANCE


class BondValuation:
    """Basic bond valuation methods"""

    @staticmethod
    @cache_calculation
    def present_value(bond: Bond, discount_rate: Decimal,
                      settlement_date: Optional[date] = None) -> Decimal:
        """Calculate present value of bond using single discount rate"""
        ValidationUtils.validate_yield(discount_rate, "Discount rate")

        if settlement_date is None:
            settlement_date = date.today()

        bond_instrument = create_bond_instrument(bond)
        cash_flows = bond_instrument.generate_cash_flows(settlement_date)

        total_pv = Decimal('0')

        for cf in cash_flows:
            time_to_payment = DateUtils.calculate_day_count_fraction(
                settlement_date, cf.date, bond.day_count_convention
            )

            pv = MathUtils.present_value(cf.amount, discount_rate, time_to_payment)
            total_pv += pv

        return total_pv

    @staticmethod
    def present_value_with_curve(bond: Bond, spot_curve: SpotCurve,
                                 settlement_date: Optional[date] = None) -> Decimal:
        """Calculate present value using spot rate curve"""
        if settlement_date is None:
            settlement_date = date.today()

        bond_instrument = create_bond_instrument(bond)
        cash_flows = bond_instrument.generate_cash_flows(settlement_date)

        total_pv = Decimal('0')

        for cf in cash_flows:
            time_to_payment = DateUtils.calculate_day_count_fraction(
                settlement_date, cf.date, bond.day_count_convention
            )

            spot_rate = spot_curve.get_rate(time_to_payment)
            pv = MathUtils.present_value(cf.amount, spot_rate, time_to_payment)
            total_pv += pv

        return total_pv

    @staticmethod
    def yield_to_maturity(bond: Bond, price: Decimal,
                          settlement_date: Optional[date] = None) -> Decimal:
        """Calculate yield to maturity using iterative methods"""
        ValidationUtils.validate_price(price, "Bond price")

        if settlement_date is None:
            settlement_date = date.today()

        # Define function: PV(ytm) - price = 0
        def price_function(ytm: Decimal) -> Decimal:
            pv = BondValuation.present_value(bond, ytm, settlement_date)
            return pv - price

        # Derivative of price function with respect to yield
        def price_derivative(ytm: Decimal) -> Decimal:
            # Numerical derivative using small increment
            delta = Decimal('0.0001')
            pv1 = BondValuation.present_value(bond, ytm - delta, settlement_date)
            pv2 = BondValuation.present_value(bond, ytm + delta, settlement_date)
            return (pv2 - pv1) / (Decimal('2') * delta)

        # Initial guess based on current yield
        if bond.coupon_rate > 0:
            initial_guess = bond.coupon_rate
        else:
            # For zero coupon bonds
            time_to_maturity = DateUtils.calculate_day_count_fraction(
                settlement_date, bond.maturity_date, bond.day_count_convention
            )
            initial_guess = (bond.face_value / price) ** (Decimal('1') / time_to_maturity) - Decimal('1')

        try:
            # Use Newton-Raphson method
            ytm = MathUtils.newton_raphson(price_function, price_derivative, initial_guess)
            ValidationUtils.validate_yield(ytm, "Yield to maturity")
            return ytm
        except:
            # Fallback to bisection method
            try:
                return MathUtils.bisection_method(price_function, Decimal('0.001'), Decimal('0.50'))
            except:
                # Last resort: Brent's method
                return MathUtils.brent_method(price_function, Decimal('0.001'), Decimal('0.50'))

    @staticmethod
    def current_yield(bond: Bond, price: Decimal) -> Decimal:
        """Calculate current yield (annual coupon / price)"""
        ValidationUtils.validate_price(price, "Bond price")

        if bond.is_zero_coupon:
            return Decimal('0')

        annual_coupon = bond.face_value * bond.coupon_rate
        return annual_coupon / price

    @staticmethod
    def accrued_interest(bond: Bond, settlement_date: date) -> Decimal:
        """Calculate accrued interest"""
        bond_instrument = create_bond_instrument(bond)
        return bond_instrument.accrued_interest(settlement_date)

    @staticmethod
    def clean_price(dirty_price: Decimal, accrued: Decimal) -> Decimal:
        """Calculate clean price from dirty price"""
        return dirty_price - accrued

    @staticmethod
    def dirty_price(clean_price: Decimal, accrued: Decimal) -> Decimal:
        """Calculate dirty price from clean price"""
        return clean_price + accrued


class CallableBondValuation:
    """Valuation methods for callable bonds"""

    @staticmethod
    def yield_to_call(callable_bond: CallableBond, price: Decimal,
                      call_date: date, call_price: Decimal,
                      settlement_date: Optional[date] = None) -> Decimal:
        """Calculate yield to call"""
        ValidationUtils.validate_price(price, "Bond price")
        ValidationUtils.validate_price(call_price, "Call price")

        if settlement_date is None:
            settlement_date = date.today()

        # Create temporary bond with call date as maturity
        temp_bond = Bond(
            isin=callable_bond.isin + "_CALL",
            issue_date=callable_bond.issue_date,
            maturity_date=call_date,
            face_value=call_price,  # Use call price as face value
            currency=callable_bond.currency,
            coupon_rate=callable_bond.coupon_rate,
            coupon_frequency=callable_bond.coupon_frequency,
            day_count_convention=callable_bond.day_count_convention
        )

        return BondValuation.yield_to_maturity(temp_bond, price, settlement_date)

    @staticmethod
    def yield_to_worst(callable_bond: CallableBond, price: Decimal,
                       settlement_date: Optional[date] = None) -> Decimal:
        """Calculate yield to worst (minimum of YTM and all YTCs)"""
        if settlement_date is None:
            settlement_date = date.today()

        # Calculate YTM
        ytm = BondValuation.yield_to_maturity(callable_bond, price, settlement_date)

        yields = [ytm]

        # Calculate YTC for each call date
        for call_feature in callable_bond.call_schedule:
            if call_feature.call_date > settlement_date:
                ytc = CallableBondValuation.yield_to_call(
                    callable_bond, price, call_feature.call_date,
                    call_feature.call_price, settlement_date
                )
                yields.append(ytc)

        return min(yields)


class ArbitrageFreeValuation:
    """Arbitrage-free valuation using binomial trees and Monte Carlo"""

    @staticmethod
    def binomial_tree_value(bond: Bond, spot_curve: SpotCurve, volatility: Decimal,
                            steps: int = 100) -> Decimal:
        """Value bond using binomial interest rate tree"""
        ValidationUtils.validate_positive(volatility, "Volatility")
        ValidationUtils.validate_positive(Decimal(str(steps)), "Steps")

        time_to_maturity = DateUtils.calculate_day_count_fraction(
            date.today(), bond.maturity_date, bond.day_count_convention
        )

        dt = time_to_maturity / Decimal(str(steps))

        # Build interest rate tree
        rate_tree = ArbitrageFreeValuation._build_rate_tree(
            spot_curve, volatility, steps, dt
        )

        # Generate bond cash flows
        bond_instrument = create_bond_instrument(bond)
        cash_flows = bond_instrument.generate_cash_flows()

        # Backward induction valuation
        return ArbitrageFreeValuation._backward_induction(
            rate_tree, cash_flows, bond, dt, steps
        )

    @staticmethod
    def _build_rate_tree(spot_curve: SpotCurve, volatility: Decimal,
                         steps: int, dt: Decimal) -> List[List[Decimal]]:
        """Build binomial interest rate tree"""
        tree = []

        # Initial rate
        r0 = spot_curve.get_rate(dt)

        # Up and down factors
        u = Decimal(str(2.71828182845905)) ** (volatility * (dt ** Decimal('0.5')))
        d = Decimal('1') / u

        for i in range(steps + 1):
            level = []
            for j in range(i + 1):
                # Rate at node (i, j)
                rate = r0 * (u ** Decimal(str(j))) * (d ** Decimal(str(i - j)))
                level.append(rate)
            tree.append(level)

        return tree

    @staticmethod
    def _backward_induction(rate_tree: List[List[Decimal]], cash_flows: List[CashFlow],
                            bond: Bond, dt: Decimal, steps: int) -> Decimal:
        """Perform backward induction on the tree"""
        # Create cash flow mapping by time step
        cf_map = {}
        for cf in cash_flows:
            time_to_cf = DateUtils.calculate_day_count_fraction(
                date.today(), cf.date, bond.day_count_convention
            )
            step = int(time_to_cf / dt)
            if step <= steps:
                cf_map[step] = cf_map.get(step, Decimal('0')) + cf.amount

        # Initialize final values
        values = [bond.face_value] * (steps + 1)

        # Work backwards through the tree
        for i in range(steps - 1, -1, -1):
            new_values = []
            for j in range(i + 1):
                # Discount expected value
                up_value = values[j + 1] if j + 1 < len(values) else bond.face_value
                down_value = values[j] if j < len(values) else bond.face_value

                expected_value = (up_value + down_value) / Decimal('2')
                discount_rate = rate_tree[i][j]

                discounted_value = expected_value / (Decimal('1') + discount_rate * dt)

                # Add any cash flow at this step
                if i in cf_map:
                    discounted_value += cf_map[i]

                new_values.append(discounted_value)

            values = new_values

        return values[0] if values else bond.face_value

    @staticmethod
    def monte_carlo_value(bond: Bond, spot_curve: SpotCurve, volatility: Decimal,
                          paths: int = 10000, steps: int = 252) -> Tuple[Decimal, Decimal]:
        """Value bond using Monte Carlo simulation"""
        ValidationUtils.validate_positive(volatility, "Volatility")
        ValidationUtils.validate_positive(Decimal(str(paths)), "Paths")

        time_to_maturity = DateUtils.calculate_day_count_fraction(
            date.today(), bond.maturity_date, bond.day_count_convention
        )

        dt = time_to_maturity / Decimal(str(steps))

        # Generate bond cash flows
        bond_instrument = create_bond_instrument(bond)
        cash_flows = bond_instrument.generate_cash_flows()

        path_values = []

        for _ in range(paths):
            # Generate interest rate path
            rate_path = ArbitrageFreeValuation._generate_rate_path(
                spot_curve, volatility, steps, dt
            )

            # Value bond along this path
            path_value = ArbitrageFreeValuation._value_along_path(
                cash_flows, rate_path, bond, dt
            )
            path_values.append(path_value)

        # Calculate statistics
        mean_value = sum(path_values) / Decimal(str(len(path_values)))

        # Standard error
        variance = sum((v - mean_value) ** 2 for v in path_values) / Decimal(str(len(path_values) - 1))
        std_error = (variance / Decimal(str(len(path_values)))) ** Decimal('0.5')

        return mean_value, std_error

    @staticmethod
    def _generate_rate_path(spot_curve: SpotCurve, volatility: Decimal,
                            steps: int, dt: Decimal) -> List[Decimal]:
        """Generate single interest rate path using Vasicek model"""
        path = []
        r = spot_curve.get_rate(dt)  # Initial rate

        # Vasicek parameters (simplified)
        kappa = Decimal('0.1')  # Mean reversion speed
        theta = spot_curve.get_rate(Decimal('10'))  # Long-term mean

        for _ in range(steps):
            # Vasicek SDE: dr = kappa(theta - r)dt + sigma * dW
            drift = kappa * (theta - r) * dt
            shock = volatility * (dt ** Decimal('0.5')) * Decimal(str(random.gauss(0, 1)))

            r = r + drift + shock
            r = max(r, Decimal('0.0001'))  # Floor at 1bp
            path.append(r)

        return path

    @staticmethod
    def _value_along_path(cash_flows: List[CashFlow], rate_path: List[Decimal],
                          bond: Bond, dt: Decimal) -> Decimal:
        """Value bond along specific rate path"""
        total_value = Decimal('0')

        for cf in cash_flows:
            time_to_cf = DateUtils.calculate_day_count_fraction(
                date.today(), cf.date, bond.day_count_convention
            )

            # Find appropriate rate from path
            step = min(int(time_to_cf / dt), len(rate_path) - 1)
            discount_rate = rate_path[step]

            pv = MathUtils.present_value(cf.amount, discount_rate, time_to_cf)
            total_value += pv

        return total_value


class OptionAdjustedSpread:
    """Option-Adjusted Spread (OAS) calculations"""

    @staticmethod
    def calculate_oas(bond: Bond, market_price: Decimal, spot_curve: SpotCurve,
                      volatility: Decimal, steps: int = 100) -> Decimal:
        """Calculate Option-Adjusted Spread"""
        ValidationUtils.validate_price(market_price, "Market price")

        # Define function: Theoretical Price(OAS) - Market Price = 0
        def price_difference(oas: Decimal) -> Decimal:
            # Create shifted curve
            shifted_curve = spot_curve.shift_curve(oas, "parallel")

            # Value bond with shifted curve
            if isinstance(bond, (CallableBond, PutableBond)):
                theoretical_price = ArbitrageFreeValuation.binomial_tree_value(
                    bond, shifted_curve, volatility, steps
                )
            else:
                theoretical_price = BondValuation.present_value_with_curve(
                    bond, shifted_curve
                )

            return theoretical_price - market_price

        # Solve for OAS using bisection method
        try:
            oas = MathUtils.bisection_method(
                price_difference,
                Decimal('-0.05'),  # -500 bps
                Decimal('0.05')  # +500 bps
            )
            return oas
        except:
            # Fallback to wider range
            return MathUtils.bisection_method(
                price_difference,
                Decimal('-0.10'),  # -1000 bps
                Decimal('0.10')  # +1000 bps
            )

    @staticmethod
    def oas_duration(bond: Bond, market_price: Decimal, spot_curve: SpotCurve,
                     volatility: Decimal, steps: int = 100) -> Decimal:
        """Calculate OAS duration (sensitivity to parallel curve shifts)"""
        shift_size = Decimal('0.0001')  # 1 bp

        # Calculate OAS for shifted curves
        up_curve = spot_curve.shift_curve(shift_size, "parallel")
        down_curve = spot_curve.shift_curve(-shift_size, "parallel")

        oas_up = OptionAdjustedSpread.calculate_oas(bond, market_price, up_curve, volatility, steps)
        oas_down = OptionAdjustedSpread.calculate_oas(bond, market_price, down_curve, volatility, steps)

        # Duration = -(dOAS/dy) where y is yield
        return -(oas_up - oas_down) / (Decimal('2') * shift_size)


class ConvertibleBondValuation:
    """Valuation methods for convertible bonds"""

    @staticmethod
    def convertible_bond_value(convertible: ConvertibleBond, spot_curve: SpotCurve,
                               stock_price: Decimal, stock_volatility: Decimal,
                               risk_free_rate: Decimal) -> Dict[str, Decimal]:
        """Comprehensive convertible bond valuation"""
        ValidationUtils.validate_positive(stock_price, "Stock price")
        ValidationUtils.validate_positive(stock_volatility, "Stock volatility")
        ValidationUtils.validate_yield(risk_free_rate, "Risk-free rate")

        # 1. Straight bond value
        straight_value = BondValuation.present_value_with_curve(convertible, spot_curve)

        # 2. Conversion value
        conversion_value = convertible.conversion_ratio * stock_price

        # 3. Option value using Black-Scholes approximation
        time_to_maturity = DateUtils.calculate_day_count_fraction(
            date.today(), convertible.maturity_date, convertible.day_count_convention
        )

        # Treat conversion as call option on stock
        conversion_option_value = black_scholes_call_price(
            S=stock_price,
            K=convertible.conversion_price,
            T=time_to_maturity,
            r=risk_free_rate,
            sigma=stock_volatility
        ) * convertible.conversion_ratio

        # 4. Total convertible value (simplified)
        total_value = max(straight_value, conversion_value) + conversion_option_value * Decimal('0.1')

        return {
            'total_value': total_value,
            'straight_value': straight_value,
            'conversion_value': conversion_value,
            'option_value': conversion_option_value,
            'conversion_premium': max(Decimal('0'), total_value - conversion_value),
            'investment_premium': max(Decimal('0'), total_value - straight_value)
        }


class MatrixPricing:
    """Matrix pricing for illiquid bonds"""

    @staticmethod
    def matrix_price(target_bond: Bond, comparable_bonds: List[Tuple[Bond, Decimal]],
                     weights: Optional[List[Decimal]] = None) -> Decimal:
        """Calculate matrix price using comparable bonds"""
        if not comparable_bonds:
            raise ValueError("Need at least one comparable bond")

        if weights is None:
            # Equal weights
            weights = [Decimal('1') / Decimal(str(len(comparable_bonds)))] * len(comparable_bonds)

        if len(weights) != len(comparable_bonds):
            raise ValueError("Number of weights must match number of comparables")

        # Calculate weighted average yield spread
        target_maturity = DateUtils.calculate_day_count_fraction(
            date.today(), target_bond.maturity_date, target_bond.day_count_convention
        )

        weighted_spread = Decimal('0')

        for i, (comp_bond, comp_price) in enumerate(comparable_bonds):
            # Calculate comparable's YTM
            comp_ytm = BondValuation.yield_to_maturity(comp_bond, comp_price)

            # Adjust for maturity difference (simplified)
            maturity_adj = MatrixPricing._maturity_adjustment(target_bond, comp_bond)

            # Adjust for credit difference
            credit_adj = MatrixPricing._credit_adjustment(target_bond, comp_bond)

            adjusted_ytm = comp_ytm + maturity_adj + credit_adj
            weighted_spread += weights[i] * adjusted_ytm

        # Calculate target bond price using weighted spread
        target_price = BondValuation.present_value(target_bond, weighted_spread)

        return target_price

    @staticmethod
    def _maturity_adjustment(target: Bond, comparable: Bond) -> Decimal:
        """Calculate maturity adjustment"""
        target_maturity = DateUtils.calculate_day_count_fraction(
            date.today(), target.maturity_date, target.day_count_convention
        )
        comp_maturity = DateUtils.calculate_day_count_fraction(
            date.today(), comparable.maturity_date, comparable.day_count_convention
        )

        # Simple linear adjustment: 5 bps per year difference
        maturity_diff = target_maturity - comp_maturity
        return maturity_diff * Decimal('0.0005')  # 5 bps per year

    @staticmethod
    def _credit_adjustment(target: Bond, comparable: Bond) -> Decimal:
        """Calculate credit quality adjustment"""
        if target.issuer_rating is None or comparable.issuer_rating is None:
            return Decimal('0')

        from config import RATING_SCORES

        target_score = RATING_SCORES.get(target.issuer_rating, 10)
        comp_score = RATING_SCORES.get(comparable.issuer_rating, 10)

        # 25 bps per notch difference
        rating_diff = target_score - comp_score
        return Decimal(str(rating_diff)) * Decimal('0.0025')


# Utility functions for common valuation scenarios
def value_bond_portfolio(bonds: List[Tuple[Bond, Decimal]], spot_curve: SpotCurve) -> Dict[str, Decimal]:
    """Value a portfolio of bonds"""
    total_value = Decimal('0')
    total_face_value = Decimal('0')

    individual_values = []

    for bond, quantity in bonds:
        bond_value = BondValuation.present_value_with_curve(bond, spot_curve)
        position_value = bond_value * quantity

        total_value += position_value
        total_face_value += bond.face_value * quantity

        individual_values.append({
            'bond': bond,
            'quantity': quantity,
            'unit_value': bond_value,
            'position_value': position_value
        })

    return {
        'total_market_value': total_value,
        'total_face_value': total_face_value,
        'portfolio_yield': (total_face_value - total_value) / total_value if total_value > 0 else Decimal('0'),
        'individual_positions': individual_values
    }


def stress_test_bond(bond: Bond, base_curve: SpotCurve,
                     stress_scenarios: List[Tuple[str, Decimal]]) -> Dict[str, Decimal]:
    """Stress test bond value under different scenarios"""
    base_value = BondValuation.present_value_with_curve(bond, base_curve)

    results = {'base_case': base_value}

    for scenario_name, shift_amount in stress_scenarios:
        stressed_curve = base_curve.shift_curve(shift_amount, "parallel")
        stressed_value = BondValuation.present_value_with_curve(bond, stressed_curve)

        results[scenario_name] = {
            'value': stressed_value,
            'change': stressed_value - base_value,
            'percentage_change': (stressed_value - base_value) / base_value * Decimal('100')
        }

    return results