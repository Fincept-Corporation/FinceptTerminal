#!/usr/bin/env python3
"""
openAFRICA API Wrapper - African Open Data Portal API

A wrapper for the open.africa (CKAN-based) API to access African open data.
Implements the hierarchical pattern: Catalog → Datasets → Data Series

API Documentation:
- Base URL: https://africaopendata.org/api/3/action/
- Official CKAN API: https://docs.ckan.org/en/2.9/api/
- openAFRICA Guide: https://medium.com/code-for-africa/using-the-ckan-api-for-bulk-data-uploads-onto-openafrica-f481b6d33239

Environment Variables:
- AFRICA_API_KEY: Optional API key for enhanced rate limits (optional)
"""

import os
import sys
import json
import requests
from datetime import datetime
from typing import Dict, Any, Optional, List

# --- 1. CONFIGURATION ---
BASE_URL = "https://open.africa/api/3/action"
API_KEY = os.environ.get('AFRICA_API_KEY')  # Optional API key
TIMEOUT = 30
RATE_LIMIT_DELAY = 0.5  # Conservative delay for respectful usage

# --- 2. CORE REQUEST HANDLER ---
def _make_request(action: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for CKAN Action API with comprehensive error handling

    Args:
        action: CKAN action name (e.g., 'package_search', 'organization_list')
        params: Dictionary of parameters for the action

    Returns:
        Standardized response format with data, metadata, and error handling
    """
    try:
        # Construct the full URL
        url = f"{BASE_URL}/{action}"

        # Prepare headers
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Fincept-Terminal/1.0 (openAFRICA API Wrapper)'
        }

        # Add API key if available (for better rate limits)
        if API_KEY:
            headers['Authorization'] = API_KEY

        # Prepare request parameters
        request_params = {}
        if params:
            request_params.update(params)

        # Make the request
        response = requests.post(
            url,
            headers=headers,
            json=request_params,
            timeout=TIMEOUT
        )

        # Check HTTP status
        response.raise_for_status()

        # Parse JSON response
        api_response = response.json()

        # Check CKAN API success status
        if not api_response.get('success', False):
            error_msg = api_response.get('error', {}).get('message', 'Unknown CKAN API error')
            return {
                "data": [],
                "metadata": {
                    "source": "openAFRICA",
                    "action": action,
                    "parameters": params,
                    "http_status": response.status_code,
                    "url": url,
                    "last_updated": datetime.now().isoformat()
                },
                "error": f"CKAN API Error: {error_msg}"
            }

        # Extract the result data from CKAN response
        result_data = api_response.get('result', {})

        return {
            "data": result_data,
            "metadata": {
                "source": "openAFRICA",
                "action": action,
                "parameters": params,
                "http_status": response.status_code,
                "url": url,
                "last_updated": datetime.now().isoformat()
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {
            "data": [],
            "metadata": {
                "source": "openAFRICA",
                "action": action,
                "parameters": params,
                "http_status": e.response.status_code if e.response else None,
                "url": url if 'url' in locals() else f"{BASE_URL}/{action}",
                "last_updated": datetime.now().isoformat()
            },
            "error": f"HTTP Error {e.response.status_code if e.response else 'Unknown'}: {str(e)}"
        }

    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {
                "source": "openAFRICA",
                "action": action,
                "parameters": params,
                "url": f"{BASE_URL}/{action}",
                "last_updated": datetime.now().isoformat()
            },
            "error": "Request timeout - openAFRICA API took too long to respond"
        }

    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {
                "source": "openAFRICA",
                "action": action,
                "parameters": params,
                "url": f"{BASE_URL}/{action}",
                "last_updated": datetime.now().isoformat()
            },
            "error": "Connection error - unable to reach openAFRICA API"
        }

    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {
                "source": "openAFRICA",
                "action": action,
                "parameters": params,
                "url": f"{BASE_URL}/{action}",
                "last_updated": datetime.now().isoformat()
            },
            "error": f"Network error: {str(e)}"
        }

    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {
                "source": "openAFRICA",
                "action": action,
                "parameters": params,
                "url": url if 'url' in locals() else f"{BASE_URL}/{action}",
                "last_updated": datetime.now().isoformat()
            },
            "error": "Invalid JSON response from openAFRICA API"
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "source": "openAFRICA",
                "action": action,
                "parameters": params,
                "url": f"{BASE_URL}/{action}",
                "last_updated": datetime.now().isoformat()
            },
            "error": f"Unexpected error: {str(e)}"
        }

# --- 3. CORE FUNCTIONS - MODULAR ENDPOINT GROUPS ---

# CATALOG LEVEL FUNCTIONS

def get_organizations() -> Dict[str, Any]:
    """
    Get all organizations (data providers/catalogs) from openAFRICA
    This provides the catalog level - all data publishing organizations

    Returns:
        List of organizations with enhanced information
    """
    try:
        # Get organization list (returns list of organization names)
        result = _make_request('organization_list')

        if result["error"]:
            return result

        # Get basic organization names
        org_names = result.get("data", [])
        enhanced_orgs = []

        # Get details for each organization
        for org_name in org_names[:50]:  # Limit to first 50 to avoid timeouts
            try:
                # Get detailed info for each organization
                org_result = _make_request('organization_show', {'id': org_name})

                if not org_result["error"] and org_result.get("data"):
                    org_data = org_result["data"]
                    enhanced_org = {
                        "id": org_data.get("id", ""),
                        "name": org_data.get("name", ""),
                        "title": org_data.get("title", org_data.get("display_name", "")),
                        "description": org_data.get("description", ""),
                        "image_url": org_data.get("image_display_url", ""),
                        "created": org_data.get("created", ""),
                        "dataset_count": len(org_data.get("packages", [])),
                        "state": org_data.get("state", "active"),
                        "display_name": org_data.get("title", org_data.get("display_name", org_data.get("name", "")))
                    }
                    enhanced_orgs.append(enhanced_org)
                else:
                    # Fallback to basic info if detailed fetch fails
                    enhanced_org = {
                        "id": org_name,
                        "name": org_name,
                        "title": org_name.replace("-", " ").title(),
                        "description": "",
                        "image_url": "",
                        "created": "",
                        "dataset_count": 0,
                        "state": "active",
                        "display_name": org_name.replace("-", " ").title()
                    }
                    enhanced_orgs.append(enhanced_org)

                # Small delay to be respectful to the API
                import time
                time.sleep(0.1)

            except Exception as e:
                # Skip this organization but continue with others
                continue

        # Sort by dataset count (descending)
        enhanced_orgs.sort(key=lambda x: x.get("dataset_count", 0), reverse=True)

        result["data"] = enhanced_orgs
        result["metadata"]["count"] = len(enhanced_orgs)
        result["metadata"]["description"] = "List of data publishing organizations on openAFRICA (limited to 50 with most data)"

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching organizations: {str(e)}"
        }

def get_organization_details(organization_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific organization

    Args:
        organization_id: The ID or name of the organization

    Returns:
        Detailed organization information with datasets
    """
    try:
        # Get organization details
        result = _make_request('organization_show', {'id': organization_id, 'include_datasets': True})

        if result["error"]:
            return result

        # Enhance organization data
        org_data = result.get("data", {})
        enhanced_org = {
            "id": org_data.get("id", ""),
            "name": org_data.get("name", ""),
            "title": org_data.get("title", org_data.get("display_name", "")),
            "description": org_data.get("description", ""),
            "image_url": org_data.get("image_display_url", ""),
            "created": org_data.get("created", ""),
            "users": org_data.get("users", []),
            "dataset_count": len(org_data.get("packages", [])),
            "state": org_data.get("state", "active"),
            "tags": [tag.get("display_name", "") for tag in org_data.get("tags", [])],
            "datasets": org_data.get("packages", [])
        }

        result["data"] = enhanced_org
        result["metadata"]["organization_id"] = organization_id
        result["metadata"]["description"] = f"Detailed information for organization: {organization_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"organization_id": organization_id},
            "error": f"Error fetching organization details: {str(e)}"
        }

