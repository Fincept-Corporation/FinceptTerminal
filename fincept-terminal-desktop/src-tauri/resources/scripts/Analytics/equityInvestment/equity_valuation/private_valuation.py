"""
Equity Investment Private Valuation Module
======================================

Private company valuation methods

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Market price data and trading volume information
  - Industry reports and competitive analysis data
  - Management guidance and analyst estimates
  - Economic indicators affecting equity markets

OUTPUT:
  - Equity valuation models and fair value estimates
  - Fundamental analysis metrics and financial ratios
  - Investment recommendations and target prices
  - Risk assessments and portfolio implications
  - Sector and industry comparative analysis

PARAMETERS:
  - valuation_method: Primary valuation methodology (default: 'DCF')
  - discount_rate: Discount rate for valuation (default: 0.10)
  - terminal_growth: Terminal growth rate assumption (default: 0.025)
  - earnings_multiple: Target earnings multiple (default: 15.0)
  - reporting_currency: Reporting currency (default: 'USD')
"""

import numpy as np
import pandas as pd
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
import warnings

from .base_models import (
    BaseValuationModel, CompanyData, MarketData, ValuationResult,
    ValuationMethod, CalculationEngine, ModelValidator, ValidationError
)


class ValuationApproach(Enum):
    """Private company valuation approaches"""
    INCOME_APPROACH = "income_approach"
    MARKET_APPROACH = "market_approach"
    ASSET_APPROACH = "asset_approach"


class DiscountPremiumType(Enum):
    """Types of discounts and premiums"""
    MARKETABILITY_DISCOUNT = "marketability_discount"
    CONTROL_PREMIUM = "control_premium"
    MINORITY_DISCOUNT = "minority_discount"
    KEY_PERSON_DISCOUNT = "key_person_discount"
    SIZE_DISCOUNT = "size_discount"


@dataclass
class PrivateCompanyData:
    """Private company specific data structure"""
    company_name: str
    industry: str
    annual_revenue: float
    ebitda: float
    net_income: float
    book_value: float
    total_assets: float
    total_debt: float
    working_capital: float
    capex: float
    employee_count: int
    years_in_business: int
    ownership_structure: Dict[str, float]
    key_person_dependency: str  # "High", "Medium", "Low"
    management_quality: str  # "Strong", "Average", "Weak"
    financial_reporting_quality: str  # "Audited", "Reviewed", "Compiled"


@dataclass
class MarketableSecurityData:
    """Comparable public company data"""
    symbol: str
    market_cap: float
    enterprise_value: float
    revenue: float
    ebitda: float
    net_income: float
    price_multiples: Dict[str, float]


@dataclass
class PrivateTransaction:
    """Private company transaction data"""
    transaction_date: str
    target_company: str
    buyer_type: str  # "Strategic", "Financial"
    transaction_value: float
    revenue: float
    ebitda: float
    transaction_multiples: Dict[str, float]
    control_transaction: bool


class PrivateCompanyNormalizer:
    """Normalize private company financials for valuation"""

    def __init__(self):
        self.normalization_categories = [
            'owner_compensation',
            'related_party_transactions',
            'non_recurring_items',
            'non_operating_income',
            'discretionary_expenses'
        ]

    def normalize_earnings(self, reported_earnings: float,
                           adjustments: Dict[str, float]) -> Dict[str, float]:
        """Normalize earnings for valuation purposes"""

        normalized_earnings = reported_earnings
        adjustment_details = {}

        # Owner compensation normalization
        if 'excess_owner_compensation' in adjustments:
            excess_comp = adjustments['excess_owner_compensation']
            normalized_earnings += excess_comp
            adjustment_details['owner_compensation_addback'] = excess_comp

        # Related party adjustments
        if 'related_party_expense_adjustment' in adjustments:
            rp_adj = adjustments['related_party_expense_adjustment']
            normalized_earnings += rp_adj
            adjustment_details['related_party_adjustment'] = rp_adj

        # Non-recurring items
        if 'non_recurring_expenses' in adjustments:
            nr_exp = adjustments['non_recurring_expenses']
            normalized_earnings += nr_exp
            adjustment_details['non_recurring_addback'] = nr_exp

        if 'non_recurring_income' in adjustments:
            nr_inc = adjustments['non_recurring_income']
            normalized_earnings -= nr_inc
            adjustment_details['non_recurring_deduction'] = -nr_inc

        # Discretionary expenses
        if 'discretionary_expenses' in adjustments:
            disc_exp = adjustments['discretionary_expenses']
            normalized_earnings += disc_exp
            adjustment_details['discretionary_addback'] = disc_exp

        # Personal expenses
        if 'personal_expenses' in adjustments:
            pers_exp = adjustments['personal_expenses']
            normalized_earnings += pers_exp
            adjustment_details['personal_expense_addback'] = pers_exp

        return {
            'normalized_earnings': normalized_earnings,
            'reported_earnings': reported_earnings,
            'total_adjustments': normalized_earnings - reported_earnings,
            'adjustment_details': adjustment_details
        }

    def normalize_balance_sheet(self, assets: Dict[str, float],
                                liabilities: Dict[str, float]) -> Dict[str, Any]:
        """Normalize balance sheet items"""

        normalized_assets = assets.copy()
        normalized_liabilities = liabilities.copy()
        adjustments = []

        # Asset adjustments
        # Remove personal assets
        if 'personal_assets' in normalized_assets:
            personal_assets = normalized_assets.pop('personal_assets')
            adjustments.append(f"Removed personal assets: ${personal_assets:,.0f}")

        # Adjust asset values to fair value
        if 'real_estate_book_value' in assets and 'real_estate_market_value' in assets:
            book_value = assets['real_estate_book_value']
            market_value = assets['real_estate_market_value']
            if market_value != book_value:
                normalized_assets['real_estate'] = market_value
                adjustments.append(f"Real estate marked to market: ${market_value - book_value:+,.0f}")

        # Liability adjustments
        # Remove personal liabilities
        if 'personal_liabilities' in normalized_liabilities:
            personal_liabs = normalized_liabilities.pop('personal_liabilities')
            adjustments.append(f"Removed personal liabilities: ${personal_liabs:,.0f}")

        total_normalized_assets = sum(normalized_assets.values())
        total_normalized_liabilities = sum(normalized_liabilities.values())
        normalized_equity = total_normalized_assets - total_normalized_liabilities

        return {
            'normalized_assets': normalized_assets,
            'normalized_liabilities': normalized_liabilities,
            'normalized_equity': normalized_equity,
            'total_assets': total_normalized_assets,
            'total_liabilities': total_normalized_liabilities,
            'adjustments': adjustments
        }


