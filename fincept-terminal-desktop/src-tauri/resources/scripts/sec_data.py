# SEC (Securities and Exchange Commission) Data Wrapper
# Modular, fault-tolerant design - each endpoint works independently
# Official SEC data from EDGAR database

import sys
import json
import requests
import pandas as pd
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import traceback
import re
import time


class SECError:
    """Custom error class for SEC API errors"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "endpoint": self.endpoint,
            "error": self.error,
            "status_code": self.status_code,
            "timestamp": self.timestamp,
            "type": "SECError"
        }


class SECDataWrapper:
    """Modular SEC data wrapper with fault-tolerant endpoints"""

    def __init__(self):
        self.base_url = "https://www.sec.gov"
        self.data_url = "https://data.sec.gov"
        self.session = requests.Session()

        # SEC requires specific headers to avoid blocking
        self.session.headers.update({
            'User-Agent': 'Fincept Terminal - financial analysis tool (contact@fincept.com)',
            'Accept-Encoding': 'gzip, deflate',
            'Accept': 'application/json'
        })

        # Cache for company tickers
        self._company_tickers_cache = None
        self._cache_timestamp = None

        # Form types list (simplified from full SEC list)
        self.common_form_types = [
            "10-K", "10-Q", "8-K", "10-D", "20-F", "40-F", "6-K", "8-A",
            "S-1", "S-3", "S-4", "S-8", "F-1", "F-3", "F-4", "424A", "424B",
            "SC 13D", "SC 13G", "13F-HR", "13F-NT", "3", "4", "5", "144",
            "N-1A", "N-2", "N-CSR", "N-PX", "N-PORT", "485BPOS"
        ]

        # SEC taxonomy for financial data
        self.taxonomy_facts = [
            "Revenues", "RevenueFromContractWithCustomerExcludingAssessedTax",
            "NetIncomeLoss", "Assets", "Liabilities", "StockholdersEquity",
            "CashAndCashEquivalentsAtCarryingValue", "AccountsReceivableNetCurrent",
            "InventoryNet", "PropertyPlantAndEquipmentNet", "Goodwill",
            "LongTermDebt", "OperatingIncomeLoss", "EarningsPerShareBasic",
            "EarningsPerShareDiluted", "CostOfGoodsAndServicesSold", "SellingGeneralAndAdministrativeExpense",
            "ResearchAndDevelopmentExpense", "InterestExpenseDebt", "IncomeTaxExpenseBenefit"
        ]

        # Transaction code mappings for insider trading
        self.transaction_codes = {
            "A": "Grant, award or other acquisition",
            "C": "Conversion of derivative security",
            "D": "Disposition to the issuer",
            "E": "Expiration of short derivative position",
            "F": "Payment of exercise price or tax liability",
            "G": "Bona fide gift",
            "H": "Expiration of long derivative position",
            "I": "Discretionary transaction",
            "J": "Other acquisition or disposition",
            "L": "Small acquisition under Rule 16a-6",
            "M": "Exercise or conversion of derivative security",
            "O": "Exercise of out-of-the-money derivative security",
            "P": "Open market or private purchase",
            "S": "Open market or private sale",
            "U": "Disposition pursuant to a tender of shares",
            "W": "Acquisition or disposition by will or the laws of descent and distribution",
            "X": "Exercise of in-the-money or at-the-money derivative security",
            "Z": "Deposit into or withdrawal from voting trust"
        }

    def _make_request(self, url: str, use_cache: bool = True) -> Dict[str, Any]:
        """Make HTTP request with proper headers and error handling"""
        try:
            # Add delay to respect SEC rate limits
            time.sleep(0.1)

            response = self.session.get(url, timeout=30)
            response.raise_for_status()

            # Parse JSON response
            data = response.json()

            # Check for API errors
            if isinstance(data, dict) and "error" in data:
                raise Exception(f"SEC API error: {data['error']}")

            return data

        except requests.exceptions.RequestException as e:
            raise Exception(f"HTTP request failed: {str(e)}")
        except json.JSONDecodeError as e:
            raise Exception(f"JSON decode error: {str(e)}")

    def _normalize_cik(self, cik: Union[str, int]) -> str:
        """Normalize CIK to 10-digit format with leading zeros"""
        cik_str = str(cik).strip()

        # Remove leading zeros and reformat
        cik_str = cik_str.lstrip('0')

        # Pad to 10 digits
        return cik_str.zfill(10)

    def _format_date(self, date_str: str) -> str:
        """Format date string to SEC format (YYYYMMDD)"""
        try:
            if not date_str:
                return ""

            # Handle different date formats
            if '-' in date_str:
                # Already in YYYY-MM-DD format
                return date_str.replace('-', '')
            elif len(date_str) == 8 and date_str.isdigit():
                # Already in YYYYMMDD format
                return date_str
            else:
                # Try to parse and format
                date_obj = pd.to_datetime(date_str)
                return date_obj.strftime('%Y%m%d')

        except:
            return date_str

    def _get_company_tickers(self) -> Dict[str, Any]:
        """Fetch company tickers from SEC API with caching (1 hour cache)"""
        try:
            # Check cache validity (1 hour = 3600 seconds)
            if self._company_tickers_cache is not None and self._cache_timestamp is not None:
                cache_age = time.time() - self._cache_timestamp
                if cache_age < 3600:
                    return self._company_tickers_cache

            # Fetch fresh data from SEC
            url = "https://www.sec.gov/files/company_tickers.json"
            data = self._make_request(url)

            # Transform data structure for easier lookup
            transformed = {}
            cik_to_ticker = {}

            for entry in data.values():
                ticker = entry.get('ticker', '').upper()
                cik = str(entry.get('cik_str', '')).zfill(10)
                title = entry.get('title', '')

                if ticker and cik:
                    transformed[ticker] = {'cik': cik, 'name': title, 'ticker': ticker}
                    cik_to_ticker[cik] = ticker

            # Cache the result
            self._company_tickers_cache = {'by_ticker': transformed, 'by_cik': cik_to_ticker, 'raw': data}
            self._cache_timestamp = time.time()

            return self._company_tickers_cache

        except Exception as e:
            return {'by_ticker': {}, 'by_cik': {}, 'raw': {}, 'error': str(e)}

    # COMPANY FILINGS ENDPOINTS

    def get_company_filings(self, symbol: Optional[str] = None, cik: Optional[Union[str, int]] = None,
                          form_type: Optional[str] = None, start_date: Optional[str] = None,
                          end_date: Optional[str] = None, limit: Optional[int] = 100) -> Dict[str, Any]:
        """Get company filings from SEC database"""
        try:
            if not symbol and not cik:
                return {"error": SECError("company_filings", "Either symbol or CIK must be provided").to_dict()}

            # Get CIK if only symbol is provided
            if symbol and not cik:
                cik_result = self.get_cik_map(symbol)
                if not cik_result.get("success"):
                    return cik_result
                cik = cik_result["data"].get("cik")

            if not cik:
                return {"error": SECError("company_filings", "CIK not found for the provided symbol").to_dict()}

            # Normalize CIK
            normalized_cik = self._normalize_cik(cik)

            # Build URL
            url = f"{self.data_url}/submissions/CIK{normalized_cik}.json"

            # Make request
            data = self._make_request(url)

            if not data or "filings" not in data:
                return {"error": SECError("company_filings", f"No filings found for CIK: {cik}").to_dict()}

            # Extract recent filings
            filings_data = data["filings"]["recent"]

            # Convert to DataFrame for filtering
            filings_df = pd.DataFrame(filings_data)

            # Apply filters
            if form_type:
                form_types = [ft.upper().strip() for ft in form_type.split(',')]
                filings_df = filings_df[filings_df['form'].isin(form_types)]

            if start_date:
                start_formatted = self._format_date(start_date)
                filings_df = filings_df[filings_df['filingDate'] >= start_formatted]

            if end_date:
                end_formatted = self._format_date(end_date)
                filings_df = filings_df[filings_df['filingDate'] <= end_formatted]

            # Sort by filing date (most recent first)
            filings_df = filings_df.sort_values('filingDate', ascending=False)

            # Apply limit
            if limit and limit > 0:
                filings_df = filings_df.head(limit)

            # Format URLs
            base_url = f"{self.base_url}/Archives/edgar/data/{str(int(normalized_cik))}/"

            def format_urls(row):
                accession = row['accessionNumber']
                accession_clean = accession.replace('-', '')
                primary_doc = row['primaryDocument']

                return {
                    'filing_url': f"{base_url}{accession_clean}/{primary_doc}",
                    'complete_submission_url': f"{base_url}{accession}.txt",
                    'filing_detail_url': f"{base_url}{accession}-index.htm"
                }

            url_data = filings_df.apply(format_urls, axis=1)
            url_df = pd.DataFrame(list(url_data))

            # Combine data
            filings_df = pd.concat([filings_df.reset_index(drop=True), url_df], axis=1)

            # Convert to list of dictionaries
            filings_list = filings_df.to_dict('records')

            return {
                "success": True,
                "data": filings_list,
                "parameters": {
                    "symbol": symbol,
                    "cik": str(cik),
                    "form_type": form_type,
                    "start_date": start_date,
                    "end_date": end_date,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": SECError("company_filings", str(e)).to_dict()}

    # CIK MAPPING ENDPOINTS

    def get_cik_map(self, symbol: str) -> Dict[str, Any]:
        """Get CIK (Central Index Key) for a company symbol"""
        try:
            if not symbol:
                return {"error": SECError("cik_map", "Symbol parameter is required").to_dict()}

            symbol_upper = symbol.upper().strip()

            # Fetch company tickers from SEC API
            company_data = self._get_company_tickers()

            if 'error' in company_data:
                return {"success": False, "error": SECError("cik_map", f"Failed to fetch company data: {company_data['error']}").to_dict()}

            # Look up the CIK in SEC data
            by_ticker = company_data.get('by_ticker', {})
            company_info = by_ticker.get(symbol_upper)

            if not company_info:
                return {"success": False, "error": SECError("cik_map", f"CIK not found for symbol: {symbol_upper}").to_dict(), "note": f"Symbol '{symbol_upper}' not found in SEC database. Total companies available: {len(by_ticker)}"}

            return {"success": True, "data": {"cik": company_info['cik'], "symbol": symbol_upper, "name": company_info['name']}, "parameters": {"symbol": symbol}}

        except Exception as e:
            return {"error": SECError("cik_map", str(e)).to_dict()}

    def get_symbol_map(self, cik: Union[str, int]) -> Dict[str, Any]:
        """Get symbol mapping for a CIK"""
        try:
            if not cik:
                return {"error": SECError("symbol_map", "CIK parameter is required").to_dict()}

            normalized_cik = self._normalize_cik(cik)

            # Fetch company tickers from SEC API
            company_data = self._get_company_tickers()

            if 'error' in company_data:
                return {"success": False, "error": SECError("symbol_map", f"Failed to fetch company data: {company_data['error']}").to_dict()}

            # Look up the symbol in SEC data
            by_cik = company_data.get('by_cik', {})
            symbol = by_cik.get(normalized_cik)

            if not symbol:
                return {"success": False, "error": SECError("symbol_map", f"Symbol not found for CIK: {normalized_cik}").to_dict(), "note": f"CIK '{normalized_cik}' not found in SEC database. Total companies available: {len(by_cik)}"}

            # Get company info
            by_ticker = company_data.get('by_ticker', {})
            company_info = by_ticker.get(symbol, {})

            return {"success": True, "data": {"cik": normalized_cik, "symbol": symbol, "name": company_info.get('name', '')}, "parameters": {"cik": str(cik)}}

        except Exception as e:
            return {"error": SECError("symbol_map", str(e)).to_dict()}

    # FILING CONTENT ENDPOINTS

    def get_filing_content(self, filing_url: str) -> Dict[str, Any]:
        """Get content of a specific SEC filing"""
        try:
            if not filing_url:
                return {"error": SECError("filing_content", "Filing URL is required").to_dict()}

            # Validate URL format
            if "/data/" not in filing_url:
                return {"error": SECError("filing_content", "Invalid SEC filing URL format").to_dict()}

            # This would download and parse the filing content
            # For now, return basic filing info

            return {
                "success": True,
                "data": {
                    "url": filing_url,
                    "content_type": "text/html",
                    "note": "Filing content download would be implemented here"
                },
                "parameters": {"filing_url": filing_url}
            }

        except Exception as e:
            return {"error": SECError("filing_content", str(e)).to_dict()}

    def parse_filing_html(self, html_content: str) -> Dict[str, Any]:
        """Parse HTML content from SEC filing"""
        try:
            if not html_content:
                return {"error": SECError("parse_filing_html", "HTML content is required").to_dict()}

            # Import BeautifulSoup for HTML parsing
            try:
                from bs4 import BeautifulSoup
                import re
            except ImportError:
                return {
                    "success": True,
                    "data": {
                        "parsed_content": "BeautifulSoup not available - install with 'pip install beautifulsoup4'",
                        "tables_found": 0,
                        "text_content": "HTML parsing requires additional dependencies"
                    },
                    "parameters": {"content_length": len(html_content)},
                    "note": "Install BeautifulSoup4 for full HTML parsing capabilities"
                }

            soup = BeautifulSoup(html_content, 'html.parser')

            # Extract text content
            text_content = soup.get_text(separator=' ', strip=True)
            # Clean up whitespace
            text_content = re.sub(r'\s+', ' ', text_content).strip()

            # Find all tables
            tables = soup.find_all('table')
            table_data = []

            for i, table in enumerate(tables):
                rows = table.find_all('tr')
                table_rows = []

                for row in rows:
                    cells = row.find_all(['td', 'th'])
                    row_data = [cell.get_text(strip=True) for cell in cells]
                    if row_data:  # Only add non-empty rows
                        table_rows.append(row_data)

                if table_rows:
                    table_data.append({
                        "table_index": i,
                        "headers": table_rows[0] if table_rows else [],
                        "data": table_rows[1:] if len(table_rows) > 1 else [],
                        "row_count": len(table_rows),
                        "column_count": len(table_rows[0]) if table_rows else 0
                    })

            # Extract key information patterns
            extracted_info = {
                "company_name": self._extract_company_name(text_content),
                "filing_date": self._extract_filing_date(text_content),
                "period_end_date": self._extract_period_end_date(text_content),
                "form_type": self._extract_form_type(text_content),
                "cik": self._extract_cik(text_content)
            }

            # Look for financial data patterns
            financial_keywords = ['revenue', 'net income', 'earnings', 'cash flow', 'assets', 'liabilities', 'equity']
            financial_data_found = []
            for keyword in financial_keywords:
                if keyword.lower() in text_content.lower():
                    financial_data_found.append(keyword)

            return {
                "success": True,
                "data": {
                    "parsed_content": "HTML content successfully parsed",
                    "text_content": text_content[:1000] + "..." if len(text_content) > 1000 else text_content,
                    "text_length": len(text_content),
                    "tables_found": len(tables),
                    "table_data": table_data,
                    "extracted_info": extracted_info,
                    "financial_keywords_found": financial_data_found,
                    "html_elements": {
                        "title": soup.title.get_text(strip=True) if soup.title else None,
                        "links": len(soup.find_all('a')),
                        "images": len(soup.find_all('img')),
                        "divs": len(soup.find_all('div'))
                    }
                },
                "parameters": {
                    "content_length": len(html_content),
                    "parsing_method": "BeautifulSoup"
                }
            }

        except Exception as e:
            return {"error": SECError("parse_filing_html", str(e)).to_dict()}

    def _extract_company_name(self, text_content: str) -> str:
        """Extract company name from text content"""
        patterns = [
            r'COMPANY CONFORMED NAME:\s*([^\n]+)',
            r'Company Name:\s*([^\n]+)',
            r'NAME OF ISSUER:\s*([^\n]+)'
        ]
        for pattern in patterns:
            match = re.search(pattern, text_content, re.IGNORECASE)
            if match:
                return match.group(1).strip()
        return ""

    def _extract_filing_date(self, text_content: str) -> str:
        """Extract filing date from text content"""
        patterns = [
            r'FILED AS OF DATE:\s*(\d{8})',
            r'Filed:\s*(\d{4}-\d{2}-\d{2})',
            r'FILING DATE:\s*(\d{8})'
        ]
        for pattern in patterns:
            match = re.search(pattern, text_content, re.IGNORECASE)
            if match:
                date_str = match.group(1)
                # Format date consistently
                if '-' in date_str:
                    return date_str
                elif len(date_str) == 8:
                    return f"{date_str[:4]}-{date_str[4:6]}-{date_str[6:8]}"
        return ""

    def _extract_period_end_date(self, text_content: str) -> str:
        """Extract period end date from text content"""
        patterns = [
            r'PERIOD OF REPORT:\s*(\d{8})',
            r'For the period ended\s*(\d{4}-\d{2}-\d{2})',
            r'PERIOD END DATE:\s*(\d{8})'
        ]
        for pattern in patterns:
            match = re.search(pattern, text_content, re.IGNORECASE)
            if match:
                date_str = match.group(1)
                if '-' in date_str:
                    return date_str
                elif len(date_str) == 8:
                    return f"{date_str[:4]}-{date_str[4:6]}-{date_str[6:8]}"
        return ""

    def _extract_form_type(self, text_content: str) -> str:
        """Extract form type from text content"""
        patterns = [
            r'FORM TYPE:\s*([^\n]+)',
            r'Form\s+([0-9]+[A-Z\-]*)',
            r'CONFORMED SUBMISSION TYPE:\s*([^\n]+)'
        ]
        for pattern in patterns:
            match = re.search(pattern, text_content, re.IGNORECASE)
            if match:
                return match.group(1).strip()
        return ""

    def _extract_cik(self, text_content: str) -> str:
        """Extract CIK from text content"""
        patterns = [
            r'CENTRAL INDEX KEY:\s*(\d{10})',
            r'CIK:\s*(\d{10})',
            r'SEC FILE NUMBER:\s*(\d{10})'
        ]
        for pattern in patterns:
            match = re.search(pattern, text_content, re.IGNORECASE)
            if match:
                return match.group(1)
        return ""

    # INSIDER TRADING ENDPOINTS

    def get_insider_trading(self, symbol: Optional[str] = None, cik: Optional[Union[str, int]] = None,
                          start_date: Optional[str] = None, end_date: Optional[str] = None,
                          limit: Optional[int] = 100) -> Dict[str, Any]:
        """Get insider trading data (Form 4 filings)"""
        try:
            if not symbol and not cik:
                return {"error": SECError("insider_trading", "Either symbol or CIK must be provided").to_dict()}

            # Use Form 4 filings for insider trading
            filings_result = self.get_company_filings(
                symbol=symbol,
                cik=cik,
                form_type="4",
                start_date=start_date,
                end_date=end_date,
                limit=limit
            )

            if not filings_result.get("success"):
                return filings_result

            # Process Form 4 filings to extract insider trading data
            filings_data = filings_result["data"]
            insider_trades = []

            for filing in filings_data:
                # This would parse Form 4 content to extract insider trading details
                # For now, return basic filing info
                insider_trade = {
                    "filing_date": filing.get("filingDate"),
                    "accession_number": filing.get("accessionNumber"),
                    "company_name": "Extracted from filing",
                    "insider_name": "Extracted from filing",
                    "transaction_type": "P",
                    "securities_transacted": 0,
                    "price": 0,
                    "amount": 0,
                    "ownership_type": "Direct",
                    "note": "Form 4 parsing would be implemented here"
                }
                insider_trades.append(insider_trade)

            return {
                "success": True,
                "data": insider_trades,
                "parameters": {
                    "symbol": symbol,
                    "cik": str(cik) if cik else None,
                    "start_date": start_date,
                    "end_date": end_date,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": SECError("insider_trading", str(e)).to_dict()}

    # INSTITUTIONAL OWNERSHIP ENDPOINTS

    def get_institutional_ownership(self, symbol: Optional[str] = None, cik: Optional[Union[str, int]] = None,
                                  start_date: Optional[str] = None, end_date: Optional[str] = None,
                                  limit: Optional[int] = 50) -> Dict[str, Any]:
        """Get institutional ownership data (Form 13F filings)"""
        try:
            if not symbol and not cik:
                return {"error": SECError("institutional_ownership", "Either symbol or CIK must be provided").to_dict()}

            # Use Form 13F-HR filings for institutional ownership
            filings_result = self.get_company_filings(
                symbol=symbol,
                cik=cik,
                form_type="13F-HR",
                start_date=start_date,
                end_date=end_date,
                limit=limit
            )

            if not filings_result.get("success"):
                return filings_result

            # Process Form 13F filings to extract institutional ownership data
            filings_data = filings_result["data"]
            institutional_holdings = []

            for filing in filings_data:
                # This would parse Form 13F content to extract institutional holdings
                # For now, return basic filing info
                holding = {
                    "filing_date": filing.get("filingDate", ""),
                    "accession_number": filing.get("accessionNumber", ""),
                    "institution_name": "Extracted from filing",
                    "cusip": "Extracted from filing",
                    "security_name": "Extracted from filing",
                    "shares": 0,
                    "market_value": 0,
                    "note": "Form 13F parsing would be implemented here"
                }
                institutional_holdings.append(holding)

            return {
                "success": True,
                "data": institutional_holdings,
                "parameters": {
                    "symbol": symbol,
                    "cik": str(cik) if cik else None,
                    "start_date": start_date,
                    "end_date": end_date,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": SECError("institutional_ownership", str(e)).to_dict()}

    # COMPANY SEARCH ENDPOINTS

    def search_companies(self, query: str, is_fund: bool = False) -> Dict[str, Any]:
        """Search for companies in SEC database"""
        try:
            if not query:
                return {"error": SECError("search_companies", "Search query is required").to_dict()}

            query_upper = query.upper().strip()

            # Fetch company tickers from SEC API
            company_data = self._get_company_tickers()

            if 'error' in company_data:
                return {"success": False, "error": SECError("search_companies", f"Failed to fetch company data: {company_data['error']}").to_dict()}

            by_ticker = company_data.get('by_ticker', {})
            raw_data = company_data.get('raw', {})

            # Search for exact matches first
            results = []

            # Exact symbol match
            if query_upper in by_ticker:
                company_info = by_ticker[query_upper]
                results.append({
                    "cik": company_info['cik'],
                    "name": company_info['name'],
                    "symbol": query_upper,
                    "match_type": "exact_symbol"
                })

            # Partial symbol matches
            for symbol, data in by_ticker.items():
                if query_upper in symbol and symbol != query_upper:
                    results.append({
                        "cik": data['cik'],
                        "name": data['name'],
                        "symbol": symbol,
                        "match_type": "partial_symbol"
                    })

            # Name matches (case-insensitive)
            for symbol, data in by_ticker.items():
                if query_upper in data['name'].upper() and not any(r['cik'] == data['cik'] for r in results):
                    results.append({
                        "cik": data['cik'],
                        "name": data['name'],
                        "symbol": symbol,
                        "match_type": "name_match"
                    })

            # Remove duplicates and sort by relevance
            unique_results = []
            seen_ciks = set()

            # Prioritize exact matches, then partial symbols, then name matches
            priority_order = {"exact_symbol": 0, "partial_symbol": 1, "name_match": 2}
            results.sort(key=lambda x: priority_order.get(x.get("match_type", "name_match"), 3))

            for result in results:
                if result["cik"] not in seen_ciks:
                    unique_results.append(result)
                    seen_ciks.add(result["cik"])
                    if len(unique_results) >= 20:  # Limit to 20 results
                        break

            # If no results found, provide helpful message
            if not unique_results:
                return {
                    "success": True,
                    "data": [],
                    "parameters": {
                        "query": query,
                        "is_fund": is_fund
                    },
                    "note": f"No companies found matching '{query}' in SEC database of {len(by_ticker)} companies."
                }

            return {
                "success": True,
                "data": unique_results,
                "parameters": {
                    "query": query,
                    "is_fund": is_fund
                },
                "total_results": len(unique_results),
                "database_coverage": len(by_ticker)
            }

        except Exception as e:
            return {"error": SECError("search_companies", str(e)).to_dict()}

    def search_etfs_mutual_funds(self, query: str) -> Dict[str, Any]:
        """Search for ETFs and mutual funds"""
        try:
            if not query:
                return {"error": SECError("search_etfs_mutual_funds", "Search query is required").to_dict()}

            # Use search_companies with is_fund=True
            return self.search_companies(query, is_fund=True)

        except Exception as e:
            return {"error": SECError("search_etfs_mutual_funds", str(e)).to_dict()}

    # FORM TYPE HELPERS

    def get_available_form_types(self) -> Dict[str, Any]:
        """Get list of available SEC form types"""
        try:
            form_info = {}

            # Common form types with descriptions
            form_descriptions = {
                "10-K": "Annual report providing comprehensive overview of company's business and financial condition",
                "10-Q": "Quarterly report covering financial performance and operations",
                "8-K": "Current report disclosing material events that shareholders should know about",
                "10-D": "Annual report for registered investment companies (mutual funds)",
                "20-F": "Annual report for foreign private issuers",
                "40-F": "Annual report for foreign private issuers (Canadian)",
                "6-K": "Current report for foreign private issuers",
                "S-1": "Registration statement for new securities offerings",
                "S-3": "Registration statement for established companies",
                "S-4": "Registration statement for mergers and acquisitions",
                "F-1": "Registration statement for foreign private issuers",
                "424A": "Prospectus filed under Rule 424(a)",
                "SC 13D": "Beneficial ownership report (5%+ ownership)",
                "SC 13G": "Beneficial ownership report (passive investor)",
                "13F-HR": "Institutional investment manager holdings report",
                "3": "Initial statement of beneficial ownership",
                "4": "Statement of changes in beneficial ownership (insider trading)",
                "144": "Notice of proposed sale of securities"
            }

            for form_type in self.common_form_types:
                form_info[form_type] = {
                    "description": form_descriptions.get(form_type, "SEC filing form"),
                    "frequency": "Annual" if form_type in ["10-K", "10-D", "20-F", "40-F"] else
                               "Quarterly" if form_type in ["10-Q"] else "As needed"
                }

            return {
                "success": True,
                "data": form_info,
                "parameters": {}
            }

        except Exception as e:
            return {"error": SECError("available_form_types", str(e)).to_dict()}

    # COMPANION DATA ENDPOINTS

    def get_company_facts(self, cik: Union[str, int]) -> Dict[str, Any]:
        """Get company facts data from SEC API"""
        try:
            if not cik:
                return {"error": SECError("company_facts", "CIK parameter is required").to_dict()}

            normalized_cik = self._normalize_cik(cik)
            url = f"{self.data_url}/api/xbrl/companyfacts/CIK{normalized_cik}.json"

            data = self._make_request(url)

            if not data or "facts" not in data:
                return {"error": SECError("company_facts", f"No facts found for CIK: {cik}").to_dict()}

            # Extract key financial facts with proper parsing
            facts_data = data.get("facts", {})
            us_gaap_facts = facts_data.get("us-gaap", {})
            dei_facts = facts_data.get("dei", {})

            # Key financial metrics we want to extract
            key_metrics = {
                "RevenueFromContractWithCustomerExcludingAssessedTax": "Revenue",
                "NetIncomeLoss": "Net Income",
                "Assets": "Total Assets",
                "Liabilities": "Total Liabilities",
                "StockholdersEquity": "Shareholders' Equity",
                "CashAndCashEquivalentsAtCarryingValue": "Cash & Cash Equivalents",
                "AccountsReceivableNetCurrent": "Accounts Receivable",
                "InventoryNet": "Inventory",
                "PropertyPlantAndEquipmentNet": "PP&E (Net)",
                "Goodwill": "Goodwill",
                "LongTermDebt": "Long-Term Debt",
                "OperatingIncomeLoss": "Operating Income",
                "EarningsPerShareBasic": "EPS (Basic)",
                "EarningsPerShareDiluted": "EPS (Diluted)",
                "CostOfGoodsAndServicesSold": "Cost of Revenue",
                "SellingGeneralAndAdministrativeExpense": "SG&A Expense",
                "ResearchAndDevelopmentExpense": "R&D Expense",
                "InterestExpenseDebt": "Interest Expense",
                "IncomeTaxExpenseBenefit": "Income Tax Expense"
            }

            key_facts = {}

            # Extract most recent values for key metrics
            for fact_name, display_name in key_metrics.items():
                if fact_name in us_gaap_facts:
                    fact_data = us_gaap_facts[fact_name]
                    units = list(fact_data.keys())
                    if units:
                        unit_key = units[0]  # Usually USD or shares
                        values = fact_data[unit_key]
                        if values and isinstance(values, list):
                            # Find most recent value by end date
                            recent_value = max(
                                values,
                                key=lambda x: x.get("end", "") if isinstance(x, dict) and x.get("end") else ""
                            )
                            if isinstance(recent_value, dict):
                                key_facts[display_name] = {
                                    "value": recent_value.get("val"),
                                    "end_date": recent_value.get("end"),
                                    "unit": unit_key,
                                    "frame": recent_value.get("frame"),
                                    "fy": recent_value.get("fy"),
                                    "fp": recent_value.get("fp"),
                                    "formatted_value": f"{unit_key.upper()} {recent_value.get('val', 0):,.0f}" if recent_value.get('val') and 'USD' in unit_key else f"{recent_value.get('val', 0)}"
                                }

            # Get company info from DEI facts
            company_info = {}
            if "EntityCommonStockSharesOutstanding" in dei_facts:
                shares_data = dei_facts["EntityCommonStockSharesOutstanding"]
                if shares_data and list(shares_data.keys()):
                    values = shares_data[list(shares_data.keys())[0]]
                    if values and isinstance(values, list):
                        recent_shares = max(
                            values,
                            key=lambda x: x.get("end", "") if isinstance(x, dict) and x.get("end") else ""
                        )
                        if isinstance(recent_shares, dict):
                            company_info["shares_outstanding"] = recent_shares.get("val")

            return {
                "success": True,
                "data": {
                    "entity_name": data.get("entityName"),
                    "cik": normalized_cik,
                    "company_info": company_info,
                    "financial_metrics": key_facts,
                    "taxonomy_coverage": {
                        "us-gaap": len(us_gaap_facts),
                        "dei": len(dei_facts),
                        "total_metrics": len(key_facts)
                    },
                    "last_updated": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                },
                "parameters": {"cik": str(cik)}
            }

        except Exception as e:
            return {"error": SECError("company_facts", str(e)).to_dict()}

    def get_financial_statements(self, cik: Union[str, int], taxonomy: str = "us-gaap",
                                fact_list: Optional[List[str]] = None) -> Dict[str, Any]:
        """Get financial statements data using company facts"""
        try:
            facts_result = self.get_company_facts(cik)

            if not facts_result.get("success"):
                return facts_result

            facts_data = facts_result["data"]["facts"]

            # Filter by requested facts
            if fact_list:
                filtered_facts = {k: v for k, v in facts_data.items() if k in fact_list}
            else:
                filtered_facts = facts_data

            return {
                "success": True,
                "data": filtered_facts,
                "parameters": {
                    "cik": str(cik),
                    "taxonomy": taxonomy,
                    "fact_list": fact_list
                }
            }

        except Exception as e:
            return {"error": SECError("financial_statements", str(e)).to_dict()}

    # COMPREHENSIVE DATA ENDPOINTS

    def get_company_overview(self, symbol: Optional[str] = None, cik: Optional[Union[str, int]] = None) -> Dict[str, Any]:
        """Get comprehensive company overview including filings, facts, and insider data"""
        try:
            if not symbol and not cik:
                return {"error": SECError("company_overview", "Either symbol or CIK must be provided").to_dict()}

            # Get CIK if only symbol is provided
            if symbol and not cik:
                cik_result = self.get_cik_map(symbol)
                if cik_result.get("success"):
                    cik = cik_result["data"].get("cik")

            if not cik:
                return {"error": SECError("company_overview", "CIK not found for the provided symbol").to_dict()}

            results = {}

            # 1. Get recent filings
            filings_result = self.get_company_filings(
                cik=cik,
                form_type="10-K,10-Q,8-K",
                limit=10
            )
            results["recent_filings"] = filings_result

            # 2. Get company facts
            facts_result = self.get_company_facts(cik)
            results["company_facts"] = facts_result

            # 3. Get insider trading
            insider_result = self.get_insider_trading(
                cik=cik,
                limit=10
            )
            results["insider_trading"] = insider_result

            # Check if we have any successful data
            has_data = any(
                result.get("success") and result.get("data")
                for result in results.values()
            )

            if not has_data:
                return {"error": SECError("company_overview", f"No data found for CIK: {cik}").to_dict()}

            return {
                "success": True,
                "data": results,
                "parameters": {
                    "symbol": symbol,
                    "cik": str(cik)
                }
            }

        except Exception as e:
            return {"error": SECError("company_overview", str(e)).to_dict()}

    def get_filings_by_form_type(self, form_type: str, start_date: Optional[str] = None,
                                end_date: Optional[str] = None, limit: Optional[int] = 100) -> Dict[str, Any]:
        """Get all filings of a specific form type within date range"""
        try:
            # This would use SEC's full-text search or browse API
            # For now, return a simplified implementation

            return {
                "success": True,
                "data": [],
                "parameters": {
                    "form_type": form_type,
                    "start_date": start_date,
                    "end_date": end_date,
                    "limit": limit
                },
                "note": "This endpoint requires SEC's advanced search API for full implementation."
            }

        except Exception as e:
            return {"error": SECError("filings_by_form_type", str(e)).to_dict()}


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python sec_data.py <command> [args...]",
            "commands": [
                "company_filings [symbol] [cik] [form_type] [start_date] [end_date] [limit]",
                "cik_map [symbol]",
                "symbol_map [cik]",
                "filing_content [filing_url]",
                "parse_filing_html [html_content]",
                "insider_trading [symbol] [cik] [start_date] [end_date] [limit]",
                "institutional_ownership [symbol] [cik] [start_date] [end_date] [limit]",
                "search_companies [query] [is_fund]",
                "search_etfs_mutual_funds [query]",
                "available_form_types",
                "company_facts [cik]",
                "financial_statements [cik] [taxonomy] [fact_list]",
                "company_overview [symbol] [cik]",
                "filings_by_form_type [form_type] [start_date] [end_date] [limit]"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = SECDataWrapper()

    try:
        if command == "company_filings":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            cik = sys.argv[3] if len(sys.argv) > 3 else None
            form_type = sys.argv[4] if len(sys.argv) > 4 else None
            start_date = sys.argv[5] if len(sys.argv) > 5 else None
            end_date = sys.argv[6] if len(sys.argv) > 6 else None
            limit = int(sys.argv[7]) if len(sys.argv) > 7 else 100
            result = wrapper.get_company_filings(symbol, cik, form_type, start_date, end_date, limit)

        elif command == "cik_map":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_cik_map(symbol)

        elif command == "symbol_map":
            cik = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_symbol_map(cik)

        elif command == "filing_content":
            filing_url = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_filing_content(filing_url)

        elif command == "parse_filing_html":
            # Note: This would require proper HTML content as input
            html_content = sys.argv[2] if len(sys.argv) > 2 else ""
            result = wrapper.parse_filing_html(html_content)

        elif command == "insider_trading":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            cik = sys.argv[3] if len(sys.argv) > 3 else None
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None
            limit = int(sys.argv[6]) if len(sys.argv) > 6 else 100
            result = wrapper.get_insider_trading(symbol, cik, start_date, end_date, limit)

        elif command == "institutional_ownership":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            cik = sys.argv[3] if len(sys.argv) > 3 else None
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None
            limit = int(sys.argv[6]) if len(sys.argv) > 6 else 50
            result = wrapper.get_institutional_ownership(symbol, cik, start_date, end_date, limit)

        elif command == "search_companies":
            query = sys.argv[2] if len(sys.argv) > 2 else None
            is_fund = sys.argv[3].lower() == "true" if len(sys.argv) > 3 else False
            result = wrapper.search_companies(query, is_fund)

        elif command == "search_etfs_mutual_funds":
            query = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.search_etfs_mutual_funds(query)

        elif command == "available_form_types":
            result = wrapper.get_available_form_types()

        elif command == "company_facts":
            cik = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_company_facts(cik)

        elif command == "financial_statements":
            cik = sys.argv[2] if len(sys.argv) > 2 else None
            taxonomy = sys.argv[3] if len(sys.argv) > 3 else "us-gaap"
            fact_list = sys.argv[4].split(',') if len(sys.argv) > 4 else None
            result = wrapper.get_financial_statements(cik, taxonomy, fact_list)

        elif command == "company_overview":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            cik = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_company_overview(symbol, cik)

        elif command == "filings_by_form_type":
            form_type = sys.argv[2] if len(sys.argv) > 2 else None
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None
            limit = int(sys.argv[5]) if len(sys.argv) > 5 else 100
            result = wrapper.get_filings_by_form_type(form_type, start_date, end_date, limit)

        else:
            result = {"error": SECError(command, f"Unknown command: {command}").to_dict()}

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({"error": SECError(command, str(e)).to_dict()}, indent=2))


if __name__ == "__main__":
    main()