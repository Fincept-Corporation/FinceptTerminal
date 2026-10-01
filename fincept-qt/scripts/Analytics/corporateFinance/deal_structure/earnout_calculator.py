"""Earnout Calculator for Contingent Payments"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
from enum import Enum
import numpy as np

class EarnoutMetric(Enum):
    REVENUE = "revenue"
    EBITDA = "ebitda"
    NET_INCOME = "net_income"
    USERS = "users"
    CUSTOM = "custom"

@dataclass
class EarnoutTranche:
    metric: EarnoutMetric
    threshold: float
    payment: float
    measurement_period_years: int
    description: str = ""

class EarnoutCalculator:
    """Calculate and value earnout provisions in M&A deals"""

    def __init__(self, discount_rate: float = 0.10):
        self.discount_rate = discount_rate

    def calculate_simple_earnout(self, base_price: float,
                                 earnout_tranches: List[EarnoutTranche],
                                 probability_weights: List[float]) -> Dict[str, Any]:
        """Calculate earnout value with probability weighting"""

        if len(earnout_tranches) != len(probability_weights):
            raise ValueError("Must provide probability for each tranche")

        if not abs(sum(probability_weights) - 1.0) < 0.01:
            raise ValueError(f"Probabilities must sum to 1.0, got {sum(probability_weights)}")

        tranche_details = []
        total_expected_earnout = 0

        for tranche, probability in zip(earnout_tranches, probability_weights):
            pv = tranche.payment / ((1 + self.discount_rate) ** tranche.measurement_period_years)
            expected_value = pv * probability

            total_expected_earnout += expected_value

            tranche_details.append({
                'metric': tranche.metric.value,
                'threshold': tranche.threshold,
                'payment_if_achieved': tranche.payment,
                'measurement_period': tranche.measurement_period_years,
                'probability': probability * 100,
                'present_value': pv,
                'expected_value': expected_value,
                'description': tranche.description
            })

        total_consideration = base_price + total_expected_earnout
        earnout_pct = (total_expected_earnout / total_consideration * 100) if total_consideration > 0 else 0

        return {
            'base_price': base_price,
            'earnout_tranches': tranche_details,
            'total_earnout_face_value': sum(t.payment for t in earnout_tranches),
            'total_earnout_pv': sum(t['present_value'] for t in tranche_details),
            'total_earnout_expected_value': total_expected_earnout,
            'total_consideration': total_consideration,
            'earnout_as_pct_of_deal': earnout_pct,
            'discount_rate': self.discount_rate * 100
        }

    def tiered_earnout(self, base_price: float,
                      metric_name: str,
                      tiers: List[Dict[str, float]],
                      probability_distribution: List[float]) -> Dict[str, Any]:
        """
        Tiered earnout based on performance levels

        tiers: [{'threshold': value, 'payment': amount}, ...]
        probability_distribution: probability for each tier
        """

        if len(tiers) != len(probability_distribution):
            raise ValueError("Must provide probability for each tier")

        tier_analysis = []
        total_expected = 0

        for tier, prob in zip(tiers, probability_distribution):
            expected = tier['payment'] * prob

            tier_analysis.append({
                'threshold': tier['threshold'],
                'payment': tier['payment'],
                'probability': prob * 100,
                'expected_value': expected
            })

            total_expected += expected

        return {
            'base_price': base_price,
            'metric': metric_name,
            'tiers': tier_analysis,
            'max_earnout': max(t['payment'] for t in tiers),
            'expected_earnout': total_expected,
            'total_expected_consideration': base_price + total_expected
        }

    def continuous_earnout(self, base_price: float,
                          metric_baseline: float,
                          earnout_rate: float,
                          cap: Optional[float] = None,
                          measurement_years: int = 3,
                          expected_metric_path: List[float] = None) -> Dict[str, Any]:
        """
        Continuous earnout (e.g., $X per revenue dollar above threshold)

        Args:
            earnout_rate: Payment per unit of metric above baseline
            expected_metric_path: Projected metric values for each year
        """

        if expected_metric_path is None or len(expected_metric_path) != measurement_years:
            raise ValueError(f"Must provide {measurement_years} years of metric projections")

        yearly_earnouts = []
        total_earnout_pv = 0

        for year, metric_value in enumerate(expected_metric_path, 1):
            excess = max(0, metric_value - metric_baseline)
            earnout_payment = excess * earnout_rate

            if cap:
                earnout_payment = min(earnout_payment, cap)

            pv = earnout_payment / ((1 + self.discount_rate) ** year)
            total_earnout_pv += pv

            yearly_earnouts.append({
                'year': year,
                'metric_value': metric_value,
                'baseline': metric_baseline,
                'excess': excess,
                'earnout_payment': earnout_payment,
                'present_value': pv
            })

        return {
            'base_price': base_price,
            'earnout_structure': 'continuous',
            'baseline_metric': metric_baseline,
            'earnout_rate': earnout_rate,
            'cap': cap,
            'measurement_years': measurement_years,
            'yearly_details': yearly_earnouts,
            'total_earnout_pv': total_earnout_pv,
            'total_consideration': base_price + total_earnout_pv,
            'discount_rate': self.discount_rate * 100
        }

    def revenue_milestone_earnout(self, base_price: float,
                                  milestones: List[Dict[str, float]],
                                  probabilities: List[float],
                                  timing: List[int]) -> Dict[str, Any]:
        """
        Revenue milestone-based earnout

        Args:
            milestones: [{'revenue_target': X, 'payment': Y}, ...]
            probabilities: Achievement probability for each
            timing: Years until each milestone
        """

        if not (len(milestones) == len(probabilities) == len(timing)):
            raise ValueError("Milestones, probabilities, and timing must have same length")

        milestone_analysis = []
        total_expected_pv = 0

        for milestone, prob, years in zip(milestones, probabilities, timing):
            pv = milestone['payment'] / ((1 + self.discount_rate) ** years)
            expected_pv = pv * prob

            total_expected_pv += expected_pv

            milestone_analysis.append({
                'revenue_target': milestone['revenue_target'],
                'payment_if_achieved': milestone['payment'],
                'years_to_milestone': years,
                'probability': prob * 100,
                'present_value': pv,
                'expected_value': expected_pv
            })

        return {
            'base_price': base_price,
            'milestones': milestone_analysis,
            'total_milestone_payments': sum(m['payment'] for m in milestones),
            'total_expected_earnout_pv': total_expected_pv,
            'total_expected_consideration': base_price + total_expected_pv,
            'discount_rate': self.discount_rate * 100
        }

    def earnout_sensitivity(self, base_earnout: Dict[str, Any],
                           probability_scenarios: List[List[float]],
                           scenario_names: List[str]) -> Dict[str, Any]:
        """Sensitivity analysis for earnout value under different probability assumptions"""

        if 'earnout_tranches' not in base_earnout:
            raise ValueError("Base earnout must have tranche structure")

        results = []

        for probs, name in zip(probability_scenarios, scenario_names):
            total_expected = 0
            for tranche, prob in zip(base_earnout['earnout_tranches'], probs):
                expected = tranche['present_value'] * prob
                total_expected += expected

            total_consideration = base_earnout['base_price'] + total_expected

            results.append({
                'scenario': name,
                'probabilities': [p * 100 for p in probs],
                'expected_earnout': total_expected,
                'total_consideration': total_consideration
            })

        return {
            'base_case': base_earnout,
            'sensitivity_scenarios': results,
            'valuation_range': {
                'min': min(r['total_consideration'] for r in results),
                'max': max(r['total_consideration'] for r in results),
                'base': base_earnout['total_consideration']
            }
        }

    def earnout_risk_adjustment(self, earnout_value: float,
                                measurement_period: int,
                                execution_risk: str = 'medium') -> Dict[str, Any]:
        """Apply risk adjustment to earnout valuation"""

        risk_discounts = {
            'low': 0.10,
            'medium': 0.25,
            'high': 0.40,
            'very_high': 0.60
        }

        discount = risk_discounts.get(execution_risk, 0.25)

        risk_adjusted_value = earnout_value * (1 - discount)

        return {
            'unadjusted_earnout_value': earnout_value,
            'execution_risk_level': execution_risk,
            'risk_discount': discount * 100,
            'risk_adjusted_value': risk_adjusted_value,
            'value_haircut': earnout_value - risk_adjusted_value,
            'measurement_period_years': measurement_period
        }

# ---- service ABI shim (MAAnalyticsService) BEGIN ----
# The Qt MAAnalyticsService calls `earnout_calculator.py calculate <flat-params-json>` (argv length 3). The native
# form is `earnout earnout_params financial_projections`, so a service-style call is translated here and then falls
# through to the native dispatch. Any other argv shape is untouched.
_SERVICE_COMMANDS = ("calculate",)


def _svc_num(p, *keys, default=None):
    for k in keys:
        v = p.get(k)
        if isinstance(v, (int, float)) and not isinstance(v, bool):
            return float(v)
    return default


def _service_argv(argv):
    import json
    if len(argv) != 3 or argv[1] not in _SERVICE_COMMANDS:
        return argv
    try:
        p = json.loads(argv[2])
    except ValueError:
        return argv
    if not isinstance(p, dict):
        return argv
    params = {"base_price": _svc_num(p, "base_price", default=0.0),
              "discount_rate": _svc_num(p, "discount_rate", default=0.10)}
    tranches = p.get("tranches")
    if isinstance(tranches, list) and tranches:
        params["tranches"] = tranches
        if isinstance(p.get("probabilities"), list):
            params["probabilities"] = p["probabilities"]
    else:
        # Panel shape: one revenue-threshold tranche paying `earnout_amount` with probability `probability`. The
        # calculator treats the tranches as mutually exclusive outcomes whose probabilities must sum to 1, so the
        # "threshold missed, nothing paid" outcome is added explicitly.
        probability = min(max(_svc_num(p, "probability", default=0.5), 0.0), 1.0)
        period = _svc_num(p, "period", "measurement_period_years", default=1.0)
        params["tranches"] = [
            {"metric": str(p.get("metric", "revenue")),
             "threshold": _svc_num(p, "revenue_threshold", "threshold", default=0.0),
             "payment": _svc_num(p, "earnout_amount", "payment", default=0.0),
             "measurement_period_years": period, "description": "Threshold met"},
            {"metric": str(p.get("metric", "revenue")), "threshold": 0.0, "payment": 0.0,
             "measurement_period_years": period, "description": "Threshold missed"},
        ]
        params["probabilities"] = [probability, 1.0 - probability]
    projections = p.get("financial_projections") if isinstance(p.get("financial_projections"), dict) else {}
    return [argv[0], "earnout", json.dumps(params), json.dumps(projections)]
# ---- service ABI shim (MAAnalyticsService) END ----


def main():
    """CLI entry point - outputs JSON for C++ integration"""
    import sys
    import json

    sys.argv = _service_argv(sys.argv)
    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "earnout":
            if len(sys.argv) < 4:
                raise ValueError("Earnout params and financial projections required")

            earnout_params = json.loads(sys.argv[2])
            financial_projections = json.loads(sys.argv[3])

            calculator = EarnoutCalculator(discount_rate=earnout_params.get('discount_rate', 0.10))

            # Basic earnout calculation
            base_price = earnout_params.get('base_price', 0)
            tranches_data = earnout_params.get('tranches', [])

            tranches = []
            for t in tranches_data:
                # Handle metric: accept string or EarnoutMetric enum value
                metric_raw = t.get('metric', 'revenue')
                try:
                    metric = EarnoutMetric(metric_raw)
                except (ValueError, KeyError):
                    metric = EarnoutMetric.CUSTOM

                tranches.append(EarnoutTranche(
                    metric=metric,
                    threshold=t.get('threshold', t.get('target_value', 0)),
                    payment=t.get('payment', 0),
                    measurement_period_years=t.get('measurement_period_years', t.get('measurement_period', 1)),
                    description=t.get('description', '')
                ))

            # Handle probabilities: default to equal weighting that sums to 1.0
            probabilities = earnout_params.get('probabilities', None)
            if probabilities is None or len(probabilities) != len(tranches):
                n = len(tranches)
                probabilities = [1.0 / n] * n if n > 0 else []

            analysis = calculator.calculate_simple_earnout(base_price, tranches, probabilities)

            result = {"success": True, "data": analysis}
            print(json.dumps(result))

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))
            sys.exit(1)

    except Exception as e:
        result = {"success": False, "error": str(e)}
        print(json.dumps(result))
        sys.exit(1)

if __name__ == '__main__':
    main()
