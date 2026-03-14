"""Sensitivity Analysis for Merger Models"""
import sys
from pathlib import Path
from typing import Dict, Any, List
import numpy as np

# Add Analytics path for absolute imports
analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

# Use absolute imports instead of relative imports
from corporateFinance.merger_models.pro_forma_builder import ProFormaBuilder

class SensitivityAnalyzer:
    """Perform sensitivity analysis on merger model assumptions"""

    def __init__(self, pro_forma_builder: ProFormaBuilder):
        self.builder = pro_forma_builder
        self.original_synergies = pro_forma_builder.synergies
        self.original_integration_costs = pro_forma_builder.integration_costs

    def synergy_sensitivity(self, min_synergies: float, max_synergies: float,
                          steps: int = 11) -> Dict[str, Any]:
        """Analyze EPS sensitivity to synergy assumptions"""

        synergy_values = np.linspace(min_synergies, max_synergies, steps)
        results = []

        for synergy in synergy_values:
            self.builder.synergies = synergy
            ad_result = self.builder.calculate_accretion_dilution()

            results.append({
                'synergies': synergy,
                'pro_forma_eps': ad_result['pro_forma_eps'],
                'accretion_dilution_pct': ad_result['accretion_dilution_pct']
            })

        self.builder.synergies = self.original_synergies

        return {
            'sensitivity_data': results,
            'synergy_range': [min_synergies, max_synergies],
            'base_case': self.original_synergies
        }

    def purchase_price_sensitivity(self, base_price: float, price_range_pct: float = 0.20,
                                  steps: int = 11) -> Dict[str, Any]:
        """Analyze EPS sensitivity to purchase price"""

        min_price = base_price * (1 - price_range_pct)
        max_price = base_price * (1 + price_range_pct)
        prices = np.linspace(min_price, max_price, steps)

        results = []

        for price in prices:
            goodwill_adjustment = price - base_price
            original_goodwill = self.builder.goodwill

            self.builder.goodwill = original_goodwill + goodwill_adjustment

            ad_result = self.builder.calculate_accretion_dilution()

            results.append({
                'purchase_price': price,
                'pro_forma_eps': ad_result['pro_forma_eps'],
                'accretion_dilution_pct': ad_result['accretion_dilution_pct']
            })

            self.builder.goodwill = original_goodwill

        return {
            'sensitivity_data': results,
            'price_range': [min_price, max_price],
            'base_case': base_price
        }

    def two_way_sensitivity(self, var1_name: str, var1_range: List[float],
                          var2_name: str, var2_range: List[float]) -> Dict[str, Any]:
        """Two-way sensitivity table"""

        matrix = []

        for v1 in var1_range:
            row = []
            for v2 in var2_range:
                if var1_name == 'synergies':
                    self.builder.synergies = v1
                if var2_name == 'synergies':
                    self.builder.synergies = v2

                ad_result = self.builder.calculate_accretion_dilution()
                row.append(ad_result['accretion_dilution_pct'])

            matrix.append(row)

        self.builder.synergies = self.original_synergies

        return {
            'matrix': matrix,
            'var1_name': var1_name,
            'var1_range': var1_range,
            'var2_name': var2_name,
            'var2_range': var2_range
        }
