"""
Statistics Canada Data Fetcher
Statistics Canada: Canadian economic, social, demographic data — GDP, trade, labour, housing.

Series commands (gdp, cpi, unemployment, employment, population, housing) read StatCan's
Web Data Service by vector id (POST getDataFromVectorsAndLatestNPeriods) - no API key
required. The previous "!downloadTbl" query-parameter URLs return HTTP 404.
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

WDS_URL = "https://www150.statcan.gc.ca/t1/wds/rest/getDataFromVectorsAndLatestNPeriods"

# command -> WDS vector. Latest observations are requested newest-first and returned oldest-first.
STATCAN_VECTORS = {
    "gdp": {"vector": 65201210, "label": "Real GDP at basic prices, all industries (chained 2017 $ millions, SA)",
            "table": "36-10-0434-01", "unit": "CAD millions"},
    "cpi": {"vector": 41690973, "label": "Consumer Price Index, all-items, Canada (2002=100)",
            "table": "18-10-0004-01", "unit": "index"},
    "unemployment": {"vector": 2062815, "label": "Unemployment rate, 15 years and over, SA",
                     "table": "14-10-0287-01", "unit": "%"},
    "employment": {"vector": 2062817, "label": "Employment rate, 15 years and over, SA",
                   "table": "14-10-0287-01", "unit": "%"},
    "population": {"vector": 466668, "label": "Population estimate, Canada (July 1)",
                   "table": "17-10-0005-01", "unit": "persons"},
    "housing": {"vector": 52300157, "label": "Housing starts, all areas, SAAR",
                "table": "34-10-0158-01", "unit": "units"},
}
DEFAULT_LATEST_N = 600

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


def _vector_series(command: str, start_date: str = None, end_date: str = None,
                   latest_n: int = DEFAULT_LATEST_N) -> Any:
    """Fetch one StatCan vector and normalise it to {success, label, unit, count, data:[{date, value}]}."""
    spec = STATCAN_VECTORS[command]
    try:
        response = session.post(WDS_URL, json=[{"vectorId": spec["vector"], "latestN": latest_n}], timeout=30)
        response.raise_for_status()
        payload = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

    item = payload[0] if isinstance(payload, list) and payload else {}
    if item.get("status") != "SUCCESS" or "object" not in item:
        return {"error": f"StatCan returned no data for vector v{spec['vector']}: {item.get('object') or item}"}

    rows = []
    for point in item["object"].get("vectorDataPoint", []):
        ref = str(point.get("refPer", ""))
        if start_date and ref < start_date:
            continue
        if end_date and ref > end_date:
            continue
        try:
            value = float(point.get("value"))
        except (TypeError, ValueError):
            continue  # suppressed / missing observation - a gap stays a gap
        # Monthly reference periods are the 1st of the month; show them as YYYY-MM
        rows.append({"date": ref[:7] if ref.endswith("-01") and command != "population" else ref, "value": value})
    rows.sort(key=lambda r: r["date"])

    return {
        "success": True,
        "series": command,
        "vector_id": f"v{spec['vector']}",
        "label": spec["label"],
        "unit": spec["unit"],
        "table": spec["table"],
        "count": len(rows),
        "data": rows,
        "source": "Statistics Canada",
    }


def get_gdp(province: str = "Canada", frequency: str = "quarterly", start_date: str = None, end_date: str = None) -> Any:
    return _vector_series("gdp", start_date, end_date)


def get_cpi(product_group: str = "All-items", province: str = "Canada", start_date: str = None, end_date: str = None) -> Any:
    return _vector_series("cpi", start_date, end_date)


def get_labour_force(province: str = "Canada", sex: str = "Both sexes", age: str = "15 years and over", start_date: str = None, end_date: str = None) -> Any:
    return _vector_series("unemployment", start_date, end_date)


def get_housing(province: str = "Canada", indicator: str = "Housing starts", start_date: str = None, end_date: str = None) -> Any:
    return _vector_series("housing", start_date, end_date)


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
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        result = get_gdp(province, frequency, start_date, end_date)
    elif command == "cpi":
        product_group = args[1] if len(args) > 1 else "All-items"
        province = args[2] if len(args) > 2 else "Canada"
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        result = get_cpi(product_group, province, start_date, end_date)
    elif command == "labour":
        province = args[1] if len(args) > 1 else "Canada"
        sex = args[2] if len(args) > 2 else "Both sexes"
        age = args[3] if len(args) > 3 else "15 years and over"
        start_date = args[4] if len(args) > 4 else None
        end_date = args[5] if len(args) > 5 else None
        result = get_labour_force(province, sex, age, start_date, end_date)
    elif command == "housing":
        province = args[1] if len(args) > 1 else "Canada"
        indicator = args[2] if len(args) > 2 else "Housing starts"
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        result = get_housing(province, indicator, start_date, end_date)
    elif command in ("unemployment", "employment", "population"):
        # <command> [start_date] [end_date]  (YYYY-MM-DD)
        start_date = args[1] if len(args) > 1 else None
        end_date = args[2] if len(args) > 2 else None
        result = _vector_series(command, start_date, end_date)
    elif command == "trade":
        commodity = args[1] if len(args) > 1 else "Total"
        partner = args[2] if len(args) > 2 else "All countries"
        start_date = args[3] if len(args) > 3 else "2020-01-01"
        end_date = args[4] if len(args) > 4 else "2024-01-01"
        result = get_trade(commodity, partner, start_date, end_date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
