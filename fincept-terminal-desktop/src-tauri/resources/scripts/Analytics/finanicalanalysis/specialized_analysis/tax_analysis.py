
"""
Financial Statement Tax Analysis Module
========================================

Tax analysis and optimization strategies

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


class TaxDifferenceType(Enum):
    """Types of tax differences"""
    TEMPORARY = "temporary"
    PERMANENT = "permanent"


class DeferredTaxType(Enum):
    """Types of deferred tax items"""
    ASSET = "deferred_tax_asset"
    LIABILITY = "deferred_tax_liability"


class TaxStrategy(Enum):
    """Tax strategy classification"""
    AGGRESSIVE = "aggressive"
    MODERATE = "moderate"
    CONSERVATIVE = "conservative"


@dataclass
class TaxRateAnalysis:
    """Comprehensive tax rate analysis"""
    statutory_tax_rate: float
    effective_tax_rate: float
    cash_tax_rate: float

    # Rate comparisons
    etr_vs_statutory: float
    cash_vs_effective: float

    # Tax efficiency metrics
    tax_efficiency_score: float
    rate_volatility: float = None

    # Explanatory factors
    rate_reconciliation: Dict[str, float] = field(default_factory=dict)
    permanent_differences: List[str] = field(default_factory=list)


@dataclass
class DeferredTaxAnalysis:
    """Deferred tax assets and liabilities analysis"""
    deferred_tax_assets: float
    deferred_tax_liabilities: float
    net_deferred_tax_position: float


    # Quality metrics
    valuation_allowance: float
    dta_realization_probability: float

    # Trend analysis
    net_position_trend: TrendDirection = TrendDirection.STABLE
    reversal_timeline: str = ""


@dataclass
class TaxPlanningAnalysis:
    """Tax planning and strategy analysis"""
    tax_strategy_classification: TaxStrategy

    # Planning indicators
    tax_optimization_score: float
    international_planning: bool


    # Risk assessment
    tax_audit_risk: RiskLevel
    compliance_quality: float
    uncertain_tax_positions: float = 0.0


class TaxAnalyzer(BaseAnalyzer):
    """
    Comprehensive income tax analyzer implementing CFA Institute standards.
    Covers tax rates, deferred taxes, and tax planning assessment.
    """

    def __init__(self, enable_logging: bool = True):
        super().__init__(enable_logging)
        self._initialize_tax_formulas()
        self._initialize_tax_benchmarks()

    def _initialize_tax_formulas(self):
        """Initialize tax-specific formulas"""
        self.formula_registry.update({
            'effective_tax_rate': lambda tax_expense, pretax_income: self.safe_divide(tax_expense, pretax_income),
            'cash_tax_rate': lambda cash_taxes_paid, pretax_income: self.safe_divide(cash_taxes_paid, pretax_income),
            'deferred_tax_ratio': lambda deferred_tax_expense, total_tax_expense: self.safe_divide(deferred_tax_expense,
                                                                                                   total_tax_expense),
            'dta_to_assets': lambda dta, total_assets: self.safe_divide(dta, total_assets),
            'dtl_to_assets': lambda dtl, total_assets: self.safe_divide(dtl, total_assets),
            'valuation_allowance_ratio': lambda allowance, gross_dta: self.safe_divide(allowance, gross_dta),
            'tax_shield_value': lambda interest_expense, tax_rate: interest_expense * tax_rate
        })

    def _initialize_tax_benchmarks(self):
        """Initialize tax-specific benchmarks"""
        # Typical tax rate ranges (country-dependent)
        self.tax_benchmarks = {
            'effective_tax_rate': {
                'us_corporate': {'low': 0.15, 'normal': 0.21, 'high': 0.35},
                'international': {'low': 0.10, 'normal': 0.25, 'high': 0.40},
                'general': {'low': 0.15, 'normal': 0.25, 'high': 0.35}
            },
            'etr_volatility': {'low': 0.05, 'moderate': 0.15, 'high': 0.30},
            'cash_vs_book_difference': {'minimal': 0.05, 'moderate': 0.15, 'significant': 0.30},
            'deferred_tax_ratio': {'low': 0.1, 'moderate': 0.3, 'high': 0.6}
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """
        Comprehensive income tax analysis

        Args:
            statements: Current period financial statements
            comparative_data: Historical financial statements for trend analysis
            industry_data: Industry benchmarks and peer data

        Returns:
            List of analysis results covering all tax aspects
        """
        results = []

        # Check if tax analysis is applicable
        pretax_income = statements.income_statement.get('pretax_income', 0)
        tax_expense = statements.income_statement.get('tax_expense', 0)

        if pretax_income == 0 and tax_expense == 0:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Tax Analysis",
                value=0.0,
                interpretation="No taxable income or tax expense - tax analysis limited",
                risk_level=RiskLevel.LOW,
                methodology="Income statement tax examination"
            ))
            return results

        # Tax rate analysis
        results.extend(self._analyze_tax_rates(statements, comparative_data, industry_data))

        # Deferred tax analysis
        results.extend(self._analyze_deferred_taxes(statements, comparative_data))

        # Tax reconciliation analysis
        results.extend(self._analyze_tax_reconciliation(statements))

        # Cash vs book tax analysis
        results.extend(self._analyze_cash_vs_book_taxes(statements, comparative_data))

        # Tax planning and strategy assessment
        results.extend(self._assess_tax_planning(statements, comparative_data))

        # International tax considerations
        results.extend(self._analyze_international_taxes(statements))

        return results

    def _analyze_tax_rates(self, statements: FinancialStatements,
                           comparative_data: Optional[List[FinancialStatements]] = None,
                           industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Analyze various tax rates and their implications"""
        results = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        pretax_income = income_statement.get('pretax_income', 0)
        tax_expense = income_statement.get('tax_expense', 0)
        cash_taxes_paid = cash_flow.get('cash_taxes_paid', 0)

        # Effective Tax Rate
        if pretax_income != 0:
            effective_tax_rate = self.safe_divide(tax_expense, pretax_income)

            # Get statutory rate for comparison
            statutory_rate = notes.get('statutory_tax_rate', 0.25)  # Default assumption
            country = statements.company_info.country.lower()

            # Country-specific statutory rates (simplified)
            if 'us' in country or 'united states' in country:
                statutory_rate = 0.21
            elif 'uk' in country or 'britain' in country:
                statutory_rate = 0.19
            elif any(eu_country in country for eu_country in ['germany', 'france', 'italy']):
                statutory_rate = 0.30

            etr_vs_statutory = effective_tax_rate - statutory_rate

            # Interpret effective tax rate
            if abs(etr_vs_statutory) < 0.05:
                etr_interpretation = f"Effective tax rate of {self.format_percentage(effective_tax_rate)} aligns with statutory rate"
                etr_risk = RiskLevel.LOW
            elif etr_vs_statutory < -0.10:
                etr_interpretation = f"Low effective tax rate of {self.format_percentage(effective_tax_rate)} indicates tax optimization strategies"
                etr_risk = RiskLevel.MODERATE
            elif etr_vs_statutory > 0.10:
                etr_interpretation = f"High effective tax rate of {self.format_percentage(effective_tax_rate)} above statutory rate"
                etr_risk = RiskLevel.MODERATE
            else:
                etr_interpretation = f"Effective tax rate of {self.format_percentage(effective_tax_rate)} shows moderate variance from statutory"
                etr_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Effective Tax Rate",
                value=effective_tax_rate,
                interpretation=etr_interpretation,
                risk_level=etr_risk,
                benchmark_comparison=self.compare_to_industry(effective_tax_rate, industry_data.get(
                    'effective_tax_rate') if industry_data else None),
                methodology="Income Tax Expense / Pretax Income",
                limitations=["Single period rate may not reflect ongoing tax burden"]
            ))

        # Cash Tax Rate
        if pretax_income != 0 and cash_taxes_paid > 0:
            cash_tax_rate = self.safe_divide(cash_taxes_paid, pretax_income)

            cash_vs_book_difference = abs(cash_tax_rate - effective_tax_rate) if 'effective_tax_rate' in locals() else 0

            if cash_vs_book_difference < 0.05:
                cash_interpretation = "Cash and book tax rates are closely aligned"
                cash_risk = RiskLevel.LOW
            elif cash_vs_book_difference < 0.15:
                cash_interpretation = "Moderate difference between cash and book tax rates"
                cash_risk = RiskLevel.LOW
            else:
                cash_interpretation = "Significant difference between cash and book tax rates - indicates timing differences"
                cash_risk = RiskLevel.MODERATE

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Cash Tax Rate",
                value=cash_tax_rate,
                interpretation=cash_interpretation,
                risk_level=cash_risk,
                methodology="Cash Taxes Paid / Pretax Income",
                limitations=["Cash taxes may include payments for prior years or be affected by timing"]
            ))

        # Tax Rate Volatility Analysis
        if comparative_data and len(comparative_data) >= 2:
            etr_values = []
            periods = []

            for i, past_statements in enumerate(comparative_data):
                past_pretax = past_statements.income_statement.get('pretax_income', 0)
                past_tax = past_statements.income_statement.get('tax_expense', 0)
                if past_pretax != 0:
                    past_etr = self.safe_divide(past_tax, past_pretax)
                    etr_values.append(past_etr)
                    periods.append(f"Period-{len(comparative_data) - i}")

            if 'effective_tax_rate' in locals():
                etr_values.append(effective_tax_rate)
                periods.append("Current")

            if len(etr_values) > 1:
                etr_volatility = np.std(etr_values)

                volatility_interpretation = "High tax rate volatility - inconsistent tax planning" if etr_volatility > 0.15 else "Moderate tax rate variation" if etr_volatility > 0.05 else "Stable effective tax rate"
                volatility_risk = RiskLevel.HIGH if etr_volatility > 0.20 else RiskLevel.MODERATE if etr_volatility > 0.10 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Tax Rate Volatility",
                    value=etr_volatility,
                    interpretation=volatility_interpretation,
                    risk_level=volatility_risk,
                    methodology="Standard deviation of effective tax rates over time"
                ))

        return results

    def _analyze_deferred_taxes(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze deferred tax assets and liabilities"""
        results = []

        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement
        notes = statements.notes

        deferred_tax_assets = balance_sheet.get('deferred_tax_asset', 0)
        deferred_tax_liabilities = balance_sheet.get('deferred_tax_liability', 0)
        deferred_tax_expense = income_statement.get('deferred_tax', 0)
        total_tax_expense = income_statement.get('tax_expense', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        # Net Deferred Tax Position
        net_deferred_position = deferred_tax_assets - deferred_tax_liabilities

        if abs(net_deferred_position) > 0:
            net_position_interpretation = f"Net deferred tax {'asset' if net_deferred_position > 0 else 'liability'} of ${abs(net_deferred_position):,.0f}"

            # Assess significance
            if total_assets > 0:
                net_position_ratio = abs(net_deferred_position) / total_assets
                if net_position_ratio > 0.05:
                    net_position_interpretation += " - significant deferred tax position"
                    position_risk = RiskLevel.MODERATE
                else:
                    net_position_interpretation += " - modest deferred tax position"
                    position_risk = RiskLevel.LOW
            else:
                position_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Net Deferred Tax Position",
                value=net_deferred_position,
                interpretation=net_position_interpretation,
                risk_level=position_risk,
                methodology="Deferred Tax Assets - Deferred Tax Liabilities"
            ))

        # Deferred Tax Assets Analysis
        if deferred_tax_assets > 0:
            valuation_allowance = notes.get('dta_valuation_allowance', 0)

            if total_assets > 0:
                dta_ratio = self.safe_divide(deferred_tax_assets, total_assets)

                dta_interpretation = "Significant deferred tax assets requiring realization assessment" if dta_ratio > 0.10 else "Moderate deferred tax assets" if dta_ratio > 0.05 else "Limited deferred tax assets"
                dta_risk = RiskLevel.MODERATE if dta_ratio > 0.15 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="DTA to Total Assets",
                    value=dta_ratio,
                    interpretation=dta_interpretation,
                    risk_level=dta_risk,
                    methodology="Deferred Tax Assets / Total Assets",
                    limitations=["DTA realization depends on future taxable income"]
                ))

            # Valuation Allowance Analysis
            if valuation_allowance > 0:
                allowance_ratio = self.safe_divide(valuation_allowance, deferred_tax_assets + valuation_allowance)

                allowance_interpretation = "High valuation allowance indicates uncertainty about DTA realization" if allowance_ratio > 0.5 else "Moderate valuation allowance" if allowance_ratio > 0.2 else "Low valuation allowance suggests confident DTA realization"
                allowance_risk = RiskLevel.HIGH if allowance_ratio > 0.6 else RiskLevel.MODERATE if allowance_ratio > 0.3 else RiskLevel.LOW

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="DTA Valuation Allowance Ratio",
                    value=allowance_ratio,
                    interpretation=allowance_interpretation,
                    risk_level=allowance_risk,
                    methodology="Valuation Allowance / (DTA + Valuation Allowance)",
                    limitations=["High allowance may indicate poor earnings prospects"]
                ))

        # Deferred Tax Liabilities Analysis
        if deferred_tax_liabilities > 0 and total_assets > 0:
            dtl_ratio = self.safe_divide(deferred_tax_liabilities, total_assets)

            dtl_interpretation = "Significant deferred tax liabilities from temporary differences" if dtl_ratio > 0.10 else "Moderate deferred tax liabilities" if dtl_ratio > 0.05 else "Limited deferred tax liabilities"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="DTL to Total Assets",
                value=dtl_ratio,
                interpretation=dtl_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Deferred Tax Liabilities / Total Assets",
                limitations=["DTL reversal timing depends on asset usage and accounting methods"]
            ))

        # Deferred Tax Expense Analysis
        if total_tax_expense != 0 and abs(deferred_tax_expense) > 0:
            deferred_ratio = self.safe_divide(abs(deferred_tax_expense), abs(total_tax_expense))

            deferred_interpretation = "Significant deferred tax component in total tax expense" if deferred_ratio > 0.3 else "Moderate deferred tax impact" if deferred_ratio > 0.15 else "Limited deferred tax expense component"
            deferred_risk = RiskLevel.MODERATE if deferred_ratio > 0.5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Deferred Tax Expense Ratio",
                value=deferred_ratio,
                interpretation=deferred_interpretation,
                risk_level=deferred_risk,
                methodology="|Deferred Tax Expense| / |Total Tax Expense|"
            ))

        return results

    def _analyze_tax_reconciliation(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Analyze tax rate reconciliation and permanent differences"""
        results = []

        income_statement = statements.income_statement
        notes = statements.notes

        pretax_income = income_statement.get('pretax_income', 0)
        tax_expense = income_statement.get('tax_expense', 0)

        if pretax_income == 0:
            return results

        # Look for tax reconciliation items in notes
        reconciliation_items = {
            'statutory_rate_effect': notes.get('statutory_rate_effect', 0),
            'permanent_differences': notes.get('permanent_differences', 0),
            'tax_credits': notes.get('tax_credits', 0),
            'foreign_rate_differences': notes.get('foreign_rate_differences', 0),
            'prior_year_adjustments': notes.get('prior_year_tax_adjustments', 0),
            'other_tax_effects': notes.get('other_tax_effects', 0)
        }

        total_reconciling_items = sum(abs(value) for value in reconciliation_items.values() if value != 0)

        if total_reconciling_items > 0:
            # Analyze significant reconciling items
            for item_name, item_value in reconciliation_items.items():
                if abs(item_value) > abs(tax_expense) * 0.05:  # 5% threshold
                    item_display_name = item_name.replace('_', ' ').title()
                    item_impact = self.safe_divide(item_value, tax_expense) if tax_expense != 0 else 0

                    item_interpretation = f"{item_display_name} {'increases' if item_value > 0 else 'decreases'} tax expense by {self.format_percentage(abs(item_impact))}"

                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name=f"Tax Reconciliation - {item_display_name}",
                        value=item_impact,
                        interpretation=item_interpretation,
                        risk_level=RiskLevel.LOW,
                        methodology="Reconciliation item impact on total tax expense"
                    ))

        # Overall reconciliation quality
        effective_tax_rate = self.safe_divide(tax_expense, pretax_income)
        statutory_rate = notes.get('statutory_tax_rate', 0.25)

        reconciliation_explained = self.safe_divide(total_reconciling_items,
                                                    abs(effective_tax_rate - statutory_rate) * abs(
                                                        pretax_income)) if abs(
            effective_tax_rate - statutory_rate) * abs(pretax_income) > 0 else 0

        if reconciliation_explained > 0.8:
            reconciliation_quality = "Comprehensive tax rate reconciliation provided"
            reconciliation_risk = RiskLevel.LOW
        elif reconciliation_explained > 0.5:
            reconciliation_quality = "Adequate tax rate reconciliation"
            reconciliation_risk = RiskLevel.LOW
        else:
            reconciliation_quality = "Limited tax rate reconciliation disclosure"
            reconciliation_risk = RiskLevel.MODERATE

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Tax Reconciliation Quality",
            value=reconciliation_explained,
            interpretation=reconciliation_quality,
            risk_level=reconciliation_risk,
            methodology="Assessment of tax rate reconciliation completeness"
        ))

        return results

    def _analyze_cash_vs_book_taxes(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Analyze differences between cash and book tax amounts"""
        results = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow

        tax_expense = income_statement.get('tax_expense', 0)
        cash_taxes_paid = cash_flow.get('cash_taxes_paid', 0)
        current_tax_expense = income_statement.get('current_tax_expense',
                                                   tax_expense)  # Fallback to total if not separated
        deferred_tax_expense = income_statement.get('deferred_tax', 0)

        # Cash vs Current Tax Expense
        if current_tax_expense != 0 and cash_taxes_paid > 0:
            cash_vs_current = cash_taxes_paid - current_tax_expense
            cash_timing_ratio = self.safe_divide(abs(cash_vs_current), abs(current_tax_expense))

            if abs(cash_vs_current) < abs(current_tax_expense) * 0.1:
                timing_interpretation = "Cash and current tax expense are closely aligned"
                timing_risk = RiskLevel.LOW
            elif cash_vs_current > 0:
                timing_interpretation = f"Cash taxes exceed current expense by {self.format_percentage(cash_timing_ratio)} - paying for prior years or prepaying"
                timing_risk = RiskLevel.LOW
            else:
                timing_interpretation = f"Current tax expense exceeds cash payments by {self.format_percentage(cash_timing_ratio)} - timing differences or accruals"
                timing_risk = RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Cash vs Current Tax Timing",
                value=cash_timing_ratio,
                interpretation=timing_interpretation,
                risk_level=timing_risk,
                methodology="(Cash Taxes Paid - Current Tax Expense) / Current Tax Expense"
            ))

        # Tax Cash Flow Quality
        if comparative_data and len(comparative_data) >= 2:
            cash_tax_values = []
            book_tax_values = []

            for past_statements in comparative_data:
                past_cash_tax = past_statements.cash_flow.get('cash_taxes_paid', 0)
                past_book_tax = past_statements.income_statement.get('tax_expense', 0)
                if past_cash_tax > 0 and past_book_tax != 0:
                    cash_tax_values.append(past_cash_tax)
                    book_tax_values.append(past_book_tax)

            if cash_taxes_paid > 0 and tax_expense != 0:
                cash_tax_values.append(cash_taxes_paid)
                book_tax_values.append(tax_expense)

            if len(cash_tax_values) > 1 and len(book_tax_values) > 1:
                # Calculate correlation between cash and book taxes
                correlation = np.corrcoef(cash_tax_values, book_tax_values)[0, 1] if len(cash_tax_values) == len(
                    book_tax_values) else 0

                correlation_interpretation = "Strong correlation between cash and book taxes" if correlation > 0.8 else "Moderate correlation" if correlation > 0.5 else "Weak correlation - significant timing differences"
                correlation_risk = RiskLevel.LOW if correlation > 0.7 else RiskLevel.MODERATE if correlation > 0.4 else RiskLevel.HIGH

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Cash-Book Tax Correlation",
                    value=correlation,
                    interpretation=correlation_interpretation,
                    risk_level=correlation_risk,
                    methodology="Correlation coefficient between cash and book tax amounts over time"
                ))

        return results

    def _assess_tax_planning(self, statements: FinancialStatements,
                             comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Assess tax planning effectiveness and strategy"""
        results = []

        income_statement = statements.income_statement
        notes = statements.notes

        pretax_income = income_statement.get('pretax_income', 0)
        tax_expense = income_statement.get('tax_expense', 0)

        if pretax_income == 0:
            return results

        effective_tax_rate = self.safe_divide(tax_expense, pretax_income)
        statutory_rate = notes.get('statutory_tax_rate', 0.25)

        # Tax Efficiency Assessment
        tax_savings = (statutory_rate - effective_tax_rate) * pretax_income if pretax_income > 0 else 0
        tax_efficiency_ratio = self.safe_divide(tax_savings,
                                                abs(pretax_income * statutory_rate)) if statutory_rate > 0 else 0

        if tax_efficiency_ratio > 0.2:
            efficiency_interpretation = "High tax efficiency - significant tax optimization achieved"
            efficiency_risk = RiskLevel.MODERATE  # Could indicate aggressive strategies
            strategy_classification = TaxStrategy.AGGRESSIVE
        elif tax_efficiency_ratio > 0.05:
            efficiency_interpretation = "Moderate tax efficiency - some optimization strategies employed"
            efficiency_risk = RiskLevel.LOW
            strategy_classification = TaxStrategy.MODERATE
        elif tax_efficiency_ratio > -0.05:
            efficiency_interpretation = "Standard tax efficiency - limited optimization"
            efficiency_risk = RiskLevel.LOW
            strategy_classification = TaxStrategy.CONSERVATIVE
        else:
            efficiency_interpretation = "Below-average tax efficiency - potential for improvement"
            efficiency_risk = RiskLevel.MODERATE
            strategy_classification = TaxStrategy.CONSERVATIVE

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Tax Planning Efficiency",
            value=tax_efficiency_ratio,
            interpretation=efficiency_interpretation,
            risk_level=efficiency_risk,
            methodology="(Statutory Rate - Effective Rate) × Pretax Income / (Statutory Rate × Pretax Income)"
        ))

        # Uncertain Tax Positions
        uncertain_tax_positions = notes.get('uncertain_tax_positions', 0)
        if uncertain_tax_positions > 0:
            utp_ratio = self.safe_divide(uncertain_tax_positions, abs(tax_expense)) if tax_expense != 0 else 0

            utp_interpretation = "Significant uncertain tax positions indicate aggressive tax strategies" if utp_ratio > 0.5 else "Moderate uncertain tax positions" if utp_ratio > 0.2 else "Limited uncertain tax positions"
            utp_risk = RiskLevel.HIGH if utp_ratio > 0.8 else RiskLevel.MODERATE if utp_ratio > 0.3 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Uncertain Tax Positions",
                value=utp_ratio,
                interpretation=utp_interpretation,
                risk_level=utp_risk,
                methodology="Uncertain Tax Positions / Total Tax Expense",
                limitations=["High UTP may indicate potential tax adjustments"]
            ))

        # Tax Strategy Consistency
        if comparative_data and len(comparative_data) >= 2:
            etr_values = []
            for past_statements in comparative_data:
                past_pretax = past_statements.income_statement.get('pretax_income', 0)
                past_tax = past_statements.income_statement.get('tax_expense', 0)
                if past_pretax != 0:
                    past_etr = self.safe_divide(past_tax, past_pretax)
                    etr_values.append(past_etr)

            etr_values.append(effective_tax_rate)

            if len(etr_values) > 1:
                etr_consistency = 1 - (np.std(etr_values) / np.mean(etr_values)) if np.mean(etr_values) > 0 else 0

                consistency_interpretation = "Highly consistent tax strategy" if etr_consistency > 0.8 else "Moderately consistent tax approach" if etr_consistency > 0.6 else "Inconsistent tax strategy - may indicate changing circumstances or aggressive planning"
                consistency_risk = RiskLevel.LOW if etr_consistency > 0.7 else RiskLevel.MODERATE if etr_consistency > 0.5 else RiskLevel.HIGH

                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Tax Strategy Consistency",
                    value=etr_consistency,
                    interpretation=consistency_interpretation,
                    risk_level=consistency_risk,
                    methodology="1 - (Standard Deviation of ETR / Mean ETR)"
                ))

        return results

    def _analyze_international_taxes(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Analyze international tax considerations"""
        results = []

        notes = statements.notes
        income_statement = statements.income_statement

        # Foreign tax considerations
        foreign_tax_credit = notes.get('foreign_tax_credit', 0)
        foreign_income = notes.get('foreign_pretax_income', 0)
        domestic_income = notes.get('domestic_pretax_income', 0)

        total_pretax = income_statement.get('pretax_income', 0)

        # Geographic income analysis
        if foreign_income > 0 and total_pretax > 0:
            foreign_income_ratio = self.safe_divide(foreign_income, total_pretax)

            geographic_interpretation = "Significant international operations" if foreign_income_ratio > 0.3 else "Moderate international presence" if foreign_income_ratio > 0.1 else "Primarily domestic operations"
            geographic_risk = RiskLevel.MODERATE if foreign_income_ratio > 0.5 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="International Income Exposure",
                value=foreign_income_ratio,
                interpretation=geographic_interpretation,
                risk_level=geographic_risk,
                methodology="Foreign Pretax Income / Total Pretax Income",
                limitations=["International operations create additional tax complexity"]
            ))

        # Foreign tax credit utilization
        if foreign_tax_credit > 0:
            total_tax_expense = income_statement.get('tax_expense', 0)
            ftc_ratio = self.safe_divide(foreign_tax_credit, abs(total_tax_expense)) if total_tax_expense != 0 else 0

            ftc_interpretation = "Significant foreign tax credit utilization" if ftc_ratio > 0.2 else "Moderate foreign tax credits" if ftc_ratio > 0.1 else "Limited foreign tax credit usage"

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Foreign Tax Credit Utilization",
                value=ftc_ratio,
                interpretation=ftc_interpretation,
                risk_level=RiskLevel.LOW,
                methodology="Foreign Tax Credits / Total Tax Expense"
            ))

        # Transfer pricing considerations
        related_party_transactions = notes.get('related_party_revenue', 0) + notes.get('related_party_expenses', 0)
        revenue = income_statement.get('revenue', 0)

        if related_party_transactions > 0 and revenue > 0:
            transfer_pricing_exposure = self.safe_divide(related_party_transactions, revenue)

            tp_interpretation = "High transfer pricing exposure - significant related party transactions" if transfer_pricing_exposure > 0.3 else "Moderate transfer pricing considerations" if transfer_pricing_exposure > 0.1 else "Limited transfer pricing exposure"
            tp_risk = RiskLevel.HIGH if transfer_pricing_exposure > 0.5 else RiskLevel.MODERATE if transfer_pricing_exposure > 0.2 else RiskLevel.LOW

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Transfer Pricing Exposure",
                value=transfer_pricing_exposure,
                interpretation=tp_interpretation,
                risk_level=tp_risk,
                methodology="Related Party Transactions / Revenue",
                limitations=["High exposure increases tax audit and adjustment risk"]
            ))

        return results

    def get_key_metrics(self, statements: FinancialStatements) -> Dict[str, float]:
        """Return key tax metrics"""

        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        cash_flow = statements.cash_flow

        metrics = {}

        # Core tax metrics
        pretax_income = income_statement.get('pretax_income', 0)
        tax_expense = income_statement.get('tax_expense', 0)
        cash_taxes_paid = cash_flow.get('cash_taxes_paid', 0)

        if pretax_income != 0:
            metrics['effective_tax_rate'] = self.safe_divide(tax_expense, pretax_income)

            if cash_taxes_paid > 0:
                metrics['cash_tax_rate'] = self.safe_divide(cash_taxes_paid, pretax_income)

        # Deferred tax metrics
        deferred_tax_assets = balance_sheet.get('deferred_tax_asset', 0)
        deferred_tax_liabilities = balance_sheet.get('deferred_tax_liability', 0)
        total_assets = balance_sheet.get('total_assets', 0)

        metrics['net_deferred_tax_position'] = deferred_tax_assets - deferred_tax_liabilities

        if total_assets > 0:
            metrics['dta_to_assets'] = self.safe_divide(deferred_tax_assets, total_assets)
            metrics['dtl_to_assets'] = self.safe_divide(deferred_tax_liabilities, total_assets)

        # Tax expense components
        current_tax_expense = income_statement.get('current_tax_expense', 0)
        deferred_tax_expense = income_statement.get('deferred_tax', 0)

        if tax_expense != 0:
            metrics['current_tax_ratio'] = self.safe_divide(current_tax_expense, tax_expense)
            metrics['deferred_tax_ratio'] = self.safe_divide(deferred_tax_expense, tax_expense)

        return metrics

    def create_tax_rate_analysis(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None) -> TaxRateAnalysis:
        """Create comprehensive tax rate analysis object"""

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        pretax_income = income_statement.get('pretax_income', 0)
        tax_expense = income_statement.get('tax_expense', 0)
        cash_taxes_paid = cash_flow.get('cash_taxes_paid', 0)

        # Calculate tax rates
        effective_tax_rate = self.safe_divide(tax_expense, pretax_income) if pretax_income != 0 else 0
        cash_tax_rate = self.safe_divide(cash_taxes_paid,
                                         pretax_income) if pretax_income != 0 and cash_taxes_paid > 0 else 0
        statutory_tax_rate = notes.get('statutory_tax_rate', 0.25)

        # Rate comparisons
        etr_vs_statutory = effective_tax_rate - statutory_tax_rate
        cash_vs_effective = cash_tax_rate - effective_tax_rate

        # Tax efficiency calculation
        if statutory_tax_rate > 0 and pretax_income > 0:
            tax_savings = (statutory_tax_rate - effective_tax_rate) * pretax_income
            potential_tax = statutory_tax_rate * pretax_income
            tax_efficiency_score = self.safe_divide(tax_savings, abs(potential_tax)) * 100
        else:
            tax_efficiency_score = 0

        # Rate volatility
        rate_volatility = None
        if comparative_data and len(comparative_data) >= 2:
            etr_values = []
            for past_statements in comparative_data:
                past_pretax = past_statements.income_statement.get('pretax_income', 0)
                past_tax = past_statements.income_statement.get('tax_expense', 0)
                if past_pretax != 0:
                    past_etr = self.safe_divide(past_tax, past_pretax)
                    etr_values.append(past_etr)

            etr_values.append(effective_tax_rate)
            if len(etr_values) > 1:
                rate_volatility = np.std(etr_values)

        # Rate reconciliation
        rate_reconciliation = {
            'statutory_rate_effect': notes.get('statutory_rate_effect', 0),
            'permanent_differences': notes.get('permanent_differences', 0),
            'tax_credits': notes.get('tax_credits', 0),
            'foreign_rate_differences': notes.get('foreign_rate_differences', 0)
        }

        # Permanent differences identification
        permanent_differences = []
        if notes.get('municipal_bond_interest', 0) > 0:
            permanent_differences.append("Tax-exempt interest income")
        if notes.get('meals_entertainment', 0) > 0:
            permanent_differences.append("Non-deductible meals and entertainment")
        if notes.get('life_insurance_proceeds', 0) > 0:
            permanent_differences.append("Life insurance proceeds")

        return TaxRateAnalysis(
            statutory_tax_rate=statutory_tax_rate,
            effective_tax_rate=effective_tax_rate,
            cash_tax_rate=cash_tax_rate,
            etr_vs_statutory=etr_vs_statutory,
            cash_vs_effective=cash_vs_effective,
            tax_efficiency_score=tax_efficiency_score,
            rate_volatility=rate_volatility,
            rate_reconciliation=rate_reconciliation,
            permanent_differences=permanent_differences
        )

    def create_deferred_tax_analysis(self, statements: FinancialStatements,
                                     comparative_data: Optional[
                                         List[FinancialStatements]] = None) -> DeferredTaxAnalysis:
        """Create comprehensive deferred tax analysis object"""

        balance_sheet = statements.balance_sheet
        notes = statements.notes

        deferred_tax_assets = balance_sheet.get('deferred_tax_asset', 0)
        deferred_tax_liabilities = balance_sheet.get('deferred_tax_liability', 0)
        net_deferred_tax_position = deferred_tax_assets - deferred_tax_liabilities

        # Composition analysis
        dta_composition = {
            'nol_carryforwards': notes.get('nol_dta', 0),
            'depreciation_differences': notes.get('depreciation_dta', 0),
            'accrued_expenses': notes.get('accrual_dta', 0),
            'other': notes.get('other_dta', 0)
        }

        dtl_composition = {
            'depreciation_differences': notes.get('depreciation_dtl', 0),
            'intangible_amortization': notes.get('intangible_dtl', 0),
            'other': notes.get('other_dtl', 0)
        }

        # Valuation allowance and realization
        valuation_allowance = notes.get('dta_valuation_allowance', 0)
        gross_dta = deferred_tax_assets + valuation_allowance
        dta_realization_probability = self.safe_divide(deferred_tax_assets, gross_dta) if gross_dta > 0 else 1.0

        # Trend analysis
        net_position_trend = TrendDirection.STABLE
        if comparative_data and len(comparative_data) >= 2:
            net_position_values = []
            for past_statements in comparative_data:
                past_dta = past_statements.balance_sheet.get('deferred_tax_asset', 0)
                past_dtl = past_statements.balance_sheet.get('deferred_tax_liability', 0)
                net_position_values.append(past_dta - past_dtl)

            net_position_values.append(net_deferred_tax_position)

            if len(net_position_values) >= 3:
                if net_position_values[-1] > net_position_values[0] * 1.1:
                    net_position_trend = TrendDirection.IMPROVING
                elif net_position_values[-1] < net_position_values[0] * 0.9:
                    net_position_trend = TrendDirection.DETERIORATING

        # Reversal timeline assessment
        reversal_timeline = "Long-term reversal expected" if abs(
            net_deferred_tax_position) > 0 else "No significant deferred taxes"

        return DeferredTaxAnalysis(
            deferred_tax_assets=deferred_tax_assets,
            deferred_tax_liabilities=deferred_tax_liabilities,
            net_deferred_tax_position=net_deferred_tax_position,
            dta_composition=dta_composition,
            dtl_composition=dtl_composition,
            valuation_allowance=valuation_allowance,
            dta_realization_probability=dta_realization_probability,
            net_position_trend=net_position_trend,
            reversal_timeline=reversal_timeline
        )

    def assess_tax_planning_strategy(self, statements: FinancialStatements,
                                     comparative_data: Optional[
                                         List[FinancialStatements]] = None) -> TaxPlanningAnalysis:
        """Assess tax planning effectiveness and strategy"""

        income_statement = statements.income_statement
        notes = statements.notes

        pretax_income = income_statement.get('pretax_income', 0)
        tax_expense = income_statement.get('tax_expense', 0)
        effective_tax_rate = self.safe_divide(tax_expense, pretax_income) if pretax_income != 0 else 0
        statutory_rate = notes.get('statutory_tax_rate', 0.25)

        # Strategy classification
        tax_savings_rate = (statutory_rate - effective_tax_rate) / statutory_rate if statutory_rate > 0 else 0

        if tax_savings_rate > 0.25:
            tax_strategy_classification = TaxStrategy.AGGRESSIVE
        elif tax_savings_rate > 0.10:
            tax_strategy_classification = TaxStrategy.MODERATE
        else:
            tax_strategy_classification = TaxStrategy.CONSERVATIVE

        # Tax optimization score
        tax_optimization_score = min(100, max(0, tax_savings_rate * 100))

        # International planning assessment
        foreign_income = notes.get('foreign_pretax_income', 0)
        international_planning = foreign_income > 0

        # Timing strategies identification
        timing_strategies = []
        if notes.get('installment_sales', 0) > 0:
            timing_strategies.append("Installment sale deferrals")
        if notes.get('like_kind_exchanges', 0) > 0:
            timing_strategies.append("Like-kind exchanges")
        if notes.get('depreciation_elections', 0) > 0:
            timing_strategies.append("Accelerated depreciation elections")

        # Risk assessment
        uncertain_tax_positions = notes.get('uncertain_tax_positions', 0)
        if uncertain_tax_positions > abs(tax_expense) * 0.5:
            tax_audit_risk = RiskLevel.HIGH
        elif uncertain_tax_positions > abs(tax_expense) * 0.2:
            tax_audit_risk = RiskLevel.MODERATE
        else:
            tax_audit_risk = RiskLevel.LOW

        # Compliance quality
        compliance_quality = 100
        if notes.get('tax_penalties', 0) > 0:
            compliance_quality -= 20
        if notes.get('prior_year_adjustments', 0) > abs(tax_expense) * 0.1:
            compliance_quality -= 15

        compliance_quality = max(0, compliance_quality)

        return TaxPlanningAnalysis(
            tax_strategy_classification=tax_strategy_classification,
            tax_optimization_score=tax_optimization_score,
            international_planning=international_planning,
            timing_strategies=timing_strategies,
            tax_audit_risk=tax_audit_risk,
            compliance_quality=compliance_quality,
            uncertain_tax_positions=uncertain_tax_positions
        )