"""
Kaggle Datasets Data Fetcher
Fetches public finance, economics, and ML datasets from Kaggle — stock prices,
economic indicators, alternative data, and competitions.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('KAGGLE_KEY', '')
KAGGLE_USERNAME = os.environ.get('KAGGLE_USERNAME', '')
BASE_URL = "https://www.kaggle.com/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

FINANCE_TAGS = [
    "finance", "stock-market", "economics", "trading", "cryptocurrency",
    "investment", "banking", "financial-markets", "forex", "options",
]

COMPETITION_CATEGORIES = {
    "featured": "Featured",
    "research": "Research",
    "getting_started": "Getting Started",
    "playground": "Playground",
    "recruitment": "Recruitment",
    "all": "All",
}

SORT_BY_OPTIONS = {
    "hottest": "hottest",
    "votes": "voteCount",
    "updated": "lastUpdated",
    "active": "relevance",
    "published": "publishedAt",
    "file_size": "usabilityRating",
}


def _check_auth() -> Optional[Dict]:
    if not API_KEY or not KAGGLE_USERNAME:
        return {
            "error": "KAGGLE_KEY and KAGGLE_USERNAME environment variables not set",
            "note": "Get credentials at https://www.kaggle.com/settings/account -> Create New Token",
        }
    return None


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    auth = (KAGGLE_USERNAME, API_KEY) if API_KEY and KAGGLE_USERNAME else None
    try:
        response = session.get(url, params=params, auth=auth, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def search_datasets(search: str = "", tags: str = "", sort_by: str = "hottest",
                    size: str = "", limit: int = 20) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    params = {
        "search": search,
        "sortBy": SORT_BY_OPTIONS.get(sort_by, sort_by),
        "page": 1,
        "pageSize": min(limit, 100),
    }
    if tags:
        params["tagIds"] = tags
    if size:
        params["maxSize"] = size
    data = _make_request("datasets/list", params)
    if isinstance(data, list):
        return {
            "search": search,
            "total": len(data),
            "datasets": [
                {
                    "ref": d.get("ref", ""),
                    "title": d.get("title", ""),
                    "subtitle": d.get("subtitle", ""),
                    "total_votes": d.get("totalVotes", 0),
                    "usability_rating": d.get("usabilityRating", 0),
                    "total_bytes": d.get("totalBytes", 0),
                    "last_updated": d.get("lastUpdated", ""),
                    "tags": [t.get("name", "") for t in d.get("tags", [])],
                    "license": d.get("licenseName", ""),
                    "url": f"https://www.kaggle.com/datasets/{d.get('ref', '')}",
                }
                for d in data
            ],
        }
    return data if isinstance(data, dict) else {"error": "Unexpected response format", "data": data}


def get_dataset_metadata(owner_slug: str, dataset_slug: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    data = _make_request(f"datasets/{owner_slug}/{dataset_slug}", {})
    if "error" in data:
        return data
    return {
        "ref": data.get("ref", ""),
        "title": data.get("title", ""),
        "subtitle": data.get("subtitle", ""),
        "description": data.get("description", ""),
        "owner": owner_slug,
        "dataset_slug": dataset_slug,
        "total_votes": data.get("totalVotes", 0),
        "usability_rating": data.get("usabilityRating", 0),
        "total_bytes": data.get("totalBytes", 0),
        "created": data.get("createdAt", ""),
        "last_updated": data.get("lastUpdated", ""),
        "tags": [t.get("name", "") for t in data.get("tags", [])],
        "license": data.get("licenseName", ""),
        "files_count": len(data.get("files", [])),
        "url": f"https://www.kaggle.com/datasets/{owner_slug}/{dataset_slug}",
    }


def list_dataset_files(owner_slug: str, dataset_slug: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    data = _make_request(f"datasets/{owner_slug}/{dataset_slug}/files", {})
    if "error" in data:
        return data
    files = data.get("datasetFiles", data if isinstance(data, list) else [])
    return {
        "owner": owner_slug,
        "dataset_slug": dataset_slug,
        "files": [
            {
                "name": f.get("name", ""),
                "total_bytes": f.get("totalBytes", 0),
                "url": f.get("url", ""),
                "creation_date": f.get("creationDate", ""),
            }
            for f in files
        ],
    }


def download_dataset_file(owner_slug: str, dataset_slug: str, file_name: str) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    params = {"datasetFileId": file_name}
    url = f"{BASE_URL}/datasets/{owner_slug}/{dataset_slug}/download/{file_name}"
    auth = (KAGGLE_USERNAME, API_KEY)
    try:
        response = session.get(url, auth=auth, timeout=60, stream=True)
        response.raise_for_status()
        content_length = response.headers.get("Content-Length", "unknown")
        content_type = response.headers.get("Content-Type", "")
        return {
            "owner": owner_slug,
            "dataset_slug": dataset_slug,
            "file_name": file_name,
            "status": "download_ready",
            "content_type": content_type,
            "content_length": content_length,
            "download_url": url,
            "note": "File download initiated. Use streaming download for large files.",
        }
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def get_competitions(category: str = "all", sort_by: str = "latestDeadline", limit: int = 20) -> Dict:
    auth_err = _check_auth()
    if auth_err:
        return auth_err
    params = {
        "sortBy": sort_by,
        "pageSize": min(limit, 100),
        "page": 1,
    }
    if category and category != "all":
        params["category"] = category
    data = _make_request("competitions/list", params)
    if isinstance(data, list):
        return {
            "category": category,
            "total": len(data),
            "competitions": [
                {
                    "ref": c.get("ref", ""),
                    "title": c.get("title", ""),
                    "category": c.get("category", ""),
                    "deadline": c.get("deadline", ""),
                    "reward": c.get("reward", ""),
                    "team_count": c.get("teamCount", 0),
                    "evaluation_metric": c.get("evaluationMetric", ""),
                    "url": f"https://www.kaggle.com/competitions/{c.get('ref', '')}",
                }
                for c in data
            ],
        }
    return data if isinstance(data, dict) else {"error": "Unexpected response", "data": data}


def get_featured_datasets(limit: int = 20) -> Dict:
    return search_datasets(search="finance economics", sort_by="votes", limit=limit)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "search":
        search = args[1] if len(args) > 1 else "finance"
        tags = args[2] if len(args) > 2 else ""
        sort_by = args[3] if len(args) > 3 else "hottest"
        size = args[4] if len(args) > 4 else ""
        limit = int(args[5]) if len(args) > 5 else 20
        result = search_datasets(search, tags, sort_by, size, limit)
    elif command == "metadata":
        if len(args) < 3:
            result = {"error": "owner_slug and dataset_slug required"}
        else:
            result = get_dataset_metadata(args[1], args[2])
    elif command == "files":
        if len(args) < 3:
            result = {"error": "owner_slug and dataset_slug required"}
        else:
            result = list_dataset_files(args[1], args[2])
    elif command == "download":
        if len(args) < 4:
            result = {"error": "owner_slug, dataset_slug, and file_name required"}
        else:
            result = download_dataset_file(args[1], args[2], args[3])
    elif command == "competitions":
        category = args[1] if len(args) > 1 else "all"
        sort_by = args[2] if len(args) > 2 else "latestDeadline"
        limit = int(args[3]) if len(args) > 3 else 20
        result = get_competitions(category, sort_by, limit)
    elif command == "featured":
        limit = int(args[1]) if len(args) > 1 else 20
        result = get_featured_datasets(limit)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
