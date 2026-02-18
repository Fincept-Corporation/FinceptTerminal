"""Integration Cost Analysis"""
from typing import Dict, Any, List, Optional
from dataclasses import dataclass
from enum import Enum

class IntegrationCategory(Enum):
    IT_INTEGRATION = "it_integration"
    REBRANDING = "rebranding"
    EMPLOYEE_RETENTION = "employee_retention"
    CHANGE_MANAGEMENT = "change_management"
    LEGAL_REGULATORY = "legal_regulatory"
    ADVISORS_FEES = "advisors_fees"
    FACILITIES = "facilities"
    CUSTOMER_RETENTION = "customer_retention"

@dataclass
class IntegrationCost:
    category: IntegrationCategory
    description: str
    estimated_cost: float
    timing_months: int
    is_capitalized: bool = False

class IntegrationCostAnalyzer:
    """Analyze and track M&A integration costs"""

    def __init__(self, deal_value: float):
        self.deal_value = deal_value

    def estimate_it_integration(self, systems_to_integrate: int,
                                complexity_factor: float = 1.0,
                                third_party_consulting: bool = True) -> Dict[str, Any]:
        """Estimate IT systems integration costs"""

        base_cost_per_system = 500_000
        consulting_premium = 1.3 if third_party_consulting else 1.0

        integration_cost = systems_to_integrate * base_cost_per_system * complexity_factor * consulting_premium

        data_migration_cost = integration_cost * 0.25
        testing_cost = integration_cost * 0.15
        training_cost = integration_cost * 0.10

        total_cost = integration_cost + data_migration_cost + testing_cost + training_cost

        return {
            'category': 'it_integration',
            'systems_to_integrate': systems_to_integrate,
            'complexity_factor': complexity_factor,
            'third_party_consulting': third_party_consulting,
            'base_integration_cost': integration_cost,
            'data_migration': data_migration_cost,
            'testing': testing_cost,
            'training': training_cost,
            'total_cost': total_cost,
            'estimated_duration_months': 12 + (systems_to_integrate * 2)
        }

    def estimate_rebranding_cost(self, locations: int,
                                 digital_properties: int,
                                 product_rebrand: bool,
                                 target_revenue: float) -> Dict[str, Any]:
        """Estimate rebranding and marketing integration costs"""

        physical_signage = locations * 25_000
        digital_rebrand = digital_properties * 50_000
        collateral_cost = target_revenue * 0.005

        product_cost = 0
        if product_rebrand:
            product_cost = target_revenue * 0.02

        advertising_campaign = target_revenue * 0.03

        total_cost = physical_signage + digital_rebrand + collateral_cost + product_cost + advertising_campaign

        return {
            'category': 'rebranding',
            'locations': locations,
            'digital_properties': digital_properties,
            'product_rebrand': product_rebrand,
            'physical_signage_cost': physical_signage,
            'digital_rebrand_cost': digital_rebrand,
            'marketing_collateral': collateral_cost,
            'product_rebranding': product_cost,
            'advertising_campaign': advertising_campaign,
            'total_cost': total_cost,
            'as_pct_of_revenue': (total_cost / target_revenue * 100) if target_revenue > 0 else 0
        }

    def estimate_retention_programs(self, key_employees: int,
                                    average_compensation: float,
                                    retention_bonus_pct: float = 0.50,
                                    vesting_years: int = 2) -> Dict[str, Any]:
        """Estimate employee retention program costs"""

        retention_pool = key_employees * average_compensation * retention_bonus_pct

        stay_bonus_cost = retention_pool * 0.60
        equity_grant_cost = retention_pool * 0.40

        annual_cost = retention_pool / vesting_years

        return {
            'category': 'employee_retention',
            'key_employees': key_employees,
            'average_compensation': average_compensation,
            'retention_bonus_pct': retention_bonus_pct * 100,
            'total_retention_pool': retention_pool,
            'stay_bonus_component': stay_bonus_cost,
            'equity_grant_component': equity_grant_cost,
            'vesting_years': vesting_years,
            'annual_cost': annual_cost,
            'cost_per_employee': retention_pool / key_employees if key_employees > 0 else 0
        }

    def estimate_change_management(self, total_employees: int,
                                   training_hours_per_employee: int,
                                   external_consultants: bool = True) -> Dict[str, Any]:
        """Estimate change management and training costs"""

        training_cost_per_hour = 150
        training_cost = total_employees * training_hours_per_employee * training_cost_per_hour

        communication_cost = total_employees * 100

        consultant_cost = 0
        if external_consultants:
            consultant_cost = self.deal_value * 0.005

        total_cost = training_cost + communication_cost + consultant_cost

        return {
            'category': 'change_management',
            'total_employees': total_employees,
            'training_hours_per_employee': training_hours_per_employee,
            'external_consultants': external_consultants,
            'training_cost': training_cost,
            'communication_programs': communication_cost,
            'external_consultants_cost': consultant_cost,
            'total_cost': total_cost,
            'cost_per_employee': total_cost / total_employees if total_employees > 0 else 0
        }

    def estimate_legal_regulatory(self, jurisdictions: int,
                                  regulatory_approvals_needed: int,
                                  litigation_risk: str = 'medium') -> Dict[str, Any]:
        """Estimate legal and regulatory integration costs"""

        legal_base = jurisdictions * 250_000
        regulatory_filings = regulatory_approvals_needed * 500_000

        litigation_reserves = {
            'low': 0.002,
            'medium': 0.005,
            'high': 0.010
        }

        litigation_reserve = self.deal_value * litigation_reserves.get(litigation_risk, 0.005)

        compliance_cost = jurisdictions * 100_000

        total_cost = legal_base + regulatory_filings + litigation_reserve + compliance_cost

        return {
            'category': 'legal_regulatory',
            'jurisdictions': jurisdictions,
            'regulatory_approvals': regulatory_approvals_needed,
            'litigation_risk': litigation_risk,
            'legal_base_cost': legal_base,
            'regulatory_filings': regulatory_filings,
            'litigation_reserve': litigation_reserve,
            'compliance_programs': compliance_cost,
            'total_cost': total_cost
        }

    def estimate_transaction_fees(self, investment_banker_fee_pct: float = 0.01,
                                  legal_fees: float = 5_000_000,
                                  accounting_fees: float = 2_000_000,
                                  other_advisors: float = 1_000_000) -> Dict[str, Any]:
        """Estimate transaction advisory fees"""

        ib_fee = self.deal_value * investment_banker_fee_pct

        total_cost = ib_fee + legal_fees + accounting_fees + other_advisors

        return {
            'category': 'advisors_fees',
            'investment_banking_fee': ib_fee,
            'ib_fee_percentage': investment_banker_fee_pct * 100,
            'legal_fees': legal_fees,
            'accounting_fees': accounting_fees,
            'other_advisors': other_advisors,
            'total_cost': total_cost,
            'as_pct_of_deal_value': (total_cost / self.deal_value * 100) if self.deal_value > 0 else 0
        }

    def estimate_customer_retention(self, at_risk_revenue: float,
                                    retention_program_intensity: str = 'medium') -> Dict[str, Any]:
        """Estimate customer retention program costs"""

        intensity_factors = {
            'low': 0.03,
            'medium': 0.05,
            'high': 0.08
        }

        program_cost_pct = intensity_factors.get(retention_program_intensity, 0.05)

        program_cost = at_risk_revenue * program_cost_pct

        incentive_cost = at_risk_revenue * 0.02
        relationship_mgmt = program_cost * 0.30

        total_cost = program_cost + incentive_cost + relationship_mgmt

        return {
            'category': 'customer_retention',
            'at_risk_revenue': at_risk_revenue,
            'program_intensity': retention_program_intensity,
            'retention_program_cost': program_cost,
            'customer_incentives': incentive_cost,
            'relationship_management': relationship_mgmt,
            'total_cost': total_cost,
            'as_pct_of_at_risk_revenue': (total_cost / at_risk_revenue * 100) if at_risk_revenue > 0 else 0
        }

    def comprehensive_integration_budget(self, integration_costs: List[IntegrationCost]) -> Dict[str, Any]:
        """Create comprehensive integration cost budget"""

        cost_breakdown = []
        total_cost = 0
        capitalized_total = 0
        expensed_total = 0

        by_year = {}

        for cost in integration_costs:
            year = (cost.timing_months - 1) // 12 + 1

            if year not in by_year:
                by_year[year] = 0
            by_year[year] += cost.estimated_cost

            total_cost += cost.estimated_cost

            if cost.is_capitalized:
                capitalized_total += cost.estimated_cost
            else:
                expensed_total += cost.estimated_cost

            cost_breakdown.append({
                'category': cost.category.value,
                'description': cost.description,
                'estimated_cost': cost.estimated_cost,
                'timing_months': cost.timing_months,
                'year': year,
                'is_capitalized': cost.is_capitalized,
                'treatment': 'capitalized' if cost.is_capitalized else 'expensed'
            })

        cost_breakdown.sort(key=lambda x: x['estimated_cost'], reverse=True)

        yearly_schedule = [{'year': y, 'costs': c} for y, c in sorted(by_year.items())]

        return {
            'total_integration_cost': total_cost,
            'capitalized_costs': capitalized_total,
            'expensed_costs': expensed_total,
            'as_pct_of_deal_value': (total_cost / self.deal_value * 100) if self.deal_value > 0 else 0,
            'cost_breakdown': cost_breakdown,
            'yearly_schedule': yearly_schedule,
            'peak_year': max(by_year.items(), key=lambda x: x[1])[0] if by_year else None,
            'integration_duration_months': max(c.timing_months for c in integration_costs) if integration_costs else 0
        }

    def integration_cost_benchmarks(self) -> Dict[str, Any]:
        """Provide industry benchmarks for integration costs"""

        return {
            'typical_ranges_pct_of_deal': {
                'total_integration': {'low': 2.0, 'median': 5.0, 'high': 10.0},
                'it_integration': {'low': 0.5, 'median': 1.5, 'high': 3.0},
                'severance': {'low': 0.5, 'median': 1.0, 'high': 2.5},
                'retention': {'low': 0.3, 'median': 0.8, 'high': 1.5},
                'advisors': {'low': 0.5, 'median': 1.0, 'high': 2.0}
            },
            'timing_benchmarks': {
                'it_integration': '12-24 months',
                'organization_design': '6-12 months',
                'culture_integration': '18-36 months',
                'full_integration': '24-36 months'
            },
            'success_factors': [
                'Clear day-one priorities',
                'Dedicated integration team',
                'Strong executive sponsorship',
                'Regular communication',
                'Customer focus',
                'Quick wins identification'
            ]
        }

    def integration_risk_assessment(self, integration_complexity: str,
                                    cultural_fit: str,
                                    size_ratio: float) -> Dict[str, Any]:
        """Assess integration risk and potential cost overruns"""

        complexity_risk = {
            'low': 1.0,
            'medium': 1.2,
            'high': 1.5,
            'very_high': 2.0
        }

        cultural_risk = {
            'high': 1.0,
            'medium': 1.15,
            'low': 1.3
        }

        size_risk = 1.0
        if size_ratio > 0.5:
            size_risk = 1.2
        if size_ratio > 0.75:
            size_risk = 1.4

        combined_risk_multiplier = (
            complexity_risk.get(integration_complexity, 1.2) *
            cultural_risk.get(cultural_fit, 1.15) *
            size_risk
        )

        risk_rating = 'low'
        if combined_risk_multiplier > 1.5:
            risk_rating = 'high'
        elif combined_risk_multiplier > 1.2:
            risk_rating = 'medium'

        return {
            'integration_complexity': integration_complexity,
            'cultural_fit': cultural_fit,
            'size_ratio': size_ratio,
            'complexity_multiplier': complexity_risk.get(integration_complexity, 1.2),
            'cultural_multiplier': cultural_risk.get(cultural_fit, 1.15),
            'size_multiplier': size_risk,
            'combined_risk_multiplier': combined_risk_multiplier,
            'risk_rating': risk_rating,
            'recommended_contingency_pct': (combined_risk_multiplier - 1) * 100
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
        if command in ("integration", "integration_cost"):
            if len(sys.argv) < 3:
                raise ValueError("Deal size and complexity level required")

            # Accept either JSON dict or positional args
            try:
                params = json.loads(sys.argv[2])
                deal_value = float(params.get('deal_size', params.get('deal_value', 0)))
                complexity_level = str(params.get('complexity', params.get('complexity_level', 'medium')))
                integration_plan = {k: v for k, v in params.items() if k not in ('deal_size', 'deal_value', 'complexity', 'complexity_level')}
            except (json.JSONDecodeError, TypeError, AttributeError):
                if len(sys.argv) < 4:
                    raise ValueError("Deal size and complexity level required")
                deal_value = float(sys.argv[2])
                complexity_level = sys.argv[3]
                integration_plan = json.loads(sys.argv[4]) if len(sys.argv) > 4 else {}

            # Map complexity level string to numeric factor
            complexity_map = {'low': 0.5, 'medium': 1.0, 'high': 1.5, 'very_high': 2.0}
            complexity_factor = complexity_map.get(complexity_level, 1.0)
            if isinstance(complexity_level, (int, float)) or complexity_level.replace('.', '').isdigit():
                complexity_factor = float(complexity_level)

            systems_to_integrate = integration_plan.get('systems_to_integrate', 5)

            analyzer = IntegrationCostAnalyzer(deal_value=deal_value)

            analysis = analyzer.estimate_it_integration(
                systems_to_integrate=systems_to_integrate,
                complexity_factor=complexity_factor,
                third_party_consulting=integration_plan.get('third_party_consulting', True)
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
