"""
IRENA Renewable Energy Statistics Fetcher
IRENASTAT (keyless PxWeb API): installed electricity capacity (MW) and generation (GWh) by country/region and technology, renewable share of capacity/generation, and public investment in energy by technology.
"""
import sys
import json
import requests
from typing import Dict, Any, List, Optional

BASE_URL = "https://pxweb.irena.org/api/v1/en/IRENASTAT"
# Table ids carry the release (e.g. Country_ELECSTAT_2026_H2_PX.px), so they are discovered by prefix.
TABLES = {
    "country_elec": ("Power Capacity and Generation", "Country_ELECSTAT"),
    "region_elec": ("Power Capacity and Generation", "Region_ELECSTAT"),
    "re_share": ("Power Capacity and Generation", "RE-SHARE"),
    "public_finance": ("Finance", "PUBFIN"),
}
REGION_ALIASES = {"WORLD": "GLO", "GLOBAL": "GLO", "AFRICA": "RAF", "ASIA": "RAS", "EUROPE": "RER",
                  "EURASIA": "REA", "MIDDLE EAST": "RME", "NORTH AMERICA": "RNA", "N AMERICA": "RNA",
                  "SOUTH AMERICA": "RSA", "S AMERICA": "RSA", "OCEANIA": "ROC", "CENTRAL AMERICA": "RCC"}
TECH_ALIASES = {"SOLAR": ["Solar photovoltaic", "Solar energy"], "WIND": ["Onshore wind energy", "Wind energy"],
                "OFFSHORE WIND": ["Offshore wind energy"], "HYDRO": ["Renewable hydropower"],
                "RENEWABLES": ["Total Renewable"], "RENEWABLE": ["Total Renewable"],
                "NUCLEAR": ["Nuclear energy"], "GAS": ["Natural gas"], "COAL": ["Coal"],
                "BIOENERGY": ["Solid biofuels", "Bioenergy"], "GEOTHERMAL": ["Geothermal energy"]}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _get(path: str) -> Any:
    r = session.get(f"{BASE_URL}/{path}", timeout=30)
    r.raise_for_status()
    return r.json()


def _table(key: str) -> str:
    folder, prefix = TABLES[key]
    items = _get(requests.utils.quote(folder))
    for it in items:
        if it.get("type") == "t" and it.get("id", "").startswith(prefix):
            return f"{requests.utils.quote(folder)}/{it['id']}"
    raise ValueError(f"IRENASTAT table {prefix} not found")


def _match(var: Dict, wanted: Optional[str], aliases: Dict[str, List[str]] = None) -> List[str]:
    """Resolve a user value (code, alias or case-insensitive name fragment; comma-separated) to codes."""
    if not wanted:
        return []
    codes = []
    for w in [x.strip() for x in wanted.split(",") if x.strip()]:
        names = (aliases or {}).get(w.upper())
        vals = list(zip(var["values"], var["valueTexts"]))
        hit = None
        if names:
            hit = next((c for n in names for c, t in vals if t.lower() == n.lower()), None)
        if hit is None:
            hit = next((c for c, t in vals if c.upper() == w.upper() or t.lower() == w.lower()), None)
        if hit is None:
            hit = next((c for c, t in vals if w.lower() in t.lower()), None)
        if hit is None:
            raise ValueError(f"'{w}' is not a valid {var['code']}. Options: {var['valueTexts'][:40]}")
        codes.append(hit)
    return codes


