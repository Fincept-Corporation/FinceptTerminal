"""
UNDP Human Development Data Fetcher
Human Development Index (HDI), Gender Inequality Index (GII), and
Multidimensional Poverty Index (MPI) for all countries via the UNDP HDR API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UNDP_API_KEY', '')
# api.hdr.undp.org no longer exists; the HDRO Data API is at hdrdata.org and needs a free key
# (subscribe at https://hdrdata.org - see the HDRO data API manual).
BASE_URL = "https://hdrdata.org/api/CompositeIndices"
COMPOSITE_INDICATORS = {
    "hdi": "Human Development Index", "le": "Life expectancy at birth", "eys": "Expected years of schooling",
    "mys": "Mean years of schooling", "gnipc": "GNI per capita (2017 PPP $)", "gdi": "Gender Development Index",
    "gii": "Gender Inequality Index", "ihdi": "Inequality-adjusted HDI", "phdi": "Planetary pressures-adjusted HDI",
    "mpi_value": "Multidimensional Poverty Index", "co2_prod": "CO2 emissions per capita (production)",
}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(country: str = None, indicator: str = None, year: str = None) -> Any:
    if not API_KEY:
        return {"error": "The UNDP HDRO Data API requires a free API key: subscribe at https://hdrdata.org "
                         "and save the emailed key as UNDP_API_KEY in Settings > Credentials"}
    params = {"apikey": API_KEY}
    if country:
        params["countryOrAggregation"] = country.upper()
    if indicator:
        params["indicator"] = indicator
    if year:
        params["year"] = year
    try:
        response = session.get(f"{BASE_URL}/query", params=params, timeout=60)
        text = response.text.strip()
        if response.status_code in (401, 403) or text.startswith("The API key is invalid"):
            return {"error": "UNDP rejected the API key (UNDP_API_KEY) - check it in Settings > Credentials"}
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError):
        return {"error": f"Unexpected response from hdrdata.org: {text[:200]}"}
    return {"count": len(data) if isinstance(data, list) else None, "data": data, "source": "UNDP HDRO Data API"}


def _years(start_year: str = None, end_year: str = None) -> str:
    if start_year and end_year:
        return ",".join(str(y) for y in range(int(start_year), int(end_year) + 1))
    return start_year or end_year


def get_hdi(country: str = None, start_year: str = None, end_year: str = None) -> Any:
    return _make_request(country, "hdi", _years(start_year, end_year))


def get_gii(country: str = None, start_year: str = None, end_year: str = None) -> Any:
    return _make_request(country, "gii", _years(start_year, end_year))


def get_mpi(country: str = None, year: str = None) -> Any:
    return _make_request(country, "mpi_value", year)


def get_country_profile(country_code: str) -> Any:
    """All composite indices and components for one country."""
    return _make_request(country_code, None, None)


def get_indicators() -> Any:
    return {"indicators": COMPOSITE_INDICATORS}


def get_rankings(indicator: str = None, year: str = None) -> Any:
    """One indicator for every country in a year, sorted high to low."""
    res = _make_request(None, indicator or "hdi", year or "2023")
    rows = res.get("data") if isinstance(res, dict) else None
    if isinstance(rows, list):
        def val(r):
            try:
                return float(r.get("value"))
            except (TypeError, ValueError):
                return float("-inf")
        res["data"] = sorted(rows, key=val, reverse=True)
    return res


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "hdi":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        end_year = args[3] if len(args) > 3 else None
        result = get_hdi(country, start_year, end_year)
    elif command == "gii":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        end_year = args[3] if len(args) > 3 else None
        result = get_gii(country, start_year, end_year)
    elif command == "mpi":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_mpi(country, year)
    elif command == "profile":
        if len(args) < 2:
            result = {"error": "country_code required"}
        else:
            result = get_country_profile(args[1])
    elif command == "indicators":
        result = get_indicators()
    elif command == "rankings":
        indicator = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_rankings(indicator, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
