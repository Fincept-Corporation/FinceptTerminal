"""
Semantic Scholar Data Fetcher
AI-powered academic search with citation graphs and paper recommendations
for 200M+ papers from the Semantic Scholar API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('SEMANTIC_SCHOLAR_API_KEY', '')
BASE_URL = "https://api.semanticscholar.org/graph/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if API_KEY:
    session.headers.update({"x-api-key": API_KEY})

DEFAULT_PAPER_FIELDS = (
    "paperId,externalIds,title,abstract,year,authors,citationCount,"
    "referenceCount,influentialCitationCount,isOpenAccess,openAccessPdf,"
    "fieldsOfStudy,s2FieldsOfStudy,publicationTypes,publicationDate,"
    "journal,venue,url"
)

DEFAULT_AUTHOR_FIELDS = (
    "authorId,externalIds,name,affiliations,homepage,paperCount,"
    "citationCount,hIndex"
)


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


def search_papers(query: str, fields: str = "", limit: int = 20, offset: int = 0) -> Dict:
    params = {
        "query": query,
        "limit": min(limit, 100),
        "offset": offset,
        "fields": fields if fields else DEFAULT_PAPER_FIELDS,
    }
    data = _make_request("paper/search", params)
    if "error" in data:
        return data
    return {
        "total": data.get("total", 0),
        "offset": data.get("offset", 0),
        "next": data.get("next", None),
        "papers": data.get("data", []),
    }


def get_paper(paper_id: str, fields: str = "") -> Dict:
    params = {"fields": fields if fields else DEFAULT_PAPER_FIELDS}
    data = _make_request(f"paper/{paper_id}", params)
    return data


def get_paper_citations(paper_id: str, limit: int = 50) -> Dict:
    params = {
        "limit": min(limit, 1000),
        "fields": "paperId,title,year,authors,citationCount,externalIds,abstract",
    }
    data = _make_request(f"paper/{paper_id}/citations", params)
    if "error" in data:
        return data
    return {
        "paper_id": paper_id,
        "total": data.get("total", 0),
        "offset": data.get("offset", 0),
        "citations": data.get("data", []),
    }


def get_paper_references(paper_id: str, limit: int = 50) -> Dict:
    params = {
        "limit": min(limit, 1000),
        "fields": "paperId,title,year,authors,citationCount,externalIds,abstract",
    }
    data = _make_request(f"paper/{paper_id}/references", params)
    if "error" in data:
        return data
    return {
        "paper_id": paper_id,
        "total": data.get("total", 0),
        "offset": data.get("offset", 0),
        "references": data.get("data", []),
    }


def get_author(author_id: str, fields: str = "") -> Dict:
    params = {"fields": fields if fields else DEFAULT_AUTHOR_FIELDS}
    data = _make_request(f"author/{author_id}", params)
    if "error" in data:
        return data
    paper_params = {
        "limit": 50,
        "fields": "paperId,title,year,citationCount,externalIds",
    }
    papers_data = _make_request(f"author/{author_id}/papers", paper_params)
    result = dict(data)
    if "error" not in papers_data:
        result["papers"] = papers_data.get("data", [])
        result["papers_total"] = papers_data.get("total", 0)
    return result


def get_recommended(paper_id: str, limit: int = 10) -> Dict:
    params = {
        "limit": min(limit, 500),
        "fields": DEFAULT_PAPER_FIELDS,
    }
    rec_url = f"https://api.semanticscholar.org/recommendations/v1/papers/forpaper/{paper_id}"
    data = _make_request(rec_url, params)
    if "error" in data:
        return data
    return {
        "paper_id": paper_id,
        "recommendations": data.get("recommendedPapers", []),
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
        query = args[1] if len(args) > 1 else "quantitative finance"
        fields = args[2] if len(args) > 2 else ""
        limit = int(args[3]) if len(args) > 3 else 20
        offset = int(args[4]) if len(args) > 4 else 0
        result = search_papers(query, fields, limit, offset)
    elif command == "paper":
        paper_id = args[1] if len(args) > 1 else ""
        if not paper_id:
            result = {"error": "paper_id required"}
        else:
            fields = args[2] if len(args) > 2 else ""
            result = get_paper(paper_id, fields)
    elif command == "citations":
        paper_id = args[1] if len(args) > 1 else ""
        if not paper_id:
            result = {"error": "paper_id required"}
        else:
            limit = int(args[2]) if len(args) > 2 else 50
            result = get_paper_citations(paper_id, limit)
    elif command == "references":
        paper_id = args[1] if len(args) > 1 else ""
        if not paper_id:
            result = {"error": "paper_id required"}
        else:
            limit = int(args[2]) if len(args) > 2 else 50
            result = get_paper_references(paper_id, limit)
    elif command == "author":
        author_id = args[1] if len(args) > 1 else ""
        if not author_id:
            result = {"error": "author_id required"}
        else:
            fields = args[2] if len(args) > 2 else ""
            result = get_author(author_id, fields)
    elif command == "recommended":
        paper_id = args[1] if len(args) > 1 else ""
        if not paper_id:
            result = {"error": "paper_id required"}
        else:
            limit = int(args[2]) if len(args) > 2 else 10
            result = get_recommended(paper_id, limit)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
