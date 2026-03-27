"""
ArXiv Data Fetcher
Fetches research papers in finance, economics, machine learning, and quantitative finance
from the ArXiv API — full metadata and abstracts.
"""
import sys
import json
import os
import requests
import xml.etree.ElementTree as ET
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('ARXIV_API_KEY', '')
BASE_URL = "http://export.arxiv.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

ARXIV_NS = "http://www.w3.org/2005/Atom"
OPENSEARCH_NS = "http://a9.com/-/spec/opensearch/1.1/"

QUANT_FINANCE_SUBCATEGORIES = {
    "portfolio": "q-fin.PM",
    "risk": "q-fin.RM",
    "pricing": "q-fin.PR",
    "trading": "q-fin.TR",
    "computational": "q-fin.CP",
    "economics": "q-fin.EC",
    "general_finance": "q-fin.GN",
    "statistics": "q-fin.ST",
    "mathematical_finance": "q-fin.MF",
}

ML_SUBCATEGORIES = {
    "learning": "cs.LG",
    "ai": "cs.AI",
    "neural_networks": "cs.NE",
    "cv": "cs.CV",
    "nlp": "cs.CL",
    "stats_ml": "stat.ML",
}


def _parse_atom_entry(entry) -> Dict:
    ns = ARXIV_NS
    arxiv_ns = "http://arxiv.org/schemas/atom"

    def tag(name, namespace=ns):
        return f"{{{namespace}}}{name}"

    paper = {}
    id_elem = entry.find(tag("id"))
    if id_elem is not None:
        full_id = id_elem.text.strip()
        paper["arxiv_id"] = full_id.split("/abs/")[-1]
        paper["url"] = full_id
    title_elem = entry.find(tag("title"))
    if title_elem is not None:
        paper["title"] = " ".join(title_elem.text.strip().split())
    summary_elem = entry.find(tag("summary"))
    if summary_elem is not None:
        paper["abstract"] = " ".join(summary_elem.text.strip().split())
    published_elem = entry.find(tag("published"))
    if published_elem is not None:
        paper["published"] = published_elem.text.strip()
    updated_elem = entry.find(tag("updated"))
    if updated_elem is not None:
        paper["updated"] = updated_elem.text.strip()
    authors = []
    for author_elem in entry.findall(tag("author")):
        name_elem = author_elem.find(tag("name"))
        if name_elem is not None:
            authors.append(name_elem.text.strip())
    paper["authors"] = authors
    categories = []
    for cat_elem in entry.findall(tag("category")):
        term = cat_elem.get("term", "")
        if term:
            categories.append(term)
    paper["categories"] = categories
    primary_elem = entry.find(f"{{{arxiv_ns}}}primary_category")
    if primary_elem is not None:
        paper["primary_category"] = primary_elem.get("term", "")
    doi_elem = entry.find(f"{{{arxiv_ns}}}doi")
    if doi_elem is not None:
        paper["doi"] = doi_elem.text.strip()
    comment_elem = entry.find(f"{{{arxiv_ns}}}comment")
    if comment_elem is not None:
        paper["comment"] = comment_elem.text.strip()
    for link_elem in entry.findall(tag("link")):
        if link_elem.get("type") == "application/pdf":
            paper["pdf_url"] = link_elem.get("href", "")
    return paper


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.text
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def _parse_atom_response(xml_text: str) -> Dict:
    if isinstance(xml_text, dict):
        return xml_text
    try:
        root = ET.fromstring(xml_text)
        ns = ARXIV_NS
        opensearch_ns = OPENSEARCH_NS

        def tag(name, namespace=ns):
            return f"{{{namespace}}}{name}"

        total_elem = root.find(f"{{{opensearch_ns}}}totalResults")
        total = int(total_elem.text) if total_elem is not None else 0
        start_elem = root.find(f"{{{opensearch_ns}}}startIndex")
        start = int(start_elem.text) if start_elem is not None else 0
        per_page_elem = root.find(f"{{{opensearch_ns}}}itemsPerPage")
        per_page = int(per_page_elem.text) if per_page_elem is not None else 0
        papers = []
        for entry in root.findall(tag("entry")):
            papers.append(_parse_atom_entry(entry))
        return {
            "total_results": total,
            "start_index": start,
            "items_per_page": per_page,
            "papers": papers,
        }
    except ET.ParseError as e:
        return {"error": f"XML parse error: {str(e)}"}


