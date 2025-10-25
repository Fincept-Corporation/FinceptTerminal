"""
Mercantilist Economic Agent
Processes economic data through the lens of mercantilist economic principles
"""

from typing import Dict, List, Any
from base_agent import EconomicAgent, EconomicData, PolicyRecommendation

class MercantilistAgent(EconomicAgent):
    """Mercantilist economic analysis agent emphasizing trade surpluses, national wealth accumulation, and protectionism"""

    def __init__(self):
        super().__init__(
            "Mercantilist Agent",
            "Analyzes economic data through mercantilist principles emphasizing trade surpluses, protectionism, and national wealth accumulation"
        )
        self.principles = [
            "Trade surplus as wealth measure",
            "Export promotion",
            "Import substitution",
            "Protectionism and tariffs",
            "National self-sufficiency",
            "Colonial resource acquisition",
            "Bullion/specie accumulation"
        ]
        self.priorities = [
            "Trade surplus maximization",
            "Domestic industry protection",
            "National wealth accumulation",
            "Export competitiveness",
            "Import reduction",
            "Resource self-sufficiency"
        ]

    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        """Analyze economy from mercantilist perspective"""

        # Calculate mercantilist-specific metrics
        trade_balance_score = self._calculate_trade_balance_score(data)
        self_sufficiency_score = self._calculate_self_sufficiency_score(data)
        protection_score = self._calculate_protection_score(data)
        national_accumulation_score = self._calculate_national_accumulation_score(data)

        # Identify strengths and concerns
        strengths = []
        concerns = []

        # Trade performance strengths
        if data.trade_balance > 5:
            strengths.append("Strong trade surplus indicating wealth accumulation")
        if data.government_spending > 30:
            strengths.append("Strong government support for domestic industries")
        if data.private_investment > 25:
            strengths.append("Robust domestic investment in productive capacity")

        # National wealth strengths
        if data.gdp > 3:
            strengths.append("Strong national output and productivity")
        if data.consumer_confidence > 60:
            strengths.append("Strong domestic demand supporting national industries")

        # Mercantilist concerns
        if data.trade_balance < -5:
            concerns.append("Trade deficit indicates wealth outflow and dependency")
        if data.trade_balance < -10:
            concerns.append("Severe trade deficit undermining national wealth")
        if data.government_spending < 25:
            concerns.append("Insufficient government support for domestic industries")
        if data.private_investment < 20:
            concerns.append("Low domestic investment threatens self-sufficiency")
        if data.inflation > 5:
            concerns.append("High inflation erodes national wealth and competitiveness")

        # Import dependency concerns (inferred from negative trade balance)
        if data.trade_balance < 0:
            concerns.append(f"Import dependency costing {-data.trade_balance}% of GDP")

        # Calculate overall mercantilist health score
        health_score = (
            trade_balance_score * 0.35 +
            self_sufficiency_score * 0.25 +
            protection_score * 0.25 +
            national_accumulation_score * 0.15
        )

        return {
            "ideology": "mercantilism",
            "trade_balance_score": trade_balance_score,
            "self_sufficiency_score": self_sufficiency_score,
            "protection_score": protection_score,
            "national_accumulation_score": national_accumulation_score,
            "overall_health_score": health_score,
            "strengths": strengths,
            "concerns": concerns,
            "assessment": self._generate_assessment(health_score, strengths, concerns),
            "key_indicators": {
                "trade_surplus": data.trade_balance,
                "export_orientation": max(0, data.trade_balance),
                "import_dependency": max(0, -data.trade_balance),
                "national_productivity": data.gdp,
                "government_support": data.government_spending,
                "domestic_investment": data.private_investment
            }
        }

    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        """Generate mercantilist policy recommendations"""
        recommendations = []

        # Trade surplus recommendations
        if data.trade_balance < 5:
            recommendations.append(PolicyRecommendation(
                title="Implement Export Promotion Policies",
                description="Subsidies and incentives for export-oriented industries",
                priority="high",
                expected_impact="Increased exports and improved trade balance",
                implementation_time="6-18 months",
                risk_level="medium"
            ))

        if data.trade_balance < 0:
            recommendations.append(PolicyRecommendation(
                title="Impose Import Restrictions",
                description="Tariffs and quotas to reduce import dependency",
                priority="high",
                expected_impact="Reduced imports and improved trade balance",
                implementation_time="3-6 months",
                risk_level="low"
            ))

        # Industrial development recommendations
        if data.private_investment < 30:
            recommendations.append(PolicyRecommendation(
                title="Develop Domestic Industries",
                description="Government support and protection for strategic industries",
                priority="high",
                expected_impact="Increased self-sufficiency and reduced imports",
                implementation_time="24-60 months",
                risk_level="medium"
            ))

        # Protectionism recommendations
        recommendations.append(PolicyRecommendation(
            title="Implement Protectionist Trade Policies",
            description="Comprehensive tariff structure to protect domestic industries",
            priority="medium",
            expected_impact="Domestic industry growth and employment",
            implementation_time="3-12 months",
            risk_level="low"
        ))

        # Currency manipulation recommendations
        recommendations.append(PolicyRecommendation(
            title="Maintain Favorable Exchange Rate",
            description="Monetary policy to keep currency undervalued for export competitiveness",
            priority="medium",
            expected_impact="Improved export competitiveness",
            implementation_time="1-6 months",
            risk_level="medium"
        ))

        # Strategic resource acquisition
        recommendations.append(PolicyRecommendation(
            title="Secure Strategic Resources",
            description="Government-led acquisition of critical resources and supply chains",
            priority="medium",
            expected_impact="Reduced foreign dependency and increased security",
            implementation_time="12-48 months",
            risk_level="medium"
        ))

        # Colonial/resource expansion recommendations
        if data.trade_balance < -8:
            recommendations.append(PolicyRecommendation(
                title="Expand Resource Access",
                description="Secure foreign sources of raw materials and resources",
                priority="high",
                expected_impact="Reduced import dependency and cost control",
                implementation_time="24-72 months",
                risk_level="high"
            ))

        # Domestic savings and capital accumulation
        if data.consumer_confidence > 80:
            recommendations.append(PolicyRecommendation(
                title("Promote Domestic Savings"),
                description="Policies to increase national savings and capital accumulation",
                priority="medium",
                expected_impact="Increased domestic capital formation",
                implementation_time="12-24 months",
                risk_level="low"
            ))

        return recommendations

    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Evaluate policy from mercantilist perspective"""
        policy_name = policy.get("name", "Unknown Policy")
        policy_type = policy.get("type", "unknown")

        # Scoring criteria for mercantilist evaluation
        scores = {
            "trade_balance_impact": 0,  # How it affects trade balance
            "national_wealth": 0,  # How it affects national wealth accumulation
            "self_sufficiency": 0,  # How it affects self-sufficiency
            "domestic_industry": 0,  # How it helps domestic industries
            "resource_control": 0  # How it increases resource control
        }

        # Evaluate based on policy type
        if policy_type == "export_subsidy":
            scores["trade_balance_impact"] = 90
            scores["domestic_industry"] = 85
            scores["national_wealth"] = 80
            scores["self_sufficiency"] = 75
        elif policy_type == "import_tariff":
            scores["trade_balance_impact"] = 85
            scores["self_sufficiency"] = 90
            scores["domestic_industry"] = 80
            scores["resource_control"] = 70
        elif policy_type == "import_quota":
            scores["trade_balance_impact"] = 80
            scores["self_sufficiency"] = 95
            scores["domestic_industry"] = 85
            scores["resource_control"] = 75
        elif policy_type == "industrial_subsidy":
            scores["domestic_industry"] = 95
            scores["self_sufficiency"] = 85
            scores["trade_balance_impact"] = 75
            scores["national_wealth"] = 70
        elif policy_type == "currency_devaluation":
            scores["trade_balance_impact"] = 85
            scores["domestic_industry"] = 80
            scores["national_wealth"] = 60
        elif policy_type == "free_trade":
            scores["trade_balance_impact"] = -80
            scores["self_sufficiency"] = -90
            scores["domestic_industry"] = -70
            scores["resource_control"] = -60
        elif policy_type == "trade_liberalization":
            scores["trade_balance_impact"] = -70
            scores["self_sufficiency"] = -80
            scores["domestic_industry"] = -65
            scores["resource_control"] = -55
        elif policy_type == "foreign_aid":
            scores["national_wealth"] = -40
            scores["resource_control"] = -30

        overall_score = sum(scores.values()) / len(scores)

        return {
            "policy_name": policy_name,
            "mercantilist_score": overall_score,
            "detailed_scores": scores,
            "recommendation": self._get_policy_recommendation(overall_score),
            "justification": self._generate_policy_justification(scores, policy_type)
        }

    def _calculate_trade_balance_score(self, data: EconomicData) -> float:
        """Calculate trade balance score (0-100)"""
        # Positive trade balance is good for mercantilism
        if data.trade_balance >= 10:
            return 100
        elif data.trade_balance >= 5:
            return 80
        elif data.trade_balance >= 0:
            return 60
        elif data.trade_balance >= -5:
            return 40
        elif data.trade_balance >= -10:
            return 20
        else:
            return max(0, 10 + data.trade_balance)  # Negative but not below zero

    def _calculate_self_sufficiency_score(self, data: EconomicData) -> float:
        """Calculate self-sufficiency score (0-100)"""
        # High domestic investment indicates self-sufficiency efforts
        investment_score = min(data.private_investment * 3, 100)

        # Government support for domestic industries
        government_score = min(data.government_spending * 2.5, 100)

        # Trade balance affects self-sufficiency
        trade_score = self._calculate_trade_balance_score(data)

        # Low unemployment indicates strong domestic economy
        employment_score = max(100 - data.unemployment * 10, 0)

        return (investment_score + government_score + trade_score + employment_score) / 4

    def _calculate_protection_score(self, data: EconomicData) -> float:
        """Calculate protection score (0-100)"""
        # This would ideally include actual protectionist policy data
        # Using proxy indicators that might suggest protectionist stance

        # Higher government spending might indicate industrial policy
        industrial_policy = min(data.government_spending * 2, 100)

        # Negative trade balance might trigger protectionism
        protection_need = max(0, -data.trade_balance * 10)

        # GDP growth suggests strength to implement policies
        economic_strength = min(data.gdp * 20, 100)

        return (industrial_policy + protection_need + economic_strength) / 3

    def _calculate_national_accumulation_score(self, data: EconomicData) -> float:
        """Calculate national wealth accumulation score (0-100)"""
        # GDP strength
        gdp_score = min(data.gdp * 25, 100)

        # Investment in productive capacity
        investment_score = min(data.private_investment * 3, 100)

        # Government capacity to accumulate wealth
        government_score = min(data.government_spending * 2.5, 100)

        # Price stability preserves wealth
        price_stability = max(100 - data.inflation * 15, 0)

        return (gdp_score + investment_score + government_score + price_stability) / 4

    def _generate_assessment(self, health_score: float, strengths: List[str], concerns: List[str]) -> str:
        """Generate overall assessment"""
        if health_score >= 80:
            return f"Excellent mercantilist performance (Score: {health_score:.1f}/100). Strong trade surplus and national wealth accumulation."
        elif health_score >= 60:
            return f"Good mercantilist indicators (Score: {health_score:.1f}/100). Positive trade performance with room for improvement."
        elif health_score >= 40:
            return f"Moderate mercantilist success (Score: {health_score:.1f}/100). Mixed performance requiring stronger trade policies."
        else:
            return f"Poor mercantilist outcomes (Score: {health_score:.1f}/100). Trade deficits undermining national wealth accumulation."

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

        if scores["trade_balance_impact"] > 60:
            justification.append("Improves trade balance")
        elif scores["trade_balance_impact"] < -20:
            justification.append("Worsens trade balance")

        if scores["self_sufficiency"] > 60:
            justification.append("Increases self-sufficiency")
        elif scores["self_sufficiency"] < -20:
            justification.append("Reduces self-sufficiency")

        if scores["domestic_industry"] > 60:
            justification.append("Supports domestic industries")
        elif scores["domestic_industry"] < -20:
            justification.append("Harms domestic industries")

        if scores["national_wealth"] > 60:
            justification.append("Accumulates national wealth")
        elif scores["national_wealth"] < -20:
            justification.append("Reduces national wealth")

        if scores["resource_control"] > 60:
            justification.append("Increases resource control")
        elif scores["resource_control"] < -20:
            justification.append("Reduces resource control"

        return "; ".join(justification) if justification else "Mixed effects on mercantilist objectives"