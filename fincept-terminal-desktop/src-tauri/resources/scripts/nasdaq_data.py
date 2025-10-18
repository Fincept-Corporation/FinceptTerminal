# NASDAQ Data Wrapper
# Based on OpenBB NASDAQ provider - https://github.com/OpenBB-finance/OpenBB/tree/main/openbb_platform/providers/nasdaq

import sys
import json
import requests
import os
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Union, Any, Literal
from io import StringIO
import pandas as pd
import asyncio
import aiohttp
import re
import html

# NASDAQ API URLs
NASDAQ_BASE_URL = "https://api.nasdaq.com/api"
NASDAQ_EQUITY_DIR_URL = "https://www.nasdaqtrader.com/dynamic/SymDir/nasdaqtraded.txt"
NASDAQ_RTAT_URL = "https://data.nasdaq.com/api/v3/datatables/NDAQ/RTAT10/"

# Market cap classifications
MARKET_CAP_CHOICES = {
    "mega": "> $200B",
    "large": "$10B - $200B",
    "mid": "$2B - $10B",
    "small": "$300M - $2B",
    "micro": "$50M - $300M"
}

# Exchange choices
EXCHANGE_CHOICES = ["nasdaq", "nyse", "amex", "all"]

# Sector choices
SECTOR_CHOICES = [
    "energy", "basic_materials", "industrials", "consumer_staples",
    "consumer_discretionary", "health_care", "financial_services",
    "technology", "communication_services", "utilities", "real_estate"
]

# IPO status choices
IPO_STATUS_CHOICES = ["upcoming", "priced", "filed", "withdrawn"]


