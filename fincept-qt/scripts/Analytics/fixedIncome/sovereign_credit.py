"""
Sovereign and Non-Sovereign Government Credit Analysis
CFA Fixed Income - Sovereign Credit Risk Module

Covers:
- Sovereign credit analysis factors
- Non-sovereign (municipal) government credit
- Comparing government vs corporate bond issuance
- Country risk assessment frameworks

The sovereign / municipal scores are an INDICATIVE linear scorecard built from the CFA factor list
with hand-chosen weights - not an agency rating model and not calibrated to default data.

Usage: python sovereign_credit.py <command> '<json params>'   (same commands and params as cli.py)
"""

import json
import os
import sys
from dataclasses import dataclass, asdict
from typing import Any, List, Dict, Optional, Tuple
from enum import Enum

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fi_common as fc  # noqa: E402
from fi_common import FIError, num  # noqa: E402

SCORECARD_NOTE = ('Indicative linear scorecard (hand-chosen weights on CFA factors); not an agency rating '
                  'and not calibrated to default data')


class SovereignRating(Enum):
    """Sovereign credit ratings"""
    AAA = "AAA"
    AA_PLUS = "AA+"
    AA = "AA"
    AA_MINUS = "AA-"
    A_PLUS = "A+"
    A = "A"
    A_MINUS = "A-"
    BBB_PLUS = "BBB+"
    BBB = "BBB"
    BBB_MINUS = "BBB-"
    BB_PLUS = "BB+"
    BB = "BB"
    BB_MINUS = "BB-"
    B_PLUS = "B+"
    B = "B"
    B_MINUS = "B-"
    CCC = "CCC"
    CC = "CC"
    C = "C"
    D = "D"


class DebtCurrency(Enum):
    """Debt currency classification"""
    LOCAL = "local_currency"
    FOREIGN = "foreign_currency"
    MIXED = "mixed"


@dataclass
class SovereignCreditFactors:
    """Factors for sovereign credit analysis"""
    # Institutional factors
    institutional_effectiveness: float  # 0-100 score
    political_stability: float
    rule_of_law: float
    corruption_index: float

    # Economic factors
    gdp_growth_rate: float  # percentage
    gdp_per_capita: float  # USD
    inflation_rate: float
    unemployment_rate: float
    current_account_balance_gdp: float  # % of GDP

    # Fiscal factors
    government_debt_gdp: float  # % of GDP
    fiscal_balance_gdp: float  # % of GDP
    interest_expense_revenue: float  # % of revenue

    # External factors
    foreign_reserves_months_imports: float
    external_debt_gdp: float
    fx_regime: str  # "floating", "fixed", "managed"
    reserve_currency_issuer: bool


@dataclass
class MunicipalCreditFactors:
    """Factors for municipal/non-sovereign credit analysis"""
    # Revenue factors
    tax_base_diversity: float  # 0-100 score
    revenue_volatility: float
    economic_base_strength: float

    # Debt factors
    debt_per_capita: float
    debt_service_coverage: float
    unfunded_pension_liability: float

    # Management factors
    budget_management: float  # 0-100 score
    reserve_levels: float  # % of budget

    # Governance
    state_support_level: str  # "strong", "moderate", "weak", "none"
    legal_framework: str


