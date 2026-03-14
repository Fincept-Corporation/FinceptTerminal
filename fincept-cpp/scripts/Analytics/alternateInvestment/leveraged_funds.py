"""
Leveraged Funds Module - 2x, 3x ETFs with daily rebalancing

Analysis: Leveraged Funds (THE UGLY)
Rating: 1/10 - Volatility decay destroys value; not for investors
"""

from decimal import Decimal, getcontext
from typing import Dict, Any, List
from config import AssetParameters, AssetClass
from base_analytics import AlternativeInvestmentBase

getcontext().prec = 28


class LeveragedFundAnalyzer(AlternativeInvestmentBase):
    """
    THE UGLY
    - 2x, 3x leveraged ETFs
    - Daily rebalancing causes volatility decay
    - Path-dependent returns
    - NOT for buy-and-hold investors
    - Only for day traders
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.leverage_ratio = getattr(parameters, 'leverage_ratio', Decimal('2.0'))  # 2x
        self.expense_ratio = getattr(parameters, 'expense_ratio', Decimal('0.0095'))  # 0.95%
        self.tracking_error = getattr(parameters, 'tracking_error', Decimal('0.005'))  # Additional cost

    def volatility_decay_example(self) -> Dict[str, Any]:
        """
        Demonstrate volatility decay

        Key insight: Daily rebalancing + volatility = GUARANTEED underperformance
        """
        # Example: Market goes +10%, -10%, +10%, -10%
        # Unleveraged: 1.10 * 0.90 * 1.10 * 0.90 = 0.9801 (-1.99%)
        # 2x Leveraged: 1.20 * 0.80 * 1.20 * 0.80 = 0.9216 (-7.84%)

        scenarios = [
            {'name': 'Up 10%, Down 10% (repeat)', 'market': [0.10, -0.10, 0.10, -0.10]},
            {'name': 'Up 20%, Down 15%', 'market': [0.20, -0.15]},
        ]

        results = []
        for scenario in scenarios:
            unlev_value = Decimal('1.0')
            lev_value = Decimal('1.0')

            for ret in scenario['market']:
                market_ret = Decimal(str(ret))
                unlev_value *= (Decimal('1') + market_ret)
                lev_value *= (Decimal('1') + market_ret * self.leverage_ratio)

            results.append({
                'scenario': scenario['name'],
                'unleveraged_final': float(unlev_value),
                'leveraged_final': float(lev_value),
                'unleveraged_return': float(unlev_value - Decimal('1')),
                'leveraged_return': float(lev_value - Decimal('1')),
                'decay': float((unlev_value - Decimal('1')) * self.leverage_ratio - (lev_value - Decimal('1')))
            })

        return {
            'leverage_ratio': float(self.leverage_ratio),
            'scenarios': results,
            'analysis_lesson': 'Volatility decay means leveraged funds ALWAYS underperform over time'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        return {
            'product': 'Leveraged Funds (2x, 3x ETFs)',
            'category': 'THE UGLY',
            'rating': '1/10',
            'analysis_summary': 'Volatility decay destroys value - NOT for investors',
            'key_problems': [
                'VOLATILITY DECAY: Daily rebalancing + volatility = guaranteed underperformance',
                'PATH DEPENDENT: Returns depend on sequence, not just start/end',
                'HIGH COSTS: Expense ratios 0.75-1.5% + trading costs',
                'TRACKING ERROR: Often fail to match stated leverage',
                'ONLY FOR DAY TRADERS: Designed for intraday speculation, not investing',
                'MISUNDERSTOOD: Investors think 2x = double returns (WRONG)'
            ],
            'proof_in_pudding': 'Study after study shows leveraged ETFs lose money for buy-and-hold',
            'analysis_quotes': [
                '"Leveraged ETFs are weapons of wealth destruction"',
                '"Daily rebalancing + volatility = decay"',
                '"If you hold overnight, you\'re doing it wrong"'
            ],
            'who_might_use': 'Professional day traders ONLY (not investors)',
            'analysis_recommendation': 'NEVER hold leveraged ETFs - avoid entirely',
            'better_alternative': 'Want leverage? Use futures or options (cheaper, more efficient)',
            'bottom_line': 'Leveraged ETFs guaranteed to underperform due to math - stay away'
        }

    def calculate_nav(self) -> Decimal:
        return Decimal('10000')  # Placeholder

    def calculate_key_metrics(self) -> Dict[str, Any]:
        return {
            'leverage_ratio': float(self.leverage_ratio),
            'expense_ratio': float(self.expense_ratio),
            'volatility_decay_demo': self.volatility_decay_example(),
            'analysis_category': 'THE UGLY'
        }


__all__ = ['LeveragedFundAnalyzer']
