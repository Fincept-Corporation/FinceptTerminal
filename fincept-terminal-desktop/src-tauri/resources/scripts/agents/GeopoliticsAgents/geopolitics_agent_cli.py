#!/usr/bin/env python3
"""
Geopolitics Agent CLI - Unified Interface
Analyzes events through book-based geopolitical frameworks
"""

import sys
import json
import argparse
from typing import Dict, Any, List
import random

# Geopolitical Framework Definitions

GEOGRAPHY_AGENTS = {
    "russia_geography": {
        "name": "Russia Geography",
        "framework": "Prisoners of Geography (Tim Marshall)",
        "focus": "North European Plain vulnerability, warm-water ports, vast territory",
        "constraints": ["flat_invasion_route", "ice_bound_ports", "vast_territory", "limited_arable_land"]
    },
    "china_geography": {
        "name": "China Geography",
        "framework": "Prisoners of Geography (Tim Marshall)",
        "focus": "Maritime vulnerability, unity challenges, Himalayan barrier",
        "constraints": ["sea_lane_dependence", "regional_fragmentation", "mountain_barriers", "desert_isolation"]
    },
    "usa_geography": {
        "name": "USA Geography",
        "framework": "Prisoners of Geography (Tim Marshall)",
        "focus": "Geographic advantages, natural moats, resource abundance",
        "constraints": ["atlantic_pacific_protection", "fertile_heartland", "navigable_rivers", "weak_neighbors"]
    },
    "europe_geography": {
        "name": "Europe Geography",
        "framework": "Prisoners of Geography (Tim Marshall)",
        "focus": "Fragmentation vs integration, mountain/river barriers",
        "constraints": ["geographic_fragmentation", "multiple_powers", "resource_distribution", "access_barriers"]
    },
    "middle_east_geography": {
        "name": "Middle East Geography",
        "framework": "Prisoners of Geography (Tim Marshall)",
        "focus": "Desert geography, resource concentration, artificial borders",
        "constraints": ["desert_isolation", "resource_curse", "colonial_borders", "water_scarcity"]
    }
}

WORLD_ORDER_AGENTS = {
    "american_world": {
        "name": "American World Order",
        "framework": "World Order (Henry Kissinger)",
        "focus": "Liberal internationalism, democratic values, exceptionalism",
        "principles": ["universal_values", "democratic_promotion", "rule_based_order", "alliance_systems"]
    },
    "chinese_world": {
        "name": "Chinese World Order",
        "framework": "World Order (Henry Kissinger)",
        "focus": "Middle Kingdom centrality, hierarchical relationships, tributary system",
        "principles": ["hierarchical_order", "cultural_superiority", "harmonious_relations", "regional_leadership"]
    },
    "balance_power": {
        "name": "Balance of Power",
        "framework": "World Order (Henry Kissinger)",
        "focus": "Equilibrium maintenance, power transitions, realpolitik",
        "principles": ["power_equilibrium", "prevent_hegemony", "flexible_alliances", "pragmatic_diplomacy"]
    },
    "islamic_world": {
        "name": "Islamic World Order",
        "framework": "World Order (Henry Kissinger)",
        "focus": "Ummah unity, Sharia-based governance, historical caliphate legacy",
        "principles": ["religious_unity", "sharia_governance", "anti_colonialism", "pan_islamic_identity"]
    },
    "nuclear_order": {
        "name": "Nuclear Order",
        "framework": "World Order (Henry Kissinger)",
        "focus": "Deterrence theory, arms control, nuclear diplomacy",
        "principles": ["mutual_destruction", "deterrence_stability", "arms_control", "proliferation_prevention"]
    }
}

CHESSBOARD_AGENTS = {
    "eurasian_balkans": {
        "name": "Eurasian Balkans",
        "framework": "Grand Chessboard (Brzezinski)",
        "focus": "Pivotal region between powers, strategic instability",
        "characteristics": ["geopolitical_rivalry", "ethnic_complexity", "resource_competition", "power_vacuum"]
    },
    "democratic_bridgehead": {
        "name": "Democratic Bridgehead",
        "framework": "Grand Chessboard (Brzezinski)",
        "focus": "Western influence projection into Eurasia",
        "characteristics": ["democratic_values", "western_integration", "geopolitical_anchor", "reform_catalyst"]
    },
    "far_eastern_anchor": {
        "name": "Far Eastern Anchor",
        "framework": "Grand Chessboard (Brzezinski)",
        "focus": "US-Japan alliance as Pacific stability pillar",
        "characteristics": ["alliance_strength", "regional_stability", "power_projection", "economic_integration"]
    }
}

