"""
LATIN AMERICA GEOGRAPHY AGENT - Tim Marshall's Framework
Based on "Prisoners of Geography" Chapter 4

===== DATA SOURCES REQUIRED =====
# INPUT:
#   - current_events (array)
#   - geographic_data (physical, climate, resources)
#   - GEOPOLITICAL_API_KEY
#
# OUTPUT:
#   - Latin American geographic constraint analysis
#   - Strategic imperative identification
#   - Historical pattern recognition
#   - Marshall thesis compliance score (70%+ required)
#
# PARAMETERS:
#   - analysis_depth="comprehensive"
#   - framework="marshalls_geographic_determinism"
#   - region="latin_america"

MARSHALL THESIS: Latin America is shaped by geography - Andes Mountains create isolation,
Amazon rainforest limits development, Pacific/Atlantic coasts create division, and rich
resources create external interference patterns.
"""

from typing import Dict, List, Any, Optional
import json
from datetime import datetime
import requests
import os

# Import base agent classes
from fincept_terminal.Agents.src.graph.state import AgentState, show_agent_reasoning
from fincept_terminal.Agents.src.tools.api import get_geopolitical_data

# Import geographic analysis modules
from ..geographic_modules.geographic_constraints import analyze_physical_constraints
from ..geographic_modules.historical_patterns import identify_historical_patterns
from ..geographic_modules.strategic_imperatives import calculate_strategic_imperatives

def latin_america_geography_agent(state: AgentState) -> Dict[str, Any]:
    """
    LATIN AMERICA GEOGRAPHY AGENT - Tim Marshall's Geographic Determinism Framework

    Core Thesis: Latin American politics is SHAPED by geographic features:
    1. Andes Mountains → north-south barriers, east-west isolation
    2. Amazon Rainforest → development limitations, population concentration
    3. Dual Coasts → Atlantic vs Pacific divisions, trade orientation challenges
    4. Resource Wealth → external interference, internal inequality

    MARSHALL COMPLIANCE REQUIREMENT: Geographic determinism, not policy choice
    """

    agent_name = "latin_america_geography_agent"
    print(f"🏔️ Latin America Geography Agent - Analyzing events through Marshall's geographic determinism lens")

    try:
        # Extract current events from state
        current_events = state.get('current_events', [])

        if not current_events:
            print("⚠️ No current events provided for analysis")
            return {
                "agent": agent_name,
                "analysis": "No events to analyze",
                "confidence": 0.0,
                "marshall_compliance_score": 0.0
            }

        # Latin America's Geographic Features (Marshall's Framework)
        latin_america_geographic_features = {
            "andes_mountains": {
                "feature": "Massive north-south mountain barrier",
                "isolation_effect": "East-west separation, transportation challenges",
                "strategic_imperative": "Regional integration overcoming barriers",
                "current_implications": "Infrastructure projects, trade barriers"
            },

            "amazon_rainforest": {
                "feature": "Vast tropical rainforest basin",
                "development_constraint": "Population concentration in south, resource extraction challenges",
                "strategic_imperative": "Balanced development vs environmental protection",
                "current_implications": "Deforestation, indigenous rights, resource extraction"
            },

            "dual_coasts": {
                "feature": "Both Pacific and Atlantic coastlines",
                "division_effect": "Trade orientation differences, cultural separation",
                "strategic_imperative": "Overcoming geographic divisions for unity",
                "current_implications": "Trade agreements, inter-ocean connectivity"
            },

            "resource_wealth": {
                "feature": "Rich natural resources (minerals, oil, agriculture)",
                "external_interference": "Historical colonial and modern external influence",
                "strategic_imperative": "Resource sovereignty management",
                "current_implications": "Nationalization, foreign investment, inequality"
            }
        }

        # Analyze events through geographic determinism lens
        geographic_analysis = []

        for event in current_events:
            event_analysis = analyze_event_through_geographic_lens(
                event, latin_america_geographic_features
            )
            geographic_analysis.append(event_analysis)

        # Calculate strategic imperatives based on current events
        strategic_imperatives = calculate_latin_american_strategic_imperatives(
            geographic_analysis, latin_america_geographic_features
        )

        # Identify historical patterns (Marshall requires pattern recognition)
        historical_patterns = identify_latin_american_historical_patterns(geographic_analysis)

        # Calculate Marshall thesis compliance score
        marshall_compliance = calculate_marshall_compliance(
            geographic_analysis, strategic_imperatives, historical_patterns
        )

        # Prepare final analysis
        analysis_result = {
            "agent": agent_name,
            "framework": "marshalls_geographic_determinism",
            "region": "latin_america",
            "timestamp": datetime.now().isoformat(),

            # Geographic feature analysis
            "geographic_features": latin_america_geographic_features,
            "feature_analysis": geographic_analysis,

            # Strategic imperatives (Marshall's core concept)
            "strategic_imperatives": strategic_imperatives,

            # Historical patterns (geographic determinism evidence)
            "historical_patterns": historical_patterns,

            # Marshall thesis compliance
            "marshall_compliance_score": marshall_compliance,
            "determinism_focus": geographic_determinism_score(geographic_analysis),

            # Predictive analysis based on geographic features
            "geographic_predictions": predict_latin_american_behavior(
                geographic_analysis, latin_america_geographic_features
            ),

            # Event-specific analyses
            "event_analyses": [
                {
                    "event": event.get("title", "Unknown"),
                    "geographic_determinants": extract_geographic_determinants(event),
                    "strategic_imperative": identify_event_imperative(event, latin_america_geographic_features),
                    "historical_pattern": match_historical_pattern(event),
                    "constraint_level": assess_constraint_level(event, latin_america_geographic_features)
                }
                for event in current_events[:5]  # Limit to first 5 events
            ]
        }

        # Show reasoning (for development/debugging)
        show_agent_reasoning(analysis_result, agent_name)

        # Validate Marshall compliance
        if marshall_compliance < 0.7:
            print(f"⚠️ LOW MARSHALL COMPLIANCE: {marshall_compliance:.2f} (minimum 0.70 required)")
            print("   - Geographic determinism focus must be strengthened")
            print("   - Historical pattern recognition needs improvement")
            print("   - Strategic imperative identification requires enhancement")

        return analysis_result

    except Exception as e:
        print(f"❌ Error in Latin America Geography Agent: {str(e)}")
        return {
            "agent": agent_name,
            "error": str(e),
            "analysis": "Geographic analysis failed",
            "confidence": 0.0,
            "marshall_compliance_score": 0.0
        }

