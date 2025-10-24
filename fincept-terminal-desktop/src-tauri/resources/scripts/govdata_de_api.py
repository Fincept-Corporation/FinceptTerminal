"""
German Government Open Data Portal (GovData.de) API Wrapper
Fetches German government data using CKAN-based hierarchical structure:
1. Organizations - Lists all data providers/publishers (organizations)
2. Datasets - Lists datasets within each organization
3. Resources - Gets actual data files (CSV/XLS) for datasets

API Documentation: https://docs.ckan.org/en/3.0/api/index.html
Portal URL: https://www.govdata.de/

Usage: python govdata_de_api.py <command> [args]
"""

import sys
import json
import os
import requests
from typing import Dict, List, Optional, Any
from datetime import datetime
import urllib.parse

# --- 1. CONFIGURATION ---
BASE_URL = "https://www.govdata.de/ckan/api/3/action"
API_KEY = os.environ.get('GOVDATA_API_KEY')  # Optional - GovData.de has public access
TIMEOUT = 30
RATE_LIMIT_DELAY = 1.0  # Be respectful to the public API

def _make_request(action: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for GovData.de CKAN API

    Args:
        action: CKAN action name (e.g., 'organization_list', 'package_search')
        params: Query parameters for the request

    Returns:
        Dict with consistent structure: {"data": [...], "metadata": {...}, "error": None/error_msg}
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

        # Add API key to header if available (CKAN uses Authorization header)
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
                "metadata": {
                    "source": "GovData.de",
                    "action": action,
                    "parameters": params,
                    "http_status": response.status_code
                },
                "error": f"CKAN API Error: {error_msg}"
            }

        return {
            "data": data.get('result', []),
            "metadata": {
                "source": "GovData.de",
                "action": action,
                "parameters": params,
                "last_updated": datetime.now().isoformat(),
                "url": response.url
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {
            "data": [],
            "metadata": {
                "source": "GovData.de",
                "action": action,
                "parameters": params,
                "http_status": e.response.status_code
            },
            "error": f"HTTP Error: {e.response.status_code} - {e.response.text}"
        }
    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {
                "source": "GovData.de",
                "action": action,
                "parameters": params
            },
            "error": "Request timeout. The GovData.de API is taking too long to respond."
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {
                "source": "GovData.de",
                "action": action,
                "parameters": params
            },
            "error": "Connection error. Could not connect to GovData.de API."
        }
    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {
                "source": "GovData.de",
                "action": action,
                "parameters": params
            },
            "error": f"Network or request error: {str(e)}"
        }
    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {
                "source": "GovData.de",
                "action": action,
                "parameters": params
            },
            "error": "Invalid JSON response from GovData.de API."
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "source": "GovData.de",
                "action": action,
                "parameters": params
            },
            "error": f"An unexpected error occurred: {str(e)}"
        }

# --- 2. CORE FUNCTIONS (GROUPED BY API CATEGORY) ---

