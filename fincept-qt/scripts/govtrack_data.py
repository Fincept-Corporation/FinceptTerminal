"""
GovTrack Data Fetcher
GovTrack US Congress: bill tracking, voting records, committee data, member statistics.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.govtrack.us/api/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


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
    print(json.dumps(result))


if __name__ == "__main__":
    main()
