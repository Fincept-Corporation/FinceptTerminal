
"""
Financial Statement Income Statement Module
========================================

Income statement analysis and profitability assessment

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


class RevenueRecognitionMethod(Enum):
    """Revenue recognition methods"""
    POINT_IN_TIME = "point_in_time"
    OVER_TIME = "over_time"
    PERCENTAGE_COMPLETION = "percentage_completion"
    COMPLETED_CONTRACT = "completed_contract"
    INSTALLMENT = "installment"


class ExpenseRecognitionMethod(Enum):
    """Expense recognition methods"""
    MATCHING_PRINCIPLE = "matching"
    SYSTEMATIC_ALLOCATION = "systematic_allocation"
    IMMEDIATE_RECOGNITION = "immediate"
    CAPITALIZED = "capitalized"


class IncomeQualityIndicator(Enum):
    """Income quality assessment indicators"""
    HIGH_QUALITY = "high_quality"
    MODERATE_QUALITY = "moderate_quality"
    LOW_QUALITY = "low_quality"
    RED_FLAG = "red_flag"


@dataclass
class EPSAnalysis:
    """Comprehensive EPS analysis results"""
    basic_eps: float
    diluted_eps: float
    basic_shares: float
    diluted_shares: float
    dilution_effect: float
    eps_quality: IncomeQualityIndicator
    antidilutive_securities: bool
    eps_growth_rate: float = None
    eps_volatility: float = None
    normalized_eps: float = None


@dataclass
class NonRecurringItemsAnalysis:
    """Analysis of non-recurring and unusual items"""
    total_non_recurring: float
    discontinued_operations: float
    unusual_items: float
    extraordinary_items: float
    restructuring_charges: float
    impairment_losses: float
    gains_losses_disposals: float
    impact_on_core_earnings: float
    frequency_analysis: str
    persistence_assessment: str


@dataclass
class RevenueQualityAssessment:
    """Revenue quality and recognition analysis"""
    revenue_growth_rate: float
    revenue_volatility: float
    seasonality_factor: float
    revenue_concentration: float
    days_sales_outstanding: float
    revenue_quality_score: float
    recognition_issues: List[str] = field(default_factory=list)
    quality_indicators: List[str] = field(default_factory=list)


class IncomeStatementAnalyzer(BaseAnalyzer):
    """
    Comprehensive income statement analyzer implementing CFA Institute standards.
    Covers revenue/expense recognition, EPS calculations, non-recurring items analysis.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_income_formulas()
        self._initialize_quality_thresholds()

    def _initialize_income_formulas(self):
        """Initialize income statement specific formulas"""
        self.formula_registry.update({
            'gross_profit_margin': lambda revenue, cogs: self.safe_divide(revenue - cogs, revenue),
            'operating_profit_margin': lambda operating_income, revenue: self.safe_divide(operating_income, revenue),
            'net_profit_margin': lambda net_income, revenue: self.safe_divide(net_income, revenue),
            'ebitda_margin': lambda ebitda, revenue: self.safe_divide(ebitda, revenue),
            'basic_eps': lambda net_income, shares: self.safe_divide(net_income, shares),
            'diluted_eps': lambda net_income_diluted, diluted_shares: self.safe_divide(net_income_diluted,
                                                                                       diluted_shares),
            'tax_rate': lambda tax_expense, pretax_income: self.safe_divide(tax_expense, pretax_income),
            'interest_coverage': lambda ebit, interest_expense: self.safe_divide(ebit, interest_expense)
        })

    def _initialize_quality_thresholds(self):
        """Initialize income quality assessment thresholds"""
        self.quality_thresholds.update({
            'revenue_growth_volatility': {'low': 0.1, 'moderate': 0.2, 'high': 0.4},
            'earnings_persistence': {'high': 0.8, 'moderate': 0.6, 'low': 0.4},
            'accruals_ratio': {'good': 0.05, 'moderate': 0.1, 'poor': 0.2},
            'non_recurring_frequency': {'rare': 0.1, 'occasional': 0.2, 'frequent': 0.4}
        })

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive income statement analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all income statement aspects
        """
        results = []

        # Validate data sufficiency
        required_fields = ['revenue', 'net_income', 'operating_income']
        is_sufficient, missing_fields = self.validate_data_sufficiency(statements, required_fields)

        if not is_sufficient:
            if self.logger:
                self.logger.warning(f"Insufficient data for complete analysis. Missing: {missing_fields}")

        # Core profitability analysis
        results.extend(self._analyze_profitability_ratios(statements, industry_data))

        # Revenue analysis
        results.extend(self._analyze_revenue_recognition(statements, comparative_data))

        # Expense analysis
        results.extend(self._analyze_expense_recognition(statements, comparative_data))

        # EPS analysis
        eps_results = self._analyze_earnings_per_share(statements, comparative_data)
        if eps_results:
            results.extend(eps_results)

        # Non-recurring items analysis
        results.extend(self._analyze_non_recurring_items(statements, comparative_data))

        # Income quality assessment
        results.extend(self._assess_income_quality(statements, comparative_data))

        # Common-size analysis
        results.extend(self._perform_common_size_analysis(statements, comparative_data))

        return results

    def _analyze_profitability_ratios(self, statements: FinancialStatements,
                                      industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze core profitability ratios"""
        results = []
        income = statements.income_statement

        # Gross Profit Margin
        revenue = income.get('revenue', 0)
        cogs = income.get('cost_of_sales', 0)

        if revenue > 0:
            gross_margin = self.safe_divide(revenue - cogs, revenue)
            benchmark = self.profitability_benchmarks.get('gross_margin', {})
            risk_level = self.assess_risk_level(gross_margin, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Gross Profit Margin",
                value=gross_margin,
                interpretation=self.generate_interpretation("gross profit margin", gross_margin, risk_level,
                                                            AnalysisType.PROFITABILITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(gross_margin, industry_data.get(
                    'gross_margin') if industry_data else None),
                methodology="(Revenue - Cost of Sales) / Revenue",
                limitations=["Does not reflect operating efficiency or overhead costs"]
            ))

        # Operating Profit Margin
        operating_income = income.get('operating_income', 0)
        if revenue > 0 and operating_income is not None:
            operating_margin = self.safe_divide(operating_income, revenue)
            benchmark = self.profitability_benchmarks.get('operating_margin', {})
            risk_level = self.assess_risk_level(operating_margin, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Operating Profit Margin",
                value=operating_margin,
                interpretation=self.generate_interpretation("operating profit margin", operating_margin, risk_level,
                                                            AnalysisType.PROFITABILITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(operating_margin, industry_data.get(
                    'operating_margin') if industry_data else None),
                methodology="Operating Income / Revenue",
                limitations=["Excludes non-operating income and expenses"]
            ))

        # Net Profit Margin
        net_income = income.get('net_income', 0)
        if revenue > 0:
            net_margin = self.safe_divide(net_income, revenue)
            benchmark = self.profitability_benchmarks.get('net_margin', {})
            risk_level = self.assess_risk_level(net_margin, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Net Profit Margin",
                value=net_margin,
                interpretation=self.generate_interpretation("net profit margin", net_margin, risk_level,
                                                            AnalysisType.PROFITABILITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(net_margin, industry_data.get(
                    'net_margin') if industry_data else None),
                methodology="Net Income / Revenue",
                limitations=["May include non-recurring items affecting comparability"]
            ))

        # EBITDA Margin (if calculable)
        ebitda = self._calculate_ebitda(statements)
        if ebitda is not None and revenue > 0:
            ebitda_margin = self.safe_divide(ebitda, revenue)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="EBITDA Margin",
                value=ebitda_margin,
                interpretation=f"EBITDA margin of {self.format_percentage(ebitda_margin)} shows operational profitability before financing and accounting decisions",
                risk_level=self.assess_risk_level(ebitda_margin,
                                                  self.profitability_benchmarks.get('operating_margin', {}),
                                                  higher_is_better=True),
                methodology="(Operating Income + Depreciation + Amortization) / Revenue",
                limitations=["Does not reflect capital expenditure requirements or working capital needs"]
            ))

        return results

    def _analyze_revenue_recognition(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze revenue recognition and quality"""
        results = []
        income = statements.income_statement
        revenue = income.get('revenue', 0)

        if revenue <= 0:
            return results

        # Revenue growth analysis
        if comparative_data and len(comparative_data) > 0:
            prev_revenue = comparative_data[-1].income_statement.get('revenue', 0)
            if prev_revenue > 0:
                revenue_growth = (revenue / prev_revenue) - 1

                # Assess revenue growth quality
                if revenue_growth > 0.2:
                    growth_quality = "Strong revenue growth - monitor sustainability"
                elif revenue_growth > 0.1:
                    growth_quality = "Healthy revenue growth"
                elif revenue_growth > 0:
                    growth_quality = "Modest revenue growth"
                elif revenue_growth > -0.05:
                    growth_quality = "Flat revenue - investigate causes"
                else:
                    growth_quality = "Declining revenue - significant concern"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Revenue Growth Rate",
                    value=revenue_growth,
                    interpretation=growth_quality,
                    risk_level=RiskLevel.LOW if revenue_growth > 0.05 else RiskLevel.HIGH if revenue_growth < -0.05 else RiskLevel.MODERATE,
                    methodology="(Current Revenue - Previous Revenue) / Previous Revenue",
                    limitations=["Single period comparison may not reflect underlying trends"]
                ))

        # Revenue recognition quality indicators
        balance_sheet = statements.balance_sheet
        accounts_receivable = balance_sheet.get('accounts_receivable', 0)

        if accounts_receivable > 0 and revenue > 0:
            # Days Sales Outstanding
            dso = (accounts_receivable / revenue) * 365

            dso_interpretation = "Normal collection period" if dso <= 45 else "Extended collection period - monitor credit quality" if dso <= 90 else "Very long collection period - potential collection issues"
            dso_risk = RiskLevel.LOW if dso <= 45 else RiskLevel.MODERATE if dso <= 90 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Days Sales Outstanding",
                value=dso,
                interpretation=dso_interpretation,
                risk_level=dso_risk,
                methodology="(Accounts Receivable / Revenue) × 365",
                limitations=["May vary by industry and seasonality"]
            ))

        # Check for potential revenue manipulation indicators
        revenue_quality_issues = self._identify_revenue_quality_issues(statements, comparative_data)
        if revenue_quality_issues:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Revenue Quality Assessment",
                value=len(revenue_quality_issues),
                interpretation=f"Identified {len(revenue_quality_issues)} potential revenue quality concerns",
                risk_level=RiskLevel.HIGH if len(revenue_quality_issues) > 2 else RiskLevel.MODERATE,
                limitations=revenue_quality_issues
            ))

        return results

    def _analyze_expense_recognition(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze expense recognition patterns and quality"""
        results = []
        income = statements.income_statement

        # Operating leverage analysis
        revenue = income.get('revenue', 0)
        operating_income = income.get('operating_income', 0)

        if comparative_data and len(comparative_data) > 0 and revenue > 0:
            prev_statements = comparative_data[-1]
            prev_revenue = prev_statements.income_statement.get('revenue', 0)
            prev_operating_income = prev_statements.income_statement.get('operating_income', 0)

            if prev_revenue > 0 and prev_operating_income != 0:
                revenue_change = (revenue / prev_revenue) - 1
                operating_change = (operating_income / prev_operating_income) - 1 if prev_operating_income != 0 else 0

                if revenue_change != 0:
                    operating_leverage = operating_change / revenue_change

                    leverage_interpretation = "High operating leverage - earnings sensitive to revenue changes" if abs(
                        operating_leverage) > 2 else "Moderate operating leverage" if abs(
                        operating_leverage) > 1 else "Low operating leverage"

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.PROFITABILITY,
                        metric_name="Operating Leverage",
                        value=operating_leverage,
                        interpretation=leverage_interpretation,
                        risk_level=RiskLevel.HIGH if abs(operating_leverage) > 3 else RiskLevel.MODERATE,
                        methodology="% Change in Operating Income / % Change in Revenue",
                        limitations=["Single period calculation may not reflect long-term leverage"]
                    ))

        # Expense ratios analysis
        if revenue > 0:
            # R&D Intensity
            rd_expenses = income.get('rd_expenses', 0)
            if rd_expenses > 0:
                rd_intensity = self.safe_divide(rd_expenses, revenue)
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name="R&D Intensity",
                    value=rd_intensity,
                    interpretation=f"R&D spending represents {self.format_percentage(rd_intensity)} of revenue, indicating {'high' if rd_intensity > 0.05 else 'moderate' if rd_intensity > 0.02 else 'low'} innovation investment",
                    risk_level=RiskLevel.LOW,
                    methodology="R&D Expenses / Revenue"
                ))

            # SG&A Efficiency
            selling_expenses = income.get('selling_expenses', 0)
            admin_expenses = income.get('administrative_expenses', 0)
            sga_total = selling_expenses + admin_expenses

            if sga_total > 0:
                sga_ratio = self.safe_divide(sga_total, revenue)
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name="SG&A Ratio",
                    value=sga_ratio,
                    interpretation=f"SG&A expenses represent {self.format_percentage(sga_ratio)} of revenue",
                    risk_level=RiskLevel.HIGH if sga_ratio > 0.3 else RiskLevel.MODERATE if sga_ratio > 0.2 else RiskLevel.LOW,
                    methodology="(Selling + General & Administrative Expenses) / Revenue"
                ))

        return results

    def _analyze_earnings_per_share(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Comprehensive EPS analysis including basic, diluted, and quality assessment"""
        results = []
        income = statements.income_statement

        # Extract EPS data
        basic_eps = income.get('basic_eps')
        diluted_eps = income.get('diluted_eps')
        basic_shares = income.get('shares_outstanding_basic')
        diluted_shares = income.get('shares_outstanding_diluted')
        net_income = income.get('net_income', 0)

        # Calculate EPS if not provided
        if not basic_eps and basic_shares and basic_shares > 0:
            basic_eps = self.safe_divide(net_income, basic_shares)

        if not diluted_eps and diluted_shares and diluted_shares > 0:
            diluted_eps = self.safe_divide(net_income, diluted_shares)

        if basic_eps is not None:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Basic EPS",
                value=basic_eps,
                interpretation=f"Basic earnings per share of ${basic_eps:.2f}",
                risk_level=RiskLevel.LOW if basic_eps > 0 else RiskLevel.HIGH,
                methodology="Net Income / Weighted Average Basic Shares Outstanding"
            ))

        if diluted_eps is not None:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Diluted EPS",
                value=diluted_eps,
                interpretation=f"Diluted earnings per share of ${diluted_eps:.2f}",
                risk_level=RiskLevel.LOW if diluted_eps > 0 else RiskLevel.HIGH,
                methodology="Net Income (adjusted for dilutive securities) / Weighted Average Diluted Shares Outstanding"
            ))

        # Dilution analysis
        if basic_eps and diluted_eps and basic_eps != 0:
            dilution_effect = (basic_eps - diluted_eps) / basic_eps

            if dilution_effect > 0.05:
                dilution_interpretation = "Significant dilution from potential securities conversions"
                dilution_risk = RiskLevel.MODERATE
            elif dilution_effect > 0.02:
                dilution_interpretation = "Moderate dilution from potential securities conversions"
                dilution_risk = RiskLevel.LOW
            else:
                dilution_interpretation = "Minimal dilution from potential securities conversions"
                dilution_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="EPS Dilution Effect",
                value=dilution_effect,
                interpretation=dilution_interpretation,
                risk_level=dilution_risk,
                methodology="(Basic EPS - Diluted EPS) / Basic EPS"
            ))

        # EPS growth analysis
        if comparative_data and basic_eps is not None:
            eps_values = []
            periods = []

            # Collect historical EPS
            for i, past_statements in enumerate(comparative_data):
                past_eps = past_statements.income_statement.get('basic_eps')
                if past_eps is not None:
                    eps_values.append(past_eps)
                    periods.append(f"Period-{len(comparative_data) - i}")

            eps_values.append(basic_eps)
            periods.append("Current")

            if len(eps_values) > 1:
                eps_trend = self.calculate_trend(eps_values, periods)

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.PROFITABILITY,
                    metric_name="EPS Growth Trend",
                    value=eps_trend.growth_rate or 0,
                    interpretation=eps_trend.trend_analysis,
                    risk_level=RiskLevel.LOW if eps_trend.growth_rate and eps_trend.growth_rate > 0 else RiskLevel.HIGH,
                    methodology="Compound Annual Growth Rate of Basic EPS"
                ))

        return results

    def _analyze_non_recurring_items(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze non-recurring and unusual items"""
        results = []
        income = statements.income_statement

        # Identify non-recurring items
        non_recurring_items = {
            'discontinued_operations': income.get('discontinued_operations', 0),
            'extraordinary_items': income.get('extraordinary_items', 0),
            'restructuring_charges': income.get('restructuring_charges', 0),
            'impairment_losses': income.get('impairment_losses', 0),
            'gains_losses_disposals': income.get('gains_losses_disposals', 0)
        }

        total_non_recurring = sum(abs(value) for value in non_recurring_items.values())
        net_income = income.get('net_income', 0)

        if total_non_recurring > 0:
            # Impact on earnings
            if net_income != 0:
                non_recurring_impact = total_non_recurring / abs(net_income)

                impact_interpretation = "Significant non-recurring items affecting earnings comparability" if non_recurring_impact > 0.1 else "Moderate non-recurring items impact" if non_recurring_impact > 0.05 else "Minor non-recurring items impact"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Non-Recurring Items Impact",
                    value=non_recurring_impact,
                    interpretation=impact_interpretation,
                    risk_level=RiskLevel.HIGH if non_recurring_impact > 0.2 else RiskLevel.MODERATE if non_recurring_impact > 0.1 else RiskLevel.LOW,
                    methodology="Total Non-Recurring Items / |Net Income|",
                    limitations=["Adjustment may be needed for normalized earnings analysis"]
                ))

            # Frequency analysis
            if comparative_data:
                historical_non_recurring = []
                for past_statements in comparative_data:
                    past_income = past_statements.income_statement
                    past_non_recurring = sum(abs(past_income.get(item, 0)) for item in non_recurring_items.keys())
                    historical_non_recurring.append(past_non_recurring)

                non_recurring_frequency = sum(1 for x in historical_non_recurring if x > 0) / len(
                    historical_non_recurring)

                frequency_interpretation = "Frequent non-recurring items - may indicate operational issues" if non_recurring_frequency > 0.5 else "Occasional non-recurring items" if non_recurring_frequency > 0.2 else "Rare non-recurring items"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Non-Recurring Items Frequency",
                    value=non_recurring_frequency,
                    interpretation=frequency_interpretation,
                    risk_level=RiskLevel.HIGH if non_recurring_frequency > 0.6 else RiskLevel.MODERATE if non_recurring_frequency > 0.3 else RiskLevel.LOW,
                    methodology="Number of periods with non-recurring items / Total periods"
                ))

        return results

    def _assess_income_quality(self, statements: FinancialStatements,
                               comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Comprehensive income quality assessment"""
        results = []

        # Earnings persistence analysis
        if comparative_data and len(comparative_data) >= 2:
            net_incomes = []
            for past_statements in comparative_data:
                past_income = past_statements.income_statement.get('net_income', 0)
                net_incomes.append(past_income)

            current_income = statements.income_statement.get('net_income', 0)
            net_incomes.append(current_income)

            # Calculate earnings volatility
            if len(net_incomes) > 1:
                mean_income = np.mean(net_incomes)
                std_income = np.std(net_incomes)
                earnings_volatility = std_income / abs(mean_income) if mean_income != 0 else 0

                volatility_interpretation = "High earnings volatility - low predictability" if earnings_volatility > 0.3 else "Moderate earnings volatility" if earnings_volatility > 0.15 else "Low earnings volatility - stable earnings"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Earnings Volatility",
                    value=earnings_volatility,
                    interpretation=volatility_interpretation,
                    risk_level=RiskLevel.HIGH if earnings_volatility > 0.4 else RiskLevel.MODERATE if earnings_volatility > 0.2 else RiskLevel.LOW,
                    methodology="Standard Deviation of Net Income / |Mean Net Income|"
                ))

        # Accruals quality (if cash flow data available)
        cash_flow = statements.cash_flow
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        net_income = statements.income_statement.get('net_income', 0)

        if operating_cash_flow is not None and net_income != 0:
            accruals_ratio = abs(net_income - operating_cash_flow) / abs(net_income)

            accruals_interpretation = "High accruals - potential earnings manipulation risk" if accruals_ratio > 0.2 else "Moderate accruals level" if accruals_ratio > 0.1 else "Low accruals - high earnings quality"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Accruals Ratio",
                value=accruals_ratio,
                interpretation=accruals_interpretation,
                risk_level=RiskLevel.HIGH if accruals_ratio > 0.3 else RiskLevel.MODERATE if accruals_ratio > 0.15 else RiskLevel.LOW,
                methodology="|Net Income - Operating Cash Flow| / |Net Income|",
                limitations=["High accruals may be justified by business model or growth phase"]
            ))

        return results

    def _perform_common_size_analysis(self, statements: FinancialStatements,
                                      comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Perform common-size income statement analysis"""
        results = []
        income = statements.income_statement
        revenue = income.get('revenue', 0)

        if revenue == 0:
            return results

        # Calculate common-size percentages for key items
        common_size_items = {
            'Cost of Sales': income.get('cost_of_sales', 0),
            'Operating Expenses': income.get('operating_expenses', 0),
            'Interest Expense': income.get('interest_expense', 0),
            'Tax Expense': income.get('tax_expense', 0)
        }

        for item_name, item_value in common_size_items.items():
            if item_value != 0:
                common_size_pct = self.safe_divide(item_value, revenue)

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name=f"{item_name} as % of Revenue",
                    value=common_size_pct,
                    interpretation=f"{item_name} represents {self.format_percentage(common_size_pct)} of total revenue",
                    risk_level=RiskLevel.LOW,
                    methodology=f"{item_name} / Revenue"
                ))

        return results

    def _calculate_ebitda(self, statements: FinancialStatements) -> Optional[float]:
        """Calculate EBITDA from available data"""
        income = statements.income_statement

        operating_income = income.get('operating_income')
        depreciation = income.get('depreciation', 0)
        amortization = income.get('amortization', 0)

        if operating_income is not None:
            return operating_income + depreciation + amortization

        return None

    def _identify_revenue_quality_issues(self, statements: FinancialStatements,
                                         comparative_data: Optional[List[FinancialStatements]] = None) -> List[str]:
        """Identify potential revenue quality and manipulation issues"""
        quality_issues = []

        income = statements.income_statement
        balance_sheet = statements.balance_sheet

        revenue = income.get('revenue', 0)
        accounts_receivable = balance_sheet.get('accounts_receivable', 0)

        # Red flag: Accounts receivable growing faster than revenue
        if comparative_data and len(comparative_data) > 0:
            prev_statements = comparative_data[-1]
            prev_revenue = prev_statements.income_statement.get('revenue', 0)
            prev_receivables = prev_statements.balance_sheet.get('accounts_receivable', 0)

            if prev_revenue > 0 and prev_receivables > 0:
                revenue_growth = (revenue / prev_revenue) - 1 if prev_revenue > 0 else 0
                receivables_growth = (accounts_receivable / prev_receivables) - 1 if prev_receivables > 0 else 0

                if receivables_growth > revenue_growth + 0.1:  # 10% threshold
                    quality_issues.append("Accounts receivable growing significantly faster than revenue")

        # Red flag: Very high Days Sales Outstanding
        if revenue > 0 and accounts_receivable > 0:
            dso = (accounts_receivable / revenue) * 365
            if dso > 120:  # Industry-dependent threshold
                quality_issues.append(f"Very high Days Sales Outstanding ({dso:.0f} days)")

        # Red flag: Revenue recognition timing issues (quarter-end spikes)
        # This would require quarterly data to detect properly

        # Red flag: Related party transactions (would need notes data)
        notes = statements.notes
        if any('related_party' in key.lower() for key in notes.keys()):
            quality_issues.append("Related party revenue transactions require scrutiny")

        return quality_issues

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key income statement metrics"""
        income = statements.income_statement
        revenue = income.get('revenue', 0)

        metrics = {}

        if revenue > 0:
            metrics['gross_profit_margin'] = self.safe_divide(
                revenue - income.get('cost_of_sales', 0), revenue)
            metrics['operating_profit_margin'] = self.safe_divide(
                income.get('operating_income', 0), revenue)
            metrics['net_profit_margin'] = self.safe_divide(
                income.get('net_income', 0), revenue)

            ebitda = self._calculate_ebitda(statements)
            if ebitda is not None:
                metrics['ebitda_margin'] = self.safe_divide(ebitda, revenue)

        metrics['basic_eps'] = income.get('basic_eps', 0)
        metrics['diluted_eps'] = income.get('diluted_eps', 0)

        # Tax rate
        pretax_income = income.get('pretax_income', 0)
        tax_expense = income.get('tax_expense', 0)
        if pretax_income != 0:
            metrics['effective_tax_rate'] = self.safe_divide(tax_expense, pretax_income)

        return metrics

    def create_eps_analysis(self, statements: FinancialStatements,
                            comparative_data: Optional[List[FinancialStatements]] = None) -> EPSAnalysis:
        """Create comprehensive EPS analysis object"""
        income = statements.income_statement

        basic_eps = income.get('basic_eps', 0)
        diluted_eps = income.get('diluted_eps', 0)
        basic_shares = income.get('shares_outstanding_basic', 0)
        diluted_shares = income.get('shares_outstanding_diluted', 0)

        # Calculate dilution effect
        dilution_effect = 0
        if basic_eps != 0 and diluted_eps != 0:
            dilution_effect = (basic_eps - diluted_eps) / basic_eps

        # Assess EPS quality
        eps_quality = IncomeQualityIndicator.HIGH_QUALITY
        if dilution_effect > 0.1:
            eps_quality = IncomeQualityIndicator.MODERATE_QUALITY

        # Check for antidilutive securities
        antidilutive_securities = diluted_shares < basic_shares if basic_shares > 0 else False

        # Calculate EPS growth and volatility if historical data available
        eps_growth_rate = None
        eps_volatility = None

        if comparative_data and len(comparative_data) > 0:
            eps_values = []
            for past_statements in comparative_data:
                past_eps = past_statements.income_statement.get('basic_eps')
                if past_eps is not None:
                    eps_values.append(past_eps)

            if eps_values and basic_eps is not None:
                eps_values.append(basic_eps)

                if len(eps_values) > 1:
                    # Growth rate calculation
                    if eps_values[0] != 0:
                        if len(eps_values) == 2:
                            eps_growth_rate = (eps_values[-1] / eps_values[0]) - 1
                        else:
                            n_periods = len(eps_values) - 1
                            eps_growth_rate = (eps_values[-1] / eps_values[0]) ** (1 / n_periods) - 1

                    # Volatility calculation
                    mean_eps = np.mean(eps_values)
                    std_eps = np.std(eps_values)
                    eps_volatility = std_eps / abs(mean_eps) if mean_eps != 0 else 0

        return EPSAnalysis(
            basic_eps=basic_eps,
            diluted_eps=diluted_eps,
            basic_shares=basic_shares,
            diluted_shares=diluted_shares,
            dilution_effect=dilution_effect,
            eps_quality=eps_quality,
            antidilutive_securities=antidilutive_securities,
            eps_growth_rate=eps_growth_rate,
            eps_volatility=eps_volatility
        )

    def analyze_non_recurring_items(self, statements: FinancialStatements,
                                    comparative_data: Optional[
                                        List[FinancialStatements]] = None) -> NonRecurringItemsAnalysis:
        """Create detailed non-recurring items analysis"""
        income = statements.income_statement

        # Extract non-recurring items
        discontinued_operations = income.get('discontinued_operations', 0)
        unusual_items = income.get('unusual_items', 0)
        extraordinary_items = income.get('extraordinary_items', 0)
        restructuring_charges = income.get('restructuring_charges', 0)
        impairment_losses = income.get('impairment_losses', 0)
        gains_losses_disposals = income.get('gains_losses_disposals', 0)

        total_non_recurring = sum(abs(x) for x in [
            discontinued_operations, unusual_items, extraordinary_items,
            restructuring_charges, impairment_losses, gains_losses_disposals
        ])

        # Calculate impact on core earnings
        net_income = income.get('net_income', 0)
        impact_on_core_earnings = total_non_recurring / abs(net_income) if net_income != 0 else 0

        # Frequency analysis
        frequency_analysis = "Single period analysis"
        persistence_assessment = "Cannot assess without historical data"

        if comparative_data:
            periods_with_non_recurring = 0
            total_periods = len(comparative_data) + 1

            for past_statements in comparative_data:
                past_income = past_statements.income_statement
                past_non_recurring = sum(abs(past_income.get(item, 0)) for item in [
                    'discontinued_operations', 'unusual_items', 'extraordinary_items',
                    'restructuring_charges', 'impairment_losses', 'gains_losses_disposals'
                ])
                if past_non_recurring > 0:
                    periods_with_non_recurring += 1

            if total_non_recurring > 0:
                periods_with_non_recurring += 1

            frequency_rate = periods_with_non_recurring / total_periods

            if frequency_rate > 0.6:
                frequency_analysis = "Frequent non-recurring items - may indicate operational issues"
                persistence_assessment = "High persistence - items may be recurring in nature"
            elif frequency_rate > 0.3:
                frequency_analysis = "Occasional non-recurring items"
                persistence_assessment = "Moderate persistence"
            else:
                frequency_analysis = "Rare non-recurring items"
                persistence_assessment = "Low persistence - truly non-recurring"

        return NonRecurringItemsAnalysis(
            total_non_recurring=total_non_recurring,
            discontinued_operations=discontinued_operations,
            unusual_items=unusual_items,
            extraordinary_items=extraordinary_items,
            restructuring_charges=restructuring_charges,
            impairment_losses=impairment_losses,
            gains_losses_disposals=gains_losses_disposals,
            impact_on_core_earnings=impact_on_core_earnings,
            frequency_analysis=frequency_analysis,
            persistence_assessment=persistence_assessment
        )

    def assess_revenue_quality(self, statements: FinancialStatements,
                               comparative_data: Optional[
                                   List[FinancialStatements]] = None) -> RevenueQualityAssessment:
        """Comprehensive revenue quality assessment"""
        income = statements.income_statement
        balance_sheet = statements.balance_sheet

        revenue = income.get('revenue', 0)
        accounts_receivable = balance_sheet.get('accounts_receivable', 0)

        # Initialize metrics
        revenue_growth_rate = 0
        revenue_volatility = 0
        seasonality_factor = 0
        revenue_concentration = 0  # Would need segment data
        days_sales_outstanding = 0

        # Calculate DSO
        if revenue > 0 and accounts_receivable >= 0:
            days_sales_outstanding = (accounts_receivable / revenue) * 365

        # Calculate growth and volatility if historical data available
        if comparative_data and len(comparative_data) > 0:
            revenue_values = []
            for past_statements in comparative_data:
                past_revenue = past_statements.income_statement.get('revenue', 0)
                revenue_values.append(past_revenue)

            revenue_values.append(revenue)

            if len(revenue_values) > 1:
                # Growth rate
                if revenue_values[0] > 0:
                    if len(revenue_values) == 2:
                        revenue_growth_rate = (revenue_values[-1] / revenue_values[0]) - 1
                    else:
                        n_periods = len(revenue_values) - 1
                        revenue_growth_rate = (revenue_values[-1] / revenue_values[0]) ** (1 / n_periods) - 1

                # Volatility
                mean_revenue = np.mean(revenue_values)
                std_revenue = np.std(revenue_values)
                revenue_volatility = std_revenue / mean_revenue if mean_revenue > 0 else 0

        # Quality indicators
        quality_indicators = []
        recognition_issues = []

        if days_sales_outstanding <= 45:
            quality_indicators.append("Healthy collection period")
        elif days_sales_outstanding > 90:
            recognition_issues.append("Extended collection period may indicate quality issues")

        if revenue_growth_rate > 0:
            quality_indicators.append("Positive revenue growth")
        elif revenue_growth_rate < -0.1:
            recognition_issues.append("Significant revenue decline")

        if revenue_volatility < 0.1:
            quality_indicators.append("Stable revenue pattern")
        elif revenue_volatility > 0.3:
            recognition_issues.append("High revenue volatility")

        # Calculate overall quality score
        quality_score = 100
        quality_score -= len(recognition_issues) * 20
        quality_score -= max(0, (days_sales_outstanding - 45) / 10 * 5)  # Penalize high DSO
        quality_score -= max(0, revenue_volatility * 100)  # Penalize volatility
        quality_score = max(0, min(100, quality_score))

        return RevenueQualityAssessment(
            revenue_growth_rate=revenue_growth_rate,
            revenue_volatility=revenue_volatility,
            seasonality_factor=seasonality_factor,
            revenue_concentration=revenue_concentration,
            days_sales_outstanding=days_sales_outstanding,
            revenue_quality_score=quality_score,
            recognition_issues=recognition_issues,
            quality_indicators=quality_indicators
        )