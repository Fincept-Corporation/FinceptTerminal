# -*- coding: utf-8 -*-
# oecd_provider.py

import asyncio
import aiohttp
from datetime import datetime, date
from typing import Any, Dict, Optional, List, Literal, Union
from warnings import warn
import ssl
from io import StringIO
import pandas as pd

from fincept_terminal.Utils.Logging.logger import info, debug, warning, error, operation, monitor_performance
from fincept_terminal.DatabaseConnector.DataSources.oced_data.constants import (
    COUNTRY_TO_CODE_GDP, CODE_TO_COUNTRY_GDP,
    COUNTRY_TO_CODE_CPI, CODE_TO_COUNTRY_CPI,
    COUNTRY_TO_CODE_UNEMPLOYMENT, CODE_TO_COUNTRY_UNEMPLOYMENT,
    COUNTRY_TO_CODE_IR, CODE_TO_COUNTRY_IR,
    COUNTRY_TO_CODE_RGDP, CODE_TO_COUNTRY_RGDP,
    COUNTRY_TO_CODE_CLI, CODE_TO_COUNTRY_CLI,
    COUNTRY_TO_CODE_SHARES, CODE_TO_COUNTRY_SHARES,
    COUNTRY_TO_CODE_GDP_FORECAST, CODE_TO_COUNTRY_GDP_FORECAST
)


