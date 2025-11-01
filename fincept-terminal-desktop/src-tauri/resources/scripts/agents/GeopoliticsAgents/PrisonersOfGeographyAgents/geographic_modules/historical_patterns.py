"""
Historical Patterns Analysis Module
Shared tools for identifying recurring geographic determinism patterns
Based on Tim Marshall's framework that geography creates recurring historical behaviors
"""

from typing import Dict, List, Any, Optional
import json
from datetime import datetime

def identify_historical_patterns(region: str, event_data: Dict) -> Dict[str, Any]:
    """
    Identify historical patterns that demonstrate geographic determinism

    Core thesis: Geographic constraints create recurring patterns of behavior
    throughout history, demonstrating that geography determines rather than influences
    geopolitical actions.
    """

    pattern_libraries = {
        "russia": {
            "expansion_patterns": {
                "pattern": "territorial_expansion_follows_geographic_insecurity",
                "historical_examples": [
                    "moscow_expansion_for_defensive_depth",
                    "warm_water_port_acquisition_series",
                    "siberia_expansion_for_resources_security"
                ],
                "geographic_driver": "north_european_plain_vulnerability",
                "determinism_evidence": "consistent_expansion_regardless_of_regime_type"
            },
            "security_patterns": {
                "pattern": "perpetual_search_for_buffer_zones",
                "historical_examples": [
                    "poland_lithuania_control",
                    "eastern_europe_sphere_of_influence",
                    "modern_near_abroad_policy"
                ],
                "geographic_driver": "flat_invasion_routes",
                "determinism_evidence": "buffer_zone_creation across centuries"
            },
            "economic_patterns": {
                "pattern": "resource_control_drives_foreign_policy",
                "historical_examples": [
                    "grain_export_control",
                    "energy_pipeline_politics",
                    "arctic_resource_access"
                ],
                "geographic_driver": "resource_distribution_challenges",
                "determinism_evidence": "consistent_resource_focus regardless_of_ideology"
            }
        },

        "china": {
            "unity_patterns": {
                "pattern": "infrastructure_development_maintains_unity",
                "historical_examples": [
                    "grand_canal_construction",
                    "great_wall_building",
                    "modern_belt_and_road_initiative"
                ],
                "geographic_driver": "geographic_barriers_to_cohesion",
                "determinism_evidence": "consistent_infrastructure_investment across_dynasties"
            },
            "maritime_patterns": {
                "pattern": "coastal_vulnerability_drives_maritime_policy",
                "historical_examples": [
                    "treasure_fleet_period",
                    "maritime_silk_road",
                    "modern_south_china_sea_policy"
                ],
                "geographic_driver": "historical_maritime_invasions",
                "determinism_evidence": "recurring_maritime_security_focus"
            },
            "economic_patterns": {
                "pattern": "agricultural_heartland_protection",
                "historical_examples": [
                    "yellow_river_valley_control",
                    "food_storage_systems",
                    "modern_agricultural_security"
                ],
                "geographic_driver": "limited_arable_land",
                "determinism_evidence": "consistent_food_security_priority"
            }
        },

        "middle_east": {
            "resource_patterns": {
                "pattern": "oil_geography_determines_political_structures",
                "historical_examples": [
                    "pre_oil_tribal_systems",
                    "oil_state_formation",
                    "modern_rentier_states"
                ],
                "geographic_driver": "oil_resource_distribution",
                "determinism_evidence": "political_structures_follow_oil_geography"
            },
            "conflict_patterns": {
                "pattern": "water_scarcity_drives_recurring_conflicts",
                "historical_examples": [
                    "ancient_mesopotamian_water_wars",
                    "ottoman_water_control",
                    "modern_river_disputes"
                ],
                "geographic_driver": "arid_climate_water_scarcity",
                "determinism_evidence": "water_conflicts_across_millennia"
            },
            "stability_patterns": {
                "pattern": "colonial_borders_create_perpetual_instability",
                "historical_examples": [
                    "sykes_picot_boundaries",
                    "post-colonial_state_failures",
                    "modern_separatist_movements"
                ],
                "geographic_driver": "artificial_colonial_boundaries",
                "determinism_evidence": "consistent_instability_along_colonial_lines"
            }
        },

        "europe": {
            "conflict_patterns": {
                "pattern": "geographic_proximity_drives_perpetual_competition",
                "historical_examples": [
                    "hundred_years_war",
                    "napoleonic_wars",
                    "world_wars"
                ],
                "geographic_driver": "flat_plains_and_close_proximity",
                "determinism_evidence": "recurring_conflict_regardless_of_regime_types"
            },
            "cooperation_patterns": {
                "pattern": "geographic_constraints_drive_integration_attempts",
                "historical_examples": [
                    "holy_roman_empire",
                    "concert_of_europe",
                    "european_union"
                ],
                "geographic_driver": "mutual_vulnerability_and_economic_interdependence",
                "determinism_evidence": "recurring_integration_attempts"
            },
            "maritime_patterns": {
                "pattern": "natural_harbors_enable_global_power",
                "historical_examples": [
                    "age_of_exploration",
                    "colonial_empires",
                    "modern_trade_dominance"
                ],
                "geographic_driver": "numerous_natural_harbors",
                "determinism_evidence": "consistent_maritime_power_development"
            }
        },

        "usa": {
            "expansion_patterns": {
                "pattern": "geographic_security_enables_expansion",
                "historical_examples": [
                    "westward_territorial_expansion",
                    "monroe_doctrine_implementation",
                    "global_power_projection"
                ],
                "geographic_driver": "ocean_buffers_and_security",
                "determinism_evidence": "consistent_expansion_when_secure"
            },
            "economic_patterns": {
                "pattern": "resource_abundance_enables_wealth_creation",
                "historical_examples": [
                    "agricultural_export_development",
                    "industrial_revolution_acceleration",
                    "energy_independence_achievement"
                ],
                "geographic_driver": "abundant_natural_resources",
                "determinism_evidence": "consistent_economic_advantage_from_resources"
            }
        }
    }

    # Get relevant patterns for the region
    regional_patterns = pattern_libraries.get(region.lower(), {})

    # Analyze current event against historical patterns
    pattern_matches = []
    for pattern_type, pattern_data in regional_patterns.items():
        match_score = match_event_to_pattern(event_data, pattern_data)
        if match_score > 0.5:
            pattern_matches.append({
                "pattern_type": pattern_type,
                "pattern_name": pattern_data["pattern"],
                "match_score": match_score,
                "geographic_driver": pattern_data["geographic_driver"],
                "historical_examples": pattern_data["historical_examples"]
            })

    return {
        "region": region,
        "event": event_data.get("title", "Unknown"),
        "identified_patterns": pattern_matches,
        "determinism_evidence": extract_determinism_evidence(pattern_matches),
        "pattern_confidence": calculate_pattern_confidence(pattern_matches)
    }