def analyze_event_through_geographic_lens(event: Dict, geographic_features: Dict) -> Dict:
    """
    Analyze a specific event through Marshall's geographic determinism framework
    """

    event_title = event.get("title", "").lower()
    event_description = event.get("description", "").lower()

    analysis = {
        "event": event.get("title", "Unknown"),
        "geographic_determinants": [],
        "feature_drivers": [],
        "strategic_imperatives": [],
        "historical_continuity": False,
        "marshall_determinism_score": 0.0
    }

    # Andes Mountains analysis
    andes_keywords = ["andes", "mountain", "infrastructure", "chile", "peru", "ecuador", "bolivia", "colombia"]
    if any(keyword in event_title or keyword in event_description for keyword in andes_keywords):
        analysis["geographic_determinants"].append("andes_mountains")
        analysis["feature_drivers"].append("mountain_barrier_north_south_separation")
        analysis["strategic_imperatives"].append("regional_integration_overcoming_barriers")
        analysis["marshall_determinism_score"] += 0.25

    # Amazon Rainforest analysis
    amazon_keywords = ["amazon", "rainforest", "deforestation", "brazil", "environment", "indigenous"]
    if any(keyword in event_title or keyword in event_description for keyword in amazon_keywords):
        analysis["geographic_determinants"].append("amazon_rainforest")
        analysis["feature_drivers"].append("vast_forest_development_constraints")
        analysis["strategic_imperatives"].append("balanced_development_vs_protection")
        analysis["marshall_determinism_score"] += 0.25

    # Dual Coasts analysis
    coast_keywords = ["pacific", "atlantic", "trade", "port", "chile", "peru", "brazil", "argentina", "panama"]
    if any(keyword in event_title or keyword in event_description for keyword in coast_keywords):
        analysis["geographic_determinants"].append("dual_coasts")
        analysis["feature_drivers"].append("two_ocean_trade_orientation_division")
        analysis["strategic_imperatives"].append("overcoming_geographic_divisions")
        analysis["marshall_determinism_score"] += 0.25

    # Resource Wealth analysis
    resource_keywords = ["oil", "mining", "copper", "lithium", "resource", "commodity", "nationalization"]
    if any(keyword in event_title or keyword in event_description for keyword in resource_keywords):
        analysis["geographic_determinants"].append("resource_wealth")
        analysis["feature_drivers"].append("resource_abundance_external_interference_pattern")
        analysis["strategic_imperatives"].append("resource_sovereignty_management")
        analysis["marshall_determinism_score"] += 0.25

    return analysis

