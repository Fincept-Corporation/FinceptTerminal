"""
Inter-American Development Bank (IADB) Data Fetcher
IDB lending operations in Latin America and the Caribbean - projects, approvals by country/sector/year, sectors and countries - from the IDB Open Data portal's projects dataset (CKAN datastore API, no key).
"""
import re
import sys
import json
import requests
from typing import Dict, Any

# api.iadb.org no longer exists; the projects list is published on data.iadb.org (CKAN).
BASE_URL = "https://data.iadb.org/api/3/action"
PROJECTS_RESOURCE = "814b7b54-477a-4c25-b3bf-6be05412069d"  # "EN - IDB Projects List" (idb-projects-dataset)
FIELDS = ["oper_num", "oper_nm", "apprvl_dt", "sign_dt", "cntry_cd", "cntry_nm", "publc_sts_nm", "sector_cd",
          "sector_nm", "subsector_nm", "opertyp_nm", "orig_apprvd_useq_amnt", "totl_cost_orig", "lending_typ_nm",
          "modality_nm", "envmntl_clssfctn_cd"]

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.headers.update({"User-Agent": "Mozilla/5.0 (FinceptTerminal/4.5; +https://fincept.in)"})


def _make_request(action: str, params: Dict) -> Any:
    try:
        response = session.get(f"{BASE_URL}/{action}", params=params, timeout=60)
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if not data.get("success"):
        err = data.get("error", {})
        return {"error": f"IDB open data: {err.get('message') or err}"}
    return data["result"]


def _sql(sql: str) -> Any:
    res = _make_request("datastore_search_sql", {"sql": sql})
    return res if "error" in res else res.get("records", [])


def _code(v: str) -> str:
    v = (v or "").strip().upper()
    if not re.fullmatch(r"[A-Z0-9\-]{1,12}", v):
        raise ValueError(f"invalid code '{v}'")
    return v


def _num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def _project(r: Dict) -> Dict:
    out = {k: r.get(k) for k in FIELDS}
    out["orig_apprvd_useq_amnt"] = _num(out["orig_apprvd_useq_amnt"])
    out["totl_cost_orig"] = _num(out["totl_cost_orig"])
    for k in ("apprvl_dt", "sign_dt"):
        out[k] = (out[k] or "")[:10] or None
    return out


def get_projects(country: str = None, sector: str = None, status: str = None, limit: int = 50) -> Any:
    """Most recently approved projects, optionally by IDB country code (see `countries`; e.g. BR, ME = Mexico), sector code and status."""
    filters = {}
    try:
        if country:
            filters["cntry_cd"] = _code(country)
        if sector:
            filters["sector_cd"] = _code(sector)
    except ValueError as e:
        return {"error": str(e)}
    params = {"resource_id": PROJECTS_RESOURCE, "limit": max(1, min(int(limit), 500)), "sort": "apprvl_dt desc"}
    if status:
        filters["publc_sts_nm"] = status.strip().title()
    if filters:
        params["filters"] = json.dumps(filters)
    res = _make_request("datastore_search", params)
    if "error" in res:
        return res
    rows = [_project(r) for r in res.get("records", [])]
    return {"total": res.get("total"), "count": len(rows), "projects": rows,
            "note": "orig_apprvd_useq_amnt = IDB approved amount (USD)"}


def get_project_details(project_id: str) -> Any:
    try:
        pid = _code(project_id)
    except ValueError as e:
        return {"error": str(e)}
    res = _make_request("datastore_search", {"resource_id": PROJECTS_RESOURCE, "filters": json.dumps({"oper_num": pid})})
    if "error" in res:
        return res
    recs = res.get("records", [])
    if not recs:
        return {"error": f"Project {pid} not found"}
    r = recs[0]
    out = _project(r)
    out["objective"] = r.get("objtv")
    return out


def get_approvals(country: str = None, start_year: str = None) -> Any:
    """IDB approvals (count and USD amount) per year, optionally for one country (aggregated client-side:
    the portal's SQL endpoint does not allow CAST/aggregate helpers)."""
    params = {"resource_id": PROJECTS_RESOURCE, "limit": 32000, "fields": "apprvl_dt,orig_apprvd_useq_amnt"}
    try:
        if country:
            params["filters"] = json.dumps({"cntry_cd": _code(country)})
        first = int(start_year) if start_year else 0
    except ValueError as e:
        return {"error": str(e)}
    res = _make_request("datastore_search", params)
    if "error" in res:
        return res
    years: Dict[str, Dict] = {}
    for r in res.get("records", []):
        y = (r.get("apprvl_dt") or "")[:4]
        if not y.isdigit() or int(y) < first:
            continue
        e = years.setdefault(y, {"year": y, "projects": 0, "approved_usd": 0.0})
        e["projects"] += 1
        e["approved_usd"] += _num(r.get("orig_apprvd_useq_amnt")) or 0.0
    return {"country": country, "approvals": [years[y] for y in sorted(years)]}


def get_sectors() -> Any:
    rows = _sql(f'SELECT sector_cd, sector_nm, COUNT(*) AS projects FROM "{PROJECTS_RESOURCE}" '
                "GROUP BY sector_cd, sector_nm ORDER BY projects DESC")
    return rows if isinstance(rows, dict) else {"count": len(rows), "sectors": rows}


def get_countries() -> Any:
    rows = _sql(f'SELECT cntry_cd, cntry_nm, COUNT(*) AS projects FROM "{PROJECTS_RESOURCE}" '
                "GROUP BY cntry_cd, cntry_nm ORDER BY projects DESC")
    return rows if isinstance(rows, dict) else {"count": len(rows), "countries": rows}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "projects":
        country = args[1] if len(args) > 1 else None
        sector = args[2] if len(args) > 2 else None
        status = args[3] if len(args) > 3 else None
        limit = int(args[4]) if len(args) > 4 else 50
        result = get_projects(country, sector, status, limit)
    elif command == "project":
        if len(args) < 2:
            result = {"error": "project_id required"}
        else:
            result = get_project_details(args[1])
    elif command == "approvals":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        result = get_approvals(country, start_year)
    elif command == "sectors":
        result = get_sectors()
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
