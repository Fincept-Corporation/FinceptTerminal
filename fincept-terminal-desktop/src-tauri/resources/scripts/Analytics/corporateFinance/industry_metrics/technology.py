"""Technology Industry-Specific M&A Metrics"""
from typing import Dict, Any, List, Optional
from enum import Enum

class TechBusinessModel(Enum):
    SAAS = "saas"
    ENTERPRISE_SOFTWARE = "enterprise_software"
    CONSUMER_SOFTWARE = "consumer_software"
    SEMICONDUCTOR = "semiconductor"
    HARDWARE = "hardware"
    MARKETPLACE = "marketplace"
    ADTECH = "adtech"

class TechnologyMetrics:
    """Technology sector-specific M&A metrics and multiples"""

    def __init__(self, business_model: TechBusinessModel):
        self.business_model = business_model

    def calculate_saas_metrics(self, arr: float,
                               revenue_growth_rate: float,
                               gross_margin: float,
                               net_retention_rate: float,
                               cac: float,
                               ltv: float,
                               burn_multiple: Optional[float] = None) -> Dict[str, Any]:
        """Calculate SaaS-specific metrics"""

        rule_of_40 = revenue_growth_rate + (gross_margin - 0.80) * 100

        ltv_cac_ratio = ltv / cac if cac > 0 else 0

        cac_payback_months = (cac / (arr / 12)) if arr > 0 else 0

        magic_number = (arr * 0.25) / (arr * 0.20) if arr > 0 else revenue_growth_rate / 100

        arr_per_employee = None

        efficiency_score = 0
        if rule_of_40 >= 40:
            efficiency_score += 30
        elif rule_of_40 >= 30:
            efficiency_score += 20

        if ltv_cac_ratio >= 3:
            efficiency_score += 25
        elif ltv_cac_ratio >= 2:
            efficiency_score += 15

        if net_retention_rate >= 120:
            efficiency_score += 25
        elif net_retention_rate >= 110:
            efficiency_score += 15

        if gross_margin >= 0.75:
            efficiency_score += 20
        elif gross_margin >= 0.70:
            efficiency_score += 10

        return {
            'business_model': 'saas',
            'arr': arr,
            'revenue_growth_rate': revenue_growth_rate,
            'gross_margin': gross_margin * 100,
            'net_retention_rate': net_retention_rate,
            'key_metrics': {
                'rule_of_40': rule_of_40,
                'ltv_cac_ratio': ltv_cac_ratio,
                'cac_payback_months': cac_payback_months,
                'magic_number': magic_number,
                'burn_multiple': burn_multiple
            },
            'efficiency_score': efficiency_score,
            'valuation_implications': {
                'premium_multiple_justified': efficiency_score >= 70,
                'suggested_multiple_range': self._saas_multiple_range(rule_of_40, revenue_growth_rate, net_retention_rate)
            }
        }

    def _saas_multiple_range(self, rule_of_40: float, growth: float, nrr: float) -> Dict[str, float]:
        """Determine appropriate ARR multiple range for SaaS"""

        base_multiple = 5.0

        if rule_of_40 >= 50:
            base_multiple = 12.0
        elif rule_of_40 >= 40:
            base_multiple = 9.0
        elif rule_of_40 >= 30:
            base_multiple = 7.0

        if growth > 50:
            base_multiple *= 1.3
        elif growth > 30:
            base_multiple *= 1.15

        if nrr >= 130:
            base_multiple *= 1.2
        elif nrr >= 120:
            base_multiple *= 1.1

        return {
            'low': base_multiple * 0.8,
            'mid': base_multiple,
            'high': base_multiple * 1.2
        }

    def calculate_user_metrics(self, mau: int,
                               dau: int,
                               revenue: float,
                               user_growth_rate: float,
                               arpu: float,
                               engagement_rate: float) -> Dict[str, Any]:
        """Calculate user-based metrics for consumer tech"""

        dau_mau_ratio = (dau / mau) if mau > 0 else 0

        revenue_per_mau = revenue / mau if mau > 0 else 0
        revenue_per_dau = revenue / dau if dau > 0 else 0

        user_quality_score = 0
        if dau_mau_ratio >= 0.5:
            user_quality_score += 30
        elif dau_mau_ratio >= 0.3:
            user_quality_score += 20

        if user_growth_rate >= 50:
            user_quality_score += 25
        elif user_growth_rate >= 30:
            user_quality_score += 15

        if engagement_rate >= 60:
            user_quality_score += 25
        elif engagement_rate >= 40:
            user_quality_score += 15

        if arpu >= 10:
            user_quality_score += 20
        elif arpu >= 5:
            user_quality_score += 10

        return {
            'mau': mau,
            'dau': dau,
            'dau_mau_ratio': dau_mau_ratio * 100,
            'arpu': arpu,
            'revenue_per_mau': revenue_per_mau,
            'revenue_per_dau': revenue_per_dau,
            'user_growth_rate': user_growth_rate,
            'engagement_rate': engagement_rate,
            'user_quality_score': user_quality_score,
            'valuation_approach': 'user_based',
            'implied_value_per_user': self._value_per_user(user_quality_score, arpu, engagement_rate)
        }

    def _value_per_user(self, quality_score: int, arpu: float, engagement: float) -> Dict[str, float]:
        """Estimate value per user for M&A"""

        base_value = arpu * 36

        if quality_score >= 70:
            base_value *= 1.5
        elif quality_score >= 50:
            base_value *= 1.2

        if engagement >= 60:
            base_value *= 1.3

        return {
            'value_per_mau': base_value,
            'value_per_dau': base_value * 2.5
        }

    def calculate_marketplace_metrics(self, gmv: float,
                                     take_rate: float,
                                     revenue: float,
                                     liquidity_score: float,
                                     supply_side_retention: float,
                                     demand_side_retention: float) -> Dict[str, Any]:
        """Calculate marketplace-specific metrics"""

        implied_take_rate = (revenue / gmv) if gmv > 0 else take_rate

        network_strength = (supply_side_retention + demand_side_retention) / 2

        marketplace_quality = 0
        if liquidity_score >= 80:
            marketplace_quality += 30
        elif liquidity_score >= 60:
            marketplace_quality += 20

        if take_rate >= 20:
            marketplace_quality += 25
        elif take_rate >= 15:
            marketplace_quality += 15

        if network_strength >= 85:
            marketplace_quality += 25
        elif network_strength >= 75:
            marketplace_quality += 15

        if supply_side_retention >= 80 and demand_side_retention >= 80:
            marketplace_quality += 20

        gmv_multiple = self._gmv_multiple_range(marketplace_quality, take_rate, network_strength)

        return {
            'gmv': gmv,
            'revenue': revenue,
            'take_rate': take_rate,
            'implied_take_rate': implied_take_rate * 100,
            'liquidity_score': liquidity_score,
            'supply_side_retention': supply_side_retention,
            'demand_side_retention': demand_side_retention,
            'network_strength': network_strength,
            'marketplace_quality_score': marketplace_quality,
            'gmv_multiple_range': gmv_multiple
        }

    def _gmv_multiple_range(self, quality: int, take_rate: float, network: float) -> Dict[str, float]:
        """Determine GMV multiple range"""

        base = 0.5

        if quality >= 70:
            base = 1.2
        elif quality >= 50:
            base = 0.8

        if take_rate >= 20:
            base *= 1.3

        if network >= 85:
            base *= 1.2

        return {
            'low': base * 0.8,
            'mid': base,
            'high': base * 1.2
        }

    def calculate_semiconductor_metrics(self, revenue: float,
                                       r_and_d_pct: float,
                                       gross_margin: float,
                                       design_win_backlog: float,
                                       process_node: int,
                                       fabless: bool) -> Dict[str, Any]:
        """Calculate semiconductor-specific metrics"""

        r_and_d_intensity = r_and_d_pct

        technology_leadership_score = 0
        if process_node <= 5:
            technology_leadership_score = 100
        elif process_node <= 7:
            technology_leadership_score = 85
        elif process_node <= 10:
            technology_leadership_score = 70
        elif process_node <= 14:
            technology_leadership_score = 55
        else:
            technology_leadership_score = 40

        design_win_coverage = (design_win_backlog / revenue) if revenue > 0 else 0

        semiconductor_quality = 0
        if gross_margin >= 60:
            semiconductor_quality += 30
        elif gross_margin >= 50:
            semiconductor_quality += 20

        if technology_leadership_score >= 85:
            semiconductor_quality += 30
        elif technology_leadership_score >= 70:
            semiconductor_quality += 20

        if design_win_coverage >= 2.0:
            semiconductor_quality += 20
        elif design_win_coverage >= 1.5:
            semiconductor_quality += 15

        if fabless:
            semiconductor_quality += 20

        return {
            'revenue': revenue,
            'r_and_d_pct': r_and_d_pct * 100,
            'gross_margin': gross_margin * 100,
            'process_node_nm': process_node,
            'fabless': fabless,
            'design_win_backlog': design_win_backlog,
            'design_win_coverage_years': design_win_coverage,
            'technology_leadership_score': technology_leadership_score,
            'semiconductor_quality_score': semiconductor_quality,
            'typical_revenue_multiple': self._semiconductor_multiple(semiconductor_quality, gross_margin)
        }

    def _semiconductor_multiple(self, quality: int, margin: float) -> Dict[str, float]:
        """Semiconductor revenue multiple range"""

        if quality >= 80:
            base = 6.0
        elif quality >= 60:
            base = 4.5
        else:
            base = 3.0

        if margin >= 0.60:
            base *= 1.2

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def tech_acquisition_multiples_benchmark(self) -> Dict[str, Any]:
        """Industry benchmark multiples for tech M&A"""

        return {
            'saas': {
                'arr_multiple': {'25th': 6.0, 'median': 9.0, '75th': 13.0},
                'revenue_multiple': {'25th': 5.0, 'median': 8.0, '75th': 12.0},
                'key_driver': 'Rule of 40, NRR, growth rate'
            },
            'enterprise_software': {
                'revenue_multiple': {'25th': 4.0, 'median': 6.5, '75th': 10.0},
                'ebitda_multiple': {'25th': 18.0, 'median': 24.0, '75th': 32.0},
                'key_driver': 'Recurring revenue %, customer concentration'
            },
            'consumer_software': {
                'revenue_multiple': {'25th': 2.5, 'median': 4.5, '75th': 7.0},
                'mau_value': {'25th': 25, 'median': 50, '75th': 100},
                'key_driver': 'User engagement, monetization, growth'
            },
            'semiconductor': {
                'revenue_multiple': {'25th': 2.5, 'median': 4.0, '75th': 6.0},
                'ebitda_multiple': {'25th': 12.0, 'median': 16.0, '75th': 22.0},
                'key_driver': 'Process technology, gross margin, design wins'
            },
            'marketplace': {
                'gmv_multiple': {'25th': 0.4, 'median': 0.8, '75th': 1.5},
                'revenue_multiple': {'25th': 3.0, 'median': 5.0, '75th': 8.0},
                'key_driver': 'Take rate, liquidity, network effects'
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
        if command == "tech":
            if len(sys.argv) < 4:
                raise ValueError("Sector and company data required")

            sector = sys.argv[2]
            company_data = json.loads(sys.argv[3])

            # Determine business model from sector
            if 'saas' in sector.lower():
                model = TechBusinessModel.SAAS
            elif 'marketplace' in sector.lower():
                model = TechBusinessModel.MARKETPLACE
            elif 'semiconductor' in sector.lower():
                model = TechBusinessModel.SEMICONDUCTOR
            else:
                model = TechBusinessModel.SAAS  # default

            analyzer = TechnologyMetrics(model)

            # Map common alternative key names for SaaS
            saas_aliases = {
                'revenue': 'arr', 'annual_recurring_revenue': 'arr',
                'growth_rate': 'revenue_growth_rate', 'growth': 'revenue_growth_rate',
                'net_retention': 'net_retention_rate', 'nrr': 'net_retention_rate',
                'customer_acquisition_cost': 'cac',
                'lifetime_value': 'ltv',
            }
            for old_key, new_key in saas_aliases.items():
                if old_key in company_data and new_key not in company_data:
                    company_data[new_key] = company_data.pop(old_key)

            # Route to appropriate calculation based on model
            if model == TechBusinessModel.SAAS:
                analysis = analyzer.calculate_saas_metrics(**company_data)
            elif model == TechBusinessModel.MARKETPLACE:
                analysis = analyzer.calculate_marketplace_metrics(**company_data)
            elif model == TechBusinessModel.SEMICONDUCTOR:
                analysis = analyzer.calculate_semiconductor_metrics(**company_data)

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
