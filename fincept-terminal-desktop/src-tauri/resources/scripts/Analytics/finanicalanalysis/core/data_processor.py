
"""
Financial Statement Data Processor Module
========================================

Financial data processing and standardization for analysis

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


import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
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


class DataProcessor:
    """
    Universal financial data processor supporting multiple data sources and formats.
    Ensures CFA-compliant standardization and validation.
    """

    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self._initialize_standard_mappings()

    def _initialize_standard_mappings(self):
        """Initialize standard account mappings for different reporting standards"""

        # Income Statement Standard Mappings
        self.income_statement_mapping = {
            'revenue': ['revenue', 'sales', 'net_sales', 'total_revenue', 'net_revenue'],
            'cost_of_sales': ['cost_of_sales', 'cost_of_goods_sold', 'cogs', 'cost_of_revenue'],
            'gross_profit': ['gross_profit', 'gross_income'],
            'operating_expenses': ['operating_expenses', 'total_operating_expenses'],
            'selling_expenses': ['selling_expenses', 'sales_expenses', 'marketing_expenses'],
            'administrative_expenses': ['administrative_expenses', 'admin_expenses', 'general_admin'],
            'rd_expenses': ['research_development', 'rd_expenses', 'r_and_d'],
            'depreciation': ['depreciation', 'depreciation_amortization', 'da_expense'],
            'operating_income': ['operating_income', 'ebit', 'operating_profit'],
            'interest_expense': ['interest_expense', 'interest_cost', 'finance_costs'],
            'interest_income': ['interest_income', 'interest_revenue'],
            'other_income': ['other_income', 'other_revenue', 'non_operating_income'],
            'pretax_income': ['pretax_income', 'ebt', 'income_before_tax'],
            'tax_expense': ['tax_expense', 'income_tax', 'provision_for_taxes'],
            'net_income': ['net_income', 'net_profit', 'profit_after_tax'],
            'discontinued_operations': ['discontinued_operations', 'discontinued_ops'],
            'extraordinary_items': ['extraordinary_items', 'exceptional_items'],
            'basic_eps': ['basic_eps', 'earnings_per_share_basic'],
            'diluted_eps': ['diluted_eps', 'earnings_per_share_diluted'],
            'shares_outstanding_basic': ['shares_outstanding_basic', 'basic_shares'],
            'shares_outstanding_diluted': ['shares_outstanding_diluted', 'diluted_shares']
        }

        # Balance Sheet Standard Mappings
        self.balance_sheet_mapping = {
            # Assets
            'cash_equivalents': ['cash', 'cash_equivalents', 'cash_and_equivalents'],
            'short_term_investments': ['short_term_investments', 'marketable_securities'],
            'accounts_receivable': ['accounts_receivable', 'receivables', 'trade_receivables'],
            'inventory': ['inventory', 'inventories'],
            'prepaid_expenses': ['prepaid_expenses', 'prepaid_assets'],
            'current_assets': ['current_assets', 'total_current_assets'],
            'ppe_gross': ['ppe_gross', 'property_plant_equipment_gross'],
            'accumulated_depreciation': ['accumulated_depreciation', 'accum_depreciation'],
            'ppe_net': ['ppe_net', 'property_plant_equipment_net'],
            'intangible_assets': ['intangible_assets', 'intangibles'],
            'goodwill': ['goodwill'],
            'long_term_investments': ['long_term_investments', 'investments'],
            'total_assets': ['total_assets', 'assets'],

            # Liabilities
            'accounts_payable': ['accounts_payable', 'payables', 'trade_payables'],
            'short_term_debt': ['short_term_debt', 'current_debt'],
            'accrued_liabilities': ['accrued_liabilities', 'accrued_expenses'],
            'current_liabilities': ['current_liabilities', 'total_current_liabilities'],
            'long_term_debt': ['long_term_debt', 'non_current_debt'],
            'deferred_tax_liability': ['deferred_tax_liability', 'deferred_tax_liab'],
            'other_liabilities': ['other_liabilities', 'other_non_current_liab'],
            'total_liabilities': ['total_liabilities', 'liabilities'],

            # Equity
            'common_stock': ['common_stock', 'share_capital'],
            'retained_earnings': ['retained_earnings'],
            'accumulated_oci': ['accumulated_oci', 'other_comprehensive_income'],
            'treasury_stock': ['treasury_stock', 'treasury_shares'],
            'total_equity': ['total_equity', 'shareholders_equity', 'stockholders_equity']
        }

        # Cash Flow Statement Standard Mappings
        self.cash_flow_mapping = {
            # Operating Activities
            'net_income_cf': ['net_income', 'profit_after_tax'],
            'depreciation_cf': ['depreciation', 'depreciation_amortization'],
            'amortization_cf': ['amortization'],
            'stock_compensation': ['stock_based_compensation', 'share_based_comp'],
            'deferred_tax': ['deferred_tax', 'deferred_tax_expense'],
            'working_capital_change': ['working_capital_change', 'change_working_capital'],
            'accounts_receivable_change': ['accounts_receivable_change'],
            'inventory_change': ['inventory_change'],
            'accounts_payable_change': ['accounts_payable_change'],
            'operating_cash_flow': ['operating_cash_flow', 'cash_from_operations'],

            # Investing Activities
            'capex': ['capital_expenditures', 'capex', 'ppe_investments'],
            'acquisitions': ['acquisitions', 'business_acquisitions'],
            'asset_sales': ['asset_sales', 'asset_disposals'],
            'investment_purchases': ['investment_purchases', 'securities_purchased'],
            'investment_sales': ['investment_sales', 'securities_sold'],
            'investing_cash_flow': ['investing_cash_flow', 'cash_from_investing'],

            # Financing Activities
            'debt_issued': ['debt_issued', 'debt_proceeds'],
            'debt_repaid': ['debt_repaid', 'debt_repayments'],
            'equity_issued': ['equity_issued', 'stock_issued'],
            'equity_repurchased': ['equity_repurchased', 'stock_repurchased'],
            'dividends_paid': ['dividends_paid', 'dividend_payments'],
            'financing_cash_flow': ['financing_cash_flow', 'cash_from_financing'],

            # Net Change
            'net_cash_change': ['net_cash_change', 'net_change_cash'],
            'cash_beginning': ['cash_beginning_period', 'beginning_cash'],
            'cash_ending': ['cash_ending_period', 'ending_cash']
        }

    def process_data(self, data: Union[Dict, pd.DataFrame, str],
                     source_type: DataSource,
                     company_info: CompanyInfo,
                     period_info: FinancialPeriod) -> FinancialStatements:
        """
        Main entry point for processing financial data from any source

        Args:
            data: Raw financial data
            source_type: Type of data source
            company_info: Company identification information
            period_info: Financial period information

        Returns:
            FinancialStatements: Standardized financial statement object
        """
        try:
            # Load data based on source type
            raw_data = self._load_data(data, source_type)

            # Validate data structure
            self._validate_raw_data(raw_data)

            # Standardize account names and structure
            standardized_data = self._standardize_accounts(raw_data)

            # Create financial statements object
            statements = FinancialStatements(
                company_info=company_info,
                period_info=period_info
            )

            # Populate statement sections
            statements.income_statement = self._extract_income_statement(standardized_data)
            statements.balance_sheet = self._extract_balance_sheet(standardized_data)
            statements.cash_flow = self._extract_cash_flow(standardized_data)
            statements.equity_statement = self._extract_equity_statement(standardized_data)
            statements.notes = self._extract_notes(standardized_data)

            # Perform data quality assessment
            statements.data_quality = self._assess_data_quality(statements)

            # Validate financial statement integrity
            self._validate_financial_statements(statements)

            self.logger.info(f"Successfully processed data for {company_info.ticker} - {period_info.period_end}")
            return statements

        except Exception as e:
            self.logger.error(f"Error processing data: {str(e)}")
            raise

    def _load_data(self, data: Union[Dict, pd.DataFrame, str], source_type: DataSource) -> Dict:
        """Load data from various sources"""
        if source_type == DataSource.CSV:
            if isinstance(data, str):
                df = pd.read_csv(data)
            else:
                df = data
            return df.to_dict('records')[0] if len(df) == 1 else df.to_dict('list')

        elif source_type == DataSource.EXCEL:
            if isinstance(data, str):
                df = pd.read_excel(data)
            else:
                df = data
            return df.to_dict('records')[0] if len(df) == 1 else df.to_dict('list')

        elif source_type == DataSource.JSON:
            if isinstance(data, str):
                import json
                with open(data, 'r') as f:
                    return json.load(f)
            return data

        elif source_type in [DataSource.API, DataSource.TERMINAL, DataSource.MANUAL]:
            return data if isinstance(data, dict) else data.to_dict()

        else:
            raise ValueError(f"Unsupported source type: {source_type}")

    def _standardize_accounts(self, raw_data: Dict) -> Dict:
        """Standardize account names using mapping dictionaries"""
        standardized = {}

        # Convert all keys to lowercase for matching
        raw_lower = {k.lower().replace(' ', '_').replace('-', '_'): v
                     for k, v in raw_data.items()}

        # Apply mappings for each statement type
        all_mappings = {
            **self.income_statement_mapping,
            **self.balance_sheet_mapping,
            **self.cash_flow_mapping
        }

        for standard_name, possible_names in all_mappings.items():
            for possible_name in possible_names:
                if possible_name.lower() in raw_lower:
                    standardized[standard_name] = raw_lower[possible_name.lower()]
                    break

        # Add any unmapped accounts with original names
        for orig_key, value in raw_lower.items():
            if not any(orig_key in mapping for mapping in all_mappings.values()):
                standardized[orig_key] = value

        return standardized

    def _extract_income_statement(self, data: Dict) -> Dict[str, float]:
        """Extract and organize income statement items"""
        income_items = {}

        for key in self.income_statement_mapping.keys():
            if key in data:
                income_items[key] = float(data[key]) if data[key] is not None else 0.0

        # Calculate derived items if not present
        if 'gross_profit' not in income_items and 'revenue' in income_items and 'cost_of_sales' in income_items:
            income_items['gross_profit'] = income_items['revenue'] - income_items['cost_of_sales']

        if 'operating_income' not in income_items and 'gross_profit' in income_items and 'operating_expenses' in income_items:
            income_items['operating_income'] = income_items['gross_profit'] - income_items['operating_expenses']

        return income_items

    def _extract_balance_sheet(self, data: Dict) -> Dict[str, float]:
        """Extract and organize balance sheet items"""
        balance_items = {}

        for key in self.balance_sheet_mapping.keys():
            if key in data:
                balance_items[key] = float(data[key]) if data[key] is not None else 0.0

        return balance_items

    def _extract_cash_flow(self, data: Dict) -> Dict[str, float]:
        """Extract and organize cash flow statement items"""
        cf_items = {}

        for key in self.cash_flow_mapping.keys():
            if key in data:
                cf_items[key] = float(data[key]) if data[key] is not None else 0.0

        return cf_items

    def _extract_equity_statement(self, data: Dict) -> Dict[str, float]:
        """Extract equity statement information"""
        equity_items = {}

        # Extract equity-related items
        equity_keys = ['common_stock', 'retained_earnings', 'accumulated_oci', 'treasury_stock']
        for key in equity_keys:
            if key in data:
                equity_items[key] = float(data[key]) if data[key] is not None else 0.0

        return equity_items

    def _extract_notes(self, data: Dict) -> Dict[str, Any]:
        """Extract notes and supplementary information"""
        notes = {}

        # Look for note-related information
        note_keywords = ['accounting_policy', 'segment', 'geographic', 'related_party',
                         'contingency', 'commitment', 'subsequent_event']

        for key, value in data.items():
            if any(keyword in key.lower() for keyword in note_keywords):
                notes[key] = value

        return notes

    def _validate_raw_data(self, data: Dict):
        """Validate raw data structure and completeness"""
        if not data:
            raise ValueError("Empty data provided")

        # Check for minimum required fields
        required_fields = ['revenue', 'total_assets', 'total_equity']
        missing_fields = []

        data_lower = {k.lower(): v for k, v in data.items()}

        for field in required_fields:
            field_found = False
            if field in self.income_statement_mapping:
                for possible_name in self.income_statement_mapping[field]:
                    if possible_name.lower() in data_lower:
                        field_found = True
                        break
            elif field in self.balance_sheet_mapping:
                for possible_name in self.balance_sheet_mapping[field]:
                    if possible_name.lower() in data_lower:
                        field_found = True
                        break

            if not field_found:
                missing_fields.append(field)

        if missing_fields:
            self.logger.warning(f"Missing recommended fields: {missing_fields}")

    def _validate_financial_statements(self, statements: FinancialStatements):
        """Validate financial statement integrity and relationships"""
        errors = []
        warnings = []

        # Balance Sheet Equation: Assets = Liabilities + Equity
        if statements.balance_sheet:
            assets = statements.balance_sheet.get('total_assets', 0)
            liabilities = statements.balance_sheet.get('total_liabilities', 0)
            equity = statements.balance_sheet.get('total_equity', 0)

            if abs(assets - (liabilities + equity)) > 0.01:  # Allow for rounding
                errors.append(
                    f"Balance sheet doesn't balance: Assets({assets}) != Liabilities({liabilities}) + Equity({equity})")

        # Cash Flow Statement: Net change should equal ending - beginning cash
        if statements.cash_flow:
            net_change = statements.cash_flow.get('net_cash_change', 0)
            beginning = statements.cash_flow.get('cash_beginning', 0)
            ending = statements.cash_flow.get('cash_ending', 0)

            if abs(net_change - (ending - beginning)) > 0.01:
                warnings.append("Cash flow net change doesn't match beginning/ending cash difference")

        # EPS consistency check
        if statements.income_statement:
            net_income = statements.income_statement.get('net_income', 0)
            basic_eps = statements.income_statement.get('basic_eps', 0)
            basic_shares = statements.income_statement.get('shares_outstanding_basic', 0)

            if basic_eps and basic_shares and abs(basic_eps - (net_income / basic_shares)) > 0.01:
                warnings.append("Basic EPS calculation inconsistent with net income and shares outstanding")

        statements.data_quality['validation_errors'] = errors
        statements.data_quality['validation_warnings'] = warnings

        if errors:
            raise ValueError(f"Financial statement validation failed: {errors}")

    def _assess_data_quality(self, statements: FinancialStatements) -> Dict[str, Any]:
        """Assess data quality and completeness"""
        quality_metrics = {
            'completeness_score': 0.0,
            'consistency_score': 0.0,
            'missing_fields': [],
            'data_issues': []
        }

        # Assess completeness
        total_expected_fields = len(self.income_statement_mapping) + len(self.balance_sheet_mapping) + len(
            self.cash_flow_mapping)
        total_present_fields = len(statements.income_statement) + len(statements.balance_sheet) + len(
            statements.cash_flow)

        quality_metrics['completeness_score'] = min(total_present_fields / total_expected_fields, 1.0)

        # Identify missing critical fields
        critical_fields = ['revenue', 'net_income', 'total_assets', 'total_equity', 'operating_cash_flow']
        for field in critical_fields:
            if field not in statements.income_statement and field not in statements.balance_sheet and field not in statements.cash_flow:
                quality_metrics['missing_fields'].append(field)

        # Assess consistency (basic checks)
        consistency_checks = 0
        passed_checks = 0

        # Check if current assets <= total assets
        if statements.balance_sheet.get('current_assets', 0) <= statements.balance_sheet.get('total_assets', 0):
            passed_checks += 1
        consistency_checks += 1

        # Check if revenue > 0 for operating companies
        if statements.income_statement.get('revenue', 0) > 0:
            passed_checks += 1
        consistency_checks += 1

        quality_metrics['consistency_score'] = passed_checks / consistency_checks if consistency_checks > 0 else 0.0

        return quality_metrics

    def convert_currency(self, statements: FinancialStatements,
                         target_currency: str,
                         exchange_rates: Dict[str, float]) -> FinancialStatements:
        """Convert financial statements to target currency"""
        # This is a placeholder for currency conversion functionality
        # In practice, this would handle translation methods (current rate vs temporal)
        # and multinational operations as per CFA Level II curriculum
        pass

    def adjust_for_inflation(self, statements: FinancialStatements,
                             inflation_rate: float) -> FinancialStatements:
        """Adjust financial statements for inflation (hyperinflationary economies)"""
        # Placeholder for inflation adjustment methods
        pass