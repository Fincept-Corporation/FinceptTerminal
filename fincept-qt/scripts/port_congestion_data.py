"""
Port Congestion Data Fetcher
Global port congestion and container shipping data from public sources.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('PORTCHAIN_API_KEY', '')
BASE_URL = "https://api.portchain.com"

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


def get_global_congestion_index(start_date: str, end_date: str) -> Any:
    params = {"startDate": start_date, "endDate": end_date, "metric": "global_congestion_index"}
    return _make_request("v1/congestion/global", params)


def get_port_congestion(port_code: str, start_date: str, end_date: str) -> Any:
    params = {"portCode": port_code, "startDate": start_date, "endDate": end_date}
    return _make_request("v1/congestion/port", params)


def get_container_rates(tradelane: str, start_date: str, end_date: str) -> Any:
    params = {"tradelane": tradelane, "startDate": start_date, "endDate": end_date}
    return _make_request("v1/rates/container", params)


def get_vessel_queue(port_code: str, date: str) -> Any:
    params = {"portCode": port_code, "date": date}
    return _make_request("v1/queue/vessels", params)


def get_major_ports() -> Any:
    return _make_request("v1/ports/major", {})


def get_port_throughput(port_code: str, year: str) -> Any:
    params = {"portCode": port_code, "year": year}
    return _make_request("v1/throughput/port", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "global_index":
        start_date = args[1] if len(args) > 1 else "2024-01-01"
        end_date = args[2] if len(args) > 2 else "2024-03-31"
        result = get_global_congestion_index(start_date, end_date)
    elif command == "port":
        port_code = args[1] if len(args) > 1 else "CNSHA"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-03-31"
        result = get_port_congestion(port_code, start_date, end_date)
    elif command == "container_rates":
        tradelane = args[1] if len(args) > 1 else "Asia-Europe"
        start_date = args[2] if len(args) > 2 else "2024-01-01"
        end_date = args[3] if len(args) > 3 else "2024-03-31"
        result = get_container_rates(tradelane, start_date, end_date)
    elif command == "queue":
        port_code = args[1] if len(args) > 1 else "CNSHA"
        date = args[2] if len(args) > 2 else "2024-01-15"
        result = get_vessel_queue(port_code, date)
    elif command == "ports":
        result = get_major_ports()
    elif command == "throughput":
        port_code = args[1] if len(args) > 1 else "CNSHA"
        year = args[2] if len(args) > 2 else "2023"
        result = get_port_throughput(port_code, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
