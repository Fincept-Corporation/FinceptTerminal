"""
OpenFIGI Data Fetcher
Map tickers/ISINs/CUSIPs to Bloomberg FIGI identifiers. 1700+ asset classes globally.
No key for basic use (higher rate with OPENFIGI_API_KEY).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OPENFIGI_API_KEY', '')
BASE_URL = "https://api.openfigi.com/v3"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

_headers = {"Content-Type": "application/json"}
if API_KEY:
    _headers["X-OPENFIGI-APIKEY"] = API_KEY


def _make_post(endpoint: str, payload: Any) -> Any:
    """Make POST request with error handling."""
    url = f"{BASE_URL}/{endpoint}"
    try:
        response = session.post(url, json=payload, headers=_headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make GET request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, headers=_headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def map_identifiers(mappings: List[Dict]) -> Any:
    """Map a list of identifiers to FIGIs.
    mappings: list of dicts with keys idType, idValue, and optional exchCode, currency, marketSecDes, etc.
    idType options: ID_ISIN, ID_CUSIP, TICKER, ID_SEDOL, ID_WERTPAPIER, ID_BB_GLOBAL, etc.
    """
    if not mappings:
        return {"error": "No mappings provided"}
    # API limit: 100 items per request
    chunk = mappings[:100]
    data = _make_post("mapping", chunk)
    if isinstance(data, list):
        results = []
        for i, item in enumerate(data):
            entry = {"input": chunk[i] if i < len(chunk) else {}}
            if "data" in item:
                entry["matches"] = item["data"]
                entry["count"] = len(item["data"])
            elif "error" in item:
                entry["error"] = item["error"]
            else:
                entry["raw"] = item
            results.append(entry)
        return {"results": results, "total": len(results)}
    return data


def map_from_args(id_pairs: str) -> Any:
    """Parse comma-separated idType:idValue pairs and map them.
    Example: 'TICKER:AAPL,ID_ISIN:US0378331005'
    """
    mappings = []
    for pair in id_pairs.split(","):
        pair = pair.strip()
        if ":" not in pair:
            continue
        parts = pair.split(":", 1)
        if len(parts) == 2:
            id_type, id_value = parts
            mappings.append({"idType": id_type.strip(), "idValue": id_value.strip()})
    if not mappings:
        return {"error": "No valid idType:idValue pairs found. Format: TICKER:AAPL,ID_ISIN:US0378331005"}
    return map_identifiers(mappings)


def search_figi(query: str, asset_class: str = None) -> Any:
    """Search for FIGIs by name/ticker with optional asset class filter.
    asset_class options: Equity, Fixed Income, Commodity, Mortgage, Fund, etc.
    """
    payload = {"query": query}
    if asset_class:
        payload["assetClass"] = asset_class
    data = _make_post("search", payload)
    if isinstance(data, dict) and "data" in data:
        return {"query": query, "asset_class": asset_class, "results": data["data"], "count": len(data["data"])}
    return data


def get_figi(figi_id: str) -> Any:
    """Look up detailed metadata for a known FIGI identifier."""
    data = _make_post("mapping", [{"idType": "ID_BB_GLOBAL", "idValue": figi_id}])
    if isinstance(data, list) and len(data) > 0:
        item = data[0]
        if "data" in item:
            return {"figi": figi_id, "result": item["data"]}
        elif "error" in item:
            return {"error": item["error"]}
    return data


def get_enum_values(enum_type: str) -> Any:
    """Get valid enumeration values for OpenFIGI fields.
    enum_type options: idType, exchCode, micCode, currency, marketSecDes, securityType, securityType2.
    """
    data = _make_request(f"mapping/values/{enum_type}")
    if isinstance(data, dict) and "values" in data:
        return {"enum_type": enum_type, "values": data["values"], "count": len(data["values"])}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: map, search, lookup, enum"}))
        return

    command = args[0]

    if command == "map":
        if len(args) < 2:
            result = {"error": "Usage: map <idType:idValue,...> (e.g. 'TICKER:AAPL,ID_ISIN:US0378331005')"}
        else:
            result = map_from_args(args[1])
    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: search <query> [asset_class]"}
        else:
            asset_class = args[2] if len(args) > 2 else None
            result = search_figi(args[1], asset_class)
    elif command == "lookup":
        if len(args) < 2:
            result = {"error": "Usage: lookup <figi_id>"}
        else:
            result = get_figi(args[1])
    elif command == "enum":
        if len(args) < 2:
            result = {"error": "Usage: enum <enum_type> (idType, exchCode, micCode, currency, marketSecDes, securityType, securityType2)"}
        else:
            result = get_enum_values(args[1])
    else:
        result = {"error": f"Unknown command: {command}. Available: map, search, lookup, enum"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
