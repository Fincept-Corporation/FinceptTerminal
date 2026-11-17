"""
13F-HR Institutional Holdings Extraction
=========================================

Extract institutional holdings data from 13F-HR filings including:
- Portfolio holdings (ticker, shares, value)
- Fund manager information
- Quarter-over-quarter changes
- Top positions
- Total portfolio value
"""

from typing import Dict, Any, List, Optional
import os
import sys
import traceback

try:
    from edgar import Company
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

from .base import EdgarError, check_edgar_available

try:
    import fincept_json  # NaN/Infinity-safe JSON boundary shared by the app's scripts
except ImportError:  # scripts/ not on sys.path (in-process use from an unusual launcher)
    sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    import fincept_json


def _holdings_frame(thirteenf: Any):
    """
    The holdings table of a parsed 13F as a pandas DataFrame, or None.

    edgartools 5.x exposes `ThirteenF.holdings` as a DataFrame already (columns
    Issuer, Class, Cusip, Ticker, PutCall, SharesPrnAmount, Value, SoleVoting,
    SharedVoting, NonVoting, Type). The old code looked only for `to_pandas()`
    and otherwise fell back to `list(holdings)`, which on a DataFrame yields the
    COLUMN LABELS - so every 13F command returned column names as "holdings".
    """
    import pandas as pd

    holdings = getattr(thirteenf, "holdings", None)
    if holdings is None:
        return None
    if isinstance(holdings, pd.DataFrame):
        return holdings
    if hasattr(holdings, "to_pandas"):
        return holdings.to_pandas()
    if hasattr(holdings, "to_dataframe"):
        return holdings.to_dataframe()
    try:
        return pd.DataFrame(list(holdings))
    except Exception:
        return None


def _value_column(df) -> Optional[str]:
    for col in ("Value", "value"):
        if col in df.columns:
            return col
    return None


def _sorted_by_value(df):
    """Largest position first (Value coerced to numbers; unparseable values last)."""
    import pandas as pd

    value_col = _value_column(df)
    if not value_col:
        return df
    return df.sort_values(by=value_col, ascending=False, na_position="last",
                          key=lambda s: pd.to_numeric(s, errors="coerce"))


def _records(df) -> List[Dict[str, Any]]:
    """DataFrame rows as plain dicts with every NaN/NaT/numpy scalar made JSON-safe."""
    return fincept_json.sanitize(df.to_dict("records"))


