"""
Freightos Baltic Index (FBX) Data Fetcher
Global container freight rates for 12 tradelanes — fetch from public data sources
and Freightos public chart endpoints.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://fbx.freightos.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    "Accept": "application/json, text/plain, */*",
    "Referer": "https://fbx.freightos.com/"
})

TRADELANES = {
    "FBX01": "Global Container",
    "FBX11": "China/East Asia to North America West Coast",
    "FBX12": "China/East Asia to North America East Coast",
    "FBX13": "China/East Asia to North Europe",
    "FBX14": "China/East Asia to Mediterranean",
    "FBX21": "North America West Coast to China/East Asia",
    "FBX22": "North America East Coast to China/East Asia",
    "FBX23": "North Europe to China/East Asia",
    "FBX24": "Mediterranean to China/East Asia",
    "FBX31": "North Europe to North America East Coast",
    "FBX32": "North America East Coast to North Europe",
    "FBX41": "North Europe to South America",
}

ALTERNATIVE_BASE = "https://api.freightos.com/v1"
DATAHUB_BASE = "https://datahub.io/core/freight-prices"


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


def _fetch_fbx_chart_data(index_id: str = "FBX01") -> Any:
    url = f"{BASE_URL}/api/index/{index_id}"
    try:
        resp = session.get(url, timeout=30)
        resp.raise_for_status()
        return resp.json()
    except Exception:
        pass
    # Fallback: try public data endpoint
    url2 = f"https://raw.githubusercontent.com/datasets/freight-prices/main/data/freight-prices.csv"
    try:
        resp2 = session.get(url2, timeout=30)
        resp2.raise_for_status()
        lines = resp2.text.strip().split('\n')
        headers = lines[0].split(',')
        records = []
        for line in lines[1:100]:
            parts = line.split(',')
            if len(parts) >= len(headers):
                records.append({headers[i]: parts[i] for i in range(len(headers))})
        return {"source": "datahub", "index": index_id, "data": records}
    except Exception as e2:
        return {"error": f"All sources failed. Primary: chart endpoint unavailable. Fallback: {str(e2)}"}


def get_global_index(start_date: str = None, end_date: str = None) -> Any:
    data = _fetch_fbx_chart_data("FBX01")
    if isinstance(data, dict) and "error" in data:
        return data
    if start_date or end_date:
        items = data.get("data", data) if isinstance(data, dict) else data
        if isinstance(items, list):
            filtered = []
            for item in items:
                date = item.get("date", item.get("Date", ""))
                if start_date and date < start_date:
                    continue
                if end_date and date > end_date:
                    continue
                filtered.append(item)
            if isinstance(data, dict):
                data["data"] = filtered
            else:
                data = filtered
    return {"index": "FBX01", "name": "Freightos Baltic Global Container Index",
            "description": TRADELANES.get("FBX01"), "data": data}


def get_tradelane_rates(tradelane: str = "FBX11", start_date: str = None, end_date: str = None) -> Any:
    if tradelane not in TRADELANES:
        return {"error": f"Unknown tradelane '{tradelane}'. Available: {list(TRADELANES.keys())}"}
    data = _fetch_fbx_chart_data(tradelane)
    return {"index": tradelane, "name": TRADELANES[tradelane],
            "start_date": start_date, "end_date": end_date, "data": data}


def get_latest_rates() -> Any:
    results = {}
    # Fetch global index as representative latest
    data = _fetch_fbx_chart_data("FBX01")
    if isinstance(data, dict) and "error" not in data:
        items = data.get("data", [])
        if isinstance(items, list) and items:
            results["FBX01"] = items[-1] if items else {}
    results["tradelanes"] = TRADELANES
    results["note"] = "Latest rates from Freightos Baltic Index"
    return results


def get_available_tradelanes() -> Any:
    return {"tradelanes": TRADELANES,
            "count": len(TRADELANES),
            "description": "Freightos Baltic Index (FBX) tradelane codes and descriptions"}


def get_rate_history(tradelane: str = "FBX01", weeks: int = 52) -> Any:
    data = _fetch_fbx_chart_data(tradelane)
    if isinstance(data, dict) and "error" in data:
        return data
    items = data.get("data", data) if isinstance(data, dict) else data
    if isinstance(items, list):
        items = items[-weeks:] if len(items) > weeks else items
    return {"index": tradelane, "name": TRADELANES.get(tradelane, tradelane),
            "weeks": weeks, "count": len(items) if isinstance(items, list) else 0,
            "data": items}


def get_spot_vs_contract(tradelane: str = "FBX11") -> Any:
    spot = _fetch_fbx_chart_data(tradelane)
    return {"tradelane": tradelane, "name": TRADELANES.get(tradelane, tradelane),
            "spot_rates": spot,
            "note": "Contract rate data requires premium subscription"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "global":
        start_date = args[1] if len(args) > 1 else None
        end_date = args[2] if len(args) > 2 else None
        result = get_global_index(start_date, end_date)
    elif command == "tradelane":
        tradelane = args[1] if len(args) > 1 else "FBX11"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_tradelane_rates(tradelane, start_date, end_date)
    elif command == "latest":
        result = get_latest_rates()
    elif command == "lanes":
        result = get_available_tradelanes()
    elif command == "history":
        tradelane = args[1] if len(args) > 1 else "FBX01"
        weeks = int(args[2]) if len(args) > 2 else 52
        result = get_rate_history(tradelane, weeks)
    elif command == "comparison":
        tradelane = args[1] if len(args) > 1 else "FBX11"
        result = get_spot_vs_contract(tradelane)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
