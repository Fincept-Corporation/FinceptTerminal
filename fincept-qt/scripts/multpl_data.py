# MULTPL (S&P 500 Multiples) Data Wrapper
# Provides access to S&P 500 valuation multiples from https://multpl.com/

import sys
import json
import asyncio
from typing import Dict, List, Optional, Union, Any
from datetime import datetime, date
from io import StringIO
import warnings

import requests
from bs4 import BeautifulSoup
import pandas as pd
from numpy import nan

# Constants
BASE_URL = "https://www.multpl.com/"
USER_AGENTS = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
]

# URL mappings for different data series
URL_DICT = {
    "shiller_pe_month": "shiller-pe/table/by-month",
    "shiller_pe_year": "shiller-pe/table/by-year",
    "pe_year": "s-p-500-pe-ratio/table/by-year",
    "pe_month": "s-p-500-pe-ratio/table/by-month",
    "dividend_year": "s-p-500-dividend/table/by-year",
    "dividend_month": "s-p-500-dividend/table/by-month",
    "dividend_growth_quarter": "s-p-500-dividend-growth/table/by-quarter",
    "dividend_growth_year": "s-p-500-dividend-growth/table/by-year",
    "dividend_yield_year": "s-p-500-dividend-yield/table/by-year",
    "dividend_yield_month": "s-p-500-dividend-yield/table/by-month",
    "earnings_year": "s-p-500-earnings/table/by-year",
    "earnings_month": "s-p-500-earnings/table/by-month",
    "earnings_growth_year": "s-p-500-earnings-growth/table/by-year",
    "earnings_growth_quarter": "s-p-500-earnings-growth/table/by-quarter",
    "real_earnings_growth_year": "s-p-500-real-earnings-growth/table/by-year",
    "real_earnings_growth_quarter": "s-p-500-real-earnings-growth/table/by-quarter",
    "earnings_yield_year": "s-p-500-earnings-yield/table/by-year",
    "earnings_yield_month": "s-p-500-earnings-yield/table/by-month",
    "real_price_year": "s-p-500-historical-prices/table/by-year",
    "real_price_month": "s-p-500-historical-prices/table/by-month",
    "inflation_adjusted_price_year": "inflation-adjusted-s-p-500/table/by-year",
    "inflation_adjusted_price_month": "inflation-adjusted-s-p-500/table/by-month",
    "sales_year": "s-p-500-sales/table/by-year",
    "sales_quarter": "s-p-500-sales/table/by-quarter",
    "sales_growth_year": "s-p-500-sales-growth/table/by-year",
    "sales_growth_quarter": "s-p-500-sales-growth/table/by-quarter",
    "real_sales_year": "s-p-500-real-sales/table/by-year",
    "real_sales_quarter": "s-p-500-real-sales/table/by-quarter",
    "real_sales_growth_year": "s-p-500-real-sales-growth/table/by-year",
    "real_sales_growth_quarter": "s-p-500-real-sales-growth/table/by-quarter",
    "price_to_sales_year": "s-p-500-price-to-sales/table/by-year",
    "price_to_sales_quarter": "s-p-500-price-to-sales/table/by-quarter",
    "price_to_book_value_year": "s-p-500-price-to-book/table/by-year",
    "price_to_book_value_quarter": "s-p-500-price-to-book/table/by-quarter",
    "book_value_year": "s-p-500-book-value/table/by-year",
    "book_value_quarter": "s-p-500-book-value/table/by-quarter",
}

class MULTPLError(Exception):
    """Custom exception for MULTPL API errors"""
    pass

