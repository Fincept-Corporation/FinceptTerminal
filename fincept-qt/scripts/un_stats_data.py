"""
UN Statistics Division Data Fetcher
Fetches national accounts, environment accounts, demographic statistics, and gender
statistics for all countries from the UN Statistics Division SDG and SNA APIs.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('UN_STATS_API_KEY', '')
BASE_URL = "https://unstats.un.org/sdgapi/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

SNA_BASE_URL = "https://unstats.un.org/unsd/snaama/downloads"
SDG_BASE_URL = "https://unstats.un.org/sdgapi/v1"
UN_DATA_BASE = "http://data.un.org/ws/rest/data"

UN_SDG_GOALS = {
    "1": "No Poverty",
    "2": "Zero Hunger",
    "3": "Good Health and Well-Being",
    "4": "Quality Education",
    "5": "Gender Equality",
    "6": "Clean Water and Sanitation",
    "7": "Affordable and Clean Energy",
    "8": "Decent Work and Economic Growth",
    "9": "Industry, Innovation and Infrastructure",
    "10": "Reduced Inequalities",
    "11": "Sustainable Cities and Communities",
    "12": "Responsible Consumption and Production",
    "13": "Climate Action",
    "14": "Life Below Water",
    "15": "Life on Land",
    "16": "Peace, Justice and Strong Institutions",
    "17": "Partnerships for the Goals",
}

NATIONAL_ACCOUNTS_INDICATORS = {
    "gdp_current": "GDP, current prices (US dollars)",
    "gdp_constant": "GDP, constant prices (national currency)",
    "gdp_per_capita": "GDP per capita, current prices (US dollars)",
    "gni_current": "GNI, current prices (US dollars)",
    "household_consumption": "Household final consumption expenditure",
    "govt_consumption": "General government final consumption expenditure",
    "gross_capital_formation": "Gross fixed capital formation",
    "exports": "Exports of goods and services",
    "imports": "Imports of goods and services",
    "trade_balance": "Trade balance in goods and services",
}

ENVIRONMENT_INDICATORS = {
    "co2_emissions": "CO2 emissions (metric tons per capita) - EN.ATM.CO2E.PC",
    "forest_area": "Forest area (% of land area) - AG.LND.FRST.ZS",
    "renewable_energy": "Renewable energy consumption (% of total final energy consumption) - EG.FEC.RNEW.ZS",
    "freshwater_withdrawal": "Annual freshwater withdrawals, total (% of internal resources) - ER.H2O.FWTL.ZS",
    "protected_areas": "Terrestrial and marine protected areas (% of total territorial area) - ER.PTD.TOTL.ZS",
}

DEMOGRAPHIC_INDICATORS = {
    "population_total": "Population, total",
    "population_growth": "Population growth (annual %)",
    "urban_population": "Urban population (% of total population)",
    "life_expectancy": "Life expectancy at birth, total (years)",
    "fertility_rate": "Fertility rate, total (births per woman)",
    "mortality_under5": "Mortality rate, under-5 (per 1,000 live births)",
    "net_migration": "Net migration",
    "age_dependency_old": "Age dependency ratio, old (% of working-age population)",
    "median_age": "Median age of population",
}

GENDER_INDICATORS = {
    "gender_inequality_index": "Gender Inequality Index",
    "female_labour_force": "Labor force, female (% of total labor force)",
    "maternal_mortality": "Maternal mortality ratio (modeled estimate, per 100,000 live births)",
    "girls_in_school": "School enrollment, primary, female (% gross)",
    "women_in_parliament": "Proportion of seats held by women in national parliaments (%)",
    "female_business_ownership": "Women who own a bank account at a formal institution (% age 15+)",
    "gender_wage_gap": "Ratio of female to male labor force participation rate (%)",
    "sdg5_1_1": "SDG 5.1.1 - Legal frameworks for gender equality",
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


def _get_wb_indicator(country: str, wb_indicator: str, year: str = "") -> Dict:
    country_code = country.upper()
    url = f"https://api.worldbank.org/v2/country/{country_code}/indicator/{wb_indicator}"
    params = {"format": "json", "per_page": 20}
    if year:
        params["date"] = year
    else:
        params["mrv"] = 10
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        if isinstance(data, list) and len(data) >= 2:
            observations = data[1] or []
            return [
                {"year": obs.get("date", ""), "value": obs.get("value", None)}
                for obs in observations
                if obs.get("value") is not None
            ]
        return []
    except Exception:
        return []


def get_national_accounts(country: str, indicator: str, year: str = "") -> Dict:
    indicator_info = NATIONAL_ACCOUNTS_INDICATORS.get(indicator, indicator)
    wb_map = {
        "gdp_current": "NY.GDP.MKTP.CD",
        "gdp_constant": "NY.GDP.MKTP.KN",
        "gdp_per_capita": "NY.GDP.PCAP.CD",
        "gni_current": "NY.GNP.MKTP.CD",
        "household_consumption": "NE.CON.PRVT.CD",
        "govt_consumption": "NE.CON.GOVT.CD",
        "gross_capital_formation": "NE.GDI.TOTL.CD",
        "exports": "NE.EXP.GNFS.CD",
        "imports": "NE.IMP.GNFS.CD",
        "trade_balance": "BN.CAB.XOKA.CD",
    }
    wb_id = wb_map.get(indicator, indicator)
    data = _get_wb_indicator(country, wb_id, year)
    return {
        "country": country,
        "indicator": indicator,
        "indicator_description": indicator_info,
        "wb_indicator_id": wb_id,
        "source": "World Bank / UN National Accounts",
        "data": sorted(data, key=lambda x: x["year"]) if data else [],
        "available_indicators": NATIONAL_ACCOUNTS_INDICATORS,
    }


def get_environment_accounts(country: str, year: str = "") -> Dict:
    wb_env_map = {
        "co2_emissions": "EN.ATM.CO2E.PC",
        "forest_area": "AG.LND.FRST.ZS",
        "renewable_energy": "EG.FEC.RNEW.ZS",
        "freshwater_withdrawal": "ER.H2O.FWTL.ZS",
        "protected_areas": "ER.PTD.TOTL.ZS",
    }
    result = {"country": country, "environment_indicators": {}}
    for name, wb_id in wb_env_map.items():
        data = _get_wb_indicator(country, wb_id, year)
        result["environment_indicators"][name] = {
            "description": ENVIRONMENT_INDICATORS.get(name, name),
            "data": sorted(data, key=lambda x: x["year"]) if data else [],
        }
    result["source"] = "World Bank / UN Environment Statistics"
    return result


def get_demographic_stats(country: str, indicator: str, year: str = "") -> Dict:
    wb_demo_map = {
        "population_total": "SP.POP.TOTL",
        "population_growth": "SP.POP.GROW",
        "urban_population": "SP.URB.TOTL.IN.ZS",
        "life_expectancy": "SP.DYN.LE00.IN",
        "fertility_rate": "SP.DYN.TFRT.IN",
        "mortality_under5": "SH.DYN.MORT",
        "net_migration": "SM.POP.NETM",
        "age_dependency_old": "SP.POP.DPND.OL",
    }
    wb_id = wb_demo_map.get(indicator, indicator)
    data = _get_wb_indicator(country, wb_id, year)
    return {
        "country": country,
        "indicator": indicator,
        "indicator_description": DEMOGRAPHIC_INDICATORS.get(indicator, indicator),
        "wb_indicator_id": wb_id,
        "source": "UN Population Division / World Bank",
        "data": sorted(data, key=lambda x: x["year"]) if data else [],
        "available_indicators": DEMOGRAPHIC_INDICATORS,
    }


def get_gender_stats(country: str, indicator: str, year: str = "") -> Dict:
    wb_gender_map = {
        "female_labour_force": "SL.TLF.TOTL.FE.ZS",
        "maternal_mortality": "SH.STA.MMRT",
        "girls_in_school": "SE.ENR.PRIM.FM.ZS",
        "women_in_parliament": "SG.GEN.PARL.ZS",
        "gender_wage_gap": "SL.TLF.ACTI.FM.ZS",
    }
    wb_id = wb_gender_map.get(indicator, indicator)
    data = _get_wb_indicator(country, wb_id, year) if wb_id else []
    sdg_data = _make_request(f"sdg/Indicator/Data?indicator=5.1.1&area={country}", {})
    return {
        "country": country,
        "indicator": indicator,
        "indicator_description": GENDER_INDICATORS.get(indicator, indicator),
        "source": "UN Statistics Division / World Bank Gender Data Portal",
        "data": sorted(data, key=lambda x: x["year"]) if data else [],
        "sdg5_indicators": GENDER_INDICATORS,
        "available_indicators": GENDER_INDICATORS,
    }


def get_available_indicators() -> Dict:
    sdg_data = _make_request("sdg/Indicator/List", {})
    return {
        "sdg_goals": UN_SDG_GOALS,
        "national_accounts": NATIONAL_ACCOUNTS_INDICATORS,
        "environment": ENVIRONMENT_INDICATORS,
        "demographic": DEMOGRAPHIC_INDICATORS,
        "gender": GENDER_INDICATORS,
        "sdg_api_base": SDG_BASE_URL,
        "undata_base": UN_DATA_BASE,
        "note": "UN Statistics Division provides SDG indicators, SNA data, demographic, and environmental statistics.",
    }


def get_countries() -> Dict:
    data = _make_request("sdg/GeoArea/List", {})
    if "error" in data:
        return {
            "note": "UN Statistics Division country list",
            "total": 249,
            "un_stats_url": "https://unstats.un.org",
            "sdg_api": SDG_BASE_URL,
            "error": data.get("error", ""),
        }
    return {
        "total": len(data) if isinstance(data, list) else 0,
        "countries": data if isinstance(data, list) else [],
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "national_accounts":
        country = args[1] if len(args) > 1 else "USA"
        indicator = args[2] if len(args) > 2 else "gdp_current"
        year = args[3] if len(args) > 3 else ""
        result = get_national_accounts(country, indicator, year)
    elif command == "environment":
        country = args[1] if len(args) > 1 else "USA"
        year = args[2] if len(args) > 2 else ""
        result = get_environment_accounts(country, year)
    elif command == "demographic":
        country = args[1] if len(args) > 1 else "USA"
        indicator = args[2] if len(args) > 2 else "population_total"
        year = args[3] if len(args) > 3 else ""
        result = get_demographic_stats(country, indicator, year)
    elif command == "gender":
        country = args[1] if len(args) > 1 else "USA"
        indicator = args[2] if len(args) > 2 else "women_in_parliament"
        year = args[3] if len(args) > 3 else ""
        result = get_gender_stats(country, indicator, year)
    elif command == "indicators":
        result = get_available_indicators()
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
