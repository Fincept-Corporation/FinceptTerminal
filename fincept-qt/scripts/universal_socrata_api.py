#!/usr/bin/env python3
"""
Universal Socrata (SODA) API Wrapper

A comprehensive wrapper for querying any Socrata-powered open data portal.
Supports SODA 2.0, 2.1, and 3.0 endpoints across hundreds of government portals.

Usage Examples:
    # Query a dataset (JSON)
    python universal_socrata_api.py --action query --domain data.wa.gov --dataset f6w7-q2d2 --select "make,model,ev_type" --where "make='TESLA'" --limit 50

    # Get dataset metadata / schema
    python universal_socrata_api.py --action metadata --domain data.wa.gov --dataset f6w7-q2d2

    # Get total row count
    python universal_socrata_api.py --action count --domain data.wa.gov --dataset f6w7-q2d2

    # Aggregate: count by make
    python universal_socrata_api.py --action query --domain data.wa.gov --dataset f6w7-q2d2 --select "make,count(*) AS total" --group "make" --order "total DESC" --limit 20

    # Full-text search
    python universal_socrata_api.py --action query --domain data.wa.gov --dataset f6w7-q2d2 --search "LEAF" --limit 10

    # Export full dataset as CSV
    python universal_socrata_api.py --action export --domain data.wa.gov --dataset f6w7-q2d2 --format csv

    # Paginate through all records
    python universal_socrata_api.py --action paginate --domain data.wa.gov --dataset f6w7-q2d2 --limit 1000 --page 2

    # List well-known Socrata portals
    python universal_socrata_api.py --action list_portals

Known Portals (examples):
    - data.wa.gov            (Washington State)
    - data.cityofchicago.org (Chicago)
    - data.cityofnewyork.us  (New York City)
    - data.sfgov.org         (San Francisco)
    - data.seattle.gov       (Seattle)
    - data.oregon.gov        (Oregon)
    - data.colorado.gov      (Colorado)
    - opendata.cityofboston.gov (Boston)
    - data.montgomerycountymd.gov (Montgomery County, MD)
    - data.austintexas.gov   (Austin, TX)
    - data.somervillema.gov  (Somerville, MA)
    - opendata.minneapolismn.gov (Minneapolis)
    - data.cityofmadison.com (Madison, WI)
    - data.cms.gov           (Centers for Medicare & Medicaid)
    - data.cdc.gov           (CDC)
    - data.baltimorecity.gov (Baltimore)
    - soda.demo.socrata.com  (Socrata demo/sandbox)

API Documentation: https://dev.socrata.com/docs/
"""

import sys
import json
import argparse
import urllib.request
import urllib.parse
import urllib.error
from typing import Dict, Any, Optional, List

# --- KNOWN PORTALS ---

KNOWN_PORTALS = {
    "washington":   {"domain": "data.wa.gov",              "description": "Washington State Open Data"},
    "chicago":      {"domain": "data.cityofchicago.org",   "description": "City of Chicago"},
    "nyc":          {"domain": "data.cityofnewyork.us",    "description": "New York City Open Data"},
    "sf":           {"domain": "data.sfgov.org",           "description": "San Francisco Open Data"},
    "seattle":      {"domain": "data.seattle.gov",         "description": "Seattle Open Data"},
    "oregon":       {"domain": "data.oregon.gov",          "description": "Oregon Open Data"},
    "colorado":     {"domain": "data.colorado.gov",        "description": "Colorado Open Data"},
    "boston":       {"domain": "opendata.cityofboston.gov","description": "Boston Open Data"},
    "austin":       {"domain": "data.austintexas.gov",     "description": "Austin, TX Open Data"},
    "cms":          {"domain": "data.cms.gov",             "description": "Centers for Medicare & Medicaid"},
    "cdc":          {"domain": "data.cdc.gov",             "description": "Centers for Disease Control"},
    "baltimore":    {"domain": "data.baltimorecity.gov",   "description": "Baltimore Open Data"},
    "demo":         {"domain": "soda.demo.socrata.com",    "description": "Socrata Demo/Sandbox"},
}

# --- CORE CLIENT ---