class MULTPLDataFetcher:
    """Fault-tolerant MULTPL data fetcher"""

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': USER_AGENTS[0],
            'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
            'Upgrade-Insecure-Requests': '1',
        })

    def _get_random_user_agent(self) -> str:
        """Get a random user agent"""
        import random
        return random.choice(USER_AGENTS)

    def _make_request(self, url: str, timeout: int = 30) -> Optional[str]:
        """Make HTTP request with error handling"""
        try:
            # Rotate user agent
            self.session.headers['User-Agent'] = self._get_random_user_agent()

            response = self.session.get(url, timeout=timeout)
            response.raise_for_status()
            return response.text
        except requests.exceptions.RequestException as e:
            raise MULTPLError(f"HTTP request failed for {url}: {str(e)}")
        except Exception as e:
            raise MULTPLError(f"Unexpected error fetching {url}: {str(e)}")

    def _parse_html_table(self, html_content: str, series_name: str) -> pd.DataFrame:
        """Parse HTML table content"""
        try:
            # Use pandas to read HTML tables
            df_list = pd.read_html(StringIO(html_content))
            if not df_list:
                raise MULTPLError(f"No tables found in HTML content for {series_name}")

            df = df_list[0].copy()  # Use the first table

            # Ensure required columns exist
            if 'Date' not in df.columns or 'Value' not in df.columns:
                raise MULTPLError(f"Expected columns 'Date' and 'Value' not found for {series_name}")

            # Clean and convert data
            df['Date'] = pd.to_datetime(df['Date']).dt.date
            df = df.sort_values('Date').reset_index(drop=True)

            # Clean value column
            def clean_value(x):
                if isinstance(x, str):
                    # Remove special characters and percentage signs
                    x = x.strip().replace('† ', '').replace('%', '')
                    try:
                        return float(x) if x else None
                    except ValueError:
                        return None
                return x

            df['Value'] = df['Value'].apply(clean_value)

            # Convert growth and yield series to decimal (from percentage)
            if 'growth' in series_name or 'yield' in series_name:
                df['Value'] = df['Value'] / 100

            # Add series name
            df['name'] = series_name

            # Replace NaN with None
            df = df.replace({nan: None})

            return df

        except Exception as e:
            raise MULTPLError(f"Error parsing HTML table for {series_name}: {str(e)}")

    def get_series_data(
        self,
        series_name: str,
        start_date: Optional[str] = None,
        end_date: Optional[str] = None
    ) -> Dict[str, Any]:
        """Get data for a specific series"""
        try:
            if series_name not in URL_DICT:
                raise MULTPLError(f"Invalid series name: {series_name}")

            url = f"{BASE_URL}{URL_DICT[series_name]}"
            html_content = self._make_request(url)

            if not html_content:
                raise MULTPLError(f"No content received from {url}")

            df = self._parse_html_table(html_content, series_name)

            # Filter by date range if provided
            if start_date:
                start_date_obj = datetime.strptime(start_date, '%Y-%m-%d').date()
                df = df[df['Date'] >= start_date_obj]

            if end_date:
                end_date_obj = datetime.strptime(end_date, '%Y-%m-%d').date()
                df = df[df['Date'] <= end_date_obj]

            return {
                "success": True,
                "series_name": series_name,
                "data": df.to_dict(orient='records'),
                "count": len(df),
                "url": url
            }

        except MULTPLError:
            raise
        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching data for {series_name}: {str(e)}",
                "series_name": series_name
            }

    def get_multiple_series(
        self,
        series_names: List[str],
        start_date: Optional[str] = None,
        end_date: Optional[str] = None
    ) -> Dict[str, Any]:
        """Get data for multiple series"""
        results = []
        errors = []

        for series_name in series_names:
            try:
                result = self.get_series_data(series_name, start_date, end_date)
                if result['success']:
                    results.append(result)
                else:
                    errors.append(result)
            except Exception as e:
                errors.append({
                    "success": False,
                    "error": str(e),
                    "series_name": series_name
                })

        return {
            "success": len(results) > 0,
            "results": results,
            "errors": errors,
            "total_requested": len(series_names),
            "successful_fetches": len(results),
            "failed_fetches": len(errors)
        }

    def get_available_series(self) -> Dict[str, Any]:
        """Get list of available series"""
        return {
            "success": True,
            "available_series": sorted(list(URL_DICT.keys())),
            "total_series": len(URL_DICT),
            "categories": {
                "valuation": [
                    "shiller_pe_month", "shiller_pe_year", "pe_year", "pe_month",
                    "price_to_sales_year", "price_to_sales_quarter",
                    "price_to_book_value_year", "price_to_book_value_quarter"
                ],
                "dividend": [
                    "dividend_year", "dividend_month", "dividend_growth_quarter",
                    "dividend_growth_year", "dividend_yield_year", "dividend_yield_month"
                ],
                "earnings": [
                    "earnings_year", "earnings_month", "earnings_growth_year",
                    "earnings_growth_quarter", "real_earnings_growth_year",
                    "real_earnings_growth_quarter", "earnings_yield_year", "earnings_yield_month"
                ],
                "price": [
                    "real_price_year", "real_price_month",
                    "inflation_adjusted_price_year", "inflation_adjusted_price_month"
                ],
                "sales": [
                    "sales_year", "sales_quarter", "sales_growth_year", "sales_growth_quarter",
                    "real_sales_year", "real_sales_quarter", "real_sales_growth_year",
                    "real_sales_growth_quarter"
                ],
                "book_value": [
                    "book_value_year", "book_value_quarter"
                ]
            }
        }

