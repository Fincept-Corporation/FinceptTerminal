"""
Financial Statement Data Processor Module
=========================================

Turns raw statement data (flat keys, per-statement sections, or yfinance-style
row names) into one standardized ``FinancialStatements`` object.

This is the single input boundary of the package. Everything downstream relies on
the conventions fixed here:

MISSING IS NOT ZERO
  A line item that is absent, null, empty, "N/A" or non-finite (NaN / Infinity) is
  simply not stored. Analyzers test for presence and report a metric as
  unavailable instead of computing it from an invented 0.

SIGN CONVENTION (normalised once, here)
  Feeds disagree on signs (yfinance / EDGAR report outflows negative, many
  hand-built inputs report them positive). After processing:

  * Cash-flow outflows are POSITIVE magnitudes:
      capex, acquisitions, investment_purchases, debt_repaid, equity_repurchased,
      dividends_paid, interest_paid, cash_taxes_paid
  * Cash-flow inflows are POSITIVE magnitudes:
      debt_issued, equity_issued, asset_sales, investment_sales,
      dividends_received, cash_from_customers
  * Cash-flow subtotals keep the reported sign:
      operating_cash_flow, investing_cash_flow, financing_cash_flow,
      net_cash_change, cash_beginning, cash_ending
  * Operating adjustments keep the cash-flow-statement sign (negative = the item
      reduced operating cash flow): working_capital_change,
      accounts_receivable_change, inventory_change, accounts_payable_change,
      depreciation_cf, amortization_cf, stock_compensation, deferred_tax
  * Income-statement expenses are POSITIVE magnitudes:
      cost_of_sales, operating_expenses, selling_expenses,
      administrative_expenses, sga_expense, rd_expenses, depreciation,
      amortization, interest_expense, restructuring_charges, impairment_losses
    tax_expense keeps its sign (a negative value is a tax benefit).

  Every value whose sign was flipped is listed in
  ``statements.data_quality['sign_normalised']``.

BALANCE CHECK
  Total assets vs total liabilities + total equity (+ minority interest when
  supplied) is checked with a relative tolerance of 0.5% of total assets. A larger
  gap is recorded in ``data_quality['validation_warnings']``; it never aborts the
  analysis.

UNMAPPED KEYS
  Every key that is not a recognised statement line item is kept in
  ``statements.notes`` (numeric strings converted to numbers), so the specialized
  analyzers can read note disclosures such as ``lifo_reserve`` or
  ``statutory_tax_rate``.
"""

import math
import re
from typing import Dict, List, Optional, Union, Any, Tuple
from datetime import datetime, date
from dataclasses import dataclass, field
from enum import Enum
import logging


class ReportingStandard(Enum):
    """Financial reporting standards"""
    IFRS = "IFRS"
    US_GAAP = "US_GAAP"
    LOCAL_GAAP = "LOCAL_GAAP"


class CurrencyType(Enum):
    """Currency classification for multinational operations"""
    PRESENTATION = "presentation"  # Reporting currency
    FUNCTIONAL = "functional"  # Primary economic environment
    LOCAL = "local"  # Local country currency


class DataSource(Enum):
    """Supported data sources"""
    API = "api"
    CSV = "csv"
    EXCEL = "excel"
    JSON = "json"
    MANUAL = "manual"
    TERMINAL = "terminal"


@dataclass
class CompanyInfo:
    """Company identification and basic information"""
    ticker: str
    name: str
    sector: str
    industry: str
    country: str
    reporting_standard: ReportingStandard
    fiscal_year_end: str
    presentation_currency: str
    functional_currency: str = None
    exchange: str = None
    market_cap: float = None


@dataclass
class FinancialPeriod:
    """Financial reporting period information"""
    period_end: date
    period_type: str  # 'annual', 'quarterly', 'interim'
    fiscal_year: int
    fiscal_period: str  # 'Q1', 'Q2', 'Q3', 'Q4', 'FY'
    reporting_date: date = None
    audit_status: str = None  # 'audited', 'reviewed', 'unaudited'


