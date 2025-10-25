"""
DEMOCRATIC BRIDGEHEAD AGENT
European Strategic Analysis for Grand Chessboard Framework

===== DATA SOURCES REQUIRED =====
# INPUT:
#   - european_events (array)
#   - nato_developments (array)
#   - eu_integration_data (object)
#   - european_economic_indicators (object)
#   - DEMOCRATIC_BRIDGEHEAD_API_KEY
#
# OUTPUT:
#   - European integration assessment
#   - NATO strategic effectiveness analysis
#   - Transatlantic relationship evaluation
#   - Western stability recommendations
#
# PARAMETERS:
#   - analysis_focus="european_integration"
#   - strategic_timeframe="decade"
#   - perspective="transatlantic"
"""

import logging
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum

# Follow existing codebase patterns
from fincept_terminal.Agents.src.graph.state import AgentState, show_agent_reasoning
from fincept_terminal.Agents.src.tools.api import get_geopolitical_data, get_strategic_analysis

# Import geostrategic modules
from ..geostrategic_modules.alliance_management import AllianceManager
from ..geostrategic_modules.balance_of_power import BalanceOfPowerCalculator

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class IntegrationStatus(Enum):
    """European Union integration status"""
    DEEPENING = "deepening"         # Moving toward deeper integration
    STAGNATING = "stagnating"        # Integration progress stalled
    FRAGMENTING = "fragmenting"       # Risk of disintegration
    EXPANDING = "expanding"          # New members joining
    REFORMING = "reforming"          # Major institutional reforms

class NATOStrategicValue(Enum):
    """NATO's strategic value assessment"""
    ESSENTIAL = "essential"           # Critical to European security
    STRONG = "strong"               # Important but with challenges
    EVOLVING = "evolving"           # Adapting to new challenges
    QUESTIONED = "questioned"       # Future role debated
    TRANSFORMING = "transforming"     # Major transformation underway

@dataclass
class EuropeanAssessment:
    """Comprehensive European strategic assessment"""
    integration_status: IntegrationStatus
    nato_effectiveness: NATOStrategicValue
    transatlantic_strength: float       # 0-1 scale
    strategic_challenges: List[str]
    american_opportunities: List[str]
    regional_stability: str            # stable, tense, unstable
    strategic_recommendations: List[str]

@dataclass
class BridgeheadMetrics:
    """Metrics for democratic bridgehead effectiveness"""
    political_cohesion: float          # 0-1 scale
    economic_integration: float        # 0-1 scale
    defense_capability: float          # 0-1 scale
    institutional_effectiveness: float  # 0-1 scale
    transatlantic_alignment: float     # 0-1 scale

