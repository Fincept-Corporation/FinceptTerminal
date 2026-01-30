"""Berkus Method - Pre-Revenue Startup Valuation"""
from typing import Dict, Any
from enum import Enum

class BerkusFactor(Enum):
    """Five key value factors in Berkus Method"""
    SOUND_IDEA = "sound_idea"
    PROTOTYPE = "prototype"
    QUALITY_MANAGEMENT = "quality_management"
    STRATEGIC_RELATIONSHIPS = "strategic_relationships"
    PRODUCT_ROLLOUT = "product_rollout"

class BerkusMethod:
    """Berkus Method for pre-revenue startup valuation"""

    def __init__(self, max_value_per_factor: float = 500_000):
        self.max_value = max_value_per_factor
        self.max_total_valuation = max_value_per_factor * 5

        self.factor_descriptions = {
            BerkusFactor.SOUND_IDEA: "Basic value, viable idea with significant market opportunity",
            BerkusFactor.PROTOTYPE: "Technology risk reduced through working prototype/product",
            BerkusFactor.QUALITY_MANAGEMENT: "Strong management team with relevant experience",
            BerkusFactor.STRATEGIC_RELATIONSHIPS: "Strategic alliances, partnerships, or customer commitments",
            BerkusFactor.PRODUCT_ROLLOUT: "Evidence of market acceptance, initial sales or validation"
        }

    def assess_factor(self, factor: BerkusFactor, score: float) -> float:
        """
        Assess single factor value

        Args:
            factor: BerkusFactor enum
            score: 0-1 score (0 = none, 0.5 = moderate, 1 = excellent)

        Returns:
            Dollar value for this factor
        """
        if not 0 <= score <= 1:
            raise ValueError("Score must be between 0 and 1")

        return self.max_value * score

    def calculate_valuation(self, factor_scores: Dict[BerkusFactor, float]) -> Dict[str, Any]:
        """
        Calculate total startup valuation using Berkus Method

        Args:
            factor_scores: Dict mapping BerkusFactor to score (0-1)

        Returns:
            Valuation breakdown and total
        """

        factor_values = {}
        total_valuation = 0

        for factor in BerkusFactor:
            score = factor_scores.get(factor, 0)
            value = self.assess_factor(factor, score)
            factor_values[factor.value] = {
                'score': score,
                'value': value,
                'max_value': self.max_value,
                'description': self.factor_descriptions[factor]
            }
            total_valuation += value

        return {
            'method': 'Berkus Method',
            'factor_assessments': factor_values,
            'total_valuation': total_valuation,
            'max_possible_valuation': self.max_total_valuation,
            'valuation_percentage': (total_valuation / self.max_total_valuation * 100)
        }

    def quick_assessment(self, idea_score: float, prototype_score: float,
                        team_score: float, relationships_score: float,
                        rollout_score: float) -> Dict[str, Any]:
        """Quick valuation with direct scores"""

        factor_scores = {
            BerkusFactor.SOUND_IDEA: idea_score,
            BerkusFactor.PROTOTYPE: prototype_score,
            BerkusFactor.QUALITY_MANAGEMENT: team_score,
            BerkusFactor.STRATEGIC_RELATIONSHIPS: relationships_score,
            BerkusFactor.PRODUCT_ROLLOUT: rollout_score
        }

        return self.calculate_valuation(factor_scores)

    def guided_assessment(self) -> Dict[str, Any]:
        """Interactive guided assessment with questions"""

        assessment_questions = {
            BerkusFactor.SOUND_IDEA: [
                "Is the market opportunity large (>$1B)?",
                "Is the problem clearly defined and significant?",
                "Is the solution innovative and defensible?"
            ],
            BerkusFactor.PROTOTYPE: [
                "Does a working prototype exist?",
                "Has the technology been validated?",
                "Are key technical risks mitigated?"
            ],
            BerkusFactor.QUALITY_MANAGEMENT: [
                "Does the team have relevant industry experience?",
                "Have founders successfully built companies before?",
                "Is the team complete with necessary skills?"
            ],
            BerkusFactor.STRATEGIC_RELATIONSHIPS: [
                "Are there signed LOIs or partnerships?",
                "Does the startup have strategic investors?",
                "Are there channel partners committed?"
            ],
            BerkusFactor.PRODUCT_ROLLOUT: [
                "Is there evidence of product-market fit?",
                "Are there paying customers or users?",
                "Is there positive market feedback?"
            ]
        }

        return {
            'method': 'Berkus Method - Guided Assessment',
            'assessment_framework': assessment_questions,
            'scoring_guide': {
                '0.0-0.2': 'None/Minimal',
                '0.2-0.4': 'Weak',
                '0.4-0.6': 'Moderate',
                '0.6-0.8': 'Strong',
                '0.8-1.0': 'Excellent'
            }
        }

if __name__ == '__main__':
    berkus = BerkusMethod(max_value_per_factor=500_000)

    valuation = berkus.quick_assessment(
        idea_score=0.8,
        prototype_score=0.6,
        team_score=0.9,
        relationships_score=0.4,
        rollout_score=0.3
    )

    print(f"=== BERKUS METHOD VALUATION ===\n")
    print(f"Total Valuation: ${valuation['total_valuation']:,.0f}")
    print(f"Percentage of Max: {valuation['valuation_percentage']:.1f}%\n")

    print("Factor Breakdown:")
    for factor, data in valuation['factor_assessments'].items():
        print(f"  {factor.replace('_', ' ').title()}: ${data['value']:,.0f} (score: {data['score']:.1f})")
