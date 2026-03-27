"""
SSRN Data Fetcher
Fetches working papers from the Social Science Research Network — finance, economics,
accounting — including abstracts, citations, and download statistics.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('SSRN_API_KEY', '')
BASE_URL = "https://api.ssrn.com/content/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

SSRN_NETWORKS = {
    "finance": "Finance",
    "economics": "Economics",
    "accounting": "Accounting",
    "law": "Law",
    "management": "Management",
    "marketing": "Marketing",
    "political_science": "Political Science",
    "psychology": "Psychology",
    "real_estate": "Real Estate",
}

SSRN_TOPICS = {
    "asset_pricing": "asset pricing",
    "corporate_finance": "corporate finance",
    "behavioral_finance": "behavioral finance",
    "derivatives": "derivatives",
    "fixed_income": "fixed income",
    "portfolio": "portfolio management",
    "risk_management": "risk management",
    "market_microstructure": "market microstructure",
    "macro_finance": "macro finance",
    "fintech": "fintech",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    headers = {}
    if API_KEY:
        headers["Authorization"] = f"Bearer {API_KEY}"
    try:
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _make_public_request(url: str, params: Dict = None) -> Any:
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


def search_papers(query: str, journal: str = "", start_date: str = "", limit: int = 20) -> Dict:
    params = {
        "query": query,
        "limit": limit,
    }
    if journal:
        params["journal"] = journal
    if start_date:
        params["start_date"] = start_date
    result = _make_request("papers/search", params)
    if "error" in result:
        fallback_url = "https://api.ssrn.com/content/v1/papers"
        fallback_params = {"q": query, "limit": limit}
        result = _make_public_request(fallback_url, fallback_params)
        if "error" in result:
            return {
                "query": query,
                "note": "SSRN public API access may require authentication",
                "ssrn_search_url": f"https://papers.ssrn.com/sol3/results.cfm?RequestTimeout=50000&txtKey_Words={query.replace(' ', '+')}",
                "suggested_approach": "Use SSRN website search or obtain API credentials",
                "available_networks": SSRN_NETWORKS,
                "available_topics": SSRN_TOPICS,
            }
    return result


def get_paper_details(abstract_id: str) -> Dict:
    result = _make_request(f"papers/{abstract_id}", {})
    if "error" in result:
        return {
            "abstract_id": abstract_id,
            "ssrn_url": f"https://papers.ssrn.com/sol3/papers.cfm?abstract_id={abstract_id}",
            "note": "Direct paper metadata requires SSRN API access",
            "download_link": f"https://ssrn.com/abstract={abstract_id}",
        }
    return result


def get_recent_by_topic(topic: str, limit: int = 20) -> Dict:
    topic_query = SSRN_TOPICS.get(topic, topic)
    params = {
        "q": topic_query,
        "limit": limit,
        "sort": "date_posted",
        "order": "desc",
    }
    result = _make_request("papers", params)
    if "error" in result:
        return {
            "topic": topic,
            "topic_query": topic_query,
            "ssrn_search_url": f"https://papers.ssrn.com/sol3/results.cfm?txtKey_Words={topic_query.replace(' ', '+')}",
            "available_topics": SSRN_TOPICS,
            "note": "SSRN API access required for programmatic results",
        }
    return result


def get_top_downloads(network: str = "finance", limit: int = 20) -> Dict:
    network_name = SSRN_NETWORKS.get(network, network)
    params = {
        "network": network_name,
        "limit": limit,
        "sort": "downloads",
        "order": "desc",
    }
    result = _make_request("papers/top", params)
    if "error" in result:
        return {
            "network": network,
            "network_name": network_name,
            "ssrn_top_url": f"https://papers.ssrn.com/sol3/topten/topTenResults.cfm?subjectGroupCode={network}",
            "available_networks": SSRN_NETWORKS,
            "note": "SSRN API access required for programmatic results",
        }
    return result


def get_author_papers(author_id: str) -> Dict:
    result = _make_request(f"authors/{author_id}/papers", {})
    if "error" in result:
        return {
            "author_id": author_id,
            "ssrn_author_url": f"https://papers.ssrn.com/sol3/cf_dev/AbsByAuth.cfm?per_id={author_id}",
            "note": "SSRN API access required for programmatic author paper listing",
        }
    return result


def get_networks() -> Dict:
    return {
        "networks": SSRN_NETWORKS,
        "topics": SSRN_TOPICS,
        "ssrn_base_url": "https://www.ssrn.com",
        "api_base_url": BASE_URL,
        "note": "SSRN organizes papers into subject-matter networks",
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
        query = args[1] if len(args) > 1 else "asset pricing"
        journal = args[2] if len(args) > 2 else ""
        start_date = args[3] if len(args) > 3 else ""
        limit = int(args[4]) if len(args) > 4 else 20
        result = search_papers(query, journal, start_date, limit)
    elif command == "paper":
        abstract_id = args[1] if len(args) > 1 else ""
        if not abstract_id:
            result = {"error": "abstract_id required"}
        else:
            result = get_paper_details(abstract_id)
    elif command == "recent":
        topic = args[1] if len(args) > 1 else "asset_pricing"
        limit = int(args[2]) if len(args) > 2 else 20
        result = get_recent_by_topic(topic, limit)
    elif command == "top_downloads":
        network = args[1] if len(args) > 1 else "finance"
        limit = int(args[2]) if len(args) > 2 else 20
        result = get_top_downloads(network, limit)
    elif command == "author":
        author_id = args[1] if len(args) > 1 else ""
        if not author_id:
            result = {"error": "author_id required"}
        else:
            result = get_author_papers(author_id)
    elif command == "networks":
        result = get_networks()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
