"""Risk Factor Summation Method"""
from typing import Dict, Any
from enum import Enum

class RiskFactor(Enum):
    """12 standard risk factors"""
    MANAGEMENT = "management"
    STAGE_OF_BUSINESS = "stage_of_business"
    LEGISLATION_POLITICAL = "legislation_political"
    MANUFACTURING = "manufacturing"
    SALES_MARKETING = "sales_marketing"
    FUNDING_CAPITAL = "funding_capital"
    COMPETITION = "competition"
    TECHNOLOGY = "technology"
    LITIGATION = "litigation"
    INTERNATIONAL = "international"
    REPUTATION = "reputation"
    LUCRATIVE_EXIT = "lucrative_exit"

class RiskFactorSummation:
    """Risk Factor Summation Method for startup valuation"""

    def __init__(self, base_valuation: float, adjustment_per_factor: float = 250_000):
        self.base_valuation = base_valuation
        self.adjustment_per_factor = adjustment_per_factor

        self.risk_descriptions = {
            RiskFactor.MANAGEMENT: "Quality and experience of management team",
            RiskFactor.STAGE_OF_BUSINESS: "Development stage and operational maturity",
            RiskFactor.LEGISLATION_POLITICAL: "Regulatory and political risk exposure",
            RiskFactor.MANUFACTURING: "Production and supply chain risk",
            RiskFactor.SALES_MARKETING: "Go-to-market and customer acquisition risk",
            RiskFactor.FUNDING_CAPITAL: "Future financing and capital needs risk",
            RiskFactor.COMPETITION: "Competitive intensity and market dynamics",
            RiskFactor.TECHNOLOGY: "Technical execution and IP protection risk",
            RiskFactor.LITIGATION: "Legal disputes and liability exposure",
            RiskFactor.INTERNATIONAL: "Cross-border and currency risk",
            RiskFactor.REPUTATION: "Brand and reputational risk",
            RiskFactor.LUCRATIVE_EXIT: "Exit opportunity and liquidity risk"
        }

    def assess_risk_factor(self, factor: RiskFactor, risk_level: int) -> float:
        """
        Assess single risk factor

        Args:
            factor: RiskFactor enum
            risk_level: -2 (very high risk) to +2 (very low risk, adds value)
                       0 = neutral/average risk

        Returns:
            Valuation adjustment for this factor
        """

        if not -2 <= risk_level <= 2:
            raise ValueError("Risk level must be between -2 and +2")

        return risk_level * self.adjustment_per_factor

    def calculate_valuation(self, risk_assessments: Dict[RiskFactor, int]) -> Dict[str, Any]:
        """
        Calculate valuation using risk factor summation

        Args:
            risk_assessments: Dict mapping RiskFactor to risk_level (-2 to +2)

        Returns:
            Complete valuation breakdown
        """

        factor_adjustments = {}
        total_adjustment = 0

        for factor in RiskFactor:
            risk_level = risk_assessments.get(factor, 0)
            adjustment = self.assess_risk_factor(factor, risk_level)

            factor_adjustments[factor.value] = {
                'risk_level': risk_level,
                'adjustment': adjustment,
                'description': self.risk_descriptions[factor]
            }

            total_adjustment += adjustment

        final_valuation = self.base_valuation + total_adjustment

        return {
            'method': 'Risk Factor Summation',
            'base_valuation': self.base_valuation,
            'risk_assessments': factor_adjustments,
            'total_adjustment': total_adjustment,
            'final_valuation': max(0, final_valuation),
            'adjustment_pct': (total_adjustment / self.base_valuation * 100) if self.base_valuation else 0
        }

    def quick_assessment(self, base_valuation: float,
                        management_risk: int, stage_risk: int, technology_risk: int,
                        competition_risk: int, market_risk: int, funding_risk: int) -> Dict[str, Any]:
        """Quick assessment with key risk factors"""

        assessments = {
            RiskFactor.MANAGEMENT: management_risk,
            RiskFactor.STAGE_OF_BUSINESS: stage_risk,
            RiskFactor.TECHNOLOGY: technology_risk,
            RiskFactor.COMPETITION: competition_risk,
            RiskFactor.SALES_MARKETING: market_risk,
            RiskFactor.FUNDING_CAPITAL: funding_risk,
            RiskFactor.LEGISLATION_POLITICAL: 0,
            RiskFactor.MANUFACTURING: 0,
            RiskFactor.LITIGATION: 0,
            RiskFactor.INTERNATIONAL: 0,
            RiskFactor.REPUTATION: 0,
            RiskFactor.LUCRATIVE_EXIT: 0
        }

        self.base_valuation = base_valuation
        return self.calculate_valuation(assessments)

    def risk_scoring_guide(self) -> Dict[str, Any]:
        """Get risk scoring guidelines"""

        return {
            'risk_levels': {
                '+2': 'Very Low Risk / Significant Positive (+$500K)',
                '+1': 'Below Average Risk / Positive (+$250K)',
                '0': 'Average Risk / Neutral ($0)',
                '-1': 'Above Average Risk / Negative (-$250K)',
                '-2': 'Very High Risk / Significant Negative (-$500K)'
            },
            'factor_guidelines': {
                'Management': {
                    '+2': 'Serial successful entrepreneurs, complete A-team',
                    '+1': 'Strong relevant experience, mostly complete team',
                    '0': 'Some relevant experience, gaps in team',
                    '-1': 'Limited experience, significant gaps',
                    '-2': 'First-time founders, incomplete team'
                },
                'Technology': {
                    '+2': 'Proven technology, strong IP protection',
                    '+1': 'Working prototype, some IP',
                    '0': 'Early-stage technology, uncertain IP',
                    '-1': 'Unproven technology, weak IP',
                    '-2': 'Technology risk very high, no IP'
                },
                'Competition': {
                    '+2': 'Clear differentiation, high barriers',
                    '+1': 'Good positioning, some barriers',
                    '0': 'Competitive market, moderate barriers',
                    '-1': 'Intense competition, low barriers',
                    '-2': 'Dominated by incumbents, no barriers'
                }
            }
        }

    def sensitivity_analysis(self, base_assessments: Dict[RiskFactor, int],
                           variable_factor: RiskFactor) -> Dict[str, Any]:
        """Analyze valuation sensitivity to single risk factor"""

        results = []

        for risk_level in [-2, -1, 0, 1, 2]:
            test_assessments = base_assessments.copy()
            test_assessments[variable_factor] = risk_level

            valuation = self.calculate_valuation(test_assessments)

            results.append({
                'risk_level': risk_level,
                'valuation': valuation['final_valuation'],
                'adjustment': valuation['total_adjustment']
            })

        return {
            'variable_factor': variable_factor.value,
            'sensitivity_data': results,
            'base_valuation': self.base_valuation
        }

if __name__ == '__main__':
    rfs = RiskFactorSummation(base_valuation=2_000_000, adjustment_per_factor=250_000)

    valuation = rfs.quick_assessment(
        base_valuation=2_000_000,
        management_risk=1,
        stage_risk=-1,
        technology_risk=0,
        competition_risk=-1,
        market_risk=0,
        funding_risk=-1
    )

    print("=== RISK FACTOR SUMMATION VALUATION ===\n")
    print(f"Base Valuation: ${valuation['base_valuation']:,.0f}")
    print(f"Total Adjustment: ${valuation['total_adjustment']:,} ({valuation['adjustment_pct']:+.1f}%)")
    print(f"Final Valuation: ${valuation['final_valuation']:,.0f}\n")

    print("Risk Factor Assessments:")
    for factor, data in valuation['risk_assessments'].items():
        if data['adjustment'] != 0:
            print(f"  {factor.replace('_', ' ').title()}: {data['risk_level']:+d} (${data['adjustment']:+,})")

    guide = rfs.risk_scoring_guide()
    print("\n=== RISK SCORING GUIDE ===")
    for level, desc in guide['risk_levels'].items():
        print(f"{level}: {desc}")
