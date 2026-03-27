"""
Gas Infrastructure Data Fetcher
ENTSO-G (European gas grid): gas flows, storage levels, LNG sendout, interconnection capacities.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ENTSOG_API_KEY', '')
BASE_URL = "https://transparency.entsog.eu/api/v1"

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


def get_storage_data(country: str, start_date: str, end_date: str) -> Any:
    params = {"country": country, "from": start_date, "to": end_date, "indicator": "Storage"}
    return _make_request("operationaldatas", params)


def get_interconnection_flows(from_country: str, to_country: str, start_date: str, end_date: str) -> Any:
    params = {"fromCountry": from_country, "toCountry": to_country, "from": start_date, "to": end_date, "indicator": "PhysicalFlow"}
    return _make_request("operationaldatas", params)


def get_lng_sendout(terminal: str, start_date: str, end_date: str) -> Any:
    params = {"pointKey": terminal, "from": start_date, "to": end_date, "indicator": "LngSendout"}
    return _make_request("operationaldatas", params)


def get_production(country: str, start_date: str, end_date: str) -> Any:
    params = {"country": country, "from": start_date, "to": end_date, "indicator": "Production"}
    return _make_request("operationaldatas", params)


def get_consumption(country: str, start_date: str, end_date: str) -> Any:
    params = {"country": country, "from": start_date, "to": end_date, "indicator": "Consumption"}
    return _make_request("operationaldatas", params)


def get_points() -> Any:
    return _make_request("interconnections", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "storage":
        country = args[1] if len(args) > 1 else "DE"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-01-31"
        result = get_storage_data(country, start_date, end_date)
    elif command == "flows":
        from_country = args[1] if len(args) > 1 else "NO"
        to_country = args[2] if len(args) > 2 else "DE"
        start_date = args[3] if len(args) > 3 else "2024-01-01"
        end_date = args[4] if len(args) > 4 else "2024-01-31"
        result = get_interconnection_flows(from_country, to_country, start_date, end_date)
    elif command == "lng":
        terminal = args[1] if len(args) > 1 else "21Z000000000321Z"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-01-31"
        result = get_lng_sendout(terminal, start_date, end_date)
    elif command == "production":
        country = args[1] if len(args) > 1 else "NO"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-01-31"
        result = get_production(country, start_date, end_date)
    elif command == "consumption":
        country = args[1] if len(args) > 1 else "DE"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-01-31"
        result = get_consumption(country, start_date, end_date)
    elif command == "points":
        result = get_points()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
