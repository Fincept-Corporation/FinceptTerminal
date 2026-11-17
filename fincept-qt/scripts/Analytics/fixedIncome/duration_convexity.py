"""
Duration and Convexity Analytics Module
=======================================

Interest rate risk measures (CFA-curriculum methods).

Conventions
-----------
* macaulay_duration: PV-weighted average time to the cash flows, in YEARS.
* modified_duration = macaulay / (1 + y/f) (y/f -> y for frequency 0): -dP/dy / P, in years.
* convexity: (1/P) d2P/dy2 in years^2, consistent with
  %dP ~= -modified * dy + 0.5 * convexity * dy^2.
* All three are computed on the dirty (full) price; `price` in the output is the clean price.
* Yield: nominal annual compounded `frequency` times a year (frequency 0: zero-coupon, annual).
* Fractional periods to maturity use a fractional first period (no truncation); settlement_date +
  maturity_date (YYYY-MM-DD) + day_count can be given instead of years_to_maturity.
* dollar_duration = modified * dirty price * 0.01 (price change per 100 bp); dv01 = per 1 bp.

PARAMETERS:
  face_value (1000), coupon_rate (0.05), years_to_maturity (10), ytm (0.05), frequency (2),
  delta_yield (for effective measures)
"""

import logging
import os
import sys
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict, List, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num, r  # noqa: E402

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


def _bond(face_value, coupon_rate, years_to_maturity, ytm, frequency, timing):
    fc.check_frequency(frequency)
    if ytm <= -(frequency or 1):
        raise FIError(f"ytm must be > {-(frequency or 1)} for frequency {frequency}")
    if timing is None:
        timing = fc.timing_from_years(years_to_maturity, frequency, coupon_rate)
    flows = fc.bond_flows(face_value, coupon_rate, timing)
    m = fc.risk_measures(ytm, flows, frequency)
    accrued = face_value * timing.accrued_per_face
    return timing, flows, m, accrued