# DATASET LEVEL FUNCTIONS

def search_datasets(query: str = "", rows: int = 50, start: int = 0,
                   organization: Optional[str] = None, tags: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    Search for datasets using the powerful package_search action
    This is the primary dataset discovery method

    Args:
        query: Search keywords
        rows: Number of results to return (default 50)
        start: Starting offset for pagination (default 0)
        organization: Filter by organization ID/name
        tags: Filter by list of tags

    Returns:
        Search results with enhanced dataset information
    """
    try:
        # Build search parameters
        params = {
            'q': query,
            'rows': rows,
            'start': start,
            'sort': 'metadata_modified desc'  # Most recently updated first
        }

        # Add organization filter
        if organization:
            params['fq'] = f'organization:"{organization}"'

        # Add tags filter
        if tags:
            tag_filter = ' OR '.join([f'tags:"{tag}"' for tag in tags])
            if 'fq' in params:
                params['fq'] += f' AND ({tag_filter})'
            else:
                params['fq'] = f'({tag_filter})'

        # Make search request
        result = _make_request('package_search', params)

        if result["error"]:
            return result

        # Extract and enhance dataset results
        search_data = result.get("data", {})
        raw_datasets = search_data.get("results", [])

        enhanced_datasets = []
        for dataset in raw_datasets:
            enhanced_dataset = {
                "id": dataset.get("id", ""),
                "name": dataset.get("name", ""),
                "title": dataset.get("title", ""),
                "notes": dataset.get("notes", ""),
                "organization": dataset.get("organization", {}),
                "author": dataset.get("author", ""),
                "author_email": dataset.get("author_email", ""),
                "maintainer": dataset.get("maintainer", ""),
                "maintainer_email": dataset.get("maintainer_email", ""),
                "license": dataset.get("license_title", ""),
                "license_id": dataset.get("license_id", ""),
                "private": dataset.get("private", False),
                "state": dataset.get("state", "active"),
                "created": dataset.get("metadata_created", ""),
                "modified": dataset.get("metadata_modified", ""),
                "tags": [tag.get("display_name", "") for tag in dataset.get("tags", [])],
                "resources": dataset.get("resources", []),
                "resource_count": len(dataset.get("resources", [])),
                "download_url": None,
                "size": None
            }

            # Add primary download URL and size if available
            resources = dataset.get("resources", [])
            if resources:
                # Use the first resource as primary
                enhanced_dataset["download_url"] = resources[0].get("url", "")
                enhanced_dataset["size"] = resources[0].get("size", "")

            enhanced_datasets.append(enhanced_dataset)

        # Update result with enhanced data
        result["data"] = {
            "datasets": enhanced_datasets,
            "count": search_data.get("count", 0),
            "facets": search_data.get("facets", {}),
            "search_params": params
        }
        result["metadata"]["returned_count"] = len(enhanced_datasets)
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["description"] = f"Dataset search results for query: '{query}'"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"query": query, "rows": rows, "start": start},
            "error": f"Error searching datasets: {str(e)}"
        }

def get_dataset_details(dataset_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific dataset including all resources

    Args:
        dataset_id: The ID or name of the dataset

    Returns:
        Complete dataset information with all resources and data series
    """
    try:
        # Get dataset details
        result = _make_request('package_show', {'id': dataset_id})

        if result["error"]:
            return result

        # Enhance dataset data
        dataset_data = result.get("data", {})
        enhanced_dataset = {
            "id": dataset_data.get("id", ""),
            "name": dataset_data.get("name", ""),
            "title": dataset_data.get("title", ""),
            "notes": dataset_data.get("notes", ""),
            "organization": dataset_data.get("organization", {}),
            "author": dataset_data.get("author", ""),
            "author_email": dataset_data.get("author_email", ""),
            "maintainer": dataset_data.get("maintainer", ""),
            "maintainer_email": dataset_data.get("maintainer_email", ""),
            "license": dataset_data.get("license_title", ""),
            "license_id": dataset_data.get("license_id", ""),
            "version": dataset_data.get("version", ""),
            "private": dataset_data.get("private", False),
            "state": dataset_data.get("state", "active"),
            "created": dataset_data.get("metadata_created", ""),
            "modified": dataset_data.get("metadata_modified", ""),
            "tags": [tag.get("display_name", "") for tag in dataset_data.get("tags", [])],
            "resources": [],
            "resource_count": len(dataset_data.get("resources", [])),
            "extras": dataset_data.get("extras", []),
            "relationships": dataset_data.get("relationships", [])
        }

        # Enhance resources (data series)
        enhanced_resources = []
        for resource in dataset_data.get("resources", []):
            enhanced_resource = {
                "id": resource.get("id", ""),
                "name": resource.get("name", ""),
                "description": resource.get("description", ""),
                "url": resource.get("url", ""),
                "format": resource.get("format", ""),
                "mimetype": resource.get("mimetype", ""),
                "size": resource.get("size", ""),
                "created": resource.get("created", ""),
                "last_modified": resource.get("last_modified", ""),
                "cache_last_updated": resource.get("cache_last_updated", ""),
                "package_id": resource.get("package_id", ""),
                "state": resource.get("state", "active"),
                "resource_type": resource.get("resource_type", ""),
                "position": resource.get("position", 0),
                "datastore_active": resource.get("datastore_active", False),
                "webstore_url": resource.get("webstore_url", "")
            }
            enhanced_resources.append(enhanced_resource)

        enhanced_dataset["resources"] = enhanced_resources

        result["data"] = enhanced_dataset
        result["metadata"]["dataset_id"] = dataset_id
        result["metadata"]["resource_count"] = len(enhanced_resources)
        result["metadata"]["description"] = f"Complete dataset details for: {dataset_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"dataset_id": dataset_id},
            "error": f"Error fetching dataset details: {str(e)}"
        }

