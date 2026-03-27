"""
UN SDG Statistics API Data Fetcher
All 17 SDG goals, 230+ indicators for all countries.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://unstats.un.org/SDGAPI/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=60)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_goals() -> Any:
    """Get all 17 UN Sustainable Development Goals with titles and descriptions."""
    data = _make_request("sdg/Goal/List")
    if isinstance(data, list):
        return {"goals": data, "count": len(data)}
    return data


def get_targets(goal_code: str) -> Any:
    """Get targets for a specific SDG goal.
    goal_code: 1-17 (e.g. '1' for No Poverty, '13' for Climate Action).
    """
    data = _make_request(f"sdg/Goal/{goal_code}/Target/List")
    if isinstance(data, list):
        return {"goal": goal_code, "targets": data, "count": len(data)}
    return data


def get_indicators(goal_code: str = None) -> Any:
    """Get SDG indicators, optionally filtered by goal.
    goal_code: 1-17, or None for all indicators.
    """
    if goal_code:
        data = _make_request(f"sdg/Goal/{goal_code}/Indicator/List")
    else:
        data = _make_request("sdg/Indicator/List")
    if isinstance(data, list):
        return {"goal": goal_code, "indicators": data, "count": len(data)}
    return data


def get_data(indicator_code: str, area_code: str = None, start_year: int = None, end_year: int = None) -> Any:
    """Get SDG indicator data for countries/areas.
    indicator_code: e.g. '1.1.1', '13.2.2', '8.10.1'.
    area_code: ISO M49 or ISO 3166-1 code (e.g. '840' for USA, '276' for Germany).
    start_year/end_year: filter by year range.
    """
    params = {"pageSize": 500}
    if area_code:
        params["areaCode"] = area_code
    if start_year:
        params["timePeriodStart"] = start_year
    if end_year:
        params["timePeriodEnd"] = end_year

    data = _make_request(f"sdg/Indicator/Data?indicator={indicator_code}", params=params)
    if isinstance(data, dict) and "data" in data:
        rows = data["data"]
        return {
            "indicator": indicator_code,
            "area_code": area_code,
            "data": rows[:200],
            "total_records": data.get("totalElements", len(rows)),
        }
    return data


def get_areas() -> Any:
    """Get list of all geographic areas/countries with M49 codes."""
    data = _make_request("sdg/GeoArea/List")
    if isinstance(data, list):
        return {"areas": data, "count": len(data)}
    return data


def get_series() -> Any:
    """Get list of all available SDG data series codes."""
    data = _make_request("sdg/Series/List")
    if isinstance(data, list):
        return {"series": data[:300], "total_count": len(data)}
    return data


def get_series_data(series_code: str, area_code: str = None, start_year: int = None) -> Any:
    """Get data for a specific SDG series code.
    series_code: e.g. 'SP_ACS_BSRVH2O', 'EN_ATM_CO2'.
    """
    params = {"pageSize": 300}
    if area_code:
        params["areaCode"] = area_code
    if start_year:
        params["timePeriodStart"] = start_year

    data = _make_request(f"sdg/Series/Data?series={series_code}", params=params)
    if isinstance(data, dict) and "data" in data:
        rows = data["data"]
        return {
            "series": series_code,
            "area_code": area_code,
            "data": rows[:200],
            "total_records": data.get("totalElements", len(rows)),
        }
    return data


def get_indicator_overview(indicator_code: str) -> Any:
    """Get metadata and description for a specific indicator."""
    data = _make_request(f"sdg/Indicator/{indicator_code}")
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: goals, targets, indicators, data, areas, series, series_data, indicator_info"}))
        return

    command = args[0]

    if command == "goals":
        result = get_goals()
    elif command == "targets":
        if len(args) < 2:
            result = {"error": "Usage: targets <goal_code> (1-17)"}
        else:
            result = get_targets(args[1])
    elif command == "indicators":
        goal_code = args[1] if len(args) > 1 else None
        result = get_indicators(goal_code)
    elif command == "data":
        if len(args) < 2:
            result = {"error": "Usage: data <indicator_code> [area_code] [start_year] [end_year]"}
        else:
            area_code = args[2] if len(args) > 2 else None
            start_year = int(args[3]) if len(args) > 3 else None
            end_year = int(args[4]) if len(args) > 4 else None
            result = get_data(args[1], area_code, start_year, end_year)
    elif command == "areas":
        result = get_areas()
    elif command == "series":
        result = get_series()
    elif command == "series_data":
        if len(args) < 2:
            result = {"error": "Usage: series_data <series_code> [area_code] [start_year]"}
        else:
            area_code = args[2] if len(args) > 2 else None
            start_year = int(args[3]) if len(args) > 3 else None
            result = get_series_data(args[1], area_code, start_year)
    elif command == "indicator_info":
        if len(args) < 2:
            result = {"error": "Usage: indicator_info <indicator_code>"}
        else:
            result = get_indicator_overview(args[1])
    else:
        result = {"error": f"Unknown command: {command}. Available: goals, targets, indicators, data, areas, series, series_data, indicator_info"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