def _query(key: str, selections: Dict[str, Any]) -> Any:
    """selections: {variable code: user value or None (=all)} plus {"Year": "2020-2023"|"2023"|None}."""
    try:
        path = _table(key)
        meta = _get(path)
        variables = {v["code"]: v for v in meta["variables"]}
        query = []
        for code, wanted in selections.items():
            var = variables.get(code)
            if var is None:
                continue
            if code == "Year":
                if not wanted:
                    vals = var["values"][-10:]
                elif "-" in str(wanted):
                    a, b = str(wanted).split("-", 1)
                    vals = [c for c, t in zip(var["values"], var["valueTexts"]) if int(a) <= int(t) <= int(b)]
                else:
                    vals = [c for c, t in zip(var["values"], var["valueTexts"]) if t in str(wanted).split(",")]
                if not vals:
                    return {"error": f"Year {wanted} not available (data covers {var['valueTexts'][0]}-{var['valueTexts'][-1]})"}
            else:
                vals = _match(var, wanted[0], wanted[1]) if isinstance(wanted, tuple) else _match(var, wanted)
                if not vals:
                    continue
            query.append({"code": code, "selection": {"filter": "item", "values": vals}})
        r = session.post(f"{BASE_URL}/{path}", json={"query": query, "response": {"format": "json"}}, timeout=60)
        if r.status_code == 403:
            return {"error": "IRENASTAT refused the query (too many cells) - narrow the selection"}
        r.raise_for_status()
        data = r.json()
    except ValueError as e:
        return {"error": str(e)}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    texts = {code: dict(zip(v["values"], v["valueTexts"])) for code, v in variables.items()}
    cols = [c["code"] for c in data.get("columns", [])]
    dims = [c for c in cols if c in texts]
    rows = []
    for rec in data.get("data", []):
        row = {d: texts[d].get(k, k) for d, k in zip(dims, rec.get("key", []))}
        try:
            row["value"] = float(rec["values"][0])
        except (ValueError, TypeError, IndexError):
            continue  # ".." = not available
        rows.append(row)
    return {"table": meta.get("title"), "count": len(rows), "data": rows, "source": "IRENA IRENASTAT"}


def _region_or_country(country: str):
    c = (country or "World").strip()
    code = REGION_ALIASES.get(c.upper(), c)
    return ("region_elec", "Region", code) if code.upper() in REGION_ALIASES.values() else ("country_elec", "Country/area", c)


def _elec(country: str, technology: str, year: str, data_type: str) -> Any:
    table, dim, code = _region_or_country(country)
    sel = {dim: code, "Technology": (technology, TECH_ALIASES), "Data Type": data_type, "Year": year}
    if table == "country_elec":
        sel["Grid connection"] = "All"
    return _query(table, sel)


def get_capacity(country: str = "World", technology: str = "Solar", year: str = None) -> Any:
    return _elec(country, technology, year, "Capacity")


def get_generation(country: str = "World", source: str = "Solar", year: str = None) -> Any:
    return _elec(country, source, year, "Generation")


def get_re_share(country: str = "World", year: str = None) -> Any:
    code = REGION_ALIASES.get((country or "World").upper(), country)
    return _query("re_share", {"Region/country/area": code, "Year": year})


def get_investment(country: str = "IND", technology: str = None, year: str = None) -> Any:
    return _query("public_finance", {"Country/area": country, "Technology": (technology, TECH_ALIASES), "Year": year})


def get_countries() -> Any:
    try:
        meta = _get(_table("country_elec"))
    except (requests.exceptions.RequestException, ValueError) as e:
        return {"error": str(e)}
    var = next(v for v in meta["variables"] if v["code"] == "Country/area")
    tech = next(v for v in meta["variables"] if v["code"] == "Technology")
    return {"countries": [{"code": c, "name": t} for c, t in zip(var["values"], var["valueTexts"])],
            "regions": REGION_ALIASES, "technologies": tech["valueTexts"]}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "capacity":
        country = args[1] if len(args) > 1 else "World"
        technology = args[2] if len(args) > 2 else "Solar"
        year = args[3] if len(args) > 3 else None
        result = get_capacity(country, technology, year)
    elif command == "generation":
        country = args[1] if len(args) > 1 else "World"
        source = args[2] if len(args) > 2 else "Solar"
        year = args[3] if len(args) > 3 else None
        result = get_generation(country, source, year)
    elif command == "re_share":
        country = args[1] if len(args) > 1 else "World"
        year = args[2] if len(args) > 2 else None
        result = get_re_share(country, year)
    elif command == "investment":
        country = args[1] if len(args) > 1 else "IND"
        technology = args[2] if len(args) > 2 else None
        year = args[3] if len(args) > 3 else None
        result = get_investment(country, technology, year)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
