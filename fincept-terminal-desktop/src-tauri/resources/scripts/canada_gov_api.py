#!/usr/bin/env python3
"""
Government of Canada Open Government Portal API Wrapper
Fetches Canadian government data using hierarchical structure:
1. Catalogue - Lists all data publishers (organizations)
2. Datasets - Lists datasets from a specific publisher
3. Resources - Gets actual data files (CSV/XLS) for a dataset

Usage: python canada_gov_api.py <command> [args]
"""

import sys
import json
import os
import requests
from typing import Dict, List, Optional, Any
from datetime import datetime
import urllib.parse

# --- 1. CONFIGURATION ---
BASE_URL = "https://open.canada.ca/data/api/action"
API_KEY = os.environ.get('CANADA_GOV_API_KEY')
TIMEOUT = 30

def _make_request(action: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for Government of Canada API

    Args:
        action: CKAN action name (e.g., 'organization_list', 'package_search')
        params: Query parameters for the request

    Returns:
        JSON response data or error information
    """
    try:
        url = f"{BASE_URL}/{action}"

        # Default parameters
        if params is None:
            params = {}

        # Setup headers
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Fincept-Terminal/1.0'
        }

        # Add API key to header if available
        if API_KEY:
            headers['Authorization'] = API_KEY

        # Make request with timeout
        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        data = response.json()

        # Check for CKAN API errors
        if not data.get('success', False):
            error_msg = data.get('error', {}).get('message', 'Unknown CKAN API error')
            return {
                "data": [],
                "metadata": {"source": "open.canada.ca", "last_updated": datetime.now().isoformat()},
                "error": f"CKAN API Error: {error_msg}"
            }

        return {
            "data": data.get('result', []),
            "metadata": {
                "source": "open.canada.ca",
                "last_updated": datetime.now().isoformat(),
                "action": action
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"HTTP Error: {e.response.status_code} - {e.response.text}"
        }
    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {},
            "error": "Request timeout. The Government of Canada API is taking too long to respond."
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {},
            "error": "Connection error. Unable to connect to Government of Canada API."
        }
    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Network or request error: {str(e)}"
        }
    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {},
            "error": "Invalid JSON response from Government of Canada API"
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"An unexpected error occurred: {str(e)}"
        }

# --- 2. CORE FUNCTIONS (GROUPED BY API CATEGORY) ---

# ====== CATALOGUE (ORGANIZATIONS/PUBLISHERS) ======
def get_publishers() -> Dict[str, Any]:
    """
    Get list of all data publishers (organizations) in Government of Canada portal

    Returns:
        JSON response with publisher list
    """
    try:
        result = _make_request("organization_list")

        if result["error"]:
            return result

        # Enhance publisher data with basic info
        enhanced_data = []
        publishers = result.get("data", [])

        for publisher_id in publishers:
            enhanced_publisher = {
                "id": publisher_id,
                "name": publisher_id.replace("-", " ").title(),
                "display_name": publisher_id.replace("-", " ").title()
            }
            enhanced_data.append(enhanced_publisher)

        result["data"] = enhanced_data
        result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching publishers: {str(e)}"
        }

def get_publisher_details(publisher_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific publisher

    Args:
        publisher_id: The unique ID of the publisher

    Returns:
        JSON response with publisher details
    """
    try:
        params = {'id': publisher_id}
        result = _make_request("organization_show", params)

        if result["error"]:
            return result

        # Enhance publisher data
        publisher_data = result.get("data", {})
        enhanced_publisher = {
            "id": publisher_data.get("id"),
            "name": publisher_data.get("name"),
            "title": publisher_data.get("title"),
            "description": publisher_data.get("description"),
            "image_url": publisher_data.get("image_display_url"),
            "created": publisher_data.get("created"),
            "num_datasets": publisher_data.get("package_count", 0),
            "users": publisher_data.get("users", [])
        }

        result["data"] = enhanced_publisher
        result["metadata"]["publisher_id"] = publisher_id

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching publisher details: {str(e)}"
        }

