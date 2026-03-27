"""
Our World in Data CO2 & Energy Data Fetcher
CO2 emissions, energy mix, per-capita emissions for all countries
via OWID GitHub raw CSV (no key required).
"""
import sys
import json
import os
import io
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://raw.githubusercontent.com/owid/co2-data/master"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

_CACHE = {}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=60)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _load_owid_csv() -> Any:
    global _CACHE
    if "owid_co2" in _CACHE:
        return _CACHE["owid_co2"]
    url = f"{BASE_URL}/owid-co2-data.csv"
    try:
        resp = session.get(url, timeout=60)
        resp.raise_for_status()
        lines = resp.text.strip().split('\n')
        headers = [h.strip() for h in lines[0].split(',')]
        records = []
        for line in lines[1:]:
            parts = line.split(',')
            if len(parts) >= len(headers):
                row = {headers[i]: parts[i].strip() for i in range(len(headers))}
                records.append(row)
        _CACHE["owid_co2"] = records
        return records
    except Exception as e:
        return {"error": f"Failed to load OWID CSV: {str(e)}"}


def get_country_co2(country: str = "United States", start_year: int = 2000,
                     end_year: int = 2022) -> Any:
    records = _load_owid_csv()
    if isinstance(records, dict) and "error" in records:
        return records
    filtered = [r for r in records
                if r.get("country", "").lower() == country.lower()
                and start_year <= int(r.get("year", 0) or 0) <= end_year]
    return {"country": country, "start_year": start_year, "end_year": end_year,
            "count": len(filtered), "data": filtered}


def get_global_co2(start_year: int = 1990, end_year: int = 2022) -> Any:
    records = _load_owid_csv()
    if isinstance(records, dict) and "error" in records:
        return records
    filtered = [r for r in records
                if r.get("country", "").lower() == "world"
                and start_year <= int(r.get("year", 0) or 0) <= end_year]
    return {"region": "World", "start_year": start_year, "end_year": end_year,
            "count": len(filtered), "data": filtered}


def get_energy_mix(country: str = "United States", year: int = 2021) -> Any:
    records = _load_owid_csv()
    if isinstance(records, dict) and "error" in records:
        return records
    row = next((r for r in records
                if r.get("country", "").lower() == country.lower()
                and int(r.get("year", 0) or 0) == year), None)
    if not row:
        return {"error": f"No data found for {country} in {year}"}
    energy_fields = [k for k in row.keys() if any(x in k.lower() for x in
                     ["coal", "oil", "gas", "nuclear", "renewab", "hydro", "solar", "wind",
                      "energy", "electricity"])]
    energy_data = {k: row[k] for k in energy_fields}
    return {"country": country, "year": year, "energy_mix": energy_data}


def get_per_capita(country: str = "United States", start_year: int = 2000,
                    end_year: int = 2022) -> Any:
    records = _load_owid_csv()
    if isinstance(records, dict) and "error" in records:
        return records
    per_capita_fields = [k for k in (records[0].keys() if records else [])
                         if "per_capita" in k.lower() or "percapita" in k.lower()]
    filtered = []
    for r in records:
        if r.get("country", "").lower() == country.lower():
            yr = int(r.get("year", 0) or 0)
            if start_year <= yr <= end_year:
                row_data = {"year": r.get("year"), "country": r.get("country")}
                for f in per_capita_fields:
                    row_data[f] = r.get(f)
                filtered.append(row_data)
    return {"country": country, "start_year": start_year, "end_year": end_year,
            "fields": per_capita_fields, "count": len(filtered), "data": filtered}


def get_cumulative_co2(country: str = "United States") -> Any:
    records = _load_owid_csv()
    if isinstance(records, dict) and "error" in records:
        return records
    filtered = [r for r in records if r.get("country", "").lower() == country.lower()]
    cumulative_fields = [k for k in (filtered[0].keys() if filtered else [])
                          if "cumulative" in k.lower()]
    if not filtered:
        return {"error": f"No data found for {country}"}
    latest = sorted(filtered, key=lambda x: int(x.get("year", 0) or 0), reverse=True)
    result = {"country": country, "latest_year": latest[0].get("year") if latest else None,
              "cumulative_fields": cumulative_fields,
              "cumulative_data": {f: latest[0].get(f) for f in cumulative_fields} if latest else {}}
    return result


def get_available_countries() -> Any:
    records = _load_owid_csv()
    if isinstance(records, dict) and "error" in records:
        return records
    countries = sorted(set(r.get("country", "") for r in records if r.get("country")))
    return {"count": len(countries), "countries": countries}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "country_co2":
        country = args[1] if len(args) > 1 else "United States"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2022
        result = get_country_co2(country, start_year, end_year)
    elif command == "global_co2":
        start_year = int(args[1]) if len(args) > 1 else 1990
        end_year = int(args[2]) if len(args) > 2 else 2022
        result = get_global_co2(start_year, end_year)
    elif command == "energy_mix":
        country = args[1] if len(args) > 1 else "United States"
        year = int(args[2]) if len(args) > 2 else 2021
        result = get_energy_mix(country, year)
    elif command == "per_capita":
        country = args[1] if len(args) > 1 else "United States"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2022
        result = get_per_capita(country, start_year, end_year)
    elif command == "cumulative":
        country = args[1] if len(args) > 1 else "United States"
        result = get_cumulative_co2(country)
    elif command == "countries":
        result = get_available_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