def get_13f_holdings(ticker: str, quarters: int = 2) -> Dict[str, Any]:
    """
    Get institutional holdings from 13F-HR filings

    Args:
        ticker: Stock ticker symbol
        quarters: Number of quarters to retrieve

    Returns:
        Holdings data for specified quarters
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="13F-HR")

        if not filings or len(filings) == 0:
            return {
                "success": True,
                "data": [],
                "count": 0
            }

        holdings_data = []
        count = 0

        for filing in filings:
            if count >= quarters:
                break

            try:
                thirteenf = filing.obj()

                if not thirteenf:
                    continue

                # Get holdings, largest position first
                holdings_list = []
                df = _holdings_frame(thirteenf)
                if df is not None and not df.empty:
                    holdings_list = _records(_sorted_by_value(df))

                holdings_data.append({
                    "filing_date": str(filing.filing_date),
                    "report_period": str(thirteenf.report_period) if hasattr(thirteenf, 'report_period') else None,
                    "manager_name": thirteenf.manager_name if hasattr(thirteenf, 'manager_name') else None,
                    "total_value": float(thirteenf.total_value) if hasattr(thirteenf, 'total_value') and thirteenf.total_value else None,
                    "holdings_count": len(holdings_list),
                    "holdings": holdings_list
                })

                count += 1

            except Exception as e:
                continue

        return {
            "success": True,
            "data": holdings_data,
            "count": len(holdings_data)
        }
    except Exception as e:
        return {"error": EdgarError("get_13f_holdings", str(e), traceback.format_exc()).to_dict()}


def get_13f_top_holdings(ticker: str, top_n: int = 20) -> Dict[str, Any]:
    """
    Get top holdings from latest 13F filing

    Args:
        ticker: Stock ticker symbol (of the fund/institution)
        top_n: Number of top holdings to return

    Returns:
        Top N holdings by value
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="13F-HR").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_13f_top_holdings",
                                  f"No 13F-HR filing found for {ticker}").to_dict()
            }

        thirteenf = filing.obj()

        if not thirteenf or not hasattr(thirteenf, 'holdings'):
            return {
                "error": EdgarError("get_13f_top_holdings",
                                  "No holdings data available").to_dict()
            }

        # Holdings as a DataFrame, sorted by value, top N
        holdings_list = []
        df = _holdings_frame(thirteenf)
        if df is not None and not df.empty:
            holdings_list = _records(_sorted_by_value(df).head(top_n))

        return {
            "success": True,
            "data": {
                "filing_date": str(filing.filing_date),
                "report_period": str(thirteenf.report_period) if hasattr(thirteenf, 'report_period') else None,
                "manager_name": thirteenf.manager_name if hasattr(thirteenf, 'manager_name') else None,
                "total_value": float(thirteenf.total_value) if hasattr(thirteenf, 'total_value') and thirteenf.total_value else None,
                "top_holdings": holdings_list,
                "count": len(holdings_list)
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_13f_top_holdings", str(e), traceback.format_exc()).to_dict()}


def get_13f_manager_info(ticker: str) -> Dict[str, Any]:
    """
    Get fund manager information from latest 13F

    Args:
        ticker: Stock ticker symbol (of the fund/institution)

    Returns:
        Manager information and portfolio summary
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="13F-HR").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_13f_manager_info",
                                  f"No 13F-HR filing found for {ticker}").to_dict()
            }

        thirteenf = filing.obj()

        if not thirteenf:
            return {
                "error": EdgarError("get_13f_manager_info",
                                  "Could not parse 13F filing").to_dict()
            }

        # Extract manager info
        manager_info = {
            "manager_name": thirteenf.manager_name if hasattr(thirteenf, 'manager_name') else None,
            "report_period": str(thirteenf.report_period) if hasattr(thirteenf, 'report_period') else None,
            "filing_date": str(filing.filing_date),
            "total_value": float(thirteenf.total_value) if hasattr(thirteenf, 'total_value') and thirteenf.total_value else None,
            "total_holdings": thirteenf.total_holdings if hasattr(thirteenf, 'total_holdings') else None,
            "management_company_name": thirteenf.management_company_name if hasattr(thirteenf, 'management_company_name') else None,
        }

        # Get portfolio managers if available
        if hasattr(thirteenf, 'get_portfolio_managers'):
            try:
                managers = thirteenf.get_portfolio_managers()
                manager_info["portfolio_managers"] = managers
            except:
                pass

        return {
            "success": True,
            "data": fincept_json.sanitize(manager_info)
        }
    except Exception as e:
        return {"error": EdgarError("get_13f_manager_info", str(e), traceback.format_exc()).to_dict()}


def get_13f_summary(ticker: str) -> Dict[str, Any]:
    """
    Get summary statistics from latest 13F filing

    Args:
        ticker: Stock ticker symbol (of the fund/institution)

    Returns:
        Portfolio summary statistics
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="13F-HR").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_13f_summary",
                                  f"No 13F-HR filing found for {ticker}").to_dict()
            }

        thirteenf = filing.obj()

        if not thirteenf:
            return {
                "error": EdgarError("get_13f_summary",
                                  "Could not parse 13F filing").to_dict()
            }

        summary = {
            "manager_name": thirteenf.manager_name if hasattr(thirteenf, 'manager_name') else None,
            "report_period": str(thirteenf.report_period) if hasattr(thirteenf, 'report_period') else None,
            "filing_date": str(filing.filing_date),
            "total_value": float(thirteenf.total_value) if hasattr(thirteenf, 'total_value') and thirteenf.total_value else None,
            "total_holdings": thirteenf.total_holdings if hasattr(thirteenf, 'total_holdings') else 0,
        }

        # Calculate additional stats from holdings
        df = _holdings_frame(thirteenf)
        if df is not None and not df.empty:
            value_col = _value_column(df)
            if value_col:
                import pandas as pd
                values = pd.to_numeric(df[value_col], errors="coerce")
                # Top 10 concentration
                top_10_value = values.nlargest(10).sum()
                total_value = values.sum()

                summary["top_10_concentration"] = float(top_10_value / total_value * 100) if total_value > 0 else 0
                summary["average_position_size"] = float(total_value / len(df)) if len(df) > 0 else 0

        return {
            "success": True,
            "data": fincept_json.sanitize(summary)
        }
    except Exception as e:
        return {"error": EdgarError("get_13f_summary", str(e), traceback.format_exc()).to_dict()}