class SovereignCreditAnalyzer:
    """
    Comprehensive sovereign credit analysis following CFA curriculum.
    Analyzes ability and willingness to pay.
    """

    def __init__(self):
        # Rating thresholds (simplified scoring model)
        self.rating_thresholds = {
            90: SovereignRating.AAA,
            85: SovereignRating.AA_PLUS,
            80: SovereignRating.AA,
            75: SovereignRating.AA_MINUS,
            70: SovereignRating.A_PLUS,
            65: SovereignRating.A,
            60: SovereignRating.A_MINUS,
            55: SovereignRating.BBB_PLUS,
            50: SovereignRating.BBB,
            45: SovereignRating.BBB_MINUS,
            40: SovereignRating.BB_PLUS,
            35: SovereignRating.BB,
            30: SovereignRating.BB_MINUS,
            25: SovereignRating.B_PLUS,
            20: SovereignRating.B,
            15: SovereignRating.B_MINUS,
            10: SovereignRating.CCC,
            5: SovereignRating.CC,
            0: SovereignRating.C
        }

    def analyze_ability_to_pay(self, factors: SovereignCreditFactors) -> Dict:
        """
        Analyze sovereign's ability to pay based on economic and fiscal factors.

        Returns:
            Dict with ability score and component breakdown
        """
        scores = {}

        # Economic strength (25% weight)
        gdp_score = min(100, max(0, factors.gdp_per_capita / 800))  # $80k = 100
        growth_score = min(100, max(0, (factors.gdp_growth_rate + 2) * 20))  # -2% to 3% range
        inflation_score = max(0, 100 - abs(factors.inflation_rate - 2) * 10)  # 2% target
        unemployment_score = max(0, 100 - factors.unemployment_rate * 5)

        economic_score = (gdp_score * 0.4 + growth_score * 0.2 +
                         inflation_score * 0.2 + unemployment_score * 0.2)
        scores['economic_strength'] = economic_score

        # Fiscal strength (25% weight)
        debt_score = max(0, 100 - factors.government_debt_gdp)
        fiscal_balance_score = min(100, max(0, (factors.fiscal_balance_gdp + 5) * 10))
        interest_burden_score = max(0, 100 - factors.interest_expense_revenue * 4)

        fiscal_score = (debt_score * 0.4 + fiscal_balance_score * 0.3 +
                       interest_burden_score * 0.3)
        scores['fiscal_strength'] = fiscal_score

        # External position (25% weight)
        reserves_score = min(100, factors.foreign_reserves_months_imports * 10)
        external_debt_score = max(0, 100 - factors.external_debt_gdp)
        ca_score = min(100, max(0, (factors.current_account_balance_gdp + 10) * 5))

        external_score = (reserves_score * 0.4 + external_debt_score * 0.3 +
                         ca_score * 0.3)
        scores['external_position'] = external_score

        # Monetary flexibility (25% weight)
        fx_scores = {"floating": 80, "managed": 60, "fixed": 40}
        fx_score = fx_scores.get(factors.fx_regime, 50)
        reserve_currency_bonus = 20 if factors.reserve_currency_issuer else 0

        monetary_score = min(100, fx_score + reserve_currency_bonus)
        scores['monetary_flexibility'] = monetary_score

        # Overall ability score
        overall = (economic_score * 0.25 + fiscal_score * 0.25 +
                  external_score * 0.25 + monetary_score * 0.25)

        return {
            "overall_ability_score": round(overall, 2),
            "component_scores": {k: round(v, 2) for k, v in scores.items()},
            "interpretation": self._interpret_ability_score(overall)
        }

    def analyze_willingness_to_pay(self, factors: SovereignCreditFactors) -> Dict:
        """
        Analyze sovereign's willingness to pay based on institutional factors.

        Returns:
            Dict with willingness score and analysis
        """
        # Institutional quality score
        institutional_score = (
            factors.institutional_effectiveness * 0.3 +
            factors.political_stability * 0.25 +
            factors.rule_of_law * 0.25 +
            (100 - factors.corruption_index) * 0.2  # Lower corruption = higher score
        )

        return {
            "willingness_score": round(institutional_score, 2),
            "components": {
                "institutional_effectiveness": factors.institutional_effectiveness,
                "political_stability": factors.political_stability,
                "rule_of_law": factors.rule_of_law,
                "anti_corruption": 100 - factors.corruption_index
            },
            "interpretation": self._interpret_willingness_score(institutional_score)
        }

    def calculate_sovereign_rating(self, factors: SovereignCreditFactors) -> Dict:
        """
        Calculate implied sovereign credit rating.

        Returns:
            Dict with rating and supporting analysis
        """
        ability = self.analyze_ability_to_pay(factors)
        willingness = self.analyze_willingness_to_pay(factors)

        # Combined score (ability 60%, willingness 40%)
        combined_score = (ability['overall_ability_score'] * 0.6 +
                         willingness['willingness_score'] * 0.4)

        # Determine rating
        implied_rating = SovereignRating.D
        for threshold, rating in sorted(self.rating_thresholds.items(), reverse=True):
            if combined_score >= threshold:
                implied_rating = rating
                break

        # Investment grade threshold
        ig_ratings = [SovereignRating.AAA, SovereignRating.AA_PLUS,
                     SovereignRating.AA, SovereignRating.AA_MINUS,
                     SovereignRating.A_PLUS, SovereignRating.A,
                     SovereignRating.A_MINUS, SovereignRating.BBB_PLUS,
                     SovereignRating.BBB, SovereignRating.BBB_MINUS]

        return {
            "implied_rating": implied_rating.value,
            "model": SCORECARD_NOTE,
            "combined_score": round(combined_score, 2),
            "ability_score": ability['overall_ability_score'],
            "willingness_score": willingness['willingness_score'],
            "investment_grade": implied_rating in ig_ratings,
            "ability_analysis": ability,
            "willingness_analysis": willingness
        }

    def compare_local_vs_foreign_currency_debt(
        self,
        factors: SovereignCreditFactors
    ) -> Dict:
        """
        Compare credit risk for local vs foreign currency sovereign debt.

        Local currency debt:
        - Issuer has monetary policy control
        - Can print money (inflation risk vs default risk)
        - Generally lower default probability

        Foreign currency debt:
        - No monetary control over debt currency
        - FX risk adds to credit risk
        - Higher default probability
        """
        base_rating = self.calculate_sovereign_rating(factors)
        base_score = base_rating['combined_score']

        # Local currency adjustment
        local_adjustment = 0
        if factors.reserve_currency_issuer:
            local_adjustment = 10  # Major reserve currencies almost never default
        elif factors.fx_regime == "floating":
            local_adjustment = 5  # Monetary flexibility

        # Foreign currency adjustment
        foreign_adjustment = 0
        if factors.foreign_reserves_months_imports < 3:
            foreign_adjustment = -10  # Weak reserves
        if factors.external_debt_gdp > 60:
            foreign_adjustment -= 5  # High external debt
        if factors.current_account_balance_gdp < -5:
            foreign_adjustment -= 5  # CA deficit

        local_score = min(100, max(0, base_score + local_adjustment))
        foreign_score = min(100, max(0, base_score + foreign_adjustment))

        return {
            "local_currency_debt": {
                "implied_score": round(local_score, 2),
                "risk_factors": [
                    "Inflation risk if monetized",
                    "Currency depreciation possible",
                    "Lower default probability"
                ],
                "advantages": [
                    "Central bank as lender of last resort",
                    "No FX mismatch risk",
                    "Monetary policy flexibility"
                ]
            },
            "foreign_currency_debt": {
                "implied_score": round(foreign_score, 2),
                "risk_factors": [
                    "FX depreciation increases debt burden",
                    "No monetary policy control",
                    "Dependent on FX reserves",
                    "Higher default probability"
                ],
                "advantages": [
                    "Access to broader investor base",
                    "Often lower nominal yields",
                    "No inflation premium"
                ]
            },
            "notching_difference": round(local_score - foreign_score, 2)
        }

    def analyze_default_restructuring_factors(
        self,
        factors: SovereignCreditFactors,
        has_imf_program: bool = False,
        debt_to_exports_ratio: float = 100
    ) -> Dict:
        """
        Analyze factors affecting sovereign default and restructuring.

        Returns:
            Analysis of default probability drivers and restructuring implications
        """
        warning_signs = []

        # Assess warning indicators
        if factors.government_debt_gdp > 90:
            warning_signs.append("Debt-to-GDP above 90% threshold")
        if factors.fiscal_balance_gdp < -6:
            warning_signs.append("Large fiscal deficit (>6% GDP)")
        if factors.interest_expense_revenue > 20:
            warning_signs.append("Interest expense exceeds 20% of revenue")
        if factors.foreign_reserves_months_imports < 3:
            warning_signs.append("FX reserves below 3 months imports")
        if factors.current_account_balance_gdp < -8:
            warning_signs.append("Large current account deficit (>8% GDP)")
        if debt_to_exports_ratio > 200:
            warning_signs.append("Debt-to-exports ratio above 200%")

        # Restructuring implications
        restructuring_factors = {
            "preferred_creditor_status": {
                "imf": has_imf_program,
                "world_bank": True,  # Always preferred
                "description": "Multilateral debt typically excluded from restructuring"
            },
            "collective_action_clauses": {
                "description": "CACs allow majority creditor approval for restructuring",
                "impact": "Reduces holdout risk"
            },
            "pari_passu_clause": {
                "description": "Equal treatment of creditors",
                "litigation_risk": "Can lead to holdout litigation"
            }
        }

        return {
            "warning_signs": warning_signs,
            "warning_count": len(warning_signs),
            "risk_level": "High" if len(warning_signs) >= 3 else "Moderate" if len(warning_signs) >= 1 else "Low",
            "restructuring_considerations": restructuring_factors,
            "recovery_expectations": {
                "local_currency": "Higher recovery (can inflate away)",
                "foreign_currency": "Typical recovery 40-60 cents on dollar",
                "historical_average": "Approximately 50% recovery rate"
            }
        }

    def _interpret_ability_score(self, score: float) -> str:
        if score >= 80:
            return "Very strong ability to meet obligations"
        elif score >= 60:
            return "Strong ability with some vulnerabilities"
        elif score >= 40:
            return "Moderate ability, susceptible to adverse conditions"
        elif score >= 20:
            return "Weak ability, significant vulnerabilities"
        else:
            return "Very weak ability, high default risk"

    def _interpret_willingness_score(self, score: float) -> str:
        if score >= 80:
            return "Very strong institutional framework supporting willingness"
        elif score >= 60:
            return "Strong institutions with some political risks"
        elif score >= 40:
            return "Moderate institutional strength, political uncertainty"
        elif score >= 20:
            return "Weak institutions, willingness uncertain"
        else:
            return "Very weak institutions, willingness questionable"


