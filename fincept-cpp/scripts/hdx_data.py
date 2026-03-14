"""
HDX (Humanitarian Data Exchange) Data Wrapper
Direct CKAN API implementation - no external dependencies required
Based on HDX CKAN API Cookbook
"""

import sys
import json
import urllib.request
import urllib.parse
import urllib.error
from typing import Optional, Dict, List, Any


# HDX CKAN API Configuration
HDX_API_BASE = "https://data.humdata.org/api/3"
USER_AGENT = "FinceptTerminal_GeopoliticsAnalytics/3.0"


def make_api_request(endpoint: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Make a request to the HDX CKAN API

    Args:
        endpoint: API endpoint (e.g., 'action/package_search')
        params: Query parameters

    Returns:
        Parsed JSON response
    """
    url = f"{HDX_API_BASE}/{endpoint}"

    if params:
        # Filter out None values and convert to strings
        clean_params = {k: str(v) for k, v in params.items() if v is not None}
        query_string = urllib.parse.urlencode(clean_params)
        url = f"{url}?{query_string}"

    # Create request with user agent
    req = urllib.request.Request(url)
    req.add_header('User-Agent', USER_AGENT)

    try:
        with urllib.request.urlopen(req, timeout=30) as response:
            data = json.loads(response.read().decode('utf-8'))

            if not data.get('success', False):
                raise Exception(f"API request failed: {data.get('error', {}).get('message', 'Unknown error')}")

            return data.get('result', {})

    except urllib.error.HTTPError as e:
        raise Exception(f"HTTP Error {e.code}: {e.reason}")
    except urllib.error.URLError as e:
        raise Exception(f"URL Error: {e.reason}")
    except Exception as e:
        raise Exception(f"Request failed: {str(e)}")


def package_search(q: str = "", start: int = 0, rows: int = 10, sort: Optional[str] = None) -> Dict[str, Any]:
    """
    Search for datasets using CKAN package_search

    Args:
        q: Query string (field:value format)
        start: Starting position for pagination
        rows: Number of results per page
        sort: Sort order (e.g., 'metadata_created desc')

    Returns:
        Search results with count and datasets
    """
    params = {
        'q': q,
        'start': start,
        'rows': rows
    }

    if sort:
        params['sort'] = sort

    return make_api_request('action/package_search', params)


def package_show(id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific dataset

    Args:
        id: Dataset ID or name

    Returns:
        Complete dataset metadata
    """
    params = {'id': id}
    return make_api_request('action/package_show', params)


def group_list(all_fields: bool = True) -> List[Dict[str, Any]]:
    """
    List all countries/groups available in HDX

    Args:
        all_fields: Return full metadata (not just names)

    Returns:
        List of groups/countries
    """
    params = {'all_fields': 'true' if all_fields else 'false'}
    return make_api_request('action/group_list', params)


def organization_list(all_fields: bool = True) -> List[Dict[str, Any]]:
    """
    List all organizations providing data to HDX

    Args:
        all_fields: Return full metadata (not just names)

    Returns:
        List of organizations
    """
    params = {'all_fields': 'true' if all_fields else 'false'}
    return make_api_request('action/organization_list', params)


def tag_list(vocabulary_id: str = "Topics", all_fields: bool = True) -> List[Dict[str, Any]]:
    """
    List all tags from a specific vocabulary

    Args:
        vocabulary_id: Vocabulary name (default: "Topics")
        all_fields: Return full metadata

    Returns:
        List of tags
    """
    params = {
        'vocabulary_id': vocabulary_id,
        'all_fields': 'true' if all_fields else 'false'
    }
    return make_api_request('action/tag_list', params)


def extract_dataset_summary(dataset: Dict[str, Any]) -> Dict[str, Any]:
    """Extract key fields from dataset metadata for summary view"""
    return {
        "id": dataset.get("id", ""),
        "name": dataset.get("name", ""),
        "title": dataset.get("title", ""),
        "organization": dataset.get("organization", {}).get("title", "") if isinstance(dataset.get("organization"), dict) else "",
        "dataset_date": dataset.get("dataset_date", ""),
        "num_resources": dataset.get("num_resources", 0),
        "tags": [tag.get("name", "") if isinstance(tag, dict) else tag for tag in dataset.get("tags", [])],
        "notes": (dataset.get("notes", "")[:200] + "...") if dataset.get("notes") and len(dataset.get("notes", "")) > 200 else dataset.get("notes", "")
    }


def extract_dataset_details(dataset: Dict[str, Any]) -> Dict[str, Any]:
    """Extract complete dataset metadata"""
    resources = dataset.get("resources", [])

    return {
        "id": dataset.get("id", ""),
        "name": dataset.get("name", ""),
        "title": dataset.get("title", ""),
        "organization": dataset.get("organization", {}).get("title", "") if isinstance(dataset.get("organization"), dict) else "",
        "maintainer": dataset.get("maintainer", ""),
        "dataset_date": dataset.get("dataset_date", ""),
        "last_modified": dataset.get("last_modified", ""),
        "update_frequency": dataset.get("data_update_frequency", ""),
        "methodology": dataset.get("methodology", ""),
        "caveats": dataset.get("caveats_and_warnings", ""),
        "license": dataset.get("license_title", ""),
        "tags": [tag.get("name", "") if isinstance(tag, dict) else tag for tag in dataset.get("tags", [])],
        "groups": [group.get("title", "") if isinstance(group, dict) else group for group in dataset.get("groups", [])],
        "notes": dataset.get("notes", ""),
        "resources": [
            {
                "id": res.get("id", ""),
                "name": res.get("name", ""),
                "description": res.get("description", ""),
                "format": res.get("format", ""),
                "url": res.get("url", ""),
                "size": res.get("size", ""),
                "last_modified": res.get("last_modified", "")
            }
            for res in resources
        ]
    }


def main(args: Optional[List[str]] = None) -> str:
    """Main entry point for script execution"""
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        return json.dumps({"error": "Usage: hdx_data.py <command> [args...]"})

    command = args[0]

    try:
        result = {}

        if command == "search_datasets":
            # Search for datasets by query
            query = args[1] if len(args) > 1 else ""
            limit = int(args[2]) if len(args) > 2 else 10

            if not query:
                return json.dumps({"error": "Query parameter required"})

            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "query": query,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "get_dataset":
            # Get specific dataset by ID or name
            dataset_id = args[1] if len(args) > 1 else ""

            if not dataset_id:
                return json.dumps({"error": "Dataset ID required"})

            dataset = package_show(id=dataset_id)

            result = {
                "success": True,
                "data": extract_dataset_details(dataset)
            }

        elif command == "search_conflict":
            # Search for conflict-related datasets
            country = args[1] if len(args) > 1 else ""
            limit = int(args[2]) if len(args) > 2 else 10

            # Build query using CKAN field syntax
            query_parts = []

            # Add country filter if specified
            if country:
                # Try to convert country name to ISO3 code (lowercase)
                country_code = country.lower()[:3]
                query_parts.append(f"groups:{country_code}")

            # Add conflict-related topic tags
            # Using vocab_Topics field for thematic tags
            query_parts.append('vocab_Topics:"conflict, violence and peace"')

            query = " ".join(query_parts)

            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "country": country,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "search_humanitarian":
            # Search for humanitarian crisis data
            country = args[1] if len(args) > 1 else ""
            limit = int(args[2]) if len(args) > 2 else 10

            # Build query using CKAN field syntax
            query_parts = []

            if country:
                country_code = country.lower()[:3]
                query_parts.append(f"groups:{country_code}")

            # Add humanitarian-related topic tags
            query_parts.append('vocab_Topics:"humanitarian access-aid-workers"')

            query = " ".join(query_parts) if query_parts else "humanitarian"

            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "country": country,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "search_by_country":
            # Search datasets for a specific country
            country_code = args[1] if len(args) > 1 else ""
            limit = int(args[2]) if len(args) > 2 else 10

            if not country_code:
                return json.dumps({"error": "Country code required"})

            query = f"groups:{country_code.lower()}"
            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "country_code": country_code,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "search_by_organization":
            # Search datasets by organization
            org_slug = args[1] if len(args) > 1 else ""
            limit = int(args[2]) if len(args) > 2 else 10

            if not org_slug:
                return json.dumps({"error": "Organization slug required"})

            query = f"organization:{org_slug}"
            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "organization": org_slug,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "search_by_topic":
            # Search datasets by topic tag
            topic = args[1] if len(args) > 1 else ""
            limit = int(args[2]) if len(args) > 2 else 10

            if not topic:
                return json.dumps({"error": "Topic required"})

            # Quote topic if it contains spaces
            if " " in topic:
                query = f'vocab_Topics:"{topic}"'
            else:
                query = f'vocab_Topics:{topic}'

            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "topic": topic,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "search_by_dataseries":
            # Search datasets by data series
            series_name = args[1] if len(args) > 1 else ""
            limit = int(args[2]) if len(args) > 2 else 10

            if not series_name:
                return json.dumps({"error": "Data series name required"})

            # Quote series name as it likely contains spaces
            query = f'dataseries_name:"{series_name}"'
            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "series": series_name,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "list_countries":
            # List all countries/groups
            limit = int(args[1]) if len(args) > 1 else 300

            groups = group_list(all_fields=True)

            result = {
                "success": True,
                "data": {
                    "count": len(groups[:limit]),
                    "countries": [
                        {
                            "id": g.get("id", ""),
                            "name": g.get("name", ""),
                            "title": g.get("title", ""),
                            "description": g.get("description", ""),
                            "package_count": g.get("package_count", 0)
                        }
                        for g in groups[:limit]
                    ]
                }
            }

        elif command == "list_organizations":
            # List top organizations providing data
            limit = int(args[1]) if len(args) > 1 else 20

            orgs = organization_list(all_fields=True)

            # Sort by package count (descending)
            sorted_orgs = sorted(orgs, key=lambda x: x.get("package_count", 0), reverse=True)

            result = {
                "success": True,
                "data": {
                    "count": len(sorted_orgs[:limit]),
                    "organizations": [
                        {
                            "id": org.get("id", ""),
                            "name": org.get("name", ""),
                            "title": org.get("title", ""),
                            "description": org.get("description", ""),
                            "package_count": org.get("package_count", 0)
                        }
                        for org in sorted_orgs[:limit]
                    ]
                }
            }

        elif command == "list_topics":
            # List all topic tags
            limit = int(args[1]) if len(args) > 1 else 100

            topics = tag_list(vocabulary_id="Topics", all_fields=True)

            result = {
                "success": True,
                "data": {
                    "count": len(topics[:limit]),
                    "topics": [
                        {
                            "id": t.get("id", ""),
                            "name": t.get("name", "")
                        }
                        for t in topics[:limit]
                    ]
                }
            }

        elif command == "get_resource_download_url":
            # Get download URL for a specific resource
            dataset_id = args[1] if len(args) > 1 else ""
            resource_index = int(args[2]) if len(args) > 2 else 0

            if not dataset_id:
                return json.dumps({"error": "Dataset ID required"})

            dataset = package_show(id=dataset_id)
            resources = dataset.get("resources", [])

            if resource_index >= len(resources):
                return json.dumps({"error": f"Resource index {resource_index} out of range (max: {len(resources) - 1})"})

            resource = resources[resource_index]

            result = {
                "success": True,
                "data": {
                    "url": resource.get("url", ""),
                    "name": resource.get("name", ""),
                    "format": resource.get("format", ""),
                    "size": resource.get("size", "")
                }
            }

        elif command == "advanced_search":
            # Advanced search with multiple filters
            # Format: advanced_search country:ukr organization:ocha topic:"conflict, violence and peace" limit:20
            filters = {}
            limit = 10

            for arg in args[1:]:
                if ":" in arg:
                    key, value = arg.split(":", 1)
                    if key == "limit":
                        limit = int(value)
                    else:
                        filters[key] = value

            # Build query
            query_parts = []
            for key, value in filters.items():
                if " " in value:
                    query_parts.append(f'{key}:"{value}"')
                else:
                    query_parts.append(f'{key}:{value}')

            query = " ".join(query_parts) if query_parts else "*:*"
            search_result = package_search(q=query, rows=limit, sort="last_modified desc")

            result = {
                "success": True,
                "data": {
                    "filters": filters,
                    "count": search_result.get("count", 0),
                    "datasets": [extract_dataset_summary(ds) for ds in search_result.get("results", [])]
                }
            }

        elif command == "test_connection":
            # Test HDX connection
            try:
                search_result = package_search(q="*:*", rows=1)
                result = {
                    "success": True,
                    "data": {
                        "status": "connected",
                        "message": "HDX CKAN API is accessible",
                        "total_datasets": search_result.get("count", 0)
                    }
                }
            except Exception as e:
                result = {
                    "success": False,
                    "error": f"Connection test failed: {str(e)}"
                }

        else:
            result = {"error": f"Unknown command: {command}"}

        output = json.dumps(result, indent=2)
        print(output)
        return output

    except Exception as e:
        error_result = {"error": str(e)}
        error_output = json.dumps(error_result, indent=2)
        print(error_output)
        return error_output


if __name__ == "__main__":
    main()
