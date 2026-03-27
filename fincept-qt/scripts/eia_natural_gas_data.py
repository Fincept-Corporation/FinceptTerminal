"""
EIA Natural Gas Data Fetcher
EIA Natural Gas: Henry Hub prices, storage levels, production, consumption, LNG exports.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EIA_API_KEY', '')
BASE_URL = "https://api.eia.gov/v2/natural-gas"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["api_key"] = API_KEY
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
    print(json.dumps(result))


if __name__ == "__main__":
    main()
