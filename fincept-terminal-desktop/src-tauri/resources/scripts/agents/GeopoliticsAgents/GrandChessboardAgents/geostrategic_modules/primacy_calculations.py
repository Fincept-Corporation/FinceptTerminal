"""
PRIMACY ASSESSMENT MODULE
Geostrategic Module for Grand Chessboard Agents

This module implements Brzezinski's American primacy assessment framework,
evaluating the current state and prospects of American global leadership
and providing strategic recommendations for maintaining primacy.
"""

import logging
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
import math

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class PrimacyDimension(Enum):
    """Dimensions of national power relevant to global primacy"""
    MILITARY_SUPREMACY = "military_supremacy"
    ECONOMIC_DOMINANCE = "economic_dominance"
    TECHNOLOGICAL_SUPERIORITY = "technological_superiority"
    DIPLOMATIC_LEADERSHIP = "diplomatic_leadership"
    CULTURAL_INFLUENCE = "cultural_influence"
    ALLIANCE_NETWORK = "alliance_network"

class PrimacyStatus(Enum):
    """Current status of American primacy"""
    DOMINANT = "dominant"           # Clear global leadership across all dimensions
    MAINTAINING = "maintaining"     # Strong but facing challenges
    CHALLENGED = "challenged"        # Significant threats to primacy
    DECLINING = "declining"         # Clear erosion of primacy
    COMPETITIVE = "competitive"      # Multipolar competition

@dataclass
class PrimacyMetrics:
    """Metrics for evaluating American primacy"""
    current_status: PrimacyStatus
    military_advantage: float        # 0-1 scale
    economic_leadership: float       # 0-1 scale
    technological_edge: float        # 0-1 scale
    diplomatic_influence: float      # 0-1 scale
    cultural_reach: float           # 0-1 scale
    alliance_strength: float          # 0-1 scale
    overall_primacy_score: float     # Composite score

@dataclass
class PrimacyChallenge:
    """Specific challenge to American primacy"""
    challenge_type: str
    severity: str                    # low, medium, high, critical
    affected_dimensions: List[PrimacyDimension]
    strategic_impact: str
    recommended_responses: List[str]

class PrimacyAssessment:
    """Comprehensive primacy assessment results"""
    current_metrics: PrimacyMetrics
    challenges: List[PrimacyChallenge]
    opportunities: List[str]
    primacy_trends: Dict[str, Any]
    strategic_recommendations: List[str]
    future_outlook: Dict[str, Any]

