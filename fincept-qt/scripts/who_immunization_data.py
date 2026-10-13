"""
WHO Immunization Data Fetcher
Vaccination coverage (WHO/UNICEF WUENIC estimates) and reported cases of vaccine-preventable
diseases, by country, WHO region or globally, from the keyless WHO Global Health Observatory
OData API (ghoapi.azureedge.net).
"""
import sys
import json
import requests
from typing import Dict, Any

BASE_URL = "https://ghoapi.azureedge.net/api"

VACCINES = {
    "BCG": "WHS4_543", "DTP1": "VACCINECOVERAGE_DTP1", "DTP3": "WHS4_100", "HEPB3": "WHS4_117",
    "HIB3": "WHS4_129", "IPV1": "VACCINECOVERAGE_IPV1", "POL3": "WHS4_544", "IPVC": "WHS4_544",
    "MCV1": "WHS8_110", "MCV2": "MCV2", "PCV3": "PCV3", "ROTAC": "ROTAC", "YFV": "VACCINECOVERAGE_YFV",
    "MENA": "VACCINECOVERAGE_MENA_C", "HPV": "SDGHPVRECEIVED",
}
DISEASES = {
    "MEASLES": "WHS3_62", "DIPHTHERIA": "WHS3_41", "PERTUSSIS": "WHS3_43", "TETANUS": "WHS3_46",
    "NEONATAL_TETANUS": "WHS3_56", "POLIO": "WHS3_49", "WILD_POLIO": "VACCINEPREVENTABLE_WILDPOLIO",
    "RUBELLA": "WHS3_57", "CRS": "WHS3_55", "MUMPS": "WHS3_53", "YELLOW_FEVER": "WHS3_50",
    "JAPANESE_ENCEPHALITIS": "WHS3_42", "MENINGITIS": "WHS3_47",
}
REGIONS = {"AFR", "AMR", "SEAR", "EUR", "EMR", "WPR"}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}"
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


def _gho(code: str, area: str = None, year: int = None) -> Any:
    filters = []
    if area:
        a = area.upper()
        if a == "GLOBAL":
            filters.append("SpatialDimType eq 'GLOBAL'")
        else:
            filters.append(f"SpatialDim eq '{a}'")
    if year:
        filters.append(f"TimeDim eq {int(year)}")
    params = {"$filter": " and ".join(filters)} if filters else None
    data = _make_request(code, params)
    if isinstance(data, dict) and "error" in data:
        return data
    rows = [{"area": r.get("SpatialDim"), "area_type": r.get("SpatialDimType"), "year": r.get("TimeDim"),
             "value": r.get("NumericValue"), "display": r.get("Value")}
            for r in (data or {}).get("value", []) if r.get("NumericValue") is not None]
    rows.sort(key=lambda r: (r["area"] or "", r["year"] or 0))
    return rows


def _resolve(name: str, table: Dict[str, str]) -> str:
    return table.get(name.upper().replace("-", "_"), name)


def get_coverage(vaccine: str, country: str, year: int = None) -> Any:
    """Vaccination coverage (%) for a vaccine and country (ISO-3) or WHO region code."""
    code = _resolve(vaccine, VACCINES)
    rows = _gho(code, country, year)
    if isinstance(rows, dict):
        return rows
    if not rows:
        return {"error": f"No coverage data for {vaccine} / {country}"}
    return {"vaccine": vaccine.upper(), "indicator": code, "country": country.upper(), "year": year,
            "unit": "% of target population", "coverage": rows}


def get_global_coverage(vaccine: str = "DTP3", year: int = None) -> Any:
    """Global and WHO-region coverage (%) for a vaccine."""
    code = _resolve(vaccine, VACCINES)
    rows = _gho(code, None, year)
    if isinstance(rows, dict):
        return rows
    rows = [r for r in rows if r["area_type"] in ("GLOBAL", "REGION")]
    if not rows:
        return {"error": f"No global coverage data for {vaccine}"}
    return {"vaccine": vaccine.upper(), "indicator": code, "year": year, "unit": "% of target population",
            "global_coverage": rows}


def get_disease_incidence(disease: str, country: str, year: int = None) -> Any:
    """Reported cases of a vaccine-preventable disease for a country (ISO-3) / region / GLOBAL."""
    code = _resolve(disease, DISEASES)
    rows = _gho(code, country, year)
    if isinstance(rows, dict):
        return rows
    if not rows:
        return {"error": f"No reported-case data for {disease} / {country}"}
    return {"disease": disease.upper(), "indicator": code, "country": country.upper(), "year": year,
            "unit": "reported cases", "incidence": rows}


def get_countries() -> Any:
    """Countries (ISO-3 codes) available in the WHO Global Health Observatory."""
    data = _make_request("DIMENSION/COUNTRY/DimensionValues")
    if isinstance(data, dict) and "error" in data:
        return data
    rows = [{"code": r.get("Code"), "name": r.get("Title"), "region": r.get("ParentCode")}
            for r in data.get("value", [])]
    return {"count": len(rows), "countries": rows, "regions": sorted(REGIONS)}


def get_vaccines() -> Any:
    return {"vaccines": VACCINES, "diseases": DISEASES}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "coverage":
        if len(args) < 3:
            result = {"error": "vaccine and country required"}
        else:
            vaccine = args[1]
            country = args[2]
            year = int(args[3]) if len(args) > 3 else None
            result = get_coverage(vaccine, country, year)
    elif command == "global":
        vaccine = args[1] if len(args) > 1 else "DTP3"
        year = int(args[2]) if len(args) > 2 else None
        result = get_global_coverage(vaccine, year)
    elif command == "incidence":
        if len(args) < 3:
            result = {"error": "disease and country required"}
        else:
            disease = args[1]
            country = args[2]
            year = int(args[3]) if len(args) > 3 else None
            result = get_disease_incidence(disease, country, year)
    elif command == "countries":
        result = get_countries()
    elif command == "vaccines":
        result = get_vaccines()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