@dataclass
class FinancialStatements:
    """Standardized financial statement data structure"""
    # Company and period info
    company_info: CompanyInfo
    period_info: FinancialPeriod

    # Income Statement
    income_statement: Dict[str, float] = field(default_factory=dict)

    # Balance Sheet
    balance_sheet: Dict[str, float] = field(default_factory=dict)

    # Cash Flow Statement
    cash_flow: Dict[str, float] = field(default_factory=dict)

    # Statement of Equity
    equity_statement: Dict[str, float] = field(default_factory=dict)

    # Notes and supplementary information
    notes: Dict[str, Any] = field(default_factory=dict)

    # Ratios and metrics (calculated)
    ratios: Dict[str, float] = field(default_factory=dict)

    # Data quality indicators
    data_quality: Dict[str, Any] = field(default_factory=dict)


# Keys whose sign is normalised to a positive magnitude (see module docstring).
CASH_FLOW_MAGNITUDE_KEYS = (
    'capex', 'acquisitions', 'investment_purchases', 'debt_repaid', 'equity_repurchased',
    'dividends_paid', 'interest_paid', 'cash_taxes_paid',
    'debt_issued', 'equity_issued', 'asset_sales', 'investment_sales', 'dividends_received',
    'cash_from_customers',
)
INCOME_EXPENSE_MAGNITUDE_KEYS = (
    'cost_of_sales', 'operating_expenses', 'selling_expenses', 'administrative_expenses',
    'sga_expense', 'rd_expenses', 'depreciation', 'amortization', 'interest_expense',
    'restructuring_charges', 'impairment_losses',
)

# Tokens that mean "no value" in hand-typed or exported data.
_MISSING_TOKENS = {'', '-', '--', 'n/a', 'na', 'nan', 'none', 'null', 'nil', '—'}

# Section names accepted for per-statement input.
SECTION_ALIASES = {
    'income_statement': 'income', 'income': 'income', 'income_stmt': 'income',
    'balance_sheet': 'balance', 'balance': 'balance',
    'cash_flow': 'cash', 'cash_flow_statement': 'cash', 'cashflow': 'cash', 'cash_flows': 'cash',
}


def parse_number(value: Any, key: str = '') -> Optional[float]:
    """Parse one statement value.

    Returns a finite float, or None when the value means "not reported" (None, empty,
    "N/A", NaN, +-Infinity). Accepts thousands separators, currency signs and
    accounting-style negatives "(1,234)". Raises ValueError for anything else so a
    typo is reported instead of silently becoming 0.
    """
    if value is None:
        return None
    if isinstance(value, bool):
        raise ValueError(f"'{key}': boolean {value!r} is not a number")
    if isinstance(value, (int, float)):
        v = float(value)
        return v if math.isfinite(v) else None
    if isinstance(value, str):
        s = value.strip()
        if s.lower() in _MISSING_TOKENS:
            return None
        negative = s.startswith('(') and s.endswith(')')
        if negative:
            s = s[1:-1]
        s = s.replace(',', '').replace('$', '').replace('€', '').replace('£', '').replace(' ', '')
        try:
            v = float(s)
        except ValueError:
            raise ValueError(f"'{key}': {value!r} is not a number")
        if not math.isfinite(v):
            return None
        return -v if negative else v
    # numpy scalars and similar
    try:
        v = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"'{key}': {type(value).__name__} value is not a number")
    return v if math.isfinite(v) else None


def normalize_key(key: Any) -> str:
    """'Total Revenue' / 'total-revenue' / 'TotalRevenue ' -> 'total_revenue'."""
    k = str(key).strip().lower()
    k = re.sub(r'[\s\-/.&]+', '_', k)
    k = re.sub(r'[^a-z0-9_]', '', k)
    return re.sub(r'_+', '_', k).strip('_')


