"""
SRI (Socially Responsible Investing) Funds Module

Mutual funds with ethical/social screening criteria

CFA Standards: ESG Investing, Portfolio Construction

Key Concepts: - Screens (negative): Exclude tobacco, alcohol, gambling, weapons
- Screens (positive): Include renewable energy, diversity, labor practices
- Performance impact of constraints
- Reduced diversification from screening
- Higher expense ratios

Verdict: Mixed - Values-based investing acceptable if willing to pay cost
Rating: 6/10 - Personal choice, but expect potential underperformance
"""

from decimal import Decimal, getcontext
from typing import Dict, Any, List
from datetime import datetime
import logging

from config import AssetParameters, AssetClass
from base_analytics import AlternativeInvestmentBase

logger = logging.getLogger(__name__)
getcontext().prec = 28


class SRIFundAnalyzer(AlternativeInvestmentBase):
    """
    Socially Responsible Investing (SRI) Fund Analyzer

    1. Diversification Cost: Screening excludes sectors/companies
    2. Performance: Historically slight underperformance vs broad market
    3. Expense Ratios: Higher costs (50-100 bps more)
    4. Screening Criteria: Negative (exclude) vs Positive (include)
    5. Investor Choice: Aligning values with portfolio acceptable if willing to pay cost

    Verdict: "Mixed" - Personal choice
    Rating: 6/10 - Acceptable if values alignment worth potential cost
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)

        # Fund characteristics
        self.fund_name = getattr(parameters, 'name', 'SRI Fund')
        self.expense_ratio = getattr(parameters, 'expense_ratio', Decimal('0.0080'))  # 80 bps typical
        self.benchmark_expense = getattr(parameters, 'benchmark_expense', Decimal('0.0015'))  # 15 bps index fund

        # Screening criteria
        self.negative_screens = getattr(parameters, 'negative_screens', [])  # Exclusions
        self.positive_screens = getattr(parameters, 'positive_screens', [])  # Inclusions

        # Performance
        self.fund_return = getattr(parameters, 'fund_return', Decimal('0.09'))
        self.benchmark_return = getattr(parameters, 'benchmark_return', Decimal('0.10'))

        # Portfolio characteristics
        self.num_holdings = getattr(parameters, 'num_holdings', 100)
        self.benchmark_holdings = getattr(parameters, 'benchmark_holdings', 500)

        # Sector exclusions estimate
        self.excluded_sectors_pct = getattr(parameters, 'excluded_sectors_pct', Decimal('0.15'))  # 15% of market excluded

    def screening_impact_analysis(self) -> Dict[str, Any]:
        """
        Analyze impact of screening criteria on diversification

        Key insight: Screening reduces diversification and may hurt returns
        """
        # Typical SRI screens
        common_negative_screens = [
            'Tobacco',
            'Alcohol',
            'Gambling',
            'Weapons/Defense',
            'Nuclear Power',
            'Fossil Fuels',
            'Adult Entertainment'
        ]

        common_positive_screens = [
            'Renewable Energy',
            'Gender Diversity',
            'Labor Practices',
            'Environmental Protection',
            'Community Development'
        ]

        # Diversification reduction
        diversification_ratio = Decimal(str(self.num_holdings)) / Decimal(str(self.benchmark_holdings))

        # Market exclusion
        excluded_market_cap = self.excluded_sectors_pct

        return {
            'common_negative_screens': common_negative_screens,
            'common_positive_screens': common_positive_screens,
            'fund_holdings': self.num_holdings,
            'benchmark_holdings': self.benchmark_holdings,
            'diversification_ratio': float(diversification_ratio),
            'estimated_market_excluded': float(excluded_market_cap),
            'diversification_cost': 'Reduced exposure to 10-20% of market',
            'analysis_warning': 'Screening constraints reduce diversification'
        }

    def performance_comparison(self, years: int = 10) -> Dict[str, Any]:
        """
        Compare SRI fund performance to broad market

        Finding: SRI funds have historically underperformed slightly
        Reasons: Higher costs + diversification constraints
        """
        # Annual returns
        sri_return = self.fund_return
        benchmark_return = self.benchmark_return

        # Compound over period
        sri_value = Decimal('10000') * ((Decimal('1') + sri_return) ** years)
        benchmark_value = Decimal('10000') * ((Decimal('1') + benchmark_return) ** years)

        # Performance gap
        underperformance = benchmark_return - sri_return
        terminal_value_diff = benchmark_value - sri_value

        # Attribute to costs vs screening
        expense_diff = self.expense_ratio - self.benchmark_expense
        screening_cost_estimate = underperformance - expense_diff

        return {
            'time_period_years': years,
            'sri_fund_return': float(sri_return),
            'benchmark_return': float(benchmark_return),
            'annual_underperformance': float(underperformance),
            'terminal_value_sri': float(sri_value),
            'terminal_value_benchmark': float(benchmark_value),
            'cumulative_shortfall': float(terminal_value_diff),
            'attribution': {
                'expense_ratio_drag': float(expense_diff),
                'screening_cost_estimate': float(screening_cost_estimate),
                'note': 'Screening cost is rough estimate'
            },
            'analysis_finding': 'SRI funds tend to underperform by 0.5-1.5% annually',
            'caveat': 'Performance varies widely; some SRI funds match benchmarks'
        }

    def expense_ratio_analysis(self) -> Dict[str, Any]:
        """
        Analyze expense ratio impact

        Key insight: SRI funds typically charge 50-100 bps more than index funds
        """
        expense_premium = self.expense_ratio - self.benchmark_expense

        # Cost over time
        years_list = [10, 20, 30]
        cost_analysis = []

        for years in years_list:
            # Assume same gross return
            gross_return = Decimal('0.10')

            sri_net = gross_return - self.expense_ratio
            benchmark_net = gross_return - self.benchmark_expense

            sri_value = Decimal('10000') * ((Decimal('1') + sri_net) ** years)
            benchmark_value = Decimal('10000') * ((Decimal('1') + benchmark_net) ** years)

            cost_of_expenses = benchmark_value - sri_value

            cost_analysis.append({
                'years': years,
                'sri_value': float(sri_value),
                'benchmark_value': float(benchmark_value),
                'cost_of_higher_expenses': float(cost_of_expenses),
                'percentage_loss': float((cost_of_expenses / benchmark_value) * Decimal('100'))
            })

        return {
            'sri_expense_ratio': float(self.expense_ratio),
            'benchmark_expense_ratio': float(self.benchmark_expense),
            'expense_premium': float(expense_premium),
            'expense_premium_bps': float(expense_premium * Decimal('10000')),
            'cost_over_time': cost_analysis,
            'analysis_note': '50-100 bps higher expenses common in SRI funds'
        }

    def values_alignment_framework(self) -> Dict[str, Any]:
        """
        Framework for deciding if SRI is right for investor

        Key insight: SRI is personal choice - acceptable if values matter more than returns
        """
        return {
            'considerations': {
                'financial_cost': {
                    'higher_expenses': '50-100 bps typical',
                    'potential_underperformance': '0.5-1.5% annually',
                    'reduced_diversification': '10-20% of market excluded',
                    'cumulative_impact': 'Can be significant over decades'
                },
                'values_benefit': {
                    'portfolio_alignment': 'Investments match personal values',
                    'corporate_influence': 'Vote with your dollars',
                    'peace_of_mind': 'Avoid ethical conflicts',
                    'impact_investing': 'Support positive change'
                }
            },
            'decision_framework': [
                '1. Identify your core values and priorities',
                '2. Understand the financial cost (expenses + potential underperformance)',
                '3. Assess if values alignment is worth the cost',
                '4. If yes: Choose low-cost SRI fund with clear criteria',
                '5. If no: Consider non-investment ways to support causes'
            ],
            'analysis_guidance': 'SRI is personal choice - no right/wrong answer',
            'alternatives_to_sri': [
                'Invest in low-cost index funds + donate profits to causes',
                'Direct charitable giving (tax deduction)',
                'Volunteer time to causes',
                'Vote proxies on ESG issues',
                'Targeted impact investing (venture, private equity)'
            ]
        }

    def compare_sri_approaches(self) -> Dict[str, Any]:
        """
        Compare different approaches to socially responsible investing

        discusses multiple paths to align values with investments
        """
        return {
            'approach_1_sri_mutual_funds': {
                'method': 'Buy SRI mutual fund',
                'pros': [
                    'Simple - one fund',
                    'Professional screening',
                    'Diversified within constraints'
                ],
                'cons': [
                    'Higher expenses (50-100 bps)',
                    'Reduced diversification',
                    'Potential underperformance',
                    'Limited control over screening'
                ],
                'cost': 'High (expenses + underperformance)'
            },
            'approach_2_index_plus_donate': {
                'method': 'Index fund + charitable giving',
                'pros': [
                    'Low cost (index expenses)',
                    'Full diversification',
                    'Market returns',
                    'Tax deduction for donations',
                    'Direct impact on causes'
                ],
                'cons': [
                    'Portfolio not "clean"',
                    'Requires separate donation decision'
                ],
                'cost': 'Low',
                'analysis_preference': 'This approach often superior'
            },
            'approach_3_direct_exclusion': {
                'method': 'DIY screening - exclude specific stocks',
                'pros': [
                    'Full control',
                    'Precise values alignment'
                ],
                'cons': [
                    'Very high transaction costs',
                    'Difficult to diversify',
                    'Rebalancing complexity',
                    'Not practical for most'
                ],
                'cost': 'Very high'
            },
            'approach_4_proxy_voting': {
                'method': 'Index fund + active proxy voting',
                'pros': [
                    'Low cost',
                    'Full diversification',
                    'Shareholder influence',
                    'No performance drag'
                ],
                'cons': [
                    'Limited impact',
                    'Time consuming'
                ],
                'cost': 'Zero'
            },
            'analysis_recommendation': 'Approach 2 (Index + Donate) often most efficient'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """verdict on SRI funds"""
        return {
            'strategy': 'Socially Responsible Investing (SRI) Funds',
            'category': 'MIXED - Personal Choice',
            'rating': '6/10',
            'analysis_summary': 'Values-based investing acceptable if cost understood',
            'pros': [
                'Align investments with personal values',
                'Avoid ethical conflicts',
                'Support positive corporate behavior',
                'Peace of mind for some investors'
            ],
            'cons': [
                'Higher expense ratios (50-100 bps typical)',
                'Reduced diversification (exclude 10-20% of market)',
                'Historical slight underperformance vs broad market',
                'Screening criteria may not match your values',
                'Performance drag compounds over time'
            ],
            'analysis_position': 'No judgment - personal choice',
            'key_insight': 'Financial cost vs values benefit - investor must decide',
            'analysis_quote': '"If your values are more important than maximizing returns, SRI is acceptable. Just understand the cost."',
            'alternatives': [
                'Index fund + donate excess returns (often more efficient)',
                'Direct impact investing in specific ventures',
                'Proxy voting on ESG issues',
                'Non-investment activism'
            ],
            'bottom_line': 'SRI acceptable if values > returns, but alternatives may be more efficient'
        }

    def calculate_nav(self) -> Decimal:
        """Current NAV estimate"""
        # Use acquisition price or default
        return getattr(self, 'book_value', Decimal('10000'))

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Key SRI fund metrics"""
        return {
            'fund_characteristics': {
                'expense_ratio': float(self.expense_ratio),
                'num_holdings': self.num_holdings,
                'excluded_market_pct': float(self.excluded_sectors_pct)
            },
            'screening_analysis': self.screening_impact_analysis(),
            'performance_comparison': self.performance_comparison(),
            'expense_analysis': self.expense_ratio_analysis(),
            'decision_framework': self.values_alignment_framework(),
            'approach_comparison': self.compare_sri_approaches(),
            'analysis_category': 'MIXED - Personal Choice'
        }

    def valuation_summary(self) -> Dict[str, Any]:
        """Valuation summary"""
        return {
            'fund_overview': {
                'type': 'SRI Mutual Fund',
                'fund_name': self.fund_name,
                'expense_ratio': float(self.expense_ratio),
                'holdings': self.num_holdings,
                'negative_screens': len(self.negative_screens) if self.negative_screens else 'Typical (7-10)',
                'positive_screens': len(self.positive_screens) if self.positive_screens else 'Typical (5-8)'
            },
            'key_metrics': self.calculate_key_metrics(),
            'analysis_category': 'MIXED - Personal Choice',
            'recommendation': 'Acceptable if values > returns; consider alternatives'
        }

    def calculate_performance(self) -> Dict[str, Any]:
        """Performance metrics"""
        return {
            'fund_return': float(self.fund_return),
            'benchmark_return': float(self.benchmark_return),
            'underperformance': float(self.benchmark_return - self.fund_return),
            'expense_ratio': float(self.expense_ratio),
            'note': 'Historical underperformance typical but not universal'
        }


# Export
__all__ = ['SRIFundAnalyzer']
