"""
World Air Quality Index Data Fetcher
AQI for 11000+ stations globally, PM2.5/PM10/NO2/CO/SO2/O3.
Requires free API key via WAQI_TOKEN env var.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WAQI_TOKEN', '')
BASE_URL = "https://api.waqi.info"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params['token'] = API_KEY if API_KEY else 'demo'
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


def _unwrap(data: Any, label: str = "data") -> Any:
    """Unwrap WAQI API response envelope."""
    if isinstance(data, dict):
        if data.get("status") == "ok":
            return {label: data.get("data"), "status": "ok"}
        elif data.get("status") == "error":
            return {"error": data.get("data", "Unknown WAQI error")}
    return data


def get_city_aqi(city: str) -> Any:
    """Get real-time AQI and pollutant breakdown for a city name.
    city: city name (e.g. london, beijing, new-york) or @station_id.
    """
    data = _make_request(f"feed/{city}/")
    result = _unwrap(data, "aqi_data")
    if isinstance(result, dict) and "aqi_data" in result:
        d = result["aqi_data"]
        return {
            "city": d.get("city", {}).get("name") if isinstance(d.get("city"), dict) else d.get("city"),
            "aqi": d.get("aqi"),
            "time": d.get("time", {}).get("s") if isinstance(d.get("time"), dict) else d.get("time"),
            "dominantpol": d.get("dominantpol"),
            "iaqi": d.get("iaqi"),
            "forecast": d.get("forecast"),
            "attributions": d.get("attributions"),
        }
    return result


def get_station_aqi(station_id: int) -> Any:
    """Get AQI data for a specific station by numeric ID."""
    data = _make_request(f"feed/@{station_id}/")
    return _unwrap(data, "station_data")


def get_nearby_stations(lat: float, lng: float) -> Any:
    """Get AQI readings from stations near a lat/lng coordinate."""
    data = _make_request(f"feed/geo:{lat};{lng}/")
    return _unwrap(data, "nearby_data")


def search_stations(keyword: str) -> Any:
    """Search for monitoring stations by keyword/city name."""
    params = {"keyword": keyword}
    data = _make_request("search/", params=params)
    if isinstance(data, dict) and data.get("status") == "ok":
        stations = data.get("data", [])
        return {"keyword": keyword, "stations": stations, "count": len(stations)}
    return _unwrap(data, "search_results")


def get_map_data(lat1: float, lng1: float, lat2: float, lng2: float) -> Any:
    """Get AQI readings for all stations within a geographic bounding box.
    lat1,lng1: south-west corner; lat2,lng2: north-east corner.
    """
    params = {"latlng": f"{lat1},{lng1},{lat2},{lng2}"}
    data = _make_request("map/bounds/", params=params)
    if isinstance(data, dict) and data.get("status") == "ok":
        stations = data.get("data", [])
        return {
            "bounds": {"sw": [lat1, lng1], "ne": [lat2, lng2]},
            "stations": stations,
            "count": len(stations),
        }
    return _unwrap(data, "map_data")


def get_historical_forecast(city: str) -> Any:
    """Get AQI forecast and historical trend embedded in station data."""
    data = _make_request(f"feed/{city}/")
    result = _unwrap(data, "aqi_data")
    if isinstance(result, dict) and "aqi_data" in result:
        d = result["aqi_data"]
        return {
            "city": d.get("city", {}).get("name") if isinstance(d.get("city"), dict) else d.get("city"),
            "current_aqi": d.get("aqi"),
            "forecast": d.get("forecast", {}),
            "time": d.get("time", {}).get("s") if isinstance(d.get("time"), dict) else d.get("time"),
        }
    return result


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: city, station, nearby, search, map, historical. Set WAQI_TOKEN env var."}))
        return

    command = args[0]

    if command == "city":
        if len(args) < 2:
            result = {"error": "Usage: city <city_name_or_@station_id>"}
        else:
            result = get_city_aqi(args[1])
    elif command == "station":
        if len(args) < 2:
            result = {"error": "Usage: station <station_id>"}
        else:
            try:
                result = get_station_aqi(int(args[1]))
            except ValueError:
                result = {"error": f"station_id must be numeric, got: {args[1]}"}
    elif command == "nearby":
        if len(args) < 3:
            result = {"error": "Usage: nearby <lat> <lng>"}
        else:
            result = get_nearby_stations(float(args[1]), float(args[2]))
    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: search <keyword>"}
        else:
            result = search_stations(" ".join(args[1:]))
    elif command == "map":
        if len(args) < 5:
            result = {"error": "Usage: map <lat1> <lng1> <lat2> <lng2> (bounding box SW to NE)"}
        else:
            result = get_map_data(float(args[1]), float(args[2]), float(args[3]), float(args[4]))
    elif command == "historical":
        if len(args) < 2:
            result = {"error": "Usage: historical <city_name>"}
        else:
            result = get_historical_forecast(args[1])
    else:
        result = {"error": f"Unknown command: {command}. Available: city, station, nearby, search, map, historical"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
