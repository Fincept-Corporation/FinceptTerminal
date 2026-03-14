"""
Equity Investment Fundamental Analysis Module
==============================================

Comprehensive financial ratio analysis and fundamental company evaluation
following CFA curriculum standards.

Covers:
- Profitability Ratios (ROE, ROA, ROIC, margins)
- Liquidity Ratios (current, quick, cash)
- Solvency/Leverage Ratios (D/E, interest coverage)
- Efficiency/Activity Ratios (turnover ratios)
- DuPont Analysis (3-way and 5-way decomposition)
- Working Capital Analysis
- Cash Flow Analysis
- Cost of Equity and Required Returns

===== DATA SOURCES REQUIRED =====
INPUT:
  - Income statement data (revenue, expenses, net income)
  - Balance sheet data (assets, liabilities, equity)
  - Cash flow statement data
  - Market data (stock price, shares outstanding)

OUTPUT:
  - Financial ratio calculations and interpretations
  - DuPont decomposition analysis
  - Trend analysis and peer comparisons
  - Financial health assessments
"""

import numpy as np
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum
import json
import sys


class RatioCategory(Enum):
    """Categories of financial ratios"""
    PROFITABILITY = "profitability"
    LIQUIDITY = "liquidity"
    SOLVENCY = "solvency"
    EFFICIENCY = "efficiency"
    VALUATION = "valuation"
    COVERAGE = "coverage"


@dataclass
class IncomeStatementData:
    """Income statement data structure"""
    revenue: float
    cost_of_goods_sold: float
    gross_profit: float
    operating_expenses: float
    operating_income: float  # EBIT
    interest_expense: float
    pretax_income: float
    tax_expense: float
    net_income: float
    ebitda: Optional[float] = None
    depreciation: Optional[float] = None


@dataclass
class BalanceSheetData:
    """Balance sheet data structure"""
    # Assets
    cash: float
    marketable_securities: float
    accounts_receivable: float
    inventory: float
    total_current_assets: float
    ppe_net: float
    total_assets: float

    # Liabilities
    accounts_payable: float
    short_term_debt: float
    current_portion_ltd: float
    total_current_liabilities: float
    long_term_debt: float
    total_liabilities: float

    # Equity
    total_equity: float
    retained_earnings: float


@dataclass
class CashFlowData:
    """Cash flow statement data structure"""
    operating_cash_flow: float
    capital_expenditures: float
    free_cash_flow: float
    dividends_paid: float
    net_borrowing: float


