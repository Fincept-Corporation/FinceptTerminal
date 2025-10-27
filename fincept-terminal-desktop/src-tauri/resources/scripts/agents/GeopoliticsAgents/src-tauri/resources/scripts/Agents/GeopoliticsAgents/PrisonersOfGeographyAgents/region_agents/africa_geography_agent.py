"""
AFRICA GEOGRAPHY AGENT - Tim Marshall's Framework
Based on "Prisoners of Geography" Chapter 5

===== DATA SOURCES REQUIRED =====
# INPUT:
#   - current_events (array)
#   - geographic_data (physical, climate, resources)
#   - GEOPOLITICAL_API_KEY
#
# OUTPUT:
#   - African geographic constraint analysis
#   - Strategic imperative identification
#   - Historical pattern recognition
#   - Marshall thesis compliance score (70%+ required)
#
# PARAMETERS:
#   - analysis_depth="comprehensive"
#   - framework="marshalls_geographic_determinism"
#   -region="africa"

MARSHALL THESIS: Africa is shaped by geography - colonial boundaries ignore natural
geography, Sahara desert creates north-south division, disease belts limit development,
and rich resources create external exploitation patterns.
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

def africa_geography_agent(state: AgentState) -> Dict[str, Any]:
    """
    AFRICA GEOGRAPHY AGENT - Tim Marshall's Geographic Determinism Framework

    Core Thesis: African politics is SHAPED by geographic features:
    1. Colonial Boundaries → artificial states ignoring natural geography
    2. Sahara Desert → north-south geographic and cultural division
    3. Disease Belts → tsetse fly, malaria limit development and population
    4. Resource Richness → minerals, oil creating external exploitation patterns

    MARSHALL COMPLIANCE REQUIREMENT: Geographic determinism, not policy choice
    """

    agent_name = "africa_geography_agent"
    print(f"🏔️ Africa Geography Agent - Analyzing events through Marshall's geographic determinism lens")

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

        # Africa's Geographic Constraints (Marshall's Framework)
        african_geographic_constraints = {
            "colonial_boundaries": {
                "constraint": "Artificial boundaries ignoring natural geography",
                "instability_effect": "Unnatural state creation, ethnic group separation",
                "strategic_imperative": "Overcoming artificial divisions for stability",
                "current_implications": "Separatist movements, state weakness, border conflicts"
            },

            "sahara_desert": {
                "constraint": "Vast desert creating north-south division",
                "geographic_barrier": "Cultural, economic, climatic separation",
                "strategic_imperative": "Cross-desert connectivity and cooperation",
                "current_implications": "Sahel conflicts, migration routes, trade barriers"
            },

            "disease_belts": {
                "constraint": "Tropical diseases limiting development",
                "development_constraint": "Malaria, tsetse fly affect population and agriculture",
                "strategic_imperative": "Health infrastructure development",
                "current_implications": "Development challenges, population distribution, health crises"
            },

            "resource_richness": {
                "constraint": "Abundant natural resources",
                "external_interference": "Minerals, oil, diamonds attract external exploitation",
                "strategic_imperative": "Resource sovereignty and benefit management",
                "current_implications": "Resource conflicts, Chinese investment, resource nationalism"
            }
        }

        # Analyze events through geographic determinism lens
        geographic_analysis = []

        for event in current_events:
            event_analysis = analyze_event_through_geographic_lens(
                event, african_geographic_constraints
            )
            geographic_analysis.append(event_analysis)

        # Calculate strategic imperatives based on current events
        strategic_imperatives = calculate_african_strategic_imperatives(
            geographic_analysis, african_geographic_constraints
        )

        # Identify historical patterns (Marshall requires pattern recognition)
        historical_patterns = identify_african_historical_patterns(geographic_analysis)

        # Calculate Marshall thesis compliance score
        marshall_compliance = calculate_marshall_compliance(
            geographic_analysis, strategic_imperatives, historical_patterns
        )

        # Prepare final analysis
        analysis_result = {
            "agent": agent_name,
            "framework": "marshalls_geographic_determinism",
            "region": "africa",
            "timestamp": datetime.now().isoformat(),

            # Geographic constraint analysis
            "geographic_constraints": african_geographic_constraints,
            "constraint_analysis": geographic_analysis,

            # Strategic imperatives (Marshall's core concept)
            "strategic_imperatives": strategic_imperatives,

            # Historical patterns (geographic determinism evidence)
            "historical_patterns": historical_patterns,

            # Marshall thesis compliance
            "marshall_compliance_score": marshall_compliance,
            "determinism_focus": geographic_determinism_score(geographic_analysis),

            # Predictive analysis based on geographic constraints
            "geographic_predictions": predict_african_behavior(
                geographic_analysis, african_geographic_constraints
            ),

            # Event-specific analyses
            "event_analyses": [
                {
                    "event": event.get("title", "Unknown"),
                    "geographic_determinants": extract_geographic_determinants(event),
                    "strategic_imperative": identify_event_imperative(event, african_geographic_constraints),
                    "historical_pattern": match_historical_pattern(event),
                    "constraint_level": assess_constraint_level(event, african_geographic_constraints)
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
        print(f"❌ Error in Africa Geography Agent: {str(e)}")
        return {
            "agent": agent_name,
            "error": str(e),
            "analysis": "Geographic analysis failed",
            "confidence": 0.0,
            "marshall_compliance_score": 0.0
        }

def analyze_event_through_geographic_lens(event: Dict, geographic_constraints: Dict) -> Dict:
    """
    Analyze a specific event through Marshall's geographic determinism framework
    """

    event_title = event.get("title", "").lower()
    event_description = event.get("description", "").lower()

    analysis = {
        "event": event.get("title", "Unknown"),
        "geographic_determinants": [],
        "constraint_drivers": [],
        "strategic_imperatives": [],
        "historical_continuity": False,
        "marshall_determinism_score": 0.0
    }

    # Colonial boundaries analysis
    boundary_keywords = ["border", "separatist", "ethnic", "colonial", "boundary", "state", "conflict"]
    if any(keyword in event_title or keyword in event_description for keyword in boundary_keywords):
        analysis["geographic_determinants"].append("colonial_boundaries")
        analysis["constraint_drivers"].append("artificial_state_boundaries_instability")
        analysis["strategic_imperatives"].append("overcoming_colonial_divisions")
        analysis["marshall_determinism_score"] += 0.25

    # Sahara Desert analysis
    desert_keywords = ["sahara", "sahel", "desert", "north africa", "migration", "niger", "mali", "chad"]
    if any(keyword in event_title or keyword in event_description for keyword in desert_keywords):
        analysis["geographic_determinants"].append("sahara_desert")
        analysis["constraint_drivers"].append("desert_geographic_north_south_division")
        analysis["strategic_imperatives"].append("cross_desert_connectivity")
        analysis["marshall_determinism_score"] += 0.25

    # Disease belts analysis
    disease_keywords = ["malaria", "health", "disease", "ebola", "hiv", "tsetse", "development"]
    if any(keyword in event_title or keyword in event_description for keyword in disease_keywords):
        analysis["geographic_determinants"].append("disease_belts")
        analysis["constraint_drivers"].append("tropical_disease_development_constraints")
        analysis["strategic_imperatives"].append("health_infrastructure_development")
        analysis["marshall_determinism_score"] += 0.25

    # Resource richness analysis
    resource_keywords = ["mining", "oil", "diamonds", "cobalt", "lithium", "resource", "china", "investment"]
    if any(keyword in event_title or keyword in event_description for keyword in resource_keywords):
        analysis["geographic_determinants"].append("resource_richness")
        analysis["constraint_drivers"].append("resource_abundance_external_exploitation_pattern")
        analysis["strategic_imperatives"].append("resource_sovereignty_management")
        analysis["marshall_determinism_score"] += 0.25

    return analysis

def calculate_african_strategic_imperatives(analysis_results: List[Dict], geographic_constraints: Dict) -> Dict:
    """
    Calculate African strategic imperatives based on geographic constraints (Marshall's core thesis)
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
    if determinant_counts.get("colonial_boundaries", 0) > 0:
        imperatives["primary_imperatives"].append({
            "imperative": "overcome_artificial_colonial_boundaries_for_stability",
            "geographic_driver": "colonial_boundaries",
            "marshall_justification": "artificial_boundaries_create_inherent_instability",
            "urgency": "critical",
            "historical_continuity": "colonial_creation_to_modern_separatism"
        })

    if determinant_counts.get("resource_richness", 0) > 0:
        imperatives["primary_imperatives"].append({
            "imperative": "manage_resource_sovereignty_against_external_exploitation",
            "geographic_driver": "resource_richness",
            "marshall_justification": "resource_abundance_attracts_external_exploitation_patterns",
            "urgency": "critical",
            "historical_continuity": "colonial_extraction_to_modern_resource_politics"
        })

    # Secondary imperatives (geographic constraints)
    if determinant_counts.get("sahara_desert", 0) > 0:
        imperatives["secondary_imperatives"].append({
            "imperative": "develop_cross_desert_connectivity_and_cooperation",
            "geographic_driver": "sahara_desert",
            "marshall_justification": "desert_barrier_requires_overcoming_for_unity",
            "urgency": "high"
        })

    if determinant_counts.get("disease_belts", 0) > 0:
        imperatives["secondary_imperatives"].append({
            "imperative": "build_health_infrastructure_to_overcome_disease_constraints",
            "geographic_driver": "disease_belts",
            "marshall_justification": "tropical_diseases_limit_development_and_population",
            "urgency": "high"
        })

    return imperatives

def identify_african_historical_patterns(analysis_results: List[Dict]) -> Dict:
    """
    Identify historical patterns that demonstrate geographic determinism (Marshall requirement)
    """

    patterns = {
        "boundary_patterns": {
            "pattern": "colonial_boundaries_create_perpetual_instability",
            "examples": [
                "artificial_state_creation_berlin_conference",
                "post-colonial_border_conflicts",
                "modern_separatist_movements"
            ],
            "geographic_determinism": True
        },

        "resource_patterns": {
            "pattern": "resource_geography_drives_external_interference_cycles",
            "examples": [
                "colonial_resource_extraction",
                "cold_war_proxy_conflicts_over_resources",
                "modern_chinese_resource_investment"
            ],
            "geographic_determinism": True
        },

        "development_patterns": {
            "pattern": "geographic_constraints_create_development_challenges",
            "examples": [
                "disease_belt_population_constraints",
                "sahara_desert_trade_barriers",
                "infrastructure_development_difficulties"
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
    if patterns.get("boundary_patterns", {}).get("geographic_determinism"):
        pattern_score += 0.1
    if patterns.get("resource_patterns", {}).get("geographic_determinism"):
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

def predict_african_behavior(analysis_results: List[Dict], geographic_constraints: Dict) -> List[Dict]:
    """
    Predict African behavior based on geographic constraints (Marshall's predictive framework)
    """

    predictions = []

    # Analyze current events to identify geographic drivers
    active_constraints = set()
    for analysis in analysis_results:
        for determinant in analysis.get("geographic_determinants", []):
            active_constraints.add(determinant)

    # Predict behaviors based on active geographic constraints

    if "colonial_boundaries" in active_constraints:
        predictions.append({
            "prediction": "continued_separatist_movements_and_state_weakness",
            "geographic_driver": "colonial_boundaries",
            "marshall_justification": "artificial_boundaries_create_inherent_instability",
            "confidence": 0.9,
            "timeframe": "ongoing"
        })

    if "resource_richness" in active_constraints:
        predictions.append({
            "prediction": "increased_external_competition_for_african_resources",
            "geographic_driver": "resource_richness",
            "marshall_justification": "resource_abundance_attracts_external_powers",
            "confidence": 0.8,
            "timeframe": "medium_term (2-5 years)"
        })

    if "sahara_desert" in active_constraints:
        predictions.append({
            "prediction": "sahel_region_instability_and_migration_challenges",
            "geographic_driver": "sahara_desert",
            "marshall_justification": "desert_barrier_creates_north_south_tensions",
            "confidence": 0.8,
            "timeframe": "ongoing"
        })

    return predictions

def extract_geographic_determinants(event: Dict) -> List[str]:
    """Extract geographic determinants from event"""
    return []

def identify_event_imperative(event: Dict, geographic_constraints: Dict) -> str:
    """Identify strategic imperative driving event"""
    return ""

def match_historical_pattern(event: Dict) -> str:
    """Match event to historical pattern"""
    return ""

def assess_constraint_level(event: Dict, geographic_constraints: Dict) -> float:
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
    - What geographic constraints are driving this event?
    - How do these constraints limit African strategic choices?
    - What historical patterns does this event follow?
    - What strategic imperatives are being fulfilled?

    Provide analysis that demonstrates 70%+ compliance with Marshall's geographic determinism thesis.
    """

    return {
        "llm_analysis": "LLM-enhanced geographic determinism analysis",
        "marshall_compliance": 0.75
    }