def search_papers(query: str, category: str = "", max_results: int = 10, start: int = 0) -> Dict:
    search_query = query
    if category:
        search_query = f"({query}) AND cat:{category}"
    params = {
        "search_query": f"all:{search_query}",
        "start": start,
        "max_results": max_results,
        "sortBy": "relevance",
        "sortOrder": "descending",
    }
    raw = _make_request("query", params)
    return _parse_atom_response(raw)


def get_paper(arxiv_id: str) -> Dict:
    params = {"id_list": arxiv_id}
    raw = _make_request("query", params)
    result = _parse_atom_response(raw)
    if "papers" in result and result["papers"]:
        return result["papers"][0]
    return {"error": f"Paper not found: {arxiv_id}"}


def get_recent_papers(category: str = "q-fin", max_results: int = 20) -> Dict:
    params = {
        "search_query": f"cat:{category}",
        "start": 0,
        "max_results": max_results,
        "sortBy": "submittedDate",
        "sortOrder": "descending",
    }
    raw = _make_request("query", params)
    return _parse_atom_response(raw)


def get_quant_finance_papers(subcategory: str = "", max_results: int = 20) -> Dict:
    if subcategory and subcategory in QUANT_FINANCE_SUBCATEGORIES:
        cat = QUANT_FINANCE_SUBCATEGORIES[subcategory]
    elif subcategory and subcategory.startswith("q-fin"):
        cat = subcategory
    else:
        cat = "q-fin"
    params = {
        "search_query": f"cat:{cat}",
        "start": 0,
        "max_results": max_results,
        "sortBy": "submittedDate",
        "sortOrder": "descending",
    }
    raw = _make_request("query", params)
    result = _parse_atom_response(raw)
    result["subcategory"] = cat
    result["available_subcategories"] = QUANT_FINANCE_SUBCATEGORIES
    return result


def get_ml_papers(subcategory: str = "", max_results: int = 20) -> Dict:
    if subcategory and subcategory in ML_SUBCATEGORIES:
        cat = ML_SUBCATEGORIES[subcategory]
    elif subcategory and "." in subcategory:
        cat = subcategory
    else:
        cat = "cs.LG"
    params = {
        "search_query": f"cat:{cat}",
        "start": 0,
        "max_results": max_results,
        "sortBy": "submittedDate",
        "sortOrder": "descending",
    }
    raw = _make_request("query", params)
    result = _parse_atom_response(raw)
    result["subcategory"] = cat
    result["available_subcategories"] = ML_SUBCATEGORIES
    return result


def get_author_papers(author_name: str, max_results: int = 20) -> Dict:
    params = {
        "search_query": f"au:{author_name}",
        "start": 0,
        "max_results": max_results,
        "sortBy": "submittedDate",
        "sortOrder": "descending",
    }
    raw = _make_request("query", params)
    result = _parse_atom_response(raw)
    result["author_query"] = author_name
    return result


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "search":
        query = args[1] if len(args) > 1 else "financial markets"
        category = args[2] if len(args) > 2 else ""
        max_results = int(args[3]) if len(args) > 3 else 10
        start = int(args[4]) if len(args) > 4 else 0
        result = search_papers(query, category, max_results, start)
    elif command == "paper":
        arxiv_id = args[1] if len(args) > 1 else ""
        if not arxiv_id:
            result = {"error": "arxiv_id required"}
        else:
            result = get_paper(arxiv_id)
    elif command == "recent":
        category = args[1] if len(args) > 1 else "q-fin"
        max_results = int(args[2]) if len(args) > 2 else 20
        result = get_recent_papers(category, max_results)
    elif command == "quant_finance":
        subcategory = args[1] if len(args) > 1 else ""
        max_results = int(args[2]) if len(args) > 2 else 20
        result = get_quant_finance_papers(subcategory, max_results)
    elif command == "ml":
        subcategory = args[1] if len(args) > 1 else ""
        max_results = int(args[2]) if len(args) > 2 else 20
        result = get_ml_papers(subcategory, max_results)
    elif command == "author":
        author_name = args[1] if len(args) > 1 else ""
        if not author_name:
            result = {"error": "author_name required"}
        else:
            max_results = int(args[2]) if len(args) > 2 else 20
            result = get_author_papers(author_name, max_results)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
