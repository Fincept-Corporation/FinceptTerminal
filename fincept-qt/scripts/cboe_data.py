# CBOE (Chicago Board Options Exchange) Data Wrapper
# Based on OpenBB CBOE provider - https://github.com/OpenBB-finance/OpenBB/tree/main/openbb_platform/providers/cboe

import os
import sys
import json
import requests
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Union, Any, Literal
from io import StringIO
import pandas as pd

try:
    import fincept_json
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

# CBOE API URLs
BASE_URL = "https://cdn.cboe.com/api/global/delayed_quotes"
EU_BASE_URL = "https://cdn.cboe.com/api/global/european_indices"
US_INDICES_URL = "https://cdn.cboe.com/api/global/delayed_quotes/quotes/all_us_indices.json"
EU_INDICES_URL = "https://cdn.cboe.com/api/global/european_indices/index_quotes/all-indices.json"
SYMBOL_BOOK_URL = "https://cdn.cboe.com/api/global/delayed_quotes/symbol_book/symbol-book.json"
INDEX_DIRECTORY_URL = "https://cdn.cboe.com/api/global/us_indices/definitions/all_indices.json"

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

    @staticmethod
    def _payload(result: Dict[str, Any]) -> Any:
        """The endpoint's own records from a _make_request result.

        _make_request keeps the whole response body under "data", and the Cboe CDN wraps its
        records once more ({"timestamp": ..., "data": <records>, "symbol": ...}). Reading
        result["data"] directly returned that envelope: equity_quote answered success with
        every field null, options_chains never found an option, and index_constituents /
        index_snapshots rows came back as {"timestamp", "data"} pairs. Bodies that are the
        records themselves (the index directory is a bare list) are returned as they are.
        """
        body = result.get("data")
        if isinstance(body, dict) and "data" in body:
            return body["data"]
        return body

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
            quote_data = self._payload(result)
            if not isinstance(quote_data, dict) or all(
                    quote_data.get(k) is None for k in ("current_price", "close", "bid", "ask")):
                return CBOEError("equity_quote", f"No quote data found for {symbol}").to_dict()

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

            constituents_data = self._payload(result)
            if not isinstance(constituents_data, list) or not constituents_data:
                return CBOEError("index_constituents", f"No constituents found for {symbol}").to_dict()

            df = pd.DataFrame(constituents_data)

            # price_change_percent stays in percent points, as Cboe sends it and as
            # equity_quote reports it (dividing by 100 would label a fraction "percent").

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

            indices_data = self._payload(result)
            if not isinstance(indices_data, list) or not indices_data:
                return CBOEError("index_snapshots", f"No indices data found for region: {region}").to_dict()

            df = pd.DataFrame(indices_data)

            # price_change_percent / iv30 stay in percent points, as Cboe sends them and as
            # equity_quote reports them (dividing by 100 would label a fraction "percent").

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

            futures_data = self._payload(result)
            if not isinstance(futures_data, list) or not futures_data:
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

    def get_options_chains(self, symbol: str, expiration: str = "", option_type: str = "") -> Dict[str, Any]:
        """Get options chains data for a symbol

        Args:
            symbol: Stock, ETF or index symbol (e.g., "AAPL", "SPY", "^SPX")
            expiration: "" or "all" for every expiration, "nearest" for the first one on or
                        after today, or a date YYYY-MM-DD
            option_type: "" or "both" for calls and puts, "call" or "put"

        Returns:
            Dict containing options chains data with metadata. implied_volatility is a
            fraction (0.25 = 25%); change_percent is in percent points.
        """
        try:
            import re

            symbol_clean = symbol.replace("^", "").upper()
            if not symbol_clean:
                return CBOEError("options_chains", "Symbol is required").to_dict()

            exp_filter = (expiration or "").strip().lower()
            if exp_filter == "all":
                exp_filter = ""
            elif exp_filter and exp_filter != "nearest":
                try:
                    exp_filter = datetime.strptime(exp_filter, "%Y-%m-%d").strftime("%Y-%m-%d")
                except ValueError:
                    return CBOEError("options_chains",
                                     f"Invalid expiration '{expiration}': use YYYY-MM-DD, 'nearest' or 'all'").to_dict()
            type_aliases = {"": "", "both": "", "all": "", "call": "call", "calls": "call", "c": "call",
                            "put": "put", "puts": "put", "p": "put"}
            type_filter = type_aliases.get((option_type or "").strip().lower())
            if type_filter is None:
                return CBOEError("options_chains", f"Invalid option type '{option_type}': use call, put or both").to_dict()

            # Determine URL pattern
            if symbol_clean in TICKER_EXCEPTIONS:
                url = f"{BASE_URL}/options/_{symbol_clean}.json"
            else:
                url = f"{BASE_URL}/options/{symbol_clean}.json"

            result = self._make_request(url)

            if "error" in result:
                return result

            data = self._payload(result)
            if not isinstance(data, dict) or not data:
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
            options = data.get("options") or []
            if not options:
                return CBOEError("options_chains", f"No options chains found for {symbol}").to_dict()

            # OCC contract symbol: root, YYMMDD expiration, C/P, strike x 1000 in 8 digits.
            # (Roots of adjusted contracts can contain digits, e.g. "AAPL1".)
            occ = re.compile(r"^(?P<root>.+?)(?P<exp>\d{6})(?P<cp>[CP])(?P<strike>\d{8})$")
            today = datetime.now().date()

            def number(value):
                return value if isinstance(value, (int, float)) and not isinstance(value, bool) else None

            parsed_data = []
            for opt in options:
                match = occ.match(str(opt.get("option", "")))
                if not match:
                    continue
                try:
                    exp_date = datetime.strptime(match.group("exp"), "%y%m%d").date()
                except ValueError:
                    continue
                bid, ask = number(opt.get("bid")), number(opt.get("ask"))
                parsed_data.append({
                    "contract_symbol": opt.get("option"),
                    "underlying_symbol": match.group("root"),
                    "expiration": exp_date.isoformat(),
                    "strike": int(match.group("strike")) / 1000,
                    "option_type": "call" if match.group("cp") == "C" else "put",
                    "dte": (exp_date - today).days,
                    "last": opt.get("last_trade_price"),
                    "bid": bid,
                    "bid_size": opt.get("bid_size"),
                    "ask": ask,
                    "ask_size": opt.get("ask_size"),
                    "mid": round((bid + ask) / 2, 4) if bid and ask and bid > 0 and ask > 0 else None,
                    "change": opt.get("change"),
                    "change_percent": opt.get("percent_change"),
                    "volume": opt.get("volume"),
                    "open_interest": opt.get("open_interest"),
                    "implied_volatility": opt.get("iv"),
                    "theoretical_price": opt.get("theo"),
                    "delta": opt.get("delta"),
                    "gamma": opt.get("gamma"),
                    "theta": opt.get("theta"),
                    "vega": opt.get("vega"),
                    "rho": opt.get("rho"),
                    "prev_close": opt.get("prev_day_close"),
                    "last_trade_time": opt.get("last_trade_time")
                })

            if not parsed_data:
                return CBOEError("options_chains", "Failed to parse options data").to_dict()

            expirations = sorted({o["expiration"] for o in parsed_data})
            if exp_filter == "nearest":
                upcoming = [e for e in expirations if e >= today.isoformat()]
                if not upcoming:
                    return CBOEError("options_chains", f"No {symbol_clean} expiration on or after today").to_dict()
                selected = {upcoming[0]}
            elif exp_filter:
                if exp_filter not in expirations:
                    shown = ", ".join(expirations[:12]) + (" ..." if len(expirations) > 12 else "")
                    return CBOEError("options_chains",
                                     f"No {symbol_clean} options expire on {exp_filter}. Available: {shown}").to_dict()
                selected = {exp_filter}
            else:
                selected = set(expirations)
            rows = [o for o in parsed_data
                    if o["expiration"] in selected and (not type_filter or o["option_type"] == type_filter)]
            rows.sort(key=lambda o: (o["expiration"], o["strike"], o["option_type"]))

            return {
                "success": True,
                "data": {
                    "metadata": {k: v for k, v in metadata.items() if v is not None},
                    "expirations": expirations,
                    "expiration_filter": exp_filter or "all",
                    "option_type_filter": type_filter or "both",
                    "count": len(rows),
                    "units": {"implied_volatility": "fraction (0.25 = 25%)", "change_percent": "percent points"},
                    "options": rows
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
                # directory/symbol_search.json was retired (HTTP 403); the symbol book lists
                # every Cboe-quoted symbol as {"name": <symbol>, "company_name": <name>}.
                result = self._make_request(SYMBOL_BOOK_URL)

                if "error" in result:
                    return result

                data = self._payload(result)
                symbols_df = pd.DataFrame(data if isinstance(data, list) else [])
                symbols_df = symbols_df.rename(columns={"name": "symbol", "company_name": "name"})
                self._set_cache_data(cache_key, symbols_df)

            if symbols_df.empty:
                return CBOEError("equity_search", "Company directory not available").to_dict()

            # Search (the query is literal text, not a regular expression)
            target = "name" if not is_symbol else "symbol"
            mask = symbols_df[target].str.contains(query, case=False, na=False, regex=False)
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
                # directory/index_search.json was retired (HTTP 403); the index definitions
                # file lists every Cboe index (index_symbol, name, description, ...).
                result = self._make_request(INDEX_DIRECTORY_URL)

                if "error" in result:
                    return result

                data = self._payload(result)
                indices_df = pd.DataFrame(data if isinstance(data, list) else [])

                # Drop source column like OpenBB
                if "source" in indices_df.columns:
                    indices_df = indices_df.drop(columns=["source"])

                self._set_cache_data(cache_key, indices_df)

            if indices_df.empty:
                return CBOEError("index_search", "Index directory not available").to_dict()

            # Search (the query is literal text, not a regular expression)
            if is_symbol:
                mask = indices_df["index_symbol"].str.contains(query, case=False, na=False, regex=False)
            else:
                mask = (
                    indices_df["name"].str.contains(query, case=False, na=False, regex=False) |
                    indices_df["index_symbol"].str.contains(query, case=False, na=False, regex=False) |
                    indices_df["description"].str.contains(query, case=False, na=False, regex=False)
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
                # directory/index_search.json was retired (HTTP 403) — see search_indices.
                result = self._make_request(INDEX_DIRECTORY_URL)

                if "error" in result:
                    return result

                data = self._payload(result)
                indices_df = pd.DataFrame(data if isinstance(data, list) else [])
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


def main(args=None):
    
    if args is None:
        args = sys.argv[1:]
    """Main function for CLI interface"""
    if len(args) + 1 < 2:
        print(json.dumps(CBOEError("cli", "Usage: python cboe_data.py <command> [args...]").to_dict()))
        sys.exit(1)

    command = args[0]
    api = CBOEDataAPI()

    # Map commands to methods
    command_map = {
        "equity_quote": lambda: api.get_equity_quote(args[1] if len(args) + 1 > 2 else ""),
        "equity_historical": lambda: api.get_equity_historical(
            args[1] if len(args) + 1 > 2 else "",
            args[2] if len(args) + 1 > 3 else "1d",
            args[3] if len(args) + 1 > 4 else None,
            args[4] if len(args) + 1 > 5 else None
        ),
        "equity_search": lambda: api.search_equities(
            args[1] if len(args) + 1 > 2 else "",
            args[2].lower() == "true" if len(args) + 1 > 3 else False
        ),
        "index_constituents": lambda: api.get_index_constituents(args[1] if len(args) + 1 > 2 else ""),
        "index_historical": lambda: api.get_index_historical(
            args[1] if len(args) + 1 > 2 else "",
            args[2] if len(args) + 1 > 3 else "1d",
            args[3] if len(args) + 1 > 4 else None,
            args[4] if len(args) + 1 > 5 else None
        ),
        "index_search": lambda: api.search_indices(
            args[1] if len(args) + 1 > 2 else "",
            args[2].lower() == "true" if len(args) + 1 > 3 else False
        ),
        "index_snapshots": lambda: api.get_index_snapshots(args[1] if len(args) + 1 > 2 else "us"),
        "futures_curve": lambda: api.get_futures_curve(
            args[1] if len(args) + 1 > 2 else "VX_EOD",
            args[2] if len(args) + 1 > 3 else None
        ),
        "options_chains": lambda: api.get_options_chains(
            args[1] if len(args) + 1 > 2 else "",
            args[2] if len(args) + 1 > 3 else "",
            args[3] if len(args) + 1 > 4 else ""
        ),
        "available_indices": lambda: api.get_available_indices()
    }

    if command not in command_map:
        print(json.dumps(CBOEError("cli", f"Unknown command: {command}").to_dict()))
        sys.exit(1)

    try:
        result = command_map[command]()
        # fincept_json: a missing cell (pandas NaN) prints null instead of the bare NaN
        # token, which is not JSON and makes the terminal discard the whole payload.
        print(fincept_json.dumps(result, indent=2, default=str, ensure_ascii=True))
    except Exception as e:
        print(json.dumps(CBOEError(command, str(e)).to_dict(), indent=2))
        sys.exit(1)


if __name__ == "__main__":
    main()