"""
EIA Electricity Data Fetcher
EIA Electricity: US generation by source, grid demand, net imports, retail prices by state.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EIA_API_KEY', '')
BASE_URL = "https://api.eia.gov/v2/electricity"

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


def get_generation(source: str, state: str, start_period: str, end_period: str) -> Any:
    params = {"facets[fueltypeid][]": source, "facets[stateid][]": state, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("electric-power-operational-data/data", params)


def get_retail_prices(sector: str, state: str, start_period: str, end_period: str) -> Any:
    params = {"facets[sectorName][]": sector, "facets[stateid][]": state, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("retail-sales/data", params)


def get_demand(region: str, start_period: str, end_period: str) -> Any:
    params = {"facets[respondent][]": region, "start": start_period, "end": end_period, "frequency": "hourly"}
    return _make_request("rto/region-data/data", params)


def get_capacity(source: str, state: str, year: str) -> Any:
    params = {"facets[energy_source_code][]": source, "facets[stateid][]": state, "start": year, "end": year, "frequency": "annual"}
    return _make_request("electric-power-operational-data/data", params)


def get_net_generation_mix(state: str, year: str) -> Any:
    params = {"facets[stateid][]": state, "start": year, "end": year, "frequency": "annual"}
    return _make_request("electric-power-operational-data/data", params)


def get_utility_data(utility_id: str, start_period: str, end_period: str) -> Any:
    params = {"facets[utilityid][]": utility_id, "start": start_period, "end": end_period, "frequency": "monthly"}
    return _make_request("retail-sales/data", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "generation":
        source = args[1] if len(args) > 1 else "SUN"
        state = args[2] if len(args) > 2 else "CA"
        start_period = args[3] if len(args) > 3 else "2024-01"
        end_period = args[4] if len(args) > 4 else "2024-03"
        result = get_generation(source, state, start_period, end_period)
    elif command == "prices":
        sector = args[1] if len(args) > 1 else "residential"
        state = args[2] if len(args) > 2 else "CA"
        start_period = args[3] if len(args) > 3 else "2024-01"
        end_period = args[4] if len(args) > 4 else "2024-03"
        result = get_retail_prices(sector, state, start_period, end_period)
    elif command == "demand":
        region = args[1] if len(args) > 1 else "CISO"
        start_period = args[2] if len(args) > 2 else "2024-01-01T00"
        end_period = args[3] if len(args) > 3 else "2024-01-02T00"
        result = get_demand(region, start_period, end_period)
    elif command == "capacity":
        source = args[1] if len(args) > 1 else "SUN"
        state = args[2] if len(args) > 2 else "CA"
        year = args[3] if len(args) > 3 else "2023"
        result = get_capacity(source, state, year)
    elif command == "mix":
        state = args[1] if len(args) > 1 else "CA"
        year = args[2] if len(args) > 2 else "2023"
        result = get_net_generation_mix(state, year)
    elif command == "utility":
        utility_id = args[1] if len(args) > 1 else "14328"
        start_period = args[2] if len(args) > 2 else "2024-01"
        end_period = args[3] if len(args) > 3 else "2024-03"
        result = get_utility_data(utility_id, start_period, end_period)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
