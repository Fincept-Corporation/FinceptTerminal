"""
World Bank Sovereign Fiscal Data Fetcher
Fetches sovereign debt data, budget transparency, and fiscal statistics using the
World Bank Open Finances and WDI APIs — no API key required.
"""
import sys
import json
import os
import requests
import fincept_connector as fc
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLDBANK_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2"

# The World Bank API often takes 10-20 s per uncached query and sometimes stalls for minutes:
# no read retries (they multiplied the timeout), a bounded timeout, and the indicators of one
# category are fetched concurrently so a command finishes well inside the runner's limits.
session = fc.http_session()
TIMEOUT = (8, 30)

FISCAL_INDICATORS = {
    "sovereign_debt": {
        "total_debt_gdp": "GC.DOD.TOTL.GD.ZS",
        "external_debt_gdp": "DT.DOD.DECT.GN.ZS",
        "central_govt_debt": "GC.DOD.TOTL.CN",
    },
    "fiscal_balance": {
        "fiscal_balance_gdp": "GC.NLD.TOTL.GD.ZS",
        "revenue_gdp": "GC.REV.XGRT.GD.ZS",
        "expenditure_gdp": "GC.XPN.TOTL.GD.ZS",
    },
    "tax_revenue": {
        "tax_revenue_gdp": "GC.TAX.TOTL.GD.ZS",
        "income_tax": "GC.TAX.YPKG.RV.ZS",
        "goods_services_tax": "GC.TAX.GSRV.RV.ZS",
        "trade_tax": "GC.TAX.INTT.RV.ZS",
        "customs_duties": "GC.TAX.IMPT.ZS",
    },
    "government_expenditure": {
        "total_expenditure": "GC.XPN.TOTL.GD.ZS",
        "education": "SE.XPD.TOTL.GD.ZS",
        "health": "SH.XPD.GHED.GD.ZS",
        "military": "MS.MIL.XPND.GD.ZS",
        "subsidies_transfers": "GC.XPN.TRFT.ZS",
        "compensation_of_employees": "GC.XPN.COMP.ZS",
    },
    "debt_service": {
        "debt_service_exports": "DT.TDS.DECT.EX.ZS",
        "debt_service_gni": "DT.TDS.DECT.GN.ZS",
        "interest_payments_revenue": "GC.XPN.INTP.RV.ZS",
        "principal_repayments": "DT.AMT.DECT.CD",
    },
    "external_debt": {
        "external_debt_stocks": "DT.DOD.DECT.CD",
        "external_debt_gni": "DT.DOD.DECT.GN.ZS",
        "short_term_debt": "DT.DOD.DSTC.CD",
        "concessional_debt": "DT.DOD.ALLC.CD",
        "reserves_months_imports": "FI.RES.TOTL.MO",
    },
}

COUNTRY_MAPPING = {
    "USA": "US", "GBR": "GB", "DEU": "DE", "FRA": "FR", "JPN": "JP",
    "CHN": "CN", "IND": "IN", "BRA": "BR", "RUS": "RU", "CAN": "CA",
    "AUS": "AU", "KOR": "KR", "ESP": "ES", "ITA": "IT", "MEX": "MX",
    "IDN": "ID", "NLD": "NL", "SAU": "SA", "CHE": "CH", "SWE": "SE",
    "TUR": "TR", "ZAF": "ZA", "ARG": "AR", "NGA": "NG", "EGY": "EG",
    "SGP": "SG", "MYS": "MY", "THA": "TH", "PHL": "PH", "VNM": "VN",
    "PAK": "PK", "BGD": "BD", "POL": "PL", "GRC": "GR", "PRT": "PT",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
    params["per_page"] = params.get("per_page", 50)
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


def _get_indicator(country: str, indicator_id: str, year: str = "") -> Dict:
    country_code = COUNTRY_MAPPING.get(country.upper(), country)
    endpoint = f"country/{country_code}/indicator/{indicator_id}"
    params = {"mrv": 10}
    if year:
        params["date"] = year
    data = _make_request(endpoint, params)
    if isinstance(data, list) and len(data) >= 2:
        meta = data[0]
        observations = data[1] or []
        cleaned = [
            {"year": fc.number(obs.get("date", "")), "value": obs.get("value", None)}
            for obs in observations
            if obs.get("value") is not None
        ]
        return {
            "country": country,
            "country_code": country_code,
            "indicator_id": indicator_id,
            "indicator_name": (observations[0].get("indicator", {}) or {}).get("value", "") if observations else "",
            "total": meta.get("total", 0),
            "data": sorted(cleaned, key=lambda x: x["year"]),
        }
    if isinstance(data, dict) and "error" in data:
        return {"error": f"{data['error']} (indicator {indicator_id})"}
    return {"error": f"Unexpected World Bank API response for indicator {indicator_id}"}


def _get_multiple_indicators(country: str, indicators: Dict[str, str], year: str = "") -> Dict:
    return fc.parallel_map(lambda indicator_id: _get_indicator(country, indicator_id, year), indicators)


def get_sovereign_debt(country: str, year: str = "") -> Dict:
    indicators = FISCAL_INDICATORS["sovereign_debt"]
    data = _get_multiple_indicators(country, indicators, year)
    return {"country": country, "category": "sovereign_debt", "indicators": data}


def get_fiscal_balance(country: str, year: str = "") -> Dict:
    indicators = FISCAL_INDICATORS["fiscal_balance"]
    data = _get_multiple_indicators(country, indicators, year)
    return {"country": country, "category": "fiscal_balance", "indicators": data}


def get_tax_revenue(country: str, year: str = "") -> Dict:
    indicators = FISCAL_INDICATORS["tax_revenue"]
    data = _get_multiple_indicators(country, indicators, year)
    return {"country": country, "category": "tax_revenue", "indicators": data}


def get_government_expenditure(country: str, year: str = "", sector: str = "") -> Dict:
    if sector and sector in FISCAL_INDICATORS["government_expenditure"]:
        indicator_id = FISCAL_INDICATORS["government_expenditure"][sector]
        data = _get_indicator(country, indicator_id, year)
        return {"country": country, "sector": sector, "data": data}
    indicators = FISCAL_INDICATORS["government_expenditure"]
    data = _get_multiple_indicators(country, indicators, year)
    return {"country": country, "category": "government_expenditure", "sector": sector or "all", "indicators": data}


def get_debt_service(country: str, year: str = "") -> Dict:
    indicators = FISCAL_INDICATORS["debt_service"]
    data = _get_multiple_indicators(country, indicators, year)
    return {"country": country, "category": "debt_service", "indicators": data}


def get_external_debt(country: str, year: str = "") -> Dict:
    indicators = FISCAL_INDICATORS["external_debt"]
    data = _get_multiple_indicators(country, indicators, year)
    return {"country": country, "category": "external_debt", "indicators": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "debt":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_sovereign_debt(country, year)
    elif command == "fiscal":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_fiscal_balance(country, year)
    elif command == "tax":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_tax_revenue(country, year)
    elif command == "expenditure":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        sector = args[3] if len(args) > 3 else ""
        result = get_government_expenditure(country, year, sector)
    elif command == "service":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_debt_service(country, year)
    elif command == "external":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_external_debt(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
