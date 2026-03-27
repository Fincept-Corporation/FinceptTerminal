"""
Global Fishing Watch Data Fetcher
Provides vessel tracking, fishing activity hours, port events, and AIS data
for vessels worldwide via the Global Fishing Watch public API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GFW_API_KEY', '')
BASE_URL = "https://gateway.api.globalfishingwatch.org/v3"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {}
    if API_KEY:
        headers["Authorization"] = f"Bearer {API_KEY}"
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


def _check_api_key() -> Optional[Dict]:
    if not API_KEY:
        return {"error": "GFW_API_KEY environment variable not set"}
    return None


def get_vessel_search(query: str) -> Any:
    """Search for vessels by name, MMSI, IMO, or callsign."""
    err = _check_api_key()
    if err:
        return err
    params = {"query": query, "datasets[0]": "public-global-vessel-identity:latest"}
    data = _make_request("vessels/search", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"query": query, "results": data}


def get_vessel_tracks(vessel_id: str, start_date: str, end_date: str) -> Any:
    """Return AIS track points for a vessel within a date range (YYYY-MM-DD)."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "startDate": start_date,
        "endDate": end_date,
        "datasets[0]": "public-global-fishing-tracks:latest"
    }
    data = _make_request(f"vessels/{vessel_id}/tracks", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"vessel_id": vessel_id, "start_date": start_date, "end_date": end_date, "tracks": data}


def get_fishing_activity(lat: float, lon: float, radius: float, start_date: str, end_date: str) -> Any:
    """Return fishing activity hours within a radius (km) of a point for a date range."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "datasets[0]": "public-global-fishing-effort:latest",
        "startDate": start_date,
        "endDate": end_date,
        "center-lat": lat,
        "center-lon": lon,
        "radius": radius
    }
    data = _make_request("4wings/report", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"lat": lat, "lon": lon, "radius_km": radius, "start_date": start_date, "end_date": end_date, "activity": data}


def get_port_visits(port_id: str, start_date: str, end_date: str) -> Any:
    """Return vessel port visit events for a specific port within a date range."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "datasets[0]": "public-global-port-visits-c2-events:latest",
        "startDate": start_date,
        "endDate": end_date,
        "anchorage-id": port_id
    }
    data = _make_request("events", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"port_id": port_id, "start_date": start_date, "end_date": end_date, "visits": data}


def get_events(vessel_id: str, event_type: str = "fishing") -> Any:
    """Return events (fishing, port visits, encounters, loitering) for a vessel."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "vessels[0]": vessel_id,
        "datasets[0]": f"public-global-{event_type}-events:latest"
    }
    data = _make_request("events", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"vessel_id": vessel_id, "event_type": event_type, "events": data}


def get_datasets() -> Any:
    """Return all available datasets in the Global Fishing Watch API."""
    err = _check_api_key()
    if err:
        return err
    data = _make_request("datasets")
    if isinstance(data, dict) and "error" in data:
        return data
    return {"datasets": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "search":
        query = args[1] if len(args) > 1 else ""
        if not query:
            result = {"error": "query required"}
        else:
            result = get_vessel_search(query)
    elif command == "tracks":
        if len(args) < 4:
            result = {"error": "vessel_id, start_date, end_date required"}
        else:
            result = get_vessel_tracks(args[1], args[2], args[3])
    elif command == "fishing":
        if len(args) < 6:
            result = {"error": "lat, lon, radius, start_date, end_date required"}
        else:
            result = get_fishing_activity(float(args[1]), float(args[2]), float(args[3]), args[4], args[5])
    elif command == "ports":
        if len(args) < 4:
            result = {"error": "port_id, start_date, end_date required"}
        else:
            result = get_port_visits(args[1], args[2], args[3])
    elif command == "events":
        if len(args) < 2:
            result = {"error": "vessel_id required"}
        else:
            event_type = args[2] if len(args) > 2 else "fishing"
            result = get_events(args[1], event_type)
    elif command == "datasets":
        result = get_datasets()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
