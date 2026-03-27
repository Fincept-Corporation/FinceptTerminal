"""
Open Budget Survey & GIFT Fiscal Openness Data Fetcher
Provides government budget transparency scores, Open Budget Index (OBI),
fiscal openness rankings, and governance dimensions for countries globally.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OBS_API_KEY', '')
BASE_URL = "https://www.internationalbudget.org/api"

# Public OBS data is available via IBP GitHub and API endpoints
OBS_DATA_URL = "https://raw.githubusercontent.com/IBPNetwork/obs-data/main/data"

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


def _fetch_obs_json(path: str) -> Any:
    """Fetch OBS data from the IBP public data repository."""
    url = f"{OBS_DATA_URL}/{path}"
    try:
        response = session.get(url, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_obs_scores(year: int = 2023, country: str = None) -> Any:
    """Return Open Budget Index scores for all countries in a survey year."""
    data = _make_request(f"obs/scores/{year}")
    if isinstance(data, dict) and "error" in data:
        # Fallback to known OBS survey years with static structure
        obs_years = [2006, 2008, 2010, 2012, 2015, 2017, 2019, 2021, 2023]
        return {"year": year, "available_years": obs_years,
                "note": "OBS scores available for survey years only", "data": []}
    return {"year": year, "country_filter": country, "scores": data}


def get_country_score(country: str, year: int = 2021) -> Any:
    """Return the full OBI score breakdown for a specific country and survey year."""
    data = _make_request(f"obs/country/{country}/{year}")
    if isinstance(data, dict) and "error" in data:
        return {"country": country, "year": year, "note": "Direct API access may require credentials", "data": {}}
    return {"country": country, "year": year, "score": data}


def get_rankings(year: int = 2021, dimension: str = "obi") -> Any:
    """Return country rankings by OBI score or a specific dimension."""
    valid_dims = ["obi", "transparency", "oversight", "participation"]
    if dimension not in valid_dims:
        return {"error": f"dimension must be one of: {valid_dims}"}
    data = _make_request(f"obs/rankings/{year}/{dimension}")
    if isinstance(data, dict) and "error" in data:
        return {"year": year, "dimension": dimension, "note": "Rankings data requires API access", "rankings": []}
    return {"year": year, "dimension": dimension, "rankings": data}


def get_historical_trends(country: str) -> Any:
    """Return historical OBI scores across all available survey years for a country."""
    years = [2006, 2008, 2010, 2012, 2015, 2017, 2019, 2021]
    history = []
    for year in years:
        data = _make_request(f"obs/country/{country}/{year}")
        if not (isinstance(data, dict) and "error" in data):
            history.append({"year": year, "data": data})
    return {"country": country, "history": history, "years_covered": [h["year"] for h in history]}


def get_regional_averages(year: int = 2021, region: str = None) -> Any:
    """Return regional average OBI scores for a survey year."""
    params = {"year": year}
    if region:
        params["region"] = region
    data = _make_request(f"obs/regional/{year}", params=params)
    if isinstance(data, dict) and "error" in data:
        # Return known regions structure
        regions = ["Sub-Saharan Africa", "Asia & Pacific", "Eastern Europe & Central Asia",
                   "Latin America & Caribbean", "Middle East & North Africa",
                   "South Asia", "Western Europe & North America"]
        return {"year": year, "regions": regions,
                "note": "Regional averages require API access", "averages": {}}
    return {"year": year, "region": region, "regional_averages": data}


def get_dimensions() -> Any:
    """Return all OBS assessment dimensions and their descriptions."""
    dimensions = [
        {"id": "obi", "name": "Open Budget Index", "description": "Composite transparency score 0-100"},
        {"id": "transparency", "name": "Budget Transparency", "description": "Availability of budget documents"},
        {"id": "oversight", "name": "Legislative Oversight", "description": "Legislature's budget oversight capacity"},
        {"id": "participation", "name": "Public Participation", "description": "Mechanisms for citizen budget engagement"},
        {"id": "audit", "name": "Supreme Audit", "description": "Independence and effectiveness of supreme audit institution"}
    ]
    return {"dimensions": dimensions, "count": len(dimensions)}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "scores":
        year = int(args[1]) if len(args) > 1 else 2021
        country = args[2] if len(args) > 2 else None
        result = get_obs_scores(year, country)
    elif command == "country":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else 2021
            result = get_country_score(country, year)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else 2021
        dimension = args[2] if len(args) > 2 else "obi"
        result = get_rankings(year, dimension)
    elif command == "trends":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            result = get_historical_trends(country)
    elif command == "regional":
        year = int(args[1]) if len(args) > 1 else 2021
        region = args[2] if len(args) > 2 else None
        result = get_regional_averages(year, region)
    elif command == "dimensions":
        result = get_dimensions()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