def get_datasets_by_organization(organization_id: str, rows: int = 100) -> Dict[str, Any]:
    """
    Get all datasets from a specific organization

    Args:
        organization_id: The ID or name of the organization
        rows: Maximum number of datasets to return

    Returns:
        List of datasets from the specified organization
    """
    try:
        # Use search with organization filter
        result = search_datasets(organization=organization_id, rows=rows)

        if result["error"]:
            return result

        result["metadata"]["organization_id"] = organization_id
        result["metadata"]["description"] = f"Datasets from organization: {organization_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"organization_id": organization_id},
            "error": f"Error fetching datasets by organization: {str(e)}"
        }

# DATA SERIES LEVEL FUNCTIONS

def get_resource_details(resource_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific resource (data series)

    Args:
        resource_id: The ID of the resource

    Returns:
        Complete resource information
    """
    try:
        # Get resource details
        result = _make_request('resource_show', {'id': resource_id})

        if result["error"]:
            return result

        # Enhance resource data
        resource_data = result.get("data", {})
        enhanced_resource = {
            "id": resource_data.get("id", ""),
            "name": resource_data.get("name", ""),
            "description": resource_data.get("description", ""),
            "url": resource_data.get("url", ""),
            "format": resource_data.get("format", ""),
            "mimetype": resource_data.get("mimetype", ""),
            "size": resource_data.get("size", ""),
            "created": resource_data.get("created", ""),
            "last_modified": resource_data.get("last_modified", ""),
            "cache_last_updated": resource_data.get("cache_last_updated", ""),
            "package_id": resource_data.get("package_id", ""),
            "state": resource_data.get("state", "active"),
            "resource_type": resource_data.get("resource_type", ""),
            "position": resource_data.get("position", 0),
            "datastore_active": resource_data.get("datastore_active", False),
            "webstore_url": resource_data.get("webstore_url", ""),
            "cache_url": resource_data.get("cache_url", "")
        }

        result["data"] = enhanced_resource
        result["metadata"]["resource_id"] = resource_id
        result["metadata"]["description"] = f"Resource details for: {resource_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"resource_id": resource_id},
            "error": f"Error fetching resource details: {str(e)}"
        }

def get_recent_datasets(limit: int = 50) -> Dict[str, Any]:
    """
    Get recently updated datasets across all organizations

    Args:
        limit: Maximum number of recent datasets to return

    Returns:
        List of recently updated datasets
    """
    try:
        result = search_datasets(rows=limit)

        if result["error"]:
            return result

        result["metadata"]["description"] = f"Recently updated datasets (limit: {limit})"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"limit": limit},
            "error": f"Error fetching recent datasets: {str(e)}"
        }

def get_popular_organizations(limit: int = 20) -> Dict[str, Any]:
    """
    Get organizations with the most datasets (most popular by data volume)

    Args:
        limit: Maximum number of organizations to return

    Returns:
        List of popular organizations
    """
    try:
        # Get all organizations
        result = get_organizations()

        if result["error"]:
            return result

        # Sort and limit
        orgs = result.get("data", [])
        orgs.sort(key=lambda x: x.get("dataset_count", 0), reverse=True)
        popular_orgs = orgs[:limit]

        result["data"] = popular_orgs
        result["metadata"]["count"] = len(popular_orgs)
        result["metadata"]["description"] = f"Popular organizations by dataset count (limit: {limit})"

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {"limit": limit},
            "error": f"Error fetching popular organizations: {str(e)}"
        }

# UTILITY FUNCTIONS

def get_all_datasets(rows: int = 100) -> Dict[str, Any]:
    """
    Get a comprehensive list of datasets across all organizations

    Args:
        rows: Maximum number of datasets to return

    Returns:
        List of datasets from all organizations
    """
    try:
        result = search_datasets(rows=rows)

        if result["error"]:
            return result

        result["metadata"]["description"] = f"List of datasets from all organizations (limit: {rows})"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"rows": rows},
            "error": f"Error fetching all datasets: {str(e)}"
        }

def list_all_dataset_names() -> Dict[str, Any]:
    """
    Get a simple list of all dataset names available on the portal

    Returns:
        Simple list of dataset names/IDs
    """
    try:
        result = _make_request('package_list')

        if result["error"]:
            return result

        dataset_names = result.get("data", [])

        result["metadata"]["count"] = len(dataset_names)
        result["metadata"]["description"] = "Simple list of all dataset names on openAFRICA"

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error listing dataset names: {str(e)}"
        }

# --- 4. CLI INTERFACE ---

def main():
    """
    CLI interface for openAFRICA API Wrapper

    Usage:
        python openafrica_api.py <command> [arguments]

    Available Commands:
        organizations - List all data publishing organizations
        org-details <id> - Get details for a specific organization
        search <query> [rows] - Search datasets with optional result limit
        dataset-details <id> - Get detailed information about a specific dataset
        org-datasets <org_id> [rows] - Get datasets from a specific organization
        resource-details <id> - Get details for a specific resource
        recent [limit] - Get recently updated datasets
        popular-orgs [limit] - Get popular organizations by dataset count
        all-datasets [rows] - Get datasets from all organizations
        list-names - Get simple list of all dataset names
    """

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python openafrica_api.py <command> [arguments]",
            "available_commands": [
                "organizations - List all data publishing organizations",
                "org-details <id> - Get details for a specific organization",
                "search <query> [rows] - Search datasets with optional result limit",
                "dataset-details <id> - Get detailed information about a specific dataset",
                "org-datasets <org_id> [rows] - Get datasets from a specific organization",
                "resource-details <id> - Get details for a specific resource",
                "recent [limit] - Get recently updated datasets",
                "popular-orgs [limit] - Get popular organizations by dataset count",
                "all-datasets [rows] - Get datasets from all organizations",
                "list-names - Get simple list of all dataset names"
            ],
            "examples": [
                "python openafrica_api.py organizations",
                "python openafrica_api.py search \"health\" 25",
                "python openafrica_api.py dataset-details kenya-health-data",
                "python openafrica_api.py org-datasets world-bank",
                "python openafrica_api.py recent 20"
            ],
            "environment_variables": {
                "AFRICA_API_KEY": "Optional API key for enhanced rate limits"
            }
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "organizations":
            result = get_organizations()

        elif command == "org-details":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "error": "Organization ID required",
                    "usage": "python openafrica_api.py org-details <organization_id>"
                }))
                sys.exit(1)
            result = get_organization_details(sys.argv[2])

        elif command == "search":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "error": "Search query required",
                    "usage": "python openafrica_api.py search <query> [rows]"
                }))
                sys.exit(1)
            query = sys.argv[2]
            rows = int(sys.argv[3]) if len(sys.argv) > 3 else 50
            result = search_datasets(query=query, rows=rows)

        elif command == "dataset-details":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "error": "Dataset ID required",
                    "usage": "python openafrica_api.py dataset-details <dataset_id>"
                }))
                sys.exit(1)
            result = get_dataset_details(sys.argv[2])

        elif command == "org-datasets":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "error": "Organization ID required",
                    "usage": "python openafrica_api.py org-datasets <organization_id> [rows]"
                }))
                sys.exit(1)
            org_id = sys.argv[2]
            rows = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = get_datasets_by_organization(org_id, rows)

        elif command == "resource-details":
            if len(sys.argv) < 3:
                print(json.dumps({
                    "error": "Resource ID required",
                    "usage": "python openafrica_api.py resource-details <resource_id>"
                }))
                sys.exit(1)
            result = get_resource_details(sys.argv[2])

        elif command == "recent":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 50
            result = get_recent_datasets(limit)

        elif command == "popular-orgs":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 20
            result = get_popular_organizations(limit)

        elif command == "all-datasets":
            rows = int(sys.argv[2]) if len(sys.argv) > 2 else 100
            result = get_all_datasets(rows)

        elif command == "list-names":
            result = list_all_dataset_names()

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "organizations", "org-details", "search", "dataset-details",
                    "org-datasets", "resource-details", "recent", "popular-orgs",
                    "all-datasets", "list-names"
                ]
            }))
            sys.exit(1)

        # Add execution timestamp
        if isinstance(result, dict):
            result["executed_at"] = datetime.now().isoformat()

        print(json.dumps(result, indent=2))

    except ValueError as e:
        print(json.dumps({
            "error": f"Invalid argument: {str(e)}",
            "command": command,
            "timestamp": datetime.now().isoformat()
        }))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({
            "error": f"Command execution failed: {str(e)}",
            "command": command,
            "timestamp": datetime.now().isoformat()
        }))
        sys.exit(1)

if __name__ == "__main__":
    main()