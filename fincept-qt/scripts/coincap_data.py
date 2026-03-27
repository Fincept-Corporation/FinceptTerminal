"""
CoinCap v2 Data Fetcher
1000+ crypto prices, market caps, volumes, exchange data, candles.
No API key required (rate limited). Optional key via COINCAP_API_KEY.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('COINCAP_API_KEY', '')
BASE_URL = "https://api.coincap.io/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if API_KEY:
    session.headers.update({"Authorization": f"Bearer {API_KEY}"})


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


def get_assets(limit: int = 100, offset: int = 0) -> Any:
    """Get paginated list of crypto assets with market data."""
    params = {"limit": limit, "offset": offset}
    data = _make_request("assets", params=params)
    if isinstance(data, dict) and "data" in data:
        return {"assets": data["data"], "timestamp": data.get("timestamp"), "count": len(data["data"])}
    return data


def get_asset(asset_id: str) -> Any:
    """Get detailed data for a specific asset by ID (e.g. bitcoin, ethereum)."""
    data = _make_request(f"assets/{asset_id}")
    if isinstance(data, dict) and "data" in data:
        return data["data"]
    return data


def get_asset_history(asset_id: str, interval: str = "d1") -> Any:
    """Get price history for an asset. Intervals: m1,m5,m15,m30,h1,h2,h6,h12,d1."""
    params = {"interval": interval}
    data = _make_request(f"assets/{asset_id}/history", params=params)
    if isinstance(data, dict) and "data" in data:
        history = data["data"]
        return {"asset_id": asset_id, "interval": interval, "history": history, "count": len(history)}
    return data


def get_markets(asset_id: str) -> Any:
    """Get exchange markets for a specific asset."""
    params = {"limit": 100}
    data = _make_request(f"assets/{asset_id}/markets", params=params)
    if isinstance(data, dict) and "data" in data:
        return {"asset_id": asset_id, "markets": data["data"], "count": len(data["data"])}
    return data


def get_exchanges() -> Any:
    """Get list of exchanges with volume data."""
    params = {"limit": 100}
    data = _make_request("exchanges", params=params)
    if isinstance(data, dict) and "data" in data:
        return {"exchanges": data["data"], "count": len(data["data"])}
    return data


def get_rates() -> Any:
    """Get current conversion rates against USD for all supported currencies."""
    data = _make_request("rates")
    if isinstance(data, dict) and "data" in data:
        return {"rates": data["data"], "timestamp": data.get("timestamp"), "count": len(data["data"])}
    return data


def get_candles(exchange_id: str, base_id: str, quote_id: str, interval: str = "d1") -> Any:
    """Get OHLCV candles for a trading pair on an exchange."""
    params = {
        "exchange": exchange_id,
        "interval": interval,
        "baseId": base_id,
        "quoteId": quote_id,
        "limit": 200
    }
    data = _make_request("candles", params=params)
    if isinstance(data, dict) and "data" in data:
        return {"exchange": exchange_id, "base": base_id, "quote": quote_id, "interval": interval, "candles": data["data"]}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: assets, asset, history, markets, exchanges, rates, candles"}))
        return

    command = args[0]

    if command == "assets":
        limit = int(args[1]) if len(args) > 1 else 100
        offset = int(args[2]) if len(args) > 2 else 0
        result = get_assets(limit, offset)
    elif command == "asset":
        if len(args) < 2:
            result = {"error": "Usage: asset <asset_id> (e.g. bitcoin, ethereum)"}
        else:
            result = get_asset(args[1])
    elif command == "history":
        if len(args) < 2:
            result = {"error": "Usage: history <asset_id> [interval] (intervals: m1,m5,m15,m30,h1,h2,h6,h12,d1)"}
        else:
            interval = args[2] if len(args) > 2 else "d1"
            result = get_asset_history(args[1], interval)
    elif command == "markets":
        if len(args) < 2:
            result = {"error": "Usage: markets <asset_id>"}
        else:
            result = get_markets(args[1])
    elif command == "exchanges":
        result = get_exchanges()
    elif command == "rates":
        result = get_rates()
    elif command == "candles":
        if len(args) < 4:
            result = {"error": "Usage: candles <exchange_id> <base_id> <quote_id> [interval]"}
        else:
            interval = args[4] if len(args) > 4 else "d1"
            result = get_candles(args[1], args[2], args[3], interval)
    else:
        result = {"error": f"Unknown command: {command}. Available: assets, asset, history, markets, exchanges, rates, candles"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
