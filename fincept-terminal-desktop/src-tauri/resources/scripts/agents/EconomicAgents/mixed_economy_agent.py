"""
Mixed Economy Economic Agent
Processes economic data through the lens of mixed economy principles combining market and government elements
"""

from typing import Dict, List, Any
from base_agent import EconomicAgent, EconomicData, PolicyRecommendation

class MixedEconomyAgent(EconomicAgent):
    """Mixed economy agent balancing private enterprise with government intervention"""

    def __init__(self):
        super().__init__(
            "Mixed Economy Agent",
            "Analyzes economic data through mixed economy principles balancing market efficiency with social welfare"
        )
        self.principles = [
            "Balance of private and public sectors",
            "Market efficiency with government regulation",
            "Economic freedom with social responsibility",
            "Competition with safety nets",
            "Innovation with stability",
            "Individual prosperity with collective welfare"
        ]
        self.priorities = [
            "Economic stability",
            "Balanced growth",
            "Social safety net",
            "Market efficiency",
            "Regulatory framework",
            "Inclusive prosperity"
        ]

    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        """Analyze economy from mixed economy perspective"""

        # Calculate mixed economy specific metrics
        balance_score = self._calculate_balance_score(data)
        stability_score = self._calculate_stability_score(data)
        efficiency_equity_ratio = self._calculate_efficiency_equity_ratio(data)
        regulatory_effectiveness = self._calculate_regulatory_effectiveness(data)

        # Identify strengths and concerns
        strengths = []
        concerns = []

        # Market strengths
        if data.gdp > 2.5:
            strengths.append("Healthy GDP growth indicating productive market activity")
        if data.private_investment > 15 and data.private_investment < 35:
            strengths.append("Balanced private investment level")
        if data.unemployment < 6:
            strengths.append("Reasonable unemployment showing effective labor market")

        # Government strengths
        if data.government_spending > 20 and data.government_spending < 40:
            strengths.append("Balanced government spending providing stability without crowding out")
        if data.tax_rate > 20 and data.tax_rate < 40:
            strengths.append("Moderate tax burden supporting public services")
        if 1 < data.inflation < 4:
            strengths.append("Controlled inflation indicating effective monetary policy")

        # Imbalance concerns
        if data.government_spending > 50:
            concerns.append("Excessive government spending may crowd out private sector")
        if data.government_spending < 15:
            concerns.append("Insufficient government spending may lack social safety nets")
        if data.private_investment > 50:
            concerns.append("Over-reliance on private sector may increase inequality")
        if data.private_investment < 10:
            concerns.append("Insufficient private investment may limit growth and innovation")
        if data.tax_rate > 50:
            concerns.append("High tax rates may discourage private enterprise")
        if data.tax_rate < 15:
            concerns.append("Low tax rates may inadequately fund public services")

        # Calculate overall mixed economy health score
        health_score = (
            balance_score * 0.3 +
            stability_score * 0.25 +
            efficiency_equity_ratio * 0.25 +
            regulatory_effectiveness * 0.2
        )

        return {
            "ideology": "mixed_economy",
            "balance_score": balance_score,
            "stability_score": stability_score,
            "efficiency_equity_ratio": efficiency_equity_ratio,
            "regulatory_effectiveness": regulatory_effectiveness,
            "overall_health_score": health_score,
            "strengths": strengths,
            "concerns": concerns,
            "assessment": self._generate_assessment(health_score, strengths, concerns),
            "key_indicators": {
                "public_private_balance": abs(50 - data.government_spending),
                "gdp_growth": data.gdp,
                "unemployment_rate": data.unemployment,
                "inflation_rate": data.inflation,
                "tax_burden": data.tax_rate,
                "private_investment_share": data.private_investment
            }
        }

    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        """Generate mixed economy policy recommendations"""
        recommendations = []

        # Balance correction recommendations
        if data.government_spending > 45:
            recommendations.append(PolicyRecommendation(
                title="Reduce Government Spending",
                description="Moderate reduction in government expenditure to improve private sector participation",
                priority="medium",
                expected_impact="Improved economic efficiency and growth",
                implementation_time="12-24 months",
                risk_level="medium"
            ))
        elif data.government_spending < 20:
            recommendations.append(PolicyRecommendation(
                title="Increase Government Spending",
                description="Targeted increase in government spending on essential services and infrastructure",
                priority="medium",
                expected_impact="Enhanced social welfare and economic stability",
                implementation_time="12-24 months",
                risk_level="medium"
            ))

        # Tax balance recommendations
        if data.tax_rate > 45:
            recommendations.append(PolicyRecommendation(
                title="Reform Tax System",
                description="Implement moderate tax cuts while maintaining progressive structure",
                priority="medium",
                expected_impact="Balanced revenue generation with economic incentives",
                implementation_time="6-18 months",
                risk_level="low"
            ))
        elif data.tax_rate < 20:
            recommendations.append(PolicyRecommendation(
                title="Enhance Tax Revenue",
                description="Implement progressive tax reforms to fund essential services",
                priority="medium",
                expected_impact="Improved public service provision",
                implementation_time="6-12 months",
                risk_level="low"
            ))

        # Regulatory recommendations
        recommendations.append(PolicyRecommendation(
            title="Optimize Regulatory Framework",
            description="Review and streamline regulations to balance protection with efficiency",
            priority="medium",
            expected_impact="Improved business climate with necessary protections",
            implementation_time="12-30 months",
            risk_level="low"
        ))

        # Social safety net recommendations
        if data.unemployment > 7:
            recommendations.append(PolicyRecommendation(
                title="Strengthen Social Safety Net",
                description="Enhance unemployment benefits and job training programs",
                priority="high",
                expected_impact="Reduced hardship and better workforce preparedness",
                implementation_time="6-18 months",
                risk_level="low"
            ))

        # Infrastructure investment recommendations
        recommendations.append(PolicyRecommendation(
            title="Strategic Infrastructure Investment",
            description="Public-private partnerships for infrastructure development",
            priority="medium",
            expected_impact="Improved productivity and competitiveness",
            implementation_time="24-60 months",
                risk_level="medium"
        ))

        # Economic stabilization recommendations
        if data.inflation > 5:
            recommendations.append(PolicyRecommendation(
                title="Implement Anti-Inflation Measures",
                description="Monetary and fiscal policies to control inflation",
                priority="high",
                expected_impact="Price stability and purchasing power protection",
                implementation_time="3-12 months",
                risk_level="medium"
            ))

        # Innovation and entrepreneurship recommendations
        if data.private_investment < 15:
            recommendations.append(PolicyRecommendation(
                title="Promote Innovation and Entrepreneurship",
                description="Government support for R&D and small business development",
                priority="medium",
                expected_impact="Increased innovation and job creation",
                implementation_time="12-24 months",
                risk_level="low"
            ))

        return recommendations

    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Evaluate policy from mixed economy perspective"""
        policy_name = policy.get("name", "Unknown Policy")
        policy_type = policy.get("type", "unknown")

        # Scoring criteria for mixed economy evaluation
        scores = {
            "economic_efficiency": 0,  # How it affects market efficiency
            "social_welfare": 0,  # How it affects social welfare
            "economic_balance": 0,  # How it maintains public-private balance
            "economic_stability": 0,  # How it affects economic stability
            "long_term_growth": 0  # How it affects sustainable growth
        }

        # Evaluate based on policy type
        if policy_type == "moderate_tax_change":
            scores["economic_balance"] = 85
            scores["economic_stability"] = 80
            scores["long_term_growth"] = 75
        elif policy_type == "infrastructure_investment":
            scores["economic_efficiency"] = 80
            scores["long_term_growth"] = 85
            scores["social_welfare"] = 70
        elif policy_type == "education_spending":
            scores["social_welfare"] = 85
            scores["long_term_growth"] = 80
            scores["economic_efficiency"] = 75
        elif policy_type == "healthcare_reform":
            scores["social_welfare"] = 90
            scores["economic_stability"] = 80
            scores["economic_balance"] = 75
        elif policy_type == "extreme_austerity":
            scores["economic_balance"] = -40
            scores["social_welfare"] = -60
            scores["economic_stability"] = -30
        elif policy_type == "excessive_spending":
            scores["economic_balance"] = -50
            scores["economic_efficiency"] = -40
            scores["economic_stability"] = -35
        elif policy_type == "deregulation":
            scores["economic_efficiency"] = 75
            scores["economic_balance"] = 50
            scores["social_welfare"] = 40

        overall_score = sum(scores.values()) / len(scores)

        return {
            "policy_name": policy_name,
            "mixed_economy_score": overall_score,
            "detailed_scores": scores,
            "recommendation": self._get_policy_recommendation(overall_score),
            "justification": self._generate_policy_justification(scores, policy_type)
        }

    def _calculate_balance_score(self, data: EconomicData) -> float:
        """Calculate public-private sector balance score (0-100)"""
        # Ideal balance is around 40% government spending, 60% private
        ideal_gov_spending = 40
        balance_penalty = abs(data.government_spending - ideal_gov_spending) * 2
        balance_score = max(100 - balance_penalty, 0)

        # Tax balance (ideal around 30%)
        ideal_tax_rate = 30
        tax_penalty = abs(data.tax_rate - ideal_tax_rate) * 1.5
        tax_score = max(100 - tax_penalty, 0)

        # Investment balance (ideal around 25% private)
        ideal_private_investment = 25
        investment_penalty = abs(data.private_investment - ideal_private_investment) * 2
        investment_score = max(100 - investment_penalty, 0)

        return (balance_score + tax_score + investment_score) / 3

    def _calculate_stability_score(self, data: EconomicData) -> float:
        """Calculate economic stability score (0-100)"""
        # Inflation stability (ideal 2-3%)
        if 2 <= data.inflation <= 3:
            inflation_score = 100
        else:
            inflation_score = max(100 - abs(data.inflation - 2.5) * 15, 0)

        # Unemployment stability (ideal 4-6%)
        if 4 <= data.unemployment <= 6:
            unemployment_score = 100
        else:
            unemployment_score = max(100 - abs(data.unemployment - 5) * 10, 0)

        # GDP growth stability (ideal 2-4%)
        if 2 <= data.gdp <= 4:
            gdp_score = 100
        else:
            gdp_score = max(100 - abs(data.gdp - 3) * 10, 0)

        return (inflation_score + unemployment_score + gdp_score) / 3

    def _calculate_efficiency_equity_ratio(self, data: EconomicData) -> float:
        """Calculate efficiency-equity balance ratio (0-100)"""
        # Efficiency indicators (private investment, moderate taxes)
        efficiency_score = min(data.private_investment * 3, 100) + max(100 - data.tax_rate * 2, 0)
        efficiency_score = efficiency_score / 2

        # Equity indicators (government spending, progressive taxation)
        equity_score = min(data.government_spending * 2.5, 100) + min(data.tax_rate * 2, 100)
        equity_score = equity_score / 2

        # Balance the two
        if abs(efficiency_score - equity_score) < 20:
            return (efficiency_score + equity_score) / 2
        else:
            # Penalize extreme imbalance
            return (efficiency_score + equity_score) / 2 - abs(efficiency_score - equity_score) / 4

    def _calculate_regulatory_effectiveness(self, data: EconomicData) -> float:
        """Calculate regulatory effectiveness score (0-100)"""
        # Balance between freedom and protection
        # Moderate government spending suggests balanced regulation
        regulation_balance = max(100 - abs(data.government_spending - 35) * 2, 0)

        # Consumer confidence reflects regulatory trust
        confidence_score = min(data.consumer_confidence * 1.2, 100)

        # Economic stability reflects regulatory effectiveness
        stability_score = max(100 - abs(data.inflation - 2.5) * 20, 0)

        return (regulation_balance + confidence_score + stability_score) / 3

    def _generate_assessment(self, health_score: float, strengths: List[str], concerns: List[str]) -> str:
        """Generate overall assessment"""
        if health_score >= 80:
            return f"Excellent mixed economy balance (Score: {health_score:.1f}/100). Optimal public-private balance."
        elif health_score >= 60:
            return f"Good mixed economy performance (Score: {health_score:.1f}/100). Well-balanced with minor adjustments needed."
        elif health_score >= 40:
            return f"Moderate mixed economy health (Score: {health_score:.1f}/100). Some imbalance requiring policy correction."
        else:
            return f"Poor mixed economy balance (Score: {health_score:.1f}/100). Significant rebalancing needed."

    def _get_policy_recommendation(self, score: float) -> str:
        """Get policy recommendation based on score"""
        if score >= 70:
            return "Highly Recommended"
        elif score >= 40:
            return "Recommended"
        elif score >= 0:
            return "Conditionally Recommended"
        else:
            return "Not Recommended"

    def _generate_policy_justification(self, scores: Dict[str, float], policy_type: str) -> str:
        """Generate justification for policy evaluation"""
        justification = []

        if scores["economic_balance"] > 60:
            justification.append("Maintains public-private balance")
        elif scores["economic_balance"] < -20:
            justification.append("Disrupts economic balance")

        if scores["economic_efficiency"] > 60:
            justification.append("Promotes market efficiency")
        elif scores["economic_efficiency"] < -20:
            justification.append("Reduces economic efficiency")

        if scores["social_welfare"] > 60:
            justification.append("Enhances social welfare")
        elif scores["social_welfare"] < -20:
            justification.append("Reduces social protection")

        if scores["economic_stability"] > 60:
            justification.append("Supports economic stability")
        elif scores["economic_stability"] < -20:
            justification.append("Creates economic instability"

        return "; ".join(justification) if justification else "Mixed effects on mixed economy objectives"