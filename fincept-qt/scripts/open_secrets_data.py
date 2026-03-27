"""
OpenSecrets Data Fetcher
OpenSecrets: US campaign finance, lobbying data, PAC contributions, revolving door.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OPENSECRETS_API_KEY', '')
BASE_URL = "https://www.opensecrets.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["output"] = "json"
    if API_KEY:
        params["apikey"] = API_KEY
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


def get_candidates(cycle: str = "2024", state: str = "CA") -> Any:
    params = {
        "method": "getCandidates",
        "cycle": cycle,
        "state": state
    }
    return _make_request("", params=params)


def get_candidate_summary(cid: str, cycle: str = "2024") -> Any:
    params = {
        "method": "candSummary",
        "cid": cid,
        "cycle": cycle
    }
    return _make_request("", params=params)


def get_lobby_firms(lobiid: str, year: str = "2023") -> Any:
    params = {
        "method": "lobbyFirm",
        "lobiid": lobiid,
        "year": year
    }
    return _make_request("", params=params)


def get_industry_summary(ind: str, cycle: str = "2024") -> Any:
    params = {
        "method": "getIndustries",
        "ind": ind,
        "cycle": cycle
    }
    return _make_request("", params=params)


def get_pac_contributions(cmte_id: str, cycle: str = "2024") -> Any:
    params = {
        "method": "pacSummary",
        "cmte_id": cmte_id,
        "cycle": cycle
    }
    return _make_request("", params=params)


def get_congress_member(cid: str) -> Any:
    params = {
        "method": "getLegislators",
        "id": cid
    }
    return _make_request("", params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    if not API_KEY:
        print(json.dumps({"error": "OPENSECRETS_API_KEY environment variable not set"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "candidates":
        cycle = args[1] if len(args) > 1 else "2024"
        state = args[2] if len(args) > 2 else "CA"
        result = get_candidates(cycle, state)
    elif command == "candidate":
        cid = args[1] if len(args) > 1 else "N00007360"
        cycle = args[2] if len(args) > 2 else "2024"
        result = get_candidate_summary(cid, cycle)
    elif command == "lobby":
        lobiid = args[1] if len(args) > 1 else "D000000082"
        year = args[2] if len(args) > 2 else "2023"
        result = get_lobby_firms(lobiid, year)
    elif command == "industry":
        ind = args[1] if len(args) > 1 else "F10"
        cycle = args[2] if len(args) > 2 else "2024"
        result = get_industry_summary(ind, cycle)
    elif command == "pac":
        cmte_id = args[1] if len(args) > 1 else "C00000935"
        cycle = args[2] if len(args) > 2 else "2024"
        result = get_pac_contributions(cmte_id, cycle)
    elif command == "congress":
        cid = args[1] if len(args) > 1 else "N00007360"
        result = get_congress_member(cid)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
