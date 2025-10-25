"""
Neoliberal Economic Agent
Processes economic data through the lens of neoliberal economic principles
"""

from typing import Dict, List, Any
from base_agent import EconomicAgent, EconomicData, PolicyRecommendation

class NeoliberalAgent(EconomicAgent):
    """Neoliberal economic analysis agent emphasizing free markets, deregulation, and minimal government"""

    def __init__(self):
        super().__init__(
            "Neoliberal Agent",
            "Analyzes economic data through neoliberal principles emphasizing free markets, deregulation, privatization, and minimal government intervention"
        )
        self.principles = [
            "Free market fundamentalism",
            "Deregulation and liberalization",
            "Privatization of state enterprises",
            "Minimal government intervention",
            "Globalization and free trade",
            "Supply-side economics",
            "Monetary stability"
        ]
        self.priorities = [
            "Economic liberalization",
            "Market efficiency",
            "Private sector growth",
            "Fiscal discipline",
            "Monetary stability",
            "International trade integration"
        ]

    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        """Analyze economy from neoliberal perspective"""

        # Calculate neoliberal-specific metrics
        market_freedom_score = self._calculate_market_freedom_score(data)
        liberalization_score = self._calculate_liberalization_score(data)
        fiscal_discipline_score = self._calculate_fiscal_discipline_score(data)
        globalization_score = self._calculate_globalization_score(data)

        # Identify strengths and concerns
        strengths = []
        concerns = []

        # Market freedom strengths
        if data.tax_rate < 25:
            strengths.append("Low tax burden encouraging investment")
        if data.government_spending < 25:
            strengths.append("Limited government intervention in economy")
        if data.private_investment > 40:
            strengths.append("Strong private sector driving growth")
        if data.unemployment < 5:
            strengths.append("Flexible labor markets with low unemployment")

        # Regulatory strengths (assumed from data patterns)
        if data.gdp > 3:
            strengths.append("Strong economic growth indicating business-friendly environment")

        # Market efficiency strengths
        if data.inflation < 3:
            strengths.append("Price stability through sound monetary policy")

        # Concerns from neoliberal perspective
        if data.government_spending > 35:
            concerns.append("Excessive government spending crowding out private sector")
        if data.tax_rate > 35:
            concerns.append("High tax rates discouraging investment and entrepreneurship")
        if data.government_spending > 50:
            concerns.append("Overly large public sector reducing economic freedom")
        if data.private_investment < 20:
            concerns.append("Insufficient private investment and economic dynamism")
        if data.inflation > 5:
            concerns.append("High inflation eroding purchasing power and investment confidence")

        # Calculate overall neoliberal health score
        health_score = (
            market_freedom_score * 0.3 +
            liberalization_score * 0.25 +
            fiscal_discipline_score * 0.25 +
            globalization_score * 0.2
        )

        return {
            "ideology": "neoliberal",
            "market_freedom_score": market_freedom_score,
            "liberalization_score": liberalization_score,
            "fiscal_discipline_score": fiscal_discipline_score,
            "globalization_score": globalization_score,
            "overall_health_score": health_score,
            "strengths": strengths,
            "concerns": concerns,
            "assessment": self._generate_assessment(health_score, strengths, concerns),
            "key_indicators": {
                "tax_burden": data.tax_rate,
                "government_spending_share": data.government_spending,
                "private_investment_share": data.private_investment,
                "market_openness": max(0, 100 - data.tax_rate - data.government_spending/2),
                "economic_freedom": self._calculate_economic_freedom_index(data)
            }
        }

    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        """Generate neoliberal policy recommendations"""
        recommendations = []

        # Tax reduction recommendations
        if data.tax_rate > 30:
            recommendations.append(PolicyRecommendation(
                title="Implement Supply-Side Tax Cuts",
                description="Reduce marginal tax rates on individuals and corporations to stimulate investment and growth",
                priority="high",
                expected_impact="Increased investment, job creation, and economic growth",
                implementation_time="3-6 months",
                risk_level="low"
            ))

        # Privatization recommendations
        if data.government_spending > 40:
            recommendations.append(PolicyRecommendation(
                title="Accelerate Privatization Program",
                description="Sell state-owned enterprises to improve efficiency and reduce government size",
                priority="high",
                expected_impact="Improved productivity and reduced fiscal burden",
                implementation_time="12-36 months",
                risk_level="medium"
            ))

        # Deregulation recommendations
        recommendations.append(PolicyRecommendation(
            title="Comprehensive Deregulation",
            description="Eliminate unnecessary regulations that hinder business and market efficiency",
            priority="high",
            expected_impact="Increased business activity and economic efficiency",
            implementation_time="12-24 months",
            risk_level="low"
        ))

        # Government spending reduction
        if data.government_spending > 35:
            recommendations.append(PolicyRecommendation(
                title="Reduce Government Spending",
                description("Cut government expenditures and eliminate wasteful programs"),
                priority="high",
                expected_impact="Reduced tax burden and increased private sector growth",
                implementation_time="12-48 months",
                risk_level="medium"
            ))

        # Trade liberalization
        recommendations.append(PolicyRecommendation(
            title="Expand Trade Liberalization",
            description="Remove trade barriers and negotiate free trade agreements",
            priority="medium",
            expected_impact="Increased competition and economic efficiency",
            implementation_time="6-18 months",
            risk_level="low"
        ))

        # Labor market flexibility
        recommendations.append(PolicyRecommendation(
            title="Increase Labor Market Flexibility",
            description="Reduce labor regulations and make hiring/firing easier",
            priority="medium",
            expected_impact("Improved employment and economic adaptability"),
            implementation_time="6-12 months",
            risk_level="medium"
        ))

        # Monetary stability
        if data.inflation > 3:
            recommendations.append(PolicyRecommendation(
                title="Ensure Monetary Stability",
                description="Implement strict monetary policy to control inflation",
                priority="high",
                expected_impact="Price stability and investment confidence",
                implementation_time="1-3 months",
                risk_level="low"
            ))

        # Financial liberalization
        recommendations.append(PolicyRecommendation(
            title="Liberalize Financial Markets",
            description="Remove capital controls and financial regulations",
            priority="medium",
            expected_impact="Increased capital flow and investment",
            implementation_time="6-18 months",
            risk_level="medium"
        ))

        return recommendations

    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Evaluate policy from neoliberal perspective"""
        policy_name = policy.get("name", "Unknown Policy")
        policy_type = policy.get("type", "unknown")

        # Scoring criteria for neoliberal evaluation
        scores = {
            "market_efficiency": 0,  # How it affects market efficiency
            "economic_freedom": 0,  # How it affects economic freedom
            "private_sector": 0,  # How it benefits private sector
            "fiscal_responsibility": 0,  # How it affects fiscal discipline
            "global_integration": 0  # How it affects global integration
        }

        # Evaluate based on policy type
        if policy_type == "tax_cut":
            scores["economic_freedom"] = 95
            scores["market_efficiency"] = 85
            scores["private_sector"] = 90
            scores["fiscal_responsibility"] = 70
        elif policy_type == "privatization":
            scores["private_sector"] = 100
            scores["market_efficiency"] = 90
            scores["economic_freedom"] = 85
            scores["fiscal_responsibility"] = 80
        elif policy_type == "deregulation":
            scores["economic_freedom"] = 100
            scores["market_efficiency"] = 95
            scores["private_sector"] = 90
        elif policy_type == "trade_liberalization":
            scores["global_integration"] = 100
            scores["market_efficiency"] = 85
            scores["economic_freedom"] = 80
        elif policy_type == "spending_cut":
            scores["fiscal_responsibility"] = 90
            scores["private_sector"] = 75
            scores["economic_freedom"] = 70
        elif policy_type == "government_intervention":
            scores["market_efficiency"] = -80
            scores["economic_freedom"] = -90
            scores["private_sector"] = -70
            scores["fiscal_responsibility"] = -60
        elif policy_type == "price_controls":
            scores["market_efficiency"] = -90
            scores["economic_freedom"] = -85
            scores["private_sector"] = -80
        elif policy_type == "trade_barriers":
            scores["global_integration"] = -85
            scores["market_efficiency"] = -75
            scores["economic_freedom"] = -70

        overall_score = sum(scores.values()) / len(scores)

        return {
            "policy_name": policy_name,
            "neoliberal_score": overall_score,
            "detailed_scores": scores,
            "recommendation": self._get_policy_recommendation(overall_score),
            "justification": self._generate_policy_justification(scores, policy_type)
        }

    def _calculate_market_freedom_score(self, data: EconomicData) -> float:
        """Calculate market freedom score (0-100)"""
        # Low taxes = more freedom
        tax_freedom = max(100 - data.tax_rate * 2.5, 0)

        # Low government spending = more freedom
        spending_freedom = max(100 - data.government_spending * 2, 0)

        # High private investment = market freedom
        investment_freedom = min(data.private_investment * 2.5, 100)

        # Price stability = market freedom
        price_stability = max(100 - data.inflation * 10, 0)

        return (tax_freedom + spending_freedom + investment_freedom + price_stability) / 4

    def _calculate_liberalization_score(self, data: EconomicData) -> float:
        """Calculate economic liberalization score (0-100)"""
        # Trade openness (inferred from data patterns)
        trade_openness = max(100 - abs(data.trade_balance) * 8, 0)

        # Investment liberalization
        investment_liberalization = min(data.private_investment * 3, 100)

        # Financial liberalization (low inflation and stable interest rates)
        financial_liberalization = max(100 - abs(data.interest_rate - 5) * 10 - data.inflation * 10, 0)

        # Business freedom (low unemployment indicates flexible markets)
        business_freedom = max(100 - data.unemployment * 8, 0)

        return (trade_openness + investment_liberalization + financial_liberalization + business_freedom) / 4

    def _calculate_fiscal_discipline_score(self, data: EconomicData) -> float:
        """Calculate fiscal discipline score (0-100)"""
        # Low government spending = discipline
        spending_discipline = max(100 - data.government_spending * 2.5, 0)

        # Reasonable tax rates = discipline
        tax_discipline = max(100 - abs(data.tax_rate - 25) * 3, 0)

        # Balanced approach (not too low to starve essential functions)
        balance_score = 80 if 15 < data.tax_rate < 35 and 20 < data.government_spending < 40 else 60

        return (spending_discipline + tax_discipline + balance_score) / 3

    def _calculate_globalization_score(self, data: EconomicData) -> float:
        """Calculate globalization integration score (0-100)"""
        # Trade integration
        trade_integration = max(100 - abs(data.trade_balance) * 10, 0)

        # Investment openness
        investment_openness = min(data.private_investment * 3, 100)

        # Economic openness (low barriers assumed from market indicators)
        economic_openness = max(100 - data.tax_rate - data.government_spending/3, 0)

        # Financial integration (stable indicators)
        financial_integration = max(100 - abs(data.interest_rate - 4) * 15 - data.inflation * 15, 0)

        return (trade_integration + investment_openness + economic_openness + financial_integration) / 4

    def _calculate_economic_freedom_index(self, data: EconomicData) -> float:
        """Calculate comprehensive economic freedom index"""
        return self._calculate_market_freedom_score(data)

    def _generate_assessment(self, health_score: float, strengths: List[str], concerns: List[str]) -> str:
        """Generate overall assessment"""
        if health_score >= 80:
            return f"Excellent neoliberal economic conditions (Score: {health_score:.1f}/100). Strong market freedom and minimal government intervention."
        elif health_score >= 60:
            return f"Good market-oriented economy (Score: {health_score:.1f}/100). Generally favorable conditions with room for more reforms."
        elif health_score >= 40:
            return f"Moderate economic freedom (Score: {health_score:.1f}/100). Mixed conditions requiring significant liberalization."
        else:
            return f"Poor market conditions (Score: {health_score:.1f}/100). Excessive government intervention requiring radical reforms."

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

        if scores["market_efficiency"] > 60:
            justification.append("Increases market efficiency")
        elif scores["market_efficiency"] < -20:
            justification.append("Reduces market efficiency")

        if scores["economic_freedom"] > 60:
            justification.append("Enhances economic freedom")
        elif scores["economic_freedom"] < -20:
            justification.append("Restricts economic freedom")

        if scores["private_sector"] > 60:
            justification.append("Benefits private sector")
        elif scores["private_sector"] < -20:
            justification.append("Harms private sector")

        if scores["fiscal_responsibility"] > 60:
            justification.append("Promotes fiscal discipline")
        elif scores["fiscal_responsibility"] < -20:
            justification.append("Undermines fiscal responsibility")

        return "; ".join(justification) if justification else "Mixed effects on neoliberal objectives"