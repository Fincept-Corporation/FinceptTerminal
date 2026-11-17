"""
Global Financial Development (GFD) Data Fetcher
Banking system depth, efficiency, stability, and access indicators for 200+
countries via the World Bank Global Financial Development database.
"""
import sys
import json
import os
import requests
import fincept_connector as fc
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GFD_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2/en/indicator"

# The World Bank API often takes 10-20 s per uncached query and sometimes stalls for minutes:
# no read retries (they multiplied the timeout) and a bounded timeout.
session = fc.http_session()
TIMEOUT = (8, 30)

# Indicator codes. GFDD.* live in World Bank source 32 (Global Financial Development), the
# others in source 2 (World Development Indicators); the API needs the source for GFDD codes.
GFD_SOURCE = 32
WDI_SOURCE = 2
DEPTH_INDICATORS = {
    "private_credit_gdp": "FS.AST.PRVT.GD.ZS",
    "bank_deposits_gdp": "GFDD.OI.02",
    "stock_market_cap_gdp": "CM.MKT.LCAP.GD.ZS",
    "liquid_liabilities_gdp": "GFDD.DI.05",
}
ACCESS_INDICATORS = {
    "bank_accounts_1000": "GFDD.AI.01",
    "atm_per_100k": "FB.ATM.TOTL.P5",
    "borrowers_per_1000": "FB.CBK.BRWR.P3",
}
EFFICIENCY_INDICATORS = {
    "net_interest_margin": "GFDD.EI.01",
    "bank_overhead_costs": "GFDD.EI.04",
    "return_on_assets": "GFDD.EI.05",
}
STABILITY_INDICATORS = {
    "z_score": "GFDD.SI.01",
    "npl_ratio": "GFDD.SI.02",
    "capital_adequacy": "GFDD.SI.05",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=TIMEOUT)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error("World Bank API", e)}
    except ValueError:
        return {"error": "World Bank API returned a response that is not JSON"}
    # Errors come back as HTTP 200 with [{"message": [{"id", "key", "value"}]}].
    if isinstance(data, list) and data and isinstance(data[0], dict) and "message" in data[0]:
        msg = (data[0].get("message") or [{}])[0]
        return {"error": f"World Bank API: {msg.get('value') or msg.get('key') or 'request rejected'}"}
    return data


def _fetch_wb_indicator(indicator_code: str, country: str, start_year: str, end_year: str) -> Any:
    """Observations of one indicator: every year for one country (or comma-separated countries),
    or - without a country - the latest value of every country."""
    country_code = country.replace(",", ";") if country else "all"
    url = f"https://api.worldbank.org/v2/country/{country_code}/indicator/{indicator_code}"
    source = GFD_SOURCE if indicator_code.startswith("GFDD.") else WDI_SOURCE
    params = {"format": "json", "per_page": 1000, "source": source}
    if start_year:
        params["date"] = f"{start_year}:{end_year}" if end_year else start_year
    elif country_code == "all":
        params["mrnev"] = 1  # latest non-empty value per country
    data = _make_request(url, params)
    if isinstance(data, dict):
        return data
    if not isinstance(data, list) or len(data) < 2 or not data[1]:
        return {"error": f"No World Bank data for {indicator_code} ({country_code})"}
    rows = [{"country": (r.get("country") or {}).get("value"), "iso3": r.get("countryiso3code"),
             "year": fc.number(r.get("date")), "value": r.get("value")}
            for r in data[1] if r.get("value") is not None]
    return {"indicator": (data[1][0].get("indicator") or {}).get("value"), "code": indicator_code,
            "count": len(rows), "data": rows}


def _fetch_named(table: Dict[str, str], indicator: str, country, start_year, end_year) -> Any:
    code = table.get(indicator)
    if code is None:
        return {"error": f"Unknown indicator '{indicator}' - choose one of: {', '.join(table)}"}
    return _fetch_wb_indicator(code, country, start_year, end_year)


def get_banking_depth(country: str = None, indicator: str = "private_credit_gdp",
                      start_year: str = None, end_year: str = None) -> Any:
    return _fetch_named(DEPTH_INDICATORS, indicator, country, start_year, end_year)


def get_financial_access(country: str = None, indicator: str = "bank_accounts_1000",
                         start_year: str = None, end_year: str = None) -> Any:
    return _fetch_named(ACCESS_INDICATORS, indicator, country, start_year, end_year)


def get_financial_efficiency(country: str = None, indicator: str = "bank_overhead_costs",
                              start_year: str = None, end_year: str = None) -> Any:
    return _fetch_named(EFFICIENCY_INDICATORS, indicator, country, start_year, end_year)


def get_financial_stability(country: str = None, indicator: str = "z_score",
                             start_year: str = None, end_year: str = None) -> Any:
    return _fetch_named(STABILITY_INDICATORS, indicator, country, start_year, end_year)


def get_available_indicators() -> Any:
    all_indicators = {}
    all_indicators.update({"depth": list(DEPTH_INDICATORS.keys())})
    all_indicators.update({"access": list(ACCESS_INDICATORS.keys())})
    all_indicators.update({"efficiency": list(EFFICIENCY_INDICATORS.keys())})
    all_indicators.update({"stability": list(STABILITY_INDICATORS.keys())})
    return all_indicators


def get_countries() -> Any:
    url = "https://api.worldbank.org/v2/country"
    data = _make_request(url, {"format": "json", "per_page": 400})
    if isinstance(data, list) and len(data) >= 2 and isinstance(data[1], list):
        return {"count": len(data[1]), "countries": [
            {"id": c.get("id"), "iso2": c.get("iso2Code"), "name": c.get("name"),
             "region": (c.get("region") or {}).get("value"),
             "income_level": (c.get("incomeLevel") or {}).get("value")} for c in data[1]]}
    return data


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
