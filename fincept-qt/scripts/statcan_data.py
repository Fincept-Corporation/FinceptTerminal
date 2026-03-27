"""
Statistics Canada Data Fetcher
Statistics Canada: Canadian economic, social, demographic data — GDP, trade, labour, housing.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www150.statcan.gc.ca/t1/tbl1/en/tv.action"
API_BASE = "https://www150.statcan.gc.ca/t1/tbl1/en/dtbl"

GDP_TABLE_ID = "36100104"
CPI_TABLE_ID = "18100004"
LABOUR_TABLE_ID = "14100287"
HOUSING_TABLE_ID = "34100143"
TRADE_TABLE_ID = "12100011"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{API_BASE}/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def _get_table_data(pid: str, start_date: str = None, end_date: str = None) -> Any:
    url = f"https://www150.statcan.gc.ca/t1/tbl1/en/dtbl/!downloadTbl?pid={pid}01&startDate={start_date or ''}&endDate={end_date or ''}"
    params = {"pid": f"{pid}01"}
    if start_date:
        params["startDate"] = start_date
    if end_date:
        params["endDate"] = end_date
    return _make_request(url, params=params)


def get_dataset(pid: str, start_date: str = None, end_date: str = None) -> Any:
    return _get_table_data(pid, start_date, end_date)


def get_gdp(province: str = "Canada", frequency: str = "quarterly", start_date: str = "2020-01-01", end_date: str = "2024-01-01") -> Any:
    params = {
        "pid": f"{GDP_TABLE_ID}01",
        "province": province,
        "frequency": frequency,
        "startDate": start_date,
        "endDate": end_date
    }
    url = f"https://www150.statcan.gc.ca/t1/tbl1/en/dtbl/!downloadTbl"
    return _make_request(url, params=params)


def get_cpi(product_group: str = "All-items", province: str = "Canada", start_date: str = "2020-01-01", end_date: str = "2024-01-01") -> Any:
    params = {
        "pid": f"{CPI_TABLE_ID}01",
        "productGroup": product_group,
        "province": province,
        "startDate": start_date,
        "endDate": end_date
    }
    url = f"https://www150.statcan.gc.ca/t1/tbl1/en/dtbl/!downloadTbl"
    return _make_request(url, params=params)


def get_labour_force(province: str = "Canada", sex: str = "Both sexes", age: str = "15 years and over", start_date: str = "2020-01-01", end_date: str = "2024-01-01") -> Any:
    params = {
        "pid": f"{LABOUR_TABLE_ID}01",
        "province": province,
        "sex": sex,
        "ageGroup": age,
        "startDate": start_date,
        "endDate": end_date
    }
    url = f"https://www150.statcan.gc.ca/t1/tbl1/en/dtbl/!downloadTbl"
    return _make_request(url, params=params)


def get_housing(province: str = "Canada", indicator: str = "Housing starts", start_date: str = "2020-01-01", end_date: str = "2024-01-01") -> Any:
    params = {
        "pid": f"{HOUSING_TABLE_ID}01",
        "province": province,
        "indicator": indicator,
        "startDate": start_date,
        "endDate": end_date
    }
    url = f"https://www150.statcan.gc.ca/t1/tbl1/en/dtbl/!downloadTbl"
    return _make_request(url, params=params)


def get_trade(commodity: str = "Total", partner: str = "All countries", start_date: str = "2020-01-01", end_date: str = "2024-01-01") -> Any:
    params = {
        "pid": f"{TRADE_TABLE_ID}01",
        "commodity": commodity,
        "partner": partner,
        "startDate": start_date,
        "endDate": end_date
    }
    url = f"https://www150.statcan.gc.ca/t1/tbl1/en/dtbl/!downloadTbl"
    return _make_request(url, params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "dataset":
        pid = args[1] if len(args) > 1 else GDP_TABLE_ID
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_dataset(pid, start_date, end_date)
    elif command == "gdp":
        province = args[1] if len(args) > 1 else "Canada"
        frequency = args[2] if len(args) > 2 else "quarterly"
        start_date = args[3] if len(args) > 3 else "2020-01-01"
        end_date = args[4] if len(args) > 4 else "2024-01-01"
        result = get_gdp(province, frequency, start_date, end_date)
    elif command == "cpi":
        product_group = args[1] if len(args) > 1 else "All-items"
        province = args[2] if len(args) > 2 else "Canada"
        start_date = args[3] if len(args) > 3 else "2020-01-01"
        end_date = args[4] if len(args) > 4 else "2024-01-01"
        result = get_cpi(product_group, province, start_date, end_date)
    elif command == "labour":
        province = args[1] if len(args) > 1 else "Canada"
        sex = args[2] if len(args) > 2 else "Both sexes"
        age = args[3] if len(args) > 3 else "15 years and over"
        start_date = args[4] if len(args) > 4 else "2020-01-01"
        end_date = args[5] if len(args) > 5 else "2024-01-01"
        result = get_labour_force(province, sex, age, start_date, end_date)
    elif command == "housing":
        province = args[1] if len(args) > 1 else "Canada"
        indicator = args[2] if len(args) > 2 else "Housing starts"
        start_date = args[3] if len(args) > 3 else "2020-01-01"
        end_date = args[4] if len(args) > 4 else "2024-01-01"
        result = get_housing(province, indicator, start_date, end_date)
    elif command == "trade":
        commodity = args[1] if len(args) > 1 else "Total"
        partner = args[2] if len(args) > 2 else "All countries"
        start_date = args[3] if len(args) > 3 else "2020-01-01"
        end_date = args[4] if len(args) > 4 else "2024-01-01"
        result = get_trade(commodity, partner, start_date, end_date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