class OECDProvider:
    """OECD data provider with complete API integration using OpenBB constants"""

    def __init__(self, rate_limit: int = 5):
        self.base_url = "https://sdmx.oecd.org/public/rest/data"
        self.rate_limit = rate_limit
        self._session = None

    async def _get_session(self) -> aiohttp.ClientSession:
        """Get or create aiohttp session with SSL configuration"""
        if self._session is None or self._session.closed:
            # Create SSL context for OECD compatibility
            ssl_context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH)
            ssl_context.options |= 0x4  # OP_LEGACY_SERVER_CONNECT

            connector = aiohttp.TCPConnector(
                limit=10,
                ssl=ssl_context
            )

            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=30),
                connector=connector
            )
        return self._session

    async def _make_request(self, url: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
        """Make API request with common error handling"""
        try:
            session = await self._get_session()
            headers = {"Accept": "application/vnd.sdmx.data+csv; charset=utf-8"}

            async with session.get(url, params=params, headers=headers) as response:
                if response.status == 200:
                    text = await response.text()
                    return {"success": True, "data": text}
                else:
                    error(f"OECD API HTTP error: {response.status}", module="OECDProvider")
                    return {"success": False, "error": f"HTTP {response.status}", "source": "oecd"}
        except Exception as e:
            error(f"OECD API request error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    def _parse_csv_response(self, csv_text: str) -> pd.DataFrame:
        """Parse CSV response from OECD API"""
        try:
            df = pd.read_csv(StringIO(csv_text))
            if df.empty:
                return pd.DataFrame()

            # Standard column renaming
            column_mapping = {
                "REF_AREA": "country",
                "TIME_PERIOD": "date",
                "OBS_VALUE": "value"
            }

            for old_col, new_col in column_mapping.items():
                if old_col in df.columns:
                    df = df.rename(columns={old_col: new_col})

            return df
        except Exception as e:
            error(f"Error parsing CSV response: {str(e)}", module="OECDProvider")
            return pd.DataFrame()

    def _parse_date(self, date_str: str) -> str:
        """Parse OECD date format to standard format"""
        try:
            if "Q" in date_str:  # Quarterly: 2023-Q1
                year, quarter = date_str.split("-")
                quarter_num = int(quarter[1])
                month = quarter_num * 3
                return f"{year}-{month:02d}-01"
            elif len(date_str) == 4:  # Annual: 2023
                return f"{date_str}-12-31"
            elif len(date_str) == 7:  # Monthly: 2023-01
                return f"{date_str}-01"
            else:
                return date_str
        except:
            return date_str

    def _validate_countries(self, countries: str, country_mapping: Dict[str, str]) -> str:
        """Validate and convert countries using OpenBB constants"""
        if countries == "all":
            return ""

        country_list = [c.strip().lower() for c in countries.split(",")]
        valid_codes = []

        for country in country_list:
            if country in country_mapping:
                valid_codes.append(country_mapping[country])
            else:
                warning(f"Country '{country}' not supported for this indicator. Skipping...")

        if not valid_codes:
            raise ValueError(f"No valid countries found in: {countries}")

        return "+".join(valid_codes)

    def get_available_countries(self, indicator: str) -> List[str]:
        """Get list of available countries for a specific indicator"""
        mapping = {
            "gdp_nominal": COUNTRY_TO_CODE_GDP,
            "gdp_real": COUNTRY_TO_CODE_GDP,
            "gdp_forecast": COUNTRY_TO_CODE_GDP_FORECAST,
            "cpi": COUNTRY_TO_CODE_CPI,
            "unemployment": COUNTRY_TO_CODE_UNEMPLOYMENT,
            "interest_rates": COUNTRY_TO_CODE_IR,
            "composite_leading_indicator": COUNTRY_TO_CODE_CLI,
            "house_price_index": COUNTRY_TO_CODE_RGDP,
            "share_price_index": COUNTRY_TO_CODE_SHARES
        }

        if indicator not in mapping:
            return []

        return list(mapping[indicator].keys())

    @monitor_performance
    async def get_gdp_nominal(self, countries: str = "united_states", frequency: str = "quarter",
                              start_date: str = None, end_date: str = None,
                              units: str = "level", price_base: str = "current_prices") -> Dict[str, Any]:
        """Get nominal GDP data"""
        try:
            with operation(f"OECD GDP nominal for {countries}"):
                freq = "Q" if frequency == "quarter" else "A"
                unit = "USD" if units == "level" else "INDICES" if units == "index" else "CAPITA"
                price = "V" if price_base == "current_prices" else "LR"

                if unit == "INDICES" and price == "V":
                    price = "DR"

                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_GDP)

                url = (f"{self.base_url}/OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA_EXPENDITURE_{unit},1.1"
                       f"/{freq}..{country_codes}.S1..B1GQ.....{price}..?")

                if start_date:
                    url += f"&startPeriod={start_date}"
                if end_date:
                    url += f"&endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly&format=csvfile"

                if units == "capita":
                    url = url.replace("B1GQ", "B1GQ_POP")

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                # Transform data
                df["country"] = df["country"].map(CODE_TO_COUNTRY_GDP)
                df["date"] = df["date"].apply(self._parse_date)

                if units == "level":
                    df["value"] = (df["value"].astype(float) * 1_000_000).astype("int64")

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "gdp_nominal",
                    "countries": countries,
                    "frequency": frequency,
                    "units": units,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"GDP nominal error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_gdp_real(self, countries: str = "united_states", frequency: str = "quarter",
                           start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get real GDP data (PPP-adjusted)"""
        try:
            with operation(f"OECD GDP real for {countries}"):
                freq = "Q" if frequency == "quarter" else "A"
                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_GDP)

                url = (f"{self.base_url}/OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA,1.1"
                       f"/{freq}..{country_codes}.S1..B1GQ._Z...USD_PPP.LR.LA.T0102?")

                if start_date:
                    url += f"&startPeriod={start_date}"
                if end_date:
                    url += f"&endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly&format=csvfile"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_GDP)
                df["date"] = df["date"].apply(self._parse_date)
                df["value"] = (df["value"].astype(float) * 1_000_000).astype("int64")

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "gdp_real",
                    "countries": countries,
                    "frequency": frequency,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"GDP real error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_gdp_forecast(self, countries: str = "all", frequency: str = "annual",
                               start_date: str = None, end_date: str = None,
                               units: str = "volume") -> Dict[str, Any]:
        """Get GDP forecast data"""
        try:
            with operation(f"OECD GDP forecast for {countries}"):
                freq = "Q" if frequency == "quarter" else "A"

                measure_dict = {
                    "current_prices": "GDP_USD",
                    "volume": "GDPV_USD",
                    "capita": "GDPVD_CAP",
                    "growth": "GDPV_ANNPCT",
                    "deflator": "PGDP"
                }
                measure = measure_dict.get(units, "GDPV_USD")

                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_GDP_FORECAST)

                url = (f"{self.base_url}/OECD.ECO.MAD,DSD_EO@DF_EO,1.1"
                       f"/{country_codes}.{measure}.{freq}?")

                if start_date:
                    url += f"startPeriod={start_date}"
                if end_date:
                    url += f"&endPeriod={end_date}" if start_date else f"endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly&format=csvfile"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_GDP_FORECAST)
                df["date"] = df["date"].apply(self._parse_date)

                if units == "growth":
                    df["value"] = df["value"].astype(float) / 100
                else:
                    df["value"] = df["value"].astype("int64")

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "gdp_forecast",
                    "countries": countries,
                    "frequency": frequency,
                    "units": units,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"GDP forecast error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_cpi(self, countries: str = "united_states", frequency: str = "monthly",
                      start_date: str = None, end_date: str = None,
                      transform: str = "index", harmonized: bool = False,
                      expenditure: str = "total") -> Dict[str, Any]:
        """Get Consumer Price Index data"""
        try:
            with operation(f"OECD CPI for {countries}"):
                methodology = "HICP" if harmonized else "N"
                freq = frequency[0].upper()

                units_map = {"index": "IX", "yoy": "PA", "mom": "PC", "period": "PC"}
                units = units_map.get(transform, "IX")

                expenditure_map = {
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
                    "services_less_housing": "SERVXCP041_042_0432",
                    "services": "SERV"
                }
                exp_code = expenditure_map.get(expenditure, "_T")

                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_CPI)

                url = (f"{self.base_url}/OECD.SDD.TPS,DSD_PRICES@DF_PRICES_ALL,1.0/"
                       f"{country_codes}.{freq}.{methodology}.CPI.{units}.{exp_code}.N.")

                if start_date or end_date:
                    url += "?"
                    if start_date:
                        url += f"startPeriod={start_date}"
                    if end_date:
                        url += f"&endPeriod={end_date}" if start_date else f"endPeriod={end_date}"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_CPI)
                df["date"] = df["date"].apply(self._parse_date)

                if transform in ("yoy", "mom", "period"):
                    df["value"] = df["value"].astype(float) / 100

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "cpi",
                    "countries": countries,
                    "frequency": frequency,
                    "transform": transform,
                    "expenditure": expenditure,
                    "harmonized": harmonized,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"CPI error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_unemployment(self, countries: str = "united_states", frequency: str = "monthly",
                               start_date: str = None, end_date: str = None,
                               sex: str = "total", age: str = "total",
                               seasonal_adjustment: bool = False) -> Dict[str, Any]:
        """Get unemployment rate data"""
        try:
            with operation(f"OECD unemployment for {countries}"):
                sex_map = {"total": "_T", "male": "M", "female": "F"}
                sex_code = sex_map.get(sex, "_T")

                age_map = {"total": "Y_GE15", "15-24": "Y15T24", "25+": "Y_GE25"}
                age_code = age_map.get(age, "Y_GE15")

                freq = frequency[0].upper()
                seasonal = "Y" if seasonal_adjustment else "N"

                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_UNEMPLOYMENT)

                url = (f"{self.base_url}/OECD.SDD.TPS,DSD_LFS@DF_IALFS_UNE_M,1.0/"
                       f"{country_codes}..._Z.{seasonal}.{sex_code}.{age_code}..{freq}")

                if start_date or end_date:
                    url += "?"
                    if start_date:
                        url += f"startPeriod={start_date}"
                    if end_date:
                        url += f"&endPeriod={end_date}" if start_date else f"endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_UNEMPLOYMENT)
                df["date"] = df["date"].apply(self._parse_date)
                df["value"] = df["value"].astype(float) / 100

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "unemployment",
                    "countries": countries,
                    "frequency": frequency,
                    "sex": sex,
                    "age": age,
                    "seasonal_adjustment": seasonal_adjustment,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"Unemployment error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_interest_rates(self, countries: str = "united_states", duration: str = "short",
                                 frequency: str = "monthly", start_date: str = None,
                                 end_date: str = None) -> Dict[str, Any]:
        """Get interest rates data"""
        try:
            with operation(f"OECD interest rates for {countries}"):
                duration_map = {"immediate": "IRSTCI", "short": "IR3TIB", "long": "IRLT"}
                duration_code = duration_map.get(duration, "IR3TIB")

                freq = frequency[0].upper()
                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_IR)

                url = (f"{self.base_url}/OECD.SDD.STES,DSD_KEI@DF_KEI,4.0"
                       f"/{country_codes}.{freq}.{duration_code}....?")

                if start_date:
                    url += f"startPeriod={start_date}"
                if end_date:
                    url += f"&endPeriod={end_date}" if start_date else f"endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_IR)
                df["date"] = df["date"].apply(self._parse_date)
                df["value"] = df["value"].astype(float) / 100

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "interest_rates",
                    "countries": countries,
                    "frequency": frequency,
                    "duration": duration,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"Interest rates error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_composite_leading_indicator(self, countries: str = "g20",
                                              start_date: str = None, end_date: str = None,
                                              adjustment: str = "amplitude",
                                              growth_rate: bool = False) -> Dict[str, Any]:
        """Get Composite Leading Indicator data"""
        try:
            with operation(f"OECD CLI for {countries}"):
                growth = "GY" if growth_rate else "IX"
                adjust = "AA" if adjustment == "amplitude" else "NOR"

                if growth == "GY":
                    adjust = ""

                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_CLI)

                url = (f"{self.base_url}/OECD.SDD.STES,DSD_STES@DF_CLI,4.1"
                       f"/{country_codes}.M.LI...{adjust}.{growth}..H")

                if start_date or end_date:
                    url += "?"
                    if start_date:
                        url += f"startPeriod={start_date}"
                    if end_date:
                        url += f"&endPeriod={end_date}" if start_date else f"endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly&format=csvfile"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_CLI)
                df["date"] = df["date"].apply(self._parse_date)

                if growth_rate:
                    df["value"] = df["value"].astype(float) / 100

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "composite_leading_indicator",
                    "countries": countries,
                    "adjustment": adjustment,
                    "growth_rate": growth_rate,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"CLI error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_house_price_index(self, countries: str = "united_states",
                                    frequency: str = "quarter", start_date: str = None,
                                    end_date: str = None, transform: str = "yoy") -> Dict[str, Any]:
        """Get House Price Index data"""
        try:
            with operation(f"OECD house price index for {countries}"):
                freq_map = {"monthly": "M", "quarter": "Q", "annual": "A"}
                freq = freq_map.get(frequency, "Q")

                transform_map = {"yoy": "PA", "period": "PC", "index": "IX"}
                trans = transform_map.get(transform, "PA")

                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_RGDP)

                url = (f"{self.base_url}/OECD.SDD.TPS,DSD_RHPI_TARGET@DF_RHPI_TARGET,1.0/"
                       f"COU.{country_codes}.{freq}.RHPI.{trans}....?")

                if start_date:
                    url += f"startPeriod={start_date}"
                if end_date:
                    url += f"&endPeriod={end_date}" if start_date else f"endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_RGDP)
                df["date"] = df["date"].apply(self._parse_date)

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "house_price_index",
                    "countries": countries,
                    "frequency": frequency,
                    "transform": transform,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"House price index error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    @monitor_performance
    async def get_share_price_index(self, countries: str = "united_states",
                                    frequency: str = "monthly", start_date: str = None,
                                    end_date: str = None) -> Dict[str, Any]:
        """Get Share Price Index data"""
        try:
            with operation(f"OECD share price index for {countries}"):
                freq_map = {"monthly": "M", "quarter": "Q", "annual": "A"}
                freq = freq_map.get(frequency, "M")

                country_codes = self._validate_countries(countries, COUNTRY_TO_CODE_SHARES)

                url = (f"{self.base_url}/OECD.SDD.STES,DSD_STES@DF_FINMARK,4.0/"
                       f"{country_codes}.{freq}.SHARE......?")

                if start_date:
                    url += f"startPeriod={start_date}"
                if end_date:
                    url += f"&endPeriod={end_date}" if start_date else f"endPeriod={end_date}"

                url += "&dimensionAtObservation=TIME_PERIOD&detail=dataonly"

                response = await self._make_request(url)
                if not response["success"]:
                    return response

                df = self._parse_csv_response(response["data"])
                if df.empty:
                    return {"success": False, "error": "No data found", "source": "oecd"}

                df["country"] = df["country"].map(CODE_TO_COUNTRY_SHARES)
                df["date"] = df["date"].apply(self._parse_date)

                return {
                    "success": True,
                    "source": "oecd",
                    "indicator": "share_price_index",
                    "countries": countries,
                    "frequency": frequency,
                    "data": df.to_dict("records"),
                    "fetched_at": datetime.now().isoformat()
                }

        except Exception as e:
            error(f"Share price index error: {str(e)}", module="OECDProvider")
            return {"success": False, "error": str(e), "source": "oecd"}

    async def close(self):
        """Close the aiohttp session"""
        if self._session and not self._session.closed:
            await self._session.close()

    def __del__(self):
        """Cleanup on deletion"""
        if hasattr(self, '_session') and self._session and not self._session.closed:
            asyncio.create_task(self._session.close())