"""
Global Forest Watch Data Fetcher
Deforestation alerts, tree cover loss, forest carbon stock, protected areas,
and fire data via the Global Forest Watch Data API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GFW_API_KEY', '')
BASE_URL = "https://data-api.globalforestwatch.org/v0"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {}
        if API_KEY:
            headers["x-api-key"] = API_KEY
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_tree_cover_loss(country: str = None, year: str = None) -> Any:
    params = {"sql": "SELECT * FROM umd_tree_cover_loss LIMIT 100"}
    if country:
        params["geostore_origin"] = "gadm"
        params["iso"] = country
    if year:
        params["sql"] = f"SELECT * FROM umd_tree_cover_loss WHERE umd_tree_cover_loss__year = {year} LIMIT 100"
    return _make_request("query", params)


def get_deforestation_alerts(country: str = None, start_date: str = None, end_date: str = None) -> Any:
    params = {"sql": "SELECT * FROM umd_glad_landsat_alerts LIMIT 100"}
    if country:
        params["iso"] = country
    if start_date and end_date:
        params["sql"] = (
            f"SELECT * FROM umd_glad_landsat_alerts "
            f"WHERE umd_glad_landsat_alerts__date >= '{start_date}' "
            f"AND umd_glad_landsat_alerts__date <= '{end_date}' LIMIT 100"
        )
    return _make_request("query", params)


def get_forest_carbon(country: str = None, year: str = None) -> Any:
    params = {"sql": "SELECT * FROM whrc_aboveground_co2_emissions__Mg LIMIT 100"}
    if country:
        params["iso"] = country
    return _make_request("query", params)


def get_protected_areas(country: str = None) -> Any:
    params = {"sql": "SELECT * FROM wdpa_protected_areas LIMIT 100"}
    if country:
        params["iso"] = country
    return _make_request("query", params)


def get_fires_data(country: str = None, start_date: str = None, end_date: str = None) -> Any:
    params = {"sql": "SELECT * FROM nasa_viirs_fire_alerts LIMIT 100"}
    if country:
        params["iso"] = country
    if start_date and end_date:
        params["sql"] = (
            f"SELECT * FROM nasa_viirs_fire_alerts "
            f"WHERE alert__date >= '{start_date}' "
            f"AND alert__date <= '{end_date}' LIMIT 100"
        )
    return _make_request("query", params)


def get_countries() -> Any:
    return _make_request("country")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "cover_loss":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_tree_cover_loss(country, year)
    elif command == "alerts":
        country = args[1] if len(args) > 1 else None
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_deforestation_alerts(country, start_date, end_date)
    elif command == "carbon":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_forest_carbon(country, year)
    elif command == "protected":
        country = args[1] if len(args) > 1 else None
        result = get_protected_areas(country)
    elif command == "fires":
        country = args[1] if len(args) > 1 else None
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_fires_data(country, start_date, end_date)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