def match_event_to_pattern(event_data: Dict, pattern_data: Dict) -> float:
    """
    Calculate how well an event matches a historical pattern
    """

    event_text = (event_data.get("title", "") + " " +
                  event_data.get("description", "")).lower()

    pattern_keywords = extract_pattern_keywords(pattern_data)

    # Count keyword matches
    matches = sum(1 for keyword in pattern_keywords if keyword in event_text)

    # Calculate match score
    if len(pattern_keywords) == 0:
        return 0.0

    match_score = matches / len(pattern_keywords)

    # Boost score if geographic driver is explicitly mentioned
    geographic_driver = pattern_data.get("geographic_driver", "").lower()
    if geographic_driver and geographic_driver in event_text:
        match_score += 0.2

    return min(match_score, 1.0)

def extract_pattern_keywords(pattern_data: Dict) -> List[str]:
    """
    Extract keywords that define a historical pattern
    """

    keywords = []

    # Extract from pattern name
    pattern_name = pattern_data.get("pattern", "").lower()
    keywords.extend(pattern_name.split())

    # Extract from historical examples
    examples = pattern_data.get("historical_examples", [])
    for example in examples:
        keywords.extend(example.lower().split())

    # Extract from geographic driver
    driver = pattern_data.get("geographic_driver", "").lower()
    keywords.extend(driver.split())

    # Remove common words and deduplicate
    stop_words = {"the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for", "of", "with", "by"}
    keywords = [word for word in keywords if word not in stop_words and len(word) > 2]

    return list(set(keywords))

def extract_determinism_evidence(pattern_matches: List[Dict]) -> List[str]:
    """
    Extract evidence of geographic determinism from pattern matches
    """

    evidence = []

    for match in pattern_matches:
        if match["match_score"] > 0.7:
            evidence.append(f"Strong_match_to_{match['pattern_type']}_pattern")
            evidence.append(f"Geographic_driver_{match['geographic_driver']}")

            # Add historical continuity evidence
            examples = match.get("historical_examples", [])
            if examples:
                evidence.append(f"Historical_continuity_across_{len(examples)}_periods")

    return evidence

