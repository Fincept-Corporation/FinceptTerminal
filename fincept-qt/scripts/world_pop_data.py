"""
WorldPop Data Fetcher
High-resolution population grids, demographic estimates, and urbanization data
from the University of Southampton WorldPop project REST API.
The API (hub.worldpop.org/rest/data) is a catalogue: each record describes one GeoTIFF dataset (id, year,
country, DOI, download links); the grids themselves are not returned as numbers.
"""
import re
import sys
import json
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

BASE_URL = "https://hub.worldpop.org/rest/data"
SERVICE = "WorldPop"

session = fc.http_session()
session.headers.update({"User-Agent": "Mozilla/5.0 (FinceptTerminal; +https://fincept.in)",
                        "Accept": "application/json"})

# <project alias>/<dataset alias> paths of the catalogue (GET /rest/data and /rest/data/<project>)
GRID_DATASETS = {"100m": "pop/wpgp", "1km": "pop/wpic1km"}      # unconstrained counts 2000-2020
AGE_DATASET = "age_structures/aswpgp"                           # age/sex structures 2000-2020, 100m
URBAN_DATASET = "dug/dug_g2_v1"                                 # Degree of Urbanisation 2015-2030
RECORD_FIELDS = ["id", "title", "popyear", "country", "iso3", "category", "data_format", "doi", "date",
                 "url_summary", "license"]


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=(10, 40))
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        # e.g. HTTP 500: the API answers unknown parameters (the old popyear/resolution filters) with a 500 page
        return {"error": fc.request_error(SERVICE, e)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    if not isinstance(data, dict) or "data" not in data:
        return {"error": f"{SERVICE} returned an unexpected response"}
    return data["data"]


def _iso3(country: Optional[str]) -> Optional[str]:
    c = (country or "").strip().upper()
    if c and not re.fullmatch(r"[A-Z]{3}", c):
        raise ValueError(f"country must be an ISO3 code such as NPL or USA, got '{country}'")
    return c or None


def _record(r: Dict) -> Dict:
    out = {k: r.get(k) for k in RECORD_FIELDS if k in r}
    if "popyear" in out:  # the year arrives as text ("2020"); the dataset "id" stays text
        out["popyear"] = fc.number(out["popyear"])
    files = r.get("files")
    if isinstance(files, str):  # the API serialises the list as a Python repr string
        files = re.findall(r"https?://[^'\"\s,\]]+", files)
    if files:
        out["files"] = files
    return out


def _datasets(path: str, country: Optional[str], year: Optional[str]) -> Any:
    """Catalogue records of one dataset, for one country (full records) or all countries (id/title/year/iso3)."""
    try:
        iso3 = _iso3(country)
    except ValueError as e:
        return {"error": str(e)}
    rows = _make_request(path, {"iso3": iso3} if iso3 else None)
    if isinstance(rows, dict):
        return rows
    if year:
        rows = [r for r in rows if str(r.get("popyear") or "") == str(year)]
    records = [_record(r) for r in rows]
    if not records:
        return {"error": f"{SERVICE}: no {path} datasets" + (f" for {iso3}" if iso3 else "")
                         + (f" in {year}" if year else "")}
    return {"dataset": path, "country": iso3, "year": year, "count": len(records), "records": records}


def get_population_grid(country: str = None, year: str = None, resolution: str = "100m") -> Any:
    path = GRID_DATASETS.get((resolution or "100m").lower())
    if not path:
        return {"error": f"resolution must be one of {list(GRID_DATASETS)}"}
    # without a country the listing covers every country, so default to one year to keep it short
    return _datasets(path, country, year or (None if country else "2020"))


def get_urban_population(country: str = None, year: str = None) -> Any:
    return _datasets(URBAN_DATASET, country, year)


def get_demographic_estimates(country: str = None, year: str = None) -> Any:
    return _datasets(AGE_DATASET, country, year or (None if country else "2020"))


def get_available_datasets(country: str = None) -> Any:
    """The catalogue's projects and the population-count datasets they contain."""
    projects = _make_request("")
    if isinstance(projects, dict):
        return projects
    pop = _make_request("pop")
    out = {"projects": [{"alias": p.get("alias"), "name": p.get("name")} for p in projects],
           "population_datasets": [] if isinstance(pop, dict) else
           [{"alias": p.get("alias"), "name": p.get("name")} for p in pop if p.get("alias")]}
    if country:
        grid = _datasets(GRID_DATASETS["100m"], country, None)
        out["country"] = country
        out["country_population_grids"] = grid.get("records", grid)
    return out


def get_file_metadata(file_id: str) -> Any:
    if not str(file_id).isdigit():
        return {"error": f"file_id must be a numeric WorldPop dataset id, got '{file_id}'"}
    rec = _make_request("pop/wpgp", {"id": file_id})   # id lookups are global across the catalogue
    if isinstance(rec, dict) and "error" in rec:
        return rec
    if not rec:
        return {"error": f"{SERVICE}: no dataset with id {file_id}"}
    out = _record(rec)
    out["desc"] = re.sub(r"<[^>]+>|&nbsp;", " ", rec.get("desc") or "").strip()
    out["citation"] = rec.get("citation")
    return out


def get_countries() -> Any:
    rows = _make_request(GRID_DATASETS["1km"])
    if isinstance(rows, dict):
        return rows
    seen = {}
    for r in rows:
        if r.get("iso3"):
            seen.setdefault(r["iso3"], re.sub(r"^.* in \d{4},?\s*", "", r.get("title") or "").strip())
    countries = [{"iso3": k, "name": v} for k, v in sorted(seen.items())]
    return {"count": len(countries), "countries": countries}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "grid":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        resolution = args[3] if len(args) > 3 else "100m"
        result = get_population_grid(country, year, resolution)
    elif command == "urban":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_urban_population(country, year)
    elif command == "demographic":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_demographic_estimates(country, year)
    elif command == "datasets":
        country = args[1] if len(args) > 1 else None
        result = get_available_datasets(country)
    elif command == "metadata":
        if len(args) < 2:
            result = {"error": "file_id required"}
        else:
            result = get_file_metadata(args[1])
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
