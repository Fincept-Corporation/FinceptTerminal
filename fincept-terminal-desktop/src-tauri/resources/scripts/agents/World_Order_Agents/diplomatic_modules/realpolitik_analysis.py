"""
REALPOLITIK ANALYSIS MODULE - Henry Kissinger Framework

===== DATA SOURCES REQUIRED =====
# INPUT:
#   - geopolitical_events
#   - power_distribution_data
#   - national_interest_calculations
#   - strategic_considerations
#
# OUTPUT:
#   - Realpolitik assessments
#   - Power-based recommendations
#   - Interest calculations over ideology
#   - Pragmatic diplomatic solutions
#
# PARAMETERS:
#   - analysis_type="power_based"
#   - framework="kissinger_realpolitik"
#   - ideological_vs_power_balance="power_priority"
"""

from typing import Dict, List, Any, Tuple
import json

class RealpolitikAnalyzer:
    """
    REALPOLITIK ANALYSIS MODULE - Henry Kissinger Framework

    Core Principle: Power calculations override ideological preferences
    - National interest defined in terms of power, not values
    - Pragmatic solutions over moral considerations
    - Balance of power considerations drive policy
    - Historical experience guides current decisions
    """

    def __init__(self):
        self.power_factors = {
            'military_capability': 0.30,
            'economic_strength': 0.25,
            'geographic_position': 0.15,
            'diplomatic_influence': 0.15,
            'technological_advantage': 0.10,
            'resource_control': 0.05
        }

        self.realpolitik_principles = {
            'power_primacy': 'Power calculations take precedence over ideology',
            'national_interest': 'Actions based on concrete national interests',
            'balance_maintenance': 'Prevent hegemonic dominance',
            'pragmatic_solutions': 'Practical over moral considerations',
            'historical_guidance': 'Use historical patterns for current decisions'
        }

    def analyze_power_dynamics(self, event: Dict[str, Any]) -> Dict[str, Any]:
        """
        Analyze event through pure power calculation lens

        Kissinger Realpolitik Approach:
        1. Assess power distribution between actors
        2. Calculate national interest implications
        3. Identify balance of power considerations
        4. Generate pragmatic, power-based recommendations
        """

        actors = event.get('actors', [])
        description = event.get('description', '')

        # Calculate power scores for each actor
        power_scores = self._calculate_power_scores(actors)

        # Analyze power distribution
        power_analysis = self._analyze_power_distribution(power_scores)

        # Assess balance of power implications
        balance_assessment = self._assess_balance_implications(power_scores, description)

        # Generate realpolitik recommendations
        recommendations = self._generate_realpolitik_recommendations(
            power_analysis, balance_assessment, event
        )

        return {
            'power_scores': power_scores,
            'power_distribution': power_analysis,
            'balance_assessment': balance_assessment,
            'realpolitik_recommendations': recommendations,
            'ideology_vs_power': 'Power calculation prioritized',
            'framework_compliance': self._calculate_realpolitik_compliance(power_analysis, balance_assessment)
        }

    def _calculate_power_scores(self, actors: List[str]) -> Dict[str, float]:
        """Calculate relative power scores for actors"""

        # Power ratings based on general knowledge (simplified)
        base_power_ratings = {
            'usa': 95.0, 'united states': 95.0, 'america': 95.0,
            'china': 85.0, 'prc': 85.0,
            'russia': 75.0, 'soviet union': 70.0,
            'eu': 80.0, 'european union': 80.0,
            'india': 65.0,
            'japan': 60.0,
            'uk': 55.0, 'britain': 55.0,
            'france': 50.0,
            'germany': 50.0,
            'iran': 40.0,
            'saudi arabia': 35.0,
            'israel': 40.0,
            'turkey': 35.0,
            'brazil': 35.0,
            'nigeria': 25.0,
            'pakistan': 30.0,
            'south korea': 35.0,
            'canada': 30.0,
            'australia': 25.0
        }

        power_scores = {}
        for actor in actors:
            actor_lower = actor.lower()
            # Find matching power rating
            power_score = base_power_ratings.get(actor_lower, 20.0)  # Default for unknown actors
            power_scores[actor] = power_score

        return power_scores

    def _analyze_power_distribution(self, power_scores: Dict[str, float]) -> Dict[str, Any]:
        """Analyze distribution of power among actors"""

        if not power_scores:
            return {'analysis': 'No actors to analyze', 'distribution_type': 'none'}

        sorted_powers = sorted(power_scores.items(), key=lambda x: x[1], reverse=True)
        total_power = sum(power_scores.values())

        # Calculate power concentration
        top_actor_power = sorted_powers[0][1] if sorted_powers else 0
        power_concentration = (top_actor_power / total_power) * 100 if total_power > 0 else 0

        # Determine distribution type
        if power_concentration > 60:
            distribution_type = 'hegemonic_dominance'
        elif power_concentration > 40:
            distribution_type = 'unipolar_tendencies'
        elif len(sorted_powers) >= 2 and sorted_powers[0][1] - sorted_powers[1][1] < 20:
            distribution_type = 'bipolar_balance'
        elif len(sorted_powers) >= 3:
            distribution_type = 'multipolar_distribution'
        else:
            distribution_type = 'balanced_distribution'

        return {
            'sorted_powers': sorted_powers,
            'total_power': total_power,
            'power_concentration': power_concentration,
            'distribution_type': distribution_type,
            'dominant_actor': sorted_powers[0][0] if sorted_powers else None,
            'power_gaps': self._calculate_power_gaps(sorted_powers)
        }

    def _calculate_power_gaps(self, sorted_powers: List[Tuple[str, float]]) -> List[Dict[str, Any]]:
        """Calculate power gaps between consecutive actors"""

        gaps = []
        for i in range(len(sorted_powers) - 1):
            current_actor, current_power = sorted_powers[i]
            next_actor, next_power = sorted_powers[i + 1]
            gap = current_power - next_power
            gap_percentage = (gap / current_power) * 100 if current_power > 0 else 0

            gaps.append({
                'stronger_actor': current_actor,
                'weaker_actor': next_actor,
                'power_gap': gap,
                'gap_percentage': gap_percentage,
                'significance': 'large' if gap > 30 else 'medium' if gap > 15 else 'small'
            })

        return gaps

    def _assess_balance_implications(self, power_scores: Dict[str, float], description: str) -> Dict[str, Any]:
        """Assess implications for balance of power"""

        power_analysis = self._analyze_power_distribution(power_scores)
        distribution_type = power_analysis['distribution_type']

        # Balance of power implications
        implications = {
            'hegemony_threat': 'high' if 'hegemonic' in distribution_type else 'medium' if 'unipolar' in distribution_type else 'low',
            'stability_assessment': self._assess_stability(power_analysis),
            'coalition_needs': self._assess_coalition_needs(power_analysis),
            'power_transition_risk': self._assess_power_transition_risk(power_analysis)
        }

        # Event-specific implications
        event_implications = []
        if 'conflict' in description.lower():
            event_implications.append('Power calculations critical in conflict resolution')
        if 'cooperation' in description.lower():
            event_implications.append('Power balance affects cooperation prospects')
        if 'alliance' in description.lower():
            event_implications.append('Alliance formations shift power distribution')

        implications['event_specific'] = event_implications

        return implications

    def _assess_stability(self, power_analysis: Dict[str, Any]) -> str:
        """Assess stability based on power distribution"""

        distribution_type = power_analysis['distribution_type']
        power_concentration = power_analysis['power_concentration']

        if distribution_type == 'hegemonic_dominance':
            return 'unstable_due_to_resistance'
        elif distribution_type == 'unipolar_tendencies':
            return 'moderately_stable_with_challenges'
        elif distribution_type == 'bipolar_balance':
            return 'stable_with_rivalry_tensions'
        elif distribution_type == 'multipolar_distribution':
            return 'complex_stability_with_shifting_alliances'
        else:
            return 'relatively_stable'

    def _assess_coalition_needs(self, power_analysis: Dict[str, Any]) -> List[str]:
        """Assess coalition-building needs"""

        needs = []
        distribution_type = power_analysis['distribution_type']
        dominant_actor = power_analysis.get('dominant_actor')

        if 'hegemonic' in distribution_type:
            needs.append(f'Anti-{dominant_actor} coalition needed')
        elif 'unipolar' in distribution_type:
            needs.append('Balancing coalition formation')
        elif 'multipolar' in distribution_type:
            needs.append('Flexible coalition arrangements')

        return needs

    def _assess_power_transition_risk(self, power_analysis: Dict[str, Any]) -> str:
        """Assess risk of power transition conflicts"""

        power_gaps = power_analysis.get('power_gaps', [])

        large_gaps = [gap for gap in power_gaps if gap['significance'] == 'large']
        medium_gaps = [gap for gap in power_gaps if gap['significance'] == 'medium']

        if len(large_gaps) > 0:
            return 'low_risk_clear_hierarchy'
        elif len(medium_gaps) > 2:
            return 'high_risk_multiple_contenders'
        elif len(medium_gaps) > 0:
            return 'medium_risk_competition_likely'
        else:
            return 'low_risk_stable_hierarchy'

    def _generate_realpolitik_recommendations(self, power_analysis: Dict[str, Any],
                                           balance_assessment: Dict[str, Any],
                                           event: Dict[str, Any]) -> List[str]:
        """Generate recommendations based on pure power calculations"""

        recommendations = []
        distribution_type = power_analysis['distribution_type']
        dominance_threat = balance_assessment['hegemony_threat']

        # Power-based recommendations
        if dominance_threat == 'high':
            recommendations.append('Form counterbalancing coalition against dominant power')
            recommendations.append('Support rising powers to maintain equilibrium')

        if distribution_type == 'multipolar_distribution':
            recommendations.append('Maintain diplomatic flexibility with multiple partners')
            recommendations.append('Balance relationships among competing powers')

        if dominance_threat == 'low':
            recommendations.append('Preserve current power distribution')
            recommendations.append('Use diplomatic engagement to maintain stability')

        # Event-specific recommendations
        description = event.get('description', '').lower()
        if 'conflict' in description:
            recommendations.append('Prioritize power interests over moral considerations')
        if 'cooperation' in description:
            recommendations.append('Assess cooperation benefits for power enhancement')

        # Classic realpolitik principles
        recommendations.extend([
            'Focus on concrete national interests rather than ideological goals',
            'Use balance of power logic as primary analytical tool',
            'Maintain strategic flexibility in diplomatic relationships'
        ])

        return recommendations

    def _calculate_realpolitik_compliance(self, power_analysis: Dict[str, Any],
                                        balance_assessment: Dict[str, Any]) -> float:
        """Calculate compliance with Kissinger's realpolitik framework"""

        score = 0.0

        # Power Analysis (40%)
        if power_analysis.get('distribution_type') != 'none':
            score += 40

        # Balance Assessment (30%)
        if balance_assessment.get('hegemony_threat'):
            score += 30

        # Pragmatic Focus (20%)
        # This would be assessed based on recommendations
        score += 20

        # Historical Continuity (10%)
        # This would be assessed based on use of historical patterns
        score += 10

        return score

# Example usage and testing
if __name__ == "__main__":
    analyzer = RealpolitikAnalyzer()

    # Test event typical of power politics
    test_event = {
        'type': 'great_power_competition',
        'actors': ['USA', 'China', 'Russia'],
        'location': 'Indo-Pacific',
        'description': 'Competition for influence and strategic positioning'
    }

    result = analyzer.analyze_power_dynamics(test_event)

    print("=== REALPOLITIK ANALYSIS ===")
    print(f"Power Distribution: {result['power_distribution']['distribution_type']}")
    print(f"Hegemony Threat: {result['balance_assessment']['hegemony_threat']}")
    print(f"Stability Assessment: {result['balance_assessment']['stability_assessment']}")
    print("\nPOWER SCORES:")
    for actor, score in result['power_scores'].items():
        print(f"  {actor}: {score}")

    print("\nREALPOLITIK RECOMMENDATIONS:")
    for rec in result['realpolitik_recommendations']:
        print(f"  - {rec}")

    print(f"\nFramework Compliance: {result['framework_compliance']:.1f}%")