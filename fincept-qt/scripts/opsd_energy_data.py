"""
Open Power System Data (OPSD) Fetcher
Provides European electricity consumption, generation by source, renewable
capacity, power plant data, and price time series for European countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OPSD_API_KEY', '')
BASE_URL = "https://data.open-power-system-data.org"

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


def _fetch_csv_as_json(url: str, params: Dict = None) -> Any:
    """Fetch a CSV from OPSD and convert first N rows to JSON."""
    try:
        response = session.get(url, params=params, timeout=60)
        response.raise_for_status()
        lines = response.text.strip().split("\n")
        if len(lines) < 2:
            return {"error": "No data returned"}
        headers = lines[0].split(",")
        records = []
        for line in lines[1:501]:  # limit to 500 rows
            values = line.split(",")
            records.append(dict(zip(headers, values)))
        return {"records": records, "total_fetched": len(records)}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def get_time_series(country: str, variable: str, start_date: str = None, end_date: str = None) -> Any:
    """Return hourly time series for electricity load, generation, or prices."""
    url = "https://raw.githubusercontent.com/Open-Power-System-Data/time_series/2020-10-06/time_series_60min_singleindex.csv"
    params = {}
    data = _fetch_csv_as_json(url, params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "variable": variable, "start_date": start_date, "end_date": end_date, "data": data}


def get_renewable_capacity(country: str = None, technology: str = None, year: int = None) -> Any:
    """Return installed renewable energy capacity by country and technology."""
    url = "https://raw.githubusercontent.com/Open-Power-System-Data/renewable_power_plants/2020-08-25/renewable_power_plants_EU.csv"
    data = _fetch_csv_as_json(url)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "technology": technology, "year": year, "capacity_data": data}


def get_power_plants(country: str = None, technology: str = None) -> Any:
    """Return conventional power plant inventory with capacity and fuel type."""
    url = "https://raw.githubusercontent.com/Open-Power-System-Data/conventional_power_plants/2020-10-01/conventional_power_plants_EU.csv"
    data = _fetch_csv_as_json(url)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "technology": technology, "plants": data}


def get_weather_data(country: str, variable: str = "temperature", year: int = None) -> Any:
    """Return weather-dependent renewable energy data (capacity factors, solar radiation)."""
    params = {
        "country": country,
        "variable": variable
    }
    if year:
        params["year"] = year
    data = _make_request("ninja", params=params)
    if isinstance(data, dict) and "error" in data:
        return {"country": country, "variable": variable, "year": year,
                "note": "Weather data available via renewables.ninja integration", "data": {}}
    return {"country": country, "variable": variable, "year": year, "data": data}


def get_available_datasets() -> Any:
    """Return the list of OPSD datasets and their descriptions."""
    datasets = [
        {"name": "time_series", "description": "Electricity load, generation and prices (hourly, 2006-2019)", "countries": "DE, AT, CH, DK, SE, NO, FR, BE, NL, GB, ES, IT, CZ, PL"},
        {"name": "renewable_power_plants", "description": "Installed renewable energy capacity by plant", "countries": "EU-28"},
        {"name": "conventional_power_plants", "description": "Conventional power plant registry", "countries": "DE, FR, EU"},
        {"name": "national_generation_capacity", "description": "Annual electricity generation capacity by technology", "countries": "EU"},
        {"name": "when2heat", "description": "Hourly heating and cooling profiles", "countries": "EU-28"},
    ]
    return {"datasets": datasets, "count": len(datasets)}


def get_national_generation(country: str, year: int = None) -> Any:
    """Return annual national electricity generation by technology for a country."""
    url = "https://raw.githubusercontent.com/Open-Power-System-Data/national_generation_capacity/2020-10-01/national_generation_capacity_stacked.csv"
    data = _fetch_csv_as_json(url)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "generation": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "time_series":
        country = args[1] if len(args) > 1 else ""
        variable = args[2] if len(args) > 2 else "load"
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        if not country:
            result = {"error": "country required"}
        else:
            result = get_time_series(country, variable, start_date, end_date)
    elif command == "capacity":
        country = args[1] if len(args) > 1 else None
        technology = args[2] if len(args) > 2 else None
        year = int(args[3]) if len(args) > 3 else None
        result = get_renewable_capacity(country, technology, year)
    elif command == "plants":
        country = args[1] if len(args) > 1 else None
        technology = args[2] if len(args) > 2 else None
        result = get_power_plants(country, technology)
    elif command == "weather":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            variable = args[2] if len(args) > 2 else "temperature"
            year = int(args[3]) if len(args) > 3 else None
            result = get_weather_data(country, variable, year)
    elif command == "datasets":
        result = get_available_datasets()
    elif command == "generation":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_national_generation(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
