"""
Zenodo Data Fetcher
Fetches open research data from Zenodo (CERN) — datasets, software, and papers
across all scientific disciplines. Supports optional access token for higher rate limits.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ZENODO_ACCESS_TOKEN', '')
BASE_URL = "https://zenodo.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

RECORD_TYPES = {
    "dataset": "dataset",
    "software": "software",
    "paper": "publication",
    "presentation": "presentation",
    "poster": "poster",
    "report": "publication-report",
    "thesis": "publication-thesis",
    "preprint": "publication-preprint",
    "journal_article": "publication-article",
    "image": "image",
    "video": "video",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["access_token"] = API_KEY
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


def _extract_record(record: Dict) -> Dict:
    metadata = record.get("metadata", {})
    creators = metadata.get("creators", [])
    authors = [c.get("name", "") for c in creators]
    files = record.get("files", [])
    file_links = [
        {"key": f.get("key", ""), "size": f.get("size", 0), "url": f.get("links", {}).get("self", "")}
        for f in files
    ]
    return {
        "id": record.get("id", ""),
        "doi": record.get("doi", ""),
        "title": metadata.get("title", ""),
        "description": (metadata.get("description", "") or "")[:500],
        "creators": authors,
        "resource_type": metadata.get("resource_type", {}).get("type", ""),
        "publication_date": metadata.get("publication_date", ""),
        "version": metadata.get("version", ""),
        "keywords": metadata.get("keywords", []),
        "access_right": metadata.get("access_right", ""),
        "license": metadata.get("license", {}).get("id", "") if isinstance(metadata.get("license"), dict) else metadata.get("license", ""),
        "communities": [c.get("id", "") for c in metadata.get("communities", [])],
        "files": file_links,
        "stats": record.get("stats", {}),
        "url": record.get("links", {}).get("html", f"https://zenodo.org/record/{record.get('id', '')}"),
        "conceptdoi": record.get("conceptdoi", ""),
    }


def search_records(query: str, type_: str = "", keywords: str = "",
                   communities: str = "", size: int = 20) -> Dict:
    params = {
        "q": query,
        "size": min(size, 100),
        "page": 1,
        "sort": "mostrecent",
    }
    resolved_type = RECORD_TYPES.get(type_, type_)
    if resolved_type:
        params["type"] = resolved_type
    if keywords:
        params["q"] = f"{query} AND keywords:{keywords}"
    if communities:
        params["communities"] = communities
    data = _make_request("records", params)
    if "error" in data:
        return data
    hits = data.get("hits", {})
    records = hits.get("hits", [])
    return {
        "query": query,
        "total": hits.get("total", {}).get("value", len(records)) if isinstance(hits.get("total"), dict) else hits.get("total", len(records)),
        "records": [_extract_record(r) for r in records],
        "available_types": list(RECORD_TYPES.keys()),
    }


def get_record(record_id: str) -> Dict:
    data = _make_request(f"records/{record_id}", {})
    if "error" in data:
        return data
    return _extract_record(data)


def get_community_records(community_id: str, size: int = 20) -> Dict:
    params = {
        "communities": community_id,
        "size": min(size, 100),
        "page": 1,
        "sort": "mostrecent",
    }
    data = _make_request("records", params)
    if "error" in data:
        return data
    hits = data.get("hits", {})
    records = hits.get("hits", [])
    total = hits.get("total", {})
    return {
        "community_id": community_id,
        "total": total.get("value", len(records)) if isinstance(total, dict) else total,
        "records": [_extract_record(r) for r in records],
    }


def get_latest_records(type_: str = "dataset", size: int = 20) -> Dict:
    resolved_type = RECORD_TYPES.get(type_, type_)
    params = {
        "size": min(size, 100),
        "page": 1,
        "sort": "mostrecent",
    }
    if resolved_type:
        params["type"] = resolved_type
    data = _make_request("records", params)
    if "error" in data:
        return data
    hits = data.get("hits", {})
    records = hits.get("hits", [])
    total = hits.get("total", {})
    return {
        "type": type_,
        "total": total.get("value", len(records)) if isinstance(total, dict) else total,
        "records": [_extract_record(r) for r in records],
    }


def get_file_links(record_id: str) -> Dict:
    data = _make_request(f"records/{record_id}", {})
    if "error" in data:
        return data
    record = _extract_record(data)
    return {
        "record_id": record_id,
        "title": record.get("title", ""),
        "doi": record.get("doi", ""),
        "files": record.get("files", []),
        "access_right": record.get("access_right", ""),
    }


def search_by_doi(doi: str) -> Dict:
    params = {"q": f"doi:{doi}", "size": 5}
    data = _make_request("records", params)
    if "error" in data:
        return data
    hits = data.get("hits", {})
    records = hits.get("hits", [])
    if records:
        return _extract_record(records[0])
    return {"error": f"No record found for DOI: {doi}", "doi": doi}


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
        type_ = args[2] if len(args) > 2 else ""
        keywords = args[3] if len(args) > 3 else ""
        communities = args[4] if len(args) > 4 else ""
        size = int(args[5]) if len(args) > 5 else 20
        result = search_records(query, type_, keywords, communities, size)
    elif command == "record":
        record_id = args[1] if len(args) > 1 else ""
        if not record_id:
            result = {"error": "record_id required"}
        else:
            result = get_record(record_id)
    elif command == "community":
        community_id = args[1] if len(args) > 1 else ""
        if not community_id:
            result = {"error": "community_id required"}
        else:
            size = int(args[2]) if len(args) > 2 else 20
            result = get_community_records(community_id, size)
    elif command == "latest":
        type_ = args[1] if len(args) > 1 else "dataset"
        size = int(args[2]) if len(args) > 2 else 20
        result = get_latest_records(type_, size)
    elif command == "files":
        record_id = args[1] if len(args) > 1 else ""
        if not record_id:
            result = {"error": "record_id required"}
        else:
            result = get_file_links(record_id)
    elif command == "doi":
        doi = args[1] if len(args) > 1 else ""
        if not doi:
            result = {"error": "DOI required"}
        else:
            result = search_by_doi(doi)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
