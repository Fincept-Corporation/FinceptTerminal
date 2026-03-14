"""
UN Comtrade API Data Fetcher
Comprehensive wrapper for the United Nations Comtrade Database API.
Provides access to global bilateral trade flows for 200+ countries at
HS product code level — the most comprehensive global trade dataset.

API Documentation: https://comtradedeveloper.un.org/
Official Python lib: https://github.com/uncomtrade/comtradeapicall
Base URL (public):  https://comtradeapi.un.org/public/v1/
Base URL (auth):    https://comtradeapi.un.org/data/v1/
Metadata URL:       https://comtradeapi.un.org/files/v1/app/

Authentication:
  - Without key: unlimited calls/day, max 500 records per call (preview endpoints)
  - With subscription key: 500 calls/day, max 100,000 records per call
  - Register free at: https://comtradedeveloper.un.org/

Key Concepts:
  - typeCode:   'C' = Goods/Commodities, 'S' = Services
  - freqCode:   'A' = Annual, 'M' = Monthly
  - clCode:     Classification scheme — 'HS' (Harmonized System), 'S1'/'S2'/'S3'/'S4' (SITC revisions), 'B4'/'B5' (BEC)
  - flowCode:   'X' = Export, 'M' = Import, 'RX' = Re-export, 'RM' = Re-import
  - period:     'YYYY' for annual, 'YYYYMM' for monthly
  - cmdCode:    HS commodity code e.g. '0101' (horses), 'TOTAL' for all
  - reporterCode / partnerCode: UN numeric country codes (e.g. 842=USA, 156=China, 356=India)
"""

import sys
import json
import os
import requests
from datetime import datetime, timezone, timedelta
from typing import Dict, Any, Optional, List, Union


BASE_URL_PUBLIC = "https://comtradeapi.un.org/public/v1"
BASE_URL_DATA   = "https://comtradeapi.un.org/data/v1"
BASE_URL_TOOLS  = "https://comtradeapi.un.org/tools/v1"
BASE_URL_BULK   = "https://comtradeapi.un.org/bulk/v1"
BASE_URL_FILES  = "https://comtradeapi.un.org/files/v1/app/reference"

# Static reference JSON files
REF_LIST_URL     = f"{BASE_URL_FILES}/../ListofReferences.json"
REF_REPORTERS    = f"{BASE_URL_FILES}/Reporters.json"
REF_PARTNERS     = f"{BASE_URL_FILES}/partnerAreas.json"
REF_FLOWS        = f"{BASE_URL_FILES}/tradeRegimes.json"
REF_MOT          = f"{BASE_URL_FILES}/ModeOfTransportCodes.json"
REF_CUSTOMS      = f"{BASE_URL_FILES}/CustomsCodes.json"
REF_ISO_CODES    = f"{BASE_URL_FILES}/country_area_code_iso.json"

# Reference category → URL map (populated after imports)
REFERENCE_URLS = {
    "reporter":          f"{BASE_URL_FILES}/Reporters.json",
    "partner":           f"{BASE_URL_FILES}/partnerAreas.json",
    "flow":              f"{BASE_URL_FILES}/tradeRegimes.json",
    "mot":               f"{BASE_URL_FILES}/ModeOfTransportCodes.json",
    "customs":           f"{BASE_URL_FILES}/CustomsCodes.json",
    "freq":              f"{BASE_URL_FILES}/Frequency.json",
    "qtyunit":           f"{BASE_URL_FILES}/QuantityUnits.json",
    "mos":               f"{BASE_URL_FILES}/ModeOfSupply.json",
    "dataitem":          f"{BASE_URL_FILES}/TradeDataItems.json",
    "HS":                f"{BASE_URL_FILES}/HS.json",
    "H0":                f"{BASE_URL_FILES}/H0.json",
    "H1":                f"{BASE_URL_FILES}/H1.json",
    "H2":                f"{BASE_URL_FILES}/H2.json",
    "H3":                f"{BASE_URL_FILES}/H3.json",
    "H4":                f"{BASE_URL_FILES}/H4.json",
    "H5":                f"{BASE_URL_FILES}/H5.json",
    "H6":                f"{BASE_URL_FILES}/H6.json",
    "S1":                f"{BASE_URL_FILES}/S1.json",
    "S2":                f"{BASE_URL_FILES}/S2.json",
    "S3":                f"{BASE_URL_FILES}/S3.json",
    "S4":                f"{BASE_URL_FILES}/S4.json",
    "B4":                f"{BASE_URL_FILES}/B4.json",
    "B5":                f"{BASE_URL_FILES}/B5.json",
    "EB02":              f"{BASE_URL_FILES}/EB02.json",
    "EB10":              f"{BASE_URL_FILES}/EB10.json",
}

# Common UN country codes for convenience
COUNTRY_CODES = {
    "USA": 842, "CHN": 156, "DEU": 276, "JPN": 392, "GBR": 826,
    "FRA": 251, "IND": 356, "ITA": 381, "CAN": 124, "KOR": 410,
    "RUS": 643, "AUS": 36,  "BRA": 76,  "ESP": 724, "MEX": 484,
    "IDN": 360, "NLD": 528, "SAU": 682, "TUR": 792, "CHE": 757,
    "POL": 616, "BEL": 56,  "SWE": 752, "ARG": 32,  "NOR": 578,
    "ARE": 784, "THA": 764, "ZAF": 710, "MYS": 458, "SGP": 702,
    "DNK": 208, "EGY": 818, "PHL": 608, "CHL": 152, "VNM": 704,
    "BGD": 50,  "NGA": 566, "PAK": 586, "IRN": 364, "ISR": 376,
    "AUT": 40,  "PRT": 620, "GRC": 300, "CZE": 203, "ROU": 642,
    "HUN": 348, "FIN": 246, "NZL": 554, "PER": 604, "COL": 170,
}

