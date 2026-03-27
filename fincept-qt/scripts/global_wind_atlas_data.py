"""
Global Wind Atlas Data Fetcher
Global Wind Atlas (DTU/World Bank): wind speed, power density, capacity factor data for any location.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GLOBAL_WIND_ATLAS_API_KEY', '')
BASE_URL = "https://globalwindatlas.info/api"

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


def get_wind_speed(lat: str, lon: str, height: str) -> Any:
    params = {"lat": lat, "lon": lon, "height": height, "variable": "wind-speed"}
    return _make_request("gwa/wind-speed", params)


def get_power_density(lat: str, lon: str, height: str) -> Any:
    params = {"lat": lat, "lon": lon, "height": height, "variable": "air-density"}
    return _make_request("gwa/power-density", params)


def get_capacity_factor(lat: str, lon: str, turbine_type: str) -> Any:
    params = {"lat": lat, "lon": lon, "turbineType": turbine_type, "variable": "capacity-factor"}
    return _make_request("gwa/capacity-factor", params)


def get_wind_rose(lat: str, lon: str, height: str) -> Any:
    params = {"lat": lat, "lon": lon, "height": height, "variable": "wind-rose"}
    return _make_request("gwa/wind-rose", params)


def get_regional_stats(country: str, admin_level: str) -> Any:
    params = {"country": country, "adminLevel": admin_level}
    return _make_request("gwa/region", params)


def get_resource_data(lat: str, lon: str) -> Any:
    params = {"lat": lat, "lon": lon}
    return _make_request("gwa/resource", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "speed":
        lat = args[1] if len(args) > 1 else "52.5"
        lon = args[2] if len(args) > 2 else "13.4"
        height = args[3] if len(args) > 3 else "100"
        result = get_wind_speed(lat, lon, height)
    elif command == "density":
        lat = args[1] if len(args) > 1 else "52.5"
        lon = args[2] if len(args) > 2 else "13.4"
        height = args[3] if len(args) > 3 else "100"
        result = get_power_density(lat, lon, height)
    elif command == "capacity_factor":
        lat = args[1] if len(args) > 1 else "52.5"
        lon = args[2] if len(args) > 2 else "13.4"
        turbine_type = args[3] if len(args) > 3 else "IEC1"
        result = get_capacity_factor(lat, lon, turbine_type)
    elif command == "wind_rose":
        lat = args[1] if len(args) > 1 else "52.5"
        lon = args[2] if len(args) > 2 else "13.4"
        height = args[3] if len(args) > 3 else "100"
        result = get_wind_rose(lat, lon, height)
    elif command == "regional":
        country = args[1] if len(args) > 1 else "DE"
        admin_level = args[2] if len(args) > 2 else "1"
        result = get_regional_stats(country, admin_level)
    elif command == "resource":
        lat = args[1] if len(args) > 1 else "52.5"
        lon = args[2] if len(args) > 2 else "13.4"
        result = get_resource_data(lat, lon)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
