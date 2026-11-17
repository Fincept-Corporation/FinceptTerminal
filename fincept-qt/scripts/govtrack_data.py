"""
GovTrack Data Fetcher
GovTrack US Congress: bill tracking, voting records, committee data, member statistics.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

BASE_URL = "https://www.govtrack.us/api/v2"
SERVICE = "GovTrack"

session = fc.http_session()
session.headers.update({"User-Agent": "Mozilla/5.0 (FinceptTerminal; +https://fincept.in)",
                        "Accept": "application/json"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=(10, 30))
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e)}
    if response.headers.get("cf-mitigated") == "challenge" or (
            response.status_code == 403 and "json" not in response.headers.get("content-type", "")):
        # Checked 2026-11-16: the whole site, /api/v2 included, sits behind a Cloudflare browser challenge.
        return {"error": "GovTrack blocks automated requests: www.govtrack.us answers with a Cloudflare browser "
                         "challenge (HTTP 403) that a script cannot pass - use the Congress.gov connector "
                         "(congress_gov_data.py) for bills, votes and members"}
    if not response.ok:
        return {"error": fc.http_error(SERVICE, response)}
    try:
        return response.json()
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}


def get_person(person_id: int) -> Any:
    return _make_request(f"person/{person_id}")


def get_bill(bill_id: int) -> Any:
    return _make_request(f"bill/{bill_id}")


def get_vote(vote_id: int) -> Any:
    return _make_request(f"vote/{vote_id}")


def search_bills(query: str, congress: int = 118, status: str = None, sort: str = "-introduced_date") -> Any:
    params = {
        "q": query,
        "congress": congress,
        "sort": sort,
        "limit": 20
    }
    if status:
        params["current_status"] = status
    return _make_request("bill", params=params)


def get_committee(committee_id: int) -> Any:
    return _make_request(f"committee/{committee_id}")


def get_role(role_id: int) -> Any:
    return _make_request(f"role/{role_id}")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    try:
        result = _dispatch(command, args, result)
    except ValueError:
        result = {"error": "ids and congress must be whole numbers"}
    print(json.dumps(result))


def _dispatch(command: str, args: List[str], result: Dict) -> Any:
    if command == "person":
        person_id = int(args[1]) if len(args) > 1 else 400629
        result = get_person(person_id)
    elif command == "bill":
        bill_id = int(args[1]) if len(args) > 1 else 428803
        result = get_bill(bill_id)
    elif command == "vote":
        vote_id = int(args[1]) if len(args) > 1 else 1
        result = get_vote(vote_id)
    elif command == "search":
        query = args[1] if len(args) > 1 else "infrastructure"
        congress = int(args[2]) if len(args) > 2 else 118
        status = args[3] if len(args) > 3 else None
        sort = args[4] if len(args) > 4 else "-introduced_date"
        result = search_bills(query, congress, status, sort)
    elif command == "committee":
        committee_id = int(args[1]) if len(args) > 1 else 1
        result = get_committee(committee_id)
    elif command == "role":
        role_id = int(args[1]) if len(args) > 1 else 1
        result = get_role(role_id)
    return result


if __name__ == "__main__":
    main()
