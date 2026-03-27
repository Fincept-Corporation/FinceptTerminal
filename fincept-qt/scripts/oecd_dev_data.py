"""
OECD Development Cooperation Data Fetcher
Official Development Assistance (ODA) flows, aid effectiveness, donor statistics,
and recipient country data via the OECD SDMX-JSON stats API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('OECD_API_KEY', '')
BASE_URL = "https://stats.oecd.org/sdmx-json/data/TABLE2A"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {"Accept": "application/json"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_oda_flows(donor: str = "all", recipient: str = "all", year: str = None) -> Any:
    key = f"{donor}.{recipient}.206.D.Q"
    params = {"dimensionAtObservation": "allDimensions"}
    if year:
        params["startTime"] = year
        params["endTime"] = year
    url = f"https://stats.oecd.org/sdmx-json/data/TABLE2A/{key}/all"
    return _make_request(url, params)


def get_bilateral_aid(donor: str = "all", recipient: str = "all",
                      sector: str = None, year: str = None) -> Any:
    key = f"{donor}.{recipient}.{sector or '10000'}.D.Q"
    params = {"dimensionAtObservation": "allDimensions"}
    if year:
        params["startTime"] = year
        params["endTime"] = year
    url = f"https://stats.oecd.org/sdmx-json/data/CRS1/{key}/all"
    return _make_request(url, params)


def get_multilateral_aid(donor: str = "all", year: str = None) -> Any:
    key = f"{donor}.MULTILATERAL.206.D.Q"
    params = {"dimensionAtObservation": "allDimensions"}
    if year:
        params["startTime"] = year
        params["endTime"] = year
    url = f"https://stats.oecd.org/sdmx-json/data/TABLE2A/{key}/all"
    return _make_request(url, params)


def get_aid_effectiveness(recipient: str = "all", year: str = None) -> Any:
    params = {"dimensionAtObservation": "allDimensions"}
    if year:
        params["startTime"] = year
        params["endTime"] = year
    url = f"https://stats.oecd.org/sdmx-json/data/AIDEFFECTIVENESS/all.{recipient}../all"
    return _make_request(url, params)


def get_donors() -> Any:
    url = "https://stats.oecd.org/sdmx-json/data/TABLE2A/all.all.206.D.Q/all"
    params = {"dimensionAtObservation": "allDimensions", "detail": "serieskeysonly"}
    return _make_request(url, params)


def get_recipients() -> Any:
    url = "https://stats.oecd.org/sdmx-json/data/TABLE2A/all.all.206.D.Q/all"
    params = {"dimensionAtObservation": "allDimensions", "detail": "serieskeysonly"}
    return _make_request(url, params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "oda":
        donor = args[1] if len(args) > 1 else "all"
        recipient = args[2] if len(args) > 2 else "all"
        year = args[3] if len(args) > 3 else None
        result = get_oda_flows(donor, recipient, year)
    elif command == "bilateral":
        donor = args[1] if len(args) > 1 else "all"
        recipient = args[2] if len(args) > 2 else "all"
        sector = args[3] if len(args) > 3 else None
        year = args[4] if len(args) > 4 else None
        result = get_bilateral_aid(donor, recipient, sector, year)
    elif command == "multilateral":
        donor = args[1] if len(args) > 1 else "all"
        year = args[2] if len(args) > 2 else None
        result = get_multilateral_aid(donor, year)
    elif command == "effectiveness":
        recipient = args[1] if len(args) > 1 else "all"
        year = args[2] if len(args) > 2 else None
        result = get_aid_effectiveness(recipient, year)
    elif command == "donors":
        result = get_donors()
    elif command == "recipients":
        result = get_recipients()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
