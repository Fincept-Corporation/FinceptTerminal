"""
IMF DataMapper Data Fetcher
IMF DataMapper API: WEO forecasts, global debt, fiscal monitor, and financial
soundness indicators for 190+ countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IMF_API_KEY', '')
BASE_URL = "https://www.imf.org/external/datamapper/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
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


def get_indicators() -> Any:
    return _make_request("indicators")


def get_datasets() -> Any:
    return _make_request("datasets")


def get_indicator_data(indicator: str, countries: str = None) -> Any:
    if countries:
        endpoint = f"{indicator}/{countries}"
    else:
        endpoint = indicator
    return _make_request(endpoint)


def get_weo_data(indicator: str, country: str, start_year: str = None, end_year: str = None) -> Any:
    endpoint = f"{indicator}/{country}"
    params = {}
    if start_year:
        params["startYear"] = start_year
    if end_year:
        params["endYear"] = end_year
    return _make_request(endpoint, params if params else None)


def get_fiscal_data(indicator: str, country: str) -> Any:
    fiscal_indicators = {
        "overall_balance": "GGXCNL_NGDP",
        "gross_debt": "GGXWDG_NGDP",
        "net_debt": "GGXWDN_NGDP",
        "revenue": "GGR_NGDP",
        "expenditure": "GGX_NGDP",
        "primary_balance": "GGXONLB_NGDP",
    }
    imf_indicator = fiscal_indicators.get(indicator, indicator)
    return _make_request(f"{imf_indicator}/{country.upper()}")


def get_financial_soundness(country: str) -> Any:
    fsi_indicators = ["FSI_NIBT_MCA", "FSI_CAR_MCA", "FSI_NPL_NLSS", "FSI_ROA_MCA"]
    results = {}
    for ind in fsi_indicators:
        results[ind] = _make_request(f"{ind}/{country.upper()}")
    return results


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "indicators":
        result = get_indicators()
    elif command == "datasets":
        result = get_datasets()
    elif command == "data":
        indicator = args[1] if len(args) > 1 else "NGDP_RPCH"
        countries = args[2] if len(args) > 2 else None
        result = get_indicator_data(indicator, countries)
    elif command == "weo":
        indicator = args[1] if len(args) > 1 else "NGDP_RPCH"
        country = args[2] if len(args) > 2 else "US"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        result = get_weo_data(indicator, country, start_year, end_year)
    elif command == "fiscal":
        indicator = args[1] if len(args) > 1 else "gross_debt"
        country = args[2] if len(args) > 2 else "US"
        result = get_fiscal_data(indicator, country)
    elif command == "fsi":
        country = args[1] if len(args) > 1 else "US"
        result = get_financial_soundness(country)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
