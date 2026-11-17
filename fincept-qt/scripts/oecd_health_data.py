"""
OECD Health Statistics Data Fetcher
Provides health expenditure, life expectancy, disease burden, pharmaceutical
market data, health workforce, and hospital statistics for OECD countries.
Source: OECD Data Explorer SDMX API (sdmx.oecd.org, agency OECD.ELS.HD), no key needed.
"""
import csv
import io
import re
import sys
import json
import requests
from typing import Dict, Any, List

import fincept_connector as fc
import fincept_json

SERVICE = "OECD Data Explorer (sdmx.oecd.org)"
DATA_URL = "https://sdmx.oecd.org/public/rest/data/OECD.ELS.HD,{flow},/{key}"

# Dataflows of the OECD Health Statistics (sdmx.oecd.org/public/rest/dataflow/OECD.ELS.HD, checked 2026-11-16)
DF_SHA = "DSD_SHA@DF_SHA"                          # Health expenditure and financing
DF_LE = "DSD_HEALTH_STAT@DF_LE"                    # Life expectancy
DF_COM = "DSD_HEALTH_STAT@DF_COM"                  # Causes of mortality
DF_REAC = "DSD_HEALTH_EMP_REAC@DF_REAC"            # Healthcare human resources
DF_HOSP = "DSD_HEALTH_REAC_HOSP@DF_HOSP_REAC"      # Healthcare provider resources (hospitals, beds, equipment)

# expenditure type_ -> FINANCING_SCHEME code (raw HF codes are accepted too)
FINANCING = {"TOT": "_T", "GOV": "HF1", "COMP": "HF1", "VOL": "HF2", "OOP": "HF3", "PRIV": "HF2HF3"}
# workers type_ -> HEALTH_PROF code (raw codes such as EMPLGENP are accepted too)
PROFESSIONS = {"PHYS": "PHYS", "NURS": "MINU", "NURSE": "MINU", "MIDW": "MIDW", "DENT": "DNST", "DNST": "DNST",
               "PHARM": "PHST", "PHST": "PHST", "PHYSIO": "PSIO", "PSIO": "PSIO", "CARE": "CARE"}
# hospitals indicator -> (MEASURE, UNIT_MEASURE, MEDICAL_TECH)
HOSPITAL_INDICATORS = {
    "HOSPBED": ("HB", "10P3HB", ""),            # hospital beds per 1 000 inhabitants
    "HOSP": ("H", "10P6HB", ""),                # hospitals per million inhabitants
    "ICU": ("HBU_ICU", "10P5HB", ""),           # ICU beds per 100 000 inhabitants
    "MRI": ("MTU", "10P6HB", "MRI"),            # MRI units per million inhabitants
    "CT": ("MTU", "10P6HB", "CT_SCAN"),         # CT scanners per million inhabitants
    "PET": ("MTU", "10P6HB", "PET_SCAN"),
    "MAMM": ("MTU", "10P6HB", "MAMM"),
    "RT": ("MTU", "10P6HB", "RT"),
}

_CODE = re.compile(r"[A-Za-z0-9_+]{1,20}")

session = fc.http_session()
session.headers.update({"User-Agent": "Mozilla/5.0 (FinceptTerminal; +https://fincept.in)"})


def _code(value: str, what: str) -> str:
    value = (value or "").strip()
    if not _CODE.fullmatch(value):
        raise ValueError(f"invalid {what} '{value}'")
    return value.upper()


