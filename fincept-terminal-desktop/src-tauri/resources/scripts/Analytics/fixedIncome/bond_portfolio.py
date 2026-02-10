"""
Bond Portfolio Analytics Module
===============================

Portfolio-level fixed income analytics implementing CFA Institute standard
methodologies for bond portfolio management.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Portfolio holdings (bonds with weights/values)
  - Liability stream for ALM
  - Benchmark indices
  - Duration and convexity of holdings

OUTPUT:
  - Portfolio duration and convexity
  - Immunization strategies
  - Liability matching
  - Rebalancing recommendations
  - Attribution analysis
  - Risk metrics (tracking error, VaR)

PARAMETERS:
  - holdings: List of bond holdings with characteristics
  - liabilities: Future liability cash flows
  - target_duration: Target portfolio duration
  - rebalancing_threshold: Threshold for rebalancing - default: 0.10
"""

from dataclasses import dataclass, field
from typing import Dict, Any, List, Optional, Tuple
from enum import Enum
import numpy as np
from scipy import optimize
import logging

logging.basicConfig(level=logging.INFO)
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
    """Individual bond holding in portfolio"""
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


class BondPortfolioAnalyzer:
    """
    Bond portfolio analytics engine.

    Provides portfolio-level metrics and optimization.
    """

    def calculate_portfolio_metrics(
        self,
        holdings: List[Dict[str, Any]],
    ) -> Dict[str, Any]:
        """
        Calculate aggregate portfolio metrics.

        Args:
            holdings: List of bond holdings with market_value, duration, convexity, ytm

        Returns:
            Dictionary with portfolio metrics
        """
        total_mv = sum(h.get('market_value', 0) for h in holdings)

        if total_mv == 0:
            return {'error': 'Total market value is zero'}

        # Weighted average duration
        portfolio_duration = sum(
            h.get('duration', 0) * h.get('market_value', 0) / total_mv
            for h in holdings
        )

        # Weighted average convexity
        portfolio_convexity = sum(
            h.get('convexity', 0) * h.get('market_value', 0) / total_mv
            for h in holdings
        )

        # Weighted average yield
        portfolio_yield = sum(
            h.get('ytm', 0) * h.get('market_value', 0) / total_mv
            for h in holdings
        )

        # Weighted average maturity
        portfolio_maturity = sum(
            h.get('maturity', 0) * h.get('market_value', 0) / total_mv
            for h in holdings
        )

        # Dollar duration
        dollar_duration = portfolio_duration * total_mv / 100

        # DV01
        dv01 = portfolio_duration * total_mv * 0.0001

        # Portfolio convexity adjustment for 100bp move
        convexity_adjustment = 0.5 * portfolio_convexity * (0.01 ** 2) * total_mv

        return {
            'total_market_value': round(total_mv, 2),
            'num_holdings': len(holdings),
            'portfolio_duration': round(portfolio_duration, 4),
            'portfolio_convexity': round(portfolio_convexity, 4),
            'portfolio_yield': round(portfolio_yield, 6),
            'portfolio_yield_pct': round(portfolio_yield * 100, 4),
            'portfolio_maturity': round(portfolio_maturity, 4),
            'dollar_duration': round(dollar_duration, 2),
            'dv01': round(dv01, 2),
            'convexity_adjustment_100bp': round(convexity_adjustment, 2),
            'price_change_estimate_100bp_up': round(-portfolio_duration * 0.01 * total_mv + convexity_adjustment, 2),
            'price_change_estimate_100bp_down': round(portfolio_duration * 0.01 * total_mv + convexity_adjustment, 2)
        }

    def calculate_contribution_to_duration(
        self,
        holdings: List[Dict[str, Any]],
    ) -> Dict[str, Any]:
        """
        Calculate each holding's contribution to portfolio duration.

        Args:
            holdings: List of bond holdings

        Returns:
            Dictionary with duration contributions
        """
        total_mv = sum(h.get('market_value', 0) for h in holdings)

        if total_mv == 0:
            return {'error': 'Total market value is zero'}

        contributions = []
        total_contribution = 0

        for h in holdings:
            weight = h.get('market_value', 0) / total_mv
            duration = h.get('duration', 0)
            contribution = weight * duration

            contributions.append({
                'identifier': h.get('identifier', 'Unknown'),
                'market_value': round(h.get('market_value', 0), 2),
                'weight': round(weight, 4),
                'duration': round(duration, 4),
                'duration_contribution': round(contribution, 4),
                'contribution_pct': round((contribution / duration * 100) if duration > 0 else 0, 2)
            })
            total_contribution += contribution

        # Sort by contribution
        contributions.sort(key=lambda x: x['duration_contribution'], reverse=True)

        return {
            'contributions': contributions,
            'portfolio_duration': round(total_contribution, 4),
            'top_contributors': contributions[:5] if len(contributions) > 5 else contributions
        }

    def calculate_key_rate_exposure(
        self,
        holdings: List[Dict[str, Any]],
        key_rates: List[float] = None,
    ) -> Dict[str, Any]:
        """
        Calculate portfolio exposure to key rate movements.

        Args:
            holdings: List of bond holdings with maturity info
            key_rates: Key rate tenors to analyze

        Returns:
            Dictionary with key rate exposures
        """
        if key_rates is None:
            key_rates = [1, 2, 3, 5, 7, 10, 20, 30]

        total_mv = sum(h.get('market_value', 0) for h in holdings)

        # Bucket holdings by maturity
        buckets = {kr: {'mv': 0, 'duration_contrib': 0} for kr in key_rates}

        for h in holdings:
            maturity = h.get('maturity', 0)
            mv = h.get('market_value', 0)
            duration = h.get('duration', 0)

            # Find appropriate bucket
            for i, kr in enumerate(key_rates):
                if i == len(key_rates) - 1:
                    buckets[kr]['mv'] += mv
                    buckets[kr]['duration_contrib'] += duration * mv / total_mv
                    break
                elif maturity <= (key_rates[i] + key_rates[i + 1]) / 2:
                    buckets[kr]['mv'] += mv
                    buckets[kr]['duration_contrib'] += duration * mv / total_mv
                    break

        exposures = [
            {
                'key_rate': kr,
                'market_value': round(buckets[kr]['mv'], 2),
                'weight': round(buckets[kr]['mv'] / total_mv, 4) if total_mv > 0 else 0,
                'duration_contribution': round(buckets[kr]['duration_contrib'], 4)
            }
            for kr in key_rates
        ]

        return {
            'key_rate_exposures': exposures,
            'total_market_value': round(total_mv, 2),
            'concentration_risk': self._assess_concentration(exposures)
        }

    def _assess_concentration(self, exposures: List[Dict]) -> str:
        """Assess key rate concentration risk."""
        weights = [e['weight'] for e in exposures]
        max_weight = max(weights) if weights else 0

        if max_weight > 0.50:
            return 'High concentration - over 50% in single bucket'
        elif max_weight > 0.30:
            return 'Moderate concentration - over 30% in single bucket'
        else:
            return 'Well diversified across key rates'