class DurationCalculator:
    """Duration measures: sensitivity of bond price to interest rate changes."""

    def calculate_macaulay_duration(
        self,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        ytm: float = 0.05,
        frequency: int = 2,
        timing: Optional[fc.BondTiming] = None,
    ) -> Dict[str, Any]:
        """
        Macaulay duration = sum(t * PV(CF_t)) / dirty price, t in years from settlement.
        A zero-coupon bond's Macaulay duration equals its time to maturity.
        """
        timing, flows, m, accrued = _bond(face_value, coupon_rate, years_to_maturity, ytm, frequency, timing)
        dirty = m['dirty_price']
        if frequency == 0:
            T = timing.years_from_settlement
            return {
                'macaulay_duration': round(m['macaulay'], 6),
                'price': round(dirty, 4),
                'dirty_price': round(dirty, 4),
                'weighted_cash_flows': [{'period': round(T, 6), 'weight': 1.0}],
                'conventions': {'compounding': 'annual (zero-coupon)', 'units': 'years'},
            }
        f = frequency
        b = 1 + ytm / f
        details = []
        for k, (e, cf) in enumerate(flows, 1):
            pv = cf * b ** (-e)
            details.append({
                'period': k,
                'time_years': round(e / f, 6),
                'cash_flow': round(cf, 2),
                'pv': round(pv, 4),
                'weighted_pv': round(e / f * pv, 4),
            })
        return {
            'macaulay_duration': round(m['macaulay'], 6),
            'macaulay_duration_periods': round(m['macaulay'] * f, 6),
            'price': round(dirty - accrued, 4),
            'dirty_price': round(dirty, 4),
            'accrued_interest': round(accrued, 4),
            'total_pv': round(dirty, 4),
            'weighted_sum': round(m['weighted_sum'], 4),
            'cash_flow_details': details,
            'conventions': {'units': 'years', 'weights': 'PV at the yield, on the dirty price',
                            'yield_compounding': f'nominal annual, compounded {f}x per year',
                            **timing.info},
        }

    def calculate_modified_duration(
        self,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        ytm: float = 0.05,
        frequency: int = 2,
        timing: Optional[fc.BondTiming] = None,
    ) -> Dict[str, Any]:
        """
        Modified duration = Macaulay / (1 + y/f)  (= -dP/dy / P for the periodic-compounding yield).
        """
        timing, flows, m, accrued = _bond(face_value, coupon_rate, years_to_maturity, ytm, frequency, timing)
        dirty = m['dirty_price']
        mod = m['modified']
        dollar_duration = mod * dirty / 100
        dv01 = mod * dirty * 0.0001
        chg_1pct = mod * 0.01 * dirty
        chg_50bp = mod * 0.005 * dirty
        return {
            'modified_duration': round(mod, 6),
            'macaulay_duration': round(m['macaulay'], 6),
            'dollar_duration': round(dollar_duration, 4),
            'dv01': round(dv01, 4),
            'price': round(dirty - accrued, 4),
            'dirty_price': round(dirty, 4),
            'accrued_interest': round(accrued, 4),
            'price_change_1pct_yield': round(chg_1pct, 4),
            'price_change_50bp_yield': round(chg_50bp, 4),
            'interpretation': f"A 1% increase in yield decreases price by approximately {round(chg_1pct, 2)}",
            'conventions': {
                'modified_duration': 'Macaulay / (1 + y/f); y/f -> y for frequency 0',
                'dollar_duration': 'modified x dirty price x 0.01 (per 100 bp)',
                'dv01': 'modified x dirty price x 0.0001 (per 1 bp)',
                'yield_compounding': ('annual (zero-coupon)' if frequency == 0
                                      else f'nominal annual, compounded {frequency}x per year'),
            },
        }

    def calculate_effective_duration(
        self,
        price: float,
        price_up: float,
        price_down: float,
        delta_yield: float = 0.01,
    ) -> Dict[str, Any]:
        """
        Effective duration (bonds with embedded options):
        EffDur = (P_down - P_up) / (2 * P_0 * delta_y)
        """
        if price <= 0 or price_up <= 0 or price_down <= 0:
            raise FIError('price, price_up and price_down must be > 0')
        if delta_yield <= 0:
            raise FIError('delta_yield must be > 0')
        effective_duration = (price_down - price_up) / (2 * price * delta_yield)
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
        Key rate durations: spot_rates[i] is the annually-compounded spot rate for the cash flow at
        t = (i+1)/frequency (the bond matures at the last one).

        Each key rate is shocked with a triangular ("tent") profile that is 1 at its tenor and falls
        linearly to 0 at the neighbouring key tenors (flat beyond the first and last key used), so the
        shocks add up to a parallel shift and the KRDs add up to the effective duration.
        Central differences: KRD_k = (P(-d) - P(+d)) / (2 P d).
        """
        fc.check_frequency(frequency, allow_zero=False)
        if not spot_rates:
            raise FIError('spot_rates must be a non-empty list (one annually-compounded rate per coupon period)')
        if delta_rate <= 0:
            raise FIError('delta_rate must be > 0')
        if key_rate_maturities is None:
            key_rate_maturities = [1, 2, 3, 5, 7, 10, 20, 30]
        keys = sorted(set(float(k) for k in key_rate_maturities))
        if not keys or keys[0] <= 0:
            raise FIError('key_rate_maturities must be positive')

        coupon = coupon_rate * face_value / frequency
        n = len(spot_rates)
        times = [(i + 1) / frequency for i in range(n)]
        cfs = [coupon + (face_value if i == n - 1 else 0.0) for i in range(n)]
        maturity = times[-1]
        # keys below maturity plus the first key at/after it (cash flows never extend past it)
        used = [k for k in keys if k < maturity - 1e-12]
        nxt = [k for k in keys if k >= maturity - 1e-12]
        if nxt:
            used.append(nxt[0])
        if not used:
            used = [keys[-1]]

        def tent(j: int, t: float) -> float:
            k = used[j]
            left = used[j - 1] if j > 0 else None
            right = used[j + 1] if j + 1 < len(used) else None
            if t <= k:
                if left is None:
                    return 1.0
                return 0.0 if t <= left else (t - left) / (k - left)
            if right is None:
                return 1.0
            return 0.0 if t >= right else (right - t) / (right - k)

        def price(shock) -> float:
            total = 0.0
            for i, (t, cf) in enumerate(zip(times, cfs)):
                s = spot_rates[i] + shock(t)
                if s <= -1:
                    raise FIError(f'spot_rates[{i}] must be > -1')
                total += cf / (1 + s) ** t
            return total

        base_price = price(lambda t: 0.0)
        krds = []
        for j, k in enumerate(used):
            p_up = price(lambda t, j=j: delta_rate * tent(j, t))
            p_dn = price(lambda t, j=j: -delta_rate * tent(j, t))
            krds.append((k, (p_dn - p_up) / (2 * base_price * delta_rate)))
        total_krd = sum(v for _, v in krds)
        p_up = price(lambda t: delta_rate)
        p_dn = price(lambda t: -delta_rate)
        parallel = (p_dn - p_up) / (2 * base_price * delta_rate)

        return {
            'key_rate_durations': [
                {'maturity': k, 'key_rate_duration': round(v, 6),
                 'contribution_pct': round(v / total_krd * 100, 2) if total_krd else None}
                for k, v in krds
            ],
            'total_krd': round(total_krd, 6),
            'parallel_shift_duration': round(parallel, 6),
            'base_price': round(base_price, 4),
            'key_rates_used': used,
            'note': 'Tent-shaped shocks to annually-compounded spot rates; KRDs sum to the parallel-shift duration'
        }


class ConvexityCalculator:
    """Convexity: second-order price sensitivity."""

    def calculate_convexity(
        self,
        face_value: float = 1000.0,
        coupon_rate: float = 0.05,
        years_to_maturity: float = 10.0,
        ytm: float = 0.05,
        frequency: int = 2,
        timing: Optional[fc.BondTiming] = None,
    ) -> Dict[str, Any]:
        """
        Convexity = sum(e(e+1) PV(CF)) / (P (1+y/f)^2 f^2), e = periods to the cash flow
        (zero-coupon, annual: T(T+1)/(1+y)^2). Units: years^2.
        """
        timing, flows, m, accrued = _bond(face_value, coupon_rate, years_to_maturity, ytm, frequency, timing)
        dirty = m['dirty_price']
        conv = m['convexity']
        if frequency == 0:
            return {
                'convexity': round(conv, 4),
                'price': round(dirty, 4),
                'dirty_price': round(dirty, 4),
                'note': 'Zero coupon bond convexity, annual compounding: T(T+1)/(1+y)^2',
            }
        return {
            'convexity': round(conv, 4),
            'convexity_years': round(conv, 4),
            'dollar_convexity': round(conv * dirty / 100, 4),
            'price': round(dirty - accrued, 4),
            'dirty_price': round(dirty, 4),
            'accrued_interest': round(accrued, 4),
            'convexity_adjustment_1pct': round(0.5 * conv * (0.01 ** 2) * dirty, 4),
            'interpretation': 'Higher convexity means bond benefits more from yield changes',
            'conventions': {'units': 'years^2 (annualised)', 'price_basis': 'dirty',
                            'yield_compounding': f'nominal annual, compounded {frequency}x per year',
                            'dollar_convexity': 'convexity x dirty price / 100'},
        }

    def calculate_effective_convexity(
        self,
        price: float,
        price_up: float,
        price_down: float,
        delta_yield: float = 0.01,
    ) -> Dict[str, Any]:
        """
        Effective convexity = (P_down + P_up - 2 P_0) / (P_0 * delta_y^2)
        """
        if price <= 0 or price_up <= 0 or price_down <= 0:
            raise FIError('price, price_up and price_down must be > 0')
        if delta_yield <= 0:
            raise FIError('delta_yield must be > 0')
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
        %dP = -ModDur * dy + 0.5 * Convexity * dy^2  (convexity in years^2, dy decimal)
        """
        if price <= 0:
            raise FIError('price must be > 0')
        duration_effect = -modified_duration * yield_change
        convexity_effect = 0.5 * convexity * (yield_change ** 2)
        total_pct_change = duration_effect + convexity_effect
        return {
            'yield_change': yield_change,
            'yield_change_bps': round(yield_change * 10000, 0),
            'duration_effect_pct': round(duration_effect * 100, 4),
            'convexity_effect_pct': round(convexity_effect * 100, 4),
            'total_change_pct': round(total_pct_change * 100, 4),
            'duration_effect_dollar': round(duration_effect * price, 4),
            'convexity_effect_dollar': round(convexity_effect * price, 4),
            'total_change_dollar': round(total_pct_change * price, 4),
            'original_price': round(price, 4),
            'estimated_new_price': round(price * (1 + total_pct_change), 4),
            'convexity_benefit': 'Positive convexity benefits investor in both rising and falling rate environments'
        }


