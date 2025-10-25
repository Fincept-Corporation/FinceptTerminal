
Fixed Income Risk Analytics Module
=================================

Fixed income risk analysis

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


from typing import List, Dict, Optional, Tuple
from decimal import Decimal
from datetime import date
import math
from models import Bond, Portfolio, CashFlow
from instruments import create_bond_instrument
from yield_curves import SpotCurve
from valuation import BondValuation, ArbitrageFreeValuation
from utils import MathUtils, DateUtils, ValidationUtils, cache_calculation
from config import CompoundingFrequency, ERROR_TOLERANCE


class DurationMeasures:
    """Duration calculations for fixed income securities"""

    @staticmethod
    @cache_calculation
    def macaulay_duration(bond: Bond, yield_rate: Decimal,
                          settlement_date: Optional[date] = None) -> Decimal:
        """Calculate Macaulay duration"""
        ValidationUtils.validate_yield(yield_rate, "Yield rate")

        if settlement_date is None:
            settlement_date = date.today()

        bond_instrument = create_bond_instrument(bond)
        cash_flows = bond_instrument.generate_cash_flows(settlement_date)

        if not cash_flows:
            return Decimal('0')

        # Calculate weighted average time to maturity
        total_pv = Decimal('0')
        weighted_time = Decimal('0')

        for cf in cash_flows:
            time_to_payment = DateUtils.calculate_day_count_fraction(
                settlement_date, cf.date, bond.day_count_convention
            )

            pv = MathUtils.present_value(cf.amount, yield_rate, time_to_payment)
            total_pv += pv
            weighted_time += time_to_payment * pv

        if total_pv == 0:
            return Decimal('0')

        return weighted_time / total_pv

    @staticmethod
    def modified_duration(bond: Bond, yield_rate: Decimal,
                          settlement_date: Optional[date] = None) -> Decimal:
        """Calculate Modified duration"""
        mac_duration = DurationMeasures.macaulay_duration(bond, yield_rate, settlement_date)

        # Get compounding frequency
        frequency = bond.coupon_frequency.value
        if frequency == 0:  # Continuous compounding
            return mac_duration

        return mac_duration / (Decimal('1') + yield_rate / Decimal(str(frequency)))

    @staticmethod
    def effective_duration(bond: Bond, base_curve: SpotCurve,
                           yield_shift: Decimal = Decimal('0.0001')) -> Decimal:
        """Calculate Effective duration using curve shifts"""
        ValidationUtils.validate_positive(yield_shift, "Yield shift")

        # Calculate prices with up and down shifts
        up_curve = base_curve.shift_curve(yield_shift, "parallel")
        down_curve = base_curve.shift_curve(-yield_shift, "parallel")

        base_price = BondValuation.present_value_with_curve(bond, base_curve)
        up_price = BondValuation.present_value_with_curve(bond, up_curve)
        down_price = BondValuation.present_value_with_curve(bond, down_curve)

        # Effective duration = -(P- - P+) / (2 * P0 * Δy)
        if base_price == 0:
            return Decimal('0')

        return -(down_price - up_price) / (Decimal('2') * base_price * yield_shift)

    @staticmethod
    def money_duration(bond: Bond, yield_rate: Decimal, price: Decimal,
                       settlement_date: Optional[date] = None) -> Decimal:
        """Calculate Money duration (Modified duration × Price)"""
        mod_duration = DurationMeasures.modified_duration(bond, yield_rate, settlement_date)
        return mod_duration * price

    @staticmethod
    def dollar_duration(bond: Bond, yield_rate: Decimal, price: Decimal,
                        settlement_date: Optional[date] = None) -> Decimal:
        """Calculate Dollar duration (Money duration × 0.01)"""
        money_dur = DurationMeasures.money_duration(bond, yield_rate, price, settlement_date)
        return money_dur * Decimal('0.01')

    @staticmethod
    def price_value_basis_point(bond: Bond, yield_rate: Decimal,
                                settlement_date: Optional[date] = None) -> Decimal:
        """Calculate Price Value of a Basis Point (PVBP)"""
        if settlement_date is None:
            settlement_date = date.today()

        # Calculate price at current yield and yield + 1bp
        base_price = BondValuation.present_value(bond, yield_rate, settlement_date)
        shifted_price = BondValuation.present_value(bond, yield_rate + Decimal('0.0001'), settlement_date)

        return base_price - shifted_price


