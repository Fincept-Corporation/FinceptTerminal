
"""
Financial Reporting Quality Analysis Module
============================================

Analysis of Financial Reporting Quality per CFA Institute Curriculum:
- Quality Spectrum (GAAP, Decision-Useful, Sustainable, Adequate)
- Earnings Quality Assessment
- Accrual Analysis and Earnings Persistence
- Red Flags and Warning Signs
- Balance Sheet Quality
- Cash Flow Quality
- Management Discretion and Bias Detection

===== DATA SOURCES REQUIRED =====
INPUT:
  - Company financial statements and SEC filings
  - Management discussion and analysis sections
  - Auditor reports and financial statement footnotes
  - Industry benchmarks and competitor data
  - Historical financial data for trend analysis

OUTPUT:
  - Financial reporting quality scores
  - Earnings quality metrics
  - Accrual-based quality indicators
  - Red flag identification
  - Manipulation risk assessment
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


class ReportingQualityLevel(Enum):
    """Financial Reporting Quality Spectrum per CFA"""
    GAAP_COMPLIANT = "gaap_compliant"  # Lowest - meets minimum standards
    DECISION_USEFUL = "decision_useful"  # Relevant, faithfully represented
    SUSTAINABLE = "sustainable"  # Likely to persist
    HIGH_QUALITY = "high_quality"  # All of the above


class EarningsQualityLevel(Enum):
    """Earnings Quality Levels"""
    HIGH = "high"
    MODERATE = "moderate"
    LOW = "low"
    CONCERNING = "concerning"


class ManipulationRisk(Enum):
    """Earnings Manipulation Risk Level"""
    LOW = "low"
    MODERATE = "moderate"
    ELEVATED = "elevated"
    HIGH = "high"


class AccountingAggression(Enum):
    """Accounting Policy Aggressiveness"""
    CONSERVATIVE = "conservative"
    NEUTRAL = "neutral"
    AGGRESSIVE = "aggressive"
    HIGHLY_AGGRESSIVE = "highly_aggressive"


@dataclass
class EarningsQualityAssessment:
    """Comprehensive earnings quality assessment"""
    quality_level: EarningsQualityLevel
    overall_score: float  # 0-100

    # Accrual quality
    accrual_ratio: float
    cash_to_accrual_ratio: float
    accrual_persistence: float

    # Sustainability metrics
    earnings_persistence: float
    revenue_quality: float
    expense_quality: float

    # Warning indicators
    red_flags: List[str]
    concerns: List[str]

    # Cash flow confirmation
    cfo_to_net_income: float
    fcf_to_net_income: float


@dataclass
class AccrualAnalysis:
    """Detailed accrual analysis"""
    total_accruals: float
    operating_accruals: float
    non_operating_accruals: float

    # Ratios
    accrual_ratio_bs: float  # Balance sheet approach
    accrual_ratio_cf: float  # Cash flow approach

    # Components
    change_in_receivables: float
    change_in_inventory: float
    change_in_payables: float
    change_in_deferred_revenue: float

    # Quality indicators
    accrual_quality_score: float
    abnormal_accruals: float


@dataclass
class ManipulationIndicators:
    """Earnings manipulation detection indicators"""
    beneish_m_score: float
    manipulation_probability: float
    risk_level: ManipulationRisk

    # Component scores
    days_sales_receivable_index: float
    gross_margin_index: float
    asset_quality_index: float
    sales_growth_index: float
    depreciation_index: float
    sga_expense_index: float
    leverage_index: float
    total_accruals_to_assets: float

    # Warning flags
    manipulation_flags: List[str]


@dataclass
class BalanceSheetQuality:
    """Balance sheet quality assessment"""
    quality_score: float

    # Asset quality
    asset_quality: float
    receivables_quality: float
    inventory_quality: float
    intangible_quality: float

    # Liability quality
    liability_quality: float
    off_balance_sheet_risk: float

    # Concerns
    concerns: List[str]


@dataclass
class CashFlowQuality:
    """Cash flow statement quality assessment"""
    quality_score: float

    # OCF quality
    ocf_quality: float
    ocf_sustainability: float

    # Working capital manipulation
    working_capital_adjustments: float
    unusual_items: float

    # Cash conversion
    cash_earnings_ratio: float
    cash_conversion_quality: float


class FinancialReportingQualityAnalyzer(BaseAnalyzer):
    """
    Comprehensive financial reporting quality analyzer implementing CFA Institute standards.
    Covers earnings quality, accrual analysis, manipulation detection, and overall quality assessment.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_quality_formulas()
        self._initialize_quality_thresholds()

    def _initialize_quality_formulas(self):
        """Initialize quality-specific formulas"""
        self.formula_registry.update({
            # Accrual ratios
            'accrual_ratio_bs': lambda accruals, avg_noa: self.safe_divide(accruals, avg_noa),
            'accrual_ratio_cf': lambda ni, cfo, avg_noa: self.safe_divide(ni - cfo, avg_noa),

            # Cash flow quality
            'cfo_to_ni': lambda cfo, ni: self.safe_divide(cfo, ni),
            'fcf_to_ni': lambda fcf, ni: self.safe_divide(fcf, ni),
            'cash_earnings_ratio': lambda cfo, earnings: self.safe_divide(cfo, earnings),

            # Beneish M-Score components
            'dsri': lambda dsr_current, dsr_prior: self.safe_divide(dsr_current, dsr_prior),
            'gmi': lambda gm_prior, gm_current: self.safe_divide(gm_prior, gm_current),
            'aqi': lambda aqi_current, aqi_prior: self.safe_divide(aqi_current, aqi_prior),
            'sgi': lambda sales_current, sales_prior: self.safe_divide(sales_current, sales_prior),
            'depi': lambda depi_prior, depi_current: self.safe_divide(depi_prior, depi_current),
            'sgai': lambda sgai_current, sgai_prior: self.safe_divide(sgai_current, sgai_prior),
            'lvgi': lambda leverage_current, leverage_prior: self.safe_divide(leverage_current, leverage_prior),
            'tata': lambda accruals, assets: self.safe_divide(accruals, assets)
        })

    def _initialize_quality_thresholds(self):
        """Initialize quality assessment thresholds"""
        self.quality_thresholds = {
            'accrual_ratio': {
                'high_quality': 0.05, 'moderate': 0.10, 'low': 0.15, 'concerning': 0.25
            },
            'cfo_to_ni': {
                'excellent': 1.2, 'good': 1.0, 'adequate': 0.8, 'concerning': 0.5
            },
            'beneish_m_score': {
                'low_risk': -2.22, 'moderate_risk': -1.78, 'elevated_risk': -1.0
            },
            'earnings_persistence': {
                'high': 0.7, 'moderate': 0.5, 'low': 0.3
            }
        }

        # Beneish M-Score coefficients
        self.beneish_coefficients = {
            'intercept': -4.84,
            'dsri': 0.920,
            'gmi': 0.528,
            'aqi': 0.404,
            'sgi': 0.892,
            'depi': 0.115,
            'sgai': -0.172,
            'lvgi': 0.327,
            'tata': 4.679
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive financial reporting quality analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all quality aspects
        """
        results = []

        # Earnings Quality Analysis
        results.extend(self._analyze_earnings_quality(statements, comparative_data))

        # Accrual Analysis
        results.extend(self._analyze_accruals(statements, comparative_data))

        # Earnings Manipulation Detection
        if comparative_data and len(comparative_data) > 0:
            results.extend(self._analyze_manipulation_indicators(statements, comparative_data))

        # Balance Sheet Quality
        results.extend(self._analyze_balance_sheet_quality(statements, comparative_data))

        # Cash Flow Quality
        results.extend(self._analyze_cash_flow_quality(statements, comparative_data))

        # Revenue Recognition Quality
        results.extend(self._analyze_revenue_quality(statements, comparative_data))

        # Expense Recognition Quality
        results.extend(self._analyze_expense_quality(statements, comparative_data))

        # Red Flag Analysis
        results.extend(self._identify_red_flags(statements, comparative_data))

        # Overall Quality Assessment
        results.extend(self._assess_overall_quality(statements, comparative_data))

        return results

    def _analyze_earnings_quality(self, statements: FinancialStatements,
                                  comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze earnings quality and sustainability"""
        results = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow

        net_income = income_statement.get('net_income', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)

        if net_income == 0:
            return results

        # CFO to Net Income Ratio (Key earnings quality indicator)
        cfo_to_ni = self.safe_divide(operating_cash_flow, net_income)
        threshold = self.quality_thresholds['cfo_to_ni']

        if cfo_to_ni >= threshold['excellent']:
            quality_interpretation = "Excellent earnings quality - strong cash backing"
            quality_level = EarningsQualityLevel.HIGH
            quality_risk = RiskLevel.LOW
        elif cfo_to_ni >= threshold['good']:
            quality_interpretation = "Good earnings quality - earnings well-supported by cash flow"
            quality_level = EarningsQualityLevel.HIGH
            quality_risk = RiskLevel.LOW
        elif cfo_to_ni >= threshold['adequate']:
            quality_interpretation = "Adequate earnings quality - moderate cash support"
            quality_level = EarningsQualityLevel.MODERATE
            quality_risk = RiskLevel.LOW
        elif cfo_to_ni >= threshold['concerning']:
            quality_interpretation = "Low earnings quality - weak cash flow relative to earnings"
            quality_level = EarningsQualityLevel.LOW
            quality_risk = RiskLevel.MODERATE
        else:
            quality_interpretation = "Concerning earnings quality - earnings significantly exceed cash generation"
            quality_level = EarningsQualityLevel.CONCERNING
            quality_risk = RiskLevel.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="CFO to Net Income Ratio",
            value=cfo_to_ni,
            interpretation=quality_interpretation,
            risk_level=quality_risk,
            methodology="Operating Cash Flow / Net Income",
            limitations=["Single period may be affected by timing differences"]
        ))

        # Free Cash Flow to Net Income
        capex = cash_flow.get('capex', 0)
        free_cash_flow = operating_cash_flow - capex

        if net_income != 0:
            fcf_to_ni = self.safe_divide(free_cash_flow, net_income)

            if fcf_to_ni >= 0.8:
                fcf_interpretation = "Strong free cash flow generation relative to earnings"
            elif fcf_to_ni >= 0.5:
                fcf_interpretation = "Moderate free cash flow relative to earnings"
            elif fcf_to_ni >= 0:
                fcf_interpretation = "Limited free cash flow after reinvestment"
            else:
                fcf_interpretation = "Negative free cash flow despite positive earnings - high reinvestment or quality concerns"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="FCF to Net Income Ratio",
                value=fcf_to_ni,
                interpretation=fcf_interpretation,
                risk_level=RiskLevel.MODERATE if fcf_to_ni < 0.3 else RiskLevel.LOW,
                methodology="(Operating Cash Flow - CapEx) / Net Income"
            ))

        # Earnings Persistence Analysis
        if comparative_data and len(comparative_data) >= 2:
            earnings_history = []
            for past_statements in comparative_data:
                past_ni = past_statements.income_statement.get('net_income', 0)
                earnings_history.append(past_ni)
            earnings_history.append(net_income)

            if len(earnings_history) >= 3:
                # Calculate earnings persistence (autocorrelation)
                earnings_array = np.array(earnings_history)
                if len(earnings_array) > 1 and np.std(earnings_array) > 0:
                    try:
                        persistence = np.corrcoef(earnings_array[:-1], earnings_array[1:])[0, 1]
                        threshold = self.quality_thresholds['earnings_persistence']

                        if persistence >= threshold['high']:
                            persistence_interpretation = "High earnings persistence - predictable earnings stream"
                            persistence_risk = RiskLevel.LOW
                        elif persistence >= threshold['moderate']:
                            persistence_interpretation = "Moderate earnings persistence"
                            persistence_risk = RiskLevel.LOW
                        elif persistence >= threshold['low']:
                            persistence_interpretation = "Low earnings persistence - volatile earnings"
                            persistence_risk = RiskLevel.MODERATE
                        else:
                            persistence_interpretation = "Very low or negative earnings persistence - unpredictable"
                            persistence_risk = RiskLevel.HIGH

                        results.append(AnalysisResult(
                            analysis_type=AnalysisType.QUALITY,
                            metric_name="Earnings Persistence",
                            value=persistence,
                            interpretation=persistence_interpretation,
                            risk_level=persistence_risk,
                            methodology="Autocorrelation of earnings over time"
                        ))
                    except:
                        pass

        # Recurring vs Non-Recurring Earnings
        operating_income = income_statement.get('operating_income', 0)
        non_recurring_items = income_statement.get('non_recurring_items', 0)
        special_items = income_statement.get('special_items', 0)

        total_non_recurring = abs(non_recurring_items) + abs(special_items)

        if abs(net_income) > 0:
            non_recurring_ratio = self.safe_divide(total_non_recurring, abs(net_income))

            if non_recurring_ratio > 0.3:
                recurring_interpretation = "Significant non-recurring items - core earnings quality unclear"
                recurring_risk = RiskLevel.HIGH
            elif non_recurring_ratio > 0.15:
                recurring_interpretation = "Moderate non-recurring items impacting reported earnings"
                recurring_risk = RiskLevel.MODERATE
            elif non_recurring_ratio > 0.05:
                recurring_interpretation = "Limited non-recurring items"
                recurring_risk = RiskLevel.LOW
            else:
                recurring_interpretation = "Minimal non-recurring items - clean earnings"
                recurring_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Non-Recurring Items Ratio",
                value=non_recurring_ratio,
                interpretation=recurring_interpretation,
                risk_level=recurring_risk,
                methodology="|Non-Recurring + Special Items| / |Net Income|"
            ))

        return results

    def _analyze_accruals(self, statements: FinancialStatements,
                          comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Comprehensive accrual analysis"""
        results = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        net_income = income_statement.get('net_income', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        # Aggregate Accruals (Cash Flow Approach)
        aggregate_accruals = net_income - operating_cash_flow

        if total_assets > 0:
            accrual_ratio = self.safe_divide(aggregate_accruals, total_assets)
            threshold = self.quality_thresholds['accrual_ratio']

            if abs(accrual_ratio) <= threshold['high_quality']:
                accrual_interpretation = "Low accruals - high earnings quality"
                accrual_risk = RiskLevel.LOW
            elif abs(accrual_ratio) <= threshold['moderate']:
                accrual_interpretation = "Moderate accruals - acceptable earnings quality"
                accrual_risk = RiskLevel.LOW
            elif abs(accrual_ratio) <= threshold['low']:
                accrual_interpretation = "High accruals - earnings quality concerns"
                accrual_risk = RiskLevel.MODERATE
            else:
                accrual_interpretation = "Very high accruals - significant earnings quality concerns"
                accrual_risk = RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Accrual Ratio (CF Approach)",
                value=accrual_ratio,
                interpretation=accrual_interpretation,
                risk_level=accrual_risk,
                methodology="(Net Income - Operating Cash Flow) / Total Assets",
                limitations=["High accruals may indicate growth rather than manipulation"]
            ))

        # Working Capital Accrual Components
        if comparative_data and len(comparative_data) > 0:
            prev_bs = comparative_data[-1].balance_sheet

            # Calculate changes in working capital components
            change_ar = balance_sheet.get('accounts_receivable', 0) - prev_bs.get('accounts_receivable', 0)
            change_inventory = balance_sheet.get('inventory', 0) - prev_bs.get('inventory', 0)
            change_ap = balance_sheet.get('accounts_payable', 0) - prev_bs.get('accounts_payable', 0)
            change_deferred_rev = balance_sheet.get('deferred_revenue', 0) - prev_bs.get('deferred_revenue', 0)

            revenue = income_statement.get('revenue', 0)
            cogs = income_statement.get('cost_of_goods_sold', 0)

            # Receivables Change Analysis
            if revenue > 0:
                ar_change_to_revenue = self.safe_divide(change_ar, revenue)

                if ar_change_to_revenue > 0.1:
                    ar_interpretation = "Significant receivables increase - may indicate aggressive revenue recognition"
                    ar_risk = RiskLevel.MODERATE
                elif ar_change_to_revenue > 0.05:
                    ar_interpretation = "Moderate receivables increase"
                    ar_risk = RiskLevel.LOW
                elif ar_change_to_revenue < -0.05:
                    ar_interpretation = "Receivables decrease - improved collections"
                    ar_risk = RiskLevel.LOW
                else:
                    ar_interpretation = "Stable receivables relative to revenue"
                    ar_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Receivables Change / Revenue",
                    value=ar_change_to_revenue,
                    interpretation=ar_interpretation,
                    risk_level=ar_risk,
                    methodology="Change in A/R / Revenue"
                ))

            # Inventory Change Analysis
            if cogs > 0:
                inventory_change_to_cogs = self.safe_divide(change_inventory, cogs)

                if inventory_change_to_cogs > 0.15:
                    inv_interpretation = "Large inventory build-up - potential obsolescence or channel stuffing"
                    inv_risk = RiskLevel.MODERATE
                elif inventory_change_to_cogs > 0.05:
                    inv_interpretation = "Inventory increase - growth preparation or caution needed"
                    inv_risk = RiskLevel.LOW
                elif inventory_change_to_cogs < -0.05:
                    inv_interpretation = "Inventory decrease - improved efficiency or supply issues"
                    inv_risk = RiskLevel.LOW
                else:
                    inv_interpretation = "Stable inventory levels"
                    inv_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Inventory Change / COGS",
                    value=inventory_change_to_cogs,
                    interpretation=inv_interpretation,
                    risk_level=inv_risk,
                    methodology="Change in Inventory / COGS"
                ))

            # Deferred Revenue Analysis
            if abs(change_deferred_rev) > 0 and revenue > 0:
                deferred_rev_change = self.safe_divide(change_deferred_rev, revenue)

                if deferred_rev_change < -0.05:
                    dr_interpretation = "Deferred revenue released - boosting current period revenue"
                    dr_risk = RiskLevel.MODERATE
                elif deferred_rev_change > 0.05:
                    dr_interpretation = "Deferred revenue build-up - conservative revenue recognition"
                    dr_risk = RiskLevel.LOW
                else:
                    dr_interpretation = "Stable deferred revenue"
                    dr_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Deferred Revenue Change / Revenue",
                    value=deferred_rev_change,
                    interpretation=dr_interpretation,
                    risk_level=dr_risk,
                    methodology="Change in Deferred Revenue / Revenue"
                ))

        return results

    def _analyze_manipulation_indicators(self, statements: FinancialStatements,
                                         comparative_data: List[FinancialStatements]) -> List[AnalysisResult]:
        """Analyze earnings manipulation indicators using Beneish M-Score"""
        results = []

        if not comparative_data or len(comparative_data) == 0:
            return results

        current = statements
        prior = comparative_data[-1]

        current_is = current.income_statement
        current_bs = current.balance_sheet
        prior_is = prior.income_statement
        prior_bs = prior.balance_sheet

        # Calculate Beneish M-Score components
        components = {}

        # 1. Days Sales Receivable Index (DSRI)
        current_ar = current_bs.get('accounts_receivable', 0)
        prior_ar = prior_bs.get('accounts_receivable', 0)
        current_rev = current_is.get('revenue', 0)
        prior_rev = prior_is.get('revenue', 0)

        if prior_rev > 0 and prior_ar > 0 and current_rev > 0:
            dsr_current = current_ar / current_rev * 365
            dsr_prior = prior_ar / prior_rev * 365
            components['dsri'] = dsr_current / dsr_prior if dsr_prior > 0 else 1

        # 2. Gross Margin Index (GMI)
        current_gm = 1 - (current_is.get('cost_of_goods_sold', 0) / current_rev) if current_rev > 0 else 0
        prior_gm = 1 - (prior_is.get('cost_of_goods_sold', 0) / prior_rev) if prior_rev > 0 else 0
        components['gmi'] = prior_gm / current_gm if current_gm > 0 else 1

        # 3. Asset Quality Index (AQI)
        current_assets = current_bs.get('total_assets', 0)
        prior_assets = prior_bs.get('total_assets', 0)
        current_ppe = current_bs.get('ppe_net', 0)
        prior_ppe = prior_bs.get('ppe_net', 0)
        current_ca = current_bs.get('current_assets', 0)
        prior_ca = prior_bs.get('current_assets', 0)

        if current_assets > 0 and prior_assets > 0:
            current_aqi = 1 - (current_ca + current_ppe) / current_assets
            prior_aqi = 1 - (prior_ca + prior_ppe) / prior_assets
            components['aqi'] = current_aqi / prior_aqi if prior_aqi > 0 else 1

        # 4. Sales Growth Index (SGI)
        components['sgi'] = current_rev / prior_rev if prior_rev > 0 else 1

        # 5. Depreciation Index (DEPI)
        current_dep = current_is.get('depreciation', 0)
        prior_dep = prior_is.get('depreciation', 0)
        if current_ppe > 0 and prior_ppe > 0 and (current_ppe + current_dep) > 0 and (prior_ppe + prior_dep) > 0:
            current_depi = current_dep / (current_ppe + current_dep)
            prior_depi = prior_dep / (prior_ppe + prior_dep)
            components['depi'] = prior_depi / current_depi if current_depi > 0 else 1

        # 6. SG&A Expense Index (SGAI)
        current_sga = current_is.get('sga_expense', 0)
        prior_sga = prior_is.get('sga_expense', 0)
        if current_rev > 0 and prior_rev > 0:
            current_sgai = current_sga / current_rev
            prior_sgai = prior_sga / prior_rev
            components['sgai'] = current_sgai / prior_sgai if prior_sgai > 0 else 1

        # 7. Leverage Index (LVGI)
        current_debt = current_bs.get('total_liabilities', 0)
        prior_debt = prior_bs.get('total_liabilities', 0)
        if current_assets > 0 and prior_assets > 0:
            current_lvg = current_debt / current_assets
            prior_lvg = prior_debt / prior_assets
            components['lvgi'] = current_lvg / prior_lvg if prior_lvg > 0 else 1

        # 8. Total Accruals to Total Assets (TATA)
        net_income = current_is.get('net_income', 0)
        cfo = current.cash_flow.get('operating_cash_flow', 0)
        accruals = net_income - cfo
        components['tata'] = accruals / current_assets if current_assets > 0 else 0

        # Calculate M-Score
        coef = self.beneish_coefficients
        m_score = coef['intercept']

        for component, value in components.items():
            if component in coef:
                m_score += coef[component] * value

        # Interpret M-Score
        threshold = self.quality_thresholds['beneish_m_score']

        if m_score < threshold['low_risk']:
            m_interpretation = "Low manipulation risk - M-Score indicates low likelihood of manipulation"
            m_risk = RiskLevel.LOW
            manipulation_risk = ManipulationRisk.LOW
        elif m_score < threshold['moderate_risk']:
            m_interpretation = "Moderate manipulation risk - some indicators warrant attention"
            m_risk = RiskLevel.MODERATE
            manipulation_risk = ManipulationRisk.MODERATE
        elif m_score < threshold['elevated_risk']:
            m_interpretation = "Elevated manipulation risk - multiple warning indicators"
            m_risk = RiskLevel.MODERATE
            manipulation_risk = ManipulationRisk.ELEVATED
        else:
            m_interpretation = "High manipulation risk - pattern consistent with earnings manipulators"
            m_risk = RiskLevel.HIGH
            manipulation_risk = ManipulationRisk.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Beneish M-Score",
            value=m_score,
            interpretation=m_interpretation,
            risk_level=m_risk,
            methodology="Weighted combination of 8 financial ratios (DSRI, GMI, AQI, SGI, DEPI, SGAI, LVGI, TATA)",
            limitations=["Model designed for manufacturing firms", "Not all high scores indicate fraud"]
        ))

        # Report individual component concerns
        manipulation_flags = []

        if components.get('dsri', 1) > 1.1:
            manipulation_flags.append("Disproportionate receivables growth vs revenue (DSRI > 1.1)")
        if components.get('gmi', 1) > 1.1:
            manipulation_flags.append("Declining gross margins (GMI > 1.1)")
        if components.get('aqi', 1) > 1.1:
            manipulation_flags.append("Deteriorating asset quality (AQI > 1.1)")
        if components.get('sgi', 1) > 1.5:
            manipulation_flags.append("Rapid sales growth may stress controls (SGI > 1.5)")
        if components.get('tata', 0) > 0.05:
            manipulation_flags.append("High accruals relative to assets (TATA > 0.05)")

        if manipulation_flags:
            flags_str = "; ".join(manipulation_flags)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Manipulation Warning Flags",
                value=len(manipulation_flags),
                interpretation=f"Identified concerns: {flags_str}",
                risk_level=RiskLevel.MODERATE if len(manipulation_flags) < 3 else RiskLevel.HIGH,
                methodology="Analysis of individual M-Score components"
            ))

        return results

    def _analyze_balance_sheet_quality(self, statements: FinancialStatements,
                                       comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze balance sheet quality"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        total_assets = balance_sheet.get('total_assets', 0)
        total_liabilities = balance_sheet.get('total_liabilities', 0)

        if total_assets <= 0:
            return results

        # Asset Quality Analysis

        # 1. Intangible Asset Concentration
        intangibles = balance_sheet.get('intangible_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)
        intangible_ratio = self.safe_divide(intangibles + goodwill, total_assets)

        if intangible_ratio > 0.5:
            intangible_interpretation = "Very high intangible concentration - balance sheet heavily dependent on subjective valuations"
            intangible_risk = RiskLevel.HIGH
        elif intangible_ratio > 0.3:
            intangible_interpretation = "High intangible assets - significant valuation judgment required"
            intangible_risk = RiskLevel.MODERATE
        elif intangible_ratio > 0.15:
            intangible_interpretation = "Moderate intangible assets"
            intangible_risk = RiskLevel.LOW
        else:
            intangible_interpretation = "Low intangible concentration - primarily tangible asset base"
            intangible_risk = RiskLevel.LOW

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Intangible Asset Concentration",
            value=intangible_ratio,
            interpretation=intangible_interpretation,
            risk_level=intangible_risk,
            methodology="(Intangibles + Goodwill) / Total Assets"
        ))

        # 2. Receivables Quality
        accounts_receivable = balance_sheet.get('accounts_receivable', 0)
        revenue = income_statement.get('revenue', 0)
        allowance_for_doubtful = balance_sheet.get('allowance_doubtful_accounts', 0)

        if revenue > 0:
            dso = (accounts_receivable / revenue) * 365

            if dso > 90:
                dso_interpretation = "Very high DSO - potential collection issues or aggressive revenue recognition"
                dso_risk = RiskLevel.HIGH
            elif dso > 60:
                dso_interpretation = "Elevated DSO - monitor collection trends"
                dso_risk = RiskLevel.MODERATE
            elif dso > 45:
                dso_interpretation = "Moderate DSO"
                dso_risk = RiskLevel.LOW
            else:
                dso_interpretation = "Low DSO - strong collection practices"
                dso_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Days Sales Outstanding",
                value=dso,
                interpretation=dso_interpretation,
                risk_level=dso_risk,
                methodology="(A/R / Revenue) × 365"
            ))

        # Allowance for Doubtful Accounts Adequacy
        if accounts_receivable > 0 and allowance_for_doubtful > 0:
            allowance_ratio = self.safe_divide(allowance_for_doubtful, accounts_receivable)

            if allowance_ratio < 0.01:
                allowance_interpretation = "Very low bad debt allowance - potentially aggressive"
                allowance_risk = RiskLevel.MODERATE
            elif allowance_ratio < 0.03:
                allowance_interpretation = "Low bad debt allowance"
                allowance_risk = RiskLevel.LOW
            elif allowance_ratio < 0.10:
                allowance_interpretation = "Moderate bad debt allowance"
                allowance_risk = RiskLevel.LOW
            else:
                allowance_interpretation = "High bad debt allowance - conservative or quality concerns"
                allowance_risk = RiskLevel.MODERATE

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Bad Debt Allowance Ratio",
                value=allowance_ratio,
                interpretation=allowance_interpretation,
                risk_level=allowance_risk,
                methodology="Allowance for Doubtful Accounts / Accounts Receivable"
            ))

        # 3. Off-Balance Sheet Considerations
        notes = statements.notes
        operating_leases = notes.get('operating_lease_commitments', 0)
        purchase_commitments = notes.get('purchase_commitments', 0)
        guarantees = notes.get('guarantees', 0)

        total_off_bs = operating_leases + purchase_commitments + guarantees

        if total_off_bs > 0 and total_liabilities > 0:
            off_bs_ratio = self.safe_divide(total_off_bs, total_liabilities)

            if off_bs_ratio > 0.5:
                obs_interpretation = "Significant off-balance sheet obligations"
                obs_risk = RiskLevel.MODERATE
            elif off_bs_ratio > 0.2:
                obs_interpretation = "Moderate off-balance sheet commitments"
                obs_risk = RiskLevel.LOW
            else:
                obs_interpretation = "Limited off-balance sheet exposure"
                obs_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Off-Balance Sheet Ratio",
                value=off_bs_ratio,
                interpretation=obs_interpretation,
                risk_level=obs_risk,
                methodology="Off-BS Obligations / Total Liabilities"
            ))

        return results

    def _analyze_cash_flow_quality(self, statements: FinancialStatements,
                                   comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze cash flow statement quality"""
        results = []

        cash_flow = statements.cash_flow
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet

        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        net_income = income_statement.get('net_income', 0)

        # Working Capital Adjustments Analysis
        wc_change = cash_flow.get('change_in_working_capital', 0)

        if operating_cash_flow != 0:
            wc_adjustment_ratio = self.safe_divide(abs(wc_change), abs(operating_cash_flow))

            if wc_adjustment_ratio > 0.5:
                wc_interpretation = "Large working capital adjustments - OCF heavily influenced by timing"
                wc_risk = RiskLevel.MODERATE
            elif wc_adjustment_ratio > 0.25:
                wc_interpretation = "Moderate working capital impact on OCF"
                wc_risk = RiskLevel.LOW
            else:
                wc_interpretation = "Limited working capital adjustments - stable OCF"
                wc_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Working Capital Adjustment Ratio",
                value=wc_adjustment_ratio,
                interpretation=wc_interpretation,
                risk_level=wc_risk,
                methodology="|Working Capital Change| / |OCF|"
            ))

        # Cash from Operations vs Cash Interest Paid
        interest_expense = income_statement.get('interest_expense', 0)
        cash_interest_paid = cash_flow.get('interest_paid', 0)

        if interest_expense > 0 and cash_interest_paid > 0:
            interest_accrual = self.safe_divide(interest_expense - cash_interest_paid, interest_expense)

            if abs(interest_accrual) > 0.2:
                int_interpretation = "Significant difference between interest expense and cash paid"
                int_risk = RiskLevel.MODERATE
            else:
                int_interpretation = "Interest expense and cash payments aligned"
                int_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Interest Accrual Ratio",
                value=interest_accrual,
                interpretation=int_interpretation,
                risk_level=int_risk,
                methodology="(Interest Expense - Cash Interest Paid) / Interest Expense"
            ))

        # Cash Flow Trend Analysis
        if comparative_data and len(comparative_data) >= 2:
            ocf_history = []
            for past_statements in comparative_data:
                past_ocf = past_statements.cash_flow.get('operating_cash_flow', 0)
                ocf_history.append(past_ocf)
            ocf_history.append(operating_cash_flow)

            # Check for sustained positive OCF
            positive_periods = sum(1 for ocf in ocf_history if ocf > 0)
            positive_ratio = positive_periods / len(ocf_history)

            if positive_ratio == 1.0:
                ocf_consistency = "Consistently positive operating cash flow"
                ocf_risk = RiskLevel.LOW
            elif positive_ratio >= 0.75:
                ocf_consistency = "Generally positive operating cash flow"
                ocf_risk = RiskLevel.LOW
            elif positive_ratio >= 0.5:
                ocf_consistency = "Mixed operating cash flow - inconsistent cash generation"
                ocf_risk = RiskLevel.MODERATE
            else:
                ocf_consistency = "Frequently negative operating cash flow - sustainability concerns"
                ocf_risk = RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="OCF Consistency",
                value=positive_ratio,
                interpretation=ocf_consistency,
                risk_level=ocf_risk,
                methodology="Periods with Positive OCF / Total Periods"
            ))

        return results

    def _analyze_revenue_quality(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze revenue recognition quality"""
        results = []

        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        cash_flow = statements.cash_flow
        notes = statements.notes

        revenue = income_statement.get('revenue', 0)
        accounts_receivable = balance_sheet.get('accounts_receivable', 0)
        deferred_revenue = balance_sheet.get('deferred_revenue', 0)
        cash_from_customers = cash_flow.get('cash_from_customers', 0)

        if revenue <= 0:
            return results

        # Revenue vs Cash Collections
        if cash_from_customers > 0:
            cash_to_revenue = self.safe_divide(cash_from_customers, revenue)

            if cash_to_revenue >= 0.95:
                cash_rev_interpretation = "Strong cash collection relative to revenue"
                cash_rev_risk = RiskLevel.LOW
            elif cash_to_revenue >= 0.85:
                cash_rev_interpretation = "Good cash collection"
                cash_rev_risk = RiskLevel.LOW
            elif cash_to_revenue >= 0.70:
                cash_rev_interpretation = "Moderate cash collection - review receivables"
                cash_rev_risk = RiskLevel.MODERATE
            else:
                cash_rev_interpretation = "Low cash collection vs revenue - quality concerns"
                cash_rev_risk = RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Cash Collection Ratio",
                value=cash_to_revenue,
                interpretation=cash_rev_interpretation,
                risk_level=cash_rev_risk,
                methodology="Cash from Customers / Revenue"
            ))

        # Deferred Revenue Trend (Contract Liability)
        if comparative_data and len(comparative_data) > 0:
            prior_deferred = comparative_data[-1].balance_sheet.get('deferred_revenue', 0)

            if prior_deferred > 0 and deferred_revenue > 0:
                deferred_change = (deferred_revenue - prior_deferred) / prior_deferred

                if deferred_change > 0.1:
                    def_interpretation = "Growing deferred revenue - strong order pipeline"
                    def_risk = RiskLevel.LOW
                elif deferred_change < -0.2:
                    def_interpretation = "Significant deferred revenue decline - future revenue concerns"
                    def_risk = RiskLevel.MODERATE
                else:
                    def_interpretation = "Stable deferred revenue"
                    def_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Deferred Revenue Change",
                    value=deferred_change,
                    interpretation=def_interpretation,
                    risk_level=def_risk,
                    methodology="(Current DR - Prior DR) / Prior DR"
                ))

        # Revenue Growth vs Receivables Growth
        if comparative_data and len(comparative_data) > 0:
            prior_revenue = comparative_data[-1].income_statement.get('revenue', 0)
            prior_ar = comparative_data[-1].balance_sheet.get('accounts_receivable', 0)

            if prior_revenue > 0 and prior_ar > 0:
                revenue_growth = (revenue - prior_revenue) / prior_revenue
                ar_growth = (accounts_receivable - prior_ar) / prior_ar

                growth_divergence = ar_growth - revenue_growth

                if growth_divergence > 0.15:
                    div_interpretation = "Receivables growing much faster than revenue - potential quality issue"
                    div_risk = RiskLevel.HIGH
                elif growth_divergence > 0.05:
                    div_interpretation = "Receivables outpacing revenue growth"
                    div_risk = RiskLevel.MODERATE
                elif growth_divergence < -0.05:
                    div_interpretation = "Revenue growing faster than receivables - improving quality"
                    div_risk = RiskLevel.LOW
                else:
                    div_interpretation = "Revenue and receivables growth aligned"
                    div_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Revenue-Receivables Growth Divergence",
                    value=growth_divergence,
                    interpretation=div_interpretation,
                    risk_level=div_risk,
                    methodology="A/R Growth - Revenue Growth"
                ))

        return results

    def _analyze_expense_quality(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze expense recognition quality"""
        results = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        revenue = income_statement.get('revenue', 0)
        operating_expenses = income_statement.get('operating_expenses', 0)
        sga_expense = income_statement.get('sga_expense', 0)
        depreciation = income_statement.get('depreciation', 0)

        if revenue <= 0:
            return results

        # Operating Expense Ratio Trend
        if comparative_data and len(comparative_data) >= 2:
            opex_ratios = []
            for past_statements in comparative_data:
                past_rev = past_statements.income_statement.get('revenue', 0)
                past_opex = past_statements.income_statement.get('operating_expenses', 0)
                if past_rev > 0:
                    opex_ratios.append(past_opex / past_rev)

            current_ratio = operating_expenses / revenue
            opex_ratios.append(current_ratio)

            if len(opex_ratios) >= 3:
                opex_trend = opex_ratios[-1] - opex_ratios[0]

                if opex_trend < -0.05:
                    opex_interpretation = "Improving operating efficiency - expenses declining as % of revenue"
                    opex_risk = RiskLevel.LOW
                elif opex_trend > 0.05:
                    opex_interpretation = "Operating efficiency declining - expenses rising as % of revenue"
                    opex_risk = RiskLevel.MODERATE
                else:
                    opex_interpretation = "Stable operating expense ratio"
                    opex_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Operating Expense Ratio Trend",
                    value=opex_trend,
                    interpretation=opex_interpretation,
                    risk_level=opex_risk,
                    methodology="Change in OpEx/Revenue over analysis period"
                ))

        # Depreciation vs CapEx Analysis
        capex = cash_flow.get('capex', 0)

        if depreciation > 0 and capex > 0:
            capex_to_dep = self.safe_divide(capex, depreciation)

            if capex_to_dep < 0.7:
                dep_interpretation = "CapEx below depreciation - may indicate understated depreciation or underinvestment"
                dep_risk = RiskLevel.MODERATE
            elif capex_to_dep > 2.0:
                dep_interpretation = "CapEx significantly exceeds depreciation - heavy investment or aggressive capitalization"
                dep_risk = RiskLevel.LOW
            else:
                dep_interpretation = "CapEx and depreciation reasonably aligned"
                dep_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="CapEx to Depreciation Ratio",
                value=capex_to_dep,
                interpretation=dep_interpretation,
                risk_level=dep_risk,
                methodology="Capital Expenditures / Depreciation Expense"
            ))

        # Accrued Expenses Analysis
        accrued_expenses = balance_sheet.get('accrued_expenses', 0)

        if comparative_data and len(comparative_data) > 0:
            prior_accrued = comparative_data[-1].balance_sheet.get('accrued_expenses', 0)

            if prior_accrued > 0 and accrued_expenses > 0:
                accrued_change = (accrued_expenses - prior_accrued) / prior_accrued

                if accrued_change < -0.2:
                    acc_interpretation = "Significant decrease in accrued expenses - may indicate expense acceleration"
                    acc_risk = RiskLevel.MODERATE
                elif accrued_change > 0.3:
                    acc_interpretation = "Large increase in accrued expenses - conservative or future obligation"
                    acc_risk = RiskLevel.LOW
                else:
                    acc_interpretation = "Stable accrued expense levels"
                    acc_risk = RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Accrued Expenses Change",
                    value=accrued_change,
                    interpretation=acc_interpretation,
                    risk_level=acc_risk,
                    methodology="(Current - Prior) / Prior Accrued Expenses"
                ))

        return results

    def _identify_red_flags(self, statements: FinancialStatements,
                            comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Identify red flags in financial reporting"""
        results = []
        red_flags = []

        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        cash_flow = statements.cash_flow
        notes = statements.notes

        net_income = income_statement.get('net_income', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        revenue = income_statement.get('revenue', 0)

        # 1. Persistent negative OCF with positive earnings
        if net_income > 0 and operating_cash_flow < 0:
            red_flags.append("Positive earnings with negative operating cash flow")

        # 2. Growing gap between earnings and cash flow
        if comparative_data and len(comparative_data) >= 2:
            current_gap = net_income - operating_cash_flow
            gaps = []
            for past in comparative_data:
                past_ni = past.income_statement.get('net_income', 0)
                past_ocf = past.cash_flow.get('operating_cash_flow', 0)
                gaps.append(past_ni - past_ocf)
            gaps.append(current_gap)

            if len(gaps) >= 3 and gaps[-1] > gaps[0] * 1.5 and gaps[-1] > 0:
                red_flags.append("Widening gap between earnings and operating cash flow")

        # 3. Revenue growing faster than industry/economy
        if comparative_data and len(comparative_data) > 0:
            prior_rev = comparative_data[-1].income_statement.get('revenue', 0)
            if prior_rev > 0:
                rev_growth = (revenue - prior_rev) / prior_rev
                if rev_growth > 0.5:
                    red_flags.append(f"Unusually high revenue growth ({rev_growth:.1%})")

        # 4. Declining cash conversion cycle efficiency
        # Would need working capital metrics

        # 5. Fourth quarter revenue surge
        quarterly_revenue = notes.get('q4_revenue_percent', 0)
        if quarterly_revenue > 0.35:
            red_flags.append(f"Q4 revenue concentration ({quarterly_revenue:.1%}) may indicate channel stuffing")

        # 6. Frequent changes in accounting policies
        policy_changes = notes.get('accounting_policy_changes', [])
        if len(policy_changes) >= 2:
            red_flags.append("Multiple accounting policy changes may obscure performance")

        # 7. Auditor changes
        auditor_change = notes.get('auditor_change', False)
        if auditor_change:
            red_flags.append("Recent auditor change warrants scrutiny")

        # 8. Related party transactions
        related_party = notes.get('related_party_transactions', 0)
        if related_party > revenue * 0.1:
            red_flags.append("Significant related party transactions")

        # 9. Unusual items or one-time gains
        unusual_items = income_statement.get('unusual_items', 0)
        if unusual_items > abs(net_income) * 0.2:
            red_flags.append("Large unusual items impacting earnings")

        # 10. Declining audit fees
        current_audit_fees = notes.get('audit_fees', 0)
        prior_audit_fees = notes.get('prior_audit_fees', 0)
        if prior_audit_fees > 0 and current_audit_fees < prior_audit_fees * 0.8:
            red_flags.append("Declining audit fees may indicate reduced audit scope")

        # Compile red flags into result
        if red_flags:
            flag_count = len(red_flags)

            if flag_count >= 4:
                overall_interpretation = "Multiple significant red flags identified - high scrutiny required"
                overall_risk = RiskLevel.HIGH
            elif flag_count >= 2:
                overall_interpretation = "Several red flags present - additional investigation recommended"
                overall_risk = RiskLevel.MODERATE
            else:
                overall_interpretation = "Minor red flags noted - routine monitoring"
                overall_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Red Flag Count",
                value=flag_count,
                interpretation=overall_interpretation,
                risk_level=overall_risk,
                limitations=red_flags,
                methodology="Qualitative assessment of 10+ warning indicators"
            ))
        else:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Red Flag Count",
                value=0,
                interpretation="No significant red flags identified",
                risk_level=RiskLevel.LOW,
                methodology="Qualitative assessment of 10+ warning indicators"
            ))

        return results

    def _assess_overall_quality(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Assess overall financial reporting quality"""
        results = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        net_income = income_statement.get('net_income', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        quality_score = 100  # Start with perfect score

        # 1. Cash flow confirmation (-20 points max)
        if net_income != 0:
            cfo_to_ni = self.safe_divide(operating_cash_flow, net_income)
            if cfo_to_ni < 0.5:
                quality_score -= 20
            elif cfo_to_ni < 0.8:
                quality_score -= 10
            elif cfo_to_ni < 1.0:
                quality_score -= 5

        # 2. Accrual quality (-15 points max)
        accruals = net_income - operating_cash_flow
        if total_assets > 0:
            accrual_ratio = abs(accruals / total_assets)
            if accrual_ratio > 0.25:
                quality_score -= 15
            elif accrual_ratio > 0.15:
                quality_score -= 10
            elif accrual_ratio > 0.10:
                quality_score -= 5

        # 3. Earnings persistence (-15 points max)
        if comparative_data and len(comparative_data) >= 2:
            ni_history = [s.income_statement.get('net_income', 0) for s in comparative_data]
            ni_history.append(net_income)

            if len(ni_history) >= 3:
                volatility = np.std(ni_history) / abs(np.mean(ni_history)) if np.mean(ni_history) != 0 else 1
                if volatility > 0.5:
                    quality_score -= 15
                elif volatility > 0.3:
                    quality_score -= 10
                elif volatility > 0.15:
                    quality_score -= 5

        # 4. Non-recurring items (-10 points max)
        non_recurring = abs(income_statement.get('non_recurring_items', 0))
        special_items = abs(income_statement.get('special_items', 0))
        if net_income != 0:
            non_recurring_ratio = (non_recurring + special_items) / abs(net_income)
            if non_recurring_ratio > 0.3:
                quality_score -= 10
            elif non_recurring_ratio > 0.15:
                quality_score -= 5

        # 5. Intangible concentration (-10 points max)
        intangibles = balance_sheet.get('intangible_assets', 0)
        goodwill = balance_sheet.get('goodwill', 0)
        if total_assets > 0:
            intangible_ratio = (intangibles + goodwill) / total_assets
            if intangible_ratio > 0.5:
                quality_score -= 10
            elif intangible_ratio > 0.3:
                quality_score -= 5

        # Ensure score is within bounds
        quality_score = max(0, min(100, quality_score))

        # Determine quality level
        if quality_score >= 85:
            quality_level = ReportingQualityLevel.HIGH_QUALITY
            quality_interpretation = "High financial reporting quality - sustainable and decision-useful"
        elif quality_score >= 70:
            quality_level = ReportingQualityLevel.SUSTAINABLE
            quality_interpretation = "Good reporting quality - earnings likely to persist"
        elif quality_score >= 55:
            quality_level = ReportingQualityLevel.DECISION_USEFUL
            quality_interpretation = "Adequate reporting quality - relevant but some concerns"
        else:
            quality_level = ReportingQualityLevel.GAAP_COMPLIANT
            quality_interpretation = "Low quality - meets standards but limited usefulness"

        quality_risk = RiskLevel.LOW if quality_score >= 70 else \
                       RiskLevel.MODERATE if quality_score >= 50 else RiskLevel.HIGH

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Overall Financial Reporting Quality Score",
            value=quality_score,
            interpretation=quality_interpretation,
            risk_level=quality_risk,
            methodology="Composite score based on cash flow confirmation, accruals, persistence, and transparency"
        ))

        return results

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key quality metrics"""

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        metrics = {}

        net_income = income_statement.get('net_income', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        total_assets = balance_sheet.get('total_assets', 0)
        capex = cash_flow.get('capex', 0)

        # Core quality metrics
        if net_income != 0:
            metrics['cfo_to_net_income'] = self.safe_divide(operating_cash_flow, net_income)
            metrics['fcf_to_net_income'] = self.safe_divide(operating_cash_flow - capex, net_income)

        if total_assets > 0:
            metrics['accrual_ratio'] = self.safe_divide(net_income - operating_cash_flow, total_assets)

            intangibles = balance_sheet.get('intangible_assets', 0)
            goodwill = balance_sheet.get('goodwill', 0)
            metrics['intangible_concentration'] = self.safe_divide(intangibles + goodwill, total_assets)

        # Receivables quality
        ar = balance_sheet.get('accounts_receivable', 0)
        revenue = income_statement.get('revenue', 0)
        if revenue > 0 and ar > 0:
            metrics['days_sales_outstanding'] = (ar / revenue) * 365

        return metrics

    def create_earnings_quality_assessment(self, statements: FinancialStatements,
                                           comparative_data: Optional[List[FinancialStatements]] = None) -> EarningsQualityAssessment:
        """Create comprehensive earnings quality assessment object"""

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        net_income = income_statement.get('net_income', 0)
        operating_cash_flow = cash_flow.get('operating_cash_flow', 0)
        total_assets = balance_sheet.get('total_assets', 0)
        capex = cash_flow.get('capex', 0)

        # Calculate metrics
        accrual_ratio = self.safe_divide(net_income - operating_cash_flow, total_assets) if total_assets > 0 else 0
        cfo_to_ni = self.safe_divide(operating_cash_flow, net_income) if net_income != 0 else 0
        fcf_to_ni = self.safe_divide(operating_cash_flow - capex, net_income) if net_income != 0 else 0
        cash_to_accrual = self.safe_divide(operating_cash_flow, net_income) if net_income != 0 else 0

        # Calculate persistence
        earnings_persistence = 0.5  # Default
        if comparative_data and len(comparative_data) >= 2:
            ni_history = [s.income_statement.get('net_income', 0) for s in comparative_data]
            ni_history.append(net_income)
            if len(ni_history) >= 3 and np.std(ni_history) > 0:
                try:
                    earnings_persistence = np.corrcoef(ni_history[:-1], ni_history[1:])[0, 1]
                except:
                    pass

        # Identify concerns
        concerns = []
        red_flags = []

        if cfo_to_ni < 0.5:
            concerns.append("Low cash flow relative to earnings")
        if abs(accrual_ratio) > 0.15:
            concerns.append("High accruals")
        if net_income > 0 and operating_cash_flow < 0:
            red_flags.append("Positive earnings with negative cash flow")

        # Determine quality level
        score = 100
        if cfo_to_ni < 0.5:
            score -= 30
        elif cfo_to_ni < 0.8:
            score -= 15
        if abs(accrual_ratio) > 0.15:
            score -= 20
        if earnings_persistence < 0.3:
            score -= 15

        if score >= 80:
            quality_level = EarningsQualityLevel.HIGH
        elif score >= 60:
            quality_level = EarningsQualityLevel.MODERATE
        elif score >= 40:
            quality_level = EarningsQualityLevel.LOW
        else:
            quality_level = EarningsQualityLevel.CONCERNING

        return EarningsQualityAssessment(
            quality_level=quality_level,
            overall_score=score,
            accrual_ratio=accrual_ratio,
            cash_to_accrual_ratio=cash_to_accrual,
            accrual_persistence=0.0,  # Would need more data
            earnings_persistence=earnings_persistence,
            revenue_quality=0.8,  # Placeholder
            expense_quality=0.8,  # Placeholder
            red_flags=red_flags,
            concerns=concerns,
            cfo_to_net_income=cfo_to_ni,
            fcf_to_net_income=fcf_to_ni
        )

    def calculate_beneish_m_score(self, statements: FinancialStatements,
                                  prior_statements: FinancialStatements) -> ManipulationIndicators:
        """Calculate Beneish M-Score for manipulation detection"""

        current = statements
        prior = prior_statements

        current_is = current.income_statement
        current_bs = current.balance_sheet
        prior_is = prior.income_statement
        prior_bs = prior.balance_sheet

        components = {}
        manipulation_flags = []

        # Calculate all components (similar to _analyze_manipulation_indicators)
        current_ar = current_bs.get('accounts_receivable', 0)
        prior_ar = prior_bs.get('accounts_receivable', 0)
        current_rev = current_is.get('revenue', 0)
        prior_rev = prior_is.get('revenue', 0)

        # DSRI
        if prior_rev > 0 and prior_ar > 0 and current_rev > 0:
            dsr_current = current_ar / current_rev * 365
            dsr_prior = prior_ar / prior_rev * 365
            dsri = dsr_current / dsr_prior if dsr_prior > 0 else 1
        else:
            dsri = 1

        # GMI
        current_gm = 1 - (current_is.get('cost_of_goods_sold', 0) / current_rev) if current_rev > 0 else 0
        prior_gm = 1 - (prior_is.get('cost_of_goods_sold', 0) / prior_rev) if prior_rev > 0 else 0
        gmi = prior_gm / current_gm if current_gm > 0 else 1

        # AQI
        current_assets = current_bs.get('total_assets', 0)
        prior_assets = prior_bs.get('total_assets', 0)
        current_ppe = current_bs.get('ppe_net', 0)
        prior_ppe = prior_bs.get('ppe_net', 0)
        current_ca = current_bs.get('current_assets', 0)
        prior_ca = prior_bs.get('current_assets', 0)

        if current_assets > 0 and prior_assets > 0:
            current_aqi_val = 1 - (current_ca + current_ppe) / current_assets
            prior_aqi_val = 1 - (prior_ca + prior_ppe) / prior_assets
            aqi = current_aqi_val / prior_aqi_val if prior_aqi_val > 0 else 1
        else:
            aqi = 1

        # SGI
        sgi = current_rev / prior_rev if prior_rev > 0 else 1

        # DEPI
        current_dep = current_is.get('depreciation', 0)
        prior_dep = prior_is.get('depreciation', 0)
        if (current_ppe + current_dep) > 0 and (prior_ppe + prior_dep) > 0:
            current_depi_val = current_dep / (current_ppe + current_dep)
            prior_depi_val = prior_dep / (prior_ppe + prior_dep)
            depi = prior_depi_val / current_depi_val if current_depi_val > 0 else 1
        else:
            depi = 1

        # SGAI
        current_sga = current_is.get('sga_expense', 0)
        prior_sga = prior_is.get('sga_expense', 0)
        if current_rev > 0 and prior_rev > 0:
            current_sgai_val = current_sga / current_rev
            prior_sgai_val = prior_sga / prior_rev
            sgai = current_sgai_val / prior_sgai_val if prior_sgai_val > 0 else 1
        else:
            sgai = 1

        # LVGI
        current_debt = current_bs.get('total_liabilities', 0)
        prior_debt = prior_bs.get('total_liabilities', 0)
        if current_assets > 0 and prior_assets > 0:
            current_lvg = current_debt / current_assets
            prior_lvg = prior_debt / prior_assets
            lvgi = current_lvg / prior_lvg if prior_lvg > 0 else 1
        else:
            lvgi = 1

        # TATA
        net_income = current_is.get('net_income', 0)
        cfo = current.cash_flow.get('operating_cash_flow', 0)
        accruals = net_income - cfo
        tata = accruals / current_assets if current_assets > 0 else 0

        # Calculate M-Score
        coef = self.beneish_coefficients
        m_score = (coef['intercept'] +
                   coef['dsri'] * dsri +
                   coef['gmi'] * gmi +
                   coef['aqi'] * aqi +
                   coef['sgi'] * sgi +
                   coef['depi'] * depi +
                   coef['sgai'] * sgai +
                   coef['lvgi'] * lvgi +
                   coef['tata'] * tata)

        # Determine risk level
        threshold = self.quality_thresholds['beneish_m_score']
        if m_score < threshold['low_risk']:
            risk_level = ManipulationRisk.LOW
            probability = 0.03
        elif m_score < threshold['moderate_risk']:
            risk_level = ManipulationRisk.MODERATE
            probability = 0.15
        elif m_score < threshold['elevated_risk']:
            risk_level = ManipulationRisk.ELEVATED
            probability = 0.40
        else:
            risk_level = ManipulationRisk.HIGH
            probability = 0.70

        # Identify flags
        if dsri > 1.1:
            manipulation_flags.append("High DSRI indicates disproportionate receivables growth")
        if gmi > 1.1:
            manipulation_flags.append("High GMI indicates declining gross margins")
        if aqi > 1.1:
            manipulation_flags.append("High AQI indicates deteriorating asset quality")
        if sgi > 1.5:
            manipulation_flags.append("Very high sales growth")
        if tata > 0.05:
            manipulation_flags.append("High total accruals relative to assets")

        return ManipulationIndicators(
            beneish_m_score=m_score,
            manipulation_probability=probability,
            risk_level=risk_level,
            days_sales_receivable_index=dsri,
            gross_margin_index=gmi,
            asset_quality_index=aqi,
            sales_growth_index=sgi,
            depreciation_index=depi,
            sga_expense_index=sgai,
            leverage_index=lvgi,
            total_accruals_to_assets=tata,
            manipulation_flags=manipulation_flags
        )