class ProfitabilityRatios:
    """
    Profitability ratio calculations and analysis.

    Measures company's ability to generate profits from operations.
    """

    def calculate_gross_margin(self, gross_profit: float, revenue: float) -> Dict[str, Any]:
        """
        Gross Profit Margin = Gross Profit / Revenue

        Measures pricing power and production efficiency.
        """
        if revenue <= 0:
            raise ValueError("Revenue must be positive")

        ratio = gross_profit / revenue

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "interpretation": self._interpret_gross_margin(ratio),
            "formula": "Gross Profit / Revenue"
        }

    def calculate_operating_margin(self, operating_income: float, revenue: float) -> Dict[str, Any]:
        """
        Operating Profit Margin = Operating Income (EBIT) / Revenue

        Measures operational efficiency after operating expenses.
        """
        if revenue <= 0:
            raise ValueError("Revenue must be positive")

        ratio = operating_income / revenue

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "interpretation": self._interpret_operating_margin(ratio),
            "formula": "Operating Income / Revenue"
        }

    def calculate_net_profit_margin(self, net_income: float, revenue: float) -> Dict[str, Any]:
        """
        Net Profit Margin = Net Income / Revenue

        Measures overall profitability after all expenses.
        """
        if revenue <= 0:
            raise ValueError("Revenue must be positive")

        ratio = net_income / revenue

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "interpretation": self._interpret_net_margin(ratio),
            "formula": "Net Income / Revenue"
        }

    def calculate_ebitda_margin(self, ebitda: float, revenue: float) -> Dict[str, Any]:
        """
        EBITDA Margin = EBITDA / Revenue

        Measures cash operating profitability.
        """
        if revenue <= 0:
            raise ValueError("Revenue must be positive")

        ratio = ebitda / revenue

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "formula": "EBITDA / Revenue"
        }

    def calculate_roe(self, net_income: float, average_equity: float) -> Dict[str, Any]:
        """
        Return on Equity (ROE) = Net Income / Average Shareholders' Equity

        Measures return generated on shareholders' investment.
        """
        if average_equity <= 0:
            raise ValueError("Average equity must be positive")

        ratio = net_income / average_equity

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "interpretation": self._interpret_roe(ratio),
            "formula": "Net Income / Average Equity",
            "benchmark_sp500": 0.15  # Typical S&P 500 average
        }

    def calculate_roa(self, net_income: float, average_assets: float) -> Dict[str, Any]:
        """
        Return on Assets (ROA) = Net Income / Average Total Assets

        Measures how efficiently company uses assets to generate profit.
        """
        if average_assets <= 0:
            raise ValueError("Average assets must be positive")

        ratio = net_income / average_assets

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "interpretation": self._interpret_roa(ratio),
            "formula": "Net Income / Average Total Assets"
        }

    def calculate_roic(
        self,
        nopat: float,
        invested_capital: float
    ) -> Dict[str, Any]:
        """
        Return on Invested Capital (ROIC) = NOPAT / Invested Capital

        Where:
        - NOPAT = Operating Income × (1 - Tax Rate)
        - Invested Capital = Total Debt + Total Equity - Cash

        Measures return on all capital invested in the business.
        """
        if invested_capital <= 0:
            raise ValueError("Invested capital must be positive")

        ratio = nopat / invested_capital

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "interpretation": self._interpret_roic(ratio),
            "formula": "NOPAT / Invested Capital",
            "value_creation": "Creates value if ROIC > WACC"
        }

    def calculate_nopat(
        self,
        operating_income: float,
        tax_rate: float
    ) -> float:
        """Calculate Net Operating Profit After Tax"""
        return operating_income * (1 - tax_rate)

    def calculate_invested_capital(
        self,
        total_debt: float,
        total_equity: float,
        cash: float
    ) -> float:
        """Calculate Invested Capital"""
        return total_debt + total_equity - cash

    def _interpret_gross_margin(self, ratio: float) -> str:
        if ratio > 0.50:
            return "Strong pricing power and/or efficient production"
        elif ratio > 0.30:
            return "Healthy gross margin"
        elif ratio > 0.15:
            return "Moderate gross margin - may indicate competitive pressure"
        else:
            return "Low gross margin - commodity-like business or high costs"

    def _interpret_operating_margin(self, ratio: float) -> str:
        if ratio > 0.25:
            return "Excellent operational efficiency"
        elif ratio > 0.15:
            return "Good operational control"
        elif ratio > 0.08:
            return "Average operational efficiency"
        else:
            return "Low operating margin - high operating costs"

    def _interpret_net_margin(self, ratio: float) -> str:
        if ratio > 0.20:
            return "Excellent overall profitability"
        elif ratio > 0.10:
            return "Good profitability"
        elif ratio > 0.05:
            return "Moderate profitability"
        else:
            return "Low profitability or loss-making"

    def _interpret_roe(self, ratio: float) -> str:
        if ratio > 0.20:
            return "Excellent return to shareholders"
        elif ratio > 0.15:
            return "Good return - above market average"
        elif ratio > 0.10:
            return "Average return"
        else:
            return "Below-average return to shareholders"

    def _interpret_roa(self, ratio: float) -> str:
        if ratio > 0.10:
            return "Excellent asset utilization"
        elif ratio > 0.05:
            return "Good asset efficiency"
        else:
            return "Low asset productivity"

    def _interpret_roic(self, ratio: float) -> str:
        if ratio > 0.15:
            return "Strong value creation (likely exceeds WACC)"
        elif ratio > 0.10:
            return "Good capital returns"
        elif ratio > 0.05:
            return "Moderate returns - may be near WACC"
        else:
            return "Low returns - may be destroying value"