class ImmunizationStrategy:
    """
    Bond portfolio immunization strategies.

    Implements classical and contingent immunization.
    """

    def calculate_immunization_requirements(
        self,
        liability_pv: float,
        liability_duration: float,
        current_yield: float,
    ) -> Dict[str, Any]:
        """
        Calculate requirements for classical immunization.

        For immunization:
        1. PV(Assets) = PV(Liabilities)
        2. Duration(Assets) = Duration(Liabilities)
        3. Convexity(Assets) >= Convexity(Liabilities)

        Args:
            liability_pv: Present value of liabilities
            liability_duration: Duration of liabilities
            current_yield: Current market yield

        Returns:
            Dictionary with immunization requirements
        """
        return {
            'required_asset_pv': round(liability_pv, 2),
            'required_duration': round(liability_duration, 4),
            'target_yield': round(current_yield, 6),
            'conditions': [
                f"Asset PV must equal ${liability_pv:,.2f}",
                f"Asset duration must equal {liability_duration:.2f} years",
                "Asset convexity must exceed liability convexity",
                "Portfolio must be rebalanced when duration drifts"
            ],
            'rebalancing_frequency': 'Quarterly or when duration drifts > 0.25 years'
        }

    def immunize_with_two_bonds(
        self,
        liability_duration: float,
        liability_pv: float,
        bond1: Dict[str, float],
        bond2: Dict[str, float],
    ) -> Dict[str, Any]:
        """
        Create immunized portfolio using two bonds.

        Args:
            liability_duration: Target duration
            liability_pv: Target present value
            bond1: First bond (shorter duration) with duration, price
            bond2: Second bond (longer duration) with duration, price

        Returns:
            Dictionary with allocation
        """
        d1 = bond1.get('duration', 3)
        d2 = bond2.get('duration', 10)

        if d1 == d2:
            return {'error': 'Bonds must have different durations'}

        # Solve for weights: w1*d1 + w2*d2 = D_liability, w1 + w2 = 1
        w2 = (liability_duration - d1) / (d2 - d1)
        w1 = 1 - w2

        if w1 < 0 or w2 < 0:
            return {
                'error': 'Cannot immunize - liability duration outside bond duration range',
                'liability_duration': liability_duration,
                'bond1_duration': d1,
                'bond2_duration': d2
            }

        # Calculate amounts
        amount1 = liability_pv * w1
        amount2 = liability_pv * w2

        # Portfolio duration (verify)
        portfolio_duration = w1 * d1 + w2 * d2

        # Portfolio convexity
        c1 = bond1.get('convexity', 20)
        c2 = bond2.get('convexity', 100)
        portfolio_convexity = w1 * c1 + w2 * c2

        return {
            'bond1': {
                'weight': round(w1, 4),
                'weight_pct': round(w1 * 100, 2),
                'amount': round(amount1, 2),
                'duration': d1
            },
            'bond2': {
                'weight': round(w2, 4),
                'weight_pct': round(w2 * 100, 2),
                'amount': round(amount2, 2),
                'duration': d2
            },
            'portfolio_duration': round(portfolio_duration, 4),
            'portfolio_convexity': round(portfolio_convexity, 4),
            'liability_duration': liability_duration,
            'liability_pv': liability_pv,
            'is_immunized': abs(portfolio_duration - liability_duration) < 0.01
        }

    def check_rebalancing_need(
        self,
        current_duration: float,
        target_duration: float,
        threshold: float = 0.25,
        time_elapsed: float = 0,
    ) -> Dict[str, Any]:
        """
        Check if portfolio rebalancing is needed.

        Args:
            current_duration: Current portfolio duration
            target_duration: Target (liability) duration
            threshold: Duration drift threshold for rebalancing
            time_elapsed: Time since last rebalancing (years)

        Returns:
            Dictionary with rebalancing recommendation
        """
        duration_drift = current_duration - target_duration

        # Adjust target for time elapsed (liability duration decreases)
        adjusted_target = target_duration - time_elapsed
        adjusted_drift = current_duration - adjusted_target

        needs_rebalancing = abs(adjusted_drift) > threshold

        return {
            'current_duration': round(current_duration, 4),
            'original_target': round(target_duration, 4),
            'adjusted_target': round(adjusted_target, 4),
            'duration_drift': round(adjusted_drift, 4),
            'threshold': threshold,
            'needs_rebalancing': needs_rebalancing,
            'action': 'Rebalance portfolio' if needs_rebalancing else 'No action needed',
            'recommendation': self._get_rebalancing_recommendation(adjusted_drift) if needs_rebalancing else None
        }

    def _get_rebalancing_recommendation(self, drift: float) -> str:
        """Get rebalancing recommendation based on duration drift."""
        if drift > 0:
            return f"Portfolio duration too long by {abs(drift):.2f} years. Sell longer bonds, buy shorter bonds."
        else:
            return f"Portfolio duration too short by {abs(drift):.2f} years. Sell shorter bonds, buy longer bonds."

    def calculate_contingent_immunization(
        self,
        assets: float,
        liabilities_pv: float,
        floor_rate: float,
        current_rate: float,
        duration: float,
    ) -> Dict[str, Any]:
        """
        Calculate contingent immunization parameters.

        Allows active management as long as assets exceed floor.

        Args:
            assets: Current asset value
            liabilities_pv: Present value of liabilities
            floor_rate: Minimum acceptable return
            current_rate: Current market rate
            duration: Investment horizon

        Returns:
            Dictionary with contingent immunization analysis
        """
        # Safety margin
        safety_margin = assets - liabilities_pv

        # Cushion (active management budget)
        cushion = safety_margin / assets if assets > 0 else 0

        # Dollar safety margin
        dollar_cushion = safety_margin

        # Trigger point (when to switch to full immunization)
        trigger_value = liabilities_pv

        return {
            'current_assets': round(assets, 2),
            'liability_pv': round(liabilities_pv, 2),
            'safety_margin': round(safety_margin, 2),
            'cushion_ratio': round(cushion, 4),
            'cushion_pct': round(cushion * 100, 2),
            'trigger_value': round(trigger_value, 2),
            'can_manage_actively': safety_margin > 0,
            'max_loss_before_trigger': round(dollar_cushion, 2),
            'recommendation': 'Active management allowed' if safety_margin > assets * 0.05 else 'Immunize immediately'
        }


