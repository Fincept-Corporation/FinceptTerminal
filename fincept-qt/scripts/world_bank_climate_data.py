"""
World Bank Climate Change Knowledge Portal Data Fetcher
Temperature projections, precipitation, climate risk by country
via World Bank Climate Knowledge Portal API (no key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://climateknowledgeportal.worldbank.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})

SCENARIOS = ["rcp26", "rcp45", "rcp60", "rcp85"]
PERIODS = ["1986_2005", "2020_2039", "2040_2059", "2060_2079", "2080_2099"]
VARIABLES = ["tas", "pr", "tasmax", "tasmin"]


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


def get_temperature_projection(country: str = "USA", scenario: str = "rcp85",
                                 period: str = "2040_2059") -> Any:
    endpoint = f"cru_data/climatology/{country}/tas/{period}"
    data = _make_request(endpoint)
    return {"country": country, "variable": "temperature", "scenario": scenario,
            "period": period, "data": data}


def get_precipitation_projection(country: str = "USA", scenario: str = "rcp85",
                                    period: str = "2040_2059") -> Any:
    endpoint = f"cru_data/climatology/{country}/pr/{period}"
    data = _make_request(endpoint)
    return {"country": country, "variable": "precipitation", "scenario": scenario,
            "period": period, "data": data}


def get_climate_risk_scores(country: str = "USA") -> Any:
    # World Bank climate risk indicators via general API
    risk_url = f"https://climateknowledgeportal.worldbank.org/api/cru_data/country/{country}"
    try:
        resp = session.get(risk_url, timeout=30)
        resp.raise_for_status()
        return resp.json()
    except Exception:
        pass
    # Fallback: use World Bank main API for climate-related indicators
    wb_url = f"https://api.worldbank.org/v2/country/{country}/indicator/EN.ATM.CO2E.PC"
    params = {"format": "json", "mrv": 10}
    return _make_request(wb_url, params)


def get_baseline_climate(country: str = "USA", variable: str = "tas",
                           month: int = 1) -> Any:
    if variable not in VARIABLES:
        return {"error": f"Unknown variable '{variable}'. Options: {VARIABLES}"}
    endpoint = f"cru_data/climatology/{country}/{variable}/1986_2005"
    data = _make_request(endpoint)
    return {"country": country, "variable": variable, "baseline_period": "1986-2005",
            "month": month, "data": data}


def get_extreme_events(country: str = "USA", hazard: str = "flood") -> Any:
    hazard_indicators = {
        "flood": "EN.CLC.PRCP.IN",
        "drought": "EN.CLC.HEAT.IN",
        "cyclone": "EN.CLC.WIND.IN",
        "heatwave": "EN.CLC.HEAT.IN"
    }
    indicator = hazard_indicators.get(hazard.lower(), "EN.CLC.PRCP.IN")
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator}"
    params = {"format": "json", "mrv": 20}
    data = _make_request(url, params)
    return {"country": country, "hazard": hazard, "indicator": indicator, "data": data}


def get_countries() -> Any:
    url = "https://api.worldbank.org/v2/country"
    params = {"format": "json", "per_page": 300}
    data = _make_request(url, params)
    if isinstance(data, list) and len(data) > 1:
        countries = [{"id": c.get("id"), "name": c.get("name"),
                       "region": c.get("region", {}).get("value"),
                       "income_level": c.get("incomeLevel", {}).get("value")}
                     for c in data[1] if c.get("id")]
        return {"count": len(countries), "countries": countries}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "temperature":
        country = args[1] if len(args) > 1 else "USA"
        scenario = args[2] if len(args) > 2 else "rcp85"
        period = args[3] if len(args) > 3 else "2040_2059"
        result = get_temperature_projection(country, scenario, period)
    elif command == "precipitation":
        country = args[1] if len(args) > 1 else "USA"
        scenario = args[2] if len(args) > 2 else "rcp85"
        period = args[3] if len(args) > 3 else "2040_2059"
        result = get_precipitation_projection(country, scenario, period)
    elif command == "risk":
        country = args[1] if len(args) > 1 else "USA"
        result = get_climate_risk_scores(country)
    elif command == "baseline":
        country = args[1] if len(args) > 1 else "USA"
        variable = args[2] if len(args) > 2 else "tas"
        month = int(args[3]) if len(args) > 3 else 1
        result = get_baseline_climate(country, variable, month)
    elif command == "extremes":
        country = args[1] if len(args) > 1 else "USA"
        hazard = args[2] if len(args) > 2 else "flood"
        result = get_extreme_events(country, hazard)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
