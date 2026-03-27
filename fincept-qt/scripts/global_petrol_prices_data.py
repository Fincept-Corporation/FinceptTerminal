"""
GlobalPetrolPrices Data Fetcher
Retail fuel prices (gasoline, diesel, LPG) by country — weekly updates
from GlobalPetrolPrices public data (no key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.globalpetrolprices.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
    "Accept": "application/json, text/html, */*"
})

FUEL_TYPES = {
    "gasoline": "gasoline_prices",
    "diesel": "diesel_prices",
    "lpg": "lpg_prices",
    "electricity": "electricity_prices",
    "natural_gas": "natural_gas_prices",
    "heating_oil": "heating_oil_prices"
}

DATAPORTAL_BASE = "https://www.globalpetrolprices.com/data"


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


def _fetch_json_data(fuel_slug: str) -> Any:
    # Try the public chart JSON endpoint
    urls = [
        f"{BASE_URL}/{fuel_slug}/",
        f"{BASE_URL}/{fuel_slug}/n/aaa/en/"
    ]
    for url in urls:
        try:
            resp = session.get(url, timeout=30)
            resp.raise_for_status()
            # Try to extract JSON from page script tags
            text = resp.text
            import re
            match = re.search(r'var\s+chartData\s*=\s*(\[.*?\]);', text, re.DOTALL)
            if match:
                return json.loads(match.group(1))
            match2 = re.search(r'"prices"\s*:\s*(\[.*?\])', text, re.DOTALL)
            if match2:
                return json.loads(match2.group(1))
        except Exception:
            continue
    return {"error": "Unable to fetch chart data from GlobalPetrolPrices. The site may require browser access."}


def get_gasoline_prices(country: str = None) -> Any:
    data = _fetch_json_data("gasoline_prices")
    if isinstance(data, list) and country:
        data = [d for d in data if country.lower() in str(d).lower()]
    return {"fuel_type": "gasoline", "unit": "USD per liter", "country": country, "data": data}


def get_diesel_prices(country: str = None) -> Any:
    data = _fetch_json_data("diesel_prices")
    if isinstance(data, list) and country:
        data = [d for d in data if country.lower() in str(d).lower()]
    return {"fuel_type": "diesel", "unit": "USD per liter", "country": country, "data": data}


def get_lpg_prices(country: str = None) -> Any:
    data = _fetch_json_data("lpg_prices")
    if isinstance(data, list) and country:
        data = [d for d in data if country.lower() in str(d).lower()]
    return {"fuel_type": "lpg", "unit": "USD per liter", "country": country, "data": data}


def get_electricity_prices(country: str = None) -> Any:
    data = _fetch_json_data("electricity_prices")
    if isinstance(data, list) and country:
        data = [d for d in data if country.lower() in str(d).lower()]
    return {"fuel_type": "electricity", "unit": "USD per kWh", "country": country, "data": data}


def get_price_history(fuel_type: str = "gasoline", country: str = "United States",
                        weeks: int = 52) -> Any:
    slug = FUEL_TYPES.get(fuel_type.lower(), f"{fuel_type}_prices")
    url = f"{BASE_URL}/{slug}/{country.lower().replace(' ', '_')}/"
    try:
        resp = session.get(url, timeout=30)
        resp.raise_for_status()
        text = resp.text
        import re
        match = re.search(r'var\s+chartData\s*=\s*(\[.*?\]);', text, re.DOTALL)
        if match:
            data = json.loads(match.group(1))
            if isinstance(data, list):
                data = data[-weeks:] if len(data) > weeks else data
            return {"fuel_type": fuel_type, "country": country, "weeks": weeks,
                    "count": len(data) if isinstance(data, list) else 0, "history": data}
        return {"error": f"No historical data found for {country}"}
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}


def get_country_rankings(fuel_type: str = "gasoline") -> Any:
    data = _fetch_json_data(FUEL_TYPES.get(fuel_type.lower(), f"{fuel_type}_prices"))
    return {"fuel_type": fuel_type, "unit": "USD per liter",
            "description": "Country rankings by fuel price", "data": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "gasoline":
        country = args[1] if len(args) > 1 else None
        result = get_gasoline_prices(country)
    elif command == "diesel":
        country = args[1] if len(args) > 1 else None
        result = get_diesel_prices(country)
    elif command == "lpg":
        country = args[1] if len(args) > 1 else None
        result = get_lpg_prices(country)
    elif command == "electricity":
        country = args[1] if len(args) > 1 else None
        result = get_electricity_prices(country)
    elif command == "history":
        fuel_type = args[1] if len(args) > 1 else "gasoline"
        country = args[2] if len(args) > 2 else "United States"
        weeks = int(args[3]) if len(args) > 3 else 52
        result = get_price_history(fuel_type, country, weeks)
    elif command == "rankings":
        fuel_type = args[1] if len(args) > 1 else "gasoline"
        result = get_country_rankings(fuel_type)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