class CashFlowMatching:
    """
    Cash flow matching (dedication) strategies.
    """

    def match_liabilities(
        self,
        liabilities: List[Dict[str, float]],
        available_bonds: List[Dict[str, Any]],
    ) -> Dict[str, Any]:
        """
        Match bond cash flows to liability stream.

        Args:
            liabilities: List of {date, amount} for each liability
            available_bonds: List of bonds with cash flow schedules

        Returns:
            Dictionary with matching solution
        """
        # Sort liabilities by date (latest first for backward matching)
        sorted_liabilities = sorted(liabilities, key=lambda x: x['date'], reverse=True)

        matched_bonds = []
        total_cost = 0
        unmatched = []

        for liability in sorted_liabilities:
            date = liability['date']
            amount = liability['amount']

            # Find bonds that mature at or before this date
            candidates = [b for b in available_bonds if b.get('maturity', 0) <= date]

            if candidates:
                # Select bond with maturity closest to liability date
                best_bond = min(candidates, key=lambda b: abs(b.get('maturity', 0) - date))

                # Calculate units needed
                bond_cf = best_bond.get('face_value', 1000) + \
                          best_bond.get('coupon_rate', 0.05) * best_bond.get('face_value', 1000)
                units = amount / bond_cf
                cost = units * best_bond.get('price', 1000)

                matched_bonds.append({
                    'liability_date': date,
                    'liability_amount': round(amount, 2),
                    'bond_id': best_bond.get('identifier', 'Unknown'),
                    'bond_maturity': best_bond.get('maturity', 0),
                    'units': round(units, 4),
                    'cost': round(cost, 2)
                })
                total_cost += cost
            else:
                unmatched.append({
                    'date': date,
                    'amount': round(amount, 2)
                })

        return {
            'matched_bonds': matched_bonds,
            'unmatched_liabilities': unmatched,
            'total_cost': round(total_cost, 2),
            'num_matched': len(matched_bonds),
            'num_unmatched': len(unmatched),
            'is_fully_matched': len(unmatched) == 0
        }


