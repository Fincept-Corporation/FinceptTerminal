"""
GLEIF LEI Data Fetcher
Fetches Legal Entity Identifier (LEI) data including global company identification,
ownership structures, and parent-child relationships from the GLEIF API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GLEIF_API_KEY', '')
BASE_URL = "https://api.gleif.org/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

session.headers.update({
    "Accept": "application/vnd.api+json",
    "Content-Type": "application/json",
})

ENTITY_STATUSES = {
    "ACTIVE": "Active — valid LEI",
    "INACTIVE": "Inactive — entity no longer exists",
    "MERGED": "Merged into another entity",
    "RETIRED": "Retired LEI",
}

REGISTRATION_STATUSES = {
    "ISSUED": "Fully validated and issued",
    "PENDING_VALIDATION": "Awaiting validation",
    "DUPLICATE": "Duplicate of another LEI",
    "LAPSED": "Not renewed — may be outdated",
    "MERGED": "Merged with another LEI",
    "RETIRED": "Retired LEI",
    "ANNULLED": "Annulled due to error",
    "CANCELLED": "Cancelled",
    "TRANSFERRED": "Transferred to another LOU",
    "PENDING_ARCHIVAL": "Pending archival",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def _extract_entity(item: Dict) -> Dict:
    attrs = item.get("attributes", {})
    entity = attrs.get("entity", {})
    registration = attrs.get("registration", {})
    legal_address = entity.get("legalAddress", {})
    hq_address = entity.get("headquartersAddress", {})
    return {
        "lei": attrs.get("lei", ""),
        "legal_name": entity.get("legalName", {}).get("name", ""),
        "other_names": [n.get("name", "") for n in entity.get("otherNames", [])],
        "transliterated_names": [n.get("name", "") for n in entity.get("transliteratedOtherNames", [])],
        "legal_address": {
            "line1": legal_address.get("addressLines", [""])[0] if legal_address.get("addressLines") else "",
            "city": legal_address.get("city", ""),
            "region": legal_address.get("region", ""),
            "country": legal_address.get("country", ""),
            "postal_code": legal_address.get("postalCode", ""),
        },
        "headquarters_address": {
            "line1": hq_address.get("addressLines", [""])[0] if hq_address.get("addressLines") else "",
            "city": hq_address.get("city", ""),
            "region": hq_address.get("region", ""),
            "country": hq_address.get("country", ""),
            "postal_code": hq_address.get("postalCode", ""),
        },
        "jurisdiction": entity.get("jurisdiction", ""),
        "legal_form": entity.get("legalForm", {}).get("id", ""),
        "associated_entity": entity.get("associatedEntity", {}),
        "status": entity.get("status", ""),
        "expiration_date": entity.get("expiration", {}).get("date", ""),
        "expiration_reason": entity.get("expiration", {}).get("reason", ""),
        "successor_lei": entity.get("successorLEI", ""),
        "successor_entity": entity.get("successorEntity", {}),
        "registration_status": registration.get("status", ""),
        "initial_registration_date": registration.get("initialRegistrationDate", ""),
        "last_update_date": registration.get("lastUpdateDate", ""),
        "next_renewal_date": registration.get("nextRenewalDate", ""),
        "managing_lou": registration.get("managingLOU", ""),
        "validation_sources": registration.get("validationSources", []),
    }


def search_entities(query: str, country: str = "", limit: int = 20, offset: int = 0) -> Dict:
    params = {
        "filter[fulltext]": query,
        "page[size]": min(limit, 100),
        "page[number]": (offset // limit) + 1,
    }
    if country:
        params["filter[entity.legalAddress.country]"] = country.upper()
    data = _make_request("lei-records", params)
    if "error" in data:
        return data
    items = data.get("data", [])
    meta = data.get("meta", {})
    return {
        "query": query,
        "country": country,
        "total": meta.get("total", len(items)),
        "page": meta.get("currentPage", 1),
        "entities": [_extract_entity(item) for item in items],
    }


def get_entity(lei: str) -> Dict:
    data = _make_request(f"lei-records/{lei}", {})
    if "error" in data:
        return data
    item = data.get("data", {})
    return _extract_entity(item)


def get_direct_parents(lei: str) -> Dict:
    data = _make_request(f"lei-records/{lei}/direct-parent", {})
    if "error" in data:
        return {"lei": lei, "direct_parents": [], "note": str(data.get("error", "No direct parent data available"))}
    item = data.get("data", {})
    if isinstance(item, dict) and item:
        return {"lei": lei, "direct_parent": _extract_entity(item)}
    return {"lei": lei, "direct_parents": [], "note": "No direct parent reporting exception or parent not found"}


def get_direct_children(lei: str, limit: int = 50) -> Dict:
    params = {
        "page[size]": min(limit, 100),
        "page[number]": 1,
    }
    data = _make_request(f"lei-records/{lei}/direct-children", params)
    if "error" in data:
        return {"lei": lei, "direct_children": [], "note": str(data.get("error", "No direct children data available"))}
    items = data.get("data", [])
    meta = data.get("meta", {})
    return {
        "lei": lei,
        "total_children": meta.get("total", len(items)),
        "direct_children": [_extract_entity(item) for item in items],
    }


def get_ultimate_parent(lei: str) -> Dict:
    data = _make_request(f"lei-records/{lei}/ultimate-parent", {})
    if "error" in data:
        return {"lei": lei, "ultimate_parent": None, "note": str(data.get("error", "No ultimate parent data available"))}
    item = data.get("data", {})
    if isinstance(item, dict) and item:
        return {"lei": lei, "ultimate_parent": _extract_entity(item)}
    return {"lei": lei, "ultimate_parent": None, "note": "No ultimate parent or reporting exception"}


def get_reporting_exceptions(lei: str) -> Dict:
    data = _make_request(f"lei-records/{lei}/reporting-exceptions", {})
    if "error" in data:
        return {"lei": lei, "exceptions": [], "note": str(data.get("error", ""))}
    items = data.get("data", [])
    exceptions = []
    for item in items:
        attrs = item.get("attributes", {})
        exceptions.append({
            "exception_reason": attrs.get("exceptionReason", ""),
            "exception_reference": attrs.get("exceptionReference", ""),
            "relationship_type": attrs.get("relationship", {}).get("type", ""),
        })
    return {
        "lei": lei,
        "total_exceptions": len(exceptions),
        "exceptions": exceptions,
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "search":
        query = args[1] if len(args) > 1 else ""
        if not query:
            result = {"error": "query required"}
        else:
            country = args[2] if len(args) > 2 else ""
            limit = int(args[3]) if len(args) > 3 else 20
            offset = int(args[4]) if len(args) > 4 else 0
            result = search_entities(query, country, limit, offset)
    elif command == "entity":
        lei = args[1] if len(args) > 1 else ""
        if not lei:
            result = {"error": "LEI required"}
        else:
            result = get_entity(lei)
    elif command == "parents":
        lei = args[1] if len(args) > 1 else ""
        if not lei:
            result = {"error": "LEI required"}
        else:
            result = get_direct_parents(lei)
    elif command == "children":
        lei = args[1] if len(args) > 1 else ""
        if not lei:
            result = {"error": "LEI required"}
        else:
            limit = int(args[2]) if len(args) > 2 else 50
            result = get_direct_children(lei, limit)
    elif command == "ultimate_parent":
        lei = args[1] if len(args) > 1 else ""
        if not lei:
            result = {"error": "LEI required"}
        else:
            result = get_ultimate_parent(lei)
    elif command == "exceptions":
        lei = args[1] if len(args) > 1 else ""
        if not lei:
            result = {"error": "LEI required"}
        else:
            result = get_reporting_exceptions(lei)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
