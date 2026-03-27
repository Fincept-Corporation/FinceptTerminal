"""
AISStream Data Fetcher
Global real-time AIS vessel positions, vessel info, port calls
via AISStream REST API (free key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('AISSTREAM_API_KEY', '')
BASE_URL = "https://api.aisstream.io/v0"

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


def _make_post_request(endpoint: str, payload: Dict) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {"Content-Type": "application/json"}
    if API_KEY:
        headers["X-API-Key"] = API_KEY
    try:
        response = session.post(url, json=payload, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_vessels_in_area(lat1: float, lon1: float, lat2: float, lon2: float) -> Any:
    payload = {
        "BoundingBoxes": [[[lat1, lon1], [lat2, lon2]]]
    }
    return _make_post_request("vessels/area", payload)


def get_vessel_by_mmsi(mmsi: str) -> Any:
    return _make_request(f"vessels/{mmsi}")


def get_vessel_track(mmsi: str, start_date: str = None, end_date: str = None) -> Any:
    params = {}
    if start_date:
        params["start_date"] = start_date
    if end_date:
        params["end_date"] = end_date
    return _make_request(f"vessels/{mmsi}/track", params)


def get_port_calls(port_name: str = None, start_date: str = None, end_date: str = None) -> Any:
    params = {}
    if port_name:
        params["port_name"] = port_name
    if start_date:
        params["start_date"] = start_date
    if end_date:
        params["end_date"] = end_date
    return _make_request("port-calls", params)


def get_fleet(vessel_type: str = None, limit: int = 50) -> Any:
    params = {"limit": limit}
    if vessel_type:
        params["vessel_type"] = vessel_type
    return _make_request("vessels", params)


def search_vessel(name: str) -> Any:
    params = {"name": name}
    return _make_request("vessels/search", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "area":
        lat1 = float(args[1]) if len(args) > 1 else 51.0
        lon1 = float(args[2]) if len(args) > 2 else -1.0
        lat2 = float(args[3]) if len(args) > 3 else 52.0
        lon2 = float(args[4]) if len(args) > 4 else 1.0
        result = get_vessels_in_area(lat1, lon1, lat2, lon2)
    elif command == "vessel":
        mmsi = args[1] if len(args) > 1 else "123456789"
        result = get_vessel_by_mmsi(mmsi)
    elif command == "track":
        mmsi = args[1] if len(args) > 1 else "123456789"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_vessel_track(mmsi, start_date, end_date)
    elif command == "port_calls":
        port_name = args[1] if len(args) > 1 else None
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_port_calls(port_name, start_date, end_date)
    elif command == "fleet":
        vessel_type = args[1] if len(args) > 1 else None
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_fleet(vessel_type, limit)
    elif command == "search":
        name = args[1] if len(args) > 1 else "MSC"
        result = search_vessel(name)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
