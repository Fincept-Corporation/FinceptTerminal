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

from typing import Dict, Any, Optional
import traceback

try:
    from edgar import Company
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

from .base import EdgarError, check_edgar_available


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

                # Get holdings
                holdings_list = []
                if hasattr(thirteenf, 'holdings'):
                    holdings = thirteenf.holdings

                    # Convert to list if needed
                    if hasattr(holdings, 'to_pandas'):
                        df = holdings.to_pandas()
                        holdings_list = df.to_dict('records')
                    elif hasattr(holdings, '__iter__'):
                        holdings_list = list(holdings)

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

        # Get holdings as dataframe
        holdings = thirteenf.holdings
        holdings_list = []

        if hasattr(holdings, 'to_pandas'):
            df = holdings.to_pandas()
            # Sort by value and take top N
            if 'Value' in df.columns or 'value' in df.columns:
                value_col = 'Value' if 'Value' in df.columns else 'value'
                df = df.sort_values(by=value_col, ascending=False).head(top_n)
            holdings_list = df.to_dict('records')
        elif hasattr(holdings, '__iter__'):
            holdings_list = list(holdings)[:top_n]

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
            "data": manager_info
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
        if hasattr(thirteenf, 'holdings'):
            holdings = thirteenf.holdings
            if hasattr(holdings, 'to_pandas'):
                df = holdings.to_pandas()

                if 'Value' in df.columns or 'value' in df.columns:
                    value_col = 'Value' if 'Value' in df.columns else 'value'

                    # Top 10 concentration
                    top_10_value = df.nlargest(10, value_col)[value_col].sum()
                    total_value = df[value_col].sum()

                    summary["top_10_concentration"] = float(top_10_value / total_value * 100) if total_value > 0 else 0
                    summary["average_position_size"] = float(total_value / len(df)) if len(df) > 0 else 0

        return {
            "success": True,
            "data": summary
        }
    except Exception as e:
        return {"error": EdgarError("get_13f_summary", str(e), traceback.format_exc()).to_dict()}
