"""
Global Solar Atlas Data Fetcher
Global Solar Atlas (World Bank/Solargis): solar irradiance, PV output potential for any location worldwide.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GLOBAL_SOLAR_ATLAS_API_KEY', '')
BASE_URL = "https://globalsolaratlas.info/api"

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


def get_ghi(lat: str, lon: str) -> Any:
    params = {"lat": lat, "lon": lon, "variable": "GHI"}
    return _make_request("v1/data/point", params)


def get_dni(lat: str, lon: str) -> Any:
    params = {"lat": lat, "lon": lon, "variable": "DNI"}
    return _make_request("v1/data/point", params)


def get_pvout(lat: str, lon: str, system_type: str) -> Any:
    params = {"lat": lat, "lon": lon, "variable": "PVOUT", "systemType": system_type}
    return _make_request("v1/data/point", params)


def get_dif(lat: str, lon: str) -> Any:
    params = {"lat": lat, "lon": lon, "variable": "DIF"}
    return _make_request("v1/data/point", params)


def get_regional_summary(country: str) -> Any:
    params = {"country": country}
    return _make_request("v1/data/country", params)


def get_monthly_data(lat: str, lon: str, variable: str) -> Any:
    params = {"lat": lat, "lon": lon, "variable": variable, "period": "monthly"}
    return _make_request("v1/data/monthly", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "ghi":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        result = get_ghi(lat, lon)
    elif command == "dni":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        result = get_dni(lat, lon)
    elif command == "pvout":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        system_type = args[3] if len(args) > 3 else "fixed"
        result = get_pvout(lat, lon, system_type)
    elif command == "dif":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        result = get_dif(lat, lon)
    elif command == "regional":
        country = args[1] if len(args) > 1 else "FR"
        result = get_regional_summary(country)
    elif command == "monthly":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        variable = args[3] if len(args) > 3 else "GHI"
        result = get_monthly_data(lat, lon, variable)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
