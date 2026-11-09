# BLS (Bureau of Labor Statistics) Data Wrapper
# Based on OpenBB BLS provider - https://github.com/OpenBB-finance/OpenBB/tree/main/openbb_platform/providers/bls

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

import fincept_json  # scripts/fincept_json.py: NaN / Infinity -> null at the output boundary

# Windows DNS fix: aiohttp's default resolver (aiodns/pycares) cannot read the
# system DNS configuration on Windows and fails every request with
# "Timeout while contacting DNS servers". Use the threaded getaddrinfo resolver
# (same fix as scripts/exchange/ws_stream.py). Gated to Windows so Linux/macOS
# keep the native resolver.
if sys.platform == "win32":
    import aiohttp.connector as _aiohttp_connector
    _aiohttp_connector.DefaultResolver = aiohttp.ThreadedResolver

# BLS API Configuration
BLS_API_URL = "https://api.bls.gov/publicAPI/v2/timeseries/data/"
BLS_FTP_BASE = "https://download.bls.gov/pub/time.series/"
# download.bls.gov refuses anonymous user agents; BLS asks for a contact address in it.
BLS_FILE_HEADERS = {"User-Agent": "Fincept-Terminal (support@fincept.in)"}
SEARCH_RESULT_LIMIT = 500

# Survey Categories (from OpenBB constants)
SURVEY_CATEGORIES = {
    "cpi": ["ap", "cu", "cw", "li", "su", "ei"],
    "pce": ["cx"],
    "ppi": ["wp", "pc"],
    "ip": ["ip", "pr", "mp"],
    "jolts": ["jl", "jt"],
    "nfp": ["ce"],
    "cps": ["le", "lu"],
    "lfs": ["ln", "fm", "in", "ws"],
    "wages": ["ci", "wm"],
    "ec": ["cm", "cc"],
    "sla": ["la", "sm"],
    "bed": ["bd"],
    "tu": ["tu"]
}

SURVEY_CATEGORY_NAMES = {
    "cpi": "Consumer Price Index",
    "pce": "Personal Consumption Expenditure",
    "ppi": "Producer Price Index",
    "ip": "Industry Productivity",
    "jolts": "Job Openings and Labor Turnover Survey",
    "nfp": "Nonfarm Payrolls",
    "cps": "Current Population Survey",
    "lfs": "Labor Force Statistics",
    "wages": "Wages",
    "ec": "Employer Costs",
    "sla": "State and Local Area Employment",
    "bed": "Business Employment Dynamics",
    "tu": "Time Use",
}

# Popular Series IDs for quick access. These must be BLS series ids: FRED mnemonics (UNRATE,
# PAYEMS, CPIAUCSL, CIVPART, ...) are not BLS series and the BLS API answers them with
# "Invalid Series".
POPULAR_SERIES = {
    "CUSR0000SA0": "CPI-U: All items, U.S. city average (SA)",
    "CUUR0000SA0": "CPI-U: All items, U.S. city average (NSA)",
    "CES0000000001": "All employees: Total nonfarm (SA, thousands)",
    "CES0500000001": "All employees: Total private (SA, thousands)",
    "LNS14000000": "Unemployment rate (SA)",
    "LNS13000000": "Unemployment level (SA, thousands)",
    "LNS11300000": "Labor force participation rate (SA)",
    "LNS12300000": "Employment-population ratio (SA)",
}

# Series behind the overview commands, by the key each group is returned under.
LABOR_OVERVIEW_SERIES = {
    "unemployment_rate": ["LNS14000000"],            # Unemployment rate, SA, percent
    "nonfarm_payrolls": ["CES0000000001"],           # All employees, total nonfarm, SA, thousands
    "labor_force_participation": ["LNS11300000"],    # Labor force participation rate, SA, percent
    "employment_population_ratio": ["LNS12300000"],  # Employment-population ratio, SA, percent
}