class SocrataClient:
    """
    Universal SODA 2.x client.
    Works with any Socrata-powered portal without authentication for public datasets.
    Add an app_token for higher rate limits (1000+ req/hr vs ~10 anon).
    """

    DEFAULT_LIMIT = 1000
    MAX_LIMIT = 50000

    def __init__(self, domain: str, app_token: Optional[str] = None):
        self.domain = domain.rstrip("/")
        self.app_token = app_token
        self.base_url = f"https://{self.domain}"

    def _build_headers(self) -> Dict[str, str]:
        headers = {"Accept": "application/json"}
        if self.app_token:
            headers["X-App-Token"] = self.app_token
        return headers

    def _get(self, url: str, params: Optional[Dict] = None) -> Any:
        if params:
            url = f"{url}?{urllib.parse.urlencode(params)}"
        req = urllib.request.Request(url, headers=self._build_headers())
        try:
            with urllib.request.urlopen(req, timeout=30) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            body = e.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"HTTP {e.code}: {body}") from e
        except urllib.error.URLError as e:
            raise RuntimeError(f"URL error: {e.reason}") from e

    def _get_raw(self, url: str, params: Optional[Dict] = None) -> str:
        if params:
            url = f"{url}?{urllib.parse.urlencode(params)}"
        req = urllib.request.Request(url, headers=self._build_headers())
        try:
            with urllib.request.urlopen(req, timeout=60) as resp:
                return resp.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            body = e.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"HTTP {e.code}: {body}") from e

    # ---- SODA 2.x resource endpoint (/resource/<id>.json) ----

    def resource_url(self, dataset_id: str, fmt: str = "json") -> str:
        return f"{self.base_url}/resource/{dataset_id}.{fmt}"

    def query(
        self,
        dataset_id: str,
        select: Optional[str] = None,
        where: Optional[str] = None,
        order: Optional[str] = None,
        group: Optional[str] = None,
        having: Optional[str] = None,
        search: Optional[str] = None,
        limit: int = DEFAULT_LIMIT,
        offset: int = 0,
        distinct: bool = False,
        fmt: str = "json",
    ) -> Any:
        """
        Query a dataset using SoQL parameters.
        Returns parsed JSON (list of dicts) or raw string for csv/geojson.
        """
        params: Dict[str, Any] = {}
        if select:
            params["$select"] = select
        if where:
            params["$where"] = where
        if order:
            params["$order"] = order
        if group:
            params["$group"] = group
        if having:
            params["$having"] = having
        if search:
            params["$q"] = search
        if distinct:
            params["$query"] = f"SELECT {select or '*'} DISTINCT"
        params["$limit"] = min(limit, self.MAX_LIMIT)
        params["$offset"] = offset

        url = self.resource_url(dataset_id, fmt)
        if fmt == "json":
            return self._get(url, params)
        return self._get_raw(url, params)

    def count(self, dataset_id: str, where: Optional[str] = None) -> int:
        """Return total row count, optionally filtered by a where clause."""
        params: Dict[str, Any] = {"$select": "count(*)", "$limit": 1}
        if where:
            params["$where"] = where
        result = self._get(self.resource_url(dataset_id), params)
        if result and isinstance(result, list) and len(result) > 0:
            row = result[0]
            # key is typically "count" or "count_1"
            for v in row.values():
                return int(v)
        return 0

    def paginate(self, dataset_id: str, page: int = 1, page_size: int = DEFAULT_LIMIT, **kwargs) -> Any:
        """Fetch a specific page of results (1-indexed)."""
        offset = (page - 1) * page_size
        return self.query(dataset_id, limit=page_size, offset=offset, **kwargs)

    def export_csv(self, dataset_id: str) -> str:
        """Bulk export entire dataset as CSV."""
        url = f"{self.base_url}/api/views/{dataset_id}/rows.csv"
        params = {"accessType": "DOWNLOAD"}
        return self._get_raw(url, params)

    def metadata(self, dataset_id: str) -> Dict:
        """Fetch dataset metadata including column definitions."""
        url = f"{self.base_url}/api/views/{dataset_id}"
        return self._get(url)

    def columns(self, dataset_id: str) -> List[Dict]:
        """Return list of column definitions for a dataset."""
        url = f"{self.base_url}/api/views/{dataset_id}/columns.json"
        return self._get(url)

    def schema(self, dataset_id: str) -> List[Dict]:
        """Return simplified schema: name, fieldName, dataTypeName, description."""
        cols = self.columns(dataset_id)
        return [
            {
                "name": c.get("name", ""),
                "fieldName": c.get("fieldName", ""),
                "dataTypeName": c.get("dataTypeName", ""),
                "description": c.get("description", ""),
            }
            for c in cols
            if not c.get("fieldName", "").startswith(":")  # skip system cols
        ]