class DemocraticBridgeheadAgent:
    """
    DEMOCRATIC BRIDGEHEAD AGENT - European Strategic Analysis

    Core Thesis: Europe as America's essential ally for global leadership
    - NATO expansion as strategic necessity for European stability
    - European integration as American interest for power amplification
    - France-Germany leadership as engine of European power
    - Western Balkans integration as critical for stability

    Brzezinski Framework: Europe as anchor for American global power
    """

    def __init__(self):
        """Initialize the democratic bridgehead agent"""
        self.agent_id = "democratic_bridgehead_agent"
        self.alliance_manager = AllianceManager()
        self.balance_calculator = BalanceOfPowerCalculator()

        # Critical European regions per Brzezinski
        self.critical_european_regions = [
            "western_europe", "central_europe", "eastern_europe",
            "british_isles", "scandinavian", "southern_europe"
        ]

        # Key European powers
        self.key_european_powers = [
            "germany", "france", "united_kingdom", "italy", "spain",
            "poland", "netherlands", "sweden", "norway"
        ]

        # NATO integration stages
        self.nato_integration_levels = {
            "full_members": ["united_states", "germany", "france", "united_kingdom", "canada"],
            "recent_members": ["croatia", "montenegro", "north_macedonia", "albania"],
            "aspirational_members": ["ukraine", "georgia", "bosnia", "serbia"],
            "partnership_countries": ["finland", "sweden", "austria", "ireland"]
        }

    def analyze_democratic_bridgehead(self, state: AgentState) -> Dict[str, Any]:
        """
        Main analysis function for European strategic assessment

        Args:
            state: AgentState containing European geopolitical data

        Returns:
            Comprehensive assessment of European democratic bridgehead
        """
        try:
            logger.info("Starting Democratic Bridgehead analysis")

            # Extract relevant data from state
            european_events = state.get("european_events", [])
            nato_developments = state.get("nato_developments", [])
            eu_integration_data = state.get("eu_integration_data", {})
            economic_indicators = state.get("european_economic_indicators", {})

            # Core Brzezinski analysis components
            integration_assessment = self._assess_european_integration(eu_integration_data, european_events)
            nato_assessment = self._evaluate_nato_effectiveness(nato_developments, european_events)
            transatlantic_analysis = self._analyze_transatlantic_relationship(eu_integration_data, nato_developments)
            regional_dynamics = self._evaluate_regional_dynamics(european_events)

            # Bridgehead effectiveness assessment
            bridgehead_metrics = self._calculate_bridgehead_metrics(
                integration_assessment, nato_assessment, transatlantic_analysis, regional_dynamics
            )

            # Strategic opportunities and challenges
            american_opportunities = self._identify_american_opportunities(
                integration_assessment, nato_assessment, bridgehead_metrics
            )

            strategic_challenges = self._identify_strategic_challenges(
                integration_assessment, nato_assessment, regional_dynamics
            )

            # Strategic recommendations
            strategic_recommendations = self._generate_bridgehead_recommendations(
                bridgehead_metrics, american_opportunities, strategic_challenges
            )

            # Compile comprehensive assessment
            bridgehead_assessment = {
                "assessment_type": "democratic_bridgehead",
                "timestamp": state.get("timestamp", ""),
                "integration_assessment": integration_assessment,
                "nato_assessment": nato_assessment,
                "transatlantic_analysis": transatlantic_analysis,
                "bridgehead_metrics": bridgehead_metrics,
                "american_opportunities": american_opportunities,
                "strategic_challenges": strategic_challenges,
                "strategic_recommendations": strategic_recommendations,
                "regional_dynamics": regional_dynamics,
                "brzezinski_compliance": self._validate_brzezinski_framework(),
                "confidence_score": self._calculate_confidence_score(state)
            }

            # Show reasoning for transparency
            show_agent_reasoning(
                self.agent_id,
                bridgehead_assessment,
                "Democratic Bridgehead strategic analysis completed"
            )

            logger.info("Democratic Bridgehead analysis completed successfully")
            return bridgehead_assessment

        except Exception as e:
            logger.error(f"Error in Democratic Bridgehead analysis: {str(e)}")
            return self._generate_error_assessment(e)

    def _assess_european_integration(
        self,
        eu_integration_data: Dict[str, Any],
        european_events: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Assess current state and trajectory of European integration"""

        # Key integration dimensions
        political_integration = self._assess_political_integration(eu_integration_data)
        economic_integration = self._assess_economic_integration(eu_integration_data)
        defense_integration = self._assess_defense_integration(eu_integration_data)
        institutional_effectiveness = self._assess_institutional_effectiveness(eu_integration_data)

        # Integration trajectory
        integration_events = [e for e in european_events if e.get("integration_focus")]
        trajectory = self._determine_integration_trajectory(integration_events)

        # Expansion outlook
        expansion_opportunities = self._identify_expansion_opportunities(european_events)
        expansion_challenges = self._identify_expansion_challenges(european_events)

        # Franco-German engine assessment
        franco_german_leadership = self._assess_franco_german_engine(eu_integration_data)

        return {
            "overall_status": self._determine_integration_status(
                political_integration, economic_integration, defense_integration
            ),
            "political_integration": political_integration,
            "economic_integration": economic_integration,
            "defense_integration": defense_integration,
            "institutional_effectiveness": institutional_effectiveness,
            "integration_trajectory": trajectory,
            "expansion_opportunities": expansion_opportunities,
            "expansion_challenges": expansion_challenges,
            "franco_german_leadership": franco_german_leadership,
            "key_integration_projects": self._identify_key_integration_projects(eu_integration_data),
            "brexit_impact": self._assess_brexit_impact(european_events)
        }

    def _evaluate_nato_effectiveness(
        self,
        nato_developments: List[Dict[str, Any]],
        european_events: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Assess NATO's strategic effectiveness and relevance"""

        # Current NATO capabilities
        military_capability = self._assess_nato_military_capability(nato_developments)
        political_cohesion = self._assess_nato_political_cohesion(nato_developments)
        strategic_relevance = self._assess_nato_strategic_relevance(nato_developments, european_events)

        # Expansion analysis
        expansion_progress = self._analyze_nato_expansion(nato_developments)
        eastern_flank_security = self._assess_eastern_flank_security(nato_developments, european_events)

        # Deterrence effectiveness
        deterrence_capability = self._assess_deterrence_effectiveness(nato_developments)
        rapid_response_capability = self._assess_rapid_response_capability(nato_developments)

        # Burden sharing
        burden_sharing = self._analyze_nato_burden_sharing(nato_developments)

        # Modernization priorities
        modernization_needs = self._identify_nato_modernization_priorities(nato_developments)

        return {
            "strategic_value": self._determine_nato_strategic_value(
                military_capability, political_cohesion, strategic_relevance
            ),
            "military_capability": military_capability,
            "political_cohesion": political_cohesion,
            "strategic_relevance": strategic_relevance,
            "expansion_progress": expansion_progress,
            "eastern_flank_security": eastern_flank_security,
            "deterrence_capability": deterrence_capability,
            "rapid_response_capability": rapid_response_capability,
            "burden_sharing": burden_sharing,
            "modernization_priorities": modernization_needs,
            "key_challenges": self._identify_nato_challenges(nato_developments, european_events),
            "future_direction": self._project_nato_evolution(nato_developments)
        }

    def _analyze_transatlantic_relationship(
        self,
        eu_integration_data: Dict[str, Any],
        nato_developments: List[Dict[str, Any]]
    ) -> Dict[str, Any]:
        """Analyze state and trajectory of transatlantic relationship"""

        # Political alignment
        political_alignment = self._assess_political_alignment(eu_integration_data, nato_developments)
        strategic_convergence = self._assess_strategic_convergence(eu_integration_data, nato_developments)

        # Security cooperation
        security_cooperation = self._assess_security_cooperation(nato_developments)
        intelligence_sharing = self._assess_intelligence_sharing(nato_developments)

        # Economic relationship
        economic_partnership = self._assess_transatlantic_economic_partnership(eu_integration_data)
        technological_cooperation = self._assess_technological_cooperation(eu_integration_data)

        # Institutional mechanisms
        institutional_cooperation = self._assess_institutional_cooperation(eu_integration_data, nato_developments)

        # Areas of tension
        policy_divergences = self._identify_transatlantic_divergences(eu_integration_data, nato_developments)
        competitive_tensions = self._identify_competitive_tensions(eu_integration_data)

        return {
            "overall_strength": self._calculate_transatlantic_strength(
                political_alignment, strategic_convergence, security_cooperation
            ),
            "political_alignment": political_alignment,
            "strategic_convergence": strategic_convergence,
            "security_cooperation": security_cooperation,
            "intelligence_sharing": intelligence_sharing,
            "economic_partnership": economic_partnership,
            "technological_cooperation": technological_cooperation,
            "institutional_cooperation": institutional_cooperation,
            "policy_divergences": policy_divergences,
            "competitive_tensions": competitive_tensions,
            "cooperation_opportunities": self._identify_cooperation_opportunities(),
            "relationship_trajectory": self._project_relationship_trajectory()
        }

    def _evaluate_regional_dynamics(self, european_events: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Evaluate stability and dynamics across European regions"""

        regional_assessments = {}

        for region in self.critical_european_regions:
            region_events = [e for e in european_events if e.get("region") == region]

            regional_assessments[region] = {
                "stability_level": self._assess_regional_stability(region_events),
                "key_dynamics": self._identify_regional_dynamics(region_events),
                "strategic_importance": self._assess_region_strategic_importance(region),
                "american_influence": self._assess_american_regional_influence(region_events),
                "challenge_areas": self._identify_regional_challenges(region_events),
                "opportunity_areas": self._identify_regional_opportunities(region_events)
            }

        # Cross-regional dynamics
        regional_interactions = self._analyze_regional_interactions(european_events)
        stability_interdependencies = self._identify_stability_interdependencies(european_events)

        return {
            "regional_assessments": regional_assessments,
            "regional_interactions": regional_interactions,
            "stability_interdependencies": stability_interdependencies,
            "critical_flashpoints": self._identify_european_flashpoints(european_events),
            "stability_outlook": self._project_overall_stability(european_events),
            "transatlantic_implications": self._assess_transatlantic_regional_implications(regional_assessments)
        }

    def _calculate_bridgehead_metrics(
        self,
        integration_assessment: Dict[str, Any],
        nato_assessment: Dict[str, Any],
        transatlantic_analysis: Dict[str, Any],
        regional_dynamics: Dict[str, Any]
    ) -> BridgeheadMetrics:
        """Calculate comprehensive bridgehead effectiveness metrics"""

        # Political cohesion based on EU integration and NATO cohesion
        political_cohesion = (
            integration_assessment.get("political_integration", 0.5) * 0.6 +
            nato_assessment.get("political_cohesion", 0.5) * 0.4
        )

        # Economic integration from EU assessment
        economic_integration = integration_assessment.get("economic_integration", 0.5)

        # Defense capability from NATO assessment
        defense_capability = (
            nato_assessment.get("military_capability", 0.5) * 0.7 +
            integration_assessment.get("defense_integration", 0.5) * 0.3
        )

        # Institutional effectiveness
        institutional_effectiveness = (
            integration_assessment.get("institutional_effectiveness", 0.5) * 0.5 +
            nato_assessment.get("strategic_relevance", 0.5) * 0.3 +
            transatlantic_analysis.get("institutional_cooperation", 0.5) * 0.2
        )

        # Transatlantic alignment
        transatlantic_alignment = transatlantic_analysis.get("overall_strength", 0.5)

        return BridgeheadMetrics(
            political_cohesion=political_cohesion,
            economic_integration=economic_integration,
            defense_capability=defense_capability,
            institutional_effectiveness=institutional_effectiveness,
            transatlantic_alignment=transatlantic_alignment
        )

    def _identify_american_opportunities(
        self,
        integration_assessment: Dict[str, Any],
        nato_assessment: Dict[str, Any],
        bridgehead_metrics: BridgeheadMetrics
    ) -> List[str]:
        """Identify strategic opportunities for American engagement"""

        opportunities = []

        # Opportunities based on integration status
        if integration_assessment.get("overall_status") == IntegrationStatus.DEEPENING:
            opportunities.append(
                "Support deeper EU integration for stronger transatlantic partnership"
            )
            opportunities.append(
                "Engage with European strategic autonomy initiatives to maintain alignment"
            )

        # NATO expansion opportunities
        if nato_assessment.get("expansion_progress").get("momentum") == "positive":
            opportunities.append(
                "Lead NATO expansion to enhance European security architecture"
            )
            opportunities.append(
                "Support aspirant members' NATO accession processes"
            )

        # Defense cooperation opportunities
        if bridgehead_metrics.defense_capability < 0.7:
            opportunities.append(
                "Increase American defense industry cooperation with European partners"
            )
            opportunities.append(
                "Lead joint defense modernization initiatives"
            )

        # Technology cooperation opportunities
        if bridgehead_metrics.institutional_effectiveness > 0.6:
            opportunities.append(
                "Expand technological cooperation in critical domains like AI and quantum computing"
            )
            opportunities.append(
                "Lead transatlantic technology standards setting"
            )

        # Regional engagement opportunities
        if bridgehead_metrics.political_cohesion > 0.6:
            opportunities.append(
                "Deepen political cooperation on global governance reforms"
            )
            opportunities.append(
                "Coordinate approaches to global challenges like climate change and pandemics"
            )

        return opportunities

    def _identify_strategic_challenges(
        self,
        integration_assessment: Dict[str, Any],
        nato_assessment: Dict[str, Any],
        regional_dynamics: Dict[str, Any]
    ) -> List[str]:
        """Identify strategic challenges to democratic bridgehead"""

        challenges = []

        # Integration challenges
        if integration_assessment.get("overall_status") in [IntegrationStatus.STAGNATING, IntegrationStatus.FRAGMENTING]:
            challenges.append(
                "European integration fragmentation undermines strategic coherence"
            )

        if integration_assessment.get("franco_german_leadership").get("effectiveness") < 0.6:
            challenges.append(
                "Weak Franco-German leadership reduces European strategic initiative"
            )

        # NATO challenges
        if nato_assessment.get("political_cohesion") < 0.6:
            challenges.append(
                "NATO political divisions reduce alliance effectiveness"
            )

        if nato_assessment.get("burden_sharing").get("fairness_assessment") == "poor":
            challenges.append(
                "Inequitable burden sharing creates alliance tensions"
            )

        # Regional stability challenges
        unstable_regions = [
            region for region, assessment in regional_dynamics.get("regional_assessments", {}).items()
            if assessment.get("stability_level") == "unstable"
        ]

        if unstable_regions:
            challenges.append(
                f"Regional instability in {', '.join(unstable_regions)} threatens European security"
            )

        # Transatlantic challenges
        divergences = regional_dynamics.get("transatlantic_implications", {}).get("divergences", [])
        if divergences:
            challenges.extend(divergences)

        return challenges

    def _generate_bridgehead_recommendations(
        self,
        metrics: BridgeheadMetrics,
        opportunities: List[str],
        challenges: List[str]
    ) -> List[str]:
        """Generate strategic recommendations for strengthening democratic bridgehead"""

        recommendations = []

        # Core bridgehead strengthening
        if metrics.political_cohesion < 0.7:
            recommendations.append(
                "Support European political integration through enhanced transatlantic dialogue"
            )
            recommendations.append(
                "Encourage Franco-German leadership cooperation on strategic initiatives"
            )

        if metrics.defense_capability < 0.7:
            recommendations.append(
                "Increase American defense investment in European theater"
            )
            recommendations.append(
                "Lead NATO modernization and capability development"
            )
            recommendations.append(
                "Support European defense industry integration with American partners"
            )

        if metrics.economic_integration < 0.7:
            recommendations.append(
                "Promote transatlantic economic integration and trade cooperation"
            )
            recommendations.append(
                "Support EU single market deepening and expansion"
            )

        if metrics.transatlantic_alignment < 0.7:
            recommendations.append(
                "Address policy divergences through enhanced diplomatic engagement"
            )
            recommendations.append(
                "Coordinate approaches to global governance and international institutions"
            )

        # Leverage opportunities
        if opportunities:
            recommendations.append(
                "Strategic opportunity: " + opportunities[0] if opportunities else ""
            )

        # Address challenges
        if challenges:
            recommendations.append(
                "Address challenge: " + challenges[0] if challenges else ""
            )

        # Eastern flank security
        recommendations.append(
            "Strengthen NATO's eastern flank through permanent deployments and exercises"
        )
        recommendations.append(
            "Support security capabilities of Eastern European NATO members"
        )

        # Balkans integration
        recommendations.append(
            "Lead Western Balkans integration into Euro-Atlantic structures"
        )
        recommendations.append(
                "Support regional cooperation and conflict resolution in Balkans"
        )

        return recommendations

    def _validate_brzezinski_framework(self) -> Dict[str, Any]:
        """Validate compliance with Brzezinski's European framework"""

        validation_criteria = {
            "european_bridgehead_focus": True,  # Europe as essential American ally
            "nato_central_role": True,        # NATO as central security institution
            "integration_support": True,          # European integration as American interest
            "transatlantic_partnership": True,   # Partnership rather than dominance
            "regional_stability": True,          # Stability as strategic objective
            "franco_german_engine": True        # France-Germany as leadership core
        }

        compliance_score = sum(validation_criteria.values()) / len(validation_criteria)

        return {
            "framework_compliance": compliance_score,
            "validation_criteria": validation_criteria,
            "brzezinski_principles_applied": [
                "Europe as democratic bridgehead for American global power",
                "NATO as essential transatlantic security framework",
                "European integration as amplifier of American influence",
                "Franco-German partnership as European engine",
                "Eastern Europe as critical security frontier",
                "Western Balkans as integration priority"
            ]
        }

    def _calculate_confidence_score(self, state: AgentState) -> float:
        """Calculate confidence score for bridgehead assessment"""

        # Data quality assessment
        data_completeness = self._assess_data_completeness(state)
        data_recency = self._assess_data_recency(state)

        # Analytical clarity
        regional_coverage = self._assess_regional_coverage(state)
        event_relevance = self._assess_event_relevance(state)

        # Calculate weighted confidence
        confidence = (
            data_completeness * 0.25 +
            data_recency * 0.15 +
            regional_coverage * 0.35 +
            event_relevance * 0.25
        )

        return min(confidence, 1.0)

    def _generate_error_assessment(self, error: Exception) -> Dict[str, Any]:
        """Generate error assessment when analysis fails"""

        return {
            "assessment_type": "democratic_bridgehead",
            "error_occurred": True,
            "error_message": str(error),
            "assessment_status": "failed",
            "fallback_recommendations": [
                "Strengthen transatlantic diplomatic engagement",
                "Support European integration initiatives",
                "Maintain NATO alliance effectiveness",
                "Monitor regional stability developments"
            ],
            "confidence_score": 0.0
        }

    # Helper method implementations
    def _assess_political_integration(self, eu_integration_data: Dict[str, Any]) -> float:
        """Assess level of political integration in EU"""
        # Simplified implementation - would be more sophisticated in production
        policy_harmonization = eu_integration_data.get("policy_harmonization", 0.5)
        institutional_development = eu_integration_data.get("institutional_development", 0.5)
        citizen_support = eu_integration_data.get("citizen_european_support", 0.5)

        return (policy_harmonization + institutional_development + citizen_support) / 3

    def _assess_economic_integration(self, eu_integration_data: Dict[str, Any]) -> float:
        """Assess level of economic integration"""
        single_market_completion = eu_integration_data.get("single_market_completion", 0.5)
        monetary_union_strength = eu_integration_data.get("monetary_union_strength", 0.5)
        fiscal_coordination = eu_integration_data.get("fiscal_coordination", 0.5)

        return (single_market_completion + monetary_union_strength + fiscal_coordination) / 3

    def _assess_defense_integration(self, eu_integration_data: Dict[str, Any]) -> float:
        """Assess level of defense integration"""
        defense_cooperation = eu_integration_data.get("defense_cooperation", 0.5)
        capability_development = eu_integration_data.get("joint_capability_development", 0.5)
        operational_coordination = eu_integration_data.get("operational_coordination", 0.5)

        return (defense_cooperation + capability_development + operational_coordination) / 3

    def _assess_institutional_effectiveness(self, eu_integration_data: Dict[str, Any]) -> float:
        """Assess effectiveness of EU institutions"""
        institutional_efficiency = eu_integration_data.get("institutional_efficiency", 0.5)
        democratic_legitimacy = eu_integration_data.get("democratic_legitimacy", 0.5)
        decision_making_effectiveness = eu_integration_data.get("decision_making_effectiveness", 0.5)

        return (institutional_efficiency + democratic_legitimacy + decision_making_effectiveness) / 3

    # Additional helper methods for complete functionality
    def _determine_integration_status(
        self,
        political: float,
        economic: float,
        defense: float
    ) -> IntegrationStatus:
        """Determine overall integration status"""
        average_integration = (political + economic + defense) / 3

        if average_integration > 0.7:
            return IntegrationStatus.DEEPENING
        elif average_integration > 0.5:
            return IntegrationStatus.EXPANDING
        elif average_integration > 0.3:
            return IntegrationStatus.STAGNATING
        else:
            return IntegrationStatus.FRAGMENTING

    def _assess_data_completeness(self, state: AgentState) -> float:
        """Assess completeness of input data"""
        required_fields = ["european_events", "nato_developments", "eu_integration_data"]
        present_fields = sum(1 for field in required_fields if state.get(field))
        return present_fields / len(required_fields)

    def _assess_data_recency(self, state: AgentState) -> float:
        """Assess recency of input data"""
        # Simplified implementation
        return 0.8  # Assume reasonably current data

    def _assess_regional_coverage(self, state: AgentState) -> float:
        """Assess regional coverage of events"""
        events = state.get("european_events", [])
        regions_covered = set(event.get("region") for event in events if event.get("region"))
        total_regions = len(self.critical_european_regions)
        return len(regions_covered) / total_regions if total_regions > 0 else 0

    def _assess_event_relevance(self, state: AgentState) -> float:
        """Assess relevance of events to bridgehead analysis"""
        events = state.get("european_events", [])
        relevant_events = sum(1 for event in events
                            if event.get("integration_focus") or event.get("nato_relevance"))
        return relevant_events / len(events) if events else 0

# Main agent function following existing patterns
def democratic_bridgehead_agent(state: AgentState) -> Dict[str, Any]:
    """
    Main agent function for European democratic bridgehead analysis

    Args:
        state: AgentState with European geopolitical data

    Returns:
        Strategic assessment of European democratic bridgehead
    """
    agent = DemocraticBridgeheadAgent()
    return agent.analyze_democratic_bridgehead(state)

# Entry point for testing
if __name__ == "__main__":
    # Test data for development
    test_state = {
        "european_events": [
            {"id": "1", "type": "political", "region": "eastern_europe", "description": "Poland increases defense spending", "nato_relevance": True},
            {"id": "2", "type": "economic", "region": "western_europe", "description": "EU announces new digital market regulations", "integration_focus": True},
            {"id": "3", "type": "security", "region": "central_europe", "description": "Germany announces military modernization", "integration_focus": True}
        ],
        "nato_developments": [
            {"id": "1", "type": "expansion", "description": "Finland and Sweden accession process", "status": "in_progress"},
            {"id": "2", "type": "capability", "description": "Enhanced forward presence in Eastern Europe", "status": "implemented"}
        ],
        "eu_integration_data": {
            "policy_harmonization": 0.7,
            "institutional_development": 0.6,
            "citizen_european_support": 0.5,
            "single_market_completion": 0.8,
            "monetary_union_strength": 0.6,
            "fiscal_coordination": 0.4,
            "defense_cooperation": 0.5,
            "joint_capability_development": 0.4,
            "operational_coordination": 0.6
        },
        "european_economic_indicators": {
            "eu_growth": 1.4,
            "inflation": 3.2,
            "unemployment": 6.1,
            "trade_balance": "surplus"
        }
    }

    result = democratic_bridgehead_agent(test_state)
    print("Democratic Bridgehead Analysis Results:")
    print(f"Integration Status: {result.get('integration_assessment', {}).get('overall_status', 'unknown')}")
    print(f"NATO Strategic Value: {result.get('nato_assessment', {}).get('strategic_value', 'unknown')}")
    print(f"Transatlantic Strength: {result.get('transatlantic_analysis', {}).get('overall_strength', 0):.2f}")
    print(f"Confidence Score: {result.get('confidence_score', 0):.2f}")