INFLATION_OVERVIEW_SERIES = {  # CPI-U, U.S. city average, index 1982-84=100
    "cpi_all_items": ["CUSR0000SA0", "CUUR0000SA0"],  # All items, SA and NSA
    "core_cpi": ["CUSR0000SA0L1E"],                   # All items less food and energy, SA
    "energy_inflation": ["CUSR0000SA0E"],             # Energy, SA
    "food_inflation": ["CUSR0000SAF1"],               # Food, SA
    "core_goods": ["CUSR0000SACL1E"],                 # Commodities less food and energy commodities, SA
    "core_services": ["CUSR0000SASLE"],               # Services less energy services, SA
}

# Employment Cost Index, 12-month percent change, not seasonally adjusted (BLS publishes the
# seasonally adjusted ECI only as 3-month changes).
ECI_SERIES = [
    "CIU2010000000000A",  # Total compensation, private industry
    "CIU2020000000000A",  # Wages and salaries, private industry
    "CIU2030000000000A",  # Total benefits, private industry
    "CIU3010000000000A",  # Total compensation, state and local government
    "CIU3020000000000A",  # Wages and salaries, state and local government
    "CIU3030000000000A",  # Total benefits, state and local government
]

PRODUCTIVITY_SERIES = {  # SA, percent change from previous quarter at annual rate
    "labor_productivity": ["PRS85006092", "PRS30006092"],  # Nonfarm business, manufacturing
    "unit_labor_costs": ["PRS85006112", "PRS30006112"],    # Nonfarm business, manufacturing
    "compensation": ["PRS85006102", "PRS85006152"],        # Nonfarm business: hourly, real hourly
}


def _bls_period_date(year: str, period: str):
    """BLS period code -> (ISO date of the period's first day, is_annual_average).

    M01-M12 monthly (M13 = annual average), Q01-Q04 quarterly (Q05 = annual average),
    S01-S02 semiannual (S03 = annual average), A01 annual.
    """
    code = period[:1]
    try:
        num = int(period[1:])
    except ValueError:
        return f"{year}-01-01", False
    if (code, num) in (("M", 13), ("Q", 5), ("S", 3)):
        return f"{year}-12-31", True
    if code == "M":
        return f"{year}-{num:02d}-01", False
    if code == "Q":
        return f"{year}-{(num - 1) * 3 + 1:02d}-01", False
    if code == "S":
        return f"{year}-{(num - 1) * 6 + 1:02d}-01", False
    return f"{year}-01-01", False


def _group_series_rows(series_data: List[Dict[str, Any]], groups: Dict[str, List[str]]) -> Dict[str, List[Dict[str, Any]]]:
    """Split processed rows into the named groups by exact series id."""
    by_id = {sid: name for name, ids in groups.items() for sid in ids}
    grouped: Dict[str, List[Dict[str, Any]]] = {name: [] for name in groups}
    for record in series_data:
        name = by_id.get(record.get("symbol"))
        if name:
            grouped[name].append(record)
    return grouped


class BLSError:
    """Custom error class for BLS API errors"""
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


