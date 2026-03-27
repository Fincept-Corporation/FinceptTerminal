"""
Macro Trends Data Fetcher
Long-term historical macro data — P/E ratios, Shiller CAPE, Buffett indicator via public endpoints.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('MACROTRENDS_API_KEY', '')
BASE_URL = "https://www.macrotrends.net/assets/php"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def _fetch_multpl_series(series_id: str, start_year: int = None, end_year: int = None) -> Any:
    url = f"https://www.multpl.com/{series_id}/table/by-year"
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)", "Accept": "application/json"}
        response = session.get(url, headers=headers, timeout=30)
        response.raise_for_status()
        data = response.json()
        if start_year or end_year:
            filtered = []
            for entry in data if isinstance(data, list) else data.get("data", []):
                year = int(str(entry.get("date", "0"))[:4]) if isinstance(entry, dict) else 0
                if start_year and year < start_year:
                    continue
                if end_year and year > end_year:
                    continue
                filtered.append(entry)
            return filtered
        return data
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_shiller_pe(start_year: int = None, end_year: int = None) -> Any:
    return _fetch_multpl_series("shiller-pe", start_year, end_year)

def get_buffett_indicator(start_year: int = None, end_year: int = None) -> Any:
    url = "https://fred.stlouisfed.org/graph/fredgraph.csv?id=WILL5000INDFC"
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)"}
        response = session.get(url, headers=headers, timeout=30)
        response.raise_for_status()
        lines = response.text.strip().split("\n")
        rows = []
        for line in lines[1:]:
            parts = line.split(",")
            if len(parts) == 2:
                year = int(parts[0][:4]) if parts[0] else 0
                if start_year and year < start_year:
                    continue
                if end_year and year > end_year:
                    continue
                rows.append({"date": parts[0], "value": parts[1]})
        return rows
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}

def get_sp500_historical(start_year: int = None, end_year: int = None) -> Any:
    return _fetch_multpl_series("sp500-historical-annual-returns", start_year, end_year)

def get_interest_rates_history(start_year: int = None, end_year: int = None) -> Any:
    url = "https://fred.stlouisfed.org/graph/fredgraph.json?id=FEDFUNDS"
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)"}
        response = session.get(url, headers=headers, timeout=30)
        response.raise_for_status()
        data = response.json()
        observations = data.get("observations", [])
        if start_year or end_year:
            filtered = []
            for obs in observations:
                year = int(obs.get("date", "0000")[:4])
                if start_year and year < start_year:
                    continue
                if end_year and year > end_year:
                    continue
                filtered.append(obs)
            return filtered
        return observations
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}

def get_gdp_historical(start_year: int = None, end_year: int = None) -> Any:
    url = "https://fred.stlouisfed.org/graph/fredgraph.json?id=GDP"
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)"}
        response = session.get(url, headers=headers, timeout=30)
        response.raise_for_status()
        data = response.json()
        observations = data.get("observations", [])
        if start_year or end_year:
            filtered = []
            for obs in observations:
                year = int(obs.get("date", "0000")[:4])
                if start_year and year < start_year:
                    continue
                if end_year and year > end_year:
                    continue
                filtered.append(obs)
            return filtered
        return observations
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}

def get_inflation_historical(start_year: int = None, end_year: int = None) -> Any:
    url = "https://fred.stlouisfed.org/graph/fredgraph.json?id=CPIAUCSL"
    try:
        headers = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.0)"}
        response = session.get(url, headers=headers, timeout=30)
        response.raise_for_status()
        data = response.json()
        observations = data.get("observations", [])
        if start_year or end_year:
            filtered = []
            for obs in observations:
                year = int(obs.get("date", "0000")[:4])
                if start_year and year < start_year:
                    continue
                if end_year and year > end_year:
                    continue
                filtered.append(obs)
            return filtered
        return observations
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
    start_year = int(args[1]) if len(args) > 1 else None
    end_year = int(args[2]) if len(args) > 2 else None
    if command == "shiller_pe":
        result = get_shiller_pe(start_year, end_year)
    elif command == "buffett":
        result = get_buffett_indicator(start_year, end_year)
    elif command == "sp500":
        result = get_sp500_historical(start_year, end_year)
    elif command == "rates":
        result = get_interest_rates_history(start_year, end_year)
    elif command == "gdp":
        result = get_gdp_historical(start_year, end_year)
    elif command == "inflation":
        result = get_inflation_historical(start_year, end_year)
    print(json.dumps(result))

if __name__ == "__main__":
    main()
