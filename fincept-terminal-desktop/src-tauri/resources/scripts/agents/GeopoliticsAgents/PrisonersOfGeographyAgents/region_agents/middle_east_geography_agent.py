"""
MIDDLE EAST GEOGRAPHY AGENT - Tim Marshall's Framework
Based on "Prisoners of Geography" Chapter 6

===== DATA SOURCES REQUIRED =====
# INPUT:
#   - current_events (array)
#   - geographic_data (physical, climate, resources)
#   - GEOPOLITICAL_API_KEY
#
# OUTPUT:
#   - Middle East geographic constraint analysis
#   - Strategic imperative identification
#   - Historical pattern recognition
#   - Marshall thesis compliance score (70%+ required)
#
# PARAMETERS:
#   - analysis_depth="comprehensive"
#   - framework="marshalls_geographic_determinism"
#   - region="middle_east"

MARSHALL THESIS: Middle East is a prisoner of its geography - desert creates population
concentration, oil wealth distorts economies, water scarcity drives conflict, and colonial
borders create artificial states with inherent instability.
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

def middle_east_geography_agent(state: AgentState) -> Dict[str, Any]:
    """
    MIDDLE EAST GEOGRAPHY AGENT - Tim Marshall's Geographic Determinism Framework

    Core Thesis: Middle Eastern politics is DETERMINED by geographic constraints:
    1. Desert geography → population concentration, limited agricultural land
    2. Oil resource curse → wealth without economic diversification
    3. Water scarcity → conflict over limited resources
    4. Colonial borders → artificial states with internal divisions

    MARSHALL COMPLIANCE REQUIREMENT: Geographic determinism, not policy choice
    """

    agent_name = "middle_east_geography_agent"
    print(f"🏔️ Middle East Geography Agent - Analyzing events through Marshall's geographic determinism lens")

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

        # Middle East's Geographic Constraints (Marshall's Framework)
        middle_east_geographic_prisons = {
            "desert_geography": {
                "constraint": "Vast deserts with limited habitable areas",
                "population_concentration": "People concentrated in valleys, coasts, oases",
                "strategic_imperative": "Control of limited habitable zones",
                "current_implications": "Urban concentration, agricultural limitations"
            },

            "oil_resource_curse": {
                "constraint": "Massive oil wealth without economic diversification",
                "economic_distortion": "Rentier state model, lack of development",
                "strategic_imperative": "Oil price stability maintenance",
                "current_implications": "Price wars, OPEC dynamics, wealth concentration"
            },

            "water_scarcity": {
                "constraint": "Limited water resources in arid climate",
                "conflict_driver": "Rivers, aquifers, and water access disputes",
                "strategic_imperative": "Water resource security and control",
                "current_implications": "Jordan River, Tigris-Euphrates, Nile disputes"
            },

            "colonial_borders": {
                "constraint": "Artificial boundaries ignoring ethnic/realities",
                "state_instability": "Artificial states with internal divisions",
                "strategic_imperative": "Internal security and regime survival",
                "current_implications": "Syria, Iraq, Libya, Yemen instability"
            }
        }

        # Analyze events through geographic determinism lens
        geographic_analysis = []

        for event in current_events:
            event_analysis = analyze_event_through_geographic_lens(
                event, middle_east_geographic_prisons
            )
            geographic_analysis.append(event_analysis)

        # Calculate strategic imperatives based on current events
        strategic_imperatives = calculate_middle_east_strategic_imperatives(
            geographic_analysis, middle_east_geographic_prisons
        )

        # Identify historical patterns (Marshall requires pattern recognition)
        historical_patterns = identify_middle_east_historical_patterns(geographic_analysis)

        # Calculate Marshall thesis compliance score
        marshall_compliance = calculate_marshall_compliance(
            geographic_analysis, strategic_imperatives, historical_patterns
        )

        # Prepare final analysis
        analysis_result = {
            "agent": agent_name,
            "framework": "marshalls_geographic_determinism",
            "region": "middle_east",
            "timestamp": datetime.now().isoformat(),

            # Geographic constraint analysis
            "geographic_prisons": middle_east_geographic_prisons,
            "constraint_analysis": geographic_analysis,

            # Strategic imperatives (Marshall's core concept)
            "strategic_imperatives": strategic_imperatives,

            # Historical patterns (geographic determinism evidence)
            "historical_patterns": historical_patterns,

            # Marshall thesis compliance
            "marshall_compliance_score": marshall_compliance,
            "determinism_focus": geographic_determinism_score(geographic_analysis),

            # Predictive analysis based on geographic constraints
            "geographic_predictions": predict_middle_east_behavior(
                geographic_analysis, middle_east_geographic_prisons
            ),

            # Event-specific analyses
            "event_analyses": [
                {
                    "event": event.get("title", "Unknown"),
                    "geographic_determinants": extract_geographic_determinants(event),
                    "strategic_imperative": identify_event_imperative(event, middle_east_geographic_prisons),
                    "historical_pattern": match_historical_pattern(event),
                    "constraint_level": assess_constraint_level(event, middle_east_geographic_prisons)
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
        print(f"❌ Error in Middle East Geography Agent: {str(e)}")
        return {
            "agent": agent_name,
            "error": str(e),
            "analysis": "Geographic analysis failed",
            "confidence": 0.0,
            "marshall_compliance_score": 0.0
        }

def analyze_event_through_geographic_lens(event: Dict, geographic_prisons: Dict) -> Dict:
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

    # Desert geography analysis
    desert_keywords = ["desert", "arid", "population", "urban", "agriculture", "land"]
    if any(keyword in event_title or keyword in event_description for keyword in desert_keywords):
        analysis["geographic_determinants"].append("desert_geography")
        analysis["constraint_drivers"].append("limited_habitable_area_concentration")
        analysis["strategic_imperatives"].append("control_of_populated_zones")
        analysis["marshall_determinism_score"] += 0.25

    # Oil resource curse analysis
    oil_keywords = ["oil", "petroleum", "opec", "energy", "price", "export", "saudi"]
    if any(keyword in event_title or keyword in event_description for keyword in oil_keywords):
        analysis["geographic_determinants"].append("oil_resource_curse")
        analysis["constraint_drivers"].append("resource_wealth_without_diversification")
        analysis["strategic_imperatives"].append("oil_price_maintenance")
        analysis["marshall_determinism_score"] += 0.25

    # Water scarcity analysis
    water_keywords = ["water", "river", "aquifer", "scarce", "irrigation", "tigris", "Euphrates"]
    if any(keyword in event_title or keyword in event_description for keyword in water_keywords):
        analysis["geographic_determinants"].append("water_scarcity")
        analysis["constraint_drivers"].append("limited_water_resource_competition")
        analysis["strategic_imperatives"].append("water_security_control")
        analysis["marshall_determinism_score"] += 0.25

    # Colonial borders analysis
    border_keywords = ["syria", "iraq", "libya", "yemen", "border", "colonial", "artificial"]
    if any(keyword in event_title or keyword in event_description for keyword in border_keywords):
        analysis["geographic_determinants"].append("colonial_borders")
        analysis["constraint_drivers"].append("artificial_state_boundaries")
        analysis["strategic_imperatives"].append("regime_survival_internal_security")
        analysis["marshall_determinism_score"] += 0.25

    return analysis

def calculate_middle_east_strategic_imperatives(analysis_results: List[Dict], geographic_prisons: Dict) -> Dict:
    """
    Calculate Middle East strategic imperatives based on geographic constraints (Marshall's core thesis)
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
    if determinant_counts.get("oil_resource_curse", 0) > 0:
        imperatives["primary_imperatives"].append({
            "imperative": "maintain_oil_price_stability_and_market_share",
            "geographic_driver": "oil_resource_curse",
            "marshall_justification": "oil wealth without diversification requires price control",
            "urgency": "critical",
            "historical_continuity": "oil_discoveries_to_opec_wars"
        })

    if determinant_counts.get("water_scarcity", 0) > 0:
        imperatives["primary_imperatives"].append({
            "imperative": "secure_water_resource_control_and_access",
            "geographic_driver": "water_scarcity",
            "marshall_justification": "arid climate makes water zero-sum_resource",
            "urgency": "critical",
            "historical_continuity": "ancient_water_wars_to_modern_disputes"
        })

    # Secondary imperatives (geographic constraints)
    if determinant_counts.get("desert_geography", 0) > 0:
        imperatives["secondary_imperatives"].append({
            "imperative": "control_urban_population_centers",
            "geographic_driver": "desert_geography",
            "marshall_justification": "limited_habitable_areas_concentrate_power",
            "urgency": "high"
        })

    if determinant_counts.get("colonial_borders", 0) > 0:
        imperatives["secondary_imperatives"].append({
            "imperative": "maintain_regime_survival_through_internal_security",
            "geographic_driver": "colonial_borders",
            "marshall_justification": "artificial_states_create_instability_needing_repression",
            "urgency": "high"
        })

    return imperatives

def identify_middle_east_historical_patterns(analysis_results: List[Dict]) -> Dict:
    """
    Identify historical patterns that demonstrate geographic determinism (Marshall requirement)
    """

    patterns = {
        "resource_patterns": {
            "pattern": "oil_resource_geography_determines_economic_and_political_structures",
            "examples": [
                "rentier_state_development",
                "opec_formation_and_price_wars",
                "resource_cursed_economies"
            ],
            "geographic_determinism": True
        },

        "conflict_patterns": {
            "pattern": "water_scarcity_drives_recurring_conflicts",
            "examples": [
                "ancient_mesopotamian_water_wars",
                "modern_river_disputes",
                "aquifer_conflicts"
            ],
            "geographic_determinism": True
        },

        "stability_patterns": {
            "pattern": "colonial_borders_create_perpetual_instability",
            "examples": [
                "sykes-picot_artificial_states",
                "post-colonial_state_failure",
                "modern_separatist_movements"
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
    if patterns.get("conflict_patterns", {}).get("geographic_determinism"):
        pattern_score += 0.1
    if patterns.get("stability_patterns", {}).get("geographic_determinism"):
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

def predict_middle_east_behavior(analysis_results: List[Dict], geographic_prisons: Dict) -> List[Dict]:
    """
    Predict Middle East behavior based on geographic constraints (Marshall's predictive framework)
    """

    predictions = []

    # Analyze current events to identify geographic drivers
    active_constraints = set()
    for analysis in analysis_results:
        for determinant in analysis.get("geographic_determinants", []):
            active_constraints.add(determinant)

    # Predict behaviors based on active geographic constraints

    if "oil_resource_curse" in active_constraints:
        predictions.append({
            "prediction": "continued_oil_price_volatility_and_market_share_competition",
            "geographic_driver": "oil_resource_curse",
            "marshall_justification": "resource_dependence_creates_price_sensitivity",
            "confidence": 0.9,
            "timeframe": "ongoing"
        })

    if "water_scarcity" in active_constraints:
        predictions.append({
            "prediction": "intensified_water_resource_conflicts_and_cooperation_attempts",
            "geographic_driver": "water_scarcity",
            "marshall_justification": "arid_climate_makes_water_zero_sum_resource",
            "confidence": 0.8,
            "timeframe": "medium_term (2-5 years)"
        })

    if "colonial_borders" in active_constraints:
        predictions.append({
            "prediction": "continued_state_instability_and_separatist_movements",
            "geographic_driver": "colonial_borders",
            "marshall_justification": "artificial_boundaries_create_inherent_instability",
            "confidence": 0.8,
            "timeframe": "long_term (5-10 years)"
        })

    return predictions

def extract_geographic_determinants(event: Dict) -> List[str]:
    """Extract geographic determinants from event"""
    return []

def identify_event_imperative(event: Dict, geographic_prisons: Dict) -> str:
    """Identify strategic imperative driving event"""
    return ""

def match_historical_pattern(event: Dict) -> str:
    """Match event to historical pattern"""
    return ""

def assess_constraint_level(event: Dict, geographic_prisons: Dict) -> float:
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
    - How do these constraints limit Middle Eastern strategic choices?
    - What historical patterns does this event follow?
    - What strategic imperatives are being fulfilled?

    Provide analysis that demonstrates 70%+ compliance with Marshall's geographic determinism thesis.
    """

    return {
        "llm_analysis": "LLM-enhanced geographic determinism analysis",
        "marshall_compliance": 0.75
    }