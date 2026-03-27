"""
Nasdaq Data Link (formerly Quandl) Data Fetcher
Financial, economic, and alternative datasets — Fed data, commodities, futures.
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

def get_dataset(database_code: str, dataset_code: str, start_date: str = None, end_date: str = None) -> Any:
    params = {"api_key": API_KEY}
    if start_date:
        params["start_date"] = start_date
    if end_date:
        params["end_date"] = end_date
    return _make_request(f"datasets/{database_code}/{dataset_code}/data.json", params)

def get_database_metadata(database_code: str) -> Any:
    return _make_request(f"databases/{database_code}.json", {"api_key": API_KEY})

def search_datasets(query: str, database_code: str = None, per_page: int = 20) -> Any:
    params = {"query": query, "per_page": per_page, "api_key": API_KEY}
    if database_code:
        params["database_code"] = database_code
    return _make_request("datasets.json", params)

def get_datatable(datatable_code: str, filters: Dict = None) -> Any:
    params = {"api_key": API_KEY}
    if filters:
        params.update(filters)
    return _make_request(f"datatables/{datatable_code}.json", params)

def get_bulk_download_url(database_code: str) -> Any:
    return _make_request(f"databases/{database_code}/data.json", {"api_key": API_KEY, "download_type": "full"})

def get_dataset_metadata(database_code: str, dataset_code: str) -> Any:
    return _make_request(f"datasets/{database_code}/{dataset_code}/metadata.json", {"api_key": API_KEY})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "dataset":
        database_code = args[1] if len(args) > 1 else "FRED"
        dataset_code = args[2] if len(args) > 2 else "GDP"
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        result = get_dataset(database_code, dataset_code, start_date, end_date)
    elif command == "database":
        database_code = args[1] if len(args) > 1 else "FRED"
        result = get_database_metadata(database_code)
    elif command == "search":
        query = args[1] if len(args) > 1 else "GDP"
        database_code = args[2] if len(args) > 2 else None
        per_page = int(args[3]) if len(args) > 3 else 20
        result = search_datasets(query, database_code, per_page)
    elif command == "datatable":
        datatable_code = args[1] if len(args) > 1 else "WIKI/PRICES"
        result = get_datatable(datatable_code)
    elif command == "bulk":
        database_code = args[1] if len(args) > 1 else "FRED"
        result = get_bulk_download_url(database_code)
    elif command == "metadata":
        database_code = args[1] if len(args) > 1 else "FRED"
        dataset_code = args[2] if len(args) > 2 else "GDP"
        result = get_dataset_metadata(database_code, dataset_code)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
