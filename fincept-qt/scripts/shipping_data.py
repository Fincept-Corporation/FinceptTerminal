"""
Shipping Data Fetcher
Baltic Exchange indices: BDI (Baltic Dry Index), BCI, BPI, BSI, BHSI — dry bulk shipping rates.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('BALTIC_API_KEY', '')
BASE_URL = "https://www.balticexchange.com/api"

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


def get_bdi(start_date: str, end_date: str) -> Any:
    params = {"index": "BDI", "startDate": start_date, "endDate": end_date}
    return _make_request("indices/bdi", params)


def get_capesize_index(start_date: str, end_date: str) -> Any:
    params = {"index": "BCI", "startDate": start_date, "endDate": end_date}
    return _make_request("indices/capesize", params)


def get_panamax_index(start_date: str, end_date: str) -> Any:
    params = {"index": "BPI", "startDate": start_date, "endDate": end_date}
    return _make_request("indices/panamax", params)


def get_supramax_index(start_date: str, end_date: str) -> Any:
    params = {"index": "BSI", "startDate": start_date, "endDate": end_date}
    return _make_request("indices/supramax", params)


def get_handysize_index(start_date: str, end_date: str) -> Any:
    params = {"index": "BHSI", "startDate": start_date, "endDate": end_date}
    return _make_request("indices/handysize", params)


def get_tanker_rates(vessel_class: str, start_date: str, end_date: str) -> Any:
    params = {"vesselClass": vessel_class, "startDate": start_date, "endDate": end_date}
    return _make_request("indices/tanker", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "bdi":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_bdi(start_date, end_date)
    elif command == "capesize":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_capesize_index(start_date, end_date)
    elif command == "panamax":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_panamax_index(start_date, end_date)
    elif command == "supramax":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_supramax_index(start_date, end_date)
    elif command == "handysize":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_handysize_index(start_date, end_date)
    elif command == "tanker":
        vessel_class = args[1] if len(args) > 1 else "VLCC"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-03-31"
        result = get_tanker_rates(vessel_class, start_date, end_date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
