"""
Pew Research Data Fetcher
Pew Research Center: global public opinion, religion, demographics, internet use surveys.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.pewresearch.org/api"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


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


def get_available_surveys() -> Any:
    result = _make_request("datasets")
    if "error" in result:
        result["note"] = "Pew Research datasets available at https://www.pewresearch.org/tools-and-resources/"
        result["public_datasets"] = [
            "Global Attitudes Survey",
            "Religion & Public Life",
            "American Trends Panel",
            "Internet & Technology",
            "Hispanic Trends",
            "Social & Demographic Trends"
        ]
    return result


def get_countries() -> Any:
    return _make_request("countries")


def get_survey_data(survey_id: str, question: str = None, country: str = None) -> Any:
    params = {"survey_id": survey_id}
    if question:
        params["question"] = question
    if country:
        params["country"] = country
    return _make_request("survey", params=params)


def get_religion_data(country: str, year: int = 2023) -> Any:
    params = {"country": country, "year": year, "topic": "religion"}
    return _make_request("religion", params=params)


def get_internet_use(country: str, year: int = 2023) -> Any:
    params = {"country": country, "year": year}
    return _make_request("internet", params=params)


def get_global_attitudes(topic: str, country: str = None, year: int = 2023) -> Any:
    params = {"topic": topic, "year": year}
    if country:
        params["country"] = country
    return _make_request("global_attitudes", params=params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "surveys":
        result = get_available_surveys()
    elif command == "countries":
        result = get_countries()
    elif command == "survey":
        survey_id = args[1] if len(args) > 1 else "global-attitudes-2023"
        question = args[2] if len(args) > 2 else None
        country = args[3] if len(args) > 3 else None
        result = get_survey_data(survey_id, question, country)
    elif command == "religion":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_religion_data(country, year)
    elif command == "internet":
        country = args[1] if len(args) > 1 else "US"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_internet_use(country, year)
    elif command == "attitudes":
        topic = args[1] if len(args) > 1 else "democracy"
        country = args[2] if len(args) > 2 else None
        year = int(args[3]) if len(args) > 3 else 2023
        result = get_global_attitudes(topic, country, year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
