"""
Wikipedia Pageviews Data Fetcher
Daily/monthly page traffic for any article, top-1000 lists, project-level stats.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://wikimedia.org/api/rest_v1/metrics/pageviews"
LEGACY_URL = "https://wikimedia.org/api/rest_v1/metrics/legacy/pagecounts"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({
    "User-Agent": "FinceptTerminal/4.0 (https://fincept.in) python-requests",
    "Accept": "application/json",
})


def _make_request(endpoint: str, params: Dict = None, base: str = None) -> Any:
    """Make HTTP request with error handling."""
    base_url = base if base else BASE_URL
    url = f"{base_url}/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def get_article_views(article: str, start: str, end: str, project: str = "en.wikipedia",
                      access: str = "all-access", agent: str = "all-agents", granularity: str = "daily") -> Any:
    """Get pageview counts for a specific Wikipedia article.
    article: article title (spaces become underscores, e.g. Albert_Einstein).
    start/end: YYYYMMDD format (e.g. 20240101).
    project: e.g. en.wikipedia, de.wikipedia, fr.wikipedia.
    granularity: daily or monthly.
    """
    article_enc = article.replace(" ", "_")
    endpoint = f"per-article/{project}/{access}/{agent}/{article_enc}/{granularity}/{start}/{end}"
    data = _make_request(endpoint)
    if isinstance(data, dict) and "items" in data:
        items = data["items"]
        total = sum(i.get("views", 0) for i in items)
        return {
            "article": article,
            "project": project,
            "granularity": granularity,
            "start": start,
            "end": end,
            "total_views": total,
            "data": items,
            "count": len(items),
        }
    return data


def get_top_articles(project: str = "en.wikipedia", year: str = "2024", month: str = "01",
                     access: str = "all-access") -> Any:
    """Get top 1000 most viewed articles for a project in a given month.
    project: e.g. en.wikipedia, de.wikipedia.
    year: 4-digit year string.
    month: 2-digit month string (01-12).
    """
    endpoint = f"top/{project}/{access}/{year}/{month}/all-days"
    data = _make_request(endpoint)
    if isinstance(data, dict) and "items" in data:
        items = data["items"]
        if items and "articles" in items[0]:
            articles = items[0]["articles"]
            return {
                "project": project,
                "year": year,
                "month": month,
                "top_articles": articles[:100],
                "total_in_list": len(articles),
            }
    return data


def get_project_views(project: str = "en.wikipedia", start: str = "20240101", end: str = "20241231",
                      granularity: str = "monthly", access: str = "all-access") -> Any:
    """Get total pageview counts for an entire Wikimedia project.
    granularity: daily or monthly.
    """
    endpoint = f"aggregate/{project}/{access}/all-agents/{granularity}/{start}/{end}"
    data = _make_request(endpoint)
    if isinstance(data, dict) and "items" in data:
        items = data["items"]
        total = sum(i.get("views", 0) for i in items)
        return {
            "project": project,
            "granularity": granularity,
            "start": start,
            "end": end,
            "total_views": total,
            "data": items,
        }
    return data


def get_top_by_country(country: str, project: str = "en.wikipedia", year: str = "2024",
                       month: str = "01", access: str = "all-access") -> Any:
    """Get top articles viewed from a specific country.
    country: ISO 3166-1 alpha-2 code (e.g. US, DE, JP, CN).
    """
    endpoint = f"top-by-country/{country.upper()}/{access}/{project}/{year}/{month}/all-days"
    data = _make_request(endpoint)
    if isinstance(data, dict) and "items" in data:
        return {
            "country": country.upper(),
            "project": project,
            "year": year,
            "month": month,
            "data": data["items"],
        }
    return data


def get_legacy_counts(article: str, project: str = "en.wikipedia",
                      start: str = "2015010100", end: str = "2016010100") -> Any:
    """Get legacy pagecounts for an article (pre-2016 traffic estimates).
    start/end: YYYYMMDDHH format.
    """
    article_enc = article.replace(" ", "_")
    endpoint = f"per-article/{project}/all-sites/spider/{article_enc}/monthly/{start}/{end}"
    data = _make_request(endpoint, base=LEGACY_URL)
    if isinstance(data, dict) and "items" in data:
        return {"article": article, "legacy_data": data["items"], "count": len(data["items"])}
    return data


def get_editors(project: str = "en.wikipedia", start: str = "20240101",
                end: str = "20241231", granularity: str = "monthly") -> Any:
    """Get active editor counts for a Wikimedia project."""
    url = f"https://wikimedia.org/api/rest_v1/metrics/editors/aggregate/{project}/all-editor-types/all-page-types/{granularity}/{start}/{end}"
    data = _make_request(url)
    if isinstance(data, dict) and "items" in data:
        return {"project": project, "editors": data["items"], "granularity": granularity}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: article, top, project, top_by_country, legacy, editors"}))
        return

    command = args[0]

    if command == "article":
        if len(args) < 4:
            result = {"error": "Usage: article <title> <start YYYYMMDD> <end YYYYMMDD> [project] [granularity]"}
        else:
            project = args[4] if len(args) > 4 else "en.wikipedia"
            granularity = args[5] if len(args) > 5 else "daily"
            result = get_article_views(args[1], args[2], args[3], project=project, granularity=granularity)
    elif command == "top":
        project = args[1] if len(args) > 1 else "en.wikipedia"
        year = args[2] if len(args) > 2 else "2024"
        month = args[3] if len(args) > 3 else "01"
        result = get_top_articles(project, year, month)
    elif command == "project":
        project = args[1] if len(args) > 1 else "en.wikipedia"
        start = args[2] if len(args) > 2 else "20240101"
        end = args[3] if len(args) > 3 else "20241231"
        granularity = args[4] if len(args) > 4 else "monthly"
        result = get_project_views(project, start, end, granularity)
    elif command == "top_by_country":
        if len(args) < 2:
            result = {"error": "Usage: top_by_country <country_iso2> [project] [year] [month]"}
        else:
            project = args[2] if len(args) > 2 else "en.wikipedia"
            year = args[3] if len(args) > 3 else "2024"
            month = args[4] if len(args) > 4 else "01"
            result = get_top_by_country(args[1], project, year, month)
    elif command == "legacy":
        if len(args) < 2:
            result = {"error": "Usage: legacy <article_title> [project] [start YYYYMMDDHH] [end YYYYMMDDHH]"}
        else:
            project = args[2] if len(args) > 2 else "en.wikipedia"
            start = args[3] if len(args) > 3 else "2015010100"
            end = args[4] if len(args) > 4 else "2016010100"
            result = get_legacy_counts(args[1], project, start, end)
    elif command == "editors":
        project = args[1] if len(args) > 1 else "en.wikipedia"
        start = args[2] if len(args) > 2 else "20240101"
        end = args[3] if len(args) > 3 else "20241231"
        result = get_editors(project, start, end)
    else:
        result = {"error": f"Unknown command: {command}. Available: article, top, project, top_by_country, legacy, editors"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
