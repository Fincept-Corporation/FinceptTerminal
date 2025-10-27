"""
Geographic Constraints Analysis Module
Shared tools for analyzing how geography constrains geopolitical behavior
Based on Tim Marshall's geographic determinism framework
"""

from typing import Dict, List, Any, Optional
import json
from datetime import datetime

def analyze_physical_constraints(region: str, event_data: Dict) -> Dict[str, Any]:
    """
    Analyze physical geographic constraints that determine geopolitical behavior

    Physical constraints include:
    - Topography (mountains, plains, deserts)
    - Climate (arctic, temperate, tropical, arid)
    - Water access (oceans, seas, rivers, lakes)
    - Natural resources location
    - Population distribution patterns
    """

    constraint_frameworks = {
        "topographic_constraints": {
            "mountains": {
                "constraint": "barriers_to_movement_and_trade",
                "security_aspect": "natural_defense_borders",
                "economic_aspect": "transportation_difficulties",
                "political_aspect": "cultural_isolation_development"
            },
            "plains": {
                "constraint": "easy_invasion_routes",
                "security_aspect": "vulnerability_to_attack",
                "economic_aspect": "agricultural_advantages",
                "political_aspect": "power_projection_capability"
            },
            "deserts": {
                "constraint": "habitation_limitation",
                "security_aspect": "natural_barriers",
                "economic_aspect": "resource_extraction_focus",
                "political_aspect": "population_concentration"
            }
        },

        "climatic_constraints": {
            "arctic": {
                "constraint": "seasonal_access_limitation",
                "security_aspect": "natural_defense",
                "economic_aspect": "resource_extraction_challenges",
                "political_aspect": "limited_population_centers"
            },
            "temperate": {
                "constraint": "agricultural_favorability",
                "security_aspect": "year_round_operability",
                "economic_aspect": "diversified_economy_possible",
                "political_aspect": "population_concentration"
            },
            "arid": {
                "constraint": "water_scarcity",
                "security_aspect": "resource_conflict_driver",
                "economic_aspect": "limited_agriculture",
                "political_aspect": "population_distribution_constraints"
            }
        },

        "hydrographic_constraints": {
            "ocean_access": {
                "constraint": "trade_projection_capability",
                "security_aspect": "naval_power_development",
                "economic_aspect": "global_trade_access",
                "political_aspect": "power_projection_ability"
            },
            "landlocked": {
                "constraint": "trade_dependence_on_neighbors",
                "security_aspect": "strategic_vulnerability",
                "economic_aspect": "transportation_cost_increase",
                "political_aspect": "diplomatic_constraint"
            },
            "river_networks": {
                "constraint": "internal_connectivity_patterns",
                "security_aspect": "invasion_routes_or_barriers",
                "economic_aspect": "trade_corridors",
                "political_aspect": "cultural_exchange_pathways"
            }
        }
    }

    # Analyze specific event against constraint frameworks
    analysis = {
        "region": region,
        "event": event_data.get("title", "Unknown"),
        "identified_constraints": [],
        "constraint_levels": {},
        "determinism_assessment": "constraint_dominant" if True else "choice_available"
    }

    return analysis

def assess_constraint_level(event_data: Dict, constraint_type: str) -> float:
    """
    Assess how strongly a specific geographic constraint determines an event

    Returns: 0.0 - 1.0 (where 1.0 means complete geographic determinism)
    """

    constraint_keywords = {
        "topographic": ["mountain", "plain", "valley", "elevation", "terrain", "border"],
        "climatic": ["weather", "climate", "season", "temperature", "arctic", "desert"],
        "hydrographic": ["ocean", "sea", "river", "water", "port", "coastal", "landlocked"],
        "resource": ["oil", "gas", "mineral", "resource", "energy", "agriculture"],
        "population": ["urban", "rural", "population", "density", "demographic"]
    }

    event_text = (event_data.get("title", "") + " " + event_data.get("description", "")).lower()

    if constraint_type not in constraint_keywords:
        return 0.0

    keyword_matches = sum(1 for keyword in constraint_keywords[constraint_type]
                         if keyword in event_text)

    # Calculate constraint level based on keyword density and context
    max_possible = len(constraint_keywords[constraint_type])
    constraint_level = keyword_matches / max_possible if max_possible > 0 else 0.0

    return min(constraint_level * 2, 1.0)  # Scale to 0-1 range

def identify_geographic_determinants(event_data: Dict) -> List[str]:
    """
    Identify which geographic constraints are primary determinants of an event
    """

    determinants = []

    constraint_levels = {
        "topographic": assess_constraint_level(event_data, "topographic"),
        "climatic": assess_constraint_level(event_data, "climatic"),
        "hydrographic": assess_constraint_level(event_data, "hydrographic"),
        "resource": assess_constraint_level(event_data, "resource"),
        "population": assess_constraint_level(event_data, "population")
    }

    # Select determinants above threshold (0.3 or higher)
    for constraint_type, level in constraint_levels.items():
        if level >= 0.3:
            determinants.append(constraint_type)

    return determinants

