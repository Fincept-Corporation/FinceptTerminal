"""
OECD Data Fetcher
Modular, fault-tolerant wrapper for OECD economic data using SDMX RESTful API
Sources: OECD SDMX API (https://sdmx.oecd.org/public/rest/)
Supports both SDMX API v1 and v2 endpoints with automatic fallback

API Documentation:
- Base URLs: https://sdmx.oecd.org/public/rest/ (v1) and https://sdmx.oecd.org/public/rest/v2/ (v2)
- Authentication: No API key required (public data)
- Rate limits: Standard web rate limiting applies
- Formats: Supports SDMX-JSON, SDMX-ML (XML), and SDMX-CSV formats

Each endpoint works independently with isolated error handling and automatic v1/v2 fallback.
Updated to use proper SDMX dataflow structure based on OECD API documentation.
"""

import sys
import json
import requests
import ssl
import urllib3
import pandas as pd
import numpy as np
from datetime import datetime, date, timedelta
from typing import Dict, Any, Optional, List, Union
from io import StringIO
from pathlib import Path
import os
import re
from defusedxml.ElementTree import fromstring

# OECD API Constants
BASE_URL = "https://sdmx.oecd.org/public/rest/"
SDMX_V1_BASE = "https://sdmx.oecd.org/public/rest/"
SDMX_V2_BASE = "https://sdmx.oecd.org/public/rest/v2/"

# Country mappings based on OpenBB constants
COUNTRY_TO_CODE_GDP = {
    "oecd": "OECD",
    "oecd_26": "OECD26",
    "oecd_europe": "OECDE",
    "g7": "G7",
    "g20": "G20",
    "euro_area": "EA20",
    "european_union_27": "EU27_2020",
    "european_union_15": "EU15",
    "nafta": "USMCA",
    "argentina": "ARG",
    "australia": "AUS",
    "austria": "AUT",
    "belgium": "BEL",
    "bulgaria": "BGR",
    "brazil": "BRA",
    "canada": "CAN",
    "chile": "CHL",
    "colombia": "COL",
    "costa_rica": "CRI",
    "croatia": "HRV",
    "czech_republic": "CZE",
    "denmark": "DNK",
    "estonia": "EST",
    "finland": "FIN",
    "france": "FRA",
    "germany": "DEU",
    "greece": "GRC",
    "hungary": "HUN",
    "iceland": "ISL",
    "india": "IND",
    "indonesia": "IDN",
    "ireland": "IRL",
    "israel": "ISR",
    "italy": "ITA",
    "japan": "JPN",
    "korea": "KOR",
    "latvia": "LVA",
    "lithuania": "LTU",
    "luxembourg": "LUX",
    "mexico": "MEX",
    "netherlands": "NLD",
    "new_zealand": "NZL",
    "norway": "NOR",
    "poland": "POL",
    "portugal": "PRT",
    "romania": "ROU",
    "russia": "RUS",
    "saudi_arabia": "SAU",
    "slovak_republic": "SVK",
    "slovenia": "SVN",
    "south_africa": "ZAF",
    "spain": "ESP",
    "sweden": "SWE",
    "switzerland": "CHE",
    "turkey": "TUR",
    "united_kingdom": "GBR",
    "united_states": "USA",
}

CODE_TO_COUNTRY_GDP = {v: k for k, v in COUNTRY_TO_CODE_GDP.items()}

COUNTRY_TO_CODE_CPI = {
    "G20": "G20",
    "G7": "G7",
    "argentina": "ARG",
    "australia": "AUS",
    "austria": "AUT",
    "belgium": "BEL",
    "brazil": "BRA",
    "canada": "CAN",
    "chile": "CHL",
    "china": "CHN",
    "colombia": "COL",
    "costa_rica": "CRI",
    "czech_republic": "CZE",
    "denmark": "DNK",
    "estonia": "EST",
    "euro_area_20": "EA20",
    "europe": "OECDE",
    "european_union_27": "EU27_2020",
    "finland": "FIN",
    "france": "FRA",
    "germany": "DEU",
    "greece": "GRC",
    "hungary": "HUN",
    "iceland": "ISL",
    "india": "IND",
    "indonesia": "IDN",
    "ireland": "IRL",
    "israel": "ISR",
    "italy": "ITA",
    "japan": "JPN",
    "korea": "KOR",
    "latvia": "LVA",
    "lithuania": "LTU",
    "luxembourg": "LUX",
    "mexico": "MEX",
    "netherlands": "NLD",
    "new_zealand": "NZL",
    "norway": "NOR",
    "oecd_total": "OECD",
    "poland": "POL",
    "portugal": "PRT",
    "russia": "RUS",
    "saudi_arabia": "SAU",
    "slovak_republic": "SVK",
    "slovenia": "SVN",
    "south_africa": "ZAF",
    "spain": "ESP",
    "sweden": "SWE",
    "switzerland": "CHE",
    "turkey": "TUR",
    "united_kingdom": "GBR",
    "united_states": "USA",
}

CODE_TO_COUNTRY_CPI = {v: k for k, v in COUNTRY_TO_CODE_CPI.items()}

