"""
Strategic Imperatives Analysis Module
Shared tools for calculating strategic imperatives driven by geographic constraints
Based on Tim Marshall's framework that geography creates unavoidable strategic imperatives
"""

from typing import Dict, List, Any, Optional
import json
from datetime import datetime

def calculate_strategic_imperatives(region: str, geographic_analysis: List[Dict]) -> Dict[str, Any]:
    """
    Calculate strategic imperatives based on geographic constraints

    Core thesis: Geographic constraints create unavoidable strategic imperatives
    that nations must pursue regardless of leadership or ideology
    """

    imperatives_framework = {
        "russia": {
            "primary_imperatives": [
                {
                    "imperative": "maintain_buffer_zones_in_eastern_europe",
                    "geographic_driver": "north_european_plain_vulnerability",
                    "marshall_justification": "flat_invasion_routes_require_defensive_depth",
                    "urgency": "critical",
                    "historical_continuity": "napoleon_to_nato"
                },
                {
                    "imperative": "secure_year_round_warm_water_ports",
                    "geographic_driver": "ice_bound_ports_limitation",
                    "marshall_justification": "ice_free_ports_required_for_naval_power",
                    "urgency": "critical",
                    "historical_continuity": "peter_great_to_present"
                }
            ],
            "secondary_imperatives": [
                {
                    "imperative": "control_agricultural_heartland",
                    "geographic_driver": "limited_arable_land_concentration",
                    "marshall_justification": "food_security_requires_ukraine_control",
                    "urgency": "high"
                },
                {
                    "imperative": "develop_arctic_transport_corridors",
                    "geographic_driver": "vast_territory_challenge",
                    "marshall_justification": "territorial_size_requires_transport_control",
                    "urgency": "medium"
                }
            ]
        },

        "china": {
            "primary_imperatives": [
                {
                    "imperative": "create_maritime_security_bubble",
                    "geographic_driver": "coastal_vulnerability_history",
                    "marshall_justification": "maritime_invasions_require_defensive_layers",
                    "urgency": "critical",
                    "historical_continuity": "century_of_humiliation_to_present"
                },
                {
                    "imperative": "maintain_national_unity_through_infrastructure",
                    "geographic_driver": "geographic_unity_challenges",
                    "marshall_justification": "mountains_rivers_separate_require_connection",
                    "urgency": "critical",
                    "historical_continuity": "silk_road_to_belt_and_road"
                }
            ],
            "secondary_imperatives": [
                {
                    "imperative": "secure_energy_import_routes",
                    "geographic_driver": "resource_distribution_imbalance",
                    "marshall_justification": "energy_far_from_population_requires_security",
                    "urgency": "high"
                },
                {
                    "imperative": "protect_agricultural_heartlands",
                    "geographic_driver": "limited_arable_land_pressure",
                    "marshall_justification": "feeding_population_requires_food_security",
                    "urgency": "high"
                }
            ]
        },

        "usa": {
            "primary_imperatives": [
                {
                    "imperative": "maintain_global_power_projection_capability",
                    "geographic_driver": "ocean_buffer_security",
                    "marshall_justification": "secure_borders_enable_global_engagement",
                    "urgency": "critical",
                    "historical_continuity": "monroe_doctrine_to_present"
                },
                {
                    "imperative": "exercise_global_food_system_influence",
                    "geographic_driver": "agricultural_abundance",
                    "marshall_justification": "food_surplus Creates_export_power",
                    "urgency": "high",
                    "historical_continuity": "agricultural_exports_to_food_diplomacy"
                }
            ],
            "secondary_imperatives": [
                {
                    "imperative": "leverage_energy_independence",
                    "geographic_driver": "domestic_resource_availability",
                    "marshall_justification": "energy_independence Creates_economic_leverage",
                    "urgency": "medium"
                }
            ]
        },

        "middle_east": {
            "primary_imperatives": [
                {
                    "imperative": "maintain_oil_price_stability",
                    "geographic_driver": "oil_resource_geography",
                    "marshall_justification": "oil_dependence_requires_price_control",
                    "urgency": "critical",
                    "historical_continuity": "oil_discoveries_to_opec_wars"
                },
                {
                    "imperative": "secure_water_resource_control",
                    "geographic_driver": "arid_climate_water_scarcity",
                    "marshall_justification": "water_scarcity_creates_zero_sum_competition",
                    "urgency": "critical",
                    "historical_continuity": "ancient_water_wars_to_present"
                }
            ],
            "secondary_imperatives": [
                {
                    "imperative": "maintain_regime_survival",
                    "geographic_driver": "colonial_border_instability",
                    "marshall_justification": "artificial_states_create_internal_threats",
                    "urgency": "high"
                }
            ]
        },

        "europe": {
            "primary_imperatives": [
                {
                    "imperative": "maintain_collective_security_systems",
                    "geographic_driver": "flat_plain_invasion_vulnerability",
                    "marshall_justification": "mutual_vulnerability_requires_alliance",
                    "urgency": "critical",
                    "historical_continuity": "concert_of_europe_to_nato"
                },
                {
                    "imperative": "balance_cooperation_and_competition",
                    "geographic_driver": "geographic_proximity_interaction",
                    "marshall_justification": "close_neighbors_force_constant_interaction",
                    "urgency": "critical",
                    "historical_continuity": "holy_roman_empire_to_eu"
                }
            ],
            "secondary_imperatives": [
                {
                    "imperative": "preserve_internal_trade_connectivity",
                    "geographic_driver": "navigable_river_networks",
                    "marshall_justification": "rivers_enable_economic_integration",
                    "urgency": "medium"
                }
            ]
        }
    }

    # Get region-specific imperatives
    regional_imperatives = imperatives_framework.get(region.lower(), {})

    # Analyze current events to determine active imperatives
    active_imperatives = identify_active_imperatives(
        geographic_analysis, regional_imperatives
    )

    # Calculate urgency levels based on current events
    urgency_assessment = assess_imperative_urgency(
        active_imperatives, geographic_analysis
    )

    return {
        "region": region,
        "primary_imperatives": active_imperatives.get("primary", []),
        "secondary_imperatives": active_imperatives.get("secondary", []),
        "urgency_assessment": urgency_assessment,
        "geographic_drivers": extract_geographic_drivers(active_imperatives),
        "historical_continuity": assess_historical_continuity(active_imperatives)
    }

