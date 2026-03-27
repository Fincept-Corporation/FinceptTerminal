"""
Messari Data Fetcher
Crypto asset metrics, profiles, market data, on-chain fundamentals.
Free key optional via MESSARI_API_KEY.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('MESSARI_API_KEY', '')
BASE_URL = "https://data.messari.io/api/v1"
BASE_URL_V2 = "https://data.messari.io/api/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if API_KEY:
    session.headers.update({"x-messari-api-key": API_KEY})


def _make_request(endpoint: str, params: Dict = None, v2: bool = False) -> Any:
    """Make HTTP request with error handling."""
    base = BASE_URL_V2 if v2 else BASE_URL
    url = f"{base}/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def get_all_assets(limit: int = 50, page: int = 1) -> Any:
    """Get paginated list of all tracked assets with basic metrics."""
    params = {
        "limit": min(limit, 500),
        "page": page,
        "fields": "id,slug,symbol,name,metrics/market_data/price_usd,metrics/market_data/volume_last_24_hours,metrics/marketcap/current_marketcap_usd,metrics/market_data/percent_change_usd_last_24_hours"
    }
    data = _make_request("assets", params=params)
    if isinstance(data, dict) and "data" in data:
        assets = data["data"]
        return {"assets": assets, "count": len(assets), "page": page}
    return data


def get_asset(asset_key: str) -> Any:
    """Get profile and metadata for a specific asset (slug, symbol, or ID)."""
    params = {
        "fields": "id,slug,symbol,name,profile/general/overview/project_details,profile/general/overview/official_links,profile/general/relevant_resources,profile/economics/token/token_type,profile/economics/token/block_reward,profile/technology/overview/technology_details"
    }
    data = _make_request(f"assets/{asset_key}/profile", params=params)
    if isinstance(data, dict) and "data" in data:
        return data["data"]
    return data


def get_asset_metrics(asset_key: str) -> Any:
    """Get comprehensive on-chain and market metrics for an asset."""
    params = {
        "fields": "id,slug,symbol,metrics/market_data,metrics/marketcap,metrics/supply,metrics/blockchain_stats_24_hours,metrics/all_time_high,metrics/cycle_low,metrics/token_sale_stats,metrics/mining_stats,metrics/developer_activity,metrics/roi_data"
    }
    data = _make_request(f"assets/{asset_key}/metrics", params=params)
    if isinstance(data, dict) and "data" in data:
        return data["data"]
    return data


def get_asset_market_data(asset_key: str) -> Any:
    """Get current market data (price, volume, market cap) for an asset."""
    params = {
        "fields": "id,slug,symbol,metrics/market_data"
    }
    data = _make_request(f"assets/{asset_key}/metrics", params=params)
    if isinstance(data, dict) and "data" in data:
        asset = data["data"]
        return {
            "id": asset.get("id"),
            "slug": asset.get("slug"),
            "symbol": asset.get("symbol"),
            "market_data": asset.get("metrics", {}).get("market_data", {}),
        }
    return data


def get_asset_timeseries(asset_key: str, metric: str = "price", interval: str = "1d", after: str = None, before: str = None) -> Any:
    """Get historical timeseries data for an asset metric.
    Metrics: price, volume, marketcap, returns, price-btc, etc.
    Intervals: 1m, 5m, 15m, 30m, 1h, 1d, 1w.
    """
    params = {"interval": interval}
    if after:
        params["after"] = after
    if before:
        params["before"] = before
    data = _make_request(f"assets/{asset_key}/metrics/{metric}/time-series", params=params)
    if isinstance(data, dict) and "data" in data:
        ts = data["data"]
        values = ts.get("values", [])
        return {
            "asset": asset_key,
            "metric": metric,
            "interval": interval,
            "parameters": ts.get("parameters", {}),
            "values": values,
            "count": len(values),
        }
    return data


def get_news(asset_key: str = None) -> Any:
    """Get latest crypto news, optionally filtered by asset."""
    if asset_key:
        endpoint = f"news/{asset_key}"
    else:
        endpoint = "news"
    params = {"limit": 30}
    data = _make_request(endpoint, params=params)
    if isinstance(data, dict) and "data" in data:
        articles = data["data"]
        return {"articles": articles, "count": len(articles), "asset": asset_key}
    return data


def get_markets(asset_key: str) -> Any:
    """Get exchange markets for a specific asset with prices and volumes."""
    params = {"limit": 50}
    data = _make_request(f"assets/{asset_key}/markets", params=params)
    if isinstance(data, dict) and "data" in data:
        markets = data["data"]
        return {"asset": asset_key, "markets": markets, "count": len(markets)}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: assets, asset, metrics, market_data, timeseries, news, markets"}))
        return

    command = args[0]

    if command == "assets":
        limit = int(args[1]) if len(args) > 1 else 50
        page = int(args[2]) if len(args) > 2 else 1
        result = get_all_assets(limit, page)
    elif command == "asset":
        if len(args) < 2:
            result = {"error": "Usage: asset <asset_key> (e.g. bitcoin, ethereum, btc)"}
        else:
            result = get_asset(args[1])
    elif command == "metrics":
        if len(args) < 2:
            result = {"error": "Usage: metrics <asset_key>"}
        else:
            result = get_asset_metrics(args[1])
    elif command == "market_data":
        if len(args) < 2:
            result = {"error": "Usage: market_data <asset_key>"}
        else:
            result = get_asset_market_data(args[1])
    elif command == "timeseries":
        if len(args) < 2:
            result = {"error": "Usage: timeseries <asset_key> [metric] [interval] [after] [before]"}
        else:
            metric = args[2] if len(args) > 2 else "price"
            interval = args[3] if len(args) > 3 else "1d"
            after = args[4] if len(args) > 4 else None
            before = args[5] if len(args) > 5 else None
            result = get_asset_timeseries(args[1], metric, interval, after, before)
    elif command == "news":
        asset_key = args[1] if len(args) > 1 else None
        result = get_news(asset_key)
    elif command == "markets":
        if len(args) < 2:
            result = {"error": "Usage: markets <asset_key>"}
        else:
            result = get_markets(args[1])
    else:
        result = {"error": f"Unknown command: {command}. Available: assets, asset, metrics, market_data, timeseries, news, markets"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
