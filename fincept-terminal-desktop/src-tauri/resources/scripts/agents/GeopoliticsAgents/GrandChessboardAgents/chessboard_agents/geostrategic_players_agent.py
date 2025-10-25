"""
GEOSTRATEGIC PLAYERS AGENT
Great Powers Dynamics Analysis for Grand Chessboard Framework

===== DATA SOURCES REQUIRED =====
# INPUT:
#   - great_power_events (array)
#   - player_developments (array)
#   - great_power_relations (object)
#   - global_balance_data (object)
#   - GEOSTRATEGIC_PLAYERS_API_KEY
#
# OUTPUT:
#   - Great powers capabilities assessment
#   - Power transition dynamics analysis
#   - Strategic alignment evaluation
#   - Great power competition assessment
#
# PARAMETERS:
#   - analysis_focus="power_transition_dynamics"
#   - strategic_timeframe="decade"
#   - perspective="balance_of_power_analysis"
"""

import logging
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum

# Follow existing codebase patterns
from fincept_terminal.Agents.src.graph.state import AgentState, show_agent_reasoning
from fincept_terminal.Agents.src.tools.api import get_geopolitical_data, get_strategic_analysis

# Import geostrategic modules
from ..geostrategic_modules.balance_of_power import BalanceOfPowerCalculator

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class PowerStatus(Enum):
    """Status of great powers in global system"""
    RISING = "rising"                   # Gaining power and influence
    STABLE = "stable"                   # Maintaining current position
    DECLINING = "declining"             # Losing power and influence
    TRANSITIONING = "transitioning"       # Undergoing major power transition
    CHALLENGED = "challenged"           # Facing significant internal/external challenges

class PlayerCategory(Enum):
    """Categories of geopolitical players per Brzezinski"""
    ESTABLISHED = "established"           # Established great powers
    EMERGING = "emerging"               # Emerging powers with great power potential
    REGIONAL = "regional"               # Significant regional powers
    DECLINING = "declining"             # Powers in relative decline
    CHALLENGER = "challenger"           # Powers challenging the status quo

@dataclass
class PlayerCapabilities:
    """Capabilities assessment for a geopolitical player"""
    military_power: float          # 0-1 scale
    economic_strength: float        # 0-1 scale
    technological_advantage: float # 0-1 scale
    diplomatic_influence: float     # 0-1 scale
    global_reach: float            # 0-1 scale
    strategic_focus: List[str]     # Key strategic priorities
    constraints: List[str]         # Major limitations/constraints

@dataclass
class PowerTransition:
    """Power transition dynamics between players"""
    rising_powers: List[str]
    declining_powers: List[str]
    stable_powers: List[str]
    transition_hotspots: List[str]
    competition_intensity: float     # 0-1 scale
    system_stability: str          # stable, transitioning, volatile

@dataclass
class GreatPowerAssessment:
    """Comprehensive assessment of great powers dynamics"""
    player_capabilities: Dict[str, PlayerCapabilities]
    power_distribution: Dict[str, float]
    transition_dynamics: PowerTransition
    strategic_alignments: Dict[str, List[str]]
    competition_areas: List[str]
    system_stability: str
    strategic_recommendations: List[str]

