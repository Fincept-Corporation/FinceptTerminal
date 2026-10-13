"""
World Bank Climate Change Knowledge Portal Data Fetcher
CMIP6 temperature/precipitation projections, climate-hazard indicators and ERA5 history by country
via the World Bank Climate Change Knowledge Portal API (cckpapi, no key required).
"""
import sys
import json
import requests
from typing import Dict, Any

# The old climateknowledgeportal.worldbank.org/api (CRU/CMIP5 "rcp" data) answers HTTP 403; the
# Climate Change Knowledge Portal now serves CMIP6 projections and ERA5 history from cckpapi.
BASE_URL = "https://cckpapi.worldbank.org/cckp/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json", "User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})

SCENARIOS = ["ssp126", "ssp245", "ssp370", "ssp585"]
RCP_TO_SSP = {"rcp26": "ssp126", "rcp45": "ssp245", "rcp60": "ssp370", "rcp85": "ssp585"}
PERIODS = ["2020-2039", "2040-2059", "2060-2079", "2080-2099"]
VARIABLES = ["tas", "pr", "tasmax", "tasmin"]
UNITS = {"tas": "degC", "tasmax": "degC", "tasmin": "degC", "pr": "mm/year", "hd35": "days above 35 degC",
         "rx5day": "mm (max 5-day precipitation)", "cdd": "consecutive dry days", "fd": "frost days",
         "txx": "degC (hottest day)"}
HAZARDS = {"heatwave": "hd35", "heat": "hd35", "flood": "rx5day", "drought": "cdd", "cold": "fd",
           "hottest_day": "txx"}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params or {"_format": "json"}, timeout=60)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _cckp(code: str, country: str) -> Any:
    data = _make_request(f"{code}/{country.upper()}")
    if isinstance(data, dict) and "error" in data:
        return data
    series = data.get("data", {}).get(country.upper()) if isinstance(data.get("data"), dict) else None
    if not series:
        return {"error": f"No CCKP data for {country.upper()} ({code})"}
    return series


def _scenario(s: str) -> str:
    s = (s or "ssp585").lower()
    return RCP_TO_SSP.get(s, s)


def _period(p: str) -> str:
    p = (p or "2040-2059").replace("_", "-")
    return p if p in PERIODS else "2040-2059"


def _projection(variable: str, country: str, scenario: str, period: str) -> Any:
    sc, pe = _scenario(scenario), _period(period)
    proj = _cckp(f"cmip6-x0.25_climatology_{variable}_climatology_annual_{pe}_median_{sc}_ensemble_all_mean",
                 country)
    if isinstance(proj, dict) and "error" in proj:
        return proj
    base = _cckp(f"era5-x0.25_climatology_{variable}_climatology_annual_1991-2020_mean_historical_era5_x0.25_mean",
                 country)
    proj_v = next(iter(proj.values()), None)
    base_v = next(iter(base.values()), None) if isinstance(base, dict) and "error" not in base else None
    out = {"country": country.upper(), "variable": variable, "unit": UNITS.get(variable), "scenario": sc,
           "period": pe, "projected_mean": proj_v, "baseline_1991_2020": base_v,
           "source": "World Bank CCKP (CMIP6 multi-model median, ERA5 baseline)"}
    if proj_v is not None and base_v is not None:
        out["change"] = round(proj_v - base_v, 2)
    return out


def get_temperature_projection(country: str = "USA", scenario: str = "ssp585",
                               period: str = "2040-2059") -> Any:
    return _projection("tas", country, scenario, period)


def get_precipitation_projection(country: str = "USA", scenario: str = "ssp585",
                                 period: str = "2040-2059") -> Any:
    return _projection("pr", country, scenario, period)


