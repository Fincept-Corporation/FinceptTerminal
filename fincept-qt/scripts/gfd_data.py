"""
Global Financial Development (GFD) Data Fetcher
Banking system depth, efficiency, stability, and access indicators for 200+
countries via the World Bank Global Financial Development database.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GFD_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2/en/indicator"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

# GFD indicator codes
DEPTH_INDICATORS = {
    "private_credit_gdp": "FS.AST.PRVT.GD.ZS",
    "bank_deposits_gdp": "FD.BNK.DPST.TA.P5",
    "stock_market_cap_gdp": "CM.MKT.LCAP.GD.ZS",
    "liquid_liabilities_gdp": "FS.LBL.LIQU.GD.ZS",
}
ACCESS_INDICATORS = {
    "bank_accounts_1000": "FB.BNK.ACCT.P3",
    "atm_per_100k": "FB.ATM.TOTL.P5",
    "borrowers_per_1000": "FB.CBK.BRWR.P3",
}
EFFICIENCY_INDICATORS = {
    "net_interest_margin": "FS.AST.PRVT.GD.ZS",
    "bank_overhead_costs": "FB.BNK.NONP.ZS",
    "return_on_assets": "FB.BNK.RETR.ZS",
}
STABILITY_INDICATORS = {
    "z_score": "FB.BNK.ZSCR.ZS",
    "npl_ratio": "FB.BNK.NLLS.ZS",
    "capital_adequacy": "FB.BNK.CAPA.ZS",
}


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


def _fetch_wb_indicator(indicator_code: str, country: str, start_year: str, end_year: str) -> Any:
    country_code = country if country else "all"
    url = f"https://api.worldbank.org/v2/country/{country_code}/indicator/{indicator_code}"
    params = {"format": "json", "per_page": 500}
    if start_year:
        params["date"] = f"{start_year}:{end_year}" if end_year else start_year
    return _make_request(url, params)


def get_banking_depth(country: str = None, indicator: str = "private_credit_gdp",
                      start_year: str = None, end_year: str = None) -> Any:
    code = DEPTH_INDICATORS.get(indicator, "FS.AST.PRVT.GD.ZS")
    return _fetch_wb_indicator(code, country, start_year, end_year)


def get_financial_access(country: str = None, indicator: str = "bank_accounts_1000",
                         start_year: str = None, end_year: str = None) -> Any:
    code = ACCESS_INDICATORS.get(indicator, "FB.BNK.ACCT.P3")
    return _fetch_wb_indicator(code, country, start_year, end_year)


def get_financial_efficiency(country: str = None, indicator: str = "bank_overhead_costs",
                              start_year: str = None, end_year: str = None) -> Any:
    code = EFFICIENCY_INDICATORS.get(indicator, "FB.BNK.NONP.ZS")
    return _fetch_wb_indicator(code, country, start_year, end_year)


def get_financial_stability(country: str = None, indicator: str = "z_score",
                             start_year: str = None, end_year: str = None) -> Any:
    code = STABILITY_INDICATORS.get(indicator, "FB.BNK.ZSCR.ZS")
    return _fetch_wb_indicator(code, country, start_year, end_year)


def get_available_indicators() -> Any:
    all_indicators = {}
    all_indicators.update({"depth": list(DEPTH_INDICATORS.keys())})
    all_indicators.update({"access": list(ACCESS_INDICATORS.keys())})
    all_indicators.update({"efficiency": list(EFFICIENCY_INDICATORS.keys())})
    all_indicators.update({"stability": list(STABILITY_INDICATORS.keys())})
    return all_indicators


def get_countries() -> Any:
    url = "https://api.worldbank.org/v2/country"
    return _make_request(url, {"format": "json", "per_page": 300})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "depth":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else "private_credit_gdp"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        result = get_banking_depth(country, indicator, start_year, end_year)
    elif command == "access":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else "bank_accounts_1000"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        result = get_financial_access(country, indicator, start_year, end_year)
    elif command == "efficiency":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else "bank_overhead_costs"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        result = get_financial_efficiency(country, indicator, start_year, end_year)
    elif command == "stability":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else "z_score"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        result = get_financial_stability(country, indicator, start_year, end_year)
    elif command == "indicators":
        result = get_available_indicators()
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
