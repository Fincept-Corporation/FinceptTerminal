
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
            comparative_data: Historical financial statements (oldest first)
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all balance sheet aspects
        """
        results = []

        required_fields = ['total_assets', 'total_liabilities', 'total_equity']
        is_sufficient, missing_fields = self.validate_data_sufficiency(statements, required_fields)
        if not is_sufficient and self.logger:
            self.logger.warning(f"Insufficient data for complete analysis. Missing: {missing_fields}")

        results.extend(self._analyze_liquidity(statements, comparative_data, industry_data))
        results.extend(self._analyze_assets(statements, comparative_data, industry_data))
        results.extend(self._analyze_liabilities(statements, comparative_data, industry_data))
        results.extend(self._analyze_equity(statements, comparative_data, industry_data))
        results.extend(self._assess_financial_position_quality(statements, comparative_data))
        results.extend(self._perform_common_size_analysis(statements, comparative_data))
        results.extend(self._analyze_balance_sheet_relationships(statements, comparative_data))

        return results

    # ------------------------------------------------------------------ helpers

    @staticmethod
    def _quick_assets(bs: Dict[str, float]) -> Tuple[Optional[float], str]:
        """CFA quick assets = cash + short-term marketable investments + receivables.
        Falls back to current assets - inventory - prepaid only when none of the three
        components is reported."""
        parts = [k for k in ('cash_equivalents', 'short_term_investments', 'accounts_receivable') if k in bs]
        if parts:
            return sum(bs[k] for k in parts), "(Cash + Short-term Investments + Receivables) / Current Liabilities"
        if 'current_assets' in bs:
            value = bs['current_assets'] - bs.get('inventory', 0.0) - bs.get('prepaid_expenses', 0.0)
            return value, ("(Current Assets - Inventory - Prepaid) / Current Liabilities "
                           "(cash, investments and receivables not reported separately)")
        return None, ""

    @staticmethod
    def _cash_and_investments(bs: Dict[str, float]) -> Optional[float]:
        if 'cash_equivalents' not in bs and 'short_term_investments' not in bs:
            return None
        return bs.get('cash_equivalents', 0.0) + bs.get('short_term_investments', 0.0)

    @staticmethod
    def _shares(statements: FinancialStatements) -> Tuple[Optional[float], str]:
        """Period-end shares outstanding if supplied, else weighted average basic shares."""
        shares = statements.balance_sheet.get('shares_outstanding')
        if shares and shares > 0:
            return shares, "period-end shares outstanding"
        shares = statements.income_statement.get('shares_outstanding_basic')
        if shares and shares > 0:
            return shares, "weighted average basic shares (period-end count not supplied)"
        return None, ""

    # ---------------------------------------------------------------- liquidity

    def _analyze_liquidity(self, statements: FinancialStatements,
                           comparative_data: Optional[List[FinancialStatements]] = None,
                           industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive liquidity analysis"""
        results = []
        bs = statements.balance_sheet

        current_assets = bs.get('current_assets')
        current_liabilities = bs.get('current_liabilities')

        if current_liabilities is None or current_liabilities <= 0:
            reason = ("current liabilities not reported (e.g. unclassified balance sheet of a bank or insurer)"
                      if current_liabilities is None else "current liabilities are zero or negative")
            results.append(self.unavailable(AnalysisType.LIQUIDITY, "Liquidity Ratios", reason,
                                            "Current, quick and cash ratios divide by current liabilities"))
            return results

        # Current Ratio
        if current_assets is not None:
            current_ratio = current_assets / current_liabilities
            risk_level = self.assess_risk_level(current_ratio, self.liquidity_benchmarks['current_ratio'], True)
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
        else:
            results.append(self.unavailable(AnalysisType.LIQUIDITY, "Current Ratio",
                                            "current assets not reported", "Current Assets / Current Liabilities"))

        # Quick Ratio (Acid Test)
        quick_assets, quick_method = self._quick_assets(bs)
        if quick_assets is not None:
            quick_ratio = quick_assets / current_liabilities
            risk_level = self.assess_risk_level(quick_ratio, self.liquidity_benchmarks['quick_ratio'], True)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.LIQUIDITY,
                metric_name="Quick Ratio",
                value=quick_ratio,
                interpretation=self.generate_interpretation("quick ratio", quick_ratio, risk_level,
                                                            AnalysisType.LIQUIDITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(quick_ratio, industry_data.get(
                    'quick_ratio') if industry_data else None),
                methodology=quick_method,
                limitations=["Assumes receivables are readily collectible"]
            ))

        # Cash Ratio
        cash_and_inv = self._cash_and_investments(bs)
        if cash_and_inv is not None:
            cash_ratio = cash_and_inv / current_liabilities
            risk_level = self.assess_risk_level(cash_ratio, self.liquidity_benchmarks['cash_ratio'], True)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.LIQUIDITY,
                metric_name="Cash Ratio",
                value=cash_ratio,
                interpretation=self.generate_interpretation("cash ratio", cash_ratio, risk_level,
                                                            AnalysisType.LIQUIDITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(cash_ratio, industry_data.get(
                    'cash_ratio') if industry_data else None),
                methodology="(Cash + Short-term Marketable Investments) / Current Liabilities",
                limitations=["Most conservative liquidity measure"]
            ))

        # Working Capital Ratio
        total_assets = bs.get('total_assets')
        if current_assets is not None and total_assets and total_assets > 0:
            working_capital_ratio = (current_assets - current_liabilities) / total_assets
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

    # ------------------------------------------------------------------- assets

    def _analyze_assets(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive asset analysis"""
        results = []
        bs = statements.balance_sheet
        revenue = statements.income_statement.get('revenue')
        total_assets = bs.get('total_assets')

        if total_assets is None or total_assets <= 0:
            results.append(self.unavailable(AnalysisType.ACTIVITY, "Asset Analysis",
                                            "total assets not reported or not positive"))
            return results

        # Asset Turnover
        if revenue is not None and revenue > 0:
            avg_total_assets, basis = self.average_balance(statements, comparative_data, 'total_assets')
            asset_turnover = revenue / avg_total_assets
            risk_level = self.assess_risk_level(asset_turnover, self.activity_benchmarks['asset_turnover'], True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Asset Turnover",
                value=asset_turnover,
                interpretation=self.generate_interpretation("asset turnover", asset_turnover, risk_level,
                                                            AnalysisType.ACTIVITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(asset_turnover, industry_data.get(
                    'asset_turnover') if industry_data else None),
                methodology=f"Revenue / Total Assets ({self.basis_note(basis)})",
                limitations=["Influenced by asset age and accounting methods"]
            ))

        if 'current_assets' in bs:
            current_asset_ratio = bs['current_assets'] / total_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Current Asset Ratio",
                value=current_asset_ratio,
                interpretation=f"Current assets represent {self.format_percentage(current_asset_ratio)} of total assets",
                risk_level=RiskLevel.LOW,
                methodology="Current Assets / Total Assets"
            ))

        if 'ppe_net' in bs:
            ppe_ratio = bs['ppe_net'] / total_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="PPE Ratio",
                value=ppe_ratio,
                interpretation=f"Property, plant & equipment represents {self.format_percentage(ppe_ratio)} of total assets",
                risk_level=RiskLevel.LOW,
                methodology="Net PPE / Total Assets"
            ))

        intangible_assets = bs.get('intangible_assets')
        if intangible_assets is not None and intangible_assets > 0:
            intangible_ratio = intangible_assets / total_assets
            intangible_interpretation = "High intangible asset intensity - knowledge-based business" if intangible_ratio > 0.2 else "Moderate intangible assets" if intangible_ratio > 0.1 else "Low intangible asset base"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Intangible Asset Ratio",
                value=intangible_ratio,
                interpretation=intangible_interpretation,
                risk_level=RiskLevel.MODERATE if intangible_ratio > 0.3 else RiskLevel.LOW,
                methodology="Intangible Assets (excluding goodwill) / Total Assets",
                limitations=["Requires assessment of asset impairment risk"]
            ))

        goodwill = bs.get('goodwill')
        if goodwill is not None and goodwill > 0:
            goodwill_ratio = goodwill / total_assets
            goodwill_interpretation = "Significant goodwill from acquisitions - monitor for impairment" if goodwill_ratio > 0.15 else "Moderate goodwill level" if goodwill_ratio > 0.05 else "Low goodwill"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Goodwill Ratio",
                value=goodwill_ratio,
                interpretation=goodwill_interpretation,
                risk_level=RiskLevel.MODERATE if goodwill_ratio > 0.2 else RiskLevel.LOW,
                methodology="Goodwill / Total Assets",
                limitations=["Subject to impairment testing and write-downs"]
            ))

        results.extend(self._assess_asset_quality(statements, comparative_data))
        return results

    # -------------------------------------------------------------- liabilities

    def _analyze_liabilities(self, statements: FinancialStatements,
                             comparative_data: Optional[List[FinancialStatements]] = None,
                             industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive liability analysis"""
        results = []
        bs = statements.balance_sheet
        total_assets = bs.get('total_assets')
        total_equity = bs.get('total_equity')
        total_debt = self.total_debt(bs)

        # Debt-to-Equity
        if total_debt is None:
            results.append(self.unavailable(AnalysisType.SOLVENCY, "Debt-to-Equity Ratio",
                                            "neither short-term nor long-term debt reported",
                                            "Total Debt / Total Equity"))
        elif total_equity is None:
            results.append(self.unavailable(AnalysisType.SOLVENCY, "Debt-to-Equity Ratio",
                                            "total equity not reported", "Total Debt / Total Equity"))
        elif total_equity <= 0:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt-to-Equity Ratio",
                value=None,
                interpretation=(f"Not meaningful: total equity is {'negative' if total_equity < 0 else 'zero'} "
                                f"({total_equity:,.2f}); liabilities exceed assets, which is itself a solvency red flag"),
                risk_level=RiskLevel.VERY_HIGH,
                methodology="Total Debt / Total Equity"
            ))
        else:
            debt_to_equity = total_debt / total_equity
            risk_level = self.assess_risk_level(debt_to_equity, self.solvency_benchmarks['debt_to_equity'], False)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt-to-Equity Ratio",
                value=debt_to_equity,
                interpretation=self.generate_interpretation("debt-to-equity ratio", debt_to_equity, risk_level,
                                                            AnalysisType.SOLVENCY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(debt_to_equity, industry_data.get(
                    'debt_to_equity') if industry_data else None),
                methodology="(Short-term Debt + Long-term Debt) / Total Equity",
                limitations=["Does not consider off-balance-sheet obligations"]
            ))

        if total_assets is None or total_assets <= 0:
            return results

        # Debt-to-Assets
        if total_debt is not None:
            debt_to_assets = total_debt / total_assets
            risk_level = self.assess_risk_level(debt_to_assets, self.solvency_benchmarks['debt_to_assets'], False)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt-to-Assets Ratio",
                value=debt_to_assets,
                interpretation=self.generate_interpretation("debt-to-assets ratio", debt_to_assets, risk_level,
                                                            AnalysisType.SOLVENCY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(debt_to_assets, industry_data.get(
                    'debt_to_assets') if industry_data else None),
                methodology="(Short-term Debt + Long-term Debt) / Total Assets",
                limitations=["Asset values may not reflect market values"]
            ))

        if 'current_liabilities' in bs:
            current_liability_ratio = bs['current_liabilities'] / total_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Current Liability Ratio",
                value=current_liability_ratio,
                interpretation=f"Current liabilities represent {self.format_percentage(current_liability_ratio)} of total assets",
                risk_level=RiskLevel.HIGH if current_liability_ratio > 0.4 else RiskLevel.MODERATE if current_liability_ratio > 0.25 else RiskLevel.LOW,
                methodology="Current Liabilities / Total Assets"
            ))

        if 'long_term_debt' in bs:
            long_term_liability_ratio = bs['long_term_debt'] / total_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Long-term Debt Ratio",
                value=long_term_liability_ratio,
                interpretation=f"Long-term debt represents {self.format_percentage(long_term_liability_ratio)} of total assets",
                risk_level=RiskLevel.HIGH if long_term_liability_ratio > 0.4 else RiskLevel.MODERATE if long_term_liability_ratio > 0.25 else RiskLevel.LOW,
                methodology="Long-term Debt / Total Assets"
            ))

        # Debt maturity profile needs both debt lines
        if 'short_term_debt' in bs and 'long_term_debt' in bs and total_debt and total_debt > 0:
            short_term_debt_ratio = bs['short_term_debt'] / total_debt
            maturity_interpretation = "High short-term debt concentration - refinancing risk" if short_term_debt_ratio > 0.5 else "Balanced debt maturity profile" if short_term_debt_ratio > 0.2 else "Predominantly long-term debt structure"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Short-term Debt Concentration",
                value=short_term_debt_ratio,
                interpretation=maturity_interpretation,
                risk_level=RiskLevel.HIGH if short_term_debt_ratio > 0.6 else RiskLevel.MODERATE if short_term_debt_ratio > 0.4 else RiskLevel.LOW,
                methodology="Short-term Debt / Total Debt",
                limitations=["Does not consider debt covenants or refinancing ability"]
            ))

        # Interest Coverage (EBIT / interest expense)
        income_statement = statements.income_statement
        operating_income = income_statement.get('operating_income')
        interest_expense = income_statement.get('interest_expense')
        if interest_expense is not None and interest_expense > 0:
            if operating_income is None:
                results.append(self.unavailable(AnalysisType.SOLVENCY, "Interest Coverage Ratio",
                                                "operating income (EBIT) not reported",
                                                "Operating Income (EBIT) / Interest Expense"))
            else:
                interest_coverage = operating_income / interest_expense
                risk_level = self.assess_risk_level(interest_coverage,
                                                    self.solvency_benchmarks['interest_coverage'], True)
                if interest_coverage < 1:
                    risk_level = RiskLevel.VERY_HIGH
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name="Interest Coverage Ratio",
                    value=interest_coverage,
                    interpretation=self.generate_interpretation("interest coverage ratio", interest_coverage,
                                                                risk_level, AnalysisType.SOLVENCY),
                    risk_level=risk_level,
                    methodology="Operating Income (EBIT) / Interest Expense",
                    limitations=["Based on current operating performance"]
                ))

        return results

    # ------------------------------------------------------------------- equity

    def _analyze_equity(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive equity analysis"""
        results = []
        bs = statements.balance_sheet
        income_statement = statements.income_statement

        total_assets = bs.get('total_assets')
        total_equity = bs.get('total_equity')

        if total_equity is None:
            results.append(self.unavailable(AnalysisType.SOLVENCY, "Equity Analysis", "total equity not reported"))
            return results

        # Equity Ratio
        if total_assets is not None and total_assets > 0:
            equity_ratio = total_equity / total_assets
            if equity_ratio < 0:
                equity_interpretation = "Negative equity - liabilities exceed assets (shareholders' deficit)"
                equity_risk = RiskLevel.VERY_HIGH
            else:
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
                equity_multiplier = total_assets / total_equity
                multiplier_interpretation = "High financial leverage" if equity_multiplier > 3 else "Moderate financial leverage" if equity_multiplier > 2 else "Conservative financial leverage"
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name="Equity Multiplier",
                    value=equity_multiplier,
                    interpretation=multiplier_interpretation,
                    risk_level=RiskLevel.HIGH if equity_multiplier > 4 else RiskLevel.MODERATE if equity_multiplier > 2.5 else RiskLevel.LOW,
                    methodology="Total Assets / Total Equity (ending balances)",
                    limitations=["Component of DuPont analysis"]
                ))
            else:
                results.append(self.unavailable(AnalysisType.SOLVENCY, "Equity Multiplier",
                                                "total equity is zero or negative", "Total Assets / Total Equity"))

        # Retained Earnings Ratio
        retained_earnings = bs.get('retained_earnings')
        if total_equity > 0 and retained_earnings is not None:
            retained_earnings_ratio = retained_earnings / total_equity
            re_interpretation = "Strong retained earnings base" if retained_earnings_ratio > 0.5 else "Moderate retained earnings" if retained_earnings_ratio > 0.2 else "Low or negative retained earnings - accumulated losses or high distributions"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Retained Earnings Ratio",
                value=retained_earnings_ratio,
                interpretation=re_interpretation,
                risk_level=RiskLevel.LOW if retained_earnings_ratio > 0.3 else RiskLevel.MODERATE if retained_earnings_ratio > 0 else RiskLevel.HIGH,
                methodology="Retained Earnings / Total Equity"
            ))

        # Book Value per Share
        shares, shares_basis = self._shares(statements)
        if shares is not None:
            book_value_per_share = total_equity / shares
            results.append(AnalysisResult(
                analysis_type=AnalysisType.VALUATION,
                metric_name="Book Value per Share",
                value=book_value_per_share,
                interpretation=(f"Book value per share is {book_value_per_share:.2f}" if book_value_per_share >= 0
                                else f"Negative book value per share ({book_value_per_share:.2f}) - shareholders' deficit"),
                risk_level=RiskLevel.LOW if book_value_per_share > 0 else RiskLevel.HIGH,
                methodology=f"Total Equity / Shares ({shares_basis})"
            ))

            tangible_equity = total_equity - bs.get('intangible_assets', 0.0) - bs.get('goodwill', 0.0)
            tangible_bvps = tangible_equity / shares
            results.append(AnalysisResult(
                analysis_type=AnalysisType.VALUATION,
                metric_name="Tangible Book Value per Share",
                value=tangible_bvps,
                interpretation=(f"Tangible book value per share is {tangible_bvps:.2f}" if tangible_bvps >= 0
                                else f"Negative tangible book value per share ({tangible_bvps:.2f})"),
                risk_level=RiskLevel.LOW if tangible_bvps > 0 else RiskLevel.HIGH,
                methodology=f"(Total Equity - Intangibles - Goodwill) / Shares ({shares_basis})",
                limitations=["Excludes intangible asset value"]
            ))

        # Return on Equity
        net_income = income_statement.get('net_income')
        if net_income is not None:
            avg_equity, basis = self.average_balance(statements, comparative_data, 'total_equity')
            if avg_equity is None or avg_equity <= 0:
                results.append(self.unavailable(
                    AnalysisType.PROFITABILITY, "Return on Equity",
                    "average shareholders' equity is zero or negative, ROE is not meaningful",
                    "Net Income / Total Equity"))
            else:
                roe = net_income / avg_equity
                risk_level = self.assess_risk_level(roe, self.profitability_benchmarks['roe'], True)
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.PROFITABILITY,
                    metric_name="Return on Equity",
                    value=roe,
                    interpretation=self.generate_interpretation("return on equity", roe, risk_level,
                                                                AnalysisType.PROFITABILITY),
                    risk_level=risk_level,
                    benchmark_comparison=self.compare_to_industry(roe, industry_data.get('roe') if industry_data else None),
                    methodology=f"Net Income / Total Equity ({self.basis_note(basis)})"
                ))

        return results

    def _assess_asset_quality(self, statements: FinancialStatements,
                              comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Assess asset quality and potential impairment issues"""
        results = []
        bs = statements.balance_sheet

        ppe_gross = bs.get('ppe_gross')
        accumulated_depreciation = bs.get('accumulated_depreciation')
        if ppe_gross and ppe_gross > 0 and accumulated_depreciation is not None:
            asset_age_ratio = accumulated_depreciation / ppe_gross
            age_interpretation = "Assets approaching end of useful life - significant capex likely needed" if asset_age_ratio > 0.7 else "Moderately aged assets" if asset_age_ratio > 0.5 else "Relatively new assets"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Asset Age Ratio",
                value=asset_age_ratio,
                interpretation=age_interpretation,
                risk_level=RiskLevel.HIGH if asset_age_ratio > 0.8 else RiskLevel.MODERATE if asset_age_ratio > 0.6 else RiskLevel.LOW,
                methodology="Accumulated Depreciation / Gross PPE",
                limitations=["Based on historical cost and depreciation methods"]
            ))

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
        """Composite financial position score over the factors that can be evaluated."""
        results = []
        bs = statements.balance_sheet
        quality_factors = []
        evaluated = []
        not_evaluated = []
        quality_score = 100

        ca, cl = bs.get('current_assets'), bs.get('current_liabilities')
        if ca is not None and cl and cl > 0:
            evaluated.append("liquidity")
            current_ratio = ca / cl
            if current_ratio >= 1.5:
                quality_factors.append("Strong liquidity position")
            elif current_ratio < 1.0:
                quality_score -= 20
        else:
            not_evaluated.append("liquidity (current assets/liabilities not reported)")

        total_assets = bs.get('total_assets')
        total_debt = self.total_debt(bs)
        if total_assets and total_assets > 0 and total_debt is not None:
            evaluated.append("leverage")
            debt_ratio = total_debt / total_assets
            if debt_ratio > 0.6:
                quality_score -= 25
            elif debt_ratio < 0.3:
                quality_factors.append("Conservative debt levels")
        else:
            not_evaluated.append("leverage (debt or total assets not reported)")

        if total_assets and total_assets > 0 and ('intangible_assets' in bs or 'goodwill' in bs):
            evaluated.append("asset composition")
            if (bs.get('intangible_assets', 0.0) + bs.get('goodwill', 0.0)) / total_assets > 0.4:
                quality_score -= 15

        total_equity = bs.get('total_equity')
        if total_equity is not None and total_equity < 0:
            evaluated.append("equity")
            quality_score -= 25

        net_income = statements.income_statement.get('net_income')
        if net_income is not None:
            evaluated.append("profitability")
            if net_income < 0:
                quality_score -= 20
        else:
            not_evaluated.append("profitability (net income not reported)")

        if len(evaluated) < 2:
            results.append(self.unavailable(AnalysisType.QUALITY, "Financial Position Quality Score",
                                            f"only {len(evaluated)} of the scored factors could be evaluated "
                                            f"({', '.join(evaluated) or 'none'}); at least 2 are needed"))
            return results

        quality_score = max(0, quality_score)
        quality_interpretation = "Excellent financial position" if quality_score > 80 else "Good financial position" if quality_score > 60 else "Fair financial position" if quality_score > 40 else "Weak financial position"
        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Financial Position Quality Score",
            value=quality_score,
            interpretation=quality_interpretation + f" (factors evaluated: {', '.join(evaluated)})",
            risk_level=RiskLevel.LOW if quality_score > 70 else RiskLevel.MODERATE if quality_score > 50 else RiskLevel.HIGH,
            recommendations=quality_factors,
            limitations=[f"Not evaluated: {x}" for x in not_evaluated],
            methodology="100 minus penalties for weak liquidity, high leverage, intangible-heavy assets, negative equity and losses"
        ))
        return results

    def _perform_common_size_analysis(self, statements: FinancialStatements,
                                      comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Perform common-size balance sheet analysis"""
        results = []
        bs = statements.balance_sheet
        total_assets = bs.get('total_assets')
        if total_assets is None or total_assets <= 0:
            return results

        asset_items = {'Current Assets': 'current_assets', 'PPE Net': 'ppe_net',
                       'Intangible Assets': 'intangible_assets', 'Goodwill': 'goodwill'}
        for item_name, key in asset_items.items():
            item_value = bs.get(key)
            if item_value is not None and item_value > 0:
                pct = item_value / total_assets
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name=f"{item_name} as % of Total Assets",
                    value=pct,
                    interpretation=f"{item_name} represents {self.format_percentage(pct)} of total assets",
                    risk_level=RiskLevel.LOW,
                    methodology=f"{item_name} / Total Assets"
                ))

        le_items = {'Current Liabilities': 'current_liabilities', 'Long-term Debt': 'long_term_debt',
                    'Total Equity': 'total_equity'}
        for item_name, key in le_items.items():
            item_value = bs.get(key)
            if item_value is not None and item_value != 0:  # negative equity included
                pct = item_value / total_assets
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name=f"{item_name} as % of Total Assets",
                    value=pct,
                    interpretation=f"{item_name} represents {self.format_percentage(pct)} of total assets",
                    risk_level=RiskLevel.LOW,
                    methodology=f"{item_name} / Total Assets"
                ))

        return results

    def _analyze_balance_sheet_relationships(self, statements: FinancialStatements,
                                             comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Long-term financing vs long-term assets"""
        results = []
        bs = statements.balance_sheet
        if not all(k in bs for k in ('total_assets', 'current_assets', 'total_equity')):
            return results

        long_term_assets = bs['total_assets'] - bs['current_assets']
        long_term_financing = bs.get('long_term_debt', 0.0) + bs['total_equity']
        if long_term_assets > 0:
            financing_ratio = long_term_financing / long_term_assets
            financing_interpretation = "Appropriate long-term financing for long-term assets" if financing_ratio >= 1.0 else "Potential maturity mismatch - long-term assets financed with short-term funds"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Long-term Financing Ratio",
                value=financing_ratio,
                interpretation=financing_interpretation,
                risk_level=RiskLevel.LOW if financing_ratio >= 1.0 else RiskLevel.MODERATE if financing_ratio >= 0.8 else RiskLevel.HIGH,
                methodology="(Long-term Debt + Equity) / (Total Assets - Current Assets)",
                limitations=["Simplified maturity matching analysis"]
            ))

        return results

    def _identify_impairment_indicators(self, statements: FinancialStatements,
                                        comparative_data: Optional[List[FinancialStatements]] = None) -> List[str]:
        """Identify potential asset impairment indicators"""
        indicators = []
        bs = statements.balance_sheet
        income_statement = statements.income_statement

        net_income = income_statement.get('net_income')
        if net_income is not None and net_income < 0:
            indicators.append("Negative net income may indicate asset impairment")

        goodwill = bs.get('goodwill')
        total_assets = bs.get('total_assets')
        if goodwill and goodwill > 0 and total_assets and total_assets > 0 and goodwill / total_assets > 0.3:
            indicators.append("High goodwill concentration - monitor for impairment")

        if comparative_data:
            revenue = income_statement.get('revenue')
            prev_revenue = self.prior_value(comparative_data, 'income_statement', 'revenue')
            if revenue is not None and prev_revenue and prev_revenue > 0 and revenue < prev_revenue * 0.9:
                indicators.append("Significant revenue decline may indicate asset impairment")

        return indicators

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Key balance sheet metrics. ROA/ROE/asset turnover use average balances when
        `comparative_data` is supplied, else ending balances. Metrics whose inputs are
        missing are omitted (reasons go to `unavailable`)."""
        bs = statements.balance_sheet
        income_statement = statements.income_statement
        metrics = {}
        skipped = unavailable if unavailable is not None else {}

        current_liabilities = bs.get('current_liabilities')
        if current_liabilities and current_liabilities > 0:
            if 'current_assets' in bs:
                metrics['current_ratio'] = bs['current_assets'] / current_liabilities
            else:
                skipped['current_ratio'] = "current assets not reported"
            quick_assets, _ = self._quick_assets(bs)
            if quick_assets is not None:
                metrics['quick_ratio'] = quick_assets / current_liabilities
            cash_and_inv = self._cash_and_investments(bs)
            if cash_and_inv is not None:
                metrics['cash_ratio'] = cash_and_inv / current_liabilities
        else:
            for k in ('current_ratio', 'quick_ratio', 'cash_ratio'):
                skipped[k] = "current liabilities not reported or not positive"

        total_assets = bs.get('total_assets')
        total_equity = bs.get('total_equity')
        total_debt = self.total_debt(bs)

        if total_equity is not None and total_equity > 0:
            if total_debt is not None:
                metrics['debt_to_equity'] = total_debt / total_equity
            else:
                skipped['debt_to_equity'] = "debt not reported"
            if total_assets is not None:
                metrics['equity_multiplier'] = total_assets / total_equity
        else:
            reason = "total equity not reported" if total_equity is None else "total equity is zero or negative"
            skipped['debt_to_equity'] = reason
            skipped['equity_multiplier'] = reason

        if total_assets is not None and total_assets > 0:
            if total_debt is not None:
                metrics['debt_to_assets'] = total_debt / total_assets
            else:
                skipped['debt_to_assets'] = "debt not reported"
            if total_equity is not None:
                metrics['equity_ratio'] = total_equity / total_assets

        revenue = income_statement.get('revenue')
        net_income = income_statement.get('net_income')
        avg_assets, _ = self.average_balance(statements, comparative_data, 'total_assets')
        avg_equity, _ = self.average_balance(statements, comparative_data, 'total_equity')

        if avg_assets is not None and avg_assets > 0:
            if revenue is not None and revenue > 0:
                metrics['asset_turnover'] = revenue / avg_assets
            if net_income is not None:
                metrics['roa'] = net_income / avg_assets
            else:
                skipped['roa'] = "net income not reported"
        else:
            skipped['roa'] = "total assets not reported or not positive"

        if net_income is not None and avg_equity is not None and avg_equity > 0:
            metrics['roe'] = net_income / avg_equity
        else:
            skipped['roe'] = ("net income not reported" if net_income is None
                              else "shareholders' equity not reported, zero or negative")

        return metrics

    def create_liquidity_analysis(self, statements: FinancialStatements) -> LiquidityAnalysis:
        """Create liquidity analysis object (ratios are None when not computable)."""
        bs = statements.balance_sheet
        current_assets = bs.get('current_assets')
        current_liabilities = bs.get('current_liabilities')
        total_assets = bs.get('total_assets')

        def ratio(num):
            return num / current_liabilities if num is not None and current_liabilities else None

        current_ratio = ratio(current_assets)
        quick_ratio = ratio(self._quick_assets(bs)[0])
        cash_ratio = ratio(self._cash_and_investments(bs))
        working_capital = (current_assets - current_liabilities
                           if current_assets is not None and current_liabilities is not None else None)
        working_capital_ratio = (working_capital / total_assets
                                 if working_capital is not None and total_assets else None)

        quality_score = None
        risk_level = None
        if current_ratio is not None:
            quality_score = 100
            if current_ratio < 1.0:
                quality_score -= 30
            elif current_ratio < 1.2:
                quality_score -= 15
            if quick_ratio is not None and quick_ratio < 0.8:
                quality_score -= 20
            if cash_ratio is not None and cash_ratio < 0.1:
                quality_score -= 10
            quality_score = max(0, quality_score)
            risk_level = RiskLevel.LOW if quality_score > 80 else RiskLevel.MODERATE if quality_score > 60 else RiskLevel.HIGH

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
