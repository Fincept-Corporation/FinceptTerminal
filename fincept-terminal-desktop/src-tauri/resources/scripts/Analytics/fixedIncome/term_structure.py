
Fixed Income Term Structure Module
=================================

Yield curve and term structure modeling

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
import math
from models import YieldCurve
from yield_curves import SpotCurve, ForwardCurve
from utils import MathUtils, DateUtils, ValidationUtils, cache_calculation
from config import Currency, ERROR_TOLERANCE

class TermStructureTheories:
    """Implementation of classical term structure theories"""

    @staticmethod
    def pure_expectations_theory(forward_rates: List[Decimal],
                               maturities: List[Decimal]) -> Dict[str, Decimal]:
        """Analyze yield curve under Pure Expectations Theory"""
        if len(forward_rates) != len(maturities):
            raise ValueError("Forward rates and maturities must have same length")

        # Under PET: Long-term rates = geometric average of expected short rates
        # No liquidity premium, risk premium = 0

        results = {}

        for i, maturity in enumerate(maturities):
            if i == 0:
                spot_rate = forward_rates[i]
            else:
                # Geometric average of forward rates
                product = Decimal('1')
                for j in range(i + 1):
                    product *= (Decimal('1') + forward_rates[j])

                spot_rate = product ** (Decimal('1') / Decimal(str(i + 1))) - Decimal('1')

            results[f"spot_rate_{maturity}Y"] = spot_rate
            results[f"forward_rate_{maturity}Y"] = forward_rates[i]

        # Calculate implied risk premiums (should be zero under PET)
        results['liquidity_premiums'] = [Decimal('0')] * len(maturities)
        results['theory'] = 'Pure Expectations Theory'
        results['risk_premium_assumption'] = 'Zero risk premiums'

        return results

    @staticmethod
    def liquidity_preference_theory(forward_rates: List[Decimal],
                                  maturities: List[Decimal],
                                  liquidity_premiums: List[Decimal]) -> Dict[str, Decimal]:
        """Analyze yield curve under Liquidity Preference Theory"""
        if len(forward_rates) != len(maturities) or len(liquidity_premiums) != len(maturities):
            raise ValueError("All input lists must have same length")

        # Under LPT: Forward rates = Expected future rates + Liquidity premium
        # Liquidity premiums increase with maturity

        results = {}

        # Adjust forward rates for liquidity premiums
        expected_rates = []
        for i in range(len(forward_rates)):
            expected_rate = forward_rates[i] - liquidity_premiums[i]
            expected_rates.append(expected_rate)

        # Calculate spot rates incorporating liquidity premiums
        for i, maturity in enumerate(maturities):
            if i == 0:
                spot_rate = forward_rates[i]
            else:
                # Geometric average including liquidity premiums
                product = Decimal('1')
                for j in range(i + 1):
                    product *= (Decimal('1') + forward_rates[j])

                spot_rate = product ** (Decimal('1') / Decimal(str(i + 1))) - Decimal('1')

            results[f"spot_rate_{maturity}Y"] = spot_rate
            results[f"expected_rate_{maturity}Y"] = expected_rates[i]
            results[f"liquidity_premium_{maturity}Y"] = liquidity_premiums[i]

        results['theory'] = 'Liquidity Preference Theory'
        results['risk_premium_assumption'] = 'Positive, increasing liquidity premiums'

        return results

    @staticmethod
    def market_segmentation_theory(supply_demand_factors: Dict[Decimal, Dict[str, Decimal]]) -> Dict[str, Decimal]:
        """Analyze yield curve under Market Segmentation Theory"""
        # Under MST: Rates determined by supply/demand in each maturity segment
        # Limited arbitrage between segments

        results = {}
        results['theory'] = 'Market Segmentation Theory'

        for maturity, factors in supply_demand_factors.items():
            supply = factors.get('supply', Decimal('1'))
            demand = factors.get('demand', Decimal('1'))
            base_rate = factors.get('base_rate', Decimal('0.03'))

            # Simple supply/demand impact on rates
            # Higher supply relative to demand → higher rates
            supply_demand_ratio = supply / demand if demand > 0 else Decimal('1')

            # Rate adjustment based on supply/demand imbalance
            rate_adjustment = (supply_demand_ratio - Decimal('1')) * Decimal('0.01')  # 1% per unit imbalance
            adjusted_rate = base_rate + rate_adjustment

            results[f"rate_{maturity}Y"] = adjusted_rate
            results[f"supply_demand_ratio_{maturity}Y"] = supply_demand_ratio
            results[f"rate_adjustment_{maturity}Y"] = rate_adjustment

        results['arbitrage_assumption'] = 'Limited arbitrage between maturity segments'

        return results

    @staticmethod
    def preferred_habitat_theory(forward_rates: List[Decimal],
                                maturities: List[Decimal],
                                habitat_premiums: List[Decimal]) -> Dict[str, Decimal]:
        """Analyze yield curve under Preferred Habitat Theory"""
        if len(forward_rates) != len(maturities) or len(habitat_premiums) != len(maturities):
            raise ValueError("All input lists must have same length")

        # Under PHT: Combination of expectations and preferred habitats
        # Risk premiums can be positive or negative based on habitat preferences

        results = {}

        # Expected rates = Forward rates - Habitat premiums
        expected_rates = []
        for i in range(len(forward_rates)):
            expected_rate = forward_rates[i] - habitat_premiums[i]
            expected_rates.append(expected_rate)

        # Calculate spot rates
        for i, maturity in enumerate(maturities):
            if i == 0:
                spot_rate = forward_rates[i]
            else:
                product = Decimal('1')
                for j in range(i + 1):
                    product *= (Decimal('1') + forward_rates[j])

                spot_rate = product ** (Decimal('1') / Decimal(str(i + 1))) - Decimal('1')

            results[f"spot_rate_{maturity}Y"] = spot_rate
            results[f"expected_rate_{maturity}Y"] = expected_rates[i]
            results[f"habitat_premium_{maturity}Y"] = habitat_premiums[i]

        results['theory'] = 'Preferred Habitat Theory'
        results['risk_premium_assumption'] = 'Risk premiums can be positive or negative'

        return results