class LiquidityRatios:
    """
    Liquidity ratio calculations and analysis.

    Measures company's ability to meet short-term obligations.
    """

    def calculate_current_ratio(
        self,
        current_assets: float,
        current_liabilities: float
    ) -> Dict[str, Any]:
        """
        Current Ratio = Current Assets / Current Liabilities

        Measures ability to pay short-term obligations.
        """
        if current_liabilities <= 0:
            raise ValueError("Current liabilities must be positive")

        ratio = current_assets / current_liabilities

        return {
            "ratio": ratio,
            "interpretation": self._interpret_current_ratio(ratio),
            "formula": "Current Assets / Current Liabilities",
            "benchmark": "Generally 1.5-2.0 is healthy"
        }

    def calculate_quick_ratio(
        self,
        current_assets: float,
        inventory: float,
        current_liabilities: float
    ) -> Dict[str, Any]:
        """
        Quick Ratio (Acid Test) = (Current Assets - Inventory) / Current Liabilities

        More conservative liquidity measure excluding inventory.
        """
        if current_liabilities <= 0:
            raise ValueError("Current liabilities must be positive")

        ratio = (current_assets - inventory) / current_liabilities

        return {
            "ratio": ratio,
            "interpretation": self._interpret_quick_ratio(ratio),
            "formula": "(Current Assets - Inventory) / Current Liabilities",
            "benchmark": "Generally > 1.0 is healthy"
        }

    def calculate_cash_ratio(
        self,
        cash: float,
        marketable_securities: float,
        current_liabilities: float
    ) -> Dict[str, Any]:
        """
        Cash Ratio = (Cash + Marketable Securities) / Current Liabilities

        Most conservative liquidity measure.
        """
        if current_liabilities <= 0:
            raise ValueError("Current liabilities must be positive")

        ratio = (cash + marketable_securities) / current_liabilities

        return {
            "ratio": ratio,
            "interpretation": self._interpret_cash_ratio(ratio),
            "formula": "(Cash + Marketable Securities) / Current Liabilities"
        }

    def calculate_operating_cash_flow_ratio(
        self,
        operating_cash_flow: float,
        current_liabilities: float
    ) -> Dict[str, Any]:
        """
        Operating Cash Flow Ratio = Operating Cash Flow / Current Liabilities

        Measures ability to pay obligations from operations.
        """
        if current_liabilities <= 0:
            raise ValueError("Current liabilities must be positive")

        ratio = operating_cash_flow / current_liabilities

        return {
            "ratio": ratio,
            "interpretation": "Higher is better - shows cash generation to cover short-term debt",
            "formula": "Operating Cash Flow / Current Liabilities"
        }

    def _interpret_current_ratio(self, ratio: float) -> str:
        if ratio > 2.0:
            return "Strong liquidity - may have excess working capital"
        elif ratio > 1.5:
            return "Healthy liquidity position"
        elif ratio > 1.0:
            return "Adequate liquidity but should monitor"
        else:
            return "Potential liquidity concerns - may struggle to meet obligations"

    def _interpret_quick_ratio(self, ratio: float) -> str:
        if ratio > 1.5:
            return "Strong quick liquidity"
        elif ratio > 1.0:
            return "Adequate liquidity without relying on inventory"
        else:
            return "May need to liquidate inventory to meet obligations"

    def _interpret_cash_ratio(self, ratio: float) -> str:
        if ratio > 0.5:
            return "Strong cash position"
        elif ratio > 0.2:
            return "Adequate cash on hand"
        else:
            return "Limited immediate liquidity"


class SolvencyRatios:
    """
    Solvency/Leverage ratio calculations and analysis.

    Measures company's ability to meet long-term obligations
    and financial leverage.
    """

    def calculate_debt_to_equity(
        self,
        total_debt: float,
        total_equity: float
    ) -> Dict[str, Any]:
        """
        Debt-to-Equity Ratio = Total Debt / Total Equity

        Measures financial leverage and capital structure.
        """
        if total_equity <= 0:
            raise ValueError("Total equity must be positive")

        ratio = total_debt / total_equity

        return {
            "ratio": ratio,
            "interpretation": self._interpret_debt_to_equity(ratio),
            "formula": "Total Debt / Total Equity"
        }

    def calculate_debt_to_assets(
        self,
        total_debt: float,
        total_assets: float
    ) -> Dict[str, Any]:
        """
        Debt-to-Assets Ratio = Total Debt / Total Assets

        Measures proportion of assets financed by debt.
        """
        if total_assets <= 0:
            raise ValueError("Total assets must be positive")

        ratio = total_debt / total_assets

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "interpretation": self._interpret_debt_to_assets(ratio),
            "formula": "Total Debt / Total Assets"
        }

    def calculate_debt_to_capital(
        self,
        total_debt: float,
        total_equity: float
    ) -> Dict[str, Any]:
        """
        Debt-to-Capital Ratio = Total Debt / (Total Debt + Total Equity)

        Measures debt as percentage of total capital.
        """
        total_capital = total_debt + total_equity
        if total_capital <= 0:
            raise ValueError("Total capital must be positive")

        ratio = total_debt / total_capital

        return {
            "ratio": ratio,
            "percentage": ratio * 100,
            "formula": "Total Debt / (Total Debt + Total Equity)"
        }

    def calculate_interest_coverage(
        self,
        ebit: float,
        interest_expense: float
    ) -> Dict[str, Any]:
        """
        Interest Coverage Ratio = EBIT / Interest Expense

        Measures ability to pay interest on debt.
        """
        if interest_expense <= 0:
            return {
                "ratio": float('inf'),
                "interpretation": "No interest expense - debt-free or very low debt",
                "formula": "EBIT / Interest Expense"
            }

        ratio = ebit / interest_expense

        return {
            "ratio": ratio,
            "interpretation": self._interpret_interest_coverage(ratio),
            "formula": "EBIT / Interest Expense",
            "times_covered": ratio
        }

    def calculate_fixed_charge_coverage(
        self,
        ebit: float,
        lease_payments: float,
        interest_expense: float
    ) -> Dict[str, Any]:
        """
        Fixed Charge Coverage = (EBIT + Lease Payments) / (Interest + Lease Payments)

        Broader measure including lease obligations.
        """
        numerator = ebit + lease_payments
        denominator = interest_expense + lease_payments

        if denominator <= 0:
            return {"ratio": float('inf'), "interpretation": "No fixed charges"}

        ratio = numerator / denominator

        return {
            "ratio": ratio,
            "formula": "(EBIT + Lease Payments) / (Interest + Lease Payments)"
        }

    def calculate_financial_leverage(
        self,
        total_assets: float,
        total_equity: float
    ) -> Dict[str, Any]:
        """
        Financial Leverage = Total Assets / Total Equity

        Also known as equity multiplier. Higher = more leverage.
        """
        if total_equity <= 0:
            raise ValueError("Total equity must be positive")

        ratio = total_assets / total_equity

        return {
            "ratio": ratio,
            "interpretation": f"Each $1 of equity supports ${ratio:.2f} of assets",
            "formula": "Total Assets / Total Equity"
        }

    def _interpret_debt_to_equity(self, ratio: float) -> str:
        if ratio > 2.0:
            return "High leverage - significant financial risk"
        elif ratio > 1.0:
            return "Moderate leverage"
        elif ratio > 0.5:
            return "Conservative capital structure"
        else:
            return "Very low leverage - predominantly equity financed"

    def _interpret_debt_to_assets(self, ratio: float) -> str:
        if ratio > 0.6:
            return "High debt financing - greater financial risk"
        elif ratio > 0.4:
            return "Moderate debt levels"
        else:
            return "Conservative debt levels"

    def _interpret_interest_coverage(self, ratio: float) -> str:
        if ratio > 8:
            return "Very strong - easily covers interest obligations"
        elif ratio > 5:
            return "Strong interest coverage"
        elif ratio > 3:
            return "Adequate coverage"
        elif ratio > 1.5:
            return "Marginal - limited margin of safety"
        else:
            return "Weak coverage - may struggle to meet interest payments"


