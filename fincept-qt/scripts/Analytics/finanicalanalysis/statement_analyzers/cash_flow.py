
"""
Financial Statement Cash Flow Module
========================================

Cash flow statement analysis and liquidity assessment

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


class CashFlowMethod(Enum):
    """Cash flow statement preparation methods"""
    DIRECT = "direct"
    INDIRECT = "indirect"


class CashFlowQuality(Enum):
    """Cash flow quality classification"""
    HIGH_QUALITY = "high_quality"
    MODERATE_QUALITY = "moderate_quality"
    LOW_QUALITY = "low_quality"
    MANIPULATED = "manipulated"


class CashFlowTrend(Enum):
    """Cash flow trend patterns"""
    STRENGTHENING = "strengthening"
    STABLE = "stable"
    WEAKENING = "weakening"
    VOLATILE = "volatile"


@dataclass
class CashFlowQualityAnalysis:
    """Comprehensive cash flow quality assessment"""
    operating_cash_quality: float
    investing_cash_quality: float
    financing_cash_quality: float
    overall_quality_score: float
    quality_indicators: List[str] = field(default_factory=list)
    red_flags: List[str] = field(default_factory=list)
    earnings_cash_correlation: float = None
    cash_earnings_ratio: float = None


@dataclass
class FreeCashFlowAnalysis:
    """Free cash flow calculations and analysis"""
    fcf_firm: float
    fcf_equity: float
    fcf_yield: float = None
    fcf_growth_rate: float = None
    fcf_volatility: float = None
    capex_intensity: float = None
    fcf_conversion_ratio: float = None
    sustainability_score: float = None


@dataclass
class CashFlowRatios:
    """Comprehensive cash flow ratio analysis"""
    # Performance ratios
    operating_cash_flow_ratio: float
    cash_flow_margin: float
    cash_return_on_assets: float

    # Coverage ratios
    cash_coverage_ratio: float
    debt_coverage_ratio: float
    dividend_coverage_ratio: float
    capex_coverage_ratio: float

    # Quality ratios
    operating_cash_to_net_income: float
    cash_to_earnings: float
    quality_of_earnings: float


class CashFlowAnalyzer(BaseAnalyzer):
    """
    Comprehensive cash flow statement analyzer implementing CFA Institute standards.
    Covers operating, investing, financing activities, FCF analysis, and quality assessment.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_cash_flow_formulas()
        self._initialize_cash_flow_benchmarks()

    def _initialize_cash_flow_formulas(self):
        """Initialize cash flow specific formulas"""
        self.formula_registry.update({
            'operating_cash_flow_ratio': lambda ocf, current_liabs: self.safe_divide(ocf, current_liabs),
            'cash_flow_margin': lambda ocf, revenue: self.safe_divide(ocf, revenue),
            'cash_return_on_assets': lambda ocf, total_assets: self.safe_divide(ocf, total_assets),
            'free_cash_flow_firm': lambda ocf, capex: ocf - capex,
            'free_cash_flow_equity': lambda fcf_firm, net_debt_payments: fcf_firm - net_debt_payments,
            'cash_coverage_ratio': lambda ocf, debt_payments: self.safe_divide(ocf, debt_payments),
            'quality_of_earnings': lambda ocf, net_income: self.safe_divide(ocf, net_income),
            'capex_intensity': lambda capex, revenue: self.safe_divide(capex, revenue)
        })

    def _initialize_cash_flow_benchmarks(self):
        """Initialize cash flow specific benchmarks"""
        self.cash_flow_benchmarks = {
            'operating_cash_flow_ratio': {'excellent': 0.4, 'good': 0.25, 'adequate': 0.15, 'poor': 0.1},
            'cash_flow_margin': {'excellent': 0.2, 'good': 0.15, 'adequate': 0.1, 'poor': 0.05},
            'quality_of_earnings': {'excellent': 1.2, 'good': 1.0, 'adequate': 0.8, 'poor': 0.6},
            'cash_coverage_ratio': {'excellent': 3.0, 'good': 2.0, 'adequate': 1.5, 'poor': 1.0},
            'fcf_margin': {'excellent': 0.15, 'good': 0.1, 'adequate': 0.05, 'poor': 0.02}
        }

    # Sign convention (fixed by DataProcessor): capex, dividends_paid, debt_repaid,
    # equity_repurchased, acquisitions ... are positive magnitudes; operating /
    # investing / financing subtotals keep their reported sign; working-capital
    # change lines keep the cash-flow-statement sign (negative = reduced OCF).

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive cash flow statement analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements (oldest first)
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all cash flow aspects
        """
        results = []

        is_sufficient, missing_fields = self.validate_data_sufficiency(statements, ['operating_cash_flow'])
        if not is_sufficient and self.logger:
            self.logger.warning(f"Insufficient data for complete analysis. Missing: {missing_fields}")

        results.extend(self._analyze_operating_cash_flow(statements, comparative_data, industry_data))
        results.extend(self._analyze_investing_cash_flow(statements, comparative_data))
        results.extend(self._analyze_financing_cash_flow(statements, comparative_data))
        results.extend(self._analyze_free_cash_flow(statements, comparative_data, industry_data))
        results.extend(self._calculate_cash_flow_ratios(statements, comparative_data, industry_data))
        results.extend(self._assess_cash_flow_quality(statements, comparative_data))
        results.extend(self._analyze_statement_linkages(statements, comparative_data))
        results.extend(self._analyze_reporting_differences(statements))

        return results

    @staticmethod
    def _fcf(cash_flow: Dict[str, float]) -> Optional[float]:
        """Free cash flow = operating cash flow - capital expenditure (None if either missing)."""
        if 'operating_cash_flow' not in cash_flow or 'capex' not in cash_flow:
            return None
        return cash_flow['operating_cash_flow'] - cash_flow['capex']

    def _ifrs_note(self, statements: FinancialStatements) -> List[str]:
        if statements.company_info.reporting_standard == ReportingStandard.IFRS:
            return ["IFRS lets interest and dividends paid/received sit in operating or financing activities; "
                    "OCF-based ratios may not be comparable with US GAAP filers"]
        return []

    def _analyze_operating_cash_flow(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None,
                                     industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze operating cash flow performance and quality"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        operating_cash_flow = cash_flow.get('operating_cash_flow')
        if operating_cash_flow is None:
            results.append(self.unavailable(AnalysisType.LIQUIDITY, "Operating Cash Flow Ratios",
                                            "operating cash flow not reported"))
            return results

        net_income = income_statement.get('net_income')
        revenue = income_statement.get('revenue')
        current_liabilities = balance_sheet.get('current_liabilities')

        # Operating Cash Flow Ratio (CFA: CFO / current liabilities)
        if current_liabilities and current_liabilities > 0:
            ocf_ratio = operating_cash_flow / current_liabilities
            risk_level = self.assess_risk_level(ocf_ratio, self.cash_flow_benchmarks['operating_cash_flow_ratio'], True)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.LIQUIDITY,
                metric_name="Operating Cash Flow Ratio",
                value=ocf_ratio,
                interpretation=self.generate_interpretation("operating cash flow ratio", ocf_ratio, risk_level,
                                                            AnalysisType.LIQUIDITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(ocf_ratio, industry_data.get(
                    'ocf_ratio') if industry_data else None),
                methodology="Operating Cash Flow / Current Liabilities",
                limitations=["Based on current period performance"] + self._ifrs_note(statements)
            ))

        # Cash Flow Margin
        if revenue is not None and revenue > 0:
            cf_margin = operating_cash_flow / revenue
            risk_level = self.assess_risk_level(cf_margin, self.cash_flow_benchmarks['cash_flow_margin'], True)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Cash Flow Margin",
                value=cf_margin,
                interpretation=self.generate_interpretation("cash flow margin", cf_margin, risk_level,
                                                            AnalysisType.PROFITABILITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(cf_margin, industry_data.get(
                    'cf_margin') if industry_data else None),
                methodology="Operating Cash Flow / Revenue",
                limitations=["May vary with working capital changes"]
            ))

        # Cash Return on Assets (CFA: CFO / average total assets)
        avg_assets, basis = self.average_balance(statements, comparative_data, 'total_assets')
        if avg_assets is not None and avg_assets > 0:
            cash_roa = operating_cash_flow / avg_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Cash Return on Assets",
                value=cash_roa,
                interpretation=f"Cash return on assets of {self.format_percentage(cash_roa)} shows cash generation efficiency",
                risk_level=RiskLevel.LOW if cash_roa > 0.1 else RiskLevel.MODERATE if cash_roa > 0.05 else RiskLevel.HIGH,
                methodology=f"Operating Cash Flow / Total Assets ({self.basis_note(basis)})"
            ))

        # Quality of Earnings (CFO / NI) - only meaningful with positive earnings
        if net_income is not None:
            if net_income > 0:
                quality_earnings = operating_cash_flow / net_income
                quality_interpretation = "High earnings quality - strong cash conversion" if quality_earnings >= 1.0 else "Moderate earnings quality" if quality_earnings >= 0.8 else "Low earnings quality - poor cash conversion"
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Quality of Earnings",
                    value=quality_earnings,
                    interpretation=quality_interpretation,
                    risk_level=RiskLevel.LOW if quality_earnings >= 1.0 else RiskLevel.MODERATE if quality_earnings >= 0.7 else RiskLevel.HIGH,
                    methodology="Operating Cash Flow / Net Income",
                    limitations=["Single period comparison - trends are more meaningful"]
                ))
            else:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Quality of Earnings",
                    value=None,
                    interpretation=(f"Not meaningful with a net loss (net income {net_income:,.2f}, "
                                    f"operating cash flow {operating_cash_flow:,.2f}); "
                                    + ("cash flow is positive despite the loss" if operating_cash_flow > 0
                                       else "both earnings and operating cash flow are negative")),
                    risk_level=RiskLevel.MODERATE if operating_cash_flow > 0 else RiskLevel.HIGH,
                    methodology="Operating Cash Flow / Net Income (defined for positive net income)"
                ))

        results.extend(self._analyze_working_capital_impact(statements, comparative_data))
        return results

    def _analyze_investing_cash_flow(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze investing cash flow activities"""
        results = []
        cash_flow = statements.cash_flow
        revenue = statements.income_statement.get('revenue')
        capex = cash_flow.get('capex')
        acquisitions = cash_flow.get('acquisitions')

        if capex is not None and revenue is not None and revenue > 0:
            capex_intensity = capex / revenue
            capex_interpretation = "High capital intensity - significant reinvestment" if capex_intensity > 0.1 else "Moderate capital intensity" if capex_intensity > 0.05 else "Low capital intensity"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Capital Expenditure Intensity",
                value=capex_intensity,
                interpretation=capex_interpretation,
                risk_level=RiskLevel.MODERATE if capex_intensity > 0.15 else RiskLevel.LOW,
                methodology="Capital Expenditures / Revenue",
                limitations=["Industry-dependent optimal levels"]
            ))

        investing_cash_flow = cash_flow.get('investing_cash_flow')
        if investing_cash_flow is not None and investing_cash_flow != 0:
            if investing_cash_flow < 0:
                investment_interpretation = "Net investment in assets - growth or maintenance focus"
                investment_risk = RiskLevel.LOW
            else:
                investment_interpretation = "Net divestiture - asset sales or reduced investment"
                investment_risk = RiskLevel.MODERATE
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Investing Cash Flow",
                value=investing_cash_flow,
                interpretation=investment_interpretation,
                risk_level=investment_risk,
                methodology="Total cash flow from investing activities (as reported; negative = net outflow)"
            ))

        if capex and capex > 0 and acquisitions and acquisitions > 0:
            acquisition_ratio = acquisitions / (capex + acquisitions)
            growth_interpretation = "Growth primarily through acquisitions" if acquisition_ratio > 0.5 else "Balanced acquisition and organic growth" if acquisition_ratio > 0.2 else "Primarily organic growth"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Acquisition vs Organic Growth",
                value=acquisition_ratio,
                interpretation=growth_interpretation,
                risk_level=RiskLevel.MODERATE if acquisition_ratio > 0.7 else RiskLevel.LOW,
                methodology="Acquisitions / (Acquisitions + CapEx)",
                limitations=["Acquisition strategy assessment requires multi-period analysis"]
            ))

        return results

    def _analyze_financing_cash_flow(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze financing cash flow activities"""
        results = []
        cash_flow = statements.cash_flow

        # Net borrowing (inflow - repayment); both are positive magnitudes
        if 'debt_issued' in cash_flow or 'debt_repaid' in cash_flow:
            net_debt_activity = cash_flow.get('debt_issued', 0.0) - cash_flow.get('debt_repaid', 0.0)
            if net_debt_activity != 0:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name="Net Debt Activity",
                    value=net_debt_activity,
                    interpretation="Net debt increase - leveraging up" if net_debt_activity > 0 else "Net debt reduction - deleveraging",
                    risk_level=RiskLevel.MODERATE if net_debt_activity > 0 else RiskLevel.LOW,
                    methodology="Debt Issued - Debt Repaid (positive = net borrowing)"
                ))

        if 'equity_issued' in cash_flow or 'equity_repurchased' in cash_flow:
            net_equity_activity = cash_flow.get('equity_issued', 0.0) - cash_flow.get('equity_repurchased', 0.0)
            if net_equity_activity != 0:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name="Net Equity Activity",
                    value=net_equity_activity,
                    interpretation="Net equity increase - raising capital" if net_equity_activity > 0 else "Net equity reduction - returning capital to shareholders",
                    risk_level=RiskLevel.LOW,
                    methodology="Equity Issued - Equity Repurchased (positive = net issuance)"
                ))

        # Dividend payment coverage (CFA: CFO / dividends paid)
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        dividends_paid = cash_flow.get('dividends_paid')
        if dividends_paid and dividends_paid > 0 and operating_cash_flow is not None:
            dividend_coverage = operating_cash_flow / dividends_paid
            coverage_interpretation = "Strong dividend coverage" if dividend_coverage > 2.0 else "Adequate dividend coverage" if dividend_coverage > 1.5 else "Weak dividend coverage - dividends not covered by operating cash flow" if dividend_coverage < 1.0 else "Thin dividend coverage"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Dividend Coverage Ratio",
                value=dividend_coverage,
                interpretation=coverage_interpretation,
                risk_level=RiskLevel.LOW if dividend_coverage > 2.0 else RiskLevel.MODERATE if dividend_coverage > 1.0 else RiskLevel.HIGH,
                methodology="Operating Cash Flow / Dividends Paid",
                limitations=["Does not consider capital expenditure requirements"]
            ))

        flows = [cash_flow.get(k) for k in ('debt_issued', 'equity_issued', 'debt_repaid', 'equity_repurchased')]
        total_financing = sum(abs(v) for v in flows if v is not None)
        if total_financing > 0:
            debt_part = abs(cash_flow.get('debt_issued', 0.0)) + abs(cash_flow.get('debt_repaid', 0.0))
            debt_financing_ratio = debt_part / total_financing
            financing_interpretation = "Debt-heavy financing activities" if debt_financing_ratio > 0.7 else "Balanced debt and equity financing" if debt_financing_ratio > 0.3 else "Equity-focused financing"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt Financing Ratio",
                value=debt_financing_ratio,
                interpretation=financing_interpretation,
                risk_level=RiskLevel.MODERATE if debt_financing_ratio > 0.8 else RiskLevel.LOW,
                methodology="(Debt Issued + Debt Repaid) / (Debt + Equity Issued and Repaid/Repurchased)"
            ))

        return results

    def _analyze_free_cash_flow(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None,
                                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Free cash flow analysis"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        fcf = self._fcf(cash_flow)
        if fcf is None:
            missing = [n for n, k in (('operating cash flow', 'operating_cash_flow'), ('capital expenditure', 'capex'))
                       if k not in cash_flow]
            results.append(self.unavailable(AnalysisType.PROFITABILITY, "Free Cash Flow",
                                            f"{' and '.join(missing)} not reported",
                                            "Operating Cash Flow - Capital Expenditures"))
            return results

        revenue = income_statement.get('revenue')
        if revenue is not None and revenue > 0:
            fcf_margin = fcf / revenue
            risk_level = self.assess_risk_level(fcf_margin, self.cash_flow_benchmarks['fcf_margin'], True)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Free Cash Flow Margin",
                value=fcf_margin,
                interpretation=self.generate_interpretation("free cash flow margin", fcf_margin, risk_level,
                                                            AnalysisType.PROFITABILITY),
                risk_level=risk_level,
                benchmark_comparison=self.compare_to_industry(fcf_margin, industry_data.get(
                    'fcf_margin') if industry_data else None),
                methodology="(Operating Cash Flow - Capital Expenditures) / Revenue"
            ))

        # FCFE (CFA): CFO - FCInv + net borrowing. CFO is already after interest under US GAAP.
        if 'debt_issued' in cash_flow or 'debt_repaid' in cash_flow:
            net_borrowing = cash_flow.get('debt_issued', 0.0) - cash_flow.get('debt_repaid', 0.0)
            fcf_equity = fcf + net_borrowing
            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Free Cash Flow to Equity",
                value=fcf_equity,
                interpretation=f"Free cash flow to equity of {fcf_equity:,.0f} available for dividends and share repurchases",
                risk_level=RiskLevel.LOW if fcf_equity > 0 else RiskLevel.HIGH,
                methodology="Operating Cash Flow - Capital Expenditures + (Debt Issued - Debt Repaid)",
                limitations=["Under IFRS, interest paid classified in financing must be deducted separately"]
            ))
        else:
            results.append(self.unavailable(AnalysisType.PROFITABILITY, "Free Cash Flow to Equity",
                                            "debt issued / repaid not reported, net borrowing unknown",
                                            "Operating Cash Flow - Capital Expenditures + Net Borrowing"))

        net_income = income_statement.get('net_income')
        if net_income is not None and net_income > 0:
            fcf_conversion = fcf / net_income
            conversion_interpretation = "Excellent FCF conversion" if fcf_conversion > 1.0 else "Good FCF conversion" if fcf_conversion > 0.8 else "Poor FCF conversion"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="FCF Conversion Ratio",
                value=fcf_conversion,
                interpretation=conversion_interpretation,
                risk_level=RiskLevel.LOW if fcf_conversion > 0.8 else RiskLevel.MODERATE if fcf_conversion > 0.5 else RiskLevel.HIGH,
                methodology="(Operating Cash Flow - Capital Expenditures) / Net Income",
                limitations=["High conversion indicates lower reinvestment or better working capital management"]
            ))

        # FCF growth over the supplied history
        if comparative_data:
            fcf_values, periods = [], []
            for i, past in enumerate(comparative_data):
                past_fcf = self._fcf(past.cash_flow)
                if past_fcf is not None:
                    fcf_values.append(past_fcf)
                    periods.append(f"Period-{len(comparative_data) - i}")
            fcf_values.append(fcf)
            periods.append("Current")

            if len(fcf_values) > 1:
                fcf_trend = self.calculate_trend(fcf_values, periods)
                if fcf_trend.growth_rate is None:
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.PROFITABILITY,
                        metric_name="FCF Growth Trend",
                        value=None,
                        interpretation=(f"Not available: growth rate is not defined when free cash flow is zero or "
                                        f"negative at either end of the window ({fcf_values[0]:,.0f} -> {fcf_values[-1]:,.0f}); "
                                        f"trend: {fcf_trend.trend_analysis}"),
                        risk_level=None,
                        methodology="Compound Annual Growth Rate of Free Cash Flow"
                    ))
                else:
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.PROFITABILITY,
                        metric_name="FCF Growth Trend",
                        value=fcf_trend.growth_rate,
                        interpretation=fcf_trend.trend_analysis,
                        risk_level=RiskLevel.LOW if fcf_trend.growth_rate > 0 else RiskLevel.HIGH,
                        methodology=f"Compound Annual Growth Rate of Free Cash Flow over {len(fcf_values) - 1} periods"
                    ))

        return results

    def _calculate_cash_flow_ratios(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None,
                                    industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """CFA coverage ratios"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        if operating_cash_flow is None:
            return results

        # Cash debt-service coverage: (CFO + interest paid) / (interest paid + principal repaid).
        # Under US GAAP interest paid is inside CFO, so it is added back to the numerator.
        interest = cash_flow.get('interest_paid', income_statement.get('interest_expense'))
        principal = cash_flow.get('debt_repaid')
        if interest is None and principal is not None:
            results.append(self.unavailable(AnalysisType.SOLVENCY, "Cash Coverage Ratio",
                                            "interest paid / interest expense not reported, debt service unknown",
                                            "(Operating Cash Flow + Interest Paid) / (Interest Paid + Debt Repaid)"))
        elif interest is not None:
            interest = abs(interest)
            principal_note = "" if principal is not None else "; debt repaid not reported, so interest-only coverage"
            principal = principal or 0.0
            debt_service = interest + principal
            if debt_service > 0:
                us_gaap = statements.company_info.reporting_standard == ReportingStandard.US_GAAP
                numerator = operating_cash_flow + (interest if us_gaap else 0.0)
                cash_coverage = numerator / debt_service
                risk_level = self.assess_risk_level(cash_coverage, self.cash_flow_benchmarks['cash_coverage_ratio'], True)
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.SOLVENCY,
                    metric_name="Cash Coverage Ratio",
                    value=cash_coverage,
                    interpretation=self.generate_interpretation("cash debt service coverage", cash_coverage, risk_level,
                                                                AnalysisType.SOLVENCY),
                    risk_level=risk_level,
                    methodology=("(Operating Cash Flow + Interest Paid) / (Interest Paid + Debt Repaid)" if us_gaap
                                 else "Operating Cash Flow / (Interest Paid + Debt Repaid)")
                    + ("" if 'interest_paid' in cash_flow else "; interest expense used as a proxy for interest paid")
                    + principal_note,
                    limitations=["Based on current period cash flows"]
                ))

        # Debt coverage (CFA): CFO / total debt
        total_debt = self.total_debt(statements.balance_sheet)
        if total_debt and total_debt > 0:
            debt_coverage = operating_cash_flow / total_debt
            debt_coverage_interpretation = "Strong debt coverage ability" if debt_coverage > 0.2 else "Adequate debt coverage" if debt_coverage > 0.1 else "Weak debt coverage ability"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt Coverage Ratio",
                value=debt_coverage,
                interpretation=debt_coverage_interpretation,
                risk_level=RiskLevel.LOW if debt_coverage > 0.15 else RiskLevel.MODERATE if debt_coverage > 0.08 else RiskLevel.HIGH,
                methodology="Operating Cash Flow / (Short-term + Long-term Debt)"
            ))

        # Reinvestment ratio (CFA): CFO / capital expenditure
        capex = cash_flow.get('capex')
        if capex and capex > 0:
            capex_coverage = operating_cash_flow / capex
            capex_coverage_interpretation = "Strong capex coverage - self-funding growth" if capex_coverage > 1.5 else "Adequate capex coverage" if capex_coverage > 1.0 else "Insufficient capex coverage - external funding needed"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Capital Expenditure Coverage",
                value=capex_coverage,
                interpretation=capex_coverage_interpretation,
                risk_level=RiskLevel.LOW if capex_coverage > 1.2 else RiskLevel.MODERATE if capex_coverage > 0.8 else RiskLevel.HIGH,
                methodology="Operating Cash Flow / Capital Expenditures",
                limitations=["Does not consider maintenance vs growth capex split"]
            ))

        return results

    def _analyze_working_capital_impact(self, statements: FinancialStatements,
                                        comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze working capital changes impact on cash flow"""
        results = []
        cash_flow = statements.cash_flow
        working_capital_change = cash_flow.get('working_capital_change')
        operating_cash_flow = cash_flow.get('operating_cash_flow')

        if working_capital_change and operating_cash_flow:
            wc_impact = abs(working_capital_change) / abs(operating_cash_flow)
            wc_interpretation = "Significant working capital impact on cash flow" if wc_impact > 0.2 else "Moderate working capital impact" if wc_impact > 0.1 else "Minimal working capital impact"
            # Cash-flow-statement sign: negative = working capital absorbed cash
            impact_direction = ("Working capital increase reduced operating cash flow" if working_capital_change < 0
                                else "Working capital decrease boosted operating cash flow")
            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Working Capital Impact",
                value=wc_impact,
                interpretation=f"{wc_interpretation}. {impact_direction}",
                risk_level=RiskLevel.HIGH if wc_impact > 0.3 else RiskLevel.MODERATE if wc_impact > 0.15 else RiskLevel.LOW,
                methodology="|Working Capital Change| / |Operating Cash Flow|"
            ))

        wc_components = {
            'Accounts Receivable Change': ('accounts_receivable_change', 'receivables'),
            'Inventory Change': ('inventory_change', 'inventory'),
            'Accounts Payable Change': ('accounts_payable_change', 'payables'),
        }
        for component, (key, noun) in wc_components.items():
            change = cash_flow.get(key)
            if change:
                # Cash-flow-statement sign for every line: positive added cash, negative absorbed it
                impact_description = "added to operating cash flow" if change > 0 else "reduced operating cash flow"
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name=component,
                    value=change,
                    interpretation=f"Cash-flow effect of the change in {noun}: {change:,.0f}, {impact_description}",
                    risk_level=RiskLevel.LOW,
                    methodology="Working capital line from the cash flow statement (positive = cash inflow)"
                ))

        return results

    def _assess_cash_flow_quality(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Composite cash flow quality score"""
        results = []
        cash_flow = statements.cash_flow
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        net_income = statements.income_statement.get('net_income')

        if operating_cash_flow is None or net_income is None:
            results.append(self.unavailable(AnalysisType.QUALITY, "Cash Flow Quality Score",
                                            "operating cash flow and net income are both required"))
            return results

        quality_indicators = []
        red_flags = []
        quality_score = 100
        evaluated = 0

        if net_income != 0:
            evaluated += 1
        if net_income > 0:
            ocf_ni_ratio = operating_cash_flow / net_income
            if ocf_ni_ratio >= 1.0:
                quality_indicators.append("Operating cash flow exceeds net income")
            elif ocf_ni_ratio < 0.7:
                red_flags.append("Operating cash flow significantly below net income")
                quality_score -= 20
        elif net_income < 0 and operating_cash_flow > 0:
            quality_indicators.append("Positive operating cash flow despite losses")
        elif net_income < 0 and operating_cash_flow < 0:
            red_flags.append("Both earnings and cash flow are negative")
            quality_score -= 30

        if comparative_data and len(comparative_data) >= 2:
            ocf_values = [p.cash_flow['operating_cash_flow'] for p in comparative_data
                          if 'operating_cash_flow' in p.cash_flow]
            ocf_values.append(operating_cash_flow)
            if len(ocf_values) >= 3:
                evaluated += 1
                declining_periods = sum(1 for i in range(1, len(ocf_values)) if ocf_values[i] < ocf_values[i - 1])
                if declining_periods > (len(ocf_values) - 1) // 2:
                    red_flags.append("Declining operating cash flow trend")
                    quality_score -= 15
                else:
                    quality_indicators.append("Stable or improving cash flow trend")

        working_capital_change = cash_flow.get('working_capital_change')
        if working_capital_change is not None and operating_cash_flow != 0:
            evaluated += 1
            if abs(working_capital_change) > abs(operating_cash_flow) * 0.3:
                red_flags.append("Large working capital swings relative to operating cash flow")
                quality_score -= 10

        if not evaluated:
            results.append(self.unavailable(AnalysisType.QUALITY, "Cash Flow Quality Score",
                                            "net income is zero and no other quality check had its inputs"))
            return results

        quality_score = max(0, quality_score)
        quality_interpretation = "High cash flow quality" if quality_score > 80 else "Moderate cash flow quality" if quality_score > 60 else "Low cash flow quality - requires investigation"

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Cash Flow Quality Score",
            value=quality_score,
            interpretation=quality_interpretation,
            risk_level=RiskLevel.LOW if quality_score > 75 else RiskLevel.MODERATE if quality_score > 50 else RiskLevel.HIGH,
            recommendations=quality_indicators,
            limitations=red_flags,
            methodology="100 minus penalties for weak cash conversion, negative earnings and cash flow, declining OCF and large working-capital swings"
        ))

        return results

    def _analyze_statement_linkages(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Reconciliation checks between the statements (only when both sides are reported)."""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        if 'net_income_cf' in cash_flow and 'net_income' in income_statement:
            diff = abs(cash_flow['net_income_cf'] - income_statement['net_income'])
            if diff > max(0.01, abs(income_statement['net_income']) * 0.001):
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Income Statement Reconciliation",
                    value=diff,
                    interpretation="Net income on the cash flow statement differs from the income statement",
                    risk_level=RiskLevel.MODERATE,
                    limitations=["Often net income including vs excluding minority interest; check the data source"]
                ))

        if 'cash_ending' in cash_flow and 'cash_equivalents' in balance_sheet:
            diff = abs(cash_flow['cash_ending'] - balance_sheet['cash_equivalents'])
            if diff > max(0.01, abs(balance_sheet['cash_equivalents']) * 0.001):
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Cash Balance Reconciliation",
                    value=diff,
                    interpretation="Ending cash on the cash flow statement differs from balance sheet cash",
                    risk_level=RiskLevel.MODERATE,
                    limitations=["Restricted cash or cash held for sale is often the difference"]
                ))

        if all(k in cash_flow for k in ('net_cash_change', 'cash_beginning', 'cash_ending')):
            diff = abs(cash_flow['net_cash_change'] - (cash_flow['cash_ending'] - cash_flow['cash_beginning']))
            if diff > 0.01:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Net Cash Change Reconciliation",
                    value=diff,
                    interpretation="Net cash change does not equal ending minus beginning cash",
                    risk_level=RiskLevel.HIGH,
                    limitations=["FX effects on cash are a common legitimate difference"]
                ))

        return results

    def _analyze_reporting_differences(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """IFRS / US GAAP classification differences are carried as limitations on the
        OCF-based ratios (see _ifrs_note); no static pseudo-metric is emitted."""
        return []

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Key cash flow metrics (missing inputs -> metric omitted, reason in `unavailable`).
        'free_cash_flow_firm' keeps its historical name but is OCF - capex (not FCFF)."""
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        metrics = {}
        skipped = unavailable if unavailable is not None else {}

        operating_cash_flow = cash_flow.get('operating_cash_flow')
        if operating_cash_flow is None:
            skipped['operating_cash_flow'] = "operating cash flow not reported"
            return metrics
        metrics['operating_cash_flow'] = operating_cash_flow

        fcf = self._fcf(cash_flow)
        revenue = income_statement.get('revenue')
        net_income = income_statement.get('net_income')
        current_liabilities = balance_sheet.get('current_liabilities')

        if fcf is not None:
            metrics['free_cash_flow_firm'] = fcf
        else:
            skipped['free_cash_flow_firm'] = "capital expenditure not reported"

        if revenue is not None and revenue > 0:
            metrics['cash_flow_margin'] = operating_cash_flow / revenue
            if fcf is not None:
                metrics['fcf_margin'] = fcf / revenue

        if net_income is not None and net_income > 0:
            metrics['quality_of_earnings'] = operating_cash_flow / net_income
        else:
            skipped['quality_of_earnings'] = "net income not reported or not positive"

        if current_liabilities and current_liabilities > 0:
            metrics['operating_cash_flow_ratio'] = operating_cash_flow / current_liabilities

        avg_assets, _ = self.average_balance(statements, comparative_data, 'total_assets')
        if avg_assets and avg_assets > 0:
            metrics['cash_return_on_assets'] = operating_cash_flow / avg_assets

        dividends_paid = cash_flow.get('dividends_paid')
        if dividends_paid and dividends_paid > 0:
            metrics['dividend_coverage'] = operating_cash_flow / dividends_paid

        capex = cash_flow.get('capex')
        if capex and capex > 0:
            metrics['capex_coverage'] = operating_cash_flow / capex

        return metrics

    def create_cash_flow_quality_analysis(self, statements: FinancialStatements,
                                          comparative_data: Optional[
                                              List[FinancialStatements]] = None) -> CashFlowQualityAnalysis:
        """Create cash flow quality analysis object"""
        cash_flow = statements.cash_flow
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        net_income = statements.income_statement.get('net_income')

        operating_quality = None
        if operating_cash_flow is not None and net_income is not None:
            operating_quality = 100
            if net_income > 0:
                ocf_ratio = operating_cash_flow / net_income
                if ocf_ratio < 0.8:
                    operating_quality -= 30
                elif ocf_ratio < 1.0:
                    operating_quality -= 15
            elif operating_cash_flow < 0:
                operating_quality -= 40

        investing_quality = 100
        capex = cash_flow.get('capex', 0.0)
        if cash_flow.get('asset_sales', 0.0) > capex:
            investing_quality -= 20  # Relying on asset sales

        financing_quality = 100
        if operating_cash_flow is not None and cash_flow.get('debt_issued', 0.0) > max(operating_cash_flow, 0) * 2:
            financing_quality -= 25  # High debt dependence

        parts = [q for q in (operating_quality, investing_quality, financing_quality) if q is not None]
        overall_quality = float(np.mean(parts)) if operating_quality is not None else None

        quality_indicators = []
        red_flags = []
        if operating_cash_flow is not None:
            if operating_cash_flow > 0:
                quality_indicators.append("Positive operating cash flow")
            else:
                red_flags.append("Negative operating cash flow")
            if net_income is not None and net_income > 0:
                if operating_cash_flow > net_income:
                    quality_indicators.append("Operating cash flow exceeds net income")
                elif operating_cash_flow < net_income * 0.7:
                    red_flags.append("Poor cash conversion from earnings")

        cash_earnings_ratio = (operating_cash_flow / net_income
                               if operating_cash_flow is not None and net_income and net_income > 0 else None)

        return CashFlowQualityAnalysis(
            operating_cash_quality=operating_quality,
            investing_cash_quality=investing_quality,
            financing_cash_quality=financing_quality,
            overall_quality_score=overall_quality,
            quality_indicators=quality_indicators,
            red_flags=red_flags,
            earnings_cash_correlation=None,
            cash_earnings_ratio=cash_earnings_ratio
        )

    def create_free_cash_flow_analysis(self, statements: FinancialStatements,
                                       comparative_data: Optional[
                                           List[FinancialStatements]] = None) -> FreeCashFlowAnalysis:
        """Create free cash flow analysis object (None where not computable)."""
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        fcf_firm = self._fcf(cash_flow)
        fcf_equity = None
        if fcf_firm is not None and ('debt_issued' in cash_flow or 'debt_repaid' in cash_flow):
            fcf_equity = fcf_firm + cash_flow.get('debt_issued', 0.0) - cash_flow.get('debt_repaid', 0.0)

        revenue = income_statement.get('revenue')
        net_income = income_statement.get('net_income')
        capex = cash_flow.get('capex')

        capex_intensity = capex / revenue if capex is not None and revenue and revenue > 0 else None
        fcf_conversion_ratio = fcf_firm / net_income if fcf_firm is not None and net_income and net_income > 0 else None

        fcf_growth_rate = None
        fcf_volatility = None
        if comparative_data and fcf_firm is not None:
            fcf_values = [v for v in (self._fcf(p.cash_flow) for p in comparative_data) if v is not None]
            fcf_values.append(fcf_firm)
            if len(fcf_values) > 1:
                fcf_growth_rate = self.cagr(fcf_values[0], fcf_values[-1], len(fcf_values) - 1)
                mean_fcf = float(np.mean(fcf_values))
                fcf_volatility = float(np.std(fcf_values)) / abs(mean_fcf) if mean_fcf != 0 else None

        sustainability_score = None
        if fcf_firm is not None:
            sustainability_score = 100
            if fcf_firm < 0:
                sustainability_score -= 40
            if capex_intensity is not None and capex_intensity > 0.15:
                sustainability_score -= 20
            if fcf_conversion_ratio is not None and fcf_conversion_ratio < 0.5:
                sustainability_score -= 20
            sustainability_score = max(0, sustainability_score)

        return FreeCashFlowAnalysis(
            fcf_firm=fcf_firm,
            fcf_equity=fcf_equity,
            fcf_yield=None,  # needs market capitalisation
            fcf_growth_rate=fcf_growth_rate,
            fcf_volatility=fcf_volatility,
            capex_intensity=capex_intensity,
            fcf_conversion_ratio=fcf_conversion_ratio,
            sustainability_score=sustainability_score
        )

    def convert_indirect_to_direct_method(self, statements: FinancialStatements) -> Dict[str, float]:
        """Approximate direct-method operating cash flows from the indirect method.

        Working-capital lines carry the cash-flow-statement sign (an increase in
        receivables is negative), so cash from customers = revenue + AR change and
        cash paid to suppliers = COGS - inventory change - payables change.
        """
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        depreciation = cash_flow.get('depreciation_cf', 0.0)
        amortization = cash_flow.get('amortization_cf', 0.0)
        stock_compensation = cash_flow.get('stock_compensation', 0.0)
        ar_change = cash_flow.get('accounts_receivable_change', 0.0)
        inventory_change = cash_flow.get('inventory_change', 0.0)
        ap_change = cash_flow.get('accounts_payable_change', 0.0)

        revenue = income_statement.get('revenue', 0.0)
        cash_received_from_customers = revenue + ar_change

        cost_of_sales = income_statement.get('cost_of_sales', 0.0)
        cash_paid_to_suppliers = cost_of_sales - inventory_change - ap_change

        operating_expenses = income_statement.get('operating_expenses', 0.0)
        cash_paid_for_expenses = operating_expenses - depreciation - amortization - stock_compensation

        return {
            'cash_received_from_customers': cash_received_from_customers,
            'cash_paid_to_suppliers': -abs(cash_paid_to_suppliers),
            'cash_paid_for_operating_expenses': -abs(cash_paid_for_expenses),
            'net_operating_cash_flow': cash_received_from_customers - abs(cash_paid_to_suppliers) - abs(
                cash_paid_for_expenses)
        }
