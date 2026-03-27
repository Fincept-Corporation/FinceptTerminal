"""
ONS Data Fetcher
UK Office for National Statistics (ONS): UK GDP, CPI, labour market, trade, population, housing.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.ons.gov.uk/v1"

ONS_DATASETS = {
    "gdp": "cpih01",
    "cpi": "mm23",
    "unemployment": "lms",
    "trade": "ots",
    "population": "mid-year-pop-est",
    "housing": "hpssa"
}

ONS_TIMESERIES = {
    "gdp_quarterly": {"dataset": "qna", "timeseries": "ABMI"},
    "cpi_all_items": {"dataset": "mm23", "timeseries": "D7G7"},
    "unemployment_rate": {"dataset": "lms", "timeseries": "MGSX"},
    "exports_goods": {"dataset": "ots", "timeseries": "BOKH"},
    "imports_goods": {"dataset": "ots", "timeseries": "BOKJ"}
}

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


def get_dataset(dataset_id: str) -> Any:
    return _make_request(f"datasets/{dataset_id}")


def get_timeseries(dataset_id: str, timeseries_id: str, start_year: str = None, end_year: str = None) -> Any:
    endpoint = f"datasets/{dataset_id}/timeseries/{timeseries_id}/data"
    params = {}
    if start_year:
        params["startYear"] = start_year
    if end_year:
        params["endYear"] = end_year
    return _make_request(endpoint, params=params)


def get_gdp(frequency: str = "quarterly", start: str = "2020", end: str = "2024") -> Any:
    ts = ONS_TIMESERIES["gdp_quarterly"]
    return get_timeseries(ts["dataset"], ts["timeseries"], start, end)


def get_cpi(category: str = "all_items", start: str = "2020", end: str = "2024") -> Any:
    ts = ONS_TIMESERIES["cpi_all_items"]
    return get_timeseries(ts["dataset"], ts["timeseries"], start, end)


def get_unemployment(measure: str = "rate", start: str = "2020", end: str = "2024") -> Any:
    ts = ONS_TIMESERIES["unemployment_rate"]
    return get_timeseries(ts["dataset"], ts["timeseries"], start, end)


def search(query: str) -> Any:
    params = {"q": query}
    return _make_request("search", params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "dataset":
        dataset_id = args[1] if len(args) > 1 else "mm23"
        result = get_dataset(dataset_id)
    elif command == "timeseries":
        dataset_id = args[1] if len(args) > 1 else "qna"
        timeseries_id = args[2] if len(args) > 2 else "ABMI"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        result = get_timeseries(dataset_id, timeseries_id, start_year, end_year)
    elif command == "gdp":
        frequency = args[1] if len(args) > 1 else "quarterly"
        start = args[2] if len(args) > 2 else "2020"
        end = args[3] if len(args) > 3 else "2024"
        result = get_gdp(frequency, start, end)
    elif command == "cpi":
        category = args[1] if len(args) > 1 else "all_items"
        start = args[2] if len(args) > 2 else "2020"
        end = args[3] if len(args) > 3 else "2024"
        result = get_cpi(category, start, end)
    elif command == "unemployment":
        measure = args[1] if len(args) > 1 else "rate"
        start = args[2] if len(args) > 2 else "2020"
        end = args[3] if len(args) > 3 else "2024"
        result = get_unemployment(measure, start, end)
    elif command == "search":
        query = args[1] if len(args) > 1 else "GDP"
        result = search(query)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
