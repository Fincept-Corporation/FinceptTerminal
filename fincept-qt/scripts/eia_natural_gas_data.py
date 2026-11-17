"""
EIA Natural Gas Data Fetcher
EIA Natural Gas: Henry Hub prices, storage levels, production, consumption, LNG exports.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('EIA_API_KEY', '')
BASE_URL = "https://api.eia.gov/v2/natural-gas"
SERVICE = "EIA"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every EIA v2 endpoint answers HTTP 403 without a key: say so before any network call.
    if not API_KEY:
        return fc.missing_key("The EIA Open Data API", "https://www.eia.gov/opendata/register.php",
                              "EIA_API_KEY", what="a free API key")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["api_key"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, "EIA_API_KEY", API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}


def get_henry_hub_price(start_period: str, end_period: str) -> Any:
    params = {"facets[series][]": "RNGWHHD", "start": start_period, "end": end_period, "frequency": "daily"}
    return _make_request("pri/sum/data", params)


def get_storage_levels(region: str, start_period: str, end_period: str) -> Any:
    params = {"facets[region][]": region, "start": start_period, "end": end_period, "frequency": "weekly"}
    return _make_request("stor/wkly/data", params)


def get_production(state: str, start_period: str, end_period: str) -> Any:
    params = {"facets[state][]": state, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("prod/sum/data", params)


def get_consumption(sector: str, start_period: str, end_period: str) -> Any:
    params = {"facets[sector][]": sector, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("cons/sum/data", params)


def get_lng_exports(terminal: str, start_period: str, end_period: str) -> Any:
    params = {"facets[terminal][]": terminal, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("move/lngex/data", params)


def get_spot_prices(location: str, start_period: str, end_period: str) -> Any:
    params = {"facets[series][]": location, "start": start_period, "end": end_period, "frequency": "daily"}
    return _make_request("pri/sum/data", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "henry_hub":
        start_period = args[1] if len(args) > 1 else "2024-01-01"
        end_period = args[2] if len(args) > 2 else "2024-03-31"
        result = get_henry_hub_price(start_period, end_period)
    elif command == "storage":
        region = args[1] if len(args) > 1 else "national"
        start_period = args[2] if len(args) > 2 else "2024-01-01"
        end_period = args[3] if len(args) > 3 else "2024-03-31"
        result = get_storage_levels(region, start_period, end_period)
    elif command == "production":
        state = args[1] if len(args) > 1 else "TX"
        start_period = args[2] if len(args) > 2 else "2024-01"
        end_period = args[3] if len(args) > 3 else "2024-03"
        result = get_production(state, start_period, end_period)
    elif command == "consumption":
        sector = args[1] if len(args) > 1 else "residential"
        start_period = args[2] if len(args) > 2 else "2024-01"
        end_period = args[3] if len(args) > 3 else "2024-03"
        result = get_consumption(sector, start_period, end_period)
    elif command == "lng":
        terminal = args[1] if len(args) > 1 else "Sabine Pass"
        start_period = args[2] if len(args) > 2 else "2024-01"
        end_period = args[3] if len(args) > 3 else "2024-03"
        result = get_lng_exports(terminal, start_period, end_period)
    elif command == "spot":
        location = args[1] if len(args) > 1 else "RNGWHHD"
        start_period = args[2] if len(args) > 2 else "2024-01-01"
        end_period = args[3] if len(args) > 3 else "2024-03-31"
        result = get_spot_prices(location, start_period, end_period)
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