class BondIndexing:
    """
    Bond index replication strategies.
    """

    def calculate_tracking_error(
        self,
        portfolio_returns: List[float],
        benchmark_returns: List[float],
    ) -> Dict[str, Any]:
        """
        Calculate portfolio tracking error vs benchmark.

        Args:
            portfolio_returns: List of portfolio returns
            benchmark_returns: List of benchmark returns

        Returns:
            Dictionary with tracking error metrics
        """
        if len(portfolio_returns) != len(benchmark_returns):
            return {'error': 'Return series must have same length'}

        # Active returns
        active_returns = np.array(portfolio_returns) - np.array(benchmark_returns)

        # Tracking error (standard deviation of active returns)
        tracking_error = np.std(active_returns, ddof=1)

        # Annualize (assuming monthly returns)
        tracking_error_annual = tracking_error * np.sqrt(12)

        # Information ratio
        mean_active = np.mean(active_returns)
        info_ratio = mean_active / tracking_error if tracking_error > 0 else 0
        info_ratio_annual = (mean_active * 12) / tracking_error_annual if tracking_error_annual > 0 else 0

        return {
            'tracking_error_monthly': round(tracking_error, 6),
            'tracking_error_annual': round(tracking_error_annual, 6),
            'tracking_error_annual_pct': round(tracking_error_annual * 100, 4),
            'mean_active_return': round(mean_active, 6),
            'information_ratio_monthly': round(info_ratio, 4),
            'information_ratio_annual': round(info_ratio_annual, 4),
            'num_periods': len(portfolio_returns)
        }

    def cell_matching_analysis(
        self,
        index_cells: Dict[str, float],
        portfolio_cells: Dict[str, float],
    ) -> Dict[str, Any]:
        """
        Analyze cell-matching for stratified sampling.

        Args:
            index_cells: Index weights by cell (sector/duration)
            portfolio_cells: Portfolio weights by cell

        Returns:
            Dictionary with cell matching analysis
        """
        cells = set(index_cells.keys()) | set(portfolio_cells.keys())

        mismatches = []
        total_mismatch = 0

        for cell in cells:
            idx_weight = index_cells.get(cell, 0)
            port_weight = portfolio_cells.get(cell, 0)
            diff = port_weight - idx_weight

            mismatches.append({
                'cell': cell,
                'index_weight': round(idx_weight, 4),
                'portfolio_weight': round(port_weight, 4),
                'difference': round(diff, 4),
                'abs_difference': round(abs(diff), 4)
            })
            total_mismatch += abs(diff)

        # Sort by absolute difference
        mismatches.sort(key=lambda x: x['abs_difference'], reverse=True)

        return {
            'cell_analysis': mismatches,
            'total_absolute_mismatch': round(total_mismatch, 4),
            'average_mismatch': round(total_mismatch / len(cells), 4) if cells else 0,
            'num_cells': len(cells),
            'largest_mismatch': mismatches[0] if mismatches else None
        }


