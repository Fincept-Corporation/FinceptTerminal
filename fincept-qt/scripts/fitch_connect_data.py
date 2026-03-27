"""
World Bank Sovereign Fiscal Data Fetcher
Fetches sovereign debt data, budget transparency, and fiscal statistics using the
World Bank Open Finances and WDI APIs — no API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLDBANK_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

FISCAL_INDICATORS = {
    "sovereign_debt": {
        "total_debt_gdp": "GC.DOD.TOTL.GD.ZS",
        "external_debt_gdp": "DT.DOD.DECT.GN.ZS",
        "domestic_debt": "GC.DOD.DOMX.CN",
        "central_govt_debt": "GC.DOD.TOTL.CN",
    },
    "fiscal_balance": {
        "fiscal_balance_gdp": "GC.BAL.CASH.GD.ZS",
        "primary_balance_gdp": "GC.SUR.TOTL.GD.ZS",
        "revenue_gdp": "GC.REV.TOTL.GD.ZS",
        "expenditure_gdp": "GC.XPN.TOTL.GD.ZS",
    },
    "tax_revenue": {
        "tax_revenue_gdp": "GC.TAX.TOTL.GD.ZS",
        "income_tax": "GC.TAX.YPKG.RV.ZS",
        "goods_services_tax": "GC.TAX.GSRV.RV.ZS",
        "trade_tax": "GC.TAX.INTT.RV.ZS",
        "customs_duties": "TM.TAX.MRCH.IP.ZS",
    },
    "government_expenditure": {
        "total_expenditure": "GC.XPN.TOTL.GD.ZS",
        "education": "SE.XPD.TOTL.GD.ZS",
        "health": "SH.XPD.GHED.GD.ZS",
        "military": "MS.MIL.XPND.GD.ZS",
        "social_protection": "GC.XPN.SOCL.ZS",
        "infrastructure": "GC.XPN.OFST.ZS",
    },
    "debt_service": {
        "debt_service_exports": "DT.TDS.DECT.EX.ZS",
        "debt_service_gni": "DT.TDS.DECT.GN.ZS",
        "interest_payments_gdp": "GC.XPN.INTP.GD.ZS",
        "principal_repayments": "DT.AMT.DECT.CD",
    },
    "external_debt": {
        "external_debt_stocks": "DT.DOD.DECT.CD",
        "external_debt_gni": "DT.DOD.DECT.GN.ZS",
        "short_term_debt": "DT.DOD.DSTC.CD",
        "concessional_debt": "DT.DOD.PCBK.CD",
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
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


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
            {"year": obs.get("date", ""), "value": obs.get("value", None)}
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
        return data
    return {"error": "Unexpected response format", "raw": str(data)[:200]}


def _get_multiple_indicators(country: str, indicators: Dict[str, str], year: str = "") -> Dict:
    result = {}
    for name, indicator_id in indicators.items():
        result[name] = _get_indicator(country, indicator_id, year)
    return result


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
