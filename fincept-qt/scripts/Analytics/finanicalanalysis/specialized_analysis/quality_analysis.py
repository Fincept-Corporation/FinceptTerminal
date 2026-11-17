
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


import math
import numpy as np
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

        # Beneish (1999) eight-variable M-score:
        # M = -4.84 + 0.920 DSRI + 0.528 GMI + 0.404 AQI + 0.892 SGI + 0.115 DEPI
        #     - 0.172 SGAI + 4.679 TATA - 0.327 LVGI
        self.beneish_coefficients = {
            'intercept': -4.84,
            'dsri': 0.920,
            'gmi': 0.528,
            'aqi': 0.404,
            'sgi': 0.892,
            'depi': 0.115,
            'sgai': -0.172,
            'lvgi': -0.327,
            'tata': 4.679
        }

    # ------------------------------------------------------------------
    # Beneish M-score (single implementation used by analyze() and
    # calculate_beneish_m_score())
    # ------------------------------------------------------------------

    def _beneish(self, current: FinancialStatements, prior: FinancialStatements) -> Dict:
        """Compute the eight Beneish indices and the M-score.

        Returns {'m_score': float|None, 'reason': str, 'components': {...},
        'neutral': [indices set to 1.0 because inputs were missing], 'probability': float|None}.
        An index whose inputs are missing is set to its neutral value 1.0 (TATA to 0),
        as is common practice; such indices are listed in 'neutral'. The score is not
        computed at all without revenue and total assets for both years and current
        net income and operating cash flow.
        """
        cis, cbs, ccf = current.income_statement, current.balance_sheet, current.cash_flow
        pis, pbs = prior.income_statement, prior.balance_sheet

        rev_c, rev_p = cis.get('revenue'), pis.get('revenue')
        ta_c, ta_p = cbs.get('total_assets'), pbs.get('total_assets')
        ni, cfo = cis.get('net_income'), ccf.get('operating_cash_flow')
        missing = [name for name, v in (('current revenue', rev_c), ('prior revenue', rev_p),
                                         ('current total assets', ta_c), ('prior total assets', ta_p),
                                         ('net income', ni), ('operating cash flow', cfo))
                   if v is None or (name.endswith(('revenue', 'assets')) and v <= 0)]
        if missing:
            return {'m_score': None, 'reason': f"missing or non-positive: {', '.join(missing)}",
                    'components': {}, 'neutral': [], 'probability': None}

        c, neutral = {}, []

        def ratio(a, b):
            return a / b if a is not None and b not in (None, 0) else None

        # 1. DSRI = (AR_t / Sales_t) / (AR_t-1 / Sales_t-1)
        dsr_c, dsr_p = ratio(cbs.get('accounts_receivable'), rev_c), ratio(pbs.get('accounts_receivable'), rev_p)
        c['dsri'] = dsr_c / dsr_p if dsr_c is not None and dsr_p else None
        # 2. GMI = GM_t-1 / GM_t, GM = (Sales - COGS) / Sales
        gp_c, gp_p = self.gross_profit(cis), self.gross_profit(pis)
        gm_c, gm_p = ratio(gp_c, rev_c), ratio(gp_p, rev_p)
        c['gmi'] = gm_p / gm_c if gm_c is not None and gm_p is not None and gm_c > 0 else None
        # 3. AQI = [1 - (CA_t + PPE_t)/TA_t] / [1 - (CA_t-1 + PPE_t-1)/TA_t-1]
        if all(k in cbs for k in ('current_assets', 'ppe_net')) and all(k in pbs for k in ('current_assets', 'ppe_net')):
            aq_c = 1 - (cbs['current_assets'] + cbs['ppe_net']) / ta_c
            aq_p = 1 - (pbs['current_assets'] + pbs['ppe_net']) / ta_p
            c['aqi'] = aq_c / aq_p if aq_p > 0 else None
        else:
            c['aqi'] = None
        # 4. SGI = Sales_t / Sales_t-1
        c['sgi'] = rev_c / rev_p
        # 5. DEPI = [Dep_t-1/(Dep_t-1 + PPE_t-1)] / [Dep_t/(Dep_t + PPE_t)]
        dep_c, dep_p = cis.get('depreciation'), pis.get('depreciation')
        ppe_c, ppe_p = cbs.get('ppe_net'), pbs.get('ppe_net')
        if None not in (dep_c, dep_p, ppe_c, ppe_p) and dep_c + ppe_c > 0 and dep_p + ppe_p > 0:
            rate_c, rate_p = dep_c / (dep_c + ppe_c), dep_p / (dep_p + ppe_p)
            c['depi'] = rate_p / rate_c if rate_c > 0 else None
        else:
            c['depi'] = None
        # 6. SGAI = (SGA_t / Sales_t) / (SGA_t-1 / Sales_t-1)
        sga_c, sga_p = ratio(self.sga(cis), rev_c), ratio(self.sga(pis), rev_p)
        c['sgai'] = sga_c / sga_p if sga_c is not None and sga_p else None
        # 7. LVGI = [(CL_t + LTD_t)/TA_t] / [(CL_t-1 + LTD_t-1)/TA_t-1]
        lev_note = None
        if all(k in cbs for k in ('current_liabilities', 'long_term_debt')) and \
                all(k in pbs for k in ('current_liabilities', 'long_term_debt')):
            lv_c = (cbs['current_liabilities'] + cbs['long_term_debt']) / ta_c
            lv_p = (pbs['current_liabilities'] + pbs['long_term_debt']) / ta_p
        elif 'total_liabilities' in cbs and 'total_liabilities' in pbs:
            lv_c, lv_p = cbs['total_liabilities'] / ta_c, pbs['total_liabilities'] / ta_p
            lev_note = "LVGI uses total liabilities (current liabilities + long-term debt not reported)"
        else:
            lv_c = lv_p = None
        c['lvgi'] = lv_c / lv_p if lv_c is not None and lv_p else None
        # 8. TATA = (Income from continuing operations - CFO) / TA_t
        c['tata'] = (ni - cfo) / ta_c

        for k, v in c.items():
            if v is None:
                c[k] = 0.0 if k == 'tata' else 1.0
                neutral.append(k.upper())

        coef = self.beneish_coefficients
        m_score = coef['intercept'] + sum(coef[k] * v for k, v in c.items())
        # The M-score is a probit index: model-implied probability = Phi(M)
        probability = 0.5 * (1.0 + math.erf(m_score / math.sqrt(2.0)))
        return {'m_score': m_score, 'reason': lev_note or '', 'components': c, 'neutral': neutral,
                'probability': probability}

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

        net_income = income_statement.get('net_income')
        operating_cash_flow = cash_flow.get('operating_cash_flow')

        if net_income is None or operating_cash_flow is None:
            results.append(self.unavailable(AnalysisType.QUALITY, "CFO to Net Income Ratio",
                                            "net income and operating cash flow are both required",
                                            "Operating Cash Flow / Net Income"))
        elif net_income <= 0:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="CFO to Net Income Ratio",
                value=None,
                interpretation=(f"Not meaningful with a net loss (net income {net_income:,.2f}, operating cash "
                                f"flow {operating_cash_flow:,.2f})"),
                risk_level=RiskLevel.MODERATE if operating_cash_flow > 0 else RiskLevel.HIGH,
                methodology="Operating Cash Flow / Net Income (defined for positive net income)"
            ))
        else:
            cfo_to_ni = operating_cash_flow / net_income
            threshold = self.quality_thresholds['cfo_to_ni']
            if cfo_to_ni >= threshold['excellent']:
                quality_interpretation, quality_risk = "Excellent earnings quality - strong cash backing", RiskLevel.LOW
            elif cfo_to_ni >= threshold['good']:
                quality_interpretation, quality_risk = "Good earnings quality - earnings well-supported by cash flow", RiskLevel.LOW
            elif cfo_to_ni >= threshold['adequate']:
                quality_interpretation, quality_risk = "Adequate earnings quality - moderate cash support", RiskLevel.LOW
            elif cfo_to_ni >= threshold['concerning']:
                quality_interpretation, quality_risk = "Low earnings quality - weak cash flow relative to earnings", RiskLevel.MODERATE
            else:
                quality_interpretation, quality_risk = "Concerning earnings quality - earnings significantly exceed cash generation", RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="CFO to Net Income Ratio",
                value=cfo_to_ni,
                interpretation=quality_interpretation,
                risk_level=quality_risk,
                methodology="Operating Cash Flow / Net Income",
                limitations=["Single period may be affected by timing differences"]
            ))

            capex = cash_flow.get('capex')
            if capex is not None:
                fcf_to_ni = (operating_cash_flow - capex) / net_income
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

        # Earnings persistence: slope beta of E_t = a + beta * E_t-1 (CFA definition),
        # needs at least 3 consecutive pairs (4 periods).
        if comparative_data and net_income is not None:
            history = [p.income_statement['net_income'] for p in comparative_data
                       if 'net_income' in p.income_statement] + [net_income]
            if len(history) >= 4:
                x = np.array(history[:-1], dtype=float)
                y = np.array(history[1:], dtype=float)
                if np.var(x) > 0:
                    persistence = float(np.cov(x, y, bias=True)[0, 1] / np.var(x))
                    threshold = self.quality_thresholds['earnings_persistence']
                    if persistence >= threshold['high']:
                        persistence_interpretation, persistence_risk = "High earnings persistence - predictable earnings stream", RiskLevel.LOW
                    elif persistence >= threshold['moderate']:
                        persistence_interpretation, persistence_risk = "Moderate earnings persistence", RiskLevel.LOW
                    elif persistence >= threshold['low']:
                        persistence_interpretation, persistence_risk = "Low earnings persistence - volatile earnings", RiskLevel.MODERATE
                    else:
                        persistence_interpretation, persistence_risk = "Very low or negative earnings persistence - unpredictable", RiskLevel.HIGH
                    results.append(AnalysisResult(
                        analysis_type=AnalysisType.QUALITY,
                        metric_name="Earnings Persistence",
                        value=persistence,
                        interpretation=persistence_interpretation,
                        risk_level=persistence_risk,
                        methodology=f"OLS slope of net income on prior-period net income ({len(history) - 1} pairs)",
                        limitations=["Few observations - indicative only"]
                    ))

        # Non-recurring items relative to earnings (only items actually reported)
        nr_keys = ('unusual_items', 'restructuring_charges', 'impairment_losses', 'gains_losses_disposals',
                   'discontinued_operations', 'extraordinary_items')
        reported = {k: income_statement[k] for k in nr_keys if k in income_statement}
        if reported and net_income:
            total_non_recurring = sum(abs(v) for v in reported.values())
            non_recurring_ratio = total_non_recurring / abs(net_income)
            if non_recurring_ratio > 0.3:
                recurring_interpretation, recurring_risk = "Significant non-recurring items - core earnings quality unclear", RiskLevel.HIGH
            elif non_recurring_ratio > 0.15:
                recurring_interpretation, recurring_risk = "Moderate non-recurring items impacting reported earnings", RiskLevel.MODERATE
            elif non_recurring_ratio > 0.05:
                recurring_interpretation, recurring_risk = "Limited non-recurring items", RiskLevel.LOW
            else:
                recurring_interpretation, recurring_risk = "Minimal non-recurring items - clean earnings", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Non-Recurring Items Ratio",
                value=non_recurring_ratio,
                interpretation=recurring_interpretation,
                risk_level=recurring_risk,
                methodology="Sum of |non-recurring items| / |Net Income| (items: " + ", ".join(reported) + ")"
            ))

        return results

    def _analyze_accruals(self, statements: FinancialStatements,
                          comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Accrual analysis (cash-flow approach) and working-capital accrual components"""
        results = []
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        net_income = income_statement.get('net_income')
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        avg_assets, basis = self.average_balance(statements, comparative_data, 'total_assets')

        if net_income is not None and operating_cash_flow is not None and avg_assets and avg_assets > 0:
            accrual_ratio = (net_income - operating_cash_flow) / avg_assets
            threshold = self.quality_thresholds['accrual_ratio']
            # Signed: only positive accruals (earnings above cash flow) are a quality concern.
            if accrual_ratio <= threshold['high_quality']:
                accrual_interpretation = ("Negative accruals - cash flow exceeds earnings" if accrual_ratio < 0
                                          else "Low accruals - high earnings quality")
                accrual_risk = RiskLevel.LOW
            elif accrual_ratio <= threshold['moderate']:
                accrual_interpretation, accrual_risk = "Moderate accruals - acceptable earnings quality", RiskLevel.LOW
            elif accrual_ratio <= threshold['low']:
                accrual_interpretation, accrual_risk = "High accruals - earnings quality concerns", RiskLevel.MODERATE
            else:
                accrual_interpretation, accrual_risk = "Very high accruals - significant earnings quality concerns", RiskLevel.HIGH

            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Accrual Ratio (CF Approach)",
                value=accrual_ratio,
                interpretation=accrual_interpretation,
                risk_level=accrual_risk,
                methodology=f"(Net Income - Operating Cash Flow) / Total Assets ({self.basis_note(basis)})",
                limitations=["High accruals may indicate growth rather than manipulation"]
            ))
        else:
            results.append(self.unavailable(AnalysisType.QUALITY, "Accrual Ratio (CF Approach)",
                                            "net income, operating cash flow and positive total assets are required",
                                            "(Net Income - Operating Cash Flow) / Total Assets"))

        if not comparative_data:
            return results

        prev_bs = comparative_data[-1].balance_sheet
        revenue = income_statement.get('revenue')
        cogs = income_statement.get('cost_of_sales')

        def change(key):
            if key in balance_sheet and key in prev_bs:
                return balance_sheet[key] - prev_bs[key]
            return None

        change_ar = change('accounts_receivable')
        if change_ar is not None and revenue and revenue > 0:
            ar_change_to_revenue = change_ar / revenue
            if ar_change_to_revenue > 0.1:
                ar_interpretation, ar_risk = "Significant receivables increase - may indicate aggressive revenue recognition", RiskLevel.MODERATE
            elif ar_change_to_revenue > 0.05:
                ar_interpretation, ar_risk = "Moderate receivables increase", RiskLevel.LOW
            elif ar_change_to_revenue < -0.05:
                ar_interpretation, ar_risk = "Receivables decrease - improved collections", RiskLevel.LOW
            else:
                ar_interpretation, ar_risk = "Stable receivables relative to revenue", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Receivables Change / Revenue",
                value=ar_change_to_revenue,
                interpretation=ar_interpretation,
                risk_level=ar_risk,
                methodology="Change in A/R / Revenue"
            ))

        change_inventory = change('inventory')
        if change_inventory is not None and cogs and cogs > 0:
            inventory_change_to_cogs = change_inventory / cogs
            if inventory_change_to_cogs > 0.15:
                inv_interpretation, inv_risk = "Large inventory build-up - potential obsolescence or channel stuffing", RiskLevel.MODERATE
            elif inventory_change_to_cogs > 0.05:
                inv_interpretation, inv_risk = "Inventory increase - growth preparation or caution needed", RiskLevel.LOW
            elif inventory_change_to_cogs < -0.05:
                inv_interpretation, inv_risk = "Inventory decrease - improved efficiency or supply issues", RiskLevel.LOW
            else:
                inv_interpretation, inv_risk = "Stable inventory levels", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Inventory Change / COGS",
                value=inventory_change_to_cogs,
                interpretation=inv_interpretation,
                risk_level=inv_risk,
                methodology="Change in Inventory / Cost of Sales"
            ))

        change_deferred_rev = change('deferred_revenue')
        if change_deferred_rev and revenue and revenue > 0:
            deferred_rev_change = change_deferred_rev / revenue
            if deferred_rev_change < -0.05:
                dr_interpretation, dr_risk = "Deferred revenue released - boosting current period revenue", RiskLevel.MODERATE
            elif deferred_rev_change > 0.05:
                dr_interpretation, dr_risk = "Deferred revenue build-up - conservative revenue recognition", RiskLevel.LOW
            else:
                dr_interpretation, dr_risk = "Stable deferred revenue", RiskLevel.LOW
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
        """Beneish M-score against the most recent prior period"""
        results = []
        if not comparative_data:
            return results

        b = self._beneish(statements, comparative_data[-1])
        methodology = ("Beneish (1999): -4.84 + 0.920 DSRI + 0.528 GMI + 0.404 AQI + 0.892 SGI + 0.115 DEPI "
                       "- 0.172 SGAI + 4.679 TATA - 0.327 LVGI; TATA = (net income - CFO) / total assets")
        if b['m_score'] is None:
            results.append(self.unavailable(AnalysisType.QUALITY, "Beneish M-Score", b['reason'], methodology))
            return results

        m_score = b['m_score']
        threshold = self.quality_thresholds['beneish_m_score']
        if m_score < threshold['low_risk']:
            m_interpretation, m_risk = "Low manipulation risk - M-Score below -2.22", RiskLevel.LOW
        elif m_score < threshold['moderate_risk']:
            m_interpretation, m_risk = "Moderate manipulation risk - M-Score between -2.22 and -1.78", RiskLevel.MODERATE
        elif m_score < threshold['elevated_risk']:
            m_interpretation, m_risk = "Elevated manipulation risk - M-Score above the -1.78 cut-off", RiskLevel.HIGH
        else:
            m_interpretation, m_risk = "High manipulation risk - pattern consistent with earnings manipulators", RiskLevel.HIGH

        limitations = ["Model estimated on manufacturing firms; not meaningful for banks and insurers",
                       "Not all high scores indicate fraud"]
        if b['neutral']:
            limitations.append(f"Set to neutral (1.0) for missing inputs: {', '.join(b['neutral'])}")
        if b['reason']:
            limitations.append(b['reason'])

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Beneish M-Score",
            value=m_score,
            interpretation=(f"{m_interpretation}; probit probability {b['probability']:.1%}; indices "
                            + ", ".join(f"{k.upper()} {v:.3f}" for k, v in b['components'].items())),
            risk_level=m_risk,
            methodology=methodology,
            limitations=limitations
        ))

        comp = b['components']
        manipulation_flags = []
        if comp['dsri'] > 1.1:
            manipulation_flags.append("Disproportionate receivables growth vs revenue (DSRI > 1.1)")
        if comp['gmi'] > 1.1:
            manipulation_flags.append("Declining gross margins (GMI > 1.1)")
        if comp['aqi'] > 1.1:
            manipulation_flags.append("Deteriorating asset quality (AQI > 1.1)")
        if comp['sgi'] > 1.5:
            manipulation_flags.append("Rapid sales growth may stress controls (SGI > 1.5)")
        if comp['tata'] > 0.05:
            manipulation_flags.append("High accruals relative to assets (TATA > 0.05)")

        if manipulation_flags:
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Manipulation Warning Flags",
                value=len(manipulation_flags),
                interpretation=f"Identified concerns: {'; '.join(manipulation_flags)}",
                risk_level=RiskLevel.MODERATE if len(manipulation_flags) < 3 else RiskLevel.HIGH,
                methodology="Analysis of individual M-Score components",
                limitations=manipulation_flags
            ))

        return results

    def _analyze_balance_sheet_quality(self, statements: FinancialStatements,
                                       comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Analyze balance sheet quality"""
        results = []
        balance_sheet = statements.balance_sheet
        income_statement = statements.income_statement

        total_assets = balance_sheet.get('total_assets')
        if total_assets is None or total_assets <= 0:
            return results

        if 'intangible_assets' in balance_sheet or 'goodwill' in balance_sheet:
            intangible_ratio = (balance_sheet.get('intangible_assets', 0.0) + balance_sheet.get('goodwill', 0.0)) / total_assets
            if intangible_ratio > 0.5:
                intangible_interpretation, intangible_risk = "Very high intangible concentration - balance sheet heavily dependent on subjective valuations", RiskLevel.HIGH
            elif intangible_ratio > 0.3:
                intangible_interpretation, intangible_risk = "High intangible assets - significant valuation judgment required", RiskLevel.MODERATE
            elif intangible_ratio > 0.15:
                intangible_interpretation, intangible_risk = "Moderate intangible assets", RiskLevel.LOW
            else:
                intangible_interpretation, intangible_risk = "Low intangible concentration - primarily tangible asset base", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Intangible Asset Concentration",
                value=intangible_ratio,
                interpretation=intangible_interpretation,
                risk_level=intangible_risk,
                methodology="(Intangibles + Goodwill) / Total Assets"
            ))

        revenue = income_statement.get('revenue')
        avg_ar, basis = self.average_balance(statements, comparative_data, 'accounts_receivable')
        if revenue and revenue > 0 and avg_ar is not None:
            dso = avg_ar / revenue * 365
            if dso > 90:
                dso_interpretation, dso_risk = "Very high DSO - potential collection issues or aggressive revenue recognition", RiskLevel.HIGH
            elif dso > 60:
                dso_interpretation, dso_risk = "Elevated DSO - monitor collection trends", RiskLevel.MODERATE
            elif dso > 45:
                dso_interpretation, dso_risk = "Moderate DSO", RiskLevel.LOW
            else:
                dso_interpretation, dso_risk = "Low DSO - strong collection practices", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Days Sales Outstanding",
                value=dso,
                interpretation=dso_interpretation,
                risk_level=dso_risk,
                methodology=f"Accounts Receivable ({self.basis_note(basis)}) / Revenue x 365"
            ))

        accounts_receivable = balance_sheet.get('accounts_receivable')
        allowance_for_doubtful = balance_sheet.get('allowance_doubtful_accounts',
                                                   statements.notes.get('allowance_doubtful_accounts'))
        if isinstance(allowance_for_doubtful, (int, float)) and accounts_receivable and accounts_receivable > 0 \
                and allowance_for_doubtful > 0:
            allowance_ratio = allowance_for_doubtful / accounts_receivable
            if allowance_ratio < 0.01:
                allowance_interpretation, allowance_risk = "Very low bad debt allowance - potentially aggressive", RiskLevel.MODERATE
            elif allowance_ratio < 0.03:
                allowance_interpretation, allowance_risk = "Low bad debt allowance", RiskLevel.LOW
            elif allowance_ratio < 0.10:
                allowance_interpretation, allowance_risk = "Moderate bad debt allowance", RiskLevel.LOW
            else:
                allowance_interpretation, allowance_risk = "High bad debt allowance - conservative or quality concerns", RiskLevel.MODERATE
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Bad Debt Allowance Ratio",
                value=allowance_ratio,
                interpretation=allowance_interpretation,
                risk_level=allowance_risk,
                methodology="Allowance for Doubtful Accounts / Accounts Receivable"
            ))

        notes = statements.notes
        off_bs_items = [notes.get(k) for k in ('operating_lease_commitments', 'purchase_commitments', 'guarantees')]
        off_bs_items = [v for v in off_bs_items if isinstance(v, (int, float))]
        total_liabilities = balance_sheet.get('total_liabilities')
        if off_bs_items and sum(off_bs_items) > 0 and total_liabilities and total_liabilities > 0:
            off_bs_ratio = sum(off_bs_items) / total_liabilities
            if off_bs_ratio > 0.5:
                obs_interpretation, obs_risk = "Significant off-balance sheet obligations", RiskLevel.MODERATE
            elif off_bs_ratio > 0.2:
                obs_interpretation, obs_risk = "Moderate off-balance sheet commitments", RiskLevel.LOW
            else:
                obs_interpretation, obs_risk = "Limited off-balance sheet exposure", RiskLevel.LOW
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

        operating_cash_flow = cash_flow.get('operating_cash_flow')
        wc_change = cash_flow.get('working_capital_change')

        if operating_cash_flow and wc_change is not None:
            wc_adjustment_ratio = abs(wc_change) / abs(operating_cash_flow)
            if wc_adjustment_ratio > 0.5:
                wc_interpretation, wc_risk = "Large working capital adjustments - OCF heavily influenced by timing", RiskLevel.MODERATE
            elif wc_adjustment_ratio > 0.25:
                wc_interpretation, wc_risk = "Moderate working capital impact on OCF", RiskLevel.LOW
            else:
                wc_interpretation, wc_risk = "Limited working capital adjustments - stable OCF", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Working Capital Adjustment Ratio",
                value=wc_adjustment_ratio,
                interpretation=wc_interpretation,
                risk_level=wc_risk,
                methodology="|Working Capital Change| / |OCF|"
            ))

        interest_expense = income_statement.get('interest_expense')
        cash_interest_paid = cash_flow.get('interest_paid')
        if interest_expense and interest_expense > 0 and cash_interest_paid and cash_interest_paid > 0:
            interest_accrual = (interest_expense - cash_interest_paid) / interest_expense
            if abs(interest_accrual) > 0.2:
                int_interpretation, int_risk = "Significant difference between interest expense and cash paid", RiskLevel.MODERATE
            else:
                int_interpretation, int_risk = "Interest expense and cash payments aligned", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Interest Accrual Ratio",
                value=interest_accrual,
                interpretation=int_interpretation,
                risk_level=int_risk,
                methodology="(Interest Expense - Cash Interest Paid) / Interest Expense"
            ))

        if comparative_data and len(comparative_data) >= 2 and operating_cash_flow is not None:
            ocf_history = [p.cash_flow['operating_cash_flow'] for p in comparative_data
                           if 'operating_cash_flow' in p.cash_flow] + [operating_cash_flow]
            if len(ocf_history) >= 3:
                positive_ratio = sum(1 for ocf in ocf_history if ocf > 0) / len(ocf_history)
                if positive_ratio == 1.0:
                    ocf_consistency, ocf_risk = "Consistently positive operating cash flow", RiskLevel.LOW
                elif positive_ratio >= 0.75:
                    ocf_consistency, ocf_risk = "Generally positive operating cash flow", RiskLevel.LOW
                elif positive_ratio >= 0.5:
                    ocf_consistency, ocf_risk = "Mixed operating cash flow - inconsistent cash generation", RiskLevel.MODERATE
                else:
                    ocf_consistency, ocf_risk = "Frequently negative operating cash flow - sustainability concerns", RiskLevel.HIGH
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

        revenue = income_statement.get('revenue')
        accounts_receivable = balance_sheet.get('accounts_receivable')
        deferred_revenue = balance_sheet.get('deferred_revenue', 0)
        cash_from_customers = cash_flow.get('cash_from_customers', 0)

        if revenue is None or revenue <= 0:
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

            if prior_revenue and prior_revenue > 0 and prior_ar and prior_ar > 0 and accounts_receivable is not None:
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

        revenue = income_statement.get('revenue')
        if revenue is None or revenue <= 0:
            return results

        operating_expenses = income_statement.get('operating_expenses')
        if comparative_data and len(comparative_data) >= 2 and operating_expenses is not None:
            opex_ratios = []
            for past in comparative_data:
                past_rev = past.income_statement.get('revenue')
                past_opex = past.income_statement.get('operating_expenses')
                if past_rev and past_rev > 0 and past_opex is not None:
                    opex_ratios.append(past_opex / past_rev)
            opex_ratios.append(operating_expenses / revenue)
            if len(opex_ratios) >= 3:
                opex_trend = opex_ratios[-1] - opex_ratios[0]
                if opex_trend < -0.05:
                    opex_interpretation, opex_risk = "Improving operating efficiency - expenses declining as % of revenue", RiskLevel.LOW
                elif opex_trend > 0.05:
                    opex_interpretation, opex_risk = "Operating efficiency declining - expenses rising as % of revenue", RiskLevel.MODERATE
                else:
                    opex_interpretation, opex_risk = "Stable operating expense ratio", RiskLevel.LOW
                results.append(AnalysisResult(
                    analysis_type=AnalysisType.QUALITY,
                    metric_name="Operating Expense Ratio Trend",
                    value=opex_trend,
                    interpretation=opex_interpretation,
                    risk_level=opex_risk,
                    methodology="Change in OpEx/Revenue over analysis period"
                ))

        depreciation = income_statement.get('depreciation', cash_flow.get('depreciation_cf'))
        capex = cash_flow.get('capex')
        if depreciation and depreciation > 0 and capex is not None and capex > 0:
            capex_to_dep = capex / depreciation
            if capex_to_dep < 0.7:
                dep_interpretation, dep_risk = "CapEx below depreciation - may indicate understated depreciation or underinvestment", RiskLevel.MODERATE
            elif capex_to_dep > 2.0:
                dep_interpretation, dep_risk = "CapEx significantly exceeds depreciation - heavy investment or aggressive capitalization", RiskLevel.LOW
            else:
                dep_interpretation, dep_risk = "CapEx and depreciation reasonably aligned", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="CapEx to Depreciation Ratio",
                value=capex_to_dep,
                interpretation=dep_interpretation,
                risk_level=dep_risk,
                methodology="Capital Expenditures / Depreciation Expense"
            ))

        accrued_expenses = balance_sheet.get('accrued_liabilities')
        prior_accrued = self.prior_value(comparative_data, 'balance_sheet', 'accrued_liabilities')
        if accrued_expenses and accrued_expenses > 0 and prior_accrued and prior_accrued > 0:
            accrued_change = (accrued_expenses - prior_accrued) / prior_accrued
            if accrued_change < -0.2:
                acc_interpretation, acc_risk = "Significant decrease in accrued expenses - may indicate under-accrual boosting earnings", RiskLevel.MODERATE
            elif accrued_change > 0.3:
                acc_interpretation, acc_risk = "Large increase in accrued expenses - conservative or future obligation", RiskLevel.LOW
            else:
                acc_interpretation, acc_risk = "Stable accrued expense levels", RiskLevel.LOW
            results.append(AnalysisResult(
                analysis_type=AnalysisType.QUALITY,
                metric_name="Accrued Expenses Change",
                value=accrued_change,
                interpretation=acc_interpretation,
                risk_level=acc_risk,
                methodology="(Current - Prior) / Prior Accrued Liabilities"
            ))

        return results

    def _identify_red_flags(self, statements: FinancialStatements,
                            comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Identify red flags in financial reporting (only checks whose inputs are present)"""
        results = []
        red_flags = []
        checked = []

        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        notes = statements.notes

        def num(v):
            return v if isinstance(v, (int, float)) and not isinstance(v, bool) else None

        net_income = income_statement.get('net_income')
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        revenue = income_statement.get('revenue')

        if net_income is not None and operating_cash_flow is not None:
            checked.append("earnings vs cash flow")
            if net_income > 0 and operating_cash_flow < 0:
                red_flags.append("Positive earnings with negative operating cash flow")

            if comparative_data and len(comparative_data) >= 2:
                gaps = [p.income_statement['net_income'] - p.cash_flow['operating_cash_flow']
                        for p in comparative_data
                        if 'net_income' in p.income_statement and 'operating_cash_flow' in p.cash_flow]
                gaps.append(net_income - operating_cash_flow)
                if len(gaps) >= 3 and gaps[-1] > 0 and gaps[-1] > max(gaps[0], 0) * 1.5:
                    red_flags.append("Widening gap between earnings and operating cash flow")

        rev_growth = self.growth(revenue, self.prior_value(comparative_data, 'income_statement', 'revenue'))
        if rev_growth is not None:
            checked.append("revenue growth")
            if rev_growth > 0.5:
                red_flags.append(f"Unusually high revenue growth ({rev_growth:.1%})")

        q4 = num(notes.get('q4_revenue_percent'))
        if q4 is not None:
            checked.append("Q4 revenue concentration")
            if q4 > 0.35:
                red_flags.append(f"Q4 revenue concentration ({q4:.1%}) may indicate channel stuffing")

        policy_changes = notes.get('accounting_policy_changes')
        if policy_changes is not None:
            checked.append("accounting policy changes")
            count = len(policy_changes) if isinstance(policy_changes, (list, tuple)) else num(policy_changes) or 0
            if count >= 2:
                red_flags.append("Multiple accounting policy changes may obscure performance")

        if 'auditor_change' in notes:
            checked.append("auditor change")
            if notes['auditor_change'] in (True, 1, 'true', 'yes', 'True', 'Yes'):
                red_flags.append("Recent auditor change warrants scrutiny")

        related_party = num(notes.get('related_party_transactions'))
        if related_party is not None and revenue and revenue > 0:
            checked.append("related-party transactions")
            if related_party > revenue * 0.1:
                red_flags.append("Significant related party transactions")

        unusual_items = income_statement.get('unusual_items')
        if unusual_items is not None and net_income:
            checked.append("unusual items")
            if abs(unusual_items) > abs(net_income) * 0.2:
                red_flags.append("Large unusual items impacting earnings")

        audit_fees, prior_fees = num(notes.get('audit_fees')), num(notes.get('prior_audit_fees'))
        if audit_fees is not None and prior_fees:
            checked.append("audit fees")
            if audit_fees < prior_fees * 0.8:
                red_flags.append("Declining audit fees may indicate reduced audit scope")

        if not checked:
            results.append(self.unavailable(AnalysisType.QUALITY, "Red Flag Count",
                                            "none of the red-flag checks had their inputs"))
            return results

        flag_count = len(red_flags)
        if flag_count >= 4:
            overall_interpretation, overall_risk = "Multiple significant red flags identified - high scrutiny required", RiskLevel.HIGH
        elif flag_count >= 2:
            overall_interpretation, overall_risk = "Several red flags present - additional investigation recommended", RiskLevel.MODERATE
        elif flag_count == 1:
            overall_interpretation, overall_risk = "Minor red flags noted - routine monitoring", RiskLevel.LOW
        else:
            overall_interpretation, overall_risk = "No red flags among the checks that could be run", RiskLevel.LOW

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Red Flag Count",
            value=flag_count,
            interpretation=overall_interpretation + f" (checks run: {', '.join(checked)})",
            risk_level=overall_risk,
            limitations=red_flags or None,
            methodology="Count of triggered warning indicators among the checks whose inputs were supplied"
        ))
        return results

    def _assess_overall_quality(self, statements: FinancialStatements,
                                comparative_data: Optional[List[FinancialStatements]] = None) -> List[AnalysisResult]:
        """Composite reporting quality score over the factors that can be evaluated"""
        results = []
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet

        net_income = income_statement.get('net_income')
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        avg_assets, _ = self.average_balance(statements, comparative_data, 'total_assets')

        quality_score = 100
        evaluated = []

        if net_income and operating_cash_flow is not None:
            evaluated.append("cash confirmation")
            if net_income > 0:
                cfo_to_ni = operating_cash_flow / net_income
                if cfo_to_ni < 0.5:
                    quality_score -= 20
                elif cfo_to_ni < 0.8:
                    quality_score -= 10
                elif cfo_to_ni < 1.0:
                    quality_score -= 5
            elif operating_cash_flow < 0:
                quality_score -= 20

            if avg_assets and avg_assets > 0:
                evaluated.append("accruals")
                accrual_ratio = (net_income - operating_cash_flow) / avg_assets
                if accrual_ratio > 0.25:
                    quality_score -= 15
                elif accrual_ratio > 0.15:
                    quality_score -= 10
                elif accrual_ratio > 0.10:
                    quality_score -= 5

        if comparative_data and len(comparative_data) >= 2 and net_income is not None:
            ni_history = [p.income_statement['net_income'] for p in comparative_data
                          if 'net_income' in p.income_statement] + [net_income]
            if len(ni_history) >= 3:
                evaluated.append("earnings stability")
                mean_ni = float(np.mean(ni_history))
                volatility = float(np.std(ni_history)) / abs(mean_ni) if mean_ni != 0 else 1.0
                if volatility > 0.5:
                    quality_score -= 15
                elif volatility > 0.3:
                    quality_score -= 10
                elif volatility > 0.15:
                    quality_score -= 5

        nr_keys = ('unusual_items', 'restructuring_charges', 'impairment_losses', 'gains_losses_disposals',
                   'discontinued_operations', 'extraordinary_items')
        reported = [abs(income_statement[k]) for k in nr_keys if k in income_statement]
        if reported and net_income:
            evaluated.append("non-recurring items")
            non_recurring_ratio = sum(reported) / abs(net_income)
            if non_recurring_ratio > 0.3:
                quality_score -= 10
            elif non_recurring_ratio > 0.15:
                quality_score -= 5

        total_assets = balance_sheet.get('total_assets')
        if total_assets and total_assets > 0 and ('intangible_assets' in balance_sheet or 'goodwill' in balance_sheet):
            evaluated.append("intangible concentration")
            intangible_ratio = (balance_sheet.get('intangible_assets', 0.0) + balance_sheet.get('goodwill', 0.0)) / total_assets
            if intangible_ratio > 0.5:
                quality_score -= 10
            elif intangible_ratio > 0.3:
                quality_score -= 5

        if not evaluated:
            results.append(self.unavailable(AnalysisType.QUALITY, "Overall Financial Reporting Quality Score",
                                            "non-zero net income with operating cash flow, or another scored "
                                            "factor, is required"))
            return results

        quality_score = max(0, min(100, quality_score))
        if quality_score >= 85:
            quality_interpretation = "High financial reporting quality - sustainable and decision-useful"
        elif quality_score >= 70:
            quality_interpretation = "Good reporting quality - earnings likely to persist"
        elif quality_score >= 55:
            quality_interpretation = "Adequate reporting quality - relevant but some concerns"
        else:
            quality_interpretation = "Low quality - meets standards but limited usefulness"

        results.append(AnalysisResult(
            analysis_type=AnalysisType.QUALITY,
            metric_name="Overall Financial Reporting Quality Score",
            value=quality_score,
            interpretation=quality_interpretation + f" (factors evaluated: {', '.join(evaluated)})",
            risk_level=RiskLevel.LOW if quality_score >= 70 else RiskLevel.MODERATE if quality_score >= 50 else RiskLevel.HIGH,
            methodology="100 minus penalties for weak cash confirmation, accruals, earnings volatility, non-recurring items and intangible concentration"
        ))
        return results

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Key quality metrics (missing inputs -> metric omitted)"""
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow
        balance_sheet = statements.balance_sheet
        metrics = {}

        net_income = income_statement.get('net_income')
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        capex = cash_flow.get('capex')
        avg_assets, _ = self.average_balance(statements, comparative_data, 'total_assets')

        if net_income and net_income > 0 and operating_cash_flow is not None:
            metrics['cfo_to_net_income'] = operating_cash_flow / net_income
            if capex is not None:
                metrics['fcf_to_net_income'] = (operating_cash_flow - capex) / net_income
        if avg_assets and avg_assets > 0 and net_income is not None and operating_cash_flow is not None:
            metrics['accrual_ratio'] = (net_income - operating_cash_flow) / avg_assets

        total_assets = balance_sheet.get('total_assets')
        if total_assets and total_assets > 0 and ('intangible_assets' in balance_sheet or 'goodwill' in balance_sheet):
            metrics['intangible_concentration'] = (balance_sheet.get('intangible_assets', 0.0)
                                                   + balance_sheet.get('goodwill', 0.0)) / total_assets

        ar = balance_sheet.get('accounts_receivable')
        revenue = income_statement.get('revenue')
        if revenue and revenue > 0 and ar is not None:
            metrics['days_sales_outstanding'] = ar / revenue * 365

        return metrics

    def create_earnings_quality_assessment(self, statements: FinancialStatements,
                                           comparative_data: Optional[List[FinancialStatements]] = None) -> EarningsQualityAssessment:
        """Earnings quality assessment object; fields that cannot be computed are None."""
        income_statement = statements.income_statement
        cash_flow = statements.cash_flow

        net_income = income_statement.get('net_income')
        operating_cash_flow = cash_flow.get('operating_cash_flow')
        capex = cash_flow.get('capex')
        avg_assets, _ = self.average_balance(statements, comparative_data, 'total_assets')

        accrual_ratio = ((net_income - operating_cash_flow) / avg_assets
                         if None not in (net_income, operating_cash_flow) and avg_assets and avg_assets > 0 else None)
        cfo_to_ni = (operating_cash_flow / net_income
                     if operating_cash_flow is not None and net_income and net_income > 0 else None)
        fcf_to_ni = ((operating_cash_flow - capex) / net_income
                     if cfo_to_ni is not None and capex is not None else None)

        earnings_persistence = None
        if comparative_data and net_income is not None:
            history = [p.income_statement['net_income'] for p in comparative_data
                       if 'net_income' in p.income_statement] + [net_income]
            if len(history) >= 4:
                x, y = np.array(history[:-1], dtype=float), np.array(history[1:], dtype=float)
                if np.var(x) > 0:
                    earnings_persistence = float(np.cov(x, y, bias=True)[0, 1] / np.var(x))

        concerns, red_flags = [], []
        score = None
        if cfo_to_ni is not None or accrual_ratio is not None:
            score = 100
            if cfo_to_ni is not None and cfo_to_ni < 0.5:
                concerns.append("Low cash flow relative to earnings")
                score -= 30
            elif cfo_to_ni is not None and cfo_to_ni < 0.8:
                score -= 15
            if accrual_ratio is not None and accrual_ratio > 0.15:
                concerns.append("High accruals")
                score -= 20
            if earnings_persistence is not None and earnings_persistence < 0.3:
                score -= 15
        if net_income is not None and net_income > 0 and operating_cash_flow is not None and operating_cash_flow < 0:
            red_flags.append("Positive earnings with negative cash flow")

        if score is None:
            quality_level = None
        elif score >= 80:
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
            cash_to_accrual_ratio=cfo_to_ni,
            accrual_persistence=None,   # needs a decomposition of accruals over time
            earnings_persistence=earnings_persistence,
            revenue_quality=None,       # not modelled
            expense_quality=None,       # not modelled
            red_flags=red_flags,
            concerns=concerns,
            cfo_to_net_income=cfo_to_ni,
            fcf_to_net_income=fcf_to_ni
        )

    def calculate_beneish_m_score(self, statements: FinancialStatements,
                                  prior_statements: FinancialStatements) -> ManipulationIndicators:
        """Beneish M-score (see _beneish). Fields are None when the score cannot be computed."""
        b = self._beneish(statements, prior_statements)
        c = b['components']
        m_score = b['m_score']

        risk_level = None
        if m_score is not None:
            threshold = self.quality_thresholds['beneish_m_score']
            if m_score < threshold['low_risk']:
                risk_level = ManipulationRisk.LOW
            elif m_score < threshold['moderate_risk']:
                risk_level = ManipulationRisk.MODERATE
            elif m_score < threshold['elevated_risk']:
                risk_level = ManipulationRisk.ELEVATED
            else:
                risk_level = ManipulationRisk.HIGH

        flags = []
        if c:
            if c['dsri'] > 1.1:
                flags.append("High DSRI indicates disproportionate receivables growth")
            if c['gmi'] > 1.1:
                flags.append("High GMI indicates declining gross margins")
            if c['aqi'] > 1.1:
                flags.append("High AQI indicates deteriorating asset quality")
            if c['sgi'] > 1.5:
                flags.append("Very high sales growth")
            if c['tata'] > 0.05:
                flags.append("High total accruals relative to assets")
            if b['neutral']:
                flags.append(f"Neutral value used for: {', '.join(b['neutral'])}")
        elif b['reason']:
            flags.append(f"Not computed: {b['reason']}")

        return ManipulationIndicators(
            beneish_m_score=m_score,
            manipulation_probability=b['probability'],
            risk_level=risk_level,
            days_sales_receivable_index=c.get('dsri'),
            gross_margin_index=c.get('gmi'),
            asset_quality_index=c.get('aqi'),
            sales_growth_index=c.get('sgi'),
            depreciation_index=c.get('depi'),
            sga_expense_index=c.get('sgai'),
            leverage_index=c.get('lvgi'),
            total_accruals_to_assets=c.get('tata'),
            manipulation_flags=flags
        )