def _num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def _fetch_oecd_stat(flow: str, key: str, year: int = None) -> Any:
    """Observations of one OECD.ELS.HD dataflow for an SDMX key, as a list of
    {period, value, unit, <every dimension that varies across the result>}."""
    params = {"dimensionAtObservation": "AllDimensions", "format": "csvfilewithlabels"}
    if year:
        params["startPeriod"] = str(year)
        params["endPeriod"] = str(year)
    try:
        response = session.get(DATA_URL.format(flow=flow, key=key), params=params, timeout=(10, 45))
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e)}
    if response.status_code == 404:
        return {"error": f"{SERVICE}: no data for this country / selection"
                         + (f" in {year}" if year else "")}
    if not response.ok:
        return {"error": fc.http_error(SERVICE, response)}
    rows = list(csv.DictReader(io.StringIO(response.content.decode("utf-8-sig", "replace"))))
    if not rows:
        return {"error": f"{SERVICE}: no data for this country / selection"}
    skip = {"STRUCTURE", "STRUCTURE_ID", "STRUCTURE_NAME", "ACTION", "TIME_PERIOD", "OBS_VALUE", "OBS_STATUS",
            "OBS_STATUS2", "DECIMALS", "UNIT_MULT", "BASE_PER", "CURRENCY", "REF_AREA", "UNIT_MEASURE"}
    header = list(rows[0])
    # csvfilewithlabels: every code column is followed by its label column
    labels = {c: header[i + 1] for i, c in enumerate(header[:-1]) if c.isupper() and not header[i + 1].isupper()}
    varying = [c for c in labels if c not in skip and len({r.get(c) for r in rows}) > 1]
    out = []
    for r in rows:
        obs = {"period": fc.number(r.get("TIME_PERIOD")), "value": _num(r.get("OBS_VALUE")),
               "unit": r.get(labels.get("UNIT_MEASURE", ""), r.get("UNIT_MEASURE"))}
        for c in varying:
            obs[c.lower()] = r.get(c)
            obs[c.lower() + "_label"] = r.get(labels[c])
        if r.get("OBS_STATUS"):
            obs["status"] = r.get(labels.get("OBS_STATUS", ""), r.get("OBS_STATUS"))
        out.append(obs)
    out.sort(key=lambda o: (tuple(str(o.get(c.lower())) for c in varying), o["period"] or ""))
    return out


def _result(data: Any, flow: str, payload_key: str, **meta) -> Dict:
    if isinstance(data, dict) and "error" in data:
        return data
    return {**meta, "dataflow": f"OECD.ELS.HD,{flow}", "count": len(data), payload_key: data}


def get_health_expenditure(country: str, year: int = None, type_: str = "TOT") -> Any:
    """Return current health expenditure as % of GDP for a country (type_: TOT, GOV, VOL, OOP or an HF code)."""
    try:
        c = _code(country, "country")
        scheme = FINANCING.get(type_.upper(), _code(type_, "financing type"))
    except ValueError as e:
        return {"error": str(e)}
    data = _fetch_oecd_stat(DF_SHA, f"{c}.A.EXP_HEALTH.PT_B1GQ.{scheme}._Z._T._T._T._Z._Z.", year)
    return _result(data, DF_SHA, "expenditure_data", country=c, year=year, type=type_,
                   measure="Current expenditure on health, % of GDP")


def get_life_expectancy(country: str, year: int = None, sex: str = "T") -> Any:
    """Return life expectancy at birth for a country (sex: T=total, M=male, F=female)."""
    try:
        c = _code(country, "country")
        s = {"T": "_T", "M": "M", "F": "F"}[sex.upper()]
    except (ValueError, KeyError):
        return {"error": f"invalid country '{country}' or sex '{sex}' (sex: T, M or F)"}
    data = _fetch_oecd_stat(DF_LE, f"{c}.A.LFEXP.Y.Y0.{s}._Z._Z._Z._Z._Z._Z._Z", year)
    return _result(data, DF_LE, "life_expectancy", country=c, year=year, sex=sex)


def get_disease_burden(country: str, cause: str = "ALL", year: int = None) -> Any:
    """Return age-standardised death rates per 100 000 for a country and cause
    (ALL, or an ICD group code such as ISCH, MYOC, PNEU, DMTA, CVID - the CICD prefix is optional)."""
    try:
        c = _code(country, "country")
        cause_code = "_T" if cause.upper() in ("ALL", "_T", "TOTAL") else _code(cause, "cause")
    except ValueError as e:
        return {"error": str(e)}
    if cause_code != "_T" and not cause_code.startswith("CICD"):
        cause_code = "CICD" + cause_code
    data = _fetch_oecd_stat(DF_COM, f"{c}.A.CSEM.DT_10P5HB._T._T._Z.{cause_code}.STANDARD._Z._Z._Z._Z", year)
    return _result(data, DF_COM, "disease_burden", country=c, cause=cause, year=year,
                   measure="Deaths per 100 000 inhabitants (age-standardised)")


