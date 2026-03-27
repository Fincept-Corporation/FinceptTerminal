"""
Eurostat Extra Data Fetcher
Eurostat additional datasets: energy balance, industrial production, retail trade,
construction output, trade in goods, and tourism statistics.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EUROSTAT_API_KEY', '')
BASE_URL = "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "JSON"
    params["lang"] = "EN"
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


def get_industrial_production(country: str = "DE", nace: str = "B-D", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "nace_r2": nace, "s_adj": "SCA", "unit": "I15"}
    return _make_request("STS_INPR_M" if freq == "M" else "STS_INPR_Q", params)


def get_retail_trade(country: str = "DE", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "s_adj": "SCA", "unit": "I15", "nace_r2": "G47"}
    return _make_request("STS_TRTU_M" if freq == "M" else "STS_TRTU_Q", params)


def get_energy_balance(country: str = "DE", product: str = "TOTAL", freq: str = "A") -> Any:
    params = {"geo": country.upper(), "nrg_bal": "PPRD", "siec": product, "unit": "GWH"}
    return _make_request("nrg_bal_c", params)


def get_trade_in_goods(country: str = "DE", partner: str = "US", product: str = "TOTAL", flow: str = "EXP") -> Any:
    params = {
        "reporter": country.upper(),
        "partner": partner.upper(),
        "product": product,
        "flow": flow,
        "stat_procedure": "U",
    }
    return _make_request("DS-059341", params)


def get_construction_output(country: str = "DE", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "s_adj": "SCA", "unit": "I15"}
    return _make_request("STS_COPR_M" if freq == "M" else "STS_COPR_Q", params)


def get_tourism_stats(country: str = "DE", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "c_resid": "TOTAL", "unit": "NR"}
    return _make_request("tour_occ_nim", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "industrial":
        country = args[1] if len(args) > 1 else "DE"
        nace = args[2] if len(args) > 2 else "B-D"
        freq = args[3] if len(args) > 3 else "M"
        result = get_industrial_production(country, nace, freq)
    elif command == "retail":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "M"
        result = get_retail_trade(country, freq)
    elif command == "energy":
        country = args[1] if len(args) > 1 else "DE"
        product = args[2] if len(args) > 2 else "TOTAL"
        freq = args[3] if len(args) > 3 else "A"
        result = get_energy_balance(country, product, freq)
    elif command == "trade":
        country = args[1] if len(args) > 1 else "DE"
        partner = args[2] if len(args) > 2 else "US"
        product = args[3] if len(args) > 3 else "TOTAL"
        flow = args[4] if len(args) > 4 else "EXP"
        result = get_trade_in_goods(country, partner, product, flow)
    elif command == "construction":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "M"
        result = get_construction_output(country, freq)
    elif command == "tourism":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "M"
        result = get_tourism_stats(country, freq)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
