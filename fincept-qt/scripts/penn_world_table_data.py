"""
Penn World Tables Data Fetcher
Fetches national accounts, productivity, and living standards data from the Penn World
Tables (PWT 10.x) for 183 countries from 1950 to 2019.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('PWT_API_KEY', '')
BASE_URL = "https://dataverse.nl/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

PWT_DATASET_DOI = "doi:10.34894/QT5BCC"
PWT_DATAVERSE = "https://dataverse.nl"

PWT_INDICATORS = {
    "rgdpe": "Expenditure-side real GDP (PPP, millions 2017 USD)",
    "rgdpo": "Output-side real GDP (PPP, millions 2017 USD)",
    "pop": "Population (millions)",
    "emp": "Employment (millions)",
    "avh": "Average annual hours worked per worker",
    "hc": "Human capital index",
    "ccon": "Real consumption (millions 2017 USD)",
    "cda": "Real domestic absorption (millions 2017 USD)",
    "cgdpe": "Expenditure-side real GDP per capita (2017 USD)",
    "cgdpo": "Output-side real GDP per capita (2017 USD)",
    "cn": "Capital stock (millions 2017 USD)",
    "ck": "Capital services (millions 2017 USD)",
    "ctfp": "TFP relative to USA (output-side)",
    "cwtfp": "Welfare-relevant TFP",
    "rgdpna": "Real GDP at constant 2017 national prices (millions)",
    "rconna": "Real consumption at constant 2017 national prices",
    "rdana": "Real domestic absorption at constant 2017 national prices",
    "rnna": "Capital stock at constant 2017 national prices",
    "rkna": "Capital services at constant 2017 national prices",
    "rtfpna": "TFP at constant national prices (2017=1)",
    "labsh": "Share of labour compensation in GDP",
    "irr": "Real internal rate of return",
    "delta": "Average depreciation rate of capital stock",
    "xr": "Exchange rate (LCU to USD)",
    "pl_con": "Price level of consumption",
    "pl_da": "Price level of domestic absorption",
    "pl_gdpo": "Price level of output-side GDP",
    "i_cig": "Investment-consumption-government shares",
    "i_xm": "Export-import shares",
    "i_outlier": "Outlier flag",
    "i_irr": "IRR flag",
    "cor_exp": "Correlation with expenditure-side data",
    "statcap": "Statistical capacity indicator",
    "csh_c": "Share of household consumption in GDP",
    "csh_i": "Share of gross capital formation in GDP",
    "csh_g": "Share of government consumption in GDP",
    "csh_x": "Share of merchandise exports in GDP",
    "csh_m": "Share of merchandise imports in GDP",
    "csh_r": "Share of residual trade in GDP",
    "pl_c": "Price level of consumption (USA=1)",
    "pl_i": "Price level of capital formation",
    "pl_g": "Price level of government consumption",
    "pl_x": "Price level of exports",
    "pl_m": "Price level of imports",
    "pl_n": "Price level of capital stock",
    "pl_k": "Price level of capital services",
}

PWT_COUNTRIES = {
    "USA": "United States", "GBR": "United Kingdom", "DEU": "Germany",
    "FRA": "France", "JPN": "Japan", "CHN": "China", "IND": "India",
    "BRA": "Brazil", "RUS": "Russia", "CAN": "Canada", "AUS": "Australia",
    "KOR": "South Korea", "ESP": "Spain", "ITA": "Italy", "MEX": "Mexico",
    "IDN": "Indonesia", "NLD": "Netherlands", "SAU": "Saudi Arabia",
    "CHE": "Switzerland", "SWE": "Sweden", "NOR": "Norway", "DNK": "Denmark",
    "FIN": "Finland", "BEL": "Belgium", "AUT": "Austria", "POL": "Poland",
    "TUR": "Turkey", "ZAF": "South Africa", "ARG": "Argentina", "NGA": "Nigeria",
    "EGY": "Egypt", "SGP": "Singapore", "HKG": "Hong Kong", "TWN": "Taiwan",
    "MYS": "Malaysia", "THA": "Thailand", "PHL": "Philippines", "VNM": "Vietnam",
    "BGD": "Bangladesh", "PAK": "Pakistan",
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


def _fetch_pwt_csv() -> Dict:
    csv_url = f"{PWT_DATAVERSE}/api/access/datafile/:persistentId?persistentId={PWT_DATASET_DOI}"
    try:
        response = session.get(csv_url, timeout=60)
        if response.status_code == 200:
            return {"status": "available", "url": csv_url}
        return {"error": f"HTTP {response.status_code}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def get_pwt_data(country: str, indicator: str, start_year: int = 1960, end_year: int = 2019) -> Dict:
    return {
        "country": country,
        "indicator": indicator,
        "indicator_description": PWT_INDICATORS.get(indicator, "Unknown indicator"),
        "start_year": start_year,
        "end_year": end_year,
        "dataset": "Penn World Tables 10.01",
        "doi": PWT_DATASET_DOI,
        "access_url": f"{PWT_DATAVERSE}/dataset.xhtml?persistentId={PWT_DATASET_DOI}",
        "download_url": f"https://www.rug.nl/ggdc/docs/pwt1001.xlsx",
        "note": "PWT data is available as Excel/Stata download. Use the download_url to access full dataset.",
        "available_indicators": list(PWT_INDICATORS.keys()),
        "available_countries": list(PWT_COUNTRIES.keys()),
    }


def get_gdp_data(country: str, start_year: int = 1960, end_year: int = 2019) -> Dict:
    return {
        "country": country,
        "country_name": PWT_COUNTRIES.get(country, country),
        "gdp_indicators": {
            "rgdpe": PWT_INDICATORS["rgdpe"],
            "rgdpo": PWT_INDICATORS["rgdpo"],
            "cgdpe": PWT_INDICATORS["cgdpe"],
            "cgdpo": PWT_INDICATORS["cgdpo"],
            "rgdpna": PWT_INDICATORS["rgdpna"],
        },
        "start_year": start_year,
        "end_year": end_year,
        "dataset": "Penn World Tables 10.01",
        "download_url": "https://www.rug.nl/ggdc/docs/pwt1001.xlsx",
        "note": "Download full dataset for time-series analysis.",
        "unit": "Millions of 2017 USD (PPP adjusted)",
    }


def get_productivity(country: str, start_year: int = 1960, end_year: int = 2019) -> Dict:
    return {
        "country": country,
        "country_name": PWT_COUNTRIES.get(country, country),
        "productivity_indicators": {
            "ctfp": PWT_INDICATORS["ctfp"],
            "cwtfp": PWT_INDICATORS["cwtfp"],
            "rtfpna": PWT_INDICATORS["rtfpna"],
            "hc": PWT_INDICATORS["hc"],
            "labsh": PWT_INDICATORS["labsh"],
            "avh": PWT_INDICATORS["avh"],
        },
        "start_year": start_year,
        "end_year": end_year,
        "dataset": "Penn World Tables 10.01",
        "download_url": "https://www.rug.nl/ggdc/docs/pwt1001.xlsx",
    }


def get_trade_openness(country: str, start_year: int = 1960, end_year: int = 2019) -> Dict:
    return {
        "country": country,
        "country_name": PWT_COUNTRIES.get(country, country),
        "trade_indicators": {
            "csh_x": PWT_INDICATORS["csh_x"],
            "csh_m": PWT_INDICATORS["csh_m"],
            "csh_r": PWT_INDICATORS["csh_r"],
            "pl_x": PWT_INDICATORS["pl_x"],
            "pl_m": PWT_INDICATORS["pl_m"],
        },
        "start_year": start_year,
        "end_year": end_year,
        "dataset": "Penn World Tables 10.01",
        "download_url": "https://www.rug.nl/ggdc/docs/pwt1001.xlsx",
    }


def get_available_indicators() -> Dict:
    return {
        "total_indicators": len(PWT_INDICATORS),
        "indicators": PWT_INDICATORS,
        "dataset_version": "PWT 10.01",
        "time_coverage": "1950-2019",
        "country_coverage": "183 countries",
        "reference_year": "2017 USD (PPP)",
    }


def get_countries() -> Dict:
    return {
        "total_countries": len(PWT_COUNTRIES),
        "countries": PWT_COUNTRIES,
        "dataset": "Penn World Tables 10.01",
        "note": "PWT covers 183 countries. Shown here are commonly used economies.",
        "full_list_url": "https://www.rug.nl/ggdc/productivity/pwt/",
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "data":
        country = args[1] if len(args) > 1 else "USA"
        indicator = args[2] if len(args) > 2 else "rgdpe"
        start_year = int(args[3]) if len(args) > 3 else 1960
        end_year = int(args[4]) if len(args) > 4 else 2019
        result = get_pwt_data(country, indicator, start_year, end_year)
    elif command == "gdp":
        country = args[1] if len(args) > 1 else "USA"
        start_year = int(args[2]) if len(args) > 2 else 1960
        end_year = int(args[3]) if len(args) > 3 else 2019
        result = get_gdp_data(country, start_year, end_year)
    elif command == "productivity":
        country = args[1] if len(args) > 1 else "USA"
        start_year = int(args[2]) if len(args) > 2 else 1960
        end_year = int(args[3]) if len(args) > 3 else 2019
        result = get_productivity(country, start_year, end_year)
    elif command == "trade":
        country = args[1] if len(args) > 1 else "USA"
        start_year = int(args[2]) if len(args) > 2 else 1960
        end_year = int(args[3]) if len(args) > 3 else 2019
        result = get_trade_openness(country, start_year, end_year)
    elif command == "indicators":
        result = get_available_indicators()
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
