"""
Country Risk / Governance Indicators Data Fetcher
Political stability, rule of law, corruption control, and regulatory quality
via the World Bank Worldwide Governance Indicators (WGI) API.
"""
import sys
import json
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.worldbank.org/v2"

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
    """WGI estimates from the World Bank API (source 3 = Worldwide Governance Indicators).
    The indicator ids are GOV_WGI_<code> (the old WGI.<code> ids were removed). Without a year,
    the most recent 10 values are returned for one country, or the latest value for all."""
    country_code = country.replace(",", ";") if country else "all"
    url = f"{WB_API_BASE}/country/{country_code}/indicator/GOV_WGI_{indicator_code}"
    params = {"format": "json", "per_page": 20000, "source": 3}
    if year:
        params["date"] = year
    else:
        params["mrv"] = 1 if country_code == "all" else 10
    data = _make_request(url, params)
    if isinstance(data, dict):
        return data
    if isinstance(data, list) and data and isinstance(data[0], dict) and "message" in data[0]:
        msg = data[0]["message"][0] if data[0]["message"] else {}
        return {"error": f"World Bank API: {msg.get('value') or msg}"}
    if not isinstance(data, list) or len(data) < 2 or not data[1]:
        return {"error": "No data returned"}
    rows = [{"country": r["country"]["value"], "iso3": r.get("countryiso3code"), "year": r.get("date"),
             "value": r.get("value")} for r in data[1] if r.get("value") is not None]
    return {"indicator": data[1][0]["indicator"]["value"], "code": f"GOV_WGI_{indicator_code}",
            "scale": "approx. -2.5 (weak) to +2.5 (strong governance)", "count": len(rows), "data": rows}


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