class PrimacyAssessment:
    """
    Evaluates American global primacy following Brzezinski's framework
    for maintaining American leadership in the 21st century
    """

    def __init__(self):
        """Initialize the primacy assessment module"""
        self.module_id = "primacy_calculations"

        # Dimension weights based on Brzezinski's primacy framework
        self.dimension_weights = {
            PrimacyDimension.MILITARY_SUPREMACY: 0.30,
            PrimacyDimension.ECONOMIC_DOMINANCE: 0.25,
            PrimacyDimension.TECHNOLOGICAL_SUPERIORITY: 0.20,
            PrimacyDimension.DIPLOMATIC_LEADERSHIP: 0.15,
            PrimacyDimension.ALLIANCE_NETWORK: 0.10
        }

        # Primacy thresholds
        self.dominant_threshold = 0.8
        self.maintaining_threshold = 0.6
        self.challenged_threshold = 0.4
        self.competitive_threshold = 0.2

        # Key global regions for primacy assessment
        self.critical_regions = [
            "north_america", "europe", "east_asia", "middle_east",
            "south_asia", "latin_america", "africa", "oceania"
        ]

        # Primary challengers to American primacy
        self.primary_challengers = ["china", "russia"]

    def evaluate_american_primacy(
        self,
        power_distribution: Dict[str, float],
        alliance_structures: Dict[str, List[str]],
        regional_dynamics: Dict[str, Dict[str, Any]]
    ) -> PrimacyAssessment:
        """
        Evaluate current state and prospects of American global primacy

        Args:
            power_distribution: Global power distribution by country
            alliance_structures: Current alliance network structures
            regional_dynamics: Regional geopolitical dynamics

        Returns:
            Comprehensive primacy assessment with strategic recommendations
        """
        try:
            logger.info("Evaluating American global primacy")

            # Calculate current primacy metrics
            current_metrics = self._calculate_primacy_metrics(
                power_distribution, alliance_structures, regional_dynamics
            )

            # Identify primacy challenges
            challenges = self._identify_primacy_challenges(
                current_metrics, power_distribution, regional_dynamics
            )

            # Identify primacy opportunities
            opportunities = self._identify_primacy_opportunities(
                current_metrics, alliance_structures, regional_dynamics
            )

            # Analyze primacy trends
            primacy_trends = self._analyze_primacy_trends(
                current_metrics, regional_dynamics
            )

            # Generate strategic recommendations
            strategic_recommendations = self._generate_primacy_recommendations(
                current_metrics, challenges, opportunities
            )

            # Assess future outlook
            future_outlook = self._assess_future_primacy_outlook(
                current_metrics, challenges, opportunities, regional_dynamics
            )

            return PrimacyAssessment(
                current_metrics=current_metrics,
                challenges=challenges,
                opportunities=opportunities,
                primacy_trends=primacy_trends,
                strategic_recommendations=strategic_recommendations,
                future_outlook=future_outlook
            )

        except Exception as e:
            logger.error(f"Error evaluating American primacy: {str(e)}")
            raise

    def _calculate_primacy_metrics(
        self,
        power_distribution: Dict[str, float],
        alliance_structures: Dict[str, List[str]],
        regional_dynamics: Dict[str, Dict[str, Any]]
    ) -> PrimacyMetrics:
        """Calculate comprehensive primacy metrics for the United States"""

        # Extract American power share
        us_power_share = power_distribution.get("united_states", 0)

        # Calculate military supremacy
        military_advantage = self._assess_military_supremacy(power_distribution)

        # Calculate economic leadership
        economic_leadership = self._assess_economic_leadership(power_distribution)

        # Calculate technological superiority
        technological_edge = self._assess_technological_superiority(power_distribution)

        # Calculate diplomatic leadership
        diplomatic_influence = self._assess_diplomatic_leadership(
            alliance_structures, regional_dynamics
        )

        # Calculate cultural influence
        cultural_reach = self._assess_cultural_influence(power_distribution)

        # Calculate alliance network strength
        alliance_strength = self._assess_alliance_network_strength(alliance_structures)

        # Calculate overall primacy score
        overall_primacy_score = self._calculate_composite_primacy_score(
            military_advantage, economic_leadership, technological_edge,
            diplomatic_influence, alliance_strength
        )

        # Determine primacy status
        current_status = self._determine_primacy_status(overall_primacy_score)

        return PrimacyMetrics(
            current_status=current_status,
            military_advantage=military_advantage,
            economic_leadership=economic_leadership,
            technological_edge=technological_edge,
            diplomatic_influence=diplomatic_influence,
            cultural_reach=cultural_reach,
            alliance_strength=alliance_strength,
            overall_primacy_score=overall_primacy_score
        )

    def _assess_military_supremacy(self, power_distribution: Dict[str, float]) -> float:
        """Assess American military supremacy relative to other powers"""

        us_military_power = power_distribution.get("united_states_military", 0)
        china_military_power = power_distribution.get("china_military", 0)
        russia_military_power = power_distribution.get("russia_military", 0)

        # Compare to nearest competitors
        max_competitor_power = max(china_military_power, russia_military_power)

        if max_competitor_power == 0:
            return 1.0

        # Calculate military advantage ratio
        military_ratio = us_military_power / max_competitor_power

        # Normalize to 0-1 scale (2:1 ratio = 1.0, 1:1 ratio = 0.5, 0.5:1 ratio = 0.0)
        military_advantage = max(0.0, min(1.0, (military_ratio - 0.5) / 1.5))

        return military_advantage

    def _assess_economic_leadership(self, power_distribution: Dict[str, float]) -> float:
        """Assess American economic leadership globally"""

        us_economic_power = power_distribution.get("united_states_economic", 0)
        china_economic_power = power_distribution.get("china_economic", 0)
        total_global_economy = sum(
            power for key, power in power_distribution.items()
            if "economic" in key
        )

        if total_global_economy == 0:
            return 0.0

        # Economic dominance measured by share of global economy and lead over China
        global_share = us_economic_power / total_global_economy
        china_gap = (us_economic_power - china_economic_power) / total_global_economy

        # Combine global dominance and competitive advantage
        economic_leadership = (global_share * 0.6 + max(0, china_gap) * 0.4) * 2

        return min(1.0, max(0.0, economic_leadership))

    def _assess_technological_superiority(self, power_distribution: Dict[str, float]) -> float:
        """Assess American technological superiority in critical domains"""

        us_tech_power = power_distribution.get("united_states_technology", 0)
        china_tech_power = power_distribution.get("china_technology", 0)
        total_global_tech = sum(
            power for key, power in power_distribution.items()
            if "technology" in key
        )

        if total_global_tech == 0:
            return 0.0

        # Technology dominance with emphasis on key domains
        tech_share = us_tech_power / total_global_tech
        tech_advantage = (us_tech_power - china_tech_power) / total_global_tech

        technological_edge = (tech_share * 0.7 + max(0, tech_advantage) * 0.3) * 2

        return min(1.0, max(0.0, technological_edge))

    def _assess_diplomatic_leadership(
        self,
        alliance_structures: Dict[str, List[str]],
        regional_dynamics: Dict[str, Dict[str, Any]]
    ) -> float:
        """Assess American diplomatic leadership and alliance management"""

        # Evaluate alliance network strength
        nato_strength = self._assess_nato_strength(alliance_structures)
        asian_alliances = self._assess_asian_alliance_strength(alliance_structures)
        global_partnerships = self._assess_global_partnerships(alliance_structures)

        # Evaluate diplomatic influence in regions
        regional_influence = 0
        for region in self.critical_regions:
            region_data = regional_dynamics.get(region, {})
            us_influence = region_data.get("us_influence", 0.5)
            regional_influence += us_influence

        regional_influence /= len(self.critical_regions)

        # Combine alliance strength and regional influence
        alliance_leadership = (nato_strength * 0.4 + asian_alliances * 0.3 +
                             global_partnerships * 0.2 + regional_influence * 0.1)

        return min(1.0, max(0.0, alliance_leadership))

    def _assess_cultural_influence(self, power_distribution: Dict[str, float]) -> float:
        """Assess American cultural influence globally"""

        us_cultural_power = power_distribution.get("united_states_cultural", 0)
        total_cultural_power = sum(
            power for key, power in power_distribution.items()
            if "cultural" in key
        )

        if total_cultural_power == 0:
            return 0.0

        # Cultural influence measured by global share
        cultural_reach = us_cultural_power / total_cultural_power

        # Adjust for global reach and penetration
        cultural_reach = min(1.0, cultural_reach * 1.5)  # Boost for US cultural dominance

        return cultural_reach

    def _assess_alliance_network_strength(self, alliance_structures: Dict[str, List[str]]) -> float:
        """Assess strength and effectiveness of American alliance network"""

        # Key alliance structures
        nato_members = alliance_structures.get("nato", [])
        asian_alliances = alliance_structures.get("asian_alliances", [])
        middle_east_partners = alliance_structures.get("middle_east_partners", [])

        # Calculate alliance scores
        nato_score = len([m for m in nato_members if m != "united_states"]) / 30  # 30 potential members
        asian_score = len(asian_alliances) / 10  # 10 key Asian partners
        me_score = len(middle_east_partners) / 8  # 8 key Middle East partners

        # Weight alliance importance
        alliance_strength = (nato_score * 0.5 + asian_score * 0.3 + me_score * 0.2)

        return min(1.0, max(0.0, alliance_strength))

    def _calculate_composite_primacy_score(
        self,
        military: float,
        economic: float,
        technological: float,
        diplomatic: float,
        alliance: float
    ) -> float:
        """Calculate composite primacy score from individual dimensions"""

        return (
            military * self.dimension_weights[PrimacyDimension.MILITARY_SUPREMACY] +
            economic * self.dimension_weights[PrimacyDimension.ECONOMIC_DOMINANCE] +
            technological * self.dimension_weights[PrimacyDimension.TECHNOLOGICAL_SUPERIORITY] +
            diplomatic * self.dimension_weights[PrimacyDimension.DIPLOMATIC_LEADERSHIP] +
            alliance * self.dimension_weights[PrimacyDimension.ALLIANCE_NETWORK]
        )

    def _determine_primacy_status(self, primacy_score: float) -> PrimacyStatus:
        """Determine current primacy status based on composite score"""

        if primacy_score >= self.dominant_threshold:
            return PrimacyStatus.DOMINANT
        elif primacy_score >= self.maintaining_threshold:
            return PrimacyStatus.MAINTAINING
        elif primacy_score >= self.challenged_threshold:
            return PrimacyStatus.CHALLENGED
        elif primacy_score >= self.competitive_threshold:
            return PrimacyStatus.DECLINING
        else:
            return PrimacyStatus.COMPETITIVE

    def _identify_primacy_challenges(
        self,
        metrics: PrimacyMetrics,
        power_distribution: Dict[str, float],
        regional_dynamics: Dict[str, Dict[str, Any]]
    ) -> List[PrimacyChallenge]:
        """Identify specific challenges to American primacy"""

        challenges = []

        # Military challenges
        if metrics.military_advantage < 0.7:
            challenges.append(PrimacyChallenge(
                challenge_type="military_competition",
                severity=self._assess_severity(metrics.military_advantage),
                affected_dimensions=[PrimacyDimension.MILITARY_SUPREMACY],
                strategic_impact="Reduced military deterrence capability",
                recommended_responses=[
                    "Increase defense R&D investment",
                    "Modernize nuclear arsenal",
                    "Strengthen alliance military capabilities"
                ]
            ))

        # Economic challenges
        if metrics.economic_leadership < 0.6:
            challenges.append(PrimacyChallenge(
                challenge_type="economic_competition",
                severity=self._assess_severity(metrics.economic_leadership),
                affected_dimensions=[PrimacyDimension.ECONOMIC_DOMINANCE],
                strategic_impact="Reduced global economic influence",
                recommended_responses=[
                    "Enhance technological innovation",
                    "Improve trade competitiveness",
                    "Invest in critical infrastructure"
                ]
            ))

        # China rise as specific challenge
        china_power = power_distribution.get("china", 0)
        us_power = power_distribution.get("united_states", 0)
        if china_power > us_power * 0.8:
            challenges.append(PrimacyChallenge(
                challenge_type="china_regional_hegemony",
                severity="high" if china_power > us_power * 0.9 else "medium",
                affected_dimensions=[
                    PrimacyDimension.MILITARY_SUPREMACY,
                    PrimacyDimension.ECONOMIC_DOMINANCE,
                    PrimacyDimension.DIPLOMATIC_LEADERSHIP
                ],
                strategic_impact="China approaching parity in key domains",
                recommended_responses=[
                    "Strengthen Indo-Pacific alliances",
                    "Develop regional containment strategies",
                    "Maintain forward military presence"
                ]
            ))

        # Regional instability challenges
        for region, data in regional_dynamics.items():
            if data.get("conflict_risk") == "high":
                challenges.append(PrimacyChallenge(
                    challenge_type=f"regional_instability_{region}",
                    severity="high",
                    affected_dimensions=[PrimacyDimension.DIPLOMATIC_LEADERSHIP],
                    strategic_impact=f"Regional instability in {region} challenging American influence",
                    recommended_responses=[
                        "Enhance diplomatic engagement",
                        "Provide security guarantees to partners",
                        "Support regional conflict resolution"
                    ]
                ))

        return challenges

    def _identify_primacy_opportunities(
        self,
        metrics: PrimacyMetrics,
        alliance_structures: Dict[str, List[str]],
        regional_dynamics: Dict[str, Dict[str, Any]]
    ) -> List[str]:
        """Identify opportunities to strengthen American primacy"""

        opportunities = []

        # Technological leadership opportunities
        if metrics.technological_edge > 0.7:
            opportunities.append(
                "Leverage technological superiority to expand global influence"
            )

        # Alliance expansion opportunities
        if metrics.alliance_strength < 0.7:
            opportunities.append(
                "Expand and strengthen alliance network to amplify American power"
            )

        # Regional opportunities
        for region, data in regional_dynamics.items():
            if data.get("us_influence", 0.5) > 0.6:
                opportunities.append(
                    f"Deepen strategic partnerships in {region} to consolidate influence"
                )

        # Economic opportunities
        if metrics.economic_leadership > 0.6:
            opportunities.append(
                "Use economic leverage to support diplomatic objectives and alliance commitments"
            )

        return opportunities

    def _analyze_primacy_trends(
        self,
        metrics: PrimacyMetrics,
        regional_dynamics: Dict[str, Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Analyze current trends affecting American primacy"""

        trends = {}

        # Analyze regional trends
        gaining_regions = []
        losing_regions = []

        for region, data in regional_dynamics.items():
            us_influence = data.get("us_influence", 0.5)
            historical_influence = data.get("historical_us_influence", 0.5)

            if us_influence > historical_influence * 1.1:
                gaining_regions.append(region)
            elif us_influence < historical_influence * 0.9:
                losing_regions.append(region)

        trends["regional_influence"] = {
            "gaining": gaining_regions,
            "losing": losing_regions,
            "overall_trend": "expanding" if len(gaining_regions) > len(losing_regions) else "contracting"
        }

        # Analyze dimension-specific trends
        dimension_trends = {}
        if metrics.military_advantage < 0.6:
            dimension_trends["military"] = "challenged"
        if metrics.economic_leadership < 0.6:
            dimension_trends["economic"] = "challenged"
        if metrics.technological_edge > 0.7:
            dimension_trends["technology"] = "dominant"
        if metrics.diplomatic_influence > 0.6:
            dimension_trends["diplomatic"] = "strong"

        trends["dimensional_trends"] = dimension_trends
        trends["overall_trend"] = self._determine_overall_trend(dimension_trends)

        return trends

    def _generate_primacy_recommendations(
        self,
        metrics: PrimacyMetrics,
        challenges: List[PrimacyChallenge],
        opportunities: List[str]
    ) -> List[str]:
        """Generate strategic recommendations for maintaining American primacy"""

        recommendations = []

        # Address critical challenges
        high_severity_challenges = [c for c in challenges if c.severity in ["high", "critical"]]
        for challenge in high_severity_challenges:
            recommendations.extend(challenge.recommended_responses[:2])  # Top 2 recommendations

        # Leverage opportunities
        recommendations.extend(opportunities[:3])  # Top 3 opportunities

        # Dimension-specific recommendations
        if metrics.military_advantage < 0.6:
            recommendations.extend([
                "Accelerate military modernization programs",
                "Increase defense spending to maintain technological edge",
                "Enhance force projection capabilities in critical regions"
            ])

        if metrics.economic_leadership < 0.6:
            recommendations.extend([
                "Promote innovation and technological development",
                "Address trade imbalances and competitiveness",
                "Invest in infrastructure and human capital"
            ])

        if metrics.alliance_strength < 0.6:
            recommendations.extend([
                "Deepen transatlantic partnerships",
                "Expand Indo-Pacific alliance network",
                "Strengthen burden-sharing arrangements with allies"
            ])

        # Remove duplicates and limit to top recommendations
        unique_recommendations = list(set(recommendations))
        return unique_recommendations[:10]  # Top 10 recommendations

    def _assess_future_primacy_outlook(
        self,
        metrics: PrimacyMetrics,
        challenges: List[PrimacyChallenge],
        opportunities: List[str],
        regional_dynamics: Dict[str, Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Assess future outlook for American primacy"""

        # Calculate challenge severity
        challenge_score = sum(
            {"low": 1, "medium": 2, "high": 3, "critical": 4}[c.severity]
            for c in challenges
        ) / max(1, len(challenges))

        # Calculate opportunity potential
        opportunity_score = len(opportunities) / 10  # Normalized

        # Project future primacy score
        current_score = metrics.overall_primacy_score
        projected_change = (opportunity_score - challenge_score) * 0.1
        future_score = max(0, min(1, current_score + projected_change))

        # Determine future status
        future_status = self._determine_primacy_status(future_score)

        # Identify key factors influencing outlook
        key_factors = []
        if challenge_score > 2.5:
            key_factors.append("High severity challenges require immediate attention")
        if opportunity_score > 0.5:
            key_factors.append("Significant opportunities for primacy enhancement")
        if metrics.military_advantage > 0.7:
            key_factors.append("Military superiority provides strategic foundation")
        if metrics.economic_leadership < 0.5:
            key_factors.append("Economic challenges threaten long-term primacy")

        return {
            "projected_score": future_score,
            "projected_status": future_status,
            "outlook_assessment": self._assess_outlook_quality(challenge_score, opportunity_score),
            "key_factors": key_factors,
            "critical_priorities": self._identify_critical_priorities(challenges, metrics),
            "time_horizon": "5-10 years"
        }

    def _assess_severity(self, metric_value: float) -> str:
        """Assess severity of challenge based on metric value"""
        if metric_value < 0.3:
            return "critical"
        elif metric_value < 0.5:
            return "high"
        elif metric_value < 0.7:
            return "medium"
        else:
            return "low"

    def _assess_nato_strength(self, alliance_structures: Dict[str, List[str]]) -> float:
        """Assess NATO alliance strength and cohesion"""
        nato_members = alliance_structures.get("nato", [])
        return min(1.0, len(nato_members) / 32)  # 32 potential NATO members

    def _assess_asian_alliance_strength(self, alliance_structures: Dict[str, List[str]]) -> float:
        """Assess strength of Asian alliance network"""
        asian_alliances = alliance_structures.get("asian_alliances", [])
        key_partners = ["japan", "south_korea", "australia", "philippines"]
        alliance_strength = sum(1 for partner in key_partners if partner in asian_alliances)
        return min(1.0, alliance_strength / len(key_partners))

    def _assess_global_partnerships(self, alliance_structures: Dict[str, List[str]]) -> float:
        """Assess global partnership network"""
        partnerships = alliance_structures.get("global_partnerships", [])
        return min(1.0, len(partnerships) / 20)  # 20 key global partners

    def _determine_overall_trend(self, dimension_trends: Dict[str, str]) -> str:
        """Determine overall primacy trend from dimension trends"""
        challenged_count = sum(1 for trend in dimension_trends.values() if trend == "challenged")
        strong_count = sum(1 for trend in dimension_trends.values() if trend in ["strong", "dominant"])

        if challenged_count > strong_count:
            return "declining"
        elif strong_count > challenged_count:
            return "strengthening"
        else:
            return "stable"

    def _assess_outlook_quality(self, challenge_score: float, opportunity_score: float) -> str:
        """Assess quality of future outlook"""
        if challenge_score < 1.5 and opportunity_score > 0.6:
            return "favorable"
        elif challenge_score < 2.5 and opportunity_score > 0.3:
            return "cautiously_optimistic"
        elif challenge_score < 3.0:
            return "challenged"
        else:
            return "concerning"

    def _identify_critical_priorities(
        self,
        challenges: List[PrimacyChallenge],
        metrics: PrimacyMetrics
    ) -> List[str]:
        """Identify critical priorities for maintaining primacy"""

        priorities = []

        # Address most severe challenges
        critical_challenges = [c for c in challenges if c.severity == "critical"]
        if critical_challenges:
            priorities.append("Immediate attention to critical primacy threats")

        # Strengthen weakest dimensions
        dimension_scores = [
            ("Military", metrics.military_advantage),
            ("Economic", metrics.economic_leadership),
            ("Technology", metrics.technological_edge),
            ("Diplomatic", metrics.diplomatic_influence),
            ("Alliance", metrics.alliance_strength)
        ]

        weakest_dimension = min(dimension_scores, key=lambda x: x[1])
        if weakest_dimension[1] < 0.5:
            priorities.append(f"Strengthen {weakest_dimension[0]} capabilities")

        # Maintain strongest advantages
        strongest_dimension = max(dimension_scores, key=lambda x: x[1])
        if strongest_dimension[1] > 0.7:
            priorities.append(f"Maintain {strongest_dimension[0]} superiority")

        return priorities

# Example usage and testing
if __name__ == "__main__":
    # Initialize primacy assessor
    assessor = PrimacyAssessment()

    # Sample data for testing
    power_distribution = {
        "united_states": 0.25,
        "china": 0.18,
        "russia": 0.08,
        "united_states_military": 0.35,
        "china_military": 0.20,
        "russia_military": 0.15,
        "united_states_economic": 0.24,
        "china_economic": 0.19,
        "russia_economic": 0.04,
        "united_states_technology": 0.30,
        "china_technology": 0.22,
        "russia_technology": 0.08,
        "united_states_cultural": 0.40,
        "china_cultural": 0.15,
        "russia_cultural": 0.05
    }

    alliance_structures = {
        "nato": ["united_states", "germany", "france", "united_kingdom", "italy", "canada", "poland"],
        "asian_alliances": ["united_states", "japan", "south_korea", "australia"],
        "global_partnerships": ["united_states", "india", "brazil", "south_africa", "israel"]
    }

    regional_dynamics = {
        "europe": {"us_influence": 0.7, "historical_us_influence": 0.8, "conflict_risk": "low"},
        "east_asia": {"us_influence": 0.6, "historical_us_influence": 0.7, "conflict_risk": "medium"},
        "middle_east": {"us_influence": 0.5, "historical_us_influence": 0.6, "conflict_risk": "high"},
        "europe": {"us_influence": 0.7, "historical_us_influence": 0.8, "conflict_risk": "low"},
        "north_america": {"us_influence": 0.9, "historical_us_influence": 0.9, "conflict_risk": "low"}
    }

    # Evaluate American primacy
    assessment = assessor.evaluate_american_primacy(
        power_distribution, alliance_structures, regional_dynamics
    )

    print("American Primacy Assessment:")
    print(f"Current Status: {assessment.current_metrics.current_status}")
    print(f"Overall Primacy Score: {assessment.current_metrics.overall_primacy_score:.2f}")
    print(f"Military Advantage: {assessment.current_metrics.military_advantage:.2f}")
    print(f"Economic Leadership: {assessment.current_metrics.economic_leadership:.2f}")
    print(f"Technological Edge: {assessment.current_metrics.technological_edge:.2f}")
    print(f"Diplomatic Influence: {assessment.current_metrics.diplomatic_influence:.2f}")
    print(f"Alliance Strength: {assessment.current_metrics.alliance_strength:.2f}")

    print(f"\nChallenges Identified: {len(assessment.challenges)}")
    for challenge in assessment.challenges:
        print(f"  - {challenge.challenge_type}: {challenge.severity}")

    print(f"\nStrategic Opportunities: {len(assessment.opportunities)}")
    for opportunity in assessment.opportunities:
        print(f"  - {opportunity}")

    print(f"\nFuture Outlook: {assessment.future_outlook['projected_status']}")
    print(f"Projected Score: {assessment.future_outlook['projected_score']:.2f}")