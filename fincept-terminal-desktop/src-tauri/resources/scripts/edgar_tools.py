"""
EdgarTools Comprehensive Wrapper
=================================

A complete wrapper for the edgartools library providing access to SEC EDGAR data.
This wrapper covers the full edgartools API surface including:

- Company information and filings
- Financial statements (Balance Sheet, Income Statement, Cash Flow)
- XBRL data extraction and querying
- Insider transactions (Forms 3, 4, 5)
- Fund holdings (13F filings)
- Proxy statements and executive compensation
- Current filings and real-time data
- Search and discovery functions
- Reference data and company subsets

Returns JSON output for Rust/Tauri integration.

Author: Fincept Terminal
Library: edgartools (https://github.com/dgunning/edgartools)
"""

import sys
import json
import os
from typing import Dict, Any, List, Optional, Union
from datetime import datetime
import traceback

# EdgarTools imports
try:
    from edgar import (
        # Core classes
        Company, Filing, Filings, XBRL, Entity,

        # Functions
        set_identity, get_identity,
        get_filings, get_current_filings, latest_filings,
        find, find_company, find_fund,
        get_entity, get_by_accession_number,
        get_company_facts, get_company_tickers,

        # Data objects
        ThirteenF, ProxyStatement, NPX, FundReport,
        FundCompany, FundSeries, FundClass,

        # Company reports
        company_reports,

        # Reference and search
        reference,

        # Configuration
        configure_http, set_cache_directory,
        clear_cache, storage_info,
    )
    EDGAR_AVAILABLE = True
except ImportError as e:
    EDGAR_AVAILABLE = False
    IMPORT_ERROR = str(e)


class EdgarError:
    """Custom error class for Edgar API errors"""
    def __init__(self, command: str, error: str, details: Optional[str] = None):
        self.command = command
        self.error = error
        self.details = details
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        result = {
            "command": self.command,
            "error": self.error,
            "timestamp": self.timestamp,
            "type": "EdgarError"
        }
        if self.details:
            result["details"] = self.details
        return result


