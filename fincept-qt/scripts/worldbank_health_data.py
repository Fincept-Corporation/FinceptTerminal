"""
World Bank Health/Education/Poverty Data Fetcher
Provides life expectancy, literacy, Gini coefficient, HDI-related indicators,
and poverty headcount data for countries worldwide.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLDBANK_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2"

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


def _fetch_indicator(indicator: str, country: str, start_year: int = None, end_year: int = None) -> Any:
    """Fetch a World Bank indicator for a country over a date range."""
    params = {"format": "json", "per_page": 100}
    date_range = ""
    if start_year and end_year:
        date_range = f"{start_year}:{end_year}"
    elif start_year:
        date_range = str(start_year)
    if date_range:
        params["date"] = date_range
    data = _make_request(f"country/{country}/indicator/{indicator}", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    if isinstance(data, list) and len(data) >= 2:
        meta = data[0]
        records = data[1] or []
        return {"indicator": indicator, "country": country, "records": records, "total": meta.get("total", 0)}
    return {"error": "Unexpected response format", "raw": data}


def get_life_expectancy(country: str, start_year: int = None, end_year: int = None) -> Any:
    """Life expectancy at birth, total (years) — SP.DYN.LE00.IN."""
    return _fetch_indicator("SP.DYN.LE00.IN", country, start_year, end_year)


def get_infant_mortality(country: str, start_year: int = None, end_year: int = None) -> Any:
    """Infant mortality rate (per 1,000 live births) — SP.DYN.IMRT.IN."""
    return _fetch_indicator("SP.DYN.IMRT.IN", country, start_year, end_year)


def get_literacy_rate(country: str, start_year: int = None, end_year: int = None) -> Any:
    """Literacy rate, adult total (% of people ages 15 and above) — SE.ADT.LITR.ZS."""
    return _fetch_indicator("SE.ADT.LITR.ZS", country, start_year, end_year)


def get_gini_index(country: str, start_year: int = None, end_year: int = None) -> Any:
    """Gini index (World Bank estimate) — SI.POV.GINI."""
    return _fetch_indicator("SI.POV.GINI", country, start_year, end_year)


def get_hdi_proxy(country: str, year: int = None) -> Any:
    """Human Development Index proxy via GNI per capita (constant 2017 PPP$) — NY.GNP.PCAP.PP.KD."""
    start = year
    end = year
    return _fetch_indicator("NY.GNP.PCAP.PP.KD", country, start, end)


def get_poverty_headcount(country: str, year: int = None) -> Any:
    """Poverty headcount ratio at $2.15/day (2017 PPP, % of population) — SI.POV.DDAY."""
    start = year
    end = year
    return _fetch_indicator("SI.POV.DDAY", country, start, end)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    country = args[1] if len(args) > 1 else "WLD"
    start_year = int(args[2]) if len(args) > 2 else None
    end_year = int(args[3]) if len(args) > 3 else None
    if command == "life_expectancy":
        result = get_life_expectancy(country, start_year, end_year)
    elif command == "infant_mortality":
        result = get_infant_mortality(country, start_year, end_year)
    elif command == "literacy":
        result = get_literacy_rate(country, start_year, end_year)
    elif command == "gini":
        result = get_gini_index(country, start_year, end_year)
    elif command == "hdi":
        year = int(args[2]) if len(args) > 2 else None
        result = get_hdi_proxy(country, year)
    elif command == "poverty":
        year = int(args[2]) if len(args) > 2 else None
        result = get_poverty_headcount(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
