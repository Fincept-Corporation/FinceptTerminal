"""
Financial Statement Modeling Module
===================================

Three-statement pro forma model built from the supplied base-year statements
(CFA: Introduction to Financial Statement Modeling).

What it does
  * Derives every driver from the base year (margins, DSO/DIO/DPO, capex and
    depreciation rates, interest rate, payout, tax rate) unless an assumption
    overrides it; ``assumption_sources`` says where each value came from. The one
    driver that cannot be derived from a single year - revenue growth - must be
    supplied or comes from the revenue CAGR of the supplied history.
  * Projects income statement, balance sheet and cash flow statement year by year.
    Cash comes out of the cash flow statement; a revolver keeps cash at or above
    ``min_cash``; the balance sheet balances by construction and the balance and
    cash checks are computed, not asserted.
  * Optional DCF (FCFF discounted at ``wacc`` with a Gordon terminal value at
    ``terminal_growth``) and a sensitivity analysis that rebuilds the model for
    each tested value.

Not available
  * ``FinancialStatementModelingAnalyzer.analyze()`` (the historical "modeling
    readiness" analysis) - it was written against a different analyzer API, could
    not run, and duplicated the statement analyzers. Use the ``analyze_*`` commands
    of Analytics/financial_analysis_cli.py instead.

Entry point
  python Analytics/financial_analysis_cli.py build_model '<json>'  (this file run as a
  script forwards its JSON argument to that command).
"""

if __name__ == "__main__" and not __package__:
    # Run as a script: import through the package so the core classes are the
    # package's own (no second top-level copy of `core`, PEP 366).
    import os as _os
    import sys as _sys
    _analytics = _os.path.dirname(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))))
    for _p in (_analytics, _os.path.dirname(_analytics)):
        if _p not in _sys.path:
            _sys.path.insert(0, _p)
    import finanicalanalysis.specialized_analysis  # noqa: F401
    __package__ = "finanicalanalysis.specialized_analysis"