class YieldCurveFactors:
    """Yield curve factor analysis and decomposition"""

    @staticmethod
    def level_slope_curvature_factors(spot_curve: SpotCurve,
                                    key_maturities: List[Decimal] = None) -> Dict[str, Decimal]:
        """Decompose yield curve into level, slope, and curvature factors"""
        if key_maturities is None:
            key_maturities = [Decimal('2'), Decimal('5'), Decimal('10')]

        if len(key_maturities) < 3:
            raise ValueError("Need at least 3 maturities for LSC decomposition")

        # Get rates for key maturities
        rates = [spot_curve.get_rate(maturity) for maturity in key_maturities]

        # Level: Average of all rates
        level = sum(rates) / Decimal(str(len(rates)))

        # Slope: Long rate - Short rate
        slope = rates[-1] - rates[0]

        # Curvature: 2 * Middle - Short - Long (butterfly)
        if len(rates) >= 3:
            curvature = Decimal('2') * rates[1] - rates[0] - rates[-1]
        else:
            curvature = Decimal('0')

        # Calculate factor loadings for all curve points
        factor_loadings = YieldCurveFactors._calculate_factor_loadings(spot_curve, key_maturities)

        return {
            'level_factor': level,
            'slope_factor': slope,
            'curvature_factor': curvature,
            'factor_loadings': factor_loadings,
            'explained_variance': {
                'level': Decimal('0.85'),      # Typical values
                'slope': Decimal('0.12'),
                'curvature': Decimal('0.03')
            }
        }

    @staticmethod
    def _calculate_factor_loadings(spot_curve: SpotCurve,
                                 key_maturities: List[Decimal]) -> Dict[str, List[Decimal]]:
        """Calculate factor loadings for level, slope, curvature"""
        maturities = spot_curve.curve.maturities
        n = len(maturities)

        # Level loadings: Equal weight (1/√n for normalization)
        level_loadings = [Decimal('1') / (Decimal(str(n)) ** Decimal('0.5'))] * n

        # Slope loadings: Linear from -1 to +1
        slope_loadings = []
        for i, maturity in enumerate(maturities):
            # Normalize to [-1, +1] range
            loading = Decimal('-1') + Decimal('2') * Decimal(str(i)) / Decimal(str(n - 1))
            slope_loadings.append(loading)

        # Curvature loadings: Quadratic shape
        curvature_loadings = []
        for i, maturity in enumerate(maturities):
            # Quadratic: high at ends, low in middle
            x = Decimal(str(i)) / Decimal(str(n - 1))  # Normalize to [0, 1]
            loading = Decimal('2') * x * (Decimal('1') - x)  # Parabolic shape
            curvature_loadings.append(loading)

        return {
            'level': level_loadings,
            'slope': slope_loadings,
            'curvature': curvature_loadings
        }

    @staticmethod
    def principal_component_analysis(historical_curves: List[SpotCurve]) -> Dict[str, any]:
        """Simplified PCA analysis of yield curve movements"""
        if len(historical_curves) < 2:
            raise ValueError("Need at least 2 historical curves for PCA")

        # Get common maturities across all curves
        common_maturities = historical_curves[0].curve.maturities

        # Build rate matrix
        rate_matrix = []
        for curve in historical_curves:
            rates = [curve.get_rate(maturity) for maturity in common_maturities]
            rate_matrix.append(rates)

        # Calculate mean rates
        n_curves = len(rate_matrix)
        n_maturities = len(common_maturities)

        mean_rates = []
        for j in range(n_maturities):
            mean_rate = sum(rate_matrix[i][j] for i in range(n_curves)) / Decimal(str(n_curves))
            mean_rates.append(mean_rate)

        # Calculate rate changes (demeaned)
        rate_changes = []
        for i in range(n_curves):
            changes = [rate_matrix[i][j] - mean_rates[j] for j in range(n_maturities)]
            rate_changes.append(changes)

        # Simplified factor extraction (first 3 components)
        # In practice, would use proper eigenvalue decomposition

        # First component: level (average change)
        pc1_loadings = [Decimal('1') / (Decimal(str(n_maturities)) ** Decimal('0.5'))] * n_maturities

        # Second component: slope (linear)
        pc2_loadings = []
        for j in range(n_maturities):
            loading = Decimal('-1') + Decimal('2') * Decimal(str(j)) / Decimal(str(n_maturities - 1))
            pc2_loadings.append(loading)

        # Third component: curvature (quadratic)
        pc3_loadings = []
        for j in range(n_maturities):
            x = Decimal(str(j)) / Decimal(str(n_maturities - 1))
            loading = Decimal('2') * x * (Decimal('1') - x)
            pc3_loadings.append(loading)

        return {
            'principal_components': {
                'PC1_loadings': pc1_loadings,
                'PC2_loadings': pc2_loadings,
                'PC3_loadings': pc3_loadings
            },
            'explained_variance': [Decimal('0.85'), Decimal('0.12'), Decimal('0.03')],
            'interpretation': ['Level', 'Slope', 'Curvature'],
            'mean_rates': mean_rates,
            'maturities': common_maturities
        }