# Common HS chapter codes
HS_CHAPTERS = {
    "01": "Live animals",
    "02": "Meat and edible meat offal",
    "03": "Fish and crustaceans",
    "04": "Dairy produce; eggs; honey",
    "10": "Cereals",
    "15": "Animal/vegetable fats and oils",
    "22": "Beverages, spirits and vinegar",
    "27": "Mineral fuels and oils",
    "28": "Inorganic chemicals",
    "29": "Organic chemicals",
    "30": "Pharmaceutical products",
    "39": "Plastics",
    "40": "Rubber",
    "44": "Wood and articles of wood",
    "52": "Cotton",
    "61": "Knitted/crocheted clothing",
    "72": "Iron and steel",
    "84": "Nuclear reactors, boilers, machinery",
    "85": "Electrical machinery and equipment",
    "87": "Vehicles",
    "88": "Aircraft and spacecraft",
    "90": "Optical, photographic, medical instruments",
    "TOTAL": "All commodities",
}


class ComtradeError:
    """Error handling wrapper for UN Comtrade API responses"""

    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now(timezone.utc).timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success": False,
            "error": self.error,
            "endpoint": self.endpoint,
            "status_code": self.status_code,
            "timestamp": self.timestamp,
        }


class UNComtradeWrapper:
    """
    Comprehensive UN Comtrade API wrapper.
    Uses public (no-key) preview endpoints by default.
    Pass a subscription_key for full data access (500 calls/day, 100k records/call).
    """

    def __init__(self, subscription_key: Optional[str] = None):
        self.subscription_key = subscription_key or os.environ.get("UN_COMTRADE_API_KEY", None)
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/1.0",
            "Accept": "application/json",
        })

    # ==================== INTERNAL HELPERS ====================

    def _make_request(self, url: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Centralized GET request with error handling"""
        try:
            # Strip None values
            clean_params = {k: v for k, v in params.items() if v is not None}

            # Inject subscription key if available
            if self.subscription_key:
                clean_params["subscription-key"] = self.subscription_key

            response = self.session.get(url, params=clean_params, timeout=60)

            if response.status_code == 401:
                return ComtradeError(url, "Unauthorized — invalid or missing subscription key.", 401).to_dict()
            if response.status_code == 429:
                return ComtradeError(url, "Rate limit exceeded. 500 calls/day limit reached.", 429).to_dict()
            if response.status_code == 403:
                return ComtradeError(url, "Forbidden — subscription key may be expired or insufficient.", 403).to_dict()

            response.raise_for_status()
            data = response.json()

            # Comtrade wraps results in {"data": [...], "count": N, "error": null}
            if isinstance(data, dict) and data.get("error"):
                return ComtradeError(url, str(data["error"])).to_dict()

            records = data.get("data", data) if isinstance(data, dict) else data
            count   = data.get("count", len(records) if isinstance(records, list) else 0)

            return {
                "success": True,
                "endpoint": url,
                "data": records,
                "count": count,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }

        except requests.exceptions.HTTPError as e:
            return ComtradeError(url, f"HTTP error: {str(e)}", getattr(e.response, "status_code", None)).to_dict()
        except requests.exceptions.Timeout:
            return ComtradeError(url, "Request timed out after 60 seconds.").to_dict()
        except requests.exceptions.RequestException as e:
            return ComtradeError(url, f"Network error: {str(e)}").to_dict()
        except json.JSONDecodeError as e:
            return ComtradeError(url, f"JSON decode error: {str(e)}").to_dict()
        except Exception as e:
            return ComtradeError(url, f"Unexpected error: {str(e)}").to_dict()

    def _trade_url(self, endpoint: str, type_code: str, freq_code: str, cl_code: str) -> str:
        """Build the correct base URL depending on whether we have a subscription key"""
        if self.subscription_key:
            return f"{BASE_URL_DATA}/{endpoint}/{type_code}/{freq_code}/{cl_code}"
        else:
            preview_ep = "preview" if endpoint == "get" else "previewTariffline"
            return f"{BASE_URL_PUBLIC}/{preview_ep}/{type_code}/{freq_code}/{cl_code}"

    # ==================== TRADE DATA ENDPOINTS ====================

    def get_trade_data(
        self,
        reporter_code: Union[str, int],
        period: str,
        type_code: str = "C",
        freq_code: str = "A",
        cl_code: str = "HS",
        cmd_code: str = "TOTAL",
        flow_code: str = "X,M",
        partner_code: Optional[Union[str, int]] = None,
        partner2_code: Optional[Union[str, int]] = None,
        customs_code: Optional[str] = None,
        mot_code: Optional[str] = None,
        max_records: int = 500,
        include_desc: bool = True,
        aggregate_by: Optional[str] = None,
        breakdown_mode: str = "classic",
        count_only: bool = False,
    ) -> Dict[str, Any]:
        """
        Retrieve trade data (final aggregated).
        Without subscription key: limited to 500 records (preview).
        With subscription key: up to 100,000 records.

        Args:
            reporter_code: UN country code of the reporting country (e.g. 842 = USA)
                           Use 'all' for all reporters.
            period: 'YYYY' for annual or 'YYYYMM' for monthly. Comma-separate multiple.
                    E.g. '2023', '202301,202302', '2020,2021,2022'
            type_code: 'C' = Goods, 'S' = Services
            freq_code: 'A' = Annual, 'M' = Monthly
            cl_code: 'HS' = Harmonized System, 'S1'/'S2'/'S3'/'S4' = SITC revisions, 'B4'/'B5' = BEC
            cmd_code: Commodity code(s). 'TOTAL' = all. Comma-separate multiples.
                      E.g. '27' (mineral fuels), '84,85' (machinery+electronics)
            flow_code: 'X' = exports, 'M' = imports, 'RX' = re-exports, 'RM' = re-imports
                       Comma-separate e.g. 'X,M'
            partner_code: Trading partner country code. None = all partners. '0' = world total.
            partner2_code: Second partner code (for re-export/re-import flows)
            customs_code: Customs procedure code. None = all.
            mot_code: Mode of transport code. None = all.
            max_records: Max records to return (500 for preview, up to 100000 with key)
            include_desc: Include human-readable descriptions for codes
            aggregate_by: Aggregation field
            breakdown_mode: 'classic' or 'plus'
            count_only: If True, return only record count
        """
        url = self._trade_url("get", type_code, freq_code, cl_code)
        params = {
            "reporterCode": reporter_code,
            "period": period,
            "cmdCode": cmd_code,
            "flowCode": flow_code,
            "partnerCode": partner_code,
            "partner2Code": partner2_code,
            "customsCode": customs_code,
            "motCode": mot_code,
            "maxRecords": max_records,
            "includeDesc": str(include_desc).lower(),
            "aggregateBy": aggregate_by,
            "breakdownMode": breakdown_mode,
            "countOnly": str(count_only).lower() if count_only else None,
        }
        result = self._make_request(url, params)
        if result.get("success"):
            result["reporter_code"] = reporter_code
            result["period"] = period
            result["cmd_code"] = cmd_code
            result["flow_code"] = flow_code
        return result

    def get_tariffline_data(
        self,
        reporter_code: Union[str, int],
        period: str,
        type_code: str = "C",
        freq_code: str = "A",
        cl_code: str = "HS",
        cmd_code: str = "TOTAL",
        flow_code: str = "X,M",
        partner_code: Optional[Union[str, int]] = None,
        max_records: int = 500,
        include_desc: bool = True,
    ) -> Dict[str, Any]:
        """
        Retrieve tariff-line level trade data (most granular).
        Requires subscription key for more than 500 records.

        Args:
            reporter_code: Reporting country UN code
            period: 'YYYY' or 'YYYYMM'
            type_code: 'C' or 'S'
            freq_code: 'A' or 'M'
            cl_code: Classification scheme
            cmd_code: Commodity code(s)
            flow_code: Trade flow direction(s)
            partner_code: Partner country code
            max_records: Maximum records
            include_desc: Include descriptions
        """
        if self.subscription_key:
            url = f"{BASE_URL_DATA}/getTariffline/{type_code}/{freq_code}/{cl_code}"
        else:
            url = f"{BASE_URL_PUBLIC}/previewTariffline/{type_code}/{freq_code}/{cl_code}"

        params = {
            "reporterCode": reporter_code,
            "period": period,
            "cmdCode": cmd_code,
            "flowCode": flow_code,
            "partnerCode": partner_code,
            "maxRecords": max_records,
            "includeDesc": str(include_desc).lower(),
        }
        return self._make_request(url, params)

    def get_trade_balance(
        self,
        reporter_code: Union[str, int],
        period: str,
        type_code: str = "C",
        freq_code: str = "A",
        cl_code: str = "HS",
        cmd_code: str = "TOTAL",
        partner_code: Optional[Union[str, int]] = None,
        include_desc: bool = True,
    ) -> Dict[str, Any]:
        """
        Get trade balance — exports and imports side by side for a reporter.
        Requires subscription key.

        Args:
            reporter_code: Reporting country UN code
            period: Period(s) e.g. '2023' or '2020,2021,2022,2023'
            type_code: 'C' or 'S'
            freq_code: 'A' or 'M'
            cl_code: Classification
            cmd_code: Commodity code
            partner_code: Partner country (None = world)
            include_desc: Include descriptions
        """
        if not self.subscription_key:
            return ComtradeError(
                "/tools/v1/getTradeBalance",
                "Trade balance endpoint requires a subscription key. Register free at https://comtradedeveloper.un.org/"
            ).to_dict()

        url = f"{BASE_URL_TOOLS}/getTradeBalance/{type_code}/{freq_code}/{cl_code}"
        params = {
            "reporterCode": reporter_code,
            "period": period,
            "cmdCode": cmd_code,
            "partnerCode": partner_code,
            "includeDesc": str(include_desc).lower(),
        }
        result = self._make_request(url, params)
        if result.get("success"):
            result["reporter_code"] = reporter_code
            result["period"] = period
        return result

    def get_bilateral_data(
        self,
        reporter_code: Union[str, int],
        partner_code: Union[str, int],
        period: str,
        type_code: str = "C",
        freq_code: str = "A",
        cl_code: str = "HS",
        cmd_code: str = "TOTAL",
        include_desc: bool = True,
    ) -> Dict[str, Any]:
        """
        Get bilateral trade data — both reporter's and partner's reported figures side by side.
        Useful for reconciling mirror statistics. Requires subscription key.

        Args:
            reporter_code: Primary reporting country UN code
            partner_code: Partner country UN code
            period: Period(s) e.g. '2023'
            type_code: 'C' or 'S'
            freq_code: 'A' or 'M'
            cl_code: Classification
            cmd_code: Commodity code
            include_desc: Include descriptions
        """
        if not self.subscription_key:
            return ComtradeError(
                "/tools/v1/getBilateralData",
                "Bilateral data endpoint requires a subscription key. Register free at https://comtradedeveloper.un.org/"
            ).to_dict()

        url = f"{BASE_URL_TOOLS}/getBilateralData/{type_code}/{freq_code}/{cl_code}"
        params = {
            "reporterCode": reporter_code,
            "partnerCode": partner_code,
            "period": period,
            "cmdCode": cmd_code,
            "includeDesc": str(include_desc).lower(),
        }
        result = self._make_request(url, params)
        if result.get("success"):
            result["reporter_code"] = reporter_code
            result["partner_code"] = partner_code
            result["period"] = period
        return result

    def get_trade_matrix(
        self,
        reporter_code: Union[str, int],
        period: str,
        type_code: str = "C",
        freq_code: str = "A",
    ) -> Dict[str, Any]:
        """
        Get the world trade matrix with estimates for missing data.
        Useful for constructing a complete bilateral trade network.
        Requires subscription key.

        Args:
            reporter_code: Country UN code
            period: 'YYYY' or 'YYYYMM'
            type_code: 'C' or 'S'
            freq_code: 'A' or 'M'
        """
        if not self.subscription_key:
            return ComtradeError(
                "/data/v1/getTradeMatrix",
                "Trade matrix endpoint requires a subscription key."
            ).to_dict()

        url = f"{BASE_URL_DATA}/getTradeMatrix/{type_code}/{freq_code}/TM"
        params = {
            "reporterCode": reporter_code,
            "period": period,
        }
        return self._make_request(url, params)

    # ==================== DATA AVAILABILITY ====================

    def get_data_availability(
        self,
        reporter_code: Optional[Union[str, int]] = None,
        type_code: str = "C",
        freq_code: str = "A",
        cl_code: str = "HS",
        published_date_from: Optional[str] = None,
        published_date_to: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Check which reporter/period combinations have data available.

        Args:
            reporter_code: Country code to check (None = all countries)
            type_code: 'C' or 'S'
            freq_code: 'A' or 'M'
            cl_code: Classification scheme
            published_date_from: Filter by publish date start 'YYYY-MM-DD'
            published_date_to: Filter by publish date end 'YYYY-MM-DD'
        """
        base = BASE_URL_DATA if self.subscription_key else BASE_URL_PUBLIC
        url = f"{base}/getDa/{type_code}/{freq_code}/{cl_code}"
        params = {
            "reporterCode": reporter_code,
            "publishedDateFrom": published_date_from,
            "publishedDateTo": published_date_to,
        }
        result = self._make_request(url, params)
        if result.get("success"):
            result["type_code"] = type_code
            result["freq_code"] = freq_code
            result["cl_code"] = cl_code
        return result

    def get_live_updates(self) -> Dict[str, Any]:
        """
        Get information about recent data releases and updates.
        Shows which countries and periods were recently published.
        """
        url = f"{BASE_URL_PUBLIC}/getLiveUpdate"
        return self._make_request(url, {})

    # ==================== REFERENCE / METADATA ====================

    def list_references(self) -> Dict[str, Any]:
        """
        List all available reference/lookup tables.
        Returns names of datasets like 'reporter', 'partner', 'cmd', 'flow', etc.
        """
        url = "https://comtradeapi.un.org/files/v1/app/reference/ListofReferences.json"
        return self._make_request(url, {})

    def get_reference(self, category: str) -> Dict[str, Any]:
        """
        Get a specific reference/lookup table.

        Args:
            category: Reference table name. Common values:
                'reporter'         — All reporting countries with UN codes
                'partner'          — All partner countries with UN codes
                'HS'               — Harmonized System commodity codes (latest)
                'H0'...'H6'        — HS revisions 1992–2022
                'S1','S2','S3','S4'— SITC revision commodity codes
                'B4','B5'          — BEC commodity codes
                'flow'             — Trade flow types (X, M, RX, RM)
                'mot'              — Modes of transport
                'customs'          — Customs procedure codes
                'freq'             — Frequency codes
                'qtyunit'          — Quantity unit codes
        """
        url = REFERENCE_URLS.get(category)
        if not url:
            return ComtradeError(
                "get_reference",
                f"Unknown category '{category}'. Valid: {list(REFERENCE_URLS.keys())}"
            ).to_dict()
        result = self._make_request(url, {})
        if result.get("success"):
            result["category"] = category
        return result

    def get_metadata(
        self,
        reporter_code: Optional[Union[str, int]] = None,
        type_code: str = "C",
        freq_code: str = "A",
        cl_code: str = "HS",
    ) -> Dict[str, Any]:
        """
        Get metadata and publication notes for a dataset.

        Args:
            reporter_code: Country code (None = all)
            type_code: 'C' or 'S'
            freq_code: 'A' or 'M'
            cl_code: Classification
        """
        base = BASE_URL_DATA if self.subscription_key else BASE_URL_PUBLIC
        url = f"{base}/getMetadata/{type_code}/{freq_code}/{cl_code}"
        params = {"reporterCode": reporter_code}
        return self._make_request(url, params)

    # ==================== HIGH-LEVEL CONVENIENCE METHODS ====================

    def get_country_exports(
        self,
        country_iso3: str,
        year: Union[str, int],
        top_n: int = 500,
        cl_code: str = "HS",
    ) -> Dict[str, Any]:
        """
        Convenience: Get all exports for a country in a given year.

        Args:
            country_iso3: ISO3 country code e.g. 'USA', 'CHN', 'DEU'
            year: Year e.g. 2023
            top_n: Max records (500 for free tier)
            cl_code: 'HS' or SITC revision
        """
        code = COUNTRY_CODES.get(country_iso3.upper())
        if code is None:
            return ComtradeError("get_country_exports", f"Unknown ISO3 code: {country_iso3}. Use get_reference('reporter') for valid codes.").to_dict()

        result = self.get_trade_data(
            reporter_code=code,
            period=str(year),
            cmd_code="TOTAL",
            flow_code="X",
            max_records=top_n,
            cl_code=cl_code,
        )
        if result.get("success"):
            result["country"] = country_iso3
            result["year"] = str(year)
            result["flow"] = "exports"
        return result

    def get_country_imports(
        self,
        country_iso3: str,
        year: Union[str, int],
        top_n: int = 500,
        cl_code: str = "HS",
    ) -> Dict[str, Any]:
        """
        Convenience: Get all imports for a country in a given year.

        Args:
            country_iso3: ISO3 country code e.g. 'USA', 'CHN', 'DEU'
            year: Year e.g. 2023
            top_n: Max records
            cl_code: Classification
        """
        code = COUNTRY_CODES.get(country_iso3.upper())
        if code is None:
            return ComtradeError("get_country_imports", f"Unknown ISO3 code: {country_iso3}. Use get_reference('reporter') for valid codes.").to_dict()

        result = self.get_trade_data(
            reporter_code=code,
            period=str(year),
            cmd_code="TOTAL",
            flow_code="M",
            max_records=top_n,
            cl_code=cl_code,
        )
        if result.get("success"):
            result["country"] = country_iso3
            result["year"] = str(year)
            result["flow"] = "imports"
        return result

    def get_bilateral_trade(
        self,
        reporter_iso3: str,
        partner_iso3: str,
        year: Union[str, int],
        cl_code: str = "HS",
    ) -> Dict[str, Any]:
        """
        Convenience: Get trade between two countries in a given year (both imports and exports).

        Args:
            reporter_iso3: Reporting country ISO3 e.g. 'USA'
            partner_iso3: Partner country ISO3 e.g. 'CHN'
            year: Year e.g. 2023
            cl_code: Classification
        """
        r_code = COUNTRY_CODES.get(reporter_iso3.upper())
        p_code = COUNTRY_CODES.get(partner_iso3.upper())

        if r_code is None:
            return ComtradeError("get_bilateral_trade", f"Unknown ISO3 code: {reporter_iso3}").to_dict()
        if p_code is None:
            return ComtradeError("get_bilateral_trade", f"Unknown ISO3 code: {partner_iso3}").to_dict()

        result = self.get_trade_data(
            reporter_code=r_code,
            partner_code=p_code,
            period=str(year),
            cmd_code="TOTAL",
            flow_code="X,M",
        )
        if result.get("success"):
            result["reporter"] = reporter_iso3
            result["partner"] = partner_iso3
            result["year"] = str(year)
        return result

    def get_commodity_trade(
        self,
        commodity_code: str,
        year: Union[str, int],
        reporter_code: Union[str, int] = "all",
        flow_code: str = "X,M",
        cl_code: str = "HS",
        max_records: int = 500,
    ) -> Dict[str, Any]:
        """
        Convenience: Get trade data for a specific commodity across all/selected reporters.

        Args:
            commodity_code: HS code e.g. '27' (mineral fuels), '8703' (cars), '0101' (horses)
            year: Year e.g. 2023
            reporter_code: Country code or 'all'
            flow_code: 'X', 'M', or 'X,M'
            cl_code: Classification
            max_records: Max results
        """
        result = self.get_trade_data(
            reporter_code=reporter_code,
            period=str(year),
            cmd_code=commodity_code,
            flow_code=flow_code,
            cl_code=cl_code,
            max_records=max_records,
        )
        if result.get("success"):
            result["commodity_code"] = commodity_code
            result["commodity_desc"] = HS_CHAPTERS.get(commodity_code, "")
            result["year"] = str(year)
        return result

    def get_trade_trend(
        self,
        reporter_iso3: str,
        partner_iso3: Optional[str] = None,
        start_year: int = 2018,
        end_year: Optional[int] = None,
        cmd_code: str = "TOTAL",
        flow_code: str = "X,M",
        cl_code: str = "HS",
    ) -> Dict[str, Any]:
        """
        Convenience: Get multi-year trade trend for a country/pair.
        Note: Free tier returns max 500 records across all years combined.

        Args:
            reporter_iso3: Reporting country ISO3 e.g. 'USA'
            partner_iso3: Optional partner ISO3 (None = all partners)
            start_year: Start year e.g. 2018
            end_year: End year (defaults to last year)
            cmd_code: Commodity code
            flow_code: Trade flow
            cl_code: Classification
        """
        if end_year is None:
            end_year = datetime.now(timezone.utc).year - 1

        r_code = COUNTRY_CODES.get(reporter_iso3.upper())
        if r_code is None:
            return ComtradeError("get_trade_trend", f"Unknown ISO3 code: {reporter_iso3}").to_dict()

        p_code = None
        if partner_iso3:
            p_code = COUNTRY_CODES.get(partner_iso3.upper())
            if p_code is None:
                return ComtradeError("get_trade_trend", f"Unknown ISO3 code: {partner_iso3}").to_dict()

        # Build comma-separated period string
        periods = ",".join(str(y) for y in range(start_year, end_year + 1))

        # Public preview endpoint does not support multi-year comma-separated periods.
        # Without a subscription key, fetch year by year and merge.
        if not self.subscription_key:
            all_records = []
            errors = []
            for year in range(start_year, end_year + 1):
                r = self.get_trade_data(
                    reporter_code=r_code,
                    period=str(year),
                    cmd_code=cmd_code,
                    flow_code=flow_code,
                    partner_code=p_code,
                    cl_code=cl_code,
                    max_records=500,
                )
                if r.get("success"):
                    all_records.extend(r.get("data", []))
                else:
                    errors.append({"year": year, "error": r.get("error")})
            return {
                "success": True,
                "reporter": reporter_iso3,
                "partner": partner_iso3,
                "start_year": start_year,
                "end_year": end_year,
                "data": all_records,
                "count": len(all_records),
                "errors": errors,
                "note": "Multi-year trend fetched year-by-year (no subscription key). Each year capped at 500 records.",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }

        result = self.get_trade_data(
            reporter_code=r_code,
            period=periods,
            cmd_code=cmd_code,
            flow_code=flow_code,
            partner_code=p_code,
            cl_code=cl_code,
            max_records=500,
        )
        if result.get("success"):
            result["reporter"] = reporter_iso3
            result["partner"] = partner_iso3
            result["start_year"] = start_year
            result["end_year"] = end_year
        return result

    def get_top_trading_partners(
        self,
        reporter_iso3: str,
        year: Union[str, int],
        flow_code: str = "X",
        max_records: int = 500,
    ) -> Dict[str, Any]:
        """
        Convenience: Get a country's top trading partners for exports or imports.

        Args:
            reporter_iso3: Reporting country ISO3 e.g. 'USA'
            year: Year e.g. 2023
            flow_code: 'X' = exports, 'M' = imports
            max_records: Max records (caps at 500 without key)
        """
        code = COUNTRY_CODES.get(reporter_iso3.upper())
        if code is None:
            return ComtradeError("get_top_trading_partners", f"Unknown ISO3 code: {reporter_iso3}").to_dict()

        result = self.get_trade_data(
            reporter_code=code,
            period=str(year),
            cmd_code="TOTAL",
            flow_code=flow_code,
            max_records=max_records,
            include_desc=True,
        )
        if result.get("success"):
            result["reporter"] = reporter_iso3
            result["year"] = str(year)
            result["flow"] = "exports" if flow_code == "X" else "imports"
        return result

    def get_energy_trade(
        self,
        reporter_iso3: str,
        year: Union[str, int],
        flow_code: str = "X,M",
    ) -> Dict[str, Any]:
        """
        Convenience: Get energy commodity trade (HS Chapter 27 — mineral fuels, oil, gas, coal).

        Args:
            reporter_iso3: Country ISO3
            year: Year
            flow_code: 'X', 'M', or 'X,M'
        """
        return self.get_commodity_trade(
            commodity_code="27",
            year=year,
            reporter_code=COUNTRY_CODES.get(reporter_iso3.upper(), reporter_iso3),
            flow_code=flow_code,
        )

    def get_technology_trade(
        self,
        reporter_iso3: str,
        year: Union[str, int],
        flow_code: str = "X,M",
    ) -> Dict[str, Any]:
        """
        Convenience: Get technology trade (HS 84 = machinery/nuclear reactors, HS 85 = electronics).
        Fetches chapters 84 and 85 separately (public API doesn't support comma-separated cmdCode)
        and merges results.

        Args:
            reporter_iso3: Country ISO3
            year: Year
            flow_code: 'X', 'M', or 'X,M'
        """
        code = COUNTRY_CODES.get(reporter_iso3.upper(), reporter_iso3)
        all_records = []
        errors = []
        for ch in ["84", "85"]:
            r = self.get_trade_data(
                reporter_code=code,
                period=str(year),
                cmd_code=ch,
                flow_code=flow_code,
            )
            if r.get("success"):
                all_records.extend(r.get("data", []))
            else:
                errors.append({"chapter": ch, "error": r.get("error")})

        return {
            "success": True,
            "reporter": reporter_iso3,
            "year": str(year),
            "sector": "technology",
            "hs_chapters": ["84 - Machinery & Nuclear Reactors", "85 - Electrical Machinery & Electronics"],
            "data": all_records,
            "count": len(all_records),
            "errors": errors,
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_agricultural_trade(
        self,
        reporter_iso3: str,
        year: Union[str, int],
        flow_code: str = "X,M",
    ) -> Dict[str, Any]:
        """
        Convenience: Get agricultural commodity trade (HS 01–24 = food and agriculture).
        Uses HS chapter 01 (aggregated level covers food/agriculture in TOTAL),
        or fetches chapter by chapter if subscription key available.

        Args:
            reporter_iso3: Country ISO3
            year: Year
            flow_code: 'X', 'M', or 'X,M'
        """
        code = COUNTRY_CODES.get(reporter_iso3.upper(), reporter_iso3)
        agri_chapters = [f"{i:02d}" for i in range(1, 25)]  # HS 01-24

        # With key: pass comma-separated. Without key: fetch per-chapter (expensive).
        # As a practical default, fetch 3 most important chapters individually.
        if self.subscription_key:
            cmd_code = ",".join(agri_chapters)
            result = self.get_trade_data(
                reporter_code=code, period=str(year),
                cmd_code=cmd_code, flow_code=flow_code, max_records=10000,
            )
            if result.get("success"):
                result["reporter"] = reporter_iso3
                result["sector"] = "agriculture"
            return result
        else:
            # Without key: fetch key agri chapters (cereals=10, oilseeds=12, meat=02, dairy=04)
            key_chapters = ["02", "04", "10", "12", "15", "17", "22", "23"]
            all_records = []
            errors = []
            for ch in key_chapters:
                r = self.get_trade_data(
                    reporter_code=code, period=str(year),
                    cmd_code=ch, flow_code=flow_code,
                )
                if r.get("success"):
                    all_records.extend(r.get("data", []))
                else:
                    errors.append({"chapter": ch, "error": r.get("error")})
            return {
                "success": True,
                "reporter": reporter_iso3,
                "year": str(year),
                "sector": "agriculture",
                "hs_chapters_fetched": key_chapters,
                "note": "Without subscription key, only key agri chapters fetched (02, 04, 10, 12, 15, 17, 22, 23). Subscribe for all 24 chapters.",
                "data": all_records,
                "count": len(all_records),
                "errors": errors,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }

    def get_pharmaceutical_trade(
        self,
        reporter_iso3: str,
        year: Union[str, int],
        flow_code: str = "X,M",
    ) -> Dict[str, Any]:
        """
        Convenience: Get pharmaceutical trade (HS Chapter 30).

        Args:
            reporter_iso3: Country ISO3
            year: Year
            flow_code: 'X', 'M', or 'X,M'
        """
        return self.get_commodity_trade(
            commodity_code="30",
            year=year,
            reporter_code=COUNTRY_CODES.get(reporter_iso3.upper(), reporter_iso3),
            flow_code=flow_code,
        )

    def get_country_codes(self) -> Dict[str, Any]:
        """
        Convenience: Get UN numeric country codes for all reporters.
        Returns the full reporter reference table including ISO3 codes.
        Also includes the built-in quick-reference dict for common countries.
        """
        result = self.get_reference("reporter")
        if result.get("success"):
            result["note"] = "Use the 'id' field as reporterCode in API calls"
            result["quick_reference"] = COUNTRY_CODES
        return result

    def get_iso_to_code_map(self) -> Dict[str, Any]:
        """
        Convenience: Get ISO3 → UN numeric code mapping for all countries.
        """
        url = f"{BASE_URL_FILES}/country_area_code_iso.json"
        result = self._make_request(url, {})
        if result.get("success"):
            result["note"] = "Maps ISO3 alpha codes to UN numeric reporterCode values"
        return result

    def get_hs_codes(self, level: str = "HS") -> Dict[str, Any]:
        """
        Convenience: Get Harmonized System commodity codes reference table.

        Args:
            level: 'HS' for full HS, or specific revision e.g. 'H0','H1','H2','H3','H4','H5','H6'
        """
        return self.get_reference(level)


# ==================== MAIN CLI ENTRYPOINT ====================

def main():
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "No command provided",
            "available_commands": [
                "trade_data <reporter_code> <period> [flow_code] [cmd_code] [freq_code] [cl_code] [max_records]",
                "tariffline <reporter_code> <period> [flow_code] [cmd_code]",
                "trade_balance <reporter_code> <period> [cmd_code]",
                "bilateral <reporter_code> <partner_code> <period> [cmd_code]",
                "data_availability [reporter_code] [type_code] [freq_code] [cl_code]",
                "live_updates",
                "list_references",
                "get_reference <category>",
                "metadata [reporter_code] [type_code] [freq_code] [cl_code]",
                "country_exports <iso3> <year>",
                "country_imports <iso3> <year>",
                "bilateral_trade <reporter_iso3> <partner_iso3> <year>",
                "commodity_trade <hs_code> <year> [reporter_code] [flow_code]",
                "trade_trend <reporter_iso3> <start_year> [end_year] [partner_iso3] [cmd_code]",
                "top_partners <reporter_iso3> <year> [flow_code]",
                "energy_trade <reporter_iso3> <year> [flow_code]",
                "tech_trade <reporter_iso3> <year> [flow_code]",
                "agri_trade <reporter_iso3> <year> [flow_code]",
                "pharma_trade <reporter_iso3> <year> [flow_code]",
                "country_codes",
                "hs_codes [classification]",
            ],
            "notes": [
                "Without UN_COMTRADE_API_KEY: max 500 records per call (preview endpoints)",
                "With UN_COMTRADE_API_KEY: 500 calls/day, up to 100,000 records per call",
                "Register free at: https://comtradedeveloper.un.org/",
                "flow_code: X=exports, M=imports, RX=re-exports, RM=re-imports",
                "freq_code: A=annual, M=monthly",
                "cl_code: HS, S1, S2, S3, S4, B4, B5",
                "period: YYYY for annual, YYYYMM for monthly",
            ],
            "common_country_codes": COUNTRY_CODES,
            "hs_chapter_reference": HS_CHAPTERS,
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = UNComtradeWrapper()

    try:
        if command == "trade_data":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py trade_data <reporter_code> <period> [flow_code] [cmd_code] [freq_code] [cl_code] [max_records]"}))
                sys.exit(1)
            reporter_code = sys.argv[2]
            period        = sys.argv[3]
            flow_code     = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "X,M"
            cmd_code      = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else "TOTAL"
            freq_code     = sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] else "A"
            cl_code       = sys.argv[7] if len(sys.argv) > 7 and sys.argv[7] else "HS"
            max_records   = int(sys.argv[8]) if len(sys.argv) > 8 and sys.argv[8] else 500
            result = wrapper.get_trade_data(
                reporter_code=reporter_code, period=period, flow_code=flow_code,
                cmd_code=cmd_code, freq_code=freq_code, cl_code=cl_code, max_records=max_records,
            )
            print(json.dumps(result, indent=2))

        elif command == "tariffline":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py tariffline <reporter_code> <period> [flow_code] [cmd_code]"}))
                sys.exit(1)
            reporter_code = sys.argv[2]
            period        = sys.argv[3]
            flow_code     = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "X,M"
            cmd_code      = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else "TOTAL"
            result = wrapper.get_tariffline_data(reporter_code=reporter_code, period=period, flow_code=flow_code, cmd_code=cmd_code)
            print(json.dumps(result, indent=2))

        elif command == "trade_balance":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py trade_balance <reporter_code> <period> [cmd_code]"}))
                sys.exit(1)
            reporter_code = sys.argv[2]
            period        = sys.argv[3]
            cmd_code      = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "TOTAL"
            result = wrapper.get_trade_balance(reporter_code=reporter_code, period=period, cmd_code=cmd_code)
            print(json.dumps(result, indent=2))

        elif command == "bilateral":
            if len(sys.argv) < 5:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py bilateral <reporter_code> <partner_code> <period> [cmd_code]"}))
                sys.exit(1)
            reporter_code = sys.argv[2]
            partner_code  = sys.argv[3]
            period        = sys.argv[4]
            cmd_code      = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else "TOTAL"
            result = wrapper.get_bilateral_data(reporter_code=reporter_code, partner_code=partner_code, period=period, cmd_code=cmd_code)
            print(json.dumps(result, indent=2))

        elif command == "data_availability":
            reporter_code = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
            type_code     = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else "C"
            freq_code     = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "A"
            cl_code       = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else "HS"
            result = wrapper.get_data_availability(reporter_code=reporter_code, type_code=type_code, freq_code=freq_code, cl_code=cl_code)
            print(json.dumps(result, indent=2))

        elif command == "live_updates":
            result = wrapper.get_live_updates()
            print(json.dumps(result, indent=2))

        elif command == "list_references":
            result = wrapper.list_references()
            print(json.dumps(result, indent=2))

        elif command == "get_reference":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py get_reference <category>  (e.g. reporter, partner, HS, flow)"}))
                sys.exit(1)
            result = wrapper.get_reference(sys.argv[2])
            print(json.dumps(result, indent=2))

        elif command == "metadata":
            reporter_code = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
            type_code     = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else "C"
            freq_code     = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "A"
            cl_code       = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else "HS"
            result = wrapper.get_metadata(reporter_code=reporter_code, type_code=type_code, freq_code=freq_code, cl_code=cl_code)
            print(json.dumps(result, indent=2))

        elif command == "country_exports":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py country_exports <iso3> <year>  e.g. USA 2023"}))
                sys.exit(1)
            result = wrapper.get_country_exports(country_iso3=sys.argv[2], year=sys.argv[3])
            print(json.dumps(result, indent=2))

        elif command == "country_imports":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py country_imports <iso3> <year>  e.g. CHN 2023"}))
                sys.exit(1)
            result = wrapper.get_country_imports(country_iso3=sys.argv[2], year=sys.argv[3])
            print(json.dumps(result, indent=2))

        elif command == "bilateral_trade":
            if len(sys.argv) < 5:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py bilateral_trade <reporter_iso3> <partner_iso3> <year>  e.g. USA CHN 2023"}))
                sys.exit(1)
            result = wrapper.get_bilateral_trade(reporter_iso3=sys.argv[2], partner_iso3=sys.argv[3], year=sys.argv[4])
            print(json.dumps(result, indent=2))

        elif command == "commodity_trade":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py commodity_trade <hs_code> <year> [reporter_code] [flow_code]  e.g. 27 2023 842 X"}))
                sys.exit(1)
            hs_code       = sys.argv[2]
            year          = sys.argv[3]
            reporter_code = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "all"
            flow_code     = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else "X,M"
            result = wrapper.get_commodity_trade(commodity_code=hs_code, year=year, reporter_code=reporter_code, flow_code=flow_code)
            print(json.dumps(result, indent=2))

        elif command == "trade_trend":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py trade_trend <reporter_iso3> <start_year> [end_year] [partner_iso3] [cmd_code]  e.g. USA 2018 2023 CHN TOTAL"}))
                sys.exit(1)
            reporter_iso3 = sys.argv[2]
            start_year    = int(sys.argv[3])
            end_year      = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else None
            partner_iso3  = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] else None
            cmd_code      = sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] else "TOTAL"
            result = wrapper.get_trade_trend(
                reporter_iso3=reporter_iso3, start_year=start_year, end_year=end_year,
                partner_iso3=partner_iso3, cmd_code=cmd_code,
            )
            print(json.dumps(result, indent=2))

        elif command == "top_partners":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py top_partners <reporter_iso3> <year> [flow_code]  e.g. USA 2023 X"}))
                sys.exit(1)
            flow_code = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "X"
            result = wrapper.get_top_trading_partners(reporter_iso3=sys.argv[2], year=sys.argv[3], flow_code=flow_code)
            print(json.dumps(result, indent=2))

        elif command == "energy_trade":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py energy_trade <reporter_iso3> <year> [flow_code]  e.g. DEU 2023 M"}))
                sys.exit(1)
            flow_code = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "X,M"
            result = wrapper.get_energy_trade(reporter_iso3=sys.argv[2], year=sys.argv[3], flow_code=flow_code)
            print(json.dumps(result, indent=2))

        elif command == "tech_trade":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py tech_trade <reporter_iso3> <year> [flow_code]  e.g. USA 2023 X"}))
                sys.exit(1)
            flow_code = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "X,M"
            result = wrapper.get_technology_trade(reporter_iso3=sys.argv[2], year=sys.argv[3], flow_code=flow_code)
            print(json.dumps(result, indent=2))

        elif command == "agri_trade":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py agri_trade <reporter_iso3> <year> [flow_code]  e.g. BRA 2023 X"}))
                sys.exit(1)
            flow_code = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "X,M"
            result = wrapper.get_agricultural_trade(reporter_iso3=sys.argv[2], year=sys.argv[3], flow_code=flow_code)
            print(json.dumps(result, indent=2))

        elif command == "pharma_trade":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: un_comtrade_data.py pharma_trade <reporter_iso3> <year> [flow_code]  e.g. DEU 2023 X"}))
                sys.exit(1)
            flow_code = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else "X,M"
            result = wrapper.get_pharmaceutical_trade(reporter_iso3=sys.argv[2], year=sys.argv[3], flow_code=flow_code)
            print(json.dumps(result, indent=2))

        elif command == "country_codes":
            result = wrapper.get_country_codes()
            print(json.dumps(result, indent=2))

        elif command == "iso_to_code_map":
            result = wrapper.get_iso_to_code_map()
            print(json.dumps(result, indent=2))

        elif command == "hs_codes":
            classification = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else "HS"
            result = wrapper.get_hs_codes(level=classification)
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "success": False,
                "error": f"Unknown command: {command}",
                "tip": "Run without arguments to see all available commands",
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
