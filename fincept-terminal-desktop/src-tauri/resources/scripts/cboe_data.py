# CBOE (Chicago Board Options Exchange) Data Wrapper
# Based on OpenBB CBOE provider - https://github.com/OpenBB-finance/OpenBB/tree/main/openbb_platform/providers/cboe

import sys
import json
import requests
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Union, Any, Literal
from io import StringIO
import pandas as pd

# CBOE API URLs
BASE_URL = "https://cdn.cboe.com/api/global/delayed_quotes"
EU_BASE_URL = "https://cdn.cboe.com/api/global/european_indices"
US_INDICES_URL = "https://cdn.cboe.com/api/global/delayed_quotes/quotes/all_us_indices.json"
EU_INDICES_URL = "https://cdn.cboe.com/api/global/european_indices/index_quotes/all-indices.json"

# CBOE European Index Constituents (from OpenBB constants)
EU_INDEX_CONSTITUENTS = [
    "BAT20P", "BBE20P", "BCH20P", "BCHM30P", "BDE40P", "BDEM50P", "BDES50P", "BDK25P",
    "BEP50P", "BEPACP", "BEPBUS", "BEPCNC", "BEPCONC", "BEPCONS", "BEPENGY", "BEPFIN",
    "BEPHLTH", "BEPIND", "BEPNEM", "BEPTEC", "BEPTEL", "BEPUTL", "BEPXUKP", "BES35P",
    "BEZ50P", "BEZACP", "BFI25P", "BFR40P", "BFRM20P", "BIE20P", "BIT40P", "BNL25P",
    "BNLM25P", "BNO25G", "BNORD40P", "BPT20P", "BSE30P", "BUK100P", "BUK250P", "BUK350P",
    "BUKAC", "BUKBISP", "BUKBUS", "BUKCNC", "BUKCONC", "BUKCONS", "BUKENGY", "BUKFIN",
    "BUKHI50P", "BUKHLTH", "BUKIND", "BUKLO50P", "BUKMINP", "BUKNEM", "BUKSC", "BUKTEC",
    "BUKTEL", "BUKUTL"
]

# VIX Futures Symbols (from OpenBB)
VIX_SYMBOLS = ["VX_AM", "VX_EOD"]

# Ticker Exceptions (from OpenBB)
TICKER_EXCEPTIONS = ["VIX", "VX", "SPX", "SPEU", "NDX", "NDXE", "RUT", "RUTE"]


class CBOEError:
    """Custom error class for CBOE API errors"""
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


