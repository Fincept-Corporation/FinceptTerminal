
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
import pandas as pd
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

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive cash flow statement analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all cash flow aspects
        """
        results = []

        # Validate data sufficiency
        required_fields = ['operating_cash_flow']
        is_sufficient, missing_fields = self.validate_data_sufficiency(statements, required_fields)

        if not is_sufficient:
            if self.logger:
                self.logger.warning(f"Insufficient data for complete analysis. Missing: {missing_fields}")

        # Operating cash flow analysis
        results.extend(self._analyze_operating_cash_flow(statements, comparative_data, industry_data))

        # Investing cash flow analysis
        results.extend(self._analyze_investing_cash_flow(statements, comparative_data))

        # Financing cash flow analysis
        results.extend(self._analyze_financing_cash_flow(statements, comparative_data))

        # Free cash flow analysis
        results.extend(self._analyze_free_cash_flow(statements, comparative_data, industry_data))

        # Cash flow ratios
        results.extend(self._calculate_cash_flow_ratios(statements, comparative_data, industry_data))

        # Cash flow quality assessment
        results.extend(self._assess_cash_flow_quality(statements, comparative_data))

        # Statement linkages
        results.extend(self._analyze_statement_linkages(statements, comparative_data))

        # IFRS vs US GAAP differences (if applicable)
        results.extend(self._analyze_reporting_differences(statements))

        return results

    def _analyze_operating_cash_flow(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None,
                                     industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze operating cash flow performance and quality"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        net_income = income_statement.get('net_income', 0)
        revenue = income_statement.get('revenue', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        # Operating Cash Flow Ratio
        if current_liabilities > 0:
            ocf_ratio = self.safe_divide(operating_cash_flow, current_liabilities)
            benchmark = self.cash_flow_benchmarks.get('operating_cash_flow_ratio', {})
            risk_level = self.assess_risk_level(ocf_ratio, benchmark, higher_is_better=True)

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
                limitations=["Based on current period performance"]
            ))

        # Cash Flow Margin
        if revenue > 0:
            cf_margin = self.safe_divide(operating_cash_flow, revenue)
            benchmark = self.cash_flow_benchmarks.get('cash_flow_margin', {})
            risk_level = self.assess_risk_level(cf_margin, benchmark, higher_is_better=True)

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

        # Cash Return on Assets
        if total_assets > 0:
            cash_roa = self.safe_divide(operating_cash_flow, total_assets)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.PROFITABILITY,
                metric_name="Cash Return on Assets",
                value=cash_roa,
                interpretation=f"Cash return on assets of {self.format_percentage(cash_roa)} shows cash generation efficiency",
                risk_level=RiskLevel.LOW if cash_roa > 0.1 else RiskLevel.MODERATE if cash_roa > 0.05 else RiskLevel.HIGH,
                methodology="Operating Cash Flow / Total Assets"
            ))

        # Quality of Earnings
        if net_income != 0:
            quality_earnings = self.safe_divide(operating_cash_flow, net_income)
            benchmark = self.cash_flow_benchmarks.get('quality_of_earnings', {})

            quality_interpretation = "High earnings quality - strong cash conversion" if quality_earnings >= 1.0 else "Moderate earnings quality" if quality_earnings >= 0.8 else "Low earnings quality - poor cash conversion"
            quality_risk = RiskLevel.LOW if quality_earnings >= 1.0 else RiskLevel.MODERATE if quality_earnings >= 0.7 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Quality of Earnings",
                value=quality_earnings,
                interpretation=quality_interpretation,
                risk_level=quality_risk,
                methodology="Operating Cash Flow / Net Income",
                limitations=["Single period comparison - trends are more meaningful"]
            ))

        # Working capital impact analysis
        results.extend(self._analyze_working_capital_impact(statements, comparative_data))

        return results

    def _analyze_investing_cash_flow(self, statements: FinancialStatements,
                                     comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze investing cash flow activities"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        investing_cash_flow = cash_flow.get('investing_cash_flow', 0)
        capex = cash_flow.get('capex', 0)
        acquisitions = cash_flow.get('acquisitions', 0)
        asset_sales = cash_flow.get('asset_sales', 0)
        revenue = income_statement.get('revenue', 0)

        # Capital Expenditure Analysis
        if revenue > 0 and capex > 0:
            capex_intensity = self.safe_divide(capex, revenue)

            capex_interpretation = "High capital intensity - significant reinvestment" if capex_intensity > 0.1 else "Moderate capital intensity" if capex_intensity > 0.05 else "Low capital intensity"
            capex_risk = RiskLevel.MODERATE if capex_intensity > 0.15 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Capital Expenditure Intensity",
                value=capex_intensity,
                interpretation=capex_interpretation,
                risk_level=capex_risk,
                methodology="Capital Expenditures / Revenue",
                limitations=["Industry-dependent optimal levels"]
            ))

        # Investment Strategy Analysis
        if investing_cash_flow != 0:
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
                methodology="Total cash flow from investing activities"
            ))

        # Acquisition vs Organic Growth
        if capex > 0 and acquisitions > 0:
            acquisition_ratio = self.safe_divide(acquisitions, capex + acquisitions)

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

        financing_cash_flow = cash_flow.get('financing_cash_flow', 0)
        debt_issued = cash_flow.get('debt_issued', 0)
        debt_repaid = cash_flow.get('debt_repaid', 0)
        equity_issued = cash_flow.get('equity_issued', 0)
        equity_repurchased = cash_flow.get('equity_repurchased', 0)
        dividends_paid = cash_flow.get('dividends_paid', 0)

        # Net Debt Activity
        net_debt_activity = debt_issued - debt_repaid
        if abs(net_debt_activity) > 0:
            debt_interpretation = "Net debt increase - leveraging up" if net_debt_activity > 0 else "Net debt reduction - deleveraging"
            debt_risk = RiskLevel.MODERATE if net_debt_activity > 0 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Net Debt Activity",
                value=net_debt_activity,
                interpretation=debt_interpretation,
                risk_level=debt_risk,
                methodology="Debt Issued - Debt Repaid"
            ))

        # Net Equity Activity
        net_equity_activity = equity_issued - equity_repurchased
        if abs(net_equity_activity) > 0:
            equity_interpretation = "Net equity increase - raising capital" if net_equity_activity > 0 else "Net equity reduction - returning capital to shareholders"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Net Equity Activity",
                value=net_equity_activity,
                interpretation=equity_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Equity Issued - Equity Repurchased"
            ))

        # Dividend Coverage Analysis
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        if dividends_paid > 0 and operating_cash_flow > 0:
            dividend_coverage = self.safe_divide(operating_cash_flow, dividends_paid)

            coverage_interpretation = "Strong dividend coverage" if dividend_coverage > 2.0 else "Adequate dividend coverage" if dividend_coverage > 1.5 else "Weak dividend coverage"
            coverage_risk = RiskLevel.LOW if dividend_coverage > 2.0 else RiskLevel.MODERATE if dividend_coverage > 1.0 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Dividend Coverage Ratio",
                value=dividend_coverage,
                interpretation=coverage_interpretation,
                risk_level=coverage_risk,
                methodology="Operating Cash Flow / Dividends Paid",
                limitations=["Does not consider capital expenditure requirements"]
            ))

        # Financing Mix Analysis
        total_financing = abs(debt_issued) + abs(equity_issued) + abs(debt_repaid) + abs(equity_repurchased)
        if total_financing > 0:
            debt_financing_ratio = self.safe_divide(abs(debt_issued) + abs(debt_repaid), total_financing)

            financing_interpretation = "Debt-heavy financing activities" if debt_financing_ratio > 0.7 else "Balanced debt and equity financing" if debt_financing_ratio > 0.3 else "Equity-focused financing"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt Financing Ratio",
                value=debt_financing_ratio,
                interpretation=financing_interpretation,
                risk_level=RiskLevel.MODERATE if debt_financing_ratio > 0.8 else RiskLevel.LOW,
                methodology="Debt Activities / Total Financing Activities"
            ))

        return results

    def _analyze_free_cash_flow(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None,
                                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Comprehensive free cash flow analysis"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        capex = cash_flow.get('capex', 0)
        revenue = income_statement.get('revenue', 0)

        # Free Cash Flow to the Firm
        fcf_firm = operating_cash_flow - capex

        if revenue > 0:
            fcf_margin = self.safe_divide(fcf_firm, revenue)
            benchmark = self.cash_flow_benchmarks.get('fcf_margin', {})
            risk_level = self.assess_risk_level(fcf_margin, benchmark, higher_is_better=True)

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

        # Free Cash Flow to Equity
        debt_issued = cash_flow.get('debt_issued', 0)
        debt_repaid = cash_flow.get('debt_repaid', 0)
        net_debt_payments = debt_repaid - debt_issued

        fcf_equity = fcf_firm - net_debt_payments

        results.append(AnalysisResult(
            analysis_type=AnalysisType.PROFITABILITY,
            metric_name="Free Cash Flow to Equity",
            value=fcf_equity,
            interpretation=f"Free cash flow to equity of ${fcf_equity:,.0f} available for dividends and share repurchases",
            risk_level=RiskLevel.LOW if fcf_equity > 0 else RiskLevel.HIGH,
            methodology="FCF Firm - Net Debt Payments"
        ))

        # FCF Conversion Ratio
        net_income = income_statement.get('net_income', 0)
        if net_income > 0:
            fcf_conversion = self.safe_divide(fcf_firm, net_income)

            conversion_interpretation = "Excellent FCF conversion" if fcf_conversion > 1.0 else "Good FCF conversion" if fcf_conversion > 0.8 else "Poor FCF conversion"
            conversion_risk = RiskLevel.LOW if fcf_conversion > 0.8 else RiskLevel.MODERATE if fcf_conversion > 0.5 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="FCF Conversion Ratio",
                value=fcf_conversion,
                interpretation=conversion_interpretation,
                risk_level=conversion_risk,
                methodology="Free Cash Flow / Net Income",
                limitations=["High conversion indicates lower reinvestment or better working capital management"]
            ))

        # FCF Growth Analysis
        if comparative_data and len(comparative_data) > 0:
            fcf_values = []
            periods = []

            for i, past_statements in enumerate(comparative_data):
                past_ocf = past_statements.cash_flow.get('operating_cash_flow', 0)
                past_capex = past_statements.cash_flow.get('capex', 0)
                past_fcf = past_ocf - past_capex
                fcf_values.append(past_fcf)
                periods.append(f"Period-{len(comparative_data) - i}")

            fcf_values.append(fcf_firm)
            periods.append("Current")

            if len(fcf_values) > 1:
                fcf_trend = self.calculate_trend(fcf_values, periods)

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.PROFITABILITY,
                    metric_name="FCF Growth Trend",
                    value=fcf_trend.growth_rate or 0,
                    interpretation=fcf_trend.trend_analysis,
                    risk_level=RiskLevel.LOW if fcf_trend.growth_rate and fcf_trend.growth_rate > 0 else RiskLevel.HIGH,
                    methodology="Compound Annual Growth Rate of Free Cash Flow"
                ))

        return results

    def _calculate_cash_flow_ratios(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None,
                                    industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Calculate comprehensive cash flow ratios"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        capex = cash_flow.get('capex', 0)
        total_debt = balance_sheet.get('long_term_debt', 0) + balance_sheet.get('short_term_debt', 0)

        # Cash Coverage Ratio (for debt service)
        debt_payments = cash_flow.get('debt_repaid', 0)
        interest_expense = income_statement.get('interest_expense', 0)
        total_debt_service = debt_payments + interest_expense

        if total_debt_service > 0:
            cash_coverage = self.safe_divide(operating_cash_flow, total_debt_service)
            benchmark = self.cash_flow_benchmarks.get('cash_coverage_ratio', {})
            risk_level = self.assess_risk_level(cash_coverage, benchmark, higher_is_better=True)

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Cash Coverage Ratio",
                value=cash_coverage,
                interpretation=self.generate_interpretation("cash coverage ratio", cash_coverage, risk_level,
                                                            AnalysisType.SOLVENCY),
                risk_level=risk_level,
                methodology="Operating Cash Flow / (Debt Payments + Interest Expense)",
                limitations=["Based on current period cash flows"]
            ))

        # Debt Coverage Ratio
        if total_debt > 0:
            debt_coverage = self.safe_divide(operating_cash_flow, total_debt)

            debt_coverage_interpretation = "Strong debt coverage ability" if debt_coverage > 0.2 else "Adequate debt coverage" if debt_coverage > 0.1 else "Weak debt coverage ability"
            debt_coverage_risk = RiskLevel.LOW if debt_coverage > 0.15 else RiskLevel.MODERATE if debt_coverage > 0.08 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Debt Coverage Ratio",
                value=debt_coverage,
                interpretation=debt_coverage_interpretation,
                risk_level=debt_coverage_risk,
                methodology="Operating Cash Flow / Total Debt"
            ))

        # Capital Expenditure Coverage
        if capex > 0:
            capex_coverage = self.safe_divide(operating_cash_flow, capex)

            capex_coverage_interpretation = "Strong capex coverage - self-funding growth" if capex_coverage > 1.5 else "Adequate capex coverage" if capex_coverage > 1.0 else "Insufficient capex coverage - external funding needed"
            capex_coverage_risk = RiskLevel.LOW if capex_coverage > 1.2 else RiskLevel.MODERATE if capex_coverage > 0.8 else RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.SOLVENCY,
                metric_name="Capital Expenditure Coverage",
                value=capex_coverage,
                interpretation=capex_coverage_interpretation,
                risk_level=capex_coverage_risk,
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

        working_capital_change = cash_flow.get('working_capital_change', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)

        if working_capital_change != 0 and operating_cash_flow != 0:
            wc_impact = self.safe_divide(abs(working_capital_change), abs(operating_cash_flow))

            wc_interpretation = "Significant working capital impact on cash flow" if wc_impact > 0.2 else "Moderate working capital impact" if wc_impact > 0.1 else "Minimal working capital impact"
            wc_risk = RiskLevel.HIGH if wc_impact > 0.3 else RiskLevel.MODERATE if wc_impact > 0.15 else RiskLevel.LOW

            # Determine if working capital helped or hurt cash flow
            if working_capital_change < 0:  # Negative change means working capital increased (cash outflow)
                impact_direction = "Working capital increase reduced operating cash flow"
            else:  # Positive change means working capital decreased (cash inflow)
                impact_direction = "Working capital decrease boosted operating cash flow"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.ACTIVITY,
                metric_name="Working Capital Impact",
                value=wc_impact,
                interpretation=f"{wc_interpretation}. {impact_direction}",
                risk_level=wc_risk,
                methodology="|Working Capital Change| / |Operating Cash Flow|"
            ))

        # Individual working capital components analysis
        ar_change = cash_flow.get('accounts_receivable_change', 0)
        inventory_change = cash_flow.get('inventory_change', 0)
        ap_change = cash_flow.get('accounts_payable_change', 0)

        wc_components = {
            'Accounts Receivable Change': ar_change,
            'Inventory Change': inventory_change,
            'Accounts Payable Change': ap_change
        }

        for component, change in wc_components.items():
            if abs(change) > 0:
                if 'Payable' in component:
                    # For payables, increase is good for cash flow
                    impact_description = "Improved cash flow" if change > 0 else "Reduced cash flow"
                else:
                    # For receivables and inventory, increase is bad for cash flow
                    impact_description = "Reduced cash flow" if change < 0 else "Improved cash flow"

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.ACTIVITY,
                    metric_name=component,
                    value=change,
                    interpretation=f"{component} change of ${change:,.0f} {impact_description}",
                    risk_level=RiskLevel.LOW,
                    methodology="Change in working capital component from cash flow statement"
                ))

        return results

    def _assess_cash_flow_quality(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Comprehensive cash flow quality assessment"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        net_income = income_statement.get('net_income', 0)

        # Cash flow quality indicators
        quality_indicators = []
        red_flags = []
        quality_score = 100

        # 1. Operating cash flow vs Net income relationship
        if net_income > 0:
            ocf_ni_ratio = self.safe_divide(operating_cash_flow, net_income)
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

        # 2. Cash flow trend consistency
        if comparative_data and len(comparative_data) >= 2:
            ocf_values = []
            for past_statements in comparative_data:
                past_ocf = past_statements.cash_flow.get('operating_cash_flow', 0)
                ocf_values.append(past_ocf)
            ocf_values.append(operating_cash_flow)

            # Check for declining trend
            declining_periods = sum(1 for i in range(1, len(ocf_values)) if ocf_values[i] < ocf_values[i - 1])
            if declining_periods > len(ocf_values) // 2:
                red_flags.append("Declining operating cash flow trend")
                quality_score -= 15
            else:
                quality_indicators.append("Stable or improving cash flow trend")

        # 3. Working capital manipulation indicators
        working_capital_change = cash_flow.get('working_capital_change', 0)
        if abs(working_capital_change) > abs(operating_cash_flow) * 0.3:
            red_flags.append("Large working capital changes may indicate manipulation")
            quality_score -= 10

        # 4. One-time items impact
        # This would require detailed cash flow statement analysis

        quality_score = max(0, quality_score)

        quality_interpretation = "High cash flow quality" if quality_score > 80 else "Moderate cash flow quality" if quality_score > 60 else "Low cash flow quality - requires investigation"
        quality_risk = RiskLevel.LOW if quality_score > 75 else RiskLevel.MODERATE if quality_score > 50 else RiskLevel.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Cash Flow Quality Score",
            value=quality_score,
            interpretation=quality_interpretation,
            risk_level=quality_risk,
            recommendations=quality_indicators,
            limitations=red_flags,
            methodology="Composite score based on multiple quality indicators"
        ))

        return results

    def _analyze_statement_linkages(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze linkages between cash flow statement and other financial statements"""
        results = []
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        # Cash flow to income statement reconciliation
        net_income_cf = cash_flow.get('net_income_cf', 0)
        net_income_is = income_statement.get('net_income', 0)

        if abs(net_income_cf - net_income_is) > 0.01:  # Allow for rounding
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Income Statement Reconciliation",
                value=abs(net_income_cf - net_income_is),
                interpretation="Net income figures should match between statements",
                risk_level=RiskLevel.MODERATE,
                limitations=["Potential data quality issue or reporting difference"]
            ))

        # Cash reconciliation
        net_cash_change = cash_flow.get('net_cash_change', 0)
        cash_beginning = cash_flow.get('cash_beginning', 0)
        cash_ending = cash_flow.get('cash_ending', 0)
        cash_bs = balance_sheet.get('cash_equivalents', 0)

        # Check if ending cash matches balance sheet
        if abs(cash_ending - cash_bs) > 0.01:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Cash Balance Reconciliation",
                value=abs(cash_ending - cash_bs),
                interpretation="Ending cash should match balance sheet cash",
                risk_level=RiskLevel.MODERATE,
                limitations=["Potential classification or timing difference"]
            ))

        # Check net change calculation
        calculated_change = cash_ending - cash_beginning
        if abs(net_cash_change - calculated_change) > 0.01:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Net Cash Change Reconciliation",
                value=abs(net_cash_change - calculated_change),
                interpretation="Net cash change should equal ending minus beginning cash",
                risk_level=RiskLevel.HIGH,
                limitations=["Mathematical error in cash flow statement"]
            ))

        return results

    def _analyze_reporting_differences(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Analyze IFRS vs US GAAP differences in cash flow reporting"""
        results = []

        reporting_standard = statements.company_info.reporting_standard
        cash_flow = statements.cash_flow

        # Interest and dividend classification differences
        interest_paid = cash_flow.get('interest_paid', 0)
        dividends_received = cash_flow.get('dividends_received', 0)

        if reporting_standard == ReportingStandard.IFRS:
            if interest_paid != 0 or dividends_received != 0:
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="IFRS Classification Flexibility",
                    value=1.0,
                    interpretation="Under IFRS, interest paid and dividends received can be classified in operating or financing activities",
                    risk_level=RiskLevel.LOW,
                    limitations=["Classification choice may affect comparability with US GAAP companies"],
                    methodology="IFRS allows flexibility in interest and dividend classification"
                ))
        elif reporting_standard == ReportingStandard.US_GAAP:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="US GAAP Classification Rules",
                value=1.0,
                interpretation="Under US GAAP, interest paid is operating, dividends received are operating, dividends paid are financing",
                risk_level=RiskLevel.LOW,
                methodology="US GAAP has fixed classification rules for interest and dividends"
            ))

        return results

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key cash flow metrics"""
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        metrics = {}

        # Core cash flow metrics
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        capex = cash_flow.get('capex', 0)

        metrics['operating_cash_flow'] = operating_cash_flow
        metrics['free_cash_flow_firm'] = operating_cash_flow - capex

        # Cash flow ratios
        revenue = income_statement.get('revenue', 0)
        net_income = income_statement.get('net_income', 0)
        current_liabilities = balance_sheet.get('current_liabilities', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        if revenue > 0:
            metrics['cash_flow_margin'] = self.safe_divide(operating_cash_flow, revenue)
            metrics['fcf_margin'] = self.safe_divide(operating_cash_flow - capex, revenue)

        if net_income != 0:
            metrics['quality_of_earnings'] = self.safe_divide(operating_cash_flow, net_income)

        if current_liabilities > 0:
            metrics['operating_cash_flow_ratio'] = self.safe_divide(operating_cash_flow, current_liabilities)

        if total_assets > 0:
            metrics['cash_return_on_assets'] = self.safe_divide(operating_cash_flow, total_assets)

        # Coverage ratios
        dividends_paid = cash_flow.get('dividends_paid', 0)
        if dividends_paid > 0:
            metrics['dividend_coverage'] = self.safe_divide(operating_cash_flow, dividends_paid)

        if capex > 0:
            metrics['capex_coverage'] = self.safe_divide(operating_cash_flow, capex)

        return metrics

    def create_cash_flow_quality_analysis(self, statements: FinancialStatements,
                                          comparative_data: Optional[
                                              List[FinancialStatements]] = None) -> CashFlowQualityAnalysis:
        """Create comprehensive cash flow quality analysis object"""
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        investing_cash_flow = cash_flow.get('investing_cash_flow', 0)
        financing_cash_flow = cash_flow.get('financing_cash_flow', 0)
        net_income = income_statement.get('net_income', 0)

        # Operating cash quality assessment
        operating_quality = 100
        if net_income > 0:
            ocf_ratio = self.safe_divide(operating_cash_flow, net_income)
            if ocf_ratio < 0.8:
                operating_quality -= 30
            elif ocf_ratio < 1.0:
                operating_quality -= 15

        # Investing cash quality (sustainable vs one-time)
        investing_quality = 100
        capex = cash_flow.get('capex', 0)
        asset_sales = cash_flow.get('asset_sales', 0)
        if asset_sales > abs(capex):
            investing_quality -= 20  # Relying on asset sales

        # Financing cash quality
        financing_quality = 100
        debt_issued = cash_flow.get('debt_issued', 0)
        equity_issued = cash_flow.get('equity_issued', 0)
        if debt_issued > operating_cash_flow * 2:
            financing_quality -= 25  # High debt dependence

        # Overall quality score
        overall_quality = np.mean([operating_quality, investing_quality, financing_quality])

        # Quality indicators and red flags
        quality_indicators = []
        red_flags = []

        if operating_cash_flow > 0:
            quality_indicators.append("Positive operating cash flow")
        else:
            red_flags.append("Negative operating cash flow")

        if net_income > 0 and operating_cash_flow > net_income:
            quality_indicators.append("Operating cash flow exceeds net income")
        elif net_income > 0 and operating_cash_flow < net_income * 0.7:
            red_flags.append("Poor cash conversion from earnings")

        # Earnings-cash correlation
        earnings_cash_correlation = None
        cash_earnings_ratio = None

        if net_income != 0:
            cash_earnings_ratio = self.safe_divide(operating_cash_flow, net_income)

        return CashFlowQualityAnalysis(
            operating_cash_quality=operating_quality,
            investing_cash_quality=investing_quality,
            financing_cash_quality=financing_quality,
            overall_quality_score=overall_quality,
            quality_indicators=quality_indicators,
            red_flags=red_flags,
            earnings_cash_correlation=earnings_cash_correlation,
            cash_earnings_ratio=cash_earnings_ratio
        )

    def create_free_cash_flow_analysis(self, statements: FinancialStatements,
                                       comparative_data: Optional[
                                           List[FinancialStatements]] = None) -> FreeCashFlowAnalysis:
        """Create comprehensive free cash flow analysis object"""
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        capex = cash_flow.get('capex', 0)
        debt_issued = cash_flow.get('debt_issued', 0)
        debt_repaid = cash_flow.get('debt_repaid', 0)

        # Calculate FCF
        fcf_firm = operating_cash_flow - capex
        fcf_equity = fcf_firm - (debt_repaid - debt_issued)

        # FCF metrics
        revenue = income_statement.get('revenue', 0)
        net_income = income_statement.get('net_income', 0)

        fcf_yield = None  # Would need market cap data
        fcf_growth_rate = None
        fcf_volatility = None
        capex_intensity = self.safe_divide(capex, revenue) if revenue > 0 else None
        fcf_conversion_ratio = self.safe_divide(fcf_firm, net_income) if net_income > 0 else None

        # Calculate growth and volatility if historical data available
        if comparative_data and len(comparative_data) > 0:
            fcf_values = []
            for past_statements in comparative_data:
                past_ocf = past_statements.cash_flow.get('operating_cash_flow', 0)
                past_capex = past_statements.cash_flow.get('capex', 0)
                past_fcf = past_ocf - past_capex
                fcf_values.append(past_fcf)

            fcf_values.append(fcf_firm)

            if len(fcf_values) > 1:
                # Growth rate calculation
                if fcf_values[0] > 0:
                    if len(fcf_values) == 2:
                        fcf_growth_rate = (fcf_values[-1] / fcf_values[0]) - 1
                    else:
                        n_periods = len(fcf_values) - 1
                        fcf_growth_rate = (fcf_values[-1] / fcf_values[0]) ** (1 / n_periods) - 1

                # Volatility calculation
                mean_fcf = np.mean(fcf_values)
                std_fcf = np.std(fcf_values)
                fcf_volatility = std_fcf / abs(mean_fcf) if mean_fcf != 0 else 0

        # Sustainability score
        sustainability_score = 100
        if fcf_firm < 0:
            sustainability_score -= 40
        if capex_intensity and capex_intensity > 0.15:
            sustainability_score -= 20
        if fcf_conversion_ratio and fcf_conversion_ratio < 0.5:
            sustainability_score -= 20

        sustainability_score = max(0, sustainability_score)

        return FreeCashFlowAnalysis(
            fcf_firm=fcf_firm,
            fcf_equity=fcf_equity,
            fcf_yield=fcf_yield,
            fcf_growth_rate=fcf_growth_rate,
            fcf_volatility=fcf_volatility,
            capex_intensity=capex_intensity,
            fcf_conversion_ratio=fcf_conversion_ratio,
            sustainability_score=sustainability_score
        )

    def convert_indirect_to_direct_method(self, statements: FinancialStatements) -> Dict[str, float]:
        """Convert cash flow from indirect to direct method presentation"""
        # This is a simplified conversion - in practice would require more detailed data
        cash_flow = statements.cash_flow
        income_statement = statements.income_statement

        # Start with net income
        net_income = cash_flow.get('net_income_cf', income_statement.get('net_income', 0))

        # Add back non-cash items
        depreciation = cash_flow.get('depreciation_cf', 0)
        amortization = cash_flow.get('amortization_cf', 0)
        stock_compensation = cash_flow.get('stock_compensation', 0)

        # Working capital changes
        ar_change = cash_flow.get('accounts_receivable_change', 0)
        inventory_change = cash_flow.get('inventory_change', 0)
        ap_change = cash_flow.get('accounts_payable_change', 0)

        # Approximate direct method components
        revenue = income_statement.get('revenue', 0)
        cash_received_from_customers = revenue + ar_change  # Simplified

        cost_of_sales = income_statement.get('cost_of_sales', 0)
        cash_paid_to_suppliers = cost_of_sales - inventory_change - ap_change  # Simplified

        operating_expenses = income_statement.get('operating_expenses', 0)
        cash_paid_for_expenses = operating_expenses - depreciation - amortization - stock_compensation

        direct_method = {
            'cash_received_from_customers': cash_received_from_customers,
            'cash_paid_to_suppliers': -abs(cash_paid_to_suppliers),
            'cash_paid_for_operating_expenses': -abs(cash_paid_for_expenses),
            'net_operating_cash_flow': cash_received_from_customers - abs(cash_paid_to_suppliers) - abs(
                cash_paid_for_expenses)
        }

        return direct_method