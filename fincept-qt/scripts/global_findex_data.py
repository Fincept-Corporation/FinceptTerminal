"""
World Bank Global Findex Data Fetcher
Fetches financial inclusion data — bank accounts, digital payments, credit access,
savings rates, and mobile money for 140+ countries from the Global Findex database.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLDBANK_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2/en/indicator"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

FINDEX_INDICATORS = {
    "account_ownership": {
        "account_total": "FX.OWN.TOTL.ZS",
        "account_female": "FX.OWN.TOTL.FE.ZS",
        "account_male": "FX.OWN.TOTL.MA.ZS",
        "account_young_adults": "FX.OWN.TOTL.YG.ZS",
        "account_poorest40": "FX.OWN.TOTL.PL.ZS",
        "account_rural": "FX.OWN.TOTL.RU.ZS",
        "account_secondary_edu": "FX.OWN.TOTL.SO.ZS",
    },
    "digital_payments": {
        "digital_payments_total": "FX.PAY.TOTL.ZS",
        "digital_payments_female": "FX.PAY.TOTL.FE.ZS",
        "mobile_internet_payments": "FX.PAY.MINX.ZS",
        "received_digital_payment": "FX.REG.DIRI.ZS",
        "govt_transfer_digital": "FX.PAY.GOVX.ZS",
    },
    "credit_access": {
        "borrowed_formal": "FX.BRW.FRML.ZS",
        "borrowed_informal": "FX.BRW.IFRM.ZS",
        "credit_card": "FX.CRD.CTTL.ZS",
        "borrowed_employer": "FX.BRW.EMPL.ZS",
        "borrowed_family": "FX.BRW.FAMF.ZS",
        "loan_outstanding": "FX.CRD.DSTK.ZS",
    },
    "savings": {
        "saved_formal": "FX.SAV.FRML.ZS",
        "saved_informal": "FX.SAV.IFRM.ZS",
        "saved_pension": "FX.SAV.PENS.ZS",
        "saved_money_any": "FX.SAV.TOTL.ZS",
        "saved_for_emergencies": "FX.SAV.EMRG.ZS",
        "saved_agricultural": "FX.SAV.AGRI.ZS",
    },
    "mobile_money": {
        "mobile_account": "FX.MOB.TOTL.ZS",
        "mobile_account_female": "FX.MOB.TOTL.FE.ZS",
        "mobile_account_male": "FX.MOB.TOTL.MA.ZS",
        "mobile_account_poorest40": "FX.MOB.TOTL.PL.ZS",
        "mobile_account_rural": "FX.MOB.TOTL.RU.ZS",
        "used_mobile_to_pay": "FX.MOB.PAYB.ZS",
    },
    "financial_inclusion_index": {
        "debit_card": "FX.CRD.DBIT.ZS",
        "made_digital_payment_1y": "FX.PAY.MINX.ZS",
        "paid_digitally_in_store": "FX.PAY.INST.ZS",
        "no_account_reason_far": "FX.NEA.DIST.ZS",
        "no_account_reason_cost": "FX.NEA.COST.ZS",
        "no_account_reason_doc": "FX.NEA.PROC.ZS",
        "no_account_reason_trust": "FX.NEA.TRST.ZS",
        "unbanked_adults_pct": "FX.NEA.TOTL.ZS",
    },
}

COUNTRY_CODES = {
    "USA": "US", "GBR": "GB", "DEU": "DE", "FRA": "FR", "JPN": "JP",
    "CHN": "CN", "IND": "IN", "BRA": "BR", "RUS": "RU", "CAN": "CA",
    "AUS": "AU", "KOR": "KR", "ESP": "ES", "ITA": "IT", "MEX": "MX",
    "IDN": "ID", "NLD": "NL", "SAU": "SA", "CHE": "CH", "SWE": "SE",
    "TUR": "TR", "ZAF": "ZA", "ARG": "AR", "NGA": "NG", "EGY": "EG",
    "SGP": "SG", "MYS": "MY", "THA": "TH", "PHL": "PH", "VNM": "VN",
    "PAK": "PK", "BGD": "BD", "POL": "PL", "GRC": "GR", "KEN": "KE",
    "TZA": "TZ", "ETH": "ET", "GHA": "GH", "MOZ": "MZ", "RWA": "RW",
}

FINDEX_YEARS = ["2011", "2014", "2017", "2021"]


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
    params["per_page"] = params.get("per_page", 100)
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


def _get_findex_indicator(country: str, indicator_id: str, year: str = "") -> Dict:
    country_code = COUNTRY_CODES.get(country.upper(), country)
    url = f"https://api.worldbank.org/v2/country/{country_code}/indicator/{indicator_id}"
    params = {"format": "json", "per_page": 20}
    if year:
        params["date"] = year
    else:
        params["date"] = "2011:2022"
    data = _make_request(url, params)
    if isinstance(data, list) and len(data) >= 2:
        observations = data[1] or []
        cleaned = [
            {"year": obs.get("date", ""), "value": obs.get("value", None)}
            for obs in observations
            if obs.get("value") is not None
        ]
        return {
            "country": country,
            "indicator_id": indicator_id,
            "indicator_name": (observations[0].get("indicator", {}) or {}).get("value", "") if observations else "",
            "unit": "% of population age 15+",
            "findex_years": FINDEX_YEARS,
            "data": sorted(cleaned, key=lambda x: x["year"]),
        }
    if isinstance(data, dict) and "error" in data:
        return data
    return {"error": "Unexpected response", "country": country, "indicator": indicator_id}


def _get_category_indicators(country: str, category: str, year: str = "") -> Dict:
    indicators = FINDEX_INDICATORS.get(category, {})
    if not indicators:
        return {"error": f"Unknown category: {category}", "available": list(FINDEX_INDICATORS.keys())}
    result = {"country": country, "category": category, "indicators": {}}
    for name, ind_id in indicators.items():
        result["indicators"][name] = _get_findex_indicator(country, ind_id, year)
    return result


def get_account_ownership(country: str, year: str = "") -> Dict:
    return _get_category_indicators(country, "account_ownership", year)


def get_digital_payments(country: str, year: str = "") -> Dict:
    return _get_category_indicators(country, "digital_payments", year)


def get_credit_access(country: str, year: str = "") -> Dict:
    return _get_category_indicators(country, "credit_access", year)


def get_savings_rate(country: str, year: str = "") -> Dict:
    return _get_category_indicators(country, "savings", year)


def get_mobile_money(country: str, year: str = "") -> Dict:
    return _get_category_indicators(country, "mobile_money", year)


def get_financial_inclusion_index(country: str, year: str = "") -> Dict:
    return _get_category_indicators(country, "financial_inclusion_index", year)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "accounts":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_account_ownership(country, year)
    elif command == "digital_payments":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_digital_payments(country, year)
    elif command == "credit":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_credit_access(country, year)
    elif command == "savings":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_savings_rate(country, year)
    elif command == "mobile_money":
        country = args[1] if len(args) > 1 else "KEN"
        year = args[2] if len(args) > 2 else ""
        result = get_mobile_money(country, year)
    elif command == "index":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_financial_inclusion_index(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