class EdgarToolsWrapper:
    """Comprehensive wrapper for edgartools library"""

    def __init__(self, identity: Optional[str] = None):
        """
        Initialize EdgarTools wrapper

        Args:
            identity: User identity for SEC compliance (email required by SEC)
        """
        if not EDGAR_AVAILABLE:
            raise ImportError(f"edgartools library not available: {IMPORT_ERROR}")

        # Set identity if provided
        if identity:
            set_identity(identity)
        elif 'EDGAR_IDENTITY' not in os.environ:
            # Default identity for Fincept Terminal
            set_identity("Fincept Terminal nikultilak@gmail.com")

    # =========================================================================
    # CONFIGURATION & SETUP
    # =========================================================================

    def set_user_identity(self, identity: str) -> Dict[str, Any]:
        """
        Set SEC user identity (required by SEC regulations)

        Args:
            identity: User identity string (should include name and email)

        Returns:
            Success status and configured identity
        """
        try:
            set_identity(identity)
            current = get_identity()
            return {
                "success": True,
                "identity": current,
                "message": "Identity set successfully"
            }
        except Exception as e:
            return {"error": EdgarError("set_identity", str(e)).to_dict()}

    def get_user_identity(self) -> Dict[str, Any]:
        """Get currently configured SEC user identity"""
        try:
            identity = get_identity()
            return {
                "success": True,
                "identity": identity
            }
        except Exception as e:
            return {"error": EdgarError("get_identity", str(e)).to_dict()}

    def configure_connection(self, verify_ssl: Optional[bool] = None,
                           proxy: Optional[str] = None,
                           timeout: Optional[float] = None) -> Dict[str, Any]:
        """Configure HTTP connection settings"""
        try:
            configure_http(verify_ssl=verify_ssl, proxy=proxy, timeout=timeout)
            return {
                "success": True,
                "message": "HTTP configuration updated",
                "settings": {
                    "verify_ssl": verify_ssl,
                    "proxy": proxy,
                    "timeout": timeout
                }
            }
        except Exception as e:
            return {"error": EdgarError("configure_http", str(e)).to_dict()}

    def clear_local_cache(self, dry_run: bool = True) -> Dict[str, Any]:
        """Clear local cache (dry_run=True for preview)"""
        try:
            result = clear_cache(dry_run=dry_run)
            return {
                "success": True,
                "dry_run": dry_run,
                "statistics": result
            }
        except Exception as e:
            return {"error": EdgarError("clear_cache", str(e)).to_dict()}

    def get_storage_info(self) -> Dict[str, Any]:
        """Get storage and cache information"""
        try:
            info = storage_info()
            return {
                "success": True,
                "storage_info": str(info)
            }
        except Exception as e:
            return {"error": EdgarError("storage_info", str(e)).to_dict()}

    # =========================================================================
    # COMPANY OPERATIONS
    # =========================================================================

    def get_company(self, ticker: str) -> Dict[str, Any]:
        """
        Get company information by ticker symbol

        Args:
            ticker: Stock ticker symbol (e.g., "AAPL", "MSFT")

        Returns:
            Company information including CIK, name, industry, etc.
        """
        try:
            company = Company(ticker)

            return {
                "success": True,
                "data": {
                    "cik": company.cik,
                    "name": company.name,
                    "ticker": company.get_ticker(),
                    "tickers": company.tickers if hasattr(company, 'tickers') else [],
                    "industry": company.industry if hasattr(company, 'industry') else None,
                    "sic": company.sic if hasattr(company, 'sic') else None,
                    "fiscal_year_end": company.fiscal_year_end if hasattr(company, 'fiscal_year_end') else None,
                    "business_address": str(company.business_address()) if company.business_address() else None,
                    "mailing_address": str(company.mailing_address()) if company.mailing_address() else None,
                    "filer_category": str(company.filer_category) if hasattr(company, 'filer_category') else None,
                    "is_large_accelerated_filer": company.is_large_accelerated_filer if hasattr(company, 'is_large_accelerated_filer') else False,
                    "is_accelerated_filer": company.is_accelerated_filer if hasattr(company, 'is_accelerated_filer') else False,
                    "is_smaller_reporting_company": company.is_smaller_reporting_company if hasattr(company, 'is_smaller_reporting_company') else False,
                },
                "parameters": {"ticker": ticker}
            }
        except Exception as e:
            return {"error": EdgarError("get_company", str(e), traceback.format_exc()).to_dict()}

    def get_company_filings(self, ticker: str,
                          form: Optional[str] = None,
                          year: Optional[Union[int, List[int]]] = None,
                          quarter: Optional[Union[int, List[int]]] = None,
                          limit: int = 10) -> Dict[str, Any]:
        """
        Get company filings with filtering

        Args:
            ticker: Stock ticker symbol
            form: Form type filter (e.g., "10-K", "10-Q", "8-K")
            year: Year or list of years
            quarter: Quarter or list of quarters (1-4)
            limit: Maximum number of filings to return

        Returns:
            List of filings
        """
        try:
            company = Company(ticker)
            filings = company.get_filings(form=form, year=year, quarter=quarter)

            # Convert to list and limit
            filings_list = []
            for i, filing in enumerate(filings):
                if i >= limit:
                    break
                filings_list.append({
                    "accession_number": filing.accession_number,
                    "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                    "form": filing.form if hasattr(filing, 'form') else None,
                    "company_name": filing.company if hasattr(filing, 'company') else None,
                    "cik": filing.cik if hasattr(filing, 'cik') else None,
                    "file_number": filing.file_number if hasattr(filing, 'file_number') else None,
                    "film_number": filing.film_number if hasattr(filing, 'film_number') else None,
                    "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
                    "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
                })

            return {
                "success": True,
                "data": filings_list,
                "count": len(filings_list),
                "parameters": {
                    "ticker": ticker,
                    "form": form,
                    "year": year,
                    "quarter": quarter,
                    "limit": limit
                }
            }
        except Exception as e:
            return {"error": EdgarError("get_company_filings", str(e), traceback.format_exc()).to_dict()}

    def get_corporate_events(self, ticker: str, limit: int = 10) -> Dict[str, Any]:
        """
        Get corporate events from 8-K filings.
        Fetches only the first few filing objects (limited) for items,
        and falls back to metadata for the rest.

        Args:
            ticker: Stock ticker symbol
            limit: Maximum number of 8-K filings to return

        Returns:
            List of corporate events
        """
        try:
            company = Company(ticker)
            filings = company.get_filings(form="8-K")

            events = []
            # Only parse obj() for first 3 filings to get items, rest use metadata
            PARSE_LIMIT = 3

            for i, filing in enumerate(filings):
                if i >= limit:
                    break

                event_data = {
                    "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                    "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
                    "form": "8-K",
                    "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
                    "accession_number": filing.accession_number,
                    "items": [],
                }

                # Only parse first few filings for item details
                if i < PARSE_LIMIT:
                    try:
                        form8k = filing.obj()
                        if form8k and hasattr(form8k, 'items') and form8k.items:
                            event_data["items"] = form8k.items
                        elif form8k and hasattr(form8k, 'item') and form8k.item:
                            event_data["items"] = [form8k.item]
                    except:
                        pass

                # Fallback: try filing-level description
                if not event_data["items"]:
                    if hasattr(filing, 'primary_doc_description') and filing.primary_doc_description:
                        event_data["items"] = [filing.primary_doc_description]
                    else:
                        event_data["items"] = ["8-K Filing"]

                events.append(event_data)

            return {
                "success": True,
                "data": events,
                "count": len(events),
                "parameters": {"ticker": ticker, "limit": limit}
            }
        except Exception as e:
            return {"error": EdgarError("get_corporate_events", str(e), traceback.format_exc()).to_dict()}

    def get_latest_filing(self, ticker: str, form: str, n: int = 1) -> Dict[str, Any]:
        """
        Get latest filing(s) of specific form type

        Args:
            ticker: Stock ticker symbol
            form: Form type (e.g., "10-K", "10-Q")
            n: Number of latest filings to return

        Returns:
            Latest filing(s) information
        """
        try:
            company = Company(ticker)
            filing = company.latest(form=form, n=n)

            if n == 1:
                filing_data = {
                    "accession_number": filing.accession_number,
                    "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                    "form": filing.form if hasattr(filing, 'form') else None,
                    "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
                    "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
                }
                return {
                    "success": True,
                    "data": filing_data,
                    "parameters": {"ticker": ticker, "form": form}
                }
            else:
                filings_list = []
                for f in filing:
                    filings_list.append({
                        "accession_number": f.accession_number,
                        "filing_date": str(f.filing_date) if hasattr(f, 'filing_date') else None,
                        "form": f.form if hasattr(f, 'form') else None,
                        "period_of_report": str(f.period_of_report) if hasattr(f, 'period_of_report') else None,
                        "filing_url": f.filing_url if hasattr(f, 'filing_url') else None,
                    })
                return {
                    "success": True,
                    "data": filings_list,
                    "count": len(filings_list),
                    "parameters": {"ticker": ticker, "form": form, "n": n}
                }
        except Exception as e:
            return {"error": EdgarError("get_latest_filing", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # FINANCIAL STATEMENTS
    # =========================================================================

    def get_financials(self, ticker: str, periods: int = 4,
                      annual: bool = True) -> Dict[str, Any]:
        """
        Get complete financial statements

        Args:
            ticker: Stock ticker symbol
            periods: Number of periods to retrieve
            annual: If True, annual data; if False, quarterly data

        Returns:
            Balance sheet, income statement, and cash flow data
        """
        try:
            company = Company(ticker)
            financials = company.get_financials() if annual else company.get_quarterly_financials()

            if not financials:
                return {
                    "error": EdgarError("get_financials",
                                      f"No financial data available for {ticker}").to_dict()
                }

            # Get all three statements (edgartools properties)
            balance_sheet = financials.balance_sheet
            income_stmt = financials.income_statement
            cash_flow = financials.cashflow_statement  # Note: cashflow_statement, not cash_flow

            return {
                "success": True,
                "data": {
                    "balance_sheet": balance_sheet.to_dict() if balance_sheet is not None else None,
                    "income_statement": income_stmt.to_dict() if income_stmt is not None else None,
                    "cash_flow": cash_flow.to_dict() if cash_flow is not None else None,
                    "statement_type": "annual" if annual else "quarterly",
                    "periods": periods
                },
                "parameters": {"ticker": ticker, "periods": periods, "annual": annual}
            }
        except Exception as e:
            return {"error": EdgarError("get_financials", str(e), traceback.format_exc()).to_dict()}

    def get_balance_sheet(self, ticker: str, periods: int = 4,
                         annual: bool = True, as_json: bool = True) -> Dict[str, Any]:
        """Get balance sheet data"""
        try:
            company = Company(ticker)
            financials = company.get_financials() if annual else company.get_quarterly_financials()
            if not financials:
                return {"error": EdgarError("get_balance_sheet", f"No financials for {ticker}").to_dict()}

            bs = financials.balance_sheet

            return {
                "success": True,
                "data": bs.to_dict() if as_json and bs is not None else str(bs),
                "parameters": {"ticker": ticker, "periods": periods, "annual": annual}
            }
        except Exception as e:
            return {"error": EdgarError("get_balance_sheet", str(e), traceback.format_exc()).to_dict()}

    def get_income_statement(self, ticker: str, periods: int = 4,
                            annual: bool = True, as_json: bool = True) -> Dict[str, Any]:
        """Get income statement data"""
        try:
            company = Company(ticker)
            financials = company.get_financials() if annual else company.get_quarterly_financials()
            if not financials:
                return {"error": EdgarError("get_income_statement", f"No financials for {ticker}").to_dict()}

            inc = financials.income_statement

            return {
                "success": True,
                "data": inc.to_dict() if as_json and inc is not None else str(inc),
                "parameters": {"ticker": ticker, "periods": periods, "annual": annual}
            }
        except Exception as e:
            return {"error": EdgarError("get_income_statement", str(e), traceback.format_exc()).to_dict()}

    def get_cash_flow(self, ticker: str, periods: int = 4,
                     annual: bool = True, as_json: bool = True) -> Dict[str, Any]:
        """Get cash flow statement data"""
        try:
            company = Company(ticker)
            financials = company.get_financials() if annual else company.get_quarterly_financials()
            if not financials:
                return {"error": EdgarError("get_cash_flow", f"No financials for {ticker}").to_dict()}

            cf = financials.cashflow_statement

            return {
                "success": True,
                "data": cf.to_dict() if as_json and cf is not None else str(cf),
                "parameters": {"ticker": ticker, "periods": periods, "annual": annual}
            }
        except Exception as e:
            return {"error": EdgarError("get_cash_flow", str(e), traceback.format_exc()).to_dict()}

    def get_company_facts(self, cik: Union[str, int]) -> Dict[str, Any]:
        """
        Get company facts (XBRL data) by CIK

        Args:
            cik: Company CIK number

        Returns:
            Company facts data
        """
        try:
            facts = get_company_facts(int(cik))

            # Extract key information
            return {
                "success": True,
                "data": str(facts)[:5000],  # Limit output size
                "note": "Facts data truncated for JSON output. Full data available via XBRL query.",
                "parameters": {"cik": str(cik)}
            }
        except Exception as e:
            return {"error": EdgarError("get_company_facts", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # INSIDER TRANSACTIONS (Forms 3, 4, 5)
    # =========================================================================

    def get_insider_transactions(self, ticker: str, limit: int = 20) -> Dict[str, Any]:
        """
        Get insider transactions (Form 4 filings)

        Args:
            ticker: Stock ticker symbol
            limit: Maximum number of transactions to return

        Returns:
            List of insider transactions
        """
        try:
            company = Company(ticker)
            filings = company.get_filings(form="4")

            transactions = []
            for i, filing in enumerate(filings):
                if i >= limit:
                    break

                try:
                    # Get Form 4 object
                    form4 = filing.obj()

                    if form4:
                        transactions.append({
                            "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                            "accession_number": filing.accession_number,
                            "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
                            "reporting_owner": str(form4.reporting_owner) if hasattr(form4, 'reporting_owner') else None,
                            "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
                        })
                except:
                    # If obj() fails, just add basic filing info
                    transactions.append({
                        "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                        "accession_number": filing.accession_number,
                        "form": "4",
                        "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
                    })

            return {
                "success": True,
                "data": transactions,
                "count": len(transactions),
                "parameters": {"ticker": ticker, "limit": limit}
            }
        except Exception as e:
            return {"error": EdgarError("get_insider_transactions", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # FUND HOLDINGS (13F)
    # =========================================================================

    def get_fund_holdings(self, ticker: str, limit: int = 5) -> Dict[str, Any]:
        """
        Get fund holdings (13F filings)

        Args:
            ticker: Fund ticker or company ticker
            limit: Number of filings to return

        Returns:
            List of 13F holdings data
        """
        try:
            company = Company(ticker)
            filings = company.get_filings(form="13F-HR")

            holdings_data = []
            for i, filing in enumerate(filings):
                if i >= limit:
                    break

                try:
                    thirteenf = filing.obj()

                    if thirteenf and hasattr(thirteenf, 'holdings'):
                        holdings_df = thirteenf.holdings

                        # Convert Decimal to float for JSON serialization
                        def convert_decimals(obj):
                            """Convert Decimal objects to float for JSON serialization"""
                            from decimal import Decimal
                            if isinstance(obj, Decimal):
                                return float(obj)
                            return obj

                        # Get holdings sample and convert to dict with Decimal handling
                        holdings_sample = None
                        if holdings_df is not None and hasattr(holdings_df, 'head'):
                            sample_df = holdings_df.head(10)
                            holdings_sample = sample_df.to_dict('records') if hasattr(sample_df, 'to_dict') else None
                            # Convert any Decimal values in the dict
                            if holdings_sample:
                                for record in holdings_sample:
                                    for key, value in record.items():
                                        record[key] = convert_decimals(value)

                        holdings_data.append({
                            "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                            "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
                            "total_value": convert_decimals(thirteenf.total_value) if hasattr(thirteenf, 'total_value') else None,
                            "total_holdings": int(thirteenf.total_holdings) if hasattr(thirteenf, 'total_holdings') else None,
                            "holdings_sample": holdings_sample,
                            "accession_number": filing.accession_number,
                        })
                except Exception as e:
                    holdings_data.append({
                        "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                        "accession_number": filing.accession_number,
                        "note": f"Unable to parse 13F data: {str(e)}"
                    })

            return {
                "success": True,
                "data": holdings_data,
                "count": len(holdings_data),
                "parameters": {"ticker": ticker, "limit": limit}
            }
        except Exception as e:
            return {"error": EdgarError("get_fund_holdings", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # PROXY STATEMENTS & EXECUTIVE COMPENSATION
    # =========================================================================

    def get_proxy_statement(self, ticker: str) -> Dict[str, Any]:
        """
        Get latest proxy statement with executive compensation.
        Uses a timeout to prevent hanging on large proxy documents.

        Args:
            ticker: Stock ticker symbol

        Returns:
            Proxy statement data including executive compensation
        """
        import signal
        import threading

        try:
            company = Company(ticker)
            filings = company.get_filings(form="DEF 14A")

            if not filings or len(filings) == 0:
                return {
                    "success": True,
                    "data": {
                        "named_executives": [],
                        "note": f"No proxy statements found for {ticker}"
                    },
                    "parameters": {"ticker": ticker}
                }

            latest_proxy_filing = filings[0]

            # Use a thread with timeout to prevent hanging on large proxy docs
            proxy_result = [None]
            proxy_error = [None]

            def fetch_proxy():
                try:
                    proxy_result[0] = latest_proxy_filing.obj()
                except Exception as e:
                    proxy_error[0] = str(e)

            thread = threading.Thread(target=fetch_proxy)
            thread.daemon = True
            thread.start()
            thread.join(timeout=15)  # 15 second timeout for proxy parsing

            if thread.is_alive() or proxy_error[0]:
                # Timed out or errored - return basic filing info
                return {
                    "success": True,
                    "data": {
                        "filing_date": str(latest_proxy_filing.filing_date) if hasattr(latest_proxy_filing, 'filing_date') else None,
                        "accession_number": latest_proxy_filing.accession_number,
                        "named_executives": [],
                        "note": "Proxy parsing timed out or failed"
                    },
                    "parameters": {"ticker": ticker}
                }

            proxy = proxy_result[0]

            if not proxy:
                return {
                    "success": True,
                    "data": {
                        "filing_date": str(latest_proxy_filing.filing_date) if hasattr(latest_proxy_filing, 'filing_date') else None,
                        "accession_number": latest_proxy_filing.accession_number,
                        "named_executives": [],
                        "note": "Proxy data not parseable"
                    },
                    "parameters": {"ticker": ticker}
                }

            data = {
                "filing_date": str(latest_proxy_filing.filing_date) if hasattr(latest_proxy_filing, 'filing_date') else None,
                "accession_number": latest_proxy_filing.accession_number,
                "named_executives": [],
            }

            # Named executives (check if not None and not empty)
            if hasattr(proxy, 'named_executives'):
                named_execs = proxy.named_executives
                if named_execs is not None:
                    try:
                        if hasattr(named_execs, 'to_dict'):
                            data["named_executives"] = named_execs.to_dict('records')[:10] if hasattr(named_execs, 'empty') and not named_execs.empty else []
                        elif hasattr(named_execs, '__iter__'):
                            data["named_executives"] = [
                                {
                                    "name": ne.name if hasattr(ne, 'name') else str(ne.get('name')) if isinstance(ne, dict) else None,
                                    "role": ne.role if hasattr(ne, 'role') else str(ne.get('role')) if isinstance(ne, dict) else None,
                                    "total_comp": ne.total_comp if hasattr(ne, 'total_comp') else ne.get('total_comp') if isinstance(ne, dict) else None,
                                }
                                for ne in list(named_execs)[:10]
                            ]
                    except:
                        pass

            return {
                "success": True,
                "data": data,
                "parameters": {"ticker": ticker}
            }
        except Exception as e:
            return {"error": EdgarError("get_proxy_statement", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # CURRENT FILINGS & REAL-TIME DATA
    # =========================================================================

    def get_current_filings(self, form: str = "", page_size: int = 40) -> Dict[str, Any]:
        """
        Get current filings from SEC (real-time data)

        Args:
            form: Form type filter (e.g., "10-K", "8-K")
            page_size: Number of filings per page

        Returns:
            Current filings data
        """
        try:
            current = get_current_filings(form=form, page_size=page_size)

            filings_list = []
            for filing in current:
                filings_list.append({
                    "accession_number": filing.accession_number,
                    "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                    "form": filing.form if hasattr(filing, 'form') else None,
                    "company_name": filing.company if hasattr(filing, 'company') else None,
                    "cik": filing.cik if hasattr(filing, 'cik') else None,
                })

            return {
                "success": True,
                "data": filings_list,
                "count": len(filings_list),
                "parameters": {"form": form, "page_size": page_size}
            }
        except Exception as e:
            return {"error": EdgarError("get_current_filings", str(e), traceback.format_exc()).to_dict()}

    def get_latest_filings(self, form: str = "", page_size: int = 40) -> Dict[str, Any]:
        """Get latest filings (alias for get_current_filings)"""
        return self.get_current_filings(form=form, page_size=page_size)

    # =========================================================================
    # SEARCH & DISCOVERY
    # =========================================================================

    def find_company_by_name(self, query: str, top_n: int = 10) -> Dict[str, Any]:
        """
        Search for companies by name

        Args:
            query: Search query string
            top_n: Number of results to return

        Returns:
            List of matching companies
        """
        try:
            results = find_company(query, top_n=top_n)

            companies = []
            if results:
                for result in results:
                    companies.append({
                        "cik": result.cik if hasattr(result, 'cik') else None,
                        "name": result.name if hasattr(result, 'name') else str(result),
                        "ticker": result.ticker if hasattr(result, 'ticker') else None,
                    })

            return {
                "success": True,
                "data": companies,
                "count": len(companies),
                "parameters": {"query": query, "top_n": top_n}
            }
        except Exception as e:
            return {"error": EdgarError("find_company", str(e), traceback.format_exc()).to_dict()}

    def find_by_identifier(self, identifier: str) -> Dict[str, Any]:
        """
        Find company or filing by identifier (ticker, CIK, or accession number)

        Args:
            identifier: Ticker, CIK, or accession number

        Returns:
            Entity or filing information
        """
        try:
            result = find(identifier)

            if result:
                return {
                    "success": True,
                    "data": {
                        "type": type(result).__name__,
                        "info": str(result)[:1000],
                    },
                    "parameters": {"identifier": identifier}
                }
            else:
                return {
                    "error": EdgarError("find", f"No results found for identifier: {identifier}").to_dict()
                }
        except Exception as e:
            return {"error": EdgarError("find", str(e), traceback.format_exc()).to_dict()}

    def get_company_tickers_list(self, limit: int = 100) -> Dict[str, Any]:
        """
        Get list of company tickers from SEC

        Args:
            limit: Maximum number of tickers to return

        Returns:
            List of company tickers with CIK and names
        """
        try:
            tickers_df = get_company_tickers(as_dataframe=True)

            if tickers_df is not None:
                # Limit and convert to dict
                limited_df = tickers_df.head(limit)
                tickers_list = limited_df.to_dict('records')

                return {
                    "success": True,
                    "data": tickers_list,
                    "count": len(tickers_list),
                    "total_available": len(tickers_df),
                    "parameters": {"limit": limit}
                }
            else:
                return {
                    "error": EdgarError("get_company_tickers", "Unable to retrieve ticker data").to_dict()
                }
        except Exception as e:
            return {"error": EdgarError("get_company_tickers", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # FILING OPERATIONS
    # =========================================================================

    def get_filing_by_accession(self, accession_number: str) -> Dict[str, Any]:
        """
        Get specific filing by accession number

        Args:
            accession_number: SEC accession number

        Returns:
            Filing information and metadata
        """
        try:
            filing = get_by_accession_number(accession_number)

            if not filing:
                return {
                    "error": EdgarError("get_by_accession_number",
                                      f"Filing not found: {accession_number}").to_dict()
                }

            return {
                "success": True,
                "data": {
                    "accession_number": filing.accession_number,
                    "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                    "form": filing.form if hasattr(filing, 'form') else None,
                    "company_name": filing.company if hasattr(filing, 'company') else None,
                    "cik": filing.cik if hasattr(filing, 'cik') else None,
                    "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
                    "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
                },
                "parameters": {"accession_number": accession_number}
            }
        except Exception as e:
            return {"error": EdgarError("get_by_accession_number", str(e), traceback.format_exc()).to_dict()}

    def get_filing_text(self, accession_number: str, max_length: int = 10000) -> Dict[str, Any]:
        """
        Get filing text content

        Args:
            accession_number: SEC accession number
            max_length: Maximum text length to return

        Returns:
            Filing text content
        """
        try:
            filing = get_by_accession_number(accession_number)

            if not filing:
                return {
                    "error": EdgarError("get_filing_text",
                                      f"Filing not found: {accession_number}").to_dict()
                }

            text = filing.text() if hasattr(filing, 'text') else str(filing)

            # Limit text length
            if len(text) > max_length:
                text = text[:max_length] + "... (truncated)"

            return {
                "success": True,
                "data": {
                    "text": text,
                    "text_length": len(text),
                    "accession_number": accession_number,
                },
                "parameters": {"accession_number": accession_number, "max_length": max_length}
            }
        except Exception as e:
            return {"error": EdgarError("get_filing_text", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # BULK FILING OPERATIONS
    # =========================================================================

    def get_filings_by_date(self, year: Union[int, List[int]],
                          quarter: Optional[Union[int, List[int]]] = None,
                          form: Optional[str] = None,
                          limit: int = 50) -> Dict[str, Any]:
        """
        Get filings by date range

        Args:
            year: Year or list of years
            quarter: Quarter or list of quarters (1-4)
            form: Form type filter
            limit: Maximum number of filings

        Returns:
            List of filings
        """
        try:
            filings = get_filings(year=year, quarter=quarter, form=form)

            if not filings:
                return {
                    "success": True,
                    "data": [],
                    "count": 0,
                    "parameters": {"year": year, "quarter": quarter, "form": form}
                }

            filings_list = []
            for i, filing in enumerate(filings):
                if i >= limit:
                    break
                filings_list.append({
                    "accession_number": filing.accession_number,
                    "filing_date": str(filing.filing_date) if hasattr(filing, 'filing_date') else None,
                    "form": filing.form if hasattr(filing, 'form') else None,
                    "company_name": filing.company if hasattr(filing, 'company') else None,
                    "cik": filing.cik if hasattr(filing, 'cik') else None,
                })

            return {
                "success": True,
                "data": filings_list,
                "count": len(filings_list),
                "parameters": {"year": year, "quarter": quarter, "form": form, "limit": limit}
            }
        except Exception as e:
            return {"error": EdgarError("get_filings_by_date", str(e), traceback.format_exc()).to_dict()}

    # =========================================================================
    # XBRL OPERATIONS
    # =========================================================================

    def get_xbrl_data(self, ticker: str, form: str = "10-K") -> Dict[str, Any]:
        """
        Get XBRL data from latest filing

        Args:
            ticker: Stock ticker symbol
            form: Form type (default "10-K")

        Returns:
            XBRL data summary
        """
        try:
            company = Company(ticker)
            filing = company.latest(form=form)

            if not filing:
                return {
                    "error": EdgarError("get_xbrl_data",
                                      f"No {form} filing found for {ticker}").to_dict()
                }

            xbrl = filing.xbrl()

            if not xbrl:
                return {
                    "error": EdgarError("get_xbrl_data",
                                      f"No XBRL data available for filing").to_dict()
                }

            return {
                "success": True,
                "data": {
                    "entity_name": xbrl.entity_name if hasattr(xbrl, 'entity_name') else None,
                    "document_type": xbrl.document_type if hasattr(xbrl, 'document_type') else None,
                    "period_of_report": str(xbrl.period_of_report) if hasattr(xbrl, 'period_of_report') else None,
                    "reporting_periods": [str(p) for p in xbrl.reporting_periods] if hasattr(xbrl, 'reporting_periods') else [],
                    "statement_count": len(xbrl.statements) if hasattr(xbrl, 'statements') else 0,
                    "fact_count": len(xbrl.facts) if hasattr(xbrl, 'facts') else 0,
                },
                "parameters": {"ticker": ticker, "form": form}
            }
        except Exception as e:
            return {"error": EdgarError("get_xbrl_data", str(e), traceback.format_exc()).to_dict()}


def main(args=None):
    """Main CLI interface - supports both PyO3 and subprocess execution"""
    # Support both PyO3 (args parameter) and subprocess (sys.argv)
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        print(json.dumps({
            "error": "Usage: python edgar_tools.py <command> [args...]",
            "commands": {
                "configuration": [
                    "set_identity <identity_string>",
                    "get_identity",
                    "configure_connection [verify_ssl] [proxy] [timeout]",
                    "clear_cache [dry_run:true|false]",
                    "get_storage_info"
                ],
                "company": [
                    "get_company <ticker>",
                    "get_company_filings <ticker> [form] [year] [quarter] [limit]",
                    "get_corporate_events <ticker> [limit]",
                    "get_latest_filing <ticker> <form> [n]",
                    "find_company <query> [top_n]",
                    "find <identifier>",
                    "get_company_tickers [limit]"
                ],
                "financials": [
                    "get_financials <ticker> [periods] [annual:true|false]",
                    "get_balance_sheet <ticker> [periods] [annual:true|false]",
                    "get_income_statement <ticker> [periods] [annual:true|false]",
                    "get_cash_flow <ticker> [periods] [annual:true|false]",
                    "get_company_facts <cik>",
                    "get_xbrl_data <ticker> [form]"
                ],
                "insider_transactions": [
                    "get_insider_transactions <ticker> [limit]"
                ],
                "fund_holdings": [
                    "get_fund_holdings <ticker> [limit]"
                ],
                "proxy": [
                    "get_proxy_statement <ticker>"
                ],
                "current_filings": [
                    "get_current_filings [form] [page_size]",
                    "get_latest_filings [form] [page_size]"
                ],
                "filing_operations": [
                    "get_filing_by_accession <accession_number>",
                    "get_filing_text <accession_number> [max_length]",
                    "get_filings_by_date <year> [quarter] [form] [limit]"
                ]
            }
        }, indent=2))
        sys.exit(1)

    command = args[0]

    # Initialize wrapper with default identity
    try:
        wrapper = EdgarToolsWrapper()
    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)

    try:
        # Configuration commands
        if command == "set_identity":
            identity = args[1] if len(args) > 1 else None
            result = wrapper.set_user_identity(identity)

        elif command == "get_identity":
            result = wrapper.get_user_identity()

        elif command == "configure_connection":
            verify_ssl = args[1].lower() == "true" if len(args) > 1 else None
            proxy = args[2] if len(args) > 2 else None
            timeout = float(args[3]) if len(args) > 3 else None
            result = wrapper.configure_connection(verify_ssl, proxy, timeout)

        elif command == "clear_cache":
            dry_run = args[1].lower() == "true" if len(args) > 1 else True
            result = wrapper.clear_local_cache(dry_run)

        elif command == "get_storage_info":
            result = wrapper.get_storage_info()

        # Company commands
        elif command == "get_company":
            if len(args) < 2:
                result = {"error": "Usage: edgar_tools.py get_company <ticker>"}
            else:
                ticker = args[1]
                result = wrapper.get_company(ticker)

        elif command == "get_company_filings":
            if len(args) < 2:
                result = {"error": "Usage: edgar_tools.py get_company_filings <ticker> [form] [year] [quarter] [limit]"}
            else:
                ticker = args[1]
                form = args[2] if len(args) > 2 else None
                year = int(args[3]) if len(args) > 3 else None
                quarter = int(args[4]) if len(args) > 4 else None
                limit = int(args[5]) if len(args) > 5 else 10
                result = wrapper.get_company_filings(ticker, form, year, quarter, limit)

        elif command == "get_corporate_events":
            if len(args) < 2:
                result = {"error": "Usage: edgar_tools.py get_corporate_events <ticker> [limit]"}
            else:
                ticker = args[1]
                limit = int(args[2]) if len(args) > 2 else 20
                result = wrapper.get_corporate_events(ticker, limit)

        elif command == "get_latest_filing":
            if len(args) < 2:
                result = {"error": "Usage: edgar_tools.py get_latest_filing <ticker> [form] [n]"}
            else:
                ticker = args[1]
                form = args[2] if len(args) > 2 else "10-K"
                n = int(args[3]) if len(args) > 3 else 1
                result = wrapper.get_latest_filing(ticker, form, n)

        # Financial commands
        elif command == "get_financials":
            ticker = args[1] if len(args) > 1 else None
            periods = int(args[2]) if len(args) > 2 else 4
            annual = args[3].lower() != "false" if len(args) > 3 else True
            result = wrapper.get_financials(ticker, periods, annual)

        elif command == "get_balance_sheet":
            ticker = args[1] if len(args) > 1 else None
            periods = int(args[2]) if len(args) > 2 else 4
            annual = args[3].lower() != "false" if len(args) > 3 else True
            result = wrapper.get_balance_sheet(ticker, periods, annual)

        elif command == "get_income_statement":
            ticker = args[1] if len(args) > 1 else None
            periods = int(args[2]) if len(args) > 2 else 4
            annual = args[3].lower() != "false" if len(args) > 3 else True
            result = wrapper.get_income_statement(ticker, periods, annual)

        elif command == "get_cash_flow":
            ticker = args[1] if len(args) > 1 else None
            periods = int(args[2]) if len(args) > 2 else 4
            annual = args[3].lower() != "false" if len(args) > 3 else True
            result = wrapper.get_cash_flow(ticker, periods, annual)

        elif command == "get_company_facts":
            cik = args[1] if len(args) > 1 else None
            result = wrapper.get_company_facts(cik)

        elif command == "get_xbrl_data":
            ticker = args[1] if len(args) > 1 else None
            form = args[2] if len(args) > 2 else "10-K"
            result = wrapper.get_xbrl_data(ticker, form)

        # Insider transactions
        elif command == "get_insider_transactions":
            ticker = args[1] if len(args) > 1 else None
            limit = int(args[2]) if len(args) > 2 else 20
            result = wrapper.get_insider_transactions(ticker, limit)

        # Fund holdings
        elif command == "get_fund_holdings":
            ticker = args[1] if len(args) > 1 else None
            limit = int(args[2]) if len(args) > 2 else 5
            result = wrapper.get_fund_holdings(ticker, limit)

        # Proxy statement
        elif command == "get_proxy_statement":
            if len(args) < 2:
                result = {"error": "Usage: edgar_tools.py get_proxy_statement <ticker>"}
            else:
                ticker = args[1]
                result = wrapper.get_proxy_statement(ticker)

        # Current filings
        elif command == "get_current_filings" or command == "get_latest_filings":
            form = args[1] if len(args) > 1 else ""
            page_size = int(args[2]) if len(args) > 2 else 40
            result = wrapper.get_current_filings(form, page_size)

        # Search commands
        elif command == "find_company":
            query = args[1] if len(args) > 1 else None
            top_n = int(args[2]) if len(args) > 2 else 10
            result = wrapper.find_company_by_name(query, top_n)

        elif command == "find":
            identifier = args[1] if len(args) > 1 else None
            result = wrapper.find_by_identifier(identifier)

        elif command == "get_company_tickers":
            limit = int(args[1]) if len(args) > 1 else 100
            result = wrapper.get_company_tickers_list(limit)

        # Filing operations
        elif command == "get_filing_by_accession":
            accession = args[1] if len(args) > 1 else None
            result = wrapper.get_filing_by_accession(accession)

        elif command == "get_filing_text":
            accession = args[1] if len(args) > 1 else None
            max_length = int(args[2]) if len(args) > 2 else 10000
            result = wrapper.get_filing_text(accession, max_length)

        elif command == "get_filings_by_date":
            year = int(args[1]) if len(args) > 1 else None
            quarter = int(args[2]) if len(args) > 2 else None
            form = args[3] if len(args) > 3 else None
            limit = int(args[4]) if len(args) > 4 else 50
            result = wrapper.get_filings_by_date(year, quarter, form, limit)

        else:
            result = {"error": EdgarError(command, f"Unknown command: {command}").to_dict()}

        # Return JSON for PyO3, print for subprocess
        output = json.dumps(result, indent=2)
        print(output)
        return output

    except Exception as e:
        error_output = json.dumps({
            "error": EdgarError(command, str(e), traceback.format_exc()).to_dict()
        }, indent=2)
        print(error_output)
        return error_output


if __name__ == "__main__":
    main()
