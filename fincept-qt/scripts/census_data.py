"""
US Census Bureau Data Fetcher
US Census Bureau API: ACS demographics, trade statistics, economic census,
and population estimates.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CENSUS_API_KEY', '')
BASE_URL = "https://api.census.gov/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["key"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        if isinstance(data, list) and len(data) > 1:
            headers = data[0]
            rows = data[1:]
            return {"headers": headers, "data": [dict(zip(headers, row)) for row in rows], "count": len(rows)}
        return data
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_acs_data(variables: str = "NAME,B01003_001E", geography: str = "state:*", year: str = "2022") -> Any:
    params = {"get": variables, "for": geography}
    return _make_request(f"{year}/acs/acs5", params)


def get_trade_data(commodity: str = "0101", country: str = "1220", year: str = "2023") -> Any:
    params = {
        "get": "CTY_CODE,CTY_NAME,ALL_VAL_MO,VES_WGT_MO,AIR_WGT_MO",
        "COMM_LVL": "HS10",
        "I_COMMODITY": commodity,
        "CTY_CODE": country,
    }
    return _make_request(f"timeseries/intltrade/imports/hs", params)


def get_population(state: str = "*", year: str = "2023") -> Any:
    params = {
        "get": "NAME,POP,DENSITY",
        "for": f"state:{state}" if state != "*" else "state:*",
    }
    return _make_request(f"{year}/pep/population", params)


def get_housing_data(geography: str = "state:*", year: str = "2022") -> Any:
    params = {
        "get": "NAME,B25001_001E,B25002_002E,B25002_003E,B25003_002E,B25003_003E",
        "for": geography,
    }
    return _make_request(f"{year}/acs/acs5", params)


def get_business_patterns(naics: str = "52", state: str = "*", year: str = "2021") -> Any:
    params = {
        "get": "NAME,NAICS2017,NAICS2017_TTL,EMP,PAYANN,ESTAB",
        "for": f"state:{state}",
        "NAICS2017": naics,
    }
    return _make_request(f"{year}/cbp", params)


def get_datasets() -> Any:
    return _make_request("", {"vintage": "2023"})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "acs":
        variables = args[1] if len(args) > 1 else "NAME,B01003_001E"
        geography = args[2] if len(args) > 2 else "state:*"
        year = args[3] if len(args) > 3 else "2022"
        result = get_acs_data(variables, geography, year)
    elif command == "trade":
        commodity = args[1] if len(args) > 1 else "0101"
        country = args[2] if len(args) > 2 else "1220"
        year = args[3] if len(args) > 3 else "2023"
        result = get_trade_data(commodity, country, year)
    elif command == "population":
        state = args[1] if len(args) > 1 else "*"
        year = args[2] if len(args) > 2 else "2023"
        result = get_population(state, year)
    elif command == "housing":
        geography = args[1] if len(args) > 1 else "state:*"
        year = args[2] if len(args) > 2 else "2022"
        result = get_housing_data(geography, year)
    elif command == "business":
        naics = args[1] if len(args) > 1 else "52"
        state = args[2] if len(args) > 2 else "*"
        year = args[3] if len(args) > 3 else "2021"
        result = get_business_patterns(naics, state, year)
    elif command == "datasets":
        result = get_datasets()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
