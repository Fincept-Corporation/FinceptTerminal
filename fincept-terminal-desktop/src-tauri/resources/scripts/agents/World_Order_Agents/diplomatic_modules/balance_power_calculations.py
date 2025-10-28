"""
BALANCE OF POWER CALCULATIONS MODULE - Henry Kissinger Framework

===== DATA SOURCES REQUIRED =====
# INPUT:
#   - state_capabilities_data
#   - alliance_formations
#   - power_transition_indicators
#   - regional_distributions
#
# OUTPUT:
#   - Power balance assessments
#   - Hegemony threat levels
#   - Coalition recommendations
#   - Equilibrium maintenance strategies
#
# PARAMETERS:
#   - analysis_type="equilibrium_maintenance"
#   - framework="kissinger_balance_power"
#   - time_horizon="medium_term"
"""

from typing import Dict, List, Any, Tuple
import json
import math

class BalancePowerCalculator:
    """
    BALANCE OF POWER CALCULATIONS MODULE - Henry Kissinger Framework

    Core Principle: International stability requires balanced distribution of power
    - Prevent emergence of hegemons through counterbalancing
    - Maintain equilibrium through diplomatic flexibility
    - Use alliance formations to shift power balance
    - Apply historical lessons from European state system
    """

    def __init__(self):
        self.balance_thresholds = {
            'hegemony_warning': 60.0,  # % of total power that triggers hegemony concern
            'balance_acceptable': 45.0,  # % range considered balanced
            'power_gap_warning': 25.0,  # Power gap that triggers balancing action
        }

        self.historical_precedents = {
            'concert_of_europe': {
                'period': '1815-1914',
                'mechanism': 'Great power cooperation',
                'success_rate': 0.75
            },
            'bismarckian_system': {
                'period': '1871-1890',
                'mechanism': 'Complex alliance networks',
                'success_rate': 0.80
            },
            'cold_war_balance': {
                'period': '1947-1991',
                'mechanism': 'Nuclear deterrence',
                'success_rate': 0.85
            }
        }

    def calculate_power_balance(self, actors: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Calculate comprehensive power balance among actors

        Kissinger Balance of Power Analysis:
        1. Assess relative capabilities of all actors
        2. Identify potential hegemons
        3. Calculate balancing requirements
        4. Recommend coalition strategies
        """

        # Calculate comprehensive power scores
        power_assessments = self._assess_actor_capabilities(actors)

        # Analyze power distribution
        distribution_analysis = self._analyze_power_distribution(power_assessments)

        # Identify hegemony threats
        hegemony_analysis = self._identify_hegemony_threats(power_assessments, distribution_analysis)

        # Calculate balancing requirements
        balancing_needs = self._calculate_balancing_requirements(power_assessments, hegemony_analysis)

        # Generate equilibrium strategies
        equilibrium_strategies = self._generate_equilibrium_strategies(
            power_assessments, hegemony_analysis, balancing_needs
        )

        return {
            'power_assessments': power_assessments,
            'distribution_analysis': distribution_analysis,
            'hegemony_analysis': hegemony_analysis,
            'balancing_needs': balancing_needs,
            'equilibrium_strategies': equilibrium_strategies,
            'historical_precedents': self._apply_historical_precedents(distribution_analysis),
            'stability_assessment': self._assess_overall_stability(distribution_analysis, hegemony_analysis)
        }

    def _assess_actor_capabilities(self, actors: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Assess comprehensive power capabilities of each actor"""

        power_factors = {
            'military': {'weight': 0.35, 'factors': ['personnel', 'equipment', 'technology', 'nuclear']},
            'economic': {'weight': 0.30, 'factors': ['gdp', 'trade', 'technology', 'resources']},
            'political': {'weight': 0.20, 'factors': ['leadership', 'institutions', 'cohesion', 'alliances']},
            'geographic': {'weight': 0.15, 'factors': ['location', 'size', 'resources', 'access']}
        }

        assessments = []

        for actor in actors:
            actor_name = actor.get('name', 'Unknown')
            actor_capabilities = actor.get('capabilities', {})

            category_scores = {}
            total_score = 0.0

            for category, config in power_factors.items():
                category_score = 0.0
                category_data = actor_capabilities.get(category, {})

                # Calculate score for each factor in category
                factor_scores = []
                for factor in config['factors']:
                    factor_value = category_data.get(factor, 0)
                    # Normalize to 0-100 scale
                    normalized_value = min(max(factor_value, 0), 100)
                    factor_scores.append(normalized_value)

                # Category score is average of factors
                if factor_scores:
                    category_score = sum(factor_scores) / len(factor_scores)

                category_scores[category] = category_score
                total_score += category_score * config['weight']

            assessment = {
                'name': actor_name,
                'category_scores': category_scores,
                'total_power_score': total_score,
                'power_rank': None,  # Will be calculated after all actors assessed
                'balancing_needs': {}
            }

            assessments.append(assessment)

        # Rank actors by power
        assessments.sort(key=lambda x: x['total_power_score'], reverse=True)
        for i, assessment in enumerate(assessments):
            assessment['power_rank'] = i + 1

        return assessments

    def _analyze_power_distribution(self, power_assessments: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Analyze overall distribution of power"""

        if not power_assessments:
            return {'distribution_type': 'no_data', 'analysis': 'No power data available'}

        total_power = sum(assessment['total_power_score'] for assessment in power_assessments)
        top_power = power_assessments[0]['total_power_score']
        top_power_percentage = (top_power / total_power) * 100 if total_power > 0 else 0

        # Analyze power gaps
        power_gaps = []
        for i in range(len(power_assessments) - 1):
            current_power = power_assessments[i]['total_power_score']
            next_power = power_assessments[i + 1]['total_power_score']
            gap = current_power - next_power
            gap_percentage = (gap / current_power) * 100 if current_power > 0 else 0

            power_gaps.append({
                'rank_gap': f"{i+1} to {i+2}",
                'power_gap': gap,
                'gap_percentage': gap_percentage,
                'significance': self._assess_gap_significance(gap_percentage)
            })

        # Determine distribution type
        distribution_type = self._classify_distribution_type(
            top_power_percentage, len(power_assessments), power_gaps
        )

        return {
            'distribution_type': distribution_type,
            'total_power': total_power,
            'top_power_percentage': top_power_percentage,
            'power_gaps': power_gaps,
            'concentration_index': self._calculate_concentration_index(power_assessments),
            'balance_quality': self._assess_balance_quality(power_assessments)
        }

    def _assess_gap_significance(self, gap_percentage: float) -> str:
        """Assess significance of power gaps"""
        if gap_percentage > 50:
            return 'very_large'
        elif gap_percentage > 30:
            return 'large'
        elif gap_percentage > 15:
            return 'medium'
        elif gap_percentage > 5:
            return 'small'
        else:
            return 'minimal'

    def _classify_distribution_type(self, top_power_percentage: float,
                                  num_actors: int, power_gaps: List[Dict[str, Any]]) -> str:
        """Classify the type of power distribution"""

        if top_power_percentage > self.balance_thresholds['hegemony_warning']:
            return 'hegemonic_dominance'
        elif top_power_percentage > self.balance_thresholds['balance_acceptable']:
            return 'unipolar_tendencies'
        elif num_actors >= 2 and len(power_gaps) > 0 and power_gaps[0]['gap_percentage'] < 20:
            return 'bipolar_balance'
        elif num_actors >= 3:
            return 'multipolar_distribution'
        else:
            return 'balanced_distribution'

    def _calculate_concentration_index(self, power_assessments: List[Dict[str, Any]]) -> float:
        """Calculate Herfindahl-Hirschman Index for power concentration"""
        if not power_assessments:
            return 0.0

        total_power = sum(assessment['total_power_score'] for assessment in power_assessments)
        if total_power == 0:
            return 0.0

        hhi = 0.0
        for assessment in power_assessments:
            market_share = assessment['total_power_score'] / total_power
            hhi += market_share ** 2

        return hhi * 10000  # Scale for easier interpretation

    def _assess_balance_quality(self, power_assessments: List[Dict[str, Any]]) -> str:
        """Assess overall quality of power balance"""
        if len(power_assessments) < 2:
            return 'insufficient_data'

        distribution_analysis = self._analyze_power_distribution(power_assessments)
        concentration_index = distribution_analysis['concentration_index']

        if concentration_index > 2500:
            return 'poor_balance'
        elif concentration_index > 1500:
            return 'moderate_balance'
        elif concentration_index > 1000:
            return 'good_balance'
        else:
            return 'excellent_balance'

    def _identify_hegemony_threats(self, power_assessments: List[Dict[str, Any]],
                                 distribution_analysis: Dict[str, Any]) -> Dict[str, Any]:
        """Identify potential hegemony threats"""

        hegemony_threats = []

        top_power_percentage = distribution_analysis.get('top_power_percentage', 0)
        distribution_type = distribution_analysis.get('distribution_type', '')

        # Check for hegemonic dominance
        if top_power_percentage > self.balance_thresholds['hegemony_warning']:
            top_actor = power_assessments[0]
            hegemony_threats.append({
                'actor': top_actor['name'],
                'threat_level': 'high',
                'power_percentage': top_power_percentage,
                'balancing_needed': True,
                'recommended_action': 'form_counterbalancing_coalition'
            })

        # Check for unipolar tendencies
        elif top_power_percentage > self.balance_thresholds['balance_acceptable']:
            top_actor = power_assessments[0]
            hegemony_threats.append({
                'actor': top_actor['name'],
                'threat_level': 'medium',
                'power_percentage': top_power_percentage,
                'balancing_needed': True,
                'recommended_action': 'monitor_and_balance'
            })

        # Check for rising powers
        for i, assessment in enumerate(power_assessments[1:], 1):
            if assessment['total_power_score'] > 60:  # Arbitrary threshold for major power
                hegemony_threats.append({
                    'actor': assessment['name'],
                    'threat_level': 'potential',
                    'power_percentage': (assessment['total_power_score'] /
                                       sum(a['total_power_score'] for a in power_assessments)) * 100,
                    'balancing_needed': False,
                    'recommended_action': 'strategic_engagement'
                })

        return {
            'threats_identified': hegemony_threats,
            'overall_threat_level': self._calculate_overall_threat_level(hegemony_threats),
            'balancing_requirements': self._assess_balancing_requirements(hegemony_threats)
        }

    def _calculate_overall_threat_level(self, threats: List[Dict[str, Any]]) -> str:
        """Calculate overall hegemony threat level"""
        if not threats:
            return 'low'

        high_threats = [t for t in threats if t['threat_level'] == 'high']
        medium_threats = [t for t in threats if t['threat_level'] == 'medium']

        if high_threats:
            return 'high'
        elif len(medium_threats) > 1:
            return 'medium'
        elif medium_threats:
            return 'low_medium'
        else:
            return 'low'

    def _assess_balancing_requirements(self, threats: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Assess balancing requirements based on identified threats"""
        balancing_actors = [t['actor'] for t in threats if t['balancing_needed']]

        return {
            'balancing_needed': len(balancing_actors) > 0,
            'actors_to_balance': balancing_actors,
            'urgency_level': 'high' if any(t['threat_level'] == 'high' for t in threats) else 'medium',
            'coalition_size_required': len(balancing_actors) + 2  # Minimum coalition size
        }

    def _calculate_balancing_requirements(self, power_assessments: List[Dict[str, Any]],
                                        hegemony_analysis: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate specific balancing requirements"""
        requirements = {
            'coalition_formations': [],
            'power_shifting_needs': [],
            'diplomatic_priorities': []
        }

        threats = hegemony_analysis.get('threats_identified', [])
        for threat in threats:
            if threat['balancing_needed']:
                target_actor = threat['actor']
                # Find potential balancing partners
                balancing_partners = [a for a in power_assessments if a['name'] != target_actor]

                requirements['coalition_formations'].append({
                    'target': target_actor,
                    'recommended_partners': [p['name'] for p in balancing_partners[:3]],
                    'coalition_strength_needed': threat['power_percentage'] * 1.2
                })

        return requirements

    def _generate_equilibrium_strategies(self, power_assessments: List[Dict[str, Any]],
                                       hegemony_analysis: Dict[str, Any],
                                       balancing_needs: Dict[str, Any]) -> List[str]:
        """Generate strategies for maintaining equilibrium"""
        strategies = []

        threat_level = hegemony_analysis.get('overall_threat_level', 'low')

        if threat_level == 'high':
            strategies.extend([
                'Immediate formation of counterbalancing coalition',
                'Strengthen alliances with middle powers',
                'Provide security guarantees to potential balancing partners'
            ])
        elif threat_level == 'medium':
            strategies.extend([
                'Strategic diplomatic engagement with rising powers',
                'Maintain flexible alliance options',
                'Monitor power transitions carefully'
            ])
        else:
            strategies.extend([
                'Preserve current power distribution',
                'Strengthen international institutions',
                'Promote diplomatic conflict resolution'
            ])

        # Add historical wisdom
        strategies.extend([
            'Apply lessons from historical balance of power systems',
            'Maintain strategic flexibility over fixed commitments',
            'Prioritize stability over ideological considerations'
        ])

        return strategies

    def _apply_historical_precedents(self, distribution_analysis: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Apply historical precedents to current situation"""
        applicable_precedents = []

        distribution_type = distribution_analysis.get('distribution_type', '')

        for precedent_name, precedent_data in self.historical_precedents.items():
            relevance = self._assess_precedent_relevance(distribution_type, precedent_name)
            if relevance > 0.5:
                applicable_precedents.append({
                    'name': precedent_name,
                    'period': precedent_data['period'],
                    'mechanism': precedent_data['mechanism'],
                    'success_rate': precedent_data['success_rate'],
                    'relevance_score': relevance,
                    'lessons_applicable': self._extract_lessons(precedent_name, distribution_type)
                })

        return applicable_precedents

    def _assess_precedent_relevance(self, distribution_type: str, precedent_name: str) -> float:
        """Assess relevance of historical precedent to current situation"""
        relevance_map = {
            'hegemonic_dominance': {'concert_of_europe': 0.8, 'bismarckian_system': 0.7, 'cold_war_balance': 0.9},
            'bipolar_balance': {'cold_war_balance': 0.9, 'concert_of_europe': 0.6},
            'multipolar_distribution': {'concert_of_europe': 0.8, 'bismarckian_system': 0.9},
            'unipolar_tendencies': {'concert_of_europe': 0.7, 'bismarckian_system': 0.6}
        }

        return relevance_map.get(distribution_type, {}).get(precedent_name, 0.3)

    def _extract_lessons(self, precedent_name: str, distribution_type: str) -> List[str]:
        """Extract lessons from historical precedents"""
        lessons_map = {
            'concert_of_europe': [
                'Great power cooperation essential for stability',
                'Regular diplomatic consultation prevents crises',
                'Balance of power requires mutual recognition'
            ],
            'bismarckian_system': [
                'Complex alliance networks deter aggression',
                'Strategic flexibility maintains options',
                'Isolate potential revisionist powers'
            ],
            'cold_war_balance': [
                'Nuclear deterrence prevents great power war',
                'Clear red lines reduce miscalculation',
                'Indirect competition manageable'
            ]
        }

        return lessons_map.get(precedent_name, [])

    def _assess_overall_stability(self, distribution_analysis: Dict[str, Any],
                                hegemony_analysis: Dict[str, Any]) -> Dict[str, Any]:
        """Assess overall stability of the international system"""
        threat_level = hegemony_analysis.get('overall_threat_level', 'low')
        balance_quality = distribution_analysis.get('balance_quality', 'unknown')

        stability_factors = {
            'threat_level': threat_level,
            'balance_quality': balance_quality,
            'concentration_index': distribution_analysis.get('concentration_index', 0)
        }

        # Calculate stability score
        stability_score = 100.0
        if threat_level == 'high':
            stability_score -= 40
        elif threat_level == 'medium':
            stability_score -= 20
        elif threat_level == 'low_medium':
            stability_score -= 10

        if balance_quality == 'poor_balance':
            stability_score -= 30
        elif balance_quality == 'moderate_balance':
            stability_score -= 15
        elif balance_quality == 'good_balance':
            stability_score -= 5

        concentration_index = distribution_analysis.get('concentration_index', 0)
        if concentration_index > 2500:
            stability_score -= 20
        elif concentration_index > 1500:
            stability_score -= 10

        stability_score = max(0, stability_score)

        return {
            'stability_score': stability_score,
            'stability_rating': self._classify_stability(stability_score),
            'stability_factors': stability_factors,
            'recommendations': self._generate_stability_recommendations(stability_score, threat_level)
        }

    def _classify_stability(self, score: float) -> str:
        """Classify stability based on score"""
        if score >= 80:
            return 'highly_stable'
        elif score >= 60:
            return 'moderately_stable'
        elif score >= 40:
            return 'vulnerable'
        else:
            return 'unstable'

    def _generate_stability_recommendations(self, stability_score: float, threat_level: str) -> List[str]:
        """Generate recommendations for maintaining stability"""
        recommendations = []

        if stability_score < 40:
            recommendations.extend([
                'Urgent diplomatic intervention required',
                'Form emergency balancing coalitions',
                'Crisis management mechanisms essential'
            ])
        elif stability_score < 60:
            recommendations.extend([
                'Strategic engagement with rising powers',
                'Strengthen alliance commitments',
                'Enhance diplomatic communication channels'
            ])
        else:
            recommendations.extend([
                'Maintain current diplomatic arrangements',
                'Monitor power transitions carefully',
                'Preserve international institutions'
            ])

        return recommendations

# Example usage and testing
if __name__ == "__main__":
    calculator = BalancePowerCalculator()

    # Test data typical of balance of power analysis
    test_actors = [
        {
            'name': 'USA',
            'capabilities': {
                'military': {'personnel': 90, 'equipment': 95, 'technology': 95, 'nuclear': 100},
                'economic': {'gdp': 90, 'trade': 85, 'technology': 95, 'resources': 80},
                'political': {'leadership': 85, 'institutions': 90, 'cohesion': 75, 'alliances': 95},
                'geographic': {'location': 90, 'size': 85, 'resources': 85, 'access': 95}
            }
        },
        {
            'name': 'China',
            'capabilities': {
                'military': {'personnel': 95, 'equipment': 85, 'technology': 80, 'nuclear': 70},
                'economic': {'gdp': 85, 'trade': 90, 'technology': 85, 'resources': 75},
                'political': {'leadership': 90, 'institutions': 80, 'cohesion': 85, 'alliances': 70},
                'geographic': {'location': 80, 'size': 95, 'resources': 70, 'access': 75}
            }
        },
        {
            'name': 'Russia',
            'capabilities': {
                'military': {'personnel': 75, 'equipment': 70, 'technology': 75, 'nuclear': 95},
                'economic': {'gdp': 60, 'trade': 65, 'technology': 65, 'resources': 90},
                'political': {'leadership': 85, 'institutions': 70, 'cohesion': 80, 'alliances': 75},
                'geographic': {'location': 90, 'size': 95, 'resources': 95, 'access': 80}
            }
        }
    ]

    result = calculator.calculate_power_balance(test_actors)

    print("=== BALANCE OF POWER CALCULATIONS ===")
    print(f"Distribution Type: {result['distribution_analysis']['distribution_type']}")
    print(f"Balance Quality: {result['distribution_analysis']['balance_quality']}")
    print(f"Overall Stability: {result['stability_assessment']['stability_rating']}")
    print(f"Stability Score: {result['stability_assessment']['stability_score']:.1f}")

    print("\nPOWER RANKINGS:")
    for assessment in result['power_assessments']:
        print(f"  {assessment['power_rank']}. {assessment['name']}: {assessment['total_power_score']:.1f}")

    print("\nHEGEMONY THREATS:")
    for threat in result['hegemony_analysis']['threats_identified']:
        print(f"  - {threat['actor']}: {threat['threat_level']} threat ({threat['power_percentage']:.1f}%)")

    print("\nEQUILIBRIUM STRATEGIES:")
    for strategy in result['equilibrium_strategies']:
        print(f"  - {strategy}")

    print("\nHISTORICAL PRECEDENTS:")
    for precedent in result['historical_precedents']:
        print(f"  - {precedent['name']}: Relevance {precedent['relevance_score']:.2f}")