def get_climate_risk_scores(country: str = "USA", scenario: str = "ssp585", period: str = "2040-2059") -> Any:
    """Projected change of climate-hazard indicators (hot days, extreme rain, dry spells, frost days)."""
    out = {}
    for name, var in (("hot_days_35c", "hd35"), ("max_5day_precip", "rx5day"), ("consecutive_dry_days", "cdd"),
                      ("frost_days", "fd"), ("hottest_day", "txx")):
        out[name] = _projection(var, country, scenario, period)
    return {"country": country.upper(), "scenario": _scenario(scenario), "period": _period(period),
            "indicators": out}


def get_baseline_climate(country: str = "USA", variable: str = "tas", month: int = 0) -> Any:
    """ERA5 1991-2020 monthly climatology (month 1-12 for one month, 0 for all twelve)."""
    if variable not in VARIABLES:
        return {"error": f"Unknown variable '{variable}'. Options: {VARIABLES}"}
    series = _cckp(f"era5-x0.25_climatology_{variable}_climatology_monthly_1991-2020_mean_historical_era5_x0.25_mean",
                   country)
    if isinstance(series, dict) and "error" in series:
        return series
    months = [{"month": int(k[5:7]), "value": v} for k, v in sorted(series.items())]
    if month:
        months = [m for m in months if m["month"] == month]
    unit = "mm/month" if variable == "pr" else UNITS.get(variable)
    return {"country": country.upper(), "variable": variable, "unit": unit, "baseline_period": "1991-2020",
            "source": "World Bank CCKP (ERA5)", "data": months}


def get_extreme_events(country: str = "USA", hazard: str = "heatwave") -> Any:
    """Observed annual hazard indicator 1950-2023 (ERA5): heatwave, flood, drought, cold, hottest_day."""
    var = HAZARDS.get(hazard.lower())
    if not var:
        return {"error": f"Unknown hazard '{hazard}'. Options: {sorted(HAZARDS)}"}
    series = _cckp(f"era5-x0.25_timeseries_{var}_timeseries_annual_1950-2023_mean_historical_era5_x0.25_mean",
                   country)
    if isinstance(series, dict) and "error" in series:
        return series
    return {"country": country.upper(), "hazard": hazard, "indicator": var, "unit": UNITS.get(var),
            "source": "World Bank CCKP (ERA5)",
            "data": [{"year": int(k[:4]), "value": v} for k, v in sorted(series.items())]}


def get_countries() -> Any:
    url = "https://api.worldbank.org/v2/country"
    params = {"format": "json", "per_page": 300}
    data = _make_request(url, params)
    if isinstance(data, list) and len(data) > 1:
        countries = [{"id": c.get("id"), "name": c.get("name"),
                      "region": c.get("region", {}).get("value"),
                      "income_level": c.get("incomeLevel", {}).get("value")}
                     for c in data[1] if c.get("id")]
        return {"count": len(countries), "countries": countries}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "temperature":
        country = args[1] if len(args) > 1 else "USA"
        scenario = args[2] if len(args) > 2 else "ssp585"
        period = args[3] if len(args) > 3 else "2040-2059"
        result = get_temperature_projection(country, scenario, period)
    elif command == "precipitation":
        country = args[1] if len(args) > 1 else "USA"
        scenario = args[2] if len(args) > 2 else "ssp585"
        period = args[3] if len(args) > 3 else "2040-2059"
        result = get_precipitation_projection(country, scenario, period)
    elif command == "risk":
        country = args[1] if len(args) > 1 else "USA"
        scenario = args[2] if len(args) > 2 else "ssp585"
        period = args[3] if len(args) > 3 else "2040-2059"
        result = get_climate_risk_scores(country, scenario, period)
    elif command == "baseline":
        country = args[1] if len(args) > 1 else "USA"
        variable = args[2] if len(args) > 2 else "tas"
        month = int(args[3]) if len(args) > 3 else 0
        result = get_baseline_climate(country, variable, month)
    elif command == "extremes":
        country = args[1] if len(args) > 1 else "USA"
        hazard = args[2] if len(args) > 2 else "heatwave"
        result = get_extreme_events(country, hazard)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
