
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

CONVENTIONS:
  - A metric whose inputs are not reported is returned as "not available" (value
    None) instead of being computed from 0.
  - Ratios that mix a flow with a balance (DSO, accruals) use the average of the
    opening and closing balance when a prior period is supplied, else the ending
    balance; the methodology string says which.
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


NON_RECURRING_KEYS = ('discontinued_operations', 'extraordinary_items', 'restructuring_charges',
                      'impairment_losses', 'gains_losses_disposals', 'unusual_items')


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
            # Signed accruals / average total assets
            'accruals_ratio': {'good': 0.05, 'moderate': 0.10},
            'non_recurring_frequency': {'rare': 0.1, 'occasional': 0.2, 'frequent': 0.4}
        })

    # ------------------------------------------------------------------ helpers

    @staticmethod
    def _eps(income: Dict[str, float], kind: str) -> Optional[float]:
        """Reported EPS, else net income / weighted shares; None when neither is possible."""
        eps = income.get(f'{kind}_eps')
        if eps is not None:
            return eps
        shares = income.get(f'shares_outstanding_{kind}')
        ni = income.get('net_income')
        if ni is not None and shares and shares > 0:
            return ni / shares
        return None

    # ------------------------------------------------------------------ analyze

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive income statement analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements (oldest first)
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all income statement aspects
        """
        results = []

        required_fields = ['revenue', 'net_income', 'operating_income']
        is_sufficient, missing_fields = self.validate_data_sufficiency(statements, required_fields)
        if not is_sufficient and self.logger:
            self.logger.warning(f"Insufficient data for complete analysis. Missing: {missing_fields}")

        results.extend(self._analyze_profitability_ratios(statements, industry_data))
        results.extend(self._analyze_revenue_recognition(statements, comparative_data))
        results.extend(self._analyze_expense_recognition(statements, comparative_data))
        results.extend(self._analyze_earnings_per_share(statements, comparative_data))
        results.extend(self._analyze_non_recurring_items(statements, comparative_data))
        results.extend(self._assess_income_quality(statements, comparative_data))
        results.extend(self._perform_common_size_analysis(statements, comparative_data))

        return results

    def _analyze_profitability_ratios(self, statements: FinancialStatements,
                                      industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze core profitability ratios"""
        results = []
        income = statements.income_statement
        revenue = income.get('revenue')

        if revenue is None or revenue <= 0:
            results.append(self.unavailable(
                AnalysisType.PROFITABILITY, "Profit Margins",
                "revenue not reported" if revenue is None else "revenue is zero or negative",
                "Margin = line item / Revenue"))
            return results

        # Gross Profit Margin
        gross_profit = self.gross_profit(income)
        if gross_profit is None:
            results.append(self.unavailable(AnalysisType.PROFITABILITY, "Gross Profit Margin",
                                            "neither cost of sales nor gross profit reported",
                                            "(Revenue - Cost of Sales) / Revenue"))
        else:
            gross_margin = gross_profit / revenue
            risk_level = self.assess_risk_level(gross_margin, self.profitability_benchmarks['gross_margin'], True)
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
        if 'operating_income' in income:
            operating_margin = income['operating_income'] / revenue
            risk_level = self.assess_risk_level(operating_margin,
                                                self.profitability_benchmarks['operating_margin'], True)
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
        else:
            results.append(self.unavailable(AnalysisType.PROFITABILITY, "Operating Profit Margin",
                                            "operating income not reported", "Operating Income / Revenue"))

        # Net Profit Margin
        if 'net_income' in income:
            net_margin = income['net_income'] / revenue
            risk_level = self.assess_risk_level(net_margin, self.profitability_benchmarks['net_margin'], True)
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
        else:
            results.append(self.unavailable(AnalysisType.PROFITABILITY, "Net Profit Margin",
                                            "net income not reported", "Net Income / Revenue"))

        # EBITDA Margin
        ebitda, reason = self._calculate_ebitda_with_reason(statements)
        if ebitda is None:
            results.append(self.unavailable(AnalysisType.PROFITABILITY, "EBITDA Margin", reason,
                                            "(Operating Income + Depreciation + Amortization) / Revenue"))
        else:
            ebitda_margin = ebitda / revenue
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="EBITDA Margin",
                value=ebitda_margin,
                interpretation=f"EBITDA margin of {self.format_percentage(ebitda_margin)} shows operational profitability before financing and accounting decisions",
                risk_level=self.assess_risk_level(ebitda_margin,
                                                  self.profitability_benchmarks['operating_margin'], True),
                methodology="(Operating Income + Depreciation + Amortization) / Revenue",
                limitations=["Does not reflect capital expenditure requirements or working capital needs",
                             "If 'depreciation' already includes amortization, do not also supply 'amortization'"]
            ))

        return results

    def _analyze_revenue_recognition(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze revenue recognition and quality"""
        results = []
        income = statements.income_statement
        revenue = income.get('revenue')

        if revenue is None or revenue <= 0:
            return results

        # Revenue growth analysis
        if comparative_data:
            prev_revenue = self.prior_value(comparative_data, 'income_statement', 'revenue')
            revenue_growth = self.growth(revenue, prev_revenue)
            if revenue_growth is None:
                results.append(self.unavailable(
                    AnalysisType.QUALITY, "Revenue Growth Rate",
                    "prior-period revenue not reported or not positive",
                    "(Current Revenue - Previous Revenue) / Previous Revenue"))
            else:
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

        # Days Sales Outstanding
        avg_ar, basis = self.average_balance(statements, comparative_data, 'accounts_receivable')
        if avg_ar is not None and avg_ar >= 0:
            dso = avg_ar / revenue * 365
            dso_interpretation = "Normal collection period" if dso <= 45 else "Extended collection period - monitor credit quality" if dso <= 90 else "Very long collection period - potential collection issues"
            dso_risk = RiskLevel.LOW if dso <= 45 else RiskLevel.MODERATE if dso <= 90 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Days Sales Outstanding",
                value=dso,
                interpretation=dso_interpretation,
                risk_level=dso_risk,
                methodology=f"Accounts Receivable ({self.basis_note(basis)}) / Revenue x 365",
                limitations=["May vary by industry and seasonality"]
            ))

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
        revenue = income.get('revenue')
        operating_income = income.get('operating_income')

        # Degree of operating leverage = %change EBIT / %change revenue
        if comparative_data and revenue is not None and operating_income is not None:
            prev_revenue = self.prior_value(comparative_data, 'income_statement', 'revenue')
            prev_oi = self.prior_value(comparative_data, 'income_statement', 'operating_income')
            revenue_change = self.growth(revenue, prev_revenue)
            # Direction-correct change even when prior operating income was a loss
            operating_change = self.change_over_abs_base(operating_income, prev_oi)

            if revenue_change is None or operating_change is None:
                results.append(self.unavailable(
                    AnalysisType.PROFITABILITY, "Operating Leverage",
                    "prior-period revenue (positive) and operating income (non-zero) are required",
                    "% Change in Operating Income / % Change in Revenue"))
            elif abs(revenue_change) < 0.005:
                results.append(self.unavailable(
                    AnalysisType.PROFITABILITY, "Operating Leverage",
                    "revenue changed by less than 0.5%, the ratio is not meaningful",
                    "% Change in Operating Income / % Change in Revenue"))
            else:
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
                    methodology="% Change in Operating Income (over |prior|) / % Change in Revenue",
                    limitations=["Single period calculation may not reflect long-term leverage"]
                ))

        if revenue is not None and revenue > 0:
            # R&D Intensity
            rd_expenses = income.get('rd_expenses')
            if rd_expenses is not None and rd_expenses > 0:
                rd_intensity = rd_expenses / revenue
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name="R&D Intensity",
                    value=rd_intensity,
                    interpretation=f"R&D spending represents {self.format_percentage(rd_intensity)} of revenue, indicating {'high' if rd_intensity > 0.05 else 'moderate' if rd_intensity > 0.02 else 'low'} innovation investment",
                    risk_level=RiskLevel.LOW,
                    methodology="R&D Expenses / Revenue"
                ))

            # SG&A Efficiency
            sga_total = self.sga(income)
            if sga_total is not None and sga_total > 0:
                sga_ratio = sga_total / revenue
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

        basic_eps = self._eps(income, 'basic')
        diluted_eps = self._eps(income, 'diluted')
        basic_src = "reported" if 'basic_eps' in income else "Net Income / Weighted Average Basic Shares"
        diluted_src = "reported" if 'diluted_eps' in income else "Net Income / Weighted Average Diluted Shares"

        if basic_eps is not None:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Basic EPS",
                value=basic_eps,
                interpretation=f"Basic earnings per share of {basic_eps:.2f}",
                risk_level=RiskLevel.LOW if basic_eps > 0 else RiskLevel.HIGH,
                methodology=f"Net Income / Weighted Average Basic Shares Outstanding ({basic_src})"
            ))

        if diluted_eps is not None:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Diluted EPS",
                value=diluted_eps,
                interpretation=f"Diluted earnings per share of {diluted_eps:.2f}",
                risk_level=RiskLevel.LOW if diluted_eps > 0 else RiskLevel.HIGH,
                methodology=f"Net Income (adjusted for dilutive securities) / Weighted Average Diluted Shares ({diluted_src})"
            ))

        # Dilution analysis
        if basic_eps is not None and diluted_eps is not None and basic_eps != 0:
            dilution_effect = (basic_eps - diluted_eps) / abs(basic_eps)

            if basic_eps < 0:
                dilution_interpretation = "Net loss: potential shares are anti-dilutive, diluted EPS normally equals basic EPS"
                dilution_risk = RiskLevel.LOW
            elif dilution_effect > 0.05:
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
                methodology="(Basic EPS - Diluted EPS) / |Basic EPS|"
            ))

        # EPS growth analysis
        if comparative_data and basic_eps is not None:
            eps_values = []
            periods = []
            for i, past_statements in enumerate(comparative_data):
                past_eps = self._eps(past_statements.income_statement, 'basic')
                if past_eps is not None:
                    eps_values.append(past_eps)
                    periods.append(f"Period-{len(comparative_data) - i}")
            eps_values.append(basic_eps)
            periods.append("Current")

            if len(eps_values) > 1:
                eps_trend = self.calculate_trend(eps_values, periods)
                if eps_trend.growth_rate is None:
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.PROFITABILITY,
                        metric_name="EPS Growth Trend",
                        value=None,
                        interpretation=(f"Not available: growth rate is not defined when EPS is zero or negative "
                                        f"at either end of the window (EPS {eps_values[0]:.2f} -> {eps_values[-1]:.2f}); "
                                        f"trend: {eps_trend.trend_analysis}"),
                        risk_level=None,
                        methodology="Compound Annual Growth Rate of Basic EPS"
                    ))
                else:
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.PROFITABILITY,
                        metric_name="EPS Growth Trend",
                        value=eps_trend.growth_rate,
                        interpretation=eps_trend.trend_analysis,
                        risk_level=RiskLevel.LOW if eps_trend.growth_rate > 0 else RiskLevel.HIGH,
                        methodology=f"Compound Annual Growth Rate of Basic EPS over {len(eps_values) - 1} periods"
                    ))

        return results

    def _analyze_non_recurring_items(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze non-recurring and unusual items"""
        results = []
        income = statements.income_statement

        reported = {k: income[k] for k in NON_RECURRING_KEYS if k in income}
        if not reported:
            return results

        total_non_recurring = sum(abs(v) for v in reported.values())
        net_income = income.get('net_income')

        if total_non_recurring > 0 and net_income:
            non_recurring_impact = total_non_recurring / abs(net_income)
            impact_interpretation = "Significant non-recurring items affecting earnings comparability" if non_recurring_impact > 0.1 else "Moderate non-recurring items impact" if non_recurring_impact > 0.05 else "Minor non-recurring items impact"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Non-Recurring Items Impact",
                value=non_recurring_impact,
                interpretation=impact_interpretation,
                risk_level=RiskLevel.HIGH if non_recurring_impact > 0.2 else RiskLevel.MODERATE if non_recurring_impact > 0.1 else RiskLevel.LOW,
                methodology="Sum of |non-recurring items| / |Net Income| (items: " + ", ".join(reported) + ")",
                limitations=["Adjustment may be needed for normalized earnings analysis"]
            ))

        if comparative_data and total_non_recurring > 0:
            history = []
            for past_statements in comparative_data:
                past_income = past_statements.income_statement
                past_reported = [abs(past_income[k]) for k in NON_RECURRING_KEYS if k in past_income]
                if past_reported:
                    history.append(sum(past_reported))
            if history:
                non_recurring_frequency = sum(1 for x in history if x > 0) / len(history)
                frequency_interpretation = "Frequent non-recurring items - may indicate operational issues" if non_recurring_frequency > 0.5 else "Occasional non-recurring items" if non_recurring_frequency > 0.2 else "Rare non-recurring items"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Non-Recurring Items Frequency",
                    value=non_recurring_frequency,
                    interpretation=frequency_interpretation,
                    risk_level=RiskLevel.HIGH if non_recurring_frequency > 0.6 else RiskLevel.MODERATE if non_recurring_frequency > 0.3 else RiskLevel.LOW,
                    methodology="Prior periods with non-recurring items / prior periods that report them"
                ))

        return results

    def _assess_income_quality(self, statements: FinancialStatements,
                               comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Comprehensive income quality assessment"""
        results = []
        net_income = statements.income_statement.get('net_income')

        # Earnings volatility (needs current + at least two prior periods)
        if comparative_data and len(comparative_data) >= 2 and net_income is not None:
            net_incomes = [p.income_statement['net_income'] for p in comparative_data
                           if 'net_income' in p.income_statement]
            net_incomes.append(net_income)
            if len(net_incomes) >= 3:
                mean_income = float(np.mean(net_incomes))
                if mean_income == 0:
                    results.append(self.unavailable(AnalysisType.QUALITY, "Earnings Volatility",
                                                    "mean net income over the window is zero",
                                                    "Standard Deviation of Net Income / |Mean Net Income|"))
                else:
                    earnings_volatility = float(np.std(net_incomes)) / abs(mean_income)
                    volatility_interpretation = "High earnings volatility - low predictability" if earnings_volatility > 0.3 else "Moderate earnings volatility" if earnings_volatility > 0.15 else "Low earnings volatility - stable earnings"

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Earnings Volatility",
                        value=earnings_volatility,
                        interpretation=volatility_interpretation,
                        risk_level=RiskLevel.HIGH if earnings_volatility > 0.4 else RiskLevel.MODERATE if earnings_volatility > 0.2 else RiskLevel.LOW,
                        methodology=f"Standard Deviation of Net Income / |Mean Net Income| over {len(net_incomes)} periods"
                    ))

        # Accruals ratio, cash-flow approach (Sloan 1996): (NI - CFO) / average total assets.
        # Signed: negative accruals (cash flow above earnings) are a sign of quality, not a concern.
        operating_cash_flow = statements.cash_flow.get('operating_cash_flow')
        avg_assets, basis = self.average_balance(statements, comparative_data, 'total_assets')
        methodology = f"(Net Income - Operating Cash Flow) / Total Assets ({self.basis_note(basis)})"
        if net_income is None or operating_cash_flow is None or avg_assets is None:
            missing = [n for n, v in (('net income', net_income), ('operating cash flow', operating_cash_flow),
                                      ('total assets', avg_assets)) if v is None]
            results.append(self.unavailable(AnalysisType.QUALITY, "Accruals Ratio",
                                            f"{', '.join(missing)} not reported", methodology))
        elif avg_assets <= 0:
            results.append(self.unavailable(AnalysisType.QUALITY, "Accruals Ratio",
                                            "total assets are not positive", methodology))
        else:
            accruals_ratio = (net_income - operating_cash_flow) / avg_assets
            t = self.quality_thresholds['accruals_ratio']
            if accruals_ratio <= 0:
                accruals_interpretation = "Negative accruals - operating cash flow exceeds earnings (high earnings quality)"
                accruals_risk = RiskLevel.LOW
            elif accruals_ratio <= t['good']:
                accruals_interpretation = "Low accruals - earnings largely backed by cash flow"
                accruals_risk = RiskLevel.LOW
            elif accruals_ratio <= t['moderate']:
                accruals_interpretation = "Moderate accruals - earnings run ahead of cash flow"
                accruals_risk = RiskLevel.MODERATE
            else:
                accruals_interpretation = "High accruals - earnings well above cash flow, potential earnings manipulation risk"
                accruals_risk = RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Accruals Ratio",
                value=accruals_ratio,
                interpretation=accruals_interpretation,
                risk_level=accruals_risk,
                methodology=methodology,
                limitations=["High accruals may be justified by business model or growth phase"]
            ))

        return results

    def _perform_common_size_analysis(self, statements: FinancialStatements,
                                      comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Perform common-size income statement analysis"""
        results = []
        income = statements.income_statement
        revenue = income.get('revenue')

        if revenue is None or revenue <= 0:
            return results

        common_size_items = {
            'Cost of Sales': 'cost_of_sales',
            'Operating Expenses': 'operating_expenses',
            'Interest Expense': 'interest_expense',
            'Tax Expense': 'tax_expense',
        }

        for item_name, key in common_size_items.items():
            item_value = income.get(key)
            if item_value is not None and item_value != 0:
                common_size_pct = item_value / revenue
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name=f"{item_name} as % of Revenue",
                    value=common_size_pct,
                    interpretation=f"{item_name} represents {self.format_percentage(common_size_pct)} of total revenue",
                    risk_level=RiskLevel.LOW,
                    methodology=f"{item_name} / Revenue"
                ))

        return results

    def _calculate_ebitda_with_reason(self, statements: FinancialStatements) -> Tuple[Optional[float], str]:
        income = statements.income_statement
        if 'operating_income' not in income:
            return None, "operating income not reported"
        depreciation = income.get('depreciation', statements.cash_flow.get('depreciation_cf'))
        if depreciation is None:
            return None, "depreciation not reported (EBITDA would equal EBIT)"
        amortization = income.get('amortization', 0.0)
        return income['operating_income'] + abs(depreciation) + amortization, ""

    def _calculate_ebitda(self, statements: FinancialStatements) -> Optional[float]:
        """EBITDA = operating income + D&A; None when either input is missing."""
        return self._calculate_ebitda_with_reason(statements)[0]

    def _identify_revenue_quality_issues(self, statements: FinancialStatements,
                                         comparative_data: Optional[List[FinancialStatements]] = None) -> List[str]:
        """Identify potential revenue quality and manipulation issues"""
        quality_issues = []

        income = statements.income_statement
        balance_sheet = statements.balance_sheet

        revenue = income.get('revenue')
        accounts_receivable = balance_sheet.get('accounts_receivable')

        # Accounts receivable growing faster than revenue
        if comparative_data and revenue is not None and accounts_receivable is not None:
            revenue_growth = self.growth(revenue, self.prior_value(comparative_data, 'income_statement', 'revenue'))
            receivables_growth = self.growth(accounts_receivable,
                                             self.prior_value(comparative_data, 'balance_sheet', 'accounts_receivable'))
            if revenue_growth is not None and receivables_growth is not None:
                if receivables_growth > revenue_growth + 0.1:
                    quality_issues.append("Accounts receivable growing significantly faster than revenue")

        # Very high Days Sales Outstanding (ending balance)
        if revenue and revenue > 0 and accounts_receivable and accounts_receivable > 0:
            dso = (accounts_receivable / revenue) * 365
            if dso > 120:
                quality_issues.append(f"Very high Days Sales Outstanding ({dso:.0f} days)")

        # Related party transactions disclosed with a non-zero amount
        for key, value in statements.notes.items():
            if 'related_party' in key and isinstance(value, (int, float)) and value > 0:
                quality_issues.append("Related party revenue transactions require scrutiny")
                break

        return quality_issues

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Key income statement metrics. Metrics whose inputs are missing are omitted;
        pass a dict as `unavailable` to receive the reason for each omission."""
        income = statements.income_statement
        revenue = income.get('revenue')
        metrics = {}
        skipped = unavailable if unavailable is not None else {}

        if revenue is not None and revenue > 0:
            gp = self.gross_profit(income)
            if gp is not None:
                metrics['gross_profit_margin'] = gp / revenue
            else:
                skipped['gross_profit_margin'] = "cost of sales / gross profit not reported"
            if 'operating_income' in income:
                metrics['operating_profit_margin'] = income['operating_income'] / revenue
            else:
                skipped['operating_profit_margin'] = "operating income not reported"
            if 'net_income' in income:
                metrics['net_profit_margin'] = income['net_income'] / revenue
            else:
                skipped['net_profit_margin'] = "net income not reported"
            ebitda, reason = self._calculate_ebitda_with_reason(statements)
            if ebitda is not None:
                metrics['ebitda_margin'] = ebitda / revenue
            else:
                skipped['ebitda_margin'] = reason
        else:
            for k in ('gross_profit_margin', 'operating_profit_margin', 'net_profit_margin', 'ebitda_margin'):
                skipped[k] = "revenue not reported or not positive"

        for kind in ('basic', 'diluted'):
            eps = self._eps(income, kind)
            if eps is not None:
                metrics[f'{kind}_eps'] = eps
            else:
                skipped[f'{kind}_eps'] = f"{kind} EPS not reported and net income / {kind} shares not available"

        pretax_income = income.get('pretax_income')
        tax_expense = income.get('tax_expense')
        if pretax_income is not None and tax_expense is not None and pretax_income > 0:
            metrics['effective_tax_rate'] = tax_expense / pretax_income
        else:
            skipped['effective_tax_rate'] = ("pre-tax income or tax expense not reported"
                                             if pretax_income is None or tax_expense is None
                                             else "pre-tax income is zero or negative")

        return metrics

    def create_eps_analysis(self, statements: FinancialStatements,
                            comparative_data: Optional[List[FinancialStatements]] = None) -> EPSAnalysis:
        """Create comprehensive EPS analysis object (None for anything not computable)."""
        income = statements.income_statement

        basic_eps = self._eps(income, 'basic')
        diluted_eps = self._eps(income, 'diluted')
        basic_shares = income.get('shares_outstanding_basic')
        diluted_shares = income.get('shares_outstanding_diluted')

        dilution_effect = None
        if basic_eps and diluted_eps is not None:
            dilution_effect = (basic_eps - diluted_eps) / abs(basic_eps)

        eps_quality = IncomeQualityIndicator.HIGH_QUALITY
        if dilution_effect is not None and dilution_effect > 0.1:
            eps_quality = IncomeQualityIndicator.MODERATE_QUALITY

        antidilutive_securities = (diluted_shares < basic_shares
                                   if basic_shares and diluted_shares is not None else False)

        eps_growth_rate = None
        eps_volatility = None
        if comparative_data and basic_eps is not None:
            eps_values = [e for e in (self._eps(p.income_statement, 'basic') for p in comparative_data)
                          if e is not None]
            eps_values.append(basic_eps)
            if len(eps_values) > 1:
                eps_growth_rate = self.cagr(eps_values[0], eps_values[-1], len(eps_values) - 1)
                mean_eps = float(np.mean(eps_values))
                eps_volatility = float(np.std(eps_values)) / abs(mean_eps) if mean_eps != 0 else None

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
        items = {k: income.get(k, 0.0) for k in NON_RECURRING_KEYS}
        total_non_recurring = sum(abs(x) for x in items.values())

        net_income = income.get('net_income')
        impact_on_core_earnings = total_non_recurring / abs(net_income) if net_income else None

        frequency_analysis = "Single period analysis"
        persistence_assessment = "Cannot assess without historical data"

        if comparative_data:
            periods_with_non_recurring = 0
            total_periods = len(comparative_data) + 1
            for past_statements in comparative_data:
                past_income = past_statements.income_statement
                if sum(abs(past_income.get(k, 0.0)) for k in NON_RECURRING_KEYS) > 0:
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
            discontinued_operations=items['discontinued_operations'],
            unusual_items=items['unusual_items'],
            extraordinary_items=items['extraordinary_items'],
            restructuring_charges=items['restructuring_charges'],
            impairment_losses=items['impairment_losses'],
            gains_losses_disposals=items['gains_losses_disposals'],
            impact_on_core_earnings=impact_on_core_earnings,
            frequency_analysis=frequency_analysis,
            persistence_assessment=persistence_assessment
        )

    def assess_revenue_quality(self, statements: FinancialStatements,
                               comparative_data: Optional[
                                   List[FinancialStatements]] = None) -> RevenueQualityAssessment:
        """Comprehensive revenue quality assessment (None for anything not computable)."""
        income = statements.income_statement
        balance_sheet = statements.balance_sheet

        revenue = income.get('revenue')
        accounts_receivable = balance_sheet.get('accounts_receivable')

        revenue_growth_rate = None
        revenue_volatility = None
        days_sales_outstanding = None

        if revenue and revenue > 0 and accounts_receivable is not None:
            days_sales_outstanding = (accounts_receivable / revenue) * 365

        if comparative_data and revenue is not None:
            revenue_values = [p.income_statement['revenue'] for p in comparative_data
                              if 'revenue' in p.income_statement]
            revenue_values.append(revenue)
            if len(revenue_values) > 1:
                revenue_growth_rate = self.cagr(revenue_values[0], revenue_values[-1], len(revenue_values) - 1)
                mean_revenue = float(np.mean(revenue_values))
                revenue_volatility = float(np.std(revenue_values)) / mean_revenue if mean_revenue > 0 else None

        quality_indicators = []
        recognition_issues = []

        if days_sales_outstanding is not None:
            if days_sales_outstanding <= 45:
                quality_indicators.append("Healthy collection period")
            elif days_sales_outstanding > 90:
                recognition_issues.append("Extended collection period may indicate quality issues")

        if revenue_growth_rate is not None:
            if revenue_growth_rate > 0:
                quality_indicators.append("Positive revenue growth")
            elif revenue_growth_rate < -0.1:
                recognition_issues.append("Significant revenue decline")

        if revenue_volatility is not None:
            if revenue_volatility < 0.1:
                quality_indicators.append("Stable revenue pattern")
            elif revenue_volatility > 0.3:
                recognition_issues.append("High revenue volatility")

        quality_score = None
        if days_sales_outstanding is not None or revenue_volatility is not None:
            quality_score = 100.0
            quality_score -= len(recognition_issues) * 20
            if days_sales_outstanding is not None:
                quality_score -= max(0.0, (days_sales_outstanding - 45) / 10 * 5)
            if revenue_volatility is not None:
                quality_score -= max(0.0, revenue_volatility * 100)
            quality_score = max(0.0, min(100.0, quality_score))

        return RevenueQualityAssessment(
            revenue_growth_rate=revenue_growth_rate,
            revenue_volatility=revenue_volatility,
            seasonality_factor=None,      # needs quarterly data
            revenue_concentration=None,   # needs segment data
            days_sales_outstanding=days_sales_outstanding,
            revenue_quality_score=quality_score,
            recognition_issues=recognition_issues,
            quality_indicators=quality_indicators
        )