class EfficiencyRatios:
    """
    Efficiency/Activity ratio calculations and analysis.

    Measures how efficiently company uses its assets.
    """

    def calculate_asset_turnover(
        self,
        revenue: float,
        average_assets: float
    ) -> Dict[str, Any]:
        """
        Total Asset Turnover = Revenue / Average Total Assets

        Measures revenue generated per dollar of assets.
        """
        if average_assets <= 0:
            raise ValueError("Average assets must be positive")

        ratio = revenue / average_assets

        return {
            "ratio": ratio,
            "interpretation": f"Generates ${ratio:.2f} revenue per $1 of assets",
            "formula": "Revenue / Average Total Assets"
        }

    def calculate_fixed_asset_turnover(
        self,
        revenue: float,
        average_fixed_assets: float
    ) -> Dict[str, Any]:
        """
        Fixed Asset Turnover = Revenue / Average Net PP&E

        Measures efficiency of fixed asset utilization.
        """
        if average_fixed_assets <= 0:
            raise ValueError("Average fixed assets must be positive")

        ratio = revenue / average_fixed_assets

        return {
            "ratio": ratio,
            "formula": "Revenue / Average Net PP&E"
        }

    def calculate_inventory_turnover(
        self,
        cogs: float,
        average_inventory: float
    ) -> Dict[str, Any]:
        """
        Inventory Turnover = Cost of Goods Sold / Average Inventory

        Measures how many times inventory is sold in a period.
        """
        if average_inventory <= 0:
            raise ValueError("Average inventory must be positive")

        ratio = cogs / average_inventory
        days_in_inventory = 365 / ratio

        return {
            "ratio": ratio,
            "days_in_inventory": days_in_inventory,
            "interpretation": f"Inventory turns over {ratio:.1f}x per year ({days_in_inventory:.0f} days)",
            "formula": "COGS / Average Inventory"
        }

    def calculate_receivables_turnover(
        self,
        revenue: float,
        average_receivables: float
    ) -> Dict[str, Any]:
        """
        Receivables Turnover = Revenue / Average Accounts Receivable

        Measures how quickly receivables are collected.
        """
        if average_receivables <= 0:
            raise ValueError("Average receivables must be positive")

        ratio = revenue / average_receivables
        days_sales_outstanding = 365 / ratio

        return {
            "ratio": ratio,
            "days_sales_outstanding": days_sales_outstanding,
            "interpretation": f"Collects receivables in {days_sales_outstanding:.0f} days on average",
            "formula": "Revenue / Average Accounts Receivable"
        }

    def calculate_payables_turnover(
        self,
        purchases: float,
        average_payables: float
    ) -> Dict[str, Any]:
        """
        Payables Turnover = Purchases / Average Accounts Payable

        Measures how quickly company pays suppliers.
        """
        if average_payables <= 0:
            raise ValueError("Average payables must be positive")

        ratio = purchases / average_payables
        days_payable_outstanding = 365 / ratio

        return {
            "ratio": ratio,
            "days_payable_outstanding": days_payable_outstanding,
            "interpretation": f"Pays suppliers in {days_payable_outstanding:.0f} days on average",
            "formula": "Purchases / Average Accounts Payable"
        }

    def calculate_cash_conversion_cycle(
        self,
        days_inventory: float,
        days_receivables: float,
        days_payables: float
    ) -> Dict[str, Any]:
        """
        Cash Conversion Cycle = DIO + DSO - DPO

        Measures time between cash outflow and cash inflow.
        """
        ccc = days_inventory + days_receivables - days_payables

        return {
            "days": ccc,
            "days_inventory_outstanding": days_inventory,
            "days_sales_outstanding": days_receivables,
            "days_payable_outstanding": days_payables,
            "interpretation": self._interpret_ccc(ccc),
            "formula": "DIO + DSO - DPO"
        }

    def calculate_working_capital_turnover(
        self,
        revenue: float,
        average_working_capital: float
    ) -> Dict[str, Any]:
        """
        Working Capital Turnover = Revenue / Average Working Capital

        Measures efficiency of working capital utilization.
        """
        if average_working_capital == 0:
            return {"ratio": float('inf'), "interpretation": "Zero working capital"}

        ratio = revenue / average_working_capital

        return {
            "ratio": ratio,
            "formula": "Revenue / Average Working Capital"
        }

    def _interpret_ccc(self, days: float) -> str:
        if days < 0:
            return "Negative CCC - company receives payment before paying suppliers (efficient)"
        elif days < 30:
            return "Short CCC - efficient working capital management"
        elif days < 60:
            return "Moderate CCC"
        else:
            return "Long CCC - may need to improve working capital efficiency"


