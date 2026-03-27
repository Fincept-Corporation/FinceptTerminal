"""
OECD Health Statistics Data Fetcher
Provides health expenditure, life expectancy, disease burden, pharmaceutical
market data, health workforce, and hospital statistics for OECD countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OECD_API_KEY', '')
BASE_URL = "https://sdmx.oecd.org/public/rest/data/OECD.ELS.HD"

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


def _fetch_oecd_stat(dataset: str, filter_expr: str, params: Dict = None) -> Any:
    """Fetch OECD SDMX-JSON data for a dataset and filter expression."""
    url = f"https://sdmx.oecd.org/public/rest/data/{dataset}/{filter_expr}"
    default_params = {"format": "jsondata", "dimensionAtObservation": "AllDimensions"}
    if params:
        default_params.update(params)
    try:
        response = session.get(url, params=default_params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_health_expenditure(country: str, year: int = None, type_: str = "TOT") -> Any:
    """Return health expenditure as % of GDP for a country."""
    filter_expr = f"{country}.SHA_HF.{type_}.PCC_GDP"
    if year:
        filter_expr += f"?startPeriod={year}&endPeriod={year}"
    data = _fetch_oecd_stat("OECD.ELS.HD,DSD_HEALTH_STAT@DF_SHA", filter_expr)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "type": type_, "expenditure_data": data}


def get_life_expectancy(country: str, year: int = None, sex: str = "T") -> Any:
    """Return life expectancy at birth for a country (sex: T=total, M=male, F=female)."""
    filter_expr = f"{country}.LIFEEXP.{sex}.0.Y"
    data = _fetch_oecd_stat("OECD.ELS.HD,DSD_HEALTH_STAT@DF_HEALTH_STATUS", filter_expr,
                             {"startPeriod": str(year), "endPeriod": str(year)} if year else None)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "sex": sex, "life_expectancy": data}


def get_disease_burden(country: str, cause: str = "ALL", year: int = None) -> Any:
    """Return disease burden (mortality rates, DALYs) for a country and cause."""
    filter_expr = f"{country}.MORT.{cause}"
    params = {}
    if year:
        params["startPeriod"] = str(year)
        params["endPeriod"] = str(year)
    data = _fetch_oecd_stat("OECD.ELS.HD,DSD_HEALTH_STAT@DF_HEALTH_STATUS", filter_expr, params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "cause": cause, "year": year, "disease_burden": data}


def get_pharmaceutical_market(country: str, year: int = None) -> Any:
    """Return pharmaceutical market size and per capita spending for a country."""
    filter_expr = f"{country}.PHARMAEXP.TOT.PC_GDP"
    params = {}
    if year:
        params["startPeriod"] = str(year)
        params["endPeriod"] = str(year)
    data = _fetch_oecd_stat("OECD.ELS.HD,DSD_HEALTH_STAT@DF_SHA", filter_expr, params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "pharma_market": data}


def get_health_workers(country: str, type_: str = "PHYS", year: int = None) -> Any:
    """Return health workforce counts per 1000 population (PHYS=physicians, NURS=nurses)."""
    filter_expr = f"{country}.{type_}.DENSITY"
    params = {}
    if year:
        params["startPeriod"] = str(year)
        params["endPeriod"] = str(year)
    data = _fetch_oecd_stat("OECD.ELS.HD,DSD_HEALTH_STAT@DF_HEALTH_RESOURCES", filter_expr, params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "type": type_, "year": year, "workforce": data}


def get_hospital_data(country: str, indicator: str = "HOSPBED", year: int = None) -> Any:
    """Return hospital infrastructure data (beds, admissions) for a country."""
    filter_expr = f"{country}.{indicator}"
    params = {}
    if year:
        params["startPeriod"] = str(year)
        params["endPeriod"] = str(year)
    data = _fetch_oecd_stat("OECD.ELS.HD,DSD_HEALTH_STAT@DF_HEALTH_RESOURCES", filter_expr, params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "indicator": indicator, "year": year, "hospital_data": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    country = args[1] if len(args) > 1 else ""
    if not country and command not in []:
        pass
    if command == "expenditure":
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            type_ = args[3] if len(args) > 3 else "TOT"
            result = get_health_expenditure(country, year, type_)
    elif command == "life_expectancy":
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            sex = args[3] if len(args) > 3 else "T"
            result = get_life_expectancy(country, year, sex)
    elif command == "disease":
        if not country:
            result = {"error": "country required"}
        else:
            cause = args[2] if len(args) > 2 else "ALL"
            year = int(args[3]) if len(args) > 3 else None
            result = get_disease_burden(country, cause, year)
    elif command == "pharma":
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_pharmaceutical_market(country, year)
    elif command == "workers":
        if not country:
            result = {"error": "country required"}
        else:
            type_ = args[2] if len(args) > 2 else "PHYS"
            year = int(args[3]) if len(args) > 3 else None
            result = get_health_workers(country, type_, year)
    elif command == "hospitals":
        if not country:
            result = {"error": "country required"}
        else:
            indicator = args[2] if len(args) > 2 else "HOSPBED"
            year = int(args[3]) if len(args) > 3 else None
            result = get_hospital_data(country, indicator, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
