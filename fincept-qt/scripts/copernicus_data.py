"""
Copernicus Climate Change Service (C3S) Data Fetcher
Provides ERA5 reanalysis data, seasonal forecasts, climate indicators,
sea level data, and temperature anomalies via the CDS API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('COPERNICUS_API_KEY', '')
BASE_URL = "https://cds.climate.copernicus.eu/api/v2"

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


def _check_api_key() -> Optional[Dict]:
    if not API_KEY:
        return {"error": "COPERNICUS_API_KEY environment variable not set"}
    return None


def get_datasets() -> Any:
    """Return all publicly available datasets in the Copernicus CDS catalogue."""
    err = _check_api_key()
    if err:
        return err
    data = _make_request("resources")
    if isinstance(data, dict) and "error" in data:
        return data
    return {"datasets": data}


def get_era5_monthly(variable: str, year: int, month: int, area: List[float] = None) -> Any:
    """Return ERA5 monthly averaged reanalysis data for a variable, year, and month."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "variable": variable,
        "year": str(year),
        "month": str(month).zfill(2),
        "product_type": "monthly_averaged_reanalysis",
        "format": "json"
    }
    if area:
        params["area"] = area
    data = _make_request("resources/reanalysis-era5-single-levels-monthly-means", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"variable": variable, "year": year, "month": month, "data": data}


def get_temperature_anomaly(year: int, month: int) -> Any:
    """Return global mean surface temperature anomaly for a given year and month."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "variable": "2m_temperature",
        "year": str(year),
        "month": str(month).zfill(2),
        "product_type": "monthly_averaged_reanalysis_by_hour_of_day",
        "format": "json"
    }
    data = _make_request("resources/reanalysis-era5-single-levels-monthly-means", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"year": year, "month": month, "anomaly_data": data}


def get_sea_level_indicators(year: int) -> Any:
    """Return global mean sea level indicators for a given year."""
    err = _check_api_key()
    if err:
        return err
    params = {"year": str(year), "format": "json"}
    data = _make_request("resources/sea-level-gridded-data-from-satellite-observations", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"year": year, "sea_level_data": data}


def get_climate_indices(index: str, year: int) -> Any:
    """Return a climate index (e.g., ENSO, NAO, PDO) for a given year."""
    err = _check_api_key()
    if err:
        return err
    params = {"climate_index": index, "year": str(year), "format": "json"}
    data = _make_request("resources/climate-indices", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"index": index, "year": year, "data": data}


def get_dataset_info(dataset_name: str) -> Any:
    """Return metadata and available variables for a specific CDS dataset."""
    err = _check_api_key()
    if err:
        return err
    data = _make_request(f"resources/{dataset_name}")
    if isinstance(data, dict) and "error" in data:
        return data
    return {"dataset": dataset_name, "info": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "datasets":
        result = get_datasets()
    elif command == "era5":
        if len(args) < 4:
            result = {"error": "variable, year, month required"}
        else:
            variable = args[1]
            year = int(args[2])
            month = int(args[3])
            result = get_era5_monthly(variable, year, month)
    elif command == "temperature":
        if len(args) < 3:
            result = {"error": "year and month required"}
        else:
            result = get_temperature_anomaly(int(args[1]), int(args[2]))
    elif command == "sea_level":
        year = int(args[1]) if len(args) > 1 else 2023
        result = get_sea_level_indicators(year)
    elif command == "indices":
        if len(args) < 3:
            result = {"error": "index and year required"}
        else:
            result = get_climate_indices(args[1], int(args[2]))
    elif command == "info":
        dataset_name = args[1] if len(args) > 1 else ""
        if not dataset_name:
            result = {"error": "dataset_name required"}
        else:
            result = get_dataset_info(dataset_name)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
