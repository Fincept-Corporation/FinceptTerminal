"""
MarineTraffic Data Fetcher
MarineTraffic free tier: vessel positions, expected arrivals, port congestion data
via MarineTraffic API (free key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

API_KEY = os.environ.get('MARINETRAFFIC_API_KEY', '')
BASE_URL = "https://services.marinetraffic.com/api"

SERVICE = "MarineTraffic"
KEY_ENV = "MARINETRAFFIC_API_KEY"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # The key is part of every MarineTraffic API path; without it the API answers HTTP 400.
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://www.marinetraffic.com/en/ais-api-services", KEY_ENV)
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["apikey"] = API_KEY
    params["msgtype"] = "extended"
    params["protocol"] = "json"
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        return data
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}


def get_vessel_positions(mmsi: str) -> Any:
    params = {"mmsi": mmsi}
    return _make_request(f"exportvessels/{API_KEY}", params)


def get_expected_arrivals(port_id: str, date: str = None) -> Any:
    params = {"portid": port_id}
    if date:
        params["expectedarrival"] = date
    return _make_request(f"expectedarrivals/{API_KEY}", params)


def get_port_calls(port_id: str, from_date: str = None, to_date: str = None) -> Any:
    params = {"portid": port_id}
    if from_date:
        params["fromdate"] = from_date
    if to_date:
        params["todate"] = to_date
    return _make_request(f"portcalls/{API_KEY}", params)


def get_vessel_details(mmsi: str) -> Any:
    params = {"mmsi": mmsi}
    return _make_request(f"vesseldetails/{API_KEY}", params)


def get_port_info(port_id: str) -> Any:
    params = {"portid": port_id}
    return _make_request(f"portinfo/{API_KEY}", params)


def get_nearby_vessels(lat: float, lon: float, radius: int = 10) -> Any:
    params = {
        "MINLAT": lat - 0.1, "MAXLAT": lat + 0.1,
        "MINLON": lon - 0.1, "MAXLON": lon + 0.1
    }
    return _make_request(f"exportvessels/{API_KEY}", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "positions":
        mmsi = args[1] if len(args) > 1 else "123456789"
        result = get_vessel_positions(mmsi)
    elif command == "arrivals":
        port_id = args[1] if len(args) > 1 else "1"
        date = args[2] if len(args) > 2 else None
        result = get_expected_arrivals(port_id, date)
    elif command == "port_calls":
        port_id = args[1] if len(args) > 1 else "1"
        from_date = args[2] if len(args) > 2 else None
        to_date = args[3] if len(args) > 3 else None
        result = get_port_calls(port_id, from_date, to_date)
    elif command == "vessel":
        mmsi = args[1] if len(args) > 1 else "123456789"
        result = get_vessel_details(mmsi)
    elif command == "port":
        port_id = args[1] if len(args) > 1 else "1"
        result = get_port_info(port_id)
    elif command == "nearby":
        lat = float(args[1]) if len(args) > 1 else 51.5
        lon = float(args[2]) if len(args) > 2 else 0.1
        radius = int(args[3]) if len(args) > 3 else 10
        result = get_nearby_vessels(lat, lon, radius)
    print(json.dumps(result))


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # bad argument (e.g. a non-numeric limit): still answer in JSON
        print(json.dumps({"error": fc.scrub(f"{type(e).__name__}: {e}", API_KEY)}))
