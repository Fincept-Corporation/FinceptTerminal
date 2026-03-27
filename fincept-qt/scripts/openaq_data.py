"""
OpenAQ Data Fetcher
Air quality data from 30,000+ stations in 100+ countries — PM2.5, PM10, NO2, CO, SO2, O3
via OpenAQ API v3 (free key required via OPENAQ_API_KEY).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OPENAQ_API_KEY', '')
BASE_URL = "https://api.openaq.org/v3"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {}
    if API_KEY:
        headers["X-API-Key"] = API_KEY
    try:
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_locations(country: str = None, city: str = None, limit: int = 50) -> Any:
    params = {"limit": limit, "page": 1}
    if country:
        params["countries_id"] = country
    if city:
        params["city"] = city
    data = _make_request("locations", params)
    return {"filter": {"country": country, "city": city}, "limit": limit, "data": data}


def get_measurements(location_id: int = None, parameter: str = "pm25",
                       date_from: str = None, date_to: str = None, limit: int = 100) -> Any:
    params = {"limit": limit}
    if location_id:
        params["locations_id"] = location_id
    if parameter:
        params["parameters_id"] = parameter
    if date_from:
        params["date_from"] = date_from
    if date_to:
        params["date_to"] = date_to
    data = _make_request("measurements", params)
    return {"location_id": location_id, "parameter": parameter,
            "date_from": date_from, "date_to": date_to, "data": data}


def get_countries() -> Any:
    params = {"limit": 200}
    data = _make_request("countries", params)
    return {"data": data}


def get_parameters() -> Any:
    data = _make_request("parameters")
    return {"data": data}


def get_cities(country: str = None) -> Any:
    params = {"limit": 200}
    if country:
        params["countries_id"] = country
    data = _make_request("locations", params)
    # Extract unique cities from locations
    if isinstance(data, dict) and "results" in data:
        cities = list(set(loc.get("city", "") for loc in data["results"] if loc.get("city")))
        return {"country": country, "count": len(cities), "cities": sorted(cities)}
    return data


def get_latest(location_id: int) -> Any:
    data = _make_request(f"locations/{location_id}/latest")
    return {"location_id": location_id, "data": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "locations":
        country = args[1] if len(args) > 1 else None
        city = args[2] if len(args) > 2 else None
        limit = int(args[3]) if len(args) > 3 else 50
        result = get_locations(country, city, limit)
    elif command == "measurements":
        location_id = int(args[1]) if len(args) > 1 else None
        parameter = args[2] if len(args) > 2 else "pm25"
        date_from = args[3] if len(args) > 3 else None
        date_to = args[4] if len(args) > 4 else None
        limit = int(args[5]) if len(args) > 5 else 100
        result = get_measurements(location_id, parameter, date_from, date_to, limit)
    elif command == "countries":
        result = get_countries()
    elif command == "parameters":
        result = get_parameters()
    elif command == "cities":
        country = args[1] if len(args) > 1 else None
        result = get_cities(country)
    elif command == "latest":
        location_id = int(args[1]) if len(args) > 1 else 1
        result = get_latest(location_id)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
