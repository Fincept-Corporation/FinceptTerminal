"""
Asset Location Module

Tax-efficient placement of assets across account types

CFA Standards: After-tax returns, Tax alpha

Key Concepts:
- Tax-inefficient assets → Tax-deferred accounts
- Tax-efficient assets → Taxable accounts
- Municipal bonds → Taxable (already tax-free)
- REITs → Tax-deferred (high dividend yield)
"""

from decimal import Decimal, getcontext
from typing import Dict, Any, List, Tuple
from enum import Enum
import logging

logger = logging.getLogger(__name__)
getcontext().prec = 28


class AccountType(Enum):
    """Account types for asset location"""
    TAXABLE = "taxable"
    TAX_DEFERRED = "tax_deferred"  # 401k, Traditional IRA
    TAX_FREE = "tax_free"  # Roth IRA, Roth 401k


class AssetTaxProfile(Enum):
    """Tax efficiency of asset class"""
    VERY_TAX_EFFICIENT = "very_efficient"  # Tax-managed equity, I Bonds
    TAX_EFFICIENT = "efficient"  # Index funds, municipal bonds
    TAX_NEUTRAL = "neutral"  # Balanced funds
    TAX_INEFFICIENT = "inefficient"  # Taxable bonds, actively managed
    VERY_TAX_INEFFICIENT = "very_inefficient"  # REITs, commodities, high-yield bonds