class ConvexityMeasures:
    """Convexity calculations for fixed income securities"""

    @staticmethod
    @cache_calculation
    def effective_convexity(bond: Bond, base_curve: SpotCurve,
                            yield_shift: Decimal = Decimal('0.0001')) -> Decimal:
        """Calculate Effective convexity using curve shifts"""
        ValidationUtils.validate_positive(yield_shift, "Yield shift")

        # Calculate prices with up and down shifts
        up_curve = base_curve.shift_curve(yield_shift, "parallel")
        down_curve = base_curve.shift_curve(-yield_shift, "parallel")

        base_price = BondValuation.present_value_with_curve(bond, base_curve)
        up_price = BondValuation.present_value_with_curve(bond, up_curve)
        down_price = BondValuation.present_value_with_curve(bond, down_curve)

        # Convexity = (P+ + P- - 2*P0) / (P0 * (Δy)²)
        if base_price == 0:
            return Decimal('0')

        numerator = up_price + down_price - Decimal('2') * base_price
        denominator = base_price * (yield_shift ** 2)

        return numerator / denominator

    @staticmethod
    def convexity_adjustment(duration: Decimal, convexity: Decimal,
                             yield_change: Decimal) -> Decimal:
        """Calculate convexity adjustment to duration-based price change"""
        return Decimal('0.5') * convexity * (yield_change ** 2)

    @staticmethod
    def approximate_price_change(duration: Decimal, convexity: Decimal,
                                 yield_change: Decimal) -> Decimal:
        """Approximate percentage price change using duration and convexity"""
        duration_effect = -duration * yield_change
        convexity_effect = ConvexityMeasures.convexity_adjustment(duration, convexity, yield_change)

        return duration_effect + convexity_effect


class KeyRateDuration:
    """Key Rate Duration analysis for yield curve risk"""

    @staticmethod
    def calculate_key_rate_durations(bond: Bond, base_curve: SpotCurve,
                                     key_maturities: List[Decimal],
                                     yield_shift: Decimal = Decimal('0.0001')) -> Dict[Decimal, Decimal]:
        """Calculate key rate durations for specified maturities"""
        base_price = BondValuation.present_value_with_curve(bond, base_curve)
        key_durations = {}

        for maturity in key_maturities:
            # Create curve with shift only at this maturity
            shifted_curve = KeyRateDuration._create_key_rate_shifted_curve(
                base_curve, maturity, yield_shift
            )

            shifted_price = BondValuation.present_value_with_curve(bond, shifted_curve)

            # Key rate duration = -(ΔP/P) / Δy
            if base_price != 0:
                key_duration = -(shifted_price - base_price) / (base_price * yield_shift)
            else:
                key_duration = Decimal('0')

            key_durations[maturity] = key_duration

        return key_durations

    @staticmethod
    def _create_key_rate_shifted_curve(base_curve: SpotCurve, shift_maturity: Decimal,
                                       shift_amount: Decimal) -> SpotCurve:
        """Create curve with shift concentrated at specific maturity"""
        new_rates = []

        for i, maturity in enumerate(base_curve.curve.maturities):
            base_rate = base_curve.curve.rates[i]

            # Apply shift with triangular weighting around shift_maturity
            if abs(maturity - shift_maturity) <= Decimal('1'):  # Within 1 year
                weight = Decimal('1') - abs(maturity - shift_maturity)
                shift = shift_amount * weight
            else:
                shift = Decimal('0')

            new_rates.append(base_rate + shift)

        from models import YieldCurve
        new_curve = YieldCurve(
            curve_date=base_curve.curve.curve_date,
            maturities=base_curve.curve.maturities.copy(),
            rates=new_rates,
            currency=base_curve.curve.currency,
            curve_type="spot"
        )

        from yield_curves import SpotCurve
        return SpotCurve(new_curve)


