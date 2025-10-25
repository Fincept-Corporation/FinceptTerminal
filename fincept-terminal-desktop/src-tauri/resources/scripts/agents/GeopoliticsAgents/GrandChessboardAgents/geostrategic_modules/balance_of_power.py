"""
BALANCE OF POWER CALCULATOR
Geostrategic Module for Grand Chessboard Agents

This module implements Brzezinski's balance of power analysis framework,
calculating and predicting changes in global and regional power distributions
across the Eurasian chessboard.
"""

import logging
from typing import Dict, List, Any, Tuple, Optional
from dataclasses import dataclass
from enum import Enum
import math

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class PowerType(Enum):
    """Types of power in geopolitical analysis"""
    MILITARY = "military"
    ECONOMIC = "economic"
    TECHNOLOGICAL = "technological"
    DIPLOMATIC = "diplomatic"
    CULTURAL = "cultural"

class PowerLevel(Enum):
    """Relative power levels"""
    DOMINANT = "dominant"      # > 0.7 share
    LEADING = "leading"         # 0.4 - 0.7 share
    MAJOR = "major"            # 0.2 - 0.4 share
    REGIONAL = "regional"       # 0.1 - 0.2 share
    MINOR = "minor"            # < 0.1 share

@dataclass
class PowerMetrics:
    """Metrics for a specific power's capabilities"""
    military_capability: float  # 0-1 scale
    economic_strength: float    # 0-1 scale
    technological_advantage: float  # 0-1 scale
    diplomatic_influence: float  # 0-1 scale
    cultural_reach: float       # 0-1 scale
    overall_power: float        # Composite score

@dataclass
class PowerBalance:
    """Power balance assessment for a region or globally"""
    powers: Dict[str, PowerMetrics]
    distribution: Dict[str, float]  # Power shares
    concentration_index: float       # HHI for power concentration
    stability_level: str            # stable, competitive, volatile
    hegemony_risk: str             # low, medium, high

