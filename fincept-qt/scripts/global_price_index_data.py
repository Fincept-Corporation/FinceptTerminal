"""
Global Price Index Data Fetcher
Various global price indices: Big Mac Index, Economist commodity indices,
CPI comparisons, PPP rates — fetched from public sources.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://raw.githubusercontent.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json, text/csv, */*"})

BIG_MAC_CSV_URL = "https://raw.githubusercontent.com/TheEconomist/big-mac-data/master/output-data/big-mac-full-index.csv"
IMF_WEO_BASE = "https://www.imf.org/external/datamapper/api/v1"
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


def _fetch_csv_as_records(url: str, limit: int = 500) -> Any:
    try:
        resp = session.get(url, timeout=30)
        resp.raise_for_status()
        lines = resp.text.strip().split('\n')
        if not lines:
            return {"error": "Empty CSV"}
        headers = [h.strip().strip('"') for h in lines[0].split(',')]
        records = []
        for line in lines[1:limit+1]:
            parts = line.split(',')
            if len(parts) >= 2:
                row = {}
                for i in range(min(len(headers), len(parts))):
                    row[headers[i]] = parts[i].strip().strip('"')
                records.append(row)
        return records
    except Exception as e:
        return {"error": f"CSV fetch failed: {str(e)}"}


def get_big_mac_index(year: int = None) -> Any:
    records = _fetch_csv_as_records(BIG_MAC_CSV_URL)
    if isinstance(records, dict) and "error" in records:
        return records
    if year:
        records = [r for r in records if str(year) in r.get("date", "")]
    return {"index": "Big Mac Index", "source": "The Economist",
            "year_filter": year, "count": len(records), "data": records}


def get_economist_commodity_index(type_: str = "energy",
                                    start_date: str = None, end_date: str = None) -> Any:
    # Use World Bank commodity indices as proxy for Economist indices
    indicator_map = {
        "energy": "PENERGYINDEXM",
        "non_energy": "PNRGINDEXM",
        "agriculture": "PAGRIINDEXM",
        "metals": "PMETALSINDEXM",
        "food": "PFOODINDEXM"
    }
    indicator = indicator_map.get(type_.lower(), "PENERGYINDEXM")
    url = f"{WB_API_BASE}/en/indicator/{indicator}"
    params = {"format": "json", "per_page": 500}
    if start_date:
        params["date"] = f"{start_date}:{end_date or '2024'}"
    try:
        resp = session.get(url, params=params, timeout=30)
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, list) and len(data) > 1:
            records = [{"date": r.get("date"), "value": r.get("value")}
                       for r in (data[1] or []) if r.get("value") is not None]
            return {"index_type": type_, "indicator": indicator,
                    "source": "World Bank / Pink Sheet", "data": records}
        return data
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}


def get_cpi_comparison(countries: str = "US,GB,DE,JP", year: int = 2023) -> Any:
    country_list = [c.strip().upper() for c in countries.split(',')]
    results = {}
    for country in country_list:
        url = f"{WB_API_BASE}/country/{country}/indicator/FP.CPI.TOTL"
        params = {"format": "json", "date": str(year), "per_page": 5}
        try:
            resp = session.get(url, params=params, timeout=30)
            resp.raise_for_status()
            data = resp.json()
            if isinstance(data, list) and len(data) > 1 and data[1]:
                val = data[1][0]
                results[country] = {
                    "country": val.get("country", {}).get("value"),
                    "year": val.get("date"),
                    "cpi": val.get("value")
                }
        except Exception as e:
            results[country] = {"error": str(e)}
    return {"indicator": "CPI (2010=100)", "year": year, "countries": results}


def get_ppp_rates(country: str = "US", year: int = 2023) -> Any:
    url = f"{WB_API_BASE}/country/{country}/indicator/PA.NUS.PPP"
    params = {"format": "json", "date": f"2015:{year}", "per_page": 20}
    try:
        resp = session.get(url, params=params, timeout=30)
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, list) and len(data) > 1:
            records = [{"year": r.get("date"), "ppp_rate": r.get("value")}
                       for r in (data[1] or []) if r.get("value") is not None]
            return {"country": country, "indicator": "PPP conversion factor (LCU per intl $)",
                    "data": records}
        return data
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}


def get_price_level_index(country: str = "US", year: int = 2023) -> Any:
    url = f"{WB_API_BASE}/country/{country}/indicator/PA.NUS.PRVT.PP"
    params = {"format": "json", "date": f"2015:{year}", "per_page": 20}
    try:
        resp = session.get(url, params=params, timeout=30)
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, list) and len(data) > 1:
            records = [{"year": r.get("date"), "price_level": r.get("value")}
                       for r in (data[1] or []) if r.get("value") is not None]
            country_name = data[1][0].get("country", {}).get("value") if data[1] else country
            return {"country": country, "country_name": country_name,
                    "indicator": "Price level ratio of PPP conversion factor to market exchange rate",
                    "data": records}
        return data
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "big_mac":
        year = int(args[1]) if len(args) > 1 else None
        result = get_big_mac_index(year)
    elif command == "commodity_index":
        type_ = args[1] if len(args) > 1 else "energy"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_economist_commodity_index(type_, start_date, end_date)
    elif command == "cpi_comparison":
        countries = args[1] if len(args) > 1 else "US,GB,DE,JP"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_cpi_comparison(countries, year)
    elif command == "ppp":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_ppp_rates(country, year)
    elif command == "price_level":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_price_level_index(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