class NASDAQError:
    """Custom error class for NASDAQ API errors"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "error": True,
            "endpoint": self.endpoint,
            "message": self.error,
            "status_code": self.status_code,
            "timestamp": self.timestamp
        }


class NASDAQDataAPI:
    """NASDAQ Data API wrapper for modular data fetching"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.getenv("NASDAQ_API_KEY")
        self.session = requests.Session()

        # Set up default headers
        self._update_headers()

    def _update_headers(self):
        """Update session headers with random user agent"""
        self.session.headers.update({
            'User-Agent': self._get_random_user_agent(),
            'Accept': 'application/json, text/plain, */*',
            'Accept-Encoding': 'gzip',
            'Accept-Language': 'en-CA,en-US;q=0.7,en;q=0.3',
            'Host': 'api.nasdaq.com',
            'Origin': 'https://www.nasdaq.com',
            'Referer': 'https://www.nasdaq.com/',
            'Connection': 'keep-alive'
        })

    def _get_random_user_agent(self) -> str:
        """Generate a random user agent"""
        user_agents = [
            'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
            'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36',
            'Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:89.0) Gecko/20100101 Firefox/89.0',
            'Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:89.0) Gecko/20100101 Firefox/89.0'
        ]
        import random
        return random.choice(user_agents)

    async def _make_async_request(self, url: str, headers: Optional[Dict] = None) -> Dict[str, Any]:
        """Make async HTTP request with error handling"""
        try:
            default_headers = {
                'User-Agent': self._get_random_user_agent(),
                'Accept': 'application/json, text/plain, */*',
                'Accept-Encoding': 'gzip',
                'Accept-Language': 'en-CA,en-US;q=0.7,en;q=0.3',
                'Host': 'api.nasdaq.com',
                'Origin': 'https://www.nasdaq.com',
                'Referer': 'https://www.nasdaq.com/',
                'Connection': 'keep-alive'
            }

            final_headers = {**default_headers, **(headers or {})}

            async with aiohttp.ClientSession(headers=final_headers) as session:
                async with session.get(url) as response:
                    if response.status == 200:
                        result = await response.json()
                        return {"success": True, "data": result}
                    else:
                        text = await response.text()
                        return NASDAQError(url, f"HTTP {response.status}: {text}", response.status).to_dict()

        except aiohttp.ClientError as e:
            return NASDAQError(url, f"Network error: {str(e)}").to_dict()
        except json.JSONDecodeError as e:
            return NASDAQError(url, f"JSON decode error: {str(e)}").to_dict()
        except Exception as e:
            return NASDAQError(url, f"Unexpected error: {str(e)}").to_dict()

    def _make_request(self, url: str, headers: Optional[Dict] = None) -> Dict[str, Any]:
        """Make HTTP request with error handling"""
        try:
            final_headers = {**self.session.headers, **(headers or {})}
            response = self.session.get(url, headers=final_headers, timeout=30)

            if response.status_code == 200:
                try:
                    data = response.json()
                    return {"success": True, "data": data}
                except json.JSONDecodeError:
                    return NASDAQError(url, "Invalid JSON response", response.status_code).to_dict()
            else:
                return NASDAQError(url, f"HTTP {response.status_code}: {response.text}", response.status_code).to_dict()

        except requests.exceptions.RequestException as e:
            return NASDAQError(url, f"Network error: {str(e)}", getattr(e.response, 'status_code', None)).to_dict()
        except Exception as e:
            return NASDAQError(url, f"Unexpected error: {str(e)}").to_dict()

    def _parse_equity_directory(self, content: str) -> pd.DataFrame:
        """Parse NASDAQ equity directory data"""
        try:
            # Parse pipe-delimited data
            df = pd.read_csv(StringIO(content), sep="|")

            # Remove last row (usually empty/summary)
            if len(df) > 0:
                df = df.iloc[:-1]

            # Clean up column names
            df.columns = [col.strip() for col in df.columns]

            # Remove test issues
            if 'Security Name' in df.columns:
                df = df[~df['Security Name'].str.contains('test', case=False, na=False)]

            return df
        except Exception as e:
            raise ValueError(f"Failed to parse equity directory: {str(e)}")

    def _remove_html_tags(self, text: str) -> str:
        """Remove HTML tags from text"""
        if not text:
            return text
        clean = re.compile('<.*?>')
        return re.sub(clean, ' ', text)

    def _clean_html_text(self, text: str) -> str:
        """Clean HTML entities and tags"""
        if not text:
            return text

        # Unescape HTML entities
        text = html.unescape(text)
        text = text.replace('\r\n\r\n', ' ').replace('\r\n', ' ')
        text = text.replace("''", "'")

        # Remove HTML tags
        text = self._remove_html_tags(text)

        return text.strip() if text else None

    async def search_equities(self, query: str = "", is_etf: Optional[bool] = None) -> Dict[str, Any]:
        """Search for equities in NASDAQ directory

        Args:
            query: Search query (symbol or company name)
            is_etf: Filter by ETF status (True, False, or None for all)

        Returns:
            Dict containing search results
        """
        try:
            # Get equity directory
            result = self._make_request(NASDAQ_EQUITY_DIR_URL,
                                       headers={'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8'})

            if "error" in result:
                return result

            # Parse directory data
            df = self._parse_equity_directory(result["data"])

            if df.empty:
                return NASDAQError("equity_search", "No equity data found").to_dict()

            # Filter by ETF status
            if is_etf is not None and 'ETF' in df.columns:
                df = df[df['ETF'] == ('Y' if is_etf else 'N')]

            # Apply search query
            if query.strip():
                search_columns = ['Symbol', 'Security Name']
                if 'CQS Symbol' in df.columns:
                    search_columns.append('CQS Symbol')
                if 'NASDAQ Symbol' in df.columns:
                    search_columns.append('NASDAQ Symbol')

                # Create mask for search
                mask = pd.Series([False] * len(df))
                for col in search_columns:
                    if col in df.columns:
                        mask |= df[col].str.contains(query, case=False, na=False)

                df = df[mask]

            if df.empty:
                return NASDAQError("equity_search", f"No results found for query: '{query}'").to_dict()

            # Clean up data
            if 'Market Category' in df.columns:
                df['Market Category'] = df['Market Category'].replace(' ', None)

            # Convert to records
            results = df.replace({pd.NA: None, 'nan': None}).to_dict("records")

            return {
                "success": True,
                "data": {
                    "query": query,
                    "is_etf_filter": is_etf,
                    "results": results,
                    "total_count": len(results)
                }
            }

        except Exception as e:
            return NASDAQError("equity_search", str(e)).to_dict()

    async def get_equity_screener(self, exchange: str = "all", market_cap: str = "all",
                               sector: str = "all", country: str = "all",
                               limit: Optional[int] = None) -> Dict[str, Any]:
        """Get equity screener results with filters

        Args:
            exchange: Exchange filter (nasdaq, nyse, amex, all)
            market_cap: Market cap filter (mega, large, mid, small, micro, all)
            sector: Sector filter
            country: Country filter
            limit: Maximum number of results

        Returns:
            Dict containing screener results
        """
        try:
            # Build URL with parameters
            limit_param = limit if limit else 10000
            base_url = f"{NASDAQ_BASE_URL}/screener/stocks?tableonly=true&limit={limit_param}&"

            # Process filters
            params = {}

            if exchange != "all":
                params["exchange"] = exchange.upper()

            if market_cap != "all":
                params["marketcap"] = market_cap

            if sector != "all":
                # Map sector names
                sector_mapping = {
                    "communication_services": "telecommunications",
                    "financial_services": "finance"
                }
                sector_clean = sector_mapping.get(sector, sector)
                params["sector"] = sector_clean

            if country != "all":
                params["country"] = country.lower().replace(" ", "_")

            # Build query string
            if params:
                query_string = "&".join([f"{k}={v}" for k, v in params.items()])
                url = base_url + query_string
            else:
                url = base_url

            # Make request
            result = await self._make_async_request(url)

            if "error" in result:
                return result

            # Extract data from response
            data = result.get("data", {})
            rows = data.get("data", {}).get("table", {}).get("rows", [])

            if not rows:
                return NASDAQError("equity_screener", "No screener results found").to_dict()

            # Sort by percentage change (descending)
            sorted_rows = sorted(rows, key=lambda x: float(x.get("pctchange", 0)), reverse=True)

            # Clean numeric fields
            cleaned_results = []
            for row in sorted_rows:
                cleaned_row = {}
                for key, value in row.items():
                    if key in ["lastsale", "netchange", "pctchange", "marketCap"]:
                        # Clean numeric values
                        if isinstance(value, str):
                            cleaned_value = value.replace("%", "").replace("$", "").replace(",", "")
                            cleaned_value = cleaned_value.replace("UNCH", "").replace("--", "").replace("NA", "")
                            try:
                                if key == "pctchange":
                                    cleaned_row[key] = float(cleaned_value) / 100 if cleaned_value else None
                                else:
                                    cleaned_row[key] = float(cleaned_value) if cleaned_value else None
                            except ValueError:
                                cleaned_row[key] = None
                        else:
                            cleaned_row[key] = value
                    else:
                        cleaned_row[key] = value

                cleaned_results.append(cleaned_row)

            return {
                "success": True,
                "data": {
                    "filters": {
                        "exchange": exchange,
                        "market_cap": market_cap,
                        "sector": sector,
                        "country": country
                    },
                    "results": cleaned_results[:limit] if limit else cleaned_results,
                    "total_count": len(cleaned_results)
                }
            }

        except Exception as e:
            return NASDAQError("equity_screener", str(e)).to_dict()

    async def get_dividend_calendar(self, start_date: Optional[str] = None,
                                   end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get dividend calendar

        Args:
            start_date: Start date in YYYY-MM-DD format
            end_date: End date in YYYY-MM-DD format

        Returns:
            Dict containing dividend calendar data
        """
        try:
            # Set default dates
            now = datetime.now().date()
            start = datetime.strptime(start_date, "%Y-%m-%d").date() if start_date else now
            end = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else now + timedelta(days=3)

            # Generate date range
            date_list = []
            current = start
            while current <= end:
                date_list.append(current.strftime("%Y-%m-%d"))
                current += timedelta(days=1)

            # Fetch data for each date
            all_dividends = []
            headers = {
                'User-Agent': self._get_random_user_agent(),
                'Accept': 'application/json, text/plain, */*',
                'Accept-Encoding': 'gzip',
                'Accept-Language': 'en-CA,en-US;q=0.7,en;q=0.3',
                'Host': 'api.nasdaq.com',
                'Connection': 'keep-alive'
            }

            async def fetch_dividends_for_date(date_str):
                url = f"{NASDAQ_BASE_URL}/calendar/dividends?date={date_str}"
                result = await self._make_async_request(url, headers)

                if "error" not in result:
                    data = result.get("data", {})
                    calendar_data = data.get("calendar", {}).get("rows", [])
                    return calendar_data
                return []

            # Fetch all dates concurrently
            tasks = [fetch_dividends_for_date(date_str) for date_str in date_list]
            results = await asyncio.gather(*tasks)

            # Flatten results
            for date_results in results:
                all_dividends.extend(date_results)

            if not all_dividends:
                return NASDAQError("dividend_calendar", "No dividend data found").to_dict()

            # Sort by ex-dividend date
            sorted_dividends = sorted(all_dividends,
                                     key=lambda x: x.get("dividend_Ex_Date", ""),
                                     reverse=True)

            # Clean and format data
            formatted_results = []
            for dividend in sorted_dividends:
                formatted_dividend = {
                    "symbol": dividend.get("symbol"),
                    "company_name": dividend.get("companyName"),
                    "ex_dividend_date": self._parse_date(dividend.get("dividend_Ex_Date")),
                    "payment_date": self._parse_date(dividend.get("payment_Date")),
                    "record_date": self._parse_date(dividend.get("record_Date")),
                    "declaration_date": self._parse_date(dividend.get("announcement_Date")),
                    "amount": self._parse_float(dividend.get("dividend_Rate")),
                    "annualized_amount": self._parse_float(dividend.get("indicated_Annual_Dividend"))
                }
                formatted_results.append(formatted_dividend)

            return {
                "success": True,
                "data": {
                    "date_range": {
                        "start": start.strftime("%Y-%m-%d"),
                        "end": end.strftime("%Y-%m-%d")
                    },
                    "dividends": formatted_results,
                    "total_count": len(formatted_results)
                }
            }

        except Exception as e:
            return NASDAQError("dividend_calendar", str(e)).to_dict()

    async def get_earnings_calendar(self, start_date: Optional[str] = None,
                                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get earnings calendar

        Args:
            start_date: Start date in YYYY-MM-DD format
            end_date: End date in YYYY-MM-DD format

        Returns:
            Dict containing earnings calendar data
        """
        try:
            # Set default dates
            now = datetime.now().date()
            start = datetime.strptime(start_date, "%Y-%m-%d").date() if start_date else now
            end = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else now + timedelta(days=3)

            # Generate date range
            date_list = []
            current = start
            while current <= end:
                date_list.append(current.strftime("%Y-%m-%d"))
                current += timedelta(days=1)

            # Fetch data for each date
            all_earnings = []
            headers = {
                'User-Agent': self._get_random_user_agent(),
                'Accept': 'application/json, text/plain, */*',
                'Accept-Encoding': 'gzip',
                'Accept-Language': 'en-CA,en-US;q=0.7,en;q=0.3',
                'Host': 'api.nasdaq.com',
                'Connection': 'keep-alive'
            }

            async def fetch_earnings_for_date(date_str):
                url = f"{NASDAQ_BASE_URL}/calendar/earnings?date={date_str}"
                result = await self._make_async_request(url, headers)

                if "error" not in result:
                    data = result.get("data", {})
                    rows = data.get("rows", [])

                    # Add report date to each earnings
                    if rows and data.get("asOf"):
                        report_date = datetime.strptime(data["asOf"], "%a, %b %d, %Y").date()
                        for row in rows:
                            row["date"] = report_date.strftime("%Y-%m-%d")

                    return rows
                return []

            # Fetch all dates concurrently
            tasks = [fetch_earnings_for_date(date_str) for date_str in date_list]
            results = await asyncio.gather(*tasks)

            # Flatten results
            for date_results in results:
                all_earnings.extend(date_results)

            if not all_earnings:
                return NASDAQError("earnings_calendar", "No earnings data found").to_dict()

            # Sort by report date
            sorted_earnings = sorted(all_earnings, key=lambda x: x.get("date", ""), reverse=True)

            # Clean and format data
            formatted_results = []
            for earnings in sorted_earnings:
                formatted_earnings = {
                    "symbol": earnings.get("symbol"),
                    "company_name": earnings.get("name"),
                    "report_date": earnings.get("date"),
                    "eps_previous": self._parse_float(earnings.get("lastYearEPS")),
                    "eps_consensus": self._parse_float(earnings.get("epsForecast")),
                    "eps_actual": self._parse_float(earnings.get("eps")),
                    "surprise_percent": self._parse_float(earnings.get("surprise")),
                    "num_estimates": self._parse_int(earnings.get("noOfEsts")),
                    "period_ending": self._parse_period_ending(earnings.get("fiscalQuarterEnding")),
                    "previous_report_date": self._parse_date(earnings.get("lastYearRptDt")),
                    "reporting_time": earnings.get("time", "").replace("time-", "") if earnings.get("time") else None,
                    "market_cap": self._parse_int(earnings.get("marketCap"))
                }
                formatted_results.append(formatted_earnings)

            return {
                "success": True,
                "data": {
                    "date_range": {
                        "start": start.strftime("%Y-%m-%d"),
                        "end": end.strftime("%Y-%m-%d")
                    },
                    "earnings": formatted_results,
                    "total_count": len(formatted_results)
                }
            }

        except Exception as e:
            return NASDAQError("earnings_calendar", str(e)).to_dict()

    async def get_ipo_calendar(self, status: str = "priced", is_spo: bool = False,
                               start_date: Optional[str] = None,
                               end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get IPO calendar

        Args:
            status: IPO status (upcoming, priced, filed, withdrawn)
            is_spo: Whether to include secondary public offerings
            start_date: Start date in YYYY-MM-DD format
            end_date: End date in YYYY-MM-DD format

        Returns:
            Dict containing IPO calendar data
        """
        try:
            if status not in IPO_STATUS_CHOICES:
                return NASDAQError("ipo_calendar", f"Invalid status. Choose from: {', '.join(IPO_STATUS_CHOICES)}").to_dict()

            # Set default dates
            now = datetime.now()
            start = datetime.strptime(start_date, "%Y-%m-%d").date() if start_date else now - timedelta(days=300)
            end = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else now.date()

            # Generate month range for IPO calendar
            months = set()
            current = start
            while current <= end:
                months.add(current.strftime("%Y-%m"))
                current = current.replace(day=1) + timedelta(days=32)
                current = current.replace(day=1)

            # Fetch data for each month
            all_ipos = []
            headers = {
                'User-Agent': self._get_random_user_agent(),
                'Accept': 'application/json, text/plain, */*',
                'Accept-Encoding': 'gzip',
                'Accept-Language': 'en-CA,en-US;q=0.7,en;q=0.3',
                'Host': 'api.nasdaq.com',
                'Connection': 'keep-alive'
            }

            async def fetch_ipos_for_month(month_str):
                url_base = f"{NASDAQ_BASE_URL}/ipo/calendar?date={month_str}"
                if is_spo:
                    url_base += "&type=spo"

                result = await self._make_async_request(url_base, headers)

                if "error" not in result:
                    data = result.get("data", {})
                    if status in data:
                        if status == "upcoming":
                            return data["upcoming"]["upcomingTable"]["rows"]
                        else:
                            return data[status]["rows"]
                return []

            # Fetch all months concurrently
            tasks = [fetch_ipos_for_month(month) for month in sorted(months)]
            results = await asyncio.gather(*tasks)

            # Flatten results
            for month_results in results:
                all_ipos.extend(month_results)

            if not all_ipos:
                return NASDAQError("ipo_calendar", f"No IPO data found for status: {status}").to_dict()

            # Sort by date based on status
            if status == "priced":
                sorted_ipos = sorted(all_ipos,
                                   key=lambda x: self._parse_date(x.get("pricedDate", "")))
            elif status == "withdrawn":
                sorted_ipos = sorted(all_ipos,
                                   key=lambda x: self._parse_date(x.get("withdrawDate", "")))
            elif status == "filed":
                sorted_ipos = sorted(all_ipos,
                                   key=lambda x: self._parse_date(x.get("filedDate", "")))
            else:  # upcoming
                sorted_ipos = all_ipos

            # Clean and format data
            formatted_results = []
            for ipo in sorted_ipos:
                formatted_ipo = {
                    "symbol": ipo.get("proposedTickerSymbol"),
                    "company_name": ipo.get("companyName"),
                    "ipo_date": self._parse_date(ipo.get("pricedDate")),
                    "share_price": self._parse_float(ipo.get("proposedSharePrice")),
                    "exchange": ipo.get("proposedExchange"),
                    "offer_amount": self._parse_float(ipo.get("dollarValueOfSharesOffered")),
                    "share_count": self._parse_int(ipo.get("sharesOffered")),
                    "expected_price_date": self._parse_date(ipo.get("expectedPriceDate")),
                    "filed_date": self._parse_date(ipo.get("filedDate")),
                    "withdraw_date": self._parse_date(ipo.get("withdrawDate")),
                    "deal_status": ipo.get("dealStatus"),
                    "deal_id": ipo.get("dealID")
                }
                formatted_results.append(formatted_ipo)

            return {
                "success": True,
                "data": {
                    "status": status,
                    "is_spo": is_spo,
                    "date_range": {
                        "start": start.strftime("%Y-%m-%d"),
                        "end": end.strftime("%Y-%m-%d")
                    },
                    "ipos": formatted_results,
                    "total_count": len(formatted_results)
                }
            }

        except Exception as e:
            return NASDAQError("ipo_calendar", str(e)).to_dict()

    async def get_economic_calendar(self, start_date: Optional[str] = None,
                                    end_date: Optional[str] = None,
                                    country: Optional[str] = None) -> Dict[str, Any]:
        """Get economic calendar

        Args:
            start_date: Start date in YYYY-MM-DD format
            end_date: End date in YYYY-MM-DD format
            country: Country filter (comma-separated for multiple)

        Returns:
            Dict containing economic calendar data
        """
        try:
            # Set default dates (exclude weekends)
            now = datetime.now().date()
            start = datetime.strptime(start_date, "%Y-%m-%d").date() if start_date else now - timedelta(days=2)
            end = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else now + timedelta(days=3)

            # Generate date range (weekdays only)
            date_list = []
            current = start
            while current <= end:
                if current.weekday() < 5:  # Monday to Friday
                    date_list.append(current.strftime("%Y-%m-%d"))
                current += timedelta(days=1)

            # Fetch data for each date
            all_events = []
            headers = {
                'User-Agent': self._get_random_user_agent(),
                'Accept': 'application/json, text/plain, */*',
                'Accept-Encoding': 'gzip',
                'Accept-Language': 'en-CA,en-US;q=0.7,en;q=0.3',
                'Host': 'api.nasdaq.com',
                'Connection': 'keep-alive'
            }

            async def fetch_events_for_date(date_str):
                url = f"{NASDAQ_BASE_URL}/calendar/economicevents?date={date_str}"
                result = await self._make_async_request(url, headers)

                if "error" not in result:
                    data = result.get("data", {})
                    rows = data.get("rows", [])

                    # Process each event
                    processed_events = []
                    for event in rows:
                        # Format date/time
                        gmt = event.get("gmt", "")
                        if gmt == "All Day":
                            datetime_str = f"{date_str} 00:00"
                        else:
                            clean_gmt = gmt.replace("Tentative", "00:00").replace("24H", "00:00")
                            datetime_str = f"{date_str} {clean_gmt}"

                        event["date"] = datetime_str
                        event.pop("gmt", None)  # Remove original gmt field

                        # Clean actual, previous, consensus fields
                        for field in ["actual", "previous", "consensus"]:
                            if event.get(field):
                                event[field] = event[field].replace("&nbsp;", "-")

                        # Clean description
                        if event.get("description"):
                            event["description"] = self._clean_html_text(event["description"])

                        processed_events.append(event)

                    return processed_events
                return []

            # Fetch all dates concurrently
            tasks = [fetch_events_for_date(date_str) for date_str in date_list]
            results = await asyncio.gather(*tasks)

            # Flatten results
            for date_results in results:
                all_events.extend(date_results)

            if not all_events:
                return NASDAQError("economic_calendar", "No economic events found").to_dict()

            # Filter by country if specified
            if country:
                country_list = [c.strip().lower().replace(" ", "_") for c in country.split(",")]
                all_events = [
                    event for event in all_events
                    if event.get("country", "").lower().replace(" ", "_") in country_list
                ]

            # Sort by date
            sorted_events = sorted(all_events, key=lambda x: x.get("date", ""))

            return {
                "success": True,
                "data": {
                    "date_range": {
                        "start": start.strftime("%Y-%m-%d"),
                        "end": end.strftime("%Y-%m-%d")
                    },
                    "country_filter": country,
                    "events": sorted_events,
                    "total_count": len(sorted_events)
                }
            }

        except Exception as e:
            return NASDAQError("economic_calendar", str(e)).to_dict()

    async def get_top_retail_activity(self, limit: int = 10) -> Dict[str, Any]:
        """Get top retail activity

        Args:
            limit: Maximum number of results

        Returns:
            Dict containing top retail activity data
        """
        try:
            if not self.api_key:
                return NASDAQError("top_retail", "NASDAQ API key required for retail activity data").to_dict()

            url = f"{NASDAQ_RTAT_URL}?api_key={self.api_key}"

            result = self._make_request(url)

            if "error" in result:
                return result

            data = result.get("data", {})
            if "datatable" not in data or "data" not in data["datatable"]:
                return NASDAQError("top_retail", "Invalid response format").to_dict()

            retail_data = data["datatable"]["data"]
            if not retail_data:
                return NASDAQError("top_retail", "No retail activity data found").to_dict()

            # Format results
            formatted_results = []
            for row in retail_data[:limit]:
                formatted_result = {
                    "date": self._parse_date(row[0]),
                    "symbol": row[1],
                    "activity": row[2],
                    "sentiment": row[3]
                }
                formatted_results.append(formatted_result)

            return {
                "success": True,
                "data": {
                    "retail_activity": formatted_results,
                    "total_count": len(formatted_results),
                    "limit": limit
                }
            }

        except Exception as e:
            return NASDAQError("top_retail", str(e)).to_dict()

    async def get_comprehensive_market_overview(self) -> Dict[str, Any]:
        """Get comprehensive market overview with multiple data sources"""
        try:
            results = {}

            # Get equity screener with top performers
            screener_result = await self.get_equity_screener(limit=50)
            results["top_performers"] = screener_result

            # Get upcoming dividends
            dividends_result = await self.get_dividend_calendar(
                start_date=datetime.now().strftime("%Y-%m-%d"),
                end_date=(datetime.now() + timedelta(days=7)).strftime("%Y-%m-%d")
            )
            results["upcoming_dividends"] = dividends_result

            # Get upcoming earnings
            earnings_result = await self.get_earnings_calendar(
                start_date=datetime.now().strftime("%Y-%m-%d"),
                end_date=(datetime.now() + timedelta(days=7)).strftime("%Y-%m-%d")
            )
            results["upcoming_earnings"] = earnings_result

            # Get upcoming IPOs
            ipo_result = await self.get_ipo_calendar(status="upcoming")
            results["upcoming_ipos"] = ipo_result

            # Get recent IPOs
            recent_ipo_result = await self.get_ipo_calendar(
                status="priced",
                start_date=(datetime.now() - timedelta(days=30)).strftime("%Y-%m-%d"),
                end_date=datetime.now().strftime("%Y-%m-%d")
            )
            results["recent_ipos"] = recent_ipo_result

            # Get economic events for today
            economic_result = await self.get_economic_calendar(
                start_date=datetime.now().strftime("%Y-%m-%d"),
                end_date=datetime.now().strftime("%Y-%m-%d")
            )
            results["today_economic_events"] = economic_result

            # Get top retail activity if API key available
            if self.api_key:
                retail_result = await self.get_top_retail_activity(limit=20)
                results["top_retail_activity"] = retail_result
            else:
                results["top_retail_activity"] = {
                    "success": False,
                    "message": "NASDAQ API key required for retail activity data"
                }

            return {
                "success": True,
                "data": {
                    "overview": results,
                    "generated_at": datetime.now().isoformat(),
                    "data_sources": [
                        "Equity Screener", "Dividend Calendar", "Earnings Calendar",
                        "IPO Calendar", "Economic Calendar", "Top Retail Activity"
                    ]
                }
            }

        except Exception as e:
            return NASDAQError("market_overview", str(e)).to_dict()

    # Helper methods
    def _parse_date(self, date_str: str) -> Optional[str]:
        """Parse date string and return ISO format"""
        if not date_str or date_str == "N/A":
            return None
        try:
            # Try different date formats
            for fmt in ["%m/%d/%Y", "%Y-%m-%d"]:
                try:
                    parsed_date = datetime.strptime(date_str, fmt)
                    return parsed_date.strftime("%Y-%m-%d")
                except ValueError:
                    continue
            return date_str  # Return original if can't parse
        except:
            return date_str

    def _parse_float(self, value: Any) -> Optional[float]:
        """Parse value as float"""
        if not value or value == "N/A":
            return None
        try:
            if isinstance(value, str):
                clean_value = value.replace("$", "").replace(",", "").replace("(", "-").replace(")", "")
                return float(clean_value) if clean_value else None
            return float(value)
        except:
            return None

    def _parse_int(self, value: Any) -> Optional[int]:
        """Parse value as integer"""
        if not value or value == "N/A":
            return None
        try:
            if isinstance(value, str):
                clean_value = value.replace(",", "")
                return int(clean_value) if clean_value else None
            return int(value)
        except:
            return None

    def _parse_period_ending(self, period_str: str) -> Optional[str]:
        """Parse fiscal quarter ending period"""
        if not period_str or period_str == "N/A":
            return None
        try:
            # Convert "Jan/2024" to "2024-01"
            parsed_date = datetime.strptime(period_str, "%b/%Y")
            return parsed_date.strftime("%Y-%m")
        except:
            return period_str


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps(NASDAQError("cli", "Usage: python nasdaq_data.py <command> [args...]").to_dict()))
        sys.exit(1)

    command = sys.argv[1]

    # Get API key from environment
    api_key = os.getenv("NASDAQ_API_KEY")

    # Create API instance
    api = NASDAQDataAPI(api_key=api_key)

    # Map commands to async methods
    async def run_command():
        if command == "search_equities":
            query = sys.argv[2] if len(sys.argv) > 2 else ""
            is_etf_arg = sys.argv[3] if len(sys.argv) > 3 else None
            is_etf = None
            if is_etf_arg is not None:
                is_etf = is_etf_arg.lower() == "true"
            return await api.search_equities(query, is_etf)

        elif command == "equity_screener":
            exchange = sys.argv[2] if len(sys.argv) > 2 else "all"
            market_cap = sys.argv[3] if len(sys.argv) > 3 else "all"
            sector = sys.argv[4] if len(sys.argv) > 4 else "all"
            country = sys.argv[5] if len(sys.argv) > 5 else "all"
            limit = int(sys.argv[6]) if len(sys.argv) > 6 and sys.argv[6].isdigit() else None
            return await api.get_equity_screener(exchange, market_cap, sector, country, limit)

        elif command == "dividend_calendar":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            return await api.get_dividend_calendar(start_date, end_date)

        elif command == "earnings_calendar":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            return await api.get_earnings_calendar(start_date, end_date)

        elif command == "ipo_calendar":
            status = sys.argv[2] if len(sys.argv) > 2 else "priced"
            is_spo = sys.argv[3].lower() == "true" if len(sys.argv) > 3 else False
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None
            return await api.get_ipo_calendar(status, is_spo, start_date, end_date)

        elif command == "economic_calendar":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            country = sys.argv[4] if len(sys.argv) > 4 else None
            return await api.get_economic_calendar(start_date, end_date, country)

        elif command == "top_retail":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2].isdigit() else 10
            return await api.get_top_retail_activity(limit)

        elif command == "market_overview":
            return await api.get_comprehensive_market_overview()

        else:
            return NASDAQError("cli", f"Unknown command: {command}").to_dict()

    # Run the async command
    result = asyncio.run(run_command())
    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()