class DuPontAnalysis:
    """
    DuPont Analysis for ROE Decomposition.

    Breaks down ROE into component drivers to understand
    sources of returns.
    """

    def three_way_decomposition(
        self,
        net_income: float,
        revenue: float,
        average_assets: float,
        average_equity: float
    ) -> Dict[str, Any]:
        """
        3-Way DuPont Analysis:
        ROE = Net Profit Margin × Asset Turnover × Financial Leverage

        ROE = (Net Income/Revenue) × (Revenue/Assets) × (Assets/Equity)
        """
        # Calculate components
        net_profit_margin = net_income / revenue if revenue > 0 else 0
        asset_turnover = revenue / average_assets if average_assets > 0 else 0
        financial_leverage = average_assets / average_equity if average_equity > 0 else 0

        # Calculate ROE
        roe = net_profit_margin * asset_turnover * financial_leverage

        return {
            "roe": roe,
            "roe_percentage": roe * 100,
            "components": {
                "net_profit_margin": {
                    "value": net_profit_margin,
                    "percentage": net_profit_margin * 100,
                    "interpretation": "Profitability - how much profit per dollar of sales"
                },
                "asset_turnover": {
                    "value": asset_turnover,
                    "interpretation": "Efficiency - revenue generated per dollar of assets"
                },
                "financial_leverage": {
                    "value": financial_leverage,
                    "interpretation": "Leverage - assets supported by each dollar of equity"
                }
            },
            "formula": "ROE = Net Profit Margin × Asset Turnover × Financial Leverage",
            "verification": f"Check: {net_profit_margin:.4f} × {asset_turnover:.4f} × {financial_leverage:.4f} = {roe:.4f}"
        }

    def five_way_decomposition(
        self,
        net_income: float,
        pretax_income: float,
        ebit: float,
        revenue: float,
        average_assets: float,
        average_equity: float
    ) -> Dict[str, Any]:
        """
        5-Way DuPont Analysis:
        ROE = Tax Burden × Interest Burden × EBIT Margin × Asset Turnover × Leverage

        ROE = (NI/EBT) × (EBT/EBIT) × (EBIT/Rev) × (Rev/Assets) × (Assets/Equity)
        """
        # Calculate components
        tax_burden = net_income / pretax_income if pretax_income > 0 else 0
        interest_burden = pretax_income / ebit if ebit > 0 else 0
        ebit_margin = ebit / revenue if revenue > 0 else 0
        asset_turnover = revenue / average_assets if average_assets > 0 else 0
        financial_leverage = average_assets / average_equity if average_equity > 0 else 0

        # Calculate ROE
        roe = tax_burden * interest_burden * ebit_margin * asset_turnover * financial_leverage

        return {
            "roe": roe,
            "roe_percentage": roe * 100,
            "components": {
                "tax_burden": {
                    "value": tax_burden,
                    "interpretation": "Tax efficiency - what remains after taxes (1 - tax rate)"
                },
                "interest_burden": {
                    "value": interest_burden,
                    "interpretation": "Interest impact - what remains after interest expense"
                },
                "ebit_margin": {
                    "value": ebit_margin,
                    "percentage": ebit_margin * 100,
                    "interpretation": "Operating profitability"
                },
                "asset_turnover": {
                    "value": asset_turnover,
                    "interpretation": "Asset efficiency"
                },
                "financial_leverage": {
                    "value": financial_leverage,
                    "interpretation": "Financial leverage"
                }
            },
            "formula": "ROE = Tax Burden × Interest Burden × EBIT Margin × Asset Turnover × Leverage"
        }

    def analyze_roe_drivers(
        self,
        current_decomposition: Dict[str, Any],
        prior_decomposition: Dict[str, Any]
    ) -> Dict[str, Any]:
        """
        Analyze changes in ROE drivers between periods.
        """
        changes = {}

        for component in ['net_profit_margin', 'asset_turnover', 'financial_leverage']:
            current_val = current_decomposition['components'][component]['value']
            prior_val = prior_decomposition['components'][component]['value']
            change = current_val - prior_val
            pct_change = (change / prior_val * 100) if prior_val != 0 else 0

            changes[component] = {
                "current": current_val,
                "prior": prior_val,
                "change": change,
                "percentage_change": pct_change
            }

        roe_change = current_decomposition['roe'] - prior_decomposition['roe']

        return {
            "roe_change": roe_change,
            "roe_change_percentage": roe_change * 100,
            "driver_changes": changes,
            "primary_driver": self._identify_primary_driver(changes)
        }

    def _identify_primary_driver(self, changes: Dict[str, Any]) -> str:
        """Identify which component had the largest impact on ROE change."""
        max_impact = 0
        primary_driver = None

        for component, data in changes.items():
            impact = abs(data['percentage_change'])
            if impact > max_impact:
                max_impact = impact
                primary_driver = component

        return primary_driver


