# -*- coding: utf-8 -*-
# alpha_vantage_provider.py

import asyncio
import aiohttp
from datetime import datetime
from typing import Any, Dict, Optional, List
from warnings import warn

from fincept_terminal.Utils.Logging.logger import info, debug, warning, error, operation, monitor_performance


class AlphaVantageProvider:
    """Alpha Vantage data provider with complete API integration"""

    INTERVALS_DICT = {
        "m": "TIME_SERIES_INTRADAY",
        "d": "TIME_SERIES_DAILY",
        "W": "TIME_SERIES_WEEKLY",
        "M": "TIME_SERIES_MONTHLY",
    }

    def __init__(self, api_key: str, rate_limit: int = 5):
        self.api_key = api_key
        self.base_url = "https://www.alphavantage.co/query"
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

    def get_interval(self, value: str) -> str:
        """Get the intervals for the Alpha Vantage API"""
        try:
            intervals = {
                "m": "min",
                "d": "day",
                "W": "week",
                "M": "month",
            }

            if len(value) < 2:
                return "1min"  # default

            period_num = value[:-1]
            period_type = value[-1]

            if period_type in intervals:
                return f"{period_num}{intervals[period_type]}"
            else:
                return "1min"  # default fallback

        except Exception as e:
            warning(f"Error parsing interval {value}: {str(e)}", module="AlphaVantageProvider")
            return "1min"

    async def _make_request(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Make API request with common error handling"""
        try:
            session = await self._get_session()
            async with session.get(self.base_url, params=params) as response:
                if response.status == 200:
                    return await response.json()
                else:
                    error(f"Alpha Vantage API HTTP error: {response.status}", module="AlphaVantageProvider")
                    return {"success": False, "error": f"HTTP {response.status}", "source": "alpha_vantage"}
        except Exception as e:
            error(f"Alpha Vantage API request error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _check_api_errors(self, data: Dict) -> Optional[Dict[str, Any]]:
        """Check for common API errors"""
        if "Error Message" in data:
            return {"success": False, "error": data["Error Message"], "source": "alpha_vantage"}
        if "Note" in data:
            return {"success": False, "error": "API rate limit exceeded", "source": "alpha_vantage"}
        if "Information" in data:
            warn(data["Information"])
            return {"success": False, "error": data["Information"], "source": "alpha_vantage"}
        return None



    # EXISTING METHODS (UNCHANGED)
    @monitor_performance
    async def get_stock_data(self, symbol: str, period: str = "1d", interval: str = "1d") -> Dict[str, Any]:
        """Get stock historical data from Alpha Vantage"""
        try:
            with operation(f"AlphaVantage stock data for {symbol}"):
                # Map intervals to Alpha Vantage format
                function = self.INTERVALS_DICT.get(interval[-1], "TIME_SERIES_DAILY")

                params = {
                    "function": function,
                    "symbol": symbol,
                    "apikey": self.api_key,
                    "outputsize": "full",
                    "datatype": "json"
                }

                # Add interval for intraday data
                if "INTRADAY" in function:
                    av_interval = self.get_interval(interval)
                    params["interval"] = av_interval

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_stock_data(data, symbol, interval)

        except Exception as e:
            error(f"Alpha Vantage stock data error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_exchange_rate(self, data: Dict, from_currency: str, to_currency: str) -> Dict[str, Any]:
        """Transform currency exchange rate response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            exchange_data = data.get("Realtime Currency Exchange Rate", {})
            if not exchange_data:
                return {"success": False, "error": "No exchange rate data found", "source": "alpha_vantage"}

            return {
                "success": True,
                "source": "alpha_vantage",
                "from_currency": from_currency,
                "to_currency": to_currency,
                "data": {
                    "from_currency_code": exchange_data.get("1. From_Currency Code", from_currency),
                    "from_currency_name": exchange_data.get("2. From_Currency Name", ""),
                    "to_currency_code": exchange_data.get("3. To_Currency Code", to_currency),
                    "to_currency_name": exchange_data.get("4. To_Currency Name", ""),
                    "exchange_rate": float(exchange_data.get("5. Exchange Rate", 0)),
                    "last_refreshed": exchange_data.get("6. Last Refreshed", ""),
                    "time_zone": exchange_data.get("7. Time Zone", "")
                },
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming exchange rate: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Exchange rate transformation error: {str(e)}",
                    "source": "alpha_vantage"}

    @monitor_performance
    async def get_fx_intraday(self, from_symbol: str, to_symbol: str, interval: str = "5min",
                              outputsize: str = "compact") -> Dict[str, Any]:
        """Get intraday FX data"""
        try:
            with operation(f"AlphaVantage FX intraday {from_symbol}/{to_symbol}"):
                params = {
                    "function": "FX_INTRADAY",
                    "from_symbol": from_symbol,
                    "to_symbol": to_symbol,
                    "interval": interval,
                    "outputsize": outputsize,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_fx_intraday(data, from_symbol, to_symbol, interval)
        except Exception as e:
            error(f"FX intraday error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_fx_intraday(self, data: Dict, from_symbol: str, to_symbol: str, interval: str) -> Dict[str, Any]:
        """Transform FX intraday response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            time_series_key = f"Time Series FX ({interval})"
            time_series = data.get(time_series_key, {})

            if not time_series:
                return {"success": False, "error": "No FX intraday data found", "source": "alpha_vantage"}

            timestamps = []
            opens = []
            highs = []
            lows = []
            closes = []

            for timestamp in sorted(time_series.keys()):
                tick_data = time_series[timestamp]
                timestamps.append(timestamp)
                opens.append(float(tick_data.get("1. open", 0)))
                highs.append(float(tick_data.get("2. high", 0)))
                lows.append(float(tick_data.get("3. low", 0)))
                closes.append(float(tick_data.get("4. close", 0)))

            return {
                "success": True,
                "source": "alpha_vantage",
                "from_symbol": from_symbol,
                "to_symbol": to_symbol,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    "open": opens,
                    "high": highs,
                    "low": lows,
                    "close": closes
                },
                "current_rate": closes[-1] if closes else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming FX intraday: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"FX intraday transformation error: {str(e)}", "source": "alpha_vantage"}

    @monitor_performance
    async def get_fx_weekly(self, from_symbol: str, to_symbol: str) -> Dict[str, Any]:
        """Get weekly FX data"""
        try:
            with operation(f"AlphaVantage FX weekly {from_symbol}/{to_symbol}"):
                params = {
                    "function": "FX_WEEKLY",
                    "from_symbol": from_symbol,
                    "to_symbol": to_symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_fx_weekly_monthly(data, from_symbol, to_symbol, "weekly")
        except Exception as e:
            error(f"FX weekly error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    @monitor_performance
    async def get_fx_monthly(self, from_symbol: str, to_symbol: str) -> Dict[str, Any]:
        """Get monthly FX data"""
        try:
            with operation(f"AlphaVantage FX monthly {from_symbol}/{to_symbol}"):
                params = {
                    "function": "FX_MONTHLY",
                    "from_symbol": from_symbol,
                    "to_symbol": to_symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_fx_weekly_monthly(data, from_symbol, to_symbol, "monthly")
        except Exception as e:
            error(f"FX monthly error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_fx_weekly_monthly(self, data: Dict, from_symbol: str, to_symbol: str, interval: str) -> Dict[
        str, Any]:
        """Transform FX weekly/monthly response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            time_series_key = f"Time Series FX ({interval.capitalize()})"
            time_series = data.get(time_series_key, {})

            if not time_series:
                return {"success": False, "error": f"No FX {interval} data found", "source": "alpha_vantage"}

            timestamps = []
            opens = []
            highs = []
            lows = []
            closes = []

            for date_str in sorted(time_series.keys()):
                period_data = time_series[date_str]
                timestamps.append(date_str)
                opens.append(float(period_data.get("1. open", 0)))
                highs.append(float(period_data.get("2. high", 0)))
                lows.append(float(period_data.get("3. low", 0)))
                closes.append(float(period_data.get("4. close", 0)))

            return {
                "success": True,
                "source": "alpha_vantage",
                "from_symbol": from_symbol,
                "to_symbol": to_symbol,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    "open": opens,
                    "high": highs,
                    "low": lows,
                    "close": closes
                },
                "current_rate": closes[-1] if closes else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming FX {interval}: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"FX {interval} transformation error: {str(e)}",
                    "source": "alpha_vantage"}

    # EXTENDED CRYPTO METHODS
    @monitor_performance
    async def get_crypto_intraday(self, symbol: str, market: str = "USD", interval: str = "5min",
                                  outputsize: str = "compact") -> Dict[str, Any]:
        """Get intraday crypto data"""
        try:
            with operation(f"AlphaVantage crypto intraday {symbol}/{market}"):
                params = {
                    "function": "CRYPTO_INTRADAY",
                    "symbol": symbol,
                    "market": market,
                    "interval": interval,
                    "outputsize": outputsize,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_crypto_intraday(data, symbol, market, interval)
        except Exception as e:
            error(f"Crypto intraday error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_crypto_intraday(self, data: Dict, symbol: str, market: str, interval: str) -> Dict[str, Any]:
        """Transform crypto intraday response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            time_series_key = f"Time Series Crypto ({interval})"
            time_series = data.get(time_series_key, {})

            if not time_series:
                return {"success": False, "error": "No crypto intraday data found", "source": "alpha_vantage"}

            timestamps = []
            opens = []
            highs = []
            lows = []
            closes = []
            volumes = []

            for timestamp in sorted(time_series.keys()):
                tick_data = time_series[timestamp]
                timestamps.append(timestamp)
                opens.append(float(tick_data.get("1. open", 0)))
                highs.append(float(tick_data.get("2. high", 0)))
                lows.append(float(tick_data.get("3. low", 0)))
                closes.append(float(tick_data.get("4. close", 0)))
                volumes.append(float(tick_data.get("5. volume", 0)))

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "market": market,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    "open": opens,
                    "high": highs,
                    "low": lows,
                    "close": closes,
                    "volume": volumes
                },
                "current_price": closes[-1] if closes else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming crypto intraday: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Crypto intraday transformation error: {str(e)}",
                    "source": "alpha_vantage"}

    @monitor_performance
    async def get_digital_currency_weekly(self, symbol: str, market: str = "USD") -> Dict[str, Any]:
        """Get weekly digital currency data"""
        try:
            with operation(f"AlphaVantage digital currency weekly {symbol}/{market}"):
                params = {
                    "function": "DIGITAL_CURRENCY_WEEKLY",
                    "symbol": symbol,
                    "market": market,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_digital_currency_weekly_monthly(data, symbol, market, "weekly")
        except Exception as e:
            error(f"Digital currency weekly error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    @monitor_performance
    async def get_digital_currency_monthly(self, symbol: str, market: str = "USD") -> Dict[str, Any]:
        """Get monthly digital currency data"""
        try:
            with operation(f"AlphaVantage digital currency monthly {symbol}/{market}"):
                params = {
                    "function": "DIGITAL_CURRENCY_MONTHLY",
                    "symbol": symbol,
                    "market": market,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_digital_currency_weekly_monthly(data, symbol, market, "monthly")
        except Exception as e:
            error(f"Digital currency monthly error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_digital_currency_weekly_monthly(self, data: Dict, symbol: str, market: str, interval: str) -> Dict[
        str, Any]:
        """Transform digital currency weekly/monthly response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            time_series_key = f"Time Series (Digital Currency {interval.capitalize()})"
            time_series = data.get(time_series_key, {})

            if not time_series:
                return {"success": False, "error": f"No digital currency {interval} data found",
                        "source": "alpha_vantage"}

            timestamps = []
            opens_market = []
            highs_market = []
            lows_market = []
            closes_market = []
            opens_usd = []
            highs_usd = []
            lows_usd = []
            closes_usd = []
            volumes = []

            for date_str in sorted(time_series.keys()):
                period_data = time_series[date_str]
                timestamps.append(date_str)
                opens_market.append(float(period_data.get(f"1a. open ({market})", 0)))
                highs_market.append(float(period_data.get(f"2a. high ({market})", 0)))
                lows_market.append(float(period_data.get(f"3a. low ({market})", 0)))
                closes_market.append(float(period_data.get(f"4a. close ({market})", 0)))
                opens_usd.append(float(period_data.get("1b. open (USD)", 0)))
                highs_usd.append(float(period_data.get("2b. high (USD)", 0)))
                lows_usd.append(float(period_data.get("3b. low (USD)", 0)))
                closes_usd.append(float(period_data.get("4b. close (USD)", 0)))
                volumes.append(float(period_data.get("5. volume", 0)))

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "market": market,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    f"open_{market.lower()}": opens_market,
                    f"high_{market.lower()}": highs_market,
                    f"low_{market.lower()}": lows_market,
                    f"close_{market.lower()}": closes_market,
                    "open_usd": opens_usd,
                    "high_usd": highs_usd,
                    "low_usd": lows_usd,
                    "close_usd": closes_usd,
                    "volume": volumes
                },
                "current_price_usd": closes_usd[-1] if closes_usd else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming digital currency {interval}: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Digital currency {interval} transformation error: {str(e)}",
                    "source": "alpha_vantage"}

    # COMMODITIES METHODS
    @monitor_performance
    async def get_commodity_data(self, function: str, interval: str = "monthly") -> Dict[str, Any]:
        """Generic method for commodity data"""
        try:
            with operation(f"AlphaVantage commodity {function}"):
                params = {
                    "function": function,
                    "interval": interval,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_commodity_data(data, function, interval)
        except Exception as e:
            error(f"Commodity {function} error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_commodity_data(self, data: Dict, function: str, interval: str) -> Dict[str, Any]:
        """Transform commodity data response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            # Find the data key (varies by commodity)
            data_key = None
            for key in data.keys():
                if "data" in key.lower() or function.lower() in key.lower():
                    data_key = key
                    break

            if not data_key:
                return {"success": False, "error": "No commodity data found", "source": "alpha_vantage"}

            commodity_data = data[data_key]
            if not commodity_data:
                return {"success": False, "error": "Empty commodity data", "source": "alpha_vantage"}

            timestamps = []
            values = []

            for entry in commodity_data:
                if isinstance(entry, dict):
                    # Extract date and value (key names vary by commodity)
                    date_key = next((k for k in entry.keys() if "date" in k.lower()), None)
                    value_key = next((k for k in entry.keys() if "value" in k.lower() or "price" in k.lower()), None)

                    if date_key and value_key:
                        timestamps.append(entry[date_key])
                        values.append(float(entry[value_key]) if entry[value_key] != '.' else 0)

            return {
                "success": True,
                "source": "alpha_vantage",
                "commodity": function,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    "values": values
                },
                "current_value": values[-1] if values else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming commodity data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Commodity transformation error: {str(e)}", "source": "alpha_vantage"}

    # Individual commodity methods
    async def get_wti_oil(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get WTI crude oil prices"""
        return await self.get_commodity_data("WTI", interval)

    async def get_brent_oil(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get Brent crude oil prices"""
        return await self.get_commodity_data("BRENT", interval)

    async def get_natural_gas(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get natural gas prices"""
        return await self.get_commodity_data("NATURAL_GAS", interval)

    async def get_copper(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get copper prices"""
        return await self.get_commodity_data("COPPER", interval)

    async def get_aluminum(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get aluminum prices"""
        return await self.get_commodity_data("ALUMINUM", interval)

    async def get_wheat(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get wheat prices"""
        return await self.get_commodity_data("WHEAT", interval)

    async def get_corn(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get corn prices"""
        return await self.get_commodity_data("CORN", interval)

    async def get_cotton(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get cotton prices"""
        return await self.get_commodity_data("COTTON", interval)

    async def get_sugar(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get sugar prices"""
        return await self.get_commodity_data("SUGAR", interval)

    async def get_coffee(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get coffee prices"""
        return await self.get_commodity_data("COFFEE", interval)

    async def get_all_commodities(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get all commodities index"""
        return await self.get_commodity_data("ALL_COMMODITIES", interval)

    # ECONOMIC INDICATORS METHODS
    @monitor_performance
    async def get_economic_indicator(self, function: str, interval: str = None, maturity: str = None) -> Dict[str, Any]:
        """Generic method for economic indicators"""
        try:
            with operation(f"AlphaVantage economic indicator {function}"):
                params = {
                    "function": function,
                    "apikey": self.api_key
                }

                if interval:
                    params["interval"] = interval
                if maturity:
                    params["maturity"] = maturity

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_economic_data(data, function, interval)
        except Exception as e:
            error(f"Economic indicator {function} error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_economic_data(self, data: Dict, function: str, interval: str) -> Dict[str, Any]:
        """Transform economic indicator response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            # Find the data key
            data_key = None
            for key in data.keys():
                if "data" in key.lower() or function.lower() in key.lower():
                    data_key = key
                    break

            if not data_key:
                return {"success": False, "error": "No economic data found", "source": "alpha_vantage"}

            economic_data = data[data_key]
            if not economic_data:
                return {"success": False, "error": "Empty economic data", "source": "alpha_vantage"}

            timestamps = []
            values = []

            for entry in economic_data:
                if isinstance(entry, dict):
                    # Extract date and value
                    date_key = next((k for k in entry.keys() if "date" in k.lower()), None)
                    value_key = next((k for k in entry.keys() if "value" in k.lower()), None)

                    if date_key and value_key:
                        timestamps.append(entry[date_key])
                        values.append(float(entry[value_key]) if entry[value_key] != '.' else 0)

            return {
                "success": True,
                "source": "alpha_vantage",
                "indicator": function,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    "values": values
                },
                "latest_value": values[-1] if values else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming economic data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Economic data transformation error: {str(e)}",
                    "source": "alpha_vantage"}

    # Individual economic indicator methods
    async def get_real_gdp(self, interval: str = "annual") -> Dict[str, Any]:
        """Get Real GDP data"""
        return await self.get_economic_indicator("REAL_GDP", interval)

    async def get_real_gdp_per_capita(self) -> Dict[str, Any]:
        """Get Real GDP per capita data"""
        return await self.get_economic_indicator("REAL_GDP_PER_CAPITA")

    async def get_treasury_yield(self, interval: str = "monthly", maturity: str = "10year") -> Dict[str, Any]:
        """Get Treasury yield data"""
        return await self.get_economic_indicator("TREASURY_YIELD", interval, maturity)

    async def get_federal_funds_rate(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get Federal funds rate data"""
        return await self.get_economic_indicator("FEDERAL_FUNDS_RATE", interval)

    async def get_cpi(self, interval: str = "monthly") -> Dict[str, Any]:
        """Get Consumer Price Index data"""
        return await self.get_economic_indicator("CPI", interval)

    async def get_inflation(self) -> Dict[str, Any]:
        """Get inflation data"""
        return await self.get_economic_indicator("INFLATION")

    async def get_retail_sales(self) -> Dict[str, Any]:
        """Get retail sales data"""
        return await self.get_economic_indicator("RETAIL_SALES")

    async def get_durables(self) -> Dict[str, Any]:
        """Get durable goods data"""
        return await self.get_economic_indicator("DURABLES")

    async def get_unemployment(self) -> Dict[str, Any]:
        """Get unemployment rate data"""
        return await self.get_economic_indicator("UNEMPLOYMENT")

    async def get_nonfarm_payroll(self) -> Dict[str, Any]:
        """Get nonfarm payroll data"""
        return await self.get_economic_indicator("NONFARM_PAYROLL")

    # TECHNICAL INDICATORS METHODS
    @monitor_performance
    async def get_technical_indicator(self, function: str, symbol: str, interval: str,
                                      time_period: int = None, series_type: str = "close",
                                      **kwargs) -> Dict[str, Any]:
        """Generic method for technical indicators"""
        try:
            with operation(f"AlphaVantage {function} for {symbol}"):
                params = {
                    "function": function,
                    "symbol": symbol,
                    "interval": interval,
                    "series_type": series_type,
                    "apikey": self.api_key
                }

                if time_period:
                    params["time_period"] = time_period

                # Add any additional parameters
                params.update(kwargs)

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_technical_indicator(data, function, symbol, interval)
        except Exception as e:
            error(f"Technical indicator {function} error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}

    def _transform_technical_indicator(self, data: Dict, function: str, symbol: str, interval: str) -> Dict[str, Any]:
        """Transform technical indicator response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            # Find the technical analysis key
            tech_key = None
            for key in data.keys():
                if "Technical Analysis" in key or function in key:
                    tech_key = key
                    break

            if not tech_key:
                return {"success": False, "error": "No technical indicator data found", "source": "alpha_vantage"}

            tech_data = data[tech_key]
            if not tech_data:
                return {"success": False, "error": "Empty technical indicator data", "source": "alpha_vantage"}

            timestamps = []
            values = {}

            # Initialize value arrays based on the indicator type
            for timestamp in sorted(tech_data.keys()):
                timestamps.append(timestamp)

                for key, value in tech_data[timestamp].items():
                    indicator_name = key.split('. ')[-1] if '. ' in key else key
                    if indicator_name not in values:
                        values[indicator_name] = []
                    values[indicator_name].append(float(value))

            return {
                "success": True,
                "source": "alpha_vantage",
                "function": function,
                "symbol": symbol,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    **values
                },
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming technical indicator: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Technical indicator transformation error: {str(e)}",
                    "source": "alpha_vantage"}

    # Popular technical indicators (individual methods)
    async def get_sma(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get Simple Moving Average"""
        return await self.get_technical_indicator("SMA", symbol, interval, time_period, series_type)

    async def get_ema(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get Exponential Moving Average"""
        return await self.get_technical_indicator("EMA", symbol, interval, time_period, series_type)

    async def get_wma(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get Weighted Moving Average"""
        return await self.get_technical_indicator("WMA", symbol, interval, time_period, series_type)

    async def get_dema(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[
        str, Any]:
        """Get Double Exponential Moving Average"""
        return await self.get_technical_indicator("DEMA", symbol, interval, time_period, series_type)

    async def get_tema(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[
        str, Any]:
        """Get Triple Exponential Moving Average"""
        return await self.get_technical_indicator("TEMA", symbol, interval, time_period, series_type)

    async def get_trima(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[
        str, Any]:
        """Get Triangular Moving Average"""
        return await self.get_technical_indicator("TRIMA", symbol, interval, time_period, series_type)

    async def get_kama(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[
        str, Any]:
        """Get Kaufman Adaptive Moving Average"""
        return await self.get_technical_indicator("KAMA", symbol, interval, time_period, series_type)

    async def get_mama(self, symbol: str, interval: str, series_type: str = "close",
                       fastlimit: float = 0.01, slowlimit: float = 0.01) -> Dict[str, Any]:
        """Get MESA Adaptive Moving Average"""
        return await self.get_technical_indicator("MAMA", symbol, interval, None, series_type,
                                                  fastlimit=fastlimit, slowlimit=slowlimit)

    async def get_vwap(self, symbol: str, interval: str) -> Dict[str, Any]:
        """Get Volume Weighted Average Price"""
        return await self.get_technical_indicator("VWAP", symbol, interval)

    async def get_t3(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get T3 Moving Average"""
        return await self.get_technical_indicator("T3", symbol, interval, time_period, series_type)

    async def get_macd(self, symbol: str, interval: str, series_type: str = "close",
                       fastperiod: int = 12, slowperiod: int = 26, signalperiod: int = 9) -> Dict[str, Any]:
        """Get Moving Average Convergence Divergence"""
        return await self.get_technical_indicator("MACD", symbol, interval, None, series_type,
                                                  fastperiod=fastperiod, slowperiod=slowperiod,
                                                  signalperiod=signalperiod)

    async def get_macdext(self, symbol: str, interval: str, series_type: str = "close",
                          fastperiod: int = 12, slowperiod: int = 26, signalperiod: int = 9,
                          fastmatype: int = 0, slowmatype: int = 0, signalmatype: int = 0) -> Dict[str, Any]:
        """Get MACD with controllable MA type"""
        return await self.get_technical_indicator("MACDEXT", symbol, interval, None, series_type,
                                                  fastperiod=fastperiod, slowperiod=slowperiod,
                                                  signalperiod=signalperiod, fastmatype=fastmatype,
                                                  slowmatype=slowmatype, signalmatype=signalmatype)

    async def get_stoch(self, symbol: str, interval: str, fastkperiod: int = 5, slowkperiod: int = 3,
                        slowdperiod: int = 3, slowkmatype: int = 0, slowdmatype: int = 0) -> Dict[str, Any]:
        """Get Stochastic Oscillator"""
        return await self.get_technical_indicator("STOCH", symbol, interval, None, None,
                                                  fastkperiod=fastkperiod, slowkperiod=slowkperiod,
                                                  slowdperiod=slowdperiod, slowkmatype=slowkmatype,
                                                  slowdmatype=slowdmatype)

    async def get_stochf(self, symbol: str, interval: str, fastkperiod: int = 5, fastdperiod: int = 3,
                         fastdmatype: int = 0) -> Dict[str, Any]:
        """Get Stochastic Fast"""
        return await self.get_technical_indicator("STOCHF", symbol, interval, None, None,
                                                  fastkperiod=fastkperiod, fastdperiod=fastdperiod,
                                                  fastdmatype=fastdmatype)

    async def get_rsi(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get Relative Strength Index"""
        return await self.get_technical_indicator("RSI", symbol, interval, time_period, series_type)

    async def get_stochrsi(self, symbol: str, interval: str, time_period: int, series_type: str = "close",
                           fastkperiod: int = 5, fastdperiod: int = 3, fastdmatype: int = 0) -> Dict[str, Any]:
        """Get Stochastic RSI"""
        return await self.get_technical_indicator("STOCHRSI", symbol, interval, time_period, series_type,
                                                  fastkperiod=fastkperiod, fastdperiod=fastdperiod,
                                                  fastdmatype=fastdmatype)

    async def get_willr(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Williams %R"""
        return await self.get_technical_indicator("WILLR", symbol, interval, time_period)

    async def get_adx(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Average Directional Movement Index"""
        return await self.get_technical_indicator("ADX", symbol, interval, time_period)

    async def get_adxr(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Average Directional Movement Index Rating"""
        return await self.get_technical_indicator("ADXR", symbol, interval, time_period)

    async def get_apo(self, symbol: str, interval: str, series_type: str = "close", fastperiod: int = 12,
                      slowperiod: int = 26, matype: int = 0) -> Dict[str, Any]:
        """Get Absolute Price Oscillator"""
        return await self.get_technical_indicator("APO", symbol, interval, None, series_type,
                                                  fastperiod=fastperiod, slowperiod=slowperiod, matype=matype)

    async def get_ppo(self, symbol: str, interval: str, series_type: str = "close", fastperiod: int = 12,
                      slowperiod: int = 26, matype: int = 0) -> Dict[str, Any]:
        """Get Percentage Price Oscillator"""
        return await self.get_technical_indicator("PPO", symbol, interval, None, series_type,
                                                  fastperiod=fastperiod, slowperiod=slowperiod, matype=matype)

    async def get_mom(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get Momentum"""
        return await self.get_technical_indicator("MOM", symbol, interval, time_period, series_type)

    async def get_bop(self, symbol: str, interval: str) -> Dict[str, Any]:
        """Get Balance of Power"""
        return await self.get_technical_indicator("BOP", symbol, interval)

    async def get_cci(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Commodity Channel Index"""
        return await self.get_technical_indicator("CCI", symbol, interval, time_period)

    async def get_cmo(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get Chande Momentum Oscillator"""
        return await self.get_technical_indicator("CMO", symbol, interval, time_period, series_type)

    async def get_roc(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[str, Any]:
        """Get Rate of Change"""
        return await self.get_technical_indicator("ROC", symbol, interval, time_period, series_type)

    async def get_rocr(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[
        str, Any]:
        """Get Rate of Change Ratio"""
        return await self.get_technical_indicator("ROCR", symbol, interval, time_period, series_type)

    async def get_aroon(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Aroon"""
        return await self.get_technical_indicator("AROON", symbol, interval, time_period)

    async def get_aroonosc(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Aroon Oscillator"""
        return await self.get_technical_indicator("AROONOSC", symbol, interval, time_period)

    async def get_mfi(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Money Flow Index"""
        return await self.get_technical_indicator("MFI", symbol, interval, time_period)

    async def get_trix(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[
        str, Any]:
        """Get TRIX"""
        return await self.get_technical_indicator("TRIX", symbol, interval, time_period, series_type)

    async def get_ultosc(self, symbol: str, interval: str, timeperiod1: int = 7, timeperiod2: int = 14,
                         timeperiod3: int = 28) -> Dict[str, Any]:
        """Get Ultimate Oscillator"""
        return await self.get_technical_indicator("ULTOSC", symbol, interval, None, None,
                                                  timeperiod1=timeperiod1, timeperiod2=timeperiod2,
                                                  timeperiod3=timeperiod3)

    async def get_dx(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Directional Movement Index"""
        return await self.get_technical_indicator("DX", symbol, interval, time_period)

    async def get_minus_di(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Minus Directional Indicator"""
        return await self.get_technical_indicator("MINUS_DI", symbol, interval, time_period)

    async def get_plus_di(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Plus Directional Indicator"""
        return await self.get_technical_indicator("PLUS_DI", symbol, interval, time_period)

    async def get_minus_dm(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Minus Directional Movement"""
        return await self.get_technical_indicator("MINUS_DM", symbol, interval, time_period)

    async def get_plus_dm(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Plus Directional Movement"""
        return await self.get_technical_indicator("PLUS_DM", symbol, interval, time_period)

    async def get_bbands(self, symbol: str, interval: str, time_period: int, series_type: str = "close",
                         nbdevup: int = 2, nbdevdn: int = 2, matype: int = 0) -> Dict[str, Any]:
        """Get Bollinger Bands"""
        return await self.get_technical_indicator("BBANDS", symbol, interval, time_period, series_type,
                                                  nbdevup=nbdevup, nbdevdn=nbdevdn, matype=matype)

    async def get_midpoint(self, symbol: str, interval: str, time_period: int, series_type: str = "close") -> Dict[
        str, Any]:
        """Get MidPoint"""
        return await self.get_technical_indicator("MIDPOINT", symbol, interval, time_period, series_type)

    async def get_midprice(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get MidPrice"""
        return await self.get_technical_indicator("MIDPRICE", symbol, interval, time_period)

    async def get_sar(self, symbol: str, interval: str, acceleration: float = 0.01, maximum: float = 0.20) -> Dict[
        str, Any]:
        """Get Parabolic SAR"""
        return await self.get_technical_indicator("SAR", symbol, interval, None, None,
                                                  acceleration=acceleration, maximum=maximum)

    async def get_trange(self, symbol: str, interval: str) -> Dict[str, Any]:
        """Get True Range"""
        return await self.get_technical_indicator("TRANGE", symbol, interval)

    async def get_atr(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Average True Range"""
        return await self.get_technical_indicator("ATR", symbol, interval, time_period)

    async def get_natr(self, symbol: str, interval: str, time_period: int) -> Dict[str, Any]:
        """Get Normalized Average True Range"""
        return await self.get_technical_indicator("NATR", symbol, interval, time_period)

    async def get_ad(self, symbol: str, interval: str) -> Dict[str, Any]:
        """Get Chaikin A/D Line"""
        return await self.get_technical_indicator("AD", symbol, interval)

    async def get_adosc(self, symbol: str, interval: str, fastperiod: int = 3, slowperiod: int = 10) -> Dict[str, Any]:
        """Get Chaikin A/D Oscillator"""
        return await self.get_technical_indicator("ADOSC", symbol, interval, None, None,
                                                  fastperiod=fastperiod, slowperiod=slowperiod)

    async def get_obv(self, symbol: str, interval: str) -> Dict[str, Any]:
        """Get On Balance Volume"""
        return await self.get_technical_indicator("OBV", symbol, interval)

    async def get_ht_trendline(self, symbol: str, interval: str, series_type: str = "close") -> Dict[str, Any]:
        """Get Hilbert Transform - Instantaneous Trendline"""
        return await self.get_technical_indicator("HT_TRENDLINE", symbol, interval, None, series_type)

    async def get_ht_sine(self, symbol: str, interval: str, series_type: str = "close") -> Dict[str, Any]:
        """Get Hilbert Transform - Sine Wave"""
        return await self.get_technical_indicator("HT_SINE", symbol, interval, None, series_type)

    async def get_ht_trendmode(self, symbol: str, interval: str, series_type: str = "close") -> Dict[str, Any]:
        """Get Hilbert Transform - Trend vs Cycle Mode"""
        return await self.get_technical_indicator("HT_TRENDMODE", symbol, interval, None, series_type)

    async def get_ht_dcperiod(self, symbol: str, interval: str, series_type: str = "close") -> Dict[str, Any]:
        """Get Hilbert Transform - Dominant Cycle Period"""
        return await self.get_technical_indicator("HT_DCPERIOD", symbol, interval, None, series_type)

    async def get_ht_dcphase(self, symbol: str, interval: str, series_type: str = "close") -> Dict[str, Any]:
        """Get Hilbert Transform - Dominant Cycle Phase"""
        return await self.get_technical_indicator("HT_DCPHASE", symbol, interval, None, series_type)

    async def get_ht_phasor(self, symbol: str, interval: str, series_type: str = "close") -> Dict[str, Any]:
        """Get Hilbert Transform - Phasor Components"""
        return await self.get_technical_indicator("HT_PHASOR", symbol, interval, None, series_type)

    # CLEANUP METHOD
    async def close(self):
        """Close the aiohttp session"""
        if self._session and not self._session.closed:
            await self._session.close()
            debug("Alpha Vantage session closed", module="AlphaVantageProvider")

    def __del__(self):
        """Cleanup on deletion"""
        if hasattr(self, '_session') and self._session and not self._session.closed:
            try:
                asyncio.create_task(self.close())
            except Exception:
                pass


    def _transform_stock_data(self, data: Dict, symbol: str, interval: str) -> Dict[str, Any]:
        """Transform Alpha Vantage response to standard format"""
        try:
            # Check for API errors
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            # Find the time series key
            time_series_key = None
            for key in data.keys():
                if "Time Series" in key:
                    time_series_key = key
                    break

            if not time_series_key:
                return {"success": False, "error": "No time series data found", "source": "alpha_vantage"}

            time_series = data[time_series_key]

            if not time_series:
                return {"success": False, "error": "Empty time series data", "source": "alpha_vantage"}

            timestamps = []
            opens = []
            highs = []
            lows = []
            closes = []
            volumes = []

            # Sort by date and extract data
            for date_str in sorted(time_series.keys()):
                day_data = time_series[date_str]

                timestamps.append(date_str)
                opens.append(float(day_data.get("1. open", 0)))
                highs.append(float(day_data.get("2. high", 0)))
                lows.append(float(day_data.get("3. low", 0)))
                closes.append(float(day_data.get("4. close", 0)))
                volumes.append(int(float(day_data.get("5. volume", 0))))

            debug(f"Transformed {len(timestamps)} data points for {symbol}", module="AlphaVantageProvider")

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": {
                    "timestamps": timestamps,
                    "open": opens,
                    "high": highs,
                    "low": lows,
                    "close": closes,
                    "volume": volumes
                },
                "current_price": closes[-1] if closes else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming Alpha Vantage data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Data transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_forex_data(self, pair: str, period: str = "1d") -> Dict[str, Any]:
        """Get forex data from Alpha Vantage"""
        try:
            with operation(f"AlphaVantage forex data for {pair}"):
                # Convert pair format (EURUSD -> EUR, USD)
                if len(pair) == 6:
                    from_symbol = pair[:3]
                    to_symbol = pair[3:]
                else:
                    # Try to split common formats
                    parts = pair.replace("/", "").replace("-", "").replace("_", "")
                    if len(parts) == 6:
                        from_symbol = parts[:3]
                        to_symbol = parts[3:]
                    else:
                        return {"success": False, "error": f"Invalid forex pair format: {pair}",
                                "source": "alpha_vantage"}

                params = {
                    "function": "FX_DAILY",
                    "from_symbol": from_symbol,
                    "to_symbol": to_symbol,
                    "apikey": self.api_key,
                    "outputsize": "full"
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_forex_data(data, pair)

        except Exception as e:
            error(f"Alpha Vantage forex data error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_forex_data(self, data: Dict, pair: str) -> Dict[str, Any]:
        """Transform forex data response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            time_series = data.get("Time Series FX (Daily)", {})

            if not time_series:
                return {"success": False, "error": "No forex data found", "source": "alpha_vantage"}

            timestamps = []
            rates = []

            for date_str in sorted(time_series.keys()):
                day_data = time_series[date_str]
                timestamps.append(date_str)
                rates.append(float(day_data.get("4. close", 0)))

            return {
                "success": True,
                "source": "alpha_vantage",
                "pair": pair,
                "data": {
                    "timestamps": timestamps,
                    "rates": rates
                },
                "current_rate": rates[-1] if rates else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming forex data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Forex data transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_crypto_data(self, symbol: str, period: str = "1d") -> Dict[str, Any]:
        """Get crypto data from Alpha Vantage"""
        try:
            with operation(f"AlphaVantage crypto data for {symbol}"):
                params = {
                    "function": "DIGITAL_CURRENCY_DAILY",
                    "symbol": symbol,
                    "market": "USD",
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_crypto_data(data, symbol)

        except Exception as e:
            error(f"Alpha Vantage crypto data error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_crypto_data(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform crypto data response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            time_series = data.get("Time Series (Digital Currency Daily)", {})

            if not time_series:
                return {"success": False, "error": "No crypto data found", "source": "alpha_vantage"}

            timestamps = []
            prices = []

            for date_str in sorted(time_series.keys()):
                day_data = time_series[date_str]
                timestamps.append(date_str)
                prices.append(float(day_data.get("4a. close (USD)", 0)))

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": {
                    "timestamps": timestamps,
                    "prices": prices
                },
                "current_price": prices[-1] if prices else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming crypto data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Crypto data transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def verify_api_key(self) -> Dict[str, Any]:
        """Verify Alpha Vantage API key"""
        try:
            with operation("Verify Alpha Vantage API key"):
                params = {
                    "function": "TIME_SERIES_DAILY",
                    "symbol": "AAPL",
                    "apikey": self.api_key,
                    "outputsize": "compact"
                }

                session = await self._get_session()
                async with session.get(self.base_url, params=params, timeout=10) as response:
                    if response.status == 200:
                        data = await response.json()

                        # Check for common error responses
                        if "Error Message" in data:
                            return {"valid": False, "error": data["Error Message"]}
                        elif "Note" in data:
                            return {"valid": False, "error": "API rate limit exceeded"}
                        elif "Information" in data:
                            return {"valid": False, "error": data["Information"]}
                        elif "Time Series (Daily)" in data:
                            info("Alpha Vantage API key verified successfully", module="AlphaVantageProvider")
                            return {"valid": True, "message": "API key verified successfully"}
                        else:
                            return {"valid": False, "error": "Unexpected API response"}
                    else:
                        return {"valid": False, "error": f"HTTP {response.status}"}

        except asyncio.TimeoutError:
            return {"valid": False, "error": "API request timeout"}
        except Exception as e:
            error(f"API key verification error: {str(e)}", module="AlphaVantageProvider")
            return {"valid": False, "error": str(e)}


    # NEW TIME SERIES METHODS
    @monitor_performance
    async def get_daily_adjusted(self, symbol: str, outputsize: str = "compact") -> Dict[str, Any]:
        """Get daily adjusted stock data"""
        try:
            with operation(f"AlphaVantage daily adjusted data for {symbol}"):
                params = {
                    "function": "TIME_SERIES_DAILY_ADJUSTED",
                    "symbol": symbol,
                    "outputsize": outputsize,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_adjusted_data(data, symbol, "daily")
        except Exception as e:
            error(f"Daily adjusted data error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    @monitor_performance
    async def get_weekly_adjusted(self, symbol: str) -> Dict[str, Any]:
        """Get weekly adjusted stock data"""
        try:
            with operation(f"AlphaVantage weekly adjusted data for {symbol}"):
                params = {
                    "function": "TIME_SERIES_WEEKLY_ADJUSTED",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_adjusted_data(data, symbol, "weekly")
        except Exception as e:
            error(f"Weekly adjusted data error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    @monitor_performance
    async def get_monthly_adjusted(self, symbol: str) -> Dict[str, Any]:
        """Get monthly adjusted stock data"""
        try:
            with operation(f"AlphaVantage monthly adjusted data for {symbol}"):
                params = {
                    "function": "TIME_SERIES_MONTHLY_ADJUSTED",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_adjusted_data(data, symbol, "monthly")
        except Exception as e:
            error(f"Monthly adjusted data error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_adjusted_data(self, data: Dict, symbol: str, interval: str) -> Dict[str, Any]:
        """Transform adjusted time series data"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            # Find the time series key
            time_series_key = None
            for key in data.keys():
                if "Time Series" in key:
                    time_series_key = key
                    break

            if not time_series_key:
                return {"success": False, "error": "No time series data found", "source": "alpha_vantage"}

            time_series = data[time_series_key]
            if not time_series:
                return {"success": False, "error": "Empty time series data", "source": "alpha_vantage"}

            timestamps = []
            opens = []
            highs = []
            lows = []
            closes = []
            adjusted_closes = []
            volumes = []
            dividends = []

            for date_str in sorted(time_series.keys()):
                day_data = time_series[date_str]
                timestamps.append(date_str)
                opens.append(float(day_data.get("1. open", 0)))
                highs.append(float(day_data.get("2. high", 0)))
                lows.append(float(day_data.get("3. low", 0)))
                closes.append(float(day_data.get("4. close", 0)))
                adjusted_closes.append(float(day_data.get("5. adjusted close", 0)))
                volumes.append(int(float(day_data.get("6. volume", 0))))
                dividends.append(float(day_data.get("7. dividend amount", 0)))

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "interval": interval,
                "data": {
                    "timestamps": timestamps,
                    "open": opens,
                    "high": highs,
                    "low": lows,
                    "close": closes,
                    "adjusted_close": adjusted_closes,
                    "volume": volumes,
                    "dividend_amount": dividends
                },
                "current_price": closes[-1] if closes else None,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming adjusted data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Data transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_global_quote(self, symbol: str) -> Dict[str, Any]:
        """Get real-time global quote"""
        try:
            with operation(f"AlphaVantage global quote for {symbol}"):
                params = {
                    "function": "GLOBAL_QUOTE",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_global_quote(data, symbol)
        except Exception as e:
            error(f"Global quote error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_global_quote(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform global quote response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            quote_data = data.get("Global Quote", {})
            if not quote_data:
                return {"success": False, "error": "No quote data found", "source": "alpha_vantage"}

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": {
                    "symbol": quote_data.get("01. symbol", symbol),
                    "open": float(quote_data.get("02. open", 0)),
                    "high": float(quote_data.get("03. high", 0)),
                    "low": float(quote_data.get("04. low", 0)),
                    "price": float(quote_data.get("05. price", 0)),
                    "volume": int(quote_data.get("06. volume", 0)),
                    "latest_trading_day": quote_data.get("07. latest trading day", ""),
                    "previous_close": float(quote_data.get("08. previous close", 0)),
                    "change": float(quote_data.get("09. change", 0)),
                    "change_percent": quote_data.get("10. change percent", "0%")
                },
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming global quote: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Quote transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_realtime_bulk_quotes(self, symbols: List[str]) -> Dict[str, Any]:
        """Get real-time bulk quotes (up to 100 symbols)"""
        try:
            if len(symbols) > 100:
                symbols = symbols[:100]
                warning("Truncated symbols list to 100 items", module="AlphaVantageProvider")

            symbol_string = ",".join(symbols)

            with operation(f"AlphaVantage bulk quotes for {len(symbols)} symbols"):
                params = {
                    "function": "REALTIME_BULK_QUOTES",
                    "symbol": symbol_string,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_bulk_quotes(data, symbols)
        except Exception as e:
            error(f"Bulk quotes error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_bulk_quotes(self, data: Dict, symbols: List[str]) -> Dict[str, Any]:
        """Transform bulk quotes response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            quotes = data.get("quotes", [])
            if not quotes:
                return {"success": False, "error": "No quotes data found", "source": "alpha_vantage"}

            transformed_quotes = []
            for quote in quotes:
                transformed_quotes.append({
                    "symbol": quote.get("symbol", ""),
                    "open": float(quote.get("open", 0)),
                    "high": float(quote.get("high", 0)),
                    "low": float(quote.get("low", 0)),
                    "price": float(quote.get("price", 0)),
                    "volume": int(quote.get("volume", 0)),
                    "change": float(quote.get("change", 0)),
                    "change_percent": quote.get("change_percent", "0%")
                })

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbols": symbols,
                "data": transformed_quotes,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming bulk quotes: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Bulk quotes transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def search_symbols(self, keywords: str) -> Dict[str, Any]:
        """Search for symbols by keywords"""
        try:
            with operation(f"AlphaVantage symbol search for: {keywords}"):
                params = {
                    "function": "SYMBOL_SEARCH",
                    "keywords": keywords,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_search_results(data, keywords)
        except Exception as e:
            error(f"Symbol search error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_search_results(self, data: Dict, keywords: str) -> Dict[str, Any]:
        """Transform symbol search results"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            matches = data.get("bestMatches", [])

            results = []
            for match in matches:
                results.append({
                    "symbol": match.get("1. symbol", ""),
                    "name": match.get("2. name", ""),
                    "type": match.get("3. type", ""),
                    "region": match.get("4. region", ""),
                    "market_open": match.get("5. marketOpen", ""),
                    "market_close": match.get("6. marketClose", ""),
                    "timezone": match.get("7. timezone", ""),
                    "currency": match.get("8. currency", ""),
                    "match_score": float(match.get("9. matchScore", 0))
                })

            return {
                "success": True,
                "source": "alpha_vantage",
                "keywords": keywords,
                "data": results,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming search results: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Search transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_market_status(self) -> Dict[str, Any]:
        """Get global market status"""
        try:
            with operation("AlphaVantage market status"):
                params = {
                    "function": "MARKET_STATUS",
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_market_status(data)
        except Exception as e:
            error(f"Market status error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_market_status(self, data: Dict) -> Dict[str, Any]:
        """Transform market status response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            markets = data.get("markets", [])

            return {
                "success": True,
                "source": "alpha_vantage",
                "data": markets,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming market status: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Market status transformation error: {str(e)}", "source": "alpha_vantage"}


    # OPTIONS DATA METHODS
    @monitor_performance
    async def get_realtime_options(self, symbol: str, require_greeks: bool = False, contract: str = None) -> Dict[str, Any]:
        """Get real-time options data"""
        try:
            with operation(f"AlphaVantage realtime options for {symbol}"):
                params = {
                    "function": "REALTIME_OPTIONS",
                    "symbol": symbol,
                    "require_greeks": str(require_greeks).lower(),
                    "apikey": self.api_key
                }

                if contract:
                    params["contract"] = contract

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_options_data(data, symbol)
        except Exception as e:
            error(f"Realtime options error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    @monitor_performance
    async def get_historical_options(self, symbol: str, date: str = None) -> Dict[str, Any]:
        """Get historical options data"""
        try:
            with operation(f"AlphaVantage historical options for {symbol}"):
                params = {
                    "function": "HISTORICAL_OPTIONS",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                if date:
                    params["date"] = date

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_options_data(data, symbol)
        except Exception as e:
            error(f"Historical options error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_options_data(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform options data response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            options_data = data.get("data", [])

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": options_data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming options data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Options transformation error: {str(e)}", "source": "alpha_vantage"}


    # ALPHA INTELLIGENCE METHODS
    @monitor_performance
    async def get_news_sentiment(self, tickers: str = None, topics: str = None, time_from: str = None,
                                 time_to: str = None, sort: str = "LATEST", limit: int = 50) -> Dict[str, Any]:
        """Get market news and sentiment data"""
        try:
            with operation(f"AlphaVantage news sentiment"):
                params = {
                    "function": "NEWS_SENTIMENT",
                    "sort": sort,
                    "limit": limit,
                    "apikey": self.api_key
                }

                if tickers:
                    params["tickers"] = tickers
                if topics:
                    params["topics"] = topics
                if time_from:
                    params["time_from"] = time_from
                if time_to:
                    params["time_to"] = time_to

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_news_sentiment(data)
        except Exception as e:
            error(f"News sentiment error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_news_sentiment(self, data: Dict) -> Dict[str, Any]:
        """Transform news sentiment response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming news sentiment: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"News sentiment transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_earnings_call_transcript(self, symbol: str, quarter: str) -> Dict[str, Any]:
        """Get earnings call transcript"""
        try:
            with operation(f"AlphaVantage earnings call transcript for {symbol}"):
                params = {
                    "function": "EARNINGS_CALL_TRANSCRIPT",
                    "symbol": symbol,
                    "quarter": quarter,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_earnings_transcript(data, symbol, quarter)
        except Exception as e:
            error(f"Earnings transcript error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_earnings_transcript(self, data: Dict, symbol: str, quarter: str) -> Dict[str, Any]:
        """Transform earnings transcript response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "quarter": quarter,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming earnings transcript: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Earnings transcript transformation error: {str(e)}",
                    "source": "alpha_vantage"}


    @monitor_performance
    async def get_top_gainers_losers(self) -> Dict[str, Any]:
        """Get top gainers, losers, and most active stocks"""
        try:
            with operation("AlphaVantage top gainers losers"):
                params = {
                    "function": "TOP_GAINERS_LOSERS",
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_top_gainers_losers(data)
        except Exception as e:
            error(f"Top gainers losers error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_top_gainers_losers(self, data: Dict) -> Dict[str, Any]:
        """Transform top gainers/losers response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "data": {
                    "metadata": data.get("metadata", ""),
                    "last_updated": data.get("last_updated", ""),
                    "top_gainers": data.get("top_gainers", []),
                    "top_losers": data.get("top_losers", []),
                    "most_actively_traded": data.get("most_actively_traded", [])
                },
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming top gainers losers: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Top gainers losers transformation error: {str(e)}",
                    "source": "alpha_vantage"}


    @monitor_performance
    async def get_insider_transactions(self, symbol: str) -> Dict[str, Any]:
        """Get insider transactions"""
        try:
            with operation(f"AlphaVantage insider transactions for {symbol}"):
                params = {
                    "function": "INSIDER_TRANSACTIONS",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_insider_transactions(data, symbol)
        except Exception as e:
            error(f"Insider transactions error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_insider_transactions(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform insider transactions response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming insider transactions: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Insider transactions transformation error: {str(e)}",
                    "source": "alpha_vantage"}


    @monitor_performance
    async def get_analytics_fixed_window(self, symbols: str, range_param: str, interval: str,
                                         ohlc: str = "close", calculations: str = None) -> Dict[str, Any]:
        """Get advanced analytics for fixed window"""
        try:
            with operation(f"AlphaVantage analytics fixed window"):
                params = {
                    "function": "ANALYTICS_FIXED_WINDOW",
                    "SYMBOLS": symbols,
                    "RANGE": range_param,
                    "INTERVAL": interval,
                    "OHLC": ohlc,
                    "apikey": self.api_key
                }

                if calculations:
                    params["CALCULATIONS"] = calculations

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_analytics_data(data, "fixed_window")
        except Exception as e:
            error(f"Analytics fixed window error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    @monitor_performance
    async def get_analytics_sliding_window(self, symbols: str, range_param: str, interval: str,
                                           window_size: int, ohlc: str = "close", calculations: str = None) -> Dict[
        str, Any]:
        """Get advanced analytics for sliding window"""
        try:
            with operation(f"AlphaVantage analytics sliding window"):
                params = {
                    "function": "ANALYTICS_SLIDING_WINDOW",
                    "SYMBOLS": symbols,
                    "RANGE": range_param,
                    "INTERVAL": interval,
                    "WINDOW_SIZE": window_size,
                    "OHLC": ohlc,
                    "apikey": self.api_key
                }

                if calculations:
                    params["CALCULATIONS"] = calculations

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_analytics_data(data, "sliding_window")
        except Exception as e:
            error(f"Analytics sliding window error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_analytics_data(self, data: Dict, window_type: str) -> Dict[str, Any]:
        """Transform analytics data response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "window_type": window_type,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming analytics data: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Analytics transformation error: {str(e)}", "source": "alpha_vantage"}


    # FUNDAMENTAL DATA METHODS
    @monitor_performance
    async def get_company_overview(self, symbol: str) -> Dict[str, Any]:
        """Get company overview and fundamentals"""
        try:
            with operation(f"AlphaVantage company overview for {symbol}"):
                params = {
                    "function": "OVERVIEW",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_company_overview(data, symbol)
        except Exception as e:
            error(f"Company overview error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_company_overview(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform company overview response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming company overview: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Company overview transformation error: {str(e)}",
                    "source": "alpha_vantage"}


    @monitor_performance
    async def get_etf_profile(self, symbol: str) -> Dict[str, Any]:
        """Get ETF profile and holdings"""
        try:
            with operation(f"AlphaVantage ETF profile for {symbol}"):
                params = {
                    "function": "ETF_PROFILE",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_etf_profile(data, symbol)
        except Exception as e:
            error(f"ETF profile error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_etf_profile(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform ETF profile response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming ETF profile: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"ETF profile transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_dividends(self, symbol: str) -> Dict[str, Any]:
        """Get dividend history"""
        try:
            with operation(f"AlphaVantage dividends for {symbol}"):
                params = {
                    "function": "DIVIDENDS",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_dividends(data, symbol)
        except Exception as e:
            error(f"Dividends error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_dividends(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform dividends response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming dividends: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Dividends transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_splits(self, symbol: str) -> Dict[str, Any]:
        """Get stock splits history"""
        try:
            with operation(f"AlphaVantage splits for {symbol}"):
                params = {
                    "function": "SPLITS",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_splits(data, symbol)
        except Exception as e:
            error(f"Splits error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_splits(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform splits response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming splits: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Splits transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_income_statement(self, symbol: str) -> Dict[str, Any]:
        """Get income statement"""
        try:
            with operation(f"AlphaVantage income statement for {symbol}"):
                params = {
                    "function": "INCOME_STATEMENT",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_financial_statement(data, symbol, "income_statement")
        except Exception as e:
            error(f"Income statement error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    @monitor_performance
    async def get_balance_sheet(self, symbol: str) -> Dict[str, Any]:
        """Get balance sheet"""
        try:
            with operation(f"AlphaVantage balance sheet for {symbol}"):
                params = {
                    "function": "BALANCE_SHEET",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_financial_statement(data, symbol, "balance_sheet")
        except Exception as e:
            error(f"Balance sheet error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    @monitor_performance
    async def get_cash_flow(self, symbol: str) -> Dict[str, Any]:
        """Get cash flow statement"""
        try:
            with operation(f"AlphaVantage cash flow for {symbol}"):
                params = {
                    "function": "CASH_FLOW",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_financial_statement(data, symbol, "cash_flow")
        except Exception as e:
            error(f"Cash flow error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_financial_statement(self, data: Dict, symbol: str, statement_type: str) -> Dict[str, Any]:
        """Transform financial statement response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "statement_type": statement_type,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming financial statement: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Financial statement transformation error: {str(e)}",
                    "source": "alpha_vantage"}


    @monitor_performance
    async def get_earnings(self, symbol: str) -> Dict[str, Any]:
        """Get earnings history"""
        try:
            with operation(f"AlphaVantage earnings for {symbol}"):
                params = {
                    "function": "EARNINGS",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_earnings(data, symbol)
        except Exception as e:
            error(f"Earnings error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_earnings(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform earnings response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming earnings: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Earnings transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_earnings_estimates(self, symbol: str) -> Dict[str, Any]:
        """Get earnings estimates"""
        try:
            with operation(f"AlphaVantage earnings estimates for {symbol}"):
                params = {
                    "function": "EARNINGS_ESTIMATES",
                    "symbol": symbol,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_earnings_estimates(data, symbol)
        except Exception as e:
            error(f"Earnings estimates error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_earnings_estimates(self, data: Dict, symbol: str) -> Dict[str, Any]:
        """Transform earnings estimates response"""
        try:
            error_response = self._check_api_errors(data)
            if error_response:
                return error_response

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "data": data,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming earnings estimates: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Earnings estimates transformation error: {str(e)}",
                    "source": "alpha_vantage"}


    @monitor_performance
    async def get_listing_status(self, date: str = None, state: str = "active") -> Dict[str, Any]:
        """Get listing and delisting status"""
        try:
            with operation("AlphaVantage listing status"):
                params = {
                    "function": "LISTING_STATUS",
                    "state": state,
                    "apikey": self.api_key
                }

                if date:
                    params["date"] = date

                # This endpoint returns CSV, so we need special handling
                session = await self._get_session()
                async with session.get(self.base_url, params=params) as response:
                    if response.status == 200:
                        csv_data = await response.text()
                        return self._transform_listing_status(csv_data, date, state)
                    else:
                        return {"success": False, "error": f"HTTP {response.status}", "source": "alpha_vantage"}

        except Exception as e:
            error(f"Listing status error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_listing_status(self, csv_data: str, date: str, state: str) -> Dict[str, Any]:
        """Transform listing status CSV response"""
        try:
            lines = csv_data.strip().split('\n')
            if len(lines) < 2:
                return {"success": False, "error": "No listing data found", "source": "alpha_vantage"}

            headers = lines[0].split(',')
            data_rows = []

            for line in lines[1:]:
                values = line.split(',')
                if len(values) == len(headers):
                    row_data = dict(zip(headers, values))
                    data_rows.append(row_data)

            return {
                "success": True,
                "source": "alpha_vantage",
                "date": date,
                "state": state,
                "data": data_rows,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming listing status: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Listing status transformation error: {str(e)}", "source": "alpha_vantage"}


    @monitor_performance
    async def get_earnings_calendar(self, symbol: str = None, horizon: str = "3month") -> Dict[str, Any]:
        """Get earnings calendar"""
        try:
            with operation("AlphaVantage earnings calendar"):
                params = {
                    "function": "EARNINGS_CALENDAR",
                    "horizon": horizon,
                    "apikey": self.api_key
                }

                if symbol:
                    params["symbol"] = symbol

                # This endpoint returns CSV
                session = await self._get_session()
                async with session.get(self.base_url, params=params) as response:
                    if response.status == 200:
                        csv_data = await response.text()
                        return self._transform_earnings_calendar(csv_data, symbol, horizon)
                    else:
                        return {"success": False, "error": f"HTTP {response.status}", "source": "alpha_vantage"}

        except Exception as e:
            error(f"Earnings calendar error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_earnings_calendar(self, csv_data: str, symbol: str, horizon: str) -> Dict[str, Any]:
        """Transform earnings calendar CSV response"""
        try:
            lines = csv_data.strip().split('\n')
            if len(lines) < 2:
                return {"success": False, "error": "No earnings calendar data found", "source": "alpha_vantage"}

            headers = lines[0].split(',')
            data_rows = []

            for line in lines[1:]:
                values = line.split(',')
                if len(values) == len(headers):
                    row_data = dict(zip(headers, values))
                    data_rows.append(row_data)

            return {
                "success": True,
                "source": "alpha_vantage",
                "symbol": symbol,
                "horizon": horizon,
                "data": data_rows,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming earnings calendar: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"Earnings calendar transformation error: {str(e)}",
                    "source": "alpha_vantage"}


    @monitor_performance
    async def get_ipo_calendar(self) -> Dict[str, Any]:
        """Get IPO calendar"""
        try:
            with operation("AlphaVantage IPO calendar"):
                params = {
                    "function": "IPO_CALENDAR",
                    "apikey": self.api_key
                }

                # This endpoint returns CSV
                session = await self._get_session()
                async with session.get(self.base_url, params=params) as response:
                    if response.status == 200:
                        csv_data = await response.text()
                        return self._transform_ipo_calendar(csv_data)
                    else:
                        return {"success": False, "error": f"HTTP {response.status}", "source": "alpha_vantage"}

        except Exception as e:
            error(f"IPO calendar error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}


    def _transform_ipo_calendar(self, csv_data: str) -> Dict[str, Any]:
        """Transform IPO calendar CSV response"""
        try:
            lines = csv_data.strip().split('\n')
            if len(lines) < 2:
                return {"success": False, "error": "No IPO calendar data found", "source": "alpha_vantage"}

            headers = lines[0].split(',')
            data_rows = []

            for line in lines[1:]:
                values = line.split(',')
                if len(values) == len(headers):
                    row_data = dict(zip(headers, values))
                    data_rows.append(row_data)

            return {
                "success": True,
                "source": "alpha_vantage",
                "data": data_rows,
                "fetched_at": datetime.now().isoformat()
            }

        except Exception as e:
            error(f"Error transforming IPO calendar: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": f"IPO calendar transformation error: {str(e)}", "source": "alpha_vantage"}


    # EXTENDED FOREX METHODS
    @monitor_performance
    async def get_currency_exchange_rate(self, from_currency: str, to_currency: str) -> Dict[str, Any]:
        """Get real-time currency exchange rate"""
        try:
            with operation(f"AlphaVantage exchange rate {from_currency}/{to_currency}"):
                params = {
                    "function": "CURRENCY_EXCHANGE_RATE",
                    "from_currency": from_currency,
                    "to_currency": to_currency,
                    "apikey": self.api_key
                }

                data = await self._make_request(params)
                if not data.get("success", True):
                    return data

                return self._transform_exchange_rate(data, from_currency, to_currency)
        except Exception as e:
            error(f"Exchange rate error: {str(e)}", module="AlphaVantageProvider")
            return {"success": False, "error": str(e), "source": "alpha_vantage"}