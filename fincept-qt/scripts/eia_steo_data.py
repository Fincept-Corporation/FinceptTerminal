"""
EIA Short-Term Energy Outlook (STEO) Data Fetcher
Energy forecasts, oil/gas/coal/renewables projections by quarter
via EIA API v2 (free key required via EIA_API_KEY).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EIA_API_KEY', '')
BASE_URL = "https://api.eia.gov/v2/steo"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["api_key"] = API_KEY
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


OIL_SERIES = {
    "crude_production": "COPR_US",
    "crude_price_wti": "WTIPUUS",
    "crude_price_brent": "BREPUUS",
    "crude_imports": "COIMUUS",
    "crude_exports": "COEXUUS",
    "refinery_runs": "CORIMFUUS"
}

GAS_SERIES = {
    "production": "NGPRUUS",
    "consumption": "NGCCUUS",
    "price_henry_hub": "NGHHUUS",
    "exports": "NGEXUUS",
    "imports": "NGIMUUS"
}

ELEC_SERIES = {
    "residential": "ESIRUUS",
    "commercial": "ESICUUS",
    "industrial": "ESIIUUS",
    "total": "ESITUUS",
    "retail_price": "ESRPUUS"
}

COAL_SERIES = {
    "production": "CLPRUUS",
    "consumption": "CLCCUUS",
    "exports": "CLEXUUS",
    "price": "CLPPUUS"
}


def get_steo_series(series_id: str, start_period: str = None, end_period: str = None) -> Any:
    params = {
        "data[]": "value",
        "facets[seriesId][]": series_id,
        "sort[0][column]": "period",
        "sort[0][direction]": "desc",
        "offset": 0,
        "length": 48
    }
    if start_period:
        params["start"] = start_period
    if end_period:
        params["end"] = end_period
    data = _make_request("data/", params)
    return {"series_id": series_id, "start": start_period, "end": end_period, "data": data}


def get_oil_forecast(product: str = "crude_price_wti", start: str = None, end: str = None) -> Any:
    series_id = OIL_SERIES.get(product.lower(), product)
    return get_steo_series(series_id, start, end)


def get_natural_gas_forecast(start: str = None, end: str = None) -> Any:
    results = {}
    for name, series_id in GAS_SERIES.items():
        results[name] = get_steo_series(series_id, start, end)
    return {"forecast_type": "natural_gas", "start": start, "end": end, "series": results}


def get_electricity_forecast(sector: str = "total", start: str = None, end: str = None) -> Any:
    series_id = ELEC_SERIES.get(sector.lower(), sector)
    return get_steo_series(series_id, start, end)


def get_coal_forecast(start: str = None, end: str = None) -> Any:
    results = {}
    for name, series_id in COAL_SERIES.items():
        results[name] = get_steo_series(series_id, start, end)
    return {"forecast_type": "coal", "start": start, "end": end, "series": results}


def get_available_series() -> Any:
    data = _make_request("")
    series_catalog = {
        "oil": OIL_SERIES,
        "natural_gas": GAS_SERIES,
        "electricity": ELEC_SERIES,
        "coal": COAL_SERIES,
        "description": "EIA STEO series IDs — use with 'series' command",
        "api_response": data
    }
    return series_catalog


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "series":
        series_id = args[1] if len(args) > 1 else "WTIPUUS"
        start_period = args[2] if len(args) > 2 else None
        end_period = args[3] if len(args) > 3 else None
        result = get_steo_series(series_id, start_period, end_period)
    elif command == "oil":
        product = args[1] if len(args) > 1 else "crude_price_wti"
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        result = get_oil_forecast(product, start, end)
    elif command == "natural_gas":
        start = args[1] if len(args) > 1 else None
        end = args[2] if len(args) > 2 else None
        result = get_natural_gas_forecast(start, end)
    elif command == "electricity":
        sector = args[1] if len(args) > 1 else "total"
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        result = get_electricity_forecast(sector, start, end)
    elif command == "coal":
        start = args[1] if len(args) > 1 else None
        end = args[2] if len(args) > 2 else None
        result = get_coal_forecast(start, end)
    elif command == "available":
        result = get_available_series()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