class PortfolioRiskMetrics:
    """Portfolio-level risk calculations"""

    @staticmethod
    def portfolio_duration(portfolio: Portfolio, yields: Dict[str, Decimal],
                           prices: Dict[str, Decimal]) -> Decimal:
        """Calculate portfolio duration as market-value weighted average"""
        total_market_value = Decimal('0')
        weighted_duration = Decimal('0')

        for bond, quantity in portfolio.holdings:
            bond_id = bond.isin

            if bond_id not in yields or bond_id not in prices:
                continue

            bond_yield = yields[bond_id]
            bond_price = prices[bond_id]
            market_value = bond_price * quantity

            duration = DurationMeasures.modified_duration(bond, bond_yield)

            total_market_value += market_value
            weighted_duration += duration * market_value

        if total_market_value == 0:
            return Decimal('0')

        return weighted_duration / total_market_value

    @staticmethod
    def portfolio_convexity(portfolio: Portfolio, base_curve: SpotCurve,
                            prices: Dict[str, Decimal]) -> Decimal:
        """Calculate portfolio convexity as market-value weighted average"""
        total_market_value = Decimal('0')
        weighted_convexity = Decimal('0')

        for bond, quantity in portfolio.holdings:
            bond_id = bond.isin

            if bond_id not in prices:
                continue

            bond_price = prices[bond_id]
            market_value = bond_price * quantity

            convexity = ConvexityMeasures.effective_convexity(bond, base_curve)

            total_market_value += market_value
            weighted_convexity += convexity * market_value

        if total_market_value == 0:
            return Decimal('0')

        return weighted_convexity / total_market_value

    @staticmethod
    def duration_matching_error(portfolio: Portfolio, target_duration: Decimal,
                                yields: Dict[str, Decimal], prices: Dict[str, Decimal]) -> Decimal:
        """Calculate duration matching error"""
        portfolio_dur = PortfolioRiskMetrics.portfolio_duration(portfolio, yields, prices)
        return portfolio_dur - target_duration

    @staticmethod
    def portfolio_yield(portfolio: Portfolio, yields: Dict[str, Decimal],
                        prices: Dict[str, Decimal]) -> Decimal:
        """Calculate portfolio yield as market-value weighted average"""
        total_market_value = Decimal('0')
        weighted_yield = Decimal('0')

        for bond, quantity in portfolio.holdings:
            bond_id = bond.isin

            if bond_id not in yields or bond_id not in prices:
                continue

            bond_yield = yields[bond_id]
            bond_price = prices[bond_id]
            market_value = bond_price * quantity

            total_market_value += market_value
            weighted_yield += bond_yield * market_value

        if total_market_value == 0:
            return Decimal('0')

        return weighted_yield / total_market_value


class ValueAtRisk:
    """Value at Risk calculations for fixed income portfolios"""

    @staticmethod
    def parametric_var(portfolio_value: Decimal, portfolio_duration: Decimal,
                       yield_volatility: Decimal, confidence_level: Decimal = Decimal('0.95'),
                       time_horizon: Decimal = Decimal('1')) -> Decimal:
        """Calculate parametric VaR using duration approximation"""
        ValidationUtils.validate_positive(portfolio_value, "Portfolio value")
        ValidationUtils.validate_positive(yield_volatility, "Yield volatility")
        ValidationUtils.validate_percentage(confidence_level, "Confidence level")

        # Standard normal quantile for confidence level
        if confidence_level == Decimal('0.95'):
            z_score = Decimal('1.645')  # 95% confidence
        elif confidence_level == Decimal('0.99'):
            z_score = Decimal('2.326')  # 99% confidence
        else:
            # Approximate for other confidence levels
            z_score = Decimal('1.645')  # Default to 95%

        # VaR = Portfolio Value × Duration × Yield Volatility × Z-score × √(Time Horizon)
        time_factor = time_horizon ** Decimal('0.5')
        var = portfolio_value * portfolio_duration * yield_volatility * z_score * time_factor

        return var

    @staticmethod
    def monte_carlo_var(portfolio: Portfolio, base_curve: SpotCurve,
                        yield_volatility: Decimal, simulations: int = 10000,
                        confidence_level: Decimal = Decimal('0.95'),
                        time_horizon: Decimal = Decimal('1')) -> Tuple[Decimal, List[Decimal]]:
        """Calculate VaR using Monte Carlo simulation"""
        import random

        current_value = Decimal('0')
        simulated_values = []

        # Calculate current portfolio value
        for bond, quantity in portfolio.holdings:
            bond_value = BondValuation.present_value_with_curve(bond, base_curve)
            current_value += bond_value * quantity

        # Run Monte Carlo simulations
        for _ in range(simulations):
            # Generate random yield change
            random_change = Decimal(str(random.gauss(0, float(yield_volatility * (time_horizon ** Decimal('0.5'))))))

            # Shift curve and revalue portfolio
            shifted_curve = base_curve.shift_curve(random_change, "parallel")
            portfolio_value = Decimal('0')

            for bond, quantity in portfolio.holdings:
                bond_value = BondValuation.present_value_with_curve(bond, shifted_curve)
                portfolio_value += bond_value * quantity

            value_change = portfolio_value - current_value
            simulated_values.append(value_change)

        # Calculate VaR as percentile
        simulated_values.sort()
        var_index = int((1 - confidence_level) * simulations)
        var = -simulated_values[var_index]  # VaR is positive number representing loss

        return var, simulated_values