class IncomeApproachValuator(BaseValuationModel):
    """Income approach valuation for private companies"""

    def __init__(self):
        super().__init__("Income Approach", "Income-based private company valuation")
        self.valuation_method = ValuationMethod.PRIVATE_INCOME

    def validate_inputs(self, **kwargs) -> bool:
        """Validate income approach inputs"""
        normalized_earnings = kwargs.get('normalized_earnings')
        discount_rate = kwargs.get('discount_rate')

        if normalized_earnings is None or discount_rate is None:
            raise ValidationError("Normalized earnings and discount rate required")

        ModelValidator.validate_percentage(discount_rate, "Discount rate")
        return True

    def calculate_capitalized_earnings_value(self, normalized_earnings: float,
                                             capitalization_rate: float,
                                             growth_rate: float = 0) -> float:
        """Calculate value using capitalized earnings method"""

        if capitalization_rate <= growth_rate:
            raise ValidationError("Capitalization rate must be greater than growth rate")

        if growth_rate == 0:
            # No growth capitalization
            return normalized_earnings / capitalization_rate
        else:
            # Gordon growth model for private companies
            next_year_earnings = normalized_earnings * (1 + growth_rate)
            return next_year_earnings / (capitalization_rate - growth_rate)

    def calculate_dcf_value(self, projected_cash_flows: List[float],
                            discount_rate: float, terminal_value: float = None,
                            terminal_growth: float = None) -> Dict[str, float]:
        """Calculate DCF value for private company"""

        # Present value of projected cash flows
        pv_cash_flows = 0
        for i, cf in enumerate(projected_cash_flows):
            pv_cf = CalculationEngine.present_value(cf, discount_rate, i + 1)
            pv_cash_flows += pv_cf

        # Terminal value
        if terminal_value is None and terminal_growth is not None:
            if len(projected_cash_flows) > 0:
                final_cf = projected_cash_flows[-1]
                terminal_cf = final_cf * (1 + terminal_growth)
                terminal_value = terminal_cf / (discount_rate - terminal_growth)
            else:
                terminal_value = 0
        elif terminal_value is None:
            terminal_value = 0

        # Present value of terminal value
        pv_terminal = CalculationEngine.present_value(terminal_value, discount_rate, len(projected_cash_flows))

        # Total enterprise value
        enterprise_value = pv_cash_flows + pv_terminal

        return {
            'pv_cash_flows': pv_cash_flows,
            'terminal_value': terminal_value,
            'pv_terminal': pv_terminal,
            'enterprise_value': enterprise_value
        }

    def calculate_excess_earnings_value(self, normalized_earnings: float,
                                        tangible_assets: float, required_return_assets: float,
                                        required_return_intangibles: float) -> Dict[str, float]:
        """Calculate value using excess earnings method"""

        # Return attributable to tangible assets
        tangible_asset_return = tangible_assets * required_return_assets

        # Excess earnings (attributable to intangible assets)
        excess_earnings = normalized_earnings - tangible_asset_return

        # Value of intangible assets
        if excess_earnings > 0:
            intangible_value = excess_earnings / required_return_intangibles
        else:
            intangible_value = 0

        # Total business value
        total_value = tangible_assets + intangible_value

        return {
            'tangible_asset_value': tangible_assets,
            'tangible_asset_return': tangible_asset_return,
            'excess_earnings': excess_earnings,
            'intangible_value': intangible_value,
            'total_business_value': total_value
        }

    def calculate_adjusted_present_value(self, unlevered_cash_flows: List[float],
                                         unlevered_discount_rate: float,
                                         tax_shield_values: List[float],
                                         tax_shield_discount_rate: float) -> Dict[str, float]:
        """Calculate APV for private companies with complex capital structures"""

        # Present value of unlevered cash flows
        pv_unlevered_cf = sum(
            CalculationEngine.present_value(cf, unlevered_discount_rate, i + 1)
            for i, cf in enumerate(unlevered_cash_flows)
        )

        # Present value of tax shields
        pv_tax_shields = sum(
            CalculationEngine.present_value(ts, tax_shield_discount_rate, i + 1)
            for i, ts in enumerate(tax_shield_values)
        )

        # Total firm value
        total_firm_value = pv_unlevered_cf + pv_tax_shields

        return {
            'unlevered_firm_value': pv_unlevered_cf,
            'tax_shield_value': pv_tax_shields,
            'total_firm_value': total_firm_value
        }


