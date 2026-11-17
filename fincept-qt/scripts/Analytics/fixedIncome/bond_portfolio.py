"""
Bond Portfolio Analytics Module
===============================

Portfolio-level fixed income analytics (CFA-curriculum methods): portfolio duration / convexity,
duration contribution, maturity-bucket exposure, immunization, contingent immunization, cash-flow
matching, tracking error.

Holdings contract: [{identifier, market_value, duration, convexity, ytm, maturity}] - durations and
convexities are supplied by the caller (use the duration commands to compute them). Durations are
treated as modified durations for DV01 and price-change estimates.

PARAMETERS:
  holdings, key_rates, liability_pv, liability_duration, bond1, bond2, current_duration,
  target_duration, threshold, assets, liabilities_pv, floor_rate, current_rate, duration (horizon)
"""

import logging
import math
import os
import sys
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict, List, Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num  # noqa: E402

logger = logging.getLogger(__name__)


class PortfolioStrategy(Enum):
    """Fixed income portfolio strategies"""
    IMMUNIZATION = "immunization"
    CASH_FLOW_MATCHING = "cash_flow_matching"
    INDEXING = "indexing"
    ACTIVE = "active"
    LIABILITY_DRIVEN = "liability_driven"


class RebalancingTrigger(Enum):
    """Triggers for portfolio rebalancing"""
    DURATION_DRIFT = "duration_drift"
    CALENDAR = "calendar"
    THRESHOLD = "threshold"
    CONTINGENT = "contingent"


@dataclass
class BondHolding:
    """Individual bond holding in portfolio (the holdings JSON uses the key 'ytm' for the yield)"""
    identifier: str
    market_value: float
    duration: float
    convexity: float
    yield_to_maturity: float
    coupon_rate: float
    maturity: float
    credit_rating: str = "BBB"


@dataclass
class Liability:
    """Future liability payment"""
    date: float  # Years from now
    amount: float
    present_value: float = 0


def _holdings(holdings: Any) -> List[Dict[str, Any]]:
    if not isinstance(holdings, list) or not holdings:
        raise FIError('holdings must be a non-empty list of {identifier, market_value, duration, convexity, ytm, maturity}')
    out = []
    for i, h in enumerate(holdings):
        if not isinstance(h, dict):
            raise FIError(f'holdings[{i}] must be an object')
        out.append({
            'identifier': str(h.get('identifier', f'#{i + 1}')),
            'market_value': num(h, 'market_value', lo=0),
            'duration': num(h, 'duration', 0.0),
            'convexity': num(h, 'convexity', 0.0),
            'ytm': num(h, 'ytm', 0.0, aliases=('yield_to_maturity',)),
            'maturity': num(h, 'maturity', 0.0, lo=0),
        })
    total = sum(h['market_value'] for h in out)
    if total <= 0:
        raise FIError('total market value of holdings must be > 0')
    return out


