# FMP (Financial Modeling Prep) Data Wrapper
# Modular, fault-tolerant design - each endpoint works independently
# Requires API key from https://financialmodelingprep.com/developer/docs/

import sys
import json
import requests
import pandas as pd
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import traceback
import os


class FMPError:
    """Custom error class for FMP API errors"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "endpoint": self.endpoint,
            "error": self.error,
            "status_code": self.status_code,
            "timestamp": self.timestamp,
            "type": "FMPError"
        }


class FMPDataWrapper:
    """Modular FMP data wrapper with fault-tolerant endpoints"""

    def __init__(self, api_key: Optional[str] = None):
        self.base_url = "https://financialmodelingprep.com/api/v3"
        self.api_key = api_key or os.environ.get('FMP_API_KEY', '')
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0'
        })

        # Common API parameters
        self.default_limit = 10

        # Interval mappings
        self.interval_map = {
            "1m": "1min", "5m": "5min", "15m": "15min", "30m": "30min",
            "1h": "1hour", "4h": "4hour", "1d": "day"
        }

        # Period mappings
        self.period_map = {
            "annual": "annual", "quarter": "quarter", "ttm": "ttm"
        }

        # Common parameters for statements
        self.statement_periods = ["annual", "quarter"]

    def _make_request(self, url: str) -> Dict[str, Any]:
        """Make HTTP request with error handling"""
        try:
            response = self.session.get(url, timeout=30)
            response.raise_for_status()

            # Parse JSON response
            data = response.json()

            # Check for FMP API errors
            if isinstance(data, dict):
                error_message = data.get("Error Message", data.get("error"))
                if error_message:
                    if any(keyword in error_message.lower() for keyword in ["upgrade", "subscription", "exclusive", "unauthorized"]):
                        raise Exception(f"API Access Error: {error_message}. This feature may require a premium subscription.")
                    else:
                        raise Exception(f"FMP API Error: {error_message}")

            return data

        except requests.exceptions.RequestException as e:
            raise Exception(f"HTTP request failed: {str(e)}")
        except json.JSONDecodeError as e:
            raise Exception(f"JSON decode error: {str(e)}")

    def _build_url(self, endpoint: str, params: Dict[str, Any] = None) -> str:
        """Build API URL with parameters"""
        url = f"{self.base_url}/{endpoint}?apikey={self.api_key}"

        if params:
            # Filter out None values
            filtered_params = {k: v for k, v in params.items() if v is not None}
            # Add parameters to URL
            for key, value in filtered_params.items():
                if key not in ['symbol']:  # symbol is already in endpoint
                    url += f"&{key}={value}"

        return url

    # EQUITY DATA ENDPOINTS

    def get_equity_quote(self, symbols: str) -> Dict[str, Any]:
        """Get real-time stock quotes for multiple symbols"""
        try:
            if not symbols:
                return {"error": FMPError("equity_quote", "Symbols parameter is required").to_dict()}

            symbol_list = [s.strip() for s in symbols.split(",")]
            results = []

            for symbol in symbol_list:
                try:
                    url = self._build_url(f"quote/{symbol}")
                    data = self._make_request(url)

                    if isinstance(data, list) and len(data) > 0:
                        results.extend(data)
                    elif isinstance(data, dict):
                        results.append(data)

                except Exception as e:
                    # Continue with other symbols if one fails
                    continue

            if not results:
                return {"error": FMPError("equity_quote", f"No data found for symbols: {symbols}").to_dict()}

            return {
                "success": True,
                "data": results,
                "parameters": {"symbols": symbols}
            }

        except Exception as e:
            return {"error": FMPError("equity_quote", str(e)).to_dict()}

    def get_company_profile(self, symbol: str) -> Dict[str, Any]:
        """Get company profile and basic information"""
        try:
            if not symbol:
                return {"error": FMPError("company_profile", "Symbol parameter is required").to_dict()}

            url = self._build_url(f"profile/{symbol}")
            data = self._make_request(url)

            if not data:
                return {"error": FMPError("company_profile", f"No data found for symbol: {symbol}").to_dict()}

            # Handle single company or list
            if isinstance(data, list):
                data = data[0] if data else {}

            return {
                "success": True,
                "data": data,
                "parameters": {"symbol": symbol}
            }

        except Exception as e:
            return {"error": FMPError("company_profile", str(e)).to_dict()}

    def get_historical_prices(self, symbol: str, start_date: Optional[str] = None,
                            end_date: Optional[str] = None, interval: str = "1d") -> Dict[str, Any]:
        """Get historical price data for a symbol"""
        try:
            if not symbol:
                return {"error": FMPError("historical_prices", "Symbol parameter is required").to_dict()}

            # Default dates - last year if not specified
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")
            if not start_date:
                start_date = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")

            # Map interval
            fmp_interval = self.interval_map.get(interval, "day")

            # Build URL
            params = {
                "from": start_date,
                "to": end_date
            }

            if fmp_interval == "day":
                # Use historical price full endpoint for daily data
                url = self._build_url(f"historical-price-full/{symbol}", params)
                data = self._make_request(url)
                historical_data = data.get("historical", []) if isinstance(data, dict) else []
            else:
                # Use chart endpoint for intraday data
                url = self._build_url(f"historical-chart/{fmp_interval}/{symbol}", params)
                historical_data = self._make_request(url)
                if not isinstance(historical_data, list):
                    historical_data = []

            if not historical_data:
                return {"error": FMPError("historical_prices", f"No historical data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": historical_data,
                "parameters": {
                    "symbol": symbol,
                    "start_date": start_date,
                    "end_date": end_date,
                    "interval": interval
                }
            }

        except Exception as e:
            return {"error": FMPError("historical_prices", str(e)).to_dict()}

    # FINANCIAL STATEMENTS ENDPOINTS

    def get_income_statement(self, symbol: str, period: str = "annual", limit: int = 10) -> Dict[str, Any]:
        """Get income statement data"""
        try:
            if not symbol:
                return {"error": FMPError("income_statement", "Symbol parameter is required").to_dict()}

            if period not in self.statement_periods:
                period = "annual"

            params = {"period": period, "limit": limit}
            url = self._build_url(f"income-statement/{symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("income_statement", f"No income statement data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "period": period,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("income_statement", str(e)).to_dict()}

    def get_balance_sheet(self, symbol: str, period: str = "annual", limit: int = 10) -> Dict[str, Any]:
        """Get balance sheet data"""
        try:
            if not symbol:
                return {"error": FMPError("balance_sheet", "Symbol parameter is required").to_dict()}

            if period not in self.statement_periods:
                period = "annual"

            params = {"period": period, "limit": limit}
            url = self._build_url(f"balance-sheet-statement/{symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("balance_sheet", f"No balance sheet data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "period": period,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("balance_sheet", str(e)).to_dict()}

    def get_cash_flow_statement(self, symbol: str, period: str = "annual", limit: int = 10) -> Dict[str, Any]:
        """Get cash flow statement data"""
        try:
            if not symbol:
                return {"error": FMPError("cash_flow_statement", "Symbol parameter is required").to_dict()}

            if period not in self.statement_periods:
                period = "annual"

            params = {"period": period, "limit": limit}
            url = self._build_url(f"cash-flow-statement/{symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("cash_flow_statement", f"No cash flow data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "period": period,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("cash_flow_statement", str(e)).to_dict()}

    def get_financial_ratios(self, symbol: str, period: str = "annual", limit: int = 10) -> Dict[str, Any]:
        """Get financial ratios data"""
        try:
            if not symbol:
                return {"error": FMPError("financial_ratios", "Symbol parameter is required").to_dict()}

            # Support TTM (trailing twelve months) for ratios
            if period not in self.statement_periods + ["ttm"]:
                period = "annual"

            if period == "ttm":
                url = self._build_url(f"ratios-ttm/{symbol}")
                data = self._make_request(url)
                # TTM returns single object, convert to list
                if isinstance(data, dict):
                    data = [data]
            else:
                params = {"period": period, "limit": limit}
                url = self._build_url(f"ratios/{symbol}", params)
                data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("financial_ratios", f"No financial ratios found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "period": period,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("financial_ratios", str(e)).to_dict()}

    def get_key_metrics(self, symbol: str, period: str = "annual", limit: int = 10) -> Dict[str, Any]:
        """Get key metrics data"""
        try:
            if not symbol:
                return {"error": FMPError("key_metrics", "Symbol parameter is required").to_dict()}

            if period not in self.statement_periods:
                period = "annual"

            params = {"period": period, "limit": limit}
            url = self._build_url(f"key-metrics/{symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("key_metrics", f"No key metrics found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "period": period,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("key_metrics", str(e)).to_dict()}

    # MARKET DATA ENDPOINTS

    def get_market_snapshots(self) -> Dict[str, Any]:
        """Get market snapshots and indices"""
        try:
            # Get major indices
            indices_url = self._build_url("majors-indexes")
            indices_data = self._make_request(indices_url)

            # Get market sectors
            sectors_url = self._build_url("sectors-performance")
            sectors_data = self._make_request(sectors_url)

            # Get market cap data
            market_cap_url = self._build_url("market-capitalization/AAPL")  # Use AAPL as example
            market_cap_data = self._make_request(market_cap_url)

            result = {
                "indices": indices_data if isinstance(indices_data, list) else [],
                "sectors": sectors_data if isinstance(sectors_data, list) else [],
                "market_cap_example": market_cap_data
            }

            return {
                "success": True,
                "data": result,
                "parameters": {}
            }

        except Exception as e:
            return {"error": FMPError("market_snapshots", str(e)).to_dict()}

    def get_treasury_rates(self) -> Dict[str, Any]:
        """Get current treasury rates"""
        try:
            url = self._build_url("treasury")
            data = self._make_request(url)

            if not data:
                return {"error": FMPError("treasury_rates", "No treasury rates data found").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {}
            }

        except Exception as e:
            return {"error": FMPError("treasury_rates", str(e)).to_dict()}

    # ETF DATA ENDPOINTS

    def get_etf_info(self, symbol: str) -> Dict[str, Any]:
        """Get ETF information"""
        try:
            if not symbol:
                return {"error": FMPError("etf_info", "Symbol parameter is required").to_dict()}

            url = self._build_url(f"etf-info/{symbol}")
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("etf_info", f"No ETF data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {"symbol": symbol}
            }

        except Exception as e:
            return {"error": FMPError("etf_info", str(e)).to_dict()}

    def get_etf_holdings(self, symbol: str) -> Dict[str, Any]:
        """Get ETF holdings data"""
        try:
            if not symbol:
                return {"error": FMPError("etf_holdings", "Symbol parameter is required").to_dict()}

            url = self._build_url(f"etf-holder/{symbol}")
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("etf_holdings", f"No ETF holdings found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {"symbol": symbol}
            }

        except Exception as e:
            return {"error": FMPError("etf_holdings", str(e)).to_dict()}

    # CRYPTO DATA ENDPOINTS

    def get_crypto_list(self) -> Dict[str, Any]:
        """Get list of available cryptocurrencies"""
        try:
            url = self._build_url("cryptocurrency/list")
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("crypto_list", "No cryptocurrency data found").to_dict()}

            return {
                "success": True,
                "data": data[:100],  # Limit to first 100 for performance
                "parameters": {}
            }

        except Exception as e:
            return {"error": FMPError("crypto_list", str(e)).to_dict()}

    def get_crypto_historical(self, symbol: str, start_date: Optional[str] = None,
                            end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get historical cryptocurrency prices"""
        try:
            if not symbol:
                return {"error": FMPError("crypto_historical", "Symbol parameter is required").to_dict()}

            # Default dates - last year if not specified
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")
            if not start_date:
                start_date = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")

            params = {
                "from": start_date,
                "to": end_date
            }

            url = self._build_url(f"historical-price-crypto/{symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("crypto_historical", f"No crypto data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "start_date": start_date,
                    "end_date": end_date
                }
            }

        except Exception as e:
            return {"error": FMPError("crypto_historical", str(e)).to_dict()}

    # NEWS DATA ENDPOINTS

    def get_company_news(self, symbol: str, limit: int = 50) -> Dict[str, Any]:
        """Get news for a specific company"""
        try:
            if not symbol:
                return {"error": FMPError("company_news", "Symbol parameter is required").to_dict()}

            params = {"limit": limit}
            url = self._build_url(f"stock_news?tickers={symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("company_news", f"No news found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("company_news", str(e)).to_dict()}

    def get_general_news(self) -> Dict[str, Any]:
        """Get general financial news"""
        try:
            url = self._build_url("stock_news")
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("general_news", "No news data found").to_dict()}

            return {
                "success": True,
                "data": data[:100],  # Limit to first 100 for performance
                "parameters": {}
            }

        except Exception as e:
            return {"error": FMPError("general_news", str(e)).to_dict()}

    # ECONOMIC DATA ENDPOINTS

    def get_economic_calendar(self) -> Dict[str, Any]:
        """Get economic calendar data"""
        try:
            url = self._build_url("economic_calendar")
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("economic_calendar", "No economic calendar data found").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {}
            }

        except Exception as e:
            return {"error": FMPError("economic_calendar", str(e)).to_dict()}

    # INSIDER TRADING ENDPOINTS

    def get_insider_trading(self, symbol: str, limit: int = 100) -> Dict[str, Any]:
        """Get insider trading data for a symbol"""
        try:
            if not symbol:
                return {"error": FMPError("insider_trading", "Symbol parameter is required").to_dict()}

            params = {"limit": limit}
            url = self._build_url(f"insider-trading/{symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("insider_trading", f"No insider trading data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("insider_trading", str(e)).to_dict()}

    # INSTITUTIONAL OWNERSHIP ENDPOINTS

    def get_institutional_ownership(self, symbol: str, limit: int = 100) -> Dict[str, Any]:
        """Get institutional ownership data"""
        try:
            if not symbol:
                return {"error": FMPError("institutional_ownership", "Symbol parameter is required").to_dict()}

            params = {"limit": limit}
            url = self._build_url(f"institutional-holder/{symbol}", params)
            data = self._make_request(url)

            if not data or not isinstance(data, list):
                return {"error": FMPError("institutional_ownership", f"No institutional ownership data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": data,
                "parameters": {
                    "symbol": symbol,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("institutional_ownership", str(e)).to_dict()}

    # COMPREHENSIVE DATA ENDPOINTS

    def get_company_overview(self, symbol: str) -> Dict[str, Any]:
        """Get comprehensive company overview including profile, quotes, and key metrics"""
        try:
            if not symbol:
                return {"error": FMPError("company_overview", "Symbol parameter is required").to_dict()}

            results = {}

            # 1. Get company profile
            profile_result = self.get_company_profile(symbol)
            results["profile"] = profile_result

            # 2. Get current quote
            quote_result = self.get_equity_quote(symbol)
            results["quote"] = quote_result

            # 3. Get key metrics
            metrics_result = self.get_key_metrics(symbol, "annual", 1)
            results["key_metrics"] = metrics_result

            # 4. Get financial ratios
            ratios_result = self.get_financial_ratios(symbol, "ttm", 1)
            results["financial_ratios"] = ratios_result

            # 5. Get recent news
            news_result = self.get_company_news(symbol, 10)
            results["recent_news"] = news_result

            # Check if we have any successful data
            has_data = any(
                result.get("success") and result.get("data")
                for result in results.values()
            )

            if not has_data:
                return {"error": FMPError("company_overview", f"No data found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": results,
                "parameters": {"symbol": symbol}
            }

        except Exception as e:
            return {"error": FMPError("company_overview", str(e)).to_dict()}

    def get_financial_statements(self, symbol: str, period: str = "annual", limit: int = 5) -> Dict[str, Any]:
        """Get all financial statements for a company"""
        try:
            if not symbol:
                return {"error": FMPError("financial_statements", "Symbol parameter is required").to_dict()}

            results = {}

            # 1. Get income statement
            income_result = self.get_income_statement(symbol, period, limit)
            results["income_statement"] = income_result

            # 2. Get balance sheet
            balance_result = self.get_balance_sheet(symbol, period, limit)
            results["balance_sheet"] = balance_result

            # 3. Get cash flow statement
            cash_flow_result = self.get_cash_flow_statement(symbol, period, limit)
            results["cash_flow_statement"] = cash_flow_result

            # 4. Get financial ratios
            ratios_result = self.get_financial_ratios(symbol, period, limit)
            results["financial_ratios"] = ratios_result

            # 5. Get key metrics
            metrics_result = self.get_key_metrics(symbol, period, limit)
            results["key_metrics"] = metrics_result

            # Check if we have any successful data
            has_data = any(
                result.get("success") and result.get("data")
                for result in results.values()
            )

            if not has_data:
                return {"error": FMPError("financial_statements", f"No financial statements found for symbol: {symbol}").to_dict()}

            return {
                "success": True,
                "data": results,
                "parameters": {
                    "symbol": symbol,
                    "period": period,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": FMPError("financial_statements", str(e)).to_dict()}


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python fmp_data.py <command> [args...]",
            "commands": [
                "equity_quote [symbols]",
                "company_profile [symbol]",
                "historical_prices [symbol] [start_date] [end_date] [interval]",
                "income_statement [symbol] [period] [limit]",
                "balance_sheet [symbol] [period] [limit]",
                "cash_flow_statement [symbol] [period] [limit]",
                "financial_ratios [symbol] [period] [limit]",
                "key_metrics [symbol] [period] [limit]",
                "market_snapshots",
                "treasury_rates",
                "etf_info [symbol]",
                "etf_holdings [symbol]",
                "crypto_list",
                "crypto_historical [symbol] [start_date] [end_date]",
                "company_news [symbol] [limit]",
                "general_news",
                "economic_calendar",
                "insider_trading [symbol] [limit]",
                "institutional_ownership [symbol] [limit]",
                "company_overview [symbol]",
                "financial_statements [symbol] [period] [limit]"
            ],
            "note": "FMP API key required. Set FMP_API_KEY environment variable or pass to wrapper constructor."
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = FMPDataWrapper()

    try:
        if command == "equity_quote":
            symbols = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_equity_quote(symbols)

        elif command == "company_profile":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_company_profile(symbol)

        elif command == "historical_prices":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None
            interval = sys.argv[5] if len(sys.argv) > 5 else "1d"
            result = wrapper.get_historical_prices(symbol, start_date, end_date, interval)

        elif command == "income_statement":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            period = sys.argv[3] if len(sys.argv) > 3 else "annual"
            limit = int(sys.argv[4]) if len(sys.argv) > 4 else 10
            result = wrapper.get_income_statement(symbol, period, limit)

        elif command == "balance_sheet":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            period = sys.argv[3] if len(sys.argv) > 3 else "annual"
            limit = int(sys.argv[4]) if len(sys.argv) > 4 else 10
            result = wrapper.get_balance_sheet(symbol, period, limit)

        elif command == "cash_flow_statement":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            period = sys.argv[3] if len(sys.argv) > 3 else "annual"
            limit = int(sys.argv[4]) if len(sys.argv) > 4 else 10
            result = wrapper.get_cash_flow_statement(symbol, period, limit)

        elif command == "financial_ratios":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            period = sys.argv[3] if len(sys.argv) > 3 else "annual"
            limit = int(sys.argv[4]) if len(sys.argv) > 4 else 10
            result = wrapper.get_financial_ratios(symbol, period, limit)

        elif command == "key_metrics":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            period = sys.argv[3] if len(sys.argv) > 3 else "annual"
            limit = int(sys.argv[4]) if len(sys.argv) > 4 else 10
            result = wrapper.get_key_metrics(symbol, period, limit)

        elif command == "market_snapshots":
            result = wrapper.get_market_snapshots()

        elif command == "treasury_rates":
            result = wrapper.get_treasury_rates()

        elif command == "etf_info":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_etf_info(symbol)

        elif command == "etf_holdings":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_etf_holdings(symbol)

        elif command == "crypto_list":
            result = wrapper.get_crypto_list()

        elif command == "crypto_historical":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None
            result = wrapper.get_crypto_historical(symbol, start_date, end_date)

        elif command == "company_news":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 50
            result = wrapper.get_company_news(symbol, limit)

        elif command == "general_news":
            result = wrapper.get_general_news()

        elif command == "economic_calendar":
            result = wrapper.get_economic_calendar()

        elif command == "insider_trading":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = wrapper.get_insider_trading(symbol, limit)

        elif command == "institutional_ownership":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = wrapper.get_institutional_ownership(symbol, limit)

        elif command == "company_overview":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_company_overview(symbol)

        elif command == "financial_statements":
            symbol = sys.argv[2] if len(sys.argv) > 2 else None
            period = sys.argv[3] if len(sys.argv) > 3 else "annual"
            limit = int(sys.argv[4]) if len(sys.argv) > 4 else 5
            result = wrapper.get_financial_statements(symbol, period, limit)

        else:
            result = {"error": FMPError(command, f"Unknown command: {command}").to_dict()}

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({"error": FMPError(command, str(e)).to_dict()}, indent=2))


if __name__ == "__main__":
    main()