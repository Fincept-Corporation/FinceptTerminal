"""
FRED Extended Economic Data Fetcher
Broader macro coverage from FRED — yield curves, money supply, credit conditions,
financial stress, consumer sentiment, PCE inflation, and more.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('FRED_API_KEY', '')
BASE_URL = "https://api.stlouisfed.org/fred"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

YIELD_CURVE_SERIES = {
    "1m": "DGS1MO",
    "3m": "DGS3MO",
    "6m": "DGS6MO",
    "1y": "DGS1",
    "2y": "DGS2",
    "3y": "DGS3",
    "5y": "DGS5",
    "7y": "DGS7",
    "10y": "DGS10",
    "20y": "DGS20",
    "30y": "DGS30",
    "10y_2y_spread": "T10Y2Y",
    "10y_3m_spread": "T10Y3M",
}

MONEY_SUPPLY_SERIES = {
    "m1": "M1SL",
    "m2": "M2SL",
    "m1_velocity": "M1V",
    "m2_velocity": "M2V",
    "monetary_base": "BOGMBASE",
    "reserves": "TOTRESNS",
}

CREDIT_CONDITION_SERIES = {
    "commercial_credit": "DPSACBW027SBOG",
    "consumer_credit": "TOTALSL",
    "mortgage_rate_30y": "MORTGAGE30US",
    "mortgage_rate_15y": "MORTGAGE15US",
    "prime_rate": "DPRIME",
    "fed_funds_rate": "FEDFUNDS",
    "sofr": "SOFR",
    "libor_3m": "USD3MTD156N",
    "bank_credit": "TOTBKCR",
    "charge_off_rate": "CORCCACBS",
    "delinquency_rate": "DRCCLACBS",
}

FINANCIAL_STRESS_SERIES = {
    "stlfsi": "STLFSI4",
    "kansas_city_fsi": "KCFSI",
    "ted_spread": "TEDRATE",
    "vix": "VIXCLS",
    "move_index": "MOVEINDEX",
    "nfci": "NFCI",
    "anfci": "ANFCI",
}

CONSUMER_SENTIMENT_SERIES = {
    "umich_sentiment": "UMCSENT",
    "conference_board": "CSCICP03USM665S",
    "consumer_confidence": "CONCCONF",
}

PCE_SERIES = {
    "pce": "PCE",
    "core_pce": "PCEPILFE",
    "pce_yoy": "PCEPI",
    "core_pce_yoy": "CPILFESL",
    "pce_services": "DSERRG3M086SBEA",
    "pce_goods": "DGDSRG3M086SBEA",
    "pce_energy": "DENERRG3M086SBEA",
    "pce_food": "DFXARRG3M086SBEA",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["api_key"] = API_KEY
    params["file_type"] = "json"
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


def _get_series_observations(series_id: str, start_date: str = "", end_date: str = "") -> Dict:
    params = {"series_id": series_id}
    if start_date:
        params["observation_start"] = start_date
    if end_date:
        params["observation_end"] = end_date
    data = _make_request("series/observations", params)
    if "error" in data:
        return data
    obs = data.get("observations", [])
    return {
        "series_id": series_id,
        "count": data.get("count", len(obs)),
        "start": data.get("observation_start", ""),
        "end": data.get("observation_end", ""),
        "units": data.get("units", ""),
        "observations": [{"date": o["date"], "value": o["value"]} for o in obs],
    }


def get_yield_curve(start_date: str = "", end_date: str = "") -> Dict:
    if not API_KEY:
        return {"error": "FRED_API_KEY environment variable not set"}
    result = {"yield_curve": {}, "spreads": {}, "start_date": start_date, "end_date": end_date}
    tenor_series = {k: v for k, v in YIELD_CURVE_SERIES.items() if "spread" not in k}
    spread_series = {k: v for k, v in YIELD_CURVE_SERIES.items() if "spread" in k}
    for tenor, series_id in tenor_series.items():
        obs_data = _get_series_observations(series_id, start_date, end_date)
        result["yield_curve"][tenor] = obs_data
    for spread_name, series_id in spread_series.items():
        obs_data = _get_series_observations(series_id, start_date, end_date)
        result["spreads"][spread_name] = obs_data
    result["available_tenors"] = list(YIELD_CURVE_SERIES.keys())
    return result


def get_money_supply(measure: str = "m2", start_date: str = "", end_date: str = "") -> Dict:
    if not API_KEY:
        return {"error": "FRED_API_KEY environment variable not set"}
    if measure == "all":
        result = {"money_supply": {}}
        for name, series_id in MONEY_SUPPLY_SERIES.items():
            result["money_supply"][name] = _get_series_observations(series_id, start_date, end_date)
        result["available_measures"] = list(MONEY_SUPPLY_SERIES.keys())
        return result
    series_id = MONEY_SUPPLY_SERIES.get(measure, measure)
    data = _get_series_observations(series_id, start_date, end_date)
    data["measure"] = measure
    data["available_measures"] = list(MONEY_SUPPLY_SERIES.keys())
    return data


def get_credit_conditions(series_id: str = "fed_funds_rate", start_date: str = "", end_date: str = "") -> Dict:
    if not API_KEY:
        return {"error": "FRED_API_KEY environment variable not set"}
    resolved_id = CREDIT_CONDITION_SERIES.get(series_id, series_id)
    data = _get_series_observations(resolved_id, start_date, end_date)
    data["requested_series"] = series_id
    data["available_series"] = list(CREDIT_CONDITION_SERIES.keys())
    return data


def get_financial_stress_index(start_date: str = "", end_date: str = "") -> Dict:
    if not API_KEY:
        return {"error": "FRED_API_KEY environment variable not set"}
    result = {"stress_indices": {}, "start_date": start_date, "end_date": end_date}
    for name, series_id in FINANCIAL_STRESS_SERIES.items():
        result["stress_indices"][name] = _get_series_observations(series_id, start_date, end_date)
    result["available_indices"] = list(FINANCIAL_STRESS_SERIES.keys())
    return result


def get_consumer_sentiment(start_date: str = "", end_date: str = "") -> Dict:
    if not API_KEY:
        return {"error": "FRED_API_KEY environment variable not set"}
    result = {"sentiment": {}, "start_date": start_date, "end_date": end_date}
    for name, series_id in CONSUMER_SENTIMENT_SERIES.items():
        result["sentiment"][name] = _get_series_observations(series_id, start_date, end_date)
    result["available_series"] = list(CONSUMER_SENTIMENT_SERIES.keys())
    return result


def get_pce_inflation(start_date: str = "", end_date: str = "") -> Dict:
    if not API_KEY:
        return {"error": "FRED_API_KEY environment variable not set"}
    result = {"pce": {}, "start_date": start_date, "end_date": end_date}
    for name, series_id in PCE_SERIES.items():
        result["pce"][name] = _get_series_observations(series_id, start_date, end_date)
    result["available_series"] = list(PCE_SERIES.keys())
    return result


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "yield_curve":
        start_date = args[1] if len(args) > 1 else ""
        end_date = args[2] if len(args) > 2 else ""
        result = get_yield_curve(start_date, end_date)
    elif command == "money_supply":
        measure = args[1] if len(args) > 1 else "m2"
        start_date = args[2] if len(args) > 2 else ""
        end_date = args[3] if len(args) > 3 else ""
        result = get_money_supply(measure, start_date, end_date)
    elif command == "credit":
        series_id = args[1] if len(args) > 1 else "fed_funds_rate"
        start_date = args[2] if len(args) > 2 else ""
        end_date = args[3] if len(args) > 3 else ""
        result = get_credit_conditions(series_id, start_date, end_date)
    elif command == "stress":
        start_date = args[1] if len(args) > 1 else ""
        end_date = args[2] if len(args) > 2 else ""
        result = get_financial_stress_index(start_date, end_date)
    elif command == "sentiment":
        start_date = args[1] if len(args) > 1 else ""
        end_date = args[2] if len(args) > 2 else ""
        result = get_consumer_sentiment(start_date, end_date)
    elif command == "pce":
        start_date = args[1] if len(args) > 1 else ""
        end_date = args[2] if len(args) > 2 else ""
        result = get_pce_inflation(start_date, end_date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
