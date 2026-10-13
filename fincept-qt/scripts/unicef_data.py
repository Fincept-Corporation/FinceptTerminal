"""
UNICEF Data Fetcher
Child mortality, immunization, nutrition, education and water/sanitation indicators for all
countries via the keyless UNICEF SDMX API (UNICEF Data Warehouse, GLOBAL_DATAFLOW).
"""
import sys
import json
import requests
from datetime import datetime
from typing import Dict, Any

from sdmx_json import flatten, compact

BASE_URL = "https://sdmx.data.unicef.org/ws/public/sdmxapi/rest"
FLOW = "UNICEF,GLOBAL_DATAFLOW,1.0"   # key: REF_AREA.INDICATOR.SEX

# Friendly aliases -> UNICEF Data Warehouse indicator codes (any raw code is accepted too).
ALIASES = {
    "MR_U5": "CME_MRY0T4", "U5MR": "CME_MRY0T4", "INFANT": "CME_MRY0", "NEONATAL": "CME_MRM0",
    "DTP3": "IM_DTP3", "MCV1": "IM_MCV1", "BCG": "IM_BCG", "POL3": "IM_POL3", "MCV2": "IM_MCV2",
    "STUNTING": "NT_ANT_HAZ_NE2_MOD", "WASTING": "NT_ANT_WHZ_NE2", "OVERWEIGHT": "NT_ANT_WHZ_PO2_MOD",
    "PRIMARY": "ED_ANAR_L1", "LOWER_SECONDARY": "ED_ANAR_L2", "UPPER_SECONDARY": "ED_ANAR_L3",
    "DRINKING_WATER": "WS_PPL_W-B", "SAFE_WATER": "WS_PPL_W-SM", "SANITATION": "WS_PPL_S-B",
    "SAFE_SANITATION": "WS_PPL_S-SM", "HANDWASHING": "WS_PPL_H-B",
}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(path: str, params: Dict = None) -> Any:
    params = dict(params or {})
    params["format"] = "sdmx-json"
    try:
        response = session.get(f"{BASE_URL}/{path}", params=params, timeout=90)
        if response.status_code == 404:
            return {"error": "No data for this query (HTTP 404) - check the country / indicator codes"}
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {e.response.text[:200]}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _ind(code: str) -> str:
    return "+".join(ALIASES.get(c.strip().upper(), c.strip()) for c in code.replace(",", "+").split("+") if c.strip())


def _area(country: str) -> str:
    if not country or country.upper() == "ALL":
        return ""
    return country.upper().replace(",", "+")


def _fetch(country: str, indicator: str, sex: str = "_T", year: str = None, label: str = "data") -> Any:
    key = f"{_area(country)}.{_ind(indicator)}.{sex}"
    params = {}
    if year:
        params["startPeriod"] = year
        params["endPeriod"] = year
    else:
        params["startPeriod"] = str(datetime.now().year - 15)
    raw = _make_request(f"data/{FLOW}/{key}", params)
    if isinstance(raw, dict) and "error" in raw:
        return {**raw, "key": key}
    series = compact(flatten(raw))
    if not series:
        return {"error": "No data returned", "key": key}
    return {"key": key, label: series, "series_count": len(series), "source": "UNICEF Data Warehouse"}


def get_child_mortality(country: str = "ALL", year: str = None) -> Any:
    return _fetch(country, "CME_MRY0T4", "_T", year, "mortality")


def get_immunization(country: str = "ALL", vaccine: str = "DTP3", year: str = None) -> Any:
    return _fetch(country, vaccine, "_T", year, "immunization")


def get_nutrition(country: str = "ALL", indicator: str = "STUNTING", year: str = None) -> Any:
    return _fetch(country, indicator, "_T", year, "nutrition")


def get_education(country: str = "ALL", level: str = "PRIMARY", sex: str = "T", year: str = None) -> Any:
    sex_code = {"T": "_T", "F": "F", "M": "M"}.get(sex.upper(), sex)
    return _fetch(country, level, sex_code, year, "education")


def get_water_sanitation(country: str = "ALL", indicator: str = "DRINKING_WATER", year: str = None) -> Any:
    return _fetch(country, indicator, "_T", year, "wash")


def get_datasets() -> Any:
    raw = _make_request("dataflow/UNICEF")
    if isinstance(raw, dict) and "error" in raw:
        return raw
    flows = []
    for f in raw.get("data", {}).get("dataflows", []):
        name = f.get("name") if isinstance(f.get("name"), str) else (f.get("names") or {}).get("en", "")
        flows.append({"id": f.get("id"), "version": f.get("version"), "name": name})
    return {"count": len(flows), "dataflows": flows}


def get_indicators(query: str = None) -> Any:
    raw = _make_request("codelist/UNICEF/CL_UNICEF_INDICATOR/latest")
    if isinstance(raw, dict) and "error" in raw:
        return raw
    out = []
    for cl in raw.get("data", {}).get("codelists", []):
        for c in cl.get("codes", []):
            name = c.get("name") if isinstance(c.get("name"), str) else (c.get("names") or {}).get("en", "")
            if query and query.lower() not in (c.get("id", "") + " " + name).lower():
                continue
            out.append({"code": c.get("id"), "name": name})
    return {"count": len(out), "indicators": out, "aliases": ALIASES}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "mortality":
        country = args[1] if len(args) > 1 else "ALL"
        year = args[2] if len(args) > 2 else None
        result = get_child_mortality(country, year)
    elif command == "immunization":
        country = args[1] if len(args) > 1 else "ALL"
        vaccine = args[2] if len(args) > 2 else "DTP3"
        year = args[3] if len(args) > 3 else None
        result = get_immunization(country, vaccine, year)
    elif command == "nutrition":
        country = args[1] if len(args) > 1 else "ALL"
        indicator = args[2] if len(args) > 2 else "STUNTING"
        year = args[3] if len(args) > 3 else None
        result = get_nutrition(country, indicator, year)
    elif command == "education":
        country = args[1] if len(args) > 1 else "ALL"
        level = args[2] if len(args) > 2 else "PRIMARY"
        sex = args[3] if len(args) > 3 else "T"
        year = args[4] if len(args) > 4 else None
        result = get_education(country, level, sex, year)
    elif command == "wash":
        country = args[1] if len(args) > 1 else "ALL"
        indicator = args[2] if len(args) > 2 else "DRINKING_WATER"
        year = args[3] if len(args) > 3 else None
        result = get_water_sanitation(country, indicator, year)
    elif command == "datasets":
        result = get_datasets()
    elif command == "indicators":
        query = args[1] if len(args) > 1 else None
        result = get_indicators(query)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