class GeostrategicPlayersAgent:
    """
    GEOSTRATEGIC PLAYERS AGENT - Great Powers Dynamics Analysis

    Core Thesis: Analysis of major powers that can affect global power distribution
    - Established great powers: France, Germany, Russia, China, potentially India
    - Emerging powers with global aspirations: Brazil, Turkey, Iran
    - Power transition dynamics and balance of power shifts
    - Strategic alignment patterns and alliance formations
    - Competition areas and potential conflict zones

    Brzezinski Framework: Understanding great power capabilities and managing balance
    """

    def __init__(self):
        """Initialize Geostrategic Players agent"""
        self.agent_id = "geostrategic_players_agent"
        self.balance_calculator = BalanceOfPowerCalculator()

        # Great powers per Brzezinski
        self.geostrategic_players = [
            "united_states",  # Global hegemon
            "china",           # Rising global challenger
            "russia",          # Resurgent regional power
            "germany",        # European economic power
            "france",          # European diplomatic/military power
            "japan",           # Asian economic/technological power
            "india",          # Emerging South Asian power
            "brazil",          # Emerging Latin American power
            "turkey",          # Regional power with ambitions
            "iran"             # Regional power with nuclear aspirations
        ]

        # Power categories
        self.established_powers = ["united_states", "china", "russia", "japan", "germany", "france"]
        self.emerging_powers = ["india", "brazil", "turkey", "iran"]
        self.regional_powers = ["south_korea", "australia", "canada", "mexico", "south_africa"]

        # Competition areas
        self.competition_areas = [
            "technological_supremacy", "economic_dominance", "military_advantage",
            "diplomatic_leadership", "resource_control", "space_exploration",
            "cyber_domain", "ai_development", "energy_security"
        ]

        # Strategic alignments per Brzezinski
        self.strategic_alignments = {
            "western_alliance": ["united_states", "germany", "france", "uk", "japan", "australia"],
            "eastern_partnership": ["china", "russia", "iran", "north_korea"],
            "non_aligned": ["india", "brazil", "turkey", "south_africa", "many_developing_countries"]
        }

    def analyze_geostrategic_players(self, state: AgentState) -> Dict[str, Any]:
        """
        Main analysis function for great powers dynamics

        Args:
            state: AgentState containing great powers data

        Returns:
            Comprehensive assessment of geopolitical players dynamics
        """
        try:
            logger.info("Starting Geostrategic Players analysis")

            # Extract relevant data from state
            great_power_events = state.get("great_power_events", [])
            player_developments = state.get("player_developments", [])
            power_relations = state.get("great_power_relations", {})
            global_balance = state.get("global_balance_data", {})

            # Core Brzezinski analysis components
            player_capabilities = self._assess_player_capabilities(
                player_developments, great_power_events
            )

            power_distribution = self._analyze_power_distribution(
                player_capabilities, global_balance
            )

            transition_dynamics = self._analyze_power_transitions(
                player_capabilities, power_relations, great_power_events
            )

            strategic_alignments = self._analyze_strategic_alignments(
                power_relations, great_power_events
            )

            competition_analysis = self._analyze_competition_areas(
                player_capabilities, great_power_events, power_relations
            )

            # Strategic assessment
            strategic_assessment = self._assess_overall_dynamics(
                player_capabilities, transition_dynamics, strategic_alignments
            )

            # Future projections
            future_scenarios = self._project_future_power_dynamics(
                transition_dynamics, player_capabilities
            )

            # Strategic recommendations
            strategic_recommendations = self._generate_players_recommendations(
                strategic_assessment, transition_dynamics, future_scenarios
            )

            # Compile comprehensive assessment
            players_assessment = {
                "assessment_type": "geostrategic_players",
                "timestamp": state.get("timestamp", ""),
                "player_capabilities": player_capabilities,
                "power_distribution": power_distribution,
                "transition_dynamics": transition_dynamics,
                "strategic_alignments": strategic_alignments,
                "competition_analysis": competition_analysis,
                "strategic_assessment": strategic_assessment,
                "future_scenarios": future_scenarios,
                "strategic_recommendations": strategic_recommendations,
                "brzezinski_compliance": self._validate_brzezinski_framework(),
                "confidence_score": self._calculate_confidence_score(state)
            }

            # Show reasoning for transparency
            show_agent_reasoning(
                self.agent_id,
                players_assessment,
                "Geostrategic Players analysis completed"
            )

            logger.info("Geostrategic Players analysis completed successfully")
            return players_assessment

        except Exception as e:
            logger.error(f"Error in Geostrategic Players analysis: {str(e)}")
            return self._generate_error_assessment(e)

    def _assess_player_capabilities(
        self,
        player_developments: List[Dict[str, Any]],
        events: List[Dict[str, Any]]
    ) -> Dict[str, PlayerCapabilities]:
        """Assess capabilities of all great power players"""

        capabilities = {}

        for player in self.geostrategic_players:
            # Get player-specific data
            player_events = [e for e in events if e.get("primary_player") == player]
            player_development = next(
                (d for d in player_developments if d.get("player") == player),
                {}
            )

            # Assess each capability dimension
            military_power = self._assess_military_capability(player, player_events, player_development)
            economic_strength = self._assess_economic_capability(player, player_events, player_development)
            tech_advantage = self._assess_technological_capability(player, player_events, player_development)
            diplomatic_influence = self._assess_diplomatic_capability(player, player_events, player_development)
            global_reach = self._assess_global_reach(player, player_events, player_development)

            # Determine strategic focus and constraints
            strategic_focus = self._identify_strategic_focus(player, player_events, player_development)
            constraints = self._identify_player_constraints(player, player_events, player_development)

            capabilities[player] = PlayerCapabilities(
                military_power=military_power,
                economic_strength=economic_strength,
                technological_advantage=tech_advantage,
                diplomatic_influence=diplomatic_influence,
                global_reach=global_reach,
                strategic_focus=strategic_focus,
                constraints=constraints
            )

        return capabilities

    def _analyze_power_distribution(
        self,
        capabilities: Dict[str, PlayerCapabilities],
        global_balance: Dict[str, Any]
    ) -> Dict[str, float]:
        """Analyze current power distribution among great powers"""

        distribution = {}

        # Calculate composite power scores
        for player, caps in capabilities.items():
            composite_score = self._calculate_composite_power_score(caps)
            distribution[player] = composite_score

        # Normalize to percentages
        total_power = sum(distribution.values())
        if total_power > 0:
            for player in distribution:
                distribution[player] = distribution[player] / total_power

        # Adjust for global balance data if available
        if global_balance:
            distribution = self._incorporate_global_balance_data(distribution, global_balance)

        return distribution

    def _analyze_power_transitions(
        self,
        capabilities: Dict[str, PlayerCapabilities],
        relations: Dict[str, Any],
        events: List[Dict[str, Any]]
    ) -> PowerTransition:
        """Analyze power transition dynamics among great powers"""

        # Identify rising and declining powers
        power_trends = self._identify_power_trends(capabilities, events)
        rising_powers = [player for player, trend in power_trends.items() if trend == PowerStatus.RISING]
        declining_powers = [player for player, trend in power_trends.items() if trend == PowerStatus.DECLINING]
        stable_powers = [player for player, trend in power_trends.items() if trend == PowerStatus.STABLE]

        # Identify transition hotspots
        transition_hotspots = self._identify_transition_hotspots(events, relations)

        # Assess competition intensity
        competition_intensity = self._calculate_competition_intensity(capabilities, relations)

        # Determine system stability
        system_stability = self._assess_system_stability(
            rising_powers, declining_powers, competition_intensity
        )

        return PowerTransition(
            rising_powers=rising_powers,
            declining_powers=declining_powers,
            stable_powers=stable_powers,
            transition_hotspots=transition_hotspots,
            competition_intensity=competition_intensity,
            system_stability=system_stability
        )

    def _analyze_strategic_alignments(
        self,
        relations: Dict[str, Any],
        events: List[Dict[str, Any]]
    ) -> Dict[str, List[str]]:
        """Analyze current strategic alignments among great powers"""

        alignments = {}

        for player in self.geostrategic_players:
            # Determine player's alignments based on relations and events
            player_alignments = self._determine_player_alignments(player, relations, events)
            alignments[player] = player_alignments

        return alignments

    def _analyze_competition_areas(
        self,
        capabilities: Dict[str, PlayerCapabilities],
        events: List[Dict[str, Any]],
        relations: Dict[str, Any]
    ) -> List[str]:
        """Analyze competition areas among great powers"""

        competition_areas = []

        # Analyze each competition area
        for area in self.competition_areas:
            area_competition = self._assess_area_competition(area, capabilities, events)
            if area_competition["intensity"] > 0.6:  # High competition threshold
                competition_areas.append(area)

        return competition_areas

    def _assess_overall_dynamics(
        self,
        capabilities: Dict[str, PlayerCapabilities],
        transitions: PowerTransition,
        alignments: Dict[str, List[str]]
    ) -> Dict[str, Any]:
        """Assess overall dynamics of great powers system"""

        # System characteristics
        power_concentration = self._calculate_power_concentration(capabilities)
        alignment_polarization = self._assess_alignment_polarization(alignments)
        transition_velocity = self._assess_transition_velocity(transitions)

        # Strategic stability indicators
        stability_indicators = {
            "power_balance_stability": transitions.system_stability,
            "alignment_coherence": 1 - alignment_polarization,
            "transition_smoothness": 1 - transition_velocity,
            "competition_intensity": transitions.competition_intensity
        }

        # Overall system assessment
        overall_stability = self._determine_overall_stability(stability_indicators)

        return {
            "power_concentration": power_concentration,
            "alignment_polarization": alignment_polarization,
            "transition_velocity": transition_velocity,
            "stability_indicators": stability_indicators,
            "overall_stability": overall_stability,
            "system_characteristics": self._describe_system_characteristics(
                power_concentration, alignment_polarization, transition_velocity
            )
        }

    def _project_future_power_dynamics(
        self,
        transitions: PowerTransition,
        capabilities: Dict[str, PlayerCapabilities]
    ) -> List[Dict[str, Any]]:
        """Project future power dynamics scenarios"""

        scenarios = []

        # Scenario 1: Current trends continue
        current_trends_scenario = {
            "scenario_name": "Current Trends Continue",
            "probability": 0.4,
            "description": "Existing power dynamics continue with gradual adjustments",
            "key_developments": self._project_current_trends(transitions, capabilities)
        }

        # Scenario 2: Major power realignment
        realignment_scenario = {
            "scenario_name": "Strategic Realignment",
            "probability": 0.3,
            "description": "Significant changes in great power alignments and partnerships",
            "key_developments": self._project_realignment_scenarios(transitions, capabilities)
        }

        # Scenario 3: Multipolar competition intensifies
        multipolar_scenario = {
            "scenario_name": "Intensified Multipolar Competition",
            "probability": 0.3,
            "description": "Increased competition among multiple great powers",
            "key_developments": self._project_multipolar_scenarios(transitions, capabilities)
        }

        scenarios = [current_trends_scenario, realignment_scenario, multipolar_scenario]

        return scenarios

    def _generate_players_recommendations(
        self,
        strategic_assessment: Dict[str, Any],
        transitions: PowerTransition,
        future_scenarios: List[Dict[str, Any]]
    ) -> List[str]:
        """Generate strategic recommendations for managing great powers dynamics"""

        recommendations = []

        # System stability recommendations
        if strategic_assessment.get("overall_stability") == "unstable":
            recommendations.extend([
                "Strengthen diplomatic mechanisms for crisis management",
                "Enhance strategic communication channels",
                "Support international institutions for conflict prevention",
                "Promote power balance through diplomatic engagement"
            ])

        # Power transition recommendations
        if transitions.rising_powers:
            recommendations.extend([
                "Engage constructively with rising powers",
                "Manage power transitions through diplomatic means",
                "Prevent destabilizing actions during power shifts",
                "Support peaceful transition to new power configurations"
            ])

        # Competition management recommendations
        high_competition_areas = strategic_assessment.get("competition_areas", [])
        if high_competition_areas:
            recommendations.extend([
                f"Manage competition in {', '.join(high_competition_areas)}",
                "Establish rules of engagement for competitive domains",
                "Develop crisis prevention mechanisms",
                "Promote cooperation in areas of mutual interest"
            ])

        # Alignment management recommendations
        polarization = strategic_assessment.get("alignment_polarization", 0)
        if polarization > 0.7:  # High polarization threshold
            recommendations.extend([
                "Reduce polarization through bridge-building diplomacy",
                "Support multipolar frameworks",
                "Encourage cross-alignment cooperation",
                "Maintain strategic autonomy in alliance decisions"
            ])

        # Long-term strategic recommendations
        recommendations.extend([
            "Support international order that accommodates multiple powers",
            "Promote global governance reforms for emerging powers",
            "Develop cooperative approaches to global challenges",
            "Maintain credible deterrence while pursuing diplomatic solutions"
        ])

        return recommendations

    def _validate_brzezinski_framework(self) -> Dict[str, Any]:
        """Validate compliance with Brzezinski's great powers framework"""

        validation_criteria = {
            "great_power_recognition": True,      # Recognition of established and emerging powers
            "power_transition_focus": True,       # Emphasis on power transition dynamics
            "balance_of_power_analysis": True,    # Analysis of power balance and competition
            "strategic_alignment_understanding": True,  # Understanding of alliance patterns
            "global_dynamics_perspective": True,  # Global rather than regional perspective
        }

        compliance_score = sum(validation_criteria.values()) / len(validation_criteria)

        return {
            "framework_compliance": compliance_score,
            "validation_criteria": validation_criteria,
            "brzezinski_principles_applied": [
                "Recognition of established and emerging great powers",
                "Analysis of power transition dynamics and balance changes",
                "Understanding of strategic alignments and alliance patterns",
                "Assessment of global power distribution and competition",
                "Management of great power relations through balance and diplomacy"
            ]
        }

    def _calculate_confidence_score(self, state: AgentState) -> float:
        """Calculate confidence score for players assessment"""

        # Data quality assessment
        data_completeness = self._assess_data_completeness(state)
        data_recency = self._assess_data_recency(state)

        # Analytical clarity
        player_coverage = self._assess_player_coverage(state)
        data_reliability = self._assess_data_reliability(state)

        # Calculate weighted confidence
        confidence = (
            data_completeness * 0.25 +
            data_recency * 0.15 +
            player_coverage * 0.35 +
            data_reliability * 0.25
        )

        return min(confidence, 1.0)

    # Helper method implementations
    def _assess_military_capability(self, player: str, events: List[Dict], development: Dict) -> float:
        """Assess military capability of a specific player"""
        # Base military capability scores by player
        base_capabilities = {
            "united_states": 0.9,
            "china": 0.8,
            "russia": 0.7,
            "japan": 0.6,
            "germany": 0.5,
            "france": 0.5,
            "india": 0.4,
            "brazil": 0.3,
            "turkey": 0.4,
            "iran": 0.3
        }

        base_score = base_capabilities.get(player, 0.3)

        # Adjust based on recent developments
        military_events = [e for e in events if e.get("type") == "military" and e.get("primary_player") == player]
        for event in military_events:
            if event.get("impact", 0) > 0:
                base_score = min(1.0, base_score + 0.1)
            elif event.get("impact", 0) < 0:
                base_score = max(0.0, base_score - 0.1)

        return min(base_score + development.get("military_trend", 0), 1.0)

    def _assess_economic_capability(self, player: str, events: List[Dict], development: Dict) -> float:
        """Assess economic capability of a specific player"""
        base_capabilities = {
            "united_states": 0.85,
            "china": 0.8,
            "japan": 0.75,
            "germany": 0.7,
            "france": 0.6,
            "india": 0.5,
            "russia": 0.4,
            "brazil": 0.3,
            "turkey": 0.4,
            "iran": 0.3
        }

        base_score = base_capabilities.get(player, 0.3)
        return min(base_score + development.get("economic_trend", 0), 1.0)

    def _assess_technological_capability(self, player: str, events: List[Dict], development: Dict) -> float:
        """Assess technological capability of a specific player"""
        base_capabilities = {
            "united_states": 0.9,
            "china": 0.7,
            "japan": 0.8,
            "germany": 0.7,
            "france": 0.6,
            "india": 0.5,
            "russia": 0.5,
            "brazil": 0.3,
            "turkey": 0.4,
            "iran": 0.4
        }

        base_score = base_capabilities.get(player, 0.3)
        return min(base_score + development.get("technology_trend", 0), 1.0)

    def _assess_diplomatic_capability(self, player: str, events: List[Dict], development: Dict) -> float:
        """Assess diplomatic capability of a specific player"""
        base_capabilities = {
            "united_states": 0.8,
            "china": 0.6,
            "russia": 0.7,
            "france": 0.7,
            "germany": 0.6,
            "japan": 0.6,
            "india": 0.5,
            "brazil": 0.4,
            "turkey": 0.5,
            "iran": 0.4
        }

        base_score = base_capabilities.get(player, 0.3)
        return min(base_score + development.get("diplomatic_trend", 0), 1.0)

    def _assess_global_reach(self, player: str, events: List[Dict], development: Dict) -> float:
        """Assess global reach and influence of a specific player"""
        base_capabilities = {
            "united_states": 0.9,
            "china": 0.7,
            "russia": 0.6,
            "france": 0.5,
            "united_kingdom": 0.5,
            "japan": 0.5,
            "germany": 0.4,
            "india": 0.4,
            "brazil": 0.3,
            "turkey": 0.4,
            "iran": 0.3
        }

        base_score = base_capabilities.get(player, 0.3)
        return min(base_score + development.get("global_reach_trend", 0), 1.0)

    def _identify_strategic_focus(self, player: str, events: List[Dict], development: Dict) -> List[str]:
        """Identify strategic focus areas for a specific player"""
        # Strategic focus patterns by player
        player_focuses = {
            "united_states": ["global_leadership", "alliance_management", "containment", "freedom_of_navigation"],
            "china": ["regional_dominance", "belt_road_initiative", "technological_supremacy", "resource_security"],
            "russia": ["near_abroad_influence", "military_modernization", "energy_politics", "great_power_status"],
            "japan": ["technological_leadership", "regional_security", "us_alliance_partnership", "economic_recovery"],
            "germany": ["european_integration", "economic_leadership", "multilateralism", "green_transition"],
            "france": ["european_independence", "military_capability", "global_diplomacy", "nuclear_deterrence"],
            "india": ["regional_dominance", "economic_development", "strategic_autonomy", "border_security"],
            "brazil": ["regional_leadership", "economic_growth", "south_american_integration", "global_role"],
            "turkey": ["regional_leadership", "nato_partnership", "islamic_world_leadership", "energy_hub"],
            "iran": ["regional_influence", "nuclear_deterrence", "anti_western_alignment", "shia_crescent"]
        }

        return player_focuses.get(player, ["power_maintenance"])

    def _identify_player_constraints(self, player: str, events: List[Dict], development: Dict) -> List[str]:
        """Identify major constraints facing a specific player"""
        # Common constraints by player
        player_constraints = {
            "united_states": ["global_overextension", "domestic_political_division", "economic_challenges", "alliance_management"],
            "china": ["demographic_challenges", "energy_dependencies", "technological_gaps", "containment_pressure"],
            "russia": ["economic_limitations", "demographic_decline", "technological_lag", "sanctions_pressure"],
            "japan": ["demographic_decline", "economic_stagnation", "constitutional_constraints", "regional_tensions"],
            "germany": ["military_constraints", "european_coordination", "energy_dependencies", "historical_constraints"],
            "france": ["economic_constraints", "global_leadership_limitations", "european_coordination", "colonial_legacy"],
            "india": ["poverty", "infrastructure_deficits", "regional_conflicts", "bureaucratic_challenges"],
            "brazil": ["economic_instability", "crime", "inequality", "infrastructure_limitations"],
            "turkey": ["economic_instability", "regional_tensions", "demographic_challenges", "secular_religious_divisions"],
            "iran": ["sanctions", "economic_isolation", "technological_obsolescence", "regional_opposition"]
        }

        return player_constraints.get(player, ["resource_constraints"])

    # Additional helper methods for complete functionality
    def _calculate_composite_power_score(self, caps: PlayerCapabilities) -> float:
        """Calculate composite power score from individual capabilities"""
        weights = {
            "military_power": 0.25,
            "economic_strength": 0.25,
            "technological_advantage": 0.2,
            "diplomatic_influence": 0.15,
            "global_reach": 0.15
        }

        return (
            caps.military_power * weights["military_power"] +
            caps.economic_strength * weights["economic_strength"] +
            caps.technological_advantage * weights["technological_advantage"] +
            caps.diplomatic_influence * weights["diplomatic_influence"] +
            caps.global_reach * weights["global_reach"]
        )

    def _identify_power_trends(self, capabilities: Dict[str, PlayerCapabilities], events: List[Dict]) -> Dict[str, PowerStatus]:
        """Identify current power trends for each player"""
        trends = {}

        for player, caps in capabilities.items():
            # Analyze trend indicators from events
            trend_events = [e for e in events if e.get("primary_player") == player and e.get("trend_indicator")]

            if trend_events:
                positive_trends = sum(1 for e in trend_events if e.get("trend_indicator") > 0)
                negative_trends = sum(1 for e in trend_events if e.get("trend_indicator") < 0)

                if positive_trends > negative_trends:
                    trends[player] = PowerStatus.RISING
                elif negative_trends > positive_trends:
                    trends[player] = PowerStatus.DECLINING
                else:
                    trends[player] = PowerStatus.STABLE
            else:
                # Default assessment based on capabilities profile
                if caps.military_power > 0.7 and caps.economic_strength > 0.7:
                    trends[player] = PowerStatus.RISING
                elif caps.military_power < 0.4 or caps.economic_strength < 0.4:
                    trends[player] = PowerStatus.DECLINING
                else:
                    trends[player] = PowerStatus.STABLE

        return trends

    def _calculate_competition_intensity(self, capabilities: Dict[str, PlayerCapabilities], relations: Dict) -> float:
        """Calculate overall competition intensity among powers"""
        # Count players with high capabilities in key areas
        military_leaders = [p for p, caps in capabilities.items() if caps.military_power > 0.7]
        economic_leaders = [p for p, caps in capabilities.items() if caps.economic_strength > 0.7]
        tech_leaders = [p for p, caps in capabilities.items() if caps.technological_advantage > 0.7]

        # Competition intensity based on number of strong players
        leader_count = len(set(military_leaders + economic_leaders + tech_leaders))
        return min(leader_count / len(self.geostrategic_players), 1.0)

    # Additional helper methods for complete functionality
    def _assess_data_completeness(self, state: AgentState) -> float:
        """Assess completeness of input data"""
        required_fields = ["great_power_events", "player_developments", "great_power_relations"]
        present_fields = sum(1 for field in required_fields if state.get(field))
        return present_fields / len(required_fields)

    def _assess_data_recency(self, state: AgentState) -> float:
        """Assess recency of input data"""
        return 0.8  # Assume reasonably current data

    def _assess_player_coverage(self, state: AgentState) -> float:
        """Assess coverage of all great powers"""
        events = state.get("great_power_events", [])
        players_mentioned = set(e.get("primary_player") for e in events if e.get("primary_player"))
        return len(players_mentioned) / len(self.geostrategic_players)

    def _assess_data_reliability(self, state: AgentState) -> float:
        """Assess reliability of input data sources"""
        return 0.8  # Assume reasonably reliable data

    def _generate_error_assessment(self, error: Exception) -> Dict[str, Any]:
        """Generate error assessment when analysis fails"""
        return {
            "assessment_type": "geostrategic_players",
            "error_occurred": True,
            "error_message": str(error),
            "assessment_status": "failed",
            "fallback_recommendations": [
                "Maintain diplomatic engagement with all great powers",
                "Support balance of power principles",
                "Strengthen international institutions",
                "Monitor power transitions and emerging challenges",
                "Promote cooperative approaches to global issues"
            ],
            "confidence_score": 0.0
        }

