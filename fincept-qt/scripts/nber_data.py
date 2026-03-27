"""
NBER Data Fetcher
Fetches NBER (National Bureau of Economic Research) business cycle dates,
macroeconomic datasets, and public-use datasets.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('NBER_API_KEY', '')
BASE_URL = "https://data.nber.org/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

NBER_BUSINESS_CYCLES = [
    {"peak": "1857-06", "trough": "1858-12", "expansion_months": None, "contraction_months": 18, "recession": "1857-1858"},
    {"peak": "1860-10", "trough": "1861-06", "expansion_months": 22, "contraction_months": 8, "recession": "1860-1861"},
    {"peak": "1865-04", "trough": "1867-12", "expansion_months": 46, "contraction_months": 32, "recession": "1865-1867"},
    {"peak": "1869-06", "trough": "1870-12", "expansion_months": 18, "contraction_months": 18, "recession": "1869-1870"},
    {"peak": "1873-10", "trough": "1879-03", "expansion_months": 34, "contraction_months": 65, "recession": "1873-1879"},
    {"peak": "1882-03", "trough": "1885-05", "expansion_months": 36, "contraction_months": 38, "recession": "1882-1885"},
    {"peak": "1887-03", "trough": "1888-04", "expansion_months": 22, "contraction_months": 13, "recession": "1887-1888"},
    {"peak": "1890-07", "trough": "1891-05", "expansion_months": 27, "contraction_months": 10, "recession": "1890-1891"},
    {"peak": "1893-01", "trough": "1894-06", "expansion_months": 20, "contraction_months": 17, "recession": "1893-1894"},
    {"peak": "1895-12", "trough": "1897-06", "expansion_months": 18, "contraction_months": 18, "recession": "1895-1897"},
    {"peak": "1899-06", "trough": "1900-12", "expansion_months": 24, "contraction_months": 18, "recession": "1899-1900"},
    {"peak": "1902-09", "trough": "1904-08", "expansion_months": 21, "contraction_months": 23, "recession": "1902-1904"},
    {"peak": "1907-05", "trough": "1908-06", "expansion_months": 33, "contraction_months": 13, "recession": "1907-1908"},
    {"peak": "1910-01", "trough": "1912-01", "expansion_months": 19, "contraction_months": 24, "recession": "1910-1912"},
    {"peak": "1913-01", "trough": "1914-12", "expansion_months": 12, "contraction_months": 23, "recession": "1913-1914"},
    {"peak": "1918-08", "trough": "1919-03", "expansion_months": 44, "contraction_months": 7, "recession": "1918-1919"},
    {"peak": "1920-01", "trough": "1921-07", "expansion_months": 10, "contraction_months": 18, "recession": "1920-1921"},
    {"peak": "1923-05", "trough": "1924-07", "expansion_months": 22, "contraction_months": 14, "recession": "1923-1924"},
    {"peak": "1926-10", "trough": "1927-11", "expansion_months": 27, "contraction_months": 13, "recession": "1926-1927"},
    {"peak": "1929-08", "trough": "1933-03", "expansion_months": 21, "contraction_months": 43, "recession": "1929-1933 (Great Depression)"},
    {"peak": "1937-05", "trough": "1938-06", "expansion_months": 50, "contraction_months": 13, "recession": "1937-1938"},
    {"peak": "1945-02", "trough": "1945-10", "expansion_months": 80, "contraction_months": 8, "recession": "1945"},
    {"peak": "1948-11", "trough": "1949-10", "expansion_months": 37, "contraction_months": 11, "recession": "1948-1949"},
    {"peak": "1953-07", "trough": "1954-05", "expansion_months": 45, "contraction_months": 10, "recession": "1953-1954"},
    {"peak": "1957-08", "trough": "1958-04", "expansion_months": 39, "contraction_months": 8, "recession": "1957-1958"},
    {"peak": "1960-04", "trough": "1961-02", "expansion_months": 24, "contraction_months": 10, "recession": "1960-1961"},
    {"peak": "1969-12", "trough": "1970-11", "expansion_months": 106, "contraction_months": 11, "recession": "1969-1970"},
    {"peak": "1973-11", "trough": "1975-03", "expansion_months": 36, "contraction_months": 16, "recession": "1973-1975"},
    {"peak": "1980-01", "trough": "1980-07", "expansion_months": 58, "contraction_months": 6, "recession": "1980"},
    {"peak": "1981-07", "trough": "1982-11", "expansion_months": 12, "contraction_months": 16, "recession": "1981-1982"},
    {"peak": "1990-07", "trough": "1991-03", "expansion_months": 92, "contraction_months": 8, "recession": "1990-1991"},
    {"peak": "2001-03", "trough": "2001-11", "expansion_months": 120, "contraction_months": 8, "recession": "2001"},
    {"peak": "2007-12", "trough": "2009-06", "expansion_months": 73, "contraction_months": 18, "recession": "2007-2009 (Great Recession)"},
    {"peak": "2020-02", "trough": "2020-04", "expansion_months": 128, "contraction_months": 2, "recession": "2020 (COVID-19)"},
]

NBER_DATASETS = {
    "macrohistory": {
        "description": "NBER Macrohistory Database — historical time series for pre-WWII US economy",
        "url": "https://www.nber.org/research/data/nber-macrohistory-database",
        "time_coverage": "1867-1961",
        "categories": ["output", "employment", "prices", "money", "finance", "trade"],
    },
    "cps": {
        "description": "Current Population Survey (CPS) — monthly household survey of labor force",
        "url": "https://www.nber.org/research/data/current-population-survey-cps-merged-outgoing-rotation-groups",
        "frequency": "Monthly",
        "variables": ["earnings", "employment_status", "industry", "occupation", "education", "demographics"],
    },
    "patent_data": {
        "description": "NBER Patent Citations Database — 3M+ US patents 1963-1999",
        "url": "https://www.nber.org/research/data/us-patents-1963-1999",
    },
    "trade_liberalization": {
        "description": "Trade Liberalization Episodes — tariff reductions across countries",
        "url": "https://www.nber.org/research/data/trade-liberalization",
    },
    "international_finance": {
        "description": "International Finance and Macroeconomics datasets",
        "url": "https://www.nber.org/programs-projects/programs-working-groups/international-finance-and-macroeconomics",
    },
    "health_economics": {
        "description": "Health economics datasets — mortality, healthcare utilization",
        "url": "https://www.nber.org/programs-projects/programs-working-groups/health-economics",
    },
    "women_economics": {
        "description": "Gender in the economy — wage gaps, labor participation",
        "url": "https://www.nber.org/programs-projects/programs-working-groups/women-economics",
    },
    "productivity_innovation": {
        "description": "Productivity, Innovation and Entrepreneurship datasets",
        "url": "https://www.nber.org/programs-projects/programs-working-groups/productivity-innovation-and-entrepreneurship",
    },
}

NBER_MACROHISTORY_SERIES = {
    "m04001": "Corporate bond yields",
    "m04002": "High-grade railroad bond yields",
    "m04003": "State and local bonds",
    "m06006": "Wholesale prices",
    "m08023": "Index of factory employment",
    "m01011a": "Commercial paper rates, 60-90 days",
    "m01011b": "Call money rates",
    "m12017": "Industrial production",
    "m15014": "Exports, total",
    "m15015": "Imports, total",
}


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


def get_business_cycle_dates() -> Dict:
    avg_expansion = sum(c["expansion_months"] for c in NBER_BUSINESS_CYCLES if c["expansion_months"]) / len([c for c in NBER_BUSINESS_CYCLES if c["expansion_months"]])
    avg_contraction = sum(c["contraction_months"] for c in NBER_BUSINESS_CYCLES if c["contraction_months"]) / len([c for c in NBER_BUSINESS_CYCLES if c["contraction_months"]])
    post_wwii = [c for c in NBER_BUSINESS_CYCLES if c["peak"] >= "1945"]
    avg_expansion_post_wwii = sum(c["expansion_months"] for c in post_wwii if c["expansion_months"]) / len([c for c in post_wwii if c["expansion_months"]])
    avg_contraction_post_wwii = sum(c["contraction_months"] for c in post_wwii if c["contraction_months"]) / len([c for c in post_wwii if c["contraction_months"]])
    return {
        "source": "NBER Business Cycle Dating Committee",
        "url": "https://www.nber.org/research/business-cycle-dating",
        "total_cycles": len(NBER_BUSINESS_CYCLES),
        "latest_trough": "2020-04",
        "current_status": "Expansion (as of 2024)",
        "statistics": {
            "all_cycles": {
                "avg_expansion_months": round(avg_expansion, 1),
                "avg_contraction_months": round(avg_contraction, 1),
            },
            "post_wwii": {
                "cycle_count": len(post_wwii),
                "avg_expansion_months": round(avg_expansion_post_wwii, 1),
                "avg_contraction_months": round(avg_contraction_post_wwii, 1),
            },
        },
        "cycles": NBER_BUSINESS_CYCLES,
    }


def get_recession_indicator(start_date: str = "2000-01", end_date: str = "2024-12") -> Dict:
    def in_recession(date_str: str) -> bool:
        for cycle in NBER_BUSINESS_CYCLES:
            peak = cycle["peak"]
            trough = cycle["trough"]
            if peak <= date_str <= trough:
                return True
        return False

    dates = []
    try:
        from datetime import datetime, timedelta
        start = datetime.strptime(start_date + "-01", "%Y-%m-%d")
        end = datetime.strptime(end_date + "-01", "%Y-%m-%d")
        current = start
        while current <= end:
            date_key = current.strftime("%Y-%m")
            dates.append({"date": date_key, "recession": 1 if in_recession(date_key) else 0})
            if current.month == 12:
                current = current.replace(year=current.year + 1, month=1)
            else:
                current = current.replace(month=current.month + 1)
    except Exception as e:
        return {"error": f"Date processing error: {str(e)}"}
    return {
        "start_date": start_date,
        "end_date": end_date,
        "indicator": "1 = recession, 0 = expansion",
        "source": "NBER Business Cycle Dating Committee",
        "data": dates,
    }


def get_macrohistory_data(series_id: str, start_year: int = 1870, end_year: int = 1961) -> Dict:
    csv_url = f"https://data.nber.org/macrohistory/data/{series_id}.csv"
    try:
        response = session.get(csv_url, timeout=30)
        if response.status_code == 200:
            lines = response.text.strip().split("\n")
            headers = [h.strip().strip('"') for h in lines[0].split(",")] if lines else []
            observations = []
            for line in lines[1:]:
                parts = line.split(",")
                if len(parts) >= 2:
                    try:
                        year_val = int(parts[0].strip().strip('"')[:4])
                        if start_year <= year_val <= end_year:
                            value = parts[1].strip().strip('"')
                            observations.append({"date": parts[0].strip().strip('"'), "value": value if value != "." else None})
                    except (ValueError, IndexError):
                        continue
            return {
                "series_id": series_id,
                "start_year": start_year,
                "end_year": end_year,
                "source": "NBER Macrohistory Database",
                "data": observations,
            }
        return {
            "series_id": series_id,
            "error": f"Series not found. HTTP {response.status_code}",
            "available_series": NBER_MACROHISTORY_SERIES,
            "note": "NBER Macrohistory covers 1867-1961 US economic history",
        }
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}", "series_id": series_id}


def get_available_datasets() -> Dict:
    return {
        "total_datasets": len(NBER_DATASETS),
        "datasets": NBER_DATASETS,
        "nber_url": "https://www.nber.org",
        "data_url": "https://data.nber.org",
        "working_papers_url": "https://www.nber.org/papers",
        "programs": [
            "Asset Pricing", "Corporate Finance", "Economic Fluctuations and Growth",
            "Health Care", "Industrial Organization", "International Finance and Macroeconomics",
            "International Trade and Investment", "Labor Studies", "Law and Economics",
            "Monetary Economics", "Political Economy", "Productivity, Innovation and Entrepreneurship",
            "Public Economics",
        ],
        "macrohistory_series": NBER_MACROHISTORY_SERIES,
    }


def get_cps_data(year: int = 2023, variable: str = "earnings") -> Dict:
    cps_variables = {
        "earnings": "Weekly earnings of full-time workers",
        "employment_status": "Employment status (employed, unemployed, not in labor force)",
        "hours_worked": "Usual hours worked per week",
        "industry": "Industry of employment (NAICS)",
        "occupation": "Occupation (SOC codes)",
        "education": "Educational attainment",
        "age": "Age in years",
        "race": "Race and ethnicity",
        "sex": "Sex",
        "union_status": "Union membership and coverage",
        "class_of_worker": "Private sector, government, self-employed",
    }
    return {
        "dataset": "Current Population Survey (CPS)",
        "year": year,
        "variable": variable,
        "variable_description": cps_variables.get(variable, variable),
        "source": "Bureau of Labor Statistics / NBER",
        "cps_url": "https://www.bls.gov/cps/data.htm",
        "nber_cps_url": "https://www.nber.org/research/data/current-population-survey-cps-merged-outgoing-rotation-groups",
        "available_variables": cps_variables,
        "note": "CPS microdata requires downloading from NBER or BLS. Processed extracts available.",
        "fred_cps_series": {
            "unemployment_rate": "UNRATE",
            "labor_force_participation": "CIVPART",
            "employment_level": "CE16OV",
            "median_weekly_earnings": "LES1252881500Q",
            "avg_hourly_earnings": "CES0500000003",
        },
    }


def get_unemployment_data(start_date: str = "2000-01", end_date: str = "") -> Dict:
    fred_url = "https://api.stlouisfed.org/fred/series/observations"
    fred_key = os.environ.get("FRED_API_KEY", "")
    if fred_key:
        params = {
            "series_id": "UNRATE",
            "api_key": fred_key,
            "file_type": "json",
            "observation_start": start_date,
        }
        if end_date:
            params["observation_end"] = end_date
        try:
            response = session.get(fred_url, params=params, timeout=30)
            response.raise_for_status()
            data = response.json()
            obs = data.get("observations", [])
            return {
                "series": "Unemployment Rate (UNRATE)",
                "source": "BLS via FRED",
                "units": "Percent, Seasonally Adjusted",
                "start_date": start_date,
                "end_date": end_date or "latest",
                "data": [{"date": o["date"], "value": o["value"]} for o in obs],
            }
        except Exception:
            pass
    return {
        "series": "Unemployment Rate (UNRATE)",
        "source": "BLS / NBER",
        "start_date": start_date,
        "end_date": end_date or "latest",
        "bls_url": "https://data.bls.gov/timeseries/LNS14000000",
        "fred_series": "UNRATE",
        "note": "Set FRED_API_KEY for live data. Available via FRED API.",
        "nber_recession_note": "Unemployment typically peaks after NBER recession trough by 6-12 months.",
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "cycles":
        result = get_business_cycle_dates()
    elif command == "recession":
        start_date = args[1] if len(args) > 1 else "2000-01"
        end_date = args[2] if len(args) > 2 else "2024-12"
        result = get_recession_indicator(start_date, end_date)
    elif command == "macro":
        series_id = args[1] if len(args) > 1 else ""
        if not series_id:
            result = {"error": "series_id required", "available_series": NBER_MACROHISTORY_SERIES}
        else:
            start_year = int(args[2]) if len(args) > 2 else 1870
            end_year = int(args[3]) if len(args) > 3 else 1961
            result = get_macrohistory_data(series_id, start_year, end_year)
    elif command == "datasets":
        result = get_available_datasets()
    elif command == "cps":
        year = int(args[1]) if len(args) > 1 else 2023
        variable = args[2] if len(args) > 2 else "earnings"
        result = get_cps_data(year, variable)
    elif command == "unemployment":
        start_date = args[1] if len(args) > 1 else "2000-01"
        end_date = args[2] if len(args) > 2 else ""
        result = get_unemployment_data(start_date, end_date)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
