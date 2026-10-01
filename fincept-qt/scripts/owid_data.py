"""
Our World in Data Fetcher
CO2, energy, health, poverty, GDP per capita, democracy data for all countries.
Chart data comes from the public OWID grapher CSV endpoint
(https://ourworldindata.org/grapher/<slug>.csv) - no API key required. The GitHub JSON
datasets (owid/co2-data, owid/energy-data) this script used to read no longer exist (HTTP 404).
"""
import csv
import io
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.ourworldindata.org/v1"
GITHUB_RAW = "https://raw.githubusercontent.com/owid/owid-datasets/master/datasets"
CATALOG_URL = "https://catalog.ourworldindata.org"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
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


def _fetch_owid_csv_api(dataset: str, country: str = None, columns: List[str] = None) -> Any:
    """Fetch OWID data via the indicators API."""
    params = {"datasetCode": dataset}
    if country:
        params["entityName"] = country
    data = _make_request("indicators", params=params)
    return data


def get_indicator(indicator_id: int) -> Any:
    """Get data for a specific OWID indicator by numeric ID."""
    data = _make_request(f"indicator/{indicator_id}.json")
    if isinstance(data, dict) and "error" not in data:
        return data
    return data


def search_indicators(query: str) -> Any:
    """Search OWID indicators by keyword."""
    params = {"query": query, "limit": 30}
    data = _make_request("search", params=params)
    if isinstance(data, dict) and "results" in data:
        return {"query": query, "results": data["results"][:30], "count": len(data["results"])}
    return data


# command -> (grapher chart slug, human title)
GRAPHER_CHARTS = {
    "co2": ("annual-co2-emissions-per-country", "Annual CO2 emissions (tonnes)"),
    "co2_per_capita": ("co-emissions-per-capita", "CO2 emissions per capita (tonnes)"),
    "energy": ("primary-energy-cons", "Primary energy consumption (TWh)"),
    "life_expectancy": ("life-expectancy", "Life expectancy at birth (years)"),
    "poverty": ("share-of-population-in-extreme-poverty", "Share of population in extreme poverty (%)"),
    "gdp_per_capita": ("gdp-per-capita-worldbank", "GDP per capita (PPP, constant international $)"),
    "democracy": ("electoral-democracy-index", "Electoral democracy index (V-Dem, 0-1)"),
}
GRAPHER_CSV_URL = "https://ourworldindata.org/grapher/{slug}.csv"


def get_grapher_data(slug: str, title: str, country: str = None, start: str = None, end: str = None) -> Any:
    """Fetch an OWID chart's data and return [{country, year, value}] rows.

    country: entity name or ISO-3 code, case-insensitive ('United States', 'USA', 'World').
             Empty / 'all' returns every entity.
    start/end: inclusive year bounds.
    """
    url = GRAPHER_CSV_URL.format(slug=slug)
    try:
        response = session.get(url, params={"csvType": "full", "useColumnShortNames": "true"}, timeout=60)
        response.raise_for_status()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: OWID chart '{slug}' unavailable"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}

    reader = csv.reader(io.StringIO(response.text))
    header = next(reader, None)
    if not header or len(header) < 4:
        return {"error": f"Unexpected CSV layout for OWID chart '{slug}'"}
    # entity, code, year, <value column>, [extras...]
    wanted = (country or "").strip().lower()
    all_entities = wanted in ("", "all")
    try:
        start_year = int(start) if start else None
        end_year = int(end) if end else None
    except ValueError:
        return {"error": "Start/end must be years (e.g. 2000 2023)"}

    rows = []
    entities = set()
    for rec in reader:
        if len(rec) < 4:
            continue
        entities.add(rec[0])
        if not all_entities and rec[0].lower() != wanted and rec[1].lower() != wanted:
            continue
        try:
            year = int(rec[2])
        except ValueError:
            continue
        if (start_year and year < start_year) or (end_year and year > end_year):
            continue
        if rec[3] == "":
            continue  # missing observation - a gap stays a gap
        try:
            value = float(rec[3])
        except ValueError:
            continue
        rows.append({"country": rec[0], "year": year, "value": value})

    if not rows and not all_entities:
        close = sorted(e for e in entities if wanted in e.lower())[:10]
        return {"error": f"No {title} data for '{country}' in the requested range",
                "suggestions": close or sorted(entities)[:20]}

    return {
        "success": True,
        "title": title,
        "country": "All" if all_entities else rows[0]["country"],
        "chart": slug,
        "count": len(rows),
        "fields": ["country", "year", "value"],
        "data": rows,
        "source": "Our World in Data",
        "url": url,
    }


def get_co2_data(country: str = "World", start: str = None, end: str = None) -> Any:
    """Annual CO2 emissions for a country (name as in OWID: 'United States', 'Germany', 'World')."""
    slug, title = GRAPHER_CHARTS["co2"]
    return get_grapher_data(slug, title, country, start, end)


def get_energy_data(country: str = "World", start: str = None, end: str = None) -> Any:
    """Primary energy consumption for a country (name as in OWID)."""
    slug, title = GRAPHER_CHARTS["energy"]
    return get_grapher_data(slug, title, country, start, end)


def get_health_data(country: str = None, start: str = None, end: str = None) -> Any:
    """Health indicator: life expectancy at birth."""
    slug, title = GRAPHER_CHARTS["life_expectancy"]
    return get_grapher_data(slug, title, country, start, end)


def get_poverty_data(country: str = None, start: str = None, end: str = None) -> Any:
    """Share of the population living in extreme poverty."""
    slug, title = GRAPHER_CHARTS["poverty"]
    return get_grapher_data(slug, title, country, start, end)


def get_democracy_data(country: str = None, start: str = None, end: str = None) -> Any:
    """Electoral democracy index (V-Dem)."""
    slug, title = GRAPHER_CHARTS["democracy"]
    return get_grapher_data(slug, title, country, start, end)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: co2, co2_per_capita, energy, life_expectancy, poverty, gdp_per_capita, health, democracy, indicator, search"}))
        return

    command = args[0]

    # <command> [country] [start_year] [end_year]
    chart_commands = set(GRAPHER_CHARTS) | {"health"}
    if command in chart_commands:
        country = args[1] if len(args) > 1 else ("World" if command in ("co2", "energy", "co2_per_capita") else None)
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        slug, title = GRAPHER_CHARTS["life_expectancy" if command == "health" else command]
        result = get_grapher_data(slug, title, country, start, end)
    elif command == "indicator":
        if len(args) < 2:
            result = {"error": "Usage: indicator <indicator_id>"}
        else:
            try:
                result = get_indicator(int(args[1]))
            except ValueError:
                result = {"error": f"Indicator ID must be numeric, got: {args[1]}"}
    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: search <query>"}
        else:
            result = search_indicators(args[1])
    else:
        result = {"error": f"Unknown command: {command}. Available: co2, co2_per_capita, energy, life_expectancy, poverty, gdp_per_capita, health, democracy, indicator, search"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
