"""
Quiver Quantitative Data Fetcher
Congress trades, government contracts, insider transactions, lobbying — free tier.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc

API_KEY = os.environ.get('QUIVER_API_KEY', '')
BASE_URL = "https://api.quiverquant.com/beta"

SERVICE = "Quiver Quantitative"
KEY_ENV = "QUIVER_API_KEY"

session = fc.http_session()

def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://www.quiverquant.com/", KEY_ENV)
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"Accept": "application/json", "Authorization": f"Token {API_KEY}"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    except ValueError:
        return {"error": f"{SERVICE} returned a response that is not JSON"}

def get_congress_trades(ticker: str, limit: int = 50) -> Any:
    return _make_request(f"historical/congresstrading/{ticker}", {"limit": limit})

def get_government_contracts(ticker: str, limit: int = 50) -> Any:
    return _make_request(f"historical/govcontractsall/{ticker}", {"limit": limit})

def get_insider_trades(ticker: str, limit: int = 50) -> Any:
    return _make_request(f"historical/insiders/{ticker}", {"limit": limit})

def get_lobbying(ticker: str, limit: int = 50) -> Any:
    return _make_request(f"historical/lobbying/{ticker}", {"limit": limit})

def get_twitter_followers(ticker: str) -> Any:
    return _make_request(f"historical/twitter/{ticker}", {})

def get_patent_data(ticker: str) -> Any:
    return _make_request(f"historical/patents/{ticker}", {})

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "congress":
        ticker = args[1] if len(args) > 1 else "AAPL"
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_congress_trades(ticker, limit)
    elif command == "contracts":
        ticker = args[1] if len(args) > 1 else "AAPL"
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_government_contracts(ticker, limit)
    elif command == "insiders":
        ticker = args[1] if len(args) > 1 else "AAPL"
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_insider_trades(ticker, limit)
    elif command == "lobbying":
        ticker = args[1] if len(args) > 1 else "AAPL"
        limit = int(args[2]) if len(args) > 2 else 50
        result = get_lobbying(ticker, limit)
    elif command == "twitter":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_twitter_followers(ticker)
    elif command == "patents":
        ticker = args[1] if len(args) > 1 else "AAPL"
        result = get_patent_data(ticker)
    print(json.dumps(result))

if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # bad argument (e.g. a non-numeric limit): still answer in JSON
        print(json.dumps({"error": fc.scrub(f"{type(e).__name__}: {e}", API_KEY)}))
