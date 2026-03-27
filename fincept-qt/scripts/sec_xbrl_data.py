"""
SEC EDGAR XBRL Data Fetcher
SEC EDGAR XBRL Company Facts: structured financial data — revenue, EPS,
balance sheet for all public US companies (no API key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://data.sec.gov/api/xbrl"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({
    "User-Agent": "FinceptTerminal/4.0 (support@fincept.in)",
    "Accept": "application/json"
})


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


def _format_cik(cik: str) -> str:
    return str(cik).zfill(10)


def get_company_facts(cik: str) -> Any:
    cik_fmt = _format_cik(cik)
    return _make_request(f"companyfacts/CIK{cik_fmt}.json")


def get_company_concept(cik: str, taxonomy: str = "us-gaap",
                         tag: str = "Revenues") -> Any:
    cik_fmt = _format_cik(cik)
    return _make_request(f"companyconcept/CIK{cik_fmt}/{taxonomy}/{tag}.json")


def get_frames(taxonomy: str = "us-gaap", tag: str = "Revenues",
               unit: str = "USD", period: str = "CY2023Q4I") -> Any:
    return _make_request(f"frames/{taxonomy}/{tag}/{unit}/{period}.json")


def get_company_tickers() -> Any:
    try:
        resp = session.get("https://www.sec.gov/files/company_tickers.json", timeout=30)
        resp.raise_for_status()
        data = resp.json()
        result = []
        for key, val in data.items():
            result.append({
                "cik": val.get("cik_str"),
                "ticker": val.get("ticker"),
                "title": val.get("title")
            })
        return {"count": len(result), "tickers": result}
    except Exception as e:
        return {"error": f"Failed to fetch tickers: {str(e)}"}


def search_company(name_or_ticker: str) -> Any:
    tickers_data = get_company_tickers()
    if "error" in tickers_data:
        return tickers_data
    query = name_or_ticker.lower()
    matches = []
    for t in tickers_data.get("tickers", []):
        ticker = (t.get("ticker") or "").lower()
        title = (t.get("title") or "").lower()
        if query in ticker or query in title:
            matches.append(t)
        if len(matches) >= 20:
            break
    return {"query": name_or_ticker, "count": len(matches), "results": matches}


def get_recent_filings(cik: str) -> Any:
    cik_fmt = _format_cik(cik)
    try:
        resp = session.get(
            f"https://data.sec.gov/submissions/CIK{cik_fmt}.json", timeout=30
        )
        resp.raise_for_status()
        data = resp.json()
        recent = data.get("filings", {}).get("recent", {})
        filings = []
        forms = recent.get("form", [])
        dates = recent.get("filingDate", [])
        accessions = recent.get("accessionNumber", [])
        descriptions = recent.get("primaryDocument", [])
        for i in range(min(20, len(forms))):
            filings.append({
                "form": forms[i] if i < len(forms) else "",
                "date": dates[i] if i < len(dates) else "",
                "accession": accessions[i] if i < len(accessions) else "",
                "document": descriptions[i] if i < len(descriptions) else ""
            })
        return {
            "cik": cik,
            "name": data.get("name", ""),
            "ticker": data.get("tickers", []),
            "sic": data.get("sic", ""),
            "sic_description": data.get("sicDescription", ""),
            "recent_filings": filings
        }
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "facts":
        cik = args[1] if len(args) > 1 else "320193"
        result = get_company_facts(cik)
    elif command == "concept":
        cik = args[1] if len(args) > 1 else "320193"
        taxonomy = args[2] if len(args) > 2 else "us-gaap"
        tag = args[3] if len(args) > 3 else "Revenues"
        result = get_company_concept(cik, taxonomy, tag)
    elif command == "frames":
        taxonomy = args[1] if len(args) > 1 else "us-gaap"
        tag = args[2] if len(args) > 2 else "Revenues"
        unit = args[3] if len(args) > 3 else "USD"
        period = args[4] if len(args) > 4 else "CY2023Q4I"
        result = get_frames(taxonomy, tag, unit, period)
    elif command == "tickers":
        result = get_company_tickers()
    elif command == "search":
        query = args[1] if len(args) > 1 else "Apple"
        result = search_company(query)
    elif command == "filings":
        cik = args[1] if len(args) > 1 else "320193"
        result = get_recent_filings(cik)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
