"""
ECB SDMX REST API Data Fetcher
Official ECB exchange rates, monetary stats, bond yields, balance of payments.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://data-api.ecb.europa.eu/service/data"
META_URL = "https://data-api.ecb.europa.eu/service"

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


def _parse_sdmx_json(data: Any, series_name: str = "series") -> Any:
    """Parse SDMX-JSON response into a simplified structure."""
    if isinstance(data, dict) and "error" in data:
        return data
    try:
        dataset = data.get("dataSets", [{}])[0]
        structure = data.get("structure", {})
        dimensions = structure.get("dimensions", {}).get("series", [])
        obs_dims = structure.get("dimensions", {}).get("observation", [])
        attributes = structure.get("attributes", {}).get("series", [])

        # Build time periods from observation dimension
        time_periods = []
        for obs_dim in obs_dims:
            if obs_dim.get("id") == "TIME_PERIOD":
                time_periods = [v.get("id") for v in obs_dim.get("values", [])]

        series_list = []
        for series_key, series_data in dataset.get("series", {}).items():
            # Parse dimension values from key
            key_parts = series_key.split(":")
            dim_values = {}
            for i, dim in enumerate(dimensions):
                if i < len(key_parts):
                    idx = int(key_parts[i])
                    values = dim.get("values", [])
                    if idx < len(values):
                        dim_values[dim.get("id")] = values[idx].get("id", "")

            # Extract observations
            observations = []
            for obs_key, obs_val in series_data.get("observations", {}).items():
                idx = int(obs_key)
                period = time_periods[idx] if idx < len(time_periods) else obs_key
                value = obs_val[0] if obs_val else None
                observations.append({"period": period, "value": value})

            series_list.append({
                "dimensions": dim_values,
                "observations": observations,
                "obs_count": len(observations),
            })

        return {series_name: series_list, "total_series": len(series_list)}
    except Exception as e:
        return {"error": f"Parse error: {str(e)}", "raw_keys": list(data.keys()) if isinstance(data, dict) else []}


def get_exchange_rates(currency: str = "USD", freq: str = "D") -> Any:
    """Get ECB reference exchange rates.
    currency: e.g. USD, GBP, JPY, CHF, AUD, CAD, CNY.
    freq: D=daily, M=monthly, A=annual.
    """
    params = {"format": "jsondata", "detail": "dataonly", "lastNObservations": 365}
    key = f"EXR/{freq}.{currency.upper()}.EUR.SP00.A"
    data = _make_request(key, params=params)
    result = _parse_sdmx_json(data, "exchange_rates")
    result["currency"] = currency.upper()
    result["frequency"] = freq
    return result


def get_hicp_inflation(country: str = "U2", freq: str = "M") -> Any:
    """Get HICP inflation data.
    country: U2=Eurozone, DE, FR, IT, ES, NL, etc.
    freq: M=monthly, A=annual.
    """
    params = {"format": "jsondata", "detail": "dataonly", "lastNObservations": 60}
    key = f"ICP/{freq}.{country.upper()}.N.000000.4.ANR"
    data = _make_request(key, params=params)
    result = _parse_sdmx_json(data, "inflation")
    result["country"] = country.upper()
    result["frequency"] = freq
    return result


def get_bond_yields(country: str = "DE", maturity: str = "10Y") -> Any:
    """Get government bond yields.
    country: DE, FR, IT, ES, NL, AT, BE, PT, GR, FI, IE.
    maturity: 3M, 6M, 1Y, 2Y, 5Y, 10Y, 30Y.
    """
    maturity_map = {
        "3M": "3", "6M": "6", "1Y": "12", "2Y": "24",
        "5Y": "60", "10Y": "120", "30Y": "360"
    }
    months = maturity_map.get(maturity.upper(), "120")
    params = {"format": "jsondata", "detail": "dataonly", "lastNObservations": 260}
    key = f"YC/B.U2.EUR.4F.G_N_{country.upper()}.SV_C_YM.SR_{months}Y"
    data = _make_request(key, params=params)
    if isinstance(data, dict) and "error" not in data:
        result = _parse_sdmx_json(data, "bond_yields")
    else:
        # Fallback: try IRS dataset
        key2 = f"FM/B.{country.upper()}.EUR.FR.BB.{maturity.upper()}._Z.D"
        data = _make_request(key2, params=params)
        result = _parse_sdmx_json(data, "bond_yields")
    result["country"] = country.upper()
    result["maturity"] = maturity
    return result


def get_money_supply(aggregate: str = "M3") -> Any:
    """Get ECB money supply aggregates.
    aggregate: M1, M2, M3.
    """
    params = {"format": "jsondata", "detail": "dataonly", "lastNObservations": 120}
    key = f"BSI/M.U2.Y.V.M20.X.1.U2.2300.Z01.E"
    data = _make_request(key, params=params)
    result = _parse_sdmx_json(data, "money_supply")
    result["aggregate"] = aggregate
    return result


def get_interest_rates(rate_type: str = "MRO") -> Any:
    """Get ECB key interest rates.
    rate_type: MRO (main refinancing), DFR (deposit facility), MLF (marginal lending).
    """
    rate_map = {
        "MRO": "F.U2.EUR.4F.KR.MRR_FR.LEV",
        "DFR": "F.U2.EUR.4F.KR.DFR.LEV",
        "MLF": "F.U2.EUR.4F.KR.MLFR.LEV",
    }
    series_key = rate_map.get(rate_type.upper(), rate_map["MRO"])
    params = {"format": "jsondata", "detail": "dataonly", "lastNObservations": 200}
    key = f"FM/{series_key}"
    data = _make_request(key, params=params)
    result = _parse_sdmx_json(data, "interest_rates")
    result["rate_type"] = rate_type.upper()
    return result


def get_datasets() -> Any:
    """Get list of available ECB data flow/dataset IDs."""
    url = f"{META_URL}/dataflow/ECB"
    params = {"format": "jsondata"}
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        dataflows = data.get("dataflows", data.get("Dataflows", []))
        if isinstance(dataflows, list):
            datasets = [
                {"id": df.get("id"), "name": df.get("name", {}).get("en", df.get("name", ""))}
                for df in dataflows
            ]
            return {"datasets": datasets[:100], "total_count": len(datasets)}
        return {"raw": list(data.keys()) if isinstance(data, dict) else data}
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: exchange_rates, inflation, bond_yields, money_supply, interest_rates, datasets"}))
        return

    command = args[0]

    if command == "exchange_rates":
        currency = args[1] if len(args) > 1 else "USD"
        freq = args[2] if len(args) > 2 else "D"
        result = get_exchange_rates(currency, freq)
    elif command == "inflation":
        country = args[1] if len(args) > 1 else "U2"
        freq = args[2] if len(args) > 2 else "M"
        result = get_hicp_inflation(country, freq)
    elif command == "bond_yields":
        country = args[1] if len(args) > 1 else "DE"
        maturity = args[2] if len(args) > 2 else "10Y"
        result = get_bond_yields(country, maturity)
    elif command == "money_supply":
        aggregate = args[1] if len(args) > 1 else "M3"
        result = get_money_supply(aggregate)
    elif command == "interest_rates":
        rate_type = args[1] if len(args) > 1 else "MRO"
        result = get_interest_rates(rate_type)
    elif command == "datasets":
        result = get_datasets()
    else:
        result = {"error": f"Unknown command: {command}. Available: exchange_rates, inflation, bond_yields, money_supply, interest_rates, datasets"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
