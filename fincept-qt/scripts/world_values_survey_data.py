"""
World Values Survey Data Fetcher
World Values Survey: cross-national survey data on values, beliefs, norms from 100+ countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.worldvaluessurvey.org/WVSOnline.jsp"
DATA_BASE_URL = "https://www.worldvaluessurvey.org/WVSDocumentationWV7.jsp"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = endpoint if endpoint.startswith('http') else f"{BASE_URL}/{endpoint}"
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


def get_available_waves() -> Any:
    return {
        "waves": [
            {"wave": 1, "years": "1981-1984"},
            {"wave": 2, "years": "1990-1994"},
            {"wave": 3, "years": "1995-1998"},
            {"wave": 4, "years": "1999-2004"},
            {"wave": 5, "years": "2005-2009"},
            {"wave": 6, "years": "2010-2014"},
            {"wave": 7, "years": "2017-2022"}
        ],
        "source": "World Values Survey",
        "url": "https://www.worldvaluessurvey.org"
    }


def get_countries(wave: int = 7) -> Any:
    url = f"https://www.worldvaluessurvey.org/WVSContents.jsp?CMSType=wave&CMSId={wave}"
    return _make_request(url)


def get_available_variables(wave: int = 7) -> Any:
    url = f"https://www.worldvaluessurvey.org/WVSDocumentationWV{wave}.jsp"
    return _make_request(url)


def get_wave_data(wave: int, country: str, variable: str) -> Any:
    params = {
        "wave": wave,
        "country": country,
        "variable": variable
    }
    return _make_request(BASE_URL, params=params)


def get_trend_data(variable: str, country: str, waves: str = "6,7") -> Any:
    params = {
        "variable": variable,
        "country": country,
        "waves": waves
    }
    return _make_request(BASE_URL, params=params)


def get_variable_info(variable: str) -> Any:
    params = {"variable": variable}
    return _make_request(BASE_URL, params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "waves":
        result = get_available_waves()
    elif command == "countries":
        wave = int(args[1]) if len(args) > 1 else 7
        result = get_countries(wave)
    elif command == "variables":
        wave = int(args[1]) if len(args) > 1 else 7
        result = get_available_variables(wave)
    elif command == "wave":
        wave = int(args[1]) if len(args) > 1 else 7
        country = args[2] if len(args) > 2 else "840"
        variable = args[3] if len(args) > 3 else "Q1"
        result = get_wave_data(wave, country, variable)
    elif command == "trends":
        variable = args[1] if len(args) > 1 else "Q1"
        country = args[2] if len(args) > 2 else "840"
        waves = args[3] if len(args) > 3 else "6,7"
        result = get_trend_data(variable, country, waves)
    elif command == "info":
        variable = args[1] if len(args) > 1 else "Q1"
        result = get_variable_info(variable)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
