"""
Alternative.me Data Fetcher
Alternative.me: Fear & Greed Index, crypto fear gauge,
global market sentiment (no API key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.alternative.me"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
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


def get_fear_greed_index(limit: int = 10, date_format: str = "us") -> Any:
    params = {"limit": limit, "date_format": date_format}
    data = _make_request("fng/", params)
    if isinstance(data, dict) and "data" in data:
        result = data["data"]
        for item in result:
            val = int(item.get("value", 0))
            if val <= 24:
                item["sentiment"] = "Extreme Fear"
            elif val <= 44:
                item["sentiment"] = "Fear"
            elif val <= 55:
                item["sentiment"] = "Neutral"
            elif val <= 75:
                item["sentiment"] = "Greed"
            else:
                item["sentiment"] = "Extreme Greed"
        return {"count": len(result), "index": "Fear & Greed", "data": result}
    return data


def get_crypto_fear_greed(limit: int = 30) -> Any:
    params = {"limit": limit}
    data = _make_request("fng/", params)
    if isinstance(data, dict) and "data" in data:
        items = data["data"]
        values = [int(i.get("value", 0)) for i in items if i.get("value")]
        avg = sum(values) / len(values) if values else 0
        return {
            "current": items[0] if items else {},
            "30_day_average": round(avg, 1),
            "history": items,
            "metadata": data.get("metadata", {})
        }
    return data


def get_global_crypto_data() -> Any:
    return _make_request("v2/global/")


def get_portfolio_return(symbols: str = "BTC,ETH", amount: float = 1000.0,
                          currency: str = "USD") -> Any:
    syms = [s.strip() for s in symbols.split(',')]
    results = {}
    for sym in syms:
        data = _make_request(f"v2/ticker/{sym}/", {"convert": currency})
        if isinstance(data, dict) and not data.get("error"):
            results[sym] = data
    return {
        "symbols": syms,
        "amount": amount,
        "currency": currency,
        "prices": results
    }


def get_currencies() -> Any:
    data = _make_request("v2/listings/", {"start": 1, "limit": 100})
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "fear_greed":
        limit = int(args[1]) if len(args) > 1 else 10
        date_format = args[2] if len(args) > 2 else "us"
        result = get_fear_greed_index(limit, date_format)
    elif command == "crypto_fear":
        limit = int(args[1]) if len(args) > 1 else 30
        result = get_crypto_fear_greed(limit)
    elif command == "global":
        result = get_global_crypto_data()
    elif command == "portfolio":
        symbols = args[1] if len(args) > 1 else "BTC,ETH"
        amount = float(args[2]) if len(args) > 2 else 1000.0
        currency = args[3] if len(args) > 3 else "USD"
        result = get_portfolio_return(symbols, amount, currency)
    elif command == "currencies":
        result = get_currencies()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
