"""Contingent Value Rights (CVR) Valuation"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
from enum import Enum
import numpy as np

class CVRType(Enum):
    MILESTONE = "milestone"
    EARNOUT = "earnout"
    REGULATORY = "regulatory"
    COMMODITY_PRICE = "commodity_price"
    LITIGATION = "litigation"

@dataclass
class CVRTrigger:
    trigger_type: CVRType
    description: str
    payment_if_triggered: float
    probability: float
    expected_timing_years: float

class CVRValuation:
    """Valuation of Contingent Value Rights in M&A transactions"""

    def __init__(self, discount_rate: float = 0.12,
                 risk_free_rate: float = 0.04):
        self.discount_rate = discount_rate
        self.risk_free_rate = risk_free_rate

    def value_milestone_cvr(self, triggers: List[CVRTrigger]) -> Dict[str, Any]:
        """Value milestone-based CVR (FDA approval, product launch, etc.)"""

        trigger_analysis = []
        total_expected_pv = 0

        for trigger in triggers:
            pv = trigger.payment_if_triggered / ((1 + self.discount_rate) ** trigger.expected_timing_years)
            expected_value = pv * trigger.probability

            total_expected_pv += expected_value

            trigger_analysis.append({
                'trigger_type': trigger.trigger_type.value,
                'description': trigger.description,
                'payment_if_triggered': trigger.payment_if_triggered,
                'probability': trigger.probability * 100,
                'expected_timing_years': trigger.expected_timing_years,
                'present_value': pv,
                'expected_value': expected_value
            })

        return {
            'cvr_type': 'milestone',
            'triggers': trigger_analysis,
            'total_face_value': sum(t.payment_if_triggered for t in triggers),
            'total_expected_value': total_expected_pv,
            'discount_rate': self.discount_rate * 100,
            'blended_probability': (total_expected_pv / sum(t['present_value'] for t in trigger_analysis) * 100) if trigger_analysis else 0
        }

    def value_regulatory_cvr(self, payment_if_approved: float,
                            approval_probability: float,
                            expected_decision_years: float,
                            appeal_possible: bool = False,
                            appeal_probability: float = 0,
                            appeal_delay_years: float = 0) -> Dict[str, Any]:
        """Value regulatory approval CVR (common in pharma/biotech deals)"""

        base_pv = payment_if_approved / ((1 + self.discount_rate) ** expected_decision_years)
        base_expected = base_pv * approval_probability

        if appeal_possible and approval_probability < 1.0:
            rejection_prob = 1 - approval_probability
            appeal_success_prob = rejection_prob * appeal_probability

            appeal_timing = expected_decision_years + appeal_delay_years
            appeal_pv = payment_if_approved / ((1 + self.discount_rate) ** appeal_timing)
            appeal_expected = appeal_pv * appeal_success_prob

            total_expected = base_expected + appeal_expected
            blended_probability = approval_probability + appeal_success_prob
        else:
            appeal_expected = 0
            total_expected = base_expected
            blended_probability = approval_probability

        return {
            'cvr_type': 'regulatory',
            'payment_if_approved': payment_if_approved,
            'base_approval_probability': approval_probability * 100,
            'expected_decision_years': expected_decision_years,
            'base_expected_value': base_expected,
            'appeal_scenario': {
                'appeal_possible': appeal_possible,
                'appeal_probability': appeal_probability * 100 if appeal_possible else 0,
                'appeal_expected_value': appeal_expected,
                'appeal_delay_years': appeal_delay_years
            },
            'total_expected_value': total_expected,
            'blended_success_probability': blended_probability * 100,
            'discount_rate': self.discount_rate * 100
        }

    def value_commodity_price_cvr(self, payment_schedule: List[Dict[str, float]],
                                  current_price: float,
                                  price_volatility: float,
                                  years_to_maturity: float) -> Dict[str, Any]:
        """
        Value commodity price-linked CVR

        payment_schedule: [{'threshold': price, 'payment': amount}, ...]
        Uses simplified option pricing logic
        """

        expected_payments = []
        total_expected_pv = 0

        for tier in payment_schedule:
            threshold = tier['threshold']
            payment = tier['payment']

            z_score = (np.log(threshold / current_price)) / (price_volatility * np.sqrt(years_to_maturity))

            from scipy.stats import norm
            probability = 1 - norm.cdf(z_score)

            pv = payment / ((1 + self.discount_rate) ** years_to_maturity)
            expected_value = pv * probability

            total_expected_pv += expected_value

            expected_payments.append({
                'price_threshold': threshold,
                'payment_if_reached': payment,
                'probability': probability * 100,
                'present_value': pv,
                'expected_value': expected_value
            })

        return {
            'cvr_type': 'commodity_price',
            'current_price': current_price,
            'price_volatility': price_volatility * 100,
            'years_to_maturity': years_to_maturity,
            'payment_tiers': expected_payments,
            'total_expected_value': total_expected_pv,
            'total_max_payment': sum(t['payment'] for t in payment_schedule)
        }

    def value_litigation_cvr(self, case_value: float,
                            win_probability: float,
                            expected_resolution_years: float,
                            legal_costs: float = 0,
                            settlement_probability: float = 0,
                            settlement_amount: float = 0) -> Dict[str, Any]:
        """Value litigation outcome CVR"""

        win_pv = (case_value - legal_costs) / ((1 + self.discount_rate) ** expected_resolution_years)
        win_expected = win_pv * win_probability

        if settlement_probability > 0:
            settlement_timing = expected_resolution_years * 0.7
            settlement_pv = settlement_amount / ((1 + self.discount_rate) ** settlement_timing)
            settlement_expected = settlement_pv * settlement_probability

            no_settlement_prob = 1 - settlement_probability
            adjusted_win_expected = win_expected * no_settlement_prob
            total_expected = adjusted_win_expected + settlement_expected
        else:
            settlement_expected = 0
            total_expected = win_expected

        return {
            'cvr_type': 'litigation',
            'case_value': case_value,
            'win_probability': win_probability * 100,
            'expected_resolution_years': expected_resolution_years,
            'legal_costs': legal_costs,
            'win_scenario_expected_value': win_expected,
            'settlement': {
                'settlement_probability': settlement_probability * 100,
                'settlement_amount': settlement_amount,
                'settlement_expected_value': settlement_expected
            },
            'total_expected_value': total_expected,
            'discount_rate': self.discount_rate * 100
        }

    def value_earnout_cvr(self, base_earnout: float,
                         stretch_earnout: float,
                         base_probability: float,
                         stretch_probability: float,
                         years_to_measurement: int) -> Dict[str, Any]:
        """Value multi-tier earnout CVR"""

        base_pv = base_earnout / ((1 + self.discount_rate) ** years_to_measurement)
        base_expected = base_pv * base_probability

        stretch_pv = (base_earnout + stretch_earnout) / ((1 + self.discount_rate) ** years_to_measurement)
        stretch_expected = stretch_pv * stretch_probability

        neither_prob = 1 - base_probability - stretch_probability
        neither_expected = 0

        total_expected = base_expected + stretch_expected

        return {
            'cvr_type': 'earnout',
            'base_earnout': base_earnout,
            'stretch_earnout': stretch_earnout,
            'total_max_payment': base_earnout + stretch_earnout,
            'years_to_measurement': years_to_measurement,
            'scenarios': {
                'base_achieved': {
                    'payment': base_earnout,
                    'probability': base_probability * 100,
                    'expected_value': base_expected
                },
                'stretch_achieved': {
                    'payment': base_earnout + stretch_earnout,
                    'probability': stretch_probability * 100,
                    'expected_value': stretch_expected
                },
                'neither_achieved': {
                    'payment': 0,
                    'probability': neither_prob * 100,
                    'expected_value': 0
                }
            },
            'total_expected_value': total_expected,
            'discount_rate': self.discount_rate * 100
        }

    def cvr_sensitivity_analysis(self, base_valuation: Dict[str, Any],
                                 probability_scenarios: List[float],
                                 timing_scenarios: List[float]) -> Dict[str, Any]:
        """Sensitivity analysis for CVR valuation"""

        base_payment = base_valuation.get('payment_if_approved') or base_valuation.get('case_value', 0)
        base_timing = base_valuation.get('expected_decision_years') or base_valuation.get('expected_resolution_years', 1)

        results = []

        for prob in probability_scenarios:
            for timing in timing_scenarios:
                pv = base_payment / ((1 + self.discount_rate) ** timing)
                expected = pv * prob

                results.append({
                    'probability': prob * 100,
                    'timing_years': timing,
                    'present_value': pv,
                    'expected_value': expected
                })

        return {
            'base_valuation': base_valuation,
            'sensitivity_matrix': results,
            'value_range': {
                'min': min(r['expected_value'] for r in results),
                'max': max(r['expected_value'] for r in results),
                'base': base_valuation.get('total_expected_value', 0)
            }
        }

    def compare_cvr_vs_cash(self, cash_alternative: float,
                           cvr_expected_value: float,
                           cvr_max_value: float,
                           target_shareholder_discount_rate: float = 0.15) -> Dict[str, Any]:
        """Compare CVR to cash alternative from target shareholder perspective"""

        cvr_certainty_equivalent = cvr_expected_value / (1 + target_shareholder_discount_rate)

        cash_superiority = cash_alternative > cvr_certainty_equivalent

        upside_scenario = cvr_max_value - cash_alternative
        downside_risk = cash_alternative - cvr_expected_value

        return {
            'cash_alternative': cash_alternative,
            'cvr_expected_value': cvr_expected_value,
            'cvr_max_value': cvr_max_value,
            'cvr_certainty_equivalent': cvr_certainty_equivalent,
            'cash_preferred': cash_superiority,
            'upside_potential': upside_scenario,
            'downside_risk': downside_risk,
            'risk_reward_ratio': upside_scenario / downside_risk if downside_risk > 0 else float('inf')
        }

def main():
    """CLI entry point - outputs JSON for Tauri integration"""
    import sys
    import json

    if len(sys.argv) < 2:
        result = {"success": False, "error": "No command specified"}
        print(json.dumps(result))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "cvr":
            if len(sys.argv) < 3:
                raise ValueError("CVR parameters required")

            # Rust sends: "cvr" cvr_type cvr_params_json
            # OR legacy: "cvr" cvr_params_json (single JSON arg)
            # Detect format: if sys.argv[2] is valid JSON dict, it's legacy single-arg
            # If it's a plain string (e.g. "milestone"), sys.argv[3] has the JSON params
            try:
                maybe_params = json.loads(sys.argv[2])
                if isinstance(maybe_params, dict):
                    # Legacy format: single JSON arg containing all params
                    params = maybe_params
                    cvr_type_str = params.get('cvr_type', 'regulatory')
                else:
                    raise ValueError("Not a dict")
            except (json.JSONDecodeError, ValueError, TypeError):
                # New format: sys.argv[2] is cvr_type string, sys.argv[3] is JSON params
                cvr_type_str = sys.argv[2]
                if len(sys.argv) > 3:
                    params = json.loads(sys.argv[3])
                else:
                    params = {}

            discount_rate = float(params.get('discount_rate', 0.12))
            risk_free_rate = float(params.get('risk_free_rate', 0.04))
            cvr = CVRValuation(discount_rate=discount_rate, risk_free_rate=risk_free_rate)

            if cvr_type_str == "milestone":
                triggers_data = params.get('triggers', [])
                triggers = []
                for t in triggers_data:
                    triggers.append(CVRTrigger(
                        trigger_type=CVRType(t.get('trigger_type', 'milestone')),
                        description=t.get('description', ''),
                        payment_if_triggered=float(t.get('payment_if_triggered', t.get('payment', 0))),
                        probability=float(t.get('probability', 0.5)),
                        expected_timing_years=float(t.get('expected_timing_years', t.get('timing_years', 2)))
                    ))
                if not triggers:
                    # Fallback: build single trigger from flat params
                    triggers.append(CVRTrigger(
                        trigger_type=CVRType.MILESTONE,
                        description=params.get('description', 'Milestone CVR'),
                        payment_if_triggered=float(params.get('payment_if_triggered', params.get('payout_per_share', 0))),
                        probability=float(params.get('probability', 0.5)),
                        expected_timing_years=float(params.get('expected_timing_years', params.get('expected_timeline_years', 2)))
                    ))
                analysis = cvr.value_milestone_cvr(triggers)

            elif cvr_type_str == "regulatory":
                analysis = cvr.value_regulatory_cvr(
                    payment_if_approved=float(params.get('payment_if_approved', params.get('payout_per_share', 0))),
                    approval_probability=float(params.get('approval_probability', params.get('probability', 0.5))),
                    expected_decision_years=float(params.get('expected_decision_years', params.get('expected_timeline_years', 2))),
                    appeal_possible=bool(params.get('appeal_possible', False)),
                    appeal_probability=float(params.get('appeal_probability', 0)),
                    appeal_delay_years=float(params.get('appeal_delay_years', 0))
                )

            elif cvr_type_str == "earnout":
                analysis = cvr.value_earnout_cvr(
                    base_earnout=float(params.get('base_earnout', 0)),
                    stretch_earnout=float(params.get('stretch_earnout', 0)),
                    base_probability=float(params.get('base_probability', 0.5)),
                    stretch_probability=float(params.get('stretch_probability', 0.2)),
                    years_to_measurement=int(params.get('years_to_measurement', 2))
                )

            elif cvr_type_str == "litigation":
                analysis = cvr.value_litigation_cvr(
                    case_value=float(params.get('case_value', 0)),
                    win_probability=float(params.get('win_probability', 0.5)),
                    expected_resolution_years=float(params.get('expected_resolution_years', 2)),
                    legal_costs=float(params.get('legal_costs', 0)),
                    settlement_probability=float(params.get('settlement_probability', 0)),
                    settlement_amount=float(params.get('settlement_amount', 0))
                )

            elif cvr_type_str == "commodity_price":
                analysis = cvr.value_commodity_price_cvr(
                    payment_schedule=params.get('payment_schedule', []),
                    current_price=float(params.get('current_price', 0)),
                    price_volatility=float(params.get('price_volatility', 0.3)),
                    years_to_maturity=float(params.get('years_to_maturity', 2))
                )

            else:
                # Default to regulatory for backwards compatibility
                analysis = cvr.value_regulatory_cvr(
                    payment_if_approved=float(params.get('payment_if_approved', params.get('payout_per_share', 0))),
                    approval_probability=float(params.get('approval_probability', params.get('probability', 0.5))),
                    expected_decision_years=float(params.get('expected_decision_years', params.get('expected_timeline_years', 2)))
                )

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