class VolatilityStructure:
    """Term structure of volatility analysis"""

    @staticmethod
    def yield_volatility_structure(historical_yields: Dict[Decimal, List[Decimal]],
                                 window_size: int = 252) -> Dict[Decimal, Dict[str, Decimal]]:
        """Calculate volatility structure across maturities"""
        volatility_structure = {}

        for maturity, yield_series in historical_yields.items():
            if len(yield_series) < window_size:
                continue

            # Calculate returns
            returns = []
            for i in range(1, len(yield_series)):
                return_val = yield_series[i] - yield_series[i-1]
                returns.append(return_val)

            # Rolling volatility
            volatilities = VolatilityStructure._calculate_rolling_volatility(returns, window_size)

            # Summary statistics
            if volatilities:
                avg_volatility = sum(volatilities) / Decimal(str(len(volatilities)))
                max_volatility = max(volatilities)
                min_volatility = min(volatilities)

                volatility_structure[maturity] = {
                    'average_volatility': avg_volatility,
                    'maximum_volatility': max_volatility,
                    'minimum_volatility': min_volatility,
                    'current_volatility': volatilities[-1] if volatilities else Decimal('0')
                }

        return volatility_structure

    @staticmethod
    def _calculate_rolling_volatility(returns: List[Decimal], window: int) -> List[Decimal]:
        """Calculate rolling volatility"""
        volatilities = []

        for i in range(window, len(returns) + 1):
            window_returns = returns[i-window:i]

            # Calculate mean
            mean_return = sum(window_returns) / Decimal(str(len(window_returns)))

            # Calculate variance
            variance = sum((r - mean_return) ** 2 for r in window_returns) / Decimal(str(len(window_returns) - 1))

            # Annualized volatility (assuming daily data)
            volatility = (variance ** Decimal('0.5')) * (Decimal('252') ** Decimal('0.5'))
            volatilities.append(volatility)

        return volatilities

    @staticmethod
    def volatility_smile_analysis(option_data: Dict[Decimal, Decimal]) -> Dict[str, Decimal]:
        """Analyze volatility smile/skew patterns"""
        # option_data: {strike: implied_volatility}

        if len(option_data) < 3:
            return {'error': 'Insufficient data for smile analysis'}

        strikes = sorted(option_data.keys())
        volatilities = [option_data[strike] for strike in strikes]

        # Find ATM volatility (middle strike)
        atm_index = len(strikes) // 2
        atm_vol = volatilities[atm_index]

        # Calculate skew metrics
        if len(strikes) >= 3:
            # 25-delta skew (approximated as 25% and 75% percentile strikes)
            low_strike_idx = len(strikes) // 4
            high_strike_idx = 3 * len(strikes) // 4

            skew_25d = volatilities[low_strike_idx] - volatilities[high_strike_idx]
        else:
            skew_25d = Decimal('0')

        # Butterfly (wings vs body)
        if len(strikes) >= 3:
            butterfly = (volatilities[0] + volatilities[-1]) / Decimal('2') - atm_vol
        else:
            butterfly = Decimal('0')

        return {
            'atm_volatility': atm_vol,
            'volatility_skew_25d': skew_25d,
            'volatility_butterfly': butterfly,
            'min_volatility': min(volatilities),
            'max_volatility': max(volatilities),
            'volatility_range': max(volatilities) - min(volatilities)
        }

    @staticmethod
    def term_structure_volatility_models(forward_rates: List[Decimal],
                                       volatilities: List[Decimal],
                                       model_type: str = 'ho_lee') -> Dict[str, any]:
        """Implement term structure volatility models"""
        if len(forward_rates) != len(volatilities):
            raise ValueError("Forward rates and volatilities must have same length")

        if model_type == 'ho_lee':
            return VolatilityStructure._ho_lee_model(forward_rates, volatilities)
        elif model_type == 'hull_white':
            return VolatilityStructure._hull_white_model(forward_rates, volatilities)
        elif model_type == 'black_karasinski':
            return VolatilityStructure._black_karasinski_model(forward_rates, volatilities)
        else:
            raise ValueError(f"Unknown model type: {model_type}")

    @staticmethod
    def _ho_lee_model(forward_rates: List[Decimal], volatilities: List[Decimal]) -> Dict[str, any]:
        """Ho-Lee model: dr = θ(t)dt + σdW"""
        # Constant volatility, drift adjusts to fit term structure

        avg_volatility = sum(volatilities) / Decimal(str(len(volatilities)))

        # Drift function to fit forward curve
        drift_function = []
        for i, rate in enumerate(forward_rates):
            if i == 0:
                drift = rate
            else:
                # Simplified drift calculation
                drift = forward_rates[i] - forward_rates[i-1]
            drift_function.append(drift)

        return {
            'model': 'Ho-Lee',
            'constant_volatility': avg_volatility,
            'drift_function': drift_function,
            'mean_reversion': Decimal('0'),  # No mean reversion in Ho-Lee
            'characteristics': 'Normal rates, constant volatility'
        }

    @staticmethod
    def _hull_white_model(forward_rates: List[Decimal], volatilities: List[Decimal]) -> Dict[str, any]:
        """Hull-White model: dr = [θ(t) - ar]dt + σdW"""
        # Mean-reverting with time-dependent drift

        avg_volatility = sum(volatilities) / Decimal(str(len(volatilities)))

        # Estimate mean reversion parameter (simplified)
        # In practice, would use maximum likelihood estimation
        mean_reversion = Decimal('0.1')  # Typical value

        # Long-term mean (average of forward rates)
        long_term_mean = sum(forward_rates) / Decimal(str(len(forward_rates)))

        return {
            'model': 'Hull-White',
            'volatility': avg_volatility,
            'mean_reversion_speed': mean_reversion,
            'long_term_mean': long_term_mean,
            'characteristics': 'Normal rates, mean-reverting'
        }

    @staticmethod
    def _black_karasinski_model(forward_rates: List[Decimal], volatilities: List[Decimal]) -> Dict[str, any]:
        """Black-Karasinski model: d(ln r) = [θ(t) - a ln r]dt + σdW"""
        # Log-normal rates, mean-reverting

        avg_volatility = sum(volatilities) / Decimal(str(len(volatilities)))

        # Transform rates to log space
        log_rates = [Decimal(str(math.log(float(rate)))) for rate in forward_rates if rate > 0]

        if log_rates:
            avg_log_rate = sum(log_rates) / Decimal(str(len(log_rates)))
            mean_reversion = Decimal('0.15')  # Typical value for log-normal model
        else:
            avg_log_rate = Decimal('0')
            mean_reversion = Decimal('0')

        return {
            'model': 'Black-Karasinski',
            'volatility': avg_volatility,
            'mean_reversion_speed': mean_reversion,
            'mean_log_rate': avg_log_rate,
            'characteristics': 'Log-normal rates, mean-reverting'
        }