class MunicipalCreditAnalyzer:
    """
    Non-sovereign government credit analysis.
    Covers state, provincial, and municipal issuers.
    """

    def analyze_general_obligation_bonds(
        self,
        factors: MunicipalCreditFactors,
        population: int,
        median_income: float
    ) -> Dict:
        """
        Analyze general obligation (GO) bond credit quality.
        GO bonds backed by taxing power of issuer.

        Returns:
            Credit analysis for GO bonds
        """
        # Tax base analysis
        income_per_capita = median_income  # Simplified
        tax_capacity_score = min(100, income_per_capita / 800)  # $80k = 100

        # Debt burden
        debt_burden_score = max(0, 100 - factors.debt_per_capita / 50)  # $5k/capita = 0

        # Management quality: budget_management is a 0-100 score, reserve_levels is % of budget;
        # map reserves to 0-100 (25% of budget or more = 100) before averaging
        reserve_score = min(100.0, max(0.0, factors.reserve_levels * 4))
        management_score = (factors.budget_management + reserve_score) / 2

        # Pension liability impact
        pension_impact = max(0, 100 - factors.unfunded_pension_liability / 1000)  # Per capita

        # Overall GO credit score
        go_score = (
            tax_capacity_score * 0.25 +
            factors.tax_base_diversity * 0.20 +
            debt_burden_score * 0.20 +
            management_score * 0.20 +
            pension_impact * 0.15
        )

        return {
            "go_credit_score": round(go_score, 2),
            "model": SCORECARD_NOTE,
            "population": population,
            "components": {
                "tax_capacity": round(tax_capacity_score, 2),
                "tax_base_diversity": factors.tax_base_diversity,
                "debt_burden": round(debt_burden_score, 2),
                "management_quality": round(management_score, 2),
                "reserve_score": round(reserve_score, 2),
                "pension_impact": round(pension_impact, 2)
            },
            "key_strengths": self._identify_go_strengths(factors, tax_capacity_score),
            "key_risks": self._identify_go_risks(factors)
        }

    def analyze_revenue_bonds(
        self,
        project_type: str,
        debt_service_coverage: float,
        rate_covenant: float,
        additional_bonds_test: bool,
        essentiality: str  # "essential", "important", "discretionary"
    ) -> Dict:
        """
        Analyze revenue bond credit quality.
        Revenue bonds backed by specific project revenues.

        Returns:
            Credit analysis for revenue bonds
        """
        # Coverage analysis
        if debt_service_coverage >= 2.0:
            coverage_score = 100
            coverage_assessment = "Very strong coverage"
        elif debt_service_coverage >= 1.5:
            coverage_score = 80
            coverage_assessment = "Strong coverage"
        elif debt_service_coverage >= 1.25:
            coverage_score = 60
            coverage_assessment = "Adequate coverage"
        elif debt_service_coverage >= 1.0:
            coverage_score = 40
            coverage_assessment = "Thin coverage"
        else:
            coverage_score = 20
            coverage_assessment = "Insufficient coverage"

        # Essentiality scoring
        essentiality_scores = {
            "essential": 100,  # Water, sewer, electric
            "important": 70,   # Transportation, hospitals
            "discretionary": 40  # Sports facilities, convention centers
        }
        essentiality_score = essentiality_scores.get(essentiality, 50)

        # Rate covenant protection
        rate_score = min(100, rate_covenant * 50)  # 2.0x = 100

        # Additional bonds test value
        abt_score = 80 if additional_bonds_test else 50

        # Overall revenue bond score
        revenue_score = (
            coverage_score * 0.35 +
            essentiality_score * 0.25 +
            rate_score * 0.25 +
            abt_score * 0.15
        )

        return {
            "revenue_bond_score": round(revenue_score, 2),
            "model": SCORECARD_NOTE,
            "project_type": project_type,
            "coverage_analysis": {
                "dsc_ratio": debt_service_coverage,
                "score": coverage_score,
                "assessment": coverage_assessment
            },
            "essentiality": {
                "level": essentiality,
                "score": essentiality_score,
                "description": self._describe_essentiality(essentiality)
            },
            "covenant_protection": {
                "rate_covenant": rate_covenant,
                "additional_bonds_test": additional_bonds_test,
                "protection_level": "Strong" if rate_score >= 70 and abt_score >= 70 else "Moderate"
            }
        }

    def compare_go_vs_revenue_bonds(self) -> Dict:
        """
        Compare characteristics of GO vs Revenue bonds.

        Returns:
            Comparative analysis
        """
        return {
            "general_obligation_bonds": {
                "security": "Full faith and credit, taxing power",
                "repayment_source": "General tax revenues",
                "voter_approval": "Usually required",
                "credit_factors": [
                    "Tax base breadth and diversity",
                    "Debt burden per capita",
                    "Economic base strength",
                    "Management practices",
                    "Reserve levels"
                ],
                "typical_uses": [
                    "Schools",
                    "Government buildings",
                    "Parks and recreation"
                ]
            },
            "revenue_bonds": {
                "security": "Specific project revenues only",
                "repayment_source": "User fees, tolls, charges",
                "voter_approval": "Usually not required",
                "credit_factors": [
                    "Debt service coverage ratio",
                    "Rate covenants",
                    "Additional bonds test",
                    "Service area characteristics",
                    "Management and operations"
                ],
                "typical_uses": [
                    "Water and sewer systems",
                    "Electric utilities",
                    "Toll roads and bridges",
                    "Airports",
                    "Hospitals"
                ]
            },
            "key_differences": {
                "issuer_obligation": "GO: Full recourse | Revenue: Limited to project",
                "credit_support": "GO: Taxing power | Revenue: Revenue stream",
                "typical_rating": "GO bonds generally higher rated",
                "yield_relationship": "Revenue bonds typically higher yield"
            }
        }

    def analyze_state_support(
        self,
        factors: MunicipalCreditFactors,
        state_rating: str,
        state_aid_percentage: float,
        intercept_program: bool
    ) -> Dict:
        """
        Analyze state support for local government bonds.

        Returns:
            State support analysis and impact on credit
        """
        support_levels = {
            "strong": {
                "description": "State provides explicit guarantees or strong intercept programs",
                "rating_impact": "+2 notches potential",
                "examples": ["Texas PSF", "Virginia moral obligation"]
            },
            "moderate": {
                "description": "State provides significant aid but limited direct support",
                "rating_impact": "+1 notch potential",
                "examples": ["State aid formulas", "Revenue sharing"]
            },
            "weak": {
                "description": "Limited state involvement in local finances",
                "rating_impact": "Neutral",
                "examples": ["Local control states"]
            },
            "none": {
                "description": "No state support mechanisms",
                "rating_impact": "Stand-alone credit",
                "examples": ["Legally separate entities"]
            }
        }

        support_info = support_levels.get(
            factors.state_support_level,
            support_levels["weak"]
        )

        # Calculate support score
        support_score = 0
        if factors.state_support_level == "strong":
            support_score = 30
        elif factors.state_support_level == "moderate":
            support_score = 15
        elif factors.state_support_level == "weak":
            support_score = 5

        # Aid dependency analysis
        if state_aid_percentage > 50:
            aid_dependency = "High"
            risk_note = "Significant exposure to state budget decisions"
        elif state_aid_percentage > 25:
            aid_dependency = "Moderate"
            risk_note = "Some exposure to state aid variability"
        else:
            aid_dependency = "Low"
            risk_note = "Limited state aid dependency"

        return {
            "state_support_level": factors.state_support_level,
            "support_description": support_info,
            "state_rating": state_rating,
            "state_aid_analysis": {
                "aid_percentage": state_aid_percentage,
                "dependency_level": aid_dependency,
                "risk_note": risk_note
            },
            "intercept_program": {
                "available": intercept_program,
                "impact": "Provides additional credit support" if intercept_program else "N/A"
            },
            "credit_enhancement_score": support_score
        }

    def _identify_go_strengths(
        self,
        factors: MunicipalCreditFactors,
        tax_capacity: float
    ) -> List[str]:
        strengths = []
        if tax_capacity >= 70:
            strengths.append("Strong tax base and income levels")
        if factors.tax_base_diversity >= 70:
            strengths.append("Diversified tax base reduces volatility")
        if factors.reserve_levels >= 20:
            strengths.append("Healthy reserve levels")
        if factors.debt_service_coverage >= 2.0:
            strengths.append("Strong debt service coverage")
        if factors.budget_management >= 70:
            strengths.append("Sound financial management practices")
        return strengths if strengths else ["No standout strengths identified"]

    def _identify_go_risks(self, factors: MunicipalCreditFactors) -> List[str]:
        risks = []
        if factors.unfunded_pension_liability > 5000:
            risks.append("Significant unfunded pension liabilities")
        if factors.reserve_levels < 10:
            risks.append("Low reserve levels")
        if factors.revenue_volatility > 50:
            risks.append("High revenue volatility")
        if factors.economic_base_strength < 50:
            risks.append("Weak economic base")
        if factors.tax_base_diversity < 40:
            risks.append("Concentrated tax base")
        return risks if risks else ["No major risks identified"]

    def _describe_essentiality(self, essentiality: str) -> str:
        descriptions = {
            "essential": "Service is critical to public health/safety, demand is inelastic",
            "important": "Service is valuable but alternatives may exist",
            "discretionary": "Service is optional, demand is elastic to economic conditions"
        }
        return descriptions.get(essentiality, "Unknown essentiality level")