class MarketApproachValuator(BaseValuationModel):
    """Market approach valuation for private companies"""

    def __init__(self):
        super().__init__("Market Approach", "Market-based private company valuation")
        self.valuation_method = ValuationMethod.PRIVATE_MARKET

    def validate_inputs(self, **kwargs) -> bool:
        """Validate market approach inputs"""
        return True

    def calculate_guideline_public_company_value(self, private_company_metrics: Dict[str, float],
                                                 public_comparables: List[MarketableSecurityData],
                                                 control_premium: float = 0) -> Dict[str, Any]:
        """Calculate value using guideline public company method"""

        if not public_comparables:
            raise ValidationError("At least one public comparable required")

        valuation_multiples = {}

        # Calculate multiples for each comparable
        for comp in public_comparables:
            comp_multiples = {}

            # Revenue multiple
            if comp.revenue > 0:
                comp_multiples['ev_revenue'] = comp.enterprise_value / comp.revenue
                comp_multiples['price_revenue'] = comp.market_cap / comp.revenue

            # EBITDA multiple
            if comp.ebitda > 0:
                comp_multiples['ev_ebitda'] = comp.enterprise_value / comp.ebitda

            # Earnings multiple
            if comp.net_income > 0:
                comp_multiples['price_earnings'] = comp.market_cap / comp.net_income

            valuation_multiples[comp.symbol] = comp_multiples

        # Calculate median multiples
        median_multiples = {}
        for multiple_type in ['ev_revenue', 'ev_ebitda', 'price_earnings', 'price_revenue']:
            values = []
            for comp_multiples in valuation_multiples.values():
                if multiple_type in comp_multiples:
                    values.append(comp_multiples[multiple_type])

            if values:
                median_multiples[multiple_type] = np.median(values)

        # Apply multiples to private company
        indicated_values = {}

        if 'ev_revenue' in median_multiples and 'revenue' in private_company_metrics:
            ev = median_multiples['ev_revenue'] * private_company_metrics['revenue']
            indicated_values['ev_revenue_method'] = ev

        if 'ev_ebitda' in median_multiples and 'ebitda' in private_company_metrics:
            ev = median_multiples['ev_ebitda'] * private_company_metrics['ebitda']
            indicated_values['ev_ebitda_method'] = ev

        if 'price_earnings' in median_multiples and 'net_income' in private_company_metrics:
            equity_value = median_multiples['price_earnings'] * private_company_metrics['net_income']
            indicated_values['price_earnings_method'] = equity_value

        # Apply control premium if applicable
        if control_premium > 0:
            for method, value in indicated_values.items():
                indicated_values[f"{method}_with_control_premium"] = value * (1 + control_premium)

        return {
            'comparable_multiples': valuation_multiples,
            'median_multiples': median_multiples,
            'indicated_values': indicated_values,
            'control_premium_applied': control_premium
        }

    def calculate_guideline_transaction_value(self, private_company_metrics: Dict[str, float],
                                              transaction_comparables: List[PrivateTransaction]) -> Dict[str, Any]:
        """Calculate value using guideline transaction method"""

        if not transaction_comparables:
            raise ValidationError("At least one transaction comparable required")

        transaction_multiples = {}

        # Calculate multiples for each transaction
        for i, txn in enumerate(transaction_comparables):
            txn_multiples = {}

            # Revenue multiple
            if txn.revenue > 0:
                txn_multiples['transaction_revenue'] = txn.transaction_value / txn.revenue

            # EBITDA multiple
            if txn.ebitda > 0:
                txn_multiples['transaction_ebitda'] = txn.transaction_value / txn.ebitda

            transaction_multiples[f"transaction_{i + 1}"] = txn_multiples

        # Calculate median transaction multiples
        median_txn_multiples = {}
        for multiple_type in ['transaction_revenue', 'transaction_ebitda']:
            values = []
            for txn_multiples in transaction_multiples.values():
                if multiple_type in txn_multiples:
                    values.append(txn_multiples[multiple_type])

            if values:
                median_txn_multiples[multiple_type] = np.median(values)

        # Apply to private company
        indicated_values = {}

        if 'transaction_revenue' in median_txn_multiples and 'revenue' in private_company_metrics:
            value = median_txn_multiples['transaction_revenue'] * private_company_metrics['revenue']
            indicated_values['transaction_revenue_method'] = value

        if 'transaction_ebitda' in median_txn_multiples and 'ebitda' in private_company_metrics:
            value = median_txn_multiples['transaction_ebitda'] * private_company_metrics['ebitda']
            indicated_values['transaction_ebitda_method'] = value

        return {
            'transaction_multiples': transaction_multiples,
            'median_transaction_multiples': median_txn_multiples,
            'indicated_values': indicated_values
        }


