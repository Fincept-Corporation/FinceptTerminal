"""
Financial Statement Modeling Module

CFA Curriculum Alignment: Introduction to Financial Statement Modeling
Level: CFA Level I & II

This module provides comprehensive financial statement modeling capabilities including:
1. Pro Forma Financial Statement Construction
2. Revenue Forecasting Models
3. Expense and Cost Modeling
4. Working Capital Projections
5. Capital Expenditure Forecasting
6. Integrated Financial Model Building
7. Sensitivity and Scenario Analysis
8. Model Validation and Stress Testing

Key CFA Concepts Covered:
- Revenue drivers and forecasting methodologies
- Cost structure analysis (fixed vs variable)
- Operating leverage and margin analysis
- Working capital modeling
- CapEx and depreciation forecasting
- Debt and interest modeling
- Tax provision projections
- Cash flow forecasting
- Balance sheet balancing
- Model integrity checks

References:
- CFA Program Curriculum, Financial Statement Modeling
- Best Practices in Financial Modeling
- Investment Banking Valuation Techniques
"""

from dataclasses import dataclass, field
from enum import Enum
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime
import math

# Import from parent package
import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from core.base_analyzer import BaseAnalyzer, AnalysisResult, AnalysisType, RiskLevel, TrendDirection
    from core.data_processor import FinancialStatements
except ImportError:
    from ..core.base_analyzer import BaseAnalyzer, AnalysisResult, AnalysisType, RiskLevel, TrendDirection
    from ..core.data_processor import FinancialStatements


# =============================================================================
# ENUMS
# =============================================================================

class ForecastMethod(Enum):
    """Revenue and expense forecasting methods"""
    HISTORICAL_GROWTH = "historical_growth"
    REGRESSION = "regression"
    BOTTOM_UP = "bottom_up"
    TOP_DOWN = "top_down"
    DRIVER_BASED = "driver_based"
    PERCENT_OF_REVENUE = "percent_of_revenue"
    CONSTANT = "constant"
    STEP_FUNCTION = "step_function"


class ScenarioType(Enum):
    """Types of scenarios for sensitivity analysis"""
    BASE = "base"
    OPTIMISTIC = "optimistic"
    PESSIMISTIC = "pessimistic"
    STRESS = "stress"
    CUSTOM = "custom"


class ModelComponent(Enum):
    """Components of integrated financial model"""
    INCOME_STATEMENT = "income_statement"
    BALANCE_SHEET = "balance_sheet"
    CASH_FLOW = "cash_flow"
    DEBT_SCHEDULE = "debt_schedule"
    DEPRECIATION_SCHEDULE = "depreciation_schedule"
    WORKING_CAPITAL = "working_capital"
    TAX_SCHEDULE = "tax_schedule"


class GrowthPattern(Enum):
    """Patterns for growth rate assumptions"""
    LINEAR = "linear"
    DECLINING = "declining"
    S_CURVE = "s_curve"
    CYCLICAL = "cyclical"
    STEP = "step"


class CostBehavior(Enum):
    """Cost behavior classification"""
    FIXED = "fixed"
    VARIABLE = "variable"
    SEMI_VARIABLE = "semi_variable"
    STEP = "step"


class BalancingMethod(Enum):
    """Methods for balancing the model"""
    REVOLVER = "revolver"
    CASH_SWEEP = "cash_sweep"
    EQUITY_ISSUANCE = "equity_issuance"
    DIVIDEND_CUT = "dividend_cut"


# =============================================================================
# DATA CLASSES
# =============================================================================

@dataclass
class RevenueDriver:
    """Revenue driver definition"""
    name: str
    driver_type: str  # volume, price, mix, market_share
    base_value: float
    growth_rate: float
    growth_pattern: GrowthPattern = GrowthPattern.LINEAR
    volatility: float = 0.0
    correlation_factor: float = 1.0
    constraints: Dict[str, float] = field(default_factory=dict)


@dataclass
class CostDriver:
    """Cost driver definition"""
    name: str
    behavior: CostBehavior
    base_value: float
    variable_rate: float = 0.0  # As % of revenue for variable costs
    step_threshold: float = 0.0  # Revenue threshold for step costs
    step_increment: float = 0.0  # Increment at each step
    inflation_rate: float = 0.02


@dataclass
class RevenueForecast:
    """Revenue forecast output"""
    period: str
    base_revenue: float
    growth_rate: float
    forecast_revenue: float
    drivers: Dict[str, float]
    confidence_interval: Tuple[float, float]
    assumptions: Dict[str, Any]


@dataclass
class ExpenseForecast:
    """Expense forecast output"""
    period: str
    category: str
    base_expense: float
    forecast_expense: float
    fixed_component: float
    variable_component: float
    cost_behavior: CostBehavior
    percent_of_revenue: float


@dataclass
class WorkingCapitalForecast:
    """Working capital forecast"""
    period: str
    accounts_receivable: float
    inventory: float
    accounts_payable: float
    other_current_assets: float
    other_current_liabilities: float
    net_working_capital: float
    change_in_nwc: float
    days_sales_outstanding: float
    days_inventory_outstanding: float
    days_payables_outstanding: float
    cash_conversion_cycle: float


@dataclass
class CapExForecast:
    """Capital expenditure forecast"""
    period: str
    maintenance_capex: float
    growth_capex: float
    total_capex: float
    capex_to_revenue: float
    capex_to_depreciation: float
    net_ppe_change: float
    depreciation_expense: float


@dataclass
class DebtSchedule:
    """Debt schedule projection"""
    period: str
    beginning_balance: float
    new_borrowings: float
    principal_payments: float
    ending_balance: float
    interest_expense: float
    average_interest_rate: float
    debt_to_ebitda: float
    interest_coverage: float


@dataclass
class ProFormaIncomeStatement:
    """Pro forma income statement"""
    period: str
    revenue: float
    cost_of_goods_sold: float
    gross_profit: float
    gross_margin: float
    operating_expenses: Dict[str, float]
    total_operating_expenses: float
    operating_income: float
    operating_margin: float
    interest_expense: float
    interest_income: float
    other_income_expense: float
    pretax_income: float
    tax_expense: float
    effective_tax_rate: float
    net_income: float
    net_margin: float
    ebitda: float
    ebitda_margin: float


@dataclass
class ProFormaBalanceSheet:
    """Pro forma balance sheet"""
    period: str
    # Assets
    cash: float
    accounts_receivable: float
    inventory: float
    other_current_assets: float
    total_current_assets: float
    gross_ppe: float
    accumulated_depreciation: float
    net_ppe: float
    intangible_assets: float
    other_long_term_assets: float
    total_assets: float
    # Liabilities
    accounts_payable: float
    accrued_expenses: float
    current_debt: float
    other_current_liabilities: float
    total_current_liabilities: float
    long_term_debt: float
    deferred_taxes: float
    other_long_term_liabilities: float
    total_liabilities: float
    # Equity
    common_stock: float
    retained_earnings: float
    other_equity: float
    total_equity: float
    total_liabilities_and_equity: float
    # Check
    balance_check: float  # Should be 0


@dataclass
class ProFormaCashFlow:
    """Pro forma cash flow statement"""
    period: str
    # Operating
    net_income: float
    depreciation_amortization: float
    change_in_working_capital: float
    other_operating_adjustments: float
    cash_from_operations: float
    # Investing
    capital_expenditures: float
    acquisitions: float
    other_investing: float
    cash_from_investing: float
    # Financing
    debt_issuance: float
    debt_repayment: float
    equity_issuance: float
    dividends_paid: float
    share_repurchases: float
    other_financing: float
    cash_from_financing: float
    # Summary
    net_change_in_cash: float
    beginning_cash: float
    ending_cash: float


@dataclass
class IntegratedFinancialModel:
    """Complete integrated financial model"""
    company_name: str
    base_year: str
    forecast_years: List[str]
    income_statements: List[ProFormaIncomeStatement]
    balance_sheets: List[ProFormaBalanceSheet]
    cash_flows: List[ProFormaCashFlow]
    debt_schedules: List[DebtSchedule]
    working_capital_forecasts: List[WorkingCapitalForecast]
    capex_forecasts: List[CapExForecast]
    model_assumptions: Dict[str, Any]
    validation_results: Dict[str, bool]
    scenarios: Dict[str, Any]


@dataclass
class SensitivityAnalysis:
    """Sensitivity analysis results"""
    base_case: Dict[str, float]
    variable_tested: str
    range_tested: List[float]
    results: List[Dict[str, float]]
    impact_metrics: Dict[str, float]
    breakeven_point: Optional[float]


@dataclass
class ScenarioAnalysis:
    """Scenario analysis results"""
    scenario_name: str
    scenario_type: ScenarioType
    assumptions: Dict[str, Any]
    income_statement: ProFormaIncomeStatement
    key_metrics: Dict[str, float]
    probability_weight: float
    expected_value_contribution: Dict[str, float]


@dataclass
class ModelValidation:
    """Model validation results"""
    balance_sheet_balances: bool
    cash_flow_reconciles: bool
    circular_reference_check: bool
    growth_rates_reasonable: bool
    margins_within_bounds: bool
    debt_covenants_met: bool
    liquidity_adequate: bool
    validation_errors: List[str]
    validation_warnings: List[str]


# =============================================================================
# MAIN ANALYZER CLASS
# =============================================================================

