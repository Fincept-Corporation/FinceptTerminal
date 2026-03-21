# ============================================================================
# Fincept Terminal - Strategy Engine Universe Selection
# Dynamic security filtering and selection
# ============================================================================

from typing import List, Callable, Optional
from .enums import Resolution, SecurityType, Market
from .types import Symbol


class _ScheduleSettings:
    """Schedule settings for universe."""
    def __init__(self):
        self._on_rule = None
        self._at_rule = None
    def on(self, date_rule=None):
        """Set date rule for scheduled universe selection."""
        self._on_rule = date_rule
        return self
    def at(self, time_rule=None):
        """Set time rule for scheduled universe selection."""
        self._at_rule = time_rule
        return self

class _ETFSettings:
    """ETF universe settings."""
    _is_etf = True
    def __init__(self):
        self.symbol = None
        self._filter_func = None
    def __call__(self, *args, **kwargs):
        # Called as self.universe.etf(spy, universe_settings, filter_func)
        for a in args:
            if isinstance(a, Symbol):
                self.symbol = a
            elif callable(a) and not isinstance(a, type) and not isinstance(a, UniverseSettings):
                self._filter_func = a
        return self

class _QC500Settings:
    """QC500 universe settings."""
    def __init__(self):
        pass
    def __call__(self, *args, **kwargs):
        return self

class _TopSettings:
    """Top N universe settings."""
    def __init__(self):
        pass
    def __call__(self, *args, **kwargs):
        return self

class UniverseSettings:
    """Settings for universe subscriptions."""

    def __init__(self, *args):
        self.resolution = Resolution.MINUTE
        self.leverage = 1.0
        self.fill_forward = True
        self.extended_market_hours = False
        self.minimum_time_in_universe = None
        self.data_normalization_mode = 0
        self.asynchronous = False
        self.schedule = _ScheduleSettings()
        self.etf = _ETFSettings()
        self.qc_500 = _QC500Settings()
        self.top = _TopSettings()


class CoarseFundamental:
    """Coarse fundamental data for universe selection."""

    def __init__(self, symbol: Symbol, dollar_volume: float = 0,
                 price: float = 0, volume: float = 0,
                 has_fundamental_data: bool = False):
        self.symbol = symbol
        self.dollar_volume = dollar_volume
        self.price = price
        self.volume = volume
        self.has_fundamental_data = has_fundamental_data
        self.market = symbol.market if symbol else Market.USA
        self.value = price


class FineFundamental:
    """Fine fundamental data for universe selection."""

    def __init__(self, symbol: Symbol):
        self.symbol = symbol
        self.company_reference = CompanyReference()
        self.security_reference = SecurityReference()
        self.financial_statements = FinancialStatements()
        self.earning_reports = EarningReports()
        self.operation_ratios = OperationRatios()
        self.earning_ratios = EarningRatios()
        self.valuation_ratios = ValuationRatios()
        self.asset_classification = AssetClassification()
        self.market_cap = 0


class CompanyReference:
    def __init__(self):
        self.industry_template_code = ""
        self.primary_exchange_id = ""
        self.country_id = ""


class SecurityReference:
    def __init__(self):
        self.security_type = ""
        self.listing_exchange = ""
        self.ipo_date = None


class FinancialStatements:
    def __init__(self):
        self.income_statement = IncomeStatement()
        self.balance_sheet = BalanceSheet()
        self.cash_flow_statement = CashFlowStatement()


class IncomeStatement:
    def __init__(self):
        self.total_revenue = FinancialValue()
        self.net_income = FinancialValue()
        self.ebit = FinancialValue()
        self.ebitda = FinancialValue()
        self.gross_profit = FinancialValue()


class BalanceSheet:
    def __init__(self):
        self.total_assets = FinancialValue()
        self.total_debt = FinancialValue()
        self.stockholders_equity = FinancialValue()
        self.current_assets = FinancialValue()
        self.current_liabilities = FinancialValue()
        self.working_capital = FinancialValue()


class CashFlowStatement:
    def __init__(self):
        self.free_cash_flow = FinancialValue()
        self.operating_cash_flow = FinancialValue()


class FinancialValue:
    def __init__(self, value: float = 0):
        self.value = value
        self.three_months = value
        self.six_months = value
        self.twelve_months = value

    def __float__(self):
        return self.value


class EarningReports:
    def __init__(self):
        self.basic_eps = FinancialValue()
        self.diluted_eps = FinancialValue()
        self.basic_average_shares = FinancialValue()


class OperationRatios:
    def __init__(self):
        self.roe = FinancialValue()
        self.roa = FinancialValue()
        self.revenue_growth = FinancialValue()
        self.net_margin = FinancialValue()
        self.operating_margin = FinancialValue()
        self.gross_margin = FinancialValue()


class EarningRatios:
    def __init__(self):
        self.equity_per_share_growth = FinancialValue()
        self.diluted_eps_growth = FinancialValue()


class ValuationRatios:
    def __init__(self):
        self.pe_ratio = 0
        self.pb_ratio = 0
        self.ps_ratio = 0
        self.pcf_ratio = 0
        self.forward_pe = 0
        self.peg_ratio = 0
        self.dividend_yield = 0
        self.earnings_yield = 0
        self.book_value_per_share = 0
        self.sales_per_share = 0
        self.ev_to_ebitda = 0


class AssetClassification:
    def __init__(self):
        self.morningstar_sector_code = 0
        self.morningstar_industry_group_code = 0
        self.morningstar_industry_code = 0
        self.style_box = 0
        self.growth_grade = ""
        self.profitability_grade = ""
        self.financial_health_grade = ""
        self.stock_type = 0


class Universe:
    """Base universe class."""

    UNCHANGED = "unchanged"

    def __init__(self):
        self.members = {}
        self.settings = UniverseSettings()

    @staticmethod
    def unchanged():
        return Universe.UNCHANGED
