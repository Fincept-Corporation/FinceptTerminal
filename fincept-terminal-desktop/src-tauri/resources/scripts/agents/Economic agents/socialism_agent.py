"""
Socialism Economic Agent
Processes economic data through the lens of socialist economic principles
"""

from typing import Dict, List, Any
from base_agent import EconomicAgent, EconomicData, PolicyRecommendation

class SocialismAgent(EconomicAgent):
    """Socialist economic analysis agent focusing on social ownership and equality"""

    def __init__(self):
        super().__init__(
            "Socialism Agent",
            "Analyzes economic data through socialist principles emphasizing social ownership, economic equality, and workers' rights"
        )
        self.principles = [
            "Social ownership of means of production",
            "Economic equality and social welfare",
            "Workers' rights and labor protection",
            "Democratic economic planning",
            "Redistribution of wealth",
            "Universal access to essential services"
        ]
        self.priorities = [
            "Economic equality",
            "Social welfare",
            "Workers' protection",
            "Public services",
            "Economic democracy",
            "Environmental sustainability"
        ]

    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        """Analyze economy from socialist perspective"""

        # Calculate socialism-specific metrics
        equality_index = self._calculate_equality_index(data)
        social_welfare_score = self._calculate_social_welfare_score(data)
        workers_protection_score = self._calculate_workers_protection(data)
        public_services_score = self._calculate_public_services_score(data)

        # Identify strengths and concerns
        strengths = []
        concerns = []

        if data.government_spending > 30:
            strengths.append("Strong government investment in public services")
        if data.unemployment < 6:
            strengths.append("Low unemployment showing economic security")
        if data.inflation < 3:
            strengths.append("Price stability protecting vulnerable populations")
        if data.consumer_confidence > 60:
            strengths.append("Strong consumer confidence indicating economic security")

        if data.unemployment > 8:
            concerns.append("High unemployment affecting workers' security")
        if data.tax_rate < 25:
            concerns.append("Insufficient tax revenue for social programs")
        if data.government_spending < 25:
            concerns.append("Inadequate public investment in essential services")
        if data.trade_balance < -10:
            concerns.append("Trade deficit may harm domestic industries and workers")
        if data.private_investment > 40:
            concerns.append("Excessive privatization may undermine social ownership")

        # Calculate overall socialist health score
        health_score = (
            equality_index * 0.3 +
            social_welfare_score * 0.25 +
            workers_protection_score * 0.25 +
            public_services_score * 0.2
        )

        return {
            "ideology": "socialism",
            "equality_index": equality_index,
            "social_welfare_score": social_welfare_score,
            "workers_protection_score": workers_protection_score,
            "public_services_score": public_services_score,
            "overall_health_score": health_score,
            "strengths": strengths,
            "concerns": concerns,
            "assessment": self._generate_assessment(health_score, strengths, concerns),
            "key_indicators": {
                "unemployment_rate": data.unemployment,
                "inflation_rate": data.inflation,
                "government_spending_share": data.government_spending,
                "tax_progression": data.tax_rate,
                "public_investment_ratio": max(0, 100 - data.private_investment)
            }
        }

    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        """Generate socialist policy recommendations"""
        recommendations = []

        # Progressive taxation recommendations
        if data.tax_rate < 35:
            recommendations.append(PolicyRecommendation(
                title="Implement Progressive Taxation",
                description="Increase tax rates on wealthy individuals and corporations to fund social programs",
                priority="high",
                expected_impact="Reduced inequality and increased funding for public services",
                implementation_time="6-12 months",
                risk_level="low"
            ))

        # Social spending recommendations
        if data.government_spending < 35:
            recommendations.append(PolicyRecommendation(
                title="Increase Social Spending",
                description="Expand government investment in healthcare, education, and social security",
                priority="high",
                expected_impact="Improved social welfare and economic equality",
                implementation_time="12-24 months",
                risk_level="medium"
            ))

        # Worker protection recommendations
        if data.unemployment > 6:
            recommendations.append(PolicyRecommendation(
                title="Strengthen Workers' Rights",
                description="Implement stronger labor protections, minimum wage increases, and union rights",
                priority="high",
                expected_impact="Better working conditions and reduced exploitation",
                implementation_time="6-18 months",
                risk_level="low"
            ))

        # Public ownership recommendations
        if data.private_investment > 60:
            recommendations.append(PolicyRecommendation(
                title="Expand Public Ownership",
                description="Nationalize key industries and utilities to ensure social control",
                priority="medium",
                expected_impact="Democratic control of essential services",
                implementation_time="24-48 months",
                risk_level="medium"
            ))

        # Economic planning recommendations
        recommendations.append(PolicyRecommendation(
            title="Implement Democratic Economic Planning",
            description="Create democratic institutions for economic planning and resource allocation",
            priority="medium",
            expected_impact="More equitable resource distribution",
            implementation_time="18-36 months",
            risk_level="medium"
        ))

        # Universal basic services recommendations
        recommendations.append(PolicyRecommendation(
            title="Provide Universal Basic Services",
            description="Ensure universal access to healthcare, education, housing, and utilities",
            priority="high",
            expected_impact="Elimination of poverty and improved quality of life",
            implementation_time="24-60 months",
            risk_level="medium"
        ))

        # Wealth redistribution recommendations
        if analysis["equality_index"] < 60:
            recommendations.append(PolicyRecommendation(
                title="Implement Wealth Redistribution",
                description="Policies to redistribute wealth through taxes and social programs",
                priority="high",
                expected_impact="Reduced economic inequality",
                implementation_time="12-36 months",
                risk_level="medium"
            ))

        return recommendations

    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Evaluate policy from socialist perspective"""
        policy_name = policy.get("name", "Unknown Policy")
        policy_type = policy.get("type", "unknown")

        # Scoring criteria for socialist evaluation
        scores = {
            "equality_impact": 0,  # How it affects economic equality
            "social_welfare": 0,  # How it affects social welfare
            "workers_benefits": 0,  # How it benefits workers
            "public_control": 0,  # How it increases public ownership
            "economic_security": 0  # How it provides economic security
        }

        # Evaluate based on policy type
        if policy_type == "tax_increase":
            scores["equality_impact"] = 80
            scores["social_welfare"] = 85
            scores["economic_security"] = 75
        elif policy_type == "universal_healthcare":
            scores["social_welfare"] = 95
            scores["equality_impact"] = 85
            scores["economic_security"] = 80
        elif policy_type == "minimum_wage":
            scores["workers_benefits"] = 90
            scores["equality_impact"] = 75
            scores["economic_security"] = 70
        elif policy_type == "nationalization":
            scores["public_control"] = 95
            scores["equality_impact"] = 80
            scores["social_welfare"] = 75
        elif policy_type == "social_spending":
            scores["social_welfare"] = 90
            scores["equality_impact"] = 80
            scores["economic_security"] = 85
        elif policy_type == "tax_cut":
            scores["equality_impact"] = -60
            scores["social_welfare"] = -50
            scores["economic_security"] = -40
        elif policy_type == "privatization":
            scores["public_control"] = -80
            scores["equality_impact"] = -60
            scores["social_welfare"] = -70

        overall_score = sum(scores.values()) / len(scores)

        return {
            "policy_name": policy_name,
            "socialist_score": overall_score,
            "detailed_scores": scores,
            "recommendation": self._get_policy_recommendation(overall_score),
            "justification": self._generate_policy_justification(scores, policy_type)
        }

    def _calculate_equality_index(self, data: EconomicData) -> float:
        """Calculate economic equality index (0-100)"""
        # Higher government spending and tax rates indicate more redistribution
        spending_score = min(data.government_spending * 2.5, 100)
        tax_score = min(data.tax_rate * 2.5, 100)

        # Lower unemployment indicates better economic security
        employment_score = max(100 - data.unemployment * 8, 0)

        return (spending_score + tax_score + employment_score) / 3

    def _calculate_social_welfare_score(self, data: EconomicData) -> float:
        """Calculate social welfare score (0-100)"""
        # Higher government spending indicates more social programs
        spending_score = min(data.government_spending * 2.8, 100)

        # Lower unemployment indicates better welfare
        unemployment_score = max(100 - data.unemployment * 10, 0)

        # Lower inflation protects purchasing power of the poor
        inflation_score = max(100 - data.inflation * 15, 0)

        return (spending_score + unemployment_score + inflation_score) / 3

    def _calculate_workers_protection(self, data: EconomicData) -> float:
        """Calculate workers' protection score (0-100)"""
        # Lower unemployment is better for workers
        employment_score = max(100 - data.unemployment * 12, 0)

        # Government spending often correlates with labor protection
        protection_score = min(data.government_spending * 2.2, 100)

        # Higher tax rates may indicate more labor regulations
        regulation_score = min(data.tax_rate * 2, 100)

        return (employment_score + protection_score + regulation_score) / 3

    def _calculate_public_services_score(self, data: EconomicData) -> float:
        """Calculate public services score (0-100)"""
        # Higher government spending means more public services
        services_score = min(data.government_spending * 3, 100)

        # Higher taxes provide funding for services
        funding_score = min(data.tax_rate * 2.5, 100)

        # Lower private investment may indicate more public ownership
        public_ownership_score = max(100 - data.private_investment * 1.5, 0)

        return (services_score + funding_score + public_ownership_score) / 3

    def _generate_assessment(self, health_score: float, strengths: List[str], concerns: List[str]) -> str:
        """Generate overall assessment"""
        if health_score >= 80:
            return f"Excellent socialist economic health (Score: {health_score:.1f}/100). Strong social protection and equality."
        elif health_score >= 60:
            return f"Good socialist economic health (Score: {health_score:.1f}/100). Solid social programs with room for improvement."
        elif health_score >= 40:
            return f"Moderate socialist economic health (Score: {health_score:.1f}/100). Mixed social outcomes requiring policy attention."
        else:
            return f"Poor socialist economic health (Score: {health_score:.1f}/100). Significant social and economic inequality issues."

    def _get_policy_recommendation(self, score: float) -> str:
        """Get policy recommendation based on score"""
        if score >= 70:
            return "Strongly Recommended"
        elif score >= 40:
            return "Recommended"
        elif score >= 0:
            return "Not Recommended"
        else:
            return "Strongly Opposed"

    def _generate_policy_justification(self, scores: Dict[str, float], policy_type: str) -> str:
        """Generate justification for policy evaluation"""
        justification = []

        if scores["equality_impact"] > 50:
            justification.append("Promotes economic equality")
        elif scores["equality_impact"] < -20:
            justification.append("Increases economic inequality")

        if scores["social_welfare"] > 50:
            justification.append("Improves social welfare")
        elif scores["social_welfare"] < -20:
            justification.append("Reduces social welfare")

        if scores["workers_benefits"] > 50:
            justification.append("Benefits working class")
        elif scores["workers_benefits"] < -20:
            justification.append("Harms workers")

        if scores["public_control"] > 50:
            justification.append("Increases democratic control")
        elif scores["public_control"] < -20:
            justification.append("Reduces public ownership")

        return "; ".join(justification) if justification else "Mixed effects on socialist objectives"