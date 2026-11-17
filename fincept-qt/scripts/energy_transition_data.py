"""
Energy Transition Data Fetcher
Renewable power capacity (IRENA), total energy supply by source and CO2 intensity (IEA)
from the agencies' open statistics APIs - no API key needed.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

# IEA energy-statistics API behind iea.org/data-and-statistics (keyless; series per indicator).
IEA_STATS_URL = "https://api.iea.org/stats"
# IRENASTAT PxWeb API (keyless). The table id changes with each release (e.g. Country_ELECSTAT_2026_H2_PX.px),
# so it is looked up in this folder at run time.
IRENA_FOLDER_URL = "https://pxweb.irena.org/api/v1/en/IRENASTAT/Power%20Capacity%20and%20Generation"

session = fc.http_session()
TIMEOUT = (10, 40)


def _get_json(service: str, url: str, params: Dict = None, body: Dict = None) -> Any:
    try:
        if body is None:
            response = session.get(url, params=params, timeout=TIMEOUT)
        else:
            response = session.post(url, json=body, timeout=TIMEOUT)
        if response.status_code in (400, 404) and "json" not in response.headers.get("Content-Type", ""):
            reason = response.text.strip()[:160] if len(response.text) < 300 else ""
            return {"error": f"{service}: {reason or f'HTTP {response.status_code}'}"}
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(service, e)}
    except ValueError:
        return {"error": f"{service} returned a response that is not JSON"}


def _iea_indicator(series: str, country: str, year: Optional[int] = None) -> Any:
    """Rows of one IEA indicator series for one country (ISO3 code, e.g. USA)."""
    code = country.strip().upper()
    data = _get_json("IEA", f"{IEA_STATS_URL}/indicator/{series}", {"countries": code})
    if isinstance(data, dict) and "error" in data:
        return data
    if not isinstance(data, list):
        return {"error": "IEA returned an unexpected response"}
    # An unknown country code is silently answered with regional aggregates: keep only this country.
    rows = [{"year": r.get("year"), "flow": r.get("flowLabel") or r.get("flow"),
             "product": r.get("productLabel") or r.get("product"), "value": r.get("value"),
             "units": r.get("units")}
            for r in data if str(r.get("short", "")).upper() == code or str(r.get("country", "")).upper() == code]
    if year:
        rows = [r for r in rows if str(r["year"]) == str(year)]
    if not rows:
        return {"error": f"IEA has no {series} data for '{country}'" + (f" in {year}" if year else "")
                         + " - use an ISO3 country code such as USA, DEU or IND"}
    return rows


def _irena_capacity(country: Optional[str], technology: Optional[str], year: Optional[int]) -> Any:
    listing = _get_json("IRENA", IRENA_FOLDER_URL + "/")
    if isinstance(listing, dict) and "error" in listing:
        return listing
    table = next((t.get("id") for t in listing if isinstance(t, dict)
                  and str(t.get("id", "")).startswith("Country_ELECSTAT")), None) if isinstance(listing, list) else None
    if not table:
        return {"error": "IRENA: the country electricity statistics table was not found in IRENASTAT"}
    url = f"{IRENA_FOLDER_URL}/{table}"
    meta = _get_json("IRENA", url)
    if isinstance(meta, dict) and "error" in meta:
        return meta
    variables = {v.get("code"): v for v in meta.get("variables", [])} if isinstance(meta, dict) else {}
    needed = ("Country/area", "Technology", "Data Type", "Grid connection", "Year")
    if not all(k in variables for k in needed):
        return {"error": "IRENA changed the layout of its electricity statistics table"}

    def texts(code):
        v = variables[code]
        return dict(zip(v["values"], v.get("valueTexts", v["values"])))

    countries, techs, years = texts("Country/area"), texts("Technology"), texts("Year")
    if country:
        if country.upper() not in countries:
            return {"error": f"IRENA has no country '{country}' - use an ISO3 code such as USA, DEU or IND"}
        country_sel = {"filter": "item", "values": [country.upper()]}
    else:
        country_sel = {"filter": "all", "values": ["*"]}
    if technology:
        t = technology.strip().lower()
        tech_codes = [c for c, name in techs.items() if t == c or t in name.lower()]
        if not tech_codes:
            return {"error": f"Unknown technology '{technology}'", "available": sorted(techs.values())}
    else:
        tech_codes = [c for c, name in techs.items() if name == "Total Renewable"] or ["1"]
    year_by_text = {name: c for c, name in years.items()}
    if year:
        if str(year) not in year_by_text:
            return {"error": f"IRENA has no capacity data for {year}",
                    "available_years": f"{min(year_by_text)}-{max(year_by_text)}"}
        year_codes = [year_by_text[str(year)]]
    else:
        # PxWeb's "top" filter returns the OLDEST years for this table: ask for the last two explicitly.
        year_codes = [year_by_text[y] for y in sorted(year_by_text)[-2:]]
    capacity_code = next((c for c, name in texts("Data Type").items() if "Capacity" in name), "1")
    all_grid = next((c for c, name in texts("Grid connection").items() if name == "All"), "0")
    query = {"query": [
        {"code": "Country/area", "selection": country_sel},
        {"code": "Technology", "selection": {"filter": "item", "values": tech_codes}},
        {"code": "Data Type", "selection": {"filter": "item", "values": [capacity_code]}},
        {"code": "Grid connection", "selection": {"filter": "item", "values": [all_grid]}},
        {"code": "Year", "selection": {"filter": "item", "values": year_codes}},
    ], "response": {"format": "json"}}
    data = _get_json("IRENA", url, body=query)
    if isinstance(data, dict) and "error" in data:
        return data
    latest: Dict[tuple, Dict] = {}
    for row in (data.get("data") or []) if isinstance(data, dict) else []:
        key, values = row.get("key") or [], row.get("values") or []
        if len(key) < 5 or not values:
            continue
        try:
            value = float(values[0])
        except (TypeError, ValueError):
            continue  # ".." = not available
        item = {"iso3": key[0], "country": countries.get(key[0], key[0]), "technology": techs.get(key[1], key[1]),
                "year": int(years.get(key[4], key[4])), "capacity_mw": value}
        ident = (key[0], key[1])
        if ident not in latest or item["year"] > latest[ident]["year"]:
            latest[ident] = item  # without a year: keep each country's most recent reported value
    rows = sorted(latest.values(), key=lambda r: (r["country"], r["technology"]))
    if not rows:
        return {"error": "IRENA returned no capacity figures for this selection"}
    return rows


def get_renewable_capacity(country: str = None, technology: str = None, year: int = None) -> Any:
    """Return installed renewable power capacity (MW) by country and technology (IRENA)."""
    data = _irena_capacity(country, technology, year)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "technology": technology or "Total Renewable", "year": year,
            "source": "IRENA Renewable Energy Statistics (IRENASTAT)", "unit": "MW",
            "count": len(data), "data": data}


def get_energy_mix(country: str, year: int = None) -> Any:
    """Return total energy supply by source for a country (IEA, TJ)."""
    data = _iea_indicator("TESbySource", country, year)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "source": "IEA World Energy Balances", "energy_mix": data}


def get_co2_intensity(country: str, year: int = None) -> Any:
    """Return the CO2 intensity of a country's total energy supply (IEA, t CO2/TJ)."""
    data = _iea_indicator("CO2Intensity", country, year)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "year": year, "source": "IEA Greenhouse Gas Emissions from Energy",
            "co2_intensity": data}