class BondPortfolioAnalyzer:
    """Bond portfolio analytics engine."""

    def calculate_portfolio_metrics(self, holdings: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Market-value-weighted duration, convexity, yield and maturity; DV01 and +/-100 bp estimates."""
        hs = _holdings(holdings)
        total_mv = sum(h['market_value'] for h in hs)
        w = lambda key: sum(h[key] * h['market_value'] for h in hs) / total_mv
        portfolio_duration = w('duration')
        portfolio_convexity = w('convexity')
        portfolio_yield = w('ytm')
        portfolio_maturity = w('maturity')
        convexity_adjustment = 0.5 * portfolio_convexity * (0.01 ** 2) * total_mv
        return {
            'total_market_value': round(total_mv, 2),
            'num_holdings': len(hs),
            'portfolio_duration': round(portfolio_duration, 4),
            'portfolio_convexity': round(portfolio_convexity, 4),
            'portfolio_yield': round(portfolio_yield, 6),
            'portfolio_yield_pct': round(portfolio_yield * 100, 4),
            'portfolio_maturity': round(portfolio_maturity, 4),
            'dollar_duration': round(portfolio_duration * total_mv / 100, 2),
            'dv01': round(portfolio_duration * total_mv * 0.0001, 2),
            'convexity_adjustment_100bp': round(convexity_adjustment, 2),
            'price_change_estimate_100bp_up': round(-portfolio_duration * 0.01 * total_mv + convexity_adjustment, 2),
            'price_change_estimate_100bp_down': round(portfolio_duration * 0.01 * total_mv + convexity_adjustment, 2),
            'conventions': {'weights': 'market value', 'dollar_duration': 'per 100 bp', 'dv01': 'per 1 bp',
                            'portfolio_yield': 'MV-weighted average of holding yields (an approximation of '
                                               'the portfolio IRR)'},
        }

    def calculate_contribution_to_duration(self, holdings: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Each holding's contribution = weight x duration; contribution_pct = share of portfolio duration."""
        hs = _holdings(holdings)
        total_mv = sum(h['market_value'] for h in hs)
        contributions = []
        for h in hs:
            weight = h['market_value'] / total_mv
            contributions.append({
                'identifier': h['identifier'],
                'market_value': round(h['market_value'], 2),
                'weight': round(weight, 4),
                'duration': round(h['duration'], 4),
                'duration_contribution': weight * h['duration'],
            })
        portfolio_duration = sum(c['duration_contribution'] for c in contributions)
        for c in contributions:
            c['contribution_pct'] = (round(c['duration_contribution'] / portfolio_duration * 100, 2)
                                     if portfolio_duration else None)
            c['duration_contribution'] = round(c['duration_contribution'], 4)
        contributions.sort(key=lambda x: x['duration_contribution'], reverse=True)
        return {
            'contributions': contributions,
            'portfolio_duration': round(portfolio_duration, 4),
            'top_contributors': contributions[:5],
        }

    def calculate_key_rate_exposure(self, holdings: List[Dict[str, Any]], key_rates: List[float] = None) -> Dict[str, Any]:
        """
        Maturity-bucket exposure: each holding is placed WHOLE in the bucket of the nearest key tenor
        (boundaries at the midpoints between tenors). This is bucketing, not a key-rate-duration
        calculation (use key_rate_duration for that).
        """
        hs = _holdings(holdings)
        if key_rates is None:
            key_rates = [1, 2, 3, 5, 7, 10, 20, 30]
        key_rates = sorted(set(key_rates))
        if not key_rates:
            raise FIError('key_rates must be a non-empty list of tenors')
        total_mv = sum(h['market_value'] for h in hs)
        buckets = {kr: {'mv': 0.0, 'duration_contrib': 0.0, 'count': 0} for kr in key_rates}
        for h in hs:
            for i, kr in enumerate(key_rates):
                if i == len(key_rates) - 1 or h['maturity'] <= (key_rates[i] + key_rates[i + 1]) / 2:
                    buckets[kr]['mv'] += h['market_value']
                    buckets[kr]['duration_contrib'] += h['duration'] * h['market_value'] / total_mv
                    buckets[kr]['count'] += 1
                    break
        exposures = [{
            'key_rate': kr,
            'market_value': round(buckets[kr]['mv'], 2),
            'weight': round(buckets[kr]['mv'] / total_mv, 4),
            'duration_contribution': round(buckets[kr]['duration_contrib'], 4),
            'num_holdings': buckets[kr]['count'],
        } for kr in key_rates]
        return {
            'key_rate_exposures': exposures,
            'total_market_value': round(total_mv, 2),
            'concentration_risk': self._assess_concentration(exposures),
            'method': 'maturity bucketing to the nearest key tenor (not key-rate duration)',
        }

    def _assess_concentration(self, exposures: List[Dict]) -> str:
        max_weight = max((e['weight'] for e in exposures), default=0)
        if max_weight > 0.50:
            return 'High concentration - over 50% in single bucket'
        if max_weight > 0.30:
            return 'Moderate concentration - over 30% in single bucket'
        return 'Well diversified across key rates'


class ImmunizationStrategy:
    """Classical and contingent immunization."""

    def calculate_immunization_requirements(self, liability_pv: float, liability_duration: float,
                                            current_yield: float) -> Dict[str, Any]:
        """Conditions for classical immunization (PV match, duration match, convexity >= liability)."""
        if liability_pv <= 0 or liability_duration <= 0:
            raise FIError('liability_pv and liability_duration must be > 0')
        return {
            'required_asset_pv': round(liability_pv, 2),
            'required_duration': round(liability_duration, 4),
            'target_yield': round(current_yield, 6),
            'conditions': [
                f"Asset PV must equal {liability_pv:,.2f}",
                f"Asset duration must equal {liability_duration:.2f} years",
                "Asset convexity must exceed liability convexity (minimises dispersion of cash flows)",
                "Portfolio must be rebalanced when duration drifts"
            ],
            'rebalancing_frequency': 'Quarterly or when duration drifts > 0.25 years'
        }

    def immunize_with_two_bonds(self, liability_duration: float, liability_pv: float,
                                bond1: Dict[str, float], bond2: Dict[str, float]) -> Dict[str, Any]:
        """Weights w1 + w2 = 1 with w1 D1 + w2 D2 = D_liability (market-value weights)."""
        if liability_pv <= 0:
            raise FIError('liability_pv must be > 0')
        if not isinstance(bond1, dict) or not isinstance(bond2, dict):
            raise FIError('bond1 and bond2 must be objects with duration (and optionally convexity)')
        d1 = num(bond1, 'duration', 3)
        d2 = num(bond2, 'duration', 10)
        if d1 == d2:
            raise FIError('Bonds must have different durations')
        w2 = (liability_duration - d1) / (d2 - d1)
        w1 = 1 - w2
        if w1 < 0 or w2 < 0:
            return {
                'error': 'Cannot immunize - liability duration outside bond duration range',
                'liability_duration': liability_duration,
                'bond1_duration': d1,
                'bond2_duration': d2
            }
        c1 = num(bond1, 'convexity', 20)
        c2 = num(bond2, 'convexity', 100)
        portfolio_duration = w1 * d1 + w2 * d2
        return {
            'bond1': {'weight': round(w1, 4), 'weight_pct': round(w1 * 100, 2),
                      'amount': round(liability_pv * w1, 2), 'duration': d1},
            'bond2': {'weight': round(w2, 4), 'weight_pct': round(w2 * 100, 2),
                      'amount': round(liability_pv * w2, 2), 'duration': d2},
            'portfolio_duration': round(portfolio_duration, 4),
            'portfolio_convexity': round(w1 * c1 + w2 * c2, 4),
            'liability_duration': liability_duration,
            'liability_pv': liability_pv,
            'is_immunized': abs(portfolio_duration - liability_duration) < 0.01
        }

    def check_rebalancing_need(self, current_duration: float, target_duration: float,
                               threshold: float = 0.25, time_elapsed: float = 0) -> Dict[str, Any]:
        """Duration drift against the target, optionally rolled down by time_elapsed (years)."""
        if threshold < 0 or time_elapsed < 0:
            raise FIError('threshold and time_elapsed must be >= 0')
        adjusted_target = target_duration - time_elapsed
        adjusted_drift = current_duration - adjusted_target
        needs_rebalancing = abs(adjusted_drift) > threshold
        return {
            'current_duration': round(current_duration, 4),
            'original_target': round(target_duration, 4),
            'time_elapsed': time_elapsed,
            'adjusted_target': round(adjusted_target, 4),
            'duration_drift': round(adjusted_drift, 4),
            'threshold': threshold,
            'needs_rebalancing': needs_rebalancing,
            'action': 'Rebalance portfolio' if needs_rebalancing else 'No action needed',
            'recommendation': self._get_rebalancing_recommendation(adjusted_drift) if needs_rebalancing else None
        }

    def _get_rebalancing_recommendation(self, drift: float) -> str:
        if drift > 0:
            return f"Portfolio duration too long by {abs(drift):.2f} years. Sell longer bonds, buy shorter bonds."
        return f"Portfolio duration too short by {abs(drift):.2f} years. Sell shorter bonds, buy longer bonds."

    def calculate_contingent_immunization(self, assets: float, liabilities_pv: float, floor_rate: float,
                                          current_rate: float, duration: float, frequency: int = 1) -> Dict[str, Any]:
        """
        Contingent immunization (Leibowitz-Weinberger).

        required terminal value  = liabilities_pv x (1 + floor_rate/f)^(f x T)   (the safety-net outcome)
        required assets today    = required terminal value / (1 + current_rate/f)^(f x T)
                                   (what immunizing at today's rate needs to lock that in)
        dollar safety cushion    = assets - required assets today
        With liabilities_pv = assets this is the textbook case "floor return on the initial assets".
        T = duration (the horizon, years); f = compounding frequency (1 = annual).
        """
        if assets <= 0 or liabilities_pv <= 0:
            raise FIError('assets and liabilities_pv must be > 0')
        if duration <= 0:
            raise FIError('duration (the investment horizon in years) must be > 0')
        if frequency not in (1, 2, 4, 12):
            raise FIError('frequency must be 1, 2, 4 or 12')
        if floor_rate <= -frequency or current_rate <= -frequency:
            raise FIError('rates must be > -frequency')
        n = frequency * duration
        required_tv = liabilities_pv * (1 + floor_rate / frequency) ** n
        required_assets = required_tv / (1 + current_rate / frequency) ** n
        cushion = assets - required_assets
        terminal_if_immunized = assets * (1 + current_rate / frequency) ** n
        cushion_ratio = cushion / assets
        if cushion <= 0:
            rec = 'Immunize immediately - assets are at or below the trigger'
        elif cushion_ratio < 0.02:
            rec = 'Cushion nearly exhausted - prepare to immunize'
        else:
            rec = 'Active management allowed while assets stay above the trigger value'
        return {
            'current_assets': round(assets, 2),
            'liability_pv': round(liabilities_pv, 2),
            'required_terminal_value': round(required_tv, 2),
            'terminal_value_if_immunized_now': round(terminal_if_immunized, 2),
            'trigger_value': round(required_assets, 2),
            'safety_margin': round(cushion, 2),
            'max_loss_before_trigger': round(max(cushion, 0.0), 2),
            'cushion_ratio': round(cushion_ratio, 4),
            'cushion_pct': round(cushion_ratio * 100, 2),
            'can_manage_actively': cushion > 0,
            'floor_rate': floor_rate,
            'current_rate': current_rate,
            'horizon_years': duration,
            'recommendation': rec,
            'conventions': {'compounding': f'{frequency}x per year',
                            'trigger_value': 'PV at current_rate of the required terminal value'},
        }


class CashFlowMatching:
    """Cash flow matching (dedication)."""

    def match_liabilities(self, liabilities: List[Dict[str, float]], available_bonds: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Backward dedication: starting from the latest liability, buy the bond maturing closest to (and not
        after) the liability date; coupons that bonds already bought pay between the previous liability date
        and this one reduce what must be bought for this liability. Principal received before a liability
        date is held (no reinvestment income).
        """
        if not isinstance(liabilities, list) or not liabilities:
            raise FIError('liabilities must be a non-empty list of {date, amount} (date in years)')
        if not isinstance(available_bonds, list) or not available_bonds:
            raise FIError('available_bonds must be a non-empty list of {identifier, maturity, coupon_rate, price, face_value}')
        liabs = []
        for i, l in enumerate(liabilities):
            if not isinstance(l, dict):
                raise FIError(f'liabilities[{i}] must be an object')
            liabs.append({'date': num(l, 'date', lo=0, lo_open=True), 'amount': num(l, 'amount', lo=0)})
        bonds = []
        for i, b in enumerate(available_bonds):
            if not isinstance(b, dict):
                raise FIError(f'available_bonds[{i}] must be an object')
            bonds.append({'identifier': str(b.get('identifier', f'#{i + 1}')),
                          'maturity': num(b, 'maturity', lo=0, lo_open=True),
                          'coupon_rate': num(b, 'coupon_rate', 0.05, lo=0),
                          'face_value': num(b, 'face_value', 1000, lo=0, lo_open=True),
                          'price': num(b, 'price', 1000, lo=0, lo_open=True),
                          'frequency': fc.check_frequency(num(b, 'frequency', 1, integer=True))})
        liabs.sort(key=lambda x: x['date'])
        purchased = []  # (bond, units)

        def coupons_between(b, units, a, z):
            """Coupon cash (not final payment) a purchased bond pays in (a, z]."""
            f = b['frequency']
            if f == 0 or b['coupon_rate'] == 0:
                return 0.0
            c = b['face_value'] * b['coupon_rate'] / f
            total = 0.0
            k = 1
            while True:
                t = b['maturity'] - k / f
                if t <= a + 1e-12:
                    break
                if t <= z + 1e-12:
                    total += c
                k += 1
            return units * total

        matched, unmatched, total_cost = [], [], 0.0
        for j in range(len(liabs) - 1, -1, -1):
            d, amount = liabs[j]['date'], liabs[j]['amount']
            prev_d = liabs[j - 1]['date'] if j > 0 else 0.0
            covered = sum(coupons_between(b, u, prev_d, d) for b, u in purchased)
            need = max(amount - covered, 0.0)
            candidates = [b for b in bonds if b['maturity'] <= d + 1e-12]
            if need <= 0:
                matched.append({'liability_date': d, 'liability_amount': round(amount, 2),
                                'covered_by_coupons': round(covered, 2), 'bond_id': None, 'units': 0.0, 'cost': 0.0})
                continue
            if not candidates:
                unmatched.append({'date': d, 'amount': round(amount, 2), 'shortfall': round(need, 2)})
                continue
            best = min(candidates, key=lambda b: d - b['maturity'])
            f = best['frequency']
            final_cf = best['face_value'] + (best['face_value'] * best['coupon_rate'] / f if f else 0.0)
            units = need / final_cf
            cost = units * best['price']
            purchased.append((best, units))
            matched.append({'liability_date': d, 'liability_amount': round(amount, 2),
                            'covered_by_coupons': round(covered, 2), 'bond_id': best['identifier'],
                            'bond_maturity': best['maturity'], 'units': round(units, 4), 'cost': round(cost, 2)})
            total_cost += cost
        matched.reverse()
        return {
            'matched_bonds': matched,
            'unmatched_liabilities': unmatched,
            'total_cost': round(total_cost, 2),
            'num_matched': len(matched),
            'num_unmatched': len(unmatched),
            'is_fully_matched': len(unmatched) == 0,
            'method': 'backward dedication; coupons of bonds already bought offset earlier liabilities; '
                      'no reinvestment income assumed',
        }


class BondIndexing:
    """Bond index replication analytics."""

    def calculate_tracking_error(self, portfolio_returns: List[float], benchmark_returns: List[float],
                                 periods_per_year: int = 12) -> Dict[str, Any]:
        """Tracking error = sample standard deviation of active returns, annualised by sqrt(periods_per_year)."""
        if len(portfolio_returns) != len(benchmark_returns):
            raise FIError('portfolio_returns and benchmark_returns must have the same length')
        n = len(portfolio_returns)
        if n < 2:
            raise FIError('tracking error needs at least 2 return observations per series')
        if periods_per_year <= 0:
            raise FIError('periods_per_year must be > 0')
        active = [p - b for p, b in zip(portfolio_returns, benchmark_returns)]
        mean_active = sum(active) / n
        te = math.sqrt(sum((a - mean_active) ** 2 for a in active) / (n - 1))
        te_annual = te * math.sqrt(periods_per_year)
        return {
            'tracking_error_periodic': round(te, 6),
            'tracking_error_monthly': round(te, 6) if periods_per_year == 12 else None,
            'tracking_error_annual': round(te_annual, 6),
            'tracking_error_annual_pct': round(te_annual * 100, 4),
            'mean_active_return': round(mean_active, 6),
            'information_ratio_periodic': round(mean_active / te, 4) if te > 0 else None,
            'information_ratio_monthly': round(mean_active / te, 4) if te > 0 and periods_per_year == 12 else None,
            'information_ratio_annual': round(mean_active * periods_per_year / te_annual, 4) if te_annual > 0 else None,
            'num_periods': n,
            'periods_per_year': periods_per_year,
        }

    def cell_matching_analysis(self, index_cells: Dict[str, float], portfolio_cells: Dict[str, float]) -> Dict[str, Any]:
        """Compare index and portfolio weights by cell (sector / duration bucket)."""
        if not isinstance(index_cells, dict) or not isinstance(portfolio_cells, dict):
            raise FIError('index_cells and portfolio_cells must be objects of {cell: weight}')
        cells = sorted(set(index_cells) | set(portfolio_cells))
        if not cells:
            raise FIError('index_cells and portfolio_cells are both empty')
        mismatches, total = [], 0.0
        for cell in cells:
            iw = fc.as_num(index_cells.get(cell, 0), f'index_cells[{cell}]')
            pw = fc.as_num(portfolio_cells.get(cell, 0), f'portfolio_cells[{cell}]')
            diff = pw - iw
            mismatches.append({'cell': cell, 'index_weight': round(iw, 4), 'portfolio_weight': round(pw, 4),
                               'difference': round(diff, 4), 'abs_difference': round(abs(diff), 4)})
            total += abs(diff)
        mismatches.sort(key=lambda x: x['abs_difference'], reverse=True)
        return {
            'cell_analysis': mismatches,
            'total_absolute_mismatch': round(total, 4),
            'average_mismatch': round(total / len(cells), 4),
            'num_cells': len(cells),
            'largest_mismatch': mismatches[0],
        }


ANALYSIS_TYPES = ('portfolio_metrics', 'duration_contribution', 'key_rate_exposure', 'immunization_requirements',
                  'immunize_two_bonds', 'rebalancing_check', 'contingent_immunization', 'cash_flow_matching',
                  'tracking_error', 'cell_matching')


def run_portfolio_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Main entry point for bond portfolio analysis (params['analysis_type'] in ANALYSIS_TYPES)."""
    analysis_type = params.get('analysis_type', 'portfolio_metrics')

    try:
        if analysis_type == 'portfolio_metrics':
            return BondPortfolioAnalyzer().calculate_portfolio_metrics(holdings=params.get('holdings'))

        elif analysis_type == 'duration_contribution':
            return BondPortfolioAnalyzer().calculate_contribution_to_duration(holdings=params.get('holdings'))

        elif analysis_type == 'key_rate_exposure':
            krs = params.get('key_rates')
            return BondPortfolioAnalyzer().calculate_key_rate_exposure(
                holdings=params.get('holdings'),
                key_rates=fc.num_list(params, 'key_rates', min_len=1, lo=0, lo_open=True) if krs is not None else None)

        elif analysis_type == 'immunization_requirements':
            return ImmunizationStrategy().calculate_immunization_requirements(
                liability_pv=num(params, 'liability_pv', 1000000),
                liability_duration=num(params, 'liability_duration', 7),
                current_yield=num(params, 'current_yield', 0.05))

        elif analysis_type == 'immunize_two_bonds':
            return ImmunizationStrategy().immunize_with_two_bonds(
                liability_duration=num(params, 'liability_duration', 7),
                liability_pv=num(params, 'liability_pv', 1000000),
                bond1=params.get('bond1') or {'duration': 3, 'convexity': 20},
                bond2=params.get('bond2') or {'duration': 10, 'convexity': 100})

        elif analysis_type == 'rebalancing_check':
            return ImmunizationStrategy().check_rebalancing_need(
                current_duration=num(params, 'current_duration', 7.5),
                target_duration=num(params, 'target_duration', 7),
                threshold=num(params, 'threshold', 0.25),
                time_elapsed=num(params, 'time_elapsed', 0))

        elif analysis_type == 'contingent_immunization':
            return ImmunizationStrategy().calculate_contingent_immunization(
                assets=num(params, 'assets', 1100000),
                liabilities_pv=num(params, 'liabilities_pv', 1000000),
                floor_rate=num(params, 'floor_rate', 0.04),
                current_rate=num(params, 'current_rate', 0.05),
                duration=num(params, 'duration', 10),
                frequency=num(params, 'frequency', 1, integer=True))

        elif analysis_type == 'cash_flow_matching':
            return CashFlowMatching().match_liabilities(
                liabilities=params.get('liabilities'),
                available_bonds=params.get('available_bonds'))

        elif analysis_type == 'tracking_error':
            return BondIndexing().calculate_tracking_error(
                portfolio_returns=fc.num_list(params, 'portfolio_returns'),
                benchmark_returns=fc.num_list(params, 'benchmark_returns'),
                periods_per_year=num(params, 'periods_per_year', 12, integer=True))

        elif analysis_type == 'cell_matching':
            return BondIndexing().cell_matching_analysis(
                index_cells=params.get('index_cells', {}),
                portfolio_cells=params.get('portfolio_cells', {}))

        else:
            return {'error': f'Unknown analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}

    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        logger.error(f"Portfolio analysis error: {e}")
        return {'error': str(e) or type(e).__name__}


if __name__ == "__main__":
    fc.import_fincept_json()
    fc.emit_usage_or_run(sys.argv, run_portfolio_analysis, ANALYSIS_TYPES, 'bond_portfolio')