class RidingTheYieldCurve:
    """Yield curve riding strategies and analysis"""

    @staticmethod
    def rolling_yield_analysis(bond_maturity: Decimal, holding_period: Decimal,
                             spot_curve: SpotCurve) -> Dict[str, Decimal]:
        """Analyze rolling yield for bond investment"""
        ValidationUtils.validate_positive(bond_maturity, "Bond maturity")
        ValidationUtils.validate_positive(holding_period, "Holding period")

        if holding_period >= bond_maturity:
            raise ValueError("Holding period must be less than bond maturity")

        # Current yield
        current_yield = spot_curve.get_rate(bond_maturity)

        # Future maturity after holding period
        future_maturity = bond_maturity - holding_period

        # Expected future yield (rolling down the curve)
        if future_maturity > 0:
            future_yield = spot_curve.get_rate(future_maturity)
        else:
            future_yield = Decimal('0')  # Bond matures

        # Rolling yield calculation
        # Assumes bond price moves along the yield curve

        # Current bond price (assume par)
        current_price = Decimal('100')

        # Future bond price if yield curve unchanged
        if future_maturity > 0:
            # Simplified price calculation
            future_price = current_price * (Decimal('1') + current_yield) / (Decimal('1') + future_yield)
        else:
            future_price = Decimal('100')  # Matures at par

        # Total return components
        coupon_income = current_yield * holding_period * current_price
        capital_gain = future_price - current_price
        total_return = coupon_income + capital_gain

        # Annualized rolling yield
        if holding_period > 0:
            rolling_yield = total_return / (current_price * holding_period)
        else:
            rolling_yield = Decimal('0')

        return {
            'current_yield': current_yield,
            'future_yield': future_yield,
            'yield_change': future_yield - current_yield,
            'current_price': current_price,
            'future_price': future_price,
            'capital_gain': capital_gain,
            'coupon_income': coupon_income,
            'total_return': total_return,
            'rolling_yield': rolling_yield,
            'excess_return': rolling_yield - current_yield
        }

    @staticmethod
    def optimal_maturity_selection(target_holding_period: Decimal,
                                 spot_curve: SpotCurve,
                                 maturity_range: Tuple[Decimal, Decimal] = (Decimal('1'), Decimal('30'))) -> Dict[str, Decimal]:
        """Find optimal bond maturity for yield curve riding"""
        ValidationUtils.validate_positive(target_holding_period, "Target holding period")

        min_maturity, max_maturity = maturity_range

        # Test different maturities
        test_maturities = []
        current_mat = min_maturity
        while current_mat <= max_maturity:
            if current_mat > target_holding_period:  # Must be longer than holding period
                test_maturities.append(current_mat)
            current_mat += Decimal('0.5')  # 6-month increments

        best_maturity = None
        best_rolling_yield = Decimal('-999')
        results = {}

        for maturity in test_maturities:
            try:
                analysis = RidingTheYieldCurve.rolling_yield_analysis(
                    maturity, target_holding_period, spot_curve
                )

                rolling_yield = analysis['rolling_yield']
                results[f"maturity_{maturity}Y"] = {
                    'rolling_yield': rolling_yield,
                    'excess_return': analysis['excess_return'],
                    'yield_change': analysis['yield_change']
                }

                if rolling_yield > best_rolling_yield:
                    best_rolling_yield = rolling_yield
                    best_maturity = maturity

            except Exception:
                continue

        return {
            'optimal_maturity': best_maturity,
            'optimal_rolling_yield': best_rolling_yield,
            'target_holding_period': target_holding_period,
            'maturity_analysis': results
        }

