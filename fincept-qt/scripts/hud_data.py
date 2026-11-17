"""
HUD (Housing and Urban Development) Data Fetcher
HUD: Fair Market Rents, income limits, public housing, opportunity zones
via HUD User API (free key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

API_KEY = os.environ.get('HUD_API_KEY', '')
BASE_URL = "https://www.huduser.gov/hudapi/public"

SERVICE = "HUD User"
KEY_ENV = "HUD_API_KEY"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every HUD User API endpoint answers HTTP 403 without an access token.
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://www.huduser.gov/hudapi/public/register", KEY_ENV, "an access token")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {"Authorization": f"Bearer {API_KEY}"}
    try:
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}


def get_fair_market_rents(year: str = "2024", state: str = None, county: str = None) -> Any:
    params = {"year": year}
    if state:
        params["state"] = state
    if county:
        params["county"] = county
    return _make_request("fmr/listFMRsByState", params)


def get_income_limits(year: str = "2024", state: str = None, county: str = None) -> Any:
    params = {"year": year}
    if state:
        params["state"] = state
    if county:
        params["county"] = county
    return _make_request("il/listILsByState", params)


def get_public_housing(state: str = None, city: str = None) -> Any:
    params = {}
    if state:
        params["state"] = state
    if city:
        params["city"] = city
    return _make_request("publichousings", params)


def get_opportunity_zones(state: str = None) -> Any:
    params = {}
    if state:
        params["state"] = state
    return _make_request("opportunityzones", params)


def get_chas_data(year: str = "2019", geography_id: str = None) -> Any:
    params = {"year": year}
    if geography_id:
        params["geoid"] = geography_id
    return _make_request("chas/listCHASbyState", params)


def get_fmr_history(state: str, year_start: str = "2020", year_end: str = "2024") -> Any:
    if not API_KEY:
        return _make_request("")
    results = {}
    start = int(year_start)
    end = int(year_end)
    for year in range(start, end + 1):
        data = get_fair_market_rents(str(year), state=state)
        results[str(year)] = data
    return {"state": state, "year_range": f"{year_start}-{year_end}", "history": results}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "fmr":
        year = args[1] if len(args) > 1 else "2024"
        state = args[2] if len(args) > 2 else None
        county = args[3] if len(args) > 3 else None
        result = get_fair_market_rents(year, state, county)
    elif command == "income_limits":
        year = args[1] if len(args) > 1 else "2024"
        state = args[2] if len(args) > 2 else None
        county = args[3] if len(args) > 3 else None
        result = get_income_limits(year, state, county)
    elif command == "public_housing":
        state = args[1] if len(args) > 1 else None
        city = args[2] if len(args) > 2 else None
        result = get_public_housing(state, city)
    elif command == "opportunity_zones":
        state = args[1] if len(args) > 1 else None
        result = get_opportunity_zones(state)
    elif command == "chas":
        year = args[1] if len(args) > 1 else "2019"
        geography_id = args[2] if len(args) > 2 else None
        result = get_chas_data(year, geography_id)
    elif command == "fmr_history":
        state = args[1] if len(args) > 1 else "CA"
        year_start = args[2] if len(args) > 2 else "2020"
        year_end = args[3] if len(args) > 3 else "2024"
        result = get_fmr_history(state, year_start, year_end)
    print(json.dumps(result))


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # bad argument (e.g. a non-numeric limit): still answer in JSON
        print(json.dumps({"error": fc.scrub(f"{type(e).__name__}: {e}", API_KEY)}))
