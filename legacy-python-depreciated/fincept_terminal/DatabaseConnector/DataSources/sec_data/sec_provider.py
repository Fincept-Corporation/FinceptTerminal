# -*- coding: utf-8 -*-
# sec_provider.py

import asyncio
import aiohttp
from datetime import datetime, date
from typing import Any, Dict, Optional, List, Union, Literal
from warnings import warn
import json

from fincept_terminal.utils.Logging.logger import info, debug, warning, error, operation, monitor_performance

# Import utility functions
from fincept_terminal.DatabaseConnector.DataSources.sec_data.utils.helpers import (
    symbol_map, cik_map, get_all_companies, get_all_ciks,
    get_mf_and_etf_map, search_institutions, get_schema_filelist,
    download_zip_file, get_ftd_urls, get_nport_candidates
)
from .utils.form4 import get_form_4
from .utils.parse_13f import get_13f_candidates, parse_13f_hr
from .utils.frames import get_frame, get_concept
from .utils.definitions import HEADERS, SEC_HEADERS, FORM_LIST


class SECProvider:
    """SEC data provider with complete API integration for all 17 endpoints"""

    def __init__(self, rate_limit: int = 5, use_cache: bool = True):
        self.rate_limit = rate_limit
        self.use_cache = use_cache
        self._session = None
        self.headers = SEC_HEADERS

    async def _get_session(self) -> aiohttp.ClientSession:
        """Get or create aiohttp session"""
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=60),
                connector=aiohttp.TCPConnector(limit=10)
            )
        return self._session

    def _format_response(self, data: Any, endpoint: str, **kwargs) -> Dict[str, Any]:
        """Format response with metadata"""
        return {
            "success": True,
            "source": "sec",
            "endpoint": endpoint,
            "data": data,
            "fetched_at": datetime.now().isoformat(),
            **kwargs
        }

    def _error_response(self, error_msg: str, endpoint: str) -> Dict[str, Any]:
        """Format error response"""
        return {
            "success": False,
            "error": error_msg,
            "source": "sec",
            "endpoint": endpoint,
            "fetched_at": datetime.now().isoformat()
        }

    # 1. CIK Mapping - INTEGRATED
    @monitor_performance
    async def get_cik_map(self, symbol: str) -> Dict[str, Any]:
        """Convert stock symbol to CIK number"""
        try:
            with operation(f"SEC CIK mapping for {symbol}"):
                cik = await symbol_map(symbol.upper(), self.use_cache)
                if cik:
                    return self._format_response({"cik": cik}, "cik_map", symbol=symbol)
                else:
                    return self._error_response(f"No CIK found for symbol {symbol}", "cik_map")
        except Exception as e:
            error(f"CIK mapping error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "cik_map")

    # 2. Symbol Mapping - INTEGRATED
    @monitor_performance
    async def get_symbol_map(self, cik: Union[str, int]) -> Dict[str, Any]:
        """Convert CIK number to stock symbol"""
        try:
            with operation(f"SEC symbol mapping for CIK {cik}"):
                symbol = await cik_map(cik, self.use_cache)
                if symbol and not symbol.startswith("Error"):
                    return self._format_response({"symbol": symbol}, "symbol_map", cik=cik)
                else:
                    return self._error_response(f"No symbol found for CIK {cik}", "symbol_map")
        except Exception as e:
            error(f"Symbol mapping error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "symbol_map")

    # 3. Company Filings - INTEGRATED
    @monitor_performance
    async def get_company_filings(self, symbol: Optional[str] = None, cik: Optional[str] = None,
                                  form_type: Optional[str] = None, limit: int = 100,
                                  start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get company SEC filings"""
        try:
            from .models.company_filings import SecCompanyFilingsFetcher

            identifier = symbol or cik
            with operation(f"SEC company filings for {identifier}"):

                fetcher = SecCompanyFilingsFetcher()
                params = {
                    "symbol": symbol,
                    "cik": cik,
                    "form_type": form_type,
                    "limit": limit,
                    "start_date": start_date,
                    "end_date": end_date,
                    "use_cache": self.use_cache
                }

                # Remove None values
                params = {k: v for k, v in params.items() if v is not None}

                filings = await fetcher.fetch_data(params, {})

                # Convert to dict format
                filings_data = [filing.model_dump() for filing in filings] if filings else []

                return self._format_response(filings_data, "company_filings",
                                             symbol=symbol, cik=cik, count=len(filings_data))

        except Exception as e:
            error(f"Company filings error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "company_filings")

    # 4. Compare Company Facts - INTEGRATED
    @monitor_performance
    async def get_compare_company_facts(self, symbols: Union[str, List[str]], fact: str = "Revenues",
                                        year: Optional[int] = None,
                                        fiscal_period: Optional[str] = None) -> Dict[str, Any]:
        """Compare XBRL facts across companies"""
        try:
            if isinstance(symbols, str):
                symbols = [symbols]

            with operation(f"SEC compare facts for {symbols}"):

                # Use frames utility for single company or concept utility for multiple
                if len(symbols) == 1:
                    data = await get_concept(symbols[0], fact, year, use_cache=self.use_cache)
                else:
                    # For multiple symbols, get concept data for each
                    results = []
                    for symbol in symbols:
                        try:
                            concept_data = await get_concept(symbol, fact, year, use_cache=self.use_cache)
                            results.append(concept_data)
                        except Exception:
                            continue
                    data = {"metadata": {}, "data": results}

                return self._format_response(data, "compare_company_facts",
                                             fact=fact, symbols=symbols)

        except Exception as e:
            error(f"Compare company facts error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "compare_company_facts")

    # 5. Equity FTD - INTEGRATED
    @monitor_performance
    async def get_equity_ftd(self, symbol: str, limit: int = 24, skip_reports: int = 0) -> Dict[str, Any]:
        """Get equity fails-to-deliver data"""
        try:
            from .models.equity_ftd import SecEquityFtdFetcher

            with operation(f"SEC FTD data for {symbol}"):
                fetcher = SecEquityFtdFetcher()
                params = {
                    "symbol": symbol.upper(),
                    "limit": limit,
                    "skip_reports": skip_reports,
                    "use_cache": self.use_cache
                }

                ftd_data = await fetcher.fetch_data(params, {})
                filings_data = [item.model_dump() for item in ftd_data] if ftd_data else []

                return self._format_response(filings_data, "equity_ftd", symbol=symbol)

        except Exception as e:
            error(f"Equity FTD error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "equity_ftd")

    # 6. Equity Search - INTEGRATED
    @monitor_performance
    async def get_equity_search(self, query: str, is_fund: bool = False) -> Dict[str, Any]:
        """Search for equity securities"""
        try:
            from .models.equity_search import SecEquitySearchFetcher

            with operation(f"SEC equity search for {query}"):
                fetcher = SecEquitySearchFetcher()
                params = {
                    "query": query,
                    "is_fund": is_fund,
                    "use_cache": self.use_cache
                }

                search_results = await fetcher.fetch_data(params, {})
                results_data = [item.model_dump() for item in search_results] if search_results else []

                return self._format_response(results_data, "equity_search", query=query, is_fund=is_fund)

        except Exception as e:
            error(f"Equity search error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "equity_search")

    # 7. ETF Holdings - INTEGRATED
    @monitor_performance
    async def get_etf_holdings(self, symbol: str, date: Optional[str] = None) -> Dict[str, Any]:
        """Get ETF holdings from N-PORT filings"""
        try:
            from .models.etf_holdings import SecEtfHoldingsFetcher

            with operation(f"SEC ETF holdings for {symbol}"):
                fetcher = SecEtfHoldingsFetcher()
                params = {
                    "symbol": symbol.upper(),
                    "date": date,
                    "use_cache": self.use_cache
                }

                # Remove None values
                params = {k: v for k, v in params.items() if v is not None}

                holdings_data = await fetcher.fetch_data(params, {})

                if hasattr(holdings_data, 'result'):
                    # AnnotatedResult format
                    results_data = [item.model_dump() for item in holdings_data.result]
                    metadata = holdings_data.metadata
                else:
                    # Direct list format
                    results_data = [item.model_dump() for item in holdings_data] if holdings_data else []
                    metadata = {}

                return self._format_response({
                    "holdings": results_data,
                    "metadata": metadata
                }, "etf_holdings", symbol=symbol)

        except Exception as e:
            error(f"ETF holdings error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "etf_holdings")

    # 8. Form 13F-HR - INTEGRATED
    @monitor_performance
    async def get_form_13f_hr(self, symbol: str, limit: int = 10,
                              date: Optional[str] = None) -> Dict[str, Any]:
        """Get institutional holdings from 13F-HR filings"""
        try:
            from .models.form_13FHR import SecForm13FHRFetcher

            with operation(f"SEC 13F-HR for {symbol}"):
                fetcher = SecForm13FHRFetcher()
                params = {
                    "symbol": symbol,
                    "limit": limit,
                    "date": date,
                }

                # Remove None values
                params = {k: v for k, v in params.items() if v is not None}

                filings_data = await fetcher.fetch_data(params, {})
                results_data = [item.model_dump() for item in filings_data] if filings_data else []

                return self._format_response(results_data, "form_13f_hr", symbol=symbol)

        except Exception as e:
            error(f"Form 13F-HR error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "form_13f_hr")

    # 9. HTM File Download - INTEGRATED
    @monitor_performance
    async def get_htm_file(self, url: str) -> Dict[str, Any]:
        """Download and parse SEC HTM/HTML file"""
        try:
            from .models.htm_file import SecHtmFileFetcher

            with operation(f"SEC HTM file download"):
                fetcher = SecHtmFileFetcher()
                params = {
                    "url": url,
                    "use_cache": self.use_cache
                }

                htm_data = await fetcher.fetch_data(params, {})
                result_data = htm_data.model_dump() if htm_data else {}

                return self._format_response(result_data, "htm_file")

        except Exception as e:
            error(f"HTM file error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "htm_file")

    # 10. Insider Trading - INTEGRATED
    @monitor_performance
    async def get_insider_trading(self, symbol: str, start_date: Optional[str] = None,
                                  end_date: Optional[str] = None, limit: int = 100) -> Dict[str, Any]:
        """Get insider trading data from Form 4 filings"""
        try:
            with operation(f"SEC insider trading for {symbol}"):

                # Convert string dates to date objects if provided
                start_date_obj = datetime.strptime(start_date, "%Y-%m-%d").date() if start_date else None
                end_date_obj = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else None

                trading_data = await get_form_4(
                    symbol,
                    start_date_obj,
                    end_date_obj,
                    limit,
                    self.use_cache
                )

                return self._format_response(trading_data, "insider_trading",
                                             symbol=symbol, start_date=start_date, end_date=end_date)

        except Exception as e:
            error(f"Insider trading error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "insider_trading")

    # 11. Institutions Search - INTEGRATED
    @monitor_performance
    async def get_institutions_search(self, query: str) -> Dict[str, Any]:
        """Search for institutional filers"""
        try:
            from .models.institutions_search import SecInstitutionsSearchFetcher

            with operation(f"SEC institutions search for {query}"):
                fetcher = SecInstitutionsSearchFetcher()
                params = {
                    "query": query,
                    "use_cache": self.use_cache
                }

                search_results = await fetcher.fetch_data(params, {})
                results_data = [item.model_dump() for item in search_results] if search_results else []

                return self._format_response(results_data, "institutions_search", query=query)

        except Exception as e:
            error(f"Institutions search error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "institutions_search")

    # 12. Latest Financial Reports - INTEGRATED
    @monitor_performance
    async def get_latest_financial_reports(self, date: Optional[str] = None,
                                           report_type: Optional[str] = None) -> Dict[str, Any]:
        """Get latest financial reports"""
        try:
            from .models.latest_financial_reports import SecLatestFinancialReportsFetcher

            with operation("SEC latest financial reports"):
                fetcher = SecLatestFinancialReportsFetcher()
                params = {
                    "date": date,
                    "report_type": report_type
                }

                # Remove None values
                params = {k: v for k, v in params.items() if v is not None}

                reports_data = await fetcher.fetch_data(params, {})
                results_data = [item.model_dump() for item in reports_data] if reports_data else []

                return self._format_response(results_data, "latest_financial_reports", date=date)

        except Exception as e:
            error(f"Latest financial reports error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "latest_financial_reports")

    # 13. Management Discussion & Analysis - INTEGRATED
    @monitor_performance
    async def get_management_discussion_analysis(self, symbol: str,
                                                 calendar_year: Optional[int] = None,
                                                 calendar_period: Optional[str] = None) -> Dict[str, Any]:
        """Get MD&A section from 10-K/10-Q filings"""
        try:
            from .models.management_discussion_analysis import SecManagementDiscussionAnalysisFetcher

            with operation(f"SEC MD&A for {symbol}"):
                fetcher = SecManagementDiscussionAnalysisFetcher()
                params = {
                    "symbol": symbol,
                    "calendar_year": calendar_year,
                    "calendar_period": calendar_period,
                    "use_cache": self.use_cache
                }

                # Remove None values
                params = {k: v for k, v in params.items() if v is not None}

                mda_data = await fetcher.fetch_data(params, {})
                result_data = mda_data.model_dump() if mda_data else {}

                return self._format_response(result_data, "management_discussion_analysis", symbol=symbol)

        except Exception as e:
            error(f"MD&A error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "management_discussion_analysis")

    # 14. RSS Litigation - INTEGRATED
    @monitor_performance
    async def get_rss_litigation(self) -> Dict[str, Any]:
        """Get SEC litigation releases from RSS feed"""
        try:
            from .models.rss_litigation import SecRssLitigationFetcher

            with operation("SEC RSS litigation"):
                fetcher = SecRssLitigationFetcher()
                params = {}

                litigation_data = await fetcher.fetch_data(params, {})
                results_data = [item.model_dump() for item in litigation_data] if litigation_data else []

                return self._format_response(results_data, "rss_litigation")

        except Exception as e:
            error(f"RSS litigation error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "rss_litigation")

    # 15. Schema Files - INTEGRATED
    @monitor_performance
    async def get_schema_files(self, query: str = "", url: str = "") -> Dict[str, Any]:
        """Get XBRL schema files list"""
        try:
            from .models.schema_files import SecSchemaFilesFetcher

            with operation("SEC schema files"):
                fetcher = SecSchemaFilesFetcher()
                params = {
                    "query": query,
                    "url": url,
                    "use_cache": self.use_cache
                }

                # Remove empty strings
                params = {k: v for k, v in params.items() if v != ""}

                schema_data = await fetcher.fetch_data(params, {})
                result_data = schema_data.model_dump() if schema_data else {}

                return self._format_response(result_data, "schema_files", query=query)

        except Exception as e:
            error(f"Schema files error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "schema_files")

    # 16. SEC Filing - INTEGRATED
    @monitor_performance
    async def get_sec_filing(self, url: str) -> Dict[str, Any]:
        """Get complete SEC filing data with document URLs"""
        try:
            from .models.sec_filing import SecFilingFetcher

            with operation("SEC filing data"):
                fetcher = SecFilingFetcher()
                params = {
                    "url": url,
                    "use_cache": self.use_cache
                }

                filing_data = await fetcher.fetch_data(params, {})
                result_data = filing_data.model_dump() if filing_data else {}

                return self._format_response(result_data, "sec_filing", url=url)

        except Exception as e:
            error(f"SEC filing error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "sec_filing")

    # 17. SIC Search - INTEGRATED
    @monitor_performance
    async def get_sic_search(self, query: str) -> Dict[str, Any]:
        """Search Standard Industrial Classification codes"""
        try:
            from .models.sic_search import SecSicSearchFetcher

            with operation(f"SEC SIC search for {query}"):
                fetcher = SecSicSearchFetcher()
                params = {
                    "query": query,
                    "use_cache": self.use_cache
                }

                sic_data = await fetcher.fetch_data(params, {})
                results_data = [item.model_dump() for item in sic_data] if sic_data else []

                return self._format_response(results_data, "sic_search", query=query)

        except Exception as e:
            error(f"SIC search error: {str(e)}", module="SECProvider")
            return self._error_response(str(e), "sic_search")

    # Utility Methods
    async def close(self):
        """Close the aiohttp session"""
        if self._session and not self._session.closed:
            await self._session.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._session and not self._session.closed:
            asyncio.create_task(self._session.close())

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.close()