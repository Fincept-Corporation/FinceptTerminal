"""
Statista Free / IndexMundi Data Fetcher
Open/free statistics portals: IndexMundi and other free stat aggregators
for cross-country data (no key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.indexmundi.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({
    "User-Agent": "FinceptTerminal/4.0 (support@fincept.in)",
    "Accept": "application/json"
})

WB_BASE = "https://api.worldbank.org/v2"

INDICATORS = {
    # Economy
    "gdp": "NY.GDP.MKTP.CD",
    "gdp_per_capita": "NY.GDP.PCAP.CD",
    "gdp_growth": "NY.GDP.MKTP.KD.ZG",
    "inflation": "FP.CPI.TOTL.ZG",
    "unemployment": "SL.UEM.TOTL.ZS",
    "current_account": "BN.CAB.XOKA.CD",
    "exports": "NE.EXP.GNFS.CD",
    "imports": "NE.IMP.GNFS.CD",
    "fdi_inflows": "BX.KLT.DINV.CD.WD",
    "external_debt": "DT.DOD.DECT.CD",
    "govt_debt": "GC.DOD.TOTL.GD.ZS",
    "budget_surplus": "GC.BAL.CASH.GD.ZS",
    # Demographics
    "population": "SP.POP.TOTL",
    "population_growth": "SP.POP.GROW",
    "birth_rate": "SP.DYN.CBRT.IN",
    "death_rate": "SP.DYN.CDRT.IN",
    "fertility_rate": "SP.DYN.TFRT.IN",
    "life_expectancy": "SP.DYN.LE00.IN",
    "infant_mortality": "SP.DYN.IMRT.IN",
    "urban_population": "SP.URB.TOTL.IN.ZS",
    # Energy & Environment
    "co2_per_capita": "EN.ATM.CO2E.PC",
    "electricity_access": "EG.ELC.ACCS.ZS",
    "renewable_energy": "EG.FEC.RNEW.ZS",
    # Social
    "literacy_rate": "SE.ADT.LITR.ZS",
    "internet_users": "IT.NET.USER.ZS",
    "gini_index": "SI.POV.GINI",
    "poverty_rate": "SI.POV.DDAY",
}

COMMODITIES = {
    "gold": "PGOLDUSDM",
    "silver": "PSILVERUSDM",
    "crude_oil": "POILBREUSDM",
    "natural_gas": "PNGASUSDM",
    "wheat": "PWHEAMTUSDM",
    "corn": "PMAIZMTUSDM",
    "coffee": "PCOFFAUSDM",
    "cotton": "PCOTTINDUSDM",
    "aluminum": "PALUMUSDM",
    "copper": "PCOPPUSDM",
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


def _wb_request(country: str, indicator: str, start_year: int, end_year: int) -> Any:
    url = f"{WB_BASE}/country/{country}/indicator/{indicator}"
    params = {"format": "json", "date": f"{start_year}:{end_year}", "per_page": 100}
    try:
        resp = session.get(url, params=params, timeout=30)
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, list) and len(data) > 1:
            return [{"year": r.get("date"), "value": r.get("value")}
                    for r in (data[1] or []) if r.get("value") is not None]
        return data
    except Exception as e:
        return {"error": str(e)}


def get_country_stat(country: str = "US", indicator: str = "gdp", year: int = 2022) -> Any:
    ind_code = INDICATORS.get(indicator.lower(), indicator)
    data = _wb_request(country, ind_code, year, year)
    return {"country": country, "indicator": indicator, "indicator_code": ind_code,
            "year": year, "data": data}


def get_ranking(indicator: str = "gdp", year: int = 2022, limit: int = 50) -> Any:
    ind_code = INDICATORS.get(indicator.lower(), indicator)
    url = f"{WB_BASE}/country/all/indicator/{ind_code}"
    params = {"format": "json", "date": str(year), "per_page": limit,
              "mrv": 1, "sort": "value", "order": "desc"}
    try:
        resp = session.get(url, params=params, timeout=30)
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, list) and len(data) > 1:
            records = [{"country": r.get("country", {}).get("value"),
                        "country_code": r.get("country", {}).get("id"),
                        "year": r.get("date"),
                        "value": r.get("value")}
                       for r in (data[1] or []) if r.get("value") is not None]
            records.sort(key=lambda x: float(x["value"] or 0), reverse=True)
            return {"indicator": indicator, "indicator_code": ind_code,
                    "year": year, "count": len(records), "ranking": records[:limit]}
        return {"error": "No data returned"}
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}


def get_available_indicators() -> Any:
    return {
        "count": len(INDICATORS),
        "indicators": {k: {"world_bank_code": v} for k, v in INDICATORS.items()},
        "categories": {
            "economy": ["gdp", "gdp_per_capita", "gdp_growth", "inflation", "unemployment",
                        "current_account", "exports", "imports", "fdi_inflows",
                        "external_debt", "govt_debt", "budget_surplus"],
            "demographics": ["population", "population_growth", "birth_rate", "death_rate",
                              "fertility_rate", "life_expectancy", "infant_mortality",
                              "urban_population"],
            "energy_environment": ["co2_per_capita", "electricity_access", "renewable_energy"],
            "social": ["literacy_rate", "internet_users", "gini_index", "poverty_rate"]
        }
    }


def get_commodity_price(commodity: str = "gold", start_year: int = 2000,
                          end_year: int = 2024) -> Any:
    indicator = COMMODITIES.get(commodity.lower())
    if not indicator:
        return {"error": f"Unknown commodity '{commodity}'. Options: {list(COMMODITIES.keys())}"}
    url = f"{WB_BASE}/en/indicator/{indicator}"
    params = {"format": "json", "date": f"{start_year}:{end_year}", "per_page": 500}
    try:
        resp = session.get(url, params=params, timeout=30)
        resp.raise_for_status()
        data = resp.json()
        if isinstance(data, list) and len(data) > 1:
            records = [{"date": r.get("date"), "value": r.get("value")}
                       for r in (data[1] or []) if r.get("value") is not None]
            return {"commodity": commodity, "indicator": indicator,
                    "start_year": start_year, "end_year": end_year,
                    "count": len(records), "data": records}
        return data
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}


def get_demographic_data(country: str = "US", year: int = 2022) -> Any:
    demo_indicators = ["population", "population_growth", "birth_rate", "death_rate",
                        "fertility_rate", "life_expectancy", "infant_mortality",
                        "urban_population"]
    results = {}
    for ind in demo_indicators:
        data = _wb_request(country, INDICATORS[ind], year, year)
        results[ind] = data[0].get("value") if isinstance(data, list) and data else None
    return {"country": country, "year": year, "demographics": results}


def get_economic_data(country: str = "US", year: int = 2022) -> Any:
    econ_indicators = ["gdp", "gdp_per_capita", "gdp_growth", "inflation",
                        "unemployment", "exports", "imports"]
    results = {}
    for ind in econ_indicators:
        data = _wb_request(country, INDICATORS[ind], year, year)
        results[ind] = data[0].get("value") if isinstance(data, list) and data else None
    return {"country": country, "year": year, "economics": results}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "stat":
        country = args[1] if len(args) > 1 else "US"
        indicator = args[2] if len(args) > 2 else "gdp"
        year = int(args[3]) if len(args) > 3 else 2022
        result = get_country_stat(country, indicator, year)
    elif command == "ranking":
        indicator = args[1] if len(args) > 1 else "gdp"
        year = int(args[2]) if len(args) > 2 else 2022
        limit = int(args[3]) if len(args) > 3 else 50
        result = get_ranking(indicator, year, limit)
    elif command == "indicators":
        result = get_available_indicators()
    elif command == "commodity":
        commodity = args[1] if len(args) > 1 else "gold"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2024
        result = get_commodity_price(commodity, start_year, end_year)
    elif command == "demographic":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_demographic_data(country, year)
    elif command == "economic":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2022
        result = get_economic_data(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
