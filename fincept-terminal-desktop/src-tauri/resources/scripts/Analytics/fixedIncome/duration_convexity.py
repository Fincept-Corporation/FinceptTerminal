"""
Duration and Convexity Analytics Module
=======================================

Interest rate risk measurement implementing CFA Institute standard methodologies
for duration and convexity calculations.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Bond specifications (face value, coupon rate, maturity)
  - Yield to maturity or spot rate curve
  - Cash flow schedules
  - Option-adjusted parameters for callable/putable bonds

OUTPUT:
  - Macaulay Duration
  - Modified Duration
  - Effective Duration (for bonds with embedded options)
  - Key Rate Duration
  - Convexity (standard and effective)
  - Price sensitivity estimates
  - Dollar duration and DV01

PARAMETERS:
  - face_value: Par/face value of the bond - default: 1000
  - coupon_rate: Annual coupon rate as decimal
  - years_to_maturity: Time to maturity in years
  - ytm: Yield to maturity as decimal
  - frequency: Coupon payment frequency per year - default: 2
  - delta_yield: Yield change for effective duration - default: 0.0001 (1bp)
"""

from dataclasses import dataclass
from typing import Dict, Any, List, Optional, Tuple
from enum import Enum
import numpy as np
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class DurationType(Enum):
    """Types of duration measures"""
    MACAULAY = "macaulay"
    MODIFIED = "modified"
    EFFECTIVE = "effective"
    KEY_RATE = "key_rate"
    DOLLAR = "dollar"


@dataclass
class DurationResult:
    """Container for duration calculation results"""
    macaulay_duration: float
    modified_duration: float
    dollar_duration: float
    dv01: float
    convexity: float
    price: float


