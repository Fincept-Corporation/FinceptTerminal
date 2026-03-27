"""
CBOE VIX Data Fetcher
CBOE VIX historical data and CBOE indices: VIX, VIX3M, VVIX, SKEW directly
from the CBOE CDN CSV endpoint. Parses CSV and converts to JSON.
"""
import sys
import json
import os
import io
import csv
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CBOE_API_KEY', '')
BASE_URL = "https://cdn.cboe.com/api/global/us_indices/daily_prices"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

CBOE_INDICES = {
    "VIX": "VIX_History.csv",
    "VIX3M": "VIX3M_History.csv",
    "VVIX": "VVIX_History.csv",
    "SKEW": "SKEW_History.csv",
    "VXN": "VXN_History.csv",
    "VXO": "VXO_History.csv",
    "GVZ": "GVZ_History.csv",
    "OVX": "OVX_History.csv",
    "TYVIX": "TYVIX_History.csv",
    "SHORTVOL": "ShortTermVol_History.csv",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.text
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def _parse_csv(csv_text: str, start_date: str = None, end_date: str = None) -> Any:
    if isinstance(csv_text, dict):
        return csv_text
    try:
        reader = csv.DictReader(io.StringIO(csv_text))
        rows = []
        for row in reader:
            clean = {k.strip(): v.strip() for k, v in row.items() if k}
            if not any(clean.values()):
                continue
            date_val = clean.get("DATE", clean.get("Date", ""))
            if start_date and date_val and date_val < start_date:
                continue
            if end_date and date_val and date_val > end_date:
                continue
            rows.append(clean)
        return {"data": rows, "count": len(rows)}
    except Exception as e:
        return {"error": f"CSV parse error: {str(e)}"}


def get_vix_history(index_name: str = "VIX") -> Any:
    name = index_name.upper()
    filename = CBOE_INDICES.get(name)
    if not filename:
        return {"error": f"Unknown index: {index_name}. Available: {list(CBOE_INDICES.keys())}"}
    raw = _make_request(filename)
    return _parse_csv(raw)


def get_vix_current() -> Any:
    raw = _make_request("VIX_History.csv")
    parsed = _parse_csv(raw)
    if "data" in parsed and parsed["data"]:
        return {"current": parsed["data"][-1]}
    return parsed


def get_available_indices() -> Any:
    return {"indices": list(CBOE_INDICES.keys()), "count": len(CBOE_INDICES)}


def get_vix_futures_term_structure() -> Any:
    vix_raw = _make_request("VIX_History.csv")
    vix3m_raw = _make_request("VIX3M_History.csv")
    vvix_raw = _make_request("VVIX_History.csv")
    vix_parsed = _parse_csv(vix_raw)
    vix3m_parsed = _parse_csv(vix3m_raw)
    vvix_parsed = _parse_csv(vvix_raw)
    latest_vix = vix_parsed["data"][-1] if "data" in vix_parsed and vix_parsed["data"] else {}
    latest_vix3m = vix3m_parsed["data"][-1] if "data" in vix3m_parsed and vix3m_parsed["data"] else {}
    latest_vvix = vvix_parsed["data"][-1] if "data" in vvix_parsed and vvix_parsed["data"] else {}
    return {
        "term_structure": {
            "VIX_spot": latest_vix,
            "VIX3M_3month": latest_vix3m,
            "VVIX_vol_of_vol": latest_vvix,
        }
    }


def get_index_data(index_name: str, start_date: str = None, end_date: str = None) -> Any:
    name = index_name.upper()
    filename = CBOE_INDICES.get(name)
    if not filename:
        return {"error": f"Unknown index: {index_name}. Available: {list(CBOE_INDICES.keys())}"}
    raw = _make_request(filename)
    return _parse_csv(raw, start_date, end_date)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "history":
        index_name = args[1] if len(args) > 1 else "VIX"
        result = get_vix_history(index_name)
    elif command == "current":
        result = get_vix_current()
    elif command == "indices":
        result = get_available_indices()
    elif command == "term_structure":
        result = get_vix_futures_term_structure()
    elif command == "data":
        index_name = args[1] if len(args) > 1 else "VIX"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_index_data(index_name, start_date, end_date)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