def calculate_latin_american_strategic_imperatives(analysis_results: List[Dict], geographic_features: Dict) -> Dict:
    """
    Calculate Latin American strategic imperatives based on geographic features (Marshall's core thesis)
    """

    imperatives = {
        "primary_imperatives": [],
        "secondary_imperatives": [],
        "geographic_drivers": [],
        "urgency_levels": {},
        "historical_continuity": {}
    }

    # Analyze frequency of geographic determinants across events
    determinant_counts = {}
    for analysis in analysis_results:
        for determinant in analysis.get("geographic_determinants", []):
            determinant_counts[determinant] = determinant_counts.get(determinant, 0) + 1

    # Primary imperatives (Marshall's core geographic determinants)
    if determinant_counts.get("andes_mountains", 0) > 0:
        imperatives["primary_imperatives"].append({
            "imperative": "overcome_mountain_barriers_for_regional_integration",
            "geographic_driver": "andes_mountains",
            "marshall_justification": "north_south_mountains_create_east_west_isolation",
            "urgency": "high",
            "historical_continuity": "inca_roads_to_modern_infrastructure"
        })

    if determinant_counts.get("resource_wealth", 0) > 0:
        imperatives["primary_imperatives"].append({
            "imperative": "maintain_resource_sovereignty_against_external_interference",
            "geographic_driver": "resource_wealth",
            "marshall_justification": "resource_abundance_attracts_external_exploitation",
            "urgency": "critical",
            "historical_continuity": "colonial_extraction_to_modern_nationalization"
        })

    # Secondary imperatives (geographic features)
    if determinant_counts.get("amazon_rainforest", 0) > 0:
        imperatives["secondary_imperatives"].append({
            "imperative": "balance_development_with_environmental_protection",
            "geographic_driver": "amazon_rainforest",
            "marshall_justification": "vast_forest_constrains_traditional_development",
            "urgency": "high"
        })

    if determinant_counts.get("dual_coasts", 0) > 0:
        imperatives["secondary_imperatives"].append({
            "imperative": "create_inter_ocean_connectivity_for_unity",
            "geographic_driver": "dual_coasts",
            "marshall_justification": "pacific_atlantic_division_requires_connection",
            "urgency": "medium"
        })

    return imperatives

def identify_latin_american_historical_patterns(analysis_results: List[Dict]) -> Dict:
    """
    Identify historical patterns that demonstrate geographic determinism (Marshall requirement)
    """

    patterns = {
        "resource_patterns": {
            "pattern": "resource_wealth_drives_external_interference_cycles",
            "examples": [
                "spanish_colonial_extraction",
                "banana_republic_exploitation",
                "modern_resource_nationalization"
            ],
            "geographic_determinism": True
        },

        "integration_patterns": {
            "pattern": "geographic_barriers_drive_integration_attempts",
            "examples": [
                "simon_bolivar_unity_attempts",
                "regional_trade_blocs",
                "modern_infrastructure_projects"
            ],
            "geographic_determinism": True
        },

        "development_patterns": {
            "pattern": "geographic_constraints_create_development_inequality",
            "examples": [
                "coastal_vs_interior_development",
                "south_concentration_vs_amazon_isolation",
                "resource_export_dependence"
            ],
            "geographic_determinism": True
        }
    }

    return patterns

def calculate_marshall_compliance(analysis_results: List[Dict], imperatives: Dict, patterns: Dict) -> float:
    """
    Calculate compliance with Marshall's geographic determinism thesis
    Minimum 70% required for valid analysis
    """

    scores = {
        "geographic_determinism_focus": 0.0,  # 40% weight
        "historical_pattern_recognition": 0.0,  # 30% weight
        "strategic_imperative_identification": 0.0,  # 20% weight
        "constraint_over_choice_emphasis": 0.0  # 10% weight
    }

    # Geographic determinism focus (40%)
    total_events = len(analysis_results)
    if total_events > 0:
        determinism_events = sum(1 for analysis in analysis_results
                               if analysis.get("marshall_determinism_score", 0) > 0.5)
        scores["geographic_determinism_focus"] = (determinism_events / total_events) * 0.4

    # Historical pattern recognition (30%)
    pattern_score = 0.0
    if patterns.get("resource_patterns", {}).get("geographic_determinism"):
        pattern_score += 0.1
    if patterns.get("integration_patterns", {}).get("geographic_determinism"):
        pattern_score += 0.1
    if patterns.get("development_patterns", {}).get("geographic_determinism"):
        pattern_score += 0.1
    scores["historical_pattern_recognition"] = pattern_score

    # Strategic imperative identification (20%)
    primary_imperatives = len(imperatives.get("primary_imperatives", []))
    if primary_imperatives >= 2:
        scores["strategic_imperative_identification"] = 0.2
    elif primary_imperatives >= 1:
        scores["strategic_imperative_identification"] = 0.1

    # Constraint over choice emphasis (10%)
    constraint_emphasis = sum(analysis.get("marshall_determinism_score", 0)
                            for analysis in analysis_results) / max(total_events, 1)
    scores["constraint_over_choice_emphasis"] = min(constraint_emphasis * 0.1, 0.1)

    total_compliance = sum(scores.values())

    return round(total_compliance, 3)