def get_pharmaceutical_market(country: str, year: int = None) -> Any:
    """Return pharmaceutical spending (health function HC51) as % of GDP and US$ PPP per person."""
    try:
        c = _code(country, "country")
    except ValueError as e:
        return {"error": str(e)}
    data = _fetch_oecd_stat(DF_SHA, f"{c}.A.EXP_HEALTH.PT_B1GQ+USD_PPP_PS._T._Z.HC51._T._T._Z._Z.V", year)
    if isinstance(data, list):
        data = [{k: v for k, v in o.items() if k not in ("price_base", "price_base_label")} for o in data]
    return _result(data, DF_SHA, "pharma_market", country=c, year=year,
                   measure="Expenditure on pharmaceuticals and other medical non-durable goods (HC51)")


def get_health_workers(country: str, type_: str = "PHYS", year: int = None) -> Any:
    """Return health workforce density per 1 000 population (PHYS=physicians, NURS=nurses, DENT, PHARM,
    MIDW, PHYSIO, CARE or an OECD HEALTH_PROF code), by activity status (practising / professionally
    active / licensed)."""
    try:
        c = _code(country, "country")
        prof = PROFESSIONS.get(type_.upper(), _code(type_, "profession"))
    except ValueError as e:
        return {"error": str(e)}
    data = _fetch_oecd_stat(DF_REAC, f"{c}.HSE.10P3HB...{prof}...", year)
    return _result(data, DF_REAC, "workforce", country=c, type=type_, year=year)


def get_hospital_data(country: str, indicator: str = "HOSPBED", year: int = None) -> Any:
    """Return hospital infrastructure data: HOSPBED (beds per 1 000), HOSP (hospitals per million),
    ICU (ICU beds per 100 000), MRI / CT / PET / MAMM / RT (units per million)."""
    try:
        c = _code(country, "country")
    except ValueError as e:
        return {"error": str(e)}
    spec = HOSPITAL_INDICATORS.get(indicator.upper())
    if not spec:
        return {"error": f"unknown hospital indicator '{indicator}'", "available": list(HOSPITAL_INDICATORS)}
    measure, unit, tech = spec
    data = _fetch_oecd_stat(DF_HOSP, f"{c}.{measure}.{unit}.....{tech}.", year)
    return _result(data, DF_HOSP, "hospital_data", country=c, indicator=indicator, year=year)


def _year(args: List[str], i: int):
    if len(args) > i and args[i]:
        return int(args[i])
    return None


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    country = args[1] if len(args) > 1 else ""
    try:
        if command in ("expenditure", "life_expectancy", "disease", "pharma", "workers", "hospitals") \
                and not country:
            result = {"error": "country required (ISO3 code, e.g. USA)"}
        elif command == "expenditure":
            year = _year(args, 2)
            type_ = args[3] if len(args) > 3 else "TOT"
            result = get_health_expenditure(country, year, type_)
        elif command == "life_expectancy":
            year = _year(args, 2)
            sex = args[3] if len(args) > 3 else "T"
            result = get_life_expectancy(country, year, sex)
        elif command == "disease":
            cause = args[2] if len(args) > 2 else "ALL"
            year = _year(args, 3)
            result = get_disease_burden(country, cause, year)
        elif command == "pharma":
            year = _year(args, 2)
            result = get_pharmaceutical_market(country, year)
        elif command == "workers":
            type_ = args[2] if len(args) > 2 else "PHYS"
            year = _year(args, 3)
            result = get_health_workers(country, type_, year)
        elif command == "hospitals":
            indicator = args[2] if len(args) > 2 else "HOSPBED"
            year = _year(args, 3)
            result = get_hospital_data(country, indicator, year)
    except ValueError:
        result = {"error": "year must be a number such as 2022"}
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