class StressTestingFramework:
    """Stress testing framework for fixed income portfolios"""

    @staticmethod
    def parallel_shift_stress_test(portfolio: Portfolio, base_curve: SpotCurve,
                                   stress_scenarios: List[Decimal]) -> Dict[str, Dict]:
        """Stress test portfolio under parallel yield curve shifts"""
        base_value = Decimal('0')

        # Calculate base case value
        for bond, quantity in portfolio.holdings:
            bond_value = BondValuation.present_value_with_curve(bond, base_curve)
            base_value += bond_value * quantity

        results = {'base_case': {'value': base_value, 'change': Decimal('0'), 'change_pct': Decimal('0')}}

        for shift in stress_scenarios:
            stressed_curve = base_curve.shift_curve(shift, "parallel")
            stressed_value = Decimal('0')

            for bond, quantity in portfolio.holdings:
                bond_value = BondValuation.present_value_with_curve(bond, stressed_curve)
                stressed_value += bond_value * quantity

            change = stressed_value - base_value
            change_pct = (change / base_value * Decimal('100')) if base_value != 0 else Decimal('0')

            scenario_name = f"shift_{shift * 10000:.0f}bp"
            results[scenario_name] = {
                'value': stressed_value,
                'change': change,
                'change_pct': change_pct
            }

        return results

    @staticmethod
    def yield_curve_scenario_analysis(portfolio: Portfolio, base_curve: SpotCurve,
                                      scenarios: Dict[str, str]) -> Dict[str, Dict]:
        """Analyze portfolio under different yield curve scenarios"""
        base_value = Decimal('0')

        # Calculate base case value
        for bond, quantity in portfolio.holdings:
            bond_value = BondValuation.present_value_with_curve(bond, base_curve)
            base_value += bond_value * quantity

        results = {'base_case': {'value': base_value, 'change': Decimal('0'), 'change_pct': Decimal('0')}}

        for scenario_name, scenario_type in scenarios.items():
            if scenario_type == "steepening":
                stressed_curve = base_curve.shift_curve(Decimal('0.01'), "steepening")
            elif scenario_type == "flattening":
                stressed_curve = base_curve.shift_curve(Decimal('0.01'), "flattening")
            elif scenario_type == "bear_steepening":
                # Rates up more at long end
                stressed_curve = base_curve.shift_curve(Decimal('0.005'), "parallel")
                stressed_curve = stressed_curve.shift_curve(Decimal('0.005'), "steepening")
            elif scenario_type == "bull_flattening":
                # Rates down more at long end
                stressed_curve = base_curve.shift_curve(Decimal('-0.005'), "parallel")
                stressed_curve = stressed_curve.shift_curve(Decimal('0.005'), "flattening")
            else:
                continue

            stressed_value = Decimal('0')
            for bond, quantity in portfolio.holdings:
                bond_value = BondValuation.present_value_with_curve(bond, stressed_curve)
                stressed_value += bond_value * quantity

            change = stressed_value - base_value
            change_pct = (change / base_value * Decimal('100')) if base_value != 0 else Decimal('0')

            results[scenario_name] = {
                'value': stressed_value,
                'change': change,
                'change_pct': change_pct
            }

        return results


