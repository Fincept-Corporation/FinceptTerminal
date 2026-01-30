"""Healthcare Industry-Specific M&A Metrics"""
from typing import Dict, Any, List, Optional
from enum import Enum

class HealthcareSegment(Enum):
    BIOTECH = "biotech"
    PHARMA = "pharma"
    MEDICAL_DEVICES = "medical_devices"
    HEALTHCARE_SERVICES = "healthcare_services"
    HEALTHTECH = "healthtech"

class HealthcareMetrics:
    """Healthcare sector-specific M&A metrics"""

    def __init__(self, segment: HealthcareSegment):
        self.segment = segment

    def calculate_pharma_biotech_metrics(self, pipeline_value: Dict[str, Any],
                                        marketed_products_revenue: float,
                                        r_and_d_expense: float,
                                        patent_expiry_schedule: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Calculate pharma/biotech valuation metrics"""

        total_pipeline_npv = sum(asset['npv'] for asset in pipeline_value.get('assets', []))

        marketed_value = marketed_products_revenue * 4.0

        patent_cliff_risk = self._assess_patent_cliff(patent_expiry_schedule, marketed_products_revenue)

        total_value = marketed_value + total_pipeline_npv

        pipeline_contribution = (total_pipeline_npv / total_value * 100) if total_value > 0 else 0

        r_and_d_efficiency = (total_pipeline_npv / r_and_d_expense) if r_and_d_expense > 0 else 0

        return {
            'marketed_products_revenue': marketed_products_revenue,
            'marketed_products_value': marketed_value,
            'pipeline_npv': total_pipeline_npv,
            'total_enterprise_value': total_value,
            'pipeline_contribution_pct': pipeline_contribution,
            'r_and_d_efficiency': r_and_d_efficiency,
            'patent_cliff_assessment': patent_cliff_risk,
            'valuation_breakdown': {
                'marketed_products': marketed_value,
                'pipeline': total_pipeline_npv
            }
        }

    def _assess_patent_cliff(self, schedule: List[Dict[str, Any]], revenue: float) -> Dict[str, Any]:
        """Assess patent expiry risk"""

        near_term_expiries = [p for p in schedule if p.get('years_to_expiry', 10) <= 3]

        at_risk_revenue = sum(p.get('revenue', 0) for p in near_term_expiries)
        at_risk_pct = (at_risk_revenue / revenue * 100) if revenue > 0 else 0

        risk_level = "high" if at_risk_pct > 30 else "moderate" if at_risk_pct > 15 else "low"

        return {
            'products_expiring_3_years': len(near_term_expiries),
            'revenue_at_risk': at_risk_revenue,
            'revenue_at_risk_pct': at_risk_pct,
            'risk_level': risk_level
        }

    def value_drug_candidate(self, peak_sales: float,
                            years_to_peak: int,
                            patent_life_remaining: int,
                            probability_of_success: float,
                            development_costs_remaining: float,
                            discount_rate: float = 0.12) -> Dict[str, Any]:
        """Value individual drug candidate using rNPV"""

        cash_flows = []

        for year in range(1, patent_life_remaining + 1):
            if year <= years_to_peak:
                sales = peak_sales * (year / years_to_peak)
            else:
                decline_years = year - years_to_peak
                sales = peak_sales * (0.85 ** decline_years)

            profit_margin = 0.75
            cash_flow = sales * profit_margin

            pv = cash_flow / ((1 + discount_rate) ** year)
            cash_flows.append({
                'year': year,
                'sales': sales,
                'cash_flow': cash_flow,
                'present_value': pv
            })

        npv = sum(cf['present_value'] for cf in cash_flows)

        rnpv = (npv - development_costs_remaining) * probability_of_success

        return {
            'peak_sales': peak_sales,
            'years_to_peak': years_to_peak,
            'patent_life_remaining': patent_life_remaining,
            'probability_of_success': probability_of_success * 100,
            'development_costs_remaining': development_costs_remaining,
            'npv_unrisked': npv,
            'rnpv': rnpv,
            'discount_rate': discount_rate * 100,
            'cash_flow_projections': cash_flows[:10]
        }

    def calculate_medical_device_metrics(self, revenue: float,
                                        gross_margin: float,
                                        clinical_evidence_strength: int,
                                        regulatory_approvals: List[str],
                                        reimbursement_coverage: float,
                                        installed_base: Optional[int] = None,
                                        recurring_revenue_pct: Optional[float] = None) -> Dict[str, Any]:
        """Calculate medical device company metrics"""

        regulatory_score = len(regulatory_approvals) * 20
        if 'FDA' in regulatory_approvals:
            regulatory_score += 20
        if 'CE_Mark' in regulatory_approvals:
            regulatory_score += 15

        device_quality_score = 0

        if gross_margin >= 0.70:
            device_quality_score += 25
        elif gross_margin >= 0.60:
            device_quality_score += 15

        if clinical_evidence_strength >= 80:
            device_quality_score += 25
        elif clinical_evidence_strength >= 60:
            device_quality_score += 15

        if reimbursement_coverage >= 80:
            device_quality_score += 25
        elif reimbursement_coverage >= 60:
            device_quality_score += 15

        device_quality_score += min(regulatory_score, 25)

        if recurring_revenue_pct and recurring_revenue_pct >= 40:
            device_quality_score += 10

        revenue_multiple = self._device_revenue_multiple(device_quality_score, gross_margin, recurring_revenue_pct)

        return {
            'revenue': revenue,
            'gross_margin': gross_margin * 100,
            'clinical_evidence_strength': clinical_evidence_strength,
            'regulatory_approvals': regulatory_approvals,
            'reimbursement_coverage': reimbursement_coverage,
            'installed_base': installed_base,
            'recurring_revenue_pct': recurring_revenue_pct * 100 if recurring_revenue_pct else None,
            'device_quality_score': device_quality_score,
            'regulatory_score': min(regulatory_score, 100),
            'revenue_multiple_range': revenue_multiple
        }

    def _device_revenue_multiple(self, quality: int, margin: float, recurring_pct: Optional[float]) -> Dict[str, float]:
        """Determine medical device revenue multiple"""

        base = 3.0

        if quality >= 80:
            base = 5.5
        elif quality >= 60:
            base = 4.5

        if margin >= 0.70:
            base *= 1.2

        if recurring_pct and recurring_pct >= 40:
            base *= 1.3

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def calculate_healthcare_services_metrics(self, revenue: float,
                                             ebitda: float,
                                             same_store_growth: float,
                                             payor_mix: Dict[str, float],
                                             quality_scores: Dict[str, float],
                                             regulatory_compliance_score: int) -> Dict[str, Any]:
        """Calculate healthcare services metrics"""

        ebitda_margin = (ebitda / revenue) if revenue > 0 else 0

        medicare_pct = payor_mix.get('medicare', 0)
        commercial_pct = payor_mix.get('commercial', 0)
        medicaid_pct = payor_mix.get('medicaid', 0)

        payor_quality_score = (commercial_pct * 1.0 + medicare_pct * 0.7 + medicaid_pct * 0.5)

        avg_quality_score = sum(quality_scores.values()) / len(quality_scores) if quality_scores else 0

        services_quality = 0

        if ebitda_margin >= 0.20:
            services_quality += 25
        elif ebitda_margin >= 0.15:
            services_quality += 15

        if same_store_growth >= 5:
            services_quality += 25
        elif same_store_growth >= 3:
            services_quality += 15

        if payor_quality_score >= 0.75:
            services_quality += 20
        elif payor_quality_score >= 0.60:
            services_quality += 10

        if avg_quality_score >= 4.5:
            services_quality += 15
        elif avg_quality_score >= 4.0:
            services_quality += 10

        if regulatory_compliance_score >= 90:
            services_quality += 15
        elif regulatory_compliance_score >= 80:
            services_quality += 10

        ebitda_multiple = self._services_ebitda_multiple(services_quality, ebitda_margin)

        return {
            'revenue': revenue,
            'ebitda': ebitda,
            'ebitda_margin': ebitda_margin * 100,
            'same_store_growth': same_store_growth,
            'payor_mix': {k: v * 100 for k, v in payor_mix.items()},
            'payor_quality_score': payor_quality_score * 100,
            'average_quality_score': avg_quality_score,
            'regulatory_compliance': regulatory_compliance_score,
            'services_quality_score': services_quality,
            'ebitda_multiple_range': ebitda_multiple
        }

    def _services_ebitda_multiple(self, quality: int, margin: float) -> Dict[str, float]:
        """Healthcare services EBITDA multiple range"""

        base = 8.0

        if quality >= 80:
            base = 12.0
        elif quality >= 60:
            base = 10.0

        if margin >= 0.20:
            base *= 1.15

        return {
            'low': base * 0.85,
            'mid': base,
            'high': base * 1.15
        }

    def calculate_healthtech_metrics(self, revenue: float,
                                    revenue_growth: float,
                                    clinical_integration_depth: int,
                                    provider_customers: int,
                                    patient_lives_covered: int,
                                    data_assets_value: Optional[float] = None) -> Dict[str, Any]:
        """Calculate healthtech/digital health metrics"""

        revenue_per_provider = revenue / provider_customers if provider_customers > 0 else 0
        revenue_per_life = revenue / patient_lives_covered if patient_lives_covered > 0 else 0

        healthtech_quality = 0

        if revenue_growth >= 100:
            healthtech_quality += 30
        elif revenue_growth >= 50:
            healthtech_quality += 20

        if clinical_integration_depth >= 80:
            healthtech_quality += 25
        elif clinical_integration_depth >= 60:
            healthtech_quality += 15

        if patient_lives_covered >= 1_000_000:
            healthtech_quality += 25
        elif patient_lives_covered >= 500_000:
            healthtech_quality += 15

        if revenue_per_provider >= 50_000:
            healthtech_quality += 20
        elif revenue_per_provider >= 25_000:
            healthtech_quality += 10

        revenue_multiple = self._healthtech_multiple(healthtech_quality, revenue_growth)

        return {
            'revenue': revenue,
            'revenue_growth': revenue_growth,
            'clinical_integration_depth': clinical_integration_depth,
            'provider_customers': provider_customers,
            'patient_lives_covered': patient_lives_covered,
            'revenue_per_provider': revenue_per_provider,
            'revenue_per_life': revenue_per_life,
            'data_assets_value': data_assets_value,
            'healthtech_quality_score': healthtech_quality,
            'revenue_multiple_range': revenue_multiple
        }

    def _healthtech_multiple(self, quality: int, growth: float) -> Dict[str, float]:
        """Healthtech revenue multiple"""

        base = 4.0

        if quality >= 80:
            base = 8.0
        elif quality >= 60:
            base = 6.0

        if growth >= 100:
            base *= 1.4
        elif growth >= 50:
            base *= 1.2

        return {
            'low': base * 0.8,
            'mid': base,
            'high': base * 1.2
        }

    def healthcare_ma_benchmarks(self) -> Dict[str, Any]:
        """Healthcare M&A benchmark multiples"""

        return {
            'pharma': {
                'revenue_multiple': {'25th': 3.5, 'median': 5.0, '75th': 7.0},
                'ebitda_multiple': {'25th': 14, 'median': 18, '75th': 24},
                'key_drivers': 'Pipeline strength, patent life, R&D productivity'
            },
            'biotech': {
                'market_cap_to_pipeline_npv': {'25th': 0.4, 'median': 0.6, '75th': 0.9},
                'key_drivers': 'Clinical stage, indication, probability of success'
            },
            'medical_devices': {
                'revenue_multiple': {'25th': 3.0, 'median': 4.5, '75th': 6.5},
                'ebitda_multiple': {'25th': 12, 'median': 16, '75th': 22},
                'key_drivers': 'Regulatory status, reimbursement, clinical evidence'
            },
            'healthcare_services': {
                'revenue_multiple': {'25th': 0.8, 'median': 1.2, '75th': 1.8},
                'ebitda_multiple': {'25th': 8, 'median': 11, '75th': 14},
                'key_drivers': 'Payor mix, quality scores, same-store growth'
            },
            'healthtech': {
                'revenue_multiple': {'25th': 4.0, 'median': 6.0, '75th': 9.0},
                'key_drivers': 'Growth rate, clinical integration, covered lives'
            }
        }

if __name__ == '__main__':
    pharma = HealthcareMetrics(HealthcareSegment.PHARMA)

    drug = pharma.value_drug_candidate(
        peak_sales=2_000_000_000,
        years_to_peak=5,
        patent_life_remaining=12,
        probability_of_success=0.35,
        development_costs_remaining=500_000_000,
        discount_rate=0.12
    )

    print("=== DRUG CANDIDATE VALUATION ===\n")
    print(f"Peak Sales: ${drug['peak_sales']:,.0f}")
    print(f"Probability of Success: {drug['probability_of_success']:.0f}%")
    print(f"NPV (Unrisked): ${drug['npv_unrisked']:,.0f}")
    print(f"rNPV: ${drug['rnpv']:,.0f}")
    print(f"Development Costs Remaining: ${drug['development_costs_remaining']:,.0f}")

    device = HealthcareMetrics(HealthcareSegment.MEDICAL_DEVICES)

    device_metrics = device.calculate_medical_device_metrics(
        revenue=150_000_000,
        gross_margin=0.72,
        clinical_evidence_strength=85,
        regulatory_approvals=['FDA', 'CE_Mark'],
        reimbursement_coverage=90,
        installed_base=5_000,
        recurring_revenue_pct=0.45
    )

    print("\n\n=== MEDICAL DEVICE METRICS ===")
    print(f"Revenue: ${device_metrics['revenue']:,.0f}")
    print(f"Gross Margin: {device_metrics['gross_margin']:.1f}%")
    print(f"Quality Score: {device_metrics['device_quality_score']}/100")
    print(f"Regulatory Score: {device_metrics['regulatory_score']}/100")
    print(f"\nRevenue Multiple Range:")
    mult = device_metrics['revenue_multiple_range']
    print(f"  {mult['low']:.1f}x - {mult['high']:.1f}x (Mid: {mult['mid']:.1f}x)")

    benchmarks = pharma.healthcare_ma_benchmarks()
    print("\n\n=== HEALTHCARE M&A BENCHMARKS ===")
    for sector, data in benchmarks.items():
        print(f"\n{sector.upper().replace('_', ' ')}:")
        print(f"  Key Drivers: {data['key_drivers']}")
