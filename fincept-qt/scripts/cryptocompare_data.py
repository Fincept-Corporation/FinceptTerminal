"""
CryptoCompare Data Fetcher
OHLCV historical for 5700+ coins, social stats, news, exchange volume.
Requires a free CoinDesk Data / CryptoCompare API key (CRYPTOCOMPARE_API_KEY): since the move to
CoinDesk Data, min-api.cryptocompare.com answers every keyless call with HTTP 401 "API key required".
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('CRYPTOCOMPARE_API_KEY', '')
BASE_URL = "https://min-api.cryptocompare.com/data"
SERVICE = "CryptoCompare (CoinDesk Data)"
KEY_ENV = "CRYPTOCOMPARE_API_KEY"

session = fc.http_session()

if API_KEY:
    session.headers.update({"authorization": f"Apikey {API_KEY}"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://developers.coindesk.com/", KEY_ENV, "a free API key")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=(10, 30))
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    if isinstance(data, dict) and isinstance(data.get("Err"), dict) and data["Err"].get("message"):
        return {"error": f"{SERVICE}: {fc.scrub(data['Err']['message'], API_KEY)}"}
    if not isinstance(data, dict):
        return {"error": f"{SERVICE} returned an unexpected response"}
    return data


def get_price(fsym: str, tsyms: str = "USD,BTC,ETH") -> Any:
    """Get current price of a coin in multiple currencies."""
    params = {"fsym": fsym, "tsyms": tsyms}
    data = _make_request("price", params=params)
    if "error" in data:
        return data
    if data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    return {"symbol": fsym, "prices": data}


def get_historical_daily(fsym: str, tsym: str = "USD", limit: int = 365) -> Any:
    """Get daily OHLCV historical data for a coin."""
    params = {"fsym": fsym, "tsym": tsym, "limit": min(limit, 2000)}
    data = _make_request("v2/histoday", params=params)
    if "error" in data:
        return data
    if data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    ohlcv = data.get("Data", {}).get("Data", [])
    return {"symbol": fsym, "quote": tsym, "interval": "1d", "ohlcv": ohlcv, "count": len(ohlcv)}


def get_historical_hourly(fsym: str, tsym: str = "USD", limit: int = 168) -> Any:
    """Get hourly OHLCV historical data for a coin."""
    params = {"fsym": fsym, "tsym": tsym, "limit": min(limit, 2000)}
    data = _make_request("v2/histohour", params=params)
    if "error" in data:
        return data
    if data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    ohlcv = data.get("Data", {}).get("Data", [])
    return {"symbol": fsym, "quote": tsym, "interval": "1h", "ohlcv": ohlcv, "count": len(ohlcv)}


def get_top_by_volume(tsym: str = "USD", limit: int = 50) -> Any:
    """Get top coins by 24h volume."""
    params = {"tsym": tsym, "limit": min(limit, 100)}
    data = _make_request("top/totalvolfull", params=params)
    if "error" in data:
        return data
    if data.get("Response") == "Error":
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
    if "error" in data:
        return data
    if data.get("Type") == 100:
        articles = data.get("Data", [])
        return {"articles": articles, "count": len(articles), "categories": categories}
    return {"error": f"{SERVICE}: {data.get('Message') or 'unexpected news response'}"}


def get_exchange_volume(tsym: str = "USD", fsym: str = "BTC") -> Any:
    """Get top exchanges by 24h volume for the fsym/tsym pair (the endpoint requires both)."""
    params = {"fsym": fsym.upper(), "tsym": tsym, "limit": 50}
    data = _make_request("top/exchanges/full", params=params)
    if "error" in data:
        return data
    if data.get("Response") == "Error":
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
    if "error" in data:
        return data
    if data.get("Response") == "Error":
        return {"error": data.get("Message", "Unknown error")}
    coins = data.get("Data", {})
    return {"coins": list(coins.values())[:500], "total_count": len(coins)}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(fincept_json.dumps({"error": "No command provided. Available: price, daily, hourly, top_volume, news, exchange_volume, coin_list"}))
        return

    command = args[0]

    try:
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
            fsym = args[2] if len(args) > 2 else "BTC"
            result = get_exchange_volume(tsym, fsym)
        elif command == "coin_list":
            result = get_coin_list()
        else:
            result = {"error": f"Unknown command: {command}. Available: price, daily, hourly, top_volume, news, exchange_volume, coin_list"}
    except ValueError:
        result = {"error": "limit must be a whole number"}

    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
