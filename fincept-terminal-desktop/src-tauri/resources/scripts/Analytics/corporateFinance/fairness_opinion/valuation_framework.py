"""Fairness Opinion Valuation Framework"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
from enum import Enum
import numpy as np

class ValuationMethod(Enum):
    DCF = "dcf"
    PRECEDENT_TRANSACTIONS = "precedent_transactions"
    TRADING_COMPS = "trading_comps"
    LBO_ANALYSIS = "lbo_analysis"
    PREMIUMS_PAID = "premiums_paid"
    REPLACEMENT_COST = "replacement_cost"

@dataclass
class ValuationRange:
    method: ValuationMethod
    low: float
    high: float
    midpoint: float
    weight: float

class FairnessOpinionFramework:
    """Framework for investment banking fairness opinions"""

    def __init__(self, company_name: str, offer_price: float):
        self.company_name = company_name
        self.offer_price = offer_price

    def calculate_dcf_valuation(self, free_cash_flows: List[float],
                               terminal_value: float,
                               wacc_low: float,
                               wacc_high: float) -> Dict[str, Any]:
        """Calculate DCF valuation range"""

        def npv(fcf: List[float], tv: float, discount_rate: float) -> float:
            pv_fcf = sum(cf / ((1 + discount_rate) ** (i + 1)) for i, cf in enumerate(fcf))
            pv_tv = tv / ((1 + discount_rate) ** len(fcf))
            return pv_fcf + pv_tv

        low_value = npv(free_cash_flows, terminal_value, wacc_high)
        high_value = npv(free_cash_flows, terminal_value, wacc_low)
        mid_value = npv(free_cash_flows, terminal_value, (wacc_low + wacc_high) / 2)

        return {
            'method': ValuationMethod.DCF.value,
            'valuation_range': {
                'low': low_value,
                'high': high_value,
                'midpoint': mid_value
            },
            'assumptions': {
                'wacc_low': wacc_low * 100,
                'wacc_high': wacc_high * 100,
                'terminal_value': terminal_value,
                'projection_years': len(free_cash_flows)
            },
            'fcf_projections': free_cash_flows
        }

    def calculate_trading_comps_valuation(self, subject_metric: float,
                                         multiples_low: float,
                                         multiples_high: float,
                                         metric_name: str = "EBITDA") -> Dict[str, Any]:
        """Calculate trading comparables valuation range"""

        low_value = subject_metric * multiples_low
        high_value = subject_metric * multiples_high
        mid_value = subject_metric * ((multiples_low + multiples_high) / 2)

        return {
            'method': ValuationMethod.TRADING_COMPS.value,
            'valuation_range': {
                'low': low_value,
                'high': high_value,
                'midpoint': mid_value
            },
            'assumptions': {
                'metric_name': metric_name,
                'subject_metric': subject_metric,
                'multiple_range_low': multiples_low,
                'multiple_range_high': multiples_high
            }
        }

    def calculate_precedent_transactions_valuation(self, subject_metric: float,
                                                   multiples_low: float,
                                                   multiples_high: float,
                                                   metric_name: str = "EBITDA") -> Dict[str, Any]:
        """Calculate precedent transactions valuation range"""

        low_value = subject_metric * multiples_low
        high_value = subject_metric * multiples_high
        mid_value = subject_metric * ((multiples_low + multiples_high) / 2)

        return {
            'method': ValuationMethod.PRECEDENT_TRANSACTIONS.value,
            'valuation_range': {
                'low': low_value,
                'high': high_value,
                'midpoint': mid_value
            },
            'assumptions': {
                'metric_name': metric_name,
                'subject_metric': subject_metric,
                'multiple_range_low': multiples_low,
                'multiple_range_high': multiples_high
            }
        }

    def calculate_lbo_analysis(self, entry_price: float,
                               exit_multiple_low: float,
                               exit_multiple_high: float,
                               ebitda_at_exit: float,
                               years_to_exit: int = 5,
                               target_irr: float = 0.20) -> Dict[str, Any]:
        """Calculate implied valuation from LBO analysis"""

        exit_value_low = ebitda_at_exit * exit_multiple_low
        exit_value_high = ebitda_at_exit * exit_multiple_high

        implied_value_low = exit_value_low / ((1 + target_irr) ** years_to_exit)
        implied_value_high = exit_value_high / ((1 + target_irr) ** years_to_exit)
        implied_value_mid = (implied_value_low + implied_value_high) / 2

        return {
            'method': ValuationMethod.LBO_ANALYSIS.value,
            'valuation_range': {
                'low': implied_value_low,
                'high': implied_value_high,
                'midpoint': implied_value_mid
            },
            'assumptions': {
                'exit_multiple_low': exit_multiple_low,
                'exit_multiple_high': exit_multiple_high,
                'ebitda_at_exit': ebitda_at_exit,
                'years_to_exit': years_to_exit,
                'target_irr': target_irr * 100
            }
        }

    def calculate_premiums_analysis(self, unaffected_price: float,
                                    premium_low: float,
                                    premium_high: float) -> Dict[str, Any]:
        """Calculate valuation based on acquisition premiums"""

        low_value = unaffected_price * (1 + premium_low)
        high_value = unaffected_price * (1 + premium_high)
        mid_value = unaffected_price * (1 + (premium_low + premium_high) / 2)

        return {
            'method': ValuationMethod.PREMIUMS_PAID.value,
            'valuation_range': {
                'low': low_value,
                'high': high_value,
                'midpoint': mid_value
            },
            'assumptions': {
                'unaffected_price': unaffected_price,
                'premium_low': premium_low * 100,
                'premium_high': premium_high * 100
            }
        }

    def weighted_valuation_summary(self, valuation_ranges: List[ValuationRange]) -> Dict[str, Any]:
        """Create weighted valuation summary across all methods"""

        total_weight = sum(v.weight for v in valuation_ranges)

        if abs(total_weight - 1.0) > 0.01:
            raise ValueError(f"Weights must sum to 1.0, got {total_weight}")

        weighted_low = sum(v.low * v.weight for v in valuation_ranges)
        weighted_high = sum(v.high * v.weight for v in valuation_ranges)
        weighted_mid = sum(v.midpoint * v.weight for v in valuation_ranges)

        method_details = []
        for v in valuation_ranges:
            method_details.append({
                'method': v.method.value,
                'low': v.low,
                'high': v.high,
                'midpoint': v.midpoint,
                'weight': v.weight * 100,
                'contribution_to_weighted_mid': v.midpoint * v.weight
            })

        implied_premium_low = ((self.offer_price - weighted_low) / weighted_low * 100) if weighted_low > 0 else 0
        implied_premium_high = ((self.offer_price - weighted_high) / weighted_high * 100) if weighted_high > 0 else 0
        implied_premium_mid = ((self.offer_price - weighted_mid) / weighted_mid * 100) if weighted_mid > 0 else 0

        within_range = weighted_low <= self.offer_price <= weighted_high

        return {
            'company_name': self.company_name,
            'offer_price': self.offer_price,
            'weighted_valuation_range': {
                'low': weighted_low,
                'high': weighted_high,
                'midpoint': weighted_mid
            },
            'implied_premium': {
                'to_low': implied_premium_low,
                'to_high': implied_premium_high,
                'to_midpoint': implied_premium_mid
            },
            'offer_within_range': within_range,
            'methods_used': method_details,
            'position_in_range': ((self.offer_price - weighted_low) / (weighted_high - weighted_low) * 100) if (weighted_high - weighted_low) > 0 else 0
        }

    def football_field_analysis(self, valuation_ranges: List[ValuationRange]) -> Dict[str, Any]:
        """Create football field valuation chart data"""

        sorted_ranges = sorted(valuation_ranges, key=lambda x: x.midpoint)

        overall_low = min(v.low for v in valuation_ranges)
        overall_high = max(v.high for v in valuation_ranges)

        range_data = []
        for v in sorted_ranges:
            range_data.append({
                'method': v.method.value,
                'low': v.low,
                'high': v.high,
                'midpoint': v.midpoint,
                'range_width': v.high - v.low,
                'offer_within': v.low <= self.offer_price <= v.high
            })

        return {
            'valuation_ranges': range_data,
            'overall_range': {
                'low': overall_low,
                'high': overall_high
            },
            'offer_price': self.offer_price,
            'offer_position': {
                'below_all_ranges': self.offer_price < overall_low,
                'above_all_ranges': self.offer_price > overall_high,
                'within_at_least_one': any(r['offer_within'] for r in range_data)
            }
        }

    def sensitivity_analysis(self, base_valuation: float,
                            key_assumptions: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Perform sensitivity analysis on key valuation assumptions"""

        sensitivity_results = []

        for assumption in key_assumptions:
            param_name = assumption['parameter']
            base_value = assumption['base_value']
            scenarios = assumption['scenarios']

            scenario_results = []
            for scenario in scenarios:
                variance_pct = scenario['variance_pct']
                adjusted_value = base_value * (1 + variance_pct / 100)

                valuation_impact_pct = scenario.get('valuation_impact_pct', variance_pct)
                adjusted_valuation = base_valuation * (1 + valuation_impact_pct / 100)

                scenario_results.append({
                    'scenario': scenario['name'],
                    'parameter_value': adjusted_value,
                    'variance_pct': variance_pct,
                    'valuation': adjusted_valuation,
                    'valuation_change_pct': valuation_impact_pct
                })

            sensitivity_results.append({
                'parameter': param_name,
                'base_value': base_value,
                'scenarios': scenario_results
            })

        return {
            'base_valuation': base_valuation,
            'sensitivity_analysis': sensitivity_results
        }

    def fairness_determination(self, weighted_valuation: Dict[str, Any],
                              qualitative_factors: List[str]) -> Dict[str, Any]:
        """Make fairness determination based on quantitative and qualitative analysis"""

        offer_price = weighted_valuation['offer_price']
        low = weighted_valuation['weighted_valuation_range']['low']
        high = weighted_valuation['weighted_valuation_range']['high']
        mid = weighted_valuation['weighted_valuation_range']['midpoint']

        within_range = weighted_valuation['offer_within_range']
        position = weighted_valuation['position_in_range']

        if within_range:
            if position >= 40 and position <= 60:
                quantitative_conclusion = "fair"
            elif position < 40:
                quantitative_conclusion = "fair_low_end"
            else:
                quantitative_conclusion = "fair_high_end"
        elif offer_price < low:
            shortfall_pct = ((low - offer_price) / low * 100)
            if shortfall_pct < 10:
                quantitative_conclusion = "marginally_below_range"
            else:
                quantitative_conclusion = "materially_below_range"
        else:
            premium_pct = ((offer_price - high) / high * 100)
            if premium_pct < 10:
                quantitative_conclusion = "marginally_above_range"
            else:
                quantitative_conclusion = "materially_above_range"

        is_fair = quantitative_conclusion in ["fair", "fair_low_end", "fair_high_end", "marginally_below_range", "marginally_above_range"]

        return {
            'offer_price': offer_price,
            'valuation_range': {
                'low': low,
                'high': high,
                'midpoint': mid
            },
            'quantitative_analysis': {
                'within_range': within_range,
                'position_in_range_pct': position,
                'conclusion': quantitative_conclusion
            },
            'qualitative_factors_considered': qualitative_factors,
            'fairness_conclusion': {
                'is_fair': is_fair,
                'conclusion': "fair from a financial point of view" if is_fair else "not fair from a financial point of view"
            }
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
        if command == "fairness":
            if len(sys.argv) < 5:
                raise ValueError("Valuation methods, offer price, and qualitative factors required")

            valuation_methods = json.loads(sys.argv[2])
            offer_price = float(sys.argv[3])
            qualitative_factors = json.loads(sys.argv[4])

            # valuation_methods can be a dict with 'methods' key, or a list directly
            company_name = 'Target Company'
            if isinstance(valuation_methods, dict):
                company_name = valuation_methods.get('company_name', 'Target Company')
                methods_list = valuation_methods.get('methods', [])
            elif isinstance(valuation_methods, list):
                methods_list = valuation_methods
            else:
                methods_list = []

            framework = FairnessOpinionFramework(
                company_name=company_name,
                offer_price=offer_price
            )

            # Convert valuation methods data to ValuationRange objects
            valuation_ranges = []
            for method_data in methods_list:
                method_name = method_data.get('method', 'DCF').upper().replace(' ', '_')
                val_range = ValuationRange(
                    method=ValuationMethod[method_name],
                    low=method_data['low'],
                    high=method_data['high'],
                    midpoint=method_data.get('midpoint', method_data.get('mid', (method_data['low'] + method_data['high']) / 2)),
                    weight=method_data.get('weight', 1.0)
                )
                valuation_ranges.append(val_range)

            # Auto-normalize weights if they don't sum to 1.0
            total_weight = sum(v.weight for v in valuation_ranges)
            if total_weight > 0 and abs(total_weight - 1.0) > 0.01:
                for v in valuation_ranges:
                    v.weight = v.weight / total_weight

            analysis = framework.weighted_valuation_summary(valuation_ranges)

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