# ====== ORGANIZATIONS (PROVIDERS/PUBLISHERS) ======
def get_organizations() -> Dict[str, Any]:
    """
    Get list of all organizations (data providers) in GovData.de

    Returns:
        JSON response with organization list
    """
    try:
        result = _make_request("organization_list")

        if result["error"]:
            return result

        # Enhance organization data
        enhanced_data = []
        organizations = result.get("data", [])

        for org_id in organizations:
            enhanced_org = {
                "id": org_id,
                "name": org_id.replace("-", " ").replace("_", " ").title(),
                "display_name": org_id.replace("-", " ").replace("_", " ").title()
            }
            enhanced_data.append(enhanced_org)

        result["data"] = enhanced_data
        result["metadata"]["count"] = len(enhanced_data)
        result["metadata"]["description"] = "All organizations in GovData.de"

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
        organization_id: The unique ID of the organization

    Returns:
        JSON response with organization details
    """
    try:
        params = {'id': organization_id}
        result = _make_request("organization_show", params)

        if result["error"]:
            return result

        # Enhance organization data
        org_data = result.get("data", {})
        enhanced_org = {
            "id": org_data.get("id"),
            "name": org_data.get("name"),
            "title": org_data.get("title"),
            "description": org_data.get("description"),
            "image_url": org_data.get("image_display_url"),
            "created": org_data.get("created"),
            "num_datasets": org_data.get("package_count", 0),
            "users": org_data.get("users", [])
        }

        result["data"] = enhanced_org
        result["metadata"]["organization_id"] = organization_id
        result["metadata"]["description"] = f"Details for organization {organization_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching organization details: {str(e)}"
        }

# ====== DATASETS ======
def get_datasets_by_organization(organization_id: str, rows: int = 100) -> Dict[str, Any]:
    """
    Get all datasets published by a specific organization

    Args:
        organization_id: The unique ID of the organization
        rows: Number of datasets to return (default: 100)

    Returns:
        JSON response with dataset list
    """
    try:
        # Search for datasets by owner_org (organization filter)
        query = f"owner_org:{organization_id}"
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
                "organization_id": organization_id,
                "metadata_created": dataset.get("metadata_created"),
                "metadata_modified": dataset.get("metadata_modified"),
                "state": dataset.get("state"),
                "num_resources": len(dataset.get("resources", [])),
                "tags": [tag.get("display_name") for tag in dataset.get("tags", [])]
            }
            enhanced_data.append(enhanced_dataset)

        result["data"] = enhanced_data
        result["metadata"]["organization_id"] = organization_id
        result["metadata"]["total_count"] = search_data.get("count", 0)
        result["metadata"]["returned_count"] = len(enhanced_data)
        result["metadata"]["description"] = f"Datasets for organization {organization_id}"

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching datasets: {str(e)}"
        }

def get_dataset_details(provider_id: str, dataset_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific dataset

    Args:
        provider_id: The unique ID of the provider
        dataset_id: The unique ID of the dataset

    Returns:
        JSON response with dataset details
    """
    try:
        endpoint = f"catalogue/{provider_id}/datasets/{dataset_id}"  # Replace with actual endpoint pattern
        result = _make_request(endpoint)

        if result["error"]:
            return result

        # Enhance dataset data
        dataset_data = result.get("data", {})
        enhanced_dataset = {
            "id": dataset_data.get("id") or dataset_data.get("dataset_id"),
            "name": dataset_data.get("name") or dataset_data.get("title"),
            "description": dataset_data.get("description", ""),
            "provider_id": provider_id,
            "series_count": dataset_data.get("series_count", len(dataset_data.get("series", []))),
            "created": dataset_data.get("created"),
            "updated": dataset_data.get("updated"),
            "frequency": dataset_data.get("frequency"),
            "temporal_coverage": dataset_data.get("temporal_coverage"),
            "geographic_coverage": dataset_data.get("geographic_coverage"),
            "unit_of_measure": dataset_data.get("unit_of_measure"),
            "methodology": dataset_data.get("methodology"),
            "tags": dataset_data.get("tags", []),
            "metadata": dataset_data.get("metadata", {})
        }

        result["data"] = enhanced_dataset
        result["metadata"]["provider_id"] = provider_id
        result["metadata"]["dataset_id"] = dataset_id
        result["metadata"]["description"] = f"Details for dataset {dataset_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching dataset details: {str(e)}"
        }

# ====== SERIES ======
def get_series_by_dataset(provider_id: str, dataset_id: str, limit: int = 100) -> Dict[str, Any]:
    """
    Get all data series within a specific dataset

    Args:
        provider_id: The unique ID of the provider
        dataset_id: The unique ID of the dataset
        limit: Maximum number of series to return

    Returns:
        JSON response with series list
    """
    try:
        endpoint = f"catalogue/{provider_id}/datasets/{dataset_id}/series"  # Replace with actual endpoint pattern
        params = {'limit': limit}
        result = _make_request(endpoint, params)

        if result["error"]:
            return result

        # Enhance series data
        enhanced_data = []
        series_list = result.get("data", [])

        # Handle different response formats
        if isinstance(series_list, dict):
            series_list = series_list.get("series", [])

        for series in series_list:
            enhanced_series = {
                "id": series.get("id") or series.get("series_id"),
                "name": series.get("name") or series.get("title"),
                "description": series.get("description", ""),
                "provider_id": provider_id,
                "dataset_id": dataset_id,
                "frequency": series.get("frequency"),
                "unit": series.get("unit"),
                "start_date": series.get("start_date"),
                "end_date": series.get("end_date"),
                "data_points": series.get("data_points", 0),
                "last_updated": series.get("last_updated"),
                "metadata": series.get("metadata", {})
            }
            enhanced_data.append(enhanced_series)

        result["data"] = enhanced_data
        result["metadata"]["provider_id"] = provider_id
        result["metadata"]["dataset_id"] = dataset_id
        result["metadata"]["total_count"] = len(enhanced_data)
        result["metadata"]["description"] = f"Series for dataset {dataset_id}"

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching series: {str(e)}"
        }