# --- OUTPUT HELPERS ---

def output_result(data: Any, action: str) -> None:
    if isinstance(data, str):
        print(data)
    elif isinstance(data, (list, dict)):
        print(json.dumps(data, indent=2, default=str))
    else:
        print(str(data))


def list_portals() -> None:
    portals = [{"key": k, **v} for k, v in KNOWN_PORTALS.items()]
    print(json.dumps(portals, indent=2))


# --- CLI ---

def parse_args():
    parser = argparse.ArgumentParser(
        description="Universal Socrata (SODA) API client",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--action", required=True,
        choices=["query", "count", "metadata", "schema", "columns", "export", "paginate", "list_portals"],
        help="Action to perform")
    parser.add_argument("--domain", help="Socrata portal domain (e.g. data.wa.gov)")
    parser.add_argument("--dataset", help="Dataset 4x4 identifier (e.g. f6w7-q2d2)")
    parser.add_argument("--portal", help="Named portal shortcut (use list_portals to see keys)")
    parser.add_argument("--app-token", help="Socrata app token for higher rate limits")

    # SoQL clauses
    parser.add_argument("--select", help="Columns to return, comma-separated or SoQL expression")
    parser.add_argument("--where", help="Filter expression (SoQL WHERE clause)")
    parser.add_argument("--order", help="Sort expression (e.g. 'model_year DESC')")
    parser.add_argument("--group", help="GROUP BY columns")
    parser.add_argument("--having", help="HAVING clause (use with --group)")
    parser.add_argument("--search", help="Full-text search ($q parameter)")
    parser.add_argument("--limit", type=int, default=1000, help="Max rows (default 1000, max 50000)")
    parser.add_argument("--offset", type=int, default=0, help="Row offset for pagination")
    parser.add_argument("--page", type=int, default=1, help="Page number (1-indexed, use with paginate)")
    parser.add_argument("--page-size", type=int, default=1000, help="Rows per page (default 1000)")
    parser.add_argument("--format", default="json", choices=["json", "csv", "geojson"],
        help="Output format (default json)")
    parser.add_argument("--distinct", action="store_true", help="Return distinct rows only")

    return parser.parse_args()


def main():
    args = parse_args()

    if args.action == "list_portals":
        list_portals()
        return

    # Resolve domain
    domain = args.domain
    if not domain and args.portal:
        entry = KNOWN_PORTALS.get(args.portal)
        if not entry:
            print(json.dumps({"error": f"Unknown portal '{args.portal}'. Use list_portals to see options."}))
            sys.exit(1)
        domain = entry["domain"]

    if not domain:
        print(json.dumps({"error": "--domain or --portal is required for this action"}))
        sys.exit(1)

    if not args.dataset and args.action not in ("list_portals",):
        print(json.dumps({"error": "--dataset is required for this action"}))
        sys.exit(1)

    client = SocrataClient(domain=domain, app_token=args.app_token)

    try:
        if args.action == "query":
            result = client.query(
                dataset_id=args.dataset,
                select=args.select,
                where=args.where,
                order=args.order,
                group=args.group,
                having=args.having,
                search=args.search,
                limit=args.limit,
                offset=args.offset,
                distinct=args.distinct,
                fmt=args.format,
            )
            output_result(result, args.action)

        elif args.action == "count":
            n = client.count(args.dataset, where=args.where)
            print(json.dumps({"count": n}))

        elif args.action == "metadata":
            result = client.metadata(args.dataset)
            output_result(result, args.action)

        elif args.action == "schema":
            result = client.schema(args.dataset)
            output_result(result, args.action)

        elif args.action == "columns":
            result = client.columns(args.dataset)
            output_result(result, args.action)

        elif args.action == "export":
            result = client.export_csv(args.dataset)
            print(result)

        elif args.action == "paginate":
            result = client.paginate(
                dataset_id=args.dataset,
                page=args.page,
                page_size=args.page_size,
                select=args.select,
                where=args.where,
                order=args.order,
                search=args.search,
            )
            output_result(result, args.action)

    except RuntimeError as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
