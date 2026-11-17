"""
Intrinio Data Fetcher
Financial data, news sentiment, economic indicators — free tier.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('INTRINIO_API_KEY', '')
BASE_URL = "https://api-v2.intrinio.com"
SERVICE = "Intrinio"

session = fc.http_session()

def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every endpoint needs the key: say so before any network call instead of an upstream 401.
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://intrinio.com/", "INTRINIO_API_KEY")
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, "INTRINIO_API_KEY", API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}
    if isinstance(data, dict) and data.get("error") and not data.get("data"):
        return {"error": f"{SERVICE}: {fc.scrub(data.get('message') or data['error'], API_KEY)}"}
    return data

def get_company_search(query: str, limit: int = 20) -> Any:
    return _make_request("companies/search", {"api_key": API_KEY, "query": query, "page_size": limit})

def get_security_prices(identifier: str, start_date: str = None, end_date: str = None) -> Any:
    params = {"api_key": API_KEY}
    if start_date:
        params["start_date"] = start_date
    if end_date:
        params["end_date"] = end_date
    return _make_request(f"securities/{identifier}/prices", params)

def get_company_news(identifier: str, limit: int = 20) -> Any:
    return _make_request(f"companies/{identifier}/news", {"api_key": API_KEY, "page_size": limit})

def get_economic_index(identifier: str, start_date: str = None, end_date: str = None) -> Any:
    params = {"api_key": API_KEY}
    if start_date:
        params["start_date"] = start_date
    if end_date:
        params["end_date"] = end_date
    return _make_request(f"indices/economic/{identifier}/historical_data/level", params)

def get_data_tags(type_: str = "financial") -> Any:
    return _make_request("data_tags", {"api_key": API_KEY, "type": type_})

def get_company_fundamentals(identifier: str, statement: str = "income_statement", period: str = "annual", type_: str = "reported") -> Any:
    return _make_request(f"companies/{identifier}/fundamentals", {"api_key": API_KEY, "statement_code": statement, "period_type": period, "type": type_})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "search":
        query = args[1] if len(args) > 1 else "Apple"
        limit = int(args[2]) if len(args) > 2 else 20
        result = get_company_search(query, limit)
    elif command == "prices":
        identifier = args[1] if len(args) > 1 else "AAPL"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_security_prices(identifier, start_date, end_date)
    elif command == "news":
        identifier = args[1] if len(args) > 1 else "AAPL"
        limit = int(args[2]) if len(args) > 2 else 20
        result = get_company_news(identifier, limit)
    elif command == "economic":
        identifier = args[1] if len(args) > 1 else "GDP"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_economic_index(identifier, start_date, end_date)
    elif command == "tags":
        type_ = args[1] if len(args) > 1 else "financial"
        result = get_data_tags(type_)
    elif command == "fundamentals":
        identifier = args[1] if len(args) > 1 else "AAPL"
        statement = args[2] if len(args) > 2 else "income_statement"
        period = args[3] if len(args) > 3 else "annual"
        type_ = args[4] if len(args) > 4 else "reported"
        result = get_company_fundamentals(identifier, statement, period, type_)
    print(fincept_json.dumps(result))

if __name__ == "__main__":
    main()
