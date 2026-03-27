"""
ENTSO-E Data Fetcher
ENTSO-E (European electricity grid): power generation, load, cross-border flows, capacity, prices.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ENTSOE_API_KEY', '')
BASE_URL = "https://web-api.tp.entsoe.eu/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["securityToken"] = API_KEY
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


def get_load(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A65", "processType": "A16", "outBiddingZone_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_generation(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A75", "processType": "A16", "in_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_day_ahead_prices(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A44", "in_Domain": bidding_zone, "out_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_cross_border_flows(in_domain: str, out_domain: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A11", "in_Domain": in_domain, "out_Domain": out_domain, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_installed_capacity(bidding_zone: str, year: str) -> Any:
    params = {"documentType": "A68", "processType": "A33", "in_Domain": bidding_zone, "periodStart": f"{year}01010000", "periodEnd": f"{year}12312300"}
    return _make_request("", params)


def get_balancing_data(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A81", "processType": "A16", "controlArea_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "load":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_load(bidding_zone, start_date, end_date)
    elif command == "generation":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_generation(bidding_zone, start_date, end_date)
    elif command == "prices":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_day_ahead_prices(bidding_zone, start_date, end_date)
    elif command == "flows":
        in_domain = args[1] if len(args) > 1 else "10YDE-EON------1"
        out_domain = args[2] if len(args) > 2 else "10YFR-RTE------C"
        start_date = args[3] if len(args) > 3 else "202401010000"
        end_date = args[4] if len(args) > 4 else "202401020000"
        result = get_cross_border_flows(in_domain, out_domain, start_date, end_date)
    elif command == "capacity":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        year = args[2] if len(args) > 2 else "2024"
        result = get_installed_capacity(bidding_zone, year)
    elif command == "balancing":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_balancing_data(bidding_zone, start_date, end_date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
