"""
Data.world Data Fetcher
Fetches community-published datasets from data.world covering finance, economics,
ESG, demographics, and more. Requires a free API token.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('DATA_WORLD_TOKEN', '')
BASE_URL = "https://api.data.world/v0"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if API_KEY:
    session.headers.update({"Authorization": f"Bearer {API_KEY}"})


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


def _make_post_request(endpoint: str, body: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.post(url, json=body, timeout=60)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _check_auth() -> Optional[Dict]:
    if not API_KEY:
        return {
            "error": "DATA_WORLD_TOKEN environment variable not set",
            "note": "Get a free token at https://data.world/settings/advanced",
        }
    return None


def search_datasets(query: str, tags: str = "", limit: int = 25) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    params = {
        "q": query,
        "limit": limit,
    }
    if tags:
        params["tags"] = tags
    data = _make_request("datasets/search", params)
    if "error" in data:
        return data
    records = data.get("records", [])
    return {
        "query": query,
        "total": data.get("count", len(records)),
        "datasets": [
            {
                "id": d.get("id", ""),
                "title": d.get("title", ""),
                "owner": d.get("owner", ""),
                "description": d.get("description", "")[:300] if d.get("description") else "",
                "tags": d.get("tags", []),
                "created": d.get("created", ""),
                "updated": d.get("updated", ""),
                "visibility": d.get("visibility", ""),
                "files_count": len(d.get("files", [])),
                "url": f"https://data.world/{d.get('owner', '')}/{d.get('id', '')}",
            }
            for d in records
        ],
    }


def get_dataset(owner: str, dataset_id: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    data = _make_request(f"datasets/{owner}/{dataset_id}", {})
    if "error" in data:
        return data
    return {
        "id": data.get("id", ""),
        "title": data.get("title", ""),
        "owner": data.get("owner", ""),
        "description": data.get("description", ""),
        "tags": data.get("tags", []),
        "license": data.get("license", ""),
        "visibility": data.get("visibility", ""),
        "created": data.get("created", ""),
        "updated": data.get("updated", ""),
        "files": [
            {"name": f.get("name", ""), "size_in_bytes": f.get("sizeInBytes", 0),
             "type": f.get("type", ""), "created": f.get("created", "")}
            for f in data.get("files", [])
        ],
        "tables": [t.get("tableName", "") for t in data.get("tables", [])],
        "url": f"https://data.world/{owner}/{dataset_id}",
    }


def get_dataset_files(owner: str, dataset_id: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    data = _make_request(f"datasets/{owner}/{dataset_id}/files", {})
    if "error" in data:
        data2 = _make_request(f"datasets/{owner}/{dataset_id}", {})
        if "error" in data2:
            return data
        files = data2.get("files", [])
        return {
            "owner": owner,
            "dataset_id": dataset_id,
            "files": [
                {"name": f.get("name", ""), "size_in_bytes": f.get("sizeInBytes", 0),
                 "type": f.get("type", ""), "created": f.get("created", ""),
                 "download_url": f"https://query.data.world/s/{owner}/{dataset_id}/{f.get('name', '')}"}
                for f in files
            ],
        }
    return data


def get_sql_query(owner: str, dataset_id: str, query: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    body = {"query": query}
    data = _make_post_request(f"sql/{owner}/{dataset_id}", body)
    if "error" in data:
        return data
    return {
        "owner": owner,
        "dataset_id": dataset_id,
        "query": query,
        "results": data,
    }


def get_sparql_query(owner: str, dataset_id: str, query: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    body = {"query": query}
    data = _make_post_request(f"sparql/{owner}/{dataset_id}", body)
    if "error" in data:
        return data
    return {
        "owner": owner,
        "dataset_id": dataset_id,
        "query": query,
        "results": data,
    }


def get_user_datasets(user: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    data = _make_request(f"datasets/{user}", {})
    if "error" in data:
        return data
    records = data.get("records", []) if isinstance(data, dict) else data
    if isinstance(records, list):
        return {
            "user": user,
            "total": len(records),
            "datasets": [
                {
                    "id": d.get("id", ""),
                    "title": d.get("title", ""),
                    "tags": d.get("tags", []),
                    "visibility": d.get("visibility", ""),
                    "updated": d.get("updated", ""),
                    "url": f"https://data.world/{user}/{d.get('id', '')}",
                }
                for d in records
            ],
        }
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "search":
        query = args[1] if len(args) > 1 else "finance"
        tags = args[2] if len(args) > 2 else ""
        limit = int(args[3]) if len(args) > 3 else 25
        result = search_datasets(query, tags, limit)
    elif command == "dataset":
        if len(args) < 3:
            result = {"error": "owner and dataset_id required"}
        else:
            result = get_dataset(args[1], args[2])
    elif command == "files":
        if len(args) < 3:
            result = {"error": "owner and dataset_id required"}
        else:
            result = get_dataset_files(args[1], args[2])
    elif command == "sql":
        if len(args) < 4:
            result = {"error": "owner, dataset_id, and query required"}
        else:
            result = get_sql_query(args[1], args[2], args[3])
    elif command == "sparql":
        if len(args) < 4:
            result = {"error": "owner, dataset_id, and query required"}
        else:
            result = get_sparql_query(args[1], args[2], args[3])
    elif command == "user":
        user = args[1] if len(args) > 1 else ""
        if not user:
            result = {"error": "user required"}
        else:
            result = get_user_datasets(user)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
