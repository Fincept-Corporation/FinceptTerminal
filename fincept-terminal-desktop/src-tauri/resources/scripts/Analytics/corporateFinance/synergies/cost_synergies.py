"""Cost Synergy Analysis"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
from enum import Enum
import numpy as np

class CostSynergyType(Enum):
    HEADCOUNT_REDUCTION = "headcount_reduction"
    FACILITIES_CONSOLIDATION = "facilities_consolidation"
    PROCUREMENT = "procurement"
    IT_SYSTEMS = "it_systems"
    VENDOR_RATIONALIZATION = "vendor_rationalization"
    SHARED_SERVICES = "shared_services"
    MANUFACTURING = "manufacturing"
    R_AND_D = "r_and_d"

@dataclass
class CostReduction:
    category: CostSynergyType
    description: str
    annual_savings: float
    one_time_cost: float
    ramp_years: int

class CostSynergyAnalyzer:
    """Analyze and quantify cost synergies in M&A transactions"""

    def __init__(self, tax_rate: float = 0.25,
                 cost_of_capital: float = 0.10):
        self.tax_rate = tax_rate
        self.cost_of_capital = cost_of_capital

    def calculate_headcount_synergy(self, duplicate_roles: int,
                                    average_loaded_cost: float,
                                    severance_multiple: float = 1.5,
                                    ramp_years: int = 2) -> Dict[str, Any]:
        """Calculate headcount reduction synergy"""

        annual_savings = duplicate_roles * average_loaded_cost

        one_time_cost = duplicate_roles * average_loaded_cost * severance_multiple

        yearly_cash_flows = []
        for year in range(0, 7):
            if year == 0:
                cash_flow = -one_time_cost
            elif year <= ramp_years:
                realization = year / ramp_years
                savings = annual_savings * realization
                cash_flow = savings * (1 - self.tax_rate)
            else:
                cash_flow = annual_savings * (1 - self.tax_rate)

            pv = cash_flow / ((1 + self.cost_of_capital) ** year)

            yearly_cash_flows.append({
                'year': year,
                'savings': cash_flow if year > 0 else 0,
                'one_time_cost': one_time_cost if year == 0 else 0,
                'net_cash_flow': cash_flow,
                'present_value': pv
            })

        total_pv = sum(y['present_value'] for y in yearly_cash_flows)

        payback_cumulative = 0
        payback_years = None
        for y in yearly_cash_flows:
            payback_cumulative += y['net_cash_flow']
            if payback_cumulative > 0 and payback_years is None:
                payback_years = y['year']

        return {
            'synergy_type': 'headcount_reduction',
            'duplicate_roles': duplicate_roles,
            'average_loaded_cost': average_loaded_cost,
            'annual_savings': annual_savings,
            'one_time_severance_cost': one_time_cost,
            'ramp_years': ramp_years,
            'yearly_projections': yearly_cash_flows,
            'npv': total_pv,
            'payback_period_years': payback_years,
            'roi': (total_pv / one_time_cost * 100) if one_time_cost > 0 else 0
        }

    def calculate_facilities_synergy(self, facilities_to_close: int,
                                    annual_cost_per_facility: float,
                                    closure_cost_per_facility: float,
                                    lease_termination_cost: float,
                                    ramp_years: int = 2) -> Dict[str, Any]:
        """Calculate facilities consolidation synergy"""

        annual_savings = facilities_to_close * annual_cost_per_facility

        one_time_cost = (facilities_to_close * closure_cost_per_facility) + lease_termination_cost

        yearly_cash_flows = []
        for year in range(0, 7):
            if year == 0:
                cash_flow = -one_time_cost
            elif year <= ramp_years:
                realization = year / ramp_years
                savings = annual_savings * realization
                cash_flow = savings * (1 - self.tax_rate)
            else:
                cash_flow = annual_savings * (1 - self.tax_rate)

            pv = cash_flow / ((1 + self.cost_of_capital) ** year)

            yearly_cash_flows.append({
                'year': year,
                'net_cash_flow': cash_flow,
                'present_value': pv
            })

        total_pv = sum(y['present_value'] for y in yearly_cash_flows)

        return {
            'synergy_type': 'facilities_consolidation',
            'facilities_to_close': facilities_to_close,
            'annual_savings': annual_savings,
            'total_one_time_cost': one_time_cost,
            'ramp_years': ramp_years,
            'yearly_projections': yearly_cash_flows,
            'npv': total_pv
        }

    def calculate_procurement_synergy(self, combined_spend: float,
                                     volume_discount: float,
                                     supplier_rationalization_benefit: float,
                                     implementation_cost: float,
                                     ramp_years: int = 3) -> Dict[str, Any]:
        """Calculate procurement and purchasing synergy"""

        volume_savings = combined_spend * volume_discount
        rationalization_savings = combined_spend * supplier_rationalization_benefit

        total_annual_savings = volume_savings + rationalization_savings

        yearly_cash_flows = []
        for year in range(0, 7):
            if year == 0:
                cash_flow = -implementation_cost
            elif year <= ramp_years:
                realization = year / ramp_years
                savings = total_annual_savings * realization
                cash_flow = savings * (1 - self.tax_rate)
            else:
                cash_flow = total_annual_savings * (1 - self.tax_rate)

            pv = cash_flow / ((1 + self.cost_of_capital) ** year)

            yearly_cash_flows.append({
                'year': year,
                'net_cash_flow': cash_flow,
                'present_value': pv
            })

        total_pv = sum(y['present_value'] for y in yearly_cash_flows)

        return {
            'synergy_type': 'procurement',
            'combined_spend': combined_spend,
            'volume_discount': volume_discount * 100,
            'volume_savings': volume_savings,
            'rationalization_savings': rationalization_savings,
            'total_annual_savings': total_annual_savings,
            'implementation_cost': implementation_cost,
            'yearly_projections': yearly_cash_flows,
            'npv': total_pv
        }

    def calculate_it_systems_synergy(self, duplicate_systems_cost: float,
                                    migration_cost: float,
                                    ongoing_maintenance_reduction: float,
                                    migration_years: int = 2) -> Dict[str, Any]:
        """Calculate IT systems consolidation synergy"""

        annual_savings = duplicate_systems_cost + ongoing_maintenance_reduction

        yearly_cash_flows = []
        for year in range(0, 7):
            if year == 0:
                cash_flow = -migration_cost * 0.5
            elif year <= migration_years:
                migration_spend = -migration_cost * (0.5 / migration_years)
                realization = year / migration_years
                savings = annual_savings * realization
                cash_flow = (savings * (1 - self.tax_rate)) + migration_spend
            else:
                cash_flow = annual_savings * (1 - self.tax_rate)

            pv = cash_flow / ((1 + self.cost_of_capital) ** year)

            yearly_cash_flows.append({
                'year': year,
                'net_cash_flow': cash_flow,
                'present_value': pv
            })

        total_pv = sum(y['present_value'] for y in yearly_cash_flows)

        return {
            'synergy_type': 'it_systems',
            'duplicate_systems_cost': duplicate_systems_cost,
            'maintenance_reduction': ongoing_maintenance_reduction,
            'total_annual_savings': annual_savings,
            'migration_cost': migration_cost,
            'migration_years': migration_years,
            'yearly_projections': yearly_cash_flows,
            'npv': total_pv
        }

    def calculate_shared_services_synergy(self, functions: List[Dict[str, float]],
                                         consolidation_rate: float,
                                         setup_cost: float,
                                         ramp_years: int = 3) -> Dict[str, Any]:
        """Calculate shared services consolidation synergy"""

        function_analysis = []
        total_savings = 0

        for func in functions:
            func_savings = func['annual_cost'] * consolidation_rate
            total_savings += func_savings

            function_analysis.append({
                'function': func['name'],
                'current_cost': func['annual_cost'],
                'consolidation_rate': consolidation_rate * 100,
                'annual_savings': func_savings
            })

        yearly_cash_flows = []
        for year in range(0, 7):
            if year == 0:
                cash_flow = -setup_cost
            elif year <= ramp_years:
                realization = year / ramp_years
                savings = total_savings * realization
                cash_flow = savings * (1 - self.tax_rate)
            else:
                cash_flow = total_savings * (1 - self.tax_rate)

            pv = cash_flow / ((1 + self.cost_of_capital) ** year)

            yearly_cash_flows.append({
                'year': year,
                'net_cash_flow': cash_flow,
                'present_value': pv
            })

        total_pv = sum(y['present_value'] for y in yearly_cash_flows)

        return {
            'synergy_type': 'shared_services',
            'functions': function_analysis,
            'total_annual_savings': total_savings,
            'setup_cost': setup_cost,
            'ramp_years': ramp_years,
            'yearly_projections': yearly_cash_flows,
            'npv': total_pv
        }

    def calculate_manufacturing_synergy(self, production_volume: float,
                                       unit_cost_reduction: float,
                                       plant_closure_savings: float,
                                       plant_closure_cost: float,
                                       capacity_utilization_improvement: float,
                                       ramp_years: int = 3) -> Dict[str, Any]:
        """Calculate manufacturing and operations synergy"""

        scale_savings = production_volume * unit_cost_reduction
        facility_savings = plant_closure_savings
        utilization_savings = production_volume * capacity_utilization_improvement

        total_annual_savings = scale_savings + facility_savings + utilization_savings

        yearly_cash_flows = []
        for year in range(0, 7):
            if year == 0:
                cash_flow = -plant_closure_cost
            elif year <= ramp_years:
                realization = year / ramp_years
                savings = total_annual_savings * realization
                cash_flow = savings * (1 - self.tax_rate)
            else:
                cash_flow = total_annual_savings * (1 - self.tax_rate)

            pv = cash_flow / ((1 + self.cost_of_capital) ** year)

            yearly_cash_flows.append({
                'year': year,
                'net_cash_flow': cash_flow,
                'present_value': pv
            })

        total_pv = sum(y['present_value'] for y in yearly_cash_flows)

        return {
            'synergy_type': 'manufacturing',
            'production_volume': production_volume,
            'scale_savings': scale_savings,
            'facility_savings': facility_savings,
            'utilization_savings': utilization_savings,
            'total_annual_savings': total_annual_savings,
            'plant_closure_cost': plant_closure_cost,
            'yearly_projections': yearly_cash_flows,
            'npv': total_pv
        }

    def comprehensive_cost_synergy(self, cost_reductions: List[CostReduction]) -> Dict[str, Any]:
        """Aggregate all cost synergy opportunities"""

        synergy_breakdown = []
        total_npv = 0
        total_one_time_costs = 0
        total_annual_savings = 0

        for reduction in cost_reductions:
            yearly_projections = []

            for year in range(0, 7):
                if year == 0:
                    cash_flow = -reduction.one_time_cost
                elif year <= reduction.ramp_years:
                    realization = year / reduction.ramp_years
                    savings = reduction.annual_savings * realization
                    cash_flow = savings * (1 - self.tax_rate)
                else:
                    cash_flow = reduction.annual_savings * (1 - self.tax_rate)

                pv = cash_flow / ((1 + self.cost_of_capital) ** year)

                yearly_projections.append({
                    'year': year,
                    'net_cash_flow': cash_flow,
                    'present_value': pv
                })

            reduction_npv = sum(y['present_value'] for y in yearly_projections)
            total_npv += reduction_npv
            total_one_time_costs += reduction.one_time_cost
            total_annual_savings += reduction.annual_savings

            synergy_breakdown.append({
                'category': reduction.category.value,
                'description': reduction.description,
                'annual_savings': reduction.annual_savings,
                'one_time_cost': reduction.one_time_cost,
                'ramp_years': reduction.ramp_years,
                'npv': reduction_npv,
                'roi': (reduction_npv / reduction.one_time_cost * 100) if reduction.one_time_cost > 0 else float('inf'),
                'yearly_projections': yearly_projections
            })

        synergy_breakdown.sort(key=lambda x: x['npv'], reverse=True)

        return {
            'total_cost_synergy_npv': total_npv,
            'total_annual_savings_run_rate': total_annual_savings,
            'total_one_time_costs': total_one_time_costs,
            'blended_roi': (total_npv / total_one_time_costs * 100) if total_one_time_costs > 0 else float('inf'),
            'synergy_breakdown': synergy_breakdown,
            'cost_of_capital': self.cost_of_capital * 100,
            'tax_rate': self.tax_rate * 100
        }

    def synergy_realization_tracker(self, planned_savings: float,
                                    actual_savings_by_quarter: List[float]) -> Dict[str, Any]:
        """Track actual synergy realization vs plan"""

        quarters = len(actual_savings_by_quarter)
        years = quarters / 4

        cumulative_actual = sum(actual_savings_by_quarter)
        cumulative_plan = planned_savings * years

        realization_rate = (cumulative_actual / cumulative_plan * 100) if cumulative_plan > 0 else 0

        quarterly_variance = [
            actual - (planned_savings / 4)
            for actual in actual_savings_by_quarter
        ]

        return {
            'planned_annual_savings': planned_savings,
            'quarters_tracked': quarters,
            'cumulative_actual': cumulative_actual,
            'cumulative_plan': cumulative_plan,
            'realization_rate': realization_rate,
            'quarterly_actual': actual_savings_by_quarter,
            'quarterly_variance': quarterly_variance,
            'on_track': realization_rate >= 85.0
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
        if command == "cost_synergy":
            if len(sys.argv) < 6:
                raise ValueError("Duplicate roles, average loaded cost, severance multiple, and ramp years required")

            duplicate_roles = int(sys.argv[2])
            average_loaded_cost = float(sys.argv[3])
            severance_multiple = float(sys.argv[4])
            ramp_years = int(sys.argv[5])

            analyzer = CostSynergyAnalyzer(tax_rate=0.25, cost_of_capital=0.10)

            analysis = analyzer.calculate_headcount_synergy(
                duplicate_roles=duplicate_roles,
                average_loaded_cost=average_loaded_cost,
                severance_multiple=severance_multiple,
                ramp_years=ramp_years
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