class DataProcessor:
    """
    Universal financial data processor supporting multiple data sources and formats.
    Ensures CFA-compliant standardization and validation.
    """

    BALANCE_TOLERANCE = 0.005  # 0.5% of total assets

    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self._initialize_standard_mappings()

    def _initialize_standard_mappings(self):
        """Standard account names and the input names accepted for each.

        The first alias found wins. Each standard name is implicitly an alias of
        itself. yfinance row names (e.g. 'Total Revenue', 'Net PPE') are included
        in their normalised form.
        """

        # Income Statement Standard Mappings
        self.income_statement_mapping = {
            'revenue': ['revenue', 'sales', 'net_sales', 'total_revenue', 'net_revenue', 'operating_revenue'],
            'cost_of_sales': ['cost_of_sales', 'cost_of_goods_sold', 'cogs', 'cost_of_revenue'],
            'gross_profit': ['gross_profit', 'gross_income'],
            'operating_expenses': ['operating_expenses', 'total_operating_expenses', 'operating_expense'],
            'selling_expenses': ['selling_expenses', 'sales_expenses', 'marketing_expenses',
                                 'selling_and_marketing_expense'],
            'administrative_expenses': ['administrative_expenses', 'admin_expenses', 'general_admin',
                                        'general_and_administrative_expense'],
            'sga_expense': ['sga_expense', 'sga', 'sg_and_a', 'selling_general_and_administration',
                            'selling_general_administrative', 'selling_general_and_administrative'],
            'rd_expenses': ['rd_expenses', 'research_development', 'r_and_d', 'research_and_development'],
            'depreciation': ['depreciation', 'depreciation_amortization', 'da_expense',
                             'depreciation_and_amortization_in_income_statement', 'reconciled_depreciation',
                             'depreciation_and_amortization'],
            'amortization': ['amortization', 'amortization_expense', 'amortization_of_intangibles'],
            'operating_income': ['operating_income', 'ebit', 'operating_profit'],
            'interest_expense': ['interest_expense', 'interest_cost', 'finance_costs',
                                 'interest_expense_non_operating'],
            'interest_income': ['interest_income', 'interest_revenue', 'interest_income_non_operating'],
            'other_income': ['other_income', 'other_revenue', 'non_operating_income', 'other_income_expense'],
            'pretax_income': ['pretax_income', 'ebt', 'income_before_tax'],
            'tax_expense': ['tax_expense', 'income_tax', 'provision_for_taxes', 'tax_provision',
                            'income_tax_expense'],
            'current_tax_expense': ['current_tax_expense', 'current_income_tax'],
            'deferred_tax_expense': ['deferred_tax_expense_is', 'deferred_income_tax_expense'],
            'net_income': ['net_income', 'net_profit', 'profit_after_tax', 'net_income_common_stockholders'],
            'discontinued_operations': ['discontinued_operations', 'discontinued_ops'],
            'extraordinary_items': ['extraordinary_items', 'exceptional_items'],
            'restructuring_charges': ['restructuring_charges', 'restructuring_and_mergern_acquisition',
                                      'restructuring_costs'],
            'impairment_losses': ['impairment_losses', 'impairment_of_capital_assets', 'asset_impairment_charge',
                                  'write_off'],
            'gains_losses_disposals': ['gains_losses_disposals', 'gain_on_sale_of_ppe',
                                       'gain_on_sale_of_business'],
            'unusual_items': ['unusual_items', 'total_unusual_items', 'special_income_charges',
                              'non_recurring_items', 'special_items'],
            'rental_income': ['rental_income'],
            'inventory_impairment': ['inventory_impairment'],
            'basic_eps': ['basic_eps', 'earnings_per_share_basic'],
            'diluted_eps': ['diluted_eps', 'earnings_per_share_diluted'],
            'shares_outstanding_basic': ['shares_outstanding_basic', 'basic_shares', 'basic_average_shares'],
            'shares_outstanding_diluted': ['shares_outstanding_diluted', 'diluted_shares',
                                           'diluted_average_shares'],
        }

        # Balance Sheet Standard Mappings
        self.balance_sheet_mapping = {
            # Assets
            'cash_equivalents': ['cash', 'cash_equivalents', 'cash_and_equivalents', 'cash_and_cash_equivalents'],
            'short_term_investments': ['short_term_investments', 'marketable_securities',
                                       'other_short_term_investments'],
            'accounts_receivable': ['accounts_receivable', 'receivables', 'trade_receivables'],
            'allowance_doubtful_accounts': ['allowance_doubtful_accounts', 'allowance_for_doubtful_accounts',
                                            'allowance_for_doubtful_accounts_receivable'],
            'inventory': ['inventory', 'inventories'],
            'prepaid_expenses': ['prepaid_expenses', 'prepaid_assets'],
            'other_current_assets': ['other_current_assets'],
            'current_assets': ['current_assets', 'total_current_assets'],
            'ppe_gross': ['ppe_gross', 'property_plant_equipment_gross', 'gross_ppe'],
            'accumulated_depreciation': ['accumulated_depreciation', 'accum_depreciation'],
            'ppe_net': ['ppe_net', 'property_plant_equipment_net', 'net_ppe'],
            'intangible_assets': ['intangible_assets', 'intangibles', 'other_intangible_assets'],
            'goodwill': ['goodwill'],
            'goodwill_and_intangibles': ['goodwill_and_intangibles', 'goodwill_and_other_intangible_assets'],
            'long_term_investments': ['long_term_investments', 'investments', 'investments_and_advances',
                                      'long_term_equity_investment'],
            'deferred_tax_asset': ['deferred_tax_asset', 'deferred_tax_assets', 'non_current_deferred_taxes_assets',
                                   'non_current_deferred_tax_assets'],
            'investment_property': ['investment_property', 'investment_properties'],
            'other_non_current_assets': ['other_non_current_assets'],
            'total_assets': ['total_assets', 'assets'],

            # Liabilities
            'accounts_payable': ['accounts_payable', 'payables', 'trade_payables'],
            'short_term_debt': ['short_term_debt', 'current_debt', 'current_debt_and_capital_lease_obligation'],
            'accrued_liabilities': ['accrued_liabilities', 'accrued_expenses', 'current_accrued_expenses'],
            'deferred_revenue': ['deferred_revenue', 'current_deferred_revenue', 'contract_liabilities',
                                 'unearned_revenue'],
            'other_current_liabilities': ['other_current_liabilities'],
            'current_liabilities': ['current_liabilities', 'total_current_liabilities'],
            'long_term_debt': ['long_term_debt', 'non_current_debt'],
            'deferred_tax_liability': ['deferred_tax_liability', 'deferred_tax_liab', 'deferred_tax_liabilities',
                                       'non_current_deferred_taxes_liabilities'],
            'other_liabilities': ['other_liabilities', 'other_non_current_liab', 'other_non_current_liabilities'],
            'total_liabilities': ['total_liabilities', 'liabilities', 'total_liabilities_net_minority_interest'],

            # Equity
            'common_stock': ['common_stock', 'share_capital', 'capital_stock'],
            'retained_earnings': ['retained_earnings'],
            'accumulated_oci': ['accumulated_oci', 'other_comprehensive_income',
                                'gains_losses_not_affecting_retained_earnings'],
            'treasury_stock': ['treasury_stock', 'treasury_shares'],
            'revaluation_surplus': ['revaluation_surplus', 'revaluation_reserve'],
            'total_equity': ['total_equity', 'shareholders_equity', 'stockholders_equity',
                             'total_stockholders_equity', 'common_stock_equity'],
            'minority_interest': ['minority_interest', 'non_controlling_interest', 'noncontrolling_interest'],
            'shares_outstanding': ['shares_outstanding', 'ordinary_shares_number', 'shares_outstanding_period_end'],
        }

        # Cash Flow Statement Standard Mappings
        self.cash_flow_mapping = {
            # Operating Activities
            'net_income_cf': ['net_income_cf', 'net_income', 'profit_after_tax',
                              'net_income_from_continuing_operations'],
            'depreciation_cf': ['depreciation_cf', 'depreciation', 'depreciation_amortization',
                                'depreciation_and_amortization', 'depreciation_amortization_depletion'],
            'amortization_cf': ['amortization_cf', 'amortization'],
            'stock_compensation': ['stock_compensation', 'stock_based_compensation', 'share_based_comp'],
            'deferred_tax': ['deferred_tax', 'deferred_tax_expense', 'deferred_income_tax'],
            'working_capital_change': ['working_capital_change', 'change_working_capital',
                                       'change_in_working_capital'],
            'accounts_receivable_change': ['accounts_receivable_change', 'change_in_receivables',
                                           'changes_in_account_receivables'],
            'inventory_change': ['inventory_change', 'change_in_inventory'],
            'accounts_payable_change': ['accounts_payable_change', 'change_in_payable',
                                        'change_in_account_payable'],
            'operating_cash_flow': ['operating_cash_flow', 'cash_from_operations',
                                    'cash_flow_from_continuing_operating_activities'],

            # Investing Activities
            'capex': ['capex', 'capital_expenditures', 'capital_expenditure', 'ppe_investments', 'purchase_of_ppe'],
            'acquisitions': ['acquisitions', 'business_acquisitions', 'purchase_of_business'],
            'asset_sales': ['asset_sales', 'asset_disposals', 'sale_of_ppe'],
            'investment_purchases': ['investment_purchases', 'securities_purchased', 'purchase_of_investment'],
            'investment_sales': ['investment_sales', 'securities_sold', 'sale_of_investment'],
            'investing_cash_flow': ['investing_cash_flow', 'cash_from_investing'],

            # Financing Activities
            'debt_issued': ['debt_issued', 'debt_proceeds', 'issuance_of_debt'],
            'debt_repaid': ['debt_repaid', 'debt_repayments', 'repayment_of_debt'],
            'equity_issued': ['equity_issued', 'stock_issued', 'issuance_of_capital_stock'],
            'equity_repurchased': ['equity_repurchased', 'stock_repurchased', 'repurchase_of_capital_stock'],
            'dividends_paid': ['dividends_paid', 'dividend_payments', 'cash_dividends_paid'],
            'financing_cash_flow': ['financing_cash_flow', 'cash_from_financing'],

            # Supplemental
            'interest_paid': ['interest_paid', 'interest_paid_supplemental_data', 'cash_interest_paid'],
            'cash_taxes_paid': ['cash_taxes_paid', 'income_tax_paid_supplemental_data', 'taxes_paid'],
            'dividends_received': ['dividends_received'],
            'cash_from_customers': ['cash_from_customers', 'cash_received_from_customers'],

            # Net Change
            'net_cash_change': ['net_cash_change', 'net_change_cash', 'changes_in_cash'],
            'cash_beginning': ['cash_beginning', 'cash_beginning_period', 'beginning_cash',
                               'beginning_cash_position'],
            'cash_ending': ['cash_ending', 'cash_ending_period', 'ending_cash', 'end_cash_position'],
        }

    # ------------------------------------------------------------------ public

    def process_data(self, data: Union[Dict, Any, str],
                     source_type: DataSource,
                     company_info: CompanyInfo,
                     period_info: FinancialPeriod) -> FinancialStatements:
        """
        Main entry point for processing financial data from any source.

        ``data`` may be a flat dict of line items, or a dict with per-statement
        sections (income_statement / balance_sheet / cash_flow, plus optional
        notes); flat keys next to the sections are accepted too.
        """
        raw_data = self._load_data(data, source_type)
        if not isinstance(raw_data, dict) or not raw_data:
            raise ValueError("No financial statement data provided")

        quality_log = {'sign_normalised': [], 'ignored_values': [], 'validation_warnings': []}
        sections, flat, notes_in = self._split_sections(raw_data)

        standardized = {'income': {}, 'balance': {}, 'cash': {}}
        leftovers: Dict[str, Any] = {}

        # Flat keys may belong to any statement.
        if flat:
            self._standardize_into(flat, ('income', 'balance', 'cash'), standardized, leftovers, quality_log)
        # Section keys are looked up in their own statement only, and win over flat keys.
        for section, items in sections.items():
            self._standardize_into(items, (section,), standardized, leftovers, quality_log, override=True)

        statements = FinancialStatements(company_info=company_info, period_info=period_info)
        statements.income_statement = self._finish_income_statement(standardized['income'], quality_log)
        statements.balance_sheet = self._finish_balance_sheet(standardized['balance'], quality_log)
        statements.cash_flow = self._finish_cash_flow(standardized['cash'], quality_log)
        statements.equity_statement = {k: statements.balance_sheet[k]
                                       for k in ('common_stock', 'retained_earnings', 'accumulated_oci',
                                                 'treasury_stock')
                                       if k in statements.balance_sheet}
        statements.notes = self._build_notes(leftovers, notes_in)

        statements.data_quality = self._assess_data_quality(statements)
        statements.data_quality.update({k: v for k, v in quality_log.items() if k != 'validation_warnings'})
        self._validate_financial_statements(statements, quality_log['validation_warnings'])

        self.logger.info(f"Processed data for {company_info.ticker} - {period_info.period_end}")
        return statements

    def has_statement_data(self, statements: FinancialStatements) -> bool:
        """True when at least one core line item was recognised."""
        return bool(statements.income_statement or statements.balance_sheet or statements.cash_flow)

    def convert_currency(self, statements: FinancialStatements,
                         target_currency: str,
                         exchange_rates: Dict[str, float]) -> FinancialStatements:
        """Currency translation (current-rate / temporal method) is not implemented."""
        raise NotImplementedError("Currency translation is not implemented in this package")

    def adjust_for_inflation(self, statements: FinancialStatements,
                             inflation_rate: float) -> FinancialStatements:
        """Hyperinflation restatement (IAS 29) is not implemented."""
        raise NotImplementedError("Inflation restatement is not implemented in this package")

    # ----------------------------------------------------------------- loading

    def _load_data(self, data: Any, source_type: DataSource) -> Dict:
        """Load data from various sources (pandas is imported only for CSV/Excel)."""
        if source_type in (DataSource.CSV, DataSource.EXCEL):
            import pandas as pd  # lazy: ~1 s import, needed only here
            if isinstance(data, str):
                df = pd.read_csv(data) if source_type == DataSource.CSV else pd.read_excel(data)
            else:
                df = data
            return df.to_dict('records')[0] if len(df) == 1 else df.to_dict('list')

        if source_type == DataSource.JSON:
            if isinstance(data, str):
                import json
                with open(data, 'r') as f:
                    return json.load(f)
            return data

        if source_type in (DataSource.API, DataSource.TERMINAL, DataSource.MANUAL):
            if isinstance(data, dict):
                return data
            if hasattr(data, 'to_dict'):
                return data.to_dict()
            raise ValueError(f"Unsupported data object: {type(data).__name__}")

        raise ValueError(f"Unsupported source type: {source_type}")

    def _split_sections(self, raw: Dict) -> Tuple[Dict[str, Dict], Dict, Dict]:
        """Separate per-statement sections, flat line items and explicit notes."""
        sections: Dict[str, Dict] = {}
        flat: Dict[str, Any] = {}
        notes: Dict[str, Any] = {}
        for key, value in raw.items():
            nk = normalize_key(key)
            if nk in SECTION_ALIASES and isinstance(value, dict):
                sec = SECTION_ALIASES[nk]
                sections.setdefault(sec, {}).update(value)
            elif nk in ('notes', 'supplementary', 'disclosures') and isinstance(value, dict):
                notes.update(value)
            else:
                flat[key] = value
        return sections, flat, notes

    # --------------------------------------------------------- standardisation

    def _mapping_for(self, section: str) -> Dict[str, List[str]]:
        return {'income': self.income_statement_mapping,
                'balance': self.balance_sheet_mapping,
                'cash': self.cash_flow_mapping}[section]

    def _standardize_into(self, items: Dict, sections: Tuple[str, ...], out: Dict[str, Dict],
                          leftovers: Dict[str, Any], quality_log: Dict, override: bool = False):
        """Map raw keys of one input block onto standard names."""
        normalized: Dict[str, Tuple[str, Any]] = {}
        for k, v in items.items():
            normalized.setdefault(normalize_key(k), (k, v))

        used = set()
        for section in sections:
            mapping = self._mapping_for(section)
            for standard, aliases in mapping.items():
                for alias in [standard] + [a for a in aliases if a != standard]:
                    if alias not in normalized:
                        continue
                    orig_key, raw_value = normalized[alias]
                    if isinstance(raw_value, (dict, list)):
                        continue  # structured value: left for notes
                    used.add(alias)
                    value = parse_number(raw_value, str(orig_key))
                    if value is None:
                        quality_log['ignored_values'].append(f"{orig_key}: not reported")
                        continue  # try the next alias
                    if override or standard not in out[section]:
                        out[section][standard] = value
                    break

        for nk, (orig_key, raw_value) in normalized.items():
            if nk not in used:
                leftovers.setdefault(nk, raw_value)

    def _flip_to_magnitude(self, items: Dict[str, float], keys, quality_log: Dict, label: str):
        for k in keys:
            if k in items and items[k] < 0:
                items[k] = abs(items[k])
                quality_log['sign_normalised'].append(f"{label}.{k}")

    def _finish_income_statement(self, inc: Dict[str, float], quality_log: Dict) -> Dict[str, float]:
        self._flip_to_magnitude(inc, INCOME_EXPENSE_MAGNITUDE_KEYS, quality_log, 'income_statement')
        # Identities only (never estimates).
        if 'gross_profit' not in inc and 'revenue' in inc and 'cost_of_sales' in inc:
            inc['gross_profit'] = inc['revenue'] - inc['cost_of_sales']
        if 'cost_of_sales' not in inc and 'revenue' in inc and 'gross_profit' in inc:
            inc['cost_of_sales'] = inc['revenue'] - inc['gross_profit']
        if 'sga_expense' not in inc and ('selling_expenses' in inc or 'administrative_expenses' in inc):
            inc['sga_expense'] = inc.get('selling_expenses', 0.0) + inc.get('administrative_expenses', 0.0)
        return inc

    def _finish_balance_sheet(self, bs: Dict[str, float], quality_log: Dict) -> Dict[str, float]:
        # Convention: intangible_assets EXCLUDES goodwill.
        if 'intangible_assets' not in bs and 'goodwill_and_intangibles' in bs:
            bs['intangible_assets'] = bs['goodwill_and_intangibles'] - bs.get('goodwill', 0.0)
        if 'goodwill' not in bs and 'goodwill_and_intangibles' in bs and 'intangible_assets' in bs:
            bs['goodwill'] = bs['goodwill_and_intangibles'] - bs['intangible_assets']
        bs.pop('goodwill_and_intangibles', None)
        if 'accumulated_depreciation' in bs and bs['accumulated_depreciation'] < 0:
            bs['accumulated_depreciation'] = abs(bs['accumulated_depreciation'])
            quality_log['sign_normalised'].append('balance_sheet.accumulated_depreciation')
        if 'ppe_net' not in bs and 'ppe_gross' in bs and 'accumulated_depreciation' in bs:
            bs['ppe_net'] = bs['ppe_gross'] - bs['accumulated_depreciation']
        if 'accumulated_depreciation' not in bs and 'ppe_gross' in bs and 'ppe_net' in bs:
            bs['accumulated_depreciation'] = bs['ppe_gross'] - bs['ppe_net']
        return bs

    def _finish_cash_flow(self, cf: Dict[str, float], quality_log: Dict) -> Dict[str, float]:
        self._flip_to_magnitude(cf, CASH_FLOW_MAGNITUDE_KEYS, quality_log, 'cash_flow')
        return cf

    def _build_notes(self, leftovers: Dict[str, Any], explicit: Dict[str, Any]) -> Dict[str, Any]:
        """Unmapped keys and explicit notes; numeric strings become numbers."""
        notes: Dict[str, Any] = {}
        for source in (leftovers, explicit):
            for k, v in source.items():
                nk = normalize_key(k)
                if isinstance(v, str):
                    try:
                        num = parse_number(v, nk)
                        v = num if num is not None else v
                    except ValueError:
                        pass  # genuine text, e.g. inventory_method: "LIFO"
                elif isinstance(v, float) and not math.isfinite(v):
                    continue
                notes[nk] = v
        return notes

    # -------------------------------------------------------------- validation

    def _validate_financial_statements(self, statements: FinancialStatements, warnings: List[str]):
        """Integrity checks. Problems are recorded as warnings; nothing here aborts."""
        bs = statements.balance_sheet
        have = all(k in bs for k in ('total_assets', 'total_liabilities', 'total_equity'))
        if have:
            assets = bs['total_assets']
            other_side = bs['total_liabilities'] + bs['total_equity'] + bs.get('minority_interest', 0.0)
            gap = assets - other_side
            tolerance = max(abs(assets) * self.BALANCE_TOLERANCE, 0.01)
            if abs(gap) > tolerance:
                warnings.append(
                    f"Balance sheet does not balance: total_assets {assets:,.2f} vs liabilities + equity"
                    f"{' + minority interest' if 'minority_interest' in bs else ''} {other_side:,.2f}"
                    f" (gap {gap:,.2f}, {abs(gap) / abs(assets) if assets else float('inf'):.2%} of assets)")
        elif 'total_equity' in bs and ('total_assets' in bs) != ('total_liabilities' in bs):
            # Derive the missing side only when the other two are reported.
            if 'total_assets' not in bs:
                bs['total_assets'] = bs['total_liabilities'] + bs['total_equity'] + bs.get('minority_interest', 0.0)
                warnings.append("total_assets derived as total_liabilities + total_equity")
            else:
                bs['total_liabilities'] = bs['total_assets'] - bs['total_equity'] - bs.get('minority_interest', 0.0)
                warnings.append("total_liabilities derived as total_assets - total_equity")

        cf = statements.cash_flow
        if all(k in cf for k in ('net_cash_change', 'cash_beginning', 'cash_ending')):
            if abs(cf['net_cash_change'] - (cf['cash_ending'] - cf['cash_beginning'])) > 0.01:
                warnings.append("Cash flow net change doesn't match ending - beginning cash")

        inc = statements.income_statement
        eps, shares, ni = inc.get('basic_eps'), inc.get('shares_outstanding_basic'), inc.get('net_income')
        if eps is not None and shares and ni is not None:
            if abs(eps - ni / shares) > max(0.01, abs(eps) * 0.02):
                warnings.append("Basic EPS inconsistent with net income / basic shares")

        statements.data_quality['validation_errors'] = []
        statements.data_quality['validation_warnings'] = warnings

    def _assess_data_quality(self, statements: FinancialStatements) -> Dict[str, Any]:
        """Assess data quality and completeness"""
        quality_metrics = {
            'completeness_score': 0.0,
            'consistency_score': 0.0,
            'missing_fields': [],
            'data_issues': []
        }

        total_expected_fields = (len(self.income_statement_mapping) + len(self.balance_sheet_mapping)
                                 + len(self.cash_flow_mapping))
        total_present_fields = (len(statements.income_statement) + len(statements.balance_sheet)
                                + len(statements.cash_flow))
        quality_metrics['completeness_score'] = min(total_present_fields / total_expected_fields, 1.0)

        critical_fields = ['revenue', 'net_income', 'total_assets', 'total_equity', 'operating_cash_flow']
        for f in critical_fields:
            if (f not in statements.income_statement and f not in statements.balance_sheet
                    and f not in statements.cash_flow):
                quality_metrics['missing_fields'].append(f)

        checks = passed = 0
        bs, inc = statements.balance_sheet, statements.income_statement
        if 'current_assets' in bs and 'total_assets' in bs:
            checks += 1
            passed += bs['current_assets'] <= bs['total_assets']
        if 'revenue' in inc:
            checks += 1
            passed += inc['revenue'] > 0
        quality_metrics['consistency_score'] = passed / checks if checks else None
        return quality_metrics
