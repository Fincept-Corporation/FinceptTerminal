"""
CryptoCompare Data Fetcher
OHLCV historical for 5700+ coins, social stats, news, exchange volume.
Free key optional via CRYPTOCOMPARE_API_KEY.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CRYPTOCOMPARE_API_KEY', '')
BASE_URL = "https://min-api.cryptocompare.com/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if API_KEY:
    session.headers.update({"authorization": f"Apikey {API_KEY}"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_price(fsym: str, tsyms: str = "USD,BTC,ETH") -> Any:
    """Get current price of a coin in multiple currencies."""
    params = {"fsym": fsym, "tsyms": tsyms}
    data = _make_request("price", params=params)
    if "Response" in data and data["Response"] == "Error":
        return {"error": data.get("Message", "Unknown error")}
    return {"symbol": fsym, "prices": data}


def get_historical_daily(fsym: str, tsym: str = "USD", limit: int = 365) -> Any:
    """Get daily OHLCV historical data for a coin."""
    params = {"fsym": fsym, "tsym": tsym, "limit": min(limit, 2000)}
    data = _make_request("v2/histoday", params=params)
    if isinstance(data, dict) and data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    ohlcv = data.get("Data", {}).get("Data", [])
    return {"symbol": fsym, "quote": tsym, "interval": "1d", "ohlcv": ohlcv, "count": len(ohlcv)}


def get_historical_hourly(fsym: str, tsym: str = "USD", limit: int = 168) -> Any:
    """Get hourly OHLCV historical data for a coin."""
    params = {"fsym": fsym, "tsym": tsym, "limit": min(limit, 2000)}
    data = _make_request("v2/histohour", params=params)
    if isinstance(data, dict) and data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    ohlcv = data.get("Data", {}).get("Data", [])
    return {"symbol": fsym, "quote": tsym, "interval": "1h", "ohlcv": ohlcv, "count": len(ohlcv)}


def get_top_by_volume(tsym: str = "USD", limit: int = 50) -> Any:
    """Get top coins by 24h volume."""
    params = {"tsym": tsym, "limit": min(limit, 100)}
    data = _make_request("top/totalvolfull", params=params)
    if isinstance(data, dict) and data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    coins = data.get("Data", [])
    result = []
    for item in coins:
        coin_info = item.get("CoinInfo", {})
        raw = item.get("RAW", {}).get(tsym, {})
        display = item.get("DISPLAY", {}).get(tsym, {})
        result.append({
            "symbol": coin_info.get("Name"),
            "full_name": coin_info.get("FullName"),
            "price": raw.get("PRICE"),
            "volume_24h": raw.get("TOTALVOLUME24H"),
            "market_cap": raw.get("MKTCAP"),
            "change_24h_pct": raw.get("CHANGEPCT24HOUR"),
            "display_price": display.get("PRICE"),
        })
    return {"quote": tsym, "coins": result, "count": len(result)}


def get_news(categories: str = "", lang: str = "EN") -> Any:
    """Get latest crypto news articles, optionally filtered by categories."""
    params = {"lang": lang}
    if categories:
        params["categories"] = categories
    data = _make_request("v2/news/", params=params)
    if isinstance(data, dict) and data.get("Type") == 100:
        articles = data.get("Data", [])
        return {"articles": articles, "count": len(articles), "categories": categories}
    return data


def get_exchange_volume(tsym: str = "USD") -> Any:
    """Get top exchanges by volume with market share."""
    params = {"tsym": tsym, "limit": 50}
    data = _make_request("top/exchanges/full", params=params)
    if isinstance(data, dict) and data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    exchanges = data.get("Data", {}).get("Exchanges", [])
    result = []
    for ex in exchanges:
        result.append({
            "exchange": ex.get("MARKET"),
            "from_symbol": ex.get("FROMSYMBOL"),
            "to_symbol": ex.get("TOSYMBOL"),
            "price": ex.get("PRICE"),
            "volume_24h": ex.get("VOLUME24HOUR"),
            "volume_24h_to": ex.get("VOLUME24HOURTO"),
        })
    return {"exchanges": result, "count": len(result)}


def get_coin_list() -> Any:
    """Get full list of coins with metadata."""
    data = _make_request("all/coinlist?summary=true")
    if isinstance(data, dict) and data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    coins = data.get("Data", {})
    return {"coins": list(coins.values())[:500], "total_count": len(coins)}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: price, daily, hourly, top_volume, news, exchange_volume, coin_list"}))
        return

    command = args[0]

    if command == "price":
        if len(args) < 2:
            result = {"error": "Usage: price <symbol> [quote_currencies]"}
        else:
            tsyms = args[2] if len(args) > 2 else "USD,BTC,ETH"
            result = get_price(args[1], tsyms)
    elif command == "daily":
        if len(args) < 2:
            result = {"error": "Usage: daily <symbol> [quote] [limit]"}
        else:
            tsym = args[2] if len(args) > 2 else "USD"
            limit = int(args[3]) if len(args) > 3 else 365
            result = get_historical_daily(args[1], tsym, limit)
    elif command == "hourly":
        if len(args) < 2:
            result = {"error": "Usage: hourly <symbol> [quote] [limit]"}
        else:
            tsym = args[2] if len(args) > 2 else "USD"
            limit = int(args[3]) if len(args) > 3 else 168
            result = get_historical_hourly(args[1], tsym, limit)
    elif command == "top_volume":
        tsym = args[1] if len(args) > 1 else "USD"
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_top_by_volume(tsym, limit)
    elif command == "news":
        categories = args[1] if len(args) > 1 else ""
        result = get_news(categories)
    elif command == "exchange_volume":
        tsym = args[1] if len(args) > 1 else "USD"
        result = get_exchange_volume(tsym)
    elif command == "coin_list":
        result = get_coin_list()
    else:
        result = {"error": f"Unknown command: {command}. Available: price, daily, hourly, top_volume, news, exchange_volume, coin_list"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