def calculate_portfolio_duration(
    bonds: List[Dict[str, Any]],
) -> Dict[str, Any]:
    """
    Portfolio duration / convexity as market-value-weighted averages; DV01 summed.

    bonds: [{market_value, duration, convexity, dv01}]
    """
    if not isinstance(bonds, list) or not bonds:
        raise FIError('bonds must be a non-empty list of {market_value, duration[, convexity, dv01]}')
    rows = []
    for i, b in enumerate(bonds):
        if not isinstance(b, dict):
            raise FIError(f'bonds[{i}] must be an object')
        rows.append({'mv': num(b, 'market_value', lo=0), 'dur': num(b, 'duration', 0.0),
                     'conv': num(b, 'convexity', 0.0), 'dv01': num(b, 'dv01', 0.0)})
    total_mv = sum(x['mv'] for x in rows)
    if total_mv <= 0:
        raise FIError('total market value must be > 0')
    return {
        'portfolio_duration': round(sum(x['dur'] * x['mv'] for x in rows) / total_mv, 4),
        'portfolio_convexity': round(sum(x['conv'] * x['mv'] for x in rows) / total_mv, 4),
        'portfolio_dv01': round(sum(x['dv01'] for x in rows), 4),
        'total_market_value': round(total_mv, 2),
        'num_bonds': len(rows),
        'bond_contributions': [
            {'weight': round(x['mv'] / total_mv, 4),
             'duration_contribution': round(x['dur'] * x['mv'] / total_mv, 4)}
            for x in rows
        ]
    }


