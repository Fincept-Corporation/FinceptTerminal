#!/usr/bin/env python3
"""
datos.gob.es (Spanish Government) API Wrapper
Fetches Spanish government data using hierarchical structure:
1. Catalogue - Lists all data publishers and organizations
2. Datasets - Lists datasets from specific publishers
3. Series/Resources - Gets actual data files and resources for datasets

Usage: python datos_gob_es_api.py <command> [args]
"""

import sys
import json
import os
import requests
from typing import Dict, List, Optional, Any
from datetime import datetime
import urllib.parse

# --- 1. CONFIGURATION ---
BASE_URL = "https://datos.gob.es/api/action"
API_KEY = os.environ.get('DATOS_GOB_ES_API_KEY')
TIMEOUT = 30

def _make_request(endpoint: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for datos.gob.es API

    Args:
        endpoint: API endpoint path
        params: Query parameters for the request

    Returns:
        JSON response data or error information
    """
    try:
        url = f"{BASE_URL}/{endpoint}"

        # Default parameters
        if params is None:
            params = {}

        # Setup headers
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json'
        }

        # Add API key to parameters if available
        if API_KEY:
            params['api_key'] = API_KEY

        # Make request with timeout
        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        data = response.json()

        # Check for API errors in response
        if isinstance(data, dict) and data.get('error'):
            return {
                "data": [],
                "metadata": {"source": "datos.gob.es", "last_updated": datetime.now().isoformat()},
                "error": f"API Error: {data.get('error', 'Unknown error')}"
            }

        return {
            "data": data,
            "metadata": {
                "source": "datos.gob.es",
                "last_updated": datetime.now().isoformat(),
                "endpoint": endpoint
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        error_text = e.response.text[:200] if e.response.text else "No error details"
        if "mantenimiento" in error_text.lower() or "maintenance" in error_text.lower():
            return {
                "data": [],
                "metadata": {},
                "error": "The datos.gob.es API is currently under maintenance. Please try again later."
            }
        return {
            "data": [],
            "metadata": {},
            "error": f"HTTP Error: {e.response.status_code} - {error_text}"
        }
    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {},
            "error": "Request timeout. The datos.gob.es API is taking too long to respond."
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {},
            "error": "Connection error. Unable to connect to datos.gob.es API."
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
            "error": "The datos.gob.es API is currently under maintenance or unavailable. Please try again later."
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
    Get list of all data publishers (organizations) in datos.gob.es

    Returns:
        JSON response with publisher list
    """
    try:
        result = _make_request("organization_list")

        if result["error"]:
            return result

        # Process organization data
        organizations = result.get("data", [])
        enhanced_data = []

        for org_id in organizations:
            enhanced_publisher = {
                "id": org_id,
                "name": org_id.replace("-", " ").title(),
                "title": org_id.replace("-", " ").title(),
                "description": "",
                "uri": "",
                "dataset_count": 0
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
        publisher_id: The unique ID or URI of the publisher

    Returns:
        JSON response with publisher details
    """
    try:
        params = {'id': publisher_id}
        result = _make_request("organization_show", params)

        if result["error"]:
            return result

        # Enhance publisher data
        publisher_data = result["data"]
        if isinstance(publisher_data, dict):
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
        else:
            enhanced_publisher = {
                "id": publisher_id,
                "name": str(publisher_data) if publisher_data else publisher_id,
                "title": str(publisher_data) if publisher_data else publisher_id,
                "description": "",
                "image_url": "",
                "created": "",
                "num_datasets": 0,
                "users": []
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
        publisher_id: The unique ID or URI of the publisher
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
        dataset_id: The unique ID or URI of the dataset

    Returns:
        JSON response with dataset details
    """
    try:
        params = {'id': dataset_id}
        result = _make_request("package_show", params)

        if result["error"]:
            return result

        # Enhance dataset data
        dataset_data = result["data"]
        if isinstance(dataset_data, dict):
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
        else:
            enhanced_dataset = {
                "id": dataset_id,
                "name": str(dataset_data) if dataset_data else dataset_id,
                "title": str(dataset_data) if dataset_data else dataset_id,
                "notes": "",
                "url": "",
                "author": "",
                "author_email": "",
                "maintainer": "",
                "maintainer_email": "",
                "license_id": "",
                "license_title": "",
                "organization": None,
                "metadata_created": "",
                "metadata_modified": "",
                "state": "",
                "version": "",
                "tags": []
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

# ====== SERIES/RESOURCES (DATA FILES) ======
def get_dataset_resources(dataset_id: str) -> Dict[str, Any]:
    """
    Get all data files (distributions/resources) for a specific dataset

    Args:
        dataset_id: The unique ID or URI of the dataset

    Returns:
        JSON response with resource list
    """
    try:
        # Get dataset details first to extract resources
        dataset_result = get_dataset_details(dataset_id)
        if dataset_result["error"]:
            return dataset_result

        dataset_data = dataset_result["data"]

        # Extract resources/distributions from dataset
        resources = (dataset_data.get("distribution") or
                    dataset_data.get("resources") or
                    dataset_data.get("distributions") or
                    [])

        # If no resources in dataset details, try specific endpoint
        if not resources:
            result = _make_request(f"catalog/dataset/{urllib.parse.quote(dataset_id)}/distribution")
            if not result["error"] and result["data"]:
                resource_data = result["data"]
                if isinstance(resource_data, dict):
                    resources = resource_data.get("results") or resource_data.get("items") or []
                elif isinstance(resource_data, list):
                    resources = resource_data

        # Enhance resource data
        enhanced_data = []
        for resource in resources:
            if isinstance(resource, dict):
                enhanced_resource = {
                    "id": resource.get("id") or resource.get("uri") or resource.get("identifier", ""),
                    "name": resource.get("name") or resource.get("title", ""),
                    "title": resource.get("title") or resource.get("name", ""),
                    "description": resource.get("description") or resource.get("notes", ""),
                    "format": resource.get("format") or resource.get("mediaType") or resource.get("mimetype", ""),
                    "url": resource.get("accessURL") or resource.get("downloadURL") or resource.get("url", ""),
                    "size": resource.get("byteSize") or resource.get("size", ""),
                    "mimetype": resource.get("mediaType") or resource.get("mimetype", ""),
                    "license": resource.get("license") or resource.get("rights", ""),
                    "created": resource.get("issued") or resource.get("created", ""),
                    "modified": resource.get("modified") or resource.get("updated", ""),
                    "resource_type": resource.get("type") or resource.get("@type", ""),
                    "dataset_id": dataset_id,
                    "spatial": resource.get("spatial") or resource.get("coverage", []),
                    "temporal": resource.get("temporal") or resource.get("temporal_coverage", "")
                }
            else:
                enhanced_resource = {
                    "id": str(resource),
                    "name": str(resource),
                    "title": str(resource),
                    "description": "",
                    "format": "",
                    "url": "",
                    "size": "",
                    "mimetype": "",
                    "license": "",
                    "created": "",
                    "modified": "",
                    "resource_type": "",
                    "dataset_id": dataset_id,
                    "spatial": [],
                    "temporal": ""
                }
            enhanced_data.append(enhanced_resource)

        return {
            "data": enhanced_data,
            "metadata": {
                "dataset_id": dataset_id,
                "dataset_name": dataset_data.get("name", ""),
                "resource_count": len(enhanced_data)
            },
            "error": None
        }

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
        resource_id: The unique ID or URI of the resource

    Returns:
        JSON response with resource details
    """
    try:
        # Try different endpoint patterns for resource details
        possible_endpoints = [
            f"catalog/distribution/{urllib.parse.quote(resource_id)}",
            f"distribution/{urllib.parse.quote(resource_id)}",
            f"resource/{urllib.parse.quote(resource_id)}"
        ]

        for endpoint in possible_endpoints:
            result = _make_request(endpoint)
            if not result["error"] and result["data"]:
                break
        else:
            # If no specific endpoint works, try searching
            result = _make_request("catalog", {"q": resource_id, "type": "distribution", "rows": 1})

        if result["error"]:
            return result

        # Enhance resource data
        resource_data = result["data"]
        if isinstance(resource_data, dict):
            enhanced_resource = {
                "id": resource_data.get("id") or resource_data.get("uri") or resource_id,
                "name": resource_data.get("name") or resource_data.get("title", ""),
                "title": resource_data.get("title") or resource_data.get("name", ""),
                "description": resource_data.get("description") or resource_data.get("notes", ""),
                "format": resource_data.get("format") or resource_data.get("mediaType") or resource_data.get("mimetype", ""),
                "url": resource_data.get("accessURL") or resource_data.get("downloadURL") or resource_data.get("url", ""),
                "size": resource_data.get("byteSize") or resource_data.get("size", ""),
                "mimetype": resource_data.get("mediaType") or resource_data.get("mimetype", ""),
                "license": resource_data.get("license") or resource_data.get("rights", ""),
                "created": resource_data.get("issued") or resource_data.get("created", ""),
                "modified": resource_data.get("modified") or resource_data.get("updated", ""),
                "resource_type": resource_data.get("type") or resource_data.get("@type", ""),
                "dataset_id": resource_data.get("dataset") or resource_data.get("isPartOf", "") or "",
                "spatial": resource_data.get("spatial") or resource_data.get("coverage", []),
                "temporal": resource_data.get("temporal") or resource_data.get("temporal_coverage", ""),
                "conformsTo": resource_data.get("conformsTo") or resource_data.get("schema", ""),
                "language": resource_data.get("language") or resource_data.get("inLanguage", [])
            }
        else:
            enhanced_resource = {
                "id": resource_id,
                "name": str(resource_data) if resource_data else resource_id,
                "title": str(resource_data) if resource_data else resource_id,
                "description": "",
                "format": "",
                "url": "",
                "size": "",
                "mimetype": "",
                "license": "",
                "created": "",
                "modified": "",
                "resource_type": "",
                "dataset_id": "",
                "spatial": [],
                "temporal": "",
                "conformsTo": "",
                "language": []
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

def get_series_data(resource_id: str, format_type: str = "json") -> Dict[str, Any]:
    """
    Get actual data series from a resource

    Args:
        resource_id: The unique ID or URI of the resource
        format_type: Response format (json, csv, xml, etc.)

    Returns:
        JSON response with series data
    """
    try:
        # Get resource info first to find the data URL
        resource_result = get_resource_info(resource_id)
        if resource_result["error"]:
            return resource_result

        resource_data = resource_result["data"]
        data_url = resource_data.get("url")

        if not data_url:
            return {
                "data": {},
                "metadata": {"resource_id": resource_id},
                "error": f"No data URL found for resource: {resource_id}"
            }

        # Try to download actual data
        headers = {
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json,text/csv,application/xml,text/xml'
        }

        # Make request to download data
        response = requests.get(data_url, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        content_type = response.headers.get('content-type', '').lower()

        # Parse response based on content type
        if 'json' in content_type or format_type == "json":
            try:
                series_data = response.json()
            except:
                series_data = {"raw_data": response.text}
        elif 'csv' in content_type or format_type == "csv":
            try:
                import csv
                from io import StringIO
                csv_reader = csv.DictReader(StringIO(response.text))
                series_data = list(csv_reader)
            except:
                series_data = {"raw_data": response.text}
        elif 'xml' in content_type or format_type == "xml":
            series_data = {"raw_data": response.text}
        else:
            series_data = {"raw_data": response.text}

        return {
            "data": series_data,
            "metadata": {
                "resource_id": resource_id,
                "data_url": data_url,
                "content_type": content_type,
                "format": format_type
            },
            "error": None
        }

    except requests.exceptions.RequestException as e:
        return {
            "data": {},
            "metadata": {"resource_id": resource_id},
            "error": f"Failed to download series data: {str(e)}"
        }
    except Exception as e:
        return {
            "data": {},
            "metadata": {"resource_id": resource_id},
            "error": f"Error processing series data: {str(e)}"
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
        if not ('csv' in content_type or 'text' in content_type or 'json' in content_type or 'xml' in content_type):
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
        if lines and (',' in lines[0] or ';' in lines[0] or '\t' in lines[0]):
            try:
                import csv
                from io import StringIO

                delimiter = ',' if ',' in lines[0] else ';' if ';' in lines[0] else '\t'
                csv_reader = csv.reader(StringIO('\n'.join(lines)), delimiter=delimiter)
                csv_data = list(csv_reader)

                preview_data["csv_preview"] = {
                    "headers": csv_data[0] if csv_data else [],
                    "rows": csv_data[1:] if len(csv_data) > 1 else [],
                    "total_columns": len(csv_data[0]) if csv_data else 0,
                    "delimiter": delimiter
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

        for publisher in publishers[:limit * 2]:  # Get more to sort later
            try:
                datasets_result = get_datasets_by_publisher(publisher["id"], 1)
                if not datasets_result["error"]:
                    metadata = datasets_result.get("metadata", {})
                    total_count = metadata.get("total_count", 0)

                    popular_publishers.append({
                        "id": publisher["id"],
                        "name": publisher["name"],
                        "title": publisher["title"],
                        "dataset_count": total_count
                    })
            except:
                continue  # Skip if publisher fails

        # Sort by dataset count
        popular_publishers.sort(key=lambda x: x["dataset_count"], reverse=True)
        popular_publishers = popular_publishers[:limit]

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
        params = {'rows': limit, 'sort': 'modified desc', 'type': 'dataset'}
        result = _make_request("catalog", params)

        if result["error"]:
            return result

        # Process search results
        search_data = result.get("data", {})
        if isinstance(search_data, dict):
            datasets = search_data.get("results") or search_data.get("items") or search_data.get("datasets", [])
        elif isinstance(search_data, list):
            datasets = search_data
        else:
            datasets = []

        # Enhance dataset data
        enhanced_data = []
        for dataset in datasets:
            if isinstance(dataset, dict):
                enhanced_dataset = {
                    "id": dataset.get("id") or dataset.get("uri") or dataset.get("identifier", ""),
                    "name": dataset.get("name") or dataset.get("title", ""),
                    "title": dataset.get("title") or dataset.get("name", ""),
                    "description": dataset.get("description") or dataset.get("notes", ""),
                    "publisher": dataset.get("publisher") or dataset.get("organization", {}).get("name") if dataset.get("organization") else "",
                    "modified": dataset.get("modified") or dataset.get("updated", ""),
                    "num_resources": len(dataset.get("distribution", [])) or len(dataset.get("resources", [])),
                    "theme": dataset.get("theme") or dataset.get("category", []),
                    "spatial": dataset.get("spatial") or dataset.get("coverage", [])
                }
            else:
                enhanced_dataset = {
                    "id": str(dataset),
                    "name": str(dataset),
                    "title": str(dataset),
                    "description": "",
                    "publisher": "",
                    "modified": "",
                    "num_resources": 0,
                    "theme": [],
                    "spatial": []
                }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["total_count"] = len(enhanced_data)
        result["metadata"]["returned_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching recent datasets: {str(e)}"
        }

def get_taxonomy_categories() -> Dict[str, Any]:
    """
    Get the primary sector taxonomy categories

    Returns:
        JSON response with taxonomy categories
    """
    try:
        result = _make_request("taxonomy")

        if result["error"]:
            return result

        # Process taxonomy data
        taxonomy_data = result.get("data", {})
        if isinstance(taxonomy_data, dict):
            categories = taxonomy_data.get("categories") or taxonomy_data.get("results") or taxonomy_data.get("items", [])
        elif isinstance(taxonomy_data, list):
            categories = taxonomy_data
        else:
            categories = []

        # Enhance category data
        enhanced_data = []
        for category in categories:
            if isinstance(category, dict):
                enhanced_category = {
                    "id": category.get("id") or category.get("uri") or category.get("identifier", ""),
                    "name": category.get("name") or category.get("label") or category.get("title", ""),
                    "title": category.get("title") or category.get("name", ""),
                    "description": category.get("description") or category.get("notes", ""),
                    "uri": category.get("uri") or category.get("url", ""),
                    "parent": category.get("parent") or category.get("broader", ""),
                    "children": category.get("children") or category.get("narrower", []),
                    "level": category.get("level") or category.get("depth", 0)
                }
            else:
                enhanced_category = {
                    "id": str(category),
                    "name": str(category),
                    "title": str(category),
                    "description": "",
                    "uri": "",
                    "parent": "",
                    "children": [],
                    "level": 0
                }
            enhanced_data.append(enhanced_category)

        result["data"] = enhanced_data
        result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching taxonomy categories: {str(e)}"
        }

def get_geographical_coverage() -> Dict[str, Any]:
    """
    Get geographical coverage as defined in NTI regulations

    Returns:
        JSON response with geographical coverage information
    """
    try:
        result = _make_request("coverage")

        if result["error"]:
            return result

        # Process coverage data
        coverage_data = result.get("data", {})
        if isinstance(coverage_data, dict):
            geographical = coverage_data.get("geographical") or coverage_data.get("spatial") or coverage_data.get("results", [])
        elif isinstance(coverage_data, list):
            geographical = coverage_data
        else:
            geographical = []

        # Enhance geographical data
        enhanced_data = []
        for geo in geographical:
            if isinstance(geo, dict):
                enhanced_geo = {
                    "id": geo.get("id") or geo.get("identifier", ""),
                    "name": geo.get("name") or geo.get("label") or geo.get("title", ""),
                    "title": geo.get("title") or geo.get("name", ""),
                    "description": geo.get("description") or geo.get("notes", ""),
                    "code": geo.get("code") or geo.get("iso", ""),
                    "level": geo.get("level") or geo.get("administrative_level", ""),
                    "parent": geo.get("parent") or geo.get("part_of", ""),
                    "uri": geo.get("uri") or geo.get("url", ""),
                    "annex": geo.get("annex") or geo.get("regulation_reference", "")
                }
            else:
                enhanced_geo = {
                    "id": str(geo),
                    "name": str(geo),
                    "title": str(geo),
                    "description": "",
                    "code": "",
                    "level": "",
                    "parent": "",
                    "uri": "",
                    "annex": ""
                }
            enhanced_data.append(enhanced_geo)

        result["data"] = enhanced_data
        result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching geographical coverage: {str(e)}"
        }

# --- 3. CLI INTERFACE ---
def main():
    """CLI interface for datos.gob.es API"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python datos_gob_es_api.py <command> <args>",
            "available_commands": [
                "publishers",
                "publisher-details <publisher_id>",
                "datasets <publisher_id> [rows]",
                "search <query> [rows]",
                "dataset-details <dataset_id>",
                "resources <dataset_id>",
                "resource-info <resource_id>",
                "series-data <resource_id> [format]",
                "preview <resource_url>",
                "popular-publishers [limit]",
                "recent-datasets [limit]",
                "taxonomy",
                "coverage"
            ],
            "examples": [
                "python datos_gob_es_api.py publishers",
                "python datos_gob_es_api.py publisher-details ministerio-de-hacienda",
                "python datos_gob_es_api.py datasets ministerio-de-hacienda 50",
                "python datos_gob_es_api.py search 'economia' 25",
                "python datos_gob_es_api.py dataset-presupuestos-generales-estado",
                "python datos_gob_es_api.py resources presupuesto-general-2023",
                "python datos_gob_es_api.py series-data resource-id json",
                "python datos_gob_es_api.py preview https://datos.gob.es/data.csv",
                "python datos_gob_es_api.py taxonomy",
                "python datos_gob_es_api.py coverage"
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

        elif command == "series-data":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: series-data <resource_id> [format]"}))
                sys.exit(1)
            resource_id = sys.argv[2]
            format_type = sys.argv[3] if len(sys.argv) > 3 else "json"
            result = get_series_data(resource_id, format_type)

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

        elif command == "taxonomy":
            result = get_taxonomy_categories()

        elif command == "coverage":
            result = get_geographical_coverage()

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
                    "series-data <resource_id> [format]",
                    "preview <resource_url>",
                    "popular-publishers [limit]",
                    "recent-datasets [limit]",
                    "taxonomy",
                    "coverage"
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