"""
OpenStreetMap Overpass API Data Fetcher
Query OSM features — banks, ATMs, hospitals, airports, ports,
industrial areas globally via Overpass API (no key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

BASE_URL = "https://overpass-api.de/api/interpreter"
SERVICE = "Overpass API (overpass-api.de)"

session = fc.http_session()
session.headers.update({"User-Agent": "FinceptTerminal/4.0 (support@fincept.in)"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=(10, 60))
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}


def _run_overpass(query: str) -> Any:
    # The public instance answers 502/504 "server is currently at capacity" and 429 when its query
    # workers are busy; fc.request_error turns those into a "down or overloaded / rate-limiting" message.
    try:
        response = session.post(BASE_URL, data={"data": query}, timeout=(10, 75))
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON (check the Overpass QL query)"}


def _around_query(amenity_tag: str, lat: float, lon: float, radius: int = 1000) -> str:
    return (
        f'[out:json][timeout:25];'
        f'(node["amenity"="{amenity_tag}"](around:{radius},{lat},{lon});'
        f' way["amenity"="{amenity_tag}"](around:{radius},{lat},{lon}););'
        f'out center tags;'
    )


def get_banks(lat: float, lon: float, radius: int = 1000) -> Any:
    query = _around_query("bank", lat, lon, radius)
    data = _run_overpass(query)
    if "error" in data:
        return data
    elements = data.get("elements", [])
    features = []
    for el in elements:
        center = el.get("center", {}) or {"lat": el.get("lat"), "lon": el.get("lon")}
        tags = el.get("tags", {})
        features.append({"id": el.get("id"), "type": el.get("type"),
                          "lat": center.get("lat"), "lon": center.get("lon"),
                          "name": tags.get("name", "Unknown"),
                          "operator": tags.get("operator", ""),
                          "brand": tags.get("brand", ""),
                          "addr": tags.get("addr:full", tags.get("addr:street", ""))})
    return {"query_center": {"lat": lat, "lon": lon}, "radius_m": radius,
            "count": len(features), "banks": features}


def get_atms(lat: float, lon: float, radius: int = 500) -> Any:
    query = _around_query("atm", lat, lon, radius)
    data = _run_overpass(query)
    if "error" in data:
        return data
    elements = data.get("elements", [])
    features = []
    for el in elements:
        center = el.get("center", {}) or {"lat": el.get("lat"), "lon": el.get("lon")}
        tags = el.get("tags", {})
        features.append({"id": el.get("id"), "lat": center.get("lat"), "lon": center.get("lon"),
                          "operator": tags.get("operator", ""), "brand": tags.get("brand", ""),
                          "network": tags.get("network", ""), "fee": tags.get("fee", "")})
    return {"query_center": {"lat": lat, "lon": lon}, "radius_m": radius,
            "count": len(features), "atms": features}


def get_hospitals(lat: float, lon: float, radius: int = 5000) -> Any:
    query = (
        f'[out:json][timeout:25];'
        f'(node["amenity"~"hospital|clinic"](around:{radius},{lat},{lon});'
        f' way["amenity"~"hospital|clinic"](around:{radius},{lat},{lon}););'
        f'out center tags;'
    )
    data = _run_overpass(query)
    if "error" in data:
        return data
    elements = data.get("elements", [])
    features = []
    for el in elements:
        center = el.get("center", {}) or {"lat": el.get("lat"), "lon": el.get("lon")}
        tags = el.get("tags", {})
        features.append({"id": el.get("id"), "lat": center.get("lat"), "lon": center.get("lon"),
                          "name": tags.get("name", "Unknown"), "amenity": tags.get("amenity"),
                          "beds": tags.get("beds", ""), "emergency": tags.get("emergency", "")})
    return {"query_center": {"lat": lat, "lon": lon}, "radius_m": radius,
            "count": len(features), "hospitals": features}


def get_airports(country_code: str = "US") -> Any:
    query = (
        f'[out:json][timeout:60];'
        f'area["ISO3166-1"="{country_code.upper()}"]->.a;'
        f'(node["aeroway"="aerodrome"](area.a);'
        f' way["aeroway"="aerodrome"](area.a);'
        f' relation["aeroway"="aerodrome"](area.a););'
        f'out center tags;'
    )
    data = _run_overpass(query)
    if "error" in data:
        return data
    elements = data.get("elements", [])
    airports = []
    for el in elements:
        center = el.get("center", {}) or {"lat": el.get("lat"), "lon": el.get("lon")}
        tags = el.get("tags", {})
        airports.append({"id": el.get("id"), "lat": center.get("lat"), "lon": center.get("lon"),
                          "name": tags.get("name", "Unknown"), "iata": tags.get("iata", ""),
                          "icao": tags.get("icao", ""), "type": tags.get("aerodrome:type", "")})
    return {"country": country_code, "count": len(airports), "airports": airports}


def get_ports(lat: float, lon: float, radius: int = 50000) -> Any:
    query = (
        f'[out:json][timeout:30];'
        f'(node["harbour"="yes"](around:{radius},{lat},{lon});'
        f' way["harbour"="yes"](around:{radius},{lat},{lon});'
        f' node["seamark:type"~"harbour|port"](around:{radius},{lat},{lon}););'
        f'out center tags;'
    )
    data = _run_overpass(query)
    if "error" in data:
        return data
    elements = data.get("elements", [])
    ports = []
    for el in elements:
        center = el.get("center", {}) or {"lat": el.get("lat"), "lon": el.get("lon")}
        tags = el.get("tags", {})
        ports.append({"id": el.get("id"), "lat": center.get("lat"), "lon": center.get("lon"),
                       "name": tags.get("name", "Unknown"), "operator": tags.get("operator", ""),
                       "seamark_type": tags.get("seamark:type", "")})
    return {"query_center": {"lat": lat, "lon": lon}, "radius_m": radius,
            "count": len(ports), "ports": ports}


def run_query(overpass_ql: str) -> Any:
    return _run_overpass(overpass_ql)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    try:
        result = _dispatch(command, args, result)
    except ValueError:
        result = {"error": "lat / lon must be numbers and radius a whole number of metres"}
    print(json.dumps(result))


def _dispatch(command: str, args: List[str], result: Dict) -> Any:
    if command == "banks":
        lat = float(args[1]) if len(args) > 1 else 40.7128
        lon = float(args[2]) if len(args) > 2 else -74.0060
        radius = int(args[3]) if len(args) > 3 else 1000
        result = get_banks(lat, lon, radius)
    elif command == "atms":
        lat = float(args[1]) if len(args) > 1 else 40.7128
        lon = float(args[2]) if len(args) > 2 else -74.0060
        radius = int(args[3]) if len(args) > 3 else 500
        result = get_atms(lat, lon, radius)
    elif command == "hospitals":
        lat = float(args[1]) if len(args) > 1 else 40.7128
        lon = float(args[2]) if len(args) > 2 else -74.0060
        radius = int(args[3]) if len(args) > 3 else 5000
        result = get_hospitals(lat, lon, radius)
    elif command == "airports":
        country_code = args[1] if len(args) > 1 else "US"
        result = get_airports(country_code)
    elif command == "ports":
        lat = float(args[1]) if len(args) > 1 else 51.5074
        lon = float(args[2]) if len(args) > 2 else 0.1278
        radius = int(args[3]) if len(args) > 3 else 50000
        result = get_ports(lat, lon, radius)
    elif command == "query":
        ql = args[1] if len(args) > 1 else '[out:json];node["amenity"="bank"](around:500,51.5,0.1);out;'
        result = run_query(ql)
    return result


if __name__ == "__main__":
    main()
