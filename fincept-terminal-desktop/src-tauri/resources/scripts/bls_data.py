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

# BLS API Configuration
BLS_API_URL = "https://api.bls.gov/publicAPI/v2/timeseries/data/"
BLS_FTP_BASE = "https://download.bls.gov/pub/time.series/"

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

# Popular Series IDs for quick access
POPULAR_SERIES = {
    "CPIAUCSL": "Consumer Price Index for All Urban Consumers: All Items",
    "UNRATE": "Unemployment Rate",
    "PAYEMS": "All Employees: Total Nonfarm Payrolls",
    "CPIAUCNS": "Consumer Price Index for All Urban Consumers: All Items (NSA)",
    "USPRIV": "All Employees: Private",
    "CES0000000001": "All Employees: Total Nonfarm",
    "LNS14000000": "Unemployment Rate",
    "LNS13000000": "Labor Force Participation Rate",
    "LNS11300000": "Employment-Population Ratio",
    "CIVPART": "Labor Force Participation Rate",
    "EMRATIO": "Employment-Population Ratio"
}


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
                year = point.get("year", "")
                period = point.get("period", "").replace("M", "")

                # Parse date
                if period.startswith("A") or period in ("S01", "Q01"):
                    date_str = f"{year}-01-01"
                elif period == "S02":
                    date_str = f"{year}-07-01"
                elif period in ("S03", "Q05"):
                    date_str = f"{year}-12-31"
                    period = "13"
                elif period == "Q02":
                    date_str = f"{year}-04-01"
                elif period == "Q03":
                    date_str = f"{year}-07-01"
                elif period == "Q04":
                    date_str = f"{year}-10-01"
                else:
                    date_str = f"{year}-{period.zfill(2)}-01"

                # Parse value
                value = point.get("value")
                if value and value != "-":
                    try:
                        numeric_value = float(value)
                    except ValueError:
                        numeric_value = None
                else:
                    numeric_value = None

                # Create data record
                record = {
                    "symbol": series_id,
                    "date": date_str,
                    "value": numeric_value,
                    "latest": point.get("latest") == "true"
                }

                # Add title if available
                if catalog_info and "series_title" in catalog_info:
                    title = catalog_info["series_title"]
                    if period == "13":
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

            # Format results
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
                    "results": results,
                    "metadata": metadata if metadata else None
                }
            }

        except Exception as e:
            return BLSError("bls_search", str(e)).to_dict()

    def _get_local_category_data(self, category: str) -> Optional[pd.DataFrame]:
        """Get category series data from local cache (simplified version)"""
        # In a real implementation, this would read from local cached files
        # For now, return some sample data for popular categories
        if category == "cpi":
            data = [
                {"series_id": "CPIAUCSL", "series_title": "Consumer Price Index for All Urban Consumers: All Items", "survey_name": "Consumer Price Index - All Urban Consumers"},
                {"series_id": "CPIAUCNS", "series_title": "Consumer Price Index for All Urban Consumers: All Items (NSA)", "survey_name": "Consumer Price Index - All Urban Consumers"},
                {"series_id": "CUSR0000SA0", "series_title": "Consumer Price Index for All Urban Consumers: All Items (Seasonally Adjusted)", "survey_name": "Consumer Price Index - All Urban Consumers"}
            ]
        elif category == "employment":
            data = [
                {"series_id": "UNRATE", "series_title": "Unemployment Rate", "survey_name": "Current Population Survey"},
                {"series_id": "PAYEMS", "series_title": "All Employees: Total Nonfarm Payrolls", "survey_name": "Current Employment Statistics survey (National)"},
                {"series_id": "LNS14000000", "series_title": "Unemployment Rate", "survey_name": "Labor Force Statistics from the Current Population Survey"}
            ]
        else:
            return None

        return pd.DataFrame(data)

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
            # Key labor market series
            labor_series = [
                "UNRATE",      # Unemployment Rate
                "PAYEMS",      # Nonfarm Payrolls
                "LNS14000000", # Unemployment Rate (CPS)
                "LNS13000000", # Labor Force Participation Rate
                "CIVPART",     # Labor Force Participation Rate (alternative)
                "EMRATIO",     # Employment-Population Ratio
                "LNS11300000"  # Employment-Population Ratio (alternative)
            ]

            result = await self.get_bls_timeseries(
                series_ids=labor_series,
                start_year=datetime.now().year - 3,  # 3 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            # Categorize data
            data = result.get("data", {})
            series_data = data.get("series_data", [])

            unemployment_data = [s for s in series_data if "UNRATE" in s.get("symbol", "")]
            payrolls_data = [s for s in series_data if "PAYEMS" in s.get("symbol", "")]
            participation_data = [s for s in series_data if any(pid in s.get("symbol", "") for pid in ["LNS13000000", "CIVPART"])]
            employment_ratio_data = [s for s in series_data if any(eid in s.get("symbol", "") for eid in ["LNS11300000", "EMRATIO"])]

            return {
                "success": True,
                "data": {
                    "unemployment_rate": unemployment_data,
                    "nonfarm_payrolls": payrolls_data,
                    "labor_force_participation": participation_data,
                    "employment_population_ratio": employment_ratio_data,
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
            # Key inflation series
            inflation_series = [
                "CPIAUCSL",    # Consumer Price Index All Items
                "CPIAUCNS",    # CPI All Items (NSA)
                "CUSR0000SA0", # CPI All Items (Seasonally Adjusted)
                "CUSR0000SA0L1E", # CPI All Items Less Food & Energy
                "CUSR0000SA0E",   # CPI Energy
                "CUSR0000SA0F",   # CPI Food
                "CUSR0000SAM3",   # CPI Commodities Less Food & Energy Commodities
                "CUSR0000SAD"     # CPI Services Less Energy Services
            ]

            result = await self.get_bls_timeseries(
                series_ids=inflation_series,
                start_year=datetime.now().year - 3,  # 3 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            # Categorize data
            data = result.get("data", {})
            series_data = data.get("series_data", [])

            cpi_all_data = [s for s in series_data if s.get("symbol") in ["CPIAUCSL", "CPIAUCNS", "CUSR0000SA0"]]
            cpi_core_data = [s for s in series_data if "L1E" in s.get("symbol", "")]
            cpi_energy_data = [s for s in series_data if "E" in s.get("symbol", "") and "L1E" not in s.get("symbol", "")]
            cpi_food_data = [s for s in series_data if "F" in s.get("symbol", "") and "E" not in s.get("symbol", "")]

            return {
                "success": True,
                "data": {
                    "cpi_all_items": cpi_all_data,
                    "core_cpi": cpi_core_data,
                    "energy_inflation": cpi_energy_data,
                    "food_inflation": cpi_food_data,
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
            # ECI series
            eci_series = [
                "CIU1010000000000A",  # Employment Cost Index: Total Compensation, Private
                "CIU2020000000000A",  # Employment Cost Index: Wages and Salaries, Private
                "CIU3010000000000A",  # Employment Cost Index: Benefits, Private
                "CIS1010000000000A",  # Employment Cost Index: Total Compensation, State & Local
                "CIS2020000000000A",  # Employment Cost Index: Wages and Salaries, State & Local
                "CIS3010000000000A"   # Employment Cost Index: Benefits, State & Local
            ]

            result = await self.get_bls_timeseries(
                series_ids=eci_series,
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
            # Productivity series
            productivity_series = [
                "OPHNFB",      # Nonfarm Business Sector: Labor Productivity
                "OPHPBS",      # Nonfarm Business Sector: Unit Labor Costs
                "OPHMFB",      # Manufacturing Sector: Labor Productivity
                "OPHMBS",      # Manufacturing Sector: Unit Labor Costs
                "OPHNFB",      # Nonfarm Business: Output per Hour
                "COMPNFB"      # Nonfarm Business: Compensation per Hour
            ]

            result = await self.get_bls_timeseries(
                series_ids=productivity_series,
                start_year=datetime.now().year - 5,  # 5 years of data
                calculations=True,
                catalog=True
            )

            if "error" in result:
                return result

            # Categorize data
            data = result.get("data", {})
            series_data = data.get("series_data", [])

            productivity_data = [s for s in series_data if any(p in s.get("symbol", "") for p in ["OPHNFB", "OPHMFB", "OPHNFB"])]
            unit_costs_data = [s for s in series_data if any(c in s.get("symbol", "") for c in ["OPHPBS", "OPHMBS"])]
            compensation_data = [s for s in series_data if "COMP" in s.get("symbol", "")]

            return {
                "success": True,
                "data": {
                    "labor_productivity": productivity_data,
                    "unit_labor_costs": unit_costs_data,
                    "compensation": compensation_data,
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


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps(BLSError("cli", "Usage: python bls_data.py <command> [args...]").to_dict()))
        sys.exit(1)

    command = sys.argv[1]

    # Get API key from environment or command line
    api_key = os.getenv("BLS_API_KEY")

    # Create API instance
    api = BLSDataAPI(api_key=api_key)

    # Map commands to async methods
    async def run_command():
        if command == "search_series":
            query = sys.argv[2] if len(sys.argv) > 2 else ""
            category = sys.argv[3] if len(sys.argv) > 3 else "cpi"
            include_extras = sys.argv[4].lower() == "true" if len(sys.argv) > 4 else False
            include_code_map = sys.argv[5].lower() == "true" if len(sys.argv) > 5 else False
            return api.search_bls_series(query, category, include_extras, include_code_map)

        elif command == "get_series":
            series_ids = sys.argv[2] if len(sys.argv) > 2 else ""
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None
            calculations = sys.argv[5].lower() != "false" if len(sys.argv) > 5 else True
            annual_average = sys.argv[6].lower() == "true" if len(sys.argv) > 6 else False
            aspects = sys.argv[7].lower() == "true" if len(sys.argv) > 7 else False
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
    print(json.dumps(result, indent=2, default=str))


if __name__ == "__main__":
    main()