"""
IEA (International Energy Agency) Data Fetcher
IEA world energy statistics (free tier): total energy supply by source, final consumption, electricity generation by fuel and CO2 emissions by sector, for 150+ countries (no key).
Uses the keyless JSON endpoints behind the IEA Energy Statistics Data Browser (api.iea.org/stats/indicator); they are not a documented API and may change.
"""
import sys
import json
import requests
from datetime import datetime
from typing import Dict, Any, List

BASE_URL = "https://api.iea.org/stats/indicator"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json", "User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})

# Keyword filters applied to the product / flow labels the indicators return.
SUPPLY_PRODUCTS = {
    "oil": ["MTOTOIL"], "natural_gas": ["NATGAS"], "coal": ["COAL"], "nuclear": ["NUCLEAR"],
    "hydro": ["HYDRO"], "renewables": ["HYDRO", "GEOTHERM", "COMRENEW"], "biofuels": ["COMRENEW"],
}
ELEC_SOURCES = {
    "coal": ["EHCOAL"], "oil": ["EHOIL"], "natural_gas": ["EHNATGAS"], "gas": ["EHNATGAS"],
    "nuclear": ["EHNUCLEAR"], "hydro": ["EHYDRO"], "solar": ["ESOLARPV", "EHSOLARTH"], "wind": ["EWIND"],
    "biofuels": ["EHBIOMASS", "EHWASTE"], "geothermal": ["EHGEOTHERM"],
    "renewables": ["EHYDRO", "ESOLARPV", "EHSOLARTH", "EWIND", "EHBIOMASS", "EHGEOTHERM"],
}
CO2_SECTORS = {
    "power": ["ELECHEAT"], "transport": ["TOTTRANS"], "industry": ["TOTIND"], "residential": ["RESIDENT"],
    "commercial": ["COMMPUB"], "agriculture": ["AGRICULT"], "energy_industries": ["OTHEN"],
}


def _indicator(name: str, country: str, year: int = None) -> Any:
    start = year if year else datetime.now().year - 5
    try:
        response = session.get(f"{BASE_URL}/{name}", params={"countries": country.upper(), "startYear": start},
                               timeout=30)
        if response.status_code == 404 or response.text.startswith("Indicator not found"):
            return {"error": f"IEA indicator {name} not available"}
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if isinstance(data, list):
        # an unknown country code makes the endpoint return every country - keep only the requested one
        data = [r for r in data if country.upper() in (str(r.get("country", "")).upper(), str(r.get("short", "")).upper())]
    if not isinstance(data, list) or not data:
        return {"error": f"No IEA data for {country.upper()} ({name}) - use an ISO-3 country code such as USA, DEU, IND"}
    if year:
        data = [r for r in data if str(r.get("year")) == str(year)]
        if not data:
            return {"error": f"No IEA {name} data for {country.upper()} in {year}"}
    return data


def _rows(data: List[Dict], key: str, codes: List[str] = None) -> List[Dict]:
    out = []
    for r in data:
        if codes and r.get(key) not in codes:
            continue
        label = r.get("productLabel") if key == "product" else r.get("flowLabel")
        out.append({"year": int(r["year"]), "code": r.get(key), "label": label, "value": r.get("value"),
                    "unit": r.get("units")})
    out.sort(key=lambda x: (x["year"], x["label"] or ""))
    return out


def _wrap(meta: Dict, data: Any, key: str, codes: List[str] = None) -> Any:
    if isinstance(data, dict):
        return {**meta, **data}
    rows = _rows(data, key, codes)
    return {**meta, "source": "IEA World Energy Statistics", "count": len(rows), "data": rows}


def get_energy_supply(country: str = "USA", product: str = "total", year: int = None) -> Any:
    """Total energy supply (TJ) by source."""
    codes = SUPPLY_PRODUCTS.get(product.lower())
    return _wrap({"country": country.upper(), "product": product, "flow": "total energy supply"},
                 _indicator("TESbySource", country, year), "product", codes)


def get_electricity_generation(country: str = "USA", source: str = "total", year: int = None) -> Any:
    """Electricity generation (GWh) by fuel."""
    codes = ELEC_SOURCES.get(source.lower())
    return _wrap({"country": country.upper(), "source": source},
                 _indicator("ElecGenByFuel", country, year), "flow", codes)


def get_co2_emissions(country: str = "USA", sector: str = "total", year: int = None) -> Any:
    """CO2 emissions from fuel combustion (Mt CO2) by sector."""
    codes = CO2_SECTORS.get(sector.lower())
    return _wrap({"country": country.upper(), "sector": sector},
                 _indicator("CO2BySector", country, year), "flow", codes)


def get_renewable_capacity(country: str = "USA", technology: str = "all", year: int = None) -> Any:
    """Renewable electricity generation (GWh) by technology (IEA free data has no capacity series)."""
    codes = ELEC_SOURCES.get(technology.lower()) if technology.lower() != "all" else ELEC_SOURCES["renewables"]
    out = _wrap({"country": country.upper(), "technology": technology, "measure": "electricity generation (GWh)"},
                _indicator("ElecGenByFuel", country, year), "flow", codes)
    return out


def get_final_consumption(country: str = "USA", year: int = None) -> Any:
    """Total final consumption (TJ) by sector."""
    return _wrap({"country": country.upper()}, _indicator("TFCShareBySector", country, year), "flow")


def get_energy_indicators(country: str = "USA", year: int = None) -> Any:
    """Headline totals per year: total energy supply, total final consumption, CO2, electricity generation."""
    out: Dict[str, Dict] = {}
    for name, ind, unit in (("total_energy_supply_tj", "TESbySource", "TJ"),
                            ("final_consumption_tj", "TFCShareBySector", "TJ"),
                            ("co2_mt", "CO2BySector", "MtCO2"),
                            ("electricity_generation_gwh", "ElecGenByFuel", "GWh")):
        data = _indicator(ind, country, year)
        if isinstance(data, dict):
            out[name] = data
            continue
        totals: Dict[str, float] = {}
        for r in data:
            if r.get("value") is not None:
                totals[r["year"]] = totals.get(r["year"], 0.0) + float(r["value"])
        out[name] = {"unit": unit, "by_year": {y: round(v, 3) for y, v in sorted(totals.items())}}
    return {"country": country.upper(), "source": "IEA World Energy Statistics", "indicators": out}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "supply":
        country = args[1] if len(args) > 1 else "USA"
        product = args[2] if len(args) > 2 else "total"
        year = int(args[3]) if len(args) > 3 else None
        result = get_energy_supply(country, product, year)
    elif command == "electricity":
        country = args[1] if len(args) > 1 else "USA"
        source = args[2] if len(args) > 2 else "total"
        year = int(args[3]) if len(args) > 3 else None
        result = get_electricity_generation(country, source, year)
    elif command == "co2":
        country = args[1] if len(args) > 1 else "USA"
        sector = args[2] if len(args) > 2 else "total"
        year = int(args[3]) if len(args) > 3 else None
        result = get_co2_emissions(country, sector, year)
    elif command == "renewables":
        country = args[1] if len(args) > 1 else "USA"
        technology = args[2] if len(args) > 2 else "all"
        year = int(args[3]) if len(args) > 3 else None
        result = get_renewable_capacity(country, technology, year)
    elif command == "consumption":
        country = args[1] if len(args) > 1 else "USA"
        year = int(args[2]) if len(args) > 2 else None
        result = get_final_consumption(country, year)
    elif command == "indicators":
        country = args[1] if len(args) > 1 else "USA"
        year = int(args[2]) if len(args) > 2 else None
        result = get_energy_indicators(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
