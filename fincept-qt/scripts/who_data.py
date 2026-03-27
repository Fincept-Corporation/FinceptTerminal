"""
WHO Global Health Observatory (GHO) Data Fetcher
Provides mortality, disease burden, health systems, nutrition, and immunization
data for 200+ countries via the WHO GHO OData API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WHO_API_KEY', '')
BASE_URL = "https://ghoapi.azureedge.net/api"

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


def get_indicators() -> Any:
    """Return all available WHO GHO indicators."""
    data = _make_request("Indicator")
    if "error" in data:
        return data
    items = data.get("value", [])
    return {"indicators": items, "count": len(items)}


def get_indicator_data(indicator_code: str, country: str = None, year: int = None) -> Any:
    """Return data for a specific indicator, optionally filtered by country and year."""
    filters = []
    if country:
        filters.append(f"SpatialDim eq '{country.upper()}'")
    if year:
        filters.append(f"TimeDim eq {year}")
    params = {}
    if filters:
        params["$filter"] = " and ".join(filters)
    params["$top"] = 500
    data = _make_request(f"{indicator_code}", params=params)
    if "error" in data:
        return data
    items = data.get("value", [])
    return {"indicator": indicator_code, "country": country, "year": year, "data": items, "count": len(items)}


def get_countries() -> Any:
    """Return all countries available in the WHO GHO API."""
    data = _make_request("DIMENSION/COUNTRY/DimensionValues")
    if "error" in data:
        return data
    items = data.get("value", [])
    return {"countries": items, "count": len(items)}


def get_dimensions(indicator_code: str) -> Any:
    """Return dimension metadata for a specific indicator."""
    data = _make_request(f"Indicator/{indicator_code}/Dimensions")
    if "error" in data:
        return data
    items = data.get("value", [])
    return {"indicator": indicator_code, "dimensions": items, "count": len(items)}


def get_health_topics() -> Any:
    """Return all WHO health topics/categories."""
    data = _make_request("IndicatorCategory")
    if "error" in data:
        return data
    items = data.get("value", [])
    return {"topics": items, "count": len(items)}


def get_data_by_country(country: str, limit: int = 100) -> Any:
    """Return all available indicator data for a given country."""
    params = {
        "$filter": f"SpatialDim eq '{country.upper()}'",
        "$top": limit
    }
    data = _make_request("Indicator", params=params)
    if "error" in data:
        return data
    items = data.get("value", [])
    return {"country": country, "data": items, "count": len(items)}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "indicators":
        result = get_indicators()
    elif command == "data":
        indicator_code = args[1] if len(args) > 1 else ""
        country = args[2] if len(args) > 2 else None
        year = int(args[3]) if len(args) > 3 else None
        if not indicator_code:
            result = {"error": "indicator_code required"}
        else:
            result = get_indicator_data(indicator_code, country, year)
    elif command == "countries":
        result = get_countries()
    elif command == "dimensions":
        indicator_code = args[1] if len(args) > 1 else ""
        if not indicator_code:
            result = {"error": "indicator_code required"}
        else:
            result = get_dimensions(indicator_code)
    elif command == "topics":
        result = get_health_topics()
    elif command == "country_data":
        country = args[1] if len(args) > 1 else ""
        limit = int(args[2]) if len(args) > 2 else 100
        if not country:
            result = {"error": "country required"}
        else:
            result = get_data_by_country(country, limit)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