class CostOfEquityCalculator:
    """
    Calculate cost of equity using various models.
    """

    def capm(
        self,
        risk_free_rate: float,
        beta: float,
        market_risk_premium: float
    ) -> Dict[str, Any]:
        """
        Capital Asset Pricing Model (CAPM):
        Cost of Equity = Rf + β × (Rm - Rf)

        Where:
        - Rf = Risk-free rate
        - β = Beta (systematic risk)
        - Rm - Rf = Market risk premium
        """
        cost_of_equity = risk_free_rate + beta * market_risk_premium

        return {
            "cost_of_equity": cost_of_equity,
            "percentage": cost_of_equity * 100,
            "components": {
                "risk_free_rate": risk_free_rate,
                "beta": beta,
                "market_risk_premium": market_risk_premium,
                "equity_risk_premium": beta * market_risk_premium
            },
            "formula": "Rf + β × (Rm - Rf)"
        }

    def dividend_growth_model(
        self,
        current_price: float,
        next_dividend: float,
        growth_rate: float
    ) -> Dict[str, Any]:
        """
        Dividend Growth Model (Gordon Growth):
        Cost of Equity = (D1 / P0) + g

        Where:
        - D1 = Expected dividend next period
        - P0 = Current stock price
        - g = Expected dividend growth rate
        """
        dividend_yield = next_dividend / current_price
        cost_of_equity = dividend_yield + growth_rate

        return {
            "cost_of_equity": cost_of_equity,
            "percentage": cost_of_equity * 100,
            "components": {
                "dividend_yield": dividend_yield,
                "growth_rate": growth_rate
            },
            "formula": "(D1 / P0) + g"
        }

    def bond_yield_plus_risk_premium(
        self,
        company_bond_yield: float,
        risk_premium: float = 0.03
    ) -> Dict[str, Any]:
        """
        Bond Yield Plus Risk Premium:
        Cost of Equity = Company Bond Yield + Risk Premium

        Simple approach adding equity risk premium to company's bond yield.
        """
        cost_of_equity = company_bond_yield + risk_premium

        return {
            "cost_of_equity": cost_of_equity,
            "percentage": cost_of_equity * 100,
            "components": {
                "bond_yield": company_bond_yield,
                "risk_premium": risk_premium
            },
            "formula": "Bond Yield + Risk Premium"
        }

    def compare_roe_vs_cost_of_equity(
        self,
        roe: float,
        cost_of_equity: float
    ) -> Dict[str, Any]:
        """
        Compare ROE to Cost of Equity to assess value creation.

        If ROE > Cost of Equity: Creating shareholder value
        If ROE < Cost of Equity: Destroying shareholder value
        """
        spread = roe - cost_of_equity

        return {
            "roe": roe,
            "cost_of_equity": cost_of_equity,
            "spread": spread,
            "spread_percentage": spread * 100,
            "value_creation": spread > 0,
            "interpretation": self._interpret_spread(spread)
        }

    def _interpret_spread(self, spread: float) -> str:
        if spread > 0.05:
            return "Strong value creation - ROE significantly exceeds cost of equity"
        elif spread > 0:
            return "Creating shareholder value - ROE exceeds cost of equity"
        elif spread > -0.02:
            return "Near break-even - ROE approximately equals cost of equity"
        else:
            return "Destroying shareholder value - ROE below cost of equity"


