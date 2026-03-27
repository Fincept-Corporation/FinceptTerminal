"""
Global Debt Monitor Data Fetcher
Government, corporate, and household debt ratios globally via the IMF Data
Mapper API (covers fiscal monitor, WEO, and global debt database indicators).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IMF_API_KEY', '')
BASE_URL = "https://www.imf.org/external/datamapper/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

# IMF DataMapper indicator codes
DEBT_INDICATORS = {
    "government_gross": "GGXWDG_NGDP",       # General government gross debt (% GDP)
    "government_net": "GGXWDN_NGDP",          # General government net debt (% GDP)
    "government_balance": "GGXCNL_NGDP",      # Net lending/borrowing (% GDP)
    "debt_service": "GGXFD_NGDP",             # Gross financing needs (% GDP)
    "primary_balance": "GGXONLB_NGDP",        # Primary net lending/borrowing (% GDP)
    "household_debt": "HH_LS",                # Household debt (% GDP) - BIS
    "corporate_debt": "NFC_LS",               # Non-financial corporate debt (% GDP) - BIS
    "total_private_debt": "PVTD_LS",          # Private sector debt (% GDP)
}


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


def _fetch_indicator(indicator_code: str, country: str = None,
                     start_year: str = None, end_year: str = None) -> Any:
    endpoint = f"indicators/{indicator_code}"
    if country:
        endpoint = f"{endpoint}/{country}"
    params = {}
    if start_year:
        params["periods"] = f"{start_year}:{end_year or start_year}"
    return _make_request(endpoint, params)


def get_government_debt(country: str = None, start_year: str = None, end_year: str = None) -> Any:
    return _fetch_indicator(DEBT_INDICATORS["government_gross"], country, start_year, end_year)


def get_corporate_debt(country: str = None, start_year: str = None, end_year: str = None) -> Any:
    return _fetch_indicator(DEBT_INDICATORS["corporate_debt"], country, start_year, end_year)


def get_household_debt(country: str = None, start_year: str = None, end_year: str = None) -> Any:
    return _fetch_indicator(DEBT_INDICATORS["household_debt"], country, start_year, end_year)


def get_total_debt(country: str = None, year: str = None) -> Any:
    results = {}
    for key in ["government_gross", "corporate_debt", "household_debt", "total_private_debt"]:
        results[key] = _fetch_indicator(DEBT_INDICATORS[key], country, year, year)
    return results


def get_debt_service_ratio(country: str = None, year: str = None) -> Any:
    return _fetch_indicator(DEBT_INDICATORS["debt_service"], country, year, year)


def get_debt_trends(country: str = None) -> Any:
    results = {}
    for key in ["government_gross", "government_net", "government_balance", "primary_balance"]:
        results[key] = _fetch_indicator(DEBT_INDICATORS[key], country)
    return results


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "government":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        end_year = args[3] if len(args) > 3 else None
        result = get_government_debt(country, start_year, end_year)
    elif command == "corporate":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        end_year = args[3] if len(args) > 3 else None
        result = get_corporate_debt(country, start_year, end_year)
    elif command == "household":
        country = args[1] if len(args) > 1 else None
        start_year = args[2] if len(args) > 2 else None
        end_year = args[3] if len(args) > 3 else None
        result = get_household_debt(country, start_year, end_year)
    elif command == "total":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_total_debt(country, year)
    elif command == "service_ratio":
        country = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        result = get_debt_service_ratio(country, year)
    elif command == "trends":
        country = args[1] if len(args) > 1 else None
        result = get_debt_trends(country)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
