"""
ReliefWeb (OCHA) Data Fetcher
Provides humanitarian crisis reports, disaster events, emergency job listings,
training opportunities, and related content from the ReliefWeb API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

# ReliefWeb retired API v1 (HTTP 410). API v2 only answers requests that carry an appname
# ReliefWeb has approved (request one at https://apidoc.reliefweb.int/parameters#appname);
# any other appname gets HTTP 403. The approved name is stored like an API key.
APP_ID = os.environ.get('RELIEFWEB_APP_ID', '').strip()
BASE_URL = "https://api.reliefweb.int/v2"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    if not APP_ID:
        return {"error": "ReliefWeb API v2 requires an approved appname: request one at "
                         "https://apidoc.reliefweb.int/parameters#appname and save it as RELIEFWEB_APP_ID "
                         "in Settings > Credentials"}
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    default_params = {"appname": APP_ID, "slim": 1}
    if params:
        default_params.update(params)
    try:
        response = session.get(url, params=default_params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        try:
            msg = e.response.json().get("error", {}).get("message") or str(e)
        except ValueError:
            msg = str(e)
        return {"error": f"HTTP {e.response.status_code}: {msg}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_reports(country: str = None, theme: str = None, limit: int = 20) -> Any:
    """Return humanitarian situation reports, optionally filtered by country and theme."""
    params = {"limit": limit, "sort[]": "date:desc"}
    if country:
        params["filter[field]"] = "country.iso3"
        params["filter[value]"] = country.upper()
    if theme:
        params["filter[field]"] = "theme.name"
        params["filter[value]"] = theme
    data = _make_request("reports", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "theme": theme, "reports": data.get("data", []),
            "total": data.get("totalCount", 0)}


def get_disasters(country: str = None, type_: str = None, status: str = None, limit: int = 20) -> Any:
    """Return disaster events, optionally filtered by country, type, and status."""
    params = {"limit": limit, "sort[]": "date:desc"}
    filters = []
    if country:
        filters.append({"field": "country.iso3", "value": country.upper()})
    if type_:
        filters.append({"field": "type.name", "value": type_})
    if status:
        filters.append({"field": "status", "value": status})
    if filters:
        params["filter[operator]"] = "AND"
        for i, f in enumerate(filters):
            params[f"filter[conditions][{i}][field]"] = f["field"]
            params[f"filter[conditions][{i}][value]"] = f["value"]
    data = _make_request("disasters", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "type": type_, "status": status,
            "disasters": data.get("data", []), "total": data.get("totalCount", 0)}


def get_jobs(country: str = None, limit: int = 20) -> Any:
    """Return humanitarian sector job listings, optionally filtered by country."""
    params = {"limit": limit, "sort[]": "date:desc"}
    if country:
        params["filter[field]"] = "country.iso3"
        params["filter[value]"] = country.upper()
    data = _make_request("jobs", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "jobs": data.get("data", []), "total": data.get("totalCount", 0)}


def get_training(country: str = None, limit: int = 20) -> Any:
    """Return humanitarian training and conference listings."""
    params = {"limit": limit, "sort[]": "date:desc"}
    if country:
        params["filter[field]"] = "country.iso3"
        params["filter[value]"] = country.upper()
    data = _make_request("training", params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"country": country, "training": data.get("data", []), "total": data.get("totalCount", 0)}


def search_content(query: str, content_type: str = "reports", limit: int = 20) -> Any:
    """Full-text search across ReliefWeb content types."""
    valid_types = ["reports", "disasters", "jobs", "training", "blog"]
    if content_type not in valid_types:
        return {"error": f"content_type must be one of: {valid_types}"}
    params = {"search": query, "limit": limit, "sort[]": "score:desc"}
    data = _make_request(content_type, params=params)
    if isinstance(data, dict) and "error" in data:
        return data
    return {"query": query, "content_type": content_type, "results": data.get("data", []),
            "total": data.get("totalCount", 0)}


def get_countries() -> Any:
    """Return all countries with ReliefWeb content."""
    data = _make_request("countries", params={"limit": 300, "sort[]": "name:asc"})
    if isinstance(data, dict) and "error" in data:
        return data
    return {"countries": data.get("data", []), "total": data.get("totalCount", 0)}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "reports":
        country = args[1] if len(args) > 1 else None
        theme = args[2] if len(args) > 2 else None
        limit = int(args[3]) if len(args) > 3 else 20
        result = get_reports(country, theme, limit)
    elif command == "disasters":
        country = args[1] if len(args) > 1 else None
        type_ = args[2] if len(args) > 2 else None
        status = args[3] if len(args) > 3 else None
        limit = int(args[4]) if len(args) > 4 else 20
        result = get_disasters(country, type_, status, limit)
    elif command == "jobs":
        country = args[1] if len(args) > 1 else None
        limit = int(args[2]) if len(args) > 2 else 20
        result = get_jobs(country, limit)
    elif command == "training":
        country = args[1] if len(args) > 1 else None
        limit = int(args[2]) if len(args) > 2 else 20
        result = get_training(country, limit)
    elif command == "search":
        query = args[1] if len(args) > 1 else ""
        if not query:
            result = {"error": "query required"}
        else:
            content_type = args[2] if len(args) > 2 else "reports"
            limit = int(args[3]) if len(args) > 3 else 20
            result = search_content(query, content_type, limit)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