# ====== DATASETS ======
def get_datasets_by_publisher(publisher_id: str, rows: int = 100) -> Dict[str, Any]:
    """
    Get all datasets published by a specific publisher

    Args:
        publisher_id: The unique ID of the publisher
        rows: Number of datasets to return (default: 100)

    Returns:
        JSON response with dataset list
    """
    try:
        # Search for datasets by owner_org
        query = f"owner_org:{publisher_id}"
        params = {'q': query, 'rows': rows}

        result = _make_request("package_search", params)

        if result["error"]:
            return result

        # Parse search results
        search_data = result.get("data", {})
        datasets = search_data.get("results", [])

        # Enhance dataset data
        enhanced_data = []
        for dataset in datasets:
            enhanced_dataset = {
                "id": dataset.get("id"),
                "name": dataset.get("name"),
                "title": dataset.get("title"),
                "notes": dataset.get("notes", ""),
                "publisher_id": publisher_id,
                "metadata_created": dataset.get("metadata_created"),
                "metadata_modified": dataset.get("metadata_modified"),
                "state": dataset.get("state"),
                "num_resources": len(dataset.get("resources", [])),
                "tags": [tag.get("display_name") for tag in dataset.get("tags", [])]
            }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["publisher_id"] = publisher_id
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["returned_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching datasets: {str(e)}"
        }

def search_datasets(query: str, rows: int = 50) -> Dict[str, Any]:
    """
    Search for datasets across all publishers

    Args:
        query: Search query string
        rows: Number of results to return (default: 50)

    Returns:
        JSON response with search results
    """
    try:
        params = {'q': query, 'rows': rows}

        result = _make_request("package_search", params)

        if result["error"]:
            return result

        # Parse search results
        search_data = result.get("data", {})
        datasets = search_data.get("results", [])

        # Enhance dataset data
        enhanced_data = []
        for dataset in datasets:
            enhanced_dataset = {
                "id": dataset.get("id"),
                "name": dataset.get("name"),
                "title": dataset.get("title"),
                "notes": dataset.get("notes", ""),
                "organization": dataset.get("organization", {}).get("name") if dataset.get("organization") else None,
                "metadata_created": dataset.get("metadata_created"),
                "metadata_modified": dataset.get("metadata_modified"),
                "num_resources": len(dataset.get("resources", [])),
                "tags": [tag.get("display_name") for tag in dataset.get("tags", [])]
            }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["query"] = query
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["returned_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching datasets: {str(e)}"
        }

def get_dataset_details(dataset_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific dataset

    Args:
        dataset_id: The unique ID or name of the dataset

    Returns:
        JSON response with dataset details
    """
    try:
        params = {'id': dataset_id}
        result = _make_request("package_show", params)

        if result["error"]:
            return result

        # Enhance dataset data
        dataset_data = result.get("data", {})
        enhanced_dataset = {
            "id": dataset_data.get("id"),
            "name": dataset_data.get("name"),
            "title": dataset_data.get("title"),
            "notes": dataset_data.get("notes", ""),
            "url": dataset_data.get("url"),
            "author": dataset_data.get("author"),
            "author_email": dataset_data.get("author_email"),
            "maintainer": dataset_data.get("maintainer"),
            "maintainer_email": dataset_data.get("maintainer_email"),
            "license_id": dataset_data.get("license_id"),
            "license_title": dataset_data.get("license_title"),
            "organization": dataset_data.get("organization", {}).get("name") if dataset_data.get("organization") else None,
            "metadata_created": dataset_data.get("metadata_created"),
            "metadata_modified": dataset_data.get("metadata_modified"),
            "state": dataset_data.get("state"),
            "version": dataset_data.get("version"),
            "tags": [tag.get("display_name") for tag in dataset_data.get("tags", [])]
        }

        result["data"] = enhanced_dataset
        result["metadata"]["dataset_id"] = dataset_id

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching dataset details: {str(e)}"
        }

# ====== RESOURCES (DATA FILES) ======
def get_dataset_resources(dataset_id: str) -> Dict[str, Any]:
    """
    Get all data files (resources) for a specific dataset

    Args:
        dataset_id: The unique ID or name of the dataset

    Returns:
        JSON response with resource list
    """
    try:
        params = {'id': dataset_id}
        result = _make_request("package_show", params)

        if result["error"]:
            return result

        # Extract resources from dataset
        dataset_data = result.get("data", {})
        resources = dataset_data.get("resources", [])

        # Enhance resource data
        enhanced_data = []
        for resource in resources:
            enhanced_resource = {
                "id": resource.get("id"),
                "name": resource.get("name"),
                "description": resource.get("description", ""),
                "format": resource.get("format", ""),
                "url": resource.get("url", ""),
                "size": resource.get("size"),
                "mimetype": resource.get("mimetype"),
                "mimetype_inner": resource.get("mimetype_inner"),
                "created": resource.get("created"),
                "last_modified": resource.get("last_modified"),
                "resource_type": resource.get("resource_type"),
                "package_id": dataset_id,
                "position": resource.get("position"),
                "cache_last_updated": resource.get("cache_last_updated"),
                "webstore_last_updated": resource.get("webstore_last_updated")
            }
            enhanced_data.append(enhanced_resource)

        result["data"] = enhanced_data
        result["metadata"]["dataset_id"] = dataset_id
        result["metadata"]["dataset_name"] = dataset_data.get("name")
        result["metadata"]["resource_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching dataset resources: {str(e)}"
        }

def get_resource_info(resource_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific resource

    Args:
        resource_id: The unique ID of the resource

    Returns:
        JSON response with resource details
    """
    try:
        params = {'id': resource_id}
        result = _make_request("resource_show", params)

        if result["error"]:
            return result

        # Enhance resource data
        resource_data = result.get("data", {})
        enhanced_resource = {
            "id": resource_data.get("id"),
            "name": resource_data.get("name"),
            "description": resource_data.get("description", ""),
            "format": resource_data.get("format", ""),
            "url": resource_data.get("url", ""),
            "size": resource_data.get("size"),
            "mimetype": resource_data.get("mimetype"),
            "mimetype_inner": resource_data.get("mimetype_inner"),
            "created": resource_data.get("created"),
            "last_modified": resource_data.get("last_modified"),
            "resource_type": resource_data.get("resource_type"),
            "package_id": resource_data.get("package_id"),
            "position": resource_data.get("position"),
            "cache_last_updated": resource_data.get("cache_last_updated"),
            "webstore_last_updated": resource_data.get("webstore_last_updated")
        }

        result["data"] = enhanced_resource
        result["metadata"]["resource_id"] = resource_id

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching resource info: {str(e)}"
        }

