"""
FDIC BankFind Suite Data Fetcher
US bank financial health, assets, capital ratios, NPL, and data for all
FDIC-insured banks via the FDIC BankFind Suite API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('FDIC_API_KEY', '')
BASE_URL = "https://banks.data.fdic.gov/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["output"] = "json"
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_institutions(filters: str = None, fields: str = None, limit: int = 50) -> Any:
    params = {"limit": limit, "sort_by": "ASSET", "sort_order": "DESC"}
    if filters:
        params["filters"] = filters
    if fields:
        params["fields"] = fields
    else:
        params["fields"] = "NAME,CERT,CITY,STNAME,ASSET,NETINC,LNLSNET,REPDTE,ACTIVE"
    return _make_request("institutions", params)


def get_institution(cert: str) -> Any:
    params = {
        "filters": f"CERT:{cert}",
        "fields": "NAME,CERT,CITY,STNAME,ASSET,NETINC,LNLSNET,REPDTE,ACTIVE,STALP,HCTMULT,SPECGRP",
    }
    return _make_request("institutions", params)


def get_financials(cert: str, fields: str = None, start_date: str = None, end_date: str = None) -> Any:
    params = {
        "filters": f"CERT:{cert}",
        "limit": 100,
        "sort_by": "REPDTE",
        "sort_order": "DESC",
    }
    if fields:
        params["fields"] = fields
    else:
        params["fields"] = "CERT,REPDTE,ASSET,LNLSNET,INTINC,NONII,NETINC,EQ,LNLSRES,SC"
    if start_date:
        params["filters"] += f" AND REPDTE:[{start_date.replace('-', '')} TO *]"
    if end_date:
        params["filters"] += f" AND REPDTE:[* TO {end_date.replace('-', '')}]"
    return _make_request("financials", params)


def get_history(cert: str) -> Any:
    params = {
        "filters": f"CERT:{cert}",
        "fields": "CERT,INSTNAME,CLASS,CHARTER,PCITY,PSTALP,ENDDATE,PROCDATE,RESTYPE,RESTYPE1DESC",
        "limit": 50,
        "sort_by": "ENDDATE",
        "sort_order": "DESC",
    }
    return _make_request("history", params)


def get_summary(start_date: str = None, end_date: str = None) -> Any:
    params = {
        "fields": "REPDTE,ASSET,LNLSNET,INTINC,NETINC,EQ",
        "limit": 50,
        "sort_by": "REPDTE",
        "sort_order": "DESC",
    }
    filters = ["ACTIVE:1"]
    if start_date:
        filters.append(f"REPDTE:[{start_date.replace('-', '')} TO *]")
    if end_date:
        filters.append(f"REPDTE:[* TO {end_date.replace('-', '')}]")
    params["filters"] = " AND ".join(filters)
    return _make_request("financials", params)


def search_banks(query: str, state: str = None, limit: int = 25) -> Any:
    filters = [f"NAME:{query}*"]
    if state:
        filters.append(f"STALP:{state.upper()}")
    filters.append("ACTIVE:1")
    params = {
        "filters": " AND ".join(filters),
        "fields": "NAME,CERT,CITY,STNAME,ASSET,ACTIVE",
        "limit": limit,
        "sort_by": "ASSET",
        "sort_order": "DESC",
    }
    return _make_request("institutions", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "institutions":
        filters = args[1] if len(args) > 1 else None
        fields = args[2] if len(args) > 2 else None
        limit = int(args[3]) if len(args) > 3 else 50
        result = get_institutions(filters, fields, limit)
    elif command == "institution":
        cert = args[1] if len(args) > 1 else "3511"
        result = get_institution(cert)
    elif command == "financials":
        cert = args[1] if len(args) > 1 else "3511"
        fields = args[2] if len(args) > 2 else None
        start_date = args[3] if len(args) > 3 else None
        end_date = args[4] if len(args) > 4 else None
        result = get_financials(cert, fields, start_date, end_date)
    elif command == "history":
        cert = args[1] if len(args) > 1 else "3511"
        result = get_history(cert)
    elif command == "summary":
        start_date = args[1] if len(args) > 1 else None
        end_date = args[2] if len(args) > 2 else None
        result = get_summary(start_date, end_date)
    elif command == "search":
        query = args[1] if len(args) > 1 else "JPMorgan"
        state = args[2] if len(args) > 2 else None
        limit = int(args[3]) if len(args) > 3 else 25
        result = search_banks(query, state, limit)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
