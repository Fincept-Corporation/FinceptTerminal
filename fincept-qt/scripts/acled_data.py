"""
ACLED (Armed Conflict Location & Event Data) Fetcher
Provides political violence events, protest data, conflict fatalities,
actor information, and trend analysis for countries globally.
Needs a free myACLED account (ACLED_EMAIL + ACLED_PASSWORD; ACLED no longer issues API keys).
"""
import sys
import json
import os
import requests
import fincept_connector as fc
from typing import Dict, Any, Optional, List

# ACLED retired its key+email API (api.acleddata.com no longer resolves) in 2025. The current API
# authenticates with the myACLED account itself: an OAuth password grant at /oauth/token returns
# a bearer token (valid 24 h) for https://acleddata.com/api/acled/read.
ACLED_EMAIL = os.environ.get('ACLED_EMAIL', '')
ACLED_PASSWORD = os.environ.get('ACLED_PASSWORD', '')
TOKEN_URL = "https://acleddata.com/oauth/token"
BASE_URL = "https://acleddata.com/api/acled/read"
SERVICE = "ACLED"

session = fc.http_session()
_token: Optional[str] = None


def _get_token() -> Any:
    """Bearer token for this process, or {"error": ...}."""
    global _token
    if _token:
        return _token
    form = {"username": ACLED_EMAIL, "password": ACLED_PASSWORD, "grant_type": "password", "client_id": "acled"}
    try:
        response = session.post(TOKEN_URL, data=form, timeout=30)
        if response.status_code in (400, 401):
            return {"error": "ACLED rejected the account email/password (ACLED_EMAIL / ACLED_PASSWORD) - "
                             "check them in Settings > Credentials"}
        response.raise_for_status()
        _token = response.json().get("access_token")
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, secret=ACLED_PASSWORD)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    return _token or {"error": "ACLED did not return an access token"}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    token = _get_token()
    if isinstance(token, dict):
        return token
    params = dict(params or {}, _format="json")
    try:
        response = session.get(url, params=params, headers={"Authorization": f"Bearer {token}"}, timeout=30)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, secret=token)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    if not isinstance(data, dict):
        return {"error": f"Unexpected {SERVICE} response"}
    if data.get("success") is False or (data.get("error") and not data.get("data")):
        err = data.get("error")
        msg = err.get("message") if isinstance(err, dict) else err
        return {"error": f"{SERVICE}: {fc.scrub(msg or data.get('message') or 'request failed', token)}"}
    return data


def _check_credentials() -> Optional[Dict]:
    if not ACLED_EMAIL or not ACLED_PASSWORD:
        return {"error": "ACLED needs a free myACLED account: register at https://acleddata.com/register/ "
                         "and save its email as ACLED_EMAIL and its password as ACLED_PASSWORD in "
                         "Settings > Credentials (ACLED no longer issues API keys)"}
    return None


def _base_params() -> Dict:
    return {}


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
    # ACLED filter syntax: event_date=<from>|<to> with event_date_where=BETWEEN.
    if start_date and end_date:
        params["event_date"] = f"{start_date}|{end_date}"
        params["event_date_where"] = "BETWEEN"
    elif start_date:
        params["event_date"] = start_date
        params["event_date_where"] = ">="
    elif end_date:
        params["event_date"] = end_date
        params["event_date_where"] = "<="
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
    params["fields"] = "fatalities|event_date|event_type"
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
    params["fields"] = "event_date|event_type|fatalities"
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
    params["fields"] = "actor1|actor2|inter1|inter2"
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
    numeric = {"events": 5, "fatalities": 2, "trends": 2, "actors": 2}.get(command)
    if numeric and len(args) > numeric and not args[numeric].isdigit():
        print(json.dumps({"error": f"{'limit' if command == 'events' else 'year'} must be a number, "
                                   f"got '{args[numeric]}'"}))
        return
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
