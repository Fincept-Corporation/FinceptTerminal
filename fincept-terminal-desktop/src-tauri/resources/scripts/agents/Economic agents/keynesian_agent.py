"""
Keynesian Economics Agent
Processes economic data through the lens of Keynesian economic principles
"""

from typing import Dict, List, Any
from base_agent import EconomicAgent, EconomicData, PolicyRecommendation

class KeynesianAgent(EconomicAgent):
    """Keynesian economics agent focusing on demand management and counter-cyclical policies"""

    def __init__(self):
        super().__init__(
            "Keynesian Agent",
            "Analyzes economic data through Keynesian principles emphasizing demand management, fiscal stimulus, and counter-cyclical policies"
        )
        self.principles = [
            "Aggregate demand management",
            "Counter-cyclical fiscal policy",
            "Government intervention during downturns",
            "Multiplier effect of spending",
            "Liquidity preference theory",
            "Active monetary policy"
        ]
        self.priorities = [
            "Full employment",
            "Economic stability",
            "Demand management",
            "Fiscal stimulus when needed",
            "Monetary accommodation",
            "Growth and investment"
        ]

    def analyze_economy(self, data: EconomicData) -> Dict[str, Any]:
        """Analyze economy from Keynesian perspective"""

        # Calculate Keynesian-specific metrics
        aggregate_demand_score = self._calculate_aggregate_demand_score(data)
        fiscal_stance_score = self._calculate_fiscal_stance_score(data)
        monetary_stance_score = self._calculate_monetary_stance_score(data)
        employment_gap_score = self._calculate_employment_gap_score(data)

        # Identify economic phase and needs
        economic_phase = self._determine_economic_phase(data)
        strengths = []
        concerns = []

        # Analyze based on economic phase
        if economic_phase == "recession":
            if data.government_spending > 35:
                strengths.append("Adequate government spending for stimulus")
            if data.interest_rate < 4:
                strengths.append("Accommodative monetary policy")
            if data.unemployment > 7:
                concerns.append("High unemployment requires stimulus")
            if data.consumer_confidence < 50:
                concerns.append("Weak consumer demand needs boosting")

        elif economic_phase == "expansion":
            if data.inflation > 4:
                concerns.append("Rising inflation requires cooling measures")
            if data.unemployment < 4:
                concerns.append("Very low unemployment may cause overheating")
            if data.government_spending > 40:
                concerns.append("Excessive spending may fuel inflation")

        elif economic_phase == "stable":
            if 2 <= data.inflation <= 3:
                strengths.append("Price stability maintained")
            if 4 <= data.unemployment <= 6:
                strengths.append("Sustainable employment levels")
            if data.gdp > 2 and data.gdp < 4:
                strengths.append("Stable economic growth")

        # General concerns
        if data.trade_balance < -8:
            concerns.append("Trade deficit may drain aggregate demand")
        if data.consumer_confidence < 60:
            concerns.append("Weak consumer confidence limits demand")

        # Calculate overall Keynesian health score
        health_score = (
            aggregate_demand_score * 0.3 +
            fiscal_stance_score * 0.25 +
            monetary_stance_score * 0.25 +
            employment_gap_score * 0.2
        )

        return {
            "ideology": "keynesian",
            "economic_phase": economic_phase,
            "aggregate_demand_score": aggregate_demand_score,
            "fiscal_stance_score": fiscal_stance_score,
            "monetary_stance_score": monetary_stance_score,
            "employment_gap_score": employment_gap_score,
            "overall_health_score": health_score,
            "strengths": strengths,
            "concerns": concerns,
            "assessment": self._generate_assessment(health_score, economic_phase, strengths, concerns),
            "key_indicators": {
                "economic_phase": economic_phase,
                "gdp_growth": data.gdp,
                "unemployment_rate": data.unemployment,
                "inflation_rate": data.inflation,
                "fiscal_stance": self._calculate_fiscal_stance(data),
                "monetary_stance": self._calculate_monetary_stance(data)
            }
        }

    def get_policy_recommendations(self, data: EconomicData, analysis: Dict[str, Any]) -> List[PolicyRecommendation]:
        """Generate Keynesian policy recommendations"""
        recommendations = []
        economic_phase = analysis["economic_phase"]

        # Recession-phase recommendations
        if economic_phase == "recession":
            recommendations.append(PolicyRecommendation(
                title="Implement Fiscal Stimulus",
                description="Increase government spending on infrastructure and public works to boost aggregate demand",
                priority="high",
                expected_impact="Increased GDP and employment through multiplier effect",
                implementation_time="3-6 months",
                risk_level="medium"
            ))

            recommendations.append(PolicyRecommendation(
                title="Tax Cuts for Middle and Lower Income",
                description="Targeted tax reductions to increase consumption and aggregate demand",
                priority="high",
                expected_impact="Immediate boost to consumer spending and economic activity",
                implementation_time="1-3 months",
                risk_level="low"
            ))

            recommendations.append(PolicyRecommendation(
                title="Expand Automatic Stabilizers",
                description="Strengthen unemployment benefits and progressive tax system",
                priority="medium",
                expected_impact="Automatic demand stabilization during downturns",
                implementation_time="6-12 months",
                risk_level="low"
            ))

            if data.interest_rate > 2:
                recommendations.append(PolicyRecommendation(
                    title="Accommodative Monetary Policy",
                    description="Lower interest rates to stimulate investment and consumption",
                    priority="high",
                    expected_impact="Increased private sector investment and borrowing",
                    implementation_time="1-2 months",
                    risk_level="low"
                ))

        # Expansion-phase recommendations
        elif economic_phase == "expansion":
            if data.inflation > 4:
                recommendations.append(PolicyRecommendation(
                    title="Implement Fiscal Consolidation",
                    description="Reduce government spending to cool overheating economy",
                    priority="medium",
                    expected_impact="Reduced inflationary pressures",
                    implementation_time="6-18 months",
                    risk_level="medium"
                ))

                recommendations.append(PolicyRecommendation(
                    title="Tighten Monetary Policy",
                    description="Raise interest rates to control inflation",
                    priority="high",
                    expected_impact="Reduced inflation and asset price bubbles",
                    implementation_time="1-3 months",
                    risk_level="medium"
                ))

        # Stable economy recommendations
        else:
            recommendations.append(PolicyRecommendation(
                title="Maintain Counter-Cyclical Policies",
                description="Maintain fiscal and monetary tools ready for economic fluctuations",
                priority="medium",
                expected_impact="Preparedness for future economic cycles",
                implementation_time="ongoing",
                risk_level="low"
            ))

        # Always recommend structural investments
        recommendations.append(PolicyRecommendation(
            title="Invest in Productive Infrastructure",
            description="Long-term infrastructure investment to increase productive capacity",
            priority="medium",
            expected_impact="Improved long-term growth potential",
            implementation_time="24-60 months",
            risk_level="medium"
        ))

        # Trade balance concerns
        if data.trade_balance < -8:
            recommendations.append(PolicyRecommendation(
                title="Manage Trade Balance",
                description="Policies to improve net exports and reduce trade deficit",
                priority="medium",
                expected_impact="Improved aggregate demand and economic stability",
                implementation_time="12-36 months",
                risk_level="medium"
            ))

        return recommendations

    def evaluate_policy(self, policy: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Evaluate policy from Keynesian perspective"""
        policy_name = policy.get("name", "Unknown Policy")
        policy_type = policy.get("type", "unknown")

        # Scoring criteria for Keynesian evaluation
        scores = {
            "demand_impact": 0,  # How it affects aggregate demand
            "employment_effect": 0,  # How it affects employment
            "stabilization": 0,  # How it contributes to economic stability
            "multiplier_effect": 0,  # Fiscal multiplier potential
            "counter_cyclical": 0  # How well it works counter-cyclically
        }

        economic_phase = self._determine_economic_phase(data)

        # Evaluate based on policy type and economic context
        if policy_type == "fiscal_stimulus":
            if economic_phase == "recession":
                scores["demand_impact"] = 90
                scores["employment_effect"] = 85
                scores["stabilization"] = 80
                scores["multiplier_effect"] = 95
                scores["counter_cyclical"] = 100
            else:
                scores["demand_impact"] = 20
                scores["employment_effect"] = 30
                scores["stabilization"] = -40
                scores["multiplier_effect"] = 50
                scores["counter_cyclical"] = -60

        elif policy_type == "tax_cut":
            if economic_phase == "recession":
                scores["demand_impact"] = 75
                scores["employment_effect"] = 70
                scores["stabilization"] = 65
                scores["multiplier_effect"] = 80
                scores["counter_cyclical"] = 85
            else:
                scores["demand_impact"] = 40
                scores["employment_effect"] = 45
                scores["stabilization"] = -30
                scores["counter_cyclical"] = -40

        elif policy_type == "infrastructure_spending":
            scores["demand_impact"] = 80
            scores["employment_effect"] = 85
            scores["stabilization"] = 75
            scores["multiplier_effect"] = 90
            scores["counter_cyclical"] = 70

        elif policy_type == "monetary_easing":
            if economic_phase == "recession":
                scores["demand_impact"] = 70
                scores["employment_effect"] = 65
                scores["stabilization"] = 75
                scores["multiplier_effect"] = 60
                scores["counter_cyclical"] = 80
            else:
                scores["demand_impact"] = 30
                scores["employment_effect"] = 35
                scores["stabilization"] = -20
                scores["counter_cyclical"] = -30

        elif policy_type == "austerity":
            if economic_phase == "recession":
                scores["demand_impact"] = -80
                scores["employment_effect"] = -75
                scores["stabilization"] = -70
                scores["multiplier_effect"] = -90
                scores["counter_cyclical"] = -100
            elif economic_phase == "expansion":
                scores["demand_impact"] = 50
                scores["employment_effect"] = 45
                scores["stabilization"] = 70
                scores["multiplier_effect"] = 40
                scores["counter_cyclical"] = 80

        overall_score = sum(scores.values()) / len(scores)

        return {
            "policy_name": policy_name,
            "keynesian_score": overall_score,
            "detailed_scores": scores,
            "recommendation": self._get_policy_recommendation(overall_score),
            "justification": self._generate_policy_justification(scores, policy_type, economic_phase)
        }

    def _determine_economic_phase(self, data: EconomicData) -> str:
        """Determine current economic phase"""
        if data.gdp < 1.5 and data.unemployment > 7:
            return "recession"
        elif data.gdp > 4.0 or data.inflation > 4:
            return "expansion"
        else:
            return "stable"

    def _calculate_aggregate_demand_score(self, data: EconomicData) -> float:
        """Calculate aggregate demand score (0-100)"""
        # Consumer demand (confidence, employment)
        consumer_score = (data.consumer_confidence + max(100 - data.unemployment * 10, 0)) / 2

        # Investment demand (interest rates, GDP growth)
        investment_score = (max(100 - data.interest_rate * 8, 0) + min(data.gdp * 15, 100)) / 2

        # Government demand (spending)
        gov_score = min(data.government_spending * 2, 100)

        # Net exports (trade balance)
        trade_score = max(100 - abs(data.trade_balance) * 8, 0)

        return (consumer_score + investment_score + gov_score + trade_score) / 4

    def _calculate_fiscal_stance_score(self, data: EconomicData) -> float:
        """Calculate appropriate fiscal stance score (0-100)"""
        economic_phase = self._determine_economic_phase(data)
        current_stance = self._calculate_fiscal_stance(data)

        if economic_phase == "recession":
            # Need expansionary fiscal policy
            if current_stance == "expansionary":
                return 90
            elif current_stance == "neutral":
                return 60
            else:
                return 30
        elif economic_phase == "expansion":
            # Need contractionary fiscal policy
            if current_stance == "contractionary":
                return 90
            elif current_stance == "neutral":
                return 70
            else:
                return 40
        else:
            # Neutral is fine
            return 80

    def _calculate_monetary_stance_score(self, data: EconomicData) -> float:
        """Calculate appropriate monetary stance score (0-100)"""
        economic_phase = self._determine_economic_phase(data)
        current_stance = self._calculate_monetary_stance(data)

        if economic_phase == "recession":
            # Need accommodative monetary policy
            if current_stance == "accommodative":
                return 90
            elif current_stance == "neutral":
                return 65
            else:
                return 35
        elif economic_phase == "expansion":
            # Need tight monetary policy
            if current_stance == "tight":
                return 85
            elif current_stance == "neutral":
                return 70
            else:
                return 45
        else:
            # Neutral is good
            return 85

    def _calculate_employment_gap_score(self, data: EconomicData) -> float:
        """Calculate employment gap score (0-100)"""
        # Target unemployment around 5%
        target_unemployment = 5.0
        unemployment_gap = abs(data.unemployment - target_unemployment)
        return max(100 - unemployment_gap * 15, 0)

    def _calculate_fiscal_stance(self, data: EconomicData) -> str:
        """Determine current fiscal stance"""
        if data.government_spending > 40:
            return "expansionary"
        elif data.government_spending < 25:
            return "contractionary"
        else:
            return "neutral"

    def _calculate_monetary_stance(self, data: EconomicData) -> str:
        """Determine current monetary stance"""
        if data.interest_rate < 3:
            return "accommodative"
        elif data.interest_rate > 6:
            return "tight"
        else:
            return "neutral"

    def _generate_assessment(self, health_score: float, economic_phase: str, strengths: List[str], concerns: List[str]) -> str:
        """Generate overall assessment"""
        phase_description = {
            "recession": "in recession",
            "expansion": "in expansion",
            "stable": "stable"
        }

        if health_score >= 80:
            return f"Excellent Keynesian management (Score: {health_score:.1f}/100). Appropriate policies for current {phase_description[economic_phase]} phase."
        elif health_score >= 60:
            return f"Good policy approach (Score: {health_score:.1f}/100). Generally appropriate policies for current economic conditions."
        elif health_score >= 40:
            return f"Moderate policy effectiveness (Score: {health_score:.1f}/100). Some policy adjustments needed for better management."
        else:
            return f"Poor policy alignment (Score: {health_score:.1f}/100). Significant policy reforms needed for current economic phase."

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

    def _generate_policy_justification(self, scores: Dict[str, float], policy_type: str, economic_phase: str) -> str:
        """Generate justification for policy evaluation"""
        justification = []

        if scores["demand_impact"] > 60:
            justification.append("Boosts aggregate demand")
        elif scores["demand_impact"] < -20:
            justification.append("Reduces aggregate demand")

        if scores["employment_effect"] > 60:
            justification.append("Supports full employment")
        elif scores["employment_effect"] < -20:
            justification.append("Harmful to employment")

        if scores["counter_cyclical"] > 60:
            justification.append("Appropriate for economic cycle")
        elif scores["counter_cyclical"] < -20:
            justification.append("Wrong timing for economic cycle")

        if scores["multiplier_effect"] > 60:
            justification.append("Strong multiplier effects")
        elif scores["multiplier_effect"] < 0:
            justification.append("Negative multiplier effects")

        return "; ".join(justification) if justification else "Mixed effects on Keynesian objectives"