"""
ACLED (Armed Conflict Location & Event Data) Fetcher
Provides political violence events, protest data, conflict fatalities,
actor information, and trend analysis for countries globally.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ACLED_API_KEY', '')
ACLED_EMAIL = os.environ.get('ACLED_EMAIL', '')
BASE_URL = "https://api.acleddata.com/acled/read"

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


def _check_credentials() -> Optional[Dict]:
    if not API_KEY or not ACLED_EMAIL:
        return {"error": "ACLED_API_KEY and ACLED_EMAIL environment variables required"}
    return None


def _base_params() -> Dict:
    return {"key": API_KEY, "email": ACLED_EMAIL}


def get_events(country: str, event_type: str = None, start_date: str = None, end_date: str = None, limit: int = 100) -> Any:
    """Return conflict/protest events for a country within a date range."""
    err = _check_credentials()
    if err:
        return err
    params = _base_params()
    params["country"] = country
    params["limit"] = limit
    if event_type:
        params["event_type"] = event_type
    if start_date:
        params["event_date"] = start_date
        params["event_date_where"] = "BETWEEN"
    if end_date:
        params["event_date2"] = end_date
    data = _make_request(BASE_URL, params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "event_type": event_type, "start_date": start_date,
            "end_date": end_date, "events": data.get("data", []), "count": data.get("count", 0)}


def get_fatalities(country: str, year: int) -> Any:
    """Return total fatalities from conflict events in a country for a given year."""
    err = _check_credentials()
    if err:
        return err
    params = _base_params()
    params["country"] = country
    params["year"] = year
    params["fields"] = "fatalities,event_date,event_type"
    params["limit"] = 500
    data = _make_request(BASE_URL, params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    events = data.get("data", [])
    total_fatalities = sum(int(e.get("fatalities", 0)) for e in events if e.get("fatalities"))
    return {"country": country, "year": year, "total_fatalities": total_fatalities,
            "event_count": len(events), "events": events}


def get_trends(country: str, year: int) -> Any:
    """Return monthly event count and fatality trends for a country."""
    err = _check_credentials()
    if err:
        return err
    params = _base_params()
    params["country"] = country
    params["year"] = year
    params["fields"] = "event_date,event_type,fatalities"
    params["limit"] = 1000
    data = _make_request(BASE_URL, params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    events = data.get("data", [])
    monthly: Dict[str, Dict] = {}
    for event in events:
        date_str = event.get("event_date", "")
        if date_str and len(date_str) >= 7:
            month_key = date_str[:7]
            if month_key not in monthly:
                monthly[month_key] = {"events": 0, "fatalities": 0}
            monthly[month_key]["events"] += 1
            monthly[month_key]["fatalities"] += int(event.get("fatalities", 0))
    return {"country": country, "year": year, "monthly_trends": monthly}


def get_event_types() -> Any:
    """Return all ACLED event type categories."""
    event_types = [
        "Battles", "Violence against civilians", "Explosions/Remote violence",
        "Protests", "Riots", "Strategic developments"
    ]
    return {"event_types": event_types, "count": len(event_types)}


def get_actors(country: str, year: int) -> Any:
    """Return conflict actors active in a country for a given year."""
    err = _check_credentials()
    if err:
        return err
    params = _base_params()
    params["country"] = country
    params["year"] = year
    params["fields"] = "actor1,actor2,inter1,inter2"
    params["limit"] = 500
    data = _make_request(BASE_URL, params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    events = data.get("data", [])
    actors: set = set()
    for event in events:
        if event.get("actor1"):
            actors.add(event["actor1"])
        if event.get("actor2"):
            actors.add(event["actor2"])
    return {"country": country, "year": year, "actors": sorted(list(actors)), "count": len(actors)}


def get_regions() -> Any:
    """Return all geographic regions covered by ACLED."""
    regions = [
        "Western Africa", "Middle Africa", "Eastern Africa", "Southern Africa",
        "Northern Africa", "South Asia", "Southeast Asia", "East Asia",
        "Middle East", "Central Asia", "Caucasus and Central Asia",
        "Europe", "Central America", "South America", "North America",
        "Caribbean", "Oceania"
    ]
    return {"regions": regions, "count": len(regions)}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "events":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            event_type = args[2] if len(args) > 2 else None
            start_date = args[3] if len(args) > 3 else None
            end_date = args[4] if len(args) > 4 else None
            limit = int(args[5]) if len(args) > 5 else 100
            result = get_events(country, event_type, start_date, end_date, limit)
    elif command == "fatalities":
        if len(args) < 3:
            result = {"error": "country and year required"}
        else:
            result = get_fatalities(args[1], int(args[2]))
    elif command == "trends":
        if len(args) < 3:
            result = {"error": "country and year required"}
        else:
            result = get_trends(args[1], int(args[2]))
    elif command == "event_types":
        result = get_event_types()
    elif command == "actors":
        if len(args) < 3:
            result = {"error": "country and year required"}
        else:
            result = get_actors(args[1], int(args[2]))
    elif command == "regions":
        result = get_regions()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
