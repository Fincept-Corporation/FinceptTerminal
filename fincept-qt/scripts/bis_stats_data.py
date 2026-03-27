"""
BIS Data Portal SDMX Data Fetcher
Global credit, debt securities, banking stats, effective exchange rates, property prices.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://stats.bis.org/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def _parse_sdmx_compact(data: Any, label: str = "data") -> Any:
    """Extract time-series from BIS SDMX-JSON compact format."""
    if isinstance(data, dict) and "error" in data:
        return data
    try:
        datasets = data.get("dataSets", [])
        if not datasets:
            return {"error": "No dataSets in response", "keys": list(data.keys())}
        dataset = datasets[0]
        structure = data.get("structure", {})
        dimensions = structure.get("dimensions", {})
        series_dims = dimensions.get("series", [])
        obs_dims = dimensions.get("observation", [])

        time_periods = []
        for dim in obs_dims:
            if "TIME" in dim.get("id", ""):
                time_periods = [v.get("id") for v in dim.get("values", [])]

        series_list = []
        for key, series_obj in dataset.get("series", {}).items():
            parts = key.split(":")
            dim_labels = {}
            for i, dim in enumerate(series_dims):
                if i < len(parts):
                    try:
                        idx = int(parts[i])
                        vals = dim.get("values", [])
                        if idx < len(vals):
                            dim_labels[dim["id"]] = vals[idx].get("id", "")
                    except ValueError:
                        pass
            obs_map = series_obj.get("observations", {})
            observations = []
            for obs_idx_str, obs_val in obs_map.items():
                idx = int(obs_idx_str)
                period = time_periods[idx] if idx < len(time_periods) else obs_idx_str
                value = obs_val[0] if obs_val else None
                observations.append({"period": period, "value": value})
            series_list.append({"dimensions": dim_labels, "observations": observations})

        return {label: series_list, "count": len(series_list)}
    except Exception as e:
        return {"error": f"Parse error: {str(e)}"}


def get_total_credit(country: str = "US", sector: str = "P") -> Any:
    """Get BIS total credit to private non-financial sector.
    country: US, GB, DE, FR, JP, CN, KR, AU, CA, etc.
    sector: P=private, C=corporations, H=households.
    """
    params = {"format": "jsondata", "lastNObservations": 80}
    key = f"data/total-credit/Q.{sector.upper()}.A.M.770.{country.upper()}.A"
    data = _make_request(key, params=params)
    result = _parse_sdmx_compact(data, "credit_series")
    result.update({"country": country.upper(), "sector": sector.upper(), "dataset": "Total Credit"})
    return result


def get_debt_securities(issuer_type: str = "C", currency: str = "USD") -> Any:
    """Get BIS international debt securities statistics.
    issuer_type: C=all, G=government, F=financial, N=non-financial.
    currency: USD, EUR, GBP, JPY, CHF, etc.
    """
    params = {"format": "jsondata", "lastNObservations": 40}
    key = f"data/debt-sec2/Q.N.{issuer_type.upper()}.A.{currency.upper()}.A.A.TO1.A"
    data = _make_request(key, params=params)
    result = _parse_sdmx_compact(data, "debt_securities")
    result.update({"issuer_type": issuer_type, "currency": currency.upper(), "dataset": "Debt Securities"})
    return result


def get_effective_exchange_rates(country: str = "US", type_: str = "R") -> Any:
    """Get BIS effective exchange rate indices (BIS EER).
    country: US, GB, DE, FR, JP, CN, KR, AU, CA, CH, SE, NO, etc.
    type_: R=real, N=nominal.
    """
    params = {"format": "jsondata", "lastNObservations": 120}
    eer_type = "N" if type_.upper() == "N" else "R"
    key = f"data/eer/M.{eer_type}.B.{country.upper()}"
    data = _make_request(key, params=params)
    result = _parse_sdmx_compact(data, "eer_series")
    result.update({"country": country.upper(), "type": eer_type, "dataset": "Effective Exchange Rates"})
    return result


def get_property_prices(country: str = "US") -> Any:
    """Get BIS residential property price statistics.
    country: US, GB, DE, FR, JP, KR, AU, CA, SE, NL, ES, IT, CH, etc.
    """
    params = {"format": "jsondata", "lastNObservations": 100}
    key = f"data/pp-selected/Q.{country.upper()}.N.628"
    data = _make_request(key, params=params)
    result = _parse_sdmx_compact(data, "property_prices")
    result.update({"country": country.upper(), "dataset": "Property Prices"})
    return result


def get_policy_rates(country: str = "US") -> Any:
    """Get BIS central bank policy/target interest rates.
    country: US, GB, DE, FR, JP, CN, AU, CA, SE, NO, CH, NZ, etc.
    """
    params = {"format": "jsondata", "lastNObservations": 120}
    key = f"data/cb-policy-rates/M.{country.upper()}"
    data = _make_request(key, params=params)
    result = _parse_sdmx_compact(data, "policy_rates")
    result.update({"country": country.upper(), "dataset": "Policy Rates"})
    return result


def get_datasets() -> Any:
    """Get list of available BIS statistical datasets."""
    url = f"{BASE_URL}/dataflow/BIS"
    params = {"detail": "allstubs", "format": "jsondata"}
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        flows = data.get("dataflows", data.get("Dataflows", []))
        if isinstance(flows, list):
            return {
                "datasets": [
                    {"id": df.get("id"), "name": df.get("name", {}).get("en", df.get("name", ""))}
                    for df in flows
                ],
                "count": len(flows)
            }
        return {"raw_keys": list(data.keys())}
    except Exception as e:
        # Return known datasets
        return {
            "datasets": [
                {"id": "total-credit", "name": "Total credit to the private non-financial sector"},
                {"id": "debt-sec2", "name": "International debt securities"},
                {"id": "eer", "name": "Effective exchange rates"},
                {"id": "pp-selected", "name": "Residential property prices"},
                {"id": "cb-policy-rates", "name": "Central bank policy rates"},
                {"id": "lbs-by-residence", "name": "Locational banking statistics"},
                {"id": "cbs-by-residence", "name": "Consolidated banking statistics"},
                {"id": "dsr", "name": "Debt service ratios"},
            ],
            "note": f"Dataset list endpoint failed: {str(e)}"
        }


def get_banking_stats(reporting_country: str = "US", counterpart_country: str = "ALL") -> Any:
    """Get BIS locational banking statistics (cross-border claims/liabilities).
    reporting_country: US, GB, DE, FR, JP, CH, etc.
    counterpart_country: ALL or specific country code.
    """
    params = {"format": "jsondata", "lastNObservations": 40}
    cc = counterpart_country if counterpart_country != "ALL" else "5J"
    key = f"data/lbs-by-residence/Q.S.A.A.TO.A.{cc}.{reporting_country.upper()}.USD"
    data = _make_request(key, params=params)
    result = _parse_sdmx_compact(data, "banking_stats")
    result.update({
        "reporting_country": reporting_country.upper(),
        "counterpart_country": counterpart_country,
        "dataset": "Locational Banking Statistics"
    })
    return result


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: credit, debt_securities, eer, property_prices, policy_rates, datasets, banking"}))
        return

    command = args[0]

    if command == "credit":
        country = args[1] if len(args) > 1 else "US"
        sector = args[2] if len(args) > 2 else "P"
        result = get_total_credit(country, sector)
    elif command == "debt_securities":
        issuer_type = args[1] if len(args) > 1 else "C"
        currency = args[2] if len(args) > 2 else "USD"
        result = get_debt_securities(issuer_type, currency)
    elif command == "eer":
        country = args[1] if len(args) > 1 else "US"
        type_ = args[2] if len(args) > 2 else "R"
        result = get_effective_exchange_rates(country, type_)
    elif command == "property_prices":
        country = args[1] if len(args) > 1 else "US"
        result = get_property_prices(country)
    elif command == "policy_rates":
        country = args[1] if len(args) > 1 else "US"
        result = get_policy_rates(country)
    elif command == "datasets":
        result = get_datasets()
    elif command == "banking":
        reporting = args[1] if len(args) > 1 else "US"
        counterpart = args[2] if len(args) > 2 else "ALL"
        result = get_banking_stats(reporting, counterpart)
    else:
        result = {"error": f"Unknown command: {command}. Available: credit, debt_securities, eer, property_prices, policy_rates, datasets, banking"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