class BLSDataAPI:
    """BLS Data API wrapper for modular data fetching"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.getenv("BLS_API_KEY")
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0',
            'Content-Type': 'application/json'
        })

        # Cache for series data (24-hour cache)
        self._cache_timeout = 24 * 60 * 60  # 24 hours
        self._cache = {}

    def _is_cache_valid(self, cache_key: str) -> bool:
        """Check if cached data is still valid"""
        if cache_key not in self._cache:
            return False

        cached_time = self._cache[cache_key].get("timestamp", 0)
        current_time = datetime.now().timestamp()
        return (current_time - cached_time) < self._cache_timeout

    def _get_cached_data(self, cache_key: str) -> Optional[pd.DataFrame]:
        """Get cached data if valid"""
        if self._is_cache_valid(cache_key):
            return self._cache[cache_key].get("data")
        return None

    def _set_cache_data(self, cache_key: str, data: pd.DataFrame) -> None:
        """Set cached data with timestamp"""
        self._cache[cache_key] = {
            "data": data,
            "timestamp": datetime.now().timestamp()
        }

    async def _make_async_request(self, url: str, method: str = "GET",
                                  headers: Optional[Dict] = None,
                                  data: Optional[str] = None) -> Dict[str, Any]:
        """Make async HTTP request with error handling"""
        try:
            async with aiohttp.ClientSession(headers=headers) as session:
                if method.upper() == "GET":
                    async with session.get(url) as response:
                        if response.status == 200:
                            result = await response.json()
                            return {"success": True, "data": result}
                        else:
                            return BLSError(url, f"HTTP {response.status}: {await response.text()}", response.status).to_dict()
                elif method.upper() == "POST":
                    async with session.post(url, data=data) as response:
                        if response.status == 200:
                            result = await response.json()
                            return {"success": True, "data": result}
                        else:
                            return BLSError(url, f"HTTP {response.status}: {await response.text()}", response.status).to_dict()

        except aiohttp.ClientError as e:
            return BLSError(url, f"Network error: {str(e)}").to_dict()
        except json.JSONDecodeError as e:
            return BLSError(url, f"JSON decode error: {str(e)}").to_dict()
        except Exception as e:
            return BLSError(url, f"Unexpected error: {str(e)}").to_dict()

    def _make_request(self, url: str, method: str = "GET",
                     headers: Optional[Dict] = None,
                     data: Optional[str] = None) -> Dict[str, Any]:
        """Make HTTP request with error handling"""
        try:
            if method.upper() == "GET":
                response = self.session.get(url, headers=headers, timeout=30)
            elif method.upper() == "POST":
                response = self.session.post(url, headers=headers, data=data, timeout=30)
            else:
                return BLSError(url, f"Unsupported method: {method}").to_dict()

            if response.status_code == 200:
                try:
                    data = response.json()
                    return {"success": True, "data": data}
                except json.JSONDecodeError:
                    return BLSError(url, "Invalid JSON response", response.status_code).to_dict()
            else:
                return BLSError(url, f"HTTP {response.status_code}: {response.text}", response.status_code).to_dict()

        except requests.exceptions.RequestException as e:
            return BLSError(url, f"Network error: {str(e)}", getattr(e.response, 'status_code', None)).to_dict()
        except Exception as e:
            return BLSError(url, f"Unexpected error: {str(e)}").to_dict()

    def _parse_ftp_data(self, content: str) -> pd.DataFrame:
        """Parse tab-delimited FTP data"""
        try:
            df = pd.read_csv(StringIO(content), sep="\t", low_memory=False, dtype="object")
            df.columns = [col.strip() for col in df.columns]
            df = df.apply(lambda x: x.str.strip() if x.dtype == "object" else x)

            # Clean up empty values
            df = df.replace({"''": None, '""': None, "nan": None, "": None})
            df = df.dropna(how="all", axis=1)

            return df
        except Exception as e:
            raise ValueError(f"Failed to parse FTP data: {str(e)}")

    async def get_bls_timeseries(self, series_ids: Union[str, List[str]],
                                start_year: Optional[int] = None,
                                end_year: Optional[int] = None,
                                calculations: bool = True,
                                catalog: bool = True,
                                annual_average: bool = False,
                                aspects: bool = False) -> Dict[str, Any]:
        """Get BLS timeseries data with API request chunking"""
        if not self.api_key:
            return BLSError("bls_timeseries", "BLS API key required. Get one at: https://data.bls.gov/registrationEngine/").to_dict()

        # Convert single series to list
        symbols = series_ids.split(",") if isinstance(series_ids, str) else series_ids

        # Limit to 50 symbols per request
        if len(symbols) > 50:
            symbols = symbols[:50]

        # Default date range
        current_year = datetime.now().year
        if not start_year:
            start_year = current_year - 3  # Default to 3 years of data
        if not end_year:
            end_year = current_year

        # Prepare request payload
        payload = {
            "seriesid": symbols,
            "startyear": start_year,
            "endyear": end_year,
            "catalog": catalog,
            "calculations": calculations,
            "annualaverage": annual_average,
            "aspects": aspects,
            "registrationkey": self.api_key
        }

        # Remove None values
        payload = {k: v for k, v in payload.items() if v}

        headers = {"Content-Type": "application/json"}
        payload_json = json.dumps(payload)

        # Make request
        result = await self._make_async_request(BLS_API_URL, "POST", headers, payload_json)

        if "error" in result:
            return result

        # Parse BLS response
        bls_data = result.get("data", {})
        if not bls_data or "Results" not in bls_data:
            return BLSError("bls_timeseries", "Invalid BLS response format").to_dict()

        results = bls_data.get("Results", {})
        series_data = results.get("series", [])
        messages = bls_data.get("message", [])

        # Process data
        processed_data = []
        metadata = {}

        for series in series_data:
            series_id = series.get("seriesID")
            if not series_id:
                continue

            # Store catalog metadata
            catalog_info = series.get("catalog")
            if catalog_info:
                metadata[series_id] = catalog_info

            # Process data points
            data_points = series.get("data", [])
            for point in data_points:
                year = str(point.get("year", ""))
                period = str(point.get("period", ""))
                date_str, annual_average_row = _bls_period_date(year, period)

                # Parse value
                value = point.get("value")
                if value and value != "-":
                    try:
                        numeric_value = float(value)
                    except ValueError:
                        numeric_value = None
                else:
                    numeric_value = None

                # Create data record. year + period are BLS's own period keys (M01..M13,
                # Q01..Q05, S01..S03, A01); date is the first day of that period.
                record = {
                    "symbol": series_id,
                    "date": date_str,
                    "year": year,
                    "period": period,
                    "value": numeric_value,
                    "latest": point.get("latest") == "true"
                }
                if annual_average_row:
                    record["annual_average"] = True

                # Add title if available
                if catalog_info and "series_title" in catalog_info:
                    title = catalog_info["series_title"]
                    if annual_average_row:
                        title += " (Annual Average)"
                    record["title"] = title

                # Add footnotes
                footnotes = point.get("footnotes", [])
                if footnotes:
                    footnote_text = "; ".join([
                        f.get("text", str(f)) if isinstance(f, dict) else str(f)
                        for f in footnotes if f
                    ])
                    if footnote_text.strip():
                        record["footnotes"] = footnote_text

                # Add calculations
                calculations_data = point.get("calculations", {})
                if calculations_data:
                    # Net changes
                    net_changes = calculations_data.get("net_changes", {})
                    record.update({
                        "change_1M": float(net_changes.get("1")) if net_changes.get("1") else None,
                        "change_3M": float(net_changes.get("3")) if net_changes.get("3") else None,
                        "change_6M": float(net_changes.get("6")) if net_changes.get("6") else None,
                        "change_12M": float(net_changes.get("12")) if net_changes.get("12") else None,
                    })

                    # Percentage changes
                    pct_changes = calculations_data.get("pct_changes", {})
                    record.update({
                        "change_percent_1M": float(pct_changes.get("1")) / 100 if pct_changes.get("1") else None,
                        "change_percent_3M": float(pct_changes.get("3")) / 100 if pct_changes.get("3") else None,
                        "change_percent_6M": float(pct_changes.get("6")) / 100 if pct_changes.get("6") else None,
                        "change_percent_12M": float(pct_changes.get("12")) / 100 if pct_changes.get("12") else None,
                    })

                processed_data.append(record)

        if not processed_data:
            error_msg = "; ".join(messages) if messages else "No data found"
            return BLSError("bls_timeseries", error_msg).to_dict()

        return {
            "success": True,
            "data": {
                "series_data": processed_data,
                "metadata": metadata,
                "messages": messages
            }
        }

    def search_bls_series(self, query: str, category: str = "cpi",
                         include_extras: bool = False,
                         include_code_map: bool = False) -> Dict[str, Any]:
        """Search for BLS series by category and query"""
        try:
            if category not in SURVEY_CATEGORIES:
                available_categories = ", ".join(SURVEY_CATEGORY_NAMES.keys())
                return BLSError("bls_search", f"Invalid category. Available: {available_categories}").to_dict()

            # Use cached data if available
            cache_key = f"{category}_series"
            series_df = self._get_cached_data(cache_key)

            if series_df is None:
                # Try to get from local cache or download
                series_df = self._get_local_category_data(category)
                if series_df is None:
                    return BLSError("bls_search", f"Series data for category '{category}' not available locally").to_dict()

                self._set_cache_data(cache_key, series_df)

            if series_df.empty:
                return BLSError("bls_search", f"No series data found for category '{category}'").to_dict()

            # Search for matching series
            if not query.strip():
                # Return all series in category
                results_df = series_df
            else:
                # Search within series title and ID
                search_terms = [term.strip() for term in query.split(";")]
                combined_mask = pd.Series([True] * len(series_df))

                for term in search_terms:
                    term_mask = series_df.apply(
                        lambda row, term=term: row.astype(str).str.contains(
                            term, case=False, regex=False, na=False
                        )
                    ).any(axis=1)
                    combined_mask &= term_mask

                results_df = series_df[combined_mask]

            if results_df.empty:
                return BLSError("bls_search", f"No results found for query: '{query}' in category '{category}'").to_dict()

            # Format results (empty catalogue cells are NaN here; fincept_json writes them as null)
            total_matches = len(results_df)
            results_df = results_df.head(SEARCH_RESULT_LIMIT)
            if include_extras:
                # Return all columns
                results = results_df.to_dict("records")
            else:
                # Return only essential columns
                essential_cols = ["series_id", "series_title", "survey_name"]
                available_cols = [col for col in essential_cols if col in results_df.columns]
                results = results_df[available_cols].to_dict("records")

            # Add code map if requested
            metadata = {}
            if include_code_map:
                metadata = self._get_local_code_map(category)

            return {
                "success": True,
                "data": {
                    "query": query,
                    "category": category,
                    "total_matches": total_matches,
                    "truncated": total_matches > len(results),
                    "results": results,
                    "metadata": metadata if metadata else None
                }
            }

        except Exception as e:
            return BLSError("bls_search", str(e)).to_dict()

    def _get_local_category_data(self, category: str) -> Optional[pd.DataFrame]:
        """Series catalogue of a category: BLS's own <survey>.series files (one per survey code).

        Used to return three hard-coded "sample" rows, two of them FRED ids (CPIAUCSL, CPIAUCNS)
        that the BLS API rejects.
        """
        frames = []
        for survey in SURVEY_CATEGORIES.get(category, []):
            url = f"{BLS_FTP_BASE}{survey}/{survey}.series"
            try:
                resp = self.session.get(url, headers=BLS_FILE_HEADERS, timeout=120)
            except requests.exceptions.RequestException:
                continue
            if resp.status_code != 200:
                continue
            df = self._parse_ftp_data(resp.text)
            if "series_id" not in df.columns:
                continue
            df["survey_name"] = SURVEY_CATEGORY_NAMES.get(category, category)
            df["survey_code"] = survey
            frames.append(df)
        if not frames:
            return None
        return pd.concat(frames, ignore_index=True)

    def _get_local_code_map(self, category: str) -> Dict:
        """Get code map for category (simplified version)"""
        # In a real implementation, this would read from local cached files
        return {}

    async def get_series_data(self, series_ids: Union[str, List[str]],
                            start_date: Optional[str] = None,
                            end_date: Optional[str] = None,
                            calculations: bool = True,
                            annual_average: bool = False,
                            aspects: bool = False) -> Dict[str, Any]:
        """Get time series data for specific series IDs"""
        try:
            # Convert date strings to years if provided
            start_year = None
            end_year = None

            if start_date:
                try:
                    start_dt = datetime.strptime(start_date, "%Y-%m-%d")
                    start_year = start_dt.year
                except ValueError:
                    return BLSError("get_series_data", f"Invalid start_date format: {start_date}. Use YYYY-MM-DD").to_dict()

            if end_date:
                try:
                    end_dt = datetime.strptime(end_date, "%Y-%m-%d")
                    end_year = end_dt.year
                except ValueError:
                    return BLSError("get_series_data", f"Invalid end_date format: {end_date}. Use YYYY-MM-DD").to_dict()

            return await self.get_bls_timeseries(
                series_ids=series_ids,
                start_year=start_year,
                end_year=end_year,
                calculations=calculations,
                catalog=True,
                annual_average=annual_average,
                aspects=aspects
            )

        except Exception as e:
            return BLSError("get_series_data", str(e)).to_dict()

    async def get_popular_series(self) -> Dict[str, Any]:
        """Get data for popular economic series"""
        try:
            popular_series_ids = list(POPULAR_SERIES.keys())

            result = await self.get_bls_timeseries(
                series_ids=popular_series_ids,
                start_year=datetime.now().year - 2,  # 2 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            # Add series descriptions
            data = result.get("data", {})
            series_data = data.get("series_data", [])

            # Add descriptions to each series
            for record in series_data:
                series_id = record.get("symbol")
                if series_id in POPULAR_SERIES:
                    record["description"] = POPULAR_SERIES[series_id]

            return {
                "success": True,
                "data": {
                    "popular_series": series_data,
                    "available_series": POPULAR_SERIES
                }
            }

        except Exception as e:
            return BLSError("get_popular_series", str(e)).to_dict()

    async def get_labor_market_overview(self) -> Dict[str, Any]:
        """Get comprehensive labor market overview"""
        try:
            result = await self.get_bls_timeseries(
                series_ids=[sid for ids in LABOR_OVERVIEW_SERIES.values() for sid in ids],
                start_year=datetime.now().year - 3,  # 3 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            data = result.get("data", {})
            series_data = data.get("series_data", [])

            return {
                "success": True,
                "data": {
                    **_group_series_rows(series_data, LABOR_OVERVIEW_SERIES),
                    "summary": {
                        "total_data_points": len(series_data),
                        "date_range": {
                            "start": min([s.get("date") for s in series_data]) if series_data else None,
                            "end": max([s.get("date") for s in series_data]) if series_data else None
                        }
                    }
                }
            }

        except Exception as e:
            return BLSError("get_labor_market_overview", str(e)).to_dict()

    async def get_inflation_overview(self) -> Dict[str, Any]:
        """Get comprehensive inflation overview"""
        try:
            result = await self.get_bls_timeseries(
                series_ids=[sid for ids in INFLATION_OVERVIEW_SERIES.values() for sid in ids],
                start_year=datetime.now().year - 3,  # 3 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            data = result.get("data", {})
            series_data = data.get("series_data", [])

            return {
                "success": True,
                "data": {
                    **_group_series_rows(series_data, INFLATION_OVERVIEW_SERIES),
                    "summary": {
                        "total_data_points": len(series_data),
                        "date_range": {
                            "start": min([s.get("date") for s in series_data]) if series_data else None,
                            "end": max([s.get("date") for s in series_data]) if series_data else None
                        }
                    }
                }
            }

        except Exception as e:
            return BLSError("get_inflation_overview", str(e)).to_dict()

    async def get_employment_cost_index(self) -> Dict[str, Any]:
        """Get Employment Cost Index data"""
        try:
            result = await self.get_bls_timeseries(
                series_ids=ECI_SERIES,
                start_year=datetime.now().year - 5,  # 5 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            return {
                "success": True,
                "data": {
                    "employment_cost_index": result.get("data", {}).get("series_data", []),
                    "description": "Employment Cost Index measures changes in labor costs"
                }
            }

        except Exception as e:
            return BLSError("get_employment_cost_index", str(e)).to_dict()

    async def get_productivity_costs(self) -> Dict[str, Any]:
        """Get Productivity and Costs data"""
        try:
            result = await self.get_bls_timeseries(
                series_ids=[sid for ids in PRODUCTIVITY_SERIES.values() for sid in ids],
                start_year=datetime.now().year - 5,  # 5 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            data = result.get("data", {})
            series_data = data.get("series_data", [])

            return {
                "success": True,
                "data": {
                    **_group_series_rows(series_data, PRODUCTIVITY_SERIES),
                    "description": "Productivity and Costs measures business sector efficiency"
                }
            }

        except Exception as e:
            return BLSError("get_productivity_costs", str(e)).to_dict()

    def get_survey_categories(self) -> Dict[str, Any]:
        """Get available survey categories"""
        try:
            categories_info = []
            for key, name in SURVEY_CATEGORY_NAMES.items():
                categories_info.append({
                    "category_code": key,
                    "category_name": name,
                    "survey_codes": SURVEY_CATEGORIES[key]
                })

            return {
                "success": True,
                "data": {
                    "categories": categories_info,
                    "total_categories": len(categories_info)
                }
            }

        except Exception as e:
            return BLSError("get_survey_categories", str(e)).to_dict()


def main(args=None):
    
    if args is None:
        args = sys.argv[1:]
    """Main function for CLI interface"""
    if len(args) + 1 < 2:
        print(json.dumps(BLSError("cli", "Usage: python bls_data.py <command> [args...]").to_dict()))
        sys.exit(1)

    command = args[0]

    # Get API key from environment or command line
    api_key = os.getenv("BLS_API_KEY")

    # Create API instance
    api = BLSDataAPI(api_key=api_key)

    # Map commands to async methods
    async def run_command():
        if command == "search_series":
            query = args[1] if len(args) + 1 > 2 else ""
            category = args[2] if len(args) + 1 > 3 else "cpi"
            include_extras = args[3].lower() == "true" if len(args) + 1 > 4 else False
            include_code_map = args[4].lower() == "true" if len(args) + 1 > 5 else False
            return api.search_bls_series(query, category, include_extras, include_code_map)

        elif command == "get_series":
            series_ids = args[1] if len(args) + 1 > 2 else ""
            start_date = args[2] if len(args) + 1 > 3 else None
            end_date = args[3] if len(args) + 1 > 4 else None
            calculations = args[4].lower() != "false" if len(args) + 1 > 5 else True
            annual_average = sys.argv[6].lower() == "true" if len(args) + 1 > 6 else False
            aspects = sys.argv[7].lower() == "true" if len(args) + 1 > 7 else False
            return await api.get_series_data(series_ids, start_date, end_date, calculations, annual_average, aspects)

        elif command == "get_popular":
            return await api.get_popular_series()

        elif command == "get_labor_overview":
            return await api.get_labor_market_overview()

        elif command == "get_inflation_overview":
            return await api.get_inflation_overview()

        elif command == "get_employment_cost_index":
            return await api.get_employment_cost_index()

        elif command == "get_productivity_costs":
            return await api.get_productivity_costs()

        elif command == "get_categories":
            return api.get_survey_categories()

        else:
            return BLSError("cli", f"Unknown command: {command}").to_dict()

    # Run the async command
    result = asyncio.run(run_command())
    fincept_json.emit(result)


if __name__ == "__main__":
    main()