class GovernmentVsCorporateComparison:
    """
    Compare government and corporate bond characteristics.
    """

    def compare_issuance_characteristics(self) -> Dict:
        """
        Compare government vs corporate bond issuance.

        Returns:
            Comprehensive comparison
        """
        return {
            "government_bonds": {
                "issuers": [
                    "Sovereign governments (Treasuries, Gilts, Bunds)",
                    "Government agencies (Fannie Mae, Freddie Mac)",
                    "State and local governments (Munis)",
                    "Supranational organizations (World Bank, IMF)"
                ],
                "credit_characteristics": {
                    "default_risk": "Generally lower, especially for developed market sovereigns",
                    "recovery_rates": "Variable, depends on restructuring",
                    "taxing_power": "Unique ability to raise revenue through taxation",
                    "monetary_policy": "Sovereigns can print local currency"
                },
                "market_characteristics": {
                    "liquidity": "Very high for major sovereign bonds",
                    "benchmark_status": "Often serve as risk-free rate benchmark",
                    "market_size": "Largest segment of global bond markets"
                },
                "tax_treatment": {
                    "federal": "Sovereign interest typically taxable",
                    "muni_advantage": "US munis often tax-exempt at federal/state level"
                }
            },
            "corporate_bonds": {
                "issuers": [
                    "Investment grade corporations",
                    "High yield issuers",
                    "Financial institutions",
                    "Utilities"
                ],
                "credit_characteristics": {
                    "default_risk": "Varies widely by rating and industry",
                    "recovery_rates": "Generally 40-60% for senior unsecured",
                    "asset_backing": "Claims on corporate assets in bankruptcy",
                    "covenants": "Contractual protections for bondholders"
                },
                "market_characteristics": {
                    "liquidity": "Varies by issue size and credit quality",
                    "spread_trading": "Priced as spread to government bonds",
                    "credit_analysis": "Fundamental analysis of business/financials"
                },
                "tax_treatment": {
                    "interest": "Fully taxable at ordinary income rates",
                    "capital_gains": "Subject to capital gains tax"
                }
            },
            "key_differences": {
                "source_of_repayment": {
                    "government": "Tax revenues, monetary flexibility",
                    "corporate": "Business cash flows and asset values"
                },
                "bankruptcy_process": {
                    "government": "Sovereign restructuring (ad hoc), Chapter 9 for munis",
                    "corporate": "Chapter 11 bankruptcy with clear priority structure"
                },
                "credit_spreads": {
                    "government": "Benchmark (spread = 0 for risk-free sovereign)",
                    "corporate": "Positive spread reflecting credit/liquidity risk"
                }
            }
        }

    def analyze_relative_value(
        self,
        sovereign_yield: float,
        corporate_spread: float,
        muni_yield: float,
        tax_rate: float
    ) -> Dict:
        """
        Compare relative value across government and corporate bonds.

        Args:
            sovereign_yield: Risk-free sovereign yield
            corporate_spread: Corporate bond spread over sovereign
            muni_yield: Tax-exempt municipal yield
            tax_rate: Investor's marginal tax rate

        Returns:
            Relative value analysis
        """
        if not 0 <= tax_rate < 1:
            raise FIError('tax_rate must be >= 0 and < 1 (decimal, e.g. 0.35)')
        corporate_yield = sovereign_yield + corporate_spread

        # Tax-equivalent municipal yield
        muni_taxable_equivalent = muni_yield / (1 - tax_rate)

        # Muni ratio (muni yield / Treasury yield)
        muni_ratio = muni_yield / sovereign_yield if sovereign_yield > 0 else None

        # Breakeven tax rate (where muni = taxable)
        breakeven_tax_rate = 1 - (muni_yield / corporate_yield) if corporate_yield > 0 else None

        return {
            "yields": {
                "sovereign": round(sovereign_yield, 6),
                "corporate": round(corporate_yield, 6),
                "municipal_nominal": round(muni_yield, 6),
                "municipal_taxable_equivalent": round(muni_taxable_equivalent, 6)
            },
            "analysis": {
                "corporate_vs_sovereign_spread": round(corporate_spread, 6),
                "muni_ratio": round(muni_ratio * 100, 2) if muni_ratio is not None else None,
                "historical_muni_ratio_avg": 80.0,
                "historical_muni_ratio_note": "assumed typical long-run level, not computed from data",
                "muni_appears": (None if muni_ratio is None else
                                 "Cheap" if muni_ratio > 0.85 else "Fair" if muni_ratio > 0.75 else "Rich")
            },
            "tax_analysis": {
                "investor_tax_rate": round(tax_rate * 100, 4),
                "breakeven_tax_rate": round(breakeven_tax_rate * 100, 2) if breakeven_tax_rate is not None else None,
                "muni_advantaged": muni_taxable_equivalent > corporate_yield
            },
            "recommendation": self._relative_value_recommendation(
                muni_taxable_equivalent, corporate_yield, tax_rate
            )
        }

    def _relative_value_recommendation(
        self,
        muni_te_yield: float,
        corporate_yield: float,
        tax_rate: float
    ) -> str:
        if tax_rate < 0.22:
            return "Low tax rate - taxable bonds likely more attractive"
        elif muni_te_yield > corporate_yield * 1.1:
            return "Munis offer significant tax advantage at current levels"
        elif muni_te_yield > corporate_yield:
            return "Munis marginally attractive on tax-equivalent basis"
        else:
            return "Corporate bonds offer better risk-adjusted value"