def run_portfolio_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main entry point for bond portfolio analysis.

    Args:
        params: Analysis parameters

    Returns:
        Analysis results
    """
    analysis_type = params.get('analysis_type', 'portfolio_metrics')

    try:
        if analysis_type == 'portfolio_metrics':
            analyzer = BondPortfolioAnalyzer()
            return analyzer.calculate_portfolio_metrics(
                holdings=params.get('holdings', [])
            )

        elif analysis_type == 'duration_contribution':
            analyzer = BondPortfolioAnalyzer()
            return analyzer.calculate_contribution_to_duration(
                holdings=params.get('holdings', [])
            )

        elif analysis_type == 'key_rate_exposure':
            analyzer = BondPortfolioAnalyzer()
            return analyzer.calculate_key_rate_exposure(
                holdings=params.get('holdings', []),
                key_rates=params.get('key_rates')
            )

        elif analysis_type == 'immunization_requirements':
            strategy = ImmunizationStrategy()
            return strategy.calculate_immunization_requirements(
                liability_pv=params.get('liability_pv', 1000000),
                liability_duration=params.get('liability_duration', 7),
                current_yield=params.get('current_yield', 0.05)
            )

        elif analysis_type == 'immunize_two_bonds':
            strategy = ImmunizationStrategy()
            return strategy.immunize_with_two_bonds(
                liability_duration=params.get('liability_duration', 7),
                liability_pv=params.get('liability_pv', 1000000),
                bond1=params.get('bond1', {'duration': 3, 'convexity': 20}),
                bond2=params.get('bond2', {'duration': 10, 'convexity': 100})
            )

        elif analysis_type == 'rebalancing_check':
            strategy = ImmunizationStrategy()
            return strategy.check_rebalancing_need(
                current_duration=params.get('current_duration', 7.5),
                target_duration=params.get('target_duration', 7),
                threshold=params.get('threshold', 0.25),
                time_elapsed=params.get('time_elapsed', 0.25)
            )

        elif analysis_type == 'contingent_immunization':
            strategy = ImmunizationStrategy()
            return strategy.calculate_contingent_immunization(
                assets=params.get('assets', 1100000),
                liabilities_pv=params.get('liabilities_pv', 1000000),
                floor_rate=params.get('floor_rate', 0.04),
                current_rate=params.get('current_rate', 0.05),
                duration=params.get('duration', 10)
            )

        elif analysis_type == 'cash_flow_matching':
            matcher = CashFlowMatching()
            return matcher.match_liabilities(
                liabilities=params.get('liabilities', []),
                available_bonds=params.get('available_bonds', [])
            )

        elif analysis_type == 'tracking_error':
            indexer = BondIndexing()
            return indexer.calculate_tracking_error(
                portfolio_returns=params.get('portfolio_returns', []),
                benchmark_returns=params.get('benchmark_returns', [])
            )

        elif analysis_type == 'cell_matching':
            indexer = BondIndexing()
            return indexer.cell_matching_analysis(
                index_cells=params.get('index_cells', {}),
                portfolio_cells=params.get('portfolio_cells', {})
            )

        else:
            return {'error': f'Unknown analysis type: {analysis_type}'}

    except Exception as e:
        logger.error(f"Portfolio analysis error: {str(e)}")
        return {'error': str(e)}


if __name__ == "__main__":
    import sys
    import json

    if len(sys.argv) > 1:
        try:
            params = json.loads(sys.argv[1])
            result = run_portfolio_analysis(params)
            print(json.dumps(result, indent=2))
        except json.JSONDecodeError as e:
            print(json.dumps({'error': f'Invalid JSON: {str(e)}'}))
    else:
        # Demo
        print("Bond Portfolio Analytics Demo:")

        # Sample portfolio
        holdings = [
            {'identifier': 'Bond A', 'market_value': 500000, 'duration': 3, 'convexity': 15, 'ytm': 0.04, 'maturity': 3},
            {'identifier': 'Bond B', 'market_value': 300000, 'duration': 7, 'convexity': 60, 'ytm': 0.05, 'maturity': 7},
            {'identifier': 'Bond C', 'market_value': 200000, 'duration': 10, 'convexity': 120, 'ytm': 0.055, 'maturity': 10},
        ]

        analyzer = BondPortfolioAnalyzer()
        result = analyzer.calculate_portfolio_metrics(holdings)
        print(f"\nPortfolio Duration: {result['portfolio_duration']:.2f} years")
        print(f"Portfolio DV01: ${result['dv01']:,.2f}")

        # Immunization example
        strategy = ImmunizationStrategy()
        result = strategy.immunize_with_two_bonds(
            liability_duration=7,
            liability_pv=1000000,
            bond1={'duration': 3, 'convexity': 20},
            bond2={'duration': 10, 'convexity': 100}
        )
        print(f"\nImmunization: {result['bond1']['weight_pct']}% in 3yr, {result['bond2']['weight_pct']}% in 10yr")
