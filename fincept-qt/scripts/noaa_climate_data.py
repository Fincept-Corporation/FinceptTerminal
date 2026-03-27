"""
NOAA Climate Data Online (CDO) Data Fetcher
Provides weather observations, climate normals, storm events, and sea level
data from NOAA's Climate Data Online REST API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('NOAA_CDO_TOKEN', '')
BASE_URL = "https://www.ncdc.noaa.gov/cdo-web/api/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {}
    if API_KEY:
        headers["token"] = API_KEY
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
        return {"error": "NOAA_CDO_TOKEN environment variable not set"}
    return None


def get_datasets() -> Any:
    """Return all available NOAA CDO datasets."""
    err = _check_api_key()
    if err:
        return err
    data = _make_request("datasets", params={"limit": 100})
    if isinstance(data, dict) and "error" in data:
        return data
    return {"datasets": data.get("results", []), "metadata": data.get("metadata", {})}


def get_data(datatype: str, station: str, start_date: str, end_date: str) -> Any:
    """Return climate data for a specific datatype, station, and date range (YYYY-MM-DD)."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "datasetid": "GHCND",
        "datatypeid": datatype,
        "stationid": station,
        "startdate": start_date,
        "enddate": end_date,
        "limit": 1000,
        "units": "metric"
    }
    data = _make_request("data", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"datatype": datatype, "station": station, "start": start_date, "end": end_date,
            "records": data.get("results", []), "metadata": data.get("metadata", {})}


def get_stations(location: str, datatype: str = None, limit: int = 25) -> Any:
    """Return weather stations for a location (e.g., FIPS:US), optionally filtered by datatype."""
    err = _check_api_key()
    if err:
        return err
    params = {"locationid": location, "limit": limit}
    if datatype:
        params["datatypeid"] = datatype
    data = _make_request("stations", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"location": location, "stations": data.get("results", []), "metadata": data.get("metadata", {})}


def get_datatypes(datacategory: str = None) -> Any:
    """Return available data types, optionally filtered by data category."""
    err = _check_api_key()
    if err:
        return err
    params = {"limit": 100}
    if datacategory:
        params["datacategoryid"] = datacategory
    data = _make_request("datatypes", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"datacategory": datacategory, "datatypes": data.get("results", []), "metadata": data.get("metadata", {})}


def get_locations(locationcategory: str = "CNTRY", limit: int = 50) -> Any:
    """Return available locations for a given category (CNTRY, ST, CITY, etc.)."""
    err = _check_api_key()
    if err:
        return err
    params = {"locationcategoryid": locationcategory, "limit": limit}
    data = _make_request("locations", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"locationcategory": locationcategory, "locations": data.get("results", []), "metadata": data.get("metadata", {})}


def get_climate_normals(station: str, datatype: str) -> Any:
    """Return 30-year climate normals for a station and datatype."""
    err = _check_api_key()
    if err:
        return err
    params = {
        "datasetid": "NORMAL_DLY",
        "stationid": station,
        "datatypeid": datatype,
        "startdate": "2010-01-01",
        "enddate": "2010-12-31",
        "limit": 366
    }
    data = _make_request("data", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"station": station, "datatype": datatype, "normals": data.get("results", [])}


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
    elif command == "data":
        if len(args) < 5:
            result = {"error": "datatype, station, start_date, end_date required"}
        else:
            result = get_data(args[1], args[2], args[3], args[4])
    elif command == "stations":
        location = args[1] if len(args) > 1 else ""
        if not location:
            result = {"error": "location required"}
        else:
            datatype = args[2] if len(args) > 2 else None
            limit = int(args[3]) if len(args) > 3 else 25
            result = get_stations(location, datatype, limit)
    elif command == "datatypes":
        datacategory = args[1] if len(args) > 1 else None
        result = get_datatypes(datacategory)
    elif command == "locations":
        locationcategory = args[1] if len(args) > 1 else "CNTRY"
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_locations(locationcategory, limit)
    elif command == "normals":
        if len(args) < 3:
            result = {"error": "station and datatype required"}
        else:
            result = get_climate_normals(args[1], args[2])
    print(json.dumps(result))


if __name__ == "__main__":
    main()
