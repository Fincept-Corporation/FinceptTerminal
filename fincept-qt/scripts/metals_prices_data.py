"""
Metals Prices Data Fetcher
Gold, silver, platinum, palladium, and copper real-time and historical prices
via the Metals.dev API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('METALS_DEV_API_KEY', '')
BASE_URL = "https://api.metals.dev/v1"
SERVICE = "Metals.dev"

session = fc.http_session()


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every Metals.dev endpoint answers HTTP 401 without a key: say so before any network call.
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://metals.dev/", "METALS_DEV_API_KEY", what="a free API key")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["api_key"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, "METALS_DEV_API_KEY", API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    # Metals.dev reports failures in the body, some with HTTP 200 (unknown endpoint, plan limits).
    if isinstance(data, dict) and data.get("status") == "failure":
        msg = data.get("error_message") or f"error code {data.get('error_code')}"
        return {"error": f"{SERVICE}: {fc.scrub(msg, API_KEY).strip()}"}
    return data


def get_latest(currencies: str = "USD", metals: str = "gold,silver,platinum,palladium") -> Any:
    params = {"currency": currencies, "unit": "troy_oz"}
    metals_list = [m.strip() for m in metals.split(",")]
    params["metals"] = ",".join(metals_list)
    return _make_request("latest", params)


def get_historical(date: str, currencies: str = "USD", metals: str = "gold,silver") -> Any:
    # There is no /v1/historical: one day of /v1/timeseries is the historical snapshot.
    params = {"start_date": date, "end_date": date, "currency": currencies, "unit": "troy_oz"}
    metals_list = [m.strip() for m in metals.split(",")]
    params["metals"] = ",".join(metals_list)
    return _make_request("timeseries", params)


def get_timeframe(start: str, end: str, currencies: str = "USD", metals: str = "gold") -> Any:
    # /v1/timeseries (the old /v1/timeframe path does not exist); Metals.dev caps it at 30 days.
    params = {
        "start_date": start,
        "end_date": end,
        "currency": currencies,
        "unit": "troy_oz",
    }
    metals_list = [m.strip() for m in metals.split(",")]
    params["metals"] = ",".join(metals_list)
    return _make_request("timeseries", params)


def convert(from_metal: str, to_currency: str, amount: str = "1") -> Any:
    # There is no /v1/convert: value `amount` troy ounces at the /v1/metal/spot price.
    params = {
        "metal": from_metal.lower(),
        "currency": to_currency.upper(),
    }
    data = _make_request("metal/spot", params)
    if not isinstance(data, dict) or "error" in data:
        return data
    rate = data.get("rate") if isinstance(data.get("rate"), dict) else {}
    price = rate.get("price")
    try:
        qty = float(amount)
    except (TypeError, ValueError):
        return {"error": f"amount must be a number, got {amount!r}"}
    return {
        "metal": from_metal.lower(),
        "currency": to_currency.upper(),
        "unit": data.get("unit", "toz"),
        "amount": qty,
        "price": price,
        "value": price * qty if isinstance(price, (int, float)) else None,
        "timestamp": data.get("timestamp"),
        "spot": rate,
    }


def get_supported_metals() -> Any:
    return {
        "metals": [
            "gold", "silver", "platinum", "palladium", "rhodium",
            "iridium", "osmium", "rhenium", "ruthenium", "copper",
            "aluminum", "lead", "nickel", "zinc", "tin"
        ]
    }


def get_supported_currencies() -> Any:
    return _make_request("currencies", {})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "latest":
        currencies = args[1] if len(args) > 1 else "USD"
        metals = args[2] if len(args) > 2 else "gold,silver,platinum,palladium"
        result = get_latest(currencies, metals)
    elif command == "historical":
        date = args[1] if len(args) > 1 else "2024-01-01"
        currencies = args[2] if len(args) > 2 else "USD"
        metals = args[3] if len(args) > 3 else "gold,silver"
        result = get_historical(date, currencies, metals)
    elif command == "timeframe":
        start = args[1] if len(args) > 1 else "2024-12-01"
        end = args[2] if len(args) > 2 else "2024-12-31"
        currencies = args[3] if len(args) > 3 else "USD"
        metals = args[4] if len(args) > 4 else "gold"
        result = get_timeframe(start, end, currencies, metals)
    elif command == "convert":
        from_metal = args[1] if len(args) > 1 else "gold"
        to_currency = args[2] if len(args) > 2 else "USD"
        amount = args[3] if len(args) > 3 else "1"
        result = convert(from_metal, to_currency, amount)
    elif command == "metals":
        result = get_supported_metals()
    elif command == "currencies":
        result = get_supported_currencies()

    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