_SOVEREIGN_DEFAULTS = {
    'institutional_effectiveness': 70, 'political_stability': 70, 'rule_of_law': 70, 'corruption_index': 30,
    'gdp_growth_rate': 2.0, 'gdp_per_capita': 50000, 'inflation_rate': 2.0, 'unemployment_rate': 5.0,
    'current_account_balance_gdp': 0, 'government_debt_gdp': 60, 'fiscal_balance_gdp': -3,
    'interest_expense_revenue': 10, 'foreign_reserves_months_imports': 6, 'external_debt_gdp': 40,
}
_SCORE_FIELDS = ('institutional_effectiveness', 'political_stability', 'rule_of_law', 'corruption_index')

_MUNICIPAL_DEFAULTS = {
    'tax_base_diversity': 70, 'revenue_volatility': 30, 'economic_base_strength': 70, 'debt_per_capita': 2000,
    'debt_service_coverage': 2.0, 'unfunded_pension_liability': 3000, 'budget_management': 75, 'reserve_levels': 15,
}


def build_sovereign_factors(f: Any) -> SovereignCreditFactors:
    """SovereignCreditFactors from a JSON object; missing fields take neutral defaults."""
    f = f if f is not None else {}
    if not isinstance(f, dict):
        raise FIError('factors must be a JSON object')
    values = {}
    for k, d in _SOVEREIGN_DEFAULTS.items():
        bounds = dict(lo=0, hi=100) if k in _SCORE_FIELDS else {}
        values[k] = num(f, k, d, **bounds)
    values['fx_regime'] = fc.text(f, 'fx_regime', 'floating', choices=('floating', 'managed', 'fixed'))
    values['reserve_currency_issuer'] = fc.boolean(f, 'reserve_currency_issuer', False)
    return SovereignCreditFactors(**values)


