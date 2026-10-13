"""
USDA ERS Data Fetcher
USDA Economic Research Service ARMS (Agricultural Resource Management Survey) API: farm income statements, balance sheets and financial ratios by year, state and farm type (api.data.gov key: USDA_ERS_API_KEY).
"""
import sys
import json
import os
import requests
from datetime import datetime
from typing import Dict, Any
from urllib3.util.retry import Retry

API_KEY = os.environ.get('USDA_ERS_API_KEY', '')
# The other api.ers.usda.gov/data/* paths this connector used never existed; ARMS is the ERS data API.
BASE_URL = "https://api.ers.usda.gov/data/arms"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10,
                                        max_retries=Retry(total=2, status=0, respect_retry_after_header=False))
session.mount('https://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not API_KEY:
        return {"error": "The USDA ERS ARMS API needs a free api.data.gov key: sign up at https://api.data.gov/signup "
                         "and save it as USDA_ERS_API_KEY in Settings > Credentials"}
    params = {k: v for k, v in (params or {}).items() if v not in (None, "")}
    params["api_key"] = API_KEY
    try:
        response = session.get(f"{BASE_URL}/{endpoint}", params=params, timeout=60)
        if response.status_code == 429:
            return {"error": "api.data.gov rate limit reached for USDA_ERS_API_KEY - retry later"}
        if response.status_code in (401, 403):
            return {"error": "api.data.gov rejected the API key (USDA_ERS_API_KEY)"}
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {e.response.text[:200]}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if isinstance(data, dict) and data.get("error"):
        return {"error": f"ARMS API: {data['error']}"}
    return data


def _years(year: str = None) -> str:
    if year:
        return year
    y = datetime.now().year
    return ",".join(str(v) for v in range(y - 6, y))


def get_farm_income(year: str = None, state: str = None) -> Any:
    """Farm business income statement (gross cash income, expenses, net farm income ...)."""
    return _make_request("surveydata", {"year": _years(year), "state": state or "all",
                                        "report": "farm business income statement"})


def get_balance_sheet(year: str = None, state: str = None) -> Any:
    """Farm business balance sheet (assets, debt, equity)."""
    return _make_request("surveydata", {"year": _years(year), "state": state or "all",
                                        "report": "farm business balance sheet"})


def get_survey_data(year: str = None, report: str = None, variable: str = None, state: str = None,
                    farmtype: str = None) -> Any:
    """Generic ARMS query: a report name (see `reports`) and/or a variable id (see `variables`)."""
    if not report and not variable:
        return {"error": "report or variable required (see the `reports` and `variables` commands)"}
    return _make_request("surveydata", {"year": _years(year), "state": state or "all", "report": report,
                                        "variable": variable, "farmtype": farmtype})


def get_reports() -> Any:
    return _make_request("report")


def get_variables(report: str = None) -> Any:
    return _make_request("variable", {"report": report})


def get_states() -> Any:
    return _make_request("state")


def get_years() -> Any:
    return _make_request("year")


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "farm_income":
        year = args[1] if len(args) > 1 else None
        state = args[2] if len(args) > 2 else None
        result = get_farm_income(year, state)
    elif command == "balance_sheet":
        year = args[1] if len(args) > 1 else None
        state = args[2] if len(args) > 2 else None
        result = get_balance_sheet(year, state)
    elif command == "survey":
        report = args[1] if len(args) > 1 else None
        year = args[2] if len(args) > 2 else None
        state = args[3] if len(args) > 3 else None
        variable = args[4] if len(args) > 4 else None
        result = get_survey_data(year, report, variable, state)
    elif command == "reports":
        result = get_reports()
    elif command == "variables":
        report = args[1] if len(args) > 1 else None
        result = get_variables(report)
    elif command == "states":
        result = get_states()
    elif command == "years":
        result = get_years()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