def get_series_data(provider_id: str, dataset_id: str, series_id: str,
                   start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
    """
    Get actual data points for a specific series

    Args:
        provider_id: The unique ID of the provider
        dataset_id: The unique ID of the dataset
        series_id: The unique ID of the series
        start_date: Start date for data (optional)
        end_date: End date for data (optional)

    Returns:
        JSON response with time series data
    """
    try:
        endpoint = f"catalogue/{provider_id}/datasets/{dataset_id}/series/{series_id}/data"  # Replace with actual endpoint pattern

        params = {}
        if start_date:
            params['start_date'] = start_date
        if end_date:
            params['end_date'] = end_date

        result = _make_request(endpoint, params)

        if result["error"]:
            return result

        # Enhance series data
        series_data = result.get("data", {})

        # Handle different data formats
        data_points = []
        if isinstance(series_data, dict):
            if "data" in series_data:
                data_points = series_data["data"]
            elif "observations" in series_data:
                data_points = series_data["observations"]
            elif "values" in series_data:
                data_points = series_data["values"]
        elif isinstance(series_data, list):
            data_points = series_data

        enhanced_series = {
            "series_id": series_id,
            "provider_id": provider_id,
            "dataset_id": dataset_id,
            "data_points": data_points,
            "count": len(data_points) if isinstance(data_points, list) else 0,
            "start_date": start_date,
            "end_date": end_date,
            "metadata": series_data.get("metadata", {}) if isinstance(series_data, dict) else {}
        }

        result["data"] = enhanced_series
        result["metadata"]["provider_id"] = provider_id
        result["metadata"]["dataset_id"] = dataset_id
        result["metadata"]["series_id"] = series_id
        result["metadata"]["description"] = f"Data for series {series_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching series data: {str(e)}"
        }

# ====== UTILITY FUNCTIONS ======
def search_catalogue(query: str, limit: int = 50) -> Dict[str, Any]:
    """
    Search across all providers, datasets, and series

    Args:
        query: Search query string
        limit: Maximum number of results to return

    Returns:
        JSON response with search results
    """
    try:
        endpoint = "search"  # Replace with actual search endpoint
        params = {'q': query, 'limit': limit}
        result = _make_request(endpoint, params)

        if result["error"]:
            return result

        # Enhance search results
        search_data = result.get("data", {})
        enhanced_results = []

        # Handle different search response formats
        if isinstance(search_data, dict):
            results = search_data.get("results", search_data.get("items", []))
        else:
            results = search_data if isinstance(search_data, list) else []

        for item in results:
            enhanced_item = {
                "id": item.get("id"),
                "name": item.get("name") or item.get("title"),
                "description": item.get("description", ""),
                "type": item.get("type", "unknown"),  # provider, dataset, or series
                "provider_id": item.get("provider_id"),
                "dataset_id": item.get("dataset_id"),
                "score": item.get("score", 0),
                "metadata": item.get("metadata", {})
            }
            enhanced_results.append(enhanced_item)

        result["data"] = enhanced_results
        result["metadata"]["query"] = query
        result["metadata"]["total_count"] = len(enhanced_results)
        result["metadata"]["description"] = f"Search results for '{query}'"

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching catalogue: {str(e)}"
        }

