"""
Global Forest Watch Data Fetcher
Country-level tree cover loss and its emissions, integrated deforestation alerts, forest carbon stocks/flux,
protected areas and VIIRS fire alerts via the Global Forest Watch Data API (free API key: GFW_API_KEY).
"""
import re
import sys
import json
import os
import requests
from datetime import date, timedelta
from typing import Dict, Any

API_KEY = os.environ.get('GFW_API_KEY', '')
BASE_URL = "https://data-api.globalforestwatch.org"
CANOPY_THRESHOLD = 30  # % tree-cover density used by GFW's country dashboards

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _iso(country: str) -> str:
    c = (country or "BRA").strip().upper()
    if not re.fullmatch(r"[A-Z]{3}", c):
        raise ValueError("country must be an ISO-3 code such as BRA, IDN, COD")
    return c


def _day(d: str, default: date) -> str:
    if not d:
        return default.isoformat()
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}", d):
        raise ValueError("dates must be YYYY-MM-DD")
    return d


def _query(dataset: str, sql: str) -> Any:
    """SQL query against the latest version of a GFW tabular dataset (the table is always `data`)."""
    if not API_KEY:
        return {"error": "The Global Forest Watch Data API requires a free API key: see "
                         "https://data-api.globalforestwatch.org/#tag/Authentication and save it as GFW_API_KEY "
                         "in Settings > Credentials"}
    url = f"{BASE_URL}/dataset/{dataset}/latest/query/json"
    try:
        response = session.get(url, params={"sql": sql}, headers={"x-api-key": API_KEY}, timeout=60)
        data = response.json()
        if response.status_code >= 400 or data.get("status") in ("failed", "error"):
            return {"error": f"GFW API: {data.get('message') or response.status_code}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    rows = data.get("data", [])
    return {"dataset": dataset, "count": len(rows), "data": rows}


def _safe(fn):
    def wrapper(*a, **kw):
        try:
            return fn(*a, **kw)
        except ValueError as e:
            return {"error": str(e)}
    return wrapper


@_safe
def get_tree_cover_loss(country: str = "BRA", year: str = None) -> Any:
    iso = _iso(country)
    where = f"iso = '{iso}' AND umd_tree_cover_density_2000__threshold = {CANOPY_THRESHOLD}"
    if year:
        where += f" AND umd_tree_cover_loss__year = {int(year)}"
    sql = ("SELECT umd_tree_cover_loss__year AS year, SUM(umd_tree_cover_loss__ha) AS tree_cover_loss_ha, "
           "SUM(umd_tree_cover_loss_from_fires__ha) AS loss_from_fires_ha, "
           "SUM(gfw_gross_emissions_co2e_all_gases__Mg) AS gross_emissions_co2e_mg "
           f"FROM data WHERE {where} GROUP BY umd_tree_cover_loss__year ORDER BY umd_tree_cover_loss__year")
    return {"country": iso, "canopy_threshold_pct": CANOPY_THRESHOLD, **_query("gadm__tcl__iso_change", sql)}


@_safe
def get_deforestation_alerts(country: str = "BRA", start_date: str = None, end_date: str = None) -> Any:
    iso = _iso(country)
    start = _day(start_date, date.today() - timedelta(days=30))
    end = _day(end_date, date.today())
    sql = ("SELECT gfw_integrated_alerts__date AS date, SUM(alert__count) AS alerts, SUM(alert_area__ha) AS area_ha "
           f"FROM data WHERE iso = '{iso}' AND gfw_integrated_alerts__date >= '{start}' "
           f"AND gfw_integrated_alerts__date <= '{end}' GROUP BY gfw_integrated_alerts__date "
           "ORDER BY gfw_integrated_alerts__date")
    return {"country": iso, "start": start, "end": end, **_query("gadm__integrated_alerts__iso_daily_alerts", sql)}


@_safe
def get_forest_carbon(country: str = "BRA", year: str = None) -> Any:
    iso = _iso(country)
    sql = ("SELECT SUM(umd_tree_cover_extent_2000__ha) AS tree_cover_2000_ha, "
           "SUM(gfw_aboveground_carbon_stocks_2000__Mg_C) AS aboveground_carbon_2000_mg_c, "
           "SUM(gfw_belowground_carbon_stocks_2000__Mg_C) AS belowground_carbon_2000_mg_c, "
           "SUM(gfw_soil_carbon_stocks_2000__Mg_C) AS soil_carbon_2000_mg_c, "
           "SUM(gfw_gross_emissions_co2e_all_gases__Mg) AS gross_emissions_co2e_mg, "
           "SUM(gfw_full_extent_gross_removals__Mg_CO2) AS gross_removals_co2_mg, "
           "SUM(gfw_net_flux_co2e__Mg) AS net_flux_co2e_mg "
           f"FROM data WHERE iso = '{iso}' AND umd_tree_cover_density_2000__threshold = {CANOPY_THRESHOLD}")
    res = _query("gadm__tcl__iso_summary", sql)
    if year:
        res["note"] = "carbon stocks are for 2000; emissions/removals/net flux are 2001-latest totals"
    return {"country": iso, "canopy_threshold_pct": CANOPY_THRESHOLD, **res}


@_safe
def get_protected_areas(country: str = "BRA") -> Any:
    iso = _iso(country)
    sql = ("SELECT name, desig_eng, iucn_cat, status, status_yr, gov_type, gis_area FROM data "
           f"WHERE iso3 = '{iso}' ORDER BY gis_area DESC LIMIT 100")
    return {"country": iso, "note": "largest 100 protected areas (gis_area in km2)",
            **_query("wdpa_protected_areas", sql)}


@_safe
def get_fires_data(country: str = "BRA", start_date: str = None, end_date: str = None) -> Any:
    iso = _iso(country)
    start = date.fromisoformat(_day(start_date, date.today() - timedelta(days=365)))
    end = date.fromisoformat(_day(end_date, date.today()))
    sql = ("SELECT alert__year AS year, alert__week AS week, SUM(alert__count) AS fire_alerts FROM data "
           f"WHERE iso = '{iso}' AND alert__year >= {start.year} AND alert__year <= {end.year} "
           "GROUP BY alert__year, alert__week ORDER BY alert__year, alert__week")
    return {"country": iso, "start_year": start.year, "end_year": end.year,
            **_query("gadm__viirs__iso_weekly_alerts", sql)}


def get_datasets() -> Any:
    """GFW datasets (keyless)."""
    try:
        r = session.get(f"{BASE_URL}/datasets", params={"page[size]": 200}, timeout=60)
        r.raise_for_status()
        data = r.json().get("data", [])
    except (requests.exceptions.RequestException, ValueError) as e:
        return {"error": f"Request failed: {str(e)}"}
    return {"count": len(data), "datasets": [{"dataset": d.get("dataset"),
                                              "title": (d.get("metadata") or {}).get("title")} for d in data]}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "cover_loss":
        country = args[1] if len(args) > 1 else "BRA"
        year = args[2] if len(args) > 2 else None
        result = get_tree_cover_loss(country, year)
    elif command == "alerts":
        country = args[1] if len(args) > 1 else "BRA"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_deforestation_alerts(country, start_date, end_date)
    elif command == "carbon":
        country = args[1] if len(args) > 1 else "BRA"
        year = args[2] if len(args) > 2 else None
        result = get_forest_carbon(country, year)
    elif command == "protected":
        country = args[1] if len(args) > 1 else "BRA"
        result = get_protected_areas(country)
    elif command == "fires":
        country = args[1] if len(args) > 1 else "BRA"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_fires_data(country, start_date, end_date)
    elif command == "datasets":
        result = get_datasets()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
