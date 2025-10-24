
"""
Financial Statement Balance Sheet Module
========================================

Balance sheet analysis and financial position assessment

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Management discussion and analysis sections
  - Auditor reports and financial statement footnotes
  - Industry benchmarks and competitor data
  - Economic indicators affecting financial performance

OUTPUT:
  - Financial analysis metrics and key performance indicators
  - Trend analysis and financial ratio calculations
  - Risk assessment and quality metrics
  - Comparative analysis and benchmarking results
  - Investment recommendations and insights

PARAMETERS:
  - analysis_period: Financial analysis period (default: 3 years)
  - industry_benchmark: Industry for comparative analysis (default: 'auto')
  - quality_threshold: Minimum financial quality score (default: 0.7)
  - growth_assumption: Growth rate assumption (default: 0.05)
  - currency: Reporting currency (default: 'USD')
"""


import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Tuple, Union
from dataclasses import dataclass, field
from enum import Enum
import logging

# Import from core modules
from ..core.base_analyzer import BaseAnalyzer, AnalysisResult, AnalysisType, RiskLevel, TrendDirection, \
    ComparativeAnalysis
from ..core.data_processor import FinancialStatements, ReportingStandard


class AssetQuality(Enum):
    """Asset quality classification"""
    HIGH_QUALITY = "high_quality"
    MODERATE_QUALITY = "moderate_quality"
    LOW_QUALITY = "low_quality"
    IMPAIRED = "impaired"


class LiabilityType(Enum):
    """Liability classification"""
    CURRENT = "current"
    NON_CURRENT = "non_current"
    CONTINGENT = "contingent"
    OFF_BALANCE_SHEET = "off_balance_sheet"


class EquityStructure(Enum):
    """Equity structure classification"""
    SIMPLE = "simple"
    COMPLEX = "complex"
    HIGHLY_LEVERAGED = "highly_leveraged"


@dataclass
class LiquidityAnalysis:
    """Comprehensive liquidity analysis results"""
    current_ratio: float
    quick_ratio: float
    cash_ratio: float
    working_capital: float
    working_capital_ratio: float
    net_working_capital: float
    liquidity_quality_score: float
    liquidity_risk_level: RiskLevel
    short_term_debt_coverage: float = None
    cash_conversion_cycle: float = None


@dataclass
class AssetAnalysis:
    """Detailed asset composition and quality analysis"""
    asset_turnover: float
    current_asset_ratio: float
    non_current_asset_ratio: float
    intangible_asset_ratio: float
    goodwill_ratio: float
    ppe_ratio: float
    asset_quality_score: float
    depreciation_rate: float = None
    asset_age_factor: float = None
    impairment_indicators: List[str] = field(default_factory=list)


@dataclass
class LiabilityAnalysis:
    """Comprehensive liability structure analysis"""
    debt_to_equity: float
    debt_to_assets: float
    current_liability_ratio: float
    long_term_debt_ratio: float
    interest_bearing_debt_ratio: float
    debt_maturity_profile: Dict[str, float] = field(default_factory=dict)
    off_balance_sheet_items: float = None
    contingent_liabilities: float = None


@dataclass
class EquityAnalysis:
    """Equity structure and quality analysis"""
    equity_ratio: float
    retained_earnings_ratio: float
    book_value_per_share: float
    tangible_book_value_per_share: float
    equity_multiplier: float
    return_on_equity: float = None
    dividend_coverage: float = None
    share_repurchase_activity: float = None