def get_energy_investment(country: str, sector: str = None, year: int = None) -> Any:
    """Clean energy investment: the IEA publishes it only in its World Energy Investment reports."""
    return {"error": "Clean energy investment data is not available from an open API (the IEA publishes it "
                     "only in its World Energy Investment report downloads)"}


def get_ev_adoption(country: str, year: int = None) -> Any:
    """EV stock / sales share: the IEA Global EV Data Explorer has no open API."""
    return {"error": "EV adoption data is not available from an open API (the IEA Global EV Data Explorer "
                     "offers only manual downloads)"}


def get_countries() -> Any:
    """Return the countries and regions available in the IEA statistics database."""
    data = _get_json("IEA", f"{IEA_STATS_URL}/countries")
    if isinstance(data, dict) and "error" in data:
        return data
    if not isinstance(data, list):
        return {"error": "IEA returned an unexpected response"}
    countries = [{"iso3": c.get("iso3"), "name": c.get("name"), "iea_member": c.get("isMember"),
                  "is_region": c.get("isRegion")} for c in data if isinstance(c, dict)]
    return {"countries": countries, "count": len(countries), "source": "IEA"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "capacity":
        country = args[1] if len(args) > 1 else None
        technology = args[2] if len(args) > 2 else None
        year = int(args[3]) if len(args) > 3 else None
        result = get_renewable_capacity(country, technology, year)
    elif command == "mix":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_energy_mix(country, year)
    elif command == "co2":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_co2_intensity(country, year)
    elif command == "investment":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            sector = args[2] if len(args) > 2 else None
            year = int(args[3]) if len(args) > 3 else None
            result = get_energy_investment(country, sector, year)
    elif command == "ev":
        country = args[1] if len(args) > 1 else ""
        if not country:
            result = {"error": "country required"}
        else:
            year = int(args[2]) if len(args) > 2 else None
            result = get_ev_adoption(country, year)
    elif command == "countries":
        result = get_countries()
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    try:
        main()
    except ValueError as e:  # e.g. a non-numeric year
        print(json.dumps({"error": f"Invalid argument: {e}"}))
