"""
Climate TRACE Data Fetcher
GHG emissions for 350M+ assets globally, country/sector level, 2015-2024.
No API key required (Climate TRACE API v6).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

# v4 was removed; v6 is the current public (keyless) API.
BASE_URL = "https://api.climatetrace.org/v6"
# Sector names accepted by the API are the top-level sectors (see `sectors`); map common aliases.
SECTOR_ALIASES = {"electricity-generation": "power", "electricity": "power", "oil-and-gas-production": "fossil-fuel-operations",
                  "shipping": "transportation", "aviation": "transportation"}

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


def get_country_emissions(country: str, year: int = 2024) -> Any:
    """Get GHG emissions by country and year.
    country: ISO 3166-1 alpha-3 code (e.g. USA, CHN, DEU, IND, GBR).
    year: 2015-2024.
    """
    params = {"countries": country.upper(), "since": year, "to": year}
    data = _make_request("country/emissions", params=params)
    if isinstance(data, list):
        data = data[0] if data else {}
    if isinstance(data, dict) and "error" not in data:
        return {"country": country.upper(), "year": year, "emissions": data}
    return data


def get_sector_emissions(sector: str, year: int = 2024) -> Any:
    """Get GHG emissions by sector and year.
    sector: agriculture, buildings, electricity-generation, fossil-fuel-operations,
            forestry-and-land-use, manufacturing, mineral-extraction, shipping,
            transportation, waste, oil-and-gas-production, etc.
    """
    params = {"sectors": SECTOR_ALIASES.get(sector, sector), "since": year, "to": year}
    data = _make_request("country/emissions", params=params)
    if isinstance(data, dict) and "error" not in data:
        return {"sector": sector, "year": year, "emissions": data}
    return data


def get_asset_emissions(asset_id: str) -> Any:
    """Get detailed emissions data for a specific tracked asset by ID."""
    data = _make_request(f"assets/{asset_id}")
    if isinstance(data, dict) and "error" not in data:
        return {"asset_id": asset_id, "data": data}
    return data


def search_assets(country: str = None, sector: str = None, limit: int = 50) -> Any:
    """List tracked emission sources (largest first) filtered by country (ISO-3) and/or sector."""
    params = {"limit": min(limit, 100)}
    if country:
        params["countries"] = country.upper()
    if sector:
        params["sectors"] = SECTOR_ALIASES.get(sector, sector)
    data = _make_request("assets", params=params)
    if isinstance(data, dict) and "assets" in data:
        assets = []
        for a in data["assets"]:
            summ = {e.get("Gas"): e.get("EmissionsQuantity") for e in a.get("EmissionsSummary") or []}
            assets.append({"id": a.get("Id"), "name": a.get("Name"), "country": a.get("Country"),
                           "sector": a.get("Sector"), "asset_type": a.get("AssetType"),
                           "emissions_co2e_100yr_t": summ.get("co2e_100yr"),
                           "centroid": a.get("Centroid", {}).get("Geometry") if isinstance(a.get("Centroid"), dict) else None})
        return {"assets": assets, "count": len(assets)}
    return data


def get_sectors() -> Any:
    """Get list of all available emission sectors."""
    data = _make_request("definitions/sectors")
    if isinstance(data, list):
        return {"sectors": data, "count": len(data)}
    elif isinstance(data, dict) and "sectors" in data:
        return {"sectors": data["sectors"], "count": len(data["sectors"])}
    return data


def get_summary(year: int = 2024) -> Any:
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
            year = int(args[2]) if len(args) > 2 else 2024
            result = get_country_emissions(args[1], year)
    elif command == "sector":
        if len(args) < 2:
            result = {"error": "Usage: sector <sector_name> [year]"}
        else:
            year = int(args[2]) if len(args) > 2 else 2024
            result = get_sector_emissions(args[1], year)
    elif command == "asset":
        if len(args) < 2:
            result = {"error": "Usage: asset <asset_id>"}
        else:
            result = get_asset_emissions(args[1])
    elif command == "search":
        country = args[1] if len(args) > 1 else None
        sector = args[2] if len(args) > 2 else None
        limit = int(args[3]) if len(args) > 3 else 50
        result = search_assets(country, sector, limit)
    elif command == "sectors":
        result = get_sectors()
    elif command == "summary":
        year = int(args[1]) if len(args) > 1 else 2024
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
