# -*- coding: utf-8 -*-
# sec_endpoint_checker.py

import asyncio
import json
from sec_provider import SECProvider


class SECEndpointChecker:
    """Check all 17 SEC endpoints functionality with detailed debugging"""

    def __init__(self):
        self.sec = SECProvider(use_cache=True, rate_limit=2)
        self.results = {}
        self.debug_mode = True

    def debug_print(self, message, data=None):
        """Print debug information if debug mode is enabled"""
        if self.debug_mode:
            print(f"  DEBUG: {message}")
            if data:
                if isinstance(data, dict):
                    print(f"    Response keys: {list(data.keys())}")
                    if "error" in data:
                        print(f"    Error: {data['error']}")
                    if "data" in data and isinstance(data["data"], (list, dict)):
                        if isinstance(data["data"], list):
                            print(f"    Data length: {len(data['data'])}")
                            if len(data["data"]) > 0:
                                print(
                                    f"    First item keys: {list(data['data'][0].keys()) if isinstance(data['data'][0], dict) else 'Not a dict'}")
                        else:
                            print(f"    Data keys: {list(data['data'].keys())}")
                elif isinstance(data, str):
                    print(f"    Content length: {len(data)}")
                    print(f"    Content preview: {data[:100]}...")

    async def check_cik_mapping(self):
        """Check CIK mapping endpoint"""
        try:
            print("\n1. Testing CIK Mapping for 'AAPL'...")
            result = await self.sec.get_cik_map("AAPL")
            self.debug_print("CIK mapping response", result)

            success = result.get("success", False)
            cik = result.get("data", {}).get("cik", "") if success else ""
            print(f"1. CIK Mapping: {'✓' if success else '✗'} - {cik[:10] if cik else 'Failed'}")
            self.results["cik_mapping"] = success
        except Exception as e:
            print(f"1. CIK Mapping: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["cik_mapping"] = False

    async def check_symbol_mapping(self):
        """Check symbol mapping endpoint"""
        try:
            print("\n2. Testing Symbol Mapping for CIK '320193'...")
            result = await self.sec.get_symbol_map("320193")
            self.debug_print("Symbol mapping response", result)

            success = result.get("success", False)
            symbol = result.get("data", {}).get("symbol", "") if success else ""
            print(f"2. Symbol Mapping: {'✓' if success else '✗'} - {symbol if symbol else 'Failed'}")
            self.results["symbol_mapping"] = success
        except Exception as e:
            print(f"2. Symbol Mapping: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["symbol_mapping"] = False

    async def check_company_filings(self):
        """Check company filings endpoint"""
        try:
            print("\n3. Testing Company Filings for 'AAPL'...")
            result = await self.sec.get_company_filings(symbol="AAPL", limit=5)
            self.debug_print("Company filings response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif count == 0:
                print("  WARNING: Request successful but no filings returned")

            print(f"3. Company Filings: {'✓' if success else '✗'} - {count} filings found")
            self.results["company_filings"] = success
        except Exception as e:
            print(f"3. Company Filings: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["company_filings"] = False

    async def check_compare_facts(self):
        """Check compare company facts endpoint"""
        try:
            print("\n4. Testing Compare Facts for 'AAPL' with 'Revenues'...")
            result = await self.sec.get_compare_company_facts(["AAPL"], "Revenues")
            self.debug_print("Compare facts response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif count == 0:
                print("  WARNING: Request successful but no company data returned")

            print(f"4. Compare Facts: {'✓' if success else '✗'} - {count} companies analyzed")
            self.results["compare_facts"] = success
        except Exception as e:
            print(f"4. Compare Facts: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["compare_facts"] = False

    async def check_equity_ftd(self):
        """Check equity FTD endpoint"""
        try:
            print("\n5. Testing Equity FTD for 'AAPL'...")
            result = await self.sec.get_equity_ftd("AAPL", limit=5)
            self.debug_print("Equity FTD response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0
            print(f"5. Equity FTD: {'✓' if success else '✗'} - {count} FTD files found")
            self.results["equity_ftd"] = success
        except Exception as e:
            print(f"5. Equity FTD: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["equity_ftd"] = False

    async def check_equity_search(self):
        """Check equity search endpoint"""
        try:
            print("\n6. Testing Equity Search for 'Apple'...")
            result = await self.sec.get_equity_search("Apple")
            self.debug_print("Equity search response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0
            print(f"6. Equity Search: {'✓' if success else '✗'} - {count} results found")
            self.results["equity_search"] = success
        except Exception as e:
            print(f"6. Equity Search: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["equity_search"] = False

    async def check_etf_holdings(self):
        """Check ETF holdings endpoint"""
        try:
            print("\n7. Testing ETF Holdings for 'SPY'...")
            result = await self.sec.get_etf_holdings("SPY")
            self.debug_print("ETF holdings response", result)

            success = result.get("success", False)
            name = result.get("data", {}).get("name", "") if success else ""

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif not name:
                print("  WARNING: Request successful but no fund name returned")

            print(f"7. ETF Holdings: {'✓' if success else '✗'} - {name[:30] if name else 'Failed'}")
            self.results["etf_holdings"] = success
        except Exception as e:
            print(f"7. ETF Holdings: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["etf_holdings"] = False

    async def check_form_13f_hr(self):
        """Check Form 13F-HR endpoint"""
        try:
            print("\n8. Testing Form 13F-HR for 'BRK-A'...")
            result = await self.sec.get_form_13f_hr("BRK-A", limit=3)
            self.debug_print("Form 13F-HR response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif count == 0:
                print("  WARNING: Request successful but no 13F-HR filings found")

            print(f"8. Form 13F-HR: {'✓' if success else '✗'} - {count} filings found")
            self.results["form_13f_hr"] = success
        except Exception as e:
            print(f"8. Form 13F-HR: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["form_13f_hr"] = False

    async def check_htm_file(self):
        """Check HTM file endpoint"""
        try:
            print("\n9. Testing HTM File download...")
            test_url = "https://www.sec.gov/Archives/edgar/data/320193/000032019323000106/aapl-20230930.htm"
            print(f"  URL: {test_url}")
            result = await self.sec.get_htm_file(test_url)
            self.debug_print("HTM file response", result)

            success = result.get("success", False)
            content_length = len(result.get("data", {}).get("content", "")) if success else 0

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif content_length == 0:
                print("  WARNING: Request successful but no content returned")

            print(f"9. HTM File: {'✓' if success else '✗'} - {content_length} chars downloaded")
            self.results["htm_file"] = success
        except Exception as e:
            print(f"9. HTM File: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["htm_file"] = False

    async def check_insider_trading(self):
        """Check insider trading endpoint"""
        try:
            print("\n10. Testing Insider Trading for 'AAPL'...")
            result = await self.sec.get_insider_trading("AAPL", limit=5)
            self.debug_print("Insider trading response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif count == 0:
                print("  WARNING: Request successful but no Form 4 filings found")

            print(f"10. Insider Trading: {'✓' if success else '✗'} - {count} Form 4 filings found")
            self.results["insider_trading"] = success
        except Exception as e:
            print(f"10. Insider Trading: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["insider_trading"] = False

    async def check_institutions_search(self):
        """Check institutions search endpoint"""
        try:
            print("\n11. Testing Institutions Search for 'Berkshire'...")
            result = await self.sec.get_institutions_search("Berkshire")
            self.debug_print("Institutions search response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0
            print(f"11. Institutions Search: {'✓' if success else '✗'} - {count} institutions found")
            self.results["institutions_search"] = success
        except Exception as e:
            print(f"11. Institutions Search: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["institutions_search"] = False

    async def check_latest_reports(self):
        """Check latest financial reports endpoint"""
        try:
            print("\n12. Testing Latest Financial Reports...")
            result = await self.sec.get_latest_financial_reports()
            self.debug_print("Latest reports response", result)

            success = result.get("success", False)
            count = len(result.get("data", [])) if success else 0

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif count == 0:
                print("  WARNING: Request successful but no reports found")

            print(f"12. Latest Reports: {'✓' if success else '✗'} - {count} reports found")
            self.results["latest_reports"] = success
        except Exception as e:
            print(f"12. Latest Reports: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["latest_reports"] = False

    async def check_md_analysis(self):
        """Check management discussion analysis endpoint"""
        try:
            print("\n13. Testing MD&A for 'AAPL'...")
            result = await self.sec.get_management_discussion_analysis("AAPL")
            self.debug_print("MD&A response", result)

            success = result.get("success", False)
            filing = result.get("data", {}).get("filing", {}) if success else {}
            form = filing.get("form", "") if filing else ""

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif not form:
                print("  WARNING: Request successful but no filing form returned")

            print(f"13. MD&A: {'✓' if success else '✗'} - {form if form else 'Failed'}")
            self.results["md_analysis"] = success
        except Exception as e:
            print(f"13. MD&A: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["md_analysis"] = False

    async def check_rss_litigation(self):
        """Check RSS litigation endpoint"""
        try:
            print("\n14. Testing RSS Litigation...")
            result = await self.sec.get_rss_litigation()
            self.debug_print("RSS litigation response", result)

            success = result.get("success", False)
            has_content = len(result.get("data", {}).get("content", "")) > 0 if success else False
            print(f"14. RSS Litigation: {'✓' if success else '✗'} - {'Content found' if has_content else 'No content'}")
            self.results["rss_litigation"] = success
        except Exception as e:
            print(f"14. RSS Litigation: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["rss_litigation"] = False

    async def check_schema_files(self):
        """Check schema files endpoint"""
        try:
            print("\n15. Testing Schema Files for '2024'...")
            result = await self.sec.get_schema_files("2024")
            self.debug_print("Schema files response", result)

            success = result.get("success", False)
            has_content = len(result.get("data", {}).get("content", "")) > 0 if success else False

            if not success:
                print(f"  ERROR: {result.get('error', 'Unknown error')}")
            elif not has_content:
                print("  WARNING: Request successful but no content found")

            print(f"15. Schema Files: {'✓' if success else '✗'} - {'Content found' if has_content else 'No content'}")
            self.results["schema_files"] = success
        except Exception as e:
            print(f"15. Schema Files: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["schema_files"] = False

    async def check_sec_filing(self):
        """Check SEC filing endpoint"""
        try:
            print("\n16. Testing SEC Filing...")
            test_url = "https://www.sec.gov/Archives/edgar/data/320193/000032019324000123/"
            print(f"  URL: {test_url}")
            result = await self.sec.get_sec_filing(test_url)
            self.debug_print("SEC filing response", result)

            success = result.get("success", False)
            has_content = len(result.get("data", {}).get("content", "")) > 0 if success else False
            print(f"16. SEC Filing: {'✓' if success else '✗'} - {'Content found' if has_content else 'No content'}")
            self.results["sec_filing"] = success
        except Exception as e:
            print(f"16. SEC Filing: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["sec_filing"] = False

    async def check_sic_search(self):
        """Check SIC search endpoint"""
        try:
            print("\n17. Testing SIC Search for 'computer'...")
            result = await self.sec.get_sic_search("computer")
            self.debug_print("SIC search response", result)

            success = result.get("success", False)
            has_content = len(result.get("data", {}).get("content", "")) > 0 if success else False
            print(f"17. SIC Search: {'✓' if success else '✗'} - {'Content found' if has_content else 'No content'}")
            self.results["sic_search"] = success
        except Exception as e:
            print(f"17. SIC Search: ✗ - Exception: {str(e)}")
            self.debug_print(f"Exception details: {type(e).__name__}: {str(e)}")
            self.results["sic_search"] = False

    async def run_all_checks(self):
        """Run all endpoint checks"""
        print("=" * 80)
        print("SEC ENDPOINT CHECKER - Testing All 17 Endpoints (DEBUG MODE)")
        print("=" * 80)

        checks = [
            self.check_cik_mapping(),
            self.check_symbol_mapping(),
            self.check_company_filings(),
            self.check_compare_facts(),
            self.check_equity_ftd(),
            self.check_equity_search(),
            self.check_etf_holdings(),
            self.check_form_13f_hr(),
            self.check_htm_file(),
            self.check_insider_trading(),
            self.check_institutions_search(),
            self.check_latest_reports(),
            self.check_md_analysis(),
            self.check_rss_litigation(),
            self.check_schema_files(),
            self.check_sec_filing(),
            self.check_sic_search()
        ]

        # Run checks with delay to respect rate limits
        for i, check in enumerate(checks):
            await check
            if i < len(checks) - 1:  # Don't delay after last check
                print("  Waiting 1 second...")
                await asyncio.sleep(1)  # 1 second delay between requests

        # Summary
        print("\n" + "=" * 80)
        print("DETAILED SUMMARY")
        print("=" * 80)
        working = sum(1 for success in self.results.values() if success)
        total = len(self.results)
        print(f"Working Endpoints: {working}/{total}")
        print(f"Success Rate: {(working / total) * 100:.1f}%")

        print(f"\nWorking Endpoints ({working}):")
        for endpoint, success in self.results.items():
            if success:
                print(f"  ✓ {endpoint}")

        if working < total:
            print(f"\nFailed Endpoints ({total - working}):")
            for endpoint, success in self.results.items():
                if not success:
                    print(f"  ✗ {endpoint}")

        print(f"\nRecommendations:")
        print("1. Check network connectivity and SEC API availability")
        print("2. Verify User-Agent string is acceptable to SEC")
        print("3. Check if rate limiting is being enforced")
        print("4. Validate URL construction for failed endpoints")
        print("5. Ensure proper error handling for empty responses")

        await self.sec.close()


async def main():
    """Main function to run all checks"""
    checker = SECEndpointChecker()
    await checker.run_all_checks()


if __name__ == "__main__":
    asyncio.run(main())