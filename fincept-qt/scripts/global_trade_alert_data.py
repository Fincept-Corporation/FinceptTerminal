"""
Global Trade Alert Data Fetcher
Global Trade Alert: protectionist and liberalizing trade-policy interventions worldwide (GTA API; jurisdiction and intervention-type lists are keyless, intervention data needs a GTA API key: GTA_API_KEY).
"""
import sys
import json
import os
import requests
from collections import Counter
from typing import Dict, Any, List, Optional

API_KEY = os.environ.get('GTA_API_KEY', '')
BASE_URL = "https://api.globaltradealert.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)", "Content-Type": "application/json"})

_MAPPINGS: Dict[str, Any] = {}


def _post(path: str, body: Dict, needs_key: bool = True) -> Any:
    if needs_key and not API_KEY:
        return {"error": "Global Trade Alert intervention data needs an API key: request one at "
                         "https://globaltradealert.org/api-access and save it as GTA_API_KEY in Settings > Credentials"}
    headers = {"Authorization": f"APIKey {API_KEY}"} if API_KEY else {}
    try:
        response = session.post(f"{BASE_URL}/{path}", json=body, headers=headers, timeout=60)
        if response.status_code in (401, 403):
            return {"error": "Global Trade Alert rejected the API key (GTA_API_KEY) - check it in Settings > Credentials"}
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {e.response.text[:200]}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _mappings(key: str) -> List[Dict]:
    if key not in _MAPPINGS:
        data = _post("v1/gta/mappings/", {"keys": [key]}, needs_key=False)
        _MAPPINGS[key] = data.get(key, []) if isinstance(data, dict) else []
    return _MAPPINGS[key]


def _jurisdiction_ids(country: Optional[str]) -> List[int]:
    if not country:
        return []
    ids = []
    for c in country.split(","):
        c = c.strip()
        if c.isdigit():
            ids.append(int(c))
            continue
        hit = next((j["id"] for j in _mappings("jurisdictions")
                    if c.upper() == str(j.get("iso_code", "")).upper() or c.lower() == str(j.get("name", "")).lower()),
                   None)
        if hit is None:
            raise ValueError(f"Unknown jurisdiction '{c}' (use an ISO-3 code or a name from `jurisdictions`)")
        ids.append(hit)
    return ids


def _type_ids(type_: Optional[str]) -> List[int]:
    if not type_:
        return []
    if type_.isdigit():
        return [int(type_)]
    t = type_.lower()
    ids = [m.get("intervention_type_id") for m in _mappings("intervention_types")
           if t in str(m.get("intervention_type_name", "")).lower()]
    if not ids:
        raise ValueError(f"Unknown intervention type '{type_}' (see `types`)")
    return ids


def _interventions(country: str = None, year: int = None, type_: str = None, limit: int = 50,
                   extra: Dict = None) -> Any:
    try:
        req: Dict[str, Any] = {}
        ids = _jurisdiction_ids(country)
        if ids:
            req["implementer"] = ids
        if year:
            req["announcement_period"] = [f"{int(year)}-01-01", f"{int(year)}-12-31"]
        tids = _type_ids(type_)
        if tids:
            req["intervention_types"] = tids
        req.update(extra or {})
    except ValueError as e:
        return {"error": str(e)}
    data = _post("v2/gta/data/", {"request_data": req, "limit": max(0, min(int(limit), 1000)), "offset": 0})
    if isinstance(data, dict) and "error" in data:
        return data
    rows = data if isinstance(data, list) else data.get("results", data.get("data", []))
    return rows


def get_jurisdictions() -> Any:
    rows = _mappings("jurisdictions")
    if not rows:
        return {"error": "Could not load GTA jurisdictions"}
    return {"count": len(rows), "jurisdictions": rows}


def get_intervention_types() -> Any:
    rows = _mappings("intervention_types")
    if not rows:
        return {"error": "Could not load GTA intervention types"}
    return {"count": len(rows), "intervention_types": rows}


def get_interventions(country: str = None, year: int = None, type_: str = None, limit: int = 50) -> Any:
    rows = _interventions(country, year, type_, limit)
    if isinstance(rows, dict):
        return rows
    return {"count": len(rows), "interventions": rows, "filters": {"implementer": country, "year": year,
                                                                   "type": type_}}


def get_intervention_details(id_: int) -> Any:
    rows = _interventions(limit=1, extra={"intervention_id": [int(id_)]})
    if isinstance(rows, dict):
        return rows
    return rows[0] if rows else {"error": f"Intervention {id_} not found"}


def _names(v: Any) -> List[str]:
    if isinstance(v, list):
        out = []
        for x in v:
            out.append(x.get("name") or x.get("sector_name") or str(x) if isinstance(x, dict) else str(x))
        return out
    return [str(v)] if v else []


def get_affected_sectors(country: str = None, year: int = None) -> Any:
    rows = _interventions(country, year, None, 1000)
    if isinstance(rows, dict):
        return rows
    c = Counter(s for r in rows for s in _names(r.get("affected_sectors")))
    return {"interventions_scanned": len(rows), "sectors": [{"sector": k, "interventions": n} for k, n in c.most_common(50)]}


def get_implementing_countries(year: int = None, measure_type: str = None) -> Any:
    rows = _interventions(None, year, measure_type, 1000)
    if isinstance(rows, dict):
        return rows
    c = Counter(j for r in rows for j in _names(r.get("implementing_jurisdictions")))
    return {"interventions_scanned": len(rows),
            "implementers": [{"jurisdiction": k, "interventions": n} for k, n in c.most_common(50)]}


def get_statistics(country: str = None, year: int = None) -> Any:
    rows = _interventions(country, year, None, 1000)
    if isinstance(rows, dict):
        return rows
    return {"interventions_scanned": len(rows),
            "by_evaluation": dict(Counter(str(r.get("gta_evaluation")) for r in rows)),
            "by_type": dict(Counter(str(r.get("intervention_type")) for r in rows).most_common(25)),
            "in_force": sum(1 for r in rows if r.get("is_in_force"))}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "jurisdictions":
        result = get_jurisdictions()
    elif command == "types":
        result = get_intervention_types()
    elif command == "interventions":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        type_ = args[3] if len(args) > 3 else None
        limit = int(args[4]) if len(args) > 4 else 50
        result = get_interventions(country, year, type_, limit)
    elif command == "details":
        id_ = int(args[1]) if len(args) > 1 else 1
        result = get_intervention_details(id_)
    elif command == "sectors":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        result = get_affected_sectors(country, year)
    elif command == "countries":
        year = int(args[1]) if len(args) > 1 else None
        measure_type = args[2] if len(args) > 2 else None
        result = get_implementing_countries(year, measure_type)
    elif command == "statistics":
        country = args[1] if len(args) > 1 else None
        year = int(args[2]) if len(args) > 2 else None
        result = get_statistics(country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