from dataclasses import dataclass, field, asdict
from enum import Enum
from typing import List, Dict, Optional, Any, Tuple
import math

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
    # Lines added for the integrated model (defaults keep the old constructor valid)
    short_term_investments: float = 0.0
    goodwill: float = 0.0
    revolver: float = 0.0
    minority_interest: float = 0.0
    unreconciled_base_difference: float = 0.0


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
    Three-statement financial model built from supplied statements.

    build_integrated_model() is the working entry point; analyze() is not
    available (see the module docstring).
    """

    def __init__(self, enable_logging: bool = False):
        super().__init__(enable_logging)

        # Defaults used only by the standalone create_*_forecast helpers.
        # build_integrated_model() derives its drivers from the statements instead.
        self.default_assumptions = {
            'forecast_years': 5,
            'revenue_growth_rate': 0.05,
            'depreciation_rate': 0.10,
        }

        # Industry ranges used by validate_model() warnings
        self.industry_benchmarks = {
            'technology': {'gross_margin': (0.50, 0.80), 'operating_margin': (0.15, 0.35),
                           'revenue_growth': (0.05, 0.25), 'capex_intensity': (0.03, 0.10)},
            'manufacturing': {'gross_margin': (0.20, 0.40), 'operating_margin': (0.05, 0.15),
                              'revenue_growth': (0.02, 0.10), 'capex_intensity': (0.05, 0.15)},
            'retail': {'gross_margin': (0.25, 0.45), 'operating_margin': (0.03, 0.10),
                       'revenue_growth': (0.02, 0.08), 'capex_intensity': (0.02, 0.06)},
            'healthcare': {'gross_margin': (0.40, 0.70), 'operating_margin': (0.10, 0.25),
                           'revenue_growth': (0.05, 0.15), 'capex_intensity': (0.04, 0.10)},
            'financial_services': {'gross_margin': (0.60, 0.90), 'operating_margin': (0.20, 0.40),
                                   'revenue_growth': (0.03, 0.12), 'capex_intensity': (0.01, 0.05)},
        }
        self._register_formulas()

    def _register_formulas(self):
        """Formulas used by the model (documentation strings)"""
        self.formula_registry = {
            'revenue_growth': 'Revenue(t) = Revenue(t-1) x (1 + g)',
            'operating_costs': 'COGS, SG&A, R&D, other cash opex = % of revenue (base-year ratios)',
            'depreciation': 'Depreciation(t) = depreciation rate x Gross PPE(t-1) (Net PPE when gross not reported)',
            'accounts_receivable': 'AR = Revenue x DSO / 365',
            'inventory': 'Inventory = COGS x DIO / 365',
            'accounts_payable': 'AP = COGS x DPO / 365',
            'interest_expense': 'Interest(t) = rate x debt at the start of year t (no circularity)',
            'tax': 'Tax = max(0, pre-tax income) x tax rate (no loss carry-forward modelled)',
            'cash': 'Cash(t) = Cash(t-1) + CFO + CFI + CFF; revolver keeps Cash >= min_cash',
            'fcff': 'FCFF = EBIT x (1 - t) + D&A - CapEx - change in NWC',
            'fcfe': 'FCFE = CFO - CapEx + net borrowing',
            'terminal_value': 'TV = FCFF(N) x (1 + g) / (WACC - g)',
        }

    def analyze(self, statements: FinancialStatements,
                comparative_data: Optional[List[FinancialStatements]] = None,
                industry_data: Optional[Dict] = None) -> List[AnalysisResult]:
        """Not available: the historical 'modeling readiness' analysis was written against
        another analyzer API and duplicated the statement analyzers. Use build_integrated_model()
        for projections and the statement analyzers for historical ratios."""
        raise NotImplementedError(
            "FinancialStatementModelingAnalyzer.analyze is not available; use build_integrated_model() "
            "(CLI: build_model) for projections and analyze_income / analyze_balance / analyze_cashflow "
            "for historical ratios")

    def get_key_metrics(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]] = None,
                        unavailable: Optional[Dict[str, str]] = None) -> Dict[str, float]:
        """Model drivers that can be derived from the base year (no defaults)."""
        drivers, _, _ = self._derive_drivers(statements, comparative_data, {}, strict=False)
        return {k: v for k, v in drivers.items() if isinstance(v, (int, float)) and not isinstance(v, bool)}

    # =========================================================================
    # FORECASTING METHODS (standalone helpers, not used by build_integrated_model)
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
                                         dpo: float = 45,
                                         base_nwc: Optional[float] = None) -> List[WorkingCapitalForecast]:
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
        prior_nwc = base_nwc  # base-year NWC; year-1 change is None without it

        for year, (revenue, cogs) in enumerate(zip(revenue_forecast, cogs_forecast), 1):
            period = f"Year {year}"

            ar = revenue * (dso / 365)
            inventory = cogs * (dio / 365)
            ap = cogs * (dpo / 365)

            nwc = ar + inventory - ap
            change_in_nwc = nwc - prior_nwc if prior_nwc is not None else None

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

    ASSUMPTION_KEYS = {
        'revenue_growth_rate', 'gross_margin', 'cogs_to_revenue', 'sga_to_revenue', 'rd_to_revenue',
        'other_opex_to_revenue', 'ebitda_margin', 'depreciation_rate', 'capex_to_revenue',
        'dso', 'dio', 'dpo', 'other_current_assets_to_revenue', 'accrued_to_revenue',
        'other_current_liabilities_to_revenue', 'tax_rate', 'interest_rate', 'interest_income_rate',
        'dividend_payout', 'min_cash', 'revolver_rate', 'debt_repayment', 'wacc', 'terminal_growth',
        'probability',
    }

    def _derive_drivers(self, statements: FinancialStatements,
                        comparative_data: Optional[List[FinancialStatements]],
                        assumptions: Dict[str, Any], strict: bool = True) -> Tuple[Dict, Dict, Dict]:
        """Base-year values and model drivers.

        Returns (drivers, sources, base). Every driver is either taken from
        `assumptions` or derived from the statements; `sources` records which.
        With strict=True a driver that can be neither raises ValueError.
        """
        inc, bs, cf = statements.income_statement, statements.balance_sheet, statements.cash_flow
        unknown = set(assumptions) - self.ASSUMPTION_KEYS
        if unknown:
            raise ValueError(f"unknown assumption(s): {', '.join(sorted(unknown))}; "
                             f"allowed: {', '.join(sorted(self.ASSUMPTION_KEYS - {'probability'}))}")

        def need(cond, message):
            if not cond and strict:
                raise ValueError(message)
            return cond

        drivers: Dict[str, Any] = {}
        sources: Dict[str, str] = {}

        def put(name, derived_value, derived_source):
            if name in assumptions and assumptions[name] is not None:
                drivers[name] = assumptions[name]
                sources[name] = "assumption"
            elif derived_value is not None:
                drivers[name] = derived_value
                sources[name] = derived_source

        rev0 = inc.get('revenue')
        if not need(rev0 is not None and rev0 > 0, "base-year revenue (positive) is required"):
            return drivers, sources, {}
        ebit0 = inc.get('operating_income')
        need(ebit0 is not None, "base-year operating income is required (the model is built for operating "
                                "companies; banks and insurers do not report operating income)")
        for k in ('total_assets', 'total_liabilities', 'total_equity'):
            need(k in bs, f"base-year {k} is required")
        if not strict and (ebit0 is None or any(k not in bs for k in ('total_assets', 'total_liabilities', 'total_equity'))):
            return drivers, sources, {}

        # ---- PPE and depreciation
        ppe_net0 = bs.get('ppe_net', 0.0)
        ppe_gross0 = bs.get('ppe_gross')
        accdep0 = bs.get('accumulated_depreciation')
        dep0 = inc.get('depreciation', cf.get('depreciation_cf'))
        dep0 = abs(dep0) if dep0 is not None else None
        if ppe_gross0 and ppe_gross0 > 0 and accdep0 is not None:
            dep_basis = 'gross'
            dep_base_balance = ppe_gross0
        elif ppe_net0 and ppe_net0 > 0:
            dep_basis = 'net'
            dep_base_balance = ppe_net0
        else:
            dep_basis = 'revenue'
            dep_base_balance = rev0
        put('depreciation_rate', dep0 / dep_base_balance if dep0 is not None else None,
            f"base-year depreciation / {'gross PPE' if dep_basis == 'gross' else 'net PPE' if dep_basis == 'net' else 'revenue (no PPE reported)'}")
        need('depreciation_rate' in drivers, "depreciation not reported; supply assumptions.depreciation_rate")
        if dep0 is None and 'depreciation_rate' in drivers:
            dep0 = drivers['depreciation_rate'] * dep_base_balance
        dep0 = dep0 or 0.0

        # ---- operating cost structure (all % of revenue)
        ebitda0 = (ebit0 or 0.0) + dep0
        cogs0 = None
        gp0 = self.gross_profit(inc)
        if gp0 is not None:
            cogs0 = rev0 - gp0
        sga0, rd0 = self.sga(inc), inc.get('rd_expenses')
        if 'gross_margin' in assumptions and 'cogs_to_revenue' not in assumptions and assumptions['gross_margin'] is not None:
            drivers['cogs_to_revenue'] = 1.0 - float(assumptions['gross_margin'])
            sources['cogs_to_revenue'] = "1 - assumptions.gross_margin"
        else:
            put('cogs_to_revenue', cogs0 / rev0 if cogs0 is not None else None, "base-year cost of sales / revenue")
        put('sga_to_revenue', sga0 / rev0 if sga0 is not None else 0.0,
            "base-year SG&A / revenue" if sga0 is not None else "SG&A not reported (included in other operating costs)")
        put('rd_to_revenue', rd0 / rev0 if rd0 is not None else 0.0,
            "base-year R&D / revenue" if rd0 is not None else "R&D not reported (included in other operating costs)")
        base_cogs_pct = cogs0 / rev0 if cogs0 is not None else 0.0
        base_other = (1.0 - ebitda0 / rev0 - base_cogs_pct - (sga0 / rev0 if sga0 is not None else 0.0)
                      - (rd0 / rev0 if rd0 is not None else 0.0))
        if abs(base_other) < 1e-12:
            base_other = 0.0  # floating-point noise from the subtraction
        if 'ebitda_margin' in assumptions and assumptions['ebitda_margin'] is not None \
                and 'other_opex_to_revenue' not in assumptions:
            drivers['other_opex_to_revenue'] = (1.0 - float(assumptions['ebitda_margin'])
                                                - drivers.get('cogs_to_revenue', 0.0)
                                                - drivers['sga_to_revenue'] - drivers['rd_to_revenue'])
            sources['other_opex_to_revenue'] = "balancing item so EBITDA margin = assumptions.ebitda_margin"
        else:
            put('other_opex_to_revenue', base_other,
                "balancing item: base-year EBITDA (operating income + D&A) is reproduced exactly")

        # ---- capex
        capex0 = cf.get('capex')
        put('capex_to_revenue', capex0 / rev0 if capex0 is not None else None, "base-year capex / revenue")
        need('capex_to_revenue' in drivers, "capital expenditure not reported; supply assumptions.capex_to_revenue")

        # ---- working capital
        cogs_for_days = cogs0 if cogs0 and cogs0 > 0 else None
        days_base = "cost of sales" if cogs_for_days else "revenue (cost of sales not reported)"
        ar0, inv0, ap0 = bs.get('accounts_receivable', 0.0), bs.get('inventory', 0.0), bs.get('accounts_payable', 0.0)
        put('dso', ar0 / rev0 * 365, "base-year receivables / revenue x 365")
        put('dio', inv0 / (cogs_for_days or rev0) * 365, f"base-year inventory / {days_base} x 365")
        put('dpo', ap0 / (cogs_for_days or rev0) * 365, f"base-year payables / {days_base} x 365")

        cash0 = bs.get('cash_equivalents', 0.0)
        sti0 = bs.get('short_term_investments', 0.0)
        ca0 = bs.get('current_assets')
        oca0 = ca0 - (cash0 + sti0 + ar0 + inv0) if ca0 is not None else 0.0
        cl0 = bs.get('current_liabilities')
        accr0 = bs.get('accrued_liabilities', 0.0)
        std0 = bs.get('short_term_debt', 0.0)
        ocl0 = cl0 - (ap0 + accr0 + std0) if cl0 is not None else 0.0
        put('other_current_assets_to_revenue', oca0 / rev0, "base-year other current assets / revenue")
        put('accrued_to_revenue', accr0 / rev0, "base-year accrued liabilities / revenue")
        put('other_current_liabilities_to_revenue', ocl0 / rev0, "base-year other current liabilities / revenue")

        # ---- tax
        pretax0, tax0 = inc.get('pretax_income'), inc.get('tax_expense')
        etr0 = tax0 / pretax0 if pretax0 and pretax0 > 0 and tax0 is not None else None
        if etr0 is not None and 0 <= etr0 <= 0.6:
            put('tax_rate', etr0, "base-year effective tax rate")
        else:
            from .tax_analysis import TaxAnalyzer
            statutory, why = TaxAnalyzer(enable_logging=False)._statutory_rate(statements)
            put('tax_rate', statutory, f"statutory rate ({why}); base-year effective rate not usable")
        need('tax_rate' in drivers, "tax rate cannot be derived (pre-tax loss and unknown country); "
                                    "supply assumptions.tax_rate")

        # ---- financing
        ltd0 = bs.get('long_term_debt', 0.0)
        debt0 = std0 + ltd0
        int0 = inc.get('interest_expense')
        if debt0 > 0 and int0 is not None:
            prior_debt = None
            if comparative_data:
                pb = comparative_data[-1].balance_sheet
                if 'short_term_debt' in pb or 'long_term_debt' in pb:
                    prior_debt = pb.get('short_term_debt', 0.0) + pb.get('long_term_debt', 0.0)
            avg_debt = (debt0 + prior_debt) / 2 if prior_debt else debt0
            put('interest_rate', abs(int0) / avg_debt,
                f"base-year interest expense / {'average' if prior_debt else 'ending'} debt")
        elif debt0 == 0:
            put('interest_rate', 0.0, "no debt reported")
        need('interest_rate' in drivers, "debt is reported but interest expense is not; supply assumptions.interest_rate")
        put('interest_income_rate', 0.0, "interest on cash not modelled")
        put('revolver_rate', drivers.get('interest_rate'), "same as interest_rate")

        ni0 = inc.get('net_income')
        div0 = cf.get('dividends_paid')
        if div0 is not None and ni0 is not None and ni0 > 0:
            put('dividend_payout', min(div0 / ni0, 1.0), "base-year dividends / net income (capped at 100%)")
        else:
            put('dividend_payout', 0.0, "no dividends reported" if div0 is None
                else "base-year net loss: payout not defined, no dividends projected")
        put('min_cash', 0.0, "revolver draws only to prevent negative cash")
        put('debt_repayment', 0.0, "no repayment schedule supplied: term debt held constant")

        # ---- growth
        hist_rev = [p.income_statement.get('revenue') for p in (comparative_data or [])]
        hist_rev = [r for r in hist_rev if r is not None] + [rev0]
        hist_cagr = self.cagr(hist_rev[0], hist_rev[-1], len(hist_rev) - 1) if len(hist_rev) > 1 else None
        put('revenue_growth_rate', hist_cagr, f"revenue CAGR over the {len(hist_rev) - 1} supplied prior period(s)")
        need('revenue_growth_rate' in drivers,
             "revenue growth cannot be derived from a single year; supply assumptions.revenue_growth_rate "
             "or history with positive revenue")

        # ---- base balance sheet decomposition (plugs keep base totals exact)
        ta0, tl0, te0 = bs['total_assets'], bs['total_liabilities'], bs['total_equity']
        mi0 = bs.get('minority_interest', 0.0)
        intang0, gw0 = bs.get('intangible_assets', 0.0), bs.get('goodwill', 0.0)
        other_lta0 = ta0 - (cash0 + sti0 + ar0 + inv0 + oca0 + ppe_net0 + intang0 + gw0)
        cl_total = cl0 if cl0 is not None else ap0 + accr0 + std0
        other_ltl0 = tl0 - cl_total - ltd0
        cs0 = bs.get('common_stock', 0.0)
        re0 = bs.get('retained_earnings', 0.0)
        other_eq0 = te0 - cs0 - re0
        base = {
            'revenue': rev0, 'cogs': cogs0, 'ebitda': ebitda0, 'operating_income': ebit0, 'depreciation': dep0,
            'net_income': ni0, 'cash': cash0, 'short_term_investments': sti0, 'accounts_receivable': ar0,
            'inventory': inv0, 'other_current_assets': oca0, 'ppe_gross': ppe_gross0 if dep_basis == 'gross' else None,
            'accumulated_depreciation': accdep0 if dep_basis == 'gross' else None, 'ppe_net': ppe_net0,
            'intangible_assets': intang0, 'goodwill': gw0, 'other_long_term_assets': other_lta0,
            'total_assets': ta0, 'accounts_payable': ap0, 'accrued_liabilities': accr0, 'short_term_debt': std0,
            'other_current_liabilities': ocl0, 'long_term_debt': ltd0, 'other_long_term_liabilities': other_ltl0,
            'total_liabilities': tl0, 'common_stock': cs0, 'retained_earnings': re0, 'other_equity': other_eq0,
            'total_equity': te0, 'minority_interest': mi0, 'unreconciled_base_difference': ta0 - tl0 - te0 - mi0,
            'depreciation_basis': dep_basis, 'shares': (bs.get('shares_outstanding')
                                                        or inc.get('shares_outstanding_diluted')
                                                        or inc.get('shares_outstanding_basic')),
        }
        return drivers, sources, base

    @staticmethod
    def _per_year(value, years: int, name: str) -> List[float]:
        """Scalar -> repeated; list -> must have one value per forecast year."""
        if isinstance(value, (list, tuple)):
            if len(value) != years:
                raise ValueError(f"assumptions.{name} has {len(value)} values; forecast_years is {years}")
            return [float(v) for v in value]
        return [float(value)] * years

    def build_integrated_model(self,
                               statements: FinancialStatements,
                               company_name: str,
                               forecast_years: int = 5,
                               assumptions: Optional[Dict[str, Any]] = None,
                               comparative_data: Optional[List[FinancialStatements]] = None) -> IntegratedFinancialModel:
        """Build an integrated three-statement projection from the base-year statements.

        Raises ValueError with a plain message when a required input or assumption
        is missing or out of range.
        """
        assumptions = dict(assumptions or {})
        drivers, sources, base = self._derive_drivers(statements, comparative_data, assumptions, strict=True)
        n = int(forecast_years)
        if n < 1:
            raise ValueError("forecast_years must be at least 1")

        growth = self._per_year(drivers['revenue_growth_rate'], n, 'revenue_growth_rate')
        repay = self._per_year(drivers['debt_repayment'], n, 'debt_repayment')
        scalar = {}
        for k in ('cogs_to_revenue', 'sga_to_revenue', 'rd_to_revenue', 'other_opex_to_revenue', 'depreciation_rate',
                  'capex_to_revenue', 'dso', 'dio', 'dpo', 'other_current_assets_to_revenue', 'accrued_to_revenue',
                  'other_current_liabilities_to_revenue', 'tax_rate', 'interest_rate', 'interest_income_rate',
                  'dividend_payout', 'min_cash', 'revolver_rate'):
            v = drivers.get(k, 0.0)
            try:
                scalar[k] = float(v)
            except (TypeError, ValueError):
                raise ValueError(f"assumptions.{k} must be a number")
            if not math.isfinite(scalar[k]):
                raise ValueError(f"assumptions.{k} must be finite")
        if any(g <= -1 for g in growth):
            raise ValueError("revenue_growth_rate must be greater than -100%")
        if not 0 <= scalar['tax_rate'] < 1:
            raise ValueError("tax_rate must be between 0 and 1")
        if not 0 <= scalar['dividend_payout'] <= 1:
            raise ValueError("dividend_payout must be between 0 and 1")
        for k in ('dso', 'dio', 'dpo', 'capex_to_revenue', 'depreciation_rate', 'interest_rate', 'revolver_rate',
                  'min_cash', 'interest_income_rate'):
            if scalar[k] < 0:
                raise ValueError(f"{k} must not be negative")
        if any(r < 0 for r in repay):
            raise ValueError("debt_repayment must not be negative")

        cogs_pct = scalar['cogs_to_revenue']
        has_cogs = 'cogs_to_revenue' in drivers
        dep_basis = base['depreciation_basis']

        # Opening balances (end of base year)
        prev = dict(base)
        prev['revolver'] = 0.0
        prev_nwc_assets = base['accounts_receivable'] + base['inventory'] + base['other_current_assets']
        prev_nwc_liabs = base['accounts_payable'] + base['accrued_liabilities'] + base['other_current_liabilities']
        revenue = base['revenue']

        income_statements, balance_sheets, cash_flows, fcf_rows = [], [], [], []
        warnings = []

        for year in range(1, n + 1):
            period = f"Year {year}"
            revenue = revenue * (1 + growth[year - 1])
            cogs = revenue * cogs_pct if has_cogs else 0.0
            sga = revenue * scalar['sga_to_revenue']
            rd = revenue * scalar['rd_to_revenue']
            other_opex = revenue * scalar['other_opex_to_revenue']
            ebitda = revenue - cogs - sga - rd - other_opex

            if dep_basis == 'gross':
                depreciation = scalar['depreciation_rate'] * prev['ppe_gross']
            elif dep_basis == 'net':
                depreciation = scalar['depreciation_rate'] * prev['ppe_net']
            else:
                depreciation = scalar['depreciation_rate'] * revenue
            operating_income = ebitda - depreciation

            term_debt_open = prev['short_term_debt'] + prev['long_term_debt']
            interest_expense = scalar['interest_rate'] * term_debt_open + scalar['revolver_rate'] * prev['revolver']
            interest_income = scalar['interest_income_rate'] * max(prev['cash'], 0.0)
            pretax_income = operating_income - interest_expense + interest_income
            tax_expense = max(pretax_income, 0.0) * scalar['tax_rate']
            net_income = pretax_income - tax_expense
            dividends = scalar['dividend_payout'] * max(net_income, 0.0)

            # Working capital
            days_base = cogs if has_cogs and cogs > 0 else revenue
            ar = revenue * scalar['dso'] / 365
            inventory = days_base * scalar['dio'] / 365
            ap = days_base * scalar['dpo'] / 365
            oca = revenue * scalar['other_current_assets_to_revenue']
            accrued = revenue * scalar['accrued_to_revenue']
            ocl = revenue * scalar['other_current_liabilities_to_revenue']
            nwc_assets, nwc_liabs = ar + inventory + oca, ap + accrued + ocl
            change_nwc = (nwc_assets - nwc_liabs) - (prev_nwc_assets - prev_nwc_liabs)

            # Fixed assets
            capex = revenue * scalar['capex_to_revenue']
            ppe_net = prev['ppe_net'] + capex - depreciation
            ppe_gross = prev['ppe_gross'] + capex if dep_basis == 'gross' else None
            accdep = prev['accumulated_depreciation'] + depreciation if dep_basis == 'gross' else None
            if ppe_net < 0:
                warnings.append(f"{period}: net PPE turns negative - depreciation rate too high for the capex assumption")

            # Cash flow before revolver
            cfo = net_income + depreciation - change_nwc
            cfi = -capex
            repayment = min(repay[year - 1], prev['long_term_debt'] + prev['short_term_debt'])
            ltd_repay = min(repayment, prev['long_term_debt'])
            std_repay = repayment - ltd_repay
            cash_pre = prev['cash'] + cfo + cfi - dividends - repayment

            draw = repay_rev = 0.0
            if cash_pre < scalar['min_cash']:
                draw = scalar['min_cash'] - cash_pre
            elif prev['revolver'] > 0:
                repay_rev = min(prev['revolver'], cash_pre - scalar['min_cash'])
            revolver = prev['revolver'] + draw - repay_rev
            cash = cash_pre + draw - repay_rev
            if draw > 0:
                warnings.append(f"{period}: revolver draw of {draw:,.2f} needed to keep cash at min_cash")
            cff = -dividends - repayment + draw - repay_rev

            # Balance sheet
            ltd = prev['long_term_debt'] - ltd_repay
            std = prev['short_term_debt'] - std_repay
            retained = prev['retained_earnings'] + net_income - dividends
            total_current_assets = cash + base['short_term_investments'] + ar + inventory + oca
            total_assets = (total_current_assets + ppe_net + base['intangible_assets'] + base['goodwill']
                            + base['other_long_term_assets'])
            total_current_liabilities = ap + accrued + ocl + std
            total_liabilities = total_current_liabilities + ltd + revolver + base['other_long_term_liabilities']
            total_equity = base['common_stock'] + retained + base['other_equity']
            total_le = (total_liabilities + total_equity + base['minority_interest']
                        + base['unreconciled_base_difference'])
            if total_equity < 0:
                warnings.append(f"{period}: shareholders' equity is negative ({total_equity:,.2f})")

            income_statements.append(ProFormaIncomeStatement(
                period=period, revenue=revenue, cost_of_goods_sold=cogs if has_cogs else None,
                gross_profit=revenue - cogs if has_cogs else None,
                gross_margin=(revenue - cogs) / revenue if has_cogs else None,
                operating_expenses={'sga': sga, 'rd': rd, 'other_operating_costs': other_opex,
                                    'depreciation': depreciation},
                total_operating_expenses=sga + rd + other_opex + depreciation,
                operating_income=operating_income, operating_margin=operating_income / revenue,
                interest_expense=interest_expense, interest_income=interest_income, other_income_expense=0.0,
                pretax_income=pretax_income, tax_expense=tax_expense,
                effective_tax_rate=tax_expense / pretax_income if pretax_income > 0 else None,
                net_income=net_income, net_margin=net_income / revenue,
                ebitda=ebitda, ebitda_margin=ebitda / revenue,
            ))
            balance_sheets.append(ProFormaBalanceSheet(
                period=period, cash=cash, accounts_receivable=ar, inventory=inventory, other_current_assets=oca,
                total_current_assets=total_current_assets, gross_ppe=ppe_gross, accumulated_depreciation=accdep,
                net_ppe=ppe_net, intangible_assets=base['intangible_assets'],
                other_long_term_assets=base['other_long_term_assets'], total_assets=total_assets,
                accounts_payable=ap, accrued_expenses=accrued, current_debt=std, other_current_liabilities=ocl,
                total_current_liabilities=total_current_liabilities, long_term_debt=ltd, deferred_taxes=0.0,
                other_long_term_liabilities=base['other_long_term_liabilities'], total_liabilities=total_liabilities,
                common_stock=base['common_stock'], retained_earnings=retained, other_equity=base['other_equity'],
                total_equity=total_equity, total_liabilities_and_equity=total_le,
                balance_check=total_assets - total_le,
                short_term_investments=base['short_term_investments'], goodwill=base['goodwill'],
                revolver=revolver, minority_interest=base['minority_interest'],
                unreconciled_base_difference=base['unreconciled_base_difference'],
            ))
            cash_flows.append(ProFormaCashFlow(
                period=period, net_income=net_income, depreciation_amortization=depreciation,
                change_in_working_capital=-change_nwc, other_operating_adjustments=0.0, cash_from_operations=cfo,
                capital_expenditures=-capex, acquisitions=0.0, other_investing=0.0, cash_from_investing=cfi,
                debt_issuance=draw, debt_repayment=-(repayment + repay_rev), equity_issuance=0.0,
                dividends_paid=-dividends, share_repurchases=0.0, other_financing=0.0, cash_from_financing=cff,
                net_change_in_cash=cfo + cfi + cff, beginning_cash=prev['cash'],
                ending_cash=prev['cash'] + cfo + cfi + cff,
            ))
            fcff = operating_income * (1 - scalar['tax_rate']) + depreciation - capex - change_nwc
            fcfe = cfo - capex + (draw - repay_rev - repayment)
            fcf_rows.append({'period': period, 'fcff': fcff, 'fcfe': fcfe})

            prev.update({
                'cash': cash, 'ppe_net': ppe_net, 'ppe_gross': ppe_gross, 'accumulated_depreciation': accdep,
                'long_term_debt': ltd, 'short_term_debt': std, 'revolver': revolver, 'retained_earnings': retained,
            })
            prev_nwc_assets, prev_nwc_liabs = nwc_assets, nwc_liabs

        validation = self.validate_model(income_statements, balance_sheets, cash_flows)
        validation.validation_warnings.extend(warnings)

        dcf = self._dcf(drivers, base, fcf_rows, assumptions)

        model = IntegratedFinancialModel(
            company_name=company_name,
            base_year=str(statements.period_info.period_end) if statements.period_info.period_end else "Base Year",
            forecast_years=[f"Year {i}" for i in range(1, n + 1)],
            income_statements=income_statements,
            balance_sheets=balance_sheets,
            cash_flows=cash_flows,
            debt_schedules=[],
            working_capital_forecasts=[],
            capex_forecasts=[],
            model_assumptions=drivers,
            validation_results={
                'balance_sheet_balances': validation.balance_sheet_balances,
                'cash_flow_reconciles': validation.cash_flow_reconciles,
                'model_valid': validation.balance_sheet_balances and validation.cash_flow_reconciles,
            },
            scenarios={},
        )
        model.assumption_sources = sources
        model.base_year_values = base
        model.validation = validation
        model.free_cash_flows = fcf_rows
        model.dcf = dcf
        model.user_assumptions = assumptions
        model.base_statements = statements
        model.comparative_data = comparative_data
        return model

    def _dcf(self, drivers: Dict, base: Dict, fcf_rows: List[Dict], assumptions: Dict) -> Dict[str, Any]:
        """FCFF DCF with Gordon terminal value; only when wacc is supplied."""
        if assumptions.get('wacc') is None:
            return {'available': False, 'reason': "supply assumptions.wacc and assumptions.terminal_growth for a DCF"}
        if assumptions.get('terminal_growth') is None:
            raise ValueError("assumptions.terminal_growth is required with wacc")
        wacc, g = float(assumptions['wacc']), float(assumptions['terminal_growth'])
        if not 0 < wacc < 1:
            raise ValueError("wacc must be between 0 and 1")
        if g >= wacc:
            raise ValueError("terminal_growth must be below wacc")
        pv_fcff = [row['fcff'] / (1 + wacc) ** (i + 1) for i, row in enumerate(fcf_rows)]
        n = len(fcf_rows)
        terminal_value = fcf_rows[-1]['fcff'] * (1 + g) / (wacc - g)
        pv_terminal = terminal_value / (1 + wacc) ** n
        enterprise_value = sum(pv_fcff) + pv_terminal
        net_debt = (base['short_term_debt'] + base['long_term_debt'] - base['cash'] - base['short_term_investments'])
        equity_value = enterprise_value - net_debt - base['minority_interest']
        shares = base.get('shares')
        out = {
            'available': True, 'wacc': wacc, 'terminal_growth': g,
            'pv_of_forecast_fcff': sum(pv_fcff), 'terminal_value': terminal_value, 'pv_of_terminal_value': pv_terminal,
            'enterprise_value': enterprise_value, 'net_debt': net_debt,
            'minority_interest': base['minority_interest'], 'equity_value': equity_value,
            'terminal_value_share_of_ev': pv_terminal / enterprise_value if enterprise_value else None,
            'value_per_share': equity_value / shares if shares else None,
            'method': "FCFF = EBIT(1-t) + D&A - capex - change in NWC, discounted at WACC with end-of-year "
                      "convention; TV = FCFF(N)(1+g)/(WACC-g); equity = EV - (debt - cash - short-term "
                      "investments) - minority interest (base-year balances)",
        }
        out['limitations'] = [
            "The terminal value grows the final forecast year's FCFF, which still carries investment sized for "
            "the forecast growth rate; when forecast growth exceeds terminal growth this understates the TV",
            "Valuation uses base-year net debt, not the projected balance",
        ]
        if terminal_value < 0:
            out['warning'] = "terminal-year FCFF is negative; the terminal value is negative"
        return out

    def model_to_dict(self, model: IntegratedFinancialModel) -> Dict[str, Any]:
        """JSON-ready representation of a model built by build_integrated_model()."""
        base = {k: v for k, v in getattr(model, 'base_year_values', {}).items() if k != 'depreciation_basis'}
        years = []
        for inc, bs, cf, fcf in zip(model.income_statements, model.balance_sheets, model.cash_flows,
                                    getattr(model, 'free_cash_flows', [{}] * len(model.income_statements))):
            years.append({
                'period': inc.period,
                'income_statement': {k: v for k, v in asdict(inc).items() if k != 'period'},
                'balance_sheet': {k: v for k, v in asdict(bs).items() if k != 'period'},
                'cash_flow': {k: v for k, v in asdict(cf).items() if k != 'period'},
                'free_cash_flow': {'fcff': fcf.get('fcff'), 'fcfe': fcf.get('fcfe')},
                'checks': {'balance_difference': bs.balance_check,
                           'cash_difference': cf.ending_cash - bs.cash},
            })
        validation = getattr(model, 'validation', None)
        return {
            'model': {
                'company': model.company_name,
                'base_year': model.base_year,
                'forecast_years': model.forecast_years,
                'assumptions': model.model_assumptions,
                'assumption_sources': getattr(model, 'assumption_sources', {}),
                'base_year_values': base,
                'years': years,
                'validation': {
                    **model.validation_results,
                    'errors': validation.validation_errors if validation else [],
                    'warnings': validation.validation_warnings if validation else [],
                },
                'summary': self.get_model_summary(model),
                'dcf': getattr(model, 'dcf', None),
                'conventions': [
                    "Outflows are negative in the projected cash flow statement",
                    "Interest is charged on debt at the start of each year (no circular reference)",
                    "Tax = max(0, pre-tax income) x tax rate; loss carry-forwards are not modelled",
                    "Intangibles, goodwill, other long-term assets/liabilities, share capital and minority "
                    "interest are held at base-year values; no amortisation or impairment is projected",
                    "unreconciled_base_difference carries any base-year gap between assets and "
                    "liabilities + equity (+ minority interest) unchanged",
                ],
            }
        }

    # =========================================================================
    # SENSITIVITY AND SCENARIO ANALYSIS
    # =========================================================================

    SENSITIVITY_METRICS = ('terminal_revenue', 'terminal_ebitda', 'terminal_net_income', 'ending_cash',
                           'enterprise_value')

    def _model_metrics(self, model: IntegratedFinancialModel) -> Dict[str, Optional[float]]:
        dcf = getattr(model, 'dcf', None) or {}
        return {
            'terminal_revenue': model.income_statements[-1].revenue,
            'terminal_ebitda': model.income_statements[-1].ebitda,
            'terminal_net_income': model.income_statements[-1].net_income,
            'ending_cash': model.cash_flows[-1].ending_cash,
            'enterprise_value': dcf.get('enterprise_value') if dcf.get('available') else None,
        }

    def perform_sensitivity_analysis(self,
                                     model: IntegratedFinancialModel,
                                     variable: str,
                                     range_percent: float = 0.20,
                                     num_steps: int = 5,
                                     output_metrics: List[str] = None) -> SensitivityAnalysis:
        """Rebuild the model for each tested value of one assumption.

        Test values are base x (1 - r ... 1 + r) in `num_steps` steps, or base +- r
        (absolute) when the base value is 0. Impact metrics are elasticities:
        (% change in metric) / (% change in the variable) across the tested range.
        """
        if not hasattr(model, 'base_statements'):
            raise ValueError("sensitivity needs a model built by build_integrated_model()")
        if variable not in model.model_assumptions or variable in ('probability',):
            raise ValueError(f"unknown sensitivity variable '{variable}'; use one of: "
                             f"{', '.join(sorted(k for k in model.model_assumptions if k != 'debt_repayment'))}")
        if num_steps < 3 or num_steps > 21:
            raise ValueError("num_steps must be between 3 and 21")
        if not 0 < range_percent <= 1:
            raise ValueError("range_percent must be in (0, 1]")
        output_metrics = list(output_metrics or self.SENSITIVITY_METRICS)

        base_value = model.model_assumptions[variable]
        if isinstance(base_value, (list, tuple)):
            raise ValueError(f"'{variable}' is a per-year list; sensitivity needs a single value")
        base_value = float(base_value)
        span = [(-range_percent + 2 * range_percent * i / (num_steps - 1)) for i in range(num_steps)]
        test_values = [base_value * (1 + s) if base_value != 0 else s for s in span]

        rows = []
        for v in test_values:
            assumptions = {**model.user_assumptions, variable: v}
            try:
                m = self.build_integrated_model(model.base_statements, model.company_name, len(model.forecast_years),
                                                assumptions, model.comparative_data)
                metrics = self._model_metrics(m)
                rows.append({'test_value': v, **{k: metrics.get(k) for k in output_metrics}})
            except ValueError as e:
                rows.append({'test_value': v, 'error': str(e)})

        base_metrics = self._model_metrics(model)
        impact_metrics = {}
        lo, hi = rows[0], rows[-1]
        dv = (test_values[-1] - test_values[0]) / base_value if base_value else None
        for k in output_metrics:
            b = base_metrics.get(k)
            if dv and b and lo.get(k) is not None and hi.get(k) is not None:
                impact_metrics[f'{k}_elasticity'] = ((hi[k] - lo[k]) / abs(b)) / dv
            else:
                impact_metrics[f'{k}_elasticity'] = None

        breakeven = {}
        for k in output_metrics:
            for a, b2 in zip(rows, rows[1:]):
                ya, yb = a.get(k), b2.get(k)
                if ya is not None and yb is not None and ya * yb < 0:
                    breakeven[k] = a['test_value'] + (b2['test_value'] - a['test_value']) * abs(ya) / (abs(ya) + abs(yb))
                    break

        return SensitivityAnalysis(
            base_case={k: base_metrics.get(k) for k in output_metrics},
            variable_tested=variable,
            range_tested=test_values,
            results=rows,
            impact_metrics=impact_metrics,
            breakeven_point=breakeven or None,
        )

    @staticmethod
    def sensitivity_to_dict(s: SensitivityAnalysis) -> Dict[str, Any]:
        return {
            'variable': s.variable_tested, 'tested_values': s.range_tested, 'base_case': s.base_case,
            'results': s.results, 'elasticities': s.impact_metrics, 'breakeven': s.breakeven_point,
            'method': "model rebuilt for each tested value; elasticity = (% change in metric across the range) / "
                      "(% change in the variable)",
        }

    def perform_scenario_analysis(self,
                                  statements: FinancialStatements,
                                  company_name: str,
                                  scenarios: Dict[str, Dict[str, Any]],
                                  forecast_years: int = 5,
                                  comparative_data: Optional[List[FinancialStatements]] = None) -> List[ScenarioAnalysis]:
        """Build one model per scenario (assumption set). Probabilities default to equal
        weights; expected-value contributions are only filled when they sum to 1."""
        if not scenarios:
            return []
        probs = {name: a.get('probability') for name, a in scenarios.items()}
        if all(p is None for p in probs.values()):
            probs = {name: 1.0 / len(scenarios) for name in scenarios}
        weights_ok = all(p is not None for p in probs.values()) and abs(sum(probs.values()) - 1.0) < 1e-6

        results = []
        for scenario_name, assumptions in scenarios.items():
            model_assumptions = {k: v for k, v in assumptions.items() if k != 'probability'}
            model = self.build_integrated_model(statements, company_name, forecast_years, model_assumptions,
                                                comparative_data)
            is_ = model.income_statements[-1]
            key_metrics = {'revenue': is_.revenue, 'gross_margin': is_.gross_margin,
                           'operating_margin': is_.operating_margin, 'net_income': is_.net_income,
                           'ebitda': is_.ebitda}
            growth = model.model_assumptions['revenue_growth_rate']
            growth = growth[0] if isinstance(growth, (list, tuple)) else growth
            scenario_type = (ScenarioType.OPTIMISTIC if growth > 0.10 else
                             ScenarioType.PESSIMISTIC if growth < 0 else ScenarioType.BASE)
            p = probs.get(scenario_name)
            results.append(ScenarioAnalysis(
                scenario_name=scenario_name,
                scenario_type=scenario_type,
                assumptions=model_assumptions,
                income_statement=is_,
                key_metrics=key_metrics,
                probability_weight=p,
                expected_value_contribution=({k: (v * p if v is not None else None) for k, v in key_metrics.items()}
                                             if weights_ok else {}),
            ))
        return results

    # =========================================================================
    # MODEL VALIDATION
    # =========================================================================

    def validate_model(self,
                       income_statements: List[ProFormaIncomeStatement],
                       balance_sheets: List[ProFormaBalanceSheet],
                       cash_flows: List[ProFormaCashFlow]) -> ModelValidation:
        """Checks computed from the projected statements (nothing is assumed balanced)."""
        errors = []
        warnings = []

        balance_sheet_balances = True
        for bs in balance_sheets:
            diff = bs.total_assets - bs.total_liabilities_and_equity
            if abs(diff) > max(1e-6, abs(bs.total_assets) * 1e-9):
                balance_sheet_balances = False
                errors.append(f"{bs.period}: balance sheet out of balance by {diff:,.6f}")

        cash_flow_reconciles = True
        for cf, bs in zip(cash_flows, balance_sheets):
            if abs(cf.ending_cash - bs.cash) > max(1e-6, abs(bs.cash) * 1e-9):
                cash_flow_reconciles = False
                errors.append(f"{cf.period}: cash does not reconcile (CF {cf.ending_cash:,.2f}, BS {bs.cash:,.2f})")

        growth_rates_reasonable = True
        for prior, current in zip(income_statements, income_statements[1:]):
            if prior.revenue > 0 and abs(current.revenue / prior.revenue - 1) > 0.50:
                warnings.append(f"{current.period}: revenue growth above 50%")
                growth_rates_reasonable = False

        margins_within_bounds = True
        for is_ in income_statements:
            if is_.gross_margin is not None and not 0 <= is_.gross_margin <= 1:
                margins_within_bounds = False
                warnings.append(f"{is_.period}: gross margin outside 0-100% ({is_.gross_margin:.1%})")
            if is_.operating_margin < -0.5 or is_.operating_margin > 0.6:
                warnings.append(f"{is_.period}: unusual operating margin ({is_.operating_margin:.1%})")

        debt_covenants_met = True
        for is_ in income_statements:
            if is_.interest_expense > 0 and is_.operating_income / is_.interest_expense < 1.5:
                warnings.append(f"{is_.period}: interest coverage {is_.operating_income / is_.interest_expense:.1f}x")
                debt_covenants_met = False

        liquidity_adequate = True
        for bs in balance_sheets:
            if bs.total_current_liabilities > 0 and bs.total_current_assets / bs.total_current_liabilities < 1.0:
                warnings.append(f"{bs.period}: current ratio {bs.total_current_assets / bs.total_current_liabilities:.2f}x")
                liquidity_adequate = False

        return ModelValidation(
            balance_sheet_balances=balance_sheet_balances,
            cash_flow_reconciles=cash_flow_reconciles,
            circular_reference_check=True,  # interest uses opening balances, so there is no circularity
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
        """Summary of the projection; CAGR is measured from the base year over N years."""
        if not model.income_statements:
            return {}
        terminal_year = model.income_statements[-1]
        base = getattr(model, 'base_year_values', {})
        base_revenue = base.get('revenue')
        n = len(model.income_statements)
        cagr = self.cagr(base_revenue, terminal_year.revenue, n) if base_revenue else None
        return {
            'company': model.company_name,
            'forecast_period': f"{model.forecast_years[0]} to {model.forecast_years[-1]}",
            'base_revenue': base_revenue,
            'terminal_revenue': terminal_year.revenue,
            'revenue_cagr': cagr,
            'terminal_ebitda': terminal_year.ebitda,
            'terminal_ebitda_margin': terminal_year.ebitda_margin,
            'terminal_net_income': terminal_year.net_income,
            'terminal_net_margin': terminal_year.net_margin,
            'ending_cash': model.cash_flows[-1].ending_cash if model.cash_flows else None,
            'ending_revolver': model.balance_sheets[-1].revolver if model.balance_sheets else None,
            'model_valid': model.validation_results.get('model_valid', False),
        }


# =============================================================================
# SCRIPT ENTRY POINT
# =============================================================================

def main(argv: Optional[List[str]] = None):
    """`python financial_modeling.py '<json>'` == `financial_analysis_cli.py build_model '<json>'`."""
    import os
    import sys
    argv = sys.argv[1:] if argv is None else argv
    analytics = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    for p in (analytics, os.path.dirname(analytics)):
        if p not in sys.path:
            sys.path.insert(0, p)
    import fincept_json
    import financial_analysis_cli as cli
    if not argv:
        payload = {"success": False,
                   "error": "usage: financial_modeling.py '<json params>' - same params as "
                            "Analytics/financial_analysis_cli.py build_model (run its 'list' command)"}
    else:
        payload = cli.main(["build_model", argv[-1]])
    fincept_json.emit(payload)


if __name__ == "__main__":
    main()
