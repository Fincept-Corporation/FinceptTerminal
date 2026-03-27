"""
OpenStreetMap Nominatim Data Fetcher
Global geocoding, reverse geocoding, place search, address lookup
via Nominatim public API (no key, 1 req/sec policy).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://nominatim.openstreetmap.org"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({
    "User-Agent": "FinceptTerminal/4.0 (support@fincept.in)",
    "Accept-Language": "en"
})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
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


def search(query: str, limit: int = 10, country_codes: str = None) -> Any:
    params = {"q": query, "limit": limit, "addressdetails": 1, "extratags": 1}
    if country_codes:
        params["countrycodes"] = country_codes
    return _make_request("search", params)


def reverse(lat: float, lon: float, zoom: int = 18) -> Any:
    params = {"lat": lat, "lon": lon, "zoom": zoom, "addressdetails": 1, "extratags": 1}
    return _make_request("reverse", params)


def lookup(osm_ids: str) -> Any:
    # osm_ids: comma-separated list like "N123456,W654321,R789"
    params = {"osm_ids": osm_ids, "addressdetails": 1}
    return _make_request("lookup", params)


def get_details(place_id: str) -> Any:
    params = {"place_id": place_id, "addressdetails": 1, "hierarchy": 0, "group_hierarchy": 1}
    return _make_request("details", params)


def search_structured(street: str = None, city: str = None,
                       country: str = None, postalcode: str = None) -> Any:
    params = {"addressdetails": 1}
    if street:
        params["street"] = street
    if city:
        params["city"] = city
    if country:
        params["country"] = country
    if postalcode:
        params["postalcode"] = postalcode
    return _make_request("search", params)


def get_status() -> Any:
    try:
        resp = session.get(f"{BASE_URL}/status.php", params={"format": "json"}, timeout=10)
        resp.raise_for_status()
        return resp.json()
    except Exception as e:
        return {"error": f"Status check failed: {str(e)}"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "search":
        query = args[1] if len(args) > 1 else "New York"
        limit = int(args[2]) if len(args) > 2 else 10
        country_codes = args[3] if len(args) > 3 else None
        result = search(query, limit, country_codes)
    elif command == "reverse":
        lat = float(args[1]) if len(args) > 1 else 40.7128
        lon = float(args[2]) if len(args) > 2 else -74.0060
        zoom = int(args[3]) if len(args) > 3 else 18
        result = reverse(lat, lon, zoom)
    elif command == "lookup":
        osm_ids = args[1] if len(args) > 1 else "N240109189"
        result = lookup(osm_ids)
    elif command == "details":
        place_id = args[1] if len(args) > 1 else "240109189"
        result = get_details(place_id)
    elif command == "structured":
        city = args[1] if len(args) > 1 else "London"
        country = args[2] if len(args) > 2 else "GB"
        result = search_structured(city=city, country=country)
    elif command == "status":
        result = get_status()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
