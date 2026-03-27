"""
UN Comtrade Extended Data Fetcher
Detailed bilateral trade flows by HS chapter, trade balances, and services trade
via the UN Comtrade public preview API (500 records per call, no key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('COMTRADE_API_KEY', '')
BASE_URL = "https://comtradeapi.un.org/public/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {}
        if API_KEY:
            headers["Ocp-Apim-Subscription-Key"] = API_KEY
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_trade_data(reporter: str, partner: str = "0", commodity: str = "TOTAL",
                   year: str = None, flow: str = "X") -> Any:
    params = {
        "reporterCode": reporter,
        "partnerCode": partner,
        "cmdCode": commodity,
        "flowCode": flow,
        "period": year or "2022",
        "motCode": "0",
        "partner2Code": "0",
        "aggregateBy": "defaultAggregation",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _make_request("preview/C/A/HS", params)


def get_trade_balance(reporter: str, year: str = None) -> Any:
    params = {
        "reporterCode": reporter,
        "partnerCode": "0",
        "cmdCode": "TOTAL",
        "flowCode": "X,M",
        "period": year or "2022",
        "motCode": "0",
        "partner2Code": "0",
        "aggregateBy": "defaultAggregation",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _make_request("preview/C/A/HS", params)


def get_top_partners(reporter: str, year: str = None, flow: str = "X") -> Any:
    params = {
        "reporterCode": reporter,
        "partnerCode": "all",
        "cmdCode": "TOTAL",
        "flowCode": flow,
        "period": year or "2022",
        "motCode": "0",
        "partner2Code": "0",
        "aggregateBy": "defaultAggregation",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _make_request("preview/C/A/HS", params)


def get_top_commodities(reporter: str, year: str = None, flow: str = "X") -> Any:
    params = {
        "reporterCode": reporter,
        "partnerCode": "0",
        "cmdCode": "AG2",
        "flowCode": flow,
        "period": year or "2022",
        "motCode": "0",
        "partner2Code": "0",
        "aggregateBy": "defaultAggregation",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _make_request("preview/C/A/HS", params)


def get_reporters() -> Any:
    return _make_request("references/reporterAreas")


def get_commodities(hs_chapter: str = None) -> Any:
    params = {}
    if hs_chapter:
        params["filter"] = hs_chapter
    return _make_request("references/cmdCodes", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "trade":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            partner = args[2] if len(args) > 2 else "0"
            commodity = args[3] if len(args) > 3 else "TOTAL"
            year = args[4] if len(args) > 4 else None
            flow = args[5] if len(args) > 5 else "X"
            result = get_trade_data(args[1], partner, commodity, year, flow)
    elif command == "balance":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            year = args[2] if len(args) > 2 else None
            result = get_trade_balance(args[1], year)
    elif command == "top_partners":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            year = args[2] if len(args) > 2 else None
            flow = args[3] if len(args) > 3 else "X"
            result = get_top_partners(args[1], year, flow)
    elif command == "top_commodities":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            year = args[2] if len(args) > 2 else None
            flow = args[3] if len(args) > 3 else "X"
            result = get_top_commodities(args[1], year, flow)
    elif command == "reporters":
        result = get_reporters()
    elif command == "commodities":
        hs_chapter = args[1] if len(args) > 1 else None
        result = get_commodities(hs_chapter)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
