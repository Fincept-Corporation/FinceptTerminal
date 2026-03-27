"""
Open-Meteo Data Fetcher
Global weather forecasts + 80yr historical for any lat/lon, free, no API key.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.open-meteo.com/v1"
ARCHIVE_URL = "https://archive-api.open-meteo.com/v1"
AIR_QUALITY_URL = "https://air-quality-api.open-meteo.com/v1"
MARINE_URL = "https://marine-api.open-meteo.com/v1"
FLOOD_URL = "https://flood-api.open-meteo.com/v1"
GEOCODING_URL = "https://geocoding-api.open-meteo.com/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None, base: str = None) -> Any:
    """Make HTTP request with error handling."""
    base_url = base if base else BASE_URL
    url = f"{base_url}/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def geocode(location: str) -> Optional[Dict]:
    """Geocode a location name to lat/lon coordinates."""
    params = {"name": location, "count": 1, "language": "en", "format": "json"}
    data = _make_request("search", params=params, base=GEOCODING_URL)
    if isinstance(data, dict) and "results" in data and data["results"]:
        r = data["results"][0]
        return {
            "latitude": r.get("latitude"),
            "longitude": r.get("longitude"),
            "name": r.get("name"),
            "country": r.get("country"),
        }
    return None


def get_forecast(lat: float, lon: float, variables: str = None) -> Any:
    """Get 7-day weather forecast for a lat/lon location.
    variables: comma-separated list of hourly variables, or None for defaults.
    Default: temperature_2m, precipitation, windspeed_10m, weathercode, relativehumidity_2m.
    """
    default_hourly = "temperature_2m,precipitation,windspeed_10m,weathercode,relativehumidity_2m,apparent_temperature"
    default_daily = "weathercode,temperature_2m_max,temperature_2m_min,precipitation_sum,windspeed_10m_max,sunrise,sunset"
    params = {
        "latitude": lat,
        "longitude": lon,
        "hourly": variables if variables else default_hourly,
        "daily": default_daily,
        "timezone": "auto",
        "forecast_days": 7,
    }
    return _make_request("forecast", params=params)


def get_historical(lat: float, lon: float, start_date: str, end_date: str, variables: str = None) -> Any:
    """Get historical weather data via ERA5 reanalysis (back to 1940).
    start_date/end_date: YYYY-MM-DD format.
    """
    default_vars = "temperature_2m,precipitation,windspeed_10m,weathercode,shortwave_radiation"
    params = {
        "latitude": lat,
        "longitude": lon,
        "start_date": start_date,
        "end_date": end_date,
        "hourly": variables if variables else default_vars,
        "timezone": "auto",
    }
    return _make_request("archive", params=params, base=ARCHIVE_URL)


def get_air_quality(lat: float, lon: float) -> Any:
    """Get air quality forecast (PM2.5, PM10, NO2, CO, SO2, O3, AQI) for a location."""
    params = {
        "latitude": lat,
        "longitude": lon,
        "hourly": "pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,aerosol_optical_depth,dust,uv_index,european_aqi,us_aqi",
        "forecast_days": 5,
        "timezone": "auto",
    }
    return _make_request("air-quality", params=params, base=AIR_QUALITY_URL)


def get_marine(lat: float, lon: float) -> Any:
    """Get marine wave forecast (wave height, direction, period) for ocean coordinates."""
    params = {
        "latitude": lat,
        "longitude": lon,
        "hourly": "wave_height,wave_direction,wave_period,wind_wave_height,swell_wave_height,swell_wave_direction,swell_wave_period",
        "daily": "wave_height_max,wave_direction_dominant,wind_wave_height_max,swell_wave_height_max",
        "forecast_days": 7,
        "timezone": "auto",
    }
    return _make_request("marine", params=params, base=MARINE_URL)


def get_flood(lat: float, lon: float) -> Any:
    """Get flood/river discharge forecast for a location."""
    params = {
        "latitude": lat,
        "longitude": lon,
        "daily": "river_discharge,river_discharge_max,river_discharge_min,river_discharge_mean",
        "forecast_days": 16,
    }
    return _make_request("flood", params=params, base=FLOOD_URL)


def get_climate_change(lat: float, lon: float, start_date: str = "1950-01-01", end_date: str = "2050-12-31") -> Any:
    """Get long-term climate change projections (CMIP6 models)."""
    params = {
        "latitude": lat,
        "longitude": lon,
        "start_date": start_date,
        "end_date": end_date,
        "models": "MRI_AGCM3_2_S,EC_Earth3P_HR",
        "daily": "temperature_2m_max,temperature_2m_min,precipitation_sum,windspeed_10m_max",
    }
    return _make_request("climate", params=params, base="https://climate-api.open-meteo.com/v1")


def _parse_coords(args, offset=1):
    """Parse lat/lon from args, with optional location string geocoding."""
    if len(args) > offset + 1:
        try:
            lat = float(args[offset])
            lon = float(args[offset + 1])
            return lat, lon, offset + 2
        except ValueError:
            pass
    if len(args) > offset:
        loc = " ".join(args[offset:])
        geo = geocode(loc)
        if geo:
            return geo["latitude"], geo["longitude"], len(args)
    return None, None, offset


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: forecast, historical, air_quality, marine, flood, climate, geocode"}))
        return

    command = args[0]

    if command == "forecast":
        if len(args) < 3:
            result = {"error": "Usage: forecast <lat> <lon> [variables] OR forecast <city_name>"}
        else:
            lat, lon, next_idx = _parse_coords(args)
            if lat is None:
                result = {"error": "Could not determine coordinates"}
            else:
                variables = args[next_idx] if next_idx < len(args) else None
                result = get_forecast(lat, lon, variables)
    elif command == "historical":
        if len(args) < 5:
            result = {"error": "Usage: historical <lat> <lon> <start_date YYYY-MM-DD> <end_date YYYY-MM-DD> [variables]"}
        else:
            lat, lon, next_idx = _parse_coords(args)
            if lat is None or next_idx + 1 >= len(args):
                result = {"error": "Could not parse lat/lon or missing dates"}
            else:
                start_date = args[next_idx]
                end_date = args[next_idx + 1]
                variables = args[next_idx + 2] if next_idx + 2 < len(args) else None
                result = get_historical(lat, lon, start_date, end_date, variables)
    elif command == "air_quality":
        if len(args) < 3:
            result = {"error": "Usage: air_quality <lat> <lon> OR air_quality <city_name>"}
        else:
            lat, lon, _ = _parse_coords(args)
            if lat is None:
                result = {"error": "Could not determine coordinates"}
            else:
                result = get_air_quality(lat, lon)
    elif command == "marine":
        if len(args) < 3:
            result = {"error": "Usage: marine <lat> <lon>"}
        else:
            lat, lon, _ = _parse_coords(args)
            if lat is None:
                result = {"error": "Could not determine coordinates"}
            else:
                result = get_marine(lat, lon)
    elif command == "flood":
        if len(args) < 3:
            result = {"error": "Usage: flood <lat> <lon>"}
        else:
            lat, lon, _ = _parse_coords(args)
            if lat is None:
                result = {"error": "Could not determine coordinates"}
            else:
                result = get_flood(lat, lon)
    elif command == "climate":
        if len(args) < 3:
            result = {"error": "Usage: climate <lat> <lon> [start_year] [end_year]"}
        else:
            lat, lon, next_idx = _parse_coords(args)
            if lat is None:
                result = {"error": "Could not determine coordinates"}
            else:
                start = f"{args[next_idx]}-01-01" if next_idx < len(args) else "1950-01-01"
                end = f"{args[next_idx + 1]}-12-31" if next_idx + 1 < len(args) else "2050-12-31"
                result = get_climate_change(lat, lon, start, end)
    elif command == "geocode":
        if len(args) < 2:
            result = {"error": "Usage: geocode <location_name>"}
        else:
            location = " ".join(args[1:])
            geo = geocode(location)
            result = geo if geo else {"error": f"Location not found: {location}"}
    else:
        result = {"error": f"Unknown command: {command}. Available: forecast, historical, air_quality, marine, flood, climate, geocode"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
