"""
Australian Bureau of Statistics Data Fetcher
ABS: Australian economic, demographic, social statistics — GDP, CPI, labour, trade.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.data.abs.gov.au"

ABS_DATAFLOWS = {
    "gdp": "ANA_AGG",
    "cpi": "CPI",
    "labour": "LF",
    "trade": "MERCH_EXP",
    "population": "ERP_QUARTERLY"
}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params.setdefault("format", "jsondata")
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


def get_dataflow(dataflow_id: str) -> Any:
    return _make_request(f"dataflow/ABS/{dataflow_id}")


def get_data(dataflow_id: str, key: str = "all", start_period: str = None, end_period: str = None) -> Any:
    params = {}
    if start_period:
        params["startPeriod"] = start_period
    if end_period:
        params["endPeriod"] = end_period
    return _make_request(f"data/{dataflow_id}/{key}", params=params)


def get_gdp(state: str = "AUS", frequency: str = "Q", start: str = "2020-Q1", end: str = "2024-Q4") -> Any:
    dataflow = ABS_DATAFLOWS["gdp"]
    params = {"startPeriod": start, "endPeriod": end}
    return _make_request(f"data/{dataflow}/{state}..", params=params)


def get_cpi(product_group: str = "1", state: str = "50", start: str = "2020-Q1", end: str = "2024-Q4") -> Any:
    dataflow = ABS_DATAFLOWS["cpi"]
    params = {"startPeriod": start, "endPeriod": end}
    return _make_request(f"data/{dataflow}/{product_group}.{state}.", params=params)


def get_labour(state: str = "AUS", sex: str = "3", start: str = "2020-01", end: str = "2024-12") -> Any:
    dataflow = ABS_DATAFLOWS["labour"]
    params = {"startPeriod": start, "endPeriod": end}
    return _make_request(f"data/{dataflow}/{state}.{sex}.", params=params)


def get_trade(product: str = "all", partner: str = "all", start: str = "2020-01", end: str = "2024-12") -> Any:
    dataflow = ABS_DATAFLOWS["trade"]
    params = {"startPeriod": start, "endPeriod": end}
    key = f"{product}.{partner}" if product != "all" or partner != "all" else "all"
    return _make_request(f"data/{dataflow}/{key}", params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "dataflow":
        dataflow_id = args[1] if len(args) > 1 else "ANA_AGG"
        result = get_dataflow(dataflow_id)
    elif command == "data":
        dataflow_id = args[1] if len(args) > 1 else "ANA_AGG"
        key = args[2] if len(args) > 2 else "all"
        start_period = args[3] if len(args) > 3 else None
        end_period = args[4] if len(args) > 4 else None
        result = get_data(dataflow_id, key, start_period, end_period)
    elif command == "gdp":
        state = args[1] if len(args) > 1 else "AUS"
        frequency = args[2] if len(args) > 2 else "Q"
        start = args[3] if len(args) > 3 else "2020-Q1"
        end = args[4] if len(args) > 4 else "2024-Q4"
        result = get_gdp(state, frequency, start, end)
    elif command == "cpi":
        product_group = args[1] if len(args) > 1 else "1"
        state = args[2] if len(args) > 2 else "50"
        start = args[3] if len(args) > 3 else "2020-Q1"
        end = args[4] if len(args) > 4 else "2024-Q4"
        result = get_cpi(product_group, state, start, end)
    elif command == "labour":
        state = args[1] if len(args) > 1 else "AUS"
        sex = args[2] if len(args) > 2 else "3"
        start = args[3] if len(args) > 3 else "2020-01"
        end = args[4] if len(args) > 4 else "2024-12"
        result = get_labour(state, sex, start, end)
    elif command == "trade":
        product = args[1] if len(args) > 1 else "all"
        partner = args[2] if len(args) > 2 else "all"
        start = args[3] if len(args) > 3 else "2020-01"
        end = args[4] if len(args) > 4 else "2024-12"
        result = get_trade(product, partner, start, end)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
