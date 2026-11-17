
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

    # Composition (note disclosures)
    dta_composition: Dict[str, float] = field(default_factory=dict)
    dtl_composition: Dict[str, float] = field(default_factory=dict)


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
    timing_strategies: List[str] = field(default_factory=list)


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
        """Comprehensive income tax analysis"""
        results = []
        income = statements.income_statement
        pretax_income = income.get('pretax_income')
        tax_expense = income.get('tax_expense')

        if pretax_income is None or tax_expense is None:
            missing = [n for n, v in (('pre-tax income', pretax_income), ('tax expense', tax_expense)) if v is None]
            results.append(self.unavailable(AnalysisType.QUALITY, "Tax Analysis",
                                            f"{' and '.join(missing)} not reported",
                                            "Income statement tax examination"))
            return results
        if pretax_income == 0 and tax_expense == 0:
            results.append(self.unavailable(AnalysisType.QUALITY, "Tax Analysis",
                                            "pre-tax income and tax expense are both zero",
                                            "Income statement tax examination"))
            return results

        results.extend(self._analyze_tax_rates(statements, comparative_data, industry_data))
        results.extend(self._analyze_deferred_taxes(statements, comparative_data))
        results.extend(self._analyze_tax_reconciliation(statements))
        results.extend(self._analyze_cash_vs_book_taxes(statements, comparative_data))
        results.extend(self._assess_tax_planning(statements, comparative_data))
        results.extend(self._analyze_international_taxes(statements))
        return results

    def _analyze_tax_rates(self, statements: FinancialStatements,
                           comparative_data: Optional[List[FinancialStatements]] = None,
                           industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Effective, cash and statutory tax rates"""
        results = []
        income_statement = statements.income_statement
        pretax_income = income_statement['pretax_income']
        tax_expense = income_statement['tax_expense']
        cash_taxes_paid = statements.cash_flow.get('cash_taxes_paid')
        statutory_rate, statutory_source = self._statutory_rate(statements)

        effective_tax_rate = self._etr(statements)
        if effective_tax_rate is None:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Effective Tax Rate",
                value=None,
                interpretation=(f"Not meaningful with a pre-tax loss (pre-tax {pretax_income:,.2f}, tax "
                                f"{tax_expense:,.2f}: " + ("a tax benefit was recognised" if tax_expense < 0
                                                          else "tax expense despite the loss - non-deductible items or valuation allowance") + ")"),
                risk_level=RiskLevel.MODERATE if tax_expense > 0 else RiskLevel.LOW,
                methodology="Income Tax Expense / Pretax Income (defined for positive pre-tax income)"
            ))
        else:
            if statutory_rate is None:
                etr_interpretation = (f"Effective tax rate of {self.format_percentage(effective_tax_rate)} "
                                      f"(not compared: {statutory_source})")
                etr_risk = RiskLevel.LOW
            else:
                etr_vs_statutory = effective_tax_rate - statutory_rate
                ref = f"statutory {self.format_percentage(statutory_rate)} ({statutory_source})"
                if abs(etr_vs_statutory) < 0.05:
                    etr_interpretation, etr_risk = f"Effective tax rate of {self.format_percentage(effective_tax_rate)} aligns with {ref}", RiskLevel.LOW
                elif etr_vs_statutory < -0.10:
                    etr_interpretation, etr_risk = f"Effective tax rate of {self.format_percentage(effective_tax_rate)} well below {ref} - tax optimization, credits or loss carry-forwards", RiskLevel.MODERATE
                elif etr_vs_statutory > 0.10:
                    etr_interpretation, etr_risk = f"Effective tax rate of {self.format_percentage(effective_tax_rate)} well above {ref}", RiskLevel.MODERATE
                else:
                    etr_interpretation, etr_risk = f"Effective tax rate of {self.format_percentage(effective_tax_rate)} shows moderate variance from {ref}", RiskLevel.LOW
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

        if cash_taxes_paid is not None and pretax_income > 0:
            cash_tax_rate = cash_taxes_paid / pretax_income
            if effective_tax_rate is not None:
                diff = abs(cash_tax_rate - effective_tax_rate)
                if diff < 0.05:
                    cash_interpretation, cash_risk = "Cash and book tax rates are closely aligned", RiskLevel.LOW
                elif diff < 0.15:
                    cash_interpretation, cash_risk = "Moderate difference between cash and book tax rates", RiskLevel.LOW
                else:
                    cash_interpretation, cash_risk = "Significant difference between cash and book tax rates - indicates timing differences", RiskLevel.MODERATE
            else:
                cash_interpretation, cash_risk = "Cash taxes paid relative to pre-tax income", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Cash Tax Rate",
                value=cash_tax_rate,
                interpretation=cash_interpretation,
                risk_level=cash_risk,
                methodology="Cash Taxes Paid / Pretax Income",
                limitations=["Cash taxes may include payments for prior years or be affected by timing"]
            ))

        if comparative_data and len(comparative_data) >= 2 and effective_tax_rate is not None:
            etr_values = [e for e in (self._etr(p) for p in comparative_data) if e is not None]
            etr_values.append(effective_tax_rate)
            if len(etr_values) >= 3:
                etr_volatility = float(np.std(etr_values))
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Tax Rate Volatility",
                    value=etr_volatility,
                    interpretation=("High tax rate volatility - inconsistent tax position" if etr_volatility > 0.15
                                    else "Moderate tax rate variation" if etr_volatility > 0.05 else "Stable effective tax rate"),
                    risk_level=RiskLevel.HIGH if etr_volatility > 0.20 else RiskLevel.MODERATE if etr_volatility > 0.10 else RiskLevel.LOW,
                    methodology=f"Standard deviation of effective tax rates over {len(etr_values)} profitable periods"
                ))

        return results

    def _analyze_deferred_taxes(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Deferred tax assets and liabilities"""
        results = []
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        dta = balance_sheet.get('deferred_tax_asset')
        dtl = balance_sheet.get('deferred_tax_liability')
        total_assets = balance_sheet.get('total_assets')
        total_tax_expense = income_statement.get('tax_expense')
        deferred_tax_expense = income_statement.get('deferred_tax_expense', statements.cash_flow.get('deferred_tax'))

        if dta is not None or dtl is not None:
            net_deferred_position = (dta or 0.0) - (dtl or 0.0)
            if net_deferred_position != 0:
                interpretation = f"Net deferred tax {'asset' if net_deferred_position > 0 else 'liability'} of {abs(net_deferred_position):,.0f}"
                position_risk = RiskLevel.LOW
                if total_assets and total_assets > 0:
                    if abs(net_deferred_position) / total_assets > 0.05:
                        interpretation += " - significant deferred tax position"
                        position_risk = RiskLevel.MODERATE
                    else:
                        interpretation += " - modest deferred tax position"
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Net Deferred Tax Position",
                    value=net_deferred_position,
                    interpretation=interpretation,
                    risk_level=position_risk,
                    methodology="Deferred Tax Assets - Deferred Tax Liabilities"
                              + ("" if dta is not None and dtl is not None
                                 else f" ({'assets' if dta is None else 'liabilities'} not reported, taken as 0)")
                ))

        if dta and dta > 0 and total_assets and total_assets > 0:
            dta_ratio = dta / total_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="DTA to Total Assets",
                value=dta_ratio,
                interpretation=("Significant deferred tax assets requiring realization assessment" if dta_ratio > 0.10
                                else "Moderate deferred tax assets" if dta_ratio > 0.05 else "Limited deferred tax assets"),
                risk_level=RiskLevel.MODERATE if dta_ratio > 0.15 else RiskLevel.LOW,
                methodology="Deferred Tax Assets / Total Assets",
                limitations=["DTA realization depends on future taxable income"]
            ))

            valuation_allowance = self.note_number(statements.notes, 'dta_valuation_allowance')
            if valuation_allowance and valuation_allowance > 0:
                allowance_ratio = valuation_allowance / (dta + valuation_allowance)
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="DTA Valuation Allowance Ratio",
                    value=allowance_ratio,
                    interpretation=("High valuation allowance indicates uncertainty about DTA realization" if allowance_ratio > 0.5
                                    else "Moderate valuation allowance" if allowance_ratio > 0.2
                                    else "Low valuation allowance suggests confident DTA realization"),
                    risk_level=RiskLevel.HIGH if allowance_ratio > 0.6 else RiskLevel.MODERATE if allowance_ratio > 0.3 else RiskLevel.LOW,
                    methodology="Valuation Allowance / (Net DTA + Valuation Allowance)",
                    limitations=["High allowance may indicate poor earnings prospects"]
                ))

        if dtl and dtl > 0 and total_assets and total_assets > 0:
            dtl_ratio = dtl / total_assets
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="DTL to Total Assets",
                value=dtl_ratio,
                interpretation=("Significant deferred tax liabilities from temporary differences" if dtl_ratio > 0.10
                                else "Moderate deferred tax liabilities" if dtl_ratio > 0.05 else "Limited deferred tax liabilities"),
                risk_level=RiskLevel.LOW,
                methodology="Deferred Tax Liabilities / Total Assets",
                limitations=["DTL reversal timing depends on asset usage and accounting methods"]
            ))

        if total_tax_expense and deferred_tax_expense:
            deferred_ratio = abs(deferred_tax_expense) / abs(total_tax_expense)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Deferred Tax Expense Ratio",
                value=deferred_ratio,
                interpretation=("Significant deferred tax component in total tax expense" if deferred_ratio > 0.3
                                else "Moderate deferred tax impact" if deferred_ratio > 0.15 else "Limited deferred tax expense component"),
                risk_level=RiskLevel.MODERATE if deferred_ratio > 0.5 else RiskLevel.LOW,
                methodology="|Deferred Tax Expense| / |Total Tax Expense|"
            ))

        return results

    def _analyze_tax_reconciliation(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """Statutory-to-effective rate reconciliation (only when reconciling items are disclosed)"""
        results = []
        notes = statements.notes
        pretax_income = statements.income_statement['pretax_income']
        tax_expense = statements.income_statement['tax_expense']

        item_keys = ('permanent_differences', 'tax_credits', 'foreign_rate_differences',
                     'prior_year_tax_adjustments', 'other_tax_effects')
        items = {k: self.note_number(notes, k) for k in item_keys}
        items = {k: v for k, v in items.items() if v is not None}
        if not items:
            return results

        for item_name, item_value in items.items():
            if tax_expense and abs(item_value) > abs(tax_expense) * 0.05:
                item_display_name = item_name.replace('_', ' ').title()
                item_impact = item_value / tax_expense
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name=f"Tax Reconciliation - {item_display_name}",
                    value=item_impact,
                    interpretation=f"{item_display_name} {'increases' if item_value > 0 else 'decreases'} tax expense by {self.format_percentage(abs(item_impact))}",
                    risk_level=RiskLevel.LOW,
                    methodology="Reconciling item / Total tax expense (item sign: + increases tax)"
                ))

        statutory_rate, statutory_source = self._statutory_rate(statements)
        if statutory_rate is None:
            results.append(self.unavailable(AnalysisType.QUALITY, "Tax Reconciliation Quality", statutory_source))
            return results

        gap = tax_expense - statutory_rate * pretax_income
        if abs(gap) < 1e-9:
            return results
        explained = sum(items.values()) / gap
        if 0.8 <= explained <= 1.2:
            reconciliation_quality, reconciliation_risk = "Disclosed reconciling items explain the gap to the statutory rate", RiskLevel.LOW
        elif 0.5 <= explained <= 1.5:
            reconciliation_quality, reconciliation_risk = "Disclosed reconciling items partly explain the gap to the statutory rate", RiskLevel.LOW
        else:
            reconciliation_quality, reconciliation_risk = "Disclosed reconciling items do not explain the gap to the statutory rate", RiskLevel.MODERATE
        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Tax Reconciliation Quality",
            value=explained,
            interpretation=reconciliation_quality,
            risk_level=reconciliation_risk,
            methodology=f"Sum of disclosed reconciling items / (Tax Expense - Statutory Rate x Pretax Income); {statutory_source}"
        ))
        return results

    def _analyze_cash_vs_book_taxes(self, statements: FinancialStatements,
                                    comparative_data: Optional[List[FinancialStatements]] = None) -> List[
        AnalysisResult]:
        """Cash taxes vs book tax expense"""
        results = []
        income_statement = statements.income_statement
        tax_expense = income_statement['tax_expense']
        cash_taxes_paid = statements.cash_flow.get('cash_taxes_paid')
        current_tax_expense = income_statement.get('current_tax_expense')
        basis = "current tax expense"
        if current_tax_expense is None:
            current_tax_expense, basis = tax_expense, "total tax expense (current portion not reported)"

        if current_tax_expense and cash_taxes_paid is not None:
            cash_vs_current = cash_taxes_paid - current_tax_expense
            cash_timing_ratio = cash_vs_current / abs(current_tax_expense)
            if abs(cash_timing_ratio) < 0.1:
                timing_interpretation = "Cash taxes and tax expense are closely aligned"
            elif cash_vs_current > 0:
                timing_interpretation = f"Cash taxes exceed {basis} by {self.format_percentage(abs(cash_timing_ratio))} - paying for prior years or prepaying"
            else:
                timing_interpretation = f"{basis.capitalize()} exceeds cash payments by {self.format_percentage(abs(cash_timing_ratio))} - timing differences or accruals"
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Cash vs Current Tax Timing",
                value=cash_timing_ratio,
                interpretation=timing_interpretation,
                risk_level=RiskLevel.LOW,
                methodology=f"(Cash Taxes Paid - {basis}) / |{basis}|"
            ))

        if comparative_data and cash_taxes_paid is not None:
            pairs = [(p.cash_flow['cash_taxes_paid'], p.income_statement['tax_expense']) for p in comparative_data
                     if 'cash_taxes_paid' in p.cash_flow and 'tax_expense' in p.income_statement]
            pairs.append((cash_taxes_paid, tax_expense))
            if len(pairs) >= 4:
                cash_vals, book_vals = zip(*pairs)
                if np.std(cash_vals) > 0 and np.std(book_vals) > 0:
                    correlation = float(np.corrcoef(cash_vals, book_vals)[0, 1])
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Cash-Book Tax Correlation",
                        value=correlation,
                        interpretation=("Strong correlation between cash and book taxes" if correlation > 0.8
                                        else "Moderate correlation" if correlation > 0.5
                                        else "Weak correlation - significant timing differences"),
                        risk_level=RiskLevel.LOW if correlation > 0.7 else RiskLevel.MODERATE if correlation > 0.4 else RiskLevel.HIGH,
                        methodology=f"Correlation of cash taxes and tax expense over {len(pairs)} periods"
                    ))

        return results

    def _assess_tax_planning(self, statements: FinancialStatements,
                             comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Tax planning effectiveness relative to the statutory rate"""
        results = []
        notes = statements.notes
        tax_expense = statements.income_statement['tax_expense']
        effective_tax_rate = self._etr(statements)
        statutory_rate, statutory_source = self._statutory_rate(statements)

        if effective_tax_rate is not None and statutory_rate:
            tax_efficiency_ratio = (statutory_rate - effective_tax_rate) / statutory_rate
            if tax_efficiency_ratio > 0.2:
                efficiency_interpretation, efficiency_risk = "High tax efficiency - significant tax optimization achieved", RiskLevel.MODERATE
            elif tax_efficiency_ratio > 0.05:
                efficiency_interpretation, efficiency_risk = "Moderate tax efficiency - some optimization strategies employed", RiskLevel.LOW
            elif tax_efficiency_ratio > -0.05:
                efficiency_interpretation, efficiency_risk = "Standard tax efficiency - limited optimization", RiskLevel.LOW
            else:
                efficiency_interpretation, efficiency_risk = "Effective rate above statutory - non-deductible items, foreign taxes or valuation allowances", RiskLevel.MODERATE
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Tax Planning Efficiency",
                value=tax_efficiency_ratio,
                interpretation=efficiency_interpretation,
                risk_level=efficiency_risk,
                methodology=f"(Statutory Rate - Effective Rate) / Statutory Rate; {statutory_source}"
            ))
        elif effective_tax_rate is not None:
            results.append(self.unavailable(AnalysisType.QUALITY, "Tax Planning Efficiency", statutory_source,
                                            "(Statutory Rate - Effective Rate) / Statutory Rate"))

        uncertain_tax_positions = self.note_number(notes, 'uncertain_tax_positions')
        if uncertain_tax_positions and uncertain_tax_positions > 0 and tax_expense:
            utp_ratio = uncertain_tax_positions / abs(tax_expense)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Uncertain Tax Positions",
                value=utp_ratio,
                interpretation=("Significant uncertain tax positions indicate aggressive tax strategies" if utp_ratio > 0.5
                                else "Moderate uncertain tax positions" if utp_ratio > 0.2 else "Limited uncertain tax positions"),
                risk_level=RiskLevel.HIGH if utp_ratio > 0.8 else RiskLevel.MODERATE if utp_ratio > 0.3 else RiskLevel.LOW,
                methodology="Uncertain Tax Positions / |Total Tax Expense|",
                limitations=["High UTP may indicate potential tax adjustments"]
            ))

        if comparative_data and len(comparative_data) >= 2 and effective_tax_rate is not None:
            etr_values = [e for e in (self._etr(p) for p in comparative_data) if e is not None]
            etr_values.append(effective_tax_rate)
            mean_etr = float(np.mean(etr_values))
            if len(etr_values) >= 3 and mean_etr > 0:
                etr_consistency = 1 - float(np.std(etr_values)) / mean_etr
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Tax Strategy Consistency",
                    value=etr_consistency,
                    interpretation=("Highly consistent tax strategy" if etr_consistency > 0.8
                                    else "Moderately consistent tax approach" if etr_consistency > 0.6
                                    else "Inconsistent effective tax rate - changing circumstances or aggressive planning"),
                    risk_level=RiskLevel.LOW if etr_consistency > 0.7 else RiskLevel.MODERATE if etr_consistency > 0.5 else RiskLevel.HIGH,
                    methodology=f"1 - (Standard Deviation of ETR / Mean ETR) over {len(etr_values)} profitable periods"
                ))

        return results

    def _analyze_international_taxes(self, statements: FinancialStatements) -> List[AnalysisResult]:
        """International tax considerations (note disclosures)"""
        results = []
        notes = statements.notes
        income_statement = statements.income_statement

        foreign_income = self.note_number(notes, 'foreign_pretax_income')
        total_pretax = income_statement.get('pretax_income')
        if foreign_income and foreign_income > 0 and total_pretax and total_pretax > 0:
            foreign_income_ratio = foreign_income / total_pretax
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="International Income Exposure",
                value=foreign_income_ratio,
                interpretation=("Significant international operations" if foreign_income_ratio > 0.3
                                else "Moderate international presence" if foreign_income_ratio > 0.1 else "Primarily domestic operations"),
                risk_level=RiskLevel.MODERATE if foreign_income_ratio > 0.5 else RiskLevel.LOW,
                methodology="Foreign Pretax Income / Total Pretax Income",
                limitations=["International operations create additional tax complexity"]
            ))

        foreign_tax_credit = self.note_number(notes, 'foreign_tax_credit')
        total_tax_expense = income_statement.get('tax_expense')
        if foreign_tax_credit and foreign_tax_credit > 0 and total_tax_expense:
            ftc_ratio = foreign_tax_credit / abs(total_tax_expense)
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Foreign Tax Credit Utilization",
                value=ftc_ratio,
                interpretation=("Significant foreign tax credit utilization" if ftc_ratio > 0.2
                                else "Moderate foreign tax credits" if ftc_ratio > 0.1 else "Limited foreign tax credit usage"),
                risk_level=RiskLevel.LOW,
                methodology="Foreign Tax Credits / |Total Tax Expense|"
            ))

        related = [self.note_number(notes, k) for k in ('related_party_revenue', 'related_party_expenses')]
        related = [v for v in related if v is not None]
        revenue = income_statement.get('revenue')
        if related and sum(related) > 0 and revenue and revenue > 0:
            transfer_pricing_exposure = sum(related) / revenue
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Transfer Pricing Exposure",
                value=transfer_pricing_exposure,
                interpretation=("High transfer pricing exposure - significant related party transactions" if transfer_pricing_exposure > 0.3
                                else "Moderate transfer pricing considerations" if transfer_pricing_exposure > 0.1 else "Limited transfer pricing exposure"),
                risk_level=RiskLevel.HIGH if transfer_pricing_exposure > 0.5 else RiskLevel.MODERATE if transfer_pricing_exposure > 0.2 else RiskLevel.LOW,
                methodology="Related Party Transactions / Revenue",
                limitations=["High exposure increases tax audit and adjustment risk"]
            ))

        return results

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Key tax metrics (missing inputs -> metric omitted)"""
        income_statement = statements.income_statement
        balance_sheet = statements.balance_sheet
        metrics = {}

        etr = self._etr(statements)
        if etr is not None:
            metrics['effective_tax_rate'] = etr
            cash_taxes_paid = statements.cash_flow.get('cash_taxes_paid')
            if cash_taxes_paid is not None:
                metrics['cash_tax_rate'] = cash_taxes_paid / income_statement['pretax_income']

        dta = balance_sheet.get('deferred_tax_asset')
        dtl = balance_sheet.get('deferred_tax_liability')
        total_assets = balance_sheet.get('total_assets')
        if dta is not None or dtl is not None:
            metrics['net_deferred_tax_position'] = (dta or 0.0) - (dtl or 0.0)
        if total_assets and total_assets > 0:
            if dta is not None:
                metrics['dta_to_assets'] = dta / total_assets
            if dtl is not None:
                metrics['dtl_to_assets'] = dtl / total_assets

        tax_expense = income_statement.get('tax_expense')
        if tax_expense:
            if 'current_tax_expense' in income_statement:
                metrics['current_tax_ratio'] = income_statement['current_tax_expense'] / tax_expense
            deferred = income_statement.get('deferred_tax_expense', statements.cash_flow.get('deferred_tax'))
            if deferred is not None:
                metrics['deferred_tax_ratio'] = deferred / tax_expense

        return metrics

    def create_tax_rate_analysis(self, statements: FinancialStatements,
                                 comparative_data: Optional[List[FinancialStatements]] = None) -> TaxRateAnalysis:
        """Tax rate analysis object (None where not computable)"""
        notes = statements.notes
        pretax_income = statements.income_statement.get('pretax_income')
        cash_taxes_paid = statements.cash_flow.get('cash_taxes_paid')

        effective_tax_rate = self._etr(statements)
        cash_tax_rate = (cash_taxes_paid / pretax_income
                         if cash_taxes_paid is not None and pretax_income and pretax_income > 0 else None)
        statutory_tax_rate, _ = self._statutory_rate(statements)

        etr_vs_statutory = (effective_tax_rate - statutory_tax_rate
                            if effective_tax_rate is not None and statutory_tax_rate is not None else None)
        cash_vs_effective = (cash_tax_rate - effective_tax_rate
                             if cash_tax_rate is not None and effective_tax_rate is not None else None)
        tax_efficiency_score = ((statutory_tax_rate - effective_tax_rate) / statutory_tax_rate * 100
                                if etr_vs_statutory is not None and statutory_tax_rate else None)

        rate_volatility = None
        if comparative_data and effective_tax_rate is not None:
            etr_values = [e for e in (self._etr(p) for p in comparative_data) if e is not None] + [effective_tax_rate]
            if len(etr_values) >= 3:
                rate_volatility = float(np.std(etr_values))

        rate_reconciliation = {k: self.note_number(notes, k) for k in
                               ('statutory_rate_effect', 'permanent_differences', 'tax_credits', 'foreign_rate_differences')}
        rate_reconciliation = {k: v for k, v in rate_reconciliation.items() if v is not None}

        permanent_differences = []
        if (self.note_number(notes, 'municipal_bond_interest') or 0) > 0:
            permanent_differences.append("Tax-exempt interest income")
        if (self.note_number(notes, 'meals_entertainment') or 0) > 0:
            permanent_differences.append("Non-deductible meals and entertainment")
        if (self.note_number(notes, 'life_insurance_proceeds') or 0) > 0:
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
        """Deferred tax analysis object"""
        balance_sheet = statements.balance_sheet
        notes = statements.notes

        dta = balance_sheet.get('deferred_tax_asset', 0.0)
        dtl = balance_sheet.get('deferred_tax_liability', 0.0)
        net_deferred_tax_position = dta - dtl

        dta_composition = {k: v for k, v in (
            ('nol_carryforwards', self.note_number(notes, 'nol_dta')),
            ('depreciation_differences', self.note_number(notes, 'depreciation_dta')),
            ('accrued_expenses', self.note_number(notes, 'accrual_dta')),
            ('other', self.note_number(notes, 'other_dta'))) if v is not None}
        dtl_composition = {k: v for k, v in (
            ('depreciation_differences', self.note_number(notes, 'depreciation_dtl')),
            ('intangible_amortization', self.note_number(notes, 'intangible_dtl')),
            ('other', self.note_number(notes, 'other_dtl'))) if v is not None}

        valuation_allowance = self.note_number(notes, 'dta_valuation_allowance', 0.0)
        gross_dta = dta + valuation_allowance
        # Share of gross DTA not reserved by the valuation allowance (not a statistical probability)
        dta_realization_probability = dta / gross_dta if gross_dta > 0 else None

        net_position_trend = TrendDirection.STABLE
        if comparative_data and len(comparative_data) >= 2:
            values = [p.balance_sheet.get('deferred_tax_asset', 0.0) - p.balance_sheet.get('deferred_tax_liability', 0.0)
                      for p in comparative_data] + [net_deferred_tax_position]
            if values[0] != 0:
                change = (values[-1] - values[0]) / abs(values[0])
                if change > 0.1:
                    net_position_trend = TrendDirection.IMPROVING
                elif change < -0.1:
                    net_position_trend = TrendDirection.DETERIORATING

        return DeferredTaxAnalysis(
            deferred_tax_assets=dta,
            deferred_tax_liabilities=dtl,
            net_deferred_tax_position=net_deferred_tax_position,
            valuation_allowance=valuation_allowance,
            dta_realization_probability=dta_realization_probability,
            net_position_trend=net_position_trend,
            reversal_timeline="Not disclosed" if net_deferred_tax_position else "No significant deferred taxes",
            dta_composition=dta_composition,
            dtl_composition=dtl_composition
        )

    def assess_tax_planning_strategy(self, statements: FinancialStatements,
                                     comparative_data: Optional[
                                         List[FinancialStatements]] = None) -> TaxPlanningAnalysis:
        """Tax planning strategy object"""
        notes = statements.notes
        tax_expense = statements.income_statement.get('tax_expense', 0.0)
        effective_tax_rate = self._etr(statements)
        statutory_rate, _ = self._statutory_rate(statements)

        tax_savings_rate = ((statutory_rate - effective_tax_rate) / statutory_rate
                            if effective_tax_rate is not None and statutory_rate else None)
        if tax_savings_rate is None:
            tax_strategy_classification = None
        elif tax_savings_rate > 0.25:
            tax_strategy_classification = TaxStrategy.AGGRESSIVE
        elif tax_savings_rate > 0.10:
            tax_strategy_classification = TaxStrategy.MODERATE
        else:
            tax_strategy_classification = TaxStrategy.CONSERVATIVE

        tax_optimization_score = (min(100.0, max(0.0, tax_savings_rate * 100))
                                  if tax_savings_rate is not None else None)

        timing_strategies = []
        if (self.note_number(notes, 'installment_sales') or 0) > 0:
            timing_strategies.append("Installment sale deferrals")
        if (self.note_number(notes, 'like_kind_exchanges') or 0) > 0:
            timing_strategies.append("Like-kind exchanges")
        if (self.note_number(notes, 'depreciation_elections') or 0) > 0:
            timing_strategies.append("Accelerated depreciation elections")

        uncertain_tax_positions = self.note_number(notes, 'uncertain_tax_positions', 0.0)
        if uncertain_tax_positions > abs(tax_expense) * 0.5:
            tax_audit_risk = RiskLevel.HIGH
        elif uncertain_tax_positions > abs(tax_expense) * 0.2:
            tax_audit_risk = RiskLevel.MODERATE
        else:
            tax_audit_risk = RiskLevel.LOW

        compliance_quality = 100
        if (self.note_number(notes, 'tax_penalties') or 0) > 0:
            compliance_quality -= 20
        if (self.note_number(notes, 'prior_year_adjustments') or 0) > abs(tax_expense) * 0.1:
            compliance_quality -= 15

        return TaxPlanningAnalysis(
            tax_strategy_classification=tax_strategy_classification,
            tax_optimization_score=tax_optimization_score,
            international_planning=(self.note_number(notes, 'foreign_pretax_income') or 0) > 0,
            tax_audit_risk=tax_audit_risk,
            compliance_quality=max(0, compliance_quality),
            uncertain_tax_positions=uncertain_tax_positions,
            timing_strategies=timing_strategies
        )

    # Headline national corporate income tax rates, matched on the exact country name.
    # (US: federal only, excludes state taxes; Germany: approximate combined CIT +
    # solidarity surcharge + municipal trade tax.) Supply notes.statutory_tax_rate to override.
    STATUTORY_RATES = {
        'usa': 0.21, 'us': 0.21, 'u.s.': 0.21, 'united states': 0.21, 'united states of america': 0.21,
        'uk': 0.25, 'u.k.': 0.25, 'united kingdom': 0.25, 'great britain': 0.25, 'gb': 0.25, 'britain': 0.25,
        'germany': 0.30, 'france': 0.25, 'italy': 0.24, 'spain': 0.25, 'netherlands': 0.258,
        'ireland': 0.125, 'china': 0.25, 'australia': 0.30, 'singapore': 0.17, 'hong kong': 0.165,
    }

    def _statutory_rate(self, statements: FinancialStatements) -> Tuple[Optional[float], str]:
        """(rate, source). notes.statutory_tax_rate wins (21 and 0.21 both mean 21%), then an
        exact-match country table; (None, reason) when neither is available."""
        note_rate = self.note_number(statements.notes, 'statutory_tax_rate')
        if note_rate is not None:
            rate = note_rate / 100.0 if note_rate > 1 else note_rate
            return rate, "statutory_tax_rate from notes"
        country = (statements.company_info.country or '').strip().lower()
        if country in self.STATUTORY_RATES:
            return self.STATUTORY_RATES[country], f"headline corporate rate for {statements.company_info.country}"
        return None, f"no statutory rate supplied and country '{statements.company_info.country}' not in the rate table"

    @staticmethod
    def _etr(statements: FinancialStatements) -> Optional[float]:
        """Effective tax rate, defined only for positive pre-tax income."""
        inc = statements.income_statement
        pretax, tax = inc.get('pretax_income'), inc.get('tax_expense')
        if pretax is None or tax is None or pretax <= 0:
            return None
        return tax / pretax
