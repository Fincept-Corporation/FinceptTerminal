"""
Floating Rate and Money Market Instruments Module
=================================================

Floating rate notes (FRNs) and money market instrument analysis
implementing CFA Institute curriculum.

===== CFA CURRICULUM COVERAGE =====
- Yield and yield spread measures for floating-rate instruments
- Money market instrument yields
- FRN valuation and spread measures
- Reference rate transitions (LIBOR to SOFR)
- Caps, floors, and collars

PARAMETERS:
  - reference_rate: SOFR, EURIBOR, etc.
  - quoted_margin: Spread over reference rate
  - discount_margin: Market-implied spread
  - face_value: Par value
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple
from enum import Enum
from datetime import date, timedelta
import numpy as np
from scipy import optimize
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class ReferenceRate(Enum):
    """Reference rates for floating rate instruments"""
    SOFR = "sofr"  # Secured Overnight Financing Rate
    EFFR = "effr"  # Effective Federal Funds Rate
    EURIBOR = "euribor"  # Euro Interbank Offered Rate
    SONIA = "sonia"  # Sterling Overnight Index Average
    TONAR = "tonar"  # Tokyo Overnight Average Rate
    ESTR = "estr"  # Euro Short-Term Rate
    PRIME = "prime"  # US Prime Rate
    TREASURY = "treasury"  # Treasury rate


class MoneyMarketInstrument(Enum):
    """Money market instrument types"""
    TREASURY_BILL = "t_bill"
    COMMERCIAL_PAPER = "commercial_paper"
    CD = "certificate_of_deposit"
    REPO = "repurchase_agreement"
    BANKERS_ACCEPTANCE = "bankers_acceptance"
    EURODOLLAR = "eurodollar_deposit"
    FED_FUNDS = "federal_funds"


@dataclass
class FRNSpecification:
    """Floating rate note specification"""
    face_value: float = 1000.0
    quoted_margin: float = 0.01  # 100 bps
    reference_rate: ReferenceRate = ReferenceRate.SOFR
    reset_frequency: int = 4  # Quarterly
    maturity_years: float = 5.0
    cap_rate: Optional[float] = None
    floor_rate: Optional[float] = None
    current_reference_rate: float = 0.05


class FloatingRateAnalyzer:
    """
    Floating rate note valuation and analysis.

    Implements FRN pricing, discount margin, and spread analysis.
    """

    def calculate_frn_price(
        self,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        discount_margin: float = 0.01,
        reference_rate: float = 0.05,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
    ) -> Dict[str, Any]:
        """
        Calculate FRN price given discount margin.

        Price = Sum of PV of expected cash flows discounted at reference + DM

        Args:
            face_value: Par value
            quoted_margin: Contractual spread over reference (QM)
            discount_margin: Market-required spread (DM)
            reference_rate: Current reference rate
            periods_remaining: Number of coupon periods remaining
            reset_frequency: Resets per year

        Returns:
            Dictionary with FRN price and analysis
        """
        # Periodic rates
        periodic_coupon = (reference_rate + quoted_margin) / reset_frequency
        periodic_discount = (reference_rate + discount_margin) / reset_frequency

        # Cash flows: coupon each period, principal at maturity
        coupon = face_value * periodic_coupon
        cash_flows = [coupon] * periods_remaining
        cash_flows[-1] += face_value  # Add principal to last period

        # Present value of cash flows
        pv = 0
        for t, cf in enumerate(cash_flows, 1):
            pv += cf / ((1 + periodic_discount) ** t)

        # Premium/discount analysis
        if discount_margin < quoted_margin:
            price_status = "Premium"
            explanation = "DM < QM means market requires less spread than contractual"
        elif discount_margin > quoted_margin:
            price_status = "Discount"
            explanation = "DM > QM means market requires more spread than contractual"
        else:
            price_status = "Par"
            explanation = "DM = QM means FRN trades at par"

        return {
            'price': round(pv, 4),
            'price_percent': round((pv / face_value) * 100, 4),
            'face_value': face_value,
            'quoted_margin': quoted_margin,
            'quoted_margin_bps': round(quoted_margin * 10000, 1),
            'discount_margin': discount_margin,
            'discount_margin_bps': round(discount_margin * 10000, 1),
            'reference_rate': reference_rate,
            'coupon_rate': round(reference_rate + quoted_margin, 6),
            'price_status': price_status,
            'explanation': explanation,
            'periods_remaining': periods_remaining,
            'years_remaining': periods_remaining / reset_frequency
        }

    def calculate_discount_margin(
        self,
        price: float,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        reference_rate: float = 0.05,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
    ) -> Dict[str, Any]:
        """
        Calculate discount margin from FRN price.

        Solve for DM such that PV of cash flows equals price.

        Args:
            price: Current market price
            face_value: Par value
            quoted_margin: Contractual spread
            reference_rate: Current reference rate
            periods_remaining: Periods until maturity
            reset_frequency: Resets per year

        Returns:
            Dictionary with discount margin
        """
        periodic_coupon = (reference_rate + quoted_margin) / reset_frequency
        coupon = face_value * periodic_coupon

        def price_diff(dm):
            periodic_discount = (reference_rate + dm) / reset_frequency
            pv = 0
            for t in range(1, periods_remaining + 1):
                cf = coupon if t < periods_remaining else coupon + face_value
                pv += cf / ((1 + periodic_discount) ** t)
            return pv - price

        try:
            dm = optimize.brentq(price_diff, -0.10, 0.50, xtol=1e-10)
        except:
            try:
                dm = optimize.newton(price_diff, quoted_margin, tol=1e-10)
            except:
                return {'error': 'Could not calculate discount margin'}

        # Calculate spread measures
        spread_to_qm = dm - quoted_margin

        return {
            'discount_margin': round(dm, 6),
            'discount_margin_bps': round(dm * 10000, 2),
            'quoted_margin': quoted_margin,
            'quoted_margin_bps': round(quoted_margin * 10000, 1),
            'spread_to_quoted_margin': round(spread_to_qm, 6),
            'spread_to_qm_bps': round(spread_to_qm * 10000, 2),
            'price': price,
            'price_percent': round((price / face_value) * 100, 4),
            'interpretation': f"Market requires {round(dm * 10000, 1)}bps over reference vs {round(quoted_margin * 10000, 1)}bps contractual"
        }

    def calculate_zero_discount_margin(
        self,
        price: float,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        spot_rates: List[float] = None,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
    ) -> Dict[str, Any]:
        """
        Calculate zero-discount margin (Z-DM) using spot curve.

        Similar to Z-spread for fixed-rate bonds.

        Args:
            price: Current market price
            face_value: Par value
            quoted_margin: Contractual spread
            spot_rates: Spot rate curve (if None, assumes flat)
            periods_remaining: Periods until maturity
            reset_frequency: Resets per year

        Returns:
            Dictionary with Z-DM
        """
        if spot_rates is None:
            # Assume flat curve at 5%
            spot_rates = [0.05] * periods_remaining

        # Forward rates from spot rates (simplified - assumes spot for each period)
        forward_rates = spot_rates[:periods_remaining]

        def price_with_zdm(zdm):
            pv = 0
            for t in range(1, periods_remaining + 1):
                # Expected coupon based on forward rate
                forward = forward_rates[t - 1] if t - 1 < len(forward_rates) else forward_rates[-1]
                coupon_rate = (forward + quoted_margin) / reset_frequency
                cf = face_value * coupon_rate
                if t == periods_remaining:
                    cf += face_value

                # Discount at spot + Z-DM
                spot = spot_rates[t - 1] if t - 1 < len(spot_rates) else spot_rates[-1]
                discount = (spot + zdm) / reset_frequency
                pv += cf / ((1 + discount) ** t)
            return pv

        def objective(zdm):
            return (price_with_zdm(zdm[0]) - price) ** 2

        try:
            result = optimize.minimize(objective, [0.01], method='Nelder-Mead')
            zdm = result.x[0]
        except:
            return {'error': 'Could not calculate Z-DM'}

        return {
            'zero_discount_margin': round(zdm, 6),
            'z_dm_bps': round(zdm * 10000, 2),
            'price': price,
            'interpretation': 'Z-DM uses full spot curve for discounting'
        }

    def frn_with_cap_floor(
        self,
        face_value: float = 1000.0,
        quoted_margin: float = 0.01,
        reference_rate: float = 0.05,
        cap_rate: float = 0.08,
        floor_rate: float = 0.02,
        periods_remaining: int = 20,
        reset_frequency: int = 4,
        rate_volatility: float = 0.20,
    ) -> Dict[str, Any]:
        """
        Analyze FRN with cap and/or floor.

        Args:
            face_value: Par value
            quoted_margin: Contractual spread
            reference_rate: Current reference rate
            cap_rate: Maximum coupon rate
            floor_rate: Minimum coupon rate
            periods_remaining: Periods remaining
            reset_frequency: Resets per year
            rate_volatility: Interest rate volatility

        Returns:
            Dictionary with capped/floored FRN analysis
        """
        current_coupon_rate = reference_rate + quoted_margin

        # Effective coupon with cap/floor
        effective_rate = max(floor_rate, min(cap_rate, current_coupon_rate))

        # Impact analysis
        cap_binding = current_coupon_rate > cap_rate
        floor_binding = current_coupon_rate < floor_rate

        # Simplified option value estimation (Black model approximation)
        # Cap value: sum of caplet values
        # Floor value: sum of floorlet values
        time_to_maturity = periods_remaining / reset_frequency

        # Rough approximation of cap/floor values
        if cap_rate:
            cap_intrinsic = max(0, current_coupon_rate - cap_rate) * face_value * time_to_maturity
            cap_time_value = rate_volatility * face_value * np.sqrt(time_to_maturity) * 0.4
            cap_value = cap_intrinsic + cap_time_value
        else:
            cap_value = 0

        if floor_rate:
            floor_intrinsic = max(0, floor_rate - current_coupon_rate) * face_value * time_to_maturity
            floor_time_value = rate_volatility * face_value * np.sqrt(time_to_maturity) * 0.4
            floor_value = floor_intrinsic + floor_time_value
        else:
            floor_value = 0

        return {
            'current_coupon_rate': round(current_coupon_rate, 6),
            'effective_coupon_rate': round(effective_rate, 6),
            'cap_rate': cap_rate,
            'floor_rate': floor_rate,
            'cap_binding': cap_binding,
            'floor_binding': floor_binding,
            'rate_status': 'At cap' if cap_binding else 'At floor' if floor_binding else 'Unconstrained',
            'embedded_options': {
                'cap_value_estimate': round(cap_value, 2),
                'floor_value_estimate': round(floor_value, 2),
                'collar_net_value': round(floor_value - cap_value, 2)
            },
            'investor_impact': {
                'cap': 'Limits upside in rising rate environment',
                'floor': 'Provides downside protection in falling rates',
                'collar': 'Combined cap and floor'
            }
        }

    def describe_reference_rates(
        self,
        rate_type: str = None,
    ) -> Dict[str, Any]:
        """
        Describe reference rates and LIBOR transition.

        Args:
            rate_type: Specific rate to describe

        Returns:
            Dictionary with reference rate information
        """
        rates = {
            'sofr': {
                'name': 'Secured Overnight Financing Rate',
                'administrator': 'Federal Reserve Bank of New York',
                'description': 'Overnight Treasury repo rate',
                'characteristics': {
                    'secured': True,
                    'overnight': True,
                    'risk_free': 'Near risk-free (Treasury collateral)',
                    'volume': 'Very high daily volume ($1+ trillion)'
                },
                'variants': {
                    'daily_sofr': 'Published each business day',
                    'sofr_averages': '30, 90, 180-day averages',
                    'sofr_index': 'Compounded daily index',
                    'term_sofr': 'Forward-looking term rates (CME)'
                },
                'use_cases': 'FRNs, loans, derivatives',
                'transition': 'Primary USD LIBOR replacement'
            },
            'euribor': {
                'name': 'Euro Interbank Offered Rate',
                'administrator': 'European Money Markets Institute',
                'description': 'Euro unsecured interbank lending rate',
                'tenors': ['1 week', '1 month', '3 month', '6 month', '12 month'],
                'characteristics': {
                    'unsecured': True,
                    'term_rate': True,
                    'panel_based': 'Submissions from panel banks'
                },
                'status': 'Continues (reformed methodology)'
            },
            'sonia': {
                'name': 'Sterling Overnight Index Average',
                'administrator': 'Bank of England',
                'description': 'GBP unsecured overnight rate',
                'characteristics': {
                    'overnight': True,
                    'unsecured': True,
                    'transaction_based': True
                },
                'transition': 'GBP LIBOR replacement'
            },
            'libor_transition': {
                'background': 'LIBOR ceased publication for most currencies in 2023',
                'reasons': [
                    'Declining underlying transaction volume',
                    'Manipulation scandals',
                    'Panel bank concerns'
                ],
                'replacement_rates': {
                    'usd': 'SOFR',
                    'gbp': 'SONIA',
                    'eur': 'ESTR (derivatives), EURIBOR (loans)',
                    'jpy': 'TONAR',
                    'chf': 'SARON'
                },
                'transition_challenges': [
                    'Term rate vs overnight rate',
                    'Secured vs unsecured basis',
                    'Spread adjustment for legacy contracts',
                    'Fallback language in contracts'
                ]
            }
        }

        if rate_type:
            rate_lower = rate_type.lower()
            if rate_lower in rates:
                return {'rate': rate_type, 'details': rates[rate_lower]}
            else:
                return {'error': f'Unknown rate: {rate_type}', 'available': list(rates.keys())}

        return {'reference_rates': rates}


class MoneyMarketAnalyzer:
    """
    Money market instrument analysis.

    Implements yield calculations for T-bills, CP, CDs, and repos.
    """

    def calculate_discount_yield(
        self,
        face_value: float,
        price: float,
        days_to_maturity: int,
    ) -> Dict[str, Any]:
        """
        Calculate bank discount yield (T-bills, CP).

        Discount Yield = (FV - P) / FV × (360 / Days)

        Args:
            face_value: Par value
            price: Purchase price
            days_to_maturity: Days until maturity

        Returns:
            Dictionary with discount yield
        """
        discount = face_value - price
        discount_yield = (discount / face_value) * (360 / days_to_maturity)

        return {
            'discount_yield': round(discount_yield, 6),
            'discount_yield_pct': round(discount_yield * 100, 4),
            'discount_amount': round(discount, 2),
            'face_value': face_value,
            'price': price,
            'days_to_maturity': days_to_maturity,
            'day_count': '360',
            'note': 'Bank discount yield understates true yield (based on face value, not price)'
        }

    def calculate_money_market_yield(
        self,
        face_value: float,
        price: float,
        days_to_maturity: int,
    ) -> Dict[str, Any]:
        """
        Calculate money market yield (CD equivalent yield).

        MM Yield = (FV - P) / P × (360 / Days)

        Args:
            face_value: Par value
            price: Purchase price
            days_to_maturity: Days until maturity

        Returns:
            Dictionary with money market yield
        """
        discount = face_value - price
        mm_yield = (discount / price) * (360 / days_to_maturity)

        return {
            'money_market_yield': round(mm_yield, 6),
            'mm_yield_pct': round(mm_yield * 100, 4),
            'face_value': face_value,
            'price': price,
            'days_to_maturity': days_to_maturity,
            'note': 'Also called CD equivalent yield; based on price (more accurate than discount yield)'
        }

    def calculate_bond_equivalent_yield(
        self,
        face_value: float,
        price: float,
        days_to_maturity: int,
    ) -> Dict[str, Any]:
        """
        Calculate bond equivalent yield (365-day basis).

        BEY = (FV - P) / P × (365 / Days)

        Args:
            face_value: Par value
            price: Purchase price
            days_to_maturity: Days until maturity

        Returns:
            Dictionary with bond equivalent yield
        """
        discount = face_value - price
        bey = (discount / price) * (365 / days_to_maturity)

        return {
            'bond_equivalent_yield': round(bey, 6),
            'bey_pct': round(bey * 100, 4),
            'face_value': face_value,
            'price': price,
            'days_to_maturity': days_to_maturity,
            'day_count': '365',
            'note': 'Allows comparison with bond yields (365-day basis)'
        }

    def calculate_effective_annual_yield(
        self,
        face_value: float,
        price: float,
        days_to_maturity: int,
    ) -> Dict[str, Any]:
        """
        Calculate effective annual yield with compounding.

        EAY = (FV/P)^(365/Days) - 1

        Args:
            face_value: Par value
            price: Purchase price
            days_to_maturity: Days until maturity

        Returns:
            Dictionary with effective annual yield
        """
        holding_period_return = (face_value - price) / price
        eay = (1 + holding_period_return) ** (365 / days_to_maturity) - 1

        return {
            'effective_annual_yield': round(eay, 6),
            'eay_pct': round(eay * 100, 4),
            'holding_period_return': round(holding_period_return, 6),
            'hpr_pct': round(holding_period_return * 100, 4),
            'days_to_maturity': days_to_maturity,
            'note': 'Accounts for compounding; best for comparing across maturities'
        }

    def compare_all_yields(
        self,
        face_value: float = 1000.0,
        price: float = 990.0,
        days_to_maturity: int = 90,
    ) -> Dict[str, Any]:
        """
        Calculate and compare all money market yield measures.

        Args:
            face_value: Par value
            price: Purchase price
            days_to_maturity: Days until maturity

        Returns:
            Dictionary with all yield measures
        """
        # All yield calculations
        discount_yield = (face_value - price) / face_value * (360 / days_to_maturity)
        mm_yield = (face_value - price) / price * (360 / days_to_maturity)
        bey = (face_value - price) / price * (365 / days_to_maturity)
        hpr = (face_value - price) / price
        eay = (1 + hpr) ** (365 / days_to_maturity) - 1

        return {
            'inputs': {
                'face_value': face_value,
                'price': price,
                'days_to_maturity': days_to_maturity
            },
            'yield_measures': {
                'bank_discount_yield': {
                    'value': round(discount_yield * 100, 4),
                    'formula': '(FV-P)/FV × 360/t',
                    'basis': 'Face value, 360 days',
                    'use': 'T-bill quotes'
                },
                'money_market_yield': {
                    'value': round(mm_yield * 100, 4),
                    'formula': '(FV-P)/P × 360/t',
                    'basis': 'Price, 360 days',
                    'use': 'CD quotes'
                },
                'bond_equivalent_yield': {
                    'value': round(bey * 100, 4),
                    'formula': '(FV-P)/P × 365/t',
                    'basis': 'Price, 365 days',
                    'use': 'Compare to bonds'
                },
                'effective_annual_yield': {
                    'value': round(eay * 100, 4),
                    'formula': '(1 + HPR)^(365/t) - 1',
                    'basis': 'Compounded, 365 days',
                    'use': 'True annualized return'
                }
            },
            'comparison': {
                'lowest_to_highest': 'Bank Discount < MM Yield < BEY < EAY (typically)',
                'why': 'Discount yield uses higher denominator (FV); EAY compounds'
            }
        }

    def price_from_discount_yield(
        self,
        face_value: float,
        discount_yield: float,
        days_to_maturity: int,
    ) -> Dict[str, Any]:
        """
        Calculate price from bank discount yield.

        Price = FV × (1 - Discount Yield × Days/360)

        Args:
            face_value: Par value
            discount_yield: Bank discount yield (decimal)
            days_to_maturity: Days until maturity

        Returns:
            Dictionary with price
        """
        price = face_value * (1 - discount_yield * days_to_maturity / 360)
        discount = face_value - price

        return {
            'price': round(price, 4),
            'discount': round(discount, 4),
            'face_value': face_value,
            'discount_yield': discount_yield,
            'days_to_maturity': days_to_maturity
        }


def run_floating_rate_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for floating rate and money market analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'frn_price')

    try:
        if analysis_type == 'frn_price':
            analyzer = FloatingRateAnalyzer()
            return analyzer.calculate_frn_price(
                face_value=params.get('face_value', 1000),
                quoted_margin=params.get('quoted_margin', 0.01),
                discount_margin=params.get('discount_margin', 0.01),
                reference_rate=params.get('reference_rate', 0.05),
                periods_remaining=params.get('periods_remaining', 20),
                reset_frequency=params.get('reset_frequency', 4)
            )

        elif analysis_type == 'discount_margin':
            analyzer = FloatingRateAnalyzer()
            return analyzer.calculate_discount_margin(
                price=params.get('price', 1000),
                face_value=params.get('face_value', 1000),
                quoted_margin=params.get('quoted_margin', 0.01),
                reference_rate=params.get('reference_rate', 0.05),
                periods_remaining=params.get('periods_remaining', 20),
                reset_frequency=params.get('reset_frequency', 4)
            )

        elif analysis_type == 'frn_cap_floor':
            analyzer = FloatingRateAnalyzer()
            return analyzer.frn_with_cap_floor(
                face_value=params.get('face_value', 1000),
                quoted_margin=params.get('quoted_margin', 0.01),
                reference_rate=params.get('reference_rate', 0.05),
                cap_rate=params.get('cap_rate', 0.08),
                floor_rate=params.get('floor_rate', 0.02),
                periods_remaining=params.get('periods_remaining', 20),
                reset_frequency=params.get('reset_frequency', 4),
                rate_volatility=params.get('rate_volatility', 0.20)
            )

        elif analysis_type == 'reference_rates':
            analyzer = FloatingRateAnalyzer()
            return analyzer.describe_reference_rates(
                rate_type=params.get('rate_type')
            )

        elif analysis_type == 'discount_yield':
            analyzer = MoneyMarketAnalyzer()
            return analyzer.calculate_discount_yield(
                face_value=params.get('face_value', 1000),
                price=params.get('price', 990),
                days_to_maturity=params.get('days_to_maturity', 90)
            )

        elif analysis_type == 'money_market_yield':
            analyzer = MoneyMarketAnalyzer()
            return analyzer.calculate_money_market_yield(
                face_value=params.get('face_value', 1000),
                price=params.get('price', 990),
                days_to_maturity=params.get('days_to_maturity', 90)
            )

        elif analysis_type == 'bond_equivalent_yield':
            analyzer = MoneyMarketAnalyzer()
            return analyzer.calculate_bond_equivalent_yield(
                face_value=params.get('face_value', 1000),
                price=params.get('price', 990),
                days_to_maturity=params.get('days_to_maturity', 90)
            )

        elif analysis_type == 'compare_yields':
            analyzer = MoneyMarketAnalyzer()
            return analyzer.compare_all_yields(
                face_value=params.get('face_value', 1000),
                price=params.get('price', 990),
                days_to_maturity=params.get('days_to_maturity', 90)
            )

        elif analysis_type == 'price_from_discount':
            analyzer = MoneyMarketAnalyzer()
            return analyzer.price_from_discount_yield(
                face_value=params.get('face_value', 1000),
                discount_yield=params.get('discount_yield', 0.04),
                days_to_maturity=params.get('days_to_maturity', 90)
            )

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Floating rate analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_floating_rate_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Floating Rate & Money Market Demo:")

        frn = FloatingRateAnalyzer()
        mm = MoneyMarketAnalyzer()

        # FRN discount margin
        result = frn.calculate_discount_margin(
            price=1020, quoted_margin=0.015, reference_rate=0.05
        )
        print(f"\nFRN Discount Margin: {result['discount_margin_bps']}bps")

        # Money market yields comparison
        result = mm.compare_all_yields(face_value=10000, price=9850, days_to_maturity=180)
        print("\nMoney Market Yields:")
        for name, data in result['yield_measures'].items():
            print(f"  {name}: {data['value']}%")