class AssetApproachValuator(BaseValuationModel):
    """Asset approach valuation for private companies"""

    def __init__(self):
        super().__init__("Asset Approach", "Asset-based private company valuation")
        self.valuation_method = ValuationMethod.PRIVATE_ASSET

    def validate_inputs(self, **kwargs) -> bool:
        """Validate asset approach inputs"""
        return True

    def calculate_adjusted_book_value(self, book_values: Dict[str, float],
                                      fair_value_adjustments: Dict[str, float]) -> Dict[str, float]:
        """Calculate adjusted book value method"""

        adjusted_asset_values = {}
        total_adjustments = 0

        for asset, book_value in book_values.items():
            if asset in fair_value_adjustments:
                fair_value_adj = fair_value_adjustments[asset]
                adjusted_value = book_value + fair_value_adj
                total_adjustments += fair_value_adj
            else:
                adjusted_value = book_value

            adjusted_asset_values[asset] = adjusted_value

        # Calculate total adjusted book value
        total_adjusted_book_value = sum(adjusted_asset_values.values())
        total_book_value = sum(book_values.values())

        return {
            'original_book_value': total_book_value,
            'total_adjustments': total_adjustments,
            'adjusted_book_value': total_adjusted_book_value,
            'asset_adjustments': {asset: adjusted_asset_values[asset] - book_values.get(asset, 0)
                                  for asset in adjusted_asset_values}
        }

    def calculate_replacement_cost_value(self, asset_categories: Dict[str, Dict[str, float]]) -> Dict[str, float]:
        """Calculate replacement cost of assets"""

        replacement_costs = {}
        total_replacement_cost = 0

        for category, assets in asset_categories.items():
            category_cost = 0

            for asset, params in assets.items():
                original_cost = params.get('original_cost', 0)
                current_age = params.get('current_age', 0)
                useful_life = params.get('useful_life', 10)
                inflation_factor = params.get('inflation_factor', 1.0)

                # Calculate replacement cost new
                replacement_cost_new = original_cost * inflation_factor

                # Calculate depreciation
                if useful_life > 0:
                    depreciation_rate = current_age / useful_life
                    depreciation_rate = min(depreciation_rate, 1.0)  # Cap at 100%
                else:
                    depreciation_rate = 0

                # Net replacement cost
                net_replacement_cost = replacement_cost_new * (1 - depreciation_rate)

                category_cost += net_replacement_cost

            replacement_costs[category] = category_cost
            total_replacement_cost += category_cost

        return {
            'category_costs': replacement_costs,
            'total_replacement_cost': total_replacement_cost
        }

    def calculate_liquidation_value(self, assets: Dict[str, float],
                                    liquidation_discounts: Dict[str, float],
                                    liquidation_costs: float = 0) -> Dict[str, float]:
        """Calculate liquidation value of assets"""

        liquidation_values = {}
        total_liquidation_value = 0

        for asset, book_value in assets.items():
            discount = liquidation_discounts.get(asset, 0.5)  # Default 50% discount
            liquidation_value = book_value * (1 - discount)
            liquidation_values[asset] = liquidation_value
            total_liquidation_value += liquidation_value

        # Subtract liquidation costs
        net_liquidation_value = total_liquidation_value - liquidation_costs

        return {
            'gross_liquidation_value': total_liquidation_value,
            'liquidation_costs': liquidation_costs,
            'net_liquidation_value': net_liquidation_value,
            'asset_liquidation_values': liquidation_values
        }


