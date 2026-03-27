"""
CrossRef Data Fetcher
Fetches academic paper metadata, citations, and DOI resolution for 130M+ scholarly
articles from the CrossRef API. Supports polite pool with email header.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('CROSSREF_API_KEY', '')
POLITE_EMAIL = os.environ.get('CROSSREF_EMAIL', 'research@fincept.in')
BASE_URL = "https://api.crossref.org"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

if POLITE_EMAIL:
    session.headers.update({"User-Agent": f"FinceptTerminal/4.0 (mailto:{POLITE_EMAIL})"})


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


def _extract_work(item: Dict) -> Dict:
    paper = {
        "doi": item.get("DOI", ""),
        "title": " ".join(item.get("title", [])),
        "type": item.get("type", ""),
        "is_referenced_by_count": item.get("is-referenced-by-count", 0),
        "references_count": item.get("references-count", 0),
        "score": item.get("score", 0),
    }
    authors = []
    for author in item.get("author", []):
        name = f"{author.get('given', '')} {author.get('family', '')}".strip()
        affiliation = [a.get("name", "") for a in author.get("affiliation", [])]
        authors.append({"name": name, "orcid": author.get("ORCID", ""), "affiliation": affiliation})
    paper["authors"] = authors
    published = item.get("published", {}) or item.get("published-print", {}) or item.get("published-online", {})
    parts = published.get("date-parts", [[]])
    if parts and parts[0]:
        paper["published_year"] = parts[0][0] if len(parts[0]) > 0 else None
        paper["published_month"] = parts[0][1] if len(parts[0]) > 1 else None
        paper["published_day"] = parts[0][2] if len(parts[0]) > 2 else None
    container = item.get("container-title", [])
    paper["journal"] = container[0] if container else ""
    paper["issn"] = item.get("ISSN", [])
    paper["volume"] = item.get("volume", "")
    paper["issue"] = item.get("issue", "")
    paper["page"] = item.get("page", "")
    paper["publisher"] = item.get("publisher", "")
    paper["abstract"] = item.get("abstract", "")
    paper["url"] = item.get("URL", f"https://doi.org/{paper['doi']}")
    paper["license"] = [lic.get("URL", "") for lic in item.get("license", [])]
    paper["subject"] = item.get("subject", [])
    return paper


def search_works(query: str, filter_type: str = "", filter_value: str = "",
                 rows: int = 20, offset: int = 0) -> Dict:
    params = {
        "query": query,
        "rows": rows,
        "offset": offset,
        "select": "DOI,title,author,published,type,is-referenced-by-count,references-count,container-title,publisher,abstract,subject,ISSN,URL,score",
    }
    if filter_type and filter_value:
        params["filter"] = f"{filter_type}:{filter_value}"
    data = _make_request("works", params)
    if "error" in data:
        return data
    message = data.get("message", {})
    items = message.get("items", [])
    return {
        "total_results": message.get("total-results", 0),
        "query_terms": message.get("query", {}).get("search-terms", query),
        "offset": offset,
        "rows": rows,
        "works": [_extract_work(item) for item in items],
    }


def get_work(doi: str) -> Dict:
    encoded_doi = doi.replace("/", "%2F") if "/" in doi and not doi.startswith("http") else doi
    data = _make_request(f"works/{encoded_doi}", {})
    if "error" in data:
        return data
    message = data.get("message", {})
    return _extract_work(message)


def get_journal(issn: str) -> Dict:
    data = _make_request(f"journals/{issn}", {})
    if "error" in data:
        return data
    message = data.get("message", {})
    return {
        "issn": message.get("ISSN", []),
        "title": message.get("title", ""),
        "publisher": message.get("publisher", ""),
        "total_dois": message.get("counts", {}).get("total-dois", 0),
        "current_dois": message.get("counts", {}).get("current-dois", 0),
        "backfile_dois": message.get("counts", {}).get("backfile-dois", 0),
        "subjects": message.get("subjects", []),
        "coverage": message.get("coverage", {}),
        "flags": message.get("flags", {}),
    }


def get_funder(funder_id: str) -> Dict:
    data = _make_request(f"funders/{funder_id}", {})
    if "error" in data:
        return data
    message = data.get("message", {})
    return {
        "id": message.get("id", ""),
        "name": message.get("name", ""),
        "alt_names": message.get("alt-names", []),
        "uri": message.get("uri", ""),
        "country": message.get("country", ""),
        "location": message.get("location", ""),
        "work_count": message.get("work-count", 0),
        "descendants": message.get("descendants", []),
    }


def get_citation_count(doi: str) -> Dict:
    encoded_doi = doi.replace("/", "%2F") if "/" in doi and not doi.startswith("http") else doi
    data = _make_request(f"works/{encoded_doi}", {})
    if "error" in data:
        return data
    message = data.get("message", {})
    return {
        "doi": doi,
        "title": " ".join(message.get("title", [])),
        "is_referenced_by_count": message.get("is-referenced-by-count", 0),
        "references_count": message.get("references-count", 0),
        "published_year": (message.get("published", {}).get("date-parts", [[None]])[0] or [None])[0],
    }


def get_recent_by_subject(subject: str, rows: int = 20) -> Dict:
    params = {
        "query": subject,
        "filter": f"from-pub-date:{__import__('datetime').date.today().year - 1}",
        "rows": rows,
        "sort": "published",
        "order": "desc",
        "select": "DOI,title,author,published,type,is-referenced-by-count,container-title,publisher,abstract,subject,URL,score",
    }
    data = _make_request("works", params)
    if "error" in data:
        return data
    message = data.get("message", {})
    items = message.get("items", [])
    return {
        "subject": subject,
        "total_results": message.get("total-results", 0),
        "works": [_extract_work(item) for item in items],
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
        query = args[1] if len(args) > 1 else "financial economics"
        filter_type = args[2] if len(args) > 2 else ""
        filter_value = args[3] if len(args) > 3 else ""
        rows = int(args[4]) if len(args) > 4 else 20
        offset = int(args[5]) if len(args) > 5 else 0
        result = search_works(query, filter_type, filter_value, rows, offset)
    elif command == "work":
        doi = args[1] if len(args) > 1 else ""
        if not doi:
            result = {"error": "DOI required"}
        else:
            result = get_work(doi)
    elif command == "journal":
        issn = args[1] if len(args) > 1 else ""
        if not issn:
            result = {"error": "ISSN required"}
        else:
            result = get_journal(issn)
    elif command == "funder":
        funder_id = args[1] if len(args) > 1 else ""
        if not funder_id:
            result = {"error": "funder_id required"}
        else:
            result = get_funder(funder_id)
    elif command == "citations":
        doi = args[1] if len(args) > 1 else ""
        if not doi:
            result = {"error": "DOI required"}
        else:
            result = get_citation_count(doi)
    elif command == "recent":
        subject = args[1] if len(args) > 1 else "quantitative finance"
        rows = int(args[2]) if len(args) > 2 else 20
        result = get_recent_by_subject(subject, rows)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