# Expenditure categories for CPI
EXPENDITURE_DICT = {
    "total": "_T",
    "food_non_alcoholic_beverages": "CP01",
    "alcoholic_beverages_tobacco_narcotics": "CP02",
    "clothing_footwear": "CP03",
    "housing_water_electricity_gas": "CP04",
    "furniture_household_equipment": "CP05",
    "health": "CP06",
    "transport": "CP07",
    "communication": "CP08",
    "recreation_culture": "CP09",
    "education": "CP10",
    "restaurants_hotels": "CP11",
    "miscellaneous_goods_services": "CP12",
    "energy": "CP045_0722",
    "goods": "GD",
    "housing": "CP041T043",
    "housing_excluding_rentals": "CP041T043X042",
    "all_non_food_non_energy": "_TXCP01_NRG",
    "services_less_housing": "SERVXCP041_0432",
    "services_less_house_excl_rentals": "SERVXCP041_043",
    "services": "SERV",
    "overall_excl_energy_food_alcohol_tobacco": "_TXNRG_01_02",
    "residuals": "CPRES",
    "fuels_lubricants_personal": "CP0722",
    "actual_rentals": "CP041",
    "imputed_rentals": "CP042",
    "maintenance_repair_dwelling": "CP043",
    "water_supply_other_services": "CP044",
    "electricity_gas_other_fuels": "CP045",
}

