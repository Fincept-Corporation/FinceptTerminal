"""Scorecard Valuation Method"""
from typing import Dict, Any

class ScorecardMethod:
    """Scorecard (or Payne) Method for startup valuation"""

    def __init__(self, region: str = 'US'):
        self.region = region

        self.typical_pre_money_valuations = {
            'US': {'seed': 2_000_000, 'series_a': 5_000_000},
            'Europe': {'seed': 1_500_000, 'series_a': 4_000_000},
            'Asia': {'seed': 1_800_000, 'series_a': 4_500_000}
        }

        self.factor_weights = {
            'management_team': 0.30,
            'size_of_opportunity': 0.25,
            'product_technology': 0.15,
            'competitive_environment': 0.10,
            'marketing_sales_channels': 0.10,
            'need_for_additional_investment': 0.05,
            'other_factors': 0.05
        }

    def get_baseline_valuation(self, stage: str = 'seed') -> float:
        """Get baseline valuation for region and stage"""

        region_valuations = self.typical_pre_money_valuations.get(self.region, self.typical_pre_money_valuations['US'])
        return region_valuations.get(stage, region_valuations['seed'])

    def assess_factor(self, factor_name: str, comparison_score: float) -> float:
        """
        Assess factor relative to average startup

        Args:
            factor_name: Name of factor being assessed
            comparison_score: -0.5 to +0.5 (0 = average, +0.5 = way above, -0.5 = way below)

        Returns:
            Adjustment multiplier (e.g., 1.3 for 30% above average)
        """

        if factor_name not in self.factor_weights:
            raise ValueError(f"Unknown factor: {factor_name}")

        weight = self.factor_weights[factor_name]

        adjustment = 1 + (comparison_score * weight / 0.30)

        return max(0.5, min(1.5, adjustment))

    def calculate_valuation(self, stage: str, factor_assessments: Dict[str, float],
                          custom_baseline: float = None) -> Dict[str, Any]:
        """
        Calculate valuation using scorecard method

        Args:
            stage: Funding stage ('seed', 'series_a')
            factor_assessments: Dict of factor_name -> comparison_score (-0.5 to +0.5)
            custom_baseline: Optional custom baseline instead of regional average

        Returns:
            Detailed valuation breakdown
        """

        baseline = custom_baseline or self.get_baseline_valuation(stage)

        factor_multipliers = {}
        cumulative_multiplier = 1.0

        for factor, comparison_score in factor_assessments.items():
            multiplier = self.assess_factor(factor, comparison_score)
            factor_multipliers[factor] = {
                'comparison_score': comparison_score,
                'weight': self.factor_weights[factor],
                'multiplier': multiplier
            }

            weight = self.factor_weights[factor]
            cumulative_multiplier *= (1 + (multiplier - 1) * weight / sum(self.factor_weights.values()))

        final_valuation = baseline * cumulative_multiplier

        return {
            'method': 'Scorecard Method',
            'baseline_valuation': baseline,
            'region': self.region,
            'stage': stage,
            'factor_assessments': factor_multipliers,
            'cumulative_multiplier': cumulative_multiplier,
            'final_valuation': final_valuation,
            'adjustment_pct': (cumulative_multiplier - 1) * 100
        }

    def comprehensive_assessment(self, stage: str, team_strength: str, market_size: str,
                                product_strength: str, competition: str,
                                sales_traction: str) -> Dict[str, Any]:
        """
        Comprehensive assessment with simplified inputs

        Args:
            All parameters: 'weak', 'average', 'strong', 'excellent'

        Returns:
            Valuation result
        """

        strength_to_score = {
            'weak': -0.4,
            'below_average': -0.2,
            'average': 0.0,
            'above_average': 0.2,
            'strong': 0.35,
            'excellent': 0.5
        }

        assessments = {
            'management_team': strength_to_score.get(team_strength, 0),
            'size_of_opportunity': strength_to_score.get(market_size, 0),
            'product_technology': strength_to_score.get(product_strength, 0),
            'competitive_environment': strength_to_score.get(competition, 0),
            'marketing_sales_channels': strength_to_score.get(sales_traction, 0),
            'need_for_additional_investment': 0.0,
            'other_factors': 0.0
        }

        return self.calculate_valuation(stage, assessments)

    def sensitivity_analysis(self, stage: str, base_assessments: Dict[str, float],
                           variable_factor: str) -> Dict[str, Any]:
        """Run sensitivity analysis on single factor"""

        results = []

        for score in [-0.5, -0.3, -0.1, 0, 0.1, 0.3, 0.5]:
            test_assessments = base_assessments.copy()
            test_assessments[variable_factor] = score

            valuation_result = self.calculate_valuation(stage, test_assessments)

            results.append({
                'factor_score': score,
                'valuation': valuation_result['final_valuation'],
                'multiplier': valuation_result['cumulative_multiplier']
            })

        return {
            'variable_factor': variable_factor,
            'sensitivity_data': results,
            'baseline_valuation': self.get_baseline_valuation(stage)
        }

if __name__ == '__main__':
    scorecard = ScorecardMethod(region='US')

    valuation = scorecard.comprehensive_assessment(
        stage='seed',
        team_strength='excellent',
        market_size='strong',
        product_strength='above_average',
        competition='average',
        sales_traction='below_average'
    )

    print(f"=== SCORECARD METHOD VALUATION ===\n")
    print(f"Baseline ({valuation['region']} {valuation['stage']}): ${valuation['baseline_valuation']:,.0f}")
    print(f"Adjustment: {valuation['adjustment_pct']:+.1f}%")
    print(f"Final Valuation: ${valuation['final_valuation']:,.0f}\n")

    print("Factor Assessments:")
    for factor, data in valuation['factor_assessments'].items():
        print(f"  {factor.replace('_', ' ').title()}: {data['comparison_score']:+.2f} (weight: {data['weight']:.0%})")