class DurationCalculator:
    """
    Duration calculator implementing CFA-standard methodologies.

    Duration measures the sensitivity of bond price to interest rate changes.
    """

    def calculate_macaulay_duration(
        self,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        ytm: float = 0.05,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate Macaulay Duration - weighted average time to receive cash flows.

        MacDur = sum(t * PV(CF_t)) / Price

        Args:
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_maturity: Years until maturity
            ytm: Yield to maturity (decimal)
            frequency: Coupon payments per year

        Returns:
            Dictionary with Macaulay duration and details
        """
        if frequency == 0:
            # Zero coupon bond - duration equals maturity
            price = face_value / ((1 + ytm) ** years_to_maturity)
            return {
                'macaulay_duration': years_to_maturity,
                'price': round(price, 4),
                'weighted_cash_flows': [{'period': years_to_maturity, 'weight': 1.0}]
            }

        periods = int(years_to_maturity * frequency)
        periodic_rate = ytm / frequency
        coupon = (coupon_rate * face_value) / frequency

        weighted_cf_sum = 0
        price = 0
        cash_flow_details = []

        for t in range(1, periods + 1):
            time_in_years = t / frequency
            cf = coupon if t < periods else coupon + face_value
            discount_factor = 1 / ((1 + periodic_rate) ** t)
            pv_cf = cf * discount_factor
            weighted_cf = time_in_years * pv_cf

            price += pv_cf
            weighted_cf_sum += weighted_cf

            cash_flow_details.append({
                'period': t,
                'time_years': round(time_in_years, 4),
                'cash_flow': round(cf, 2),
                'pv': round(pv_cf, 4),
                'weighted_pv': round(weighted_cf, 4)
            })

        macaulay_duration = weighted_cf_sum / price

        return {
            'macaulay_duration': round(macaulay_duration, 4),
            'macaulay_duration_periods': round(macaulay_duration * frequency, 4),
            'price': round(price, 4),
            'total_pv': round(price, 4),
            'weighted_sum': round(weighted_cf_sum, 4),
            'cash_flow_details': cash_flow_details
        }

    def calculate_modified_duration(
        self,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        ytm: float = 0.05,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate Modified Duration - price sensitivity measure.

        ModDur = MacDur / (1 + y/n)

        Args:
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_maturity: Years until maturity
            ytm: Yield to maturity (decimal)
            frequency: Coupon payments per year

        Returns:
            Dictionary with modified duration
        """
        mac_result = self.calculate_macaulay_duration(
            face_value, coupon_rate, years_to_maturity, ytm, frequency
        )
        macaulay_duration = mac_result['macaulay_duration']
        price = mac_result['price']

        # Modified duration
        if frequency > 0:
            modified_duration = macaulay_duration / (1 + ytm / frequency)
        else:
            modified_duration = macaulay_duration / (1 + ytm)

        # Dollar duration (DV01 for 1bp change)
        dollar_duration = modified_duration * price / 100
        dv01 = modified_duration * price * 0.0001

        # Price change estimates
        yield_change_1pct = modified_duration * 0.01 * price
        yield_change_50bp = modified_duration * 0.005 * price

        return {
            'modified_duration': round(modified_duration, 4),
            'macaulay_duration': round(macaulay_duration, 4),
            'dollar_duration': round(dollar_duration, 4),
            'dv01': round(dv01, 4),
            'price': round(price, 4),
            'price_change_1pct_yield': round(yield_change_1pct, 4),
            'price_change_50bp_yield': round(yield_change_50bp, 4),
            'interpretation': f"A 1% increase in yield decreases price by approximately {round(yield_change_1pct, 2)}"
        }

    def calculate_effective_duration(
        self,
        price: float,
        price_up: float,
        price_down: float,
        delta_yield: float = 0.01,
    ) -> Dict[str, Any]:
        """
        Calculate Effective Duration for bonds with embedded options.

        EffDur = (P_down - P_up) / (2 * P_0 * delta_y)

        Args:
            price: Current bond price
            price_up: Price when yield increases by delta_yield
            price_down: Price when yield decreases by delta_yield
            delta_yield: Yield change magnitude (decimal)

        Returns:
            Dictionary with effective duration
        """
        if price <= 0 or delta_yield <= 0:
            return {'error': 'Invalid inputs'}

        effective_duration = (price_down - price_up) / (2 * price * delta_yield)

        # Price sensitivity estimate
        pct_change_estimate = effective_duration * delta_yield * 100

        return {
            'effective_duration': round(effective_duration, 4),
            'delta_yield': delta_yield,
            'delta_yield_bps': round(delta_yield * 10000, 0),
            'price_base': round(price, 4),
            'price_up': round(price_up, 4),
            'price_down': round(price_down, 4),
            'price_change_pct': round(pct_change_estimate, 4),
            'note': 'Use for bonds with embedded options (callable, putable, MBS)'
        }

    def calculate_key_rate_duration(
        self,
        spot_rates: List[float],
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        frequency: int = 2,
        delta_rate: float = 0.0001,
        key_rate_maturities: List[float] = None,
    ) -> Dict[str, Any]:
        """
        Calculate Key Rate Durations - sensitivity to specific points on yield curve.

        Args:
            spot_rates: Current spot rate curve
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            frequency: Coupon payments per year
            delta_rate: Rate change for sensitivity (decimal)
            key_rate_maturities: Specific maturities to calculate KRD

        Returns:
            Dictionary with key rate durations
        """
        if key_rate_maturities is None:
            key_rate_maturities = [1, 2, 3, 5, 7, 10, 20, 30]

        coupon = (coupon_rate * face_value) / frequency
        periods = len(spot_rates)

        # Calculate base price
        base_price = 0
        for i, spot in enumerate(spot_rates):
            t = (i + 1) / frequency
            cf = coupon if i < periods - 1 else coupon + face_value
            base_price += cf / ((1 + spot) ** t)

        key_rate_durations = []

        for key_maturity in key_rate_maturities:
            if key_maturity > periods / frequency:
                continue

            # Shift rate at key maturity
            shocked_rates = spot_rates.copy()
            key_period = int(key_maturity * frequency) - 1

            if key_period < len(shocked_rates):
                shocked_rates[key_period] += delta_rate

                # Calculate shocked price
                shocked_price = 0
                for i, spot in enumerate(shocked_rates):
                    t = (i + 1) / frequency
                    cf = coupon if i < periods - 1 else coupon + face_value
                    shocked_price += cf / ((1 + spot) ** t)

                krd = -(shocked_price - base_price) / (base_price * delta_rate)

                key_rate_durations.append({
                    'maturity': key_maturity,
                    'key_rate_duration': round(krd, 4),
                    'contribution_pct': round(krd * 100 / sum(k['key_rate_duration'] for k in key_rate_durations) if key_rate_durations else 0, 2)
                })

        total_krd = sum(k['key_rate_duration'] for k in key_rate_durations)

        return {
            'key_rate_durations': key_rate_durations,
            'total_krd': round(total_krd, 4),
            'base_price': round(base_price, 4),
            'note': 'KRD shows sensitivity to specific points on the yield curve'
        }


class ConvexityCalculator:
    """
    Convexity calculator for measuring second-order price sensitivity.

    Convexity captures the curvature in the price-yield relationship.
    """

    def calculate_convexity(
        self,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        ytm: float = 0.05,
        frequency: int = 2,
    ) -> Dict[str, Any]:
        """
        Calculate bond convexity.

        Convexity = sum(t*(t+1)*PV(CF_t)) / (Price * (1+y/n)^2 * n^2)

        Args:
            face_value: Face/par value
            coupon_rate: Annual coupon rate (decimal)
            years_to_maturity: Years until maturity
            ytm: Yield to maturity (decimal)
            frequency: Coupon payments per year

        Returns:
            Dictionary with convexity measure
        """
        if frequency == 0:
            # Zero coupon bond
            price = face_value / ((1 + ytm) ** years_to_maturity)
            convexity = years_to_maturity * (years_to_maturity + 1) / ((1 + ytm) ** 2)
            return {
                'convexity': round(convexity, 4),
                'price': round(price, 4),
                'note': 'Zero coupon bond convexity'
            }

        periods = int(years_to_maturity * frequency)
        periodic_rate = ytm / frequency
        coupon = (coupon_rate * face_value) / frequency

        price = 0
        convexity_sum = 0

        for t in range(1, periods + 1):
            cf = coupon if t < periods else coupon + face_value
            discount_factor = 1 / ((1 + periodic_rate) ** t)
            pv_cf = cf * discount_factor

            price += pv_cf
            convexity_sum += t * (t + 1) * pv_cf

        # Convexity formula
        convexity = convexity_sum / (price * ((1 + periodic_rate) ** 2) * (frequency ** 2))

        # Dollar convexity
        dollar_convexity = convexity * price / 100

        return {
            'convexity': round(convexity, 4),
            'convexity_years': round(convexity, 4),
            'dollar_convexity': round(dollar_convexity, 4),
            'price': round(price, 4),
            'convexity_adjustment_1pct': round(0.5 * convexity * (0.01 ** 2) * price, 4),
            'interpretation': 'Higher convexity means bond benefits more from yield changes'
        }

    def calculate_effective_convexity(
        self,
        price: float,
        price_up: float,
        price_down: float,
        delta_yield: float = 0.01,
    ) -> Dict[str, Any]:
        """
        Calculate Effective Convexity for bonds with embedded options.

        EffConv = (P_down + P_up - 2*P_0) / (P_0 * delta_y^2)

        Args:
            price: Current bond price
            price_up: Price when yield increases
            price_down: Price when yield decreases
            delta_yield: Yield change magnitude

        Returns:
            Dictionary with effective convexity
        """
        if price <= 0 or delta_yield <= 0:
            return {'error': 'Invalid inputs'}

        effective_convexity = (price_down + price_up - 2 * price) / (price * (delta_yield ** 2))

        return {
            'effective_convexity': round(effective_convexity, 4),
            'delta_yield': delta_yield,
            'price_base': round(price, 4),
            'price_up': round(price_up, 4),
            'price_down': round(price_down, 4),
            'convexity_adjustment': round(0.5 * effective_convexity * (delta_yield ** 2) * price, 4)
        }

    def price_change_with_convexity(
        self,
        modified_duration: float,
        convexity: float,
        price: float,
        yield_change: float,
    ) -> Dict[str, Any]:
        """
        Estimate price change using duration and convexity.

        %dP = -ModDur * dy + 0.5 * Convexity * dy^2

        Args:
            modified_duration: Modified duration
            convexity: Convexity measure
            price: Current price
            yield_change: Change in yield (decimal)

        Returns:
            Dictionary with price change estimates
        """
        # Duration effect (first-order)
        duration_effect = -modified_duration * yield_change

        # Convexity adjustment (second-order)
        convexity_effect = 0.5 * convexity * (yield_change ** 2)

        # Total percentage change
        total_pct_change = duration_effect + convexity_effect

        # Dollar changes
        duration_dollar = duration_effect * price
        convexity_dollar = convexity_effect * price
        total_dollar = total_pct_change * price

        new_price = price * (1 + total_pct_change)

        return {
            'yield_change': yield_change,
            'yield_change_bps': round(yield_change * 10000, 0),
            'duration_effect_pct': round(duration_effect * 100, 4),
            'convexity_effect_pct': round(convexity_effect * 100, 4),
            'total_change_pct': round(total_pct_change * 100, 4),
            'duration_effect_dollar': round(duration_dollar, 4),
            'convexity_effect_dollar': round(convexity_dollar, 4),
            'total_change_dollar': round(total_dollar, 4),
            'original_price': round(price, 4),
            'estimated_new_price': round(new_price, 4),
            'convexity_benefit': 'Positive convexity benefits investor in both rising and falling rate environments'
        }


def calculate_portfolio_duration(
    bonds: List[Dict[str, Any]],
) -> Dict[str, Any]:
    """
    Calculate portfolio duration as weighted average.

    Args:
        bonds: List of dicts with 'duration', 'market_value'

    Returns:
        Portfolio duration metrics
    """
    total_mv = sum(b.get('market_value', 0) for b in bonds)

    if total_mv == 0:
        return {'error': 'Total market value is zero'}

    portfolio_duration = sum(
        b.get('duration', 0) * b.get('market_value', 0) / total_mv
        for b in bonds
    )

    portfolio_convexity = sum(
        b.get('convexity', 0) * b.get('market_value', 0) / total_mv
        for b in bonds
    )

    portfolio_dv01 = sum(b.get('dv01', 0) for b in bonds)

    return {
        'portfolio_duration': round(portfolio_duration, 4),
        'portfolio_convexity': round(portfolio_convexity, 4),
        'portfolio_dv01': round(portfolio_dv01, 4),
        'total_market_value': round(total_mv, 2),
        'num_bonds': len(bonds),
        'bond_contributions': [
            {
                'weight': round(b.get('market_value', 0) / total_mv, 4),
                'duration_contribution': round(b.get('duration', 0) * b.get('market_value', 0) / total_mv, 4)
            }
            for b in bonds
        ]
    }


def run_duration_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for duration/convexity analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'modified_duration')

    try:
        if analysis_type == 'macaulay_duration':
            calc = DurationCalculator()
            return calc.calculate_macaulay_duration(
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_maturity=params.get('years_to_maturity', 10),
                ytm=params.get('ytm', 0.05),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'modified_duration':
            calc = DurationCalculator()
            return calc.calculate_modified_duration(
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_maturity=params.get('years_to_maturity', 10),
                ytm=params.get('ytm', 0.05),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'effective_duration':
            calc = DurationCalculator()
            return calc.calculate_effective_duration(
                price=params.get('price', 1000),
                price_up=params.get('price_up', 990),
                price_down=params.get('price_down', 1010),
                delta_yield=params.get('delta_yield', 0.01)
            )

        elif analysis_type == 'convexity':
            calc = ConvexityCalculator()
            return calc.calculate_convexity(
                face_value=params.get('face_value', 1000),
                coupon_rate=params.get('coupon_rate', 0.05),
                years_to_maturity=params.get('years_to_maturity', 10),
                ytm=params.get('ytm', 0.05),
                frequency=params.get('frequency', 2)
            )

        elif analysis_type == 'effective_convexity':
            calc = ConvexityCalculator()
            return calc.calculate_effective_convexity(
                price=params.get('price', 1000),
                price_up=params.get('price_up', 990),
                price_down=params.get('price_down', 1010),
                delta_yield=params.get('delta_yield', 0.01)
            )

        elif analysis_type == 'price_change':
            calc = ConvexityCalculator()
            return calc.price_change_with_convexity(
                modified_duration=params.get('modified_duration', 7.5),
                convexity=params.get('convexity', 60),
                price=params.get('price', 1000),
                yield_change=params.get('yield_change', 0.01)
            )

        elif analysis_type == 'portfolio_duration':
            return calculate_portfolio_duration(
                bonds=params.get('bonds', [])
            )

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Duration analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_duration_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Duration & Convexity Demo:")

        dur_calc = DurationCalculator()
        conv_calc = ConvexityCalculator()

        # Modified duration
        result = dur_calc.calculate_modified_duration(
            coupon_rate=0.05, years_to_maturity=10, ytm=0.06
        )
        print(f"\nModified Duration: {result['modified_duration']} years")
        print(f"DV01: ${result['dv01']}")

        # Convexity
        result = conv_calc.calculate_convexity(
            coupon_rate=0.05, years_to_maturity=10, ytm=0.06
        )
        print(f"Convexity: {result['convexity']}")