def download_resource_preview(resource_url: str, max_lines: int = 10) -> Dict[str, Any]:
    """
    Download a preview of a resource (first few lines of CSV/TSV)

    Args:
        resource_url: Direct URL to the resource file
        max_lines: Maximum number of lines to preview (default: 10)

    Returns:
        JSON response with preview data
    """
    try:
        headers = {
            'User-Agent': 'Fincept-Terminal/1.0'
        }

        # Make request to download file
        response = requests.get(resource_url, headers=headers, timeout=TIMEOUT, stream=True)
        response.raise_for_status()

        # Check if it's a text-based file
        content_type = response.headers.get('content-type', '').lower()
        if not ('csv' in content_type or 'text' in content_type or 'excel' in content_type or 'zip' in content_type):
            return {
                "data": [],
                "metadata": {"url": resource_url, "content_type": content_type},
                "error": f"Preview not available for file type: {content_type}"
            }

        # Read first few lines
        lines = []
        line_count = 0

        for line in response.iter_lines(decode_unicode=True):
            if line_count >= max_lines:
                break
            if line.strip():  # Skip empty lines
                lines.append(line)
                line_count += 1

        # Try to parse as CSV if it looks like CSV
        preview_data = {
            "raw_lines": lines,
            "line_count": len(lines),
            "url": resource_url,
            "content_type": content_type
        }

        # Basic CSV parsing for preview
        if lines and ',' in lines[0]:
            try:
                import csv
                from io import StringIO

                csv_reader = csv.reader(StringIO('\n'.join(lines)))
                csv_data = list(csv_reader)

                preview_data["csv_preview"] = {
                    "headers": csv_data[0] if csv_data else [],
                    "rows": csv_data[1:] if len(csv_data) > 1 else [],
                    "total_columns": len(csv_data[0]) if csv_data else 0
                }
            except:
                pass  # Keep raw lines if CSV parsing fails

        return {
            "data": preview_data,
            "metadata": {
                "url": resource_url,
                "preview_lines": len(lines),
                "content_type": content_type
            },
            "error": None
        }

    except requests.exceptions.RequestException as e:
        return {
            "data": {},
            "metadata": {"url": resource_url},
            "error": f"Failed to download resource: {str(e)}"
        }
    except Exception as e:
        return {
            "data": {},
            "metadata": {"url": resource_url},
            "error": f"Error processing resource: {str(e)}"
        }

