"""
World Bank Extra Data Fetcher
World Bank extended indicators: climate, poverty, gender, education, health,
and governance data beyond basic coverage.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLD_BANK_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
    params["per_page"] = params.get("per_page", 500)
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        if isinstance(data, list) and len(data) == 2:
            return {"metadata": data[0], "data": data[1]}
        return data
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


CLIMATE_INDICATORS = {
    "co2_emissions": "EN.ATM.CO2E.PC",
    "renewable_energy": "EG.RNW.TOTL.ZS",
    "forest_area": "AG.LND.FRST.ZS",
    "access_clean_fuels": "EG.CFT.ACCS.ZS",
    "electricity_access": "EG.ELC.ACCS.ZS",
}

POVERTY_INDICATORS = {
    "poverty_headcount": "SI.POV.DDAY",
    "gini": "SI.POV.GINI",
    "poverty_gap": "SI.POV.GAPS",
    "bottom_40_income": "SI.DST.FRST.40",
}

GENDER_INDICATORS = {
    "female_labor_force": "SL.TLF.TOTL.FE.ZS",
    "gender_parity_primary": "SE.ENR.PRIM.FM.ZS",
    "women_parliament": "SG.GEN.PARL.ZS",
    "maternal_mortality": "SH.STA.MMRT",
}

EDUCATION_INDICATORS = {
    "primary_enrollment": "SE.PRM.ENRR",
    "secondary_enrollment": "SE.SEC.ENRR",
    "tertiary_enrollment": "SE.TER.ENRR",
    "literacy_rate": "SE.ADT.LITR.ZS",
    "govt_education_spend": "SE.XPD.TOTL.GD.ZS",
}

GOVERNANCE_INDICATORS = {
    "rule_of_law": "RL.EST",
    "control_of_corruption": "CC.EST",
    "government_effectiveness": "GE.EST",
    "regulatory_quality": "RQ.EST",
    "voice_accountability": "VA.EST",
    "political_stability": "PV.EST",
}

HEALTH_INDICATORS = {
    "life_expectancy": "SP.DYN.LE00.IN",
    "infant_mortality": "SP.DYN.IMRT.IN",
    "health_expenditure": "SH.XPD.CHEX.GD.ZS",
    "physicians": "SH.MED.PHYS.ZS",
    "hospital_beds": "SH.MED.BEDS.ZS",
    "undernourishment": "SN.ITK.DEFC.ZS",
}


def get_climate_indicators(country: str = "US", indicator: str = "co2_emissions", start_year: str = "2000", end_year: str = "2023") -> Any:
    ind_code = CLIMATE_INDICATORS.get(indicator, indicator)
    params = {"date": f"{start_year}:{end_year}"}
    return _make_request(f"country/{country.upper()}/indicator/{ind_code}", params)


def get_poverty_data(country: str = "US", year: str = "2023") -> Any:
    results = {}
    for name, code in POVERTY_INDICATORS.items():
        data = _make_request(f"country/{country.upper()}/indicator/{code}", {"date": f"2000:{year}"})
        results[name] = data
    return results


def get_gender_indicators(country: str = "US", indicator: str = "female_labor_force", year: str = "2023") -> Any:
    ind_code = GENDER_INDICATORS.get(indicator, indicator)
    params = {"date": f"2000:{year}"}
    return _make_request(f"country/{country.upper()}/indicator/{ind_code}", params)


def get_education_data(country: str = "US", indicator: str = "primary_enrollment", year: str = "2023") -> Any:
    ind_code = EDUCATION_INDICATORS.get(indicator, indicator)
    params = {"date": f"2000:{year}"}
    return _make_request(f"country/{country.upper()}/indicator/{ind_code}", params)


def get_governance_indicators(country: str = "US", year: str = "2023") -> Any:
    results = {}
    for name, code in GOVERNANCE_INDICATORS.items():
        data = _make_request(f"country/{country.upper()}/indicator/{code}", {"date": f"2000:{year}"})
        results[name] = data
    return results


def get_health_indicators(country: str = "US", indicator: str = "life_expectancy", year: str = "2023") -> Any:
    ind_code = HEALTH_INDICATORS.get(indicator, indicator)
    params = {"date": f"2000:{year}"}
    return _make_request(f"country/{country.upper()}/indicator/{ind_code}", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "climate":
        country = args[1] if len(args) > 1 else "US"
        indicator = args[2] if len(args) > 2 else "co2_emissions"
        start_year = args[3] if len(args) > 3 else "2000"
        end_year = args[4] if len(args) > 4 else "2023"
        result = get_climate_indicators(country, indicator, start_year, end_year)
    elif command == "poverty":
        country = args[1] if len(args) > 1 else "US"
        year = args[2] if len(args) > 2 else "2023"
        result = get_poverty_data(country, year)
    elif command == "gender":
        country = args[1] if len(args) > 1 else "US"
        indicator = args[2] if len(args) > 2 else "female_labor_force"
        year = args[3] if len(args) > 3 else "2023"
        result = get_gender_indicators(country, indicator, year)
    elif command == "education":
        country = args[1] if len(args) > 1 else "US"
        indicator = args[2] if len(args) > 2 else "primary_enrollment"
        year = args[3] if len(args) > 3 else "2023"
        result = get_education_data(country, indicator, year)
    elif command == "governance":
        country = args[1] if len(args) > 1 else "US"
        year = args[2] if len(args) > 2 else "2023"
        result = get_governance_indicators(country, year)
    elif command == "health":
        country = args[1] if len(args) > 1 else "US"
        indicator = args[2] if len(args) > 2 else "life_expectancy"
        year = args[3] if len(args) > 3 else "2023"
        result = get_health_indicators(country, indicator, year)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