ANALYSIS_TYPES = ('macaulay_duration', 'modified_duration', 'effective_duration', 'convexity',
                  'effective_convexity', 'price_change', 'key_rate_duration', 'portfolio_duration')


def _bond_params(params):
    face = num(params, 'face_value', 1000, lo=0, lo_open=True)
    coupon = num(params, 'coupon_rate', 0.05, lo=0)
    freq = fc.check_frequency(num(params, 'frequency', 2, integer=True))
    timing = fc.resolve_timing(params, coupon, freq)
    return dict(face_value=face, coupon_rate=coupon, ytm=num(params, 'ytm', 0.05), frequency=freq,
                timing=timing)


def _effective_params(params):
    return dict(price=num(params, 'price', 1000), price_up=num(params, 'price_up', 990),
                price_down=num(params, 'price_down', 1010), delta_yield=num(params, 'delta_yield', 0.01))


def run_duration_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Main entry point for duration/convexity analysis (params['analysis_type'] in ANALYSIS_TYPES)."""
    analysis_type = params.get('analysis_type', 'modified_duration')

    try:
        if analysis_type == 'macaulay_duration':
            return DurationCalculator().calculate_macaulay_duration(**_bond_params(params))

        elif analysis_type == 'modified_duration':
            return DurationCalculator().calculate_modified_duration(**_bond_params(params))

        elif analysis_type == 'effective_duration':
            return DurationCalculator().calculate_effective_duration(**_effective_params(params))

        elif analysis_type == 'convexity':
            return ConvexityCalculator().calculate_convexity(**_bond_params(params))

        elif analysis_type == 'effective_convexity':
            return ConvexityCalculator().calculate_effective_convexity(**_effective_params(params))

        elif analysis_type == 'price_change':
            return ConvexityCalculator().price_change_with_convexity(
                modified_duration=num(params, 'modified_duration', 7.5),
                convexity=num(params, 'convexity', 60),
                price=num(params, 'price', 1000),
                yield_change=num(params, 'yield_change', 0.01))

        elif analysis_type == 'key_rate_duration':
            keys = params.get('key_rate_maturities')
            return DurationCalculator().calculate_key_rate_duration(
                spot_rates=fc.num_list(params, 'spot_rates', min_len=1),
                face_value=num(params, 'face_value', 1000, lo=0, lo_open=True),
                coupon_rate=num(params, 'coupon_rate', 0.05, lo=0),
                frequency=num(params, 'frequency', 2, integer=True),
                delta_rate=num(params, 'delta_rate', 0.0001, lo=0, lo_open=True),
                key_rate_maturities=fc.num_list(params, 'key_rate_maturities', min_len=1, lo=0, lo_open=True)
                if keys is not None else None)

        elif analysis_type == 'portfolio_duration':
            return calculate_portfolio_duration(bonds=params.get('bonds', []))

        else:
            return {'error': f'Unknown analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}

    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        logger.error(f"Duration analysis error: {e}")
        return {'error': str(e) or type(e).__name__}


if __name__ == "__main__":
    fc.import_fincept_json()
    fc.emit_usage_or_run(sys.argv, run_duration_analysis, ANALYSIS_TYPES, 'duration_convexity')