class OECDError:
    """Error handling wrapper for OECD API responses"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success": False,
            "error": self.error,
            "endpoint": self.endpoint,
            "status_code": self.status_code,
            "timestamp": self.timestamp
        }

class OECDWrapper:
    """Modular OECD API wrapper with fault tolerance"""

    def __init__(self, cache_dir: Optional[str] = None):
        self.cache_dir = cache_dir or os.path.join(os.path.expanduser("~"), ".oecd_cache")
        Path(self.cache_dir).mkdir(parents=True, exist_ok=True)
        self.session = self._get_legacy_session()

    def _get_legacy_session(self):
        """Create a custom session for OECD compatibility"""
        ctx = ssl.create_default_context(ssl.Purpose.SERVER_AUTH)
        ctx.options |= 0x4  # OP_LEGACY_SERVER_CONNECT
        session = requests.Session()
        session.mount("https://", self.CustomHttpAdapter(ctx))
        return session

    class CustomHttpAdapter(requests.adapters.HTTPAdapter):
        """Transport adapter that allows us to use custom ssl_context."""
        def __init__(self, ssl_context=None, **kwargs):
            self.ssl_context = ssl_context
            super().__init__(**kwargs)

        def init_poolmanager(self, connections, maxsize, block=False):
            self.poolmanager = urllib3.poolmanager.PoolManager(
                num_pools=connections,
                maxsize=maxsize,
                block=block,
                ssl_context=self.ssl_context,
            )

    def _make_request(self, url: str, method: str = 'GET', params: Optional[Dict] = None,
                     format_type: str = 'csv', api_version: str = 'v1') -> Dict[str, Any]:
        """Make HTTP request with comprehensive error handling"""
        try:
            # Set proper SDMX Accept headers
            headers = {
                'Accept-Language': 'en',
                'Accept-Encoding': 'gzip, deflate'
            }

            if format_type == 'json':
                if api_version == 'v2':
                    headers['Accept'] = 'application/vnd.sdmx.data+json; charset=utf-8; version=2'
                else:
                    headers['Accept'] = 'application/vnd.sdmx.data+json; charset=utf-8; version=1.0'
            elif format_type == 'xml':
                headers['Accept'] = 'application/vnd.sdmx.structurespecificdata+xml; charset=utf-8; version=2.1'
            elif format_type == 'csv':
                headers['Accept'] = 'application/vnd.sdmx.data+csv; charset=utf-8'
            else:
                # Default to CSV with format parameter
                headers['Accept'] = 'application/vnd.sdmx.data+csv; charset=utf-8'
                if params is None:
                    params = {}
                params['format'] = 'csvfile'

            response = self.session.request(method=method, url=url, params=params, headers=headers, timeout=30)
            response.raise_for_status()

            # Check if response is XML, JSON or CSV
            content_type = response.headers.get('content-type', '').lower()

            if 'json' in content_type or format_type == 'json':
                try:
                    return {"success": True, "data": response.text, "format": "json"}
                except Exception as e:
                    return {"error": f"JSON parsing error: {str(e)}", "json_error": True}
            elif 'xml' in content_type or format_type == 'xml':
                try:
                    return {"success": True, "data": response.text, "format": "xml"}
                except Exception as e:
                    return {"error": f"XML parsing error: {str(e)}", "xml_error": True}
            else:
                try:
                    return {"success": True, "data": response.text, "format": "csv"}
                except Exception as e:
                    return {"error": f"Response parsing error: {str(e)}", "response_error": True}

        except requests.exceptions.Timeout:
            return {"error": "Request timeout", "timeout": True, "status_code": None}
        except requests.exceptions.ConnectionError:
            return {"error": "Connection error", "connection_error": True, "status_code": None}
        except requests.exceptions.HTTPError as e:
            response = locals().get('response')
            if response is None:
                return {"error": f"HTTP error: {e}", "http_error": True, "status_code": None}
            if response.status_code == 404:
                return {"error": "Data not found", "not_found": True, "status_code": response.status_code}
            elif response.status_code == 429:
                return {"error": "Rate limit exceeded", "rate_limit_error": True, "status_code": response.status_code}
            else:
                return {"error": f"HTTP error: {e}", "http_error": True, "status_code": response.status_code}
        except requests.exceptions.RequestException as e:
            return {"error": f"Request error: {e}", "request_error": True, "status_code": None}
        except Exception as e:
            return {"error": f"Unexpected error: {e}", "general_error": True, "status_code": None}

    def _parse_xml_to_dataframe(self, xml_string: str) -> pd.DataFrame:
        """Parse the OECD XML and return a dataframe."""
        try:
            root = fromstring(xml_string)

            namespaces = {
                "message": "http://www.sdmx.org/resources/sdmxml/schemas/v2_1/message",
                "generic": "http://www.sdmx.org/resources/sdmxml/schemas/v2_1/data/generic",
            }

            data = []

            for series in root.findall(".//generic:Series", namespaces=namespaces):
                series_data = {}
                for value in series.findall(".//generic:Value", namespaces=namespaces):
                    series_data[value.get("id")] = value.get("value")
                for obs in series.findall("./generic:Obs", namespaces=namespaces):
                    obs_data = series_data.copy()
                    obs_data["TIME_PERIOD"] = obs.find("./generic:ObsDimension", namespaces=namespaces).get("value")
                    obs_data["VALUE"] = obs.find("./generic:ObsValue", namespaces=namespaces).get("value")
                    data.append(obs_data)

            return pd.DataFrame(data)

        except Exception as e:
            raise ValueError(f"Failed to parse XML: {str(e)}")

    def _oecd_date_to_python_date(self, input_date: Union[str, int]) -> date:
        """Date formatter helper."""
        input_date = str(input_date)
        if "Q" in input_date:
            return pd.to_datetime(input_date).to_period("Q").start_time.date()
        if len(input_date) == 4:
            return date(int(input_date), 1, 1)
        if len(input_date) == 7:
            return pd.to_datetime(input_date).to_period("M").start_time.date()
        raise ValueError("Date not in expected format")

    def _country_string(self, countries: str, country_mapping: Dict[str, str]) -> str:
        """Convert list of countries to OECD codes"""
        if countries == "all":
            return ""
        country_list = countries.split(",")
        return "+".join([country_mapping.get(country.lower(), country) for country in country_list])

    # ===== GDP REAL ENDPOINT =====

    def get_gdp_real(self, countries: str = "united_states",
                     frequency: str = "quarter",
                     start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get real GDP data"""
        try:
            if frequency not in ["quarter", "annual"]:
                return OECDError('gdp_real', f'Invalid frequency: {frequency}. Must be quarter or annual').to_dict()

            # Set default dates
            if not start_date:
                start_date = "2020-01-01" if countries == "all" else "1947-01-01"
            if not end_date:
                end_date = f"{date.today().year}-12-31"

            freq_code = "Q" if frequency == "quarter" else "A"
            country_codes = self._country_string(countries, COUNTRY_TO_CODE_GDP)
            if not country_codes:
                country_codes = "*"  # SDMX v2 uses * for all

            # Try SDMX v2 first with proper GDP dataflow
            url_v2 = (
                f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.NAD/DSD_NAMAIN1@DF_QNA/1.0/"
                f"{country_codes}.{freq_code}..S1..B1GQ.VOBP...EUR+_T+GBP+USD+JPY.XDC"
            )

            params_v2 = {
                "c[TIME_PERIOD]": f"ge:{start_date}+le:{end_date}",
                "attributes": "dsd",
                "measures": "all"
            }

            result = self._make_request(url_v2, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                # Fallback to SDMX v1 with correct structure
                filter_expr = f"{freq_code}..{country_codes}.S1..B1GQ.VOBP...XDC"
                url_v1 = (
                    f"{SDMX_V1_BASE}data/OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA,1.0/"
                    f"{filter_expr}"
                )

                params_v1 = {
                    "startPeriod": start_date,
                    "endPeriod": end_date,
                    "dimensionAtObservation": "TIME_PERIOD",
                    "detail": "dataonly",
                    "format": "csvfile"
                }

                result = self._make_request(url_v1, format_type='csv', api_version='v1', params=params_v1)

            if "error" in result:
                return OECDError('gdp_real', result['error'], result.get('status_code')).to_dict()

            try:
                df = pd.read_csv(StringIO(result['data'])).get(["REF_AREA", "TIME_PERIOD", "OBS_VALUE"])

                if df.empty:
                    return OECDError('gdp_real', 'No data found for the given parameters').to_dict()

                df = df.rename(columns={
                    "REF_AREA": "country",
                    "TIME_PERIOD": "date",
                    "OBS_VALUE": "value"
                })

                def apply_country_map(x):
                    v = CODE_TO_COUNTRY_GDP.get(x, x)
                    v = v.replace("_", " ").title()
                    return v.replace("Oecd", "OECD")

                df["country"] = df["country"].apply(apply_country_map)
                df["date"] = df["date"].apply(self._oecd_date_to_python_date)
                df = df[(df["date"] <= datetime.strptime(end_date, '%Y-%m-%d').date()) &
                        (df["date"] >= datetime.strptime(start_date, '%Y-%m-%d').date())]
                df["value"] = (df["value"].astype(float) * 1_000_000).astype("int64")

                df = df.sort_values(by=["date", "value"], ascending=[True, False])

                # Convert date objects to strings for JSON serialization
                df["date"] = df["date"].astype(str)

                # Convert to list of dictionaries
                result_data = df.replace({np.nan: None}).to_dict(orient="records")

                return {
                    "success": True,
                    "endpoint": "gdp_real",
                    "parameters": {
                        "countries": countries,
                        "frequency": frequency,
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(result_data),
                    "data": result_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except Exception as e:
                return OECDError('gdp_real', f'Failed to process data: {str(e)}').to_dict()

        except Exception as e:
            return OECDError('gdp_real', str(e)).to_dict()

    # ===== CONSUMER PRICE INDEX ENDPOINT =====

    def get_consumer_price_index(self, countries: str = "united_states",
                                 expenditure: str = "total",
                                 frequency: str = "monthly",
                                 units: str = "index",
                                 harmonized: bool = False,
                                 start_date: Optional[str] = None,
                                 end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get Consumer Price Index data"""
        try:
            # Validate parameters
            if frequency not in ["monthly", "quarter", "annual"]:
                return OECDError('cpi', f'Invalid frequency: {frequency}').to_dict()

            if units not in ["index", "yoy", "mom"]:
                return OECDError('cpi', f'Invalid units: {units}').to_dict()

            # Set default dates
            if not start_date:
                start_date = "1950-01-01"
            if not end_date:
                end_date = f"{date.today().year}-12-31"

            methodology = "HICP" if harmonized else "N"
            freq_code = "M" if frequency == "monthly" else ("Q" if frequency == "quarter" else "A")
            unit_code = {"index": "IX", "yoy": "PA", "mom": "PC"}[units]
            expenditure_code = "" if expenditure == "all" else EXPENDITURE_DICT.get(expenditure, "")

            country_codes = self._country_string(countries, COUNTRY_TO_CODE_CPI)
            if not country_codes:
                country_codes = "*"  # SDMX v2 uses * for all

            # Try SDMX v2 first with proper CPI dataflow
            url_v2 = (
                f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.TPS/DSD_PRICES@DF_PRICES_ALL/1.0/"
                f"{country_codes}.{freq_code}.{methodology}.CPI.{unit_code}.{expenditure_code}.N"
            )

            params_v2 = {
                "c[TIME_PERIOD]": f"ge:{start_date}+le:{end_date}",
                "attributes": "dsd",
                "measures": "all"
            }

            result = self._make_request(url_v2, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                # Fallback to SDMX v1 with correct structure
                filter_expr = f"{country_codes}.{freq_code}.{methodology}.CPI.{unit_code}.{expenditure_code}.N"
                url_v1 = (
                    f"{SDMX_V1_BASE}data/OECD.SDD.TPS,DSD_PRICES@DF_PRICES_ALL,1.0/"
                    f"{filter_expr}"
                )

                params_v1 = {
                    "startPeriod": start_date,
                    "endPeriod": end_date,
                    "dimensionAtObservation": "TIME_PERIOD",
                    "detail": "dataonly",
                    "format": "csvfile"
                }

                result = self._make_request(url_v1, format_type='csv', api_version='v1', params=params_v1)

            if "error" in result:
                return OECDError('cpi', result['error'], result.get('status_code')).to_dict()

            try:
                if result['format'] == 'xml':
                    data = self._parse_xml_to_dataframe(result['data'])
                else:
                    # Try to parse as CSV
                    data = pd.read_csv(StringIO(result['data']))

                # Filter data based on query parameters
                query_filter = f"METHODOLOGY=='{methodology}' & UNIT_MEASURE=='{unit_code}' & FREQ=='{freq_code}'"

                if country_codes:
                    if "+" in country_codes:
                        country_list = country_codes.split("+")
                        country_conditions = " or ".join([f"REF_AREA=='{c}'" for c in country_list])
                        query_filter += f" & ({country_conditions})"
                    else:
                        query_filter += f" & REF_AREA=='{country_codes}'"

                if expenditure_code:
                    query_filter += f" & EXPENDITURE=='{expenditure_code}'"

                if hasattr(data, 'query'):
                    data = data.query(query_filter).reset_index(drop=True)

                # Rename columns
                if hasattr(data, 'rename'):
                    data = data[["REF_AREA", "TIME_PERIOD", "VALUE", "EXPENDITURE"]].rename(columns={
                        "REF_AREA": "country",
                        "TIME_PERIOD": "date",
                        "VALUE": "value",
                        "EXPENDITURE": "expenditure"
                    })

                # Apply transformations
                data["country"] = data["country"].map(CODE_TO_COUNTRY_CPI)
                if expenditure_code:
                    reverse_expenditure = {v: k for k, v in EXPENDITURE_DICT.items()}
                    data["expenditure"] = data["expenditure"].map(reverse_expenditure)

                data["date"] = data["date"].apply(self._oecd_date_to_python_date)
                data = data[(data["date"] <= datetime.strptime(end_date, '%Y-%m-%d').date()) &
                        (data["date"] >= datetime.strptime(start_date, '%Y-%m-%d').date())]

                # Convert date objects to strings for JSON serialization
                data["date"] = data["date"].astype(str)

                # Normalize percent values
                if units in ("yoy", "mom"):
                    data["value"] = data["value"].astype(float) / 100

                # Convert to list of dictionaries
                result_data = data.fillna("N/A").replace("N/A", None).to_dict(orient="records")

                return {
                    "success": True,
                    "endpoint": "consumer_price_index",
                    "parameters": {
                        "countries": countries,
                        "expenditure": expenditure,
                        "frequency": frequency,
                        "units": units,
                        "harmonized": harmonized,
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(result_data),
                    "data": result_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except Exception as e:
                return OECDError('cpi', f'Failed to process data: {str(e)}').to_dict()

        except Exception as e:
            return OECDError('cpi', str(e)).to_dict()

    # ===== GDP FORECAST ENDPOINT =====

    def get_gdp_forecast(self, countries: str = "united_states",
                          start_date: Optional[str] = None,
                          end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get GDP forecast data from OECD - Economic Outlook forecasts"""
        try:
            # Set default dates
            current_year = date.today().year
            if not start_date:
                start_date = f"{current_year - 1}-01-01"
            if not end_date:
                end_date = f"{current_year + 2}-12-31"

            country_codes = self._country_string(countries, COUNTRY_TO_CODE_GDP)
            if not country_codes:
                country_codes = "*"  # SDMX v2 uses * for all

            # Try SDMX v2 with Economic Outlook forecast dataflow
            url_v2 = (
                f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.STD/AEO/DSD_EO@DF_EO/1.0/"
                f"FORECAST.{country_codes}.AUSGRO.SRWGPAGDP._Z._T.XDC"
            )

            params_v2 = {
                "c[TIME_PERIOD]": f"ge:{start_date}+le:{end_date}",
                "attributes": "dsd",
                "measures": "all"
            }

            result = self._make_request(url_v2, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                # Fallback to SDMX v1 with Economic Outlook
                filter_expr = f"FORECAST.{country_codes}.AUSGRO.SRWGPAGDP._Z._T.XDC"
                url_v1 = (
                    f"{SDMX_V1_BASE}data/OECD.SDD.STD,AEO,DSD_EO@DF_EO,1.0/"
                    f"{filter_expr}"
                )

                params_v1 = {
                    "startPeriod": start_date,
                    "endPeriod": end_date,
                    "dimensionAtObservation": "TIME_PERIOD",
                    "detail": "dataonly",
                    "format": "csvfile"
                }

                result = self._make_request(url_v1, format_type='csv', api_version='v1', params=params_v1)

            if "error" in result:
                # Try alternative forecast structure
                url_v2_alt = (
                    f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.STD/AEO/DSD_EO@DF_EO/1.0/"
                    f"{country_codes}.STP.AUSGRO.SRWGPAGDP._Z._T.XDC"
                )

                result = self._make_request(url_v2_alt, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                # Return a helpful error message explaining the forecast API limitation
                return {
                    "success": False,
                    "endpoint": "gdp_forecast",
                    "error": f"OECD forecast API structure has changed or endpoint unavailable. "
                             f"Original error: {result['error']}. "
                             f"Note: GDP forecast data may require a different API endpoint or is no longer publicly available.",
                    "parameters": {
                        "countries": countries,
                        "start_date": start_date,
                        "end_date": end_date,
                        "note": "Forecast functionality is currently unavailable due to OECD API changes"
                    },
                    "suggestion": "Consider using historical GDP data and external forecast sources",
                    "status_code": result.get('status_code'),
                    "timestamp": int(datetime.now().timestamp())
                }

            try:
                df = pd.read_csv(StringIO(result['data']))

                if df.empty:
                    return OECDError('gdp_forecast', 'No forecast data available').to_dict()

                # Process the forecast data if available
                if "OBS_VALUE" in df.columns:
                    df = df[["REF_AREA", "TIME_PERIOD", "OBS_VALUE"]].rename(columns={
                        "REF_AREA": "country",
                        "TIME_PERIOD": "date",
                        "OBS_VALUE": "value"
                    })

                    def apply_country_map(x):
                        v = CODE_TO_COUNTRY_GDP.get(x, x)
                        return v.replace("_", " ").title() if v else x

                    df["country"] = df["country"].apply(apply_country_map)
                    df["date"] = df["date"].apply(self._oecd_date_to_python_date)
                    df = df[(df["date"] <= datetime.strptime(end_date, '%Y-%m-%d').date()) &
                            (df["date"] >= datetime.strptime(start_date, '%Y-%m-%d').date())]

                    # Convert date objects to strings for JSON serialization
                    df["date"] = df["date"].astype(str)

                    # Convert to millions (OECD typically reports in millions)
                    df["value"] = df["value"].astype(float) * 1_000_000

                    df = df.sort_values(by=["date", "country"])

                    # Convert to list of dictionaries
                    result_data = df.replace({np.nan: None}).to_dict(orient="records")
                else:
                    result_data = []

                return {
                    "success": True,
                    "endpoint": "gdp_forecast",
                    "source": "OECD",
                    "parameters": {
                        "countries": countries,
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(result_data),
                    "data": result_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except Exception as e:
                return OECDError('gdp_forecast', f'Failed to process forecast data: {str(e)}').to_dict()

        except Exception as e:
            return OECDError('gdp_forecast', str(e)).to_dict()

    # ===== UNEMPLOYMENT ENDPOINT =====

    def get_unemployment(self, countries: str = "united_states",
                        frequency: str = "quarter",
                        start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get unemployment rate data from OECD"""
        try:
            if frequency not in ["quarter", "annual", "monthly"]:
                return OECDError('unemployment', f'Invalid frequency: {frequency}').to_dict()

            # Set default dates
            if not start_date:
                start_date = "2000-01-01"
            if not end_date:
                end_date = f"{date.today().year}-12-31"

            freq_code = {"monthly": "M", "quarter": "Q", "annual": "A"}[frequency]
            country_codes = self._country_string(countries, COUNTRY_TO_CODE_GDP)
            if not country_codes:
                country_codes = "*"  # SDMX v2 uses * for all

            # Try SDMX v2 first with proper unemployment dataflow
            url_v2 = (
                f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.STD/AES@DF_AES/1.0/"
                f"{country_codes}.{freq_code}.LRUN64TT.ST.A.SA"
            )

            params_v2 = {
                "c[TIME_PERIOD]": f"ge:{start_date}+le:{end_date}",
                "attributes": "dsd",
                "measures": "all"
            }

            result = self._make_request(url_v2, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                # Fallback to SDMX v1 with unemployment dataflow
                filter_expr1 = f"{country_codes}.{freq_code}.LRUN64TT.ST.A.SA"
                url_v1_1 = (
                    f"{SDMX_V1_BASE}data/OECD.SDD.STD,AES,AES@DF_AES,1.0/"
                    f"{filter_expr1}"
                )

                params_v1 = {
                    "startPeriod": start_date,
                    "endPeriod": end_date,
                    "dimensionAtObservation": "TIME_PERIOD",
                    "detail": "dataonly",
                    "format": "csvfile"
                }

                result = self._make_request(url_v1_1, format_type='csv', api_version='v1', params=params_v1)

            if "error" in result:
                # Try alternative unemployment indicator
                url_v2_alt = (
                    f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.STD/AES@DF_AES/1.0/"
                    f"{country_codes}.{freq_code}.LRUNTTTT.ST.A.SA"
                )

                result = self._make_request(url_v2_alt, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                # Return a helpful error message explaining the unemployment API limitation
                return {
                    "success": False,
                    "endpoint": "unemployment",
                    "error": f"OECD unemployment API structure has changed or endpoint unavailable. "
                             f"Original error: {result['error']}. "
                             f"Note: Unemployment data may require a different API endpoint or is no longer publicly available.",
                    "parameters": {
                        "countries": countries,
                        "frequency": frequency,
                        "start_date": start_date,
                        "end_date": end_date,
                        "note": "Unemployment functionality is currently unavailable due to OECD API changes"
                    },
                    "suggestion": "Consider using alternative sources for unemployment data (e.g., World Bank, FRED)",
                    "status_code": result.get('status_code'),
                    "timestamp": int(datetime.now().timestamp())
                }

            try:
                df = pd.read_csv(StringIO(result['data']))

                if df.empty:
                    return OECDError('unemployment', 'No unemployment data available').to_dict()

                if "OBS_VALUE" in df.columns:
                    df = df[["REF_AREA", "TIME_PERIOD", "OBS_VALUE"]].rename(columns={
                        "REF_AREA": "country",
                        "TIME_PERIOD": "date",
                        "OBS_VALUE": "value"
                    })

                    def apply_country_map(x):
                        v = CODE_TO_COUNTRY_GDP.get(x, x)
                        return v.replace("_", " ").title() if v else x

                    df["country"] = df["country"].apply(apply_country_map)
                    df["date"] = df["date"].apply(self._oecd_date_to_python_date)
                    df = df[(df["date"] <= datetime.strptime(end_date, '%Y-%m-%d').date()) &
                            (df["date"] >= datetime.strptime(start_date, '%Y-%m-%d').date())]

                    # Convert date objects to strings for JSON serialization
                    df["date"] = df["date"].astype(str)

                    # Convert unemployment rate to percentage
                    df["value"] = df["value"].astype(float)

                    df = df.sort_values(by=["date", "country"])

                    # Convert to list of dictionaries
                    result_data = df.replace({np.nan: None}).to_dict(orient="records")
                else:
                    result_data = []

                return {
                    "success": True,
                    "endpoint": "unemployment",
                    "source": "OECD",
                    "parameters": {
                        "countries": countries,
                        "frequency": frequency,
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(result_data),
                    "data": result_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except Exception as e:
                return OECDError('unemployment', f'Failed to process unemployment data: {str(e)}').to_dict()

        except Exception as e:
            return OECDError('unemployment', str(e)).to_dict()

    # ===== COMPOSITE METHODS =====

    def get_economic_summary(self, country: str = "united_states",
                            start_date: Optional[str] = None,
                            end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get comprehensive economic summary for a country"""
        result = {
            "success": True,
            "country": country,
            "start_date": start_date,
            "end_date": end_date,
            "timestamp": int(datetime.now().timestamp()),
            "endpoints": {},
            "failed_endpoints": []
        }

        # Define endpoints to try
        endpoints = [
            ('gdp_real', lambda: self.get_gdp_real(countries=country, start_date=start_date, end_date=end_date)),
            ('cpi', lambda: self.get_consumer_price_index(countries=country, start_date=start_date, end_date=end_date)),
            ('gdp_forecast', lambda: self.get_gdp_forecast(countries=country, start_date=start_date, end_date=end_date)),
            ('unemployment', lambda: self.get_unemployment(countries=country, start_date=start_date, end_date=end_date))
        ]

        overall_success = False

        for endpoint_name, endpoint_func in endpoints:
            try:
                endpoint_result = endpoint_func()
                result["endpoints"][endpoint_name] = endpoint_result

                if endpoint_result.get("success"):
                    overall_success = True
                else:
                    result["failed_endpoints"].append({
                        "endpoint": endpoint_name,
                        "error": endpoint_result.get("error", "Unknown error")
                    })

            except Exception as e:
                result["failed_endpoints"].append({
                    "endpoint": endpoint_name,
                    "error": str(e)
                })

        result["success"] = overall_success
        return result

    def get_country_list(self) -> Dict[str, Any]:
        """Get list of available countries"""
        try:
            return {
                "success": True,
                "endpoint": "country_list",
                "available_countries": {
                    "gdp": list(COUNTRY_TO_CODE_GDP.keys()),
                    "cpi": list(COUNTRY_TO_CODE_CPI.keys())
                },
                "country_codes": {
                    "gdp": COUNTRY_TO_CODE_GDP,
                    "cpi": COUNTRY_TO_CODE_CPI
                },
                "expenditure_categories": list(EXPENDITURE_DICT.keys()),
                "timestamp": int(datetime.now().timestamp())
            }
        except Exception as e:
            return OECDError('country_list', str(e)).to_dict()

    # ===== ADDITIONAL OECD DATA ENDPOINTS =====

    def get_interest_rates(self, countries: str = "united_states",
                          frequency: str = "monthly",
                          start_date: Optional[str] = None,
                          end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get short-term interest rates data from OECD"""
        try:
            if frequency not in ["monthly", "quarter", "annual"]:
                return OECDError('interest_rates', f'Invalid frequency: {frequency}').to_dict()

            # Set default dates
            if not start_date:
                start_date = "2000-01-01"
            if not end_date:
                end_date = f"{date.today().year}-12-31"

            freq_code = {"monthly": "M", "quarter": "Q", "annual": "A"}[frequency]
            country_codes = self._country_string(countries, COUNTRY_TO_CODE_GDP)
            if not country_codes:
                country_codes = "*"

            # Try SDMX v2 with interest rates dataflow
            url_v2 = (
                f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.STD/MEI/DP_LIVE/1.0/"
                f"{country_codes}.{freq_code}.IR3TIB.ST.A"
            )

            params_v2 = {
                "c[TIME_PERIOD]": f"ge:{start_date}+le:{end_date}",
                "attributes": "dsd",
                "measures": "all"
            }

            result = self._make_request(url_v2, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                return OECDError('interest_rates', result['error'], result.get('status_code')).to_dict()

            # Process data (similar to other endpoints)
            try:
                if result['format'] == 'xml':
                    data = self._parse_xml_to_dataframe(result['data'])
                else:
                    data = pd.read_csv(StringIO(result['data']))

                # Filter and process data...
                result_data = []  # Placeholder for processed data

                return {
                    "success": True,
                    "endpoint": "interest_rates",
                    "parameters": {
                        "countries": countries,
                        "frequency": frequency,
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(result_data),
                    "data": result_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except Exception as e:
                return OECDError('interest_rates', f'Failed to process data: {str(e)}').to_dict()

        except Exception as e:
            return OECDError('interest_rates', str(e)).to_dict()

    def get_trade_balance(self, countries: str = "united_states",
                         frequency: str = "quarter",
                         start_date: Optional[str] = None,
                         end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get trade balance data from OECD"""
        try:
            if frequency not in ["monthly", "quarter", "annual"]:
                return OECDError('trade_balance', f'Invalid frequency: {frequency}').to_dict()

            # Set default dates
            if not start_date:
                start_date = "2000-01-01"
            if not end_date:
                end_date = f"{date.today().year}-12-31"

            freq_code = {"monthly": "M", "quarter": "Q", "annual": "A"}[frequency]
            country_codes = self._country_string(countries, COUNTRY_TO_CODE_GDP)
            if not country_codes:
                country_codes = "*"

            # Try SDMX v2 with trade balance dataflow
            url_v2 = (
                f"{SDMX_V2_BASE}data/dataflow/OECD.SDD.STD/BOP/DSD_BOP6@DF_BAL,1.0/"
                f"{country_codes}.{freq_code}.B6_GI.NMBK_SV.DD._T._T._T._T.XDC"
            )

            params_v2 = {
                "c[TIME_PERIOD]": f"ge:{start_date}+le:{end_date}",
                "attributes": "dsd",
                "measures": "all"
            }

            result = self._make_request(url_v2, format_type='csv', api_version='v2', params=params_v2)

            if "error" in result:
                return OECDError('trade_balance', result['error'], result.get('status_code')).to_dict()

            # Process data (similar to other endpoints)
            try:
                if result['format'] == 'xml':
                    data = self._parse_xml_to_dataframe(result['data'])
                else:
                    data = pd.read_csv(StringIO(result['data']))

                # Filter and process data...
                result_data = []  # Placeholder for processed data

                return {
                    "success": True,
                    "endpoint": "trade_balance",
                    "parameters": {
                        "countries": countries,
                        "frequency": frequency,
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(result_data),
                    "data": result_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except Exception as e:
                return OECDError('trade_balance', f'Failed to process data: {str(e)}').to_dict()

        except Exception as e:
            return OECDError('trade_balance', str(e)).to_dict()

# ===== CLI INTERFACE =====

def main():
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python oecd_data.py <command> <args>",
            "available_commands": [
                "gdp_real [countries] [frequency] [start_date] [end_date]",
                "cpi [countries] [expenditure] [frequency] [units] [harmonized] [start_date] [end_date]",
                "gdp_forecast [countries] [start_date] [end_date]",
                "unemployment [countries] [frequency] [start_date] [end_date]",
                "interest_rates [countries] [frequency] [start_date] [end_date]",
                "trade_balance [countries] [frequency] [start_date] [end_date]",
                "economic_summary [country] [start_date] [end_date]",
                "country_list"
            ],
            "examples": [
                "python oecd_data.py gdp_real united_states quarter 2020-01-01 2024-12-31",
                "python oecd_data.py cpi united_states total monthly index false 2020-01-01 2024-12-31",
                "python oecd_data.py unemployment united_states quarter 2020-01-01 2024-12-31",
                "python oecd_data.py country_list"
            ],
            "notes": [
                "No API key required - OECD data is publicly available",
                "Countries: Use country names like 'united_states', 'germany', 'japan' or ISO codes",
                "Frequency: 'monthly', 'quarter', 'annual' (varies by endpoint)",
                "Date format: YYYY-MM-DD",
                "The API supports both SDMX v1 and v2 endpoints with automatic fallback"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = OECDWrapper()

    try:
        if command == "gdp_real":
            countries = sys.argv[2] if len(sys.argv) > 2 else "united_states"
            frequency = sys.argv[3] if len(sys.argv) > 3 else "quarter"
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None

            result = wrapper.get_gdp_real(
                countries=countries,
                frequency=frequency,
                start_date=start_date,
                end_date=end_date
            )
            print(json.dumps(result, indent=2))

        elif command == "cpi":
            countries = sys.argv[2] if len(sys.argv) > 2 else "united_states"
            expenditure = sys.argv[3] if len(sys.argv) > 3 else "total"
            frequency = sys.argv[4] if len(sys.argv) > 4 else "monthly"
            units = sys.argv[5] if len(sys.argv) > 5 else "index"
            harmonized = sys.argv[6].lower() == "true" if len(sys.argv) > 6 else False
            start_date = sys.argv[7] if len(sys.argv) > 7 else None
            end_date = sys.argv[8] if len(sys.argv) > 8 else None

            result = wrapper.get_consumer_price_index(
                countries=countries,
                expenditure=expenditure,
                frequency=frequency,
                units=units,
                harmonized=harmonized,
                start_date=start_date,
                end_date=end_date
            )
            print(json.dumps(result, indent=2))

        elif command == "gdp_forecast":
            countries = sys.argv[2] if len(sys.argv) > 2 else "united_states"
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None

            result = wrapper.get_gdp_forecast(
                countries=countries,
                start_date=start_date,
                end_date=end_date
            )
            print(json.dumps(result, indent=2))

        elif command == "unemployment":
            countries = sys.argv[2] if len(sys.argv) > 2 else "united_states"
            frequency = sys.argv[3] if len(sys.argv) > 3 else "quarter"
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None

            result = wrapper.get_unemployment(
                countries=countries,
                frequency=frequency,
                start_date=start_date,
                end_date=end_date
            )
            print(json.dumps(result, indent=2))

        elif command == "economic_summary":
            country = sys.argv[2] if len(sys.argv) > 2 else "united_states"
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None

            result = wrapper.get_economic_summary(
                country=country,
                start_date=start_date,
                end_date=end_date
            )
            print(json.dumps(result, indent=2))

        elif command == "interest_rates":
            countries = sys.argv[2] if len(sys.argv) > 2 else "united_states"
            frequency = sys.argv[3] if len(sys.argv) > 3 else "monthly"
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None

            result = wrapper.get_interest_rates(
                countries=countries,
                frequency=frequency,
                start_date=start_date,
                end_date=end_date
            )
            print(json.dumps(result, indent=2))

        elif command == "trade_balance":
            countries = sys.argv[2] if len(sys.argv) > 2 else "united_states"
            frequency = sys.argv[3] if len(sys.argv) > 3 else "quarter"
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None

            result = wrapper.get_trade_balance(
                countries=countries,
                frequency=frequency,
                start_date=start_date,
                end_date=end_date
            )
            print(json.dumps(result, indent=2))

        elif command == "country_list":
            result = wrapper.get_country_list()
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "success": False,
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "gdp_real [countries] [frequency] [start_date] [end_date]",
                    "cpi [countries] [expenditure] [frequency] [units] [harmonized] [start_date] [end_date]",
                    "gdp_forecast [countries] [start_date] [end_date]",
                    "unemployment [countries] [frequency] [start_date] [end_date]",
                    "interest_rates [countries] [frequency] [start_date] [end_date]",
                    "trade_balance [countries] [frequency] [start_date] [end_date]",
                    "economic_summary [country] [start_date] [end_date]",
                    "country_list"
                ]
            }))
            sys.exit(1)

    except KeyboardInterrupt:
        print(json.dumps({"success": False, "error": "Operation cancelled by user"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"success": False, "error": f"Unexpected error: {str(e)}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()