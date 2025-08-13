# SEC Data Wrapper - First 5 Endpoints (Updated with Working Headers)
# File: sec_wrapper.py

import asyncio
import json
import os
from typing import Dict, List, Optional, Union
from datetime import date as dateType, datetime, timedelta
import aiohttp

try:
    from aiohttp_client_cache import SQLiteBackend, CachedSession

    CACHE_AVAILABLE = True
except ImportError:
    CACHE_AVAILABLE = False
    print("Warning: aiohttp-client-cache not installed. Running without cache.")

import pandas as pd
import dearpygui.dearpygui as dpg

# Copy these utility files to your project folder:
# - definitions.py (contains HEADERS, SEC_HEADERS, FORM_LIST, etc.)
# - helpers.py (contains symbol_map, get_all_companies, etc.)
from definitions import HEADERS, SEC_HEADERS, FORM_LIST
from helpers import symbol_map, get_all_companies, download_zip_file, get_ftd_urls, get_mf_and_etf_map


class SECDataAPI:
    """SEC Data API Wrapper for first 5 endpoints"""

    def __init__(self):
        self.cache_dir = "./cache"
        os.makedirs(self.cache_dir, exist_ok=True)

        # Working headers based on your successful test
        self.working_headers = {
            "User-Agent": "my real company name definitelynot@fakecompany.com",
            "Accept-Encoding": "gzip, deflate",
        }

    async def _make_request(self, url: str, headers: dict = None, use_cache: bool = True) -> Union[dict, str]:
        """Make HTTP request with optional caching"""
        if headers is None:
            headers = self.working_headers  # Use the working headers instead of SEC_HEADERS

        print(f"Making request to: {url}")  # Debug log

        try:
            if use_cache and CACHE_AVAILABLE:
                async with CachedSession(
                        cache=SQLiteBackend(f"{self.cache_dir}/http_cache", expire_after=3600 * 24)
                ) as session:
                    try:
                        async with session.get(url, headers=headers) as response:
                            print(f"Response status: {response.status}")  # Debug log
                            response.raise_for_status()

                            content_type = response.headers.get("Content-Type", "")
                            print(f"Content-Type: {content_type}")  # Debug log

                            if "application/json" in content_type:
                                result = await response.json()
                                print(
                                    f"JSON Response keys: {list(result.keys()) if isinstance(result, dict) else 'Not a dict'}")  # Debug log
                                print(f"JSON Response sample: {str(result)[:200]}...")  # Debug log first 200 chars
                                return result
                            else:
                                text_result = await response.text()
                                print(f"Text Response length: {len(text_result)}")  # Debug log
                                return text_result
                    finally:
                        await session.close()
            else:
                async with aiohttp.ClientSession() as session:
                    async with session.get(url, headers=headers) as response:
                        print(f"Response status: {response.status}")  # Debug log
                        response.raise_for_status()

                        content_type = response.headers.get("Content-Type", "")
                        print(f"Content-Type: {content_type}")  # Debug log

                        if "application/json" in content_type:
                            result = await response.json()
                            print(
                                f"JSON Response keys: {list(result.keys()) if isinstance(result, dict) else 'Not a dict'}")  # Debug log
                            print(f"JSON Response sample: {str(result)[:200]}...")  # Debug log first 200 chars
                            return result
                        else:
                            text_result = await response.text()
                            print(f"Text Response length: {len(text_result)}")  # Debug log
                            return text_result
        except aiohttp.ClientResponseError as e:
            print(f"HTTP Error {e.status}: {e.message} for URL: {url}")
            raise Exception(f"HTTP {e.status}: {e.message}")
        except Exception as e:
            print(f"Request failed for {url}: {str(e)}")
            raise

    # 1. CIK Map - Convert Symbol to CIK
    async def get_cik_map(self, symbol: str, use_cache: bool = True) -> dict:
        """Convert symbol to CIK number"""
        try:
            print(f"Looking up CIK for symbol: {symbol}")  # Debug log
            cik = await symbol_map(symbol.upper(), use_cache)
            print(f"Found CIK: {cik}")  # Debug log
            return {
                "symbol": symbol.upper(),
                "cik": cik if cik else "Not Found"
            }
        except Exception as e:
            print(f"Error in get_cik_map: {str(e)}")  # Debug log
            return {"error": str(e)}

    # 2. Company Filings - Get SEC Filings
    async def get_company_filings(self, symbol: str = None, cik: str = None,
                                  form_type: str = None, limit: int = 100,
                                  use_cache: bool = True) -> dict:
        """Get company SEC filings"""
        try:
            print(f"Getting filings for symbol: {symbol}, cik: {cik}")  # Debug log

            if symbol and not cik:
                cik = await symbol_map(symbol.upper(), use_cache)
                print(f"Mapped symbol {symbol} to CIK: {cik}")  # Debug log
                if not cik:
                    return {"error": f"CIK not found for symbol {symbol}"}

            if not cik:
                return {"error": "CIK or symbol must be provided"}

            # SEC submissions API requires zero-padded CIK (10 digits)
            cik_clean = str(cik).lstrip('0')
            cik_padded = cik_clean.zfill(10)
            print(f"CIK clean: {cik_clean}, CIK padded: {cik_padded}")  # Debug log

            url = f"https://data.sec.gov/submissions/CIK{cik_padded}.json"
            response = await self._make_request(url, use_cache=use_cache)

            if isinstance(response, dict) and "filings" in response:
                filings = pd.DataFrame(response["filings"]["recent"])
                print(f"Found {len(filings)} total filings")  # Debug log

                # Filter by form type if specified
                if form_type:
                    form_types = form_type.replace("_", " ").split(",")
                    filings = filings[
                        filings['form'].str.contains("|".join(form_types), case=False, na=False)
                    ]
                    print(f"After form type filter: {len(filings)} filings")  # Debug log

                # Limit results
                if limit:
                    filings = filings.head(limit)

                # Add URLs - use the clean CIK for the URL path
                base_url = f"https://www.sec.gov/Archives/edgar/data/{cik_clean}/"
                filings['report_url'] = (
                        base_url +
                        filings['accessionNumber'].str.replace("-", "") +
                        "/" + filings['primaryDocument']
                )

                return {
                    "company_name": response.get("name", ""),
                    "cik": cik_padded,
                    "filings": filings.to_dict("records")
                }

            print(
                f"Response structure: {type(response)}, keys: {response.keys() if isinstance(response, dict) else 'Not dict'}")  # Debug log
            return {"error": "No filings data found"}

        except Exception as e:
            print(f"Error in get_company_filings: {str(e)}")  # Debug log
            return {"error": str(e)}

    # 3. Compare Company Facts - XBRL Facts Comparison
    async def get_compare_company_facts(self, symbol: str = None, fact: str = "Revenues",
                                        year: int = None, use_cache: bool = True) -> dict:
        """Get company facts for comparison"""
        try:
            print(f"Getting facts for symbol: {symbol}, fact: {fact}, year: {year}")  # Debug log

            if not symbol:
                # Get frame data (all companies for a fact)
                current_year = datetime.now().year if not year else year
                quarter = (datetime.now().month - 1) // 3 + 1

                # Try different URL patterns for frames
                urls_to_try = [
                    f"https://data.sec.gov/api/xbrl/frames/us-gaap/{fact}/USD/CY{current_year}Q{quarter}.json",
                    f"https://data.sec.gov/api/xbrl/frames/us-gaap/{fact}/USD/CY{current_year}.json",
                    f"https://data.sec.gov/api/xbrl/frames/us-gaap/{fact}/USD/CY{current_year}Q{quarter}I.json"
                ]

                response = None
                for i, url in enumerate(urls_to_try):
                    try:
                        print(f"Trying URL {i + 1}/3: {url}")  # Debug log
                        response = await self._make_request(url, use_cache=use_cache)
                        if isinstance(response, dict) and "data" in response:
                            print(f"Success with URL {i + 1}")  # Debug log
                            break
                    except Exception as e:
                        print(f"Failed URL {i + 1}: {str(e)}")  # Debug log
                        continue

                if response and isinstance(response, dict) and "data" in response:
                    # Get company mappings
                    companies = await get_all_companies(use_cache)
                    cik_to_symbol = companies.set_index("cik")["symbol"].to_dict()

                    data = response["data"]
                    for item in data:
                        item["symbol"] = cik_to_symbol.get(str(int(item["cik"])), "")
                        item["fact"] = fact

                    print(f"Found {len(data)} companies with {fact} data")  # Debug log

                    return {
                        "metadata": {
                            "fact": fact,
                            "year": current_year,
                            "quarter": quarter,
                            "label": response.get("label", ""),
                            "count": len(data)
                        },
                        "data": sorted(data, key=lambda x: x.get("val", 0), reverse=True)[:50]
                    }
            else:
                # Get company concept data
                cik = await symbol_map(symbol.upper(), use_cache)
                print(f"Mapped {symbol} to CIK: {cik}")  # Debug log

                if not cik:
                    return {"error": f"CIK not found for symbol {symbol}"}

                # SEC company concept API requires zero-padded CIK (10 digits)
                cik_clean = str(cik).lstrip('0')
                cik_padded = cik_clean.zfill(10)
                print(f"Using CIK: {cik_padded}")  # Debug log

                url = f"https://data.sec.gov/api/xbrl/companyconcept/CIK{cik_padded}/us-gaap/{fact}.json"
                response = await self._make_request(url, use_cache=use_cache)

                if isinstance(response, dict) and "units" in response:
                    units = response["units"]
                    all_data = []

                    print(f"Found units: {list(units.keys())}")  # Debug log

                    for unit, values in units.items():
                        for item in values:
                            item.update({
                                "unit": unit,
                                "symbol": symbol.upper(),
                                "cik": cik_padded,
                                "fact": response.get("label", fact)
                            })
                            all_data.append(item)

                    # Filter by year if specified
                    if year:
                        all_data = [d for d in all_data if str(d.get("fy")) == str(year)]
                        print(f"After year filter: {len(all_data)} records")  # Debug log

                    return {
                        "metadata": {
                            "company": response.get("entityName", ""),
                            "cik": cik_padded,
                            "symbol": symbol.upper(),
                            "fact": response.get("label", fact)
                        },
                        "data": sorted(all_data, key=lambda x: x.get("filed", ""), reverse=True)
                    }
                else:
                    print(
                        f"Response keys: {response.keys() if isinstance(response, dict) else 'Not dict'}")  # Debug log

            return {"error": "No data found"}

        except Exception as e:
            print(f"Error in get_compare_company_facts: {str(e)}")  # Debug log
            return {"error": str(e)}

    # 4. Equity FTD - Fails to Deliver Data
    async def get_equity_ftd(self, symbol: str, limit: int = 24, use_cache: bool = True) -> dict:
        """Get fails-to-deliver data for a symbol"""
        try:
            # Get FTD URLs
            urls_data = await get_ftd_urls()
            urls = list(urls_data.values())

            if limit > 0:
                urls = urls[:limit]

            results = []
            for url in urls:
                data = await download_zip_file(url, symbol.upper(), use_cache)
                results.extend(data)

            if not results:
                return {"error": f"No FTD data found for {symbol}"}

            # Sort by date
            results = sorted(results, key=lambda d: d.get("date", ""), reverse=True)

            return {
                "symbol": symbol.upper(),
                "count": len(results),
                "data": results
            }

        except Exception as e:
            return {"error": str(e)}

    # 5. Equity Search - Search for Companies
    async def get_equity_search(self, query: str, is_fund: bool = False, use_cache: bool = True) -> dict:
        """Search for companies by name/symbol"""
        try:
            if is_fund:
                companies = await get_mf_and_etf_map(use_cache)
                results = companies[
                    companies["cik"].str.contains(query, case=False, na=False) |
                    companies["seriesId"].str.contains(query, case=False, na=False) |
                    companies["classId"].str.contains(query, case=False, na=False) |
                    companies["symbol"].str.contains(query, case=False, na=False)
                    ]
            else:
                companies = await get_all_companies(use_cache)
                results = companies[
                    companies["name"].str.contains(query, case=False, na=False) |
                    companies["symbol"].str.contains(query, case=False, na=False) |
                    companies["cik"].str.contains(query, case=False, na=False)
                    ]

            return {
                "query": query,
                "is_fund": is_fund,
                "count": len(results),
                "data": results.to_dict("records")
            }

        except Exception as e:
            return {"error": str(e)}


