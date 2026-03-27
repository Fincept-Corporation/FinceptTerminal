"""
BIS Stats Extended Data Fetcher
Bank for International Settlements extended statistics: global credit gap,
property prices, exchange rates, banking stats, and policy rates via SDMX API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('BIS_API_KEY', '')
BASE_URL = "https://stats.bis.org/api/v1/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "jsondata"
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


def get_credit_gap(country: str = "US") -> Any:
    key = f"CREDIT_GAPS/{country.upper()}.."
    return _make_request(key)


def get_residential_property_prices(country: str = "US", freq: str = "Q") -> Any:
    key = f"RPPI/{freq}.{country.upper()}..N.."
    return _make_request(key)


def get_commercial_property_prices(country: str = "US") -> Any:
    key = f"CPPI/Q.{country.upper()}..."
    return _make_request(key)


def get_banking_stats(reporting_country: str = "US", counterparty: str = "5J") -> Any:
    key = f"BIS_LBS_DISS/{reporting_country.upper()}.{counterparty}.A.B.A.TO1.A"
    return _make_request(key)


def get_consumer_prices(country: str = "US") -> Any:
    key = f"CPI/M.{country.upper()}.N.H.000000..P1M"
    return _make_request(key)


def get_policy_rates(country: str = "US") -> Any:
    key = f"CBPOL/M.{country.upper()}"
    return _make_request(key)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "credit_gap":
        country = args[1] if len(args) > 1 else "US"
        result = get_credit_gap(country)
    elif command == "residential_prices":
        country = args[1] if len(args) > 1 else "US"
        freq = args[2] if len(args) > 2 else "Q"
        result = get_residential_property_prices(country, freq)
    elif command == "commercial_prices":
        country = args[1] if len(args) > 1 else "US"
        result = get_commercial_property_prices(country)
    elif command == "banking":
        reporting_country = args[1] if len(args) > 1 else "US"
        counterparty = args[2] if len(args) > 2 else "5J"
        result = get_banking_stats(reporting_country, counterparty)
    elif command == "cpi":
        country = args[1] if len(args) > 1 else "US"
        result = get_consumer_prices(country)
    elif command == "policy_rates":
        country = args[1] if len(args) > 1 else "US"
        result = get_policy_rates(country)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
