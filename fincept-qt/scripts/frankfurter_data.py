"""
Frankfurter Data Fetcher
ECB reference exchange rates, 30+ currencies, daily history back to 1999.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.frankfurter.dev"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
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


def get_latest(base: str = "EUR", symbols: str = None) -> Any:
    """Get latest ECB reference rates for a base currency.
    base: ISO 4217 currency code (EUR, USD, GBP, JPY, etc.)
    symbols: comma-separated list of target currencies (e.g. 'USD,GBP,JPY')
    """
    params = {}
    if base and base.upper() != "EUR":
        params["base"] = base.upper()
    if symbols:
        params["symbols"] = symbols.upper()
    data = _make_request("latest", params=params)
    return data


def get_historical(date: str, base: str = "EUR", symbols: str = None) -> Any:
    """Get ECB reference rates for a specific historical date.
    date: YYYY-MM-DD format. Data available from 1999-01-04.
    """
    params = {}
    if base and base.upper() != "EUR":
        params["base"] = base.upper()
    if symbols:
        params["symbols"] = symbols.upper()
    data = _make_request(date, params=params)
    return data


def get_time_series(start_date: str, end_date: str = None, base: str = "EUR", symbols: str = None) -> Any:
    """Get ECB reference rates over a date range.
    start_date/end_date: YYYY-MM-DD format.
    If end_date is omitted, returns from start_date to latest.
    """
    if end_date:
        endpoint = f"{start_date}..{end_date}"
    else:
        endpoint = f"{start_date}.."
    params = {}
    if base and base.upper() != "EUR":
        params["base"] = base.upper()
    if symbols:
        params["symbols"] = symbols.upper()
    data = _make_request(endpoint, params=params)
    if isinstance(data, dict) and "rates" in data:
        dates = list(data["rates"].keys())
        return {
            "base": data.get("base"),
            "start_date": data.get("start_date"),
            "end_date": data.get("end_date"),
            "rates": data["rates"],
            "date_count": len(dates),
            "dates": dates,
        }
    return data


def get_currencies() -> Any:
    """Get list of all supported currencies with their names."""
    data = _make_request("currencies")
    if isinstance(data, dict) and "error" not in data:
        currencies = [{"code": code, "name": name} for code, name in data.items()]
        currencies.sort(key=lambda x: x["code"])
        return {"currencies": currencies, "count": len(currencies)}
    return data


def convert_amount(amount: float, from_currency: str, to_currency: str) -> Any:
    """Convert a specific amount between two currencies using latest ECB rates."""
    params = {
        "base": from_currency.upper(),
        "symbols": to_currency.upper(),
        "amount": amount,
    }
    data = _make_request("latest", params=params)
    if isinstance(data, dict) and "rates" in data:
        rate = data["rates"].get(to_currency.upper())
        return {
            "from": from_currency.upper(),
            "to": to_currency.upper(),
            "amount": amount,
            "rate": rate,
            "converted": rate * amount if rate else None,
            "date": data.get("date"),
        }
    return data


def get_cross_rates(currencies: str = "USD,GBP,JPY,CHF,AUD,CAD,CNY,HKD") -> Any:
    """Get cross rates matrix for a set of currencies."""
    currency_list = [c.strip().upper() for c in currencies.split(",") if c.strip()]
    results = {}
    for base in currency_list:
        targets = [c for c in currency_list if c != base]
        if not targets:
            continue
        data = get_latest(base=base, symbols=",".join(targets))
        if isinstance(data, dict) and "rates" in data:
            results[base] = data["rates"]
        else:
            results[base] = {"error": str(data)}
    return {"cross_rates": results, "currencies": currency_list}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: latest, historical, series, currencies, convert, cross_rates"}))
        return

    command = args[0]

    if command == "latest":
        base = args[1] if len(args) > 1 else "EUR"
        symbols = args[2] if len(args) > 2 else None
        result = get_latest(base, symbols)
    elif command == "historical":
        if len(args) < 2:
            result = {"error": "Usage: historical <date YYYY-MM-DD> [base] [symbols]"}
        else:
            base = args[2] if len(args) > 2 else "EUR"
            symbols = args[3] if len(args) > 3 else None
            result = get_historical(args[1], base, symbols)
    elif command == "series":
        if len(args) < 2:
            result = {"error": "Usage: series <start_date> [end_date] [base] [symbols]"}
        else:
            end_date = args[2] if len(args) > 2 else None
            base = args[3] if len(args) > 3 else "EUR"
            symbols = args[4] if len(args) > 4 else None
            result = get_time_series(args[1], end_date, base, symbols)
    elif command == "currencies":
        result = get_currencies()
    elif command == "convert":
        if len(args) < 4:
            result = {"error": "Usage: convert <amount> <from_currency> <to_currency>"}
        else:
            result = convert_amount(float(args[1]), args[2], args[3])
    elif command == "cross_rates":
        currencies = args[1] if len(args) > 1 else "USD,EUR,GBP,JPY,CHF,AUD,CAD,CNY"
        result = get_cross_rates(currencies)
    else:
        result = {"error": f"Unknown command: {command}. Available: latest, historical, series, currencies, convert, cross_rates"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