def main():
    """CLI interface for MULTPL data wrapper"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python multpl_data.py <command> [args...]"
        }))
        sys.exit(1)

    command = sys.argv[1]
    fetcher = MULTPLDataFetcher()

    try:
        if command == "get_series":
            # Usage: get_series <series_name> [start_date] [end_date]
            if len(sys.argv) < 3:
                print(json.dumps({
                    "success": False,
                    "error": "Usage: python multpl_data.py get_series <series_name> [start_date] [end_date]"
                }))
                sys.exit(1)

            series_name = sys.argv[2]
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None

            result = fetcher.get_series_data(series_name, start_date, end_date)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_multiple":
            # Usage: get_multiple <series_name1,series_name2,...> [start_date] [end_date]
            if len(sys.argv) < 3:
                print(json.dumps({
                    "success": False,
                    "error": "Usage: python multpl_data.py get_multiple <series_name1,series_name2,...> [start_date] [end_date]"
                }))
                sys.exit(1)

            series_names = [s.strip() for s in sys.argv[2].split(',')]
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None

            result = fetcher.get_multiple_series(series_names, start_date, end_date)
            print(json.dumps(result, indent=2, default=str))

        elif command == "available_series":
            result = fetcher.get_available_series()
            print(json.dumps(result, indent=2))

        elif command == "get_shiller_pe":
            # Convenience method for Shiller P/E
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = fetcher.get_series_data("shiller_pe_month", start_date, end_date)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_pe_ratio":
            # Convenience method for P/E ratio
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = fetcher.get_series_data("pe_month", start_date, end_date)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_dividend_yield":
            # Convenience method for dividend yield
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = fetcher.get_series_data("dividend_yield_month", start_date, end_date)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_earnings_yield":
            # Convenience method for earnings yield
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = fetcher.get_series_data("earnings_yield_month", start_date, end_date)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_price_to_sales":
            # Convenience method for price-to-sales ratio
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = fetcher.get_series_data("price_to_sales_year", start_date, end_date)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_valuation_overview":
            # Get key valuation metrics
            valuation_series = ["shiller_pe_month", "pe_month", "price_to_sales_year", "earnings_yield_month"]
            result = fetcher.get_multiple_series(valuation_series)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_dividend_overview":
            # Get dividend-related metrics
            dividend_series = ["dividend_yield_month", "dividend_growth_year", "dividend_month"]
            result = fetcher.get_multiple_series(dividend_series)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_earnings_overview":
            # Get earnings-related metrics
            earnings_series = ["earnings_yield_month", "earnings_growth_year", "earnings_month"]
            result = fetcher.get_multiple_series(earnings_series)
            print(json.dumps(result, indent=2, default=str))

        elif command == "get_comprehensive_overview":
            # Get comprehensive overview across all categories
            key_series = [
                "shiller_pe_month", "pe_month", "dividend_yield_month",
                "earnings_yield_month", "price_to_sales_year"
            ]
            result = fetcher.get_multiple_series(key_series)
            print(json.dumps(result, indent=2, default=str))

        else:
            print(json.dumps({
                "success": False,
                "error": f"Unknown command: {command}. Available commands: get_series, get_multiple, available_series, get_shiller_pe, get_pe_ratio, get_dividend_yield, get_earnings_yield, get_price_to_sales, get_valuation_overview, get_dividend_overview, get_earnings_overview, get_comprehensive_overview"
            }))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": f"Command execution failed: {str(e)}"
        }))
        sys.exit(1)

if __name__ == "__main__":
    main()