"""
Glassnode Data Fetcher
Bitcoin/Ethereum on-chain metrics — active addresses, tx count, hash rate, NVT ratio.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GLASSNODE_API_KEY', '')
BASE_URL = "https://api.glassnode.com/v1/metrics"

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

def get_active_addresses(asset: str = "BTC", since: int = None, until: int = None, interval: str = "24h") -> Any:
    params = {"a": asset, "i": interval, "api_key": API_KEY}
    if since:
        params["s"] = since
    if until:
        params["u"] = until
    return _make_request("addresses/active_count", params)

def get_transaction_count(asset: str = "BTC", since: int = None, until: int = None, interval: str = "24h") -> Any:
    params = {"a": asset, "i": interval, "api_key": API_KEY}
    if since:
        params["s"] = since
    if until:
        params["u"] = until
    return _make_request("transactions/count", params)

def get_hash_rate(asset: str = "BTC", since: int = None, until: int = None, interval: str = "24h") -> Any:
    params = {"a": asset, "i": interval, "api_key": API_KEY}
    if since:
        params["s"] = since
    if until:
        params["u"] = until
    return _make_request("mining/hash_rate_mean", params)

def get_nvt_ratio(asset: str = "BTC", since: int = None, until: int = None, interval: str = "24h") -> Any:
    params = {"a": asset, "i": interval, "api_key": API_KEY}
    if since:
        params["s"] = since
    if until:
        params["u"] = until
    return _make_request("indicators/nvt", params)

def get_sopr(asset: str = "BTC", since: int = None, until: int = None, interval: str = "24h") -> Any:
    params = {"a": asset, "i": interval, "api_key": API_KEY}
    if since:
        params["s"] = since
    if until:
        params["u"] = until
    return _make_request("indicators/sopr", params)

def get_available_metrics(asset: str = "BTC") -> Any:
    return _make_request("endpoints", {"a": asset, "api_key": API_KEY})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "active_addresses":
        asset = args[1] if len(args) > 1 else "BTC"
        since = int(args[2]) if len(args) > 2 else None
        until = int(args[3]) if len(args) > 3 else None
        interval = args[4] if len(args) > 4 else "24h"
        result = get_active_addresses(asset, since, until, interval)
    elif command == "transactions":
        asset = args[1] if len(args) > 1 else "BTC"
        since = int(args[2]) if len(args) > 2 else None
        until = int(args[3]) if len(args) > 3 else None
        interval = args[4] if len(args) > 4 else "24h"
        result = get_transaction_count(asset, since, until, interval)
    elif command == "hash_rate":
        asset = args[1] if len(args) > 1 else "BTC"
        since = int(args[2]) if len(args) > 2 else None
        until = int(args[3]) if len(args) > 3 else None
        interval = args[4] if len(args) > 4 else "24h"
        result = get_hash_rate(asset, since, until, interval)
    elif command == "nvt":
        asset = args[1] if len(args) > 1 else "BTC"
        since = int(args[2]) if len(args) > 2 else None
        until = int(args[3]) if len(args) > 3 else None
        interval = args[4] if len(args) > 4 else "24h"
        result = get_nvt_ratio(asset, since, until, interval)
    elif command == "sopr":
        asset = args[1] if len(args) > 1 else "BTC"
        since = int(args[2]) if len(args) > 2 else None
        until = int(args[3]) if len(args) > 3 else None
        interval = args[4] if len(args) > 4 else "24h"
        result = get_sopr(asset, since, until, interval)
    elif command == "metrics":
        asset = args[1] if len(args) > 1 else "BTC"
        result = get_available_metrics(asset)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
