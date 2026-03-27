"""
Country Risk / Governance Indicators Data Fetcher
Political stability, rule of law, corruption control, and regulatory quality
via the World Bank Worldwide Governance Indicators (WGI) API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('PRS_API_KEY', '')
BASE_URL = "https://info.worldbank.org/governance/wgi"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

WGI_INDICATORS = {
    "voice_accountability": "VA.EST",
    "political_stability": "PV.EST",
    "government_effectiveness": "GE.EST",
    "regulatory_quality": "RQ.EST",
    "rule_of_law": "RL.EST",
    "control_of_corruption": "CC.EST",
}

WB_API_BASE = "https://api.worldbank.org/v2"


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


def _fetch_wgi(indicator_code: str, country: str = None, year: str = None) -> Any:
    country_code = country if country else "all"
    url = f"{WB_API_BASE}/country/{country_code}/indicator/WGI.{indicator_code}"
    params = {"format": "json", "per_page": 200}
    if year:
        params["date"] = year
    return _make_request(url, params)


def get_governance_indicators(country: str = None, indicator: str = None, year: str = None) -> Any:
    if indicator and indicator in WGI_INDICATORS:
        return _fetch_wgi(WGI_INDICATORS[indicator], country, year)
    results = {}
    for name, code in WGI_INDICATORS.items():
        results[name] = _fetch_wgi(code, country, year)
    return results


def get_political_stability(country: str = None, year: str = None) -> Any:
    return _fetch_wgi(WGI_INDICATORS["political_stability"], country, year)


def get_rule_of_law(country: str = None, year: str = None) -> Any:
    return _fetch_wgi(WGI_INDICATORS["rule_of_law"], country, year)


def get_corruption_control(country: str = None, year: str = None) -> Any:
    return _fetch_wgi(WGI_INDICATORS["control_of_corruption"], country, year)


def get_government_effectiveness(country: str = None, year: str = None) -> Any:
    return _fetch_wgi(WGI_INDICATORS["government_effectiveness"], country, year)


def get_regulatory_quality(country: str = None, year: str = None) -> Any:
    return _fetch_wgi(WGI_INDICATORS["regulatory_quality"], country, year)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "governance":
        country = args[1] if len(args) > 1 else None
        indicator = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_governance_indicators(country, indicator, year)
    elif command == "stability":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_political_stability(country, year)
    elif command == "rule_of_law":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_rule_of_law(country, year)
    elif command == "corruption":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_corruption_control(country, year)
    elif command == "effectiveness":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_government_effectiveness(country, year)
    elif command == "regulatory":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_regulatory_quality(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