class AssetLocationAnalyzer:
    """
    Asset Location Analysis for Tax Optimization

    Rules:
    1. Place tax-inefficient assets in tax-advantaged accounts
    2. Place tax-efficient assets in taxable accounts
    3. Exceptions: Municipal bonds (already tax-free)
    4. REITs and commodities → Tax-deferred
    5. International stocks → Consider taxable (foreign tax credit)

    Verdict: Essential for wealth maximization
    Value: Can add 0.10% - 0.75% annual return
    """

    def __init__(self, tax_bracket: Decimal = Decimal('0.24')):
        """
        Initialize Asset Location Analyzer

        Args:
            tax_bracket: Federal marginal tax rate (e.g., 0.24 for 24%)
        """
        self.tax_bracket = tax_bracket
        self.ltcg_rate = Decimal('0.15')  # Long-term capital gains rate (15% typical)
        self.qualified_dividend_rate = Decimal('0.15')  # Qualified dividend rate
        self.state_tax_rate = Decimal('0.05')  # Average state tax (5%)

    def tax_efficiency_score(self, asset_class: str) -> Tuple[AssetTaxProfile, Decimal]:
        """
        Determine tax efficiency of asset class

        Returns:
            (AssetTaxProfile, tax_drag_estimate)
        """
        tax_profiles = {
            # Very Tax-Efficient (0-0.3% tax drag)
            'tax_managed_equity': (AssetTaxProfile.VERY_TAX_EFFICIENT, Decimal('0.002')),
            'i_bonds': (AssetTaxProfile.VERY_TAX_EFFICIENT, Decimal('0.001')),
            'tips': (AssetTaxProfile.VERY_TAX_EFFICIENT, Decimal('0.003')),
            'municipal_bonds': (AssetTaxProfile.VERY_TAX_EFFICIENT, Decimal('0.0')),

            # Tax-Efficient (0.3-0.8% tax drag)
            'equity_index_fund': (AssetTaxProfile.TAX_EFFICIENT, Decimal('0.005')),
            'total_market_index': (AssetTaxProfile.TAX_EFFICIENT, Decimal('0.006')),
            'international_equity': (AssetTaxProfile.TAX_EFFICIENT, Decimal('0.007')),

            # Tax-Neutral (0.8-1.5% tax drag)
            'balanced_fund': (AssetTaxProfile.TAX_NEUTRAL, Decimal('0.012')),
            'value_stocks': (AssetTaxProfile.TAX_NEUTRAL, Decimal('0.010')),

            # Tax-Inefficient (1.5-2.5% tax drag)
            'taxable_bonds': (AssetTaxProfile.TAX_INEFFICIENT, Decimal('0.020')),
            'corporate_bonds': (AssetTaxProfile.TAX_INEFFICIENT, Decimal('0.022')),
            'active_equity': (AssetTaxProfile.TAX_INEFFICIENT, Decimal('0.018')),

            # Very Tax-Inefficient (2.5%+ tax drag)
            'reits': (AssetTaxProfile.VERY_TAX_INEFFICIENT, Decimal('0.030')),
            'high_yield_bonds': (AssetTaxProfile.VERY_TAX_INEFFICIENT, Decimal('0.035')),
            'commodities': (AssetTaxProfile.VERY_TAX_INEFFICIENT, Decimal('0.028')),
            'actively_managed': (AssetTaxProfile.VERY_TAX_INEFFICIENT, Decimal('0.025')),
        }

        return tax_profiles.get(asset_class.lower().replace(' ', '_'),
                                (AssetTaxProfile.TAX_NEUTRAL, Decimal('0.012')))

    def optimal_location(self, asset_class: str) -> Dict[str, Any]:
        """Determine optimal account location for asset class"""
        profile, tax_drag = self.tax_efficiency_score(asset_class)

        # Location priority rules
        location_rules = {
            AssetTaxProfile.VERY_TAX_EFFICIENT: {
                'priority_1': AccountType.TAXABLE,
                'reason': 'Already tax-efficient, preserve tax-deferred space',
                'analysis_quote': 'Keep tax-efficient assets in taxable accounts'
            },
            AssetTaxProfile.TAX_EFFICIENT: {
                'priority_1': AccountType.TAXABLE,
                'priority_2': AccountType.TAX_DEFERRED,
                'reason': 'Low tax drag, can go in taxable',
                'analysis_quote': 'Index funds work well in taxable accounts'
            },
            AssetTaxProfile.TAX_NEUTRAL: {
                'priority_1': AccountType.TAX_DEFERRED,
                'priority_2': AccountType.TAXABLE,
                'reason': 'Moderate tax drag',
                'analysis_quote': 'Consider tax-deferred if space available'
            },
            AssetTaxProfile.TAX_INEFFICIENT: {
                'priority_1': AccountType.TAX_DEFERRED,
                'priority_2': AccountType.TAX_FREE,
                'reason': 'High tax drag, shelter from taxes',
                'analysis_quote': 'Tax-inefficient assets belong in tax-deferred accounts'
            },
            AssetTaxProfile.VERY_TAX_INEFFICIENT: {
                'priority_1': AccountType.TAX_DEFERRED,
                'priority_2': AccountType.TAX_FREE,
                'priority_3': AccountType.TAXABLE,
                'reason': 'Very high tax drag, must shelter',
                'analysis_quote': 'REITs and commodities → tax-deferred only'
            }
        }

        rule = location_rules[profile]

        return {
            'asset_class': asset_class,
            'tax_profile': profile.value,
            'estimated_tax_drag': float(tax_drag),
            'optimal_account': rule['priority_1'].value,
            'alternative_accounts': [v.value for k, v in rule.items() if k.startswith('priority') and k != 'priority_1'],
            'reason': rule['reason'],
            'analysis_guidance': rule['analysis_quote']
        }

    def location_value_added(self, asset_value: Decimal, asset_class: str,
                            years: int = 30) -> Dict[str, Any]:
        """
        Calculate value of optimal vs suboptimal location

        Compares:
        - Optimal location (tax-efficient placement)
        - Suboptimal location (all in taxable)
        """
        profile, tax_drag = self.tax_efficiency_score(asset_class)
        annual_return = Decimal('0.08')  # 8% baseline return

        # Optimal location: shelter tax-inefficient assets
        if profile in [AssetTaxProfile.TAX_INEFFICIENT, AssetTaxProfile.VERY_TAX_INEFFICIENT]:
            # Tax-deferred growth
            optimal_value = asset_value * ((Decimal('1') + annual_return) ** years)
            # Tax on withdrawal at ordinary income rate
            after_tax_optimal = optimal_value * (Decimal('1') - self.tax_bracket)
        else:
            # Tax-efficient in taxable
            after_tax_return = annual_return - tax_drag
            optimal_value = asset_value * ((Decimal('1') + after_tax_return) ** years)
            # Already after-tax
            after_tax_optimal = optimal_value

        # Suboptimal: everything in taxable with full tax drag
        suboptimal_return = annual_return - tax_drag
        suboptimal_value = asset_value * ((Decimal('1') + suboptimal_return) ** years)

        # Value added
        value_added = after_tax_optimal - suboptimal_value
        value_added_pct = (value_added / suboptimal_value) * Decimal('100')

        # Annual alpha
        annual_alpha = (((after_tax_optimal / suboptimal_value) ** (Decimal('1') / Decimal(str(years)))) - Decimal('1')) * Decimal('100')

        return {
            'years': years,
            'initial_value': float(asset_value),
            'optimal_location_value': float(after_tax_optimal),
            'suboptimal_location_value': float(suboptimal_value),
            'value_added': float(value_added),
            'value_added_percent': float(value_added_pct),
            'annual_alpha_bps': float(annual_alpha * Decimal('100')),  # basis points
            'analysis_note': f'Asset location can add {float(annual_alpha):.2f}% annually'
        }

    def portfolio_location_strategy(self, portfolio: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Optimal location strategy for entire portfolio

        Input format:
        [
            {'asset_class': 'reits', 'value': 50000},
            {'asset_class': 'equity_index_fund', 'value': 200000},
            ...
        ]

        Returns account allocation recommendations
        """
        # Categorize assets by tax efficiency
        very_inefficient = []
        inefficient = []
        neutral = []
        efficient = []
        very_efficient = []

        total_value = Decimal('0')

        for asset in portfolio:
            profile, tax_drag = self.tax_efficiency_score(asset['asset_class'])
            value = Decimal(str(asset['value']))
            total_value += value

            asset_info = {
                'asset_class': asset['asset_class'],
                'value': value,
                'tax_drag': tax_drag
            }

            if profile == AssetTaxProfile.VERY_TAX_INEFFICIENT:
                very_inefficient.append(asset_info)
            elif profile == AssetTaxProfile.TAX_INEFFICIENT:
                inefficient.append(asset_info)
            elif profile == AssetTaxProfile.TAX_NEUTRAL:
                neutral.append(asset_info)
            elif profile == AssetTaxProfile.TAX_EFFICIENT:
                efficient.append(asset_info)
            else:
                very_efficient.append(asset_info)

        # Location priority
        location_plan = {
            'tax_deferred_allocation': very_inefficient + inefficient,
            'tax_free_allocation': [],  # Highest expected return assets
            'taxable_allocation': very_efficient + efficient + neutral
        }

        return {
            'total_portfolio_value': float(total_value),
            'location_strategy': {
                'tax_deferred': [{'asset': a['asset_class'], 'value': float(a['value'])}
                                for a in location_plan['tax_deferred_allocation']],
                'taxable': [{'asset': a['asset_class'], 'value': float(a['value'])}
                           for a in location_plan['taxable_allocation']]
            },
            'analysis_rules': [
                'Place REITs and bonds in tax-deferred',
                'Place equity index funds in taxable',
                'Municipal bonds → taxable (already tax-free)',
                'International stocks → consider taxable for foreign tax credit'
            ]
        }

    def municipal_bond_decision(self, municipal_yield: Decimal,
                                taxable_yield: Decimal) -> Dict[str, Any]:
        """
        Decide between municipal and taxable bonds

        Tax-equivalent yield = Muni Yield / (1 - Tax Rate)
        """
        total_tax_rate = self.tax_bracket + self.state_tax_rate
        tax_equivalent_yield = municipal_yield / (Decimal('1') - total_tax_rate)

        recommendation = 'Municipal' if tax_equivalent_yield > taxable_yield else 'Taxable'

        return {
            'municipal_yield': float(municipal_yield),
            'taxable_yield': float(taxable_yield),
            'tax_rate': float(total_tax_rate),
            'tax_equivalent_yield': float(tax_equivalent_yield),
            'recommendation': recommendation,
            'value_difference_bps': float((tax_equivalent_yield - taxable_yield) * Decimal('10000')),
            'analysis_guidance': 'Municipal bonds if in 24%+ tax bracket'
        }

    def foreign_tax_credit_analysis(self, foreign_dividend_yield: Decimal,
                                    foreign_tax_withheld: Decimal = Decimal('0.15')) -> Dict[str, Any]:
        """
        Analyze foreign tax credit benefit in taxable accounts

        Key: Foreign tax credit only available in taxable accounts
        """
        # In taxable: can claim foreign tax credit
        taxable_benefit = foreign_tax_withheld * foreign_dividend_yield

        # In tax-deferred: lose foreign tax credit
        tax_deferred_loss = foreign_tax_withheld * foreign_dividend_yield

        return {
            'foreign_dividend_yield': float(foreign_dividend_yield),
            'foreign_tax_withheld_rate': float(foreign_tax_withheld),
            'annual_tax_credit_in_taxable': float(taxable_benefit),
            'annual_tax_loss_in_deferred': float(tax_deferred_loss),
            'recommendation': 'Place international stocks in taxable for tax credit',
            'analysis_note': 'Foreign tax credit is reason to hold intl stocks in taxable'
        }

    def analysis_verdict(self) -> Dict[str, Any]:
        """verdict on Asset Location"""
        return {
            'strategy': 'Asset Location (Tax-Efficient Placement)',
            'category': 'THE ESSENTIAL',
            'importance': '10/10',
            'value_added': '0.10% - 0.75% annually',
            'key_rules': [
                'Tax-inefficient assets → Tax-deferred accounts',
                'Tax-efficient assets → Taxable accounts',
                'REITs, bonds, commodities → Tax-deferred ONLY',
                'Equity index funds → Taxable acceptable',
                'Municipal bonds → Taxable (already tax-free)',
                'International stocks → Consider taxable (foreign tax credit)'
            ],
            'analysis_quote': '"Asset location is free money. There is no reason not to do this optimally."',
            'priority': 'Implement immediately after asset allocation decision'
        }


# Export
__all__ = ['AssetLocationAnalyzer', 'AccountType', 'AssetTaxProfile']