class SECDataUI:
    """DearPyGUI Interface for SEC Data"""

    def __init__(self):
        self.api = SECDataAPI()
        self.current_data = None

    def setup_ui(self):
        """Setup the main UI"""
        dpg.create_context()

        with dpg.window(label="SEC Data Terminal", tag="main_window"):
            # Add status indicator
            dpg.add_text("🟢 Using Working SEC Headers", color=(0, 255, 0))
            dpg.add_separator()

            # Tabs for different endpoints
            with dpg.tab_bar():
                # Tab 1: CIK Map
                with dpg.tab(label="CIK Map"):
                    dpg.add_text("Convert Symbol to CIK")
                    dpg.add_input_text(label="Symbol", tag="cik_symbol", default_value="TSLA")
                    dpg.add_button(label="Get CIK", callback=self.get_cik_callback)
                    dpg.add_text("", tag="cik_result")

                # Tab 2: Company Filings
                with dpg.tab(label="Company Filings"):
                    dpg.add_text("Get SEC Filings for Company")
                    dpg.add_input_text(label="Symbol", tag="filing_symbol", default_value="TSLA")
                    dpg.add_input_text(label="Form Type (optional)", tag="filing_form", default_value="10-K,10-Q")
                    dpg.add_input_int(label="Limit", tag="filing_limit", default_value=10)
                    dpg.add_button(label="Get Filings", callback=self.get_filings_callback)
                    with dpg.child_window(height=300, tag="filings_table"):
                        dpg.add_text("Filings will appear here...")

                # Tab 3: Company Facts
                with dpg.tab(label="Company Facts"):
                    dpg.add_text("Compare Company Facts")
                    dpg.add_input_text(label="Symbol (optional)", tag="facts_symbol", default_value="TSLA")
                    dpg.add_input_text(label="Fact", tag="facts_fact", default_value="Revenues")
                    dpg.add_input_int(label="Year (optional)", tag="facts_year", default_value=0)
                    dpg.add_button(label="Get Facts", callback=self.get_facts_callback)
                    with dpg.child_window(height=300, tag="facts_table"):
                        dpg.add_text("Facts will appear here...")

                # Tab 4: FTD Data
                with dpg.tab(label="Fails to Deliver"):
                    dpg.add_text("Get Fails-to-Deliver Data")
                    dpg.add_input_text(label="Symbol", tag="ftd_symbol", default_value="TSLA")
                    dpg.add_input_int(label="Reports Limit", tag="ftd_limit", default_value=12)
                    dpg.add_button(label="Get FTD Data", callback=self.get_ftd_callback)
                    with dpg.child_window(height=300, tag="ftd_table"):
                        dpg.add_text("FTD data will appear here...")

                # Tab 5: Equity Search
                with dpg.tab(label="Equity Search"):
                    dpg.add_text("Search Companies")
                    dpg.add_input_text(label="Search Query", tag="search_query", default_value="Tesla")
                    dpg.add_checkbox(label="Search Funds", tag="search_funds", default_value=False)
                    dpg.add_button(label="Search", callback=self.search_callback)
                    with dpg.child_window(height=300, tag="search_table"):
                        dpg.add_text("Search results will appear here...")

        dpg.create_viewport(title="SEC Data Terminal", width=1000, height=700)
        dpg.setup_dearpygui()
        dpg.show_viewport()
        dpg.set_primary_window("main_window", True)

    def get_cik_callback(self):
        """Callback for CIK mapping"""
        symbol = dpg.get_value("cik_symbol")

        def run_async_task():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(self.api.get_cik_map(symbol))
                if "error" in result:
                    dpg.set_value("cik_result", f"Error: {result['error']}")
                else:
                    dpg.set_value("cik_result", f"Symbol: {result['symbol']} -> CIK: {result['cik']}")
            finally:
                loop.close()

        # Run in thread to avoid blocking UI
        import threading
        thread = threading.Thread(target=run_async_task)
        thread.daemon = True
        thread.start()

    def get_filings_callback(self):
        """Callback for company filings"""
        symbol = dpg.get_value("filing_symbol")
        form_type = dpg.get_value("filing_form") or None
        limit = dpg.get_value("filing_limit")

        def run_async_task():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(
                    self.api.get_company_filings(symbol=symbol, form_type=form_type, limit=limit))

                # Clear previous data
                dpg.delete_item("filings_table", children_only=True)

                if "error" in result:
                    dpg.add_text(f"Error: {result['error']}", parent="filings_table")
                else:
                    # Show company info
                    dpg.add_text(f"Company: {result['company_name']} (CIK: {result['cik']})", parent="filings_table")
                    dpg.add_separator(parent="filings_table")

                    # Show filings in table format
                    filings = result['filings'][:10]  # Show first 10
                    for filing in filings:
                        filing_text = f"Form: {filing.get('form', 'N/A')} | Date: {filing.get('filingDate', 'N/A')} | Document: {filing.get('primaryDocument', 'N/A')}"
                        dpg.add_text(filing_text, parent="filings_table")
            finally:
                loop.close()

        import threading
        thread = threading.Thread(target=run_async_task)
        thread.daemon = True
        thread.start()

    def get_facts_callback(self):
        """Callback for company facts"""
        symbol = dpg.get_value("facts_symbol") or None
        fact = dpg.get_value("facts_fact")
        year = dpg.get_value("facts_year") or None

        def run_async_task():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(
                    self.api.get_compare_company_facts(symbol=symbol, fact=fact, year=year))

                # Clear previous data
                dpg.delete_item("facts_table", children_only=True)

                if "error" in result:
                    dpg.add_text(f"Error: {result['error']}", parent="facts_table")
                else:
                    # Show metadata
                    metadata = result['metadata']
                    dpg.add_text(f"Fact: {metadata.get('fact', 'N/A')}", parent="facts_table")
                    if 'company' in metadata:
                        dpg.add_text(f"Company: {metadata['company']}", parent="facts_table")
                    dpg.add_separator(parent="facts_table")

                    # Show data
                    data = result['data'][:15]  # Show first 15
                    for item in data:
                        if symbol:
                            fact_text = f"Value: {item.get('val', 'N/A')} | Period: {item.get('end', 'N/A')} | Filed: {item.get('filed', 'N/A')}"
                        else:
                            fact_text = f"Company: {item.get('symbol', 'N/A')} | Value: {item.get('val', 'N/A')}"
                        dpg.add_text(fact_text, parent="facts_table")
            finally:
                loop.close()

        import threading
        thread = threading.Thread(target=run_async_task)
        thread.daemon = True
        thread.start()

    def get_ftd_callback(self):
        """Callback for FTD data"""
        symbol = dpg.get_value("ftd_symbol")
        limit = dpg.get_value("ftd_limit")

        def run_async_task():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(self.api.get_equity_ftd(symbol, limit))

                # Clear previous data
                dpg.delete_item("ftd_table", children_only=True)

                if "error" in result:
                    dpg.add_text(f"Error: {result['error']}", parent="ftd_table")
                else:
                    # Show summary
                    dpg.add_text(f"Symbol: {result['symbol']} | Total Records: {result['count']}", parent="ftd_table")
                    dpg.add_separator(parent="ftd_table")

                    # Show FTD data
                    data = result['data'][:20]  # Show first 20
                    for item in data:
                        ftd_text = f"Date: {item.get('date', 'N/A')} | Quantity: {item.get('quantity', 'N/A')} | Price: ${item.get('price', 'N/A')}"
                        dpg.add_text(ftd_text, parent="ftd_table")
            finally:
                loop.close()

        import threading
        thread = threading.Thread(target=run_async_task)
        thread.daemon = True
        thread.start()

    def search_callback(self):
        """Callback for equity search"""
        query = dpg.get_value("search_query")
        is_fund = dpg.get_value("search_funds")

        def run_async_task():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            try:
                result = loop.run_until_complete(self.api.get_equity_search(query, is_fund))

                # Clear previous data
                dpg.delete_item("search_table", children_only=True)

                if "error" in result:
                    dpg.add_text(f"Error: {result['error']}", parent="search_table")
                else:
                    # Show summary
                    dpg.add_text(f"Query: '{result['query']}' | Results: {result['count']}", parent="search_table")
                    dpg.add_separator(parent="search_table")

                    # Show search results
                    data = result['data'][:15]  # Show first 15
                    for item in data:
                        if is_fund:
                            search_text = f"Symbol: {item.get('symbol', 'N/A')} | CIK: {item.get('cik', 'N/A')}"
                        else:
                            search_text = f"Symbol: {item.get('symbol', 'N/A')} | Name: {item.get('name', 'N/A')} | CIK: {item.get('cik', 'N/A')}"
                        dpg.add_text(search_text, parent="search_table")
            finally:
                loop.close()

        import threading
        thread = threading.Thread(target=run_async_task)
        thread.daemon = True
        thread.start()

    def run(self):
        """Run the application"""
        self.setup_ui()
        dpg.start_dearpygui()
        dpg.destroy_context()


def main():
    """Main application entry point"""
    app = SECDataUI()
    app.run()


if __name__ == "__main__":
    main()