def build_municipal_factors(f: Any) -> MunicipalCreditFactors:
    """MunicipalCreditFactors from a JSON object; missing fields take neutral defaults."""
    f = f if f is not None else {}
    if not isinstance(f, dict):
        raise FIError('factors must be a JSON object')
    values = {k: num(f, k, d) for k, d in _MUNICIPAL_DEFAULTS.items()}
    for k in ('tax_base_diversity', 'budget_management'):
        if not 0 <= values[k] <= 100:
            raise FIError(f'{k} must be a 0-100 score')
    values['state_support_level'] = fc.text(f, 'state_support_level', 'moderate',
                                            choices=('strong', 'moderate', 'weak', 'none'))
    values['legal_framework'] = fc.text(f, 'legal_framework', 'strong')
    return MunicipalCreditFactors(**values)


ANALYSIS_TYPES = ('sovereign_rating', 'sovereign_ability_to_pay', 'sovereign_willingness', 'local_vs_foreign_debt',
                  'default_restructuring', 'municipal_go', 'municipal_revenue', 'go_vs_revenue',
                  'govt_vs_corporate', 'relative_value')

# command names used by this file's own CLI before it shared cli.py's names
_LEGACY_COMMANDS = {
    'analyze_sovereign': 'sovereign_rating', 'compare_local_foreign': 'local_vs_foreign_debt',
    'analyze_municipal_go': 'municipal_go', 'analyze_revenue_bond': 'municipal_revenue',
    'compare_go_revenue': 'go_vs_revenue', 'compare_govt_corporate': 'govt_vs_corporate',
}