class CBOEDataAPI:
    """CBOE Data API wrapper for modular data fetching"""

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json',
            'Content-Type': 'application/json'
        })

        # Cache for directories (24-hour cache like OpenBB)
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

    def _make_request(self, url: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        """Make HTTP request with error handling"""
        try:
            response = self.session.get(url, params=params, timeout=30)
            response.raise_for_status()

            data = response.json()

            if "error" in data:
                return CBOEError(url, data["error"], response.status_code).to_dict()

            return {"success": True, "data": data}

        except requests.exceptions.RequestException as e:
            return CBOEError(url, str(e), getattr(e.response, 'status_code', None)).to_dict()
        except json.JSONDecodeError as e:
            return CBOEError(url, f"JSON decode error: {str(e)}").to_dict()
        except Exception as e:
            return CBOEError(url, f"Unexpected error: {str(e)}").to_dict()

    def _parse_dataframe_response(self, data: Dict) -> pd.DataFrame:
        """Parse response into DataFrame"""
        if "data" not in data or not isinstance(data["data"], list):
            return pd.DataFrame()

        return pd.DataFrame(data["data"])

    def get_equity_quote(self, symbol: str) -> Dict[str, Any]:
        """Get real-time equity quote with implied volatility data

        Args:
            symbol: Stock symbol (e.g., "AAPL", "MSFT")

        Returns:
            Dict containing equity quote data
        """
        try:
            symbol_clean = symbol.replace("^", "").upper()

            # Determine URL pattern based on ticker exceptions
            if symbol_clean in TICKER_EXCEPTIONS:
                url = f"{BASE_URL}/quotes/_{symbol_clean}.json"
            else:
                url = f"{BASE_URL}/quotes/{symbol_clean}.json"

            result = self._make_request(url)

            if "error" in result:
                return result

            # Extract the quote data from response
            quote_data = result.get("data", {})
            if not quote_data:
                return CBOEError("equity_quote", "No data found for symbol").to_dict()

            return {
                "success": True,
                "data": {
                    "symbol": quote_data.get("symbol"),
                    "current_price": quote_data.get("current_price"),
                    "open": quote_data.get("open"),
                    "high": quote_data.get("high"),
                    "low": quote_data.get("low"),
                    "close": quote_data.get("close"),
                    "volume": quote_data.get("volume"),
                    "bid": quote_data.get("bid"),
                    "ask": quote_data.get("ask"),
                    "bid_size": quote_data.get("bid_size"),
                    "ask_size": quote_data.get("ask_size"),
                    "prev_day_close": quote_data.get("prev_day_close"),
                    "price_change": quote_data.get("price_change"),
                    "price_change_percent": quote_data.get("price_change_percent"),
                    "iv30": quote_data.get("iv30"),
                    "iv30_change": quote_data.get("iv30_change"),
                    "iv30_change_percent": quote_data.get("iv30_change_percent"),
                    "last_trade_time": quote_data.get("last_trade_time"),
                    "security_type": quote_data.get("security_type"),
                    "tick": quote_data.get("tick"),
                    "mkt_data_delay": quote_data.get("mkt_data_delay")
                }
            }

        except Exception as e:
            return CBOEError("equity_quote", str(e)).to_dict()

    def get_equity_historical(self, symbol: str, interval: str = "1d",
                            start_date: Optional[str] = None,
                            end_date: Optional[str] = None,
                            use_cache: bool = True) -> Dict[str, Any]:
        """Get historical equity price data

        Args:
            symbol: Stock symbol (e.g., "AAPL", "MSFT")
            interval: Data interval ("1d" for daily, "1m" for 1-minute)
            start_date: Start date in YYYY-MM-DD format
            end_date: End date in YYYY-MM-DD format
            use_cache: Whether to use cached directory data

        Returns:
            Dict containing historical price data
        """
        try:
            symbol_clean = symbol.replace("^", "").upper()
            interval_type = "intraday" if interval == "1m" else "historical"

            # Generate URL for historical data
            if symbol_clean in TICKER_EXCEPTIONS:
                # For ticker exceptions, use intraday regardless of interval if single symbol
                if interval_type == "historical":
                    interval_type = "intraday"
                url = f"{BASE_URL}/charts/{interval_type}/_{symbol_clean}.json"
            else:
                url = f"{BASE_URL}/charts/{interval_type}/{symbol_clean}.json"

            result = self._make_request(url)

            if "error" in result:
                return result

            # Extract historical data
            data = result.get("data", {})
            if "data" not in data or not data["data"]:
                return CBOEError("equity_historical", "No historical data found").to_dict()

            historical_data = data["data"]

            # Parse and transform data
            if interval == "1d":
                # Daily data format
                df = pd.DataFrame(historical_data)
                if "date" in df.columns:
                    df["date"] = pd.to_datetime(df["date"])
            else:
                # Intraday data format
                records = []
                for item in historical_data:
                    record = {
                        "date": item.get("datetime"),
                        "open": item.get("price", {}).get("open"),
                        "high": item.get("price", {}).get("high"),
                        "low": item.get("price", {}).get("low"),
                        "close": item.get("price", {}).get("close"),
                        "volume": item.get("volume"),
                        "calls_volume": item.get("calls_volume"),
                        "puts_volume": item.get("puts_volume"),
                        "total_options_volume": item.get("total_options_volume")
                    }
                    records.append(record)
                df = pd.DataFrame(records)
                if "date" in df.columns:
                    df["date"] = pd.to_datetime(df["date"])

            # Filter by date range if provided
            if start_date:
                start_dt = pd.to_datetime(start_date)
                df = df[df["date"] >= start_dt]

            if end_date:
                end_dt = pd.to_datetime(end_date) + timedelta(days=1)
                df = df[df["date"] < end_dt]

            # Sort by date
            df = df.sort_values("date").reset_index(drop=True)

            return {
                "success": True,
                "data": {
                    "symbol": symbol,
                    "interval": interval,
                    "data": df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("equity_historical", str(e)).to_dict()

    def get_index_constituents(self, symbol: str) -> Dict[str, Any]:
        """Get constituents for European indices

        Args:
            symbol: European index symbol (e.g., "BUK100P", "BEP50P")

        Returns:
            Dict containing index constituents data
        """
        try:
            symbol_clean = symbol.upper()

            if symbol_clean not in EU_INDEX_CONSTITUENTS:
                return CBOEError("index_constituents", f"Invalid European index symbol. Supported: {', '.join(EU_INDEX_CONSTITUENTS[:10])}...").to_dict()

            url = f"{EU_BASE_URL}/constituent_quotes/{symbol_clean}.json"
            result = self._make_request(url)

            if "error" in result:
                return result

            constituents_data = result.get("data", [])
            if not constituents_data:
                return CBOEError("index_constituents", f"No constituents found for {symbol}").to_dict()

            df = pd.DataFrame(constituents_data)

            # Transform percentage fields
            if "price_change_percent" in df.columns:
                df["price_change_percent"] = df["price_change_percent"] / 100

            # Remove exchange_id column
            if "exchange_id" in df.columns:
                df = df.drop(columns=["exchange_id"])

            return {
                "success": True,
                "data": {
                    "symbol": symbol,
                    "constituents": df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("index_constituents", str(e)).to_dict()

    def get_index_historical(self, symbol: str, interval: str = "1d",
                           start_date: Optional[str] = None,
                           end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get historical index data

        Args:
            symbol: Index symbol (e.g., "SPX", "VIX", "BUK100P")
            interval: Data interval ("1d" for daily, "1m" for 1-minute)
            start_date: Start date in YYYY-MM-DD format
            end_date: End date in YYYY-MM-DD format

        Returns:
            Dict containing historical index data
        """
        try:
            symbol_clean = symbol.replace("^", "").upper()
            interval_type = "intraday" if interval == "1m" else "historical"

            # Check if European index
            is_european_index = symbol_clean in EU_INDEX_CONSTITUENTS

            if is_european_index:
                # European index URL
                base_url = f"{EU_BASE_URL}/index_history/" if interval_type == "historical" else f"{EU_BASE_URL}/intraday_chart_data/"
                url = f"{base_url}{symbol_clean}.json"
            else:
                # US index URL
                if symbol_clean in TICKER_EXCEPTIONS:
                    url = f"{BASE_URL}/charts/{interval_type}/_{symbol_clean}.json"
                else:
                    url = f"{BASE_URL}/charts/{interval_type}/{symbol_clean}.json"

            result = self._make_request(url)

            if "error" in result:
                return result

            # Extract historical data
            data = result.get("data", {})
            if "data" not in data or not data["data"]:
                return CBOEError("index_historical", "No historical data found").to_dict()

            historical_data = data["data"]

            # Parse and transform data
            if interval == "1d":
                # Daily data format
                df = pd.DataFrame(historical_data)
                if "date" in df.columns:
                    df["date"] = pd.to_datetime(df["date"])

                # Remove volume column if exists (it may be a string 0)
                if "volume" in df.columns:
                    df = df.drop(columns="volume")
            else:
                # Intraday data format
                records = []
                for item in historical_data:
                    record = {
                        "date": item.get("datetime"),
                        "open": item.get("price", {}).get("open"),
                        "high": item.get("price", {}).get("high"),
                        "low": item.get("price", {}).get("low"),
                        "close": item.get("price", {}).get("close")
                    }
                    records.append(record)
                df = pd.DataFrame(records)
                if "date" in df.columns:
                    df["date"] = pd.to_datetime(df["date"])

            # Filter by date range if provided
            if start_date:
                start_dt = pd.to_datetime(start_date)
                df = df[df["date"] >= start_dt]

            if end_date:
                end_dt = pd.to_datetime(end_date) + timedelta(days=1)
                df = df[df["date"] < end_dt]

            # Sort by date
            df = df.sort_values("date").reset_index(drop=True)

            return {
                "success": True,
                "data": {
                    "symbol": symbol,
                    "interval": interval,
                    "data": df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("index_historical", str(e)).to_dict()

    def get_index_snapshots(self, region: str = "us") -> Dict[str, Any]:
        """Get snapshots for all indices in a region

        Args:
            region: Region - "us" for US indices, "eu" for European indices

        Returns:
            Dict containing index snapshots data
        """
        try:
            region = region.lower()

            if region == "us":
                url = US_INDICES_URL
            elif region == "eu":
                url = EU_INDICES_URL
            else:
                return CBOEError("index_snapshots", f"Invalid region: {region}. Use 'us' or 'eu'").to_dict()

            result = self._make_request(url)

            if "error" in result:
                return result

            indices_data = result.get("data", [])
            if not indices_data:
                return CBOEError("index_snapshots", f"No indices data found for region: {region}").to_dict()

            df = pd.DataFrame(indices_data)

            # Transform percentage fields
            percent_cols = [
                "price_change_percent", "iv30", "iv30_change", "iv30_change_percent"
            ]
            for col in percent_cols:
                if col in df.columns:
                    df[col] = round(df[col] / 100, 6)

            # Clean data
            df = df.replace(0, None).replace("", None)
            df = df.dropna(how="all", axis=1)
            df = df.fillna("N/A").replace("N/A", None)

            # Drop unnecessary columns
            drop_cols = [
                "exchange_id", "seqno", "index", "security_type", "ask_size", "bid_size"
            ]
            for col in drop_cols:
                if col in df.columns:
                    df = df.drop(columns=col)

            return {
                "success": True,
                "data": {
                    "region": region,
                    "indices": df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("index_snapshots", str(e)).to_dict()

    def get_futures_curve(self, symbol: str = "VX_EOD", date: Optional[str] = None) -> Dict[str, Any]:
        """Get VIX futures curve data

        Args:
            symbol: VIX futures symbol ("VX_EOD" or "VX_AM")
            date: Specific date in YYYY-MM-DD format (optional)

        Returns:
            Dict containing futures curve data
        """
        try:
            symbol = symbol.upper()

            if symbol not in VIX_SYMBOLS:
                symbol = "VX_EOD"  # Default

            vx_type = "am" if symbol == "VX_AM" else "eod"

            if date:
                url = f"https://cdn.cboe.com/api/global/futures/vx_{vx_type}_curve/{date}.json"
            else:
                url = f"https://cdn.cboe.com/api/global/futures/vx_{vx_type}_curve.json"

            result = self._make_request(url)

            if "error" in result:
                return result

            futures_data = result.get("data", [])
            if not futures_data:
                return CBOEError("futures_curve", "No futures curve data found").to_dict()

            df = pd.DataFrame(futures_data)

            return {
                "success": True,
                "data": {
                    "symbol": symbol,
                    "date": date or "current",
                    "futures": df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("futures_curve", str(e)).to_dict()

    def get_options_chains(self, symbol: str) -> Dict[str, Any]:
        """Get options chains data for a symbol

        Args:
            symbol: Stock symbol (e.g., "AAPL", "MSFT")

        Returns:
            Dict containing options chains data with metadata
        """
        try:
            symbol_clean = symbol.replace("^", "").upper()

            # Determine URL pattern
            if symbol_clean in TICKER_EXCEPTIONS:
                url = f"{BASE_URL}/options/_{symbol_clean}.json"
            else:
                url = f"{BASE_URL}/options/{symbol_clean}.json"

            result = self._make_request(url)

            if "error" in result:
                return result

            data = result.get("data", {})
            if not data:
                return CBOEError("options_chains", "No options data found for symbol").to_dict()

            # Extract metadata
            metadata = {
                "symbol": data.get("symbol"),
                "security_type": data.get("security_type"),
                "bid": data.get("bid"),
                "bid_size": data.get("bid_size"),
                "ask": data.get("ask"),
                "ask_size": data.get("ask_size"),
                "open": data.get("open"),
                "high": data.get("high"),
                "low": data.get("low"),
                "close": data.get("close"),
                "volume": data.get("volume"),
                "current_price": data.get("current_price"),
                "prev_close": data.get("prev_day_close"),
                "change": data.get("price_change"),
                "change_percent": data.get("price_change_percent"),
                "iv30": data.get("iv30"),
                "iv30_change": data.get("iv30_change"),
                "iv30_change_percent": data.get("iv30_change_percent"),
                "last_trade_time": data.get("last_trade_time")
            }

            # Extract options data
            options = data.get("options", [])
            if not options:
                return CBOEError("options_chains", "No options chains found").to_dict()

            # Parse options data
            options_df = pd.DataFrame(options)

            # Parse option symbols to extract expiration, strike, and type
            def parse_option_symbol(option_symbol):
                """Parse option symbol to extract components"""
                import re
                pattern = r"^(?P<ticker>\D*)(?P<expiration>\d*)(?P<option_type>\D*)(?P<strike>\d*)$"
                match = re.match(pattern, option_symbol)
                if match:
                    ticker = match.group('ticker')
                    expiration = match.group('expiration')
                    option_type = match.group('option_type').replace('C', 'call').replace('P', 'put')
                    strike = match.group('strike').lstrip('0')
                    if strike:
                        strike = float(strike) / 1000  # Convert to actual strike price
                    return ticker, expiration, option_type, strike
                return None, None, None, None

            # Parse option symbols
            parsed_data = []
            for _, row in options_df.iterrows():
                ticker, expiration, option_type, strike = parse_option_symbol(row['option'])
                if ticker and expiration and option_type and strike:
                    # Calculate days to expiration
                    try:
                        exp_date = pd.to_datetime(expiration, format='%y%m%d')
                        dte = (exp_date - datetime.now()).days + 1
                    except:
                        dte = None

                    option_data = {
                        "contract_symbol": row['option'],
                        "underlying_symbol": ticker,
                        "expiration": expiration,
                        "strike": strike,
                        "option_type": option_type,
                        "dte": dte,
                        "last": row.get("last"),
                        "bid": row.get("bid"),
                        "ask": row.get("ask"),
                        "mid": row.get("mid"),
                        "change": row.get("change"),
                        "change_percent": row.get("percent_change") / 100 if row.get("percent_change") else None,
                        "volume": row.get("volume"),
                        "open_interest": row.get("open_interest"),
                        "implied_volatility": row.get("iv"),
                        "theoretical_price": row.get("theo"),
                        "delta": row.get("delta"),
                        "gamma": row.get("gamma"),
                        "theta": row.get("theta"),
                        "vega": row.get("vega"),
                        "prev_close": row.get("prev_day_close"),
                        "last_trade_time": row.get("last_trade_time")
                    }
                    parsed_data.append(option_data)

            if not parsed_data:
                return CBOEError("options_chains", "Failed to parse options data").to_dict()

            options_parsed_df = pd.DataFrame(parsed_data)

            return {
                "success": True,
                "data": {
                    "metadata": {k: v for k, v in metadata.items() if v is not None},
                    "options": options_parsed_df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("options_chains", str(e)).to_dict()

    def search_equities(self, query: str, is_symbol: bool = False) -> Dict[str, Any]:
        """Search for equities in CBOE directory

        Args:
            query: Search query (symbol or company name)
            is_symbol: If True, search only by symbol

        Returns:
            Dict containing search results
        """
        try:
            # Get company directory
            cache_key = "company_directory"
            symbols_df = self._get_cached_data(cache_key)

            if symbols_df is None:
                url = f"{BASE_URL}/directory/symbol_search.json"
                result = self._make_request(url)

                if "error" in result:
                    return result

                data = result.get("data", [])
                symbols_df = pd.DataFrame(data)
                self._set_cache_data(cache_key, symbols_df)

            if symbols_df.empty:
                return CBOEError("equity_search", "Company directory not available").to_dict()

            # Reset index to make symbol column available
            symbols_df = symbols_df.reset_index()

            # Search
            target = "name" if not is_symbol else "symbol"
            mask = symbols_df[target].str.contains(query, case=False, na=False)
            results_df = symbols_df[mask]

            return {
                "success": True,
                "data": {
                    "query": query,
                    "results": results_df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("equity_search", str(e)).to_dict()

    def search_indices(self, query: str, is_symbol: bool = False) -> Dict[str, Any]:
        """Search for indices in CBOE directory

        Args:
            query: Search query (symbol or index name)
            is_symbol: If True, search only by symbol

        Returns:
            Dict containing search results
        """
        try:
            # Get index directory
            cache_key = "index_directory"
            indices_df = self._get_cached_data(cache_key)

            if indices_df is None:
                url = f"{BASE_URL}/directory/index_search.json"
                result = self._make_request(url)

                if "error" in result:
                    return result

                data = result.get("data", [])
                indices_df = pd.DataFrame(data)

                # Drop source column like OpenBB
                if "source" in indices_df.columns:
                    indices_df = indices_df.drop(columns=["source"])

                self._set_cache_data(cache_key, indices_df)

            if indices_df.empty:
                return CBOEError("index_search", "Index directory not available").to_dict()

            # Search
            if is_symbol:
                mask = indices_df["index_symbol"].str.contains(query, case=False, na=False)
            else:
                mask = (
                    indices_df["name"].str.contains(query, case=False, na=False) |
                    indices_df["index_symbol"].str.contains(query, case=False, na=False) |
                    indices_df["description"].str.contains(query, case=False, na=False)
                )

            results_df = indices_df[mask]

            return {
                "success": True,
                "data": {
                    "query": query,
                    "results": results_df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("index_search", str(e)).to_dict()

    def get_available_indices(self) -> Dict[str, Any]:
        """Get list of all available indices

        Returns:
            Dict containing available indices
        """
        try:
            # Get index directory
            cache_key = "index_directory"
            indices_df = self._get_cached_data(cache_key)

            if indices_df is None:
                url = f"{BASE_URL}/directory/index_search.json"
                result = self._make_request(url)

                if "error" in result:
                    return result

                data = result.get("data", [])
                indices_df = pd.DataFrame(data)
                self._set_cache_data(cache_key, indices_df)

            if indices_df.empty:
                return CBOEError("available_indices", "Index directory not available").to_dict()

            return {
                "success": True,
                "data": {
                    "indices": indices_df.to_dict("records")
                }
            }

        except Exception as e:
            return CBOEError("available_indices", str(e)).to_dict()


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps(CBOEError("cli", "Usage: python cboe_data.py <command> [args...]").to_dict()))
        sys.exit(1)

    command = sys.argv[1]
    api = CBOEDataAPI()

    # Map commands to methods
    command_map = {
        "equity_quote": lambda: api.get_equity_quote(sys.argv[2] if len(sys.argv) > 2 else ""),
        "equity_historical": lambda: api.get_equity_historical(
            sys.argv[2] if len(sys.argv) > 2 else "",
            sys.argv[3] if len(sys.argv) > 3 else "1d",
            sys.argv[4] if len(sys.argv) > 4 else None,
            sys.argv[5] if len(sys.argv) > 5 else None
        ),
        "equity_search": lambda: api.search_equities(
            sys.argv[2] if len(sys.argv) > 2 else "",
            sys.argv[3].lower() == "true" if len(sys.argv) > 3 else False
        ),
        "index_constituents": lambda: api.get_index_constituents(sys.argv[2] if len(sys.argv) > 2 else ""),
        "index_historical": lambda: api.get_index_historical(
            sys.argv[2] if len(sys.argv) > 2 else "",
            sys.argv[3] if len(sys.argv) > 3 else "1d",
            sys.argv[4] if len(sys.argv) > 4 else None,
            sys.argv[5] if len(sys.argv) > 5 else None
        ),
        "index_search": lambda: api.search_indices(
            sys.argv[2] if len(sys.argv) > 2 else "",
            sys.argv[3].lower() == "true" if len(sys.argv) > 3 else False
        ),
        "index_snapshots": lambda: api.get_index_snapshots(sys.argv[2] if len(sys.argv) > 2 else "us"),
        "futures_curve": lambda: api.get_futures_curve(
            sys.argv[2] if len(sys.argv) > 2 else "VX_EOD",
            sys.argv[3] if len(sys.argv) > 3 else None
        ),
        "options_chains": lambda: api.get_options_chains(sys.argv[2] if len(sys.argv) > 2 else ""),
        "available_indices": lambda: api.get_available_indices()
    }

    if command not in command_map:
        print(json.dumps(CBOEError("cli", f"Unknown command: {command}").to_dict()))
        sys.exit(1)

    try:
        result = command_map[command]()
        print(json.dumps(result, indent=2, default=str))
    except Exception as e:
        print(json.dumps(CBOEError(command, str(e)).to_dict(), indent=2))
        sys.exit(1)


if __name__ == "__main__":
    main()