def get_provider_summary(provider_id: str) -> Dict[str, Any]:
    """
    Get a summary of provider with datasets and series counts

    Args:
        provider_id: The unique ID of the provider

    Returns:
        JSON response with provider summary
    """
    try:
        # Get provider details
        provider_result = get_provider_details(provider_id)
        if provider_result["error"]:
            return provider_result

        # Get datasets (limited for performance)
        datasets_result = get_datasets_by_provider(provider_id, limit=50)

        total_datasets = 0
        total_series = 0

        if not datasets_result["error"]:
            datasets = datasets_result.get("data", [])
            total_datasets = len(datasets)

            # Count series for each dataset (sample)
            for dataset in datasets[:10]:  # Sample first 10 datasets
                dataset_id = dataset.get("id")
                if dataset_id:
                    series_result = get_series_by_dataset(provider_id, dataset_id, limit=1)
                    if not series_result["error"]:
                        series_metadata = series_result.get("metadata", {})
                        total_series += series_metadata.get("total_count", 0)

        summary = {
            "provider": provider_result.get("data", {}),
            "total_datasets": total_datasets,
            "sample_series_count": total_series,
            "last_updated": datetime.now().isoformat()
        }

        return {
            "data": summary,
            "metadata": {
                "provider_id": provider_id,
                "description": f"Summary for provider {provider_id}"
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error generating provider summary: {str(e)}"
        }

# --- 3. CLI INTERFACE ---
def main():
    """Main CLI entry point for New Provider API Wrapper"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python new_provider_api.py <command> <args>",
            "available_commands": [
                "catalogue",
                "provider-details <provider_id>",
                "datasets <provider_id> [limit]",
                "dataset-details <provider_id> <dataset_id>",
                "series <provider_id> <dataset_id> [limit]",
                "series-data <provider_id> <dataset_id> <series_id> [start_date] [end_date]",
                "search <query> [limit]",
                "provider-summary <provider_id>"
            ],
            "examples": [
                "python new_provider_api.py catalogue",
                "python new_provider_api.py provider-details world-bank",
                "python new_provider_api.py datasets world-bank 50",
                "python new_provider_api.py dataset-details world-bank gdp-data",
                "python new_provider_api.py series world-bank gdp-data 100",
                "python new_provider_api.py series-data world-bank gdp-data gdp-series 2020-01-01 2023-12-31",
                "python new_provider_api.py search inflation 20",
                "python new_provider_api.py provider-summary world-bank"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "catalogue":
            result = get_catalogue()

        elif command == "provider-details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: provider-details <provider_id>"}))
                sys.exit(1)
            provider_id = sys.argv[2]
            result = get_provider_details(provider_id)

        elif command == "datasets":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: datasets <provider_id> [limit]"}))
                sys.exit(1)
            provider_id = sys.argv[2]
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = get_datasets_by_provider(provider_id, limit)

        elif command == "dataset-details":
            if len(sys.argv) < 4:
                print(json.dumps({"error": "Usage: dataset-details <provider_id> <dataset_id>"}))
                sys.exit(1)
            provider_id = sys.argv[2]
            dataset_id = sys.argv[3]
            result = get_dataset_details(provider_id, dataset_id)

        elif command == "series":
            if len(sys.argv) < 4:
                print(json.dumps({"error": "Usage: series <provider_id> <dataset_id> [limit]"}))
                sys.exit(1)
            provider_id = sys.argv[2]
            dataset_id = sys.argv[3]
            limit = int(sys.argv[4]) if len(sys.argv) > 4 else 100
            result = get_series_by_dataset(provider_id, dataset_id, limit)

        elif command == "series-data":
            if len(sys.argv) < 5:
                print(json.dumps({"error": "Usage: series-data <provider_id> <dataset_id> <series_id> [start_date] [end_date]"}))
                sys.exit(1)
            provider_id = sys.argv[2]
            dataset_id = sys.argv[3]
            series_id = sys.argv[4]
            start_date = sys.argv[5] if len(sys.argv) > 5 else None
            end_date = sys.argv[6] if len(sys.argv) > 6 else None
            result = get_series_data(provider_id, dataset_id, series_id, start_date, end_date)

        elif command == "search":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: search <query> [limit]"}))
                sys.exit(1)
            query = sys.argv[2]
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 50
            result = search_catalogue(query, limit)

        elif command == "provider-summary":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: provider-summary <provider_id>"}))
                sys.exit(1)
            provider_id = sys.argv[2]
            result = get_provider_summary(provider_id)

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "catalogue",
                    "provider-details <provider_id>",
                    "datasets <provider_id> [limit]",
                    "dataset-details <provider_id> <dataset_id>",
                    "series <provider_id> <dataset_id> [limit]",
                    "series-data <provider_id> <dataset_id> <series_id> [start_date] [end_date]",
                    "search <query> [limit]",
                    "provider-summary <provider_id>"
                ]
            }))
            sys.exit(1)

        # Output result as JSON
        print(json.dumps(result, indent=2, ensure_ascii=False))

    except Exception as e:
        print(json.dumps({
            "error": f"Command execution failed: {str(e)}",
            "command": command,
            "timestamp": datetime.now().isoformat()
        }))
        sys.exit(1)

if __name__ == "__main__":
    main()