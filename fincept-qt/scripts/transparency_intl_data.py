"""
Transparency International CPI Data Fetcher
Provides Corruption Perception Index (CPI) scores, country rankings,
historical trends, and regional averages from Transparency International.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('TRANSPARENCY_API_KEY', '')
BASE_URL = "https://www.transparency.org/api"

# Public CPI data JSON endpoint
CPI_DATA_URL = "https://images.transparencycdn.org/images/CPI2023_GlobalResults.json"

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


def _fetch_cpi_dataset(year: int = 2023) -> Any:
    """Fetch CPI dataset for a given year from TI public CDN."""
    url = f"https://images.transparencycdn.org/images/CPI{year}_GlobalResults.json"
    try:
        response = session.get(url, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError:
        # Try CSV fallback via TI research API
        fallback = _make_request(f"public/cpi/{year}", params={"format": "json"})
        return fallback
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_cpi_scores(year: int = 2023) -> Any:
    """Return CPI scores for all countries in a given year."""
    data = _fetch_cpi_dataset(year)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"year": year, "scores": data}


def get_cpi_country(country: str, start_year: int = 2012, end_year: int = 2023) -> Any:
    """Return CPI score history for a specific country over a year range."""
    records = []
    for year in range(start_year, end_year + 1):
        data = _fetch_cpi_dataset(year)
        if isinstance(data, dict) and "error" in data:
            continue
        if isinstance(data, list):
            for entry in data:
                iso = entry.get("ISO3") or entry.get("iso3") or entry.get("country_code", "")
                name = entry.get("Country") or entry.get("country", "")
                if iso.upper() == country.upper() or name.lower() == country.lower():
                    records.append({"year": year, "score": entry.get("CPI Score") or entry.get("score"), "data": entry})
                    break
    return {"country": country, "start_year": start_year, "end_year": end_year, "history": records}


def get_country_rankings(year: int = 2023) -> Any:
    """Return all countries ranked by CPI score for a given year."""
    data = _fetch_cpi_dataset(year)
    if isinstance(data, dict) and "error" in data:
        return data
    if isinstance(data, list):
        ranked = sorted(data, key=lambda x: float(x.get("CPI Score", x.get("score", 0)) or 0), reverse=True)
        for i, entry in enumerate(ranked, 1):
            entry["rank"] = i
        return {"year": year, "rankings": ranked, "count": len(ranked)}
    return {"year": year, "rankings": data}


def get_cpi_trends() -> Any:
    """Return global average CPI scores from 2012 to the latest available year."""
    trends = []
    for year in range(2012, 2024):
        data = _fetch_cpi_dataset(year)
        if isinstance(data, dict) and "error" in data:
            continue
        if isinstance(data, list) and len(data) > 0:
            scores = [float(e.get("CPI Score", e.get("score", 0)) or 0) for e in data if e.get("CPI Score") or e.get("score")]
            if scores:
                avg = round(sum(scores) / len(scores), 2)
                trends.append({"year": year, "global_average": avg, "country_count": len(scores)})
    return {"trends": trends}


def get_regional_averages(year: int = 2023) -> Any:
    """Return CPI regional average scores for a given year."""
    data = _fetch_cpi_dataset(year)
    if isinstance(data, dict) and "error" in data:
        return data
    if isinstance(data, list):
        regions: Dict[str, List[float]] = {}
        for entry in data:
            region = entry.get("Region", entry.get("region", "Unknown"))
            score = entry.get("CPI Score", entry.get("score"))
            if score:
                try:
                    regions.setdefault(region, []).append(float(score))
                except (ValueError, TypeError):
                    pass
        averages = {r: round(sum(v) / len(v), 2) for r, v in regions.items()}
        return {"year": year, "regional_averages": averages}
    return {"year": year, "data": data}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "scores":
        year = int(args[1]) if len(args) > 1 else 2023
        result = get_cpi_scores(year)
    elif command == "country":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            start_year = int(args[2]) if len(args) > 2 else 2012
            end_year = int(args[3]) if len(args) > 3 else 2023
            result = get_cpi_country(country, start_year, end_year)
    elif command == "rankings":
        year = int(args[1]) if len(args) > 1 else 2023
        result = get_country_rankings(year)
    elif command == "trends":
        result = get_cpi_trends()
    elif command == "regional":
        year = int(args[1]) if len(args) > 1 else 2023
        result = get_regional_averages(year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