class HedgingAnalytics:
    """Hedging calculations and analytics"""

    @staticmethod
    def duration_hedge_ratio(target_bond: Bond, hedge_bond: Bond,
                             target_duration: Decimal, hedge_duration: Decimal) -> Decimal:
        """Calculate hedge ratio for duration hedging"""
        if hedge_duration == 0:
            return Decimal('0')

        return target_duration / hedge_duration

    @staticmethod
    def optimal_hedge_portfolio(target_portfolio: Portfolio, hedge_instruments: List[Bond],
                                base_curve: SpotCurve) -> Dict[str, Decimal]:
        """Calculate optimal hedge portfolio weights (simplified)"""
        # Calculate target portfolio duration
        target_duration = Decimal('0')
        target_value = Decimal('0')

        for bond, quantity in target_portfolio.holdings:
            bond_value = BondValuation.present_value_with_curve(bond, base_curve)
            bond_duration = DurationMeasures.effective_duration(bond, base_curve)

            position_value = bond_value * quantity
            target_value += position_value
            target_duration += bond_duration * position_value

        if target_value != 0:
            target_duration = target_duration / target_value

        # Simple two-instrument hedge (e.g., short and long duration instruments)
        if len(hedge_instruments) >= 2:
            short_bond = hedge_instruments[0]
            long_bond = hedge_instruments[1]

            short_duration = DurationMeasures.effective_duration(short_bond, base_curve)
            long_duration = DurationMeasures.effective_duration(long_bond, base_curve)

            # Solve for weights to achieve zero portfolio duration
            # w1 * d1 + w2 * d2 = -target_duration (to offset)
            # w1 + w2 = 1 (budget constraint, simplified)

            if long_duration != short_duration:
                w2 = (-target_duration - short_duration) / (long_duration - short_duration)
                w1 = Decimal('1') - w2

                return {
                    short_bond.isin: w1,
                    long_bond.isin: w2
                }

        return {}


# Utility functions for risk reporting
def generate_risk_report(portfolio: Portfolio, base_curve: SpotCurve,
                         yields: Dict[str, Decimal], prices: Dict[str, Decimal]) -> Dict:
    """Generate comprehensive risk report for portfolio"""

    # Basic metrics
    portfolio_dur = PortfolioRiskMetrics.portfolio_duration(portfolio, yields, prices)
    portfolio_conv = PortfolioRiskMetrics.portfolio_convexity(portfolio, base_curve, prices)
    portfolio_yld = PortfolioRiskMetrics.portfolio_yield(portfolio, yields, prices)

    # Calculate total portfolio value
    total_value = Decimal('0')
    for bond, quantity in portfolio.holdings:
        if bond.isin in prices:
            total_value += prices[bond.isin] * quantity

    # VaR calculation
    yield_vol = Decimal('0.01')  # Assume 1% daily yield volatility
    var_95 = ValueAtRisk.parametric_var(total_value, portfolio_dur, yield_vol, Decimal('0.95'))
    var_99 = ValueAtRisk.parametric_var(total_value, portfolio_dur, yield_vol, Decimal('0.99'))

    # Stress tests
    stress_scenarios = [Decimal('-0.02'), Decimal('-0.01'), Decimal('0.01'), Decimal('0.02')]  # ±100bp, ±200bp
    stress_results = StressTestingFramework.parallel_shift_stress_test(portfolio, base_curve, stress_scenarios)

    return {
        'portfolio_metrics': {
            'duration': portfolio_dur,
            'convexity': portfolio_conv,
            'yield': portfolio_yld,
            'total_value': total_value
        },
        'risk_metrics': {
            'var_95': var_95,
            'var_99': var_99
        },
        'stress_test_results': stress_results
    }