def calculate_pattern_confidence(pattern_matches: List[Dict]) -> float:
    """
    Calculate overall confidence in pattern identification
    """

    if not pattern_matches:
        return 0.0

    # Weight by match scores
    total_weight = sum(match["match_score"] for match in pattern_matches)
    max_possible = len(pattern_matches)

    confidence = total_weight / max_possible if max_possible > 0 else 0.0

    return round(confidence, 3)

def compare_historical_patterns(region1: str, region2: str) -> Dict:
    """
    Compare historical patterns between two regions to identify
    common geographic determinism themes
    """

    pattern_libraries = {
        "russia": ["expansion_for_security", "buffer_zone_creation", "resource_control"],
        "china": ["infrastructure_unity", "maritime_security", "agricultural_protection"],
        "middle_east": ["resource_curse", "water_conflicts", "colonial_instability"],
        "europe": ["perpetual_competition", "integration_attempts", "maritime_power"],
        "usa": ["security_enabled_expansion", "resource_abundance", "power_projection"]
    }

    patterns1 = pattern_libraries.get(region1.lower(), [])
    patterns2 = pattern_libraries.get(region2.lower(), [])

    # Find common patterns
    common_patterns = set(patterns1) & set(patterns2)
    unique_patterns1 = set(patterns1) - set(patterns2)
    unique_patterns2 = set(patterns2) - set(patterns1)

    comparison = {
        "region1": region1,
        "region2": region2,
        "common_patterns": list(common_patterns),
        "unique_to_region1": list(unique_patterns1),
        "unique_to_region2": list(unique_patterns2),
        "pattern_similarity": len(common_patterns) / max(len(patterns1), len(patterns2), 1)
    }

    return comparison

def predict_pattern_continuation(region: str, pattern_type: str, timeframe: str = "medium") -> Dict:
    """
    Predict continuation of historical patterns based on geographic determinism
    """

    pattern_predictions = {
        "russia": {
            "expansion_for_security": {
                "short_term": "buffer_zone_maintenance",
                "medium_term": "strategic_depth_preservation",
                "long_term": "perimeter_security_ensurance"
            },
            "resource_control": {
                "short_term": "energy_leverage_maintenance",
                "medium_term": "export_route_security",
                "long_term": "resource_sphere_dominance"
            }
        },
        "china": {
            "infrastructure_unity": {
                "short_term": "connectivity_projects",
                "medium_term": "economic_integration",
                "long_term": "comprehensive_unity_achievement"
            },
            "maritime_security": {
                "short_term": "coastal_defense",
                "medium_term": "regional_maritime_control",
                "long_term": "global_maritime_presence"
            }
        },
        "middle_east": {
            "water_conflicts": {
                "short_term": "resource_control",
                "medium_term": "cooperation_or_conflict_cycles",
                "long_term": "technological_solutions_or_escalation"
            },
            "colonial_instability": {
                "short_term": "regime_survival_focus",
                "medium_term": "state_weakening_or_strengthening",
                "long_term": "boundary_redrawing_or_stabilization"
            }
        }
    }

    predictions = pattern_predictions.get(region.lower(), {}).get(pattern_type, {})
    predicted_behavior = predictions.get(timeframe, "pattern_continuation_expected")

    return {
        "region": region,
        "pattern_type": pattern_type,
        "timeframe": timeframe,
        "predicted_behavior": predicted_behavior,
        "confidence": 0.8,  # High confidence in historical pattern continuation
        "determinism_basis": "geographic_constraints_create_recurring_behaviors"
    }

def validate_historical_determinism(region: str, event_data: Dict) -> Dict:
    """
    Validate that an event demonstrates geographic determinism
    by showing historical pattern continuity
    """

    # Identify matching historical patterns
    pattern_analysis = identify_historical_patterns(region, event_data)

    # Assess determinism evidence strength
    pattern_matches = pattern_analysis.get("identified_patterns", [])
    high_confidence_matches = [p for p in pattern_matches if p["match_score"] > 0.7]

    # Calculate determinism validation score
    if high_confidence_matches:
        determinism_score = 0.8 + (0.2 * (len(high_confidence_matches) / len(pattern_matches)))
    else:
        determinism_score = 0.0

    validation = {
        "region": region,
        "event": event_data.get("title", "Unknown"),
        "determinism_validated": determinism_score > 0.7,
        "validation_score": round(determinism_score, 3),
        "supporting_patterns": len(high_confidence_matches),
        "historical_continuity": pattern_analysis.get("determinism_evidence", []),
        "marshall_compliance": determinism_score >= 0.7
    }

    return validation