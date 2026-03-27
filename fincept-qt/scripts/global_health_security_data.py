"""
Global Health Security (GHS) Index & WHO IHR Data Fetcher
Provides Global Health Security Index scores, pandemic preparedness assessments,
country rankings, category scores, and historical trend data.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GHS_API_KEY', '')
BASE_URL = "https://www.ghsindex.org/api"

# GHS public data is hosted as JSON on the GHS Index website
GHS_DATA_URL = "https://www.ghsindex.org/wp-content/uploads/2021/10"

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


def _fetch_ghs_data(year: int = 2021) -> Any:
    """Fetch GHS Index dataset for a given year."""
    # GHS Index published in 2019 and 2021
    if year == 2019:
        url = "https://www.ghsindex.org/wp-content/uploads/2020/04/2019-Global-Health-Security-Index.json"
    else:
        url = "https://www.ghsindex.org/wp-content/uploads/2021/10/2021-GHS-Index-April-2022.json"
    try:
        response = session.get(url, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError:
        return _make_request(f"scores/{year}")
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_index_scores(year: int = 2021) -> Any:
    """Return GHS Index overall scores for all countries in a given year."""
    data = _fetch_ghs_data(year)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"year": year, "scores": data}


def get_country_score(country: str, year: int = 2021) -> Any:
    """Return the GHS Index score and breakdown for a specific country."""
    data = _fetch_ghs_data(year)
    if isinstance(data, dict) and "error" in data:
        return data
    if isinstance(data, list):
        for entry in data:
            iso = entry.get("ISO_Alpha_3_Code", entry.get("iso3", entry.get("country_code", "")))
            name = entry.get("Country_Name", entry.get("country", ""))
            if (iso and iso.upper() == country.upper()) or (name and name.lower() == country.lower()):
                return {"country": country, "year": year, "score": entry}
        return {"country": country, "year": year, "score": None,
                "note": f"Country '{country}' not found in {year} GHS Index data"}
    return {"country": country, "year": year, "data": data}


def get_category_scores(country: str, year: int = 2021) -> Any:
    """Return all 6 category scores for a country (prevention, detection, response, etc.)."""
    country_data = get_country_score(country, year)
    if "error" in country_data:
        return country_data
    score_entry = country_data.get("score", {}) or {}
    categories = {
        "prevention": score_entry.get("Category_1_Prevention", score_entry.get("prevention")),
        "detection": score_entry.get("Category_2_Detection", score_entry.get("detection")),
        "response": score_entry.get("Category_3_Response", score_entry.get("response")),
        "health_system": score_entry.get("Category_4_Health_System", score_entry.get("health_system")),
        "compliance": score_entry.get("Category_5_Compliance", score_entry.get("compliance")),
        "risk_environment": score_entry.get("Category_6_Risk_Environment", score_entry.get("risk_environment")),
        "overall": score_entry.get("Overall_Score", score_entry.get("overall"))
    }
    return {"country": country, "year": year, "categories": categories}


def get_rankings(year: int = 2021, category: str = "overall") -> Any:
    """Return all countries ranked by overall or category GHS score."""
    data = _fetch_ghs_data(year)
    if isinstance(data, dict) and "error" in data:
        return data
    if isinstance(data, list):
        category_map = {
            "overall": "Overall_Score",
            "prevention": "Category_1_Prevention",
            "detection": "Category_2_Detection",
            "response": "Category_3_Response",
            "health_system": "Category_4_Health_System",
            "compliance": "Category_5_Compliance",
            "risk_environment": "Category_6_Risk_Environment"
        }
        score_key = category_map.get(category, "Overall_Score")
        ranked = sorted(data, key=lambda x: float(x.get(score_key, 0) or 0), reverse=True)
        for i, entry in enumerate(ranked, 1):
            entry["rank"] = i
        return {"year": year, "category": category, "rankings": ranked, "count": len(ranked)}
    return {"year": year, "category": category, "rankings": data}


def get_historical(country: str) -> Any:
    """Return GHS Index scores for a country across all available survey years (2019, 2021)."""
    history = []
    for year in [2019, 2021]:
        country_data = get_country_score(country, year)
        if not (isinstance(country_data, dict) and "error" in country_data):
            history.append({"year": year, "data": country_data.get("score", {})})
    return {"country": country, "historical_scores": history,
            "years_available": [h["year"] for h in history]}


def get_indicators() -> Any:
    """Return the GHS Index framework: categories, indicators, and sub-indicators."""
    framework = {
        "categories": [
            {
                "id": 1, "name": "Prevention",
                "description": "Prevention of the emergence or release of pathogens",
                "indicators": ["Antimicrobial resistance", "Zoonotic disease", "Biosafety", "Biosecurity", "Dual-use research", "Immunization"]
            },
            {
                "id": 2, "name": "Detection & Reporting",
                "description": "Early detection and reporting of epidemics of potential international concern",
                "indicators": ["Laboratory systems", "Real-time surveillance", "Reporting"]
            },
            {
                "id": 3, "name": "Rapid Response",
                "description": "Rapid response to and mitigation of the spread of an epidemic",
                "indicators": ["Emergency preparedness", "Exercising response plans", "Emergency response operations", "Linking public health & security"]
            },
            {
                "id": 4, "name": "Health System",
                "description": "Sufficient and robust health system to treat the sick and protect health workers",
                "indicators": ["Medical countermeasures", "Healthcare capacity", "Communications with public", "Access to communications infrastructure"]
            },
            {
                "id": 5, "name": "Commitments",
                "description": "Commitments to improving national capacity, financing and adherence to norms",
                "indicators": ["IHR compliance", "Financing", "Commitment to norms"]
            },
            {
                "id": 6, "name": "Risk Environment",
                "description": "Overall risk environment and country vulnerability",
                "indicators": ["Political and security risks", "Socioeconomic resilience", "Infrastructure adequacy", "Environmental risks", "Public health vulnerabilities"]
            }
        ],
        "total_indicators": 34,
        "total_sub_indicators": 85
    }
    return {"framework": framework}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "scores":
        year = int(args[1]) if len(args) > 1 else 2021
        result = get_index_scores(year)
    elif command == "country":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else 2021
            result = get_country_score(country, year)
    elif command == "categories":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else 2021
            result = get_category_scores(country, year)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else 2021
        category = args[2] if len(args) > 2 else "overall"
        result = get_rankings(year, category)
    elif command == "historical":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            result = get_historical(country)
    elif command == "indicators":
        result = get_indicators()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
