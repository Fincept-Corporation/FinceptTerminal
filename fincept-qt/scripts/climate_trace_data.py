"""
Climate TRACE Data Fetcher
GHG emissions for 350M+ assets globally, country/sector level, 2015-2025.
No API key required (beta).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.climatetrace.org/v4"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
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


def get_country_emissions(country: str, year: int = 2022) -> Any:
    """Get GHG emissions by country and year.
    country: ISO 3166-1 alpha-3 code (e.g. USA, CHN, DEU, IND, GBR).
    year: 2015-2023.
    """
    params = {"countries": country.upper(), "since": year, "to": year}
    data = _make_request("country/emissions", params=params)
    if isinstance(data, dict) and "error" not in data:
        return {"country": country.upper(), "year": year, "emissions": data}
    return data


def get_sector_emissions(sector: str, year: int = 2022) -> Any:
    """Get GHG emissions by sector and year.
    sector: agriculture, buildings, electricity-generation, fossil-fuel-operations,
            forestry-and-land-use, manufacturing, mineral-extraction, shipping,
            transportation, waste, oil-and-gas-production, etc.
    """
    params = {"sector": sector, "since": year, "to": year}
    data = _make_request("sector/emissions", params=params)
    if isinstance(data, dict) and "error" not in data:
        return {"sector": sector, "year": year, "emissions": data}
    return data


def get_asset_emissions(asset_id: str) -> Any:
    """Get detailed emissions data for a specific tracked asset by ID."""
    data = _make_request(f"asset/{asset_id}/emissions")
    if isinstance(data, dict) and "error" not in data:
        return {"asset_id": asset_id, "data": data}
    return data


def search_assets(name: str = None, country: str = None, sector: str = None, limit: int = 50) -> Any:
    """Search for emissions assets by name, country, and/or sector."""
    params = {"limit": min(limit, 100)}
    if name:
        params["name"] = name
    if country:
        params["countries"] = country.upper()
    if sector:
        params["sector"] = sector
    data = _make_request("assets", params=params)
    if isinstance(data, list):
        return {"assets": data, "count": len(data)}
    elif isinstance(data, dict) and "assets" in data:
        return {"assets": data["assets"], "count": len(data["assets"]), "total": data.get("total")}
    return data


def get_sectors() -> Any:
    """Get list of all available emission sectors."""
    data = _make_request("definitions/sectors")
    if isinstance(data, list):
        return {"sectors": data, "count": len(data)}
    elif isinstance(data, dict) and "sectors" in data:
        return {"sectors": data["sectors"], "count": len(data["sectors"])}
    return data


def get_summary(year: int = 2022) -> Any:
    """Get global emissions summary for a given year across all countries."""
    params = {"since": year, "to": year}
    data = _make_request("country/emissions", params=params)
    if isinstance(data, dict) and "error" not in data:
        return {"year": year, "summary": data}
    return data


def get_country_list() -> Any:
    """Get list of all countries tracked with emissions data."""
    data = _make_request("definitions/countries")
    if isinstance(data, list):
        return {"countries": data, "count": len(data)}
    elif isinstance(data, dict) and "countries" in data:
        return {"countries": data["countries"], "count": len(data["countries"])}
    return data


def get_asset_types() -> Any:
    """Get all asset/sub-sector types tracked in Climate TRACE."""
    data = _make_request("definitions/subsectors")
    if isinstance(data, list):
        return {"asset_types": data, "count": len(data)}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: country, sector, asset, search, sectors, summary, countries, asset_types"}))
        return

    command = args[0]

    if command == "country":
        if len(args) < 2:
            result = {"error": "Usage: country <iso3_code> [year] (e.g. country USA 2022)"}
        else:
            year = int(args[2]) if len(args) > 2 else 2022
            result = get_country_emissions(args[1], year)
    elif command == "sector":
        if len(args) < 2:
            result = {"error": "Usage: sector <sector_name> [year]"}
        else:
            year = int(args[2]) if len(args) > 2 else 2022
            result = get_sector_emissions(args[1], year)
    elif command == "asset":
        if len(args) < 2:
            result = {"error": "Usage: asset <asset_id>"}
        else:
            result = get_asset_emissions(args[1])
    elif command == "search":
        name = args[1] if len(args) > 1 else None
        country = args[2] if len(args) > 2 else None
        sector = args[3] if len(args) > 3 else None
        result = search_assets(name, country, sector)
    elif command == "sectors":
        result = get_sectors()
    elif command == "summary":
        year = int(args[1]) if len(args) > 1 else 2022
        result = get_summary(year)
    elif command == "countries":
        result = get_country_list()
    elif command == "asset_types":
        result = get_asset_types()
    else:
        result = {"error": f"Unknown command: {command}. Available: country, sector, asset, search, sectors, summary, countries, asset_types"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
