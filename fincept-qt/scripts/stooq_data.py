"""
Stooq Data Fetcher
OHLCV historical data for 21000+ global stocks (US, Japan, Poland, Germany, Hungary)
via the Stooq CSV download endpoint. Parses CSV responses and converts to JSON.
"""
import sys
import json
import os
import io
import csv
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('STOOQ_API_KEY', '')
BASE_URL = "https://stooq.com/q/d/l"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


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


def _parse_csv(csv_text: str) -> Any:
    if isinstance(csv_text, dict):
        return csv_text
    try:
        reader = csv.DictReader(io.StringIO(csv_text))
        rows = list(reader)
        if not rows:
            return {"error": "No data returned from Stooq"}
        cleaned = []
        for row in rows:
            cleaned.append({k.strip(): v.strip() for k, v in row.items()})
        return {"data": cleaned, "count": len(cleaned)}
    except Exception as e:
        return {"error": f"CSV parse error: {str(e)}"}


def get_historical(symbol: str, start: str = None, end: str = None, interval: str = "d") -> Any:
    params = {"s": symbol.lower(), "i": interval}
    if start:
        params["d1"] = start.replace("-", "")
    if end:
        params["d2"] = end.replace("-", "")
    raw = _make_request("", params)
    return _parse_csv(raw)


def get_quote(symbol: str) -> Any:
    params = {"s": symbol.lower(), "i": "d"}
    raw = _make_request("", params)
    parsed = _parse_csv(raw)
    if "data" in parsed and parsed["data"]:
        return {"symbol": symbol, "latest": parsed["data"][-1]}
    return parsed


def get_index_components(index: str) -> Any:
    index_map = {
        "^spx": "sp500",
        "^djx": "djia",
        "^ndx": "nasdaq100",
        "wig20": "wig20",
        "nikkei": "n225",
    }
    return {"error": "Index component listing not supported by Stooq CSV API — use symbol directly", "known_indices": list(index_map.keys())}


def get_multiple(symbols: str, start: str = None, end: str = None) -> Any:
    symbol_list = [s.strip() for s in symbols.split(",")]
    results = {}
    for sym in symbol_list:
        results[sym] = get_historical(sym, start, end)
    return results


def search_symbol(query: str) -> Any:
    params = {"q": query}
    try:
        response = session.get("https://stooq.com/q/s/", params=params, timeout=30)
        return {"note": "Stooq does not expose a public search API. Use symbol directly (e.g. AAPL.US, MSFT.US, ^SPX, PKN.PL)", "query": query}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "historical":
        symbol = args[1] if len(args) > 1 else "AAPL.US"
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        interval = args[4] if len(args) > 4 else "d"
        result = get_historical(symbol, start, end, interval)
    elif command == "quote":
        symbol = args[1] if len(args) > 1 else "AAPL.US"
        result = get_quote(symbol)
    elif command == "index":
        index = args[1] if len(args) > 1 else "^SPX"
        result = get_index_components(index)
    elif command == "multiple":
        symbols = args[1] if len(args) > 1 else "AAPL.US,MSFT.US"
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        result = get_multiple(symbols, start, end)
    elif command == "search":
        query = args[1] if len(args) > 1 else "apple"
        result = search_symbol(query)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
