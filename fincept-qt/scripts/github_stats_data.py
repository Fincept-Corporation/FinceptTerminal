"""
GitHub Stats Data Fetcher
Repo commit activity, star history, contributor stats, language breakdown, trending repos.
No key required (60 req/hr); GITHUB_TOKEN for 5000/hr.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('GITHUB_TOKEN', '')
BASE_URL = "https://api.github.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({
    "Accept": "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28",
})
if API_KEY:
    session.headers.update({"Authorization": f"Bearer {API_KEY}"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
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


def get_repo_stats(owner: str, repo: str) -> Any:
    """Get general repository metadata and statistics."""
    data = _make_request(f"repos/{owner}/{repo}")
    if isinstance(data, dict) and "error" not in data:
        return {
            "full_name": data.get("full_name"),
            "description": data.get("description"),
            "language": data.get("language"),
            "stars": data.get("stargazers_count"),
            "forks": data.get("forks_count"),
            "watchers": data.get("watchers_count"),
            "open_issues": data.get("open_issues_count"),
            "size_kb": data.get("size"),
            "created_at": data.get("created_at"),
            "updated_at": data.get("updated_at"),
            "pushed_at": data.get("pushed_at"),
            "default_branch": data.get("default_branch"),
            "license": data.get("license", {}).get("spdx_id") if data.get("license") else None,
            "topics": data.get("topics", []),
            "homepage": data.get("homepage"),
            "archived": data.get("archived"),
        }
    return data


def get_commit_activity(owner: str, repo: str) -> Any:
    """Get weekly commit activity (additions, deletions, commits) for the past year."""
    data = _make_request(f"repos/{owner}/{repo}/stats/commit_activity")
    if isinstance(data, list):
        return {
            "owner": owner,
            "repo": repo,
            "weekly_activity": data,
            "weeks": len(data),
            "total_commits": sum(w.get("total", 0) for w in data),
        }
    return data


def get_contributors(owner: str, repo: str, limit: int = 30) -> Any:
    """Get top contributors with commit counts and statistics."""
    data = _make_request(f"repos/{owner}/{repo}/stats/contributors")
    if isinstance(data, list):
        contributors = []
        for c in data[-limit:]:
            author = c.get("author", {}) or {}
            contributors.append({
                "login": author.get("login"),
                "avatar_url": author.get("avatar_url"),
                "total_commits": c.get("total"),
                "weeks": len(c.get("weeks", [])),
            })
        contributors.sort(key=lambda x: x.get("total_commits", 0), reverse=True)
        return {
            "owner": owner,
            "repo": repo,
            "contributors": contributors,
            "count": len(contributors),
        }
    return data


def get_languages(owner: str, repo: str) -> Any:
    """Get programming language breakdown (in bytes) for a repository."""
    data = _make_request(f"repos/{owner}/{repo}/languages")
    if isinstance(data, dict) and "error" not in data:
        total = sum(data.values()) or 1
        breakdown = [
            {"language": lang, "bytes": count, "percentage": round(count / total * 100, 2)}
            for lang, count in sorted(data.items(), key=lambda x: x[1], reverse=True)
        ]
        return {
            "owner": owner,
            "repo": repo,
            "languages": breakdown,
            "total_bytes": total,
        }
    return data


def search_repos(query: str, sort: str = "stars", limit: int = 30) -> Any:
    """Search GitHub repositories by keyword.
    sort: stars, forks, help-wanted-issues, updated.
    """
    params = {
        "q": query,
        "sort": sort,
        "order": "desc",
        "per_page": min(limit, 100),
    }
    data = _make_request("search/repositories", params=params)
    if isinstance(data, dict) and "items" in data:
        repos = []
        for r in data["items"][:limit]:
            repos.append({
                "full_name": r.get("full_name"),
                "description": r.get("description"),
                "language": r.get("language"),
                "stars": r.get("stargazers_count"),
                "forks": r.get("forks_count"),
                "updated_at": r.get("updated_at"),
                "topics": r.get("topics", []),
                "license": r.get("license", {}).get("spdx_id") if r.get("license") else None,
            })
        return {
            "query": query,
            "sort": sort,
            "total_count": data.get("total_count"),
            "repos": repos,
            "count": len(repos),
        }
    return data


def get_trending_topics() -> Any:
    """Get trending GitHub topics (most starred topics)."""
    params = {"q": "is:featured", "per_page": 30}
    data = _make_request("search/topics", params=params)
    if isinstance(data, dict) and "items" in data:
        topics = []
        for t in data["items"]:
            topics.append({
                "name": t.get("name"),
                "display_name": t.get("display_name"),
                "short_description": t.get("short_description"),
                "created_by": t.get("created_by"),
                "released": t.get("released"),
                "featured": t.get("featured"),
                "curated": t.get("curated"),
            })
        return {"topics": topics, "count": len(topics), "total": data.get("total_count")}
    return data


def get_code_frequency(owner: str, repo: str) -> Any:
    """Get weekly code change frequency (additions and deletions) for the past year."""
    data = _make_request(f"repos/{owner}/{repo}/stats/code_frequency")
    if isinstance(data, list):
        formatted = [
            {"week_timestamp": row[0], "additions": row[1], "deletions": row[2]}
            for row in data if isinstance(row, list) and len(row) >= 3
        ]
        return {
            "owner": owner,
            "repo": repo,
            "code_frequency": formatted,
            "weeks": len(formatted),
            "total_additions": sum(r["additions"] for r in formatted),
            "total_deletions": sum(r["deletions"] for r in formatted),
        }
    return data


def get_releases(owner: str, repo: str, limit: int = 10) -> Any:
    """Get recent releases for a repository."""
    params = {"per_page": min(limit, 100)}
    data = _make_request(f"repos/{owner}/{repo}/releases", params=params)
    if isinstance(data, list):
        releases = []
        for r in data[:limit]:
            releases.append({
                "tag_name": r.get("tag_name"),
                "name": r.get("name"),
                "published_at": r.get("published_at"),
                "prerelease": r.get("prerelease"),
                "draft": r.get("draft"),
                "assets_count": len(r.get("assets", [])),
                "body_preview": (r.get("body") or "")[:200],
            })
        return {"owner": owner, "repo": repo, "releases": releases, "count": len(releases)}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: repo, commits, contributors, languages, search, trending, code_frequency, releases"}))
        return

    command = args[0]

    if command == "repo":
        if len(args) < 3:
            result = {"error": "Usage: repo <owner> <repo> (e.g. repo microsoft vscode)"}
        else:
            result = get_repo_stats(args[1], args[2])
    elif command == "commits":
        if len(args) < 3:
            result = {"error": "Usage: commits <owner> <repo>"}
        else:
            result = get_commit_activity(args[1], args[2])
    elif command == "contributors":
        if len(args) < 3:
            result = {"error": "Usage: contributors <owner> <repo> [limit]"}
        else:
            limit = int(args[3]) if len(args) > 3 else 30
            result = get_contributors(args[1], args[2], limit)
    elif command == "languages":
        if len(args) < 3:
            result = {"error": "Usage: languages <owner> <repo>"}
        else:
            result = get_languages(args[1], args[2])
    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: search <query> [sort] [limit]"}
        else:
            sort = args[2] if len(args) > 2 else "stars"
            limit = int(args[3]) if len(args) > 3 else 30
            result = search_repos(args[1], sort, limit)
    elif command == "trending":
        result = get_trending_topics()
    elif command == "code_frequency":
        if len(args) < 3:
            result = {"error": "Usage: code_frequency <owner> <repo>"}
        else:
            result = get_code_frequency(args[1], args[2])
    elif command == "releases":
        if len(args) < 3:
            result = {"error": "Usage: releases <owner> <repo> [limit]"}
        else:
            limit = int(args[3]) if len(args) > 3 else 10
            result = get_releases(args[1], args[2], limit)
    else:
        result = {"error": f"Unknown command: {command}. Available: repo, commits, contributors, languages, search, trending, code_frequency, releases"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
