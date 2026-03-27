"""
OpenCorporates Data Fetcher
Fetches global corporate registry data — company information, officers, and filings
for 200M+ companies worldwide. Requires a free API key for full access.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OC_API_KEY', '')
BASE_URL = "https://api.opencorporates.com/v0.4"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

JURISDICTIONS = {
    "us_de": "Delaware, USA",
    "us_ca": "California, USA",
    "us_ny": "New York, USA",
    "us_tx": "Texas, USA",
    "us_fl": "Florida, USA",
    "us": "United States (Federal)",
    "gb": "United Kingdom",
    "de": "Germany",
    "fr": "France",
    "nl": "Netherlands",
    "ch": "Switzerland",
    "ie": "Ireland",
    "lu": "Luxembourg",
    "sg": "Singapore",
    "hk": "Hong Kong",
    "au": "Australia",
    "ca_on": "Ontario, Canada",
    "ca": "Canada (Federal)",
    "in": "India",
    "jp": "Japan",
    "kr": "South Korea",
    "br": "Brazil",
    "mx": "Mexico",
    "za": "South Africa",
    "ae": "United Arab Emirates",
    "ky": "Cayman Islands",
    "bvi": "British Virgin Islands",
    "je": "Jersey",
    "gg": "Guernsey",
    "im": "Isle of Man",
    "li": "Liechtenstein",
    "cy": "Cyprus",
    "mt": "Malta",
    "es": "Spain",
    "it": "Italy",
    "se": "Sweden",
    "no": "Norway",
    "dk": "Denmark",
    "fi": "Finland",
    "pl": "Poland",
    "nz": "New Zealand",
    "ar": "Argentina",
    "cl": "Chile",
    "co": "Colombia",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    if API_KEY:
        params["api_token"] = API_KEY
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


def _extract_company(company: Dict) -> Dict:
    return {
        "name": company.get("name", ""),
        "company_number": company.get("company_number", ""),
        "jurisdiction_code": company.get("jurisdiction_code", ""),
        "jurisdiction_name": JURISDICTIONS.get(company.get("jurisdiction_code", ""), company.get("jurisdiction_code", "")),
        "registered_address": company.get("registered_address_in_full", ""),
        "incorporation_date": company.get("incorporation_date", ""),
        "dissolution_date": company.get("dissolution_date", ""),
        "company_type": company.get("company_type", ""),
        "current_status": company.get("current_status", ""),
        "source_url": company.get("source_url", ""),
        "opencorporates_url": company.get("opencorporates_url", ""),
        "inactive": company.get("inactive", False),
        "branch": company.get("branch", ""),
        "business_number": company.get("business_number", ""),
        "native_company_number": company.get("native_company_number", ""),
    }


def search_companies(query: str, jurisdiction: str = "", limit: int = 20) -> Dict:
    params = {
        "q": query,
        "per_page": min(limit, 100),
        "page": 1,
    }
    if jurisdiction:
        params["jurisdiction_code"] = jurisdiction
    data = _make_request("companies/search", params)
    if "error" in data:
        return data
    resp = data.get("results", {})
    companies_raw = resp.get("companies", [])
    return {
        "query": query,
        "jurisdiction": jurisdiction,
        "total_count": resp.get("total_count", len(companies_raw)),
        "page": resp.get("page", 1),
        "per_page": resp.get("per_page", limit),
        "companies": [_extract_company(c.get("company", c)) for c in companies_raw],
    }


def get_company(company_number: str, jurisdiction: str) -> Dict:
    data = _make_request(f"companies/{jurisdiction}/{company_number}", {})
    if "error" in data:
        return data
    company = data.get("results", {}).get("company", data)
    return _extract_company(company)


def get_officer(officer_id: str) -> Dict:
    data = _make_request(f"officers/{officer_id}", {})
    if "error" in data:
        return data
    officer = data.get("results", {}).get("officer", data)
    return {
        "id": officer.get("id", ""),
        "name": officer.get("name", ""),
        "position": officer.get("position", ""),
        "uid": officer.get("uid", ""),
        "start_date": officer.get("start_date", ""),
        "end_date": officer.get("end_date", ""),
        "opencorporates_url": officer.get("opencorporates_url", ""),
        "company": {
            "name": officer.get("company", {}).get("name", ""),
            "jurisdiction": officer.get("company", {}).get("jurisdiction_code", ""),
            "company_number": officer.get("company", {}).get("company_number", ""),
        },
    }


def search_officers(query: str, jurisdiction: str = "", limit: int = 20) -> Dict:
    params = {
        "q": query,
        "per_page": min(limit, 100),
        "page": 1,
    }
    if jurisdiction:
        params["jurisdiction_code"] = jurisdiction
    data = _make_request("officers/search", params)
    if "error" in data:
        return data
    resp = data.get("results", {})
    officers_raw = resp.get("officers", [])
    return {
        "query": query,
        "jurisdiction": jurisdiction,
        "total_count": resp.get("total_count", len(officers_raw)),
        "officers": [
            {
                "id": o.get("officer", o).get("id", ""),
                "name": o.get("officer", o).get("name", ""),
                "position": o.get("officer", o).get("position", ""),
                "company_name": o.get("officer", o).get("company", {}).get("name", ""),
                "jurisdiction": o.get("officer", o).get("company", {}).get("jurisdiction_code", ""),
                "opencorporates_url": o.get("officer", o).get("opencorporates_url", ""),
            }
            for o in officers_raw
        ],
    }


def get_filings(company_number: str, jurisdiction: str) -> Dict:
    params = {"per_page": 50}
    data = _make_request(f"companies/{jurisdiction}/{company_number}/filings", params)
    if "error" in data:
        return data
    resp = data.get("results", {})
    filings_raw = resp.get("filings", [])
    return {
        "company_number": company_number,
        "jurisdiction": jurisdiction,
        "total_count": resp.get("total_count", len(filings_raw)),
        "filings": [
            {
                "id": f.get("filing", f).get("id", ""),
                "title": f.get("filing", f).get("title", ""),
                "date": f.get("filing", f).get("date", ""),
                "filing_type": f.get("filing", f).get("filing_type", ""),
                "description": f.get("filing", f).get("description", ""),
                "url": f.get("filing", f).get("url", ""),
                "opencorporates_url": f.get("filing", f).get("opencorporates_url", ""),
            }
            for f in filings_raw
        ],
    }


def get_jurisdictions() -> Dict:
    return {
        "total_jurisdictions": len(JURISDICTIONS),
        "jurisdictions": JURISDICTIONS,
        "opencorporates_url": "https://opencorporates.com/registers",
        "note": "OpenCorporates covers 140+ jurisdictions. Shown are commonly used ones.",
        "api_docs": "https://api.opencorporates.com/documentation/API-Reference",
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
        query = args[1] if len(args) > 1 else ""
        if not query:
            result = {"error": "query required"}
        else:
            jurisdiction = args[2] if len(args) > 2 else ""
            limit = int(args[3]) if len(args) > 3 else 20
            result = search_companies(query, jurisdiction, limit)
    elif command == "company":
        if len(args) < 3:
            result = {"error": "company_number and jurisdiction required"}
        else:
            result = get_company(args[1], args[2])
    elif command == "officer":
        officer_id = args[1] if len(args) > 1 else ""
        if not officer_id:
            result = {"error": "officer_id required"}
        else:
            result = get_officer(officer_id)
    elif command == "search_officers":
        query = args[1] if len(args) > 1 else ""
        if not query:
            result = {"error": "query required"}
        else:
            jurisdiction = args[2] if len(args) > 2 else ""
            limit = int(args[3]) if len(args) > 3 else 20
            result = search_officers(query, jurisdiction, limit)
    elif command == "filings":
        if len(args) < 3:
            result = {"error": "company_number and jurisdiction required"}
        else:
            result = get_filings(args[1], args[2])
    elif command == "jurisdictions":
        result = get_jurisdictions()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
