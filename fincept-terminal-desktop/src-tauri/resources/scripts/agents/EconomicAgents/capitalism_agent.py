"""
Capitalism Economic Agent
Processes economic data through the lens of capitalist economic principles
"""

from typing import Dict, List, Any
from base_agent import EconomicAgent, EconomicData, PolicyRecommendation

class CapitalismAgent(EconomicAgent):
    """Capitalist economic analysis agent focusing on free markets and private enterprise"""

    def __init__(self):
        super().__init__(
            "Capitalism Agent",
            "Analyzes economic data through free-market principles emphasizing private property, competition, and minimal government intervention"
        )
        self.principles = [
            "Private property rights",
            "Free market competition",
            "Profit motive",
            "Limited government intervention",
            "Voluntary exchange",
            "Capital accumulation"
        ]
        self.priorities = [
            "Economic freedom",
            "Market efficiency",
            "Innovation and entrepreneurship",
            "Wealth creation",
            "Competitive markets"
        ]

    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        """Analyze economy from capitalist perspective"""

        # Calculate capitalism-specific metrics
        market_efficiency = self._calculate_market_efficiency(data)
        investment_attractiveness = self._calculate_investment_attractiveness(data)
        economic_freedom_score = self._calculate_economic_freedom(data)
        competitiveness_score = self._calculate_competitiveness(data)

        # Identify strengths and concerns
        strengths = []
        concerns = []

        if data.gdp > 3.0:
            strengths.append("Strong GDP growth indicating productive investment")
        if data.unemployment < 5.0:
            strengths.append("Low unemployment showing efficient labor markets")
        if data.tax_rate < 30:
            strengths.append("Favorable tax environment for business growth")
        if data.private_investment > 20:
            strengths.append("High private investment driving innovation")

        if data.government_spending > 40:
            concerns.append("High government spending may crowd out private investment")
        if data.inflation > 4:
            concerns.append("High inflation erodes purchasing power and savings")
        if data.tax_rate > 40:
            concerns.append("High tax rates may discourage investment and entrepreneurship")
        if data.interest_rate > 8:
            concerns.append("High interest rates increase cost of capital")

        # Calculate overall capitalist health score
        health_score = (
            market_efficiency * 0.3 +
            investment_attractiveness * 0.25 +
            economic_freedom_score * 0.25 +
            competitiveness_score * 0.2
        )

        return {
            "ideology": "capitalism",
            "market_efficiency": market_efficiency,
            "investment_attractiveness": investment_attractiveness,
            "economic_freedom_score": economic_freedom_score,
            "competitiveness_score": competitiveness_score,
            "overall_health_score": health_score,
            "strengths": strengths,
            "concerns": concerns,
            "assessment": self._generate_assessment(health_score, strengths, concerns),
            "key_indicators": {
                "gdp_growth": data.gdp,
                "unemployment_rate": data.unemployment,
                "inflation_rate": data.inflation,
                "tax_burden": data.tax_rate,
                "private_investment_share": data.private_investment
            }
        }

    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        """Generate capitalist policy recommendations"""
        recommendations = []

        # Tax policy recommendations
        if data.tax_rate > 35:
            recommendations.append(PolicyRecommendation(
                title="Reduce Corporate and Personal Tax Rates",
                description="Lower taxes to stimulate investment, entrepreneurship, and economic growth",
                priority="high",
                expected_impact="Increased private investment and GDP growth",
                implementation_time="6-12 months",
                risk_level="medium"
            ))

        # Deregulation recommendations
        if analysis["economic_freedom_score"] < 70:
            recommendations.append(PolicyRecommendation(
                title="Reduce Regulatory Burden",
                description="Eliminate unnecessary regulations that hinder business formation and growth",
                priority="high",
                expected_impact="Improved business climate and increased competition",
                implementation_time="12-24 months",
                risk_level="low"
            ))

        # Government spending recommendations
        if data.government_spending > 40:
            recommendations.append(PolicyRecommendation(
                title="Reduce Government Spending",
                description="Cut government expenditure to reduce tax burden and allow private sector growth",
                priority="medium",
                expected_impact="More efficient resource allocation through markets",
                implementation_time="12-36 months",
                risk_level="medium"
            ))

        # Trade policy recommendations
        if data.trade_balance < -5:
            recommendations.append(PolicyRecommendation(
                title="Promote Free Trade",
                description="Remove trade barriers and negotiate favorable trade agreements",
                priority="medium",
                expected_impact="Increased export competitiveness and economic efficiency",
                implementation_time="6-18 months",
                risk_level="low"
            ))

        # Monetary policy recommendations
        if data.interest_rate > 7:
            recommendations.append(PolicyRecommendation(
                title="Implement Sound Monetary Policy",
                description="Maintain stable currency and moderate interest rates to encourage investment",
                priority="high",
                expected_impact="Stable environment for long-term investment",
                implementation_time="3-6 months",
                risk_level="low"
            ))

        # Property rights recommendations
        recommendations.append(PolicyRecommendation(
            title="Strengthen Property Rights",
            description="Enhance legal protection for private property and intellectual property",
            priority="medium",
            expected_impact="Increased investment and innovation",
            implementation_time="12-24 months",
            risk_level="low"
        ))

        return recommendations

    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Evaluate policy from capitalist perspective"""
        policy_name = policy.get("name", "Unknown Policy")
        policy_type = policy.get("type", "unknown")

        # Scoring criteria for capitalist evaluation
        scores = {
            "market_impact": 0,  # How it affects free markets
            "investment_incentive": 0,  # How it encourages private investment
            "economic_freedom": 0,  # How it affects economic freedom
            "competitiveness": 0,  # How it affects competition
            "property_rights": 0  # How it affects property rights
        }

        # Evaluate based on policy type
        if policy_type == "tax_cut":
            scores["investment_incentive"] = 85
            scores["economic_freedom"] = 80
            scores["market_impact"] = 75
        elif policy_type == "deregulation":
            scores["economic_freedom"] = 90
            scores["market_impact"] = 85
            scores["competitiveness"] = 80
        elif policy_type == "trade_agreement":
            scores["competitiveness"] = 85
            scores["market_impact"] = 80
        elif policy_type == "spending_increase":
            scores["market_impact"] = -50
            scores["economic_freedom"] = -40
        elif policy_type == "price_control":
            scores["market_impact"] = -70
            scores["economic_freedom"] = -60

        overall_score = sum(scores.values()) / len(scores)

        return {
            "policy_name": policy_name,
            "capitalist_score": overall_score,
            "detailed_scores": scores,
            "recommendation": self._get_policy_recommendation(overall_score),
            "justification": self._generate_policy_justification(scores, policy_type)
        }

    def _calculate_market_efficiency(self, data: EconomicData) -> float:
        """Calculate market efficiency score (0-100)"""
        # Factors: low unemployment, moderate inflation, reasonable interest rates
        unemployment_score = max(100 - data.unemployment * 10, 0)
        inflation_score = max(100 - abs(data.inflation - 2.5) * 20, 0)
        interest_score = max(100 - abs(data.interest_rate - 5) * 10, 0)

        return (unemployment_score + inflation_score + interest_score) / 3

    def _calculate_investment_attractiveness(self, data: EconomicData) -> float:
        """Calculate investment attractiveness score (0-100)"""
        # Factors: GDP growth, interest rates, tax burden, private investment
        gdp_score = min(data.gdp * 10, 100)
        interest_score = max(100 - data.interest_rate * 8, 0)
        tax_score = max(100 - data.tax_rate * 2, 0)
        investment_score = min(data.private_investment * 3, 100)

        return (gdp_score + interest_score + tax_score + investment_score) / 4

    def _calculate_economic_freedom(self, data: EconomicData) -> float:
        """Calculate economic freedom score (0-100)"""
        # Factors: low tax rates, low government spending, low regulation (assumed)
        tax_score = max(100 - data.tax_rate * 2.5, 0)
        spending_score = max(100 - data.government_spending, 0)

        return (tax_score + spending_score) / 2

    def _calculate_competitiveness(self, data: EconomicData) -> float:
        """Calculate economic competitiveness score (0-100)"""
        # Factors: productivity (GDP growth), trade balance, innovation (private investment)
        productivity_score = min(data.gdp * 15, 100)
        trade_score = max(100 - abs(data.trade_balance) * 10, 0)
        innovation_score = min(data.private_investment * 4, 100)

        return (productivity_score + trade_score + innovation_score) / 3

    def _generate_assessment(self, health_score: float, strengths: List[str], concerns: List[str]) -> str:
        """Generate overall assessment"""
        if health_score >= 80:
            return f"Excellent economic health (Score: {health_score:.1f}/100). Strong capitalist fundamentals with minimal concerns."
        elif health_score >= 60:
            return f"Good economic health (Score: {health_score:.1f}/100). Solid market performance with some areas for improvement."
        elif health_score >= 40:
            return f"Moderate economic health (Score: {health_score:.1f}/100). Mixed performance with significant policy challenges."
        else:
            return f"Poor economic health (Score: {health_score:.1f}/100). Major obstacles to prosperity requiring significant reforms."

    def _get_policy_recommendation(self, score: float) -> str:
        """Get policy recommendation based on score"""
        if score >= 70:
            return "Highly Recommended"
        elif score >= 40:
            return "Moderately Recommended"
        elif score >= 0:
            return "Not Recommended"
        else:
            return "Strongly Opposed"

    def _generate_policy_justification(self, scores: Dict[str, float], policy_type: str) -> str:
        """Generate justification for policy evaluation"""
        justification = []

        if scores["market_impact"] > 50:
            justification.append("Supports free market efficiency")
        elif scores["market_impact"] < -20:
            justification.append("Hinders market efficiency")

        if scores["investment_incentive"] > 50:
            justification.append("Encourages private investment")
        elif scores["investment_incentive"] < -20:
            justification.append("Discourages private investment")

        if scores["economic_freedom"] > 50:
            justification.append("Enhances economic freedom")
        elif scores["economic_freedom"] < -20:
            justification.append("Restricts economic freedom")

        return "; ".join(justification) if justification else "Mixed effects on economic outcomes"