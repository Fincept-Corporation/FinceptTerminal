"""
Global Competitiveness Data Fetcher
IMD World Competitiveness Yearbook + WEF GCI proxy via World Bank: competitiveness indicators by country.
"""
import sys
import json
import os
import requests
import fincept_connector as fc
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.worldbank.org/v2/en/indicator"

COMPETITIVENESS_INDICATORS = {
    "infrastructure": "IS.AIR.PSGR",
    "technology_readiness": "IT.NET.USER.ZS",
    # The Doing Business "ease of doing business" index (IC.BUS.EASE.XQ) was discontinued and its
    # data can no longer be queried (the API answers "indicator not found"); the Worldwide
    # Governance Indicators' Regulatory Quality estimate is the closest maintained series.
    "market_efficiency": "GOV_WGI_RQ.EST",
    "institutions": "IQ.CPA.PUBS.XQ",
    "innovation": "IP.PAT.RESD",
    "business_sophistication": "IC.FRM.TRNG.ZS",
    "higher_education": "SE.TER.ENRR",
    "financial_market": "FB.BNK.CAPA.ZS"
}
# World Bank API source per indicator: GOV_WGI_* are source 3 (Worldwide Governance
# Indicators); everything else is source 2 (World Development Indicators).
INDICATOR_SOURCE = {"GOV_WGI_RQ.EST": 3}

# The World Bank API often takes 10-20 s per uncached query and sometimes stalls for minutes:
# no read retries (they multiplied the timeout) and a bounded timeout.
session = fc.http_session()
TIMEOUT = (8, 30)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
    params.setdefault("per_page", 100)
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
    if isinstance(data, dict):
        return data
    if not isinstance(data, list) or len(data) < 2 or not data[1]:
        return {"error": "No World Bank data for this indicator / country / year"}
    rows = [{"country": (r.get("country") or {}).get("value"), "iso3": r.get("countryiso3code"),
             "year": fc.number(r.get("date")), "value": r.get("value")}
            for r in data[1] if r.get("value") is not None]
    ind = data[1][0].get("indicator") or {}
    return {"indicator": ind.get("value"), "code": ind.get("id"), "count": len(rows), "data": rows}


def _fetch_indicator(indicator_code: str, country: str, year: int) -> Any:
    url = f"https://api.worldbank.org/v2/country/{country}/indicator/{indicator_code}"
    params = {"date": str(year), "format": "json", "source": INDICATOR_SOURCE.get(indicator_code, 2)}
    return _make_request(url, params=params)


def get_infrastructure_score(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["infrastructure"], country, year)


def get_technology_readiness(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["technology_readiness"], country, year)


def get_market_efficiency(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["market_efficiency"], country, year)


def get_institutions_score(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["institutions"], country, year)


def get_innovation_score(country: str = "US", year: int = 2022) -> Any:
    return _fetch_indicator(COMPETITIVENESS_INDICATORS["innovation"], country, year)


def get_rankings(indicator: str = "technology_readiness", year: int = 2022) -> Any:
    indicator_code = COMPETITIVENESS_INDICATORS.get(indicator)
    if indicator_code is None:
        return {"error": f"Unknown indicator '{indicator}' - choose one of: "
                         f"{', '.join(COMPETITIVENESS_INDICATORS)}"}
    url = f"https://api.worldbank.org/v2/country/all/indicator/{indicator_code}"
    params = {"date": str(year), "format": "json", "per_page": 400,
              "source": INDICATOR_SOURCE.get(indicator_code, 2)}
    result = _make_request(url, params=params)
    if isinstance(result, dict) and isinstance(result.get("data"), list):
        result["data"].sort(key=lambda r: r["value"], reverse=True)  # aggregates (World, regions) included
    return result


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if len(args) > 2 and not args[2].isdigit():
        print(json.dumps({"error": f"year must be a number, got '{args[2]}'"}))
        return
    if command == "infrastructure":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_infrastructure_score(country, year)
    elif command == "technology":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_technology_readiness(country, year)
    elif command == "market":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_market_efficiency(country, year)
    elif command == "institutions":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_institutions_score(country, year)
    elif command == "innovation":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_innovation_score(country, year)
    elif command == "rankings":
        indicator = args[1] if len(args) > 1 else "technology_readiness"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_rankings(indicator, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
