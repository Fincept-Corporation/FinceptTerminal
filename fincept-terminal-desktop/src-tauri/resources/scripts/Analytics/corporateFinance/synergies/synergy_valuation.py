"""Comprehensive Synergy Valuation"""
from typing import Dict, Any, List, Optional
import numpy as np

class SynergyValuation:
    """Comprehensive synergy valuation combining revenue and cost synergies"""

    def __init__(self, wacc: float = 0.10,
                 terminal_growth_rate: float = 0.02,
                 tax_rate: float = 0.25):
        self.wacc = wacc
        self.terminal_growth_rate = terminal_growth_rate
        self.tax_rate = tax_rate

    def value_synergies_dcf(self, annual_revenue_synergies: List[float],
                           annual_cost_synergies: List[float],
                           integration_costs_by_year: List[float],
                           projection_years: int = 10) -> Dict[str, Any]:
        """Value synergies using DCF methodology"""

        if len(annual_revenue_synergies) < projection_years:
            last_revenue = annual_revenue_synergies[-1] if annual_revenue_synergies else 0
            annual_revenue_synergies.extend([last_revenue] * (projection_years - len(annual_revenue_synergies)))

        if len(annual_cost_synergies) < projection_years:
            last_cost = annual_cost_synergies[-1] if annual_cost_synergies else 0
            annual_cost_synergies.extend([last_cost] * (projection_years - len(annual_cost_synergies)))

        if len(integration_costs_by_year) < projection_years:
            integration_costs_by_year.extend([0] * (projection_years - len(integration_costs_by_year)))

        yearly_cash_flows = []
        total_pv = 0

        for year in range(1, projection_years + 1):
            idx = year - 1

            revenue_synergy = annual_revenue_synergies[idx]
            cost_synergy = annual_cost_synergies[idx]
            integration_cost = integration_costs_by_year[idx]

            gross_synergy = revenue_synergy + cost_synergy
            after_tax_synergy = gross_synergy * (1 - self.tax_rate)
            net_cash_flow = after_tax_synergy - integration_cost

            discount_factor = (1 + self.wacc) ** year
            pv = net_cash_flow / discount_factor

            total_pv += pv

            yearly_cash_flows.append({
                'year': year,
                'revenue_synergy': revenue_synergy,
                'cost_synergy': cost_synergy,
                'integration_cost': integration_cost,
                'gross_synergy': gross_synergy,
                'after_tax_synergy': after_tax_synergy,
                'net_cash_flow': net_cash_flow,
                'discount_factor': discount_factor,
                'present_value': pv
            })

        terminal_year_synergy = gross_synergy * (1 - self.tax_rate)
        terminal_value = terminal_year_synergy * (1 + self.terminal_growth_rate) / (self.wacc - self.terminal_growth_rate)
        terminal_pv = terminal_value / ((1 + self.wacc) ** projection_years)

        total_synergy_value = total_pv + terminal_pv

        return {
            'explicit_forecast_pv': total_pv,
            'terminal_value': terminal_value,
            'terminal_value_pv': terminal_pv,
            'total_synergy_value': total_synergy_value,
            'terminal_value_pct': (terminal_pv / total_synergy_value * 100) if total_synergy_value > 0 else 0,
            'yearly_projections': yearly_cash_flows,
            'valuation_assumptions': {
                'wacc': self.wacc * 100,
                'terminal_growth_rate': self.terminal_growth_rate * 100,
                'tax_rate': self.tax_rate * 100,
                'projection_years': projection_years
            }
        }

    def calculate_synergy_multiple(self, total_synergy_value: float,
                                   purchase_price: float,
                                   standalone_target_value: Optional[float] = None) -> Dict[str, Any]:
        """Calculate synergy contribution to deal value"""

        if standalone_target_value:
            premium_paid = purchase_price - standalone_target_value
            synergy_captured_by_acquirer = total_synergy_value - premium_paid
            synergy_split_acquirer = (synergy_captured_by_acquirer / total_synergy_value * 100) if total_synergy_value > 0 else 0
            synergy_split_target = 100 - synergy_split_acquirer
        else:
            premium_paid = None
            synergy_captured_by_acquirer = None
            synergy_split_acquirer = None
            synergy_split_target = None

        synergy_to_price_ratio = (total_synergy_value / purchase_price * 100) if purchase_price > 0 else 0

        return {
            'total_synergy_value': total_synergy_value,
            'purchase_price': purchase_price,
            'standalone_target_value': standalone_target_value,
            'premium_paid': premium_paid,
            'synergy_captured_by_acquirer': synergy_captured_by_acquirer,
            'synergy_split': {
                'acquirer_pct': synergy_split_acquirer,
                'target_pct': synergy_split_target
            },
            'synergy_to_price_ratio': synergy_to_price_ratio,
            'value_creation_analysis': {
                'creates_value': total_synergy_value > (premium_paid if premium_paid else 0),
                'npv_of_deal': total_synergy_value - (premium_paid if premium_paid else 0) if premium_paid else None
            }
        }

    def synergy_sensitivity_analysis(self, base_revenue_synergy: float,
                                    base_cost_synergy: float,
                                    revenue_scenarios: List[float],
                                    cost_scenarios: List[float]) -> Dict[str, Any]:
        """Two-way sensitivity analysis for synergy assumptions"""

        sensitivity_matrix = []

        for revenue_mult in revenue_scenarios:
            for cost_mult in cost_scenarios:
                revenue_synergy = base_revenue_synergy * revenue_mult
                cost_synergy = base_cost_synergy * cost_mult

                total_synergy = revenue_synergy + cost_synergy
                after_tax = total_synergy * (1 - self.tax_rate)

                perpetuity_value = after_tax / self.wacc

                sensitivity_matrix.append({
                    'revenue_multiplier': revenue_mult,
                    'cost_multiplier': cost_mult,
                    'revenue_synergy': revenue_synergy,
                    'cost_synergy': cost_synergy,
                    'total_synergy': total_synergy,
                    'synergy_value': perpetuity_value
                })

        min_value = min(s['synergy_value'] for s in sensitivity_matrix)
        max_value = max(s['synergy_value'] for s in sensitivity_matrix)
        base_case = [s for s in sensitivity_matrix if s['revenue_multiplier'] == 1.0 and s['cost_multiplier'] == 1.0][0]

        return {
            'sensitivity_matrix': sensitivity_matrix,
            'value_range': {
                'min': min_value,
                'max': max_value,
                'base': base_case['synergy_value']
            },
            'base_assumptions': {
                'revenue_synergy': base_revenue_synergy,
                'cost_synergy': base_cost_synergy
            }
        }

    def calculate_probability_adjusted_synergies(self, synergy_scenarios: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Calculate probability-weighted synergy value"""

        total_probability = sum(s['probability'] for s in synergy_scenarios)
        if abs(total_probability - 1.0) > 0.01:
            raise ValueError(f"Probabilities must sum to 1.0, got {total_probability}")

        scenario_analysis = []
        expected_value = 0

        for scenario in synergy_scenarios:
            synergy_value = scenario['synergy_value']
            probability = scenario['probability']
            weighted_value = synergy_value * probability

            expected_value += weighted_value

            scenario_analysis.append({
                'scenario_name': scenario['name'],
                'probability': probability * 100,
                'synergy_value': synergy_value,
                'weighted_value': weighted_value
            })

        values = [s['synergy_value'] for s in synergy_scenarios]
        std_dev = np.std(values)
        cv = (std_dev / expected_value * 100) if expected_value > 0 else 0

        return {
            'expected_synergy_value': expected_value,
            'scenarios': scenario_analysis,
            'value_range': {
                'min': min(values),
                'max': max(values)
            },
            'standard_deviation': std_dev,
            'coefficient_of_variation': cv
        }

    def synergy_realization_schedule(self, total_synergies: float,
                                    realization_curve: str = 'linear',
                                    full_realization_years: int = 3) -> Dict[str, Any]:
        """Model synergy realization over time"""

        yearly_realization = []

        for year in range(1, full_realization_years + 4):
            if realization_curve == 'linear':
                if year <= full_realization_years:
                    pct = year / full_realization_years
                else:
                    pct = 1.0

            elif realization_curve == 's_curve':
                if year <= full_realization_years:
                    pct = (year / full_realization_years) ** 0.7
                else:
                    pct = 1.0

            elif realization_curve == 'front_loaded':
                if year <= full_realization_years:
                    pct = 1 - (1 - year / full_realization_years) ** 2
                else:
                    pct = 1.0

            else:
                pct = year / full_realization_years if year <= full_realization_years else 1.0

            synergy_realized = total_synergies * pct

            yearly_realization.append({
                'year': year,
                'realization_pct': pct * 100,
                'synergies_realized': synergy_realized,
                'incremental_synergies': synergy_realized - (yearly_realization[-1]['synergies_realized'] if year > 1 else 0)
            })

        return {
            'total_synergies': total_synergies,
            'realization_curve': realization_curve,
            'full_realization_years': full_realization_years,
            'yearly_schedule': yearly_realization
        }

    def compare_deal_with_without_synergies(self, purchase_price: float,
                                           standalone_target_value: float,
                                           synergy_value: float,
                                           acquirer_market_cap: float) -> Dict[str, Any]:
        """Compare deal economics with and without synergies"""

        premium_paid = purchase_price - standalone_target_value
        premium_pct = (premium_paid / standalone_target_value * 100) if standalone_target_value > 0 else 0

        without_synergies_npv = -premium_paid
        with_synergies_npv = synergy_value - premium_paid

        value_creation = with_synergies_npv > 0

        implied_acquirer_value_change = with_synergies_npv
        implied_stock_impact = (implied_acquirer_value_change / acquirer_market_cap * 100) if acquirer_market_cap > 0 else 0

        return {
            'purchase_price': purchase_price,
            'standalone_target_value': standalone_target_value,
            'premium_paid': premium_paid,
            'premium_pct': premium_pct,
            'synergy_value': synergy_value,
            'without_synergies': {
                'npv': without_synergies_npv,
                'destroys_value': without_synergies_npv < 0
            },
            'with_synergies': {
                'npv': with_synergies_npv,
                'creates_value': value_creation
            },
            'synergy_impact': {
                'value_swing': with_synergies_npv - without_synergies_npv,
                'synergy_justifies_premium': synergy_value > premium_paid
            },
            'acquirer_impact': {
                'market_cap': acquirer_market_cap,
                'implied_value_change': implied_acquirer_value_change,
                'implied_stock_impact_pct': implied_stock_impact
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
        if command == "synergy_valuation":
            if len(sys.argv) < 5:
                raise ValueError("Revenue synergies, cost synergies, and integration costs required")

            revenue_synergies = json.loads(sys.argv[2])
            cost_synergies = json.loads(sys.argv[3])
            integration_costs = json.loads(sys.argv[4])

            valuator = SynergyValuation(wacc=0.10, terminal_growth_rate=0.02, tax_rate=0.25)

            analysis = valuator.value_synergies_dcf(
                annual_revenue_synergies=revenue_synergies,
                annual_cost_synergies=cost_synergies,
                integration_costs_by_year=integration_costs,
                projection_years=10
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