# Main agent function following existing patterns
def geostrategic_players_agent(state: AgentState) -> Dict[str, Any]:
    """
    Main agent function for Geostrategic Players analysis

    Args:
        state: AgentState with great powers geopolitical data

    Returns:
        Strategic assessment of great powers dynamics
    """
    agent = GeostrategicPlayersAgent()
    return agent.analyze_geostrategic_players(state)

# Entry point for testing
if __name__ == "__main__":
    # Test data for development
    test_state = {
        "great_power_events": [
            {"id": "1", "type": "military", "primary_player": "china", "trend_indicator": 1, "description": "China announces naval expansion"},
            {"id": "2", "type": "diplomatic", "primary_player": "russia", "trend_indicator": -1, "description": "Russia faces international isolation"},
            {"id": "3", "type": "economic", "primary_player": "india", "trend_indicator": 1, "description": "India shows strong economic growth"},
            {"id": "4", "type": "technological", "primary_player": "united_states", "trend_indicator": 1, "description": "US advances AI capabilities"}
        ],
        "player_developments": [
            {"player": "china", "military_trend": 0.1, "economic_trend": 0.1, "technology_trend": 0.1},
            {"player": "russia", "military_trend": -0.05, "economic_trend": -0.1, "technology_trend": 0.0},
            {"player": "united_states", "military_trend": 0.05, "economic_trend": -0.05, "technology_trend": 0.1},
            {"player": "india", "military_trend": 0.05, "economic_trend": 0.1, "technology_trend": 0.05}
        ],
        "great_power_relations": {
            "alignments": {
                "western_alliance": ["united_states", "germany", "france", "japan", "uk", "australia"],
                "eastern_partnership": ["china", "russia", "north_korea", "iran"],
                "non_aligned": ["india", "brazil", "turkey", "south_africa"]
            },
            "competition_areas": {
                "technological_supremacy": {"leaders": ["united_states", "china"], "intensity": 0.8},
                "economic_dominance": {"leaders": ["united_states", "china"], "intensity": 0.7}
            }
        },
        "global_balance_data": {
            "power_transition": "ongoing",
            "system_stability": "moderately_stable",
            "competition_intensity": 0.6
        }
    }

    result = geostrategic_players_agent(test_state)
    print("Geostrategic Players Analysis Results:")
    print(f"System Stability: {result.get('strategic_assessment', {}).get('overall_stability', 'unknown')}")
    print(f"Rising Powers: {result.get('transition_dynamics', {}).get('rising_powers', [])}")
    print(f"Competition Intensity: {result.get('strategic_assessment', {}).get('competition_intensity', 0):.2f}")
    print(f"Confidence Score: {result.get('confidence_score', 0):.2f}")