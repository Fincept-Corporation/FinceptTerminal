"""
Open Parliament Data Fetcher
Open Parliament / They Vote For You: parliamentary votes, member records for multiple countries (TheyWorkForYou API).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('TWFY_API_KEY', '')
BASE_URL = "https://www.theyworkforyou.com/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["output"] = "js"
    if API_KEY:
        params["key"] = API_KEY
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


def get_mps(search: str = "") -> Any:
    params = {"search": search} if search else {}
    return _make_request("getMPs", params=params)


def get_mp_info(id_: int) -> Any:
    params = {"id": id_}
    return _make_request("getMPInfo", params=params)


def get_debates(search: str, num: int = 10) -> Any:
    params = {"search": search, "num": num}
    return _make_request("getDebates", params=params)


def get_mp_votes(person_id: int) -> Any:
    params = {"person": person_id}
    return _make_request("getMPsVotingRecord", params=params)


def get_lords(search: str = "") -> Any:
    params = {"search": search} if search else {}
    return _make_request("getLords", params=params)


def get_committees() -> Any:
    return _make_request("getCommittees")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    if not API_KEY:
        print(json.dumps({"error": "TWFY_API_KEY environment variable not set"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "mps":
        search = args[1] if len(args) > 1 else ""
        result = get_mps(search)
    elif command == "mp":
        id_ = int(args[1]) if len(args) > 1 else 10001
        result = get_mp_info(id_)
    elif command == "debates":
        search = args[1] if len(args) > 1 else "economy"
        num = int(args[2]) if len(args) > 2 else 10
        result = get_debates(search, num)
    elif command == "votes":
        person_id = int(args[1]) if len(args) > 1 else 10001
        result = get_mp_votes(person_id)
    elif command == "lords":
        search = args[1] if len(args) > 1 else ""
        result = get_lords(search)
    elif command == "committees":
        result = get_committees()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