class DiscountPremiumAnalyzer:
    """Analyze discounts and premiums for private company valuations"""

    def __init__(self):
        # Typical discount/premium ranges based on empirical studies
        self.typical_ranges = {
            'marketability_discount': (0.20, 0.40),  # 20-40%
            'control_premium': (0.20, 0.35),  # 20-35%
            'minority_discount': (0.10, 0.25),  # 10-25%
            'key_person_discount': (0.05, 0.20),  # 5-20%
            'size_discount': (0.05, 0.15)  # 5-15%
        }

    def calculate_marketability_discount(self, company_characteristics: Dict[str, Any]) -> Dict[str, float]:
        """Calculate discount for lack of marketability (DLOM)"""

        base_discount = 0.25  # Base 25% discount

        # Adjustments based on company characteristics
        adjustments = 0

        # Financial performance
        profitability = company_characteristics.get('profitability', 'Average')
        if profitability == 'Strong':
            adjustments -= 0.05
        elif profitability == 'Weak':
            adjustments += 0.10

        # Size
        revenue = company_characteristics.get('revenue', 0)
        if revenue > 100_000_000:  # >$100M
            adjustments -= 0.05
        elif revenue < 10_000_000:  # <$10M
            adjustments += 0.05

        # Financial reporting quality
        reporting_quality = company_characteristics.get('financial_reporting_quality', 'Compiled')
        if reporting_quality == 'Audited':
            adjustments -= 0.05
        elif reporting_quality == 'Compiled':
            adjustments += 0.05

        # Growth prospects
        growth_prospects = company_characteristics.get('growth_prospects', 'Average')
        if growth_prospects == 'High':
            adjustments -= 0.05
        elif growth_prospects == 'Low':
            adjustments += 0.05

        # Management quality
        management_quality = company_characteristics.get('management_quality', 'Average')
        if management_quality == 'Strong':
            adjustments -= 0.03
        elif management_quality == 'Weak':
            adjustments += 0.07

        final_discount = max(0.10, min(0.50, base_discount + adjustments))  # Cap between 10-50%

        return {
            'base_discount': base_discount,
            'total_adjustments': adjustments,
            'final_marketability_discount': final_discount,
            'discount_range': self.typical_ranges['marketability_discount']
        }

    def calculate_control_premium(self, ownership_percentage: float,
                                  control_characteristics: Dict[str, Any]) -> Dict[str, float]:
        """Calculate control premium or minority discount"""

        if ownership_percentage >= 0.50:
            # Control position - calculate control premium
            base_premium = 0.25  # Base 25% premium

            # Adjustments
            adjustments = 0

            # Level of control
            if ownership_percentage >= 0.75:
                adjustments += 0.05  # Strong control
            elif ownership_percentage < 0.60:
                adjustments -= 0.05  # Weak control

            # Synergy potential
            synergy_potential = control_characteristics.get('synergy_potential', 'Medium')
            if synergy_potential == 'High':
                adjustments += 0.10
            elif synergy_potential == 'Low':
                adjustments -= 0.05

            # Strategic value
            strategic_value = control_characteristics.get('strategic_value', 'Medium')
            if strategic_value == 'High':
                adjustments += 0.05

            final_premium = max(0.10, min(0.50, base_premium + adjustments))

            return {
                'control_premium': final_premium,
                'ownership_percentage': ownership_percentage,
                'control_level': 'Control',
                'adjustments': adjustments
            }

        else:
            # Minority position - calculate minority discount
            base_discount = 0.15  # Base 15% discount

            # Adjustments based on rights
            adjustments = 0

            # Voting rights
            voting_rights = control_characteristics.get('voting_rights', 'Proportional')
            if voting_rights == 'Enhanced':
                adjustments -= 0.05
            elif voting_rights == 'Limited':
                adjustments += 0.05

            # Liquidity provisions
            liquidity_provisions = control_characteristics.get('liquidity_provisions', False)
            if liquidity_provisions:
                adjustments -= 0.03

            # Tag-along rights
            tag_along_rights = control_characteristics.get('tag_along_rights', False)
            if tag_along_rights:
                adjustments -= 0.02

            final_discount = max(0.05, min(0.30, base_discount + adjustments))

            return {
                'minority_discount': final_discount,
                'ownership_percentage': ownership_percentage,
                'control_level': 'Minority',
                'adjustments': adjustments
            }

    def calculate_key_person_discount(self, key_person_characteristics: Dict[str, Any]) -> Dict[str, float]:
        """Calculate discount for key person dependency"""

        dependency_level = key_person_characteristics.get('dependency_level', 'Medium')

        base_discounts = {
            'High': 0.15,
            'Medium': 0.08,
            'Low': 0.03
        }

        base_discount = base_discounts.get(dependency_level, 0.08)

        # Adjustments
        adjustments = 0

        # Age of key person
        age = key_person_characteristics.get('age', 50)
        if age > 65:
            adjustments += 0.05
        elif age < 40:
            adjustments -= 0.02

        # Succession planning
        succession_plan = key_person_characteristics.get('succession_plan', False)
        if succession_plan:
            adjustments -= 0.05
        else:
            adjustments += 0.03

        # Contract terms
        employment_contract = key_person_characteristics.get('employment_contract', False)
        if employment_contract:
            adjustments -= 0.02

        # Non-compete agreement
        non_compete = key_person_characteristics.get('non_compete', False)
        if non_compete:
            adjustments -= 0.02

        final_discount = max(0, min(0.25, base_discount + adjustments))

        return {
            'base_discount': base_discount,
            'adjustments': adjustments,
            'final_key_person_discount': final_discount,
            'dependency_level': dependency_level
        }

    def calculate_size_discount(self, company_size_metrics: Dict[str, float]) -> Dict[str, float]:
        """Calculate size-related discount"""

        revenue = company_size_metrics.get('revenue', 0)
        assets = company_size_metrics.get('total_assets', 0)
        employees = company_size_metrics.get('employee_count', 0)

        # Size-based discount schedule
        if revenue > 1_000_000_000:  # >$1B
            size_discount = 0.02
        elif revenue > 100_000_000:  # $100M-$1B
            size_discount = 0.05
        elif revenue > 25_000_000:  # $25M-$100M
            size_discount = 0.08
        elif revenue > 5_000_000:  # $5M-$25M
            size_discount = 0.12
        else:  # <$5M
            size_discount = 0.15

        return {
            'size_discount': size_discount,
            'revenue': revenue,
            'size_category': self.categorize_company_size(revenue)
        }

    def categorize_company_size(self, revenue: float) -> str:
        """Categorize company by size"""
        if revenue > 1_000_000_000:
            return "Large"
        elif revenue > 100_000_000:
            return "Medium-Large"
        elif revenue > 25_000_000:
            return "Medium"
        elif revenue > 5_000_000:
            return "Small-Medium"
        else:
            return "Small"

    def apply_all_discounts_premiums(self, base_value: float,
                                     discount_premium_factors: Dict[str, float]) -> Dict[str, float]:
        """Apply multiple discounts and premiums to base value"""

        adjusted_value = base_value
        cumulative_adjustment = 0

        # Apply control premium first (if applicable)
        if 'control_premium' in discount_premium_factors:
            control_premium = discount_premium_factors['control_premium']
            adjusted_value *= (1 + control_premium)
            cumulative_adjustment += control_premium

        # Apply minority discount (if applicable, mutually exclusive with control premium)
        elif 'minority_discount' in discount_premium_factors:
            minority_discount = discount_premium_factors['minority_discount']
            adjusted_value *= (1 - minority_discount)
            cumulative_adjustment -= minority_discount

        # Apply marketability discount
        if 'marketability_discount' in discount_premium_factors:
            marketability_discount = discount_premium_factors['marketability_discount']
            adjusted_value *= (1 - marketability_discount)
            cumulative_adjustment -= marketability_discount

        # Apply key person discount
        if 'key_person_discount' in discount_premium_factors:
            key_person_discount = discount_premium_factors['key_person_discount']
            adjusted_value *= (1 - key_person_discount)
            cumulative_adjustment -= key_person_discount

        # Apply size discount
        if 'size_discount' in discount_premium_factors:
            size_discount = discount_premium_factors['size_discount']
            adjusted_value *= (1 - size_discount)
            cumulative_adjustment -= size_discount

        return {
            'base_value': base_value,
            'adjusted_value': adjusted_value,
            'total_adjustment_percentage': cumulative_adjustment,
            'individual_adjustments': discount_premium_factors
        }