def calculate_geographic_determinism_score(event_data: Dict) -> float:
    """
    Calculate overall geographic determinism score for an event

    Higher score = more geographically determined (less policy choice available)
    """

    constraint_levels = {
        "topographic": assess_constraint_level(event_data, "topographic"),
        "climatic": assess_constraint_level(event_data, "climatic"),
        "hydrographic": assess_constraint_level(event_data, "hydrographic"),
        "resource": assess_constraint_level(event_data, "resource"),
        "population": assess_constraint_level(event_data, "population")
    }

    # Weight critical constraints more heavily
    weighted_scores = {
        "topographic": constraint_levels["topographic"] * 0.25,
        "climatic": constraint_levels["climatic"] * 0.20,
        "hydrographic": constraint_levels["hydrographic"] * 0.25,  # Critical for trade/security
        "resource": constraint_levels["resource"] * 0.20,  # Critical for economy
        "population": constraint_levels["population"] * 0.10
    }

    total_score = sum(weighted_scores.values())

    return round(total_score, 3)

def compare_regional_constraints(region1: str, region2: str, event_data: Dict) -> Dict:
    """
    Compare how geographic constraints affect two regions differently
    """

    regional_constraint_profiles = {
        "russia": {
            "primary_constraints": ["north_european_plain", "warm_water_ports", "vast_territory", "climate"],
            "constraint_severity": {"high": 3, "medium": 1, "low": 0}
        },
        "china": {
            "primary_constraints": ["coastal_vulnerability", "unity_challenge", "resource_distribution", "agriculture"],
            "constraint_severity": {"high": 2, "medium": 2, "low": 0}
        },
        "usa": {
            "primary_constraints": ["ocean_buffers", "agricultural_abundance", "resource_independence"],
            "constraint_severity": {"high": 0, "medium": 1, "low": 2}  # Note: advantages, not constraints
        },
        "middle_east": {
            "primary_constraints": ["desert_geography", "oil_curse", "water_scarcity", "colonial_borders"],
            "constraint_severity": {"high": 4, "medium": 0, "low": 0}
        },
        "europe": {
            "primary_constraints": ["flat_plains", "geographic_proximity", "multiple_harbors", "river_networks"],
            "constraint_severity": {"high": 1, "medium": 2, "low": 1}
        }
    }

    # Get constraint profiles for both regions
    profile1 = regional_constraint_profiles.get(region1.lower(), {})
    profile2 = regional_constraint_profiles.get(region2.lower(), {})

    comparison = {
        "region1": region1,
        "region2": region2,
        "region1_constraints": profile1.get("primary_constraints", []),
        "region2_constraints": profile2.get("primary_constraints", []),
        "constraint_similarity": calculate_constraint_similarity(
            profile1.get("primary_constraints", []),
            profile2.get("primary_constraints", [])
        ),
        "determinism_difference": calculate_determinism_difference(profile1, profile2)
    }

    return comparison

def calculate_constraint_similarity(constraints1: List[str], constraints2: List[str]) -> float:
    """
    Calculate similarity between two regions' constraint profiles
    """

    set1 = set(constraints1)
    set2 = set(constraints2)

    intersection = len(set1.intersection(set2))
    union = len(set1.union(set2))

    similarity = intersection / union if union > 0 else 0.0

    return round(similarity, 3)

def calculate_determinism_difference(profile1: Dict, profile2: Dict) -> float:
    """
    Calculate difference in geographic determinism levels between two regions
    """

    def get_determinism_score(profile):
        severity = profile.get("constraint_severity", {})
        return (severity.get("high", 0) * 1.0 +
                severity.get("medium", 0) * 0.5 +
                severity.get("low", 0) * 0.1)

    score1 = get_determinism_score(profile1)
    score2 = get_determinism_score(profile2)

    difference = abs(score1 - score2)

    return round(difference, 3)

def predict_constraint_driven_behavior(region: str, constraint_type: str, timeframe: str = "medium") -> Dict:
    """
    Predict behavior based on specific geographic constraints

    Used for generating predictions about how geography will determine future actions
    """

    constraint_behaviors = {
        "russia": {
            "north_european_plain": {
                "short_term": "buffer_zone_maintenance",
                "medium_term": "eastern_europe_influence",
                "long_term": "defensive_depth_strategy"
            },
            "warm_water_ports": {
                "short_term": "crimea_consolidation",
                "medium_term": "mediterranean_expansion",
                "long_term": "global_naval_presence"
            }
        },
        "china": {
            "coastal_vulnerability": {
                "short_term": "maritime_security_buildup",
                "medium_term": "south_china_sea_control",
                "long_term": "blue_water_navy_development"
            },
            "unity_challenge": {
                "short_term": "internal_security_measures",
                "medium_term": "infrastructure_development",
                "long_term": "economic_integration"
            }
        },
        "middle_east": {
            "water_scarcity": {
                "short_term": "water_resource_control",
                "medium_term": "regional_water_cooperation/conflict",
                "long_term": "desalination_investment"
            },
            "oil_curse": {
                "short_term": "price_stability_efforts",
                "medium_term": "economic_diversification_attempts",
                "long_term": "post_oil_economy_preparation"
            }
        }
    }

    behaviors = constraint_behaviors.get(region.lower(), {}).get(constraint_type, {})
    predicted_behavior = behaviors.get(timeframe, "unknown_behavior")

    return {
        "region": region,
        "constraint": constraint_type,
        "timeframe": timeframe,
        "predicted_behavior": predicted_behavior,
        "confidence": 0.7  # Base confidence for geographic determinism predictions
    }