# ====== UTILITY FUNCTIONS ======
def get_popular_publishers(limit: int = 20) -> Dict[str, Any]:
    """
    Get popular publishers based on dataset count

    Args:
        limit: Maximum number of publishers to return

    Returns:
        JSON response with popular publishers
    """
    try:
        # Get all publishers first
        publishers_result = get_publishers()

        if publishers_result["error"]:
            return publishers_result

        publishers = publishers_result.get("data", [])

        # Get dataset count for each publisher (limited for performance)
        popular_publishers = []

        for publisher in publishers[:limit]:
            try:
                datasets_result = get_datasets_by_publisher(publisher["id"], 1)
                if not datasets_result["error"]:
                    search_data = datasets_result.get("metadata", {})
                    total_count = search_data.get("total_count", 0)

                    popular_publishers.append({
                        "id": publisher["id"],
                        "name": publisher["name"],
                        "dataset_count": total_count
                    })
            except:
                continue  # Skip if publisher fails

        # Sort by dataset count
        popular_publishers.sort(key=lambda x: x["dataset_count"], reverse=True)

        return {
            "data": popular_publishers,
            "metadata": {
                "count": len(popular_publishers),
                "limit": limit
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching popular publishers: {str(e)}"
        }

def get_recent_datasets(limit: int = 50) -> Dict[str, Any]:
    """
    Get recently updated datasets

    Args:
        limit: Maximum number of datasets to return

    Returns:
        JSON response with recent datasets
    """
    try:
        params = {'rows': limit, 'sort': 'metadata_modified desc'}
        result = _make_request("package_search", params)

        if result["error"]:
            return result

        # Parse search results
        search_data = result.get("data", {})
        datasets = search_data.get("results", [])

        # Enhance dataset data
        enhanced_data = []
        for dataset in datasets:
            enhanced_dataset = {
                "id": dataset.get("id"),
                "name": dataset.get("name"),
                "title": dataset.get("title"),
                "notes": dataset.get("notes", ""),
                "organization": dataset.get("organization", {}).get("name") if dataset.get("organization") else None,
                "metadata_modified": dataset.get("metadata_modified"),
                "num_resources": len(dataset.get("resources", [])),
                "tags": [tag.get("display_name") for tag in dataset.get("tags", [])]
            }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["returned_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching recent datasets: {str(e)}"
        }

# --- 3. CLI INTERFACE ---
def main():
    """CLI interface for Government of Canada API"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python canada_gov_api.py <command> <args>",
            "available_commands": [
                "publishers",
                "publisher-details <publisher_id>",
                "datasets <publisher_id> [rows]",
                "search <query> [rows]",
                "dataset-details <dataset_id>",
                "resources <dataset_id>",
                "resource-info <resource_id>",
                "preview <resource_url>",
                "popular-publishers [limit]",
                "recent-datasets [limit]"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "publishers":
            result = get_publishers()

        elif command == "publisher-details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: publisher-details <publisher_id>"}))
                sys.exit(1)
            publisher_id = sys.argv[2]
            result = get_publisher_details(publisher_id)

        elif command == "datasets":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: datasets <publisher_id> [rows]"}))
                sys.exit(1)
            publisher_id = sys.argv[2]
            rows = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = get_datasets_by_publisher(publisher_id, rows)

        elif command == "search":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: search <query> [rows]"}))
                sys.exit(1)
            query = " ".join(sys.argv[2:-1]) if len(sys.argv) > 3 else sys.argv[2]
            rows = int(sys.argv[-1]) if len(sys.argv) > 3 and sys.argv[-1].isdigit() else 50
            result = search_datasets(query, rows)

        elif command == "dataset-details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: dataset-details <dataset_id>"}))
                sys.exit(1)
            dataset_id = sys.argv[2]
            result = get_dataset_details(dataset_id)

        elif command == "resources":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: resources <dataset_id>"}))
                sys.exit(1)
            dataset_id = sys.argv[2]
            result = get_dataset_resources(dataset_id)

        elif command == "resource-info":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: resource-info <resource_id>"}))
                sys.exit(1)
            resource_id = sys.argv[2]
            result = get_resource_info(resource_id)

        elif command == "preview":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: preview <resource_url>"}))
                sys.exit(1)
            resource_url = sys.argv[2]
            result = download_resource_preview(resource_url)

        elif command == "popular-publishers":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 20
            result = get_popular_publishers(limit)

        elif command == "recent-datasets":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 50
            result = get_recent_datasets(limit)

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "publishers",
                    "publisher-details <publisher_id>",
                    "datasets <publisher_id> [rows]",
                    "search <query> [rows]",
                    "dataset-details <dataset_id>",
                    "resources <dataset_id>",
                    "resource-info <resource_id>",
                    "preview <resource_url>",
                    "popular-publishers [limit]",
                    "recent-datasets [limit]"
                ]
            }

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({
            "error": f"Command execution failed: {str(e)}",
            "command": command,
            "timestamp": datetime.now().isoformat()
        }))

if __name__ == "__main__":
    main()