class BalanceSheetAnalyzer(BaseAnalyzer):
    """
    Comprehensive balance sheet analyzer implementing CFA Institute standards.
    Covers asset analysis, liability evaluation, liquidity assessment, and equity structure.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_balance_sheet_formulas()
        self._initialize_balance_sheet_benchmarks()

    def _initialize_balance_sheet_formulas(self):
        """Initialize balance sheet specific formulas"""
        self.formula_registry.update({
            'current_ratio': lambda current_assets, current_liabs: self.safe_divide(current_assets, current_liabs),
            'quick_ratio': lambda quick_assets, current_liabs: self.safe_divide(quick_assets, current_liabs),
            'cash_ratio': lambda cash, current_liabs: self.safe_divide(cash, current_liabs),
            'debt_to_equity': lambda total_debt, total_equity: self.safe_divide(total_debt, total_equity),
            'debt_to_assets': lambda total_debt, total_assets: self.safe_divide(total_debt, total_assets),
            'asset_turnover': lambda revenue, avg_total_assets: self.safe_divide(revenue, avg_total_assets),
            'equity_multiplier': lambda total_assets, total_equity: self.safe_divide(total_assets, total_equity),
            'working_capital_ratio': lambda working_capital, total_assets: self.safe_divide(working_capital,
                                                                                            total_assets)
        })

    def _initialize_balance_sheet_benchmarks(self):
        """Initialize balance sheet specific benchmarks"""
        # Asset composition benchmarks (industry-dependent)
        self.asset_composition_benchmarks = {
            'current_asset_ratio': {'high': 0.4, 'moderate': 0.3, 'low': 0.2},
            'intangible_ratio': {'high': 0.3, 'moderate': 0.15, 'low': 0.05},
            'goodwill_ratio': {'high': 0.2, 'moderate': 0.1, 'low': 0.05}
        }

        # Liability structure benchmarks
        self.liability_benchmarks = {
            'current_liability_ratio': {'high': 0.4, 'moderate': 0.3, 'low': 0.2},
            'long_term_debt_ratio': {'high': 0.4, 'moderate': 0.25, 'low': 0.15}
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive balance sheet analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all balance sheet aspects
        """
        results = []

        # Validate data sufficiency
        required_fields = ['total_assets', 'total_liabilities', 'total_equity']
        is_sufficient, missing_fields = self.validate_data_sufficiency(statements, required_fields)

        if not is_sufficient:
            if self.logger:
                self.logger.warning(f"Insufficient data for complete analysis. Missing: {missing_fields}")

        # Liquidity analysis
        results.extend(self._analyze_liquidity(statements, comparative_data, industry_data))

        # Asset analysis
        results.extend(self._analyze_assets(statements, comparative_data, industry_data))

        # Liability analysis
        results.extend(self._analyze_liabilities(statements, comparative_data, industry_data))

        # Equity analysis
        results.extend(self._analyze_equity(statements, comparative_data, industry_data))

        # Financial position quality
        results.extend(self._assess_financial_position_quality(statements, comparative_data))

        # Common-size analysis
        results.extend(self._perform_common_size_analysis(statements, comparative_data))

        # Balance sheet relationships
        results.extend(self._analyze_balance_sheet_relationships(statements, comparative_data))

        return results

    def _analyze_liquidity(self, statements: FinancialStatements,
                           comparative_data: Optional[List[FinancialStatements]] = None,
                           industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive liquidity analysis"""
        results = []
        balance_sheet = statements.balance_sheet

        current_assets = balance_sheet.get('current_assets', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        cash_equivalents = balance_sheet.get('cash_equivalents', 0)
        accounts_receivable = balance_sheet.get('accounts_receivable', 0)
        inventory = balance_sheet.get('inventory', 0)

        # Current Ratio
        if current_liabilities > 0:
            current_ratio = self.safe_divide(current_assets, current_liabilities)
            benchmark = self.liquidity_benchmarks.get('current_ratio', {})
            risk_level = self.assess_risk_level(current_ratio, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.LIQUIDITY,
                metric_name="Current Ratio",
                value=current_ratio,
                interpretation=self.generate_interpretation("current ratio", current_ratio, risk_level,
                                                            AnalysisType.LIQUIDITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(current_ratio, industry_data.get(
                    'current_ratio') if industry_data else None),
                methodology="Current Assets / Current Liabilities",
                limitations=["Does not consider asset quality or conversion timing"]
            ))

        # Quick Ratio (Acid Test)
        if current_liabilities > 0:
            quick_assets = current_assets - inventory  # Excluding inventory
            quick_ratio = self.safe_divide(quick_assets, current_liabilities)
            benchmark = self.liquidity_benchmarks.get('quick_ratio', {})
            risk_level = self.assess_risk_level(quick_ratio, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.LIQUIDITY,
                metric_name="Quick Ratio",
                value=quick_ratio,
                interpretation=self.generate_interpretation("quick ratio", quick_ratio, risk_level,
                                                            AnalysisType.LIQUIDITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(quick_ratio, industry_data.get(
                    'quick_ratio') if industry_data else None),
                methodology="(Current Assets - Inventory) / Current Liabilities",
                limitations=["Assumes receivables are readily collectible"]
            ))

        # Cash Ratio
        if current_liabilities > 0:
            cash_ratio = self.safe_divide(cash_equivalents, current_liabilities)
            benchmark = self.liquidity_benchmarks.get('cash_ratio', {})
            risk_level = self.assess_risk_level(cash_ratio, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.LIQUIDITY,
                metric_name="Cash Ratio",
                value=cash_ratio,
                interpretation=self.generate_interpretation("cash ratio", cash_ratio, risk_level,
                                                            AnalysisType.LIQUIDITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(cash_ratio, industry_data.get(
                    'cash_ratio') if industry_data else None),
                methodology="Cash and Cash Equivalents / Current Liabilities",
                limitations=["Most conservative liquidity measure"]
            ))

        # Working Capital Analysis
        working_capital = current_assets - current_liabilities
        total_assets = balance_sheet.get('total_assets', 0)

        if total_assets > 0:
            working_capital_ratio = self.safe_divide(working_capital, total_assets)

            wc_interpretation = "Strong working capital position" if working_capital_ratio > 0.1 else "Adequate working capital" if working_capital_ratio > 0 else "Negative working capital - liquidity concern"
            wc_risk = RiskLevel.LOW if working_capital_ratio > 0.1 else RiskLevel.MODERATE if working_capital_ratio > 0 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.LIQUIDITY,
                metric_name="Working Capital Ratio",
                value=working_capital_ratio,
                interpretation=wc_interpretation,
                risk_level=wc_risk,
                methodology="(Current Assets - Current Liabilities) / Total Assets",
                limitations=["Industry-dependent optimal levels"]
            ))

        return results

    def _analyze_assets(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive asset analysis"""
        results = []
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        total_assets = balance_sheet.get('total_assets', 0)
        current_assets = balance_sheet.get('current_assets', 0)
        ppe_net = balance_sheet.get('ppe_net', 0)
        intangible_assets = balance_sheet.get('intangible_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)
        revenue = income_statement.get('revenue', 0)

        if total_assets == 0:
            return results

        # Asset Turnover
        if revenue > 0:
            # Calculate average assets if comparative data available
            avg_total_assets = total_assets
            if comparative_data and len(comparative_data) > 0:
                prev_assets = comparative_data[-1].balance_sheet.get('total_assets', 0)
                if prev_assets > 0:
                    avg_total_assets = (total_assets + prev_assets) / 2

            asset_turnover = self.safe_divide(revenue, avg_total_assets)
            benchmark = self.activity_benchmarks.get('asset_turnover', {})
            risk_level = self.assess_risk_level(asset_turnover, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Asset Turnover",
                value=asset_turnover,
                interpretation=self.generate_interpretation("asset turnover", asset_turnover, risk_level,
                                                            AnalysisType.ACTIVITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(asset_turnover, industry_data.get(
                    'asset_turnover') if industry_data else None),
                methodology="Revenue / Average Total Assets",
                limitations=["Influenced by asset age and accounting methods"]
            ))

        # Asset Composition Analysis
        current_asset_ratio = self.safe_divide(current_assets, total_assets)
        results.append(AnalysisResult(
            analysis_type=AnalysisType.ACTIVITY,
            metric_name="Current Asset Ratio",
            value=current_asset_ratio,
            interpretation=f"Current assets represent {self.format_percentage(current_asset_ratio)} of total assets",
            risk_level=RiskLevel.LOW,
            methodology="Current Assets / Total Assets"
        ))

        # PPE Ratio
        ppe_ratio = self.safe_divide(ppe_net, total_assets)
        results.append(AnalysisResult(
            analysis_type=AnalysisType.ACTIVITY,
            metric_name="PPE Ratio",
            value=ppe_ratio,
            interpretation=f"Property, plant & equipment represents {self.format_percentage(ppe_ratio)} of total assets",
            risk_level=RiskLevel.LOW,
            methodology="Net PPE / Total Assets"
        ))

        # Intangible Assets Analysis
        if intangible_assets > 0:
            intangible_ratio = self.safe_divide(intangible_assets, total_assets)

            intangible_interpretation = "High intangible asset intensity - knowledge-based business" if intangible_ratio > 0.2 else "Moderate intangible assets" if intangible_ratio > 0.1 else "Low intangible asset base"
            intangible_risk = RiskLevel.MODERATE if intangible_ratio > 0.3 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Intangible Asset Ratio",
                value=intangible_ratio,
                interpretation=intangible_interpretation,
                risk_level=intangible_risk,
                methodology="Intangible Assets / Total Assets",
                limitations=["Requires assessment of asset impairment risk"]
            ))

        # Goodwill Analysis
        if goodwill > 0:
            goodwill_ratio = self.safe_divide(goodwill, total_assets)

            goodwill_interpretation = "Significant goodwill from acquisitions - monitor for impairment" if goodwill_ratio > 0.15 else "Moderate goodwill level" if goodwill_ratio > 0.05 else "Low goodwill"
            goodwill_risk = RiskLevel.MODERATE if goodwill_ratio > 0.2 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Goodwill Ratio",
                value=goodwill_ratio,
                interpretation=goodwill_interpretation,
                risk_level=goodwill_risk,
                methodology="Goodwill / Total Assets",
                limitations=["Subject to impairment testing and write-downs"]
            ))

        # Asset Quality Assessment
        results.extend(self._assess_asset_quality(statements, comparative_data))

        return results

    def _analyze_liabilities(self, statements: FinancialStatements,
                             comparative_data: Optional[List[FinancialStatements]] = None,
                             industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive liability analysis"""
        results = []
        balance_sheet = statements.balance_sheet

        total_assets = balance_sheet.get('total_assets', 0)
        total_liabilities = balance_sheet.get('total_liabilities', 0)
        total_equity = balance_sheet.get('total_equity', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        long_term_debt = balance_sheet.get('long_term_debt', 0)
        short_term_debt = balance_sheet.get('short_term_debt', 0)

        if total_assets == 0:
            return results

        # Total debt calculation
        total_debt = long_term_debt + short_term_debt

        # Debt-to-Equity Ratio
        if total_equity > 0:
            debt_to_equity = self.safe_divide(total_debt, total_equity)
            benchmark = self.solvency_benchmarks.get('debt_to_equity', {})
            risk_level = self.assess_risk_level(debt_to_equity, benchmark, higher_is_better=False)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt-to-Equity Ratio",
                value=debt_to_equity,
                interpretation=self.generate_interpretation("debt-to-equity ratio", debt_to_equity, risk_level,
                                                            AnalysisType.SOLVENCY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(debt_to_equity, industry_data.get(
                    'debt_to_equity') if industry_data else None),
                methodology="Total Debt / Total Equity",
                limitations=["Does not consider off-balance-sheet obligations"]
            ))

        # Debt-to-Assets Ratio
        debt_to_assets = self.safe_divide(total_debt, total_assets)
        benchmark = self.solvency_benchmarks.get('debt_to_assets', {})
        risk_level = self.assess_risk_level(debt_to_assets, benchmark, higher_is_better=False)

        results.append(AnalysisResult(
            analysis_type=AnalysisType.SOLVENCY,
            metric_name="Debt-to-Assets Ratio",
            value=debt_to_assets,
            interpretation=self.generate_interpretation("debt-to-assets ratio", debt_to_assets, risk_level,
                                                        AnalysisType.SOLVENCY),
            risk_level=risk_level,
            benchmark_comparison=self.compare_to_industry(debt_to_assets, industry_data.get(
                'debt_to_assets') if industry_data else None),
            methodology="Total Debt / Total Assets",
            limitations=["Asset values may not reflect market values"]
        ))

        # Liability Structure Analysis
        current_liability_ratio = self.safe_divide(current_liabilities, total_assets)
        long_term_liability_ratio = self.safe_divide(long_term_debt, total_assets)

        results.append(AnalysisResult(
            analysis_type=AnalysisType.SOLVENCY,
            metric_name="Current Liability Ratio",
            value=current_liability_ratio,
            interpretation=f"Current liabilities represent {self.format_percentage(current_liability_ratio)} of total assets",
            risk_level=RiskLevel.HIGH if current_liability_ratio > 0.4 else RiskLevel.MODERATE if current_liability_ratio > 0.25 else RiskLevel.LOW,
            methodology="Current Liabilities / Total Assets"
        ))

        results.append(AnalysisResult(
            analysis_type=AnalysisType.SOLVENCY,
            metric_name="Long-term Debt Ratio",
            value=long_term_liability_ratio,
            interpretation=f"Long-term debt represents {self.format_percentage(long_term_liability_ratio)} of total assets",
            risk_level=RiskLevel.HIGH if long_term_liability_ratio > 0.4 else RiskLevel.MODERATE if long_term_liability_ratio > 0.25 else RiskLevel.LOW,
            methodology="Long-term Debt / Total Assets"
        ))

        # Debt Maturity Analysis
        if total_debt > 0:
            short_term_debt_ratio = self.safe_divide(short_term_debt, total_debt)

            maturity_interpretation = "High short-term debt concentration - refinancing risk" if short_term_debt_ratio > 0.5 else "Balanced debt maturity profile" if short_term_debt_ratio > 0.2 else "Predominantly long-term debt structure"
            maturity_risk = RiskLevel.HIGH if short_term_debt_ratio > 0.6 else RiskLevel.MODERATE if short_term_debt_ratio > 0.4 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Short-term Debt Concentration",
                value=short_term_debt_ratio,
                interpretation=maturity_interpretation,
                risk_level=maturity_risk,
                methodology="Short-term Debt / Total Debt",
                limitations=["Does not consider debt covenants or refinancing ability"]
            ))

        # Interest Coverage Analysis (if income statement data available)
        income_statement = statements.income_statement
        operating_income = income_statement.get('operating_income', 0)
        interest_expense = income_statement.get('interest_expense', 0)

        if interest_expense > 0:
            interest_coverage = self.safe_divide(operating_income, interest_expense)
            benchmark = self.solvency_benchmarks.get('interest_coverage', {})
            risk_level = self.assess_risk_level(interest_coverage, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Interest Coverage Ratio",
                value=interest_coverage,
                interpretation=self.generate_interpretation("interest coverage ratio", interest_coverage, risk_level,
                                                            AnalysisType.SOLVENCY),
                risk_level=risk_level,
                methodology="Operating Income / Interest Expense",
                limitations=["Based on current operating performance"]
            ))

        return results

    def _analyze_equity(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive equity analysis"""
        results = []
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        total_assets = balance_sheet.get('total_assets', 0)
        total_equity = balance_sheet.get('total_equity', 0)
        common_stock = balance_sheet.get('common_stock', 0)
        retained_earnings = balance_sheet.get('retained_earnings', 0)
        treasury_stock = balance_sheet.get('treasury_stock', 0)
        intangible_assets = balance_sheet.get('intangible_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)

        if total_assets == 0:
            return results

        # Equity Ratio
        equity_ratio = self.safe_divide(total_equity, total_assets)

        equity_interpretation = "Strong equity position - low financial leverage" if equity_ratio > 0.6 else "Moderate equity position" if equity_ratio > 0.4 else "High financial leverage - elevated risk"
        equity_risk = RiskLevel.LOW if equity_ratio > 0.5 else RiskLevel.MODERATE if equity_ratio > 0.3 else RiskLevel.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.SOLVENCY,
            metric_name="Equity Ratio",
            value=equity_ratio,
            interpretation=equity_interpretation,
            risk_level=equity_risk,
            benchmark_comparison=self.compare_to_industry(equity_ratio,
                                                          industry_data.get('equity_ratio') if industry_data else None),
            methodology="Total Equity / Total Assets"
        ))

        # Equity Multiplier
        if total_equity > 0:
            equity_multiplier = self.safe_divide(total_assets, total_equity)

            multiplier_interpretation = "High financial leverage" if equity_multiplier > 3 else "Moderate financial leverage" if equity_multiplier > 2 else "Conservative financial leverage"
            multiplier_risk = RiskLevel.HIGH if equity_multiplier > 4 else RiskLevel.MODERATE if equity_multiplier > 2.5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Equity Multiplier",
                value=equity_multiplier,
                interpretation=multiplier_interpretation,
                risk_level=multiplier_risk,
                methodology="Total Assets / Total Equity",
                limitations=["Component of DuPont analysis"]
            ))

        # Retained Earnings Analysis
        if total_equity > 0 and retained_earnings != 0:
            retained_earnings_ratio = self.safe_divide(retained_earnings, total_equity)

            re_interpretation = "Strong retained earnings base" if retained_earnings_ratio > 0.5 else "Moderate retained earnings" if retained_earnings_ratio > 0.2 else "Low retained earnings - recent losses or high dividends"
            re_risk = RiskLevel.LOW if retained_earnings_ratio > 0.3 else RiskLevel.MODERATE if retained_earnings_ratio > 0 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Retained Earnings Ratio",
                value=retained_earnings_ratio,
                interpretation=re_interpretation,
                risk_level=re_risk,
                methodology="Retained Earnings / Total Equity"
            ))

        # Book Value per Share (if share data available)
        shares_outstanding = income_statement.get('shares_outstanding_basic', 0)
        if shares_outstanding > 0 and total_equity > 0:
            book_value_per_share = self.safe_divide(total_equity, shares_outstanding)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.VALUATION,
                metric_name="Book Value per Share",
                value=book_value_per_share,
                interpretation=f"Book value per share is ${book_value_per_share:.2f}",
                risk_level=RiskLevel.LOW,
                methodology="Total Equity / Shares Outstanding"
            ))

            # Tangible Book Value per Share
            tangible_equity = total_equity - intangible_assets - goodwill
            if tangible_equity > 0:
                tangible_bvps = self.safe_divide(tangible_equity, shares_outstanding)

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.VALUATION,
                    metric_name="Tangible Book Value per Share",
                    value=tangible_bvps,
                    interpretation=f"Tangible book value per share is ${tangible_bvps:.2f}",
                    risk_level=RiskLevel.LOW,
                    methodology="(Total Equity - Intangibles - Goodwill) / Shares Outstanding",
                    limitations=["Excludes intangible asset value"]
                ))

        # Return on Equity (if net income available)
        net_income = income_statement.get('net_income', 0)
        if total_equity > 0 and net_income != 0:
            # Calculate average equity if comparative data available
            avg_equity = total_equity
            if comparative_data and len(comparative_data) > 0:
                prev_equity = comparative_data[-1].balance_sheet.get('total_equity', 0)
                if prev_equity > 0:
                    avg_equity = (total_equity + prev_equity) / 2

            roe = self.safe_divide(net_income, avg_equity)
            benchmark = self.profitability_benchmarks.get('roe', {})
            risk_level = self.assess_risk_level(roe, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Return on Equity",
                value=roe,
                interpretation=self.generate_interpretation("return on equity", roe, risk_level,
                                                            AnalysisType.PROFITABILITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(roe, industry_data.get('roe') if industry_data else None),
                methodology="Net Income / Average Total Equity"
            ))

        return results

    def _assess_asset_quality(self, statements: FinancialStatements,
                              comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Assess asset quality and potential impairment issues"""
        results = []
        balance_sheet = statements.balance_sheet

        # Asset age analysis (if depreciation data available)
        ppe_gross = balance_sheet.get('ppe_gross', 0)
        accumulated_depreciation = balance_sheet.get('accumulated_depreciation', 0)

        if ppe_gross > 0 and accumulated_depreciation > 0:
            asset_age_ratio = self.safe_divide(accumulated_depreciation, ppe_gross)

            age_interpretation = "Assets approaching end of useful life - significant capex likely needed" if asset_age_ratio > 0.7 else "Moderately aged assets" if asset_age_ratio > 0.5 else "Relatively new assets"
            age_risk = RiskLevel.HIGH if asset_age_ratio > 0.8 else RiskLevel.MODERATE if asset_age_ratio > 0.6 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Asset Age Ratio",
                value=asset_age_ratio,
                interpretation=age_interpretation,
                risk_level=age_risk,
                methodology="Accumulated Depreciation / Gross PPE",
                limitations=["Based on historical cost and depreciation methods"]
            ))

        # Impairment indicators
        impairment_indicators = self._identify_impairment_indicators(statements, comparative_data)
        if impairment_indicators:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Asset Impairment Indicators",
                value=len(impairment_indicators),
                interpretation=f"Identified {len(impairment_indicators)} potential impairment indicators",
                risk_level=RiskLevel.HIGH if len(impairment_indicators) > 2 else RiskLevel.MODERATE,
                limitations=impairment_indicators
            ))

        return results

    def _assess_financial_position_quality(self, statements: FinancialStatements,
                                           comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Assess overall financial position quality"""
        results = []

        # Balance sheet strength score
        balance_sheet = statements.balance_sheet

        # Quality factors
        quality_factors = []
        quality_score = 100

        # Factor 1: Liquidity position
        current_assets = balance_sheet.get('current_assets', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        if current_liabilities > 0:
            current_ratio = self.safe_divide(current_assets, current_liabilities)
            if current_ratio >= 1.5:
                quality_factors.append("Strong liquidity position")
            elif current_ratio < 1.0:
                quality_score -= 20

        # Factor 2: Debt levels
        total_assets = balance_sheet.get('total_assets', 0)
        total_debt = balance_sheet.get('long_term_debt', 0) + balance_sheet.get('short_term_debt', 0)
        if total_assets > 0:
            debt_ratio = self.safe_divide(total_debt, total_assets)
            if debt_ratio > 0.6:
                quality_score -= 25
            elif debt_ratio < 0.3:
                quality_factors.append("Conservative debt levels")

        # Factor 3: Asset composition
        intangible_assets = balance_sheet.get('intangible_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)
        if total_assets > 0:
            intangible_ratio = self.safe_divide(intangible_assets + goodwill, total_assets)
            if intangible_ratio > 0.4:
                quality_score -= 15

        # Factor 4: Profitability (if available)
        income_statement = statements.income_statement
        net_income = income_statement.get('net_income', 0)
        if net_income < 0:
            quality_score -= 20

        quality_score = max(0, quality_score)

        quality_interpretation = "Excellent financial position" if quality_score > 80 else "Good financial position" if quality_score > 60 else "Fair financial position" if quality_score > 40 else "Weak financial position"
        quality_risk = RiskLevel.LOW if quality_score > 70 else RiskLevel.MODERATE if quality_score > 50 else RiskLevel.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Financial Position Quality Score",
            value=quality_score,
            interpretation=quality_interpretation,
            risk_level=quality_risk,
            recommendations=quality_factors,
            methodology="Composite score based on liquidity, leverage, asset quality, and profitability"
        ))

        return results

    def _perform_common_size_analysis(self, statements: FinancialStatements,
                                      comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Perform common-size balance sheet analysis"""
        results = []
        balance_sheet = statements.balance_sheet
        total_assets = balance_sheet.get('total_assets', 0)

        if total_assets == 0:
            return results

        # Asset composition as % of total assets
        asset_items = {
            'Current Assets': balance_sheet.get('current_assets', 0),
            'PPE Net': balance_sheet.get('ppe_net', 0),
            'Intangible Assets': balance_sheet.get('intangible_assets', 0),
            'Goodwill': balance_sheet.get('goodwill', 0)
        }

        for item_name, item_value in asset_items.items():
            if item_value > 0:
                common_size_pct = self.safe_divide(item_value, total_assets)

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name=f"{item_name} as % of Total Assets",
                    value=common_size_pct,
                    interpretation=f"{item_name} represents {self.format_percentage(common_size_pct)} of total assets",
                    risk_level=RiskLevel.LOW,
                    methodology=f"{item_name} / Total Assets"
                ))

        # Liability and equity composition
        liability_equity_items = {
            'Current Liabilities': balance_sheet.get('current_liabilities', 0),
            'Long-term Debt': balance_sheet.get('long_term_debt', 0),
            'Total Equity': balance_sheet.get('total_equity', 0)
        }

        for item_name, item_value in liability_equity_items.items():
            if item_value != 0:  # Include negative values
                common_size_pct = self.safe_divide(item_value, total_assets)

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name=f"{item_name} as % of Total Assets",
                    value=common_size_pct,
                    interpretation=f"{item_name} represents {self.format_percentage(common_size_pct)} of total assets",
                    risk_level=RiskLevel.LOW,
                    methodology=f"{item_name} / Total Assets"
                ))

        return results

    def _analyze_balance_sheet_relationships(self, statements: FinancialStatements,
                                             comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze key balance sheet relationships and efficiency metrics"""
        results = []

        # Asset-Liability matching analysis
        balance_sheet = statements.balance_sheet
        current_assets = balance_sheet.get('current_assets', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        long_term_assets = balance_sheet.get('total_assets', 0) - current_assets
        long_term_debt = balance_sheet.get('long_term_debt', 0)

        # Financing appropriateness
        if long_term_assets > 0 and (long_term_debt + balance_sheet.get('total_equity', 0)) > 0:
            long_term_financing = long_term_debt + balance_sheet.get('total_equity', 0)
            financing_ratio = self.safe_divide(long_term_financing, long_term_assets)

            financing_interpretation = "Appropriate long-term financing for long-term assets" if financing_ratio >= 1.0 else "Potential maturity mismatch - long-term assets financed with short-term funds"
            financing_risk = RiskLevel.LOW if financing_ratio >= 1.0 else RiskLevel.MODERATE if financing_ratio >= 0.8 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Long-term Financing Ratio",
                value=financing_ratio,
                interpretation=financing_interpretation,
                risk_level=financing_risk,
                methodology="(Long-term Debt + Equity) / Long-term Assets",
                limitations=["Simplified maturity matching analysis"]
            ))

        return results

    def _identify_impairment_indicators(self, statements: FinancialStatements,
                                        comparative_data: Optional[List[FinancialStatements]] = None) -> List[str]:
        """Identify potential asset impairment indicators"""
        indicators = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        # Declining profitability
        net_income = income_statement.get('net_income', 0)
        if net_income < 0:
            indicators.append("Negative net income may indicate asset impairment")

        # High goodwill relative to market cap (would need market data)
        goodwill = balance_sheet.get('goodwill', 0)
        total_assets = balance_sheet.get('total_assets', 0)
        if goodwill > 0 and total_assets > 0:
            goodwill_ratio = self.safe_divide(goodwill, total_assets)
            if goodwill_ratio > 0.3:
                indicators.append("High goodwill concentration - monitor for impairment")

        # Declining asset utilization
        if comparative_data and len(comparative_data) > 0:
            revenue = income_statement.get('revenue', 0)
            prev_revenue = comparative_data[-1].income_statement.get('revenue', 0)

            if prev_revenue > 0 and revenue < prev_revenue * 0.9:  # 10% decline
                indicators.append("Significant revenue decline may indicate asset impairment")

        return indicators

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key balance sheet metrics"""
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        metrics = {}

        # Liquidity metrics
        current_assets = balance_sheet.get('current_assets', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        cash_equivalents = balance_sheet.get('cash_equivalents', 0)

        if current_liabilities > 0:
            metrics['current_ratio'] = self.safe_divide(current_assets, current_liabilities)
            metrics['quick_ratio'] = self.safe_divide(current_assets - balance_sheet.get('inventory', 0),
                                                      current_liabilities)
            metrics['cash_ratio'] = self.safe_divide(cash_equivalents, current_liabilities)

        # Solvency metrics
        total_assets = balance_sheet.get('total_assets', 0)
        total_equity = balance_sheet.get('total_equity', 0)
        total_debt = balance_sheet.get('long_term_debt', 0) + balance_sheet.get('short_term_debt', 0)

        if total_equity > 0:
            metrics['debt_to_equity'] = self.safe_divide(total_debt, total_equity)
            metrics['equity_multiplier'] = self.safe_divide(total_assets, total_equity)

        if total_assets > 0:
            metrics['debt_to_assets'] = self.safe_divide(total_debt, total_assets)
            metrics['equity_ratio'] = self.safe_divide(total_equity, total_assets)

        # Activity metrics
        revenue = income_statement.get('revenue', 0)
        if total_assets > 0 and revenue > 0:
            metrics['asset_turnover'] = self.safe_divide(revenue, total_assets)

        # Return metrics
        net_income = income_statement.get('net_income', 0)
        if total_assets > 0:
            metrics['roa'] = self.safe_divide(net_income, total_assets)
        if total_equity > 0:
            metrics['roe'] = self.safe_divide(net_income, total_equity)

        return metrics

    def create_liquidity_analysis(self, statements: FinancialStatements) -> LiquidityAnalysis:
        """Create comprehensive liquidity analysis object"""
        balance_sheet = statements.balance_sheet

        current_assets = balance_sheet.get('current_assets', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        cash_equivalents = balance_sheet.get('cash_equivalents', 0)
        inventory = balance_sheet.get('inventory', 0)

        # Calculate ratios
        current_ratio = self.safe_divide(current_assets, current_liabilities)
        quick_ratio = self.safe_divide(current_assets - inventory, current_liabilities)
        cash_ratio = self.safe_divide(cash_equivalents, current_liabilities)

        working_capital = current_assets - current_liabilities
        total_assets = balance_sheet.get('total_assets', 0)
        working_capital_ratio = self.safe_divide(working_capital, total_assets)

        # Assess liquidity quality
        quality_score = 100
        if current_ratio < 1.0:
            quality_score -= 30
        elif current_ratio < 1.2:
            quality_score -= 15

        if quick_ratio < 0.8:
            quality_score -= 20

        if cash_ratio < 0.1:
            quality_score -= 10

        quality_score = max(0, quality_score)

        # Determine risk level
        if quality_score > 80:
            risk_level = RiskLevel.LOW
        elif quality_score > 60:
            risk_level = RiskLevel.MODERATE
        else:
            risk_level = RiskLevel.HIGH

        return LiquidityAnalysis(
            current_ratio=current_ratio,
            quick_ratio=quick_ratio,
            cash_ratio=cash_ratio,
            working_capital=working_capital,
            working_capital_ratio=working_capital_ratio,
            net_working_capital=working_capital,
            liquidity_quality_score=quality_score,
            liquidity_risk_level=risk_level
        )