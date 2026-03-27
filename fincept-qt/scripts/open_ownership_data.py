"""
Open Ownership Register Data Fetcher
Fetches beneficial ownership data — who owns and controls companies across 130+
countries from the OpenOwnership Beneficial Ownership Data Standard (BODS) API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OPEN_OWNERSHIP_API_KEY', '')
BASE_URL = "https://api.bods.openownership.org"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if API_KEY:
    session.headers.update({"Authorization": f"Bearer {API_KEY}"})

OWNERSHIP_TYPES = {
    "shareholding": "Direct shareholding",
    "voting-rights": "Voting rights",
    "appointment-of-board": "Appointment of board members",
    "other-influence-or-control": "Other influence or control",
    "senior-managing-official": "Senior managing official",
    "nominee-shareholder": "Nominee shareholder",
    "nominee-director": "Nominee director",
    "ownership-of-shares": "Ownership of shares",
    "right-to-surplus-assets": "Right to surplus assets on dissolution",
    "right-to-profit": "Right to share in profits",
}

CONTROL_THRESHOLDS = {
    "0-25": "0% to 25%",
    "25-50": "25% to 50%",
    "50-75": "50% to 75%",
    "75-100": "75% to 100%",
    "exact": "Exact percentage",
}

COUNTRY_REGISTERS = {
    "GB": {"name": "United Kingdom", "register": "Companies House PSC Register", "url": "https://register.openownership.org/"},
    "UA": {"name": "Ukraine", "register": "Unified State Register", "url": "https://data.gov.ua/"},
    "DK": {"name": "Denmark", "register": "Danish Business Authority", "url": "https://datacvr.virk.dk/"},
    "SK": {"name": "Slovakia", "register": "Slovak Business Register", "url": "https://www.orsr.sk/"},
    "NO": {"name": "Norway", "register": "Brønnøysund Register Centre", "url": "https://brreg.no/"},
    "SL": {"name": "Sierra Leone", "register": "Corporate Affairs Commission", "url": ""},
    "NG": {"name": "Nigeria", "register": "Corporate Affairs Commission", "url": ""},
    "GH": {"name": "Ghana", "register": "Registrar-General's Department", "url": ""},
    "AM": {"name": "Armenia", "register": "State Register of Legal Entities", "url": ""},
    "KG": {"name": "Kyrgyzstan", "register": "Ministry of Justice", "url": ""},
    "MK": {"name": "North Macedonia", "register": "Central Registry", "url": ""},
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


def _extract_person(stmt: Dict) -> Dict:
    names = stmt.get("names", [])
    primary_name = next((n for n in names if n.get("type") == "individual"), names[0] if names else {})
    nat_ids = stmt.get("nationalIdentifiers", [])
    addresses = stmt.get("addresses", [])
    return {
        "statement_id": stmt.get("statementID", ""),
        "statement_type": stmt.get("statementType", ""),
        "person_type": stmt.get("personType", ""),
        "name": primary_name.get("fullName", ""),
        "nationality": stmt.get("nationalities", [{}])[0].get("code", "") if stmt.get("nationalities") else "",
        "date_of_birth": stmt.get("birthDate", ""),
        "addresses": [a.get("address", "") for a in addresses],
        "national_identifiers": [{"type": n.get("scheme", ""), "id": n.get("id", "")} for n in nat_ids],
        "source_url": stmt.get("source", {}).get("url", "") if isinstance(stmt.get("source"), dict) else "",
        "publication_details": stmt.get("publicationDetails", {}),
    }


def _extract_entity_stmt(stmt: Dict) -> Dict:
    names = stmt.get("names", [])
    primary_name = names[0] if names else {}
    identifiers = stmt.get("identifiers", [])
    return {
        "statement_id": stmt.get("statementID", ""),
        "statement_type": stmt.get("statementType", ""),
        "entity_type": stmt.get("entityType", ""),
        "name": primary_name.get("fullName", ""),
        "jurisdiction": stmt.get("incorporatedInJurisdiction", {}).get("code", "") if isinstance(stmt.get("incorporatedInJurisdiction"), dict) else "",
        "identifiers": [{"scheme": i.get("scheme", ""), "id": i.get("id", "")} for i in identifiers],
        "registration_date": stmt.get("foundingDate", ""),
        "dissolution_date": stmt.get("dissolutionDate", ""),
        "source_url": stmt.get("source", {}).get("url", "") if isinstance(stmt.get("source"), dict) else "",
        "publication_details": stmt.get("publicationDetails", {}),
    }


def _extract_ownership_stmt(stmt: Dict) -> Dict:
    interests = stmt.get("interests", [])
    parsed_interests = []
    for interest in interests:
        parsed_interests.append({
            "type": interest.get("type", ""),
            "details": interest.get("details", ""),
            "share_min": interest.get("share", {}).get("minimum", None) if isinstance(interest.get("share"), dict) else None,
            "share_max": interest.get("share", {}).get("maximum", None) if isinstance(interest.get("share"), dict) else None,
            "share_exact": interest.get("share", {}).get("exact", None) if isinstance(interest.get("share"), dict) else None,
            "start_date": interest.get("startDate", ""),
            "end_date": interest.get("endDate", ""),
        })
    return {
        "statement_id": stmt.get("statementID", ""),
        "statement_type": stmt.get("statementType", ""),
        "subject_entity_id": stmt.get("subject", {}).get("describedByEntityStatement", "") if isinstance(stmt.get("subject"), dict) else "",
        "interested_party_id": stmt.get("interestedParty", {}).get("describedByPersonStatement", stmt.get("interestedParty", {}).get("describedByEntityStatement", "")) if isinstance(stmt.get("interestedParty"), dict) else "",
        "interests": parsed_interests,
        "is_component": stmt.get("isComponent", False),
        "publication_details": stmt.get("publicationDetails", {}),
    }


def search_persons(query: str, country: str = "", limit: int = 20) -> Dict:
    params = {"q": query, "limit": limit, "type": "person"}
    if country:
        params["country"] = country.upper()
    data = _make_request("v1/persons", params)
    if "error" in data:
        fallback = {"query": query, "country": country, "persons": [],
                    "note": "OpenOwnership API endpoint may require authentication or updated path",
                    "available_registers": COUNTRY_REGISTERS}
        return fallback
    items = data.get("data", data if isinstance(data, list) else [])
    return {
        "query": query,
        "country": country,
        "total": data.get("total", len(items)) if isinstance(data, dict) else len(items),
        "persons": [_extract_person(s) for s in items],
    }


def search_entities(query: str, country: str = "", limit: int = 20) -> Dict:
    params = {"q": query, "limit": limit, "type": "entity"}
    if country:
        params["country"] = country.upper()
    data = _make_request("v1/entities", params)
    if "error" in data:
        return {"query": query, "country": country, "entities": [],
                "note": "OpenOwnership API endpoint may require authentication or updated path",
                "available_registers": COUNTRY_REGISTERS}
    items = data.get("data", data if isinstance(data, list) else [])
    return {
        "query": query,
        "country": country,
        "total": data.get("total", len(items)) if isinstance(data, dict) else len(items),
        "entities": [_extract_entity_stmt(s) for s in items],
    }


def get_person_companies(person_id: str) -> Dict:
    data = _make_request(f"v1/persons/{person_id}/ownership-statements", {})
    if "error" in data:
        return {"person_id": person_id, "ownerships": [], "note": str(data.get("error", ""))}
    items = data.get("data", data if isinstance(data, list) else [])
    return {
        "person_id": person_id,
        "total": data.get("total", len(items)) if isinstance(data, dict) else len(items),
        "ownerships": [_extract_ownership_stmt(s) for s in items],
    }


def get_entity_owners(entity_id: str) -> Dict:
    data = _make_request(f"v1/entities/{entity_id}/ownership-statements", {})
    if "error" in data:
        return {"entity_id": entity_id, "owners": [], "note": str(data.get("error", ""))}
    items = data.get("data", data if isinstance(data, list) else [])
    return {
        "entity_id": entity_id,
        "total": data.get("total", len(items)) if isinstance(data, dict) else len(items),
        "owners": [_extract_ownership_stmt(s) for s in items],
    }


def get_ownership_chain(entity_id: str) -> Dict:
    data = _make_request(f"v1/entities/{entity_id}/ownership-chain", {})
    if "error" in data:
        return {
            "entity_id": entity_id,
            "chain": [],
            "note": str(data.get("error", "Ownership chain may not be available")),
            "ownership_types": OWNERSHIP_TYPES,
        }
    return {
        "entity_id": entity_id,
        "chain": data.get("data", data if isinstance(data, list) else []),
        "ownership_types": OWNERSHIP_TYPES,
    }


def get_jurisdictions() -> Dict:
    return {
        "total_registers": len(COUNTRY_REGISTERS),
        "country_registers": COUNTRY_REGISTERS,
        "ownership_types": OWNERSHIP_TYPES,
        "control_thresholds": CONTROL_THRESHOLDS,
        "openownership_url": "https://www.openownership.org",
        "register_url": "https://register.openownership.org",
        "bods_standard": "https://standard.openownership.org",
        "note": "OpenOwnership aggregates beneficial ownership data from 130+ jurisdictions.",
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "persons":
        query = args[1] if len(args) > 1 else ""
        if not query:
            result = {"error": "query required"}
        else:
            country = args[2] if len(args) > 2 else ""
            limit = int(args[3]) if len(args) > 3 else 20
            result = search_persons(query, country, limit)
    elif command == "entities":
        query = args[1] if len(args) > 1 else ""
        if not query:
            result = {"error": "query required"}
        else:
            country = args[2] if len(args) > 2 else ""
            limit = int(args[3]) if len(args) > 3 else 20
            result = search_entities(query, country, limit)
    elif command == "person_companies":
        person_id = args[1] if len(args) > 1 else ""
        if not person_id:
            result = {"error": "person_id required"}
        else:
            result = get_person_companies(person_id)
    elif command == "owners":
        entity_id = args[1] if len(args) > 1 else ""
        if not entity_id:
            result = {"error": "entity_id required"}
        else:
            result = get_entity_owners(entity_id)
    elif command == "chain":
        entity_id = args[1] if len(args) > 1 else ""
        if not entity_id:
            result = {"error": "entity_id required"}
        else:
            result = get_ownership_chain(entity_id)
    elif command == "jurisdictions":
        result = get_jurisdictions()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
