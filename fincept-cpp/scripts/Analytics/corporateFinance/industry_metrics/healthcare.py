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

def _calculate_pharma_company_metrics(data: dict) -> dict:
    """Company-level pharma/biotech valuation from frontend inputs."""
    revenue = float(data.get('revenue', 0))
    pipeline_value = float(data.get('pipeline_value', 0))
    phase3_count = int(data.get('phase3_count', 0))
    patent_expiry_pct = float(data.get('patent_expiry_revenue', 0))
    r_and_d_spend = float(data.get('r_and_d_spend', 0))
    ebitda_margin_pct = float(data.get('ebitda_margin', 0))
    ebitda = revenue * ebitda_margin_pct / 100.0
    marketed_value = revenue * 4.0
    pipeline_contribution = (pipeline_value / (marketed_value + pipeline_value) * 100) if (marketed_value + pipeline_value) > 0 else 0
    r_and_d_efficiency = (pipeline_value / r_and_d_spend) if r_and_d_spend > 0 else 0
    patent_cliff_risk = 'high' if patent_expiry_pct > 30 else ('moderate' if patent_expiry_pct > 15 else 'low')
    return {
        'revenue': revenue,
        'pipeline_value': pipeline_value,
        'phase3_count': phase3_count,
        'marketed_products_value': marketed_value,
        'ebitda': ebitda,
        'ebitda_margin': ebitda_margin_pct,
        'pipeline_contribution_pct': pipeline_contribution,
        'r_and_d_efficiency': r_and_d_efficiency,
        'patent_cliff_risk': patent_cliff_risk,
        'patent_expiry_revenue_pct': patent_expiry_pct,
    }


def _calculate_biotech_company_metrics(data: dict) -> dict:
    """Company-level biotech valuation from frontend inputs."""
    cash_position = float(data.get('cash_position', 0))
    burn_rate = float(data.get('burn_rate', 0))
    pipeline_value = float(data.get('pipeline_value', 0))
    lead_phase = int(data.get('lead_program_phase', 1))
    platform_value = float(data.get('platform_value', 0))
    partnership_revenue = float(data.get('partnership_revenue', 0))
    cash_runway_months = (cash_position / burn_rate) if burn_rate > 0 else 999
    total_value = pipeline_value + platform_value
    phase_premium = {1: 0.3, 2: 0.55, 3: 0.8}.get(lead_phase, 0.3)
    rnpv_estimate = total_value * phase_premium
    return {
        'cash_position': cash_position,
        'burn_rate': burn_rate,
        'cash_runway_months': cash_runway_months,
        'pipeline_value': pipeline_value,
        'lead_program_phase': lead_phase,
        'platform_value': platform_value,
        'partnership_revenue': partnership_revenue,
        'total_asset_value': total_value,
        'rnpv_estimate': rnpv_estimate,
        'phase_success_premium': phase_premium * 100,
    }


