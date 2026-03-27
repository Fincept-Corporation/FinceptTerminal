"""
USDA ERS Data Fetcher
USDA Economic Research Service: food prices, farm income, commodity outlooks,
and rural economics data via the ERS API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('USDA_ERS_API_KEY', '')
BASE_URL = "https://api.ers.usda.gov/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
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


def get_food_price_outlook() -> Any:
    return _make_request("foodpricesprojections/summary")


def get_farm_income(year: str = None, state: str = None) -> Any:
    params = {}
    if year:
        params["year"] = year
    if state:
        params["state"] = state
    return _make_request("Arms/farmIncome/farmsReceiptsCashRents", params)


def get_commodity_outlook(commodity: str) -> Any:
    params = {"commodity": commodity}
    return _make_request("Arms/farmIncome/valueOfProductionAndGrossReturn", params)


def get_food_expenditure(year: str = None) -> Any:
    params = {}
    if year:
        params["year"] = year
    return _make_request("foodexpenditures/FoodExpenditures", params)


def get_rural_indicators(state: str = None) -> Any:
    params = {}
    if state:
        params["state"] = state
    return _make_request("GovtPayments/Data", params)


def get_datasets() -> Any:
    return _make_request("", {"datasetsonly": "true"})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "food_prices":
        result = get_food_price_outlook()
    elif command == "farm_income":
        year = args[1] if len(args) > 1 else None
        state = args[2] if len(args) > 2 else None
        result = get_farm_income(year, state)
    elif command == "outlook":
        commodity = args[1] if len(args) > 1 else "corn"
        result = get_commodity_outlook(commodity)
    elif command == "expenditure":
        year = args[1] if len(args) > 1 else None
        result = get_food_expenditure(year)
    elif command == "rural":
        state = args[1] if len(args) > 1 else None
        result = get_rural_indicators(state)
    elif command == "datasets":
        result = get_datasets()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
