"""
Nasdaq Data Link WIKI Prices Data Fetcher
Fetches adjusted end-of-day US stock prices, dividends, and splits for 3000+ stocks
via the Nasdaq Data Link (formerly Quandl) WIKI dataset.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('NASDAQ_DATA_LINK_API_KEY', '')
BASE_URL = "https://data.nasdaq.com/api/v3"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

WIKI_TABLE = "WIKI/PRICES"
WIKI_COLUMNS = [
    "ticker", "date", "open", "high", "low", "close", "volume",
    "ex-dividend", "split_ratio", "adj_open", "adj_high", "adj_low",
    "adj_close", "adj_volume",
]


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["api_key"] = API_KEY
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


def _check_key() -> Optional[Dict]:
    if not API_KEY:
        return {
            "error": "NASDAQ_DATA_LINK_API_KEY environment variable not set",
            "note": "Get a free API key at https://data.nasdaq.com/sign-up",
        }
    return None


def _rows_to_records(columns: List[str], rows: List) -> List[Dict]:
    return [dict(zip(columns, row)) for row in rows]


def get_stock_prices(ticker: str, start_date: str = "", end_date: str = "") -> Dict:
    key_err = _check_key()
    if key_err:
        return key_err
    params = {
        "ticker": ticker.upper(),
        "qopts.columns": "ticker,date,open,high,low,close,volume,adj_open,adj_high,adj_low,adj_close,adj_volume",
    }
    if start_date:
        params["date.gte"] = start_date
    if end_date:
        params["date.lte"] = end_date
    data = _make_request("datatables/WIKI/PRICES.json", params)
    if "error" in data:
        return data
    dt = data.get("datatable", {})
    columns = [c["name"] for c in dt.get("columns", [])]
    rows = dt.get("data", [])
    return {
        "ticker": ticker.upper(),
        "start_date": start_date,
        "end_date": end_date,
        "count": len(rows),
        "columns": columns,
        "prices": _rows_to_records(columns, rows),
    }


def get_dividends(ticker: str, start_date: str = "", end_date: str = "") -> Dict:
    key_err = _check_key()
    if key_err:
        return key_err
    params = {
        "ticker": ticker.upper(),
        "qopts.columns": "ticker,date,ex-dividend",
        "ex-dividend.gt": "0",
    }
    if start_date:
        params["date.gte"] = start_date
    if end_date:
        params["date.lte"] = end_date
    data = _make_request("datatables/WIKI/PRICES.json", params)
    if "error" in data:
        return data
    dt = data.get("datatable", {})
    columns = [c["name"] for c in dt.get("columns", [])]
    rows = dt.get("data", [])
    return {
        "ticker": ticker.upper(),
        "start_date": start_date,
        "end_date": end_date,
        "dividend_count": len(rows),
        "dividends": _rows_to_records(columns, rows),
    }


def get_splits(ticker: str, start_date: str = "", end_date: str = "") -> Dict:
    key_err = _check_key()
    if key_err:
        return key_err
    params = {
        "ticker": ticker.upper(),
        "qopts.columns": "ticker,date,split_ratio",
        "split_ratio.gt": "1",
    }
    if start_date:
        params["date.gte"] = start_date
    if end_date:
        params["date.lte"] = end_date
    data = _make_request("datatables/WIKI/PRICES.json", params)
    if "error" in data:
        return data
    dt = data.get("datatable", {})
    columns = [c["name"] for c in dt.get("columns", [])]
    rows = dt.get("data", [])
    return {
        "ticker": ticker.upper(),
        "start_date": start_date,
        "end_date": end_date,
        "split_count": len(rows),
        "splits": _rows_to_records(columns, rows),
    }


def get_available_tickers() -> Dict:
    key_err = _check_key()
    if key_err:
        return key_err
    params = {
        "qopts.columns": "ticker",
        "qopts.per_page": 10000,
        "date": "2018-03-27",
    }
    data = _make_request("datatables/WIKI/PRICES.json", params)
    if "error" in data:
        return data
    dt = data.get("datatable", {})
    rows = dt.get("data", [])
    tickers = sorted(set(row[0] for row in rows if row))
    return {
        "total_tickers": len(tickers),
        "tickers": tickers,
        "note": "WIKI dataset covers US stocks up to ~2018. Use other data sources for current prices.",
    }


def get_metadata(ticker: str) -> Dict:
    key_err = _check_key()
    if key_err:
        return key_err
    params = {
        "ticker": ticker.upper(),
        "qopts.columns": "ticker,date,open,high,low,close,volume,adj_close",
        "qopts.per_page": 1,
        "qopts.sort_order": "desc",
    }
    data = _make_request("datatables/WIKI/PRICES.json", params)
    if "error" in data:
        return data
    dt = data.get("datatable", {})
    columns = [c["name"] for c in dt.get("columns", [])]
    rows = dt.get("data", [])
    if rows:
        latest = dict(zip(columns, rows[0]))
    else:
        latest = {}
    return {
        "ticker": ticker.upper(),
        "dataset": "WIKI/PRICES",
        "provider": "Nasdaq Data Link (legacy Quandl)",
        "latest_record": latest,
        "note": "WIKI dataset has data through Q1 2018.",
    }


def batch_prices(tickers: str, date: str) -> Dict:
    key_err = _check_key()
    if key_err:
        return key_err
    ticker_list = [t.strip().upper() for t in tickers.split(",")]
    params = {
        "ticker": "|".join(ticker_list),
        "date": date,
        "qopts.columns": "ticker,date,adj_open,adj_high,adj_low,adj_close,adj_volume",
    }
    data = _make_request("datatables/WIKI/PRICES.json", params)
    if "error" in data:
        return data
    dt = data.get("datatable", {})
    columns = [c["name"] for c in dt.get("columns", [])]
    rows = dt.get("data", [])
    return {
        "tickers": ticker_list,
        "date": date,
        "results": _rows_to_records(columns, rows),
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "prices":
        ticker = args[1] if len(args) > 1 else ""
        if not ticker:
            result = {"error": "ticker required"}
        else:
            start_date = args[2] if len(args) > 2 else ""
            end_date = args[3] if len(args) > 3 else ""
            result = get_stock_prices(ticker, start_date, end_date)
    elif command == "dividends":
        ticker = args[1] if len(args) > 1 else ""
        if not ticker:
            result = {"error": "ticker required"}
        else:
            start_date = args[2] if len(args) > 2 else ""
            end_date = args[3] if len(args) > 3 else ""
            result = get_dividends(ticker, start_date, end_date)
    elif command == "splits":
        ticker = args[1] if len(args) > 1 else ""
        if not ticker:
            result = {"error": "ticker required"}
        else:
            start_date = args[2] if len(args) > 2 else ""
            end_date = args[3] if len(args) > 3 else ""
            result = get_splits(ticker, start_date, end_date)
    elif command == "tickers":
        result = get_available_tickers()
    elif command == "metadata":
        ticker = args[1] if len(args) > 1 else ""
        if not ticker:
            result = {"error": "ticker required"}
        else:
            result = get_metadata(ticker)
    elif command == "batch":
        tickers = args[1] if len(args) > 1 else ""
        date = args[2] if len(args) > 2 else ""
        if not tickers or not date:
            result = {"error": "tickers (comma-separated) and date required"}
        else:
            result = batch_prices(tickers, date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