class PrivateCompanyValuator:
    """Comprehensive private company valuation framework"""

    def __init__(self):
        self.normalizer = PrivateCompanyNormalizer()
        self.income_valuator = IncomeApproachValuator()
        self.market_valuator = MarketApproachValuator()
        self.asset_valuator = AssetApproachValuator()
        self.discount_analyzer = DiscountPremiumAnalyzer()

    def comprehensive_valuation(self, private_company: PrivateCompanyData,
                                valuation_inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Perform comprehensive private company valuation"""

        # Step 1: Normalize financials
        earnings_adjustments = valuation_inputs.get('earnings_adjustments', {})
        normalized_earnings = self.normalizer.normalize_earnings(
            private_company.net_income, earnings_adjustments
        )

        # Step 2: Income Approach Valuations
        income_valuations = self.perform_income_approach_valuations(
            private_company, normalized_earnings, valuation_inputs
        )

        # Step 3: Market Approach Valuations
        market_valuations = self.perform_market_approach_valuations(
            private_company, valuation_inputs
        )

        # Step 4: Asset Approach Valuations
        asset_valuations = self.perform_asset_approach_valuations(
            private_company, valuation_inputs
        )

        # Step 5: Calculate Discounts and Premiums
        discounts_premiums = self.calculate_discounts_premiums(
            private_company, valuation_inputs
        )

        # Step 6: Synthesize Results
        final_valuation = self.synthesize_valuation_results(
            income_valuations, market_valuations, asset_valuations, discounts_premiums
        )

        return {
            'normalized_earnings': normalized_earnings,
            'income_approach': income_valuations,
            'market_approach': market_valuations,
            'asset_approach': asset_valuations,
            'discounts_premiums': discounts_premiums,
            'final_valuation': final_valuation,
            'valuation_summary': self.generate_valuation_summary(final_valuation)
        }

    def perform_income_approach_valuations(self, company: PrivateCompanyData,
                                           normalized_earnings: Dict[str, float],
                                           inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Perform all income approach valuations"""

        income_valuations = {}

        # Capitalized Earnings Method
        cap_rate = inputs.get('capitalization_rate', 0.20)
        growth_rate = inputs.get('growth_rate', 0.03)

        try:
            cap_earnings_value = self.income_valuator.calculate_capitalized_earnings_value(
                normalized_earnings['normalized_earnings'], cap_rate, growth_rate
            )
            income_valuations['capitalized_earnings'] = {
                'value': cap_earnings_value,
                'method': 'Capitalized Earnings',
                'cap_rate': cap_rate,
                'growth_rate': growth_rate
            }
        except Exception as e:
            income_valuations['capitalized_earnings'] = {'error': str(e)}

        # DCF Method
        if 'projected_cash_flows' in inputs:
            discount_rate = inputs.get('discount_rate', 0.15)
            terminal_growth = inputs.get('terminal_growth', 0.03)

            try:
                dcf_value = self.income_valuator.calculate_dcf_value(
                    inputs['projected_cash_flows'], discount_rate, None, terminal_growth
                )
                income_valuations['dcf'] = {
                    **dcf_value,
                    'method': 'Discounted Cash Flow',
                    'discount_rate': discount_rate,
                    'terminal_growth': terminal_growth
                }
            except Exception as e:
                income_valuations['dcf'] = {'error': str(e)}

        # Excess Earnings Method
        if 'tangible_assets' in inputs:
            tangible_assets = inputs['tangible_assets']
            asset_return_rate = inputs.get('asset_return_rate', 0.08)
            intangible_return_rate = inputs.get('intangible_return_rate', 0.15)

            try:
                excess_earnings_value = self.income_valuator.calculate_excess_earnings_value(
                    normalized_earnings['normalized_earnings'], tangible_assets,
                    asset_return_rate, intangible_return_rate
                )
                income_valuations['excess_earnings'] = {
                    **excess_earnings_value,
                    'method': 'Excess Earnings'
                }
            except Exception as e:
                income_valuations['excess_earnings'] = {'error': str(e)}

        return income_valuations

    def perform_market_approach_valuations(self, company: PrivateCompanyData,
                                           inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Perform market approach valuations"""

        market_valuations = {}

        # Company metrics for multiple application
        company_metrics = {
            'revenue': company.annual_revenue,
            'ebitda': company.ebitda,
            'net_income': company.net_income
        }

        # Guideline Public Company Method
        if 'public_comparables' in inputs:
            control_premium = inputs.get('control_premium', 0)

            try:
                gpc_value = self.market_valuator.calculate_guideline_public_company_value(
                    company_metrics, inputs['public_comparables'], control_premium
                )
                market_valuations['guideline_public_companies'] = {
                    **gpc_value,
                    'method': 'Guideline Public Companies'
                }
            except Exception as e:
                market_valuations['guideline_public_companies'] = {'error': str(e)}

        # Guideline Transaction Method
        if 'transaction_comparables' in inputs:
            try:
                transaction_value = self.market_valuator.calculate_guideline_transaction_value(
                    company_metrics, inputs['transaction_comparables']
                )
                market_valuations['guideline_transactions'] = {
                    **transaction_value,
                    'method': 'Guideline Transactions'
                }
            except Exception as e:
                market_valuations['guideline_transactions'] = {'error': str(e)}

        return market_valuations

    def perform_asset_approach_valuations(self, company: PrivateCompanyData,
                                          inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Perform asset approach valuations"""

        asset_valuations = {}

        # Adjusted Book Value Method
        if 'asset_fair_values' in inputs:
            book_values = {
                'current_assets': company.total_assets * 0.4,  # Simplified
                'fixed_assets': company.total_assets * 0.6
            }

            try:
                adjusted_bv = self.asset_valuator.calculate_adjusted_book_value(
                    book_values, inputs['asset_fair_values']
                )
                asset_valuations['adjusted_book_value'] = {
                    **adjusted_bv,
                    'method': 'Adjusted Book Value'
                }
            except Exception as e:
                asset_valuations['adjusted_book_value'] = {'error': str(e)}

        # Replacement Cost Method
        if 'asset_categories' in inputs:
            try:
                replacement_cost = self.asset_valuator.calculate_replacement_cost_value(
                    inputs['asset_categories']
                )
                asset_valuations['replacement_cost'] = {
                    **replacement_cost,
                    'method': 'Replacement Cost'
                }
            except Exception as e:
                asset_valuations['replacement_cost'] = {'error': str(e)}

        # Liquidation Value Method
        if 'liquidation_assumptions' in inputs:
            assets = {'total_assets': company.total_assets}
            liquidation_discounts = inputs['liquidation_assumptions'].get('discounts', {})
            liquidation_costs = inputs['liquidation_assumptions'].get('costs', 0)

            try:
                liquidation_value = self.asset_valuator.calculate_liquidation_value(
                    assets, liquidation_discounts, liquidation_costs
                )
                asset_valuations['liquidation_value'] = {
                    **liquidation_value,
                    'method': 'Liquidation Value'
                }
            except Exception as e:
                asset_valuations['liquidation_value'] = {'error': str(e)}

        return asset_valuations

    def calculate_discounts_premiums(self, company: PrivateCompanyData,
                                     inputs: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate all applicable discounts and premiums"""

        discounts_premiums = {}

        # Company characteristics for discount analysis
        characteristics = {
            'revenue': company.annual_revenue,
            'profitability': self.assess_profitability_level(company),
            'financial_reporting_quality': company.financial_reporting_quality,
            'growth_prospects': inputs.get('growth_prospects', 'Average'),
            'management_quality': company.management_quality
        }

        # Marketability Discount
        marketability_discount = self.discount_analyzer.calculate_marketability_discount(characteristics)
        discounts_premiums['marketability'] = marketability_discount

        # Control Premium/Minority Discount
        ownership_percentage = inputs.get('ownership_percentage', 1.0)
        control_characteristics = inputs.get('control_characteristics', {})

        control_analysis = self.discount_analyzer.calculate_control_premium(
            ownership_percentage, control_characteristics
        )
        discounts_premiums['control'] = control_analysis

        # Key Person Discount
        key_person_chars = {
            'dependency_level': company.key_person_dependency,
            'age': inputs.get('key_person_age', 50),
            'succession_plan': inputs.get('succession_plan', False),
            'employment_contract': inputs.get('employment_contract', False),
            'non_compete': inputs.get('non_compete', False)
        }

        key_person_discount = self.discount_analyzer.calculate_key_person_discount(key_person_chars)
        discounts_premiums['key_person'] = key_person_discount

        # Size Discount
        size_metrics = {
            'revenue': company.annual_revenue,
            'total_assets': company.total_assets,
            'employee_count': company.employee_count
        }

        size_discount = self.discount_analyzer.calculate_size_discount(size_metrics)
        discounts_premiums['size'] = size_discount

        return discounts_premiums

    def assess_profitability_level(self, company: PrivateCompanyData) -> str:
        """Assess company profitability level"""

        if company.annual_revenue == 0:
            return 'Weak'

        ebitda_margin = company.ebitda / company.annual_revenue
        net_margin = company.net_income / company.annual_revenue

        if ebitda_margin > 0.20 and net_margin > 0.10:
            return 'Strong'
        elif ebitda_margin > 0.10 and net_margin > 0.05:
            return 'Average'
        else:
            return 'Weak'

    def synthesize_valuation_results(self, income_results: Dict, market_results: Dict,
                                     asset_results: Dict, discounts_premiums: Dict) -> Dict[str, Any]:
        """Synthesize results from all valuation approaches"""

        # Collect all valid valuation indications
        valuation_indications = []
        method_weights = {}

        # Income approach values
        for method, result in income_results.items():
            if 'error' not in result:
                if method == 'capitalized_earnings':
                    value = result['value']
                    weight = 0.4  # Higher weight for income approach
                elif method == 'dcf':
                    value = result['enterprise_value']
                    weight = 0.4
                elif method == 'excess_earnings':
                    value = result['total_business_value']
                    weight = 0.3
                else:
                    continue

                valuation_indications.append(value)
                method_weights[f"income_{method}"] = weight

        # Market approach values
        for method, result in market_results.items():
            if 'error' not in result and 'indicated_values' in result:
                # Use median of indicated values from market approach
                indicated_values = list(result['indicated_values'].values())
                if indicated_values:
                    value = np.median(indicated_values)
                    weight = 0.5  # High weight for market approach
                    valuation_indications.append(value)
                    method_weights[f"market_{method}"] = weight

        # Asset approach values (typically lower weight unless asset-heavy business)
        for method, result in asset_results.items():
            if 'error' not in result:
                if method == 'adjusted_book_value':
                    value = result['adjusted_book_value']
                    weight = 0.2
                elif method == 'replacement_cost':
                    value = result['total_replacement_cost']
                    weight = 0.2
                elif method == 'liquidation_value':
                    value = result['net_liquidation_value']
                    weight = 0.1  # Lowest weight
                else:
                    continue

                valuation_indications.append(value)
                method_weights[f"asset_{method}"] = weight

        # Calculate weighted average if multiple methods available
        if len(valuation_indications) > 1:
            weights = list(method_weights.values())
            total_weight = sum(weights)
            normalized_weights = [w / total_weight for w in weights]

            weighted_value = sum(v * w for v, w in zip(valuation_indications, normalized_weights))
            simple_average = np.mean(valuation_indications)
            median_value = np.median(valuation_indications)
        else:
            weighted_value = valuation_indications[0] if valuation_indications else 0
            simple_average = weighted_value
            median_value = weighted_value

        # Apply discounts and premiums to base valuation
        base_value = weighted_value

        # Compile discount/premium factors
        dp_factors = {}

        if 'control' in discounts_premiums:
            control_result = discounts_premiums['control']
            if 'control_premium' in control_result:
                dp_factors['control_premium'] = control_result['control_premium']
            elif 'minority_discount' in control_result:
                dp_factors['minority_discount'] = control_result['minority_discount']

        if 'marketability' in discounts_premiums:
            dp_factors['marketability_discount'] = discounts_premiums['marketability']['final_marketability_discount']

        if 'key_person' in discounts_premiums:
            dp_factors['key_person_discount'] = discounts_premiums['key_person']['final_key_person_discount']

        if 'size' in discounts_premiums:
            dp_factors['size_discount'] = discounts_premiums['size']['size_discount']

        # Apply discounts and premiums
        final_adjustment = self.discount_analyzer.apply_all_discounts_premiums(base_value, dp_factors)

        return {
            'valuation_indications': valuation_indications,
            'method_weights': method_weights,
            'base_valuation': {
                'weighted_average': weighted_value,
                'simple_average': simple_average,
                'median': median_value
            },
            'discount_premium_analysis': final_adjustment,
            'final_valuation_range': {
                'low': final_adjustment['adjusted_value'] * 0.85,
                'mid': final_adjustment['adjusted_value'],
                'high': final_adjustment['adjusted_value'] * 1.15
            },
            'valuation_methods_used': len(valuation_indications)
        }

    def generate_valuation_summary(self, final_valuation: Dict[str, Any]) -> Dict[str, Any]:
        """Generate executive summary of valuation"""

        final_value = final_valuation['final_valuation_range']['mid']
        base_value = final_valuation['base_valuation']['weighted_average']

        total_adjustment = final_valuation['discount_premium_analysis']['total_adjustment_percentage']

        summary = {
            'final_value': final_value,
            'valuation_range': final_valuation['final_valuation_range'],
            'base_value_before_adjustments': base_value,
            'total_discount_premium_adjustment': total_adjustment,
            'number_of_methods_used': final_valuation['valuation_methods_used'],
            'primary_valuation_driver': self.identify_primary_driver(final_valuation),
            'confidence_level': self.assess_confidence_level(final_valuation),
            'key_assumptions': self.extract_key_assumptions(final_valuation)
        }

        return summary

    def identify_primary_driver(self, final_valuation: Dict[str, Any]) -> str:
        """Identify primary valuation driver"""

        method_weights = final_valuation['method_weights']

        if not method_weights:
            return "No clear driver"

        max_weight_method = max(method_weights.items(), key=lambda x: x[1])[0]

        if 'income' in max_weight_method:
            return "Income/Earnings Generation"
        elif 'market' in max_weight_method:
            return "Market Comparables"
        elif 'asset' in max_weight_method:
            return "Asset Values"
        else:
            return "Multiple Factors"

    def assess_confidence_level(self, final_valuation: Dict[str, Any]) -> str:
        """Assess confidence level in valuation"""

        num_methods = final_valuation['valuation_methods_used']

        if num_methods >= 3:
            return "High"
        elif num_methods == 2:
            return "Medium"
        else:
            return "Low"

    def extract_key_assumptions(self, final_valuation: Dict[str, Any]) -> List[str]:
        """Extract key valuation assumptions"""

        assumptions = [
            "Normalized earnings reflect sustainable performance",
            "Market multiples are representative of subject company",
            "Discount rates reflect appropriate risk levels"
        ]

        # Add specific assumptions based on adjustments applied
        dp_analysis = final_valuation['discount_premium_analysis']
        individual_adjustments = dp_analysis['individual_adjustments']

        if 'marketability_discount' in individual_adjustments:
            assumptions.append("Marketability discount reflects lack of ready market")

        if 'control_premium' in individual_adjustments:
            assumptions.append("Control premium reflects strategic value")
        elif 'minority_discount' in individual_adjustments:
            assumptions.append("Minority discount reflects lack of control")

        if 'key_person_discount' in individual_adjustments:
            assumptions.append("Key person discount reflects dependency risk")

        return assumptions


# Convenience functions for quick private company valuations
def quick_private_company_valuation(company_data: PrivateCompanyData,
                                    normalized_earnings: float,
                                    capitalization_rate: float = 0.20) -> float:
    """Quick private company valuation using capitalized earnings"""

    valuator = IncomeApproachValuator()
    return valuator.calculate_capitalized_earnings_value(normalized_earnings, capitalization_rate)


def calculate_marketability_discount(revenue: float, profitability: str = "Average",
                                     reporting_quality: str = "Compiled") -> float:
    """Quick marketability discount calculation"""

    analyzer = DiscountPremiumAnalyzer()
    characteristics = {
        'revenue': revenue,
        'profitability': profitability,
        'financial_reporting_quality': reporting_quality,
        'growth_prospects': 'Average',
        'management_quality': 'Average'
    }

    result = analyzer.calculate_marketability_discount(characteristics)
    return result['final_marketability_discount']


def estimate_control_premium(ownership_percentage: float) -> float:
    """Quick control premium estimation"""

    analyzer = DiscountPremiumAnalyzer()
    control_chars = {'synergy_potential': 'Medium', 'strategic_value': 'Medium'}

    result = analyzer.calculate_control_premium(ownership_percentage, control_chars)

    if 'control_premium' in result:
        return result['control_premium']
    elif 'minority_discount' in result:
        return -result['minority_discount']  # Return negative for discount
    else:
        return 0