"""
World Bank Global Findex Data Fetcher
Fetches financial inclusion data — bank accounts, digital payments, credit access,
savings rates, and mobile money for 140+ countries from the Global Findex database.
"""
import sys
import json
import os
import requests
import fincept_connector as fc
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLDBANK_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2/en/indicator"

# The World Bank API often takes 10-20 s per uncached query and sometimes stalls for minutes:
# no read retries (they multiplied the timeout), a bounded timeout, and the indicators of one
# category are fetched concurrently so a command finishes well inside the runner's limits.
session = fc.http_session()
TIMEOUT = (8, 30)

# Global Findex series as published in World Bank API source 28 (Global Financial Inclusion).
# The ".N" suffixes are breakdowns: .1 women, .2 men, .3 ages 15-24, .6 secondary education or
# more, .7 poorest 40%, .9 rural.
FINDEX_SOURCE = 28
FINDEX_INDICATORS = {
    "account_ownership": {
        "account_total": "account.t.d",
        "account_female": "account.t.d.1",
        "account_male": "account.t.d.2",
        "account_young_adults": "account.t.d.3",
        "account_poorest40": "account.t.d.7",
        "account_rural": "account.t.d.9",
        "account_secondary_edu": "account.t.d.6",
    },
    "digital_payments": {
        "digital_payments_total": "g20.any",
        "digital_payments_female": "g20.any.1",
        "mobile_internet_payments": "fin26a",
        "received_digital_payment": "g20.received",
        "govt_transfer_digital": "fin37.39.acc",
    },
    "credit_access": {
        "borrowed_formal": "fin22a",
        "borrowed_informal": "fin22c",
        "credit_card": "fin10",
        "borrowed_any": "borrow.any.t.d",
        "borrowed_family": "fin22b",
        "borrowed_for_business": "fin22e",
    },
    "savings": {
        "saved_formal": "fin17a.17a1.d",
        "saved_informal": "fin17c",
        "saved_pension": "fin17f",
        "saved_money_any": "save.any.t.d",
        "saved_mobile_money": "fin17b",
        "emergency_funds_possible": "fin24aP",
    },
    "mobile_money": {
        "mobile_account": "mobileaccount.t.d",
        "mobile_account_female": "mobileaccount.t.d.1",
        "mobile_account_male": "mobileaccount.t.d.2",
        "mobile_account_poorest40": "mobileaccount.t.d.7",
        "mobile_account_rural": "mobileaccount.t.d.9",
        "used_mobile_to_pay": "fin26a",
    },
    "financial_inclusion_index": {
        "debit_card": "fin2.t.d",
        "made_digital_payment_1y": "g20.made",
        "paid_digitally_in_store": "fin25e2",
        "no_account_reason_far": "fin11a",
        "no_account_reason_cost": "fin11b",
        "no_account_reason_doc": "fin11c",
        "no_account_reason_trust": "fin11d",
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

FINDEX_YEARS = ["2011", "2014", "2017", "2021", "2024"]


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
    params["per_page"] = params.get("per_page", 100)
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


def _get_findex_indicator(country: str, indicator_id: str, year: str = "") -> Dict:
    country_code = COUNTRY_CODES.get(country.upper(), country)
    url = f"https://api.worldbank.org/v2/country/{country_code}/indicator/{indicator_id}"
    params = {"format": "json", "per_page": 100, "source": FINDEX_SOURCE}
    if year:
        params["date"] = year
    data = _make_request(url, params)
    if isinstance(data, list) and len(data) >= 2:
        observations = data[1] or []
        cleaned = [
            {"year": fc.number(obs.get("date", "")), "value": obs.get("value", None)}
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
        return {"error": f"{data['error']} (indicator {indicator_id})"}
    return {"error": f"Unexpected World Bank API response for indicator {indicator_id}"}


def _get_category_indicators(country: str, category: str, year: str = "") -> Dict:
    indicators = FINDEX_INDICATORS.get(category, {})
    if not indicators:
        return {"error": f"Unknown category: {category}", "available": list(FINDEX_INDICATORS.keys())}
    fetched = fc.parallel_map(lambda ind_id: _get_findex_indicator(country, ind_id, year), indicators)
    return {"country": country, "category": category, "indicators": fetched}


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
