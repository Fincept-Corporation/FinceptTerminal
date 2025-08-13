# -*- coding: utf-8 -*-
# imf_provider.py

import asyncio
import aiohttp
from datetime import datetime, date
from typing import Any, Dict, Optional, List, Union
from warnings import warn
import json
from pathlib import Path

try:
    from fincept_terminal.Utils.Logging.logger import info, debug, warning, error, operation, monitor_performance
except ImportError:
    # Fallback logging if module not available
    def info(msg, **kwargs):
        print(f"INFO: {msg}")


    def debug(msg, **kwargs):
        print(f"DEBUG: {msg}")


    def warning(msg, **kwargs):
        print(f"WARNING: {msg}")


    def error(msg, **kwargs):
        print(f"ERROR: {msg}")


    def operation(msg):
        class MockOperation:
            def __enter__(self): return self

            def __exit__(self, *args): pass

        return MockOperation()


    def monitor_performance(func):
        return func


class IMFProvider:
    """IMF data provider with complete API integration"""

    def __init__(self, rate_limit: int = 5):
        self.base_url = "http://dataservices.imf.org/REST/SDMX_JSON.svc/"
        self.portwatch_base = "https://services9.arcgis.com/weJ1QsnbMYJlCHdG/arcgis/rest/services"
        self.rate_limit = rate_limit
        self._session = None

    async def _get_session(self) -> aiohttp.ClientSession:
        """Get or create aiohttp session"""
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=30),
                connector=aiohttp.TCPConnector(limit=10)
            )
        return self._session

    async def _make_request(self, url: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        """Make API request with error handling"""
        try:
            session = await self._get_session()
            async with session.get(url, params=params) as response:
                if response.status == 200:
                    data = await response.json()
                    # Don't add success field - let the transform methods handle it
                    return data
                else:
                    error(f"IMF API HTTP error: {response.status}", module="IMFProvider")
                    return {"success": False, "error": f"HTTP {response.status}", "source": "imf"}
        except Exception as e:
            error(f"IMF API request error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    def _check_api_errors(self, data: Dict) -> Optional[Dict[str, Any]]:
        """Check for common API errors"""
        if "ErrorDetails" in data:
            return {"success": False, "error": data["ErrorDetails"].get("Message", "API Error"), "source": "imf"}
        return None

    @monitor_performance
    async def get_available_indicators(self, query: Optional[str] = None) -> Dict[str, Any]:
        """Get available IMF indicators"""
        try:
            with operation("IMF available indicators"):
                # This would require loading from static symbols file
                # For now, return preset symbols
                indicators = {
                    "IRFCL": "International Reserves and Foreign Currency Liquidity",
                    "FSI": "Financial Soundness Indicators",
                    "DOT": "Direction of Trade Statistics"
                }

                if query:
                    indicators = {k: v for k, v in indicators.items() if query.lower() in v.lower()}

                return {
                    "success": True,
                    "source": "imf",
                    "data": indicators,
                    "fetched_at": datetime.now().isoformat()
                }
        except Exception as e:
            error(f"IMF available indicators error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    @monitor_performance
    async def get_direction_of_trade(self, country: str, counterpart: str = "all",
                                     direction: str = "exports", frequency: str = "quarter",
                                     start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[
        str, Any]:
        """Get bilateral trade data"""
        try:
            with operation(f"IMF direction of trade {country}-{counterpart}"):
                # Map direction to IMF indicators
                indicators = {
                    "exports": "TXG_FOB_USD",
                    "imports": "TMG_CIF_USD",
                    "balance": "TBG_USD"
                }

                freq = {"annual": "A", "quarter": "Q", "month": "M"}
                f = freq.get(frequency, "Q")
                indicator = indicators.get(direction, "TXG_FOB_USD")

                date_range = ""
                if start_date and end_date:
                    date_range = f"?startPeriod={start_date}&endPeriod={end_date}"

                url = f"{self.base_url}CompactData/DOT/{f}.{country}.{indicator}.{counterpart}{date_range}"
                print(f"Trade URL: {url}")  # Debug line

                data = await self._make_request(url)
                print(f"Trade raw data: {data}")  # Debug line

                if not data.get("success", True):
                    return data

                return self._transform_dot_data(data, country, counterpart, direction)

        except Exception as e:
            error(f"IMF direction of trade error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    def _transform_dot_data(self, data: Dict, country: str, counterpart: str, direction: str) -> Dict[str, Any]:
        """Transform direction of trade data"""
        try:
            # Check if this is an error response first
            if data.get("success") is False:
                return data

            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            # Navigate the nested structure correctly
            compact_data = data.get("CompactData", {})
            dataset = compact_data.get("DataSet", {})
            series = dataset.get("Series", {})

            if not series:
                return {"success": False, "error": "No trade data found in response", "source": "imf"}

            # Series is a single object, not a list in this case
            obs = series.get("Obs", [])
            if isinstance(obs, dict):
                obs = [obs]

            result_data = []
            for o in obs:
                try:
                    value = o.get("@OBS_VALUE")
                    if value is not None:
                        result_data.append({
                            "date": o.get("@TIME_PERIOD"),
                            "value": float(value),
                            "country": country,
                            "counterpart": counterpart,
                            "direction": direction
                        })
                except (ValueError, TypeError):
                    continue

            if not result_data:
                return {"success": False, "error": f"No valid observations found for {country}-{counterpart}",
                        "source": "imf"}

            return {
                "success": True,
                "source": "imf",
                "data": result_data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming DOT data: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    @monitor_performance
    async def get_economic_indicators(self, symbol: str = "RAF_USD", country: str = "US",
                                      frequency: str = "quarter", start_date: Optional[str] = None,
                                      end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get economic indicators (IRFCL/FSI)"""
        try:
            with operation(f"IMF economic indicators {symbol}"):
                freq = {"annual": "A", "quarter": "Q", "month": "M"}
                f = freq.get(frequency, "Q")

                # Use actual IMF symbols
                dataset = "IRFCL" if any(x in symbol for x in ["RAF", "RAO", "RAC", "RAM"]) else "FSI"

                date_range = ""
                if start_date and end_date:
                    date_range = f"?startPeriod={start_date}&endPeriod={end_date}"

                # Use empty sector for IRFCL (monetary authorities default)
                sector = "" if dataset == "IRFCL" else ""
                url = f"{self.base_url}CompactData/{dataset}/{f}.{country}.{symbol}.{sector}{date_range}"

                data = await self._make_request(url)

                if not data.get("success", True):
                    return data

                return self._transform_economic_data(data, symbol, country)

        except Exception as e:
            error(f"IMF economic indicators error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    def _transform_economic_data(self, data: Dict, symbol: str, country: str) -> Dict[str, Any]:
        """Transform economic indicators data"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            series = data.get("CompactData", {}).get("DataSet", {}).get("Series", [])
            if not series:
                return {"success": False, "error": "No economic data found", "source": "imf"}

            if isinstance(series, dict):
                series = [series]

            result_data = []
            for s in series:
                obs = s.get("Obs", [])
                if isinstance(obs, dict):
                    obs = [obs]

                for o in obs:
                    result_data.append({
                        "date": o.get("@TIME_PERIOD"),
                        "value": float(o.get("@OBS_VALUE", 0)),
                        "symbol": s.get("@INDICATOR"),
                        "country": country
                    })

            return {
                "success": True,
                "source": "imf",
                "data": result_data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming economic data: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    @monitor_performance
    async def get_maritime_chokepoint_info(self) -> Dict[str, Any]:
        """Get static maritime chokepoint information"""
        try:
            with operation("IMF maritime chokepoint info"):
                url = f"{self.portwatch_base}/PortWatch_chokepoints_database/FeatureServer/0/query"
                params = {"outFields": "*", "where": "1=1", "f": "geojson"}

                data = await self._make_request(url, params)
                if not data.get("success", True):
                    return data

                return self._transform_chokepoint_info(data)

        except Exception as e:
            error(f"IMF chokepoint info error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    def _transform_chokepoint_info(self, data: Dict) -> Dict[str, Any]:
        """Transform chokepoint info data"""
        try:
            features = data.get("features", [])
            if not features:
                return {"success": False, "error": "No chokepoint data found", "source": "imf"}

            result_data = []
            for feature in features:
                props = feature.get("properties", {})
                result_data.append({
                    "chokepoint_code": props.get("portid"),
                    "name": props.get("portname"),
                    "latitude": props.get("lat"),
                    "longitude": props.get("lon"),
                    "vessel_count_total": props.get("vessel_count_total", 0),
                    "vessel_count_tanker": props.get("vessel_count_tanker", 0),
                    "vessel_count_container": props.get("vessel_count_container", 0)
                })

            return {
                "success": True,
                "source": "imf",
                "data": result_data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming chokepoint info: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    @monitor_performance
    async def get_maritime_chokepoint_volume(self, chokepoint: Optional[str] = None,
                                             start_date: Optional[str] = None,
                                             end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get daily chokepoint volume data"""
        try:
            with operation(f"IMF chokepoint volume {chokepoint or 'all'}"):
                url = f"{self.portwatch_base}/Daily_Chokepoints_Data/FeatureServer/0/query"

                where_clause = "1=1"
                if chokepoint:
                    where_clause += f" AND portid = '{chokepoint}'"
                if start_date and end_date:
                    where_clause += f" AND date >= TIMESTAMP '{start_date} 00:00:00' AND date <= TIMESTAMP '{end_date} 00:00:00'"

                params = {
                    "where": where_clause,
                    "outFields": "*",
                    "orderByFields": "date",
                    "f": "json"
                }

                data = await self._make_request(url, params)
                if not data.get("success", True):
                    return data

                return self._transform_chokepoint_volume(data)

        except Exception as e:
            error(f"IMF chokepoint volume error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    def _transform_chokepoint_volume(self, data: Dict) -> Dict[str, Any]:
        """Transform chokepoint volume data"""
        try:
            features = data.get("features", [])
            if not features:
                return {"success": False, "error": "No chokepoint volume data found", "source": "imf"}

            result_data = []
            for feature in features:
                attrs = feature.get("attributes", {})
                # Convert date from year/month/day to ISO format
                year = attrs.get("year")
                month = attrs.get("month")
                day = attrs.get("day")
                date_str = f"{year}-{month:02d}-{day:02d}" if all([year, month, day]) else None

                result_data.append({
                    "date": date_str,
                    "chokepoint": attrs.get("portname"),
                    "vessels_total": attrs.get("n_total", 0),
                    "vessels_cargo": attrs.get("n_cargo", 0),
                    "vessels_tanker": attrs.get("n_tanker", 0),
                    "capacity_total": attrs.get("capacity", 0)
                })

            return {
                "success": True,
                "source": "imf",
                "data": result_data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming chokepoint volume: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    @monitor_performance
    async def get_port_info(self, continent: Optional[str] = None, country: Optional[str] = None,
                            limit: Optional[int] = None) -> Dict[str, Any]:
        """Get static port information"""
        try:
            with operation(f"IMF port info {country or continent or 'all'}"):
                url = f"{self.portwatch_base}/PortWatch_ports_database/FeatureServer/0/query"
                params = {
                    "where": "1=1",
                    "outFields": "*",
                    "returnGeometry": "false",
                    "f": "json"
                }

                data = await self._make_request(url, params)
                if not data.get("success", True):
                    return data

                return self._transform_port_info(data, continent, country, limit)

        except Exception as e:
            error(f"IMF port info error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    def _transform_port_info(self, data: Dict, continent: Optional[str] = None,
                             country: Optional[str] = None, limit: Optional[int] = None) -> Dict[str, Any]:
        """Transform port info data"""
        try:
            features = data.get("features", [])
            if not features:
                return {"success": False, "error": "No port data found", "source": "imf"}

            result_data = []
            for feature in features:
                attrs = feature.get("attributes", {})

                # Apply filters
                if country and attrs.get("ISO3") != country.upper():
                    continue
                if continent and attrs.get("continent") != continent:
                    continue

                result_data.append({
                    "port_code": attrs.get("portid"),
                    "port_name": attrs.get("portname"),
                    "country": attrs.get("countrynoaccents"),
                    "country_code": attrs.get("ISO3"),
                    "continent": attrs.get("continent"),
                    "latitude": attrs.get("lat"),
                    "longitude": attrs.get("lon"),
                    "vessel_count_total": attrs.get("vessel_count_total", 0)
                })

            # Sort by vessel count and apply limit
            result_data.sort(key=lambda x: x["vessel_count_total"], reverse=True)
            if limit:
                result_data = result_data[:limit]

            return {
                "success": True,
                "source": "imf",
                "data": result_data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming port info: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    @monitor_performance
    async def get_port_volume(self, port_code: Optional[str] = None, country: Optional[str] = None,
                              start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get daily port volume data"""
        try:
            with operation(f"IMF port volume {port_code or country or 'all'}"):
                url = f"{self.portwatch_base}/Daily_Trade_Data/FeatureServer/0/query"

                where_clause = "1=1"
                if port_code:
                    where_clause += f" AND portid = '{port_code}'"
                if country:
                    where_clause += f" AND ISO3 = '{country.upper()}'"
                if start_date and end_date:
                    where_clause += f" AND date >= TIMESTAMP '{start_date} 00:00:00' AND date <= TIMESTAMP '{end_date} 00:00:00'"

                params = {
                    "where": where_clause,
                    "outFields": "*",
                    "orderByFields": "date",
                    "f": "json"
                }

                data = await self._make_request(url, params)
                if not data.get("success", True):
                    return data

                return self._transform_port_volume(data)

        except Exception as e:
            error(f"IMF port volume error: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    def _transform_port_volume(self, data: Dict) -> Dict[str, Any]:
        """Transform port volume data"""
        try:
            features = data.get("features", [])
            if not features:
                return {"success": False, "error": "No port volume data found", "source": "imf"}

            result_data = []
            for feature in features:
                attrs = feature.get("attributes", {})
                # Convert date from year/month/day to ISO format
                year = attrs.get("year")
                month = attrs.get("month")
                day = attrs.get("day")
                date_str = f"{year}-{month:02d}-{day:02d}" if all([year, month, day]) else None

                result_data.append({
                    "date": date_str,
                    "port_code": attrs.get("portid"),
                    "port_name": attrs.get("portname"),
                    "country_code": attrs.get("ISO3"),
                    "portcalls": attrs.get("portcalls", 0),
                    "imports": attrs.get("import", 0),
                    "exports": attrs.get("export", 0),
                    "imports_tanker": attrs.get("import_tanker", 0),
                    "exports_tanker": attrs.get("export_tanker", 0)
                })

            return {
                "success": True,
                "source": "imf",
                "data": result_data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming port volume: {str(e)}", module="IMFProvider")
            return {"success": False, "error": str(e), "source": "imf"}

    async def close(self):
        """Close the aiohttp session"""
        if self._session and not self._session.closed:
            await self._session.close()

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.close()


# Example usage and testing
async def main():
    """Example usage of IMF Provider"""
    async with IMFProvider() as imf:
        # Test available indicators
        indicators = await imf.get_available_indicators()
        print("Available indicators:", indicators)

        # Test direction of trade
        trade_data = await imf.get_direction_of_trade("US", "CN", "exports")
        print("Trade data:", trade_data)

        # Test economic indicators
        econ_data = await imf.get_economic_indicators("RAF_USD", "US")
        print("Economic data:", econ_data)

        # Test maritime data
        chokepoint_info = await imf.get_maritime_chokepoint_info()
        print("Chokepoint info:", chokepoint_info)

        port_info = await imf.get_port_info(country="USA", limit=10)
        print("Port info:", port_info)


if __name__ == "__main__":
    asyncio.run(main())