def _build_healthcare_standard_output(analysis: dict, sector: str) -> dict:
    """Build valuation_metrics, benchmarks, and insights for healthcare output."""
    benchmarks_db = {
        'pharma': {
            'revenue': {'p25': 2000, 'median': 8000, 'p75': 25000},
            'ebitda_margin': {'p25': 20, 'median': 32, 'p75': 42},
            'r_and_d_efficiency': {'p25': 2, 'median': 4, 'p75': 8},
        },
        'biotech': {
            'cash_position': {'p25': 100, 'median': 350, 'p75': 800},
            'cash_runway_months': {'p25': 12, 'median': 24, 'p75': 48},
            'pipeline_value': {'p25': 300, 'median': 900, 'p75': 2500},
        },
        'devices': {
            'revenue': {'p25': 100, 'median': 400, 'p75': 1500},
            'gross_margin': {'p25': 55, 'median': 65, 'p75': 72},
            'recurring_revenue_pct': {'p25': 20, 'median': 35, 'p75': 55},
        },
        'services': {
            'revenue': {'p25': 200, 'median': 700, 'p75': 2500},
            'ebitda_margin': {'p25': 10, 'median': 16, 'p75': 22},
            'same_store_growth': {'p25': 2, 'median': 4, 'p75': 7},
        },
    }

    vm = {}
    insights = []

    if sector == 'pharma':
        vm = {
            'revenue': round(analysis.get('revenue', 0), 1),
            'ebitda_margin_pct': round(analysis.get('ebitda_margin', 0), 1),
            'pipeline_contribution_pct': round(analysis.get('pipeline_contribution_pct', 0), 1),
            'r_and_d_efficiency': round(analysis.get('r_and_d_efficiency', 0), 2),
            'marketed_products_value': round(analysis.get('marketed_products_value', 0), 1),
        }
        if analysis.get('patent_cliff_risk') == 'high':
            insights.append("High patent cliff risk — >30% of revenue faces near-term patent expiry. Pipeline strength is critical.")
        else:
            insights.append(f"Patent cliff risk: {analysis.get('patent_cliff_risk', 'unknown')}.")
        insights.append(f"Pipeline contributes {vm['pipeline_contribution_pct']:.0f}% of total estimated enterprise value.")
        if analysis.get('r_and_d_efficiency', 0) >= 4:
            insights.append("R&D efficiency above 4x (pipeline NPV / R&D spend) — strong innovation productivity.")

    elif sector == 'biotech':
        vm = {
            'cash_position': round(analysis.get('cash_position', 0), 1),
            'cash_runway_months': round(analysis.get('cash_runway_months', 0), 1),
            'pipeline_value': round(analysis.get('pipeline_value', 0), 1),
            'rnpv_estimate': round(analysis.get('rnpv_estimate', 0), 1),
            'phase_success_premium_pct': round(analysis.get('phase_success_premium', 0), 0),
        }
        if analysis.get('cash_runway_months', 0) < 18:
            insights.append("Cash runway below 18 months — financing risk is elevated; near-term dilution likely.")
        else:
            insights.append(f"Cash runway of {vm['cash_runway_months']:.0f} months provides adequate operational buffer.")
        insights.append(f"Risk-adjusted NPV estimate: ${vm['rnpv_estimate']:.0f}M based on lead program phase {analysis.get('lead_program_phase', 1)}.")

    elif sector == 'devices':
        vm = {
            'revenue': round(analysis.get('revenue', 0), 1),
            'gross_margin_pct': round(analysis.get('gross_margin', 0), 1),
            'device_quality_score': analysis.get('device_quality_score', 0),
            'regulatory_score': analysis.get('regulatory_score', 0),
            'recurring_revenue_pct': round(analysis.get('recurring_revenue_pct') or 0, 1),
        }
        mr = analysis.get('revenue_multiple_range', {})
        vm['revenue_multiple_mid'] = round(mr.get('mid', 0), 1)
        insights.append(f"Revenue multiple range: {mr.get('low', 0):.1f}x – {mr.get('high', 0):.1f}x based on device quality profile.")
        if analysis.get('device_quality_score', 0) >= 60:
            insights.append("Strong device quality score reflects clinical evidence, regulatory approvals, and reimbursement coverage.")

    elif sector == 'services':
        vm = {
            'revenue': round(analysis.get('revenue', 0), 1),
            'ebitda_margin_pct': round(analysis.get('ebitda_margin', 0), 1),
            'same_store_growth': round(analysis.get('same_store_growth', 0), 1),
            'payor_quality_score': round(analysis.get('payor_quality_score', 0), 1),
            'services_quality_score': analysis.get('services_quality_score', 0),
        }
        mr = analysis.get('ebitda_multiple_range', {})
        vm['ebitda_multiple_mid'] = round(mr.get('mid', 0), 1)
        insights.append(f"EBITDA multiple range: {mr.get('low', 0):.1f}x – {mr.get('high', 0):.1f}x.")
        if analysis.get('same_store_growth', 0) >= 4:
            insights.append("Same-store growth above 4% demonstrates organic volume expansion without acquisition dependency.")

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
        if command == "healthcare":
            if len(sys.argv) < 4:
                raise ValueError("Sector and company data required")

            sector = sys.argv[2]
            company_data = json.loads(sys.argv[3])

            # Determine segment from sector
            if 'pharma' in sector.lower():
                segment = HealthcareSegment.PHARMA
            elif 'biotech' in sector.lower():
                segment = HealthcareSegment.BIOTECH
            elif 'device' in sector.lower():
                segment = HealthcareSegment.MEDICAL_DEVICES
            elif 'service' in sector.lower():
                segment = HealthcareSegment.HEALTHCARE_SERVICES
            elif 'healthtech' in sector.lower() or 'tech' in sector.lower():
                segment = HealthcareSegment.HEALTHTECH
            else:
                segment = HealthcareSegment.PHARMA  # default

            analyzer = HealthcareMetrics(segment)
            sector_key = sector.lower()

            import inspect

            if segment == HealthcareSegment.PHARMA:
                # Frontend sends company-level data — use company-level calculation
                analysis = _calculate_pharma_company_metrics(company_data)

            elif segment == HealthcareSegment.BIOTECH:
                # Frontend sends company-level biotech data — use company-level calculation
                analysis = _calculate_biotech_company_metrics(company_data)
                sector_key = 'biotech'

            elif segment == HealthcareSegment.MEDICAL_DEVICES:
                # Map frontend keys to Python parameter names
                # fda_clearances (int) → regulatory_approvals (List[str])
                if 'fda_clearances' in company_data:
                    n = int(company_data.pop('fda_clearances'))
                    approvals = ['FDA'] if n >= 1 else []
                    if n >= 2:
                        approvals.append('CE_Mark')
                    company_data.setdefault('regulatory_approvals', approvals)

                # gross_margin: normalize to fraction if given as percentage
                if 'gross_margin' in company_data and company_data['gross_margin'] > 1:
                    company_data['gross_margin'] = company_data['gross_margin'] / 100.0

                # recurring_revenue_pct: normalize to fraction
                if 'recurring_revenue_pct' in company_data and company_data['recurring_revenue_pct'] > 1:
                    company_data['recurring_revenue_pct'] = company_data['recurring_revenue_pct'] / 100.0

                # Provide defaults for required params not in frontend form
                company_data.setdefault('regulatory_approvals', ['FDA'])
                company_data.setdefault('clinical_evidence_strength', 60)
                company_data.setdefault('reimbursement_coverage', 70.0)

                # Drop unknown keys
                valid_params = set(inspect.signature(analyzer.calculate_medical_device_metrics).parameters.keys())
                filtered = {k: v for k, v in company_data.items() if k in valid_params}
                analysis = analyzer.calculate_medical_device_metrics(**filtered)
                sector_key = 'devices'

            elif segment == HealthcareSegment.HEALTHCARE_SERVICES:
                # Map frontend keys to Python parameter names
                revenue = float(company_data.get('revenue', 0))
                ebitda_margin_pct = float(company_data.get('ebitda_margin', 0))
                ebitda = revenue * ebitda_margin_pct / 100.0
                same_store_growth = float(company_data.get('same_store_growth', 0))
                commercial_pct = float(company_data.get('payor_mix_commercial', 55)) / 100.0
                # Remaining split between medicare and medicaid
                remaining = max(0.0, 1.0 - commercial_pct)
                payor_mix = {
                    'commercial': commercial_pct,
                    'medicare': remaining * 0.6,
                    'medicaid': remaining * 0.4,
                }
                quality_scores = {'patient_satisfaction': 4.0, 'clinical_quality': 4.0}
                regulatory_compliance_score = 85

                analysis = analyzer.calculate_healthcare_services_metrics(
                    revenue=revenue,
                    ebitda=ebitda,
                    same_store_growth=same_store_growth,
                    payor_mix=payor_mix,
                    quality_scores=quality_scores,
                    regulatory_compliance_score=regulatory_compliance_score,
                )
                sector_key = 'services'

            elif segment == HealthcareSegment.HEALTHTECH:
                valid_params = set(inspect.signature(analyzer.calculate_healthtech_metrics).parameters.keys())
                filtered = {k: v for k, v in company_data.items() if k in valid_params}
                analysis = analyzer.calculate_healthtech_metrics(**filtered)
                sector_key = 'healthtech'

            analysis_with_standard = _build_healthcare_standard_output(analysis, sector_key)
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