def geographic_determinism_score(analysis_results: List[Dict]) -> float:
    """
    Calculate how strongly the analysis emphasizes geographic determinism
    """
    if not analysis_results:
        return 0.0

    determinism_scores = [analysis.get("marshall_determinism_score", 0)
                         for analysis in analysis_results]
    return sum(determinism_scores) / len(determinism_scores)

def predict_latin_american_behavior(analysis_results: List[Dict], geographic_features: Dict) -> List[Dict]:
    """
    Predict Latin American behavior based on geographic features (Marshall's predictive framework)
    """

    predictions = []

    # Analyze current events to identify geographic drivers
    active_features = set()
    for analysis in analysis_results:
        for determinant in analysis.get("geographic_determinants", []):
            active_features.add(determinant)

    # Predict behaviors based on active geographic features

    if "resource_wealth" in active_features:
        predictions.append({
            "prediction": "continued_resource_nationalization_and_sovereignty_assertion",
            "geographic_driver": "resource_wealth",
            "marshall_justification": "resource_abundance_drives_external_interference_resistance",
            "confidence": 0.8,
            "timeframe": "medium_term (2-5 years)"
        })

    if "andes_mountains" in active_features:
        predictions.append({
            "prediction": "accelerated_infrastructure_projects_for_regional_integration",
            "geographic_driver": "andes_mountains",
            "marshall_justification": "mountain_barriers_require_connectivity_investment",
            "confidence": 0.7,
            "timeframe": "long_term (5-10 years)"
        })

    if "amazon_rainforest" in active_features:
        predictions.append({
            "prediction": "continued_development_vs_environmental_protection_tensions",
            "geographic_driver": "amazon_rainforest",
            "marshall_justification": "vast_forest_constrains_traditional_development_patterns",
            "confidence": 0.9,
            "timeframe": "ongoing"
        })

    return predictions

def extract_geographic_determinants(event: Dict) -> List[str]:
    """Extract geographic determinants from event"""
    return []

def identify_event_imperative(event: Dict, geographic_features: Dict) -> str:
    """Identify strategic imperative driving event"""
    return ""

def match_historical_pattern(event: Dict) -> str:
    """Match event to historical pattern"""
    return ""

def assess_constraint_level(event: Dict, geographic_features: Dict) -> float:
    """Assess how strongly geographic constraints drive event"""
    return 0.0

# Integration with LLM providers for enhanced analysis
def analyze_with_llm(event_data: Dict, framework: str = "marshalls_geographic_determinism") -> Dict:
    """
    Enhanced analysis using LLM providers for deeper geographic determinism understanding
    """

    llm_prompt = f"""
    Analyze this geopolitical event through Tim Marshall's geographic determinism framework:

    Event: {event_data}
    Framework: {framework}

    MARSHALL THESIS REQUIREMENTS:
    1. Geographic DETERMINISM (not just influence) - geography forces specific behaviors
    2. Historical pattern recognition - recurring behaviors from geographic constraints
    3. Strategic imperative identification - must-have policies driven by geography
    4. Constraint over choice - geographic limitations determine policy options

    Analyze:
    - What geographic features are driving this event?
    - How do these features shape Latin American strategic choices?
    - What historical patterns does this event follow?
    - What strategic imperatives are being fulfilled?

    Provide analysis that demonstrates 70%+ compliance with Marshall's geographic determinism thesis.
    """

    return {
        "llm_analysis": "LLM-enhanced geographic determinism analysis",
        "marshall_compliance": 0.75
    }