def run_sovereign_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Sovereign / municipal credit dispatcher (params['analysis_type'] in ANALYSIS_TYPES)."""
    analysis_type = params.get('analysis_type', '')
    sov = SovereignCreditAnalyzer()
    muni = MunicipalCreditAnalyzer()
    comparison = GovernmentVsCorporateComparison()
    try:
        if analysis_type == 'sovereign_rating':
            return sov.calculate_sovereign_rating(build_sovereign_factors(params.get('factors')))
        if analysis_type == 'sovereign_ability_to_pay':
            out = sov.analyze_ability_to_pay(build_sovereign_factors(params.get('factors')))
            out['model'] = SCORECARD_NOTE
            return out
        if analysis_type == 'sovereign_willingness':
            out = sov.analyze_willingness_to_pay(build_sovereign_factors(params.get('factors')))
            out['model'] = SCORECARD_NOTE
            return out
        if analysis_type == 'local_vs_foreign_debt':
            out = sov.compare_local_vs_foreign_currency_debt(build_sovereign_factors(params.get('factors')))
            out['model'] = SCORECARD_NOTE
            return out
        if analysis_type == 'default_restructuring':
            return sov.analyze_default_restructuring_factors(
                build_sovereign_factors(params.get('factors')),
                fc.boolean(params, 'has_imf_program', False),
                num(params, 'debt_to_exports', 100, aliases=('debt_to_exports_ratio',), lo=0))
        if analysis_type == 'municipal_go':
            return muni.analyze_general_obligation_bonds(
                build_municipal_factors(params.get('factors')),
                num(params, 'population', 100000, lo=0, lo_open=True),
                num(params, 'median_income', 50000, lo=0))
        if analysis_type == 'municipal_revenue':
            return muni.analyze_revenue_bonds(
                project_type=fc.text(params, 'project_type', 'Water System', lower=False),
                debt_service_coverage=num(params, 'debt_service_coverage', 1.5, lo=0),
                rate_covenant=num(params, 'rate_covenant', 1.25, lo=0),
                additional_bonds_test=fc.boolean(params, 'additional_bonds_test', True),
                essentiality=fc.text(params, 'essentiality', 'essential',
                                     choices=('essential', 'important', 'discretionary')))
        if analysis_type == 'go_vs_revenue':
            return muni.compare_go_vs_revenue_bonds()
        if analysis_type == 'govt_vs_corporate':
            return comparison.compare_issuance_characteristics()
        if analysis_type == 'relative_value':
            return comparison.analyze_relative_value(
                sovereign_yield=num(params, 'sovereign_yield', 0.04),
                corporate_spread=num(params, 'corporate_spread', 0.015),
                muni_yield=num(params, 'muni_yield', 0.035),
                tax_rate=num(params, 'tax_rate', 0.35))
        return {'error': f'Unknown sovereign analysis type: {analysis_type}', 'analysis_types': list(ANALYSIS_TYPES)}
    except FIError as e:
        return {'error': str(e)}
    except Exception as e:
        return {'error': str(e) or type(e).__name__}


def main(argv=None):
    """python sovereign_credit.py <command> '<json params>' -> one JSON payload (inputs are honoured)."""
    fc.import_fincept_json()
    import fincept_json
    argv = sys.argv if argv is None else argv
    commands = list(ANALYSIS_TYPES) + list(_LEGACY_COMMANDS)
    if len(argv) < 2:
        fincept_json.emit({'error': "usage: python sovereign_credit.py <command> '<json params>'",
                           'available_commands': commands})
        return
    command = _LEGACY_COMMANDS.get(argv[1], argv[1])
    if command not in ANALYSIS_TYPES:
        fincept_json.emit({'error': f'Unknown command: {argv[1]}', 'available_commands': commands})
        return
    params: Any = {}
    if len(argv) > 2:
        try:
            params = json.loads(argv[2])
        except ValueError as e:
            fincept_json.emit({'error': f'Invalid JSON parameters: {e}'})
            return
    if not isinstance(params, dict):
        fincept_json.emit({'error': 'params must be a JSON object'})
        return
    params = dict(params)
    params['analysis_type'] = command
    fincept_json.emit(run_sovereign_analysis(params))


if __name__ == "__main__":
    main()