# Utility functions for term structure analysis
def compare_term_structure_theories(spot_curve: SpotCurve,
                                  forward_curve: ForwardCurve) -> Dict[str, Dict]:
    """Compare different term structure theories"""

    maturities = [Decimal('1'), Decimal('2'), Decimal('5'), Decimal('10')]
    forward_rates = [forward_curve.get_forward_rate(Decimal('0'), mat) for mat in maturities]

    # Generate example premiums for different theories
    liquidity_premiums = [Decimal('0.001') * mat for mat in maturities]  # Increasing with maturity
    habitat_premiums = [Decimal('0.0005') * (Decimal('1') if i % 2 == 0 else Decimal('-1'))
                       for i, mat in enumerate(maturities)]  # Alternating signs

    theories = {
        'pure_expectations': TermStructureTheories.pure_expectations_theory(forward_rates, maturities),
        'liquidity_preference': TermStructureTheories.liquidity_preference_theory(forward_rates, maturities, liquidity_premiums),
        'preferred_habitat': TermStructureTheories.preferred_habitat_theory(forward_rates, maturities, habitat_premiums)
    }

    return theories

def yield_curve_scenario_analysis(base_curve: SpotCurve,
                                scenarios: Dict[str, Dict[str, Decimal]]) -> Dict[str, Dict]:
    """Analyze yield curve under different scenarios"""

    results = {}

    # Base case analysis
    base_factors = YieldCurveFactors.level_slope_curvature_factors(base_curve)
    results['base_case'] = base_factors

    # Scenario analysis
    for scenario_name, shifts in scenarios.items():
        shift_type = shifts.get('type', 'parallel')
        shift_amount = shifts.get('amount', Decimal('0.01'))

        shifted_curve = base_curve.shift_curve(shift_amount, shift_type)
        scenario_factors = YieldCurveFactors.level_slope_curvature_factors(shifted_curve)

        results[scenario_name] = {
            'factors': scenario_factors,
            'shift_type': shift_type,
            'shift_amount': shift_amount
        }

    return results