class MarketValueVsBookValue:
    """
    Analyze market value vs book value of equity.
    """

    def calculate_book_value_per_share(
        self,
        total_equity: float,
        shares_outstanding: float
    ) -> float:
        """Calculate book value per share."""
        return total_equity / shares_outstanding

    def calculate_market_to_book(
        self,
        market_price: float,
        book_value_per_share: float
    ) -> Dict[str, Any]:
        """
        Price-to-Book Ratio = Market Price / Book Value per Share

        Measures market valuation relative to accounting value.
        """
        if book_value_per_share <= 0:
            raise ValueError("Book value must be positive")

        ratio = market_price / book_value_per_share

        return {
            "ratio": ratio,
            "market_price": market_price,
            "book_value_per_share": book_value_per_share,
            "interpretation": self._interpret_pb(ratio)
        }

    def _interpret_pb(self, ratio: float) -> str:
        if ratio > 3:
            return "High P/B - market expects high growth or strong intangibles"
        elif ratio > 1:
            return "Trading above book value - market values company positively"
        elif ratio > 0.5:
            return "Trading below book value - possible value opportunity or distress"
        else:
            return "Very low P/B - may indicate significant problems or undervaluation"


class ComprehensiveFundamentalAnalysis:
    """
    Comprehensive fundamental analysis combining all ratio categories.
    """

    def __init__(self):
        self.profitability = ProfitabilityRatios()
        self.liquidity = LiquidityRatios()
        self.solvency = SolvencyRatios()
        self.efficiency = EfficiencyRatios()
        self.dupont = DuPontAnalysis()
        self.cost_of_equity = CostOfEquityCalculator()

    def full_analysis(
        self,
        income_data: IncomeStatementData,
        balance_data: BalanceSheetData,
        prior_balance_data: BalanceSheetData = None
    ) -> Dict[str, Any]:
        """
        Perform comprehensive fundamental analysis.
        """
        # Calculate averages if prior data available
        if prior_balance_data:
            avg_assets = (balance_data.total_assets + prior_balance_data.total_assets) / 2
            avg_equity = (balance_data.total_equity + prior_balance_data.total_equity) / 2
            avg_inventory = (balance_data.inventory + prior_balance_data.inventory) / 2
            avg_receivables = (balance_data.accounts_receivable +
                              prior_balance_data.accounts_receivable) / 2
        else:
            avg_assets = balance_data.total_assets
            avg_equity = balance_data.total_equity
            avg_inventory = balance_data.inventory
            avg_receivables = balance_data.accounts_receivable

        results = {
            "profitability": {
                "gross_margin": self.profitability.calculate_gross_margin(
                    income_data.gross_profit, income_data.revenue
                ),
                "operating_margin": self.profitability.calculate_operating_margin(
                    income_data.operating_income, income_data.revenue
                ),
                "net_margin": self.profitability.calculate_net_profit_margin(
                    income_data.net_income, income_data.revenue
                ),
                "roe": self.profitability.calculate_roe(
                    income_data.net_income, avg_equity
                ),
                "roa": self.profitability.calculate_roa(
                    income_data.net_income, avg_assets
                )
            },
            "liquidity": {
                "current_ratio": self.liquidity.calculate_current_ratio(
                    balance_data.total_current_assets,
                    balance_data.total_current_liabilities
                ),
                "quick_ratio": self.liquidity.calculate_quick_ratio(
                    balance_data.total_current_assets,
                    balance_data.inventory,
                    balance_data.total_current_liabilities
                ),
                "cash_ratio": self.liquidity.calculate_cash_ratio(
                    balance_data.cash,
                    balance_data.marketable_securities,
                    balance_data.total_current_liabilities
                )
            },
            "solvency": {
                "debt_to_equity": self.solvency.calculate_debt_to_equity(
                    balance_data.long_term_debt + balance_data.short_term_debt,
                    balance_data.total_equity
                ),
                "interest_coverage": self.solvency.calculate_interest_coverage(
                    income_data.operating_income,
                    income_data.interest_expense
                ),
                "financial_leverage": self.solvency.calculate_financial_leverage(
                    balance_data.total_assets,
                    balance_data.total_equity
                )
            },
            "efficiency": {
                "asset_turnover": self.efficiency.calculate_asset_turnover(
                    income_data.revenue, avg_assets
                ),
                "inventory_turnover": self.efficiency.calculate_inventory_turnover(
                    income_data.cost_of_goods_sold, avg_inventory
                ),
                "receivables_turnover": self.efficiency.calculate_receivables_turnover(
                    income_data.revenue, avg_receivables
                )
            },
            "dupont_analysis": self.dupont.three_way_decomposition(
                income_data.net_income,
                income_data.revenue,
                avg_assets,
                avg_equity
            )
        }

        return results


def main():
    """CLI entry point for fundamental analysis."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Command required",
            "available_commands": [
                "profitability",
                "liquidity",
                "solvency",
                "efficiency",
                "dupont_3way",
                "dupont_5way",
                "cost_of_equity",
                "full_analysis"
            ]
        }))
        return

    command = sys.argv[1]

    try:
        if command == "profitability":
            calc = ProfitabilityRatios()
            result = {
                "gross_margin": calc.calculate_gross_margin(40000, 100000),
                "operating_margin": calc.calculate_operating_margin(20000, 100000),
                "net_margin": calc.calculate_net_profit_margin(15000, 100000),
                "roe": calc.calculate_roe(15000, 100000),
                "roa": calc.calculate_roa(15000, 200000)
            }
            print(json.dumps(result, indent=2))

        elif command == "liquidity":
            calc = LiquidityRatios()
            result = {
                "current_ratio": calc.calculate_current_ratio(150000, 100000),
                "quick_ratio": calc.calculate_quick_ratio(150000, 50000, 100000),
                "cash_ratio": calc.calculate_cash_ratio(30000, 20000, 100000)
            }
            print(json.dumps(result, indent=2))

        elif command == "solvency":
            calc = SolvencyRatios()
            result = {
                "debt_to_equity": calc.calculate_debt_to_equity(80000, 100000),
                "interest_coverage": calc.calculate_interest_coverage(25000, 5000),
                "debt_to_capital": calc.calculate_debt_to_capital(80000, 100000)
            }
            print(json.dumps(result, indent=2))

        elif command == "efficiency":
            calc = EfficiencyRatios()
            inv_turnover = calc.calculate_inventory_turnover(60000, 15000)
            rec_turnover = calc.calculate_receivables_turnover(100000, 12000)
            pay_turnover = calc.calculate_payables_turnover(60000, 10000)

            ccc = calc.calculate_cash_conversion_cycle(
                inv_turnover['days_in_inventory'],
                rec_turnover['days_sales_outstanding'],
                pay_turnover['days_payable_outstanding']
            )

            result = {
                "inventory_turnover": inv_turnover,
                "receivables_turnover": rec_turnover,
                "payables_turnover": pay_turnover,
                "cash_conversion_cycle": ccc
            }
            print(json.dumps(result, indent=2))

        elif command == "dupont_3way":
            analyzer = DuPontAnalysis()
            result = analyzer.three_way_decomposition(
                net_income=15000,
                revenue=100000,
                average_assets=200000,
                average_equity=100000
            )
            print(json.dumps(result, indent=2))

        elif command == "dupont_5way":
            analyzer = DuPontAnalysis()
            result = analyzer.five_way_decomposition(
                net_income=15000,
                pretax_income=20000,
                ebit=25000,
                revenue=100000,
                average_assets=200000,
                average_equity=100000
            )
            print(json.dumps(result, indent=2))

        elif command == "cost_of_equity":
            calc = CostOfEquityCalculator()
            capm = calc.capm(0.03, 1.2, 0.06)
            ddm = calc.dividend_growth_model(50, 2.0, 0.05)

            result = {
                "capm": capm,
                "dividend_growth_model": ddm,
                "comparison": calc.compare_roe_vs_cost_of_equity(0.15, capm['cost_of_equity'])
            }
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({"error": f"Unknown command: {command}"}))

    except Exception as e:
        print(json.dumps({"error": str(e)}))


if __name__ == "__main__":
    main()
