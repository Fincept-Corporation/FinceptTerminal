"""
Financial Data Extraction (XBRL)
=================================

Complete XBRL financial data extraction including:
- Balance Sheet
- Income Statement
- Cash Flow Statement
- Statement of Equity
- All XBRL statements (90+ types)
- Direct financial metrics
- Company facts
"""

from typing import Dict, Any, Optional
import traceback

try:
    from edgar import Company
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

from .base import EdgarError, check_edgar_available


def get_financials(ticker: str, periods: int = 4, annual: bool = True) -> Dict[str, Any]:
    """
    Get complete financial statements (Balance Sheet, Income Statement, Cash Flow)

    Args:
        ticker: Stock ticker symbol
        periods: Number of periods to retrieve
        annual: True for annual (10-K), False for quarterly (10-Q)

    Returns:
        All three financial statements
    """
    try:
        check_edgar_available()
        company = Company(ticker)

        if annual:
            financials = company.get_financials()
        else:
            financials = company.get_quarterly_financials() if hasattr(company, 'get_quarterly_financials') else company.get_financials()

        # Get statements
        bs = financials.balance_sheet()
        inc = financials.income_statement()
        cf = financials.cashflow_statement()

        # Convert to DataFrames
        bs_df = bs.to_dataframe() if hasattr(bs, 'to_dataframe') else None
        inc_df = inc.to_dataframe() if hasattr(inc, 'to_dataframe') else None
        cf_df = cf.to_dataframe() if hasattr(cf, 'to_dataframe') else None

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "periods": periods,
                "annual": annual,
                "balance_sheet": {
                    "type": bs.canonical_type if hasattr(bs, 'canonical_type') else "BalanceSheet",
                    "data": bs_df.to_dict() if bs_df is not None else str(bs)
                },
                "income_statement": {
                    "type": inc.canonical_type if hasattr(inc, 'canonical_type') else "IncomeStatement",
                    "data": inc_df.to_dict() if inc_df is not None else str(inc)
                },
                "cash_flow": {
                    "type": cf.canonical_type if hasattr(cf, 'canonical_type') else "CashFlowStatement",
                    "data": cf_df.to_dict() if cf_df is not None else str(cf)
                }
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_financials", str(e), traceback.format_exc()).to_dict()}


def get_financial_metrics(ticker: str) -> Dict[str, Any]:
    """
    Get direct financial metrics (revenue, net income, cash flow, etc.)

    Args:
        ticker: Stock ticker symbol

    Returns:
        Key financial metrics
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        financials = company.get_financials()

        metrics = {}

        # Revenue
        try:
            metrics["revenue"] = financials.get_revenue()
        except:
            metrics["revenue"] = None

        # Net Income
        try:
            metrics["net_income"] = financials.get_net_income()
        except:
            metrics["net_income"] = None

        # Operating Cash Flow
        try:
            metrics["operating_cash_flow"] = financials.get_operating_cash_flow()
        except:
            metrics["operating_cash_flow"] = None

        # Free Cash Flow
        try:
            metrics["free_cash_flow"] = financials.get_free_cash_flow()
        except:
            metrics["free_cash_flow"] = None

        # Total Assets
        try:
            metrics["total_assets"] = financials.get_total_assets()
        except:
            metrics["total_assets"] = None

        # Total Liabilities
        try:
            metrics["total_liabilities"] = financials.get_total_liabilities()
        except:
            metrics["total_liabilities"] = None

        # Stockholders Equity
        try:
            metrics["stockholders_equity"] = financials.get_stockholders_equity()
        except:
            metrics["stockholders_equity"] = None

        # Current Assets
        try:
            metrics["current_assets"] = financials.get_current_assets()
        except:
            metrics["current_assets"] = None

        # Current Liabilities
        try:
            metrics["current_liabilities"] = financials.get_current_liabilities()
        except:
            metrics["current_liabilities"] = None

        # Capital Expenditures
        try:
            metrics["capital_expenditures"] = financials.get_capital_expenditures()
        except:
            metrics["capital_expenditures"] = None

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "metrics": metrics
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_financial_metrics", str(e), traceback.format_exc()).to_dict()}


def get_xbrl_statements(ticker: str, form: str = "10-K") -> Dict[str, Any]:
    """
    Get all XBRL statements from filing

    Args:
        ticker: Stock ticker symbol
        form: Form type (10-K or 10-Q)

    Returns:
        All available XBRL statements
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form=form).latest(1)

        if not filing:
            return {
                "error": EdgarError("get_xbrl_statements",
                                  f"No {form} filing found for {ticker}").to_dict()
            }

        xbrl = filing.xbrl()

        if not xbrl:
            return {
                "error": EdgarError("get_xbrl_statements",
                                  "No XBRL data available").to_dict()
            }

        # Get all statements
        statements = xbrl.get_all_statements()

        # Extract statement types
        statement_types = []
        for stmt in statements:
            if isinstance(stmt, dict):
                statement_types.append({
                    "type": stmt.get('type'),
                    "role": stmt.get('role'),
                    "definition": stmt.get('definition'),
                    "element_count": stmt.get('element_count')
                })

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "form": form,
                "statement_count": len(statements),
                "statements": statement_types
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_xbrl_statements", str(e), traceback.format_exc()).to_dict()}


def get_company_facts(cik: str) -> Dict[str, Any]:
    """
    Get company facts from SEC

    Args:
        cik: Company CIK number

    Returns:
        Company facts data
    """
    try:
        check_edgar_available()
        from edgar import get_company_facts

        facts = get_company_facts(cik)

        return {
            "success": True,
            "data": {
                "cik": cik,
                "facts": str(facts)[:5000]  # Limit size
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_company_facts", str(e), traceback.format_exc()).to_dict()}
