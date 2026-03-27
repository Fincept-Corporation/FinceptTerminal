"""
IEA (International Energy Agency) Data Fetcher
IEA open statistics: energy supply, demand, CO2, renewables for 180+ countries
via IEA public API (no key for public endpoints).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.iea.org/stats"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})

IEA_PRODUCTS = {
    "oil": "CRUDES+NGLS+REFINERYFD+REFPROD+NONCRUDE",
    "natural_gas": "NATGAS",
    "coal": "HARDCOAL+BROWN",
    "electricity": "ELECTR",
    "renewables": "RENEWABLE",
    "nuclear": "NUCLEAR",
    "total": "TOTAL"
}

IEA_FLOWS = {
    "supply": "TPES",
    "production": "INDPROD",
    "consumption": "TFC",
    "imports": "IMP",
    "exports": "EXP",
    "co2": "CO2COMBUST"
}


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


def get_energy_supply(country: str = "USA", product: str = "total", year: int = 2022) -> Any:
    product_code = IEA_PRODUCTS.get(product.lower(), product.upper())
    params = {
        "countries": country.upper(),
        "products": product_code,
        "flows": "TPES",
        "year": year
    }
    data = _make_request("", params)
    return {"country": country, "product": product, "flow": "supply", "year": year, "data": data}


def get_electricity_generation(country: str = "USA", source: str = "total",
                                  year: int = 2022) -> Any:
    source_code = IEA_PRODUCTS.get(source.lower(), source.upper())
    params = {
        "countries": country.upper(),
        "products": source_code,
        "flows": "ELECTR",
        "year": year
    }
    data = _make_request("", params)
    return {"country": country, "source": source, "year": year, "data": data}


def get_co2_emissions(country: str = "USA", sector: str = "total", year: int = 2022) -> Any:
    params = {
        "countries": country.upper(),
        "flows": "CO2COMBUST",
        "year": year
    }
    data = _make_request("", params)
    return {"country": country, "sector": sector, "year": year,
            "unit": "Mt CO2", "data": data}


def get_renewable_capacity(country: str = "USA", technology: str = "all",
                             year: int = 2022) -> Any:
    params = {
        "countries": country.upper(),
        "products": "RENEWABLE",
        "flows": "INDPROD",
        "year": year
    }
    data = _make_request("", params)
    return {"country": country, "technology": technology, "year": year, "data": data}


def get_energy_indicators(country: str = "USA", year: int = 2022) -> Any:
    results = {}
    for flow_name, flow_code in [("supply", "TPES"), ("consumption", "TFC"), ("co2", "CO2COMBUST")]:
        params = {"countries": country.upper(), "flows": flow_code, "year": year}
        results[flow_name] = _make_request("", params)
    return {"country": country, "year": year, "indicators": results}


def get_oil_market_report() -> Any:
    # IEA Oil Market Report data - use public data endpoint
    url = "https://www.iea.org/api/report/IEA/OMR"
    try:
        resp = session.get(url, timeout=30)
        resp.raise_for_status()
        return resp.json()
    except Exception:
        pass
    # Fallback: return latest crude oil supply via stats API
    params = {"products": IEA_PRODUCTS["oil"], "flows": "TPES", "year": 2022}
    data = _make_request("", params)
    return {"report_type": "oil_market", "note": "Full OMR requires IEA subscription",
            "public_data": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "supply":
        country = args[1] if len(args) > 1 else "USA"
        product = args[2] if len(args) > 2 else "total"
        year = int(args[3]) if len(args) > 3 else 2022
        result = get_energy_supply(country, product, year)
    elif command == "electricity":
        country = args[1] if len(args) > 1 else "USA"
        source = args[2] if len(args) > 2 else "total"
        year = int(args[3]) if len(args) > 3 else 2022
        result = get_electricity_generation(country, source, year)
    elif command == "co2":
        country = args[1] if len(args) > 1 else "USA"
        sector = args[2] if len(args) > 2 else "total"
        year = int(args[3]) if len(args) > 3 else 2022
        result = get_co2_emissions(country, sector, year)
    elif command == "renewables":
        country = args[1] if len(args) > 1 else "USA"
        technology = args[2] if len(args) > 2 else "all"
        year = int(args[3]) if len(args) > 3 else 2022
        result = get_renewable_capacity(country, technology, year)
    elif command == "indicators":
        country = args[1] if len(args) > 1 else "USA"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_energy_indicators(country, year)
    elif command == "oil_market":
        result = get_oil_market_report()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
