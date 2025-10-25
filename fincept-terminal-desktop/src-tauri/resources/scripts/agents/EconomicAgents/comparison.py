"""
Comparison module for analyzing and comparing different economic ideology agents
"""

from typing import Dict, List, Any, Tuple, Optional
from collections import defaultdict
import statistics
from base_agent import EconomicAgent, EconomicData
from utils import calculate_composite_score, clamp_value

class EconomicAgentComparator:
    """Compares analyses from different economic ideology agents"""

    def __init__(self):
        self.comparison_weights = {
            'health_score': 0.3,
            'policy_alignment': 0.25,
            'consistency': 0.2,
            'feasibility': 0.15,
            'stability': 0.1
        }

    def compare_agents(self, agents: Dict[str, EconomicAgent], data: EconomicData) -> Dict[str, Any]:
        """
        Compare multiple economic agents on the same data

        Args:
            agents (Dict[str, EconomicAgent]): Dictionary of agents to compare
            data (EconomicData): Economic data to analyze

        Returns:
            Dict[str, Any]: Comprehensive comparison results
        """
        # Get analyses from all agents
        analyses = {}
        for name, agent in agents.items():
            try:
                analyses[name] = agent.analyze_economy(data)
            except Exception as e:
                print(f"Error analyzing with {name}: {e}")
                analyses[name] = {'error': str(e)}

        # Calculate comparison metrics
        comparison_metrics = self._calculate_comparison_metrics(analyses, data)

        # Rank agents by different criteria
        rankings = self._rank_agents(analyses, comparison_metrics)

        # Identify consensus and divergences
        consensus = self._identify_consensus(analyses)
        divergences = self._identify_divergences(analyses)

        # Generate policy comparison
        policy_comparison = self._compare_policies(agents, data, analyses)

        return {
            'agent_analyses': analyses,
            'comparison_metrics': comparison_metrics,
            'rankings': rankings,
            'consensus': consensus,
            'divergences': divergences,
            'policy_comparison': policy_comparison,
            'summary': self._generate_comparison_summary(analyses, comparison_metrics)
        }

    def compare_policy_recommendations(self, agents: Dict[str, EconomicAgent], data: EconomicData) -> Dict[str, Any]:
        """
        Compare policy recommendations from different agents

        Args:
            agents (Dict[str, EconomicAgent]): Dictionary of agents to compare
            data (EconomicData): Economic data to analyze

        Returns:
            Dict[str, Any]: Policy comparison results
        """
        all_policies = {}
        policy_categories = defaultdict(list)

        # Collect all policies from all agents
        for name, agent in agents.items():
            try:
                analysis = agent.analyze_economy(data)
                policies = agent.get_policy_recommendations(data, analysis)
                all_policies[name] = policies

                # Categorize policies
                for policy in policies:
                    category = self._categorize_policy(policy.title)
                    policy_categories[category].append({
                        'agent': name,
                        'policy': policy
                    })
            except Exception as e:
                print(f"Error getting policies from {name}: {e}")

        # Analyze policy overlaps and conflicts
        overlaps = self._find_policy_overlaps(policy_categories)
        conflicts = self._find_policy_conflicts(policy_categories)

        # Calculate support for each policy category
        category_support = self._calculate_policy_support(policy_categories, len(agents))

        return {
            'all_policies': all_policies,
            'policy_categories': dict(policy_categories),
            'overlaps': overlaps,
            'conflicts': conflicts,
            'category_support': category_support,
            'recommendations': self._synthesize_policy_recommendations(policy_categories, category_support)
        }

    def evaluate_economic_scenarios(self, agents: Dict[str, EconomicAgent], scenarios: List[EconomicData]) -> Dict[str, Any]:
        """
        Evaluate different economic scenarios with all agents

        Args:
            agents (Dict[str, EconomicAgent]): Dictionary of agents to evaluate
            scenarios (List[EconomicData]): Different economic scenarios to test

        Returns:
            Dict[str, Any]: Scenario evaluation results
        """
        scenario_results = {}

        for i, scenario in enumerate(scenarios):
            scenario_name = f"Scenario_{i+1}"
            scenario_results[scenario_name] = {
                'data': scenario,
                'analyses': {},
                'metrics': {}
            }

            # Get analysis from each agent for this scenario
            for name, agent in agents.items():
                try:
                    analysis = agent.analyze_economy(scenario)
                    scenario_results[scenario_name]['analyses'][name] = analysis
                except Exception as e:
                    print(f"Error analyzing {scenario_name} with {name}: {e}")

            # Calculate metrics for this scenario
            scenario_results[scenario_name]['metrics'] = self._calculate_scenario_metrics(
                scenario_results[scenario_name]['analyses']
            )

        # Compare agent performance across scenarios
        agent_performance = self._compare_agent_performance(scenario_results)

        # Identify best performing agents for different scenarios
        best_performers = self._identify_best_performers(scenario_results)

        return {
            'scenario_results': scenario_results,
            'agent_performance': agent_performance,
            'best_performers': best_performers,
            'summary': self._generate_scenario_summary(scenario_results)
        }

    def calculate_ideology_alignment(self, agent1_analysis: Dict, agent2_analysis: Dict) -> float:
        """
        Calculate alignment score between two ideological analyses

        Args:
            agent1_analysis (Dict): Analysis from first agent
            agent2_analysis (Dict): Analysis from second agent

        Returns:
            float: Alignment score (0-100)
        """
        if 'error' in agent1_analysis or 'error' in agent2_analysis:
            return 0.0

        alignment_factors = []

        # Health score alignment
        if 'overall_health_score' in agent1_analysis and 'overall_health_score' in agent2_analysis:
            score_diff = abs(agent1_analysis['overall_health_score'] - agent2_analysis['overall_health_score'])
            alignment_factors.append(max(0, 100 - score_diff))

        # Strengths overlap
        if 'strengths' in agent1_analysis and 'strengths' in agent2_analysis:
            strengths_overlap = self._calculate_text_overlap(
                agent1_analysis['strengths'],
                agent2_analysis['strengths']
            )
            alignment_factors.append(strengths_overlap)

        # Concerns overlap
        if 'concerns' in agent1_analysis and 'concerns' in agent2_analysis:
            concerns_overlap = self._calculate_text_overlap(
                agent1_analysis['concerns'],
                agent2_analysis['concerns']
            )
            alignment_factors.append(concerns_overlap)

        return statistics.mean(alignment_factors) if alignment_factors else 0.0

    def _calculate_comparison_metrics(self, analyses: Dict[str, Any], data: EconomicData) -> Dict[str, Any]:
        """Calculate comparison metrics across all agent analyses"""
        metrics = {
            'health_scores': {},
            'average_health_score': 0,
            'health_score_variance': 0,
            'most_optimistic': None,
            'most_pessimistic': None,
            'ideological_diversity': 0
        }

        health_scores = []
        for name, analysis in analyses.items():
            if 'error' not in analysis and 'overall_health_score' in analysis:
                score = analysis['overall_health_score']
                health_scores.append(score)
                metrics['health_scores'][name] = score

        if health_scores:
            metrics['average_health_score'] = statistics.mean(health_scores)
            metrics['health_score_variance'] = statistics.stdev(health_scores) if len(health_scores) > 1 else 0

            # Find most optimistic and pessimistic
            max_score_agent = max(metrics['health_scores'], key=metrics['health_scores'].get)
            min_score_agent = min(metrics['health_scores'], key=metrics['health_scores'].get)
            metrics['most_optimistic'] = max_score_agent
            metrics['most_pessimistic'] = min_score_agent

            # Calculate ideological diversity (based on score variance)
            metrics['ideological_diversity'] = clamp_value(metrics['health_score_variance'] * 10, 0, 100)

        return metrics

    def _rank_agents(self, analyses: Dict[str, Any], metrics: Dict[str, Any]) -> Dict[str, List[str]]:
        """Rank agents by different criteria"""
        rankings = {}

        if 'health_scores' in metrics and metrics['health_scores']:
            # Rank by health score
            health_ranking = sorted(metrics['health_scores'].items(), key=lambda x: x[1], reverse=True)
            rankings['by_health_score'] = [name for name, _ in health_ranking]

        # Rank by stability (inverse of variance in scores)
        stability_scores = {}
        for name in analyses.keys():
            # For simplicity, use ideology-specific stability indicators
            if name.lower() in ['mixed_economy', 'keynesian']:
                stability_scores[name] = 80
            elif name.lower() in ['capitalism', 'neoliberal']:
                stability_scores[name] = 70
            else:
                stability_scores[name] = 60

        stability_ranking = sorted(stability_scores.items(), key=lambda x: x[1], reverse=True)
        rankings['by_stability'] = [name for name, _ in stability_ranking]

        return rankings

    def _identify_consensus(self, analyses: Dict[str, Any]) -> Dict[str, Any]:
        """Identify areas of consensus among agents"""
        consensus = {
            'economic_conditions': [],
            'policy_priorities': [],
            'key_concerns': []
        }

        all_strengths = []
        all_concerns = []

        for name, analysis in analyses.items():
            if 'error' not in analysis:
                all_strengths.extend(analysis.get('strengths', []))
                all_concerns.extend(analysis.get('concerns', []))

        # Find common themes
        consensus['economic_conditions'] = self._find_common_themes(all_strengths, threshold=0.4)
        consensus['key_concerns'] = self._find_common_themes(all_concerns, threshold=0.3)

        return consensus

    def _identify_divergences(self, analyses: Dict[str, Any]) -> Dict[str, Any]:
        """Identify major divergences between agents"""
        divergences = {
            'health_score_divergence': 0,
            'ideological_gaps': [],
            'policy_conflicts': []
        }

        health_scores = []
        for name, analysis in analyses.items():
            if 'error' not in analysis and 'overall_health_score' in analysis:
                health_scores.append(analysis['overall_health_score'])

        if len(health_scores) > 1:
            divergences['health_score_divergence'] = max(health_scores) - min(health_scores)

        # Identify ideological gaps
        divergences['ideological_gaps'] = self._find_ideological_gaps(analyses)

        return divergences

    def _compare_policies(self, agents: Dict[str, EconomicAgent], data: EconomicData, analyses: Dict[str, Any]) -> Dict[str, Any]:
        """Compare policy recommendations across agents"""
        policy_comparison = {
            'common_recommendations': [],
            'conflicting_recommendations': [],
            'unique_recommendations': {}
        }

        all_policies = {}
        for name, agent in agents.items():
            if name in analyses and 'error' not in analyses[name]:
                try:
                    policies = agent.get_policy_recommendations(data, analyses[name])
                    all_policies[name] = [p.title for p in policies]
                except Exception as e:
                    print(f"Error getting policies from {name}: {e}")
                    all_policies[name] = []

        # Find common recommendations
        if all_policies:
            policy_sets = [set(policies) for policies in all_policies.values()]
            if policy_sets:
                common = set.intersection(*policy_sets)
                policy_comparison['common_recommendations'] = list(common)

        # Find unique recommendations for each agent
        for name, policies in all_policies.items():
            other_policies = set()
            for other_name, other_policies_list in all_policies.items():
                if other_name != name:
                    other_policies.update(other_policies_list)

            unique = set(policies) - other_policies
            if unique:
                policy_comparison['unique_recommendations'][name] = list(unique)

        return policy_comparison

    def _categorize_policy(self, policy_title: str) -> str:
        """Categorize policy based on keywords in title"""
        title_lower = policy_title.lower()

        if any(word in title_lower for word in ['tax', 'revenue', 'fiscal']):
            return 'fiscal'
        elif any(word in title_lower for word in ['spending', 'investment', 'infrastructure']):
            return 'expenditure'
        elif any(word in title_lower for word in ['trade', 'tariff', 'import', 'export']):
            return 'trade'
        elif any(word in title_lower for word in ['regulation', 'deregulation', 'compliance']):
            return 'regulation'
        elif any(word in title_lower for word in ['monetary', 'interest', 'inflation']):
            return 'monetary'
        elif any(word in title_lower for word in ['labor', 'employment', 'wage']):
            return 'labor'
        elif any(word in title_lower for word in ['social', 'welfare', 'healthcare', 'education']):
            return 'social'
        else:
            return 'other'

    def _find_policy_overlaps(self, policy_categories: Dict[str, List]) -> List[Dict[str, Any]]:
        """Find overlapping policy recommendations"""
        overlaps = []

        for category, policies in policy_categories.items():
            if len(policies) > 1:
                # Multiple agents recommend similar policies in this category
                overlaps.append({
                    'category': category,
                    'supporting_agents': [p['agent'] for p in policies],
                    'policy_count': len(policies),
                    'example_policies': [p['policy'].title for p in policies[:3]]
                })

        return overlaps

    def _find_policy_conflicts(self, policy_categories: Dict[str, List]) -> List[Dict[str, Any]]:
        """Find conflicting policy recommendations"""
        conflicts = []

        # Check for classic policy conflicts
        conflict_pairs = [
            ('fiscal', 'monetary'),  # Tax increases vs interest rate cuts
            ('trade', 'expenditure'),  # Trade barriers vs spending
        ]

        for category1, category2 in conflict_pairs:
            if category1 in policy_categories and category2 in policy_categories:
                agents1 = set(p['agent'] for p in policy_categories[category1])
                agents2 = set(p['agent'] for p in policy_categories[category2])

                common_agents = agents1.intersection(agents2)
                if common_agents:
                    conflicts.append({
                        'type': f'{category1}_vs_{category2}',
                        'conflicting_categories': [category1, category2],
                        'agents_with_conflicts': list(common_agents)
                    })

        return conflicts

    def _calculate_policy_support(self, policy_categories: Dict[str, List], total_agents: int) -> Dict[str, float]:
        """Calculate support percentage for each policy category"""
        support = {}

        for category, policies in policy_categories.items():
            supporting_agents = set(p['agent'] for p in policies)
            support_percentage = (len(supporting_agents) / total_agents) * 100
            support[category] = support_percentage

        return support

    def _synthesize_policy_recommendations(self, policy_categories: Dict[str, List], support: Dict[str, float]) -> List[Dict[str, Any]]:
        """Synthesize policy recommendations based on agent consensus"""
        synthesized = []

        for category, support_level in support.items():
            if support_level >= 50:  # Majority support
                synthesized.append({
                    'category': category,
                    'support_level': support_level,
                    'recommendation': f"Strong consideration for {category}-focused policies",
                    'priority': 'high' if support_level >= 75 else 'medium'
                })
            elif support_level >= 33:  # Significant minority
                synthesized.append({
                    'category': category,
                    'support_level': support_level,
                    'recommendation': f"Consider {category}-policies for balanced approach",
                    'priority': 'medium'
                })

        return sorted(synthesized, key=lambda x: x['support_level'], reverse=True)

    def _calculate_scenario_metrics(self, analyses: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate metrics for a specific scenario"""
        metrics = {
            'agent_count': len(analyses),
            'average_health_score': 0,
            'health_score_range': 0,
            'consensus_level': 0
        }

        health_scores = []
        for name, analysis in analyses.items():
            if 'error' not in analysis and 'overall_health_score' in analysis:
                health_scores.append(analysis['overall_health_score'])

        if health_scores:
            metrics['average_health_score'] = statistics.mean(health_scores)
            metrics['health_score_range'] = max(health_scores) - min(health_scores)
            metrics['consensus_level'] = 100 - (metrics['health_score_range'] / 2)  # Simple consensus measure

        return metrics

    def _compare_agent_performance(self, scenario_results: Dict[str, Any]) -> Dict[str, Dict[str, float]]:
        """Compare agent performance across all scenarios"""
        agent_performance = {}

        # Collect performance data for each agent
        for scenario_name, scenario_data in scenario_results.items():
            for agent_name, analysis in scenario_data['analyses'].items():
                if 'error' not in analysis and 'overall_health_score' in analysis:
                    if agent_name not in agent_performance:
                        agent_performance[agent_name] = []
                    agent_performance[agent_name].append(analysis['overall_health_score'])

        # Calculate performance statistics
        performance_summary = {}
        for agent_name, scores in agent_performance.items():
            performance_summary[agent_name] = {
                'average_score': statistics.mean(scores),
                'score_variance': statistics.stdev(scores) if len(scores) > 1 else 0,
                'max_score': max(scores),
                'min_score': min(scores),
                'scenario_count': len(scores)
            }

        return performance_summary

    def _identify_best_performers(self, scenario_results: Dict[str, Any]) -> Dict[str, List[str]]:
        """Identify best performing agents for different criteria"""
        best_performers = {}

        # Best average performer
        agent_performance = self._compare_agent_performance(scenario_results)
        if agent_performance:
            best_average = max(agent_performance.items(), key=lambda x: x[1]['average_score'])
            best_performers['best_average'] = [best_average[0]]

            # Most consistent (lowest variance)
            most_consistent = min(agent_performance.items(), key=lambda x: x[1]['score_variance'])
            best_performers['most_consistent'] = [most_consistent[0]]

            # Best individual scenario performance
            best_scenario_scores = {}
            for scenario_name, scenario_data in scenario_results.items():
                best_agent = None
                best_score = -1
                for agent_name, analysis in scenario_data['analyses'].items():
                    if 'error' not in analysis and 'overall_health_score' in analysis:
                        if analysis['overall_health_score'] > best_score:
                            best_score = analysis['overall_health_score']
                            best_agent = agent_name
                if best_agent:
                    if best_agent not in best_scenario_scores:
                        best_scenario_scores[best_agent] = 0
                    best_scenario_scores[best_agent] += 1

            if best_scenario_scores:
                best_overall = max(best_scenario_scores.items(), key=lambda x: x[1])
                best_performers['best_scenario_performance'] = [best_overall[0]]

        return best_performers

    def _generate_scenario_summary(self, scenario_results: Dict[str, Any]) -> str:
        """Generate a summary of scenario analysis"""
        total_scenarios = len(scenario_results)
        agent_performance = self._compare_agent_performance(scenario_results)

        if not agent_performance:
            return "Unable to generate scenario summary due to lack of valid analyses."

        best_agent = max(agent_performance.items(), key=lambda x: x[1]['average_score'])
        most_consistent = min(agent_performance.items(), key=lambda x: x[1]['score_variance'])

        return (f"Analyzed {total_scenarios} scenarios. "
                f"Best overall performer: {best_agent[0]} "
                f"(avg score: {best_agent[1]['average_score']:.1f}). "
                f"Most consistent: {most_consistent[0]} "
                f"(variance: {most_consistent[1]['score_variance']:.1f}).")

    def _generate_comparison_summary(self, analyses: Dict[str, Any], metrics: Dict[str, Any]) -> str:
        """Generate a summary of the comparison"""
        if not analyses:
            return "No valid analyses available for comparison."

        avg_health = metrics.get('average_health_score', 0)
        diversity = metrics.get('ideological_diversity', 0)

        summary = f"Comparison of {len(analyses)} economic ideologies. "
        summary += f"Average health score: {avg_health:.1f}/100. "
        summary += f"Ideological diversity: {diversity:.1f}/100. "

        if metrics.get('most_optimistic'):
            summary += f"Most optimistic: {metrics['most_optimistic']}. "
        if metrics.get('most_pessimistic'):
            summary += f"Most pessimistic: {metrics['most_pessimistic']}."

        return summary

    def _calculate_text_overlap(self, list1: List[str], list2: List[str]) -> float:
        """Calculate percentage overlap between two text lists"""
        set1 = set(list1)
        set2 = set(list2)

        if not set1 and not set2:
            return 100.0
        if not set1 or not set2:
            return 0.0

        intersection = set1.intersection(set2)
        union = set1.union(set2)

        return (len(intersection) / len(union)) * 100 if union else 0.0

    def _find_common_themes(self, text_list: List[str], threshold: float = 0.3) -> List[str]:
        """Find common themes in a list of text items"""
        if not text_list:
            return []

        # Count frequency of key terms
        term_frequency = defaultdict(int)
        for text in text_list:
            # Simple keyword extraction (could be made more sophisticated)
            words = text.lower().split()
            for word in words:
                if len(word) > 3:  # Ignore short words
                    term_frequency[word] += 1

        # Find terms that appear in at least threshold proportion of texts
        common_terms = [
            term for term, count in term_frequency.items()
            if count >= len(text_list) * threshold
        ]

        return sorted(common_terms, key=lambda x: term_frequency[x], reverse=True)[:5]

    def _find_ideological_gaps(self, analyses: Dict[str, Any]) -> List[str]:
        """Find major ideological gaps between analyses"""
        gaps = []

        health_scores = []
        for name, analysis in analyses.items():
            if 'error' not in analysis and 'overall_health_score' in analysis:
                health_scores.append((name, analysis['overall_health_score']))

        if len(health_scores) >= 2:
            health_scores.sort(key=lambda x: x[1])
            min_score = health_scores[0][1]
            max_score = health_scores[-1][1]

            if max_score - min_score > 30:
                gaps.append(f"Significant divergence in economic health assessment ({max_score - min_score:.1f} points)")

        return gaps