"""
Eurostat SDMX 2.1 Data Fetcher
GDP, employment, trade, prices, population for all EU/EEA countries.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://ec.europa.eu/eurostat/api/dissemination/sdmx/2.1/data"
META_URL = "https://ec.europa.eu/eurostat/api/dissemination/sdmx/2.1"
SEARCH_URL = "https://ec.europa.eu/eurostat/api/dissemination/catalogue/datasets"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})


def _make_request(endpoint: str, params: Dict = None, base: str = None) -> Any:
    """Make HTTP request with error handling."""
    base_url = base if base else BASE_URL
    url = f"{base_url}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=60)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _parse_eurostat_sdmx(data: Any, label: str = "series") -> Any:
    """Parse Eurostat SDMX-JSON response into simplified time series."""
    if isinstance(data, dict) and "error" in data:
        return data
    try:
        datasets = data.get("dataSets", [])
        if not datasets:
            return {"error": "No dataSets", "keys": list(data.keys())}
        dataset = datasets[0]
        structure = data.get("structure", {})
        dims_s = structure.get("dimensions", {}).get("series", [])
        dims_o = structure.get("dimensions", {}).get("observation", [])

        # Get time values
        time_values = []
        for d in dims_o:
            if "TIME" in d.get("id", ""):
                time_values = [v.get("id") for v in d.get("values", [])]

        series_list = []
        for key, series_obj in dataset.get("series", {}).items():
            parts = key.split(":")
            dim_labels = {}
            for i, dim in enumerate(dims_s):
                if i < len(parts):
                    try:
                        idx = int(parts[i])
                        vals = dim.get("values", [])
                        if idx < len(vals):
                            dim_labels[dim["id"]] = vals[idx].get("id", "")
                    except ValueError:
                        pass

            obs = []
            for obs_k, obs_v in series_obj.get("observations", {}).items():
                idx = int(obs_k)
                period = time_values[idx] if idx < len(time_values) else obs_k
                val = obs_v[0] if obs_v else None
                obs.append({"period": period, "value": val})
            obs.sort(key=lambda x: x["period"])
            series_list.append({"dimensions": dim_labels, "observations": obs, "count": len(obs)})

        return {label: series_list, "total_series": len(series_list)}
    except Exception as e:
        return {"error": f"Parse error: {str(e)}"}


def get_dataset(dataset_code: str, params: Dict = None) -> Any:
    """Fetch a raw Eurostat dataset by code (e.g. nama_10_gdp, une_rt_m, prc_hicp_midx)."""
    if params is None:
        params = {}
    params.setdefault("format", "JSON")
    params.setdefault("lang", "EN")
    data = _make_request(dataset_code, params=params)
    return _parse_eurostat_sdmx(data, "series")


def get_gdp(country: str = "DE", freq: str = "A") -> Any:
    """Get GDP data for EU/EEA countries.
    country: DE, FR, IT, ES, NL, BE, AT, PL, SE, DK, FI, etc. Or EU27_2020 for EU total.
    freq: A=annual, Q=quarterly.
    """
    params = {
        "format": "JSON",
        "lang": "EN",
        "geo": country.upper(),
        "unit": "CP_MEUR",
        "na_item": "B1GQ",
        "lastTimePeriod": 30 if freq == "A" else 40,
    }
    dataset = "nama_10_gdp" if freq == "A" else "namq_10_gdp"
    data = _make_request(dataset, params=params)
    result = _parse_eurostat_sdmx(data, "gdp_series")
    result.update({"country": country.upper(), "frequency": freq, "dataset": dataset})
    return result


def get_unemployment(country: str = "DE", freq: str = "M") -> Any:
    """Get unemployment rate for EU/EEA countries.
    country: EU27_2020, DE, FR, IT, ES, etc.
    freq: M=monthly, Q=quarterly, A=annual.
    """
    params = {
        "format": "JSON",
        "lang": "EN",
        "geo": country.upper(),
        "unit": "PC_ACT",
        "sex": "T",
        "age": "Y15-74",
        "lastTimePeriod": 60,
    }
    dataset = "une_rt_m" if freq == "M" else ("une_rt_q" if freq == "Q" else "une_rt_a")
    data = _make_request(dataset, params=params)
    result = _parse_eurostat_sdmx(data, "unemployment_series")
    result.update({"country": country.upper(), "frequency": freq, "dataset": dataset})
    return result


def get_inflation(country: str = "DE", freq: str = "M") -> Any:
    """Get HICP inflation index data for EU/EEA countries.
    country: EU27_2020, DE, FR, IT, ES, NL, PL, etc.
    freq: M=monthly, A=annual.
    """
    params = {
        "format": "JSON",
        "lang": "EN",
        "geo": country.upper(),
        "unit": "INX_A_AVG",
        "coicop": "CP00",
        "lastTimePeriod": 120,
    }
    dataset = "prc_hicp_midx" if freq == "M" else "prc_hicp_aind"
    data = _make_request(dataset, params=params)
    result = _parse_eurostat_sdmx(data, "inflation_series")
    result.update({"country": country.upper(), "frequency": freq, "dataset": dataset})
    return result


def get_population(country: str = "DE") -> Any:
    """Get annual population data for EU/EEA countries."""
    params = {
        "format": "JSON",
        "lang": "EN",
        "geo": country.upper(),
        "sex": "T",
        "age": "TOTAL",
        "lastTimePeriod": 30,
    }
    data = _make_request("demo_pjan", params=params)
    result = _parse_eurostat_sdmx(data, "population_series")
    result.update({"country": country.upper(), "dataset": "demo_pjan"})
    return result


def search_datasets(keyword: str) -> Any:
    """Search for Eurostat datasets by keyword."""
    params = {"lang": "EN", "query": keyword, "limit": 30}
    try:
        response = session.get(SEARCH_URL, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        if isinstance(data, list):
            return {"results": data[:30], "count": len(data), "keyword": keyword}
        elif isinstance(data, dict) and "results" in data:
            return {"results": data["results"][:30], "count": len(data["results"]), "keyword": keyword}
        return {"raw": data, "keyword": keyword}
    except Exception as e:
        return {"error": f"Search failed: {str(e)}"}


def get_trade(reporter: str = "DE", partner: str = "WORLD", flow: str = "X") -> Any:
    """Get EU international trade data.
    reporter: DE, FR, IT, ES, NL, etc.
    partner: WORLD, US, CN, JP, GB, etc.
    flow: X=exports, M=imports.
    """
    params = {
        "format": "JSON",
        "lang": "EN",
        "reporter": reporter.upper(),
        "partner": partner.upper(),
        "flow": flow.upper(),
        "sitc06": "TOTAL",
        "lastTimePeriod": 20,
    }
    data = _make_request("ext_lt_intratrd", params=params)
    result = _parse_eurostat_sdmx(data, "trade_series")
    result.update({"reporter": reporter.upper(), "partner": partner.upper(), "flow": flow.upper()})
    return result


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: dataset, gdp, unemployment, inflation, population, search, trade"}))
        return

    command = args[0]

    if command == "dataset":
        if len(args) < 2:
            result = {"error": "Usage: dataset <dataset_code> (e.g. nama_10_gdp, une_rt_m, prc_hicp_midx)"}
        else:
            result = get_dataset(args[1])
    elif command == "gdp":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "A"
        result = get_gdp(country, freq)
    elif command == "unemployment":
        country = args[1] if len(args) > 1 else "EU27_2020"
        freq = args[2] if len(args) > 2 else "M"
        result = get_unemployment(country, freq)
    elif command == "inflation":
        country = args[1] if len(args) > 1 else "EU27_2020"
        freq = args[2] if len(args) > 2 else "M"
        result = get_inflation(country, freq)
    elif command == "population":
        country = args[1] if len(args) > 1 else "DE"
        result = get_population(country)
    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: search <keyword>"}
        else:
            result = search_datasets(args[1])
    elif command == "trade":
        reporter = args[1] if len(args) > 1 else "DE"
        partner = args[2] if len(args) > 2 else "WORLD"
        flow = args[3] if len(args) > 3 else "X"
        result = get_trade(reporter, partner, flow)
    else:
        result = {"error": f"Unknown command: {command}. Available: dataset, gdp, unemployment, inflation, population, search, trade"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
