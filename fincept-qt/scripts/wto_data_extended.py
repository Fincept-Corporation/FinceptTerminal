"""
WTO Stats Extended Data Fetcher
World Trade Organization Statistics: global tariffs, trade in goods and services,
trade profiles, and trade indicators.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WTO_API_KEY', '')
BASE_URL = "https://stats.wto.org/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["lang"] = "1"
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


def get_tariffs(reporter: str = "840", partner: str = "000", product: str = "TOTAL") -> Any:
    params = {
        "i": "HS_M_0010,HS_A_0020",
        "r": reporter,
        "p": partner,
        "ps": product,
        "pc": "AG2",
    }
    return _make_request("data", params)


def get_merchandise_trade(reporter: str = "840", partner: str = "000", product: str = "TO", year: str = "2023") -> Any:
    params = {
        "i": "ITS_MTV_AM",
        "r": reporter,
        "p": partner,
        "ps": year,
        "pc": product,
    }
    return _make_request("data", params)


def get_services_trade(reporter: str = "840", partner: str = "000", sector: str = "S", year: str = "2023") -> Any:
    params = {
        "i": "ITS_CS_AM5",
        "r": reporter,
        "p": partner,
        "ps": year,
        "pc": sector,
    }
    return _make_request("data", params)


def get_trade_profile(country: str = "840") -> Any:
    params = {
        "i": "ITS_MTV_AM,ITS_CS_AM5",
        "r": country,
        "p": "000",
    }
    return _make_request("data", params)


def get_trade_indicators(country: str = "840", year: str = "2023") -> Any:
    params = {
        "i": "ITS_MTV_AM,ITS_CS_AM5,HS_A_0020",
        "r": country,
        "p": "000",
        "ps": year,
    }
    return _make_request("data", params)


def get_datasets() -> Any:
    return _make_request("indicators")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "tariffs":
        reporter = args[1] if len(args) > 1 else "840"
        partner = args[2] if len(args) > 2 else "000"
        product = args[3] if len(args) > 3 else "TOTAL"
        result = get_tariffs(reporter, partner, product)
    elif command == "merchandise":
        reporter = args[1] if len(args) > 1 else "840"
        partner = args[2] if len(args) > 2 else "000"
        product = args[3] if len(args) > 3 else "TO"
        year = args[4] if len(args) > 4 else "2023"
        result = get_merchandise_trade(reporter, partner, product, year)
    elif command == "services":
        reporter = args[1] if len(args) > 1 else "840"
        partner = args[2] if len(args) > 2 else "000"
        sector = args[3] if len(args) > 3 else "S"
        year = args[4] if len(args) > 4 else "2023"
        result = get_services_trade(reporter, partner, sector, year)
    elif command == "profile":
        country = args[1] if len(args) > 1 else "840"
        result = get_trade_profile(country)
    elif command == "indicators":
        country = args[1] if len(args) > 1 else "840"
        year = args[2] if len(args) > 2 else "2023"
        result = get_trade_indicators(country, year)
    elif command == "datasets":
        result = get_datasets()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
