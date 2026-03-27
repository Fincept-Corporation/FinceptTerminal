"""
ExchangeRate Data Fetcher
200+ currencies, no key, no rate limit.
Uses open.er-api.com and fawazahmed0 CDN as fallback.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://open.er-api.com/v6"
CDN_BASE = "https://cdn.jsdelivr.net/npm/@fawazahmed0/currency-api@latest/v1"
CDN_FALLBACK = "https://latest.currency-api.pages.dev/v1"

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


def _make_cdn_request(url: str) -> Any:
    """Make CDN request with fallback."""
    for base in [CDN_BASE, CDN_FALLBACK]:
        full_url = url.replace(CDN_BASE, base)
        try:
            response = session.get(full_url, timeout=30)
            response.raise_for_status()
            return response.json()
        except Exception:
            continue
    return {"error": "Both CDN endpoints failed"}


def get_latest(base: str = "USD") -> Any:
    """Get latest exchange rates for a base currency (200+ targets)."""
    data = _make_request(f"latest/{base.upper()}")
    if isinstance(data, dict) and "rates" in data:
        return {
            "base": data.get("base_code"),
            "time_last_update_utc": data.get("time_last_update_utc"),
            "time_next_update_utc": data.get("time_next_update_utc"),
            "rates": data["rates"],
            "count": len(data["rates"]),
        }
    return data


def get_currencies() -> Any:
    """Get list of all supported currency codes and names from CDN."""
    url = f"{CDN_BASE}/currencies.json"
    data = _make_cdn_request(url)
    if isinstance(data, dict) and "error" not in data:
        currencies = [{"code": code.upper(), "name": name} for code, name in data.items()]
        currencies.sort(key=lambda x: x["code"])
        return {"currencies": currencies, "count": len(currencies)}
    return data


def get_pair_rate(base: str, target: str) -> Any:
    """Get the exchange rate for a specific currency pair."""
    data = _make_request(f"latest/{base.upper()}")
    if isinstance(data, dict) and "rates" in data:
        rate = data["rates"].get(target.upper())
        return {
            "base": base.upper(),
            "target": target.upper(),
            "rate": rate,
            "date": data.get("time_last_update_utc"),
            "inverse_rate": (1.0 / rate) if rate and rate != 0 else None,
        }
    return data


def get_historical_rate(date: str, base: str = "USD") -> Any:
    """Get historical exchange rates for a specific date via CDN.
    date: YYYY-MM-DD format.
    """
    base_lower = base.lower()
    url = f"https://cdn.jsdelivr.net/npm/@fawazahmed0/currency-api@{date}/v1/currencies/{base_lower}.json"
    data = _make_cdn_request(url)
    if isinstance(data, dict) and "error" not in data:
        rates_raw = data.get(base_lower, {})
        rates = {k.upper(): v for k, v in rates_raw.items()}
        return {
            "base": base.upper(),
            "date": data.get("date", date),
            "rates": rates,
            "count": len(rates),
        }
    return data


def get_all_rates_cdn(date: str = "latest") -> Any:
    """Get all currency rates from the CDN (free, no rate limit).
    date: 'latest' or YYYY-MM-DD.
    """
    if date == "latest":
        url = f"{CDN_BASE}/currencies/usd.json"
    else:
        url = f"https://cdn.jsdelivr.net/npm/@fawazahmed0/currency-api@{date}/v1/currencies/usd.json"
    data = _make_cdn_request(url)
    if isinstance(data, dict) and "error" not in data:
        usd_rates = data.get("usd", {})
        rates = {k.upper(): v for k, v in usd_rates.items()}
        return {
            "base": "USD",
            "date": data.get("date", date),
            "rates": rates,
            "count": len(rates),
        }
    return data


def get_multi_pair(base: str, targets: str) -> Any:
    """Get rates for multiple target currencies at once.
    targets: comma-separated currency codes (e.g. 'EUR,GBP,JPY,AUD').
    """
    target_list = [t.strip().upper() for t in targets.split(",") if t.strip()]
    data = _make_request(f"latest/{base.upper()}")
    if isinstance(data, dict) and "rates" in data:
        all_rates = data["rates"]
        filtered = {t: all_rates.get(t) for t in target_list}
        return {
            "base": base.upper(),
            "date": data.get("time_last_update_utc"),
            "rates": filtered,
            "not_found": [t for t in target_list if t not in all_rates],
        }
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: latest, currencies, pair, historical, all_rates, multi"}))
        return

    command = args[0]

    if command == "latest":
        base = args[1] if len(args) > 1 else "USD"
        result = get_latest(base)
    elif command == "currencies":
        result = get_currencies()
    elif command == "pair":
        if len(args) < 3:
            result = {"error": "Usage: pair <base> <target> (e.g. pair USD EUR)"}
        else:
            result = get_pair_rate(args[1], args[2])
    elif command == "historical":
        if len(args) < 2:
            result = {"error": "Usage: historical <date YYYY-MM-DD> [base]"}
        else:
            base = args[2] if len(args) > 2 else "USD"
            result = get_historical_rate(args[1], base)
    elif command == "all_rates":
        date = args[1] if len(args) > 1 else "latest"
        result = get_all_rates_cdn(date)
    elif command == "multi":
        if len(args) < 3:
            result = {"error": "Usage: multi <base> <target1,target2,...>"}
        else:
            result = get_multi_pair(args[1], args[2])
    else:
        result = {"error": f"Unknown command: {command}. Available: latest, currencies, pair, historical, all_rates, multi"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