def identify_active_imperatives(geographic_analysis: List[Dict], regional_imperatives: Dict) -> Dict:
    """
    Identify which strategic imperatives are active based on current events
    """

    active_imperatives = {
        "primary": [],
        "secondary": []
    }

    # Count geographic determinant frequency
    determinant_counts = {}
    for analysis in geographic_analysis:
        for determinant in analysis.get("geographic_determinants", []):
            determinant_counts[determinant] = determinant_counts.get(determinant, 0) + 1

    # Map determinants to imperatives
    determinant_to_imperative_map = {
        "north_european_plain_vulnerability": "maintain_buffer_zones_in_eastern_europe",
        "warm_water_port_obsession": "secure_year_round_warm_water_ports",
        "coastal_vulnerability": "create_maritime_security_bubble",
        "unity_challenge": "maintain_national_unity_through_infrastructure",
        "oil_resource_curse": "maintain_oil_price_stability",
        "water_scarcity": "secure_water_resource_control",
        "flat_plains": "maintain_collective_security_systems",
        "geographic_proximity": "balance_cooperation_and_competition"
    }

    # Activate imperatives based on determinant frequency
    for determinant, count in determinant_counts.items():
        if count > 0 and determinant in determinant_to_imperative_map:
            imperative_name = determinant_to_imperative_map[determinant]

            # Find imperative in regional framework
            for imperative in regional_imperatives.get("primary_imperatives", []):
                if imperative["imperative"] == imperative_name:
                    active_imperatives["primary"].append(imperative)
                    break

            for imperative in regional_imperatives.get("secondary_imperatives", []):
                if imperative["imperative"] == imperative_name:
                    active_imperatives["secondary"].append(imperative)
                    break

    return active_imperatives

def assess_imperative_urgency(active_imperatives: Dict, geographic_analysis: List[Dict]) -> Dict:
    """
    Assess urgency levels for active strategic imperatives
    """

    urgency_levels = {}

    # Base urgency from framework
    for imperative in active_imperatives.get("primary", []):
        imperative_name = imperative["imperative"]
        base_urgency = imperative.get("urgency", "medium")
        urgency_levels[imperative_name] = base_urgency

    # Adjust urgency based on current events
    event_frequency = len(geographic_analysis)
    if event_frequency >= 5:
        # High event frequency increases urgency
        for imperative_name in urgency_levels:
            if urgency_levels[imperative_name] == "medium":
                urgency_levels[imperative_name] = "high"
            elif urgency_levels[imperative_name] == "high":
                urgency_levels[imperative_name] = "critical"

    return urgency_levels

def extract_geographic_drivers(active_imperatives: Dict) -> List[str]:
    """
    Extract geographic drivers for active strategic imperatives
    """

    drivers = set()

    for imperative in active_imperatives.get("primary", []):
        drivers.add(imperative.get("geographic_driver", "unknown"))

    for imperative in active_imperatives.get("secondary", []):
        drivers.add(imperative.get("geographic_driver", "unknown"))

    return list(drivers)

def assess_historical_continuity(active_imperatives: Dict) -> Dict:
    """
    Assess historical continuity of active strategic imperatives
    """

    continuity_assessment = {}

    for imperative in active_imperatives.get("primary", []):
        imperative_name = imperative["imperative"]
        historical_continuity = imperative.get("historical_continuity", "unknown")

        continuity_assessment[imperative_name] = {
            "historical_pattern": historical_continuity,
            "continuity_strength": "strong" if historical_continuity != "unknown" else "weak",
            "determinism_evidence": "geographic_constraint_drives_continuity"
        }

    return continuity_assessment

