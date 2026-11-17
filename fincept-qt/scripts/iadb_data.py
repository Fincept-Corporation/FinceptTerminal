"""
Inter-American Development Bank (IADB) Data Fetcher
IDB lending operations in Latin America and the Caribbean - projects, approvals by country/sector/year, sectors and countries - from the IDB Open Data portal's projects dataset (CKAN datastore API, no key).
"""
import re
import ssl
import sys
import json
import requests
from typing import Dict, Any

import fincept_connector as fc

# api.iadb.org no longer exists; the projects list is published on data.iadb.org (CKAN).
BASE_URL = "https://data.iadb.org/api/3/action"
PROJECTS_RESOURCE = "814b7b54-477a-4c25-b3bf-6be05412069d"  # "EN - IDB Projects List" (idb-projects-dataset)
FIELDS = ["oper_num", "oper_nm", "apprvl_dt", "sign_dt", "cntry_cd", "cntry_nm", "publc_sts_nm", "sector_cd",
          "sector_nm", "subsector_nm", "opertyp_nm", "orig_apprvd_useq_amnt", "totl_cost_orig", "lending_typ_nm",
          "modality_nm", "envmntl_clssfctn_cd"]
SERVICE = "IDB Open Data (data.iadb.org)"


class _StdlibTLSAdapter(requests.adapters.HTTPAdapter):
    """data.iadb.org sits behind Cloudflare, which answers urllib3's default TLS handshake with an HTTP 403
    'Attention Required' HTML page (whatever the headers); Python's standard ssl context gets the JSON."""

    def init_poolmanager(self, *args, **kwargs):
        kwargs["ssl_context"] = ssl.create_default_context()
        return super().init_poolmanager(*args, **kwargs)


session = fc.http_session()
session.mount('https://', _StdlibTLSAdapter(max_retries=session.get_adapter('https://').max_retries))
session.headers.update({"User-Agent": "Mozilla/5.0 (FinceptTerminal/4.5; +https://fincept.in)",
                        "Accept": "application/json"})


def _make_request(action: str, params: Dict) -> Any:
    try:
        response = session.get(f"{BASE_URL}/{action}", params=params, timeout=(10, 60))
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e)}
    if "json" not in response.headers.get("content-type", "") and response.status_code in (403, 503):
        return {"error": f"{SERVICE} blocked the request (HTTP {response.status_code} bot-protection page) - "
                         "try again later"}
    try:
        data = response.json()
    except ValueError:
        if not response.ok:
            return {"error": fc.http_error(SERVICE, response)}
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    if not isinstance(data, dict):
        return {"error": f"{SERVICE} returned an unexpected response"}
    if not data.get("success"):
        err = data.get("error", {})
        return {"error": f"IDB open data: {err.get('message') or err}"}
    return data["result"]


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


def _count_by(code_field: str, name_field: str) -> Any:
    """[{code, name, projects}] sorted by project count. Counted client-side: Cloudflare's WAF in front of
    data.iadb.org rejects every datastore_search_sql request (HTTP 403), so GROUP BY is not available."""
    res = _make_request("datastore_search", {"resource_id": PROJECTS_RESOURCE, "limit": 32000,
                                             "fields": f"{code_field},{name_field}"})
    if "error" in res:
        return res
    counts: Dict[tuple, int] = {}
    for r in res.get("records", []):
        key = (r.get(code_field), r.get(name_field))
        counts[key] = counts.get(key, 0) + 1
    return [{code_field: k[0], name_field: k[1], "projects": n}
            for k, n in sorted(counts.items(), key=lambda kv: -kv[1])]


def get_sectors() -> Any:
    rows = _count_by("sector_cd", "sector_nm")
    return rows if isinstance(rows, dict) else {"count": len(rows), "sectors": rows}


def get_countries() -> Any:
    rows = _count_by("cntry_cd", "cntry_nm")
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
        try:
            limit = int(args[4]) if len(args) > 4 else 50
            result = get_projects(country, sector, status, limit)
        except ValueError:
            result = {"error": f"limit must be a number, got '{args[4]}'"}
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
