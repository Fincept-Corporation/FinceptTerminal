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

def _build_standard_output(analysis: dict, sector: str) -> dict:
    """Build valuation_metrics, benchmarks, and insights from sector-specific analysis output."""
    benchmarks_db = {
        'saas': {
            'arr': {'p25': 50, 'median': 150, 'p75': 500},
            'arr_growth': {'p25': 25, 'median': 40, 'p75': 65},
            'gross_margin': {'p25': 65, 'median': 75, 'p75': 82},
            'net_retention': {'p25': 100, 'median': 110, 'p75': 125},
            'ltv_cac': {'p25': 2.0, 'median': 3.5, 'p75': 5.5},
        },
        'marketplace': {
            'gmv': {'p25': 200, 'median': 800, 'p75': 3000},
            'take_rate': {'p25': 10, 'median': 17, 'p75': 25},
            'repeat_rate': {'p25': 40, 'median': 58, 'p75': 72},
        },
        'semiconductor': {
            'revenue': {'p25': 100, 'median': 500, 'p75': 2000},
            'gross_margin': {'p25': 45, 'median': 55, 'p75': 65},
            'r_and_d_intensity': {'p25': 12, 'median': 18, 'p75': 25},
        },
    }

    vm = {}
    insights = []

    if sector == 'saas':
        km = analysis.get('key_metrics', {})
        vm = {
            'rule_of_40': round(km.get('rule_of_40', 0), 1),
            'ltv_cac_ratio': round(km.get('ltv_cac_ratio', 0), 2),
            'cac_payback_months': round(km.get('cac_payback_months', 0), 1),
            'efficiency_score': analysis.get('efficiency_score', 0),
            'gross_margin_pct': round(analysis.get('gross_margin', 0), 1),
            'net_retention_rate': round(analysis.get('net_retention_rate', 0), 1),
        }
        vi = analysis.get('valuation_implications', {})
        mr = vi.get('suggested_multiple_range', {})
        vm['arr_multiple_mid'] = round(mr.get('mid', 0), 1)
        if km.get('rule_of_40', 0) >= 40:
            insights.append(f"Rule of 40 score of {vm['rule_of_40']} indicates strong operational efficiency — premium multiple justified.")
        if km.get('ltv_cac_ratio', 0) >= 3:
            insights.append("LTV/CAC ratio >= 3x demonstrates healthy unit economics for SaaS growth.")
        if analysis.get('net_retention_rate', 0) >= 110:
            insights.append("Net revenue retention above 110% shows strong net expansion — customers spending more over time.")
        insights.append(f"Suggested ARR multiple range: {mr.get('low', 0):.1f}x – {mr.get('high', 0):.1f}x based on efficiency profile.")

    elif sector == 'marketplace':
        vm = {
            'gmv': round(analysis.get('gmv', 0), 1),
            'take_rate_pct': round(analysis.get('implied_take_rate', analysis.get('take_rate', 0) * 100), 1),
            'network_strength': round(analysis.get('network_strength', 0), 1),
            'marketplace_quality_score': analysis.get('marketplace_quality_score', 0),
        }
        mr = analysis.get('gmv_multiple_range', {})
        vm['gmv_multiple_mid'] = round(mr.get('mid', 0), 2)
        insights.append(f"GMV multiple range: {mr.get('low', 0):.2f}x – {mr.get('high', 0):.2f}x of GMV.")
        if analysis.get('take_rate', 0) >= 15:
            insights.append("Take rate >= 15% indicates strong monetization power in the marketplace.")
        insights.append(f"Network strength score of {vm['network_strength']:.0f}% reflects combined supply/demand retention.")

    elif sector == 'semiconductor':
        vm = {
            'revenue': round(analysis.get('revenue', 0), 1),
            'gross_margin_pct': round(analysis.get('gross_margin', 0), 1),
            'technology_leadership_score': analysis.get('technology_leadership_score', 0),
            'semiconductor_quality_score': analysis.get('semiconductor_quality_score', 0),
            'design_win_coverage_years': round(analysis.get('design_win_coverage_years', 0), 2),
        }
        mr = analysis.get('typical_revenue_multiple', {})
        vm['revenue_multiple_mid'] = round(mr.get('mid', 0), 1)
        insights.append(f"Revenue multiple range: {mr.get('low', 0):.1f}x – {mr.get('high', 0):.1f}x.")
        if analysis.get('technology_leadership_score', 0) >= 70:
            insights.append("Technology leadership score indicates competitive process node advantage.")
        if analysis.get('design_win_coverage_years', 0) >= 1.5:
            insights.append("Strong design win backlog provides revenue visibility for 1.5+ years.")

    return {
        **analysis,
        'valuation_metrics': vm,
        'benchmarks': benchmarks_db.get(sector, {}),
        'insights': insights,
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
            elif 'consumer' in sector.lower():
                model = TechBusinessModel.CONSUMER_SOFTWARE
            elif 'marketplace' in sector.lower():
                model = TechBusinessModel.MARKETPLACE
            elif 'semiconductor' in sector.lower():
                model = TechBusinessModel.SEMICONDUCTOR
            else:
                model = TechBusinessModel.SAAS  # default

            analyzer = TechnologyMetrics(model)
            sector_key = sector.lower()

            # Route to appropriate calculation based on model
            if model == TechBusinessModel.SAAS:
                # Map frontend keys to Python parameter names
                saas_aliases = {
                    'revenue': 'arr', 'annual_recurring_revenue': 'arr',
                    'arr_growth': 'revenue_growth_rate',
                    'growth_rate': 'revenue_growth_rate', 'growth': 'revenue_growth_rate',
                    'net_retention': 'net_retention_rate', 'nrr': 'net_retention_rate',
                    'customer_acquisition_cost': 'cac',
                    'lifetime_value': 'ltv',
                }
                for old_key, new_key in saas_aliases.items():
                    if old_key in company_data and new_key not in company_data:
                        company_data[new_key] = company_data.pop(old_key)

                # ltv_cac ratio: split into ltv=ratio and cac=1 so ltv/cac = ratio
                if 'ltv_cac' in company_data and 'cac' not in company_data:
                    company_data['cac'] = 1.0
                    company_data['ltv'] = float(company_data.pop('ltv_cac'))
                elif 'ltv_cac' in company_data:
                    company_data.pop('ltv_cac')

                # gross_margin: normalize to 0-1 fraction if given as percentage
                if 'gross_margin' in company_data and company_data['gross_margin'] > 1:
                    company_data['gross_margin'] = company_data['gross_margin'] / 100.0

                # net_retention_rate: keep as-is (Python compares >= 110 treating it as percentage)
                # rule_of_40 and cac_payback are outputs, not inputs — drop them
                for drop_key in ('rule_of_40', 'cac_payback'):
                    company_data.pop(drop_key, None)

                # Set defaults for required params not provided by frontend
                company_data.setdefault('revenue_growth_rate', 30.0)
                company_data.setdefault('net_retention_rate', 100.0)
                company_data.setdefault('cac', 1.0)
                company_data.setdefault('ltv', 3.0)

                import inspect
                valid_params = set(inspect.signature(analyzer.calculate_saas_metrics).parameters.keys())
                filtered = {k: v for k, v in company_data.items() if k in valid_params}
                analysis = analyzer.calculate_saas_metrics(**filtered)
                sector_key = 'saas'

            elif model == TechBusinessModel.CONSUMER_SOFTWARE:
                import inspect
                valid_params = set(inspect.signature(analyzer.calculate_user_metrics).parameters.keys())
                filtered = {k: v for k, v in company_data.items() if k in valid_params}
                analysis = analyzer.calculate_user_metrics(**filtered)
                sector_key = 'consumer'

            elif model == TechBusinessModel.MARKETPLACE:
                # Map frontend keys to Python parameter names
                marketplace_aliases = {
                    'gmv_growth': None,          # computed metric, not an input — drop
                    'active_buyers': None,        # not a param — drop
                    'active_sellers': None,       # not a param — drop
                    'repeat_rate': 'demand_side_retention',
                }
                for old_key, new_key in marketplace_aliases.items():
                    if old_key in company_data:
                        if new_key:
                            if new_key not in company_data:
                                company_data[new_key] = company_data.pop(old_key)
                            else:
                                company_data.pop(old_key)
                        else:
                            company_data.pop(old_key)

                # take_rate: normalize to fraction if given as percentage
                if 'take_rate' in company_data and company_data['take_rate'] > 1:
                    company_data['take_rate'] = company_data['take_rate'] / 100.0

                # Provide defaults for params not in frontend form
                company_data.setdefault('revenue', company_data.get('gmv', 0) * company_data.get('take_rate', 0.15))
                company_data.setdefault('liquidity_score', 70.0)
                company_data.setdefault('supply_side_retention', company_data.get('demand_side_retention', 70.0))
                company_data.setdefault('demand_side_retention', 70.0)

                import inspect
                valid_params = set(inspect.signature(analyzer.calculate_marketplace_metrics).parameters.keys())
                filtered = {k: v for k, v in company_data.items() if k in valid_params}
                analysis = analyzer.calculate_marketplace_metrics(**filtered)
                sector_key = 'marketplace'

            elif model == TechBusinessModel.SEMICONDUCTOR:
                # Map frontend keys to Python parameter names
                semi_aliases = {
                    'r_and_d_intensity': 'r_and_d_pct',
                    'design_wins': 'design_win_backlog',
                    'revenue_growth': None,   # computed metric, not an input — drop
                    'book_to_bill': None,     # not a param — drop
                }
                for old_key, new_key in semi_aliases.items():
                    if old_key in company_data:
                        if new_key:
                            if new_key not in company_data:
                                company_data[new_key] = company_data.pop(old_key)
                            else:
                                company_data.pop(old_key)
                        else:
                            company_data.pop(old_key)

                # r_and_d_pct: normalize to fraction if given as percentage
                if 'r_and_d_pct' in company_data and company_data['r_and_d_pct'] > 1:
                    company_data['r_and_d_pct'] = company_data['r_and_d_pct'] / 100.0

                # gross_margin: normalize to fraction
                if 'gross_margin' in company_data and company_data['gross_margin'] > 1:
                    company_data['gross_margin'] = company_data['gross_margin'] / 100.0

                # Provide defaults for params not in frontend form
                company_data.setdefault('process_node', 14)   # nm — 14nm default (mature)
                company_data.setdefault('fabless', True)

                import inspect
                valid_params = set(inspect.signature(analyzer.calculate_semiconductor_metrics).parameters.keys())
                filtered = {k: v for k, v in company_data.items() if k in valid_params}
                analysis = analyzer.calculate_semiconductor_metrics(**filtered)
                sector_key = 'semiconductor'

            analysis_with_standard = _build_standard_output(analysis, sector_key)
            result = {"success": True, "data": analysis_with_standard}
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
