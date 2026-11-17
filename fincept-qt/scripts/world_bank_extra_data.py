"""
World Bank Extra Data Fetcher
World Bank extended indicators: climate, poverty, gender, education, health,
and governance data beyond basic coverage.
"""
import sys
import json
import os
import requests
import fincept_connector as fc
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('WORLD_BANK_API_KEY', '')
BASE_URL = "https://api.worldbank.org/v2"

# The World Bank API often takes 10-20 s per uncached query and sometimes stalls for minutes:
# no read retries (they multiplied the timeout), a bounded timeout, and the indicators of the
# multi-indicator commands are fetched concurrently.
session = fc.http_session()
TIMEOUT = (8, 30)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "json"
    params["per_page"] = params.get("per_page", 500)
    try:
        response = session.get(url, params=params, timeout=TIMEOUT)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error("World Bank API", e)}
    except ValueError:
        return {"error": "World Bank API returned a response that is not JSON"}
    # Errors come back as HTTP 200 with [{"message": [{"id", "key", "value"}]}].
    if isinstance(data, list) and data and isinstance(data[0], dict) and "message" in data[0]:
        msg = (data[0].get("message") or [{}])[0]
        return {"error": f"World Bank API: {msg.get('value') or msg.get('key') or 'request rejected'}"}
    if isinstance(data, list) and len(data) == 2:
        rows = data[1]
        # The API sends the year as text ("2023"): a number at the source ("2023Q1" stays text).
        for r in rows if isinstance(rows, list) else []:
            if isinstance(r, dict):
                r["date"] = fc.number(r.get("date"))
        return {"metadata": data[0], "data": rows}
    return data


def _indicator(country: str, code: str, date: str) -> Any:
    params = {"date": date}
    if code.startswith("GOV_WGI_"):
        params["source"] = 3  # Worldwide Governance Indicators
    return _make_request(f"country/{country.upper()}/indicator/{code}", params)


CLIMATE_INDICATORS = {
    "co2_emissions": "EN.GHG.CO2.PC.CE.AR5",
    "renewable_energy": "EG.FEC.RNEW.ZS",
    "forest_area": "AG.LND.FRST.ZS",
    "access_clean_fuels": "EG.CFT.ACCS.ZS",
    "electricity_access": "EG.ELC.ACCS.ZS",
}

POVERTY_INDICATORS = {
    "poverty_headcount": "SI.POV.DDAY",
    "gini": "SI.POV.GINI",
    "poverty_gap": "SI.POV.GAPS",
    "bottom_20_income": "SI.DST.FRST.20",
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

# Worldwide Governance Indicators (source 3); the old WDI ids (RL.EST, ...) were archived.
GOVERNANCE_INDICATORS = {
    "rule_of_law": "GOV_WGI_RL.EST",
    "control_of_corruption": "GOV_WGI_CC.EST",
    "government_effectiveness": "GOV_WGI_GE.EST",
    "regulatory_quality": "GOV_WGI_RQ.EST",
    "voice_accountability": "GOV_WGI_VA.EST",
    "political_stability": "GOV_WGI_PV.EST",
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
    return fc.parallel_map(lambda code: _indicator(country, code, f"2000:{year}"), POVERTY_INDICATORS)


def get_gender_indicators(country: str = "US", indicator: str = "female_labor_force", year: str = "2023") -> Any:
    ind_code = GENDER_INDICATORS.get(indicator, indicator)
    params = {"date": f"2000:{year}"}
    return _make_request(f"country/{country.upper()}/indicator/{ind_code}", params)


def get_education_data(country: str = "US", indicator: str = "primary_enrollment", year: str = "2023") -> Any:
    ind_code = EDUCATION_INDICATORS.get(indicator, indicator)
    params = {"date": f"2000:{year}"}
    return _make_request(f"country/{country.upper()}/indicator/{ind_code}", params)


def get_governance_indicators(country: str = "US", year: str = "2023") -> Any:
    return fc.parallel_map(lambda code: _indicator(country, code, f"2000:{year}"), GOVERNANCE_INDICATORS)


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