def compare_regional_imperatives(region1: str, region2: str) -> Dict:
    """
    Compare strategic imperatives between two regions
    """

    # Get strategic imperatives for both regions
    imperatives1 = calculate_strategic_imperatives(region1, [])
    imperatives2 = calculate_strategic_imperatives(region2, [])

    # Extract imperative themes
    themes1 = [imp["imperative"] for imp in imperatives1.get("primary_imperatives", [])]
    themes2 = [imp["imperative"] for imp in imperatives2.get("primary_imperatives", [])]

    # Find common themes
    common_themes = set()
    for theme1 in themes1:
        for theme2 in themes2:
            if (theme1 in theme2) or (theme2 in theme1) or ("security" in theme1 and "security" in theme2):
                common_themes.add(f"{theme1}/{theme2}")

    comparison = {
        "region1": region1,
        "region2": region2,
        "region1_imperatives": themes1,
        "region2_imperatives": themes2,
        "common_themes": list(common_themes),
        "geographic_driver_similarity": calculate_driver_similarity(imperatives1, imperatives2)
    }

    return comparison

def calculate_driver_similarity(imperatives1: Dict, imperatives2: Dict) -> float:
    """
    Calculate similarity between geographic drivers of two regions
    """

    drivers1 = set(extract_geographic_drivers({
        "primary": imperatives1.get("primary_imperatives", []),
        "secondary": imperatives1.get("secondary_imperatives", [])
    }))

    drivers2 = set(extract_geographic_drivers({
        "primary": imperatives2.get("primary_imperatives", []),
        "secondary": imperatives2.get("secondary_imperatives", [])
    }))

    if not drivers1 and not drivers2:
        return 1.0  # Both empty means identical

    if not drivers1 or not drivers2:
        return 0.0  # One empty means no similarity

    intersection = len(drivers1.intersection(drivers2))
    union = len(drivers1.union(drivers2))

    similarity = intersection / union if union > 0 else 0.0

    return round(similarity, 3)

def predict_imperative_evolution(region: str, timeframe: str = "medium") -> Dict:
    """
    Predict how strategic imperatives will evolve based on geographic constraints
    """

    evolution_predictions = {
        "russia": {
            "short_term": "buffer_zone_maintenance_intensifies",
            "medium_term": "arctic_development_accelerates",
            "long_term": "warm_water_port_competition_increases"
        },
        "china": {
            "short_term": "maritime_security_expansion_continues",
            "medium_term": "infrastructure_connectivity_deepens",
            "long_term": "global_energy_network_establishment"
        },
        "middle_east": {
            "short_term": "oil_price_management_intensifies",
            "medium_term": "water_cooperation_or_conflict_cycles",
            "long_term": "economic_diversification_urgent"
        }
    }

    predictions = evolution_predictions.get(region.lower(), {})
    predicted_evolution = predictions.get(timeframe, "imperative_continuation_expected")

    return {
        "region": region,
        "timeframe": timeframe,
        "predicted_evolution": predicted_evolution,
        "confidence": 0.8,
        "geographic_basis": "constraints_create_imperative_persistence"
    }

def validate_imperative_compliance(analysis_result: Dict) -> Dict:
    """
    Validate that strategic imperatives meet Marshall's geographic determinism requirements
    """

    strategic_imperatives = analysis_result.get("strategic_imperatives", {})
    primary_imperatives = strategic_imperatives.get("primary_imperatives", [])

    # Check compliance criteria
    compliance_checks = {
        "sufficient_primary_imperatives": len(primary_imperatives) >= 2,
        "geographic_drivers_identified": bool(strategic_imperatives.get("geographic_drivers")),
        "historical_continuity_shown": bool(strategic_imperatives.get("historical_continuity")),
        "urgency_levels_assigned": bool(strategic_imperatives.get("urgency_assessment"))
    }

    # Calculate overall compliance score
    compliance_score = sum(compliance_checks.values()) / len(compliance_checks)

    validation = {
        "compliance_score": round(compliance_score, 3),
        "meets_marshall_requirements": compliance_score >= 0.75,
        "compliance_checks": compliance_checks,
        "recommendations": generate_compliance_recommendations(compliance_checks)
    }

    return validation

def generate_compliance_recommendations(compliance_checks: Dict) -> List[str]:
    """
    Generate recommendations to improve Marshall compliance
    """

    recommendations = []

    if not compliance_checks.get("sufficient_primary_imperatives", False):
        recommendations.append("Increase primary strategic imperatives to minimum 2")

    if not compliance_checks.get("geographic_drivers_identified", False):
        recommendations.append("Clearly identify geographic drivers for each imperative")

    if not compliance_checks.get("historical_continuity_shown", False):
        recommendations.append("Demonstrate historical continuity of imperatives")

    if not compliance_checks.get("urgency_levels_assigned", False):
        recommendations.append("Assign appropriate urgency levels to imperatives")

    return recommendations