"""
Census International Data Fetcher
US Census International Data: population, demographics, economic development indicators for all countries (IDB).
"""
import sys
import json
import os
import requests
from typing import Dict, Any

API_KEY = os.environ.get('CENSUS_API_KEY', '')
BASE_URL = "https://api.census.gov/data/timeseries/idb"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not API_KEY:
        return {"error": "The Census Bureau API now requires a free API key: request one at "
                         "https://api.census.gov/data/key_signup.html and save it as CENSUS_API_KEY "
                         "in Settings > Credentials"}
    url = f"{BASE_URL}/{endpoint}"
    params = dict(params or {})
    params["key"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=30, allow_redirects=False)
        if response.status_code in (301, 302) or response.headers.get("X-DataWebAPI-KeyError"):
            return {"error": "The Census API rejected the API key (CENSUS_API_KEY) - check it in Settings > Credentials"}
        if response.status_code == 204:
            return {"error": "No data for this query (check the ISO-2 country code and year)"}
        response.raise_for_status()
        rows = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {e.response.text[:200]}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if not isinstance(rows, list) or len(rows) < 2:
        return {"error": "No data returned"}
    header = rows[0]
    out = []
    for r in rows[1:]:
        rec = {}
        for k, v in zip(header, r):
            if k in ("NAME", "GENC", "GEO_ID"):
                rec[k] = v
            else:
                try:
                    f = float(v)
                    rec[k] = int(f) if f.is_integer() else f
                except (TypeError, ValueError):
                    rec[k] = v
        out.append(rec)
    return {"data": out, "count": len(out), "source": "US Census Bureau International Database"}


def _five_year(country: str, year: int, fields: str) -> Any:
    return _make_request("5year", params={"get": f"NAME,GENC,{fields}", "GENC": country.upper(), "YR": str(year)})


def get_countries() -> Any:
    return _make_request("5year", params={"get": "NAME,GENC,POP", "YR": "2025"})


def get_population_projection(country: str, year: int = 2025, sex: str = "0") -> Any:
    """Total population (sex 0), male (1) or female (2) for a country and year (projections to 2100)."""
    field = {"1": "MPOP", "2": "FPOP"}.get(str(sex), "POP")
    return _five_year(country, year, f"{field},POP_DENS,GR,MEDAGE")


def get_age_structure(country: str, year: int = 2025) -> Any:
    """Population by single year of age (both sexes)."""
    return _make_request("1year", params={"get": "NAME,GENC,AGE,POP", "GENC": country.upper(),
                                          "YR": str(year), "SEX": "0"})


def get_birth_death_rates(country: str, year: int = 2025) -> Any:
    return _five_year(country, year, "CBR,CDR,TFR,RNI,NIM")


def get_infant_mortality(country: str, year: int = 2025) -> Any:
    return _five_year(country, year, "IMR,IMR_M,IMR_F,MR0_4")


def get_life_expectancy(country: str, year: int = 2025, sex: str = "0") -> Any:
    field = {"1": "E0_M", "2": "E0_F"}.get(str(sex), "E0,E0_M,E0_F")
    return _five_year(country, year, field)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "countries":
        result = get_countries()
    elif command == "projection":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2025
        sex = args[3] if len(args) > 3 else "0"
        result = get_population_projection(country, year, sex)
    elif command == "age_structure":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2025
        result = get_age_structure(country, year)
    elif command == "birth_death":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2025
        result = get_birth_death_rates(country, year)
    elif command == "infant_mortality":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2025
        result = get_infant_mortality(country, year)
    elif command == "life_expectancy":
        country = args[1] if len(args) > 1 else "IN"
        year = int(args[2]) if len(args) > 2 else 2025
        sex = args[3] if len(args) > 3 else "0"
        result = get_life_expectancy(country, year, sex)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
