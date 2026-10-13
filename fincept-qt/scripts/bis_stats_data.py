"""
BIS Data Portal SDMX Data Fetcher
Global credit, debt securities, banking stats, effective exchange rates, property prices.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any

from sdmx_json import flatten, compact

BASE_URL = "https://stats.bis.org/api/v1"
# stats.bis.org answers HTTP 406 to `format=jsondata`; SDMX-JSON is negotiated with this Accept header.
DATA_ACCEPT = "application/vnd.sdmx.data+json;version=1.0.0"
STRUCT_ACCEPT = "application/vnd.sdmx.structure+json;version=1.0.0"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(endpoint: str, params: Dict = None, accept: str = DATA_ACCEPT) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, headers={"Accept": accept}, timeout=60)
        if response.status_code == 404:
            return {"error": "No data for this query (HTTP 404) - check the country/series codes"}
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {e.response.text[:200] or str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _fetch(flow: str, key: str, last_n: int, label: str) -> Any:
    """Fetch one BIS dataflow slice and flatten it to {label: [series...], count}."""
    data = _make_request(f"data/{flow}/{key}", params={"lastNObservations": last_n})
    if isinstance(data, dict) and "error" in data:
        return data
    series = compact(flatten(data))
    if not series:
        return {"error": "No series returned", "flow": flow, "key": key}
    return {label: series, "count": len(series), "flow": flow, "key": key}


def get_total_credit(country: str = "US", sector: str = "P") -> Any:
    """Get BIS total credit (WS_TC) to a borrowing sector, in % of GDP and in domestic currency.
    country: US, GB, DE, FR, JP, CN, KR, AU, CA, etc.
    sector: P=private non-financial, C=non-financial sector, G=government, H=households, N=non-financial corporations.
    """
    key = f"Q.{country.upper()}.{sector.upper()}.A.M.770+XDC.A"
    result = _fetch("WS_TC", key, 80, "credit_series")
    result.update({"country": country.upper(), "sector": sector.upper(), "dataset": "Total Credit"})
    return result


ISSUER_CODES = {"C": "1", "A": "1", "G": "2", "F": "B", "N": "J"}


def get_debt_securities(issuer_type: str = "C", currency: str = "USD") -> Any:
    """Get BIS international debt securities outstanding (WS_DEBT_SEC2_PUB), all issuer countries.
    issuer_type: C=all, G=general government, F=financial corporations, N=non-financial corporations.
    currency: USD, EUR, GBP, JPY, CHF, ... or TO1 for all currencies.
    """
    issuer = ISSUER_CODES.get(issuer_type.upper(), issuer_type.upper())
    key = f"Q.3P.3P.{issuer}.1.C.A.A.{currency.upper()}.A.A.A.A.A.I"
    result = _fetch("WS_DEBT_SEC2_PUB", key, 40, "debt_securities")
    result.update({"issuer_type": issuer_type, "currency": currency.upper(), "dataset": "Debt Securities",
                   "unit": "USD millions"})
    return result


def get_effective_exchange_rates(country: str = "US", type_: str = "R") -> Any:
    """Get BIS effective exchange rate indices (WS_EER, broad basket, monthly).
    country: US, GB, DE, FR, JP, CN, KR, AU, CA, CH, SE, NO, etc.
    type_: R=real, N=nominal.
    """
    eer_type = "N" if type_.upper() == "N" else "R"
    result = _fetch("WS_EER", f"M.{eer_type}.B.{country.upper()}", 120, "eer_series")
    result.update({"country": country.upper(), "type": eer_type, "dataset": "Effective Exchange Rates"})
    return result


def get_property_prices(country: str = "US") -> Any:
    """Get BIS selected residential property prices (WS_SPP): real and nominal, index and y/y change.
    country: US, GB, DE, FR, JP, KR, AU, CA, SE, NL, ES, IT, CH, etc.
    """
    result = _fetch("WS_SPP", f"Q.{country.upper()}..", 100, "property_prices")
    result.update({"country": country.upper(), "dataset": "Property Prices"})
    return result


def get_policy_rates(country: str = "US") -> Any:
    """Get BIS central bank policy rates (WS_CBPOL, monthly).
    country: US, GB, XM (euro area), JP, CN, AU, CA, SE, NO, CH, NZ, etc.
    """
    result = _fetch("WS_CBPOL", f"M.{country.upper()}", 120, "policy_rates")
    result.update({"country": country.upper(), "dataset": "Policy Rates"})
    return result


def get_datasets() -> Any:
    """Get list of available BIS statistical datasets (dataflows)."""
    data = _make_request("dataflow/BIS/all/latest", accept=STRUCT_ACCEPT)
    if isinstance(data, dict) and "error" in data:
        return data
    flows = data.get("data", {}).get("dataflows", [])
    return {"datasets": [{"id": f.get("id"), "name": f.get("name")} for f in flows], "count": len(flows)}


def get_banking_stats(reporting_country: str = "US", counterpart_country: str = "ALL") -> Any:
    """Get BIS locational banking statistics (WS_LBS_D_PUB): cross-border total claims of banks
    located in the reporting country on a counterpart country.
    reporting_country: US, GB, DE, FR, JP, CH, etc.
    counterpart_country: ALL or specific country code.
    """
    cc = counterpart_country.upper() if counterpart_country.upper() != "ALL" else "5J"
    key = f"Q.S.C.A.TO1.A.5J.A.{reporting_country.upper()}.A.{cc}.N"
    result = _fetch("WS_LBS_D_PUB", key, 40, "banking_stats")
    result.update({
        "reporting_country": reporting_country.upper(),
        "counterpart_country": counterpart_country,
        "dataset": "Locational Banking Statistics",
        "unit": "USD millions"
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
