"""
Global Risks Data Fetcher
WEF Global Risks Report data: risk likelihood, impact scores, interconnections from annual survey.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.weforum.org/reports/global-risks-report"

RISK_CATEGORIES = {
    "economic": ["Widespread cybercrime", "Asset bubble burst", "Debt crisis", "Commodity shocks"],
    "environmental": ["Climate action failure", "Extreme weather", "Biodiversity loss", "Natural disasters"],
    "geopolitical": ["State collapse", "Interstate conflict", "Terrorism", "Weapons of mass destruction"],
    "societal": ["Cost-of-living crisis", "Erosion of social cohesion", "Infectious diseases", "Mental health deterioration"],
    "technological": ["Adverse AI outcomes", "Digital power concentration", "Cybersecurity failures", "Digital inequality"]
}

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


def get_risks(year: int = 2024) -> Any:
    params = {"year": year}
    result = _make_request("risks", params=params)
    if "error" in result:
        result["categories"] = RISK_CATEGORIES
        result["source"] = "WEF Global Risks Report"
        result["note"] = "Public API may not be available; data can be accessed at weforum.org"
    return result


def get_risk_score(risk_id: str, year: int = 2024) -> Any:
    params = {"risk": risk_id, "year": year}
    return _make_request("score", params=params)


def get_top_risks(year: int = 2024, category: str = None, n: int = 10) -> Any:
    params = {"year": year, "n": n}
    if category:
        params["category"] = category
    result = _make_request("top", params=params)
    if "error" in result:
        cat_risks = RISK_CATEGORIES.get(category, []) if category else [r for risks in RISK_CATEGORIES.values() for r in risks]
        result["top_risks_reference"] = cat_risks[:n]
    return result


def get_risk_trend(risk_id: str, start_year: int = 2015, end_year: int = 2024) -> Any:
    params = {
        "risk": risk_id,
        "startYear": start_year,
        "endYear": end_year
    }
    return _make_request("trend", params=params)


def get_categories(year: int = 2024) -> Any:
    result = _make_request("categories", params={"year": year})
    if "error" in result:
        result["categories"] = list(RISK_CATEGORIES.keys())
        result["risks_by_category"] = RISK_CATEGORIES
    return result


def get_interconnections(year: int = 2024) -> Any:
    params = {"year": year}
    return _make_request("interconnections", params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "risks":
        year = int(args[1]) if len(args) > 1 else 2024
        result = get_risks(year)
    elif command == "score":
        risk_id = args[1] if len(args) > 1 else "climate_action_failure"
        year = int(args[2]) if len(args) > 2 else 2024
        result = get_risk_score(risk_id, year)
    elif command == "top":
        year = int(args[1]) if len(args) > 1 else 2024
        category = args[2] if len(args) > 2 else None
        n = int(args[3]) if len(args) > 3 else 10
        result = get_top_risks(year, category, n)
    elif command == "trend":
        risk_id = args[1] if len(args) > 1 else "climate_action_failure"
        start_year = int(args[2]) if len(args) > 2 else 2015
        end_year = int(args[3]) if len(args) > 3 else 2024
        result = get_risk_trend(risk_id, start_year, end_year)
    elif command == "categories":
        year = int(args[1]) if len(args) > 1 else 2024
        result = get_categories(year)
    elif command == "interconnections":
        year = int(args[1]) if len(args) > 1 else 2024
        result = get_interconnections(year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