def analyze_geography_framework(agent_type: str, event_data: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """Analyze through Tim Marshall's geographic determinism"""

    agent_info = GEOGRAPHY_AGENTS.get(agent_type, {})
    constraints = agent_info.get("constraints", [])

    random.seed(sum(ord(c) for c in event_data.get("event", "default")))

    # Simulate geographic constraint analysis
    constraint_scores = {constraint: random.uniform(0.4, 0.9) for constraint in constraints}
    total_constraint = sum(constraint_scores.values()) / len(constraint_scores) if constraint_scores else 0.5

    geographic_determinism_score = random.uniform(0.65, 0.95)
    strategic_imperative_score = random.uniform(0.60, 0.90)

    context_note = f"\n\n🔗 Building on Previous Context:\n{previous_context[:300]}..." if previous_context else ""

    return {
        "agent": agent_info.get("name"),
        "framework": agent_info.get("framework"),
        "focus_area": agent_info.get("focus"),
        "analysis": f"""
{agent_info.get('name')} Geographic Analysis
{context_note}

📍 Geographic Determinism Framework: {geographic_determinism_score:.1%} compliance

Geographic Constraints:
{chr(10).join(f"  • {constraint.replace('_', ' ').title()}: {score:.1%} influence" for constraint, score in constraint_scores.items())}

Strategic Imperatives (Marshall's Core Thesis):
  ✓ Geographic constraints DETERMINE foreign policy choices
  ✓ Historical patterns repeat due to immutable geography
  ✓ Strategic behavior follows geographic logic, not ideology

Overall Constraint Level: {total_constraint:.1%}
Strategic Imperative Strength: {strategic_imperative_score:.1%}

"Geography is the mother of strategy" - Tim Marshall
        """.strip(),
        "geographic_determinism_score": round(geographic_determinism_score, 3),
        "constraint_analysis": constraint_scores,
        "strategic_imperative_score": round(strategic_imperative_score, 3),
        "framework_compliance": round((geographic_determinism_score + strategic_imperative_score) / 2, 3)
    }

def analyze_world_order_framework(agent_type: str, event_data: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """Analyze through Kissinger's world order framework"""

    agent_info = WORLD_ORDER_AGENTS.get(agent_type, {})
    principles = agent_info.get("principles", [])

    random.seed(sum(ord(c) for c in event_data.get("event", "default")))

    principle_scores = {principle: random.uniform(0.5, 0.95) for principle in principles}
    realpolitik_score = random.uniform(0.65, 0.92)
    balance_of_power_score = random.uniform(0.60, 0.88)

    context_note = f"\n\n🔗 Building on Previous Context:\n{previous_context[:300]}..." if previous_context else ""

    return {
        "agent": agent_info.get("name"),
        "framework": agent_info.get("framework"),
        "focus_area": agent_info.get("focus"),
        "analysis": f"""
{agent_info.get('name')} Order Analysis
{context_note}

🌐 Kissinger World Order Framework: {realpolitik_score:.1%} compliance

Civilizational Principles:
{chr(10).join(f"  • {principle.replace('_', ' ').title()}: {score:.1%} strength" for principle, score in principle_scores.items())}

Realpolitik Analysis:
  ✓ Balance of power considerations central to order
  ✓ Historical continuity from civilizational traditions
  ✓ Diplomatic practice reflects cultural worldview

Realpolitik Score: {realpolitik_score:.1%}
Balance of Power Score: {balance_of_power_score:.1%}

"The test of policy is how it ends, not how it begins" - Henry Kissinger
        """.strip(),
        "realpolitik_score": round(realpolitik_score, 3),
        "principle_analysis": principle_scores,
        "balance_of_power_score": round(balance_of_power_score, 3),
        "framework_compliance": round((realpolitik_score + balance_of_power_score) / 2, 3)
    }

def analyze_chessboard_framework(agent_type: str, event_data: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """Analyze through Brzezinski's grand chessboard framework"""

    agent_info = CHESSBOARD_AGENTS.get(agent_type, {})
    characteristics = agent_info.get("characteristics", [])

    random.seed(sum(ord(c) for c in event_data.get("event", "default")))

    char_scores = {char: random.uniform(0.55, 0.90) for char in characteristics}
    geostrategic_score = random.uniform(0.65, 0.93)
    eurasian_centrality_score = random.uniform(0.60, 0.88)

    context_note = f"\n\n🔗 Building on Previous Context:\n{previous_context[:300]}..." if previous_context else ""

    return {
        "agent": agent_info.get("name"),
        "framework": agent_info.get("framework"),
        "focus_area": agent_info.get("focus"),
        "analysis": f"""
{agent_info.get('name')} Geostrategic Analysis
{context_note}

♟️ Brzezinski Grand Chessboard Framework: {geostrategic_score:.1%} compliance

Strategic Characteristics:
{chr(10).join(f"  • {char.replace('_', ' ').title()}: {score:.1%} relevance" for char, score in char_scores.items())}

Eurasian Pivotal Analysis:
  ✓ Geographic centrality to global power distribution
  ✓ Resource concentration and transit importance
  ✓ Multi-power competition and influence projection

Geostrategic Importance: {geostrategic_score:.1%}
Eurasian Centrality: {eurasian_centrality_score:.1%}

"Eurasia is the chessboard on which the struggle for global primacy continues" - Zbigniew Brzezinski
        """.strip(),
        "geostrategic_score": round(geostrategic_score, 3),
        "characteristic_analysis": char_scores,
        "eurasian_centrality_score": round(eurasian_centrality_score, 3),
        "framework_compliance": round((geostrategic_score + eurasian_centrality_score) / 2, 3)
    }

def execute_agent(parameters: Dict[str, Any], inputs: Dict[str, Any]) -> Dict[str, Any]:
    """Main geopolitics agent execution"""
    try:
        agent_type = parameters.get("agent_type", "russia_geography")
        event = parameters.get("event", "Current geopolitical situation")
        framework = parameters.get("framework", "auto")  # auto, geography, world_order, chessboard

        event_data = {"event": event}

        # Extract mediated context
        mediated_context = None
        if inputs and isinstance(inputs, dict):
            mediated_context = inputs.get("mediated_analysis")

        # Auto-detect framework if needed
        if framework == "auto":
            if agent_type in GEOGRAPHY_AGENTS:
                framework = "geography"
            elif agent_type in WORLD_ORDER_AGENTS:
                framework = "world_order"
            elif agent_type in CHESSBOARD_AGENTS:
                framework = "chessboard"
            else:
                framework = "geography"  # default

        # Route to appropriate framework analyzer
        if framework == "geography":
            result = analyze_geography_framework(agent_type, event_data, mediated_context)
        elif framework == "world_order":
            result = analyze_world_order_framework(agent_type, event_data, mediated_context)
        elif framework == "chessboard":
            result = analyze_chessboard_framework(agent_type, event_data, mediated_context)
        else:
            return {"success": False, "error": f"Unknown framework: {framework}"}

        response = {
            "success": True,
            "agent_type": agent_type,
            "framework": framework,
            "event_analyzed": event,
            **result
        }

        if mediated_context:
            response["used_previous_context"] = True

        return response

    except Exception as e:
        return {"success": False, "error": str(e), "agent": "geopolitics"}

def main():
    parser = argparse.ArgumentParser(description='Geopolitics Agent - Book-Based Analysis')
    parser.add_argument('--parameters', type=str, required=True)
    parser.add_argument('--inputs', type=str, required=True)
    args = parser.parse_args()

    try:
        result = execute_agent(json.loads(args.parameters), json.loads(args.inputs))
        print(json.dumps(result, indent=2))
        sys.exit(0)
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "agent": "geopolitics"}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
