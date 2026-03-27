"""
Harvard Dataverse Data Fetcher
Fetches research datasets in social science, economics, and public health
from the Harvard Dataverse repository.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('HARVARD_DATAVERSE_TOKEN', '')
BASE_URL = "https://dataverse.harvard.edu/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if API_KEY:
    session.headers.update({"X-Dataverse-key": API_KEY})

DATAVERSE_SUBJECTS = [
    "Agricultural Sciences",
    "Arts and Humanities",
    "Astronomy and Astrophysics",
    "Business and Management",
    "Chemistry",
    "Computer and Information Science",
    "Earth and Environmental Sciences",
    "Engineering",
    "Law",
    "Mathematical Sciences",
    "Medicine, Health and Life Sciences",
    "Physics",
    "Social Sciences",
    "Other",
]

ECONOMICS_SUBJECTS = [
    "Business and Management",
    "Social Sciences",
    "Mathematical Sciences",
]


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


def _extract_dataset_item(item: Dict) -> Dict:
    return {
        "name": item.get("name", ""),
        "type": item.get("type", ""),
        "url": item.get("url", ""),
        "description": item.get("description", "")[:400] if item.get("description") else "",
        "published_at": item.get("published_at", ""),
        "identifier": item.get("global_id", item.get("identifier", "")),
        "authors": item.get("authors", []),
        "keywords": item.get("keywords", []),
        "subjects": item.get("subjects", []),
        "file_count": item.get("fileCount", 0),
        "version": item.get("majorVersion", ""),
    }


def search_datasets(query: str, subject: str = "", limit: int = 20, start: int = 0) -> Dict:
    params = {
        "q": query,
        "type": "dataset",
        "per_page": min(limit, 100),
        "start": start,
    }
    if subject:
        params["fq"] = f"subject_ss:{subject}"
    data = _make_request("search", params)
    if "error" in data:
        return data
    status = data.get("status", "")
    if status == "ERROR":
        return {"error": data.get("message", "Unknown error")}
    payload = data.get("data", {})
    items = payload.get("items", [])
    return {
        "query": query,
        "subject": subject,
        "total_count": payload.get("total_count", len(items)),
        "start": payload.get("start", start),
        "datasets": [_extract_dataset_item(item) for item in items],
    }


def get_dataset(doi_or_id: str) -> Dict:
    if doi_or_id.startswith("doi:") or doi_or_id.startswith("10."):
        persistent_id = doi_or_id if doi_or_id.startswith("doi:") else f"doi:{doi_or_id}"
        params = {"persistentId": persistent_id}
        data = _make_request("datasets/:persistentId", params)
    else:
        data = _make_request(f"datasets/{doi_or_id}", {})
    if "error" in data:
        return data
    payload = data.get("data", data)
    meta = payload.get("latestVersion", {}).get("metadataBlocks", {})
    citation = meta.get("citation", {}).get("fields", [])
    title = ""
    authors = []
    description = ""
    keywords = []
    subjects = []
    for field in citation:
        name = field.get("typeName", "")
        if name == "title":
            title = field.get("value", "")
        elif name == "author":
            for auth in (field.get("value") or []):
                if isinstance(auth, dict):
                    auth_name = auth.get("authorName", {})
                    if isinstance(auth_name, dict):
                        authors.append(auth_name.get("value", ""))
                    else:
                        authors.append(str(auth_name))
        elif name == "dsDescription":
            for desc_item in (field.get("value") or []):
                if isinstance(desc_item, dict):
                    desc_val = desc_item.get("dsDescriptionValue", {})
                    if isinstance(desc_val, dict):
                        description = desc_val.get("value", "")[:500]
        elif name == "keyword":
            for kw in (field.get("value") or []):
                if isinstance(kw, dict):
                    kw_val = kw.get("keywordValue", {})
                    if isinstance(kw_val, dict):
                        keywords.append(kw_val.get("value", ""))
        elif name == "subject":
            subjects = field.get("value", [])
    latest = payload.get("latestVersion", {})
    return {
        "id": payload.get("id", ""),
        "identifier": payload.get("identifier", ""),
        "persistent_url": payload.get("persistentUrl", ""),
        "title": title,
        "authors": authors,
        "description": description,
        "keywords": keywords,
        "subjects": subjects,
        "version_state": latest.get("versionState", ""),
        "version_number": f"{latest.get('majorVersionNumber', '')}.{latest.get('minorVersionNumber', '')}",
        "create_time": latest.get("createTime", ""),
        "last_update_time": latest.get("lastUpdateTime", ""),
    }


def get_dataset_files(persistent_id: str) -> Dict:
    pid = persistent_id if persistent_id.startswith("doi:") else f"doi:{persistent_id}"
    params = {"persistentId": pid}
    data = _make_request("datasets/:persistentId/versions/:latest/files", params)
    if "error" in data:
        return data
    payload = data.get("data", [])
    files = []
    for f in payload:
        df = f.get("dataFile", {})
        files.append({
            "id": df.get("id", ""),
            "filename": df.get("filename", ""),
            "content_type": df.get("contentType", ""),
            "file_size": df.get("filesize", 0),
            "md5": df.get("md5", ""),
            "description": f.get("description", ""),
            "categories": f.get("categories", []),
            "restricted": df.get("restricted", False),
            "download_url": f"https://dataverse.harvard.edu/api/access/datafile/{df.get('id', '')}",
        })
    return {
        "persistent_id": persistent_id,
        "file_count": len(files),
        "files": files,
    }


def get_file_metadata(file_id: str) -> Dict:
    data = _make_request(f"files/{file_id}", {})
    if "error" in data:
        return data
    payload = data.get("data", data)
    df = payload.get("dataFile", payload)
    return {
        "id": df.get("id", ""),
        "filename": df.get("filename", ""),
        "content_type": df.get("contentType", ""),
        "file_size": df.get("filesize", 0),
        "md5": df.get("md5", ""),
        "restricted": df.get("restricted", False),
        "download_url": f"https://dataverse.harvard.edu/api/access/datafile/{df.get('id', '')}",
    }


def search_by_subject(subject: str, limit: int = 20) -> Dict:
    return search_datasets("", subject, limit, 0)


def get_latest_datasets(limit: int = 20) -> Dict:
    params = {
        "q": "*",
        "type": "dataset",
        "per_page": min(limit, 100),
        "start": 0,
        "sort": "date",
        "order": "desc",
    }
    data = _make_request("search", params)
    if "error" in data:
        return data
    payload = data.get("data", {})
    items = payload.get("items", [])
    return {
        "total_count": payload.get("total_count", len(items)),
        "datasets": [_extract_dataset_item(item) for item in items],
        "available_subjects": DATAVERSE_SUBJECTS,
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
        query = args[1] if len(args) > 1 else "economics"
        subject = args[2] if len(args) > 2 else ""
        limit = int(args[3]) if len(args) > 3 else 20
        start = int(args[4]) if len(args) > 4 else 0
        result = search_datasets(query, subject, limit, start)
    elif command == "dataset":
        doi_or_id = args[1] if len(args) > 1 else ""
        if not doi_or_id:
            result = {"error": "doi_or_id required"}
        else:
            result = get_dataset(doi_or_id)
    elif command == "files":
        persistent_id = args[1] if len(args) > 1 else ""
        if not persistent_id:
            result = {"error": "persistent_id required"}
        else:
            result = get_dataset_files(persistent_id)
    elif command == "file":
        file_id = args[1] if len(args) > 1 else ""
        if not file_id:
            result = {"error": "file_id required"}
        else:
            result = get_file_metadata(file_id)
    elif command == "subject":
        subject = args[1] if len(args) > 1 else "Social Sciences"
        limit = int(args[2]) if len(args) > 2 else 20
        result = search_by_subject(subject, limit)
    elif command == "latest":
        limit = int(args[1]) if len(args) > 1 else 20
        result = get_latest_datasets(limit)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