class BalanceOfPowerCalculator:
    """
    Calculates and predicts changes in global and regional power balances
    following Brzezinski's balance of power framework
    """

    def __init__(self):
        """Initialize the balance of power calculator"""
        self.module_id = "balance_of_power_calculator"

        # Power weights based on Brzezinski's framework
        self.power_weights = {
            PowerType.MILITARY: 0.30,      # Military power is primary
            PowerType.ECONOMIC: 0.25,      # Economic foundation
            PowerType.TECHNOLOGICAL: 0.20,  # Modern warfare factor
            PowerType.DIPLOMATIC: 0.15,     # Alliance influence
            PowerType.CULTURAL: 0.10        # Soft power
        }

        # Great powers per Brzezinski
        self.great_powers = [
            "united_states", "china", "russia",
            "japan", "germany", "france", "india"
        ]

        # Regional power groupings
        self.regional_powers = {
            "europe": ["germany", "france", "united_kingdom", "italy"],
            "east_asia": ["china", "japan", "south_korea"],
            "south_asia": ["india", "pakistan"],
            "middle_east": ["turkey", "iran", "saudi_arabia", "israel"],
            "central_asia": ["kazakhstan", "uzbekistan"],
            "russia_near_abroad": ["russia"]
        }

    def calculate_global_power_balance(
        self,
        military_data: Dict[str, Any],
        economic_data: Dict[str, Any],
        diplomatic_data: Dict[str, Any],
        technological_data: Optional[Dict[str, Any]] = None
    ) -> PowerBalance:
        """
        Calculate current global power distribution

        Args:
            military_data: Military capabilities by country
            economic_data: Economic indicators by country
            diplomatic_data: Diplomatic influence data
            technological_data: Technology capabilities data

        Returns:
            PowerBalance object with current distribution and stability assessment
        """
        try:
            logger.info("Calculating global power balance")

            power_metrics = {}

            # Calculate power metrics for each great power
            for power in self.great_powers:
                power_metrics[power] = self._calculate_power_metrics(
                    power, military_data, economic_data, diplomatic_data, technological_data
                )

            # Calculate power distribution (shares)
            total_power = sum(metrics.overall_power for metrics in power_metrics.values())
            distribution = {
                power: metrics.overall_power / total_power
                for power, metrics in power_metrics.items()
            }

            # Calculate concentration index (Herfindahl-Hirschman Index)
            concentration_index = sum(share ** 2 for share in distribution.values())

            # Assess stability level
            stability_level = self._assess_balance_stability(distribution, concentration_index)

            # Assess hegemony risk
            hegemony_risk = self._assess_hegemony_risk(distribution, concentration_index)

            return PowerBalance(
                powers=power_metrics,
                distribution=distribution,
                concentration_index=concentration_index,
                stability_level=stability_level,
                hegemony_risk=hegemony_risk
            )

        except Exception as e:
            logger.error(f"Error calculating global power balance: {str(e)}")
            raise

    def calculate_regional_power_balance(
        self,
        region: str,
        military_data: Dict[str, Any],
        economic_data: Dict[str, Any],
        diplomatic_data: Dict[str, Any]
    ) -> PowerBalance:
        """
        Calculate power balance for specific Eurasian region

        Args:
            region: Region identifier (europe, east_asia, central_asia, etc.)
            military_data: Regional military capabilities
            economic_data: Regional economic data
            diplomatic_data: Regional diplomatic influence

        Returns:
            PowerBalance for the specified region
        """
        try:
            logger.info(f"Calculating power balance for region: {region}")

            # Get relevant powers for this region
            regional_powers = self._get_regional_powers(region)
            power_metrics = {}

            # Calculate power metrics for regional powers
            for power in regional_powers:
                power_metrics[power] = self._calculate_power_metrics(
                    power, military_data, economic_data, diplomatic_data
                )

            # Calculate power distribution
            total_power = sum(metrics.overall_power for metrics in power_metrics.values())
            distribution = {
                power: metrics.overall_power / total_power
                for power, metrics in power_metrics.items()
            }

            # Calculate concentration index
            concentration_index = sum(share ** 2 for share in distribution.values())

            # Assess regional stability
            stability_level = self._assess_regional_stability(region, distribution)

            # Assess regional hegemony risk
            hegemony_risk = self._assess_regional_hegemony_risk(distribution, concentration_index)

            return PowerBalance(
                powers=power_metrics,
                distribution=distribution,
                concentration_index=concentration_index,
                stability_level=stability_level,
                hegemony_risk=hegemony_risk
            )

        except Exception as e:
            logger.error(f"Error calculating regional power balance for {region}: {str(e)}")
            raise

    def analyze_power_shifts(
        self,
        current_balance: PowerBalance,
        historical_balance: PowerBalance,
        time_period: str = "year"
    ) -> Dict[str, Any]:
        """
        Analyze power shifts between different time periods

        Args:
            current_balance: Current power balance
            historical_balance: Previous power balance
            time_period: Time period for comparison

        Returns:
            Analysis of power shifts and trends
        """
        try:
            logger.info(f"Analyzing power shifts over {time_period}")

            power_shifts = {}

            # Calculate shifts for each power
            all_powers = set(current_balance.distribution.keys()) | set(historical_balance.distribution.keys())

            for power in all_powers:
                current_share = current_balance.distribution.get(power, 0)
                historical_share = historical_balance.distribution.get(power, 0)

                shift = current_share - historical_share
                shift_percentage = (shift / historical_share * 100) if historical_share > 0 else 0

                power_shifts[power] = {
                    "absolute_change": shift,
                    "percentage_change": shift_percentage,
                    "direction": "gaining" if shift > 0 else "losing" if shift < 0 else "stable",
                    "significance": self._assess_shift_significance(shift, time_period)
                }

            # Identify major power shifts
            major_shifts = {
                power: data for power, data in power_shifts.items()
                if data["significance"] in ["major", "significant"]
            }

            # Analyze balance changes
            stability_change = self._analyze_stability_change(
                current_balance.stability_level,
                historical_balance.stability_level
            )

            concentration_change = current_balance.concentration_index - historical_balance.concentration_index

            return {
                "power_shifts": power_shifts,
                "major_shifts": major_shifts,
                "stability_change": stability_change,
                "concentration_change": concentration_change,
                "trend_analysis": self._analyze_power_trends(power_shifts, time_period),
                "implications": self._analyze_shift_implications(major_shifts, concentration_change)
            }

        except Exception as e:
            logger.error(f"Error analyzing power shifts: {str(e)}")
            raise

    def project_future_balance(
        self,
        current_balance: PowerBalance,
        growth_projections: Dict[str, Dict[str, float]],
        time_horizon: int = 5
    ) -> Dict[str, Any]:
        """
        Project future power balance based on growth projections

        Args:
            current_balance: Current power balance
            growth_projections: Growth rates for different power types by country
            time_horizon: Years to project forward

        Returns:
            Projected future power balance scenarios
        """
        try:
            logger.info(f"Projecting power balance {time_horizon} years forward")

            projected_balances = []

            # Project year by year
            for year in range(1, time_horizon + 1):
                projected_powers = {}

                for power, current_metrics in current_balance.powers.items():
                    # Get growth projections for this power
                    power_growth = growth_projections.get(power, {})

                    # Project each power component
                    projected_military = self._project_power_component(
                        current_metrics.military_capability,
                        power_growth.get("military_growth", 0),
                        year
                    )

                    projected_economic = self._project_power_component(
                        current_metrics.economic_strength,
                        power_growth.get("economic_growth", 0),
                        year
                    )

                    projected_technological = self._project_power_component(
                        current_metrics.technological_advantage,
                        power_growth.get("tech_growth", 0),
                        year
                    )

                    projected_diplomatic = self._project_power_component(
                        current_metrics.diplomatic_influence,
                        power_growth.get("diplomatic_growth", 0),
                        year
                    )

                    # Calculate projected overall power
                    projected_overall = self._calculate_composite_power(
                        projected_military,
                        projected_economic,
                        projected_technological,
                        projected_diplomatic
                    )

                    projected_powers[power] = PowerMetrics(
                        military_capability=projected_military,
                        economic_strength=projected_economic,
                        technological_advantage=projected_technological,
                        diplomatic_influence=projected_diplomatic,
                        cultural_reach=current_metrics.cultural_reach,  # Assume stable
                        overall_power=projected_overall
                    )

                # Calculate projected distribution
                total_power = sum(metrics.overall_power for metrics in projected_powers.values())
                projected_distribution = {
                    power: metrics.overall_power / total_power
                    for power, metrics in projected_powers.items()
                }

                projected_balance = PowerBalance(
                    powers=projected_powers,
                    distribution=projected_distribution,
                    concentration_index=sum(share ** 2 for share in projected_distribution.values()),
                    stability_level=self._assess_balance_stability(projected_distribution),
                    hegemony_risk=self._assess_hegemony_risk(projected_distribution)
                )

                projected_balances.append({
                    "year": year,
                    "balance": projected_balance
                })

            # Analyze trends and critical thresholds
            trend_analysis = self._analyze_projection_trends(projected_balances)
            critical_thresholds = self._identify_critical_thresholds(projected_balances)

            return {
                "projections": projected_balances,
                "trend_analysis": trend_analysis,
                "critical_thresholds": critical_thresholds,
                "scenarios": self._generate_balance_scenarios(projected_balances),
                "recommendations": self._generate_balance_recommendations(projected_balances)
            }

        except Exception as e:
            logger.error(f"Error projecting future balance: {str(e)}")
            raise

    def _calculate_power_metrics(
        self,
        power: str,
        military_data: Dict[str, Any],
        economic_data: Dict[str, Any],
        diplomatic_data: Dict[str, Any],
        technological_data: Optional[Dict[str, Any]] = None
    ) -> PowerMetrics:
        """Calculate comprehensive power metrics for a specific power"""

        # Extract military capability
        military_capability = self._assess_military_capability(power, military_data)

        # Extract economic strength
        economic_strength = self._assess_economic_strength(power, economic_data)

        # Extract technological advantage
        technological_advantage = self._assess_technological_advantage(
            power, technological_data or {}
        )

        # Extract diplomatic influence
        diplomatic_influence = self._assess_diplomatic_influence(power, diplomatic_data)

        # Cultural reach (simplified assessment)
        cultural_reach = self._assess_cultural_reach(power)

        # Calculate composite overall power
        overall_power = self._calculate_composite_power(
            military_capability, economic_strength,
            technological_advantage, diplomatic_influence, cultural_reach
        )

        return PowerMetrics(
            military_capability=military_capability,
            economic_strength=economic_strength,
            technological_advantage=technological_advantage,
            diplomatic_influence=diplomatic_influence,
            cultural_reach=cultural_reach,
            overall_power=overall_power
        )

    def _assess_military_capability(self, power: str, military_data: Dict[str, Any]) -> float:
        """Assess military capability on 0-1 scale"""

        # Get military data for this power
        power_military = military_data.get(power, {})

        # Key factors (normalized to 0-1)
        personnel_factor = self._normalize_value(power_military.get("personnel", 0), 0, 3000000)
        equipment_factor = self._normalize_value(power_military.get("equipment_score", 0), 0, 100)
        technology_factor = self._normalize_value(power_military.get("technology_level", 0), 0, 10)
        projection_factor = self._normalize_value(power_military.get("projection_capability", 0), 0, 10)
        nuclear_factor = 1.0 if power_military.get("nuclear_capability", False) else 0.5

        # Weighted average (military-focused weighting)
        military_capability = (
            personnel_factor * 0.25 +
            equipment_factor * 0.25 +
            technology_factor * 0.20 +
            projection_factor * 0.20 +
            nuclear_factor * 0.10
        )

        return min(military_capability, 1.0)

    def _assess_economic_strength(self, power: str, economic_data: Dict[str, Any]) -> float:
        """Assess economic strength on 0-1 scale"""

        power_economic = economic_data.get(power, {})

        # Key economic factors
        gdp_factor = self._normalize_log_value(power_economic.get("gdp", 0), 1000000000, 25000000000000)
        growth_factor = self._normalize_value(power_economic.get("growth_rate", 0), -5, 10)
        trade_factor = self._normalize_log_value(power_economic.get("trade_volume", 0), 100000000, 10000000000)
        technology_factor = self._normalize_log_value(power_economic.get("tech_investment", 0), 1000000000, 1000000000000)
        energy_factor = self._normalize_value(power_economic.get("energy_independence", 0), 0, 1)

        economic_strength = (
            gdp_factor * 0.35 +
            growth_factor * 0.20 +
            trade_factor * 0.20 +
            technology_factor * 0.15 +
            energy_factor * 0.10
        )

        return min(economic_strength, 1.0)

    def _assess_technological_advantage(self, power: str, tech_data: Dict[str, Any]) -> float:
        """Assess technological advantage on 0-1 scale"""

        power_tech = tech_data.get(power, {})

        # Technology indicators
        innovation_factor = self._normalize_value(power_tech.get("innovation_index", 0), 0, 100)
        research_factor = self._normalize_log_value(power_tech.get("rd_spending", 0), 10000000000, 800000000000)
        digital_factor = self._normalize_value(power_tech.get("digital_infrastructure", 0), 0, 100)
        ai_factor = self._normalize_value(power_tech.get("ai_capability", 0), 0, 10)
        space_factor = self._normalize_value(power_tech.get("space_capability", 0), 0, 10)

        technological_advantage = (
            innovation_factor * 0.25 +
            research_factor * 0.25 +
            digital_factor * 0.20 +
            ai_factor * 0.15 +
            space_factor * 0.15
        )

        return min(technological_advantage, 1.0)

    def _assess_diplomatic_influence(self, power: str, diplomatic_data: Dict[str, Any]) -> float:
        """Assess diplomatic influence on 0-1 scale"""

        power_diplomatic = diplomatic_data.get(power, {})

        # Diplomatic indicators
        un_influence = self._normalize_value(power_diplomatic.get("un_influence", 0), 0, 10)
        alliance_strength = self._normalize_value(power_diplomatic.get("alliance_strength", 0), 0, 10)
        soft_power = self._normalize_value(power_diplomatic.get("soft_power_index", 0), 0, 100)
        mediation_role = self._normalize_value(power_diplomatic.get("mediation_role", 0), 0, 10)
        institutional_influence = self._normalize_value(power_diplomatic.get("institutional_power", 0), 0, 10)

        diplomatic_influence = (
            un_influence * 0.20 +
            alliance_strength * 0.30 +
            soft_power * 0.25 +
            mediation_role * 0.10 +
            institutional_influence * 0.15
        )

        return min(diplomatic_influence, 1.0)

    def _assess_cultural_reach(self, power: str) -> float:
        """Assess cultural reach on 0-1 scale"""

        # Cultural influence mapping (simplified)
        cultural_scores = {
            "united_states": 0.9,    # Global cultural dominance
            "china": 0.6,            # Growing cultural influence
            "france": 0.5,           # Traditional cultural power
            "united_kingdom": 0.5,   # Historical cultural reach
            "japan": 0.4,            # Regional cultural influence
            "india": 0.4,            # Growing cultural influence
            "germany": 0.3,          # Economic cultural influence
            "russia": 0.3,           # Declining but present
        }

        return cultural_scores.get(power, 0.2)  # Default baseline

    def _calculate_composite_power(
        self,
        military: float,
        economic: float,
        technological: float,
        diplomatic: float,
        cultural: float = 0.0
    ) -> float:
        """Calculate composite overall power score"""

        return (
            military * self.power_weights[PowerType.MILITARY] +
            economic * self.power_weights[PowerType.ECONOMIC] +
            technological * self.power_weights[PowerType.TECHNOLOGICAL] +
            diplomatic * self.power_weights[PowerType.DIPLOMATIC] +
            cultural * self.power_weights[PowerType.CULTURAL]
        )

    def _assess_balance_stability(
        self,
        distribution: Dict[str, float],
        concentration_index: float
    ) -> str:
        """Assess the stability of the current power balance"""

        if concentration_index > 0.5:
            return "volatile"  # One power dominates
        elif concentration_index > 0.3:
            return "competitive"  # Clear competition among top powers
        elif concentration_index > 0.2:
            return "stable"  # Balanced distribution
        else:
            return "fragmented"  # Many powers, no clear leader

    def _assess_hegemony_risk(
        self,
        distribution: Dict[str, float],
        concentration_index: float
    ) -> str:
        """Assess the risk of a single power achieving hegemony"""

        # Find the largest power share
        max_share = max(distribution.values()) if distribution else 0

        if max_share > 0.6:
            return "high"
        elif max_share > 0.4:
            return "medium"
        else:
            return "low"

    def _normalize_value(self, value: float, min_val: float, max_val: float) -> float:
        """Normalize a value to 0-1 scale"""
        if max_val <= min_val:
            return 0.0
        return max(0.0, min(1.0, (value - min_val) / (max_val - min_val)))

    def _normalize_log_value(self, value: float, min_val: float, max_val: float) -> float:
        """Normalize a value using log scale for better distribution"""
        if value <= 0:
            return 0.0
        log_value = math.log10(value)
        log_min = math.log10(min_val)
        log_max = math.log10(max_val)
        return self._normalize_value(log_value, log_min, log_max)

    def _get_regional_powers(self, region: str) -> List[str]:
        """Get list of relevant powers for a specific region"""
        # Include both regional powers and global powers with interests in region
        regional = self.regional_powers.get(region, [])

        # Add great powers that have interests in the region
        interested_powers = []
        if region in ["europe", "middle_east", "russia_near_abroad"]:
            interested_powers.extend(["united_states", "russia"])
        if region in ["east_asia", "south_asia", "central_asia"]:
            interested_powers.extend(["united_states", "china"])
        if region == "europe":
            interested_powers.extend(["germany", "france", "united_kingdom"])

        return list(set(regional + interested_powers))

    # Additional helper methods for advanced analysis
    def _assess_shift_significance(self, shift: float, time_period: str) -> str:
        """Assess the significance of a power shift"""

        if time_period == "year":
            if abs(shift) > 0.05:
                return "major"
            elif abs(shift) > 0.02:
                return "significant"
            elif abs(shift) > 0.01:
                return "moderate"
            else:
                return "minor"
        elif time_period == "decade":
            if abs(shift) > 0.15:
                return "major"
            elif abs(shift) > 0.08:
                return "significant"
            elif abs(shift) > 0.04:
                return "moderate"
            else:
                return "minor"
        else:
            return "unknown"

    def _analyze_power_trends(
        self,
        power_shifts: Dict[str, Dict[str, Any]],
        time_period: str
    ) -> Dict[str, Any]:
        """Analyze overall power trends from shift data"""

        gaining_powers = [p for p, data in power_shifts.items() if data["direction"] == "gaining"]
        losing_powers = [p for p, data in power_shifts.items() if data["direction"] == "losing"]

        return {
            "trend_direction": "fragmentation" if len(gaining_powers) > len(losing_powers) else "concentration",
            "rising_powers": gaining_powers,
            "declining_powers": losing_powers,
            "major_trends": [p for p in gaining_powers + losing_powers
                           if power_shifts[p]["significance"] in ["major", "significant"]]
        }

    def _analyze_shift_implications(
        self,
        major_shifts: Dict[str, Dict[str, Any]],
        concentration_change: float
    ) -> List[str]:
        """Analyze strategic implications of major power shifts"""

        implications = []

        if concentration_change > 0.02:
            implications.append("Increasing power concentration suggests rising hegemony risk")
        elif concentration_change < -0.02:
            implications.append("Decreasing concentration suggests more balanced power distribution")

        for power, shift_data in major_shifts.items():
            if shift_data["direction"] == "gaining":
                implications.append(f"{power.title()}'s rising power requires strategic attention")
            else:
                implications.append(f"{power.title()}'s declining power may create strategic opportunities")

        return implications

    # Additional implementation methods for advanced features
    def _project_power_component(
        self,
        current_value: float,
        growth_rate: float,
        years: int
    ) -> float:
        """Project power component growth over time period"""
        return current_value * ((1 + growth_rate / 100) ** years)

    def _analyze_projection_trends(self, projections: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Analyze trends in power balance projections"""

        if not projections:
            return {}

        first_year = projections[0]
        last_year = projections[-1]

        trend_changes = {}
        for power in first_year["balance"].distribution:
            initial_share = first_year["balance"].distribution.get(power, 0)
            final_share = last_year["balance"].distribution.get(power, 0)
            change = final_share - initial_share

            trend_changes[power] = {
                "initial_share": initial_share,
                "final_share": final_share,
                "total_change": change,
                "direction": "gaining" if change > 0 else "losing" if change < 0 else "stable"
            }

        return {
            "power_trends": trend_changes,
            "dominant_trend": self._identify_dominant_trend(trend_changes),
            "stability_evolution": self._track_stability_evolution(projections)
        }

    def _identify_dominant_trend(self, trend_changes: Dict[str, Dict[str, Any]]) -> str:
        """Identify the dominant trend in power evolution"""

        gaining_count = sum(1 for data in trend_changes.values() if data["direction"] == "gaining")
        losing_count = sum(1 for data in trend_changes.values() if data["direction"] == "losing")

        if gaining_count > losing_count:
            return "power_diffusion"
        elif losing_count > gaining_count:
            return "power_concentration"
        else:
            return "relative_stability"

    def _track_stability_evolution(self, projections: List[Dict[str, Any]]) -> List[str]:
        """Track how stability evolves over projections"""

        stability_evolution = []
        for projection in projections:
            stability_evolution.append(projection["balance"].stability_level)

        return stability_evolution

    # Additional helper methods would be implemented for complete functionality
    def _assess_regional_stability(self, region: str, distribution: Dict[str, float]) -> str:
        """Assess stability for a specific region"""
        # Simplified implementation - would be more sophisticated in production
        concentration = max(distribution.values()) if distribution else 0

        if concentration > 0.6:
            return "volatile"
        elif concentration > 0.4:
            return "competitive"
        else:
            return "stable"

    def _assess_regional_hegemony_risk(
        self,
        distribution: Dict[str, float],
        concentration_index: float
    ) -> str:
        """Assess regional hegemony risk"""
        max_share = max(distribution.values()) if distribution else 0

        if max_share > 0.7:
            return "high"
        elif max_share > 0.5:
            return "medium"
        else:
            return "low"

    def _analyze_stability_change(self, current: str, historical: str) -> Dict[str, str]:
        """Analyze change in stability level"""
        if current == historical:
            return {"status": "unchanged", "level": current}

        stability_levels = {"stable": 1, "competitive": 2, "volatile": 3, "fragmented": 4}

        current_level = stability_levels.get(current, 2)
        historical_level = stability_levels.get(historical, 2)

        if current_level > historical_level:
            return {"status": "degraded", "from": historical, "to": current}
        else:
            return {"status": "improved", "from": historical, "to": current}

    def _identify_critical_thresholds(self, projections: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Identify when critical power thresholds are crossed"""
        thresholds = []

        for i, projection in enumerate(projections):
            year = projection["year"]
            balance = projection["balance"]

            # Check for hegemony emergence
            max_share = max(balance.distribution.values()) if balance.distribution else 0
            if max_share > 0.6:
                thresholds.append({
                    "year": year,
                    "type": "hegemony_emergence",
                    "dominant_power": max(balance.distribution, key=balance.distribution.get),
                    "power_share": max_share
                })

            # Check for balance fragmentation
            if balance.concentration_index < 0.15:
                thresholds.append({
                    "year": year,
                    "type": "fragmentation",
                    "concentration_index": balance.concentration_index
                })

        return thresholds

    def _generate_balance_scenarios(self, projections: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Generate scenarios based on power balance projections"""

        if not projections:
            return {}

        final_projection = projections[-1]
        final_balance = final_projection["balance"]

        # Determine scenario type
        max_share = max(final_balance.distribution.values()) if final_balance.distribution else 0

        if max_share > 0.5:
            scenario_type = "emerging_hegemon"
        elif final_balance.concentration_index > 0.3:
            scenario_type = "bipolar_competition"
        elif final_balance.concentration_index < 0.15:
            scenario_type = "multipolar_fragmentation"
        else:
            scenario_type = "balanced_multipolar"

        return {
            "scenario_type": scenario_type,
            "dominant_features": self._describe_scenario_features(scenario_type, final_balance),
            "key_players": self._identify_key_players(final_balance),
            "strategic_implications": self._analyze_scenario_implications(scenario_type)
        }

    def _describe_scenario_features(self, scenario_type: str, balance: PowerBalance) -> List[str]:
        """Describe key features of a power balance scenario"""

        feature_map = {
            "emerging_hegemon": [
                "Single dominant power emerging",
                "High concentration of power",
                "Potential regional hegemony",
                "Increased instability risks"
            ],
            "bipolar_competition": [
                "Two dominant powers competing",
                "Clear alliance structures forming",
                "Stable but tense competition",
                "Risk of proxy conflicts"
            ],
            "multipolar_fragmentation": [
                "Multiple competing powers",
                "Diffused power distribution",
                "Complex alliance patterns",
                "High negotiation complexity"
            ],
            "balanced_multipolar": [
                "Multiple significant powers",
                "Relatively balanced distribution",
                "Stable competitive environment",
                "Institution-based order"
            ]
        }

        return feature_map.get(scenario_type, ["Unknown scenario type"])

    def _identify_key_players(self, balance: PowerBalance) -> List[str]:
        """Identify key players in the power balance"""

        # Top 3 powers by share
        sorted_powers = sorted(
            balance.distribution.items(),
            key=lambda x: x[1],
            reverse=True
        )

        return [power for power, share in sorted_powers[:3]]

    def _analyze_scenario_implications(self, scenario_type: str) -> List[str]:
        """Analyze strategic implications of power balance scenarios"""

        implication_map = {
            "emerging_hegemon": [
                "Need for containment strategies",
                "Alliance strengthening imperative",
                "Risk of regional destabilization",
                "Balance of power restoration required"
            ],
            "bipolar_competition": [
                "Strategic stability through mutual deterrence",
                "Alliance management critical",
                "Risk of escalation but manageable",
                "Clear strategic choices available"
            ],
            "multipolar_fragmentation": [
                "Complex diplomatic management required",
                "Institution building essential",
                "Risk of regional conflicts",
                "Flexible strategic approaches needed"
            ],
            "balanced_multipolar": [
                "Institutional cooperation viable",
                "Collective security possible",
                "Economic interdependence increases",
                "Stable long-term prospects"
            ]
        }

        return implication_map.get(scenario_type, ["Strategic implications unknown"])

    def _generate_balance_recommendations(self, projections: List[Dict[str, Any]]) -> List[str]:
        """Generate strategic recommendations based on balance projections"""

        if not projections:
            return []

        recommendations = []
        final_balance = projections[-1]["balance"]

        # Hegemony prevention recommendations
        max_share = max(final_balance.distribution.values()) if final_balance.distribution else 0
        if max_share > 0.5:
            dominant_power = max(final_balance.distribution, key=final_balance.distribution.get)
            recommendations.append(f"Develop containment strategies for {dominant_power.title()}")
            recommendations.append("Strengthen alliance networks to balance emerging power")

        # Stability recommendations
        if final_balance.stability_level in ["volatile", "competitive"]:
            recommendations.append("Enhance diplomatic engagement to reduce tensions")
            recommendations.append("Strengthen international institutions for conflict prevention")

        # Power transition recommendations
        trends = self._analyze_projection_trends(projections)
        if trends.get("dominant_trend") == "power_concentration":
            recommendations.append("Monitor for power transitions and hegemonic ambitions")

        return recommendations

# Example usage and testing
if __name__ == "__main__":
    # Initialize calculator
    calculator = BalanceOfPowerCalculator()

    # Sample data for testing
    military_data = {
        "united_states": {"personnel": 1400000, "equipment_score": 95, "technology_level": 9, "projection_capability": 10, "nuclear_capability": True},
        "china": {"personnel": 2000000, "equipment_score": 85, "technology_level": 8, "projection_capability": 6, "nuclear_capability": True},
        "russia": {"personnel": 900000, "equipment_score": 70, "technology_level": 6, "projection_capability": 7, "nuclear_capability": True}
    }

    economic_data = {
        "united_states": {"gdp": 25000000000000, "growth_rate": 2.1, "trade_volume": 5000000000000, "tech_investment": 600000000000, "energy_independence": 0.8},
        "china": {"gdp": 18000000000000, "growth_rate": 4.8, "trade_volume": 6000000000000, "tech_investment": 400000000000, "energy_independence": 0.7},
        "russia": {"gdp": 2000000000000, "growth_rate": 1.5, "trade_volume": 800000000000, "tech_investment": 50000000000, "energy_independence": 0.9}
    }

    diplomatic_data = {
        "united_states": {"un_influence": 9, "alliance_strength": 9, "soft_power_index": 85, "mediation_role": 7, "institutional_power": 9},
        "china": {"un_influence": 7, "alliance_strength": 6, "soft_power_index": 60, "mediation_role": 5, "institutional_power": 7},
        "russia": {"un_influence": 5, "alliance_strength": 5, "soft_power_index": 40, "mediation_role": 6, "institutional_power": 6}
    }

    # Calculate global power balance
    balance = calculator.calculate_global_power_balance(military_data, economic_data, diplomatic_data)

    print("Global Power Balance Analysis:")
    print(f"Power Distribution: {balance.distribution}")
    print(f"Concentration Index: {balance.concentration_index:.3f}")
    print(f"Stability Level: {balance.stability_level}")
    print(f"Hegemony Risk: {balance.hegemony_risk}")

    # Show individual power metrics
    for power, metrics in balance.powers.items():
        print(f"\n{power.title()} Power Metrics:")
        print(f"  Military: {metrics.military_capability:.2f}")
        print(f"  Economic: {metrics.economic_strength:.2f}")
        print(f"  Overall: {metrics.overall_power:.2f}")