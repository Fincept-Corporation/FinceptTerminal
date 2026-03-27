"""
Energy Transition Data Fetcher
Provides global renewable energy capacity, energy transition indicators,
clean technology deployment, EV adoption, and CO2 intensity via IEA and IRENA open data.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IEA_API_KEY', '')
BASE_URL = "https://api.iea.org/stats"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {}
    if API_KEY:
        headers["Authorization"] = f"Bearer {API_KEY}"
    try:
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_renewable_capacity(country: str = None, technology: str = None, year: int = None) -> Any:
    """Return installed renewable energy capacity (GW) by country and technology."""
    params = {}
    if country:
        params["country"] = country
    if technology:
        params["product"] = technology
    if year:
        params["year"] = year
    params["flow"] = "REPOWCAP"
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        # Fall back to IRENA public data structure
        return {"country": country, "technology": technology, "year": year,
                "source": "IEA/IRENA", "note": "Capacity data requires IEA_API_KEY", "data": []}
    return {"country": country, "technology": technology, "year": year, "data": data}


def get_energy_mix(country: str, year: int = None) -> Any:
    """Return total primary energy supply by source for a country."""
    params = {"country": country, "flow": "TES"}
    if year:
        params["year"] = year
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        return {"country": country, "year": year, "note": "Energy mix data requires IEA_API_KEY", "data": []}
    return {"country": country, "year": year, "energy_mix": data}


def get_co2_intensity(country: str, year: int = None) -> Any:
    """Return CO2 intensity of electricity generation (gCO2/kWh) for a country."""
    params = {"country": country, "flow": "CO2ELEC"}
    if year:
        params["year"] = year
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        return {"country": country, "year": year, "note": "CO2 intensity data requires IEA_API_KEY", "data": []}
    return {"country": country, "year": year, "co2_intensity": data}


def get_energy_investment(country: str, sector: str = None, year: int = None) -> Any:
    """Return clean energy investment flows by country and sector."""
    params = {"country": country, "flow": "CLEANINV"}
    if sector:
        params["product"] = sector
    if year:
        params["year"] = year
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        return {"country": country, "sector": sector, "year": year,
                "note": "Investment data requires IEA_API_KEY", "data": []}
    return {"country": country, "sector": sector, "year": year, "investment": data}


def get_ev_adoption(country: str, year: int = None) -> Any:
    """Return electric vehicle stock and market share data for a country."""
    params = {"country": country, "flow": "EVSTOCK"}
    if year:
        params["year"] = year
    data = _make_request("", params=params)
    if isinstance(data, dict) and "error" in data:
        return {"country": country, "year": year, "note": "EV data requires IEA_API_KEY", "data": []}
    return {"country": country, "year": year, "ev_data": data}


def get_countries() -> Any:
    """Return all countries available in the IEA statistics database."""
    data = _make_request("countries")
    if isinstance(data, dict) and "error" in data:
        # Return known IEA member countries as fallback
        iea_members = ["AUS", "AUT", "BEL", "CAN", "CZE", "DNK", "EST", "FIN", "FRA",
                        "DEU", "GRC", "HUN", "IRL", "ITA", "JPN", "KOR", "LVA", "LTU",
                        "LUX", "MEX", "NLD", "NZL", "NOR", "POL", "PRT", "SVK", "SVN",
                        "ESP", "SWE", "CHE", "TUR", "GBR", "USA"]
        return {"countries": iea_members, "count": len(iea_members), "source": "IEA member list"}
    return {"countries": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "capacity":
        country = args[1] if len(args) > 1 else None
        technology = args[2] if len(args) > 2 else None
        year = int(args[3]) if len(args) > 3 else None
        result = get_renewable_capacity(country, technology, year)
    elif command == "mix":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_energy_mix(country, year)
    elif command == "co2":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_co2_intensity(country, year)
    elif command == "investment":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            sector = args[2] if len(args) > 2 else None
            year = int(args[3]) if len(args) > 3 else None
            result = get_energy_investment(country, sector, year)
    elif command == "ev":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_ev_adoption(country, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
