"""
Messari Data Fetcher
Crypto asset directory (sector, category, tags, rank) and the metrics catalogue keyless; asset details, market data, time series and news with a Messari API key (MESSARI_API_KEY).
"""
import sys
import json
import os
import requests
from typing import Dict, Any

API_KEY = os.environ.get('MESSARI_API_KEY', '')
# data.messari.io/api/v1 was retired; the current API lives at api.messari.io.
BASE_URL = "https://api.messari.io"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)", "Accept": "application/json"})
if API_KEY:
    session.headers.update({"X-Messari-API-Key": API_KEY})

KEY_MSG = ("This Messari endpoint requires an API key: create one at https://messari.io/api and save it as "
           "MESSARI_API_KEY in Settings > Credentials (the asset list and metrics catalogue work without one)")


def _make_request(endpoint: str, params: Dict = None, needs_key: bool = False) -> Any:
    """Make HTTP request with error handling."""
    if needs_key and not API_KEY:
        return {"error": KEY_MSG}
    url = f"{BASE_URL}/{endpoint}"
    try:
        response = session.get(url, params=params, timeout=30)
        if response.status_code == 402:
            return {"error": KEY_MSG if not API_KEY else "Messari: this endpoint is not included in your plan (HTTP 402)"}
        if response.status_code in (401, 403):
            return {"error": "Messari rejected the API key (MESSARI_API_KEY) - check it in Settings > Credentials"}
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if isinstance(data, dict) and data.get("error"):
        return {"error": f"Messari: {data['error']}"}
    return data


def get_all_assets(limit: int = 50, page: int = 1) -> Any:
    """Asset directory: name, slug, symbol, sector, category, tags and rank (keyless)."""
    data = _make_request("metrics/v2/assets", params={"limit": min(limit, 500), "page": page})
    if isinstance(data, dict) and "data" in data:
        keep = ("id", "name", "slug", "symbol", "rank", "category", "sector", "sectorV2", "subSectorV2", "tags")
        assets = [{k: a.get(k) for k in keep} for a in data["data"]]
        return {"assets": assets, "count": len(assets), "page": page}
    return data


def get_asset(asset_key: str) -> Any:
    """Asset details (market data, supply, ATH, ROI, qualitative info) - needs a key."""
    data = _make_request("metrics/v2/assets/details", params={"assets": asset_key}, needs_key=True)
    if isinstance(data, dict) and "data" in data:
        d = data["data"]
        return d[0] if isinstance(d, list) and len(d) == 1 else d
    return data


def get_asset_metrics(asset_key: str) -> Any:
    """Asset details plus ROI and all-time-high snapshots - needs a key."""
    details = get_asset(asset_key)
    if isinstance(details, dict) and "error" in details:
        return details
    roi = _make_request("metrics/v2/assets/roi", params={"assets": asset_key}, needs_key=True)
    ath = _make_request("metrics/v2/assets/ath", params={"assets": asset_key}, needs_key=True)
    return {"details": details, "roi": roi.get("data", roi), "ath": ath.get("data", ath)}


def get_asset_market_data(asset_key: str) -> Any:
    """Current market data for an asset - needs a key."""
    details = get_asset(asset_key)
    if isinstance(details, dict) and "error" not in details:
        md = details.get("marketData") or details.get("market_data")
        if md is not None:
            return {"id": details.get("id"), "slug": details.get("slug"), "symbol": details.get("symbol"),
                    "market_data": md}
    return details


def get_metrics_catalog() -> Any:
    """Available time-series datasets/metrics and their granularities (keyless)."""
    data = _make_request("metrics/v2/assets/metrics")
    if isinstance(data, dict) and "data" in data:
        ds = data["data"].get("datasets", []) if isinstance(data["data"], dict) else data["data"]
        return {"datasets": [{"slug": d.get("slug"), "granularities": d.get("granularities"),
                              "metrics": [m.get("slug") for m in d.get("metrics", [])]} for d in ds],
                "count": len(ds)}
    return data


def get_asset_timeseries(asset_key: str, metric: str = "price", interval: str = "1d", after: str = None,
                         before: str = None) -> Any:
    """Historical time series for an asset dataset (see `catalog` for dataset slugs) - needs a key.
    interval: 5m, 15m, 1h, 1d, 1w (dataset dependent); after/before: ISO dates."""
    params = {}
    if after:
        params["start"] = after
    if before:
        params["end"] = before
    data = _make_request(f"metrics/v2/assets/{asset_key}/metrics/{metric}/time-series/{interval}", params=params,
                         needs_key=True)
    if isinstance(data, dict) and "data" in data:
        return {"asset": asset_key, "metric": metric, "interval": interval, "data": data["data"]}
    return data


def get_news(asset_key: str = None) -> Any:
    """Latest crypto news, optionally for one asset - needs a key."""
    params = {"limit": 30}
    if asset_key:
        params["assetSlugs"] = asset_key
    data = _make_request("news/v1/news/feed", params=params, needs_key=True)
    if isinstance(data, dict) and "data" in data:
        articles = data["data"]
        return {"articles": articles, "count": len(articles) if isinstance(articles, list) else None,
                "asset": asset_key}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: assets, asset, metrics, market_data, catalog, timeseries, news"}))
        return

    command = args[0]

    if command == "assets":
        limit = int(args[1]) if len(args) > 1 else 50
        page = int(args[2]) if len(args) > 2 else 1
        result = get_all_assets(limit, page)
    elif command == "asset":
        if len(args) < 2:
            result = {"error": "Usage: asset <asset_key> (e.g. bitcoin, ethereum)"}
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
    elif command == "catalog":
        result = get_metrics_catalog()
    elif command == "timeseries":
        if len(args) < 2:
            result = {"error": "Usage: timeseries <asset_key> [dataset] [interval] [start] [end]"}
        else:
            metric = args[2] if len(args) > 2 else "price"
            interval = args[3] if len(args) > 3 else "1d"
            after = args[4] if len(args) > 4 else None
            before = args[5] if len(args) > 5 else None
            result = get_asset_timeseries(args[1], metric, interval, after, before)
    elif command == "news":
        asset_key = args[1] if len(args) > 1 else None
        result = get_news(asset_key)
    else:
        result = {"error": f"Unknown command: {command}. Available: assets, asset, metrics, market_data, catalog, timeseries, news"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