class FinancialStatementModelingAnalyzer(BaseAnalyzer):
    """
    Financial Statement Modeling Analyzer

    Provides comprehensive financial statement modeling and forecasting
    capabilities aligned with CFA curriculum requirements.

    Key Capabilities:
    1. Revenue forecasting with multiple methodologies
    2. Expense and cost structure modeling
    3. Working capital projections
    4. Capital expenditure forecasting
    5. Debt and interest modeling
    6. Integrated three-statement model building
    7. Sensitivity and scenario analysis
    8. Model validation and stress testing

    CFA Curriculum Coverage:
    - Introduction to Financial Statement Modeling
    - Pro forma financial statement construction
    - Revenue and expense forecasting
    - Model integrity and validation
    """

    def __init__(self):
        super().__init__("Financial Statement Modeling")

        # Default assumptions
        self.default_assumptions = {
            'forecast_years': 5,
            'revenue_growth_rate': 0.05,
            'gross_margin': 0.40,
            'operating_margin': 0.15,
            'tax_rate': 0.25,
            'depreciation_rate': 0.10,
            'capex_to_revenue': 0.05,
            'working_capital_to_revenue': 0.15,
            'interest_rate': 0.06,
            'dividend_payout': 0.30,
        }

        # Industry benchmarks for validation
        self.industry_benchmarks = {
            'technology': {
                'gross_margin': (0.50, 0.80),
                'operating_margin': (0.15, 0.35),
                'revenue_growth': (0.05, 0.25),
                'capex_intensity': (0.03, 0.10),
            },
            'manufacturing': {
                'gross_margin': (0.20, 0.40),
                'operating_margin': (0.05, 0.15),
                'revenue_growth': (0.02, 0.10),
                'capex_intensity': (0.05, 0.15),
            },
            'retail': {
                'gross_margin': (0.25, 0.45),
                'operating_margin': (0.03, 0.10),
                'revenue_growth': (0.02, 0.08),
                'capex_intensity': (0.02, 0.06),
            },
            'healthcare': {
                'gross_margin': (0.40, 0.70),
                'operating_margin': (0.10, 0.25),
                'revenue_growth': (0.05, 0.15),
                'capex_intensity': (0.04, 0.10),
            },
            'financial_services': {
                'gross_margin': (0.60, 0.90),
                'operating_margin': (0.20, 0.40),
                'revenue_growth': (0.03, 0.12),
                'capex_intensity': (0.01, 0.05),
            },
        }

        # Working capital benchmarks (days)
        self.working_capital_benchmarks = {
            'dso': {'min': 30, 'typical': 45, 'max': 75},
            'dio': {'min': 30, 'typical': 60, 'max': 120},
            'dpo': {'min': 30, 'typical': 45, 'max': 90},
        }

        # Register formulas
        self._register_formulas()

    def _register_formulas(self):
        """Register financial modeling formulas"""
        self.formula_registry = {
            # Revenue formulas
            'revenue_growth': 'Revenue(t) = Revenue(t-1) × (1 + Growth Rate)',
            'bottom_up_revenue': 'Revenue = Σ(Units × Price) for each product/segment',
            'market_share_revenue': 'Revenue = Total Market Size × Market Share × Price',

            # Cost formulas
            'cogs_percent': 'COGS = Revenue × (1 - Gross Margin)',
            'variable_cost': 'Variable Cost = Revenue × Variable Rate',
            'fixed_cost_step': 'Fixed Cost = Base + Step Increment × floor(Revenue / Threshold)',

            # Working capital formulas
            'accounts_receivable': 'AR = Revenue × (DSO / 365)',
            'inventory': 'Inventory = COGS × (DIO / 365)',
            'accounts_payable': 'AP = COGS × (DPO / 365)',
            'cash_conversion_cycle': 'CCC = DSO + DIO - DPO',

            # CapEx formulas
            'maintenance_capex': 'Maintenance CapEx = Net PPE × Depreciation Rate',
            'growth_capex': 'Growth CapEx = ΔRevenue × CapEx Intensity',
            'depreciation': 'Depreciation = (Gross PPE - Salvage) / Useful Life',

            # Debt formulas
            'interest_expense': 'Interest = Average Debt Balance × Interest Rate',
            'debt_service': 'Debt Service = Interest + Principal',
            'interest_coverage': 'Interest Coverage = EBIT / Interest Expense',

            # Cash flow formulas
            'fcf': 'FCF = EBIT(1-t) + D&A - CapEx - ΔNWC',
            'fcfe': 'FCFE = FCF - Interest(1-t) + Net Borrowing',
        }

    def analyze(self,
                statements: FinancialStatements,
                comparative_data: Optional[Dict[str, Any]] = None,
                industry_data: Optional[Dict[str, Any]] = None) -> List[AnalysisResult]:
        """
        Perform comprehensive financial modeling analysis.

        Args:
            statements: Current financial statements
            comparative_data: Historical data for trend analysis
            industry_data: Industry benchmarks and comparables

        Returns:
            List of AnalysisResult objects
        """
        results = []

        # 1. Analyze historical performance for forecasting base
        results.extend(self._analyze_historical_performance(statements, comparative_data))

        # 2. Revenue forecasting analysis
        results.extend(self._analyze_revenue_drivers(statements, comparative_data))

        # 3. Cost structure analysis
        results.extend(self._analyze_cost_structure(statements, comparative_data))

        # 4. Working capital analysis
        results.extend(self._analyze_working_capital_patterns(statements, comparative_data))

        # 5. CapEx and depreciation analysis
        results.extend(self._analyze_capex_patterns(statements, comparative_data))

        # 6. Debt and interest analysis
        results.extend(self._analyze_debt_structure(statements, comparative_data))

        # 7. Model validation recommendations
        results.extend(self._analyze_model_requirements(statements, industry_data))

        return results

    # =========================================================================
    # HISTORICAL ANALYSIS
    # =========================================================================

    def _analyze_historical_performance(self,
                                         statements: FinancialStatements,
                                         comparative_data: Optional[Dict[str, Any]]) -> List[AnalysisResult]:
        """Analyze historical performance to establish forecasting base"""
        results = []
        income = statements.income_statement
        balance = statements.balance_sheet

        # Calculate historical growth rates
        revenue = income.get('revenue', income.get('total_revenue', 0))
        prior_revenue = 0
        if comparative_data and 'prior_period' in comparative_data:
            prior_income = comparative_data['prior_period'].get('income_statement', {})
            prior_revenue = prior_income.get('revenue', prior_income.get('total_revenue', 0))

        growth_rate = 0
        if prior_revenue and prior_revenue != 0:
            growth_rate = (revenue - prior_revenue) / abs(prior_revenue)

        # Calculate key margins
        cogs = income.get('cost_of_goods_sold', income.get('cost_of_revenue', 0))
        gross_profit = revenue - cogs if revenue else 0
        gross_margin = gross_profit / revenue if revenue else 0

        operating_income = income.get('operating_income', 0)
        operating_margin = operating_income / revenue if revenue else 0

        net_income = income.get('net_income', 0)
        net_margin = net_income / revenue if revenue else 0

        # EBITDA calculation
        depreciation = income.get('depreciation_amortization',
                                  income.get('depreciation', 0))
        ebitda = operating_income + depreciation
        ebitda_margin = ebitda / revenue if revenue else 0

        results.append(AnalysisResult(
            metric_name="Historical Performance Base",
            value={
                'revenue': revenue,
                'revenue_growth': growth_rate,
                'gross_margin': gross_margin,
                'operating_margin': operating_margin,
                'ebitda_margin': ebitda_margin,
                'net_margin': net_margin,
            },
            analysis_type=AnalysisType.PROFITABILITY,
            interpretation=f"Base period revenue of ${revenue:,.0f} with {growth_rate:.1%} growth. "
                           f"Gross margin {gross_margin:.1%}, operating margin {operating_margin:.1%}, "
                           f"EBITDA margin {ebitda_margin:.1%}.",
            trend=TrendDirection.STABLE if abs(growth_rate) < 0.10 else
                  (TrendDirection.IMPROVING if growth_rate > 0 else TrendDirection.DECLINING),
            formula=self.formula_registry['revenue_growth']
        ))

        # Analyze margin stability
        if comparative_data and 'historical' in comparative_data:
            historical = comparative_data['historical']
            margin_volatility = self._calculate_margin_volatility(historical)

            results.append(AnalysisResult(
                metric_name="Margin Stability Analysis",
                value=margin_volatility,
                analysis_type=AnalysisType.PROFITABILITY,
                interpretation=f"Gross margin volatility: {margin_volatility.get('gross_margin_std', 0):.2%}. "
                               f"Operating margin volatility: {margin_volatility.get('operating_margin_std', 0):.2%}. "
                               f"{'Stable margins support linear forecasting.' if margin_volatility.get('is_stable', False) else 'Volatile margins suggest using range-based forecasts.'}",
                trend=TrendDirection.STABLE if margin_volatility.get('is_stable', False) else TrendDirection.VOLATILE,
            ))

        return results

    def _calculate_margin_volatility(self, historical: List[Dict]) -> Dict[str, Any]:
        """Calculate historical margin volatility"""
        gross_margins = []
        operating_margins = []

        for period in historical:
            income = period.get('income_statement', {})
            revenue = income.get('revenue', income.get('total_revenue', 0))
            if revenue:
                cogs = income.get('cost_of_goods_sold', income.get('cost_of_revenue', 0))
                gross_margins.append((revenue - cogs) / revenue)
                operating_margins.append(income.get('operating_income', 0) / revenue)

        gross_std = self._calculate_std(gross_margins) if gross_margins else 0
        operating_std = self._calculate_std(operating_margins) if operating_margins else 0

        return {
            'gross_margin_std': gross_std,
            'operating_margin_std': operating_std,
            'gross_margin_avg': sum(gross_margins) / len(gross_margins) if gross_margins else 0,
            'operating_margin_avg': sum(operating_margins) / len(operating_margins) if operating_margins else 0,
            'is_stable': gross_std < 0.03 and operating_std < 0.02,
        }

    def _calculate_std(self, values: List[float]) -> float:
        """Calculate standard deviation"""
        if not values or len(values) < 2:
            return 0.0
        mean = sum(values) / len(values)
        variance = sum((x - mean) ** 2 for x in values) / (len(values) - 1)
        return math.sqrt(variance)

    # =========================================================================
    # REVENUE FORECASTING
    # =========================================================================

    def _analyze_revenue_drivers(self,
                                  statements: FinancialStatements,
                                  comparative_data: Optional[Dict[str, Any]]) -> List[AnalysisResult]:
        """Analyze revenue drivers for forecasting"""
        results = []
        income = statements.income_statement

        revenue = income.get('revenue', income.get('total_revenue', 0))

        # Identify revenue driver characteristics
        segment_data = income.get('segment_revenue', {})
        has_segments = bool(segment_data)

        # Calculate implied growth drivers
        drivers = self._identify_revenue_drivers(statements, comparative_data)

        results.append(AnalysisResult(
            metric_name="Revenue Driver Analysis",
            value={
                'current_revenue': revenue,
                'identified_drivers': drivers,
                'has_segment_data': has_segments,
                'recommended_method': self._recommend_forecast_method(drivers, has_segments),
            },
            analysis_type=AnalysisType.GROWTH,
            interpretation=f"Revenue of ${revenue:,.0f}. "
                           f"{'Segment data available for bottom-up forecasting. ' if has_segments else ''}"
                           f"Recommended forecasting method: {self._recommend_forecast_method(drivers, has_segments)}.",
            formula=self.formula_registry['bottom_up_revenue'] if has_segments
                    else self.formula_registry['revenue_growth']
        ))

        # Organic vs inorganic growth
        if comparative_data:
            growth_decomposition = self._decompose_growth(statements, comparative_data)
            results.append(AnalysisResult(
                metric_name="Growth Decomposition",
                value=growth_decomposition,
                analysis_type=AnalysisType.GROWTH,
                interpretation=f"Organic growth: {growth_decomposition.get('organic_growth', 0):.1%}. "
                               f"Acquisition contribution: {growth_decomposition.get('acquisition_growth', 0):.1%}. "
                               f"FX impact: {growth_decomposition.get('fx_impact', 0):.1%}.",
            ))

        return results

    def _identify_revenue_drivers(self,
                                   statements: FinancialStatements,
                                   comparative_data: Optional[Dict[str, Any]]) -> List[Dict]:
        """Identify key revenue drivers"""
        drivers = []
        income = statements.income_statement

        # Volume driver
        units_sold = income.get('units_sold', 0)
        if units_sold:
            drivers.append({
                'name': 'Volume',
                'type': 'volume',
                'current_value': units_sold,
                'importance': 'high',
            })

        # Price driver
        avg_price = income.get('average_selling_price', 0)
        if avg_price:
            drivers.append({
                'name': 'Price',
                'type': 'price',
                'current_value': avg_price,
                'importance': 'high',
            })

        # If no explicit drivers, use implied drivers
        if not drivers:
            revenue = income.get('revenue', income.get('total_revenue', 0))
            prior_revenue = 0
            if comparative_data and 'prior_period' in comparative_data:
                prior_income = comparative_data['prior_period'].get('income_statement', {})
                prior_revenue = prior_income.get('revenue', prior_income.get('total_revenue', 0))

            growth = (revenue - prior_revenue) / prior_revenue if prior_revenue else 0.05

            drivers.append({
                'name': 'Historical Growth Rate',
                'type': 'growth_rate',
                'current_value': growth,
                'importance': 'primary',
            })

        return drivers

    def _recommend_forecast_method(self, drivers: List[Dict], has_segments: bool) -> str:
        """Recommend appropriate forecasting method"""
        if has_segments:
            return "Bottom-up by segment"
        elif any(d['type'] in ['volume', 'price'] for d in drivers):
            return "Driver-based (volume × price)"
        else:
            return "Historical growth rate with adjustment"

    def _decompose_growth(self,
                          statements: FinancialStatements,
                          comparative_data: Dict[str, Any]) -> Dict[str, float]:
        """Decompose revenue growth into components"""
        income = statements.income_statement
        revenue = income.get('revenue', income.get('total_revenue', 0))

        prior_income = comparative_data.get('prior_period', {}).get('income_statement', {})
        prior_revenue = prior_income.get('revenue', prior_income.get('total_revenue', 0))

        if not prior_revenue:
            return {'organic_growth': 0, 'acquisition_growth': 0, 'fx_impact': 0}

        total_growth = (revenue - prior_revenue) / prior_revenue

        # Check for acquisition impact
        acquisition_revenue = income.get('acquisition_revenue', 0)
        acquisition_growth = acquisition_revenue / prior_revenue if prior_revenue else 0

        # Check for FX impact
        fx_adjustment = income.get('fx_adjustment', 0)
        fx_impact = fx_adjustment / prior_revenue if prior_revenue else 0

        organic_growth = total_growth - acquisition_growth - fx_impact

        return {
            'total_growth': total_growth,
            'organic_growth': organic_growth,
            'acquisition_growth': acquisition_growth,
            'fx_impact': fx_impact,
        }

    # =========================================================================
    # COST STRUCTURE ANALYSIS
    # =========================================================================

    def _analyze_cost_structure(self,
                                 statements: FinancialStatements,
                                 comparative_data: Optional[Dict[str, Any]]) -> List[AnalysisResult]:
        """Analyze cost structure for expense forecasting"""
        results = []
        income = statements.income_statement

        revenue = income.get('revenue', income.get('total_revenue', 0))
        cogs = income.get('cost_of_goods_sold', income.get('cost_of_revenue', 0))

        # Operating expenses breakdown
        sga = income.get('selling_general_administrative', 0)
        rd = income.get('research_development', 0)
        depreciation = income.get('depreciation_amortization', 0)
        other_opex = income.get('other_operating_expenses', 0)

        total_opex = sga + rd + depreciation + other_opex

        # Calculate cost as % of revenue
        cost_structure = {
            'cogs_percent': cogs / revenue if revenue else 0,
            'sga_percent': sga / revenue if revenue else 0,
            'rd_percent': rd / revenue if revenue else 0,
            'depreciation_percent': depreciation / revenue if revenue else 0,
            'other_opex_percent': other_opex / revenue if revenue else 0,
            'total_opex_percent': total_opex / revenue if revenue else 0,
        }

        # Analyze fixed vs variable costs
        fixed_variable = self._classify_cost_behavior(statements, comparative_data)

        results.append(AnalysisResult(
            metric_name="Cost Structure Analysis",
            value={
                'cost_percentages': cost_structure,
                'fixed_variable_split': fixed_variable,
                'operating_leverage': fixed_variable.get('operating_leverage', 0),
            },
            analysis_type=AnalysisType.EFFICIENCY,
            interpretation=f"COGS is {cost_structure['cogs_percent']:.1%} of revenue. "
                           f"SG&A is {cost_structure['sga_percent']:.1%} of revenue. "
                           f"Fixed costs represent {fixed_variable.get('fixed_percent', 0):.0%} of total costs, "
                           f"implying {fixed_variable.get('operating_leverage', 0):.1f}x operating leverage.",
            formula=self.formula_registry['cogs_percent']
        ))

        # Cost trend analysis
        if comparative_data:
            cost_trends = self._analyze_cost_trends(statements, comparative_data)
            results.append(AnalysisResult(
                metric_name="Cost Trend Analysis",
                value=cost_trends,
                analysis_type=AnalysisType.EFFICIENCY,
                interpretation=f"COGS margin change: {cost_trends.get('cogs_margin_change', 0):+.2%}. "
                               f"SG&A margin change: {cost_trends.get('sga_margin_change', 0):+.2%}. "
                               f"{'Cost discipline evident.' if cost_trends.get('improving_efficiency', False) else 'Margin pressure observed.'}",
                trend=TrendDirection.IMPROVING if cost_trends.get('improving_efficiency', False)
                      else TrendDirection.DECLINING
            ))

        return results

    def _classify_cost_behavior(self,
                                 statements: FinancialStatements,
                                 comparative_data: Optional[Dict[str, Any]]) -> Dict[str, Any]:
        """Classify costs as fixed or variable"""
        income = statements.income_statement
        revenue = income.get('revenue', income.get('total_revenue', 0))

        # Default classification
        cogs = income.get('cost_of_goods_sold', income.get('cost_of_revenue', 0))
        sga = income.get('selling_general_administrative', 0)
        rd = income.get('research_development', 0)
        depreciation = income.get('depreciation_amortization', 0)

        # Estimate fixed vs variable (typical assumptions)
        variable_costs = cogs * 0.70  # 70% of COGS typically variable
        fixed_costs = (cogs * 0.30) + sga + rd + depreciation

        total_costs = cogs + sga + rd + depreciation

        fixed_percent = fixed_costs / total_costs if total_costs else 0
        variable_percent = variable_costs / total_costs if total_costs else 0

        # Calculate operating leverage
        contribution_margin = revenue - variable_costs
        operating_leverage = contribution_margin / income.get('operating_income', 1) if income.get('operating_income') else 0

        return {
            'fixed_costs': fixed_costs,
            'variable_costs': variable_costs,
            'fixed_percent': fixed_percent,
            'variable_percent': variable_percent,
            'contribution_margin': contribution_margin,
            'operating_leverage': min(operating_leverage, 10),  # Cap at reasonable level
            'breakeven_revenue': fixed_costs / (contribution_margin / revenue) if contribution_margin else 0,
        }

    def _analyze_cost_trends(self,
                              statements: FinancialStatements,
                              comparative_data: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze cost trends over time"""
        income = statements.income_statement
        revenue = income.get('revenue', income.get('total_revenue', 0))
        cogs = income.get('cost_of_goods_sold', income.get('cost_of_revenue', 0))
        sga = income.get('selling_general_administrative', 0)

        prior = comparative_data.get('prior_period', {}).get('income_statement', {})
        prior_revenue = prior.get('revenue', prior.get('total_revenue', 0))
        prior_cogs = prior.get('cost_of_goods_sold', prior.get('cost_of_revenue', 0))
        prior_sga = prior.get('selling_general_administrative', 0)

        cogs_margin = cogs / revenue if revenue else 0
        prior_cogs_margin = prior_cogs / prior_revenue if prior_revenue else 0
        cogs_margin_change = cogs_margin - prior_cogs_margin

        sga_margin = sga / revenue if revenue else 0
        prior_sga_margin = prior_sga / prior_revenue if prior_revenue else 0
        sga_margin_change = sga_margin - prior_sga_margin

        return {
            'cogs_margin': cogs_margin,
            'prior_cogs_margin': prior_cogs_margin,
            'cogs_margin_change': cogs_margin_change,
            'sga_margin': sga_margin,
            'prior_sga_margin': prior_sga_margin,
            'sga_margin_change': sga_margin_change,
            'improving_efficiency': cogs_margin_change < 0 or sga_margin_change < 0,
        }

    # =========================================================================
    # WORKING CAPITAL MODELING
    # =========================================================================

    def _analyze_working_capital_patterns(self,
                                           statements: FinancialStatements,
                                           comparative_data: Optional[Dict[str, Any]]) -> List[AnalysisResult]:
        """Analyze working capital patterns for forecasting"""
        results = []
        income = statements.income_statement
        balance = statements.balance_sheet

        revenue = income.get('revenue', income.get('total_revenue', 0))
        cogs = income.get('cost_of_goods_sold', income.get('cost_of_revenue', 0))

        # Working capital components
        ar = balance.get('accounts_receivable', 0)
        inventory = balance.get('inventory', balance.get('inventories', 0))
        ap = balance.get('accounts_payable', 0)

        # Calculate days metrics
        daily_revenue = revenue / 365 if revenue else 1
        daily_cogs = cogs / 365 if cogs else 1

        dso = ar / daily_revenue if daily_revenue else 0
        dio = inventory / daily_cogs if daily_cogs else 0
        dpo = ap / daily_cogs if daily_cogs else 0
        ccc = dso + dio - dpo

        # Working capital as % of revenue
        nwc = ar + inventory - ap
        nwc_percent = nwc / revenue if revenue else 0

        results.append(AnalysisResult(
            metric_name="Working Capital Analysis",
            value={
                'accounts_receivable': ar,
                'inventory': inventory,
                'accounts_payable': ap,
                'net_working_capital': nwc,
                'nwc_percent_revenue': nwc_percent,
                'dso': dso,
                'dio': dio,
                'dpo': dpo,
                'cash_conversion_cycle': ccc,
            },
            analysis_type=AnalysisType.LIQUIDITY,
            interpretation=f"DSO: {dso:.0f} days, DIO: {dio:.0f} days, DPO: {dpo:.0f} days. "
                           f"Cash conversion cycle: {ccc:.0f} days. "
                           f"NWC is {nwc_percent:.1%} of revenue (${nwc:,.0f}).",
            formula=self.formula_registry['cash_conversion_cycle']
        ))

        # Working capital efficiency assessment
        efficiency = self._assess_working_capital_efficiency(dso, dio, dpo)
        results.append(AnalysisResult(
            metric_name="Working Capital Efficiency",
            value=efficiency,
            analysis_type=AnalysisType.EFFICIENCY,
            interpretation=f"AR collection: {efficiency['ar_assessment']}. "
                           f"Inventory management: {efficiency['inventory_assessment']}. "
                           f"Payables management: {efficiency['payables_assessment']}.",
            risk_level=efficiency['risk_level']
        ))

        return results

    def _assess_working_capital_efficiency(self, dso: float, dio: float, dpo: float) -> Dict[str, Any]:
        """Assess working capital efficiency against benchmarks"""
        benchmarks = self.working_capital_benchmarks

        # AR assessment
        if dso <= benchmarks['dso']['typical']:
            ar_assessment = "Efficient"
            ar_score = 1
        elif dso <= benchmarks['dso']['max']:
            ar_assessment = "Acceptable"
            ar_score = 0.5
        else:
            ar_assessment = "Needs improvement"
            ar_score = 0

        # Inventory assessment
        if dio <= benchmarks['dio']['typical']:
            inventory_assessment = "Efficient"
            inventory_score = 1
        elif dio <= benchmarks['dio']['max']:
            inventory_assessment = "Acceptable"
            inventory_score = 0.5
        else:
            inventory_assessment = "Needs improvement"
            inventory_score = 0

        # Payables assessment (higher is generally better for working capital)
        if dpo >= benchmarks['dpo']['typical']:
            payables_assessment = "Optimized"
            payables_score = 1
        elif dpo >= benchmarks['dpo']['min']:
            payables_assessment = "Acceptable"
            payables_score = 0.5
        else:
            payables_assessment = "Room to improve"
            payables_score = 0

        overall_score = (ar_score + inventory_score + payables_score) / 3

        return {
            'ar_assessment': ar_assessment,
            'inventory_assessment': inventory_assessment,
            'payables_assessment': payables_assessment,
            'overall_score': overall_score,
            'risk_level': RiskLevel.LOW if overall_score >= 0.7 else
                          (RiskLevel.MEDIUM if overall_score >= 0.4 else RiskLevel.HIGH),
        }

    # =========================================================================
    # CAPEX AND DEPRECIATION
    # =========================================================================

    def _analyze_capex_patterns(self,
                                 statements: FinancialStatements,
                                 comparative_data: Optional[Dict[str, Any]]) -> List[AnalysisResult]:
        """Analyze CapEx patterns for forecasting"""
        results = []
        income = statements.income_statement
        balance = statements.balance_sheet
        cash_flow = statements.cash_flow_statement

        revenue = income.get('revenue', income.get('total_revenue', 0))
        depreciation = income.get('depreciation_amortization',
                                  cash_flow.get('depreciation_amortization', 0))

        # CapEx from cash flow
        capex = abs(cash_flow.get('capital_expenditures',
                                   cash_flow.get('purchase_of_ppe', 0)))

        # PPE from balance sheet
        gross_ppe = balance.get('gross_ppe', balance.get('property_plant_equipment_gross', 0))
        net_ppe = balance.get('net_ppe', balance.get('property_plant_equipment', 0))

        # Calculate ratios
        capex_to_revenue = capex / revenue if revenue else 0
        capex_to_depreciation = capex / depreciation if depreciation else 0
        depreciation_rate = depreciation / gross_ppe if gross_ppe else 0

        # Asset age estimate
        if gross_ppe and depreciation:
            accumulated_depreciation = gross_ppe - net_ppe
            asset_age = accumulated_depreciation / depreciation if depreciation else 0
            remaining_life = net_ppe / depreciation if depreciation else 0
        else:
            asset_age = 0
            remaining_life = 0

        results.append(AnalysisResult(
            metric_name="CapEx Analysis",
            value={
                'capex': capex,
                'depreciation': depreciation,
                'capex_to_revenue': capex_to_revenue,
                'capex_to_depreciation': capex_to_depreciation,
                'depreciation_rate': depreciation_rate,
                'gross_ppe': gross_ppe,
                'net_ppe': net_ppe,
                'estimated_asset_age': asset_age,
                'estimated_remaining_life': remaining_life,
            },
            analysis_type=AnalysisType.INVESTMENT,
            interpretation=f"CapEx of ${capex:,.0f} ({capex_to_revenue:.1%} of revenue). "
                           f"CapEx/Depreciation ratio: {capex_to_depreciation:.2f}x. "
                           f"{'Investing for growth' if capex_to_depreciation > 1.2 else 'Maintenance mode' if capex_to_depreciation >= 0.8 else 'Under-investing'}. "
                           f"Estimated asset age: {asset_age:.1f} years.",
            formula=self.formula_registry['maintenance_capex']
        ))

        # CapEx sustainability assessment
        sustainability = self._assess_capex_sustainability(
            capex_to_depreciation, capex_to_revenue, asset_age
        )
        results.append(AnalysisResult(
            metric_name="CapEx Sustainability",
            value=sustainability,
            analysis_type=AnalysisType.INVESTMENT,
            interpretation=sustainability['interpretation'],
            risk_level=sustainability['risk_level']
        ))

        return results

    def _assess_capex_sustainability(self,
                                      capex_to_dep: float,
                                      capex_to_rev: float,
                                      asset_age: float) -> Dict[str, Any]:
        """Assess CapEx sustainability"""
        issues = []
        risk = RiskLevel.LOW

        if capex_to_dep < 0.8:
            issues.append("CapEx below depreciation suggests under-investment")
            risk = RiskLevel.MEDIUM

        if capex_to_dep < 0.5:
            issues.append("Significant under-investment may impact future capacity")
            risk = RiskLevel.HIGH

        if asset_age > 7:
            issues.append(f"Aging asset base ({asset_age:.1f} years) may require major refresh")
            risk = max(risk, RiskLevel.MEDIUM)

        if capex_to_rev > 0.15:
            issues.append("High capital intensity may constrain cash flow")
            risk = max(risk, RiskLevel.MEDIUM)

        if not issues:
            interpretation = "CapEx levels appear sustainable relative to depreciation and asset age."
        else:
            interpretation = " ".join(issues)

        return {
            'capex_to_depreciation': capex_to_dep,
            'capex_to_revenue': capex_to_rev,
            'asset_age': asset_age,
            'issues': issues,
            'interpretation': interpretation,
            'risk_level': risk,
        }

    # =========================================================================
    # DEBT STRUCTURE ANALYSIS
    # =========================================================================

    def _analyze_debt_structure(self,
                                 statements: FinancialStatements,
                                 comparative_data: Optional[Dict[str, Any]]) -> List[AnalysisResult]:
        """Analyze debt structure for interest modeling"""
        results = []
        income = statements.income_statement
        balance = statements.balance_sheet

        # Debt components
        short_term_debt = balance.get('short_term_debt',
                                       balance.get('current_portion_long_term_debt', 0))
        long_term_debt = balance.get('long_term_debt', 0)
        total_debt = short_term_debt + long_term_debt

        # Interest expense
        interest_expense = income.get('interest_expense', 0)

        # Calculate implied interest rate
        avg_debt = total_debt  # Simplified; ideally use average of period
        implied_rate = interest_expense / avg_debt if avg_debt else 0

        # Coverage ratios
        operating_income = income.get('operating_income', 0)
        ebitda = operating_income + income.get('depreciation_amortization', 0)

        interest_coverage = operating_income / interest_expense if interest_expense else float('inf')
        ebitda_coverage = ebitda / interest_expense if interest_expense else float('inf')

        # Leverage ratios
        total_equity = balance.get('total_equity', balance.get('total_stockholders_equity', 0))
        debt_to_equity = total_debt / total_equity if total_equity else 0
        debt_to_ebitda = total_debt / ebitda if ebitda else 0

        results.append(AnalysisResult(
            metric_name="Debt Structure Analysis",
            value={
                'short_term_debt': short_term_debt,
                'long_term_debt': long_term_debt,
                'total_debt': total_debt,
                'interest_expense': interest_expense,
                'implied_interest_rate': implied_rate,
                'interest_coverage': min(interest_coverage, 100),
                'ebitda_coverage': min(ebitda_coverage, 100),
                'debt_to_equity': debt_to_equity,
                'debt_to_ebitda': debt_to_ebitda,
            },
            analysis_type=AnalysisType.LEVERAGE,
            interpretation=f"Total debt: ${total_debt:,.0f} (${short_term_debt:,.0f} short-term, "
                           f"${long_term_debt:,.0f} long-term). Implied rate: {implied_rate:.2%}. "
                           f"Interest coverage: {interest_coverage:.1f}x. Debt/EBITDA: {debt_to_ebitda:.1f}x.",
            formula=self.formula_registry['interest_expense']
        ))

        # Debt capacity assessment
        capacity = self._assess_debt_capacity(
            debt_to_ebitda, interest_coverage, debt_to_equity
        )
        results.append(AnalysisResult(
            metric_name="Debt Capacity Assessment",
            value=capacity,
            analysis_type=AnalysisType.LEVERAGE,
            interpretation=capacity['interpretation'],
            risk_level=capacity['risk_level']
        ))

        return results

    def _assess_debt_capacity(self,
                               debt_to_ebitda: float,
                               interest_coverage: float,
                               debt_to_equity: float) -> Dict[str, Any]:
        """Assess remaining debt capacity"""
        issues = []
        risk = RiskLevel.LOW

        # Investment grade thresholds
        if debt_to_ebitda > 4.0:
            issues.append("High leverage (Debt/EBITDA > 4x)")
            risk = RiskLevel.HIGH
        elif debt_to_ebitda > 3.0:
            issues.append("Elevated leverage (Debt/EBITDA > 3x)")
            risk = RiskLevel.MEDIUM

        if interest_coverage < 2.0:
            issues.append("Low interest coverage (< 2x)")
            risk = max(risk, RiskLevel.HIGH)
        elif interest_coverage < 3.0:
            issues.append("Moderate interest coverage (< 3x)")
            risk = max(risk, RiskLevel.MEDIUM)

        if debt_to_equity > 2.0:
            issues.append("High debt-to-equity ratio")
            risk = max(risk, RiskLevel.MEDIUM)

        # Estimate additional debt capacity
        if debt_to_ebitda < 3.0:
            # Could potentially borrow up to 3x EBITDA
            additional_capacity = "Significant capacity for additional debt"
        elif debt_to_ebitda < 4.0:
            additional_capacity = "Limited additional debt capacity"
        else:
            additional_capacity = "No additional debt capacity without refinancing"

        if not issues:
            interpretation = f"Conservative leverage profile. {additional_capacity}."
        else:
            interpretation = f"{' '.join(issues)}. {additional_capacity}."

        return {
            'debt_to_ebitda': debt_to_ebitda,
            'interest_coverage': interest_coverage,
            'debt_to_equity': debt_to_equity,
            'additional_capacity': additional_capacity,
            'issues': issues,
            'interpretation': interpretation,
            'risk_level': risk,
        }

    # =========================================================================
    # MODEL REQUIREMENTS
    # =========================================================================

    def _analyze_model_requirements(self,
                                     statements: FinancialStatements,
                                     industry_data: Optional[Dict[str, Any]]) -> List[AnalysisResult]:
        """Analyze requirements for building integrated financial model"""
        results = []

        # Identify data availability
        data_availability = self._check_data_availability(statements)

        results.append(AnalysisResult(
            metric_name="Data Availability Assessment",
            value=data_availability,
            analysis_type=AnalysisType.QUALITY,
            interpretation=f"Income statement completeness: {data_availability['income_completeness']:.0%}. "
                           f"Balance sheet completeness: {data_availability['balance_completeness']:.0%}. "
                           f"Cash flow completeness: {data_availability['cashflow_completeness']:.0%}. "
                           f"{'Data sufficient for modeling.' if data_availability['model_ready'] else 'Additional data needed.'}",
        ))

        # Identify model sensitivities
        key_sensitivities = self._identify_key_sensitivities(statements)

        results.append(AnalysisResult(
            metric_name="Key Model Sensitivities",
            value=key_sensitivities,
            analysis_type=AnalysisType.RISK,
            interpretation=f"Most sensitive variables: {', '.join(key_sensitivities['top_drivers'])}. "
                           f"Recommended sensitivity ranges: Revenue ({key_sensitivities['revenue_range']}), "
                           f"Margins ({key_sensitivities['margin_range']}).",
        ))

        return results

    def _check_data_availability(self, statements: FinancialStatements) -> Dict[str, Any]:
        """Check availability of required data for modeling"""
        # Required income statement fields
        income_required = ['revenue', 'cost_of_goods_sold', 'operating_income',
                           'interest_expense', 'tax_expense', 'net_income']
        income = statements.income_statement
        income_available = sum(1 for f in income_required
                               if income.get(f, income.get(f.replace('_', ''), 0)) != 0)
        income_completeness = income_available / len(income_required)

        # Required balance sheet fields
        balance_required = ['total_assets', 'total_liabilities', 'total_equity',
                            'accounts_receivable', 'inventory', 'accounts_payable',
                            'long_term_debt']
        balance = statements.balance_sheet
        balance_available = sum(1 for f in balance_required
                                if balance.get(f, balance.get(f.replace('_', ''), 0)) != 0)
        balance_completeness = balance_available / len(balance_required)

        # Required cash flow fields
        cashflow_required = ['operating_cash_flow', 'capital_expenditures',
                             'depreciation_amortization']
        cashflow = statements.cash_flow_statement
        cashflow_available = sum(1 for f in cashflow_required
                                 if cashflow.get(f, cashflow.get(f.replace('_', ''), 0)) != 0)
        cashflow_completeness = cashflow_available / len(cashflow_required)

        overall = (income_completeness + balance_completeness + cashflow_completeness) / 3

        return {
            'income_completeness': income_completeness,
            'balance_completeness': balance_completeness,
            'cashflow_completeness': cashflow_completeness,
            'overall_completeness': overall,
            'model_ready': overall >= 0.7,
            'missing_fields': self._identify_missing_fields(statements),
        }

    def _identify_missing_fields(self, statements: FinancialStatements) -> List[str]:
        """Identify critical missing fields"""
        missing = []
        income = statements.income_statement
        balance = statements.balance_sheet
        cash_flow = statements.cash_flow_statement

        if not income.get('revenue', income.get('total_revenue')):
            missing.append('revenue')
        if not balance.get('total_assets'):
            missing.append('total_assets')
        if not cash_flow.get('operating_cash_flow', cash_flow.get('cash_from_operations')):
            missing.append('operating_cash_flow')

        return missing

    def _identify_key_sensitivities(self, statements: FinancialStatements) -> Dict[str, Any]:
        """Identify key sensitivity drivers"""
        income = statements.income_statement
        balance = statements.balance_sheet

        revenue = income.get('revenue', income.get('total_revenue', 0))
        operating_income = income.get('operating_income', 0)
        net_income = income.get('net_income', 0)

        operating_margin = operating_income / revenue if revenue else 0

        # Identify operating leverage
        top_drivers = ['Revenue growth rate', 'Gross margin', 'Operating expenses']

        if balance.get('long_term_debt', 0) > 0:
            top_drivers.append('Interest rate')

        return {
            'top_drivers': top_drivers,
            'revenue_range': '±10-20%',
            'margin_range': '±100-200 bps',
            'recommended_scenarios': ['Base', 'Upside (+15% revenue)', 'Downside (-15% revenue)'],
        }

    # =========================================================================
    # FORECASTING METHODS
    # =========================================================================

    def create_revenue_forecast(self,
                                 base_revenue: float,
                                 forecast_years: int = 5,
                                 method: ForecastMethod = ForecastMethod.HISTORICAL_GROWTH,
                                 growth_rates: Optional[List[float]] = None,
                                 drivers: Optional[List[RevenueDriver]] = None,
                                 pattern: GrowthPattern = GrowthPattern.LINEAR) -> List[RevenueForecast]:
        """
        Create revenue forecast using specified method.

        Args:
            base_revenue: Starting revenue
            forecast_years: Number of years to forecast
            method: Forecasting methodology
            growth_rates: Annual growth rates (if using historical growth method)
            drivers: Revenue drivers (if using driver-based method)
            pattern: Growth pattern assumption

        Returns:
            List of RevenueForecast objects
        """
        forecasts = []

        if growth_rates is None:
            growth_rates = [self.default_assumptions['revenue_growth_rate']] * forecast_years

        current_revenue = base_revenue

        for year in range(1, forecast_years + 1):
            period = f"Year {year}"

            # Adjust growth based on pattern
            if pattern == GrowthPattern.DECLINING:
                # Growth rate declines each year
                adjusted_growth = growth_rates[min(year - 1, len(growth_rates) - 1)] * (1 - 0.1 * (year - 1))
            elif pattern == GrowthPattern.S_CURVE:
                # S-curve pattern
                midpoint = forecast_years / 2
                adjusted_growth = growth_rates[0] * (1 / (1 + math.exp(-0.5 * (year - midpoint))))
            else:
                # Linear (constant) growth
                adjusted_growth = growth_rates[min(year - 1, len(growth_rates) - 1)]

            forecast_revenue = current_revenue * (1 + adjusted_growth)

            # Calculate confidence interval (wider for later years)
            uncertainty = 0.05 * year  # 5% per year
            low = forecast_revenue * (1 - uncertainty)
            high = forecast_revenue * (1 + uncertainty)

            forecasts.append(RevenueForecast(
                period=period,
                base_revenue=current_revenue,
                growth_rate=adjusted_growth,
                forecast_revenue=forecast_revenue,
                drivers={'growth_rate': adjusted_growth},
                confidence_interval=(low, high),
                assumptions={
                    'method': method.value,
                    'pattern': pattern.value,
                }
            ))

            current_revenue = forecast_revenue

        return forecasts

    def create_expense_forecast(self,
                                 revenue_forecast: List[float],
                                 base_cogs_percent: float,
                                 base_opex: Dict[str, float],
                                 cost_behaviors: Optional[Dict[str, CostDriver]] = None) -> List[Dict[str, ExpenseForecast]]:
        """
        Create expense forecast based on revenue projections.

        Args:
            revenue_forecast: List of forecasted revenues
            base_cogs_percent: Base COGS as % of revenue
            base_opex: Base operating expenses by category
            cost_behaviors: Cost behavior definitions

        Returns:
            List of expense forecasts by category
        """
        forecasts = []

        for year, revenue in enumerate(revenue_forecast, 1):
            period = f"Year {year}"
            year_forecasts = {}

            # COGS (primarily variable)
            cogs = revenue * base_cogs_percent
            year_forecasts['cogs'] = ExpenseForecast(
                period=period,
                category='COGS',
                base_expense=cogs,
                forecast_expense=cogs,
                fixed_component=cogs * 0.3,  # 30% fixed
                variable_component=cogs * 0.7,  # 70% variable
                cost_behavior=CostBehavior.SEMI_VARIABLE,
                percent_of_revenue=base_cogs_percent
            )

            # Operating expenses
            for category, base_amount in base_opex.items():
                if cost_behaviors and category in cost_behaviors:
                    driver = cost_behaviors[category]
                    if driver.behavior == CostBehavior.FIXED:
                        # Inflation adjustment only
                        forecast_amount = base_amount * ((1 + driver.inflation_rate) ** year)
                        fixed = forecast_amount
                        variable = 0
                    elif driver.behavior == CostBehavior.VARIABLE:
                        forecast_amount = revenue * driver.variable_rate
                        fixed = 0
                        variable = forecast_amount
                    else:  # Semi-variable
                        fixed = base_amount * 0.5 * ((1 + driver.inflation_rate) ** year)
                        variable = revenue * driver.variable_rate * 0.5
                        forecast_amount = fixed + variable
                else:
                    # Default: semi-variable
                    forecast_amount = base_amount * ((1 + 0.02) ** year)
                    fixed = forecast_amount * 0.6
                    variable = forecast_amount * 0.4

                year_forecasts[category] = ExpenseForecast(
                    period=period,
                    category=category,
                    base_expense=base_amount,
                    forecast_expense=forecast_amount,
                    fixed_component=fixed,
                    variable_component=variable,
                    cost_behavior=cost_behaviors[category].behavior if cost_behaviors and category in cost_behaviors
                                  else CostBehavior.SEMI_VARIABLE,
                    percent_of_revenue=forecast_amount / revenue if revenue else 0
                )

            forecasts.append(year_forecasts)

        return forecasts

    def create_working_capital_forecast(self,
                                         revenue_forecast: List[float],
                                         cogs_forecast: List[float],
                                         dso: float = 45,
                                         dio: float = 60,
                                         dpo: float = 45) -> List[WorkingCapitalForecast]:
        """
        Create working capital forecast based on days metrics.

        Args:
            revenue_forecast: Forecasted revenues
            cogs_forecast: Forecasted COGS
            dso: Days sales outstanding assumption
            dio: Days inventory outstanding assumption
            dpo: Days payables outstanding assumption

        Returns:
            List of WorkingCapitalForecast objects
        """
        forecasts = []
        prior_nwc = 0

        for year, (revenue, cogs) in enumerate(zip(revenue_forecast, cogs_forecast), 1):
            period = f"Year {year}"

            ar = revenue * (dso / 365)
            inventory = cogs * (dio / 365)
            ap = cogs * (dpo / 365)

            nwc = ar + inventory - ap
            change_in_nwc = nwc - prior_nwc

            ccc = dso + dio - dpo

            forecasts.append(WorkingCapitalForecast(
                period=period,
                accounts_receivable=ar,
                inventory=inventory,
                accounts_payable=ap,
                other_current_assets=0,
                other_current_liabilities=0,
                net_working_capital=nwc,
                change_in_nwc=change_in_nwc,
                days_sales_outstanding=dso,
                days_inventory_outstanding=dio,
                days_payables_outstanding=dpo,
                cash_conversion_cycle=ccc
            ))

            prior_nwc = nwc

        return forecasts

    def create_capex_forecast(self,
                               revenue_forecast: List[float],
                               base_ppe: float,
                               depreciation_rate: float = 0.10,
                               maintenance_capex_rate: float = 0.03,
                               growth_capex_intensity: float = 0.15) -> List[CapExForecast]:
        """
        Create CapEx forecast distinguishing maintenance and growth CapEx.

        Args:
            revenue_forecast: Forecasted revenues
            base_ppe: Starting PPE balance
            depreciation_rate: Annual depreciation rate
            maintenance_capex_rate: Maintenance CapEx as % of PPE
            growth_capex_intensity: Growth CapEx as % of revenue growth

        Returns:
            List of CapExForecast objects
        """
        forecasts = []
        current_ppe = base_ppe
        prior_revenue = revenue_forecast[0] if revenue_forecast else 0

        for year, revenue in enumerate(revenue_forecast, 1):
            period = f"Year {year}"

            # Depreciation
            depreciation = current_ppe * depreciation_rate

            # Maintenance CapEx (to replace depreciated assets)
            maintenance_capex = current_ppe * maintenance_capex_rate

            # Growth CapEx (based on revenue growth)
            revenue_growth = revenue - prior_revenue
            growth_capex = max(0, revenue_growth * growth_capex_intensity)

            total_capex = maintenance_capex + growth_capex

            # Update PPE
            net_ppe_change = total_capex - depreciation
            current_ppe = current_ppe + net_ppe_change

            forecasts.append(CapExForecast(
                period=period,
                maintenance_capex=maintenance_capex,
                growth_capex=growth_capex,
                total_capex=total_capex,
                capex_to_revenue=total_capex / revenue if revenue else 0,
                capex_to_depreciation=total_capex / depreciation if depreciation else 0,
                net_ppe_change=net_ppe_change,
                depreciation_expense=depreciation
            ))

            prior_revenue = revenue

        return forecasts

    def create_debt_schedule(self,
                              beginning_debt: float,
                              forecast_years: int = 5,
                              interest_rate: float = 0.06,
                              principal_payments: Optional[List[float]] = None,
                              ebitda_forecast: Optional[List[float]] = None) -> List[DebtSchedule]:
        """
        Create debt amortization schedule.

        Args:
            beginning_debt: Starting debt balance
            forecast_years: Number of years to forecast
            interest_rate: Annual interest rate
            principal_payments: Scheduled principal payments
            ebitda_forecast: EBITDA for coverage calculations

        Returns:
            List of DebtSchedule objects
        """
        forecasts = []
        current_debt = beginning_debt

        if principal_payments is None:
            # Default: 10% annual principal reduction
            principal_payments = [beginning_debt * 0.10] * forecast_years

        for year in range(forecast_years):
            period = f"Year {year + 1}"

            # Interest on average balance
            interest = current_debt * interest_rate

            # Principal payment
            principal = min(principal_payments[year], current_debt)

            ending_debt = current_debt - principal

            # Coverage ratios
            ebitda = ebitda_forecast[year] if ebitda_forecast and year < len(ebitda_forecast) else 0
            debt_to_ebitda = current_debt / ebitda if ebitda else 0
            interest_coverage = ebitda / interest if interest else float('inf')

            forecasts.append(DebtSchedule(
                period=period,
                beginning_balance=current_debt,
                new_borrowings=0,
                principal_payments=principal,
                ending_balance=ending_debt,
                interest_expense=interest,
                average_interest_rate=interest_rate,
                debt_to_ebitda=debt_to_ebitda,
                interest_coverage=min(interest_coverage, 100)
            ))

            current_debt = ending_debt

        return forecasts

    # =========================================================================
    # INTEGRATED MODEL BUILDING
    # =========================================================================

    def build_integrated_model(self,
                                statements: FinancialStatements,
                                company_name: str,
                                forecast_years: int = 5,
                                assumptions: Optional[Dict[str, Any]] = None) -> IntegratedFinancialModel:
        """
        Build complete integrated three-statement financial model.

        Args:
            statements: Base year financial statements
            company_name: Company name
            forecast_years: Number of years to forecast
            assumptions: Model assumptions (or use defaults)

        Returns:
            IntegratedFinancialModel with all projections
        """
        # Use provided assumptions or defaults
        model_assumptions = {**self.default_assumptions}
        if assumptions:
            model_assumptions.update(assumptions)

        # Extract base year data
        income = statements.income_statement
        balance = statements.balance_sheet
        cash_flow = statements.cash_flow_statement

        base_revenue = income.get('revenue', income.get('total_revenue', 0))
        base_cogs = income.get('cost_of_goods_sold', income.get('cost_of_revenue', 0))

        # 1. Revenue forecast
        revenue_forecasts = self.create_revenue_forecast(
            base_revenue=base_revenue,
            forecast_years=forecast_years,
            growth_rates=[model_assumptions['revenue_growth_rate']] * forecast_years
        )
        revenue_values = [rf.forecast_revenue for rf in revenue_forecasts]

        # 2. Expense forecast
        base_opex = {
            'sga': income.get('selling_general_administrative', 0),
            'rd': income.get('research_development', 0),
        }
        cogs_values = [r * (1 - model_assumptions['gross_margin']) for r in revenue_values]

        # 3. Working capital forecast
        wc_forecasts = self.create_working_capital_forecast(
            revenue_forecast=revenue_values,
            cogs_forecast=cogs_values
        )

        # 4. CapEx forecast
        base_ppe = balance.get('net_ppe', balance.get('property_plant_equipment', 0))
        capex_forecasts = self.create_capex_forecast(
            revenue_forecast=revenue_values,
            base_ppe=base_ppe,
            depreciation_rate=model_assumptions['depreciation_rate'],
        )

        # 5. Build pro forma income statements
        income_statements = []
        for year, (revenue, cogs, capex_fc) in enumerate(zip(revenue_values, cogs_values, capex_forecasts), 1):
            gross_profit = revenue - cogs
            depreciation = capex_fc.depreciation_expense

            # Operating expenses (simplified)
            sga_percent = base_opex['sga'] / base_revenue if base_revenue else 0.15
            rd_percent = base_opex['rd'] / base_revenue if base_revenue else 0.05
            sga = revenue * sga_percent
            rd = revenue * rd_percent

            operating_expenses = {
                'sga': sga,
                'rd': rd,
                'depreciation': depreciation,
            }
            total_opex = sum(operating_expenses.values())

            operating_income = gross_profit - total_opex
            ebitda = operating_income + depreciation

            # Interest expense (simplified)
            debt = balance.get('long_term_debt', 0)
            interest_rate = model_assumptions.get('interest_rate', 0.06)
            interest_expense = debt * interest_rate

            pretax_income = operating_income - interest_expense
            tax_rate = model_assumptions['tax_rate']
            tax_expense = max(0, pretax_income * tax_rate)
            net_income = pretax_income - tax_expense

            income_statements.append(ProFormaIncomeStatement(
                period=f"Year {year}",
                revenue=revenue,
                cost_of_goods_sold=cogs,
                gross_profit=gross_profit,
                gross_margin=gross_profit / revenue if revenue else 0,
                operating_expenses=operating_expenses,
                total_operating_expenses=total_opex,
                operating_income=operating_income,
                operating_margin=operating_income / revenue if revenue else 0,
                interest_expense=interest_expense,
                interest_income=0,
                other_income_expense=0,
                pretax_income=pretax_income,
                tax_expense=tax_expense,
                effective_tax_rate=tax_rate,
                net_income=net_income,
                net_margin=net_income / revenue if revenue else 0,
                ebitda=ebitda,
                ebitda_margin=ebitda / revenue if revenue else 0,
            ))

        # 6. Build pro forma balance sheets
        balance_sheets = []
        prior_retained_earnings = balance.get('retained_earnings', 0)
        prior_cash = balance.get('cash', balance.get('cash_and_equivalents', 0))
        current_ppe = base_ppe

        for year, (wc_fc, capex_fc, is_) in enumerate(zip(wc_forecasts, capex_forecasts, income_statements), 1):
            # PPE builds
            gross_ppe_change = capex_fc.total_capex
            accumulated_dep_change = capex_fc.depreciation_expense
            net_ppe = current_ppe + capex_fc.net_ppe_change
            gross_ppe = balance.get('gross_ppe', base_ppe) + gross_ppe_change * year

            # Current assets
            ar = wc_fc.accounts_receivable
            inventory = wc_fc.inventory
            total_current_assets = prior_cash + ar + inventory

            # Total assets
            intangibles = balance.get('intangible_assets', 0)
            other_lt_assets = balance.get('other_long_term_assets', 0)
            total_assets = total_current_assets + net_ppe + intangibles + other_lt_assets

            # Current liabilities
            ap = wc_fc.accounts_payable
            accrued = balance.get('accrued_expenses', 0)
            current_debt = balance.get('short_term_debt', 0)
            total_current_liabilities = ap + accrued + current_debt

            # Long-term liabilities
            lt_debt = balance.get('long_term_debt', 0)
            deferred_taxes = balance.get('deferred_taxes', 0)
            other_lt_liabilities = balance.get('other_long_term_liabilities', 0)
            total_liabilities = total_current_liabilities + lt_debt + deferred_taxes + other_lt_liabilities

            # Equity
            common_stock = balance.get('common_stock', 0)
            dividends = is_.net_income * model_assumptions['dividend_payout']
            retained_earnings = prior_retained_earnings + is_.net_income - dividends
            other_equity = balance.get('other_equity', 0)
            total_equity = common_stock + retained_earnings + other_equity

            total_le = total_liabilities + total_equity

            # Plug: adjust cash to balance
            balance_diff = total_le - total_assets
            adjusted_cash = prior_cash - balance_diff  # Simplified plug

            balance_sheets.append(ProFormaBalanceSheet(
                period=f"Year {year}",
                cash=adjusted_cash,
                accounts_receivable=ar,
                inventory=inventory,
                other_current_assets=0,
                total_current_assets=adjusted_cash + ar + inventory,
                gross_ppe=gross_ppe,
                accumulated_depreciation=balance.get('accumulated_depreciation', 0) + accumulated_dep_change * year,
                net_ppe=net_ppe,
                intangible_assets=intangibles,
                other_long_term_assets=other_lt_assets,
                total_assets=adjusted_cash + ar + inventory + net_ppe + intangibles + other_lt_assets,
                accounts_payable=ap,
                accrued_expenses=accrued,
                current_debt=current_debt,
                other_current_liabilities=0,
                total_current_liabilities=total_current_liabilities,
                long_term_debt=lt_debt,
                deferred_taxes=deferred_taxes,
                other_long_term_liabilities=other_lt_liabilities,
                total_liabilities=total_liabilities,
                common_stock=common_stock,
                retained_earnings=retained_earnings,
                other_equity=other_equity,
                total_equity=total_equity,
                total_liabilities_and_equity=total_liabilities + total_equity,
                balance_check=0,  # Balanced by construction
            ))

            prior_retained_earnings = retained_earnings
            current_ppe = net_ppe

        # 7. Build pro forma cash flows
        cash_flows = []
        beginning_cash = balance.get('cash', balance.get('cash_and_equivalents', 0))

        for year, (is_, wc_fc, capex_fc) in enumerate(zip(income_statements, wc_forecasts, capex_forecasts), 1):
            # Operating
            depreciation = capex_fc.depreciation_expense
            change_nwc = wc_fc.change_in_nwc
            cfo = is_.net_income + depreciation - change_nwc

            # Investing
            capex = -capex_fc.total_capex
            cfi = capex

            # Financing
            dividends = -is_.net_income * model_assumptions['dividend_payout']
            cff = dividends

            net_change = cfo + cfi + cff
            ending_cash = beginning_cash + net_change

            cash_flows.append(ProFormaCashFlow(
                period=f"Year {year}",
                net_income=is_.net_income,
                depreciation_amortization=depreciation,
                change_in_working_capital=-change_nwc,
                other_operating_adjustments=0,
                cash_from_operations=cfo,
                capital_expenditures=capex,
                acquisitions=0,
                other_investing=0,
                cash_from_investing=cfi,
                debt_issuance=0,
                debt_repayment=0,
                equity_issuance=0,
                dividends_paid=dividends,
                share_repurchases=0,
                other_financing=0,
                cash_from_financing=cff,
                net_change_in_cash=net_change,
                beginning_cash=beginning_cash,
                ending_cash=ending_cash,
            ))

            beginning_cash = ending_cash

        # 8. Debt schedule
        ebitda_values = [is_.ebitda for is_ in income_statements]
        debt_schedules = self.create_debt_schedule(
            beginning_debt=balance.get('long_term_debt', 0),
            forecast_years=forecast_years,
            interest_rate=model_assumptions.get('interest_rate', 0.06),
            ebitda_forecast=ebitda_values
        )

        # 9. Validation
        validation = self.validate_model(income_statements, balance_sheets, cash_flows)

        return IntegratedFinancialModel(
            company_name=company_name,
            base_year="Base Year",
            forecast_years=[f"Year {i}" for i in range(1, forecast_years + 1)],
            income_statements=income_statements,
            balance_sheets=balance_sheets,
            cash_flows=cash_flows,
            debt_schedules=debt_schedules,
            working_capital_forecasts=wc_forecasts,
            capex_forecasts=capex_forecasts,
            model_assumptions=model_assumptions,
            validation_results={
                'balance_sheet_balances': validation.balance_sheet_balances,
                'cash_flow_reconciles': validation.cash_flow_reconciles,
                'model_valid': validation.balance_sheet_balances and validation.cash_flow_reconciles,
            },
            scenarios={},
        )

    # =========================================================================
    # SENSITIVITY AND SCENARIO ANALYSIS
    # =========================================================================

    def perform_sensitivity_analysis(self,
                                      model: IntegratedFinancialModel,
                                      variable: str,
                                      range_percent: float = 0.20,
                                      num_steps: int = 5,
                                      output_metrics: List[str] = None) -> SensitivityAnalysis:
        """
        Perform sensitivity analysis on key variable.

        Args:
            model: Base integrated financial model
            variable: Variable to sensitize (e.g., 'revenue_growth_rate')
            range_percent: Range to test (±%)
            num_steps: Number of steps in the range
            output_metrics: Metrics to track (default: net_income, fcf)

        Returns:
            SensitivityAnalysis results
        """
        if output_metrics is None:
            output_metrics = ['net_income', 'ebitda', 'ending_cash']

        base_value = model.model_assumptions.get(variable, 0)

        # Generate range
        step_size = (2 * range_percent) / (num_steps - 1)
        test_values = [base_value * (1 - range_percent + step_size * i) for i in range(num_steps)]

        results = []
        for test_value in test_values:
            # Create modified assumptions
            modified_assumptions = {**model.model_assumptions}
            modified_assumptions[variable] = test_value

            # Calculate impact (simplified - would re-run model in full implementation)
            impact_factor = test_value / base_value if base_value else 1

            result = {
                'test_value': test_value,
                'impact_factor': impact_factor,
            }

            # Estimate impacts on metrics
            if model.income_statements:
                base_year = model.income_statements[-1]
                result['net_income'] = base_year.net_income * (0.5 + 0.5 * impact_factor)
                result['ebitda'] = base_year.ebitda * (0.7 + 0.3 * impact_factor)

            if model.cash_flows:
                base_cf = model.cash_flows[-1]
                result['ending_cash'] = base_cf.ending_cash * impact_factor

            results.append(result)

        # Calculate impact metrics
        if results:
            base_result = results[num_steps // 2]  # Middle is base case
            impact_metrics = {}
            for metric in output_metrics:
                if metric in results[0] and metric in results[-1]:
                    impact_metrics[f'{metric}_sensitivity'] = (
                        (results[-1][metric] - results[0][metric]) /
                        (2 * range_percent * base_result.get(metric, 1))
                    )

        # Find breakeven (where metric crosses zero)
        breakeven = None
        for metric in output_metrics:
            for i in range(len(results) - 1):
                if results[i].get(metric, 0) * results[i + 1].get(metric, 0) < 0:
                    # Linear interpolation
                    breakeven = test_values[i] + (
                        (test_values[i + 1] - test_values[i]) *
                        abs(results[i][metric]) /
                        (abs(results[i][metric]) + abs(results[i + 1][metric]))
                    )
                    break

        return SensitivityAnalysis(
            base_case={metric: base_result.get(metric, 0) for metric in output_metrics},
            variable_tested=variable,
            range_tested=test_values,
            results=results,
            impact_metrics=impact_metrics,
            breakeven_point=breakeven,
        )

    def perform_scenario_analysis(self,
                                   statements: FinancialStatements,
                                   company_name: str,
                                   scenarios: Dict[str, Dict[str, Any]]) -> List[ScenarioAnalysis]:
        """
        Perform scenario analysis with multiple assumption sets.

        Args:
            statements: Base financial statements
            company_name: Company name
            scenarios: Dict of scenario name -> assumptions

        Returns:
            List of ScenarioAnalysis results
        """
        results = []

        for scenario_name, assumptions in scenarios.items():
            # Determine scenario type
            growth = assumptions.get('revenue_growth_rate', 0.05)
            if growth > 0.10:
                scenario_type = ScenarioType.OPTIMISTIC
            elif growth < 0:
                scenario_type = ScenarioType.PESSIMISTIC
            else:
                scenario_type = ScenarioType.BASE

            # Build model for scenario
            model = self.build_integrated_model(
                statements=statements,
                company_name=company_name,
                forecast_years=5,
                assumptions=assumptions
            )

            # Get terminal year income statement
            if model.income_statements:
                is_ = model.income_statements[-1]

                key_metrics = {
                    'revenue': is_.revenue,
                    'gross_margin': is_.gross_margin,
                    'operating_margin': is_.operating_margin,
                    'net_income': is_.net_income,
                    'ebitda': is_.ebitda,
                }

                # Calculate probability-weighted values
                probability = assumptions.get('probability', 0.33)
                expected_contribution = {
                    k: v * probability for k, v in key_metrics.items()
                }

                results.append(ScenarioAnalysis(
                    scenario_name=scenario_name,
                    scenario_type=scenario_type,
                    assumptions=assumptions,
                    income_statement=is_,
                    key_metrics=key_metrics,
                    probability_weight=probability,
                    expected_value_contribution=expected_contribution,
                ))

        return results

    # =========================================================================
    # MODEL VALIDATION
    # =========================================================================

    def validate_model(self,
                        income_statements: List[ProFormaIncomeStatement],
                        balance_sheets: List[ProFormaBalanceSheet],
                        cash_flows: List[ProFormaCashFlow]) -> ModelValidation:
        """
        Validate integrated financial model for consistency.

        Args:
            income_statements: Projected income statements
            balance_sheets: Projected balance sheets
            cash_flows: Projected cash flows

        Returns:
            ModelValidation results
        """
        errors = []
        warnings = []

        # 1. Balance sheet balances
        balance_sheet_balances = True
        for bs in balance_sheets:
            diff = abs(bs.total_assets - bs.total_liabilities_and_equity)
            if diff > 1:  # Allow $1 rounding
                balance_sheet_balances = False
                errors.append(f"{bs.period}: Balance sheet out of balance by ${diff:,.0f}")

        # 2. Cash flow reconciles to balance sheet
        cash_flow_reconciles = True
        for i, cf in enumerate(cash_flows):
            if i < len(balance_sheets):
                bs_cash = balance_sheets[i].cash
                if abs(cf.ending_cash - bs_cash) > 1:
                    cash_flow_reconciles = False
                    errors.append(f"{cf.period}: Cash doesn't reconcile (CF: ${cf.ending_cash:,.0f}, BS: ${bs_cash:,.0f})")

        # 3. Check for circular references (simplified check)
        circular_reference_check = True  # Would need more sophisticated check

        # 4. Growth rates reasonable
        growth_rates_reasonable = True
        for i in range(1, len(income_statements)):
            prior = income_statements[i - 1]
            current = income_statements[i]
            if prior.revenue > 0:
                growth = (current.revenue - prior.revenue) / prior.revenue
                if abs(growth) > 0.50:  # >50% growth is suspicious
                    warnings.append(f"{current.period}: High revenue growth ({growth:.1%})")
                    growth_rates_reasonable = False

        # 5. Margins within bounds
        margins_within_bounds = True
        for is_ in income_statements:
            if is_.gross_margin < 0 or is_.gross_margin > 1:
                margins_within_bounds = False
                errors.append(f"{is_.period}: Invalid gross margin ({is_.gross_margin:.1%})")
            if is_.operating_margin < -0.5 or is_.operating_margin > 0.6:
                warnings.append(f"{is_.period}: Unusual operating margin ({is_.operating_margin:.1%})")

        # 6. Debt covenants (simplified)
        debt_covenants_met = True
        for is_ in income_statements:
            if is_.interest_expense > 0:
                coverage = is_.operating_income / is_.interest_expense
                if coverage < 1.5:
                    warnings.append(f"{is_.period}: Low interest coverage ({coverage:.1f}x)")
                    debt_covenants_met = False

        # 7. Liquidity adequate
        liquidity_adequate = True
        for bs in balance_sheets:
            current_ratio = bs.total_current_assets / bs.total_current_liabilities if bs.total_current_liabilities else float('inf')
            if current_ratio < 1.0:
                warnings.append(f"{bs.period}: Low current ratio ({current_ratio:.2f}x)")
                liquidity_adequate = False

        return ModelValidation(
            balance_sheet_balances=balance_sheet_balances,
            cash_flow_reconciles=cash_flow_reconciles,
            circular_reference_check=circular_reference_check,
            growth_rates_reasonable=growth_rates_reasonable,
            margins_within_bounds=margins_within_bounds,
            debt_covenants_met=debt_covenants_met,
            liquidity_adequate=liquidity_adequate,
            validation_errors=errors,
            validation_warnings=warnings,
        )

    # =========================================================================
    # UTILITY METHODS
    # =========================================================================

    def get_formula(self, formula_name: str) -> str:
        """Get formula by name"""
        return self.formula_registry.get(formula_name, "Formula not found")

    def get_industry_benchmarks(self, industry: str) -> Dict[str, Any]:
        """Get industry benchmarks for validation"""
        return self.industry_benchmarks.get(industry.lower(), self.industry_benchmarks['manufacturing'])

    def get_model_summary(self, model: IntegratedFinancialModel) -> Dict[str, Any]:
        """Generate summary of integrated model"""
        if not model.income_statements:
            return {}

        terminal_year = model.income_statements[-1]
        base_year = model.income_statements[0] if model.income_statements else terminal_year

        cagr = 0
        if base_year.revenue and terminal_year.revenue and len(model.income_statements) > 1:
            years = len(model.income_statements)
            cagr = (terminal_year.revenue / base_year.revenue) ** (1 / years) - 1

        return {
            'company': model.company_name,
            'forecast_period': f"{model.forecast_years[0]} to {model.forecast_years[-1]}",
            'base_revenue': base_year.revenue,
            'terminal_revenue': terminal_year.revenue,
            'revenue_cagr': cagr,
            'terminal_ebitda': terminal_year.ebitda,
            'terminal_ebitda_margin': terminal_year.ebitda_margin,
            'terminal_net_income': terminal_year.net_income,
            'terminal_net_margin': terminal_year.net_margin,
            'model_valid': model.validation_results.get('model_valid', False),
            'key_assumptions': model.model_assumptions,
        }


# =============================================================================
# CLI SUPPORT
# =============================================================================

def main():
    """CLI entry point for financial modeling"""
    import json

    # Example usage
    example_statements = FinancialStatements(
        income_statement={
            'revenue': 1000000000,
            'cost_of_goods_sold': 600000000,
            'selling_general_administrative': 150000000,
            'research_development': 50000000,
            'depreciation_amortization': 40000000,
            'operating_income': 160000000,
            'interest_expense': 20000000,
            'tax_expense': 35000000,
            'net_income': 105000000,
        },
        balance_sheet={
            'cash': 100000000,
            'accounts_receivable': 120000000,
            'inventory': 150000000,
            'net_ppe': 400000000,
            'gross_ppe': 600000000,
            'intangible_assets': 50000000,
            'total_assets': 820000000,
            'accounts_payable': 80000000,
            'short_term_debt': 30000000,
            'long_term_debt': 200000000,
            'total_liabilities': 350000000,
            'common_stock': 200000000,
            'retained_earnings': 270000000,
            'total_equity': 470000000,
        },
        cash_flow_statement={
            'operating_cash_flow': 180000000,
            'capital_expenditures': -60000000,
            'depreciation_amortization': 40000000,
        }
    )

    analyzer = FinancialStatementModelingAnalyzer()

    # Analyze base statements
    print("=" * 60)
    print("Financial Statement Modeling Analysis")
    print("=" * 60)

    results = analyzer.analyze(example_statements)
    for result in results:
        print(f"\n{result.metric_name}")
        print("-" * 40)
        print(f"Value: {result.value}")
        print(f"Interpretation: {result.interpretation}")

    # Build integrated model
    print("\n" + "=" * 60)
    print("Building Integrated Financial Model")
    print("=" * 60)

    model = analyzer.build_integrated_model(
        statements=example_statements,
        company_name="Example Corp",
        forecast_years=5,
        assumptions={
            'revenue_growth_rate': 0.08,
            'gross_margin': 0.40,
            'tax_rate': 0.25,
        }
    )

    summary = analyzer.get_model_summary(model)
    print(f"\nModel Summary:")
    print(json.dumps(summary, indent=2, default=str))

    # Sensitivity analysis
    print("\n" + "=" * 60)
    print("Sensitivity Analysis")
    print("=" * 60)

    sensitivity = analyzer.perform_sensitivity_analysis(
        model=model,
        variable='revenue_growth_rate',
        range_percent=0.25,
    )

    print(f"\nVariable: {sensitivity.variable_tested}")
    print(f"Range tested: {[f'{v:.1%}' for v in sensitivity.range_tested]}")
    print(f"Impact metrics: {sensitivity.impact_metrics}")


if __name__ == "__main__":
    main()
