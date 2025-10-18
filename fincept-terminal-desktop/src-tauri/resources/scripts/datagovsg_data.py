#!/usr/bin/env python3
"""
Data.gov.sg API Wrapper
Provides access to Singapore's open data portal including real-time APIs and catalog APIs.

Catalog Structure:
- Collections (catalogues) -> Datasets -> Series/Data
"""

import sys
import json
import os
import requests
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any, Union

# Configuration
API_KEY = os.environ.get('DATAGOVSG_API_KEY')
COLLECTION_API_BASE = "https://api-production.data.gov.sg/v2/public/api"
REALTIME_API_BASE = "https://api-open.data.gov.sg/v2/real-time/api"
DATASET_API_BASE = "https://api-production.data.gov.sg/v1/public/api/datasets"

# Request settings
TIMEOUT = 30
DEFAULT_FORMAT = "json"

# Headers for API requests
def get_headers() -> Dict[str, str]:
    """Get standard headers for API requests."""
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json"
    }
    if API_KEY:
        headers["x-api-key"] = API_KEY
    return headers

# ============================================================================
# COLLECTION/CATALOGUE APIS (Catalogue Level)
# ============================================================================

def get_collections(page: int = 1) -> Dict[str, Any]:
    """
    Get all available collections (catalogues) from data.gov.sg.

    Args:
        page: Page number for pagination (default: 1)

    Returns:
        Dict with 'data', 'metadata', and 'error' keys
    """
    try:
        url = f"{COLLECTION_API_BASE}/collections"
        params = {"page": page}
        headers = get_headers()

        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        raw_data = response.json()

        enhanced_data = []
        if isinstance(raw_data, dict) and 'data' in raw_data and 'collections' in raw_data['data']:
            for collection in raw_data['data']['collections']:
                enhanced_collection = {
                    "id": collection.get("collectionId"),
                    "name": collection.get("name"),
                    "description": collection.get("description"),
                    "frequency": collection.get("frequency"),
                    "sources": collection.get("sources", []),
                    "managed_by": collection.get("managedByAgencyName"),
                    "created_at": collection.get("createdAt"),
                    "updated_at": collection.get("lastUpdatedAt")
                }
                enhanced_data.append(enhanced_collection)

        metadata = {
            "page": page,
            "total_count": len(enhanced_data),
            "api_used": "collections",
            "timestamp": datetime.utcnow().isoformat()
        }

        return {
            "data": enhanced_data,
            "metadata": metadata,
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"HTTP error fetching collections: {str(e)}"
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {},
            "error": "Connection error: Could not connect to data.gov.sg API"
        }
    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {},
            "error": "Request timeout: Could not fetch collections within time limit"
        }
    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {},
            "error": "Invalid JSON response from collections API"
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Unexpected error fetching collections: {str(e)}"
        }

def get_collection_details(collection_id: Union[str, int], with_dataset_metadata: bool = False) -> Dict[str, Any]:
    """
    Get detailed information about a specific collection including its datasets.

    Args:
        collection_id: The ID of the collection
        with_dataset_metadata: Include full metadata for datasets (default: False)

    Returns:
        Dict with 'data', 'metadata', and 'error' keys
    """
    try:
        url = f"{COLLECTION_API_BASE}/collections/{collection_id}"
        params = {"withDatasetMetadata": str(with_dataset_metadata).lower()}
        headers = get_headers()

        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        raw_data = response.json()

        collection_data = {}
        datasets_data = []

        if isinstance(raw_data, dict) and 'data' in raw_data:
            collection_info = raw_data['data']
            collection_data = {
                "id": collection_info.get("collectionId"),
                "name": collection_info.get("name"),
                "description": collection_info.get("description"),
                "frequency": collection_info.get("frequency"),
                "sources": collection_info.get("sources", []),
                "managed_by": collection_info.get("managedByAgencyName"),
                "created_at": collection_info.get("createdAt"),
                "updated_at": collection_info.get("lastUpdatedAt")
            }

            if 'datasets' in collection_info:
                for dataset in collection_info['datasets']:
                    enhanced_dataset = {
                        "id": dataset.get("datasetId"),
                        "name": dataset.get("name"),
                        "description": dataset.get("description"),
                        "created_at": dataset.get("createdAt"),
                        "updated_at": dataset.get("lastUpdatedAt"),
                        "metadata": dataset.get("metadata", {})
                    }
                    datasets_data.append(enhanced_dataset)

        result_data = {
            "collection": collection_data,
            "datasets": datasets_data
        }

        metadata = {
            "collection_id": collection_id,
            "dataset_count": len(datasets_data),
            "with_dataset_metadata": with_dataset_metadata,
            "api_used": "collection_details",
            "timestamp": datetime.utcnow().isoformat()
        }

        return {
            "data": result_data,
            "metadata": metadata,
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        error_msg = f"HTTP error fetching collection details: {str(e)}"
        if e.response.status_code == 404:
            error_msg = f"Collection with ID {collection_id} not found"
        return {
            "data": {},
            "metadata": {},
            "error": error_msg
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": {},
            "metadata": {},
            "error": "Connection error: Could not connect to data.gov.sg API"
        }
    except requests.exceptions.Timeout:
        return {
            "data": {},
            "metadata": {},
            "error": "Request timeout: Could not fetch collection details within time limit"
        }
    except json.JSONDecodeError:
        return {
            "data": {},
            "metadata": {},
            "error": "Invalid JSON response from collection details API"
        }
    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Unexpected error fetching collection details: {str(e)}"
        }

# ============================================================================
# DATASET APIS (Dataset Level)
# ============================================================================

def initiate_dataset_download(dataset_id: str, column_names: Optional[List[str]] = None,
                            filters: Optional[List[Dict[str, Any]]] = None) -> Dict[str, Any]:
    """
    Initiate a download for a specific dataset with optional filtering.

    Args:
        dataset_id: The ID of the dataset
        column_names: Optional list of column names to include
        filters: Optional list of filter objects

    Returns:
        Dict with 'data', 'metadata', and 'error' keys containing download initiation info
    """
    try:
        url = f"{DATASET_API_BASE}/{dataset_id}/initiate-download"
        headers = get_headers()

        request_body = {}
        if column_names:
            request_body["columnNames"] = column_names
        if filters:
            request_body["filters"] = {"filters": filters}

        response = requests.get(url, json=request_body if request_body else None,
                              headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        raw_data = response.json()

        # The response should contain request ID for polling
        enhanced_data = {
            "dataset_id": dataset_id,
            "request_id": raw_data.get("requestId"),
            "status": raw_data.get("status", "INITIATED"),
            "message": raw_data.get("message"),
            "timestamp": datetime.utcnow().isoformat()
        }

        metadata = {
            "dataset_id": dataset_id,
            "column_names": column_names,
            "filters_count": len(filters) if filters else 0,
            "api_used": "initiate_download",
            "timestamp": datetime.utcnow().isoformat()
        }

        return {
            "data": enhanced_data,
            "metadata": metadata,
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        error_msg = f"HTTP error initiating dataset download: {str(e)}"
        if e.response.status_code == 404:
            error_msg = f"Dataset with ID {dataset_id} not found"
        return {
            "data": {},
            "metadata": {},
            "error": error_msg
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": {},
            "metadata": {},
            "error": "Connection error: Could not connect to data.gov.sg API"
        }
    except requests.exceptions.Timeout:
        return {
            "data": {},
            "metadata": {},
            "error": "Request timeout: Could not initiate dataset download within time limit"
        }
    except json.JSONDecodeError:
        return {
            "data": {},
            "metadata": {},
            "error": "Invalid JSON response from dataset download API"
        }
    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Unexpected error initiating dataset download: {str(e)}"
        }

def poll_dataset_download(dataset_id: str) -> Dict[str, Any]:
    """
    Poll the status of a dataset download and get download URL when ready.

    Args:
        dataset_id: The ID of the dataset

    Returns:
        Dict with 'data', 'metadata', and 'error' keys containing download status and URL
    """
    try:
        url = f"{DATASET_API_BASE}/{dataset_id}/poll-download"
        headers = get_headers()

        response = requests.get(url, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        raw_data = response.json()

        enhanced_data = {
            "dataset_id": dataset_id,
            "status": raw_data.get("data", {}).get("status"),
            "download_url": raw_data.get("data", {}).get("url"),
            "error_message": raw_data.get("errorMsg"),
            "code": raw_data.get("code"),
            "timestamp": datetime.utcnow().isoformat()
        }

        metadata = {
            "dataset_id": dataset_id,
            "api_used": "poll_download",
            "timestamp": datetime.utcnow().isoformat()
        }

        return {
            "data": enhanced_data,
            "metadata": metadata,
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"HTTP error polling dataset download: {str(e)}"
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": {},
            "metadata": {},
            "error": "Connection error: Could not connect to data.gov.sg API"
        }
    except requests.exceptions.Timeout:
        return {
            "data": {},
            "metadata": {},
            "error": "Request timeout: Could not poll dataset download within time limit"
        }
    except json.JSONDecodeError:
        return {
            "data": {},
            "metadata": {},
            "error": "Invalid JSON response from dataset poll API"
        }
    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Unexpected error polling dataset download: {str(e)}"
        }

# ============================================================================
# REAL-TIME APIS
# ============================================================================

def get_pm25_readings(date: Optional[str] = None, pagination_token: Optional[str] = None) -> Dict[str, Any]:
    """
    Get PM2.5 readings from Singapore's air quality monitoring stations.

    Args:
        date: Optional date filter (YYYY-MM-DD or YYYY-MM-DDTHH:mm:ss)
        pagination_token: Optional token for pagination

    Returns:
        Dict with 'data', 'metadata', and 'error' keys
    """
    try:
        url = f"{REALTIME_API_BASE}/pm25"
        params = {}
        if date:
            params["date"] = date
        if pagination_token:
            params["paginationToken"] = pagination_token

        headers = get_headers()

        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        raw_data = response.json()

        enhanced_data = {
            "region_metadata": [],
            "readings": [],
            "timestamp": datetime.utcnow().isoformat()
        }

        if isinstance(raw_data, dict) and 'data' in raw_data:
            data = raw_data['data']

            if 'regionMetadata' in data:
                for region in data['regionMetadata']:
                    enhanced_region = {
                        "name": region.get("name"),
                        "label_location": region.get("labelLocation"),
                        "latitude": region.get("labelLocation", {}).get("latitude") if isinstance(region.get("labelLocation"), dict) else None,
                        "longitude": region.get("labelLocation", {}).get("longitude") if isinstance(region.get("labelLocation"), dict) else None
                    }
                    enhanced_data["region_metadata"].append(enhanced_region)

            if 'items' in data and data['items']:
                for item in data['items']:
                    reading_data = {
                        "timestamp": item.get("timestamp"),
                        "date": item.get("date"),
                        "readings": item.get("readings", {}),
                        "update_timestamp": item.get("updatedTimestamp")
                    }
                    enhanced_data["readings"].append(reading_data)

        metadata = {
            "api_used": "pm25_realtime",
            "date_filter": date,
            "has_pagination_token": bool(pagination_token),
            "region_count": len(enhanced_data["region_metadata"]),
            "reading_count": len(enhanced_data["readings"]),
            "timestamp": datetime.utcnow().isoformat()
        }

        return {
            "data": enhanced_data,
            "metadata": metadata,
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"HTTP error fetching PM2.5 readings: {str(e)}"
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": {},
            "metadata": {},
            "error": "Connection error: Could not connect to data.gov.sg API"
        }
    except requests.exceptions.Timeout:
        return {
            "data": {},
            "metadata": {},
            "error": "Request timeout: Could not fetch PM2.5 readings within time limit"
        }
    except json.JSONDecodeError:
        return {
            "data": {},
            "metadata": {},
            "error": "Invalid JSON response from PM2.5 API"
        }
    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Unexpected error fetching PM2.5 readings: {str(e)}"
        }

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def test_api_connectivity() -> Dict[str, Any]:
    """
    Test basic connectivity to all API endpoints.

    Returns:
        Dict with connectivity test results for each endpoint
    """
    results = {}

    # Test Collections API
    try:
        collections_response = get_collections(page=1)
        results["collections_api"] = {
            "status": "connected" if not collections_response["error"] else "error",
            "message": collections_response["error"] or "Successfully connected",
            "response_time_ms": 0  # Could be enhanced with timing
        }
    except Exception as e:
        results["collections_api"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    # Test Real-time API
    try:
        pm25_response = get_pm25_readings()
        results["realtime_api"] = {
            "status": "connected" if not pm25_response["error"] else "error",
            "message": pm25_response["error"] or "Successfully connected",
            "response_time_ms": 0
        }
    except Exception as e:
        results["realtime_api"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    return {
        "data": results,
        "metadata": {
            "test_timestamp": datetime.utcnow().isoformat(),
            "api_key_configured": bool(API_KEY)
        },
        "error": None
    }

# ============================================================================
# CLI INTERFACE
# ============================================================================

def main():
    """Command-line interface for data.gov.sg API wrapper."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python datagovsg_data.py <command> [args]",
            "available_commands": [
                "collections [page]",
                "collection-details <collection_id> [with_metadata]",
                "initiate-download <dataset_id>",
                "poll-download <dataset_id>",
                "pm25 [date]",
                "test-connectivity"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "collections":
            page = int(sys.argv[2]) if len(sys.argv) > 2 else 1
            result = get_collections(page=page)

        elif command == "collection-details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: datagovsg_data.py collection-details <collection_id> [with_metadata]"}))
                sys.exit(1)
            collection_id = sys.argv[2]
            with_metadata = sys.argv[3].lower() == "true" if len(sys.argv) > 3 else False
            result = get_collection_details(collection_id, with_dataset_metadata=with_metadata)

        elif command == "initiate-download":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: datagovsg_data.py initiate-download <dataset_id>"}))
                sys.exit(1)
            dataset_id = sys.argv[2]
            result = initiate_dataset_download(dataset_id)

        elif command == "poll-download":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: datagovsg_data.py poll-download <dataset_id>"}))
                sys.exit(1)
            dataset_id = sys.argv[2]
            result = poll_dataset_download(dataset_id)

        elif command == "pm25":
            date = sys.argv[2] if len(sys.argv) > 2 else None
            result = get_pm25_readings(date=date)

        elif command == "test-connectivity":
            result = test_api_connectivity()

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "collections [page]",
                    "collection-details <collection_id> [with_metadata]",
                    "initiate-download <dataset_id>",
                    "poll-download <dataset_id>",
                    "pm25 [date]",
                    "test-connectivity"
                ]
            }

        print(json.dumps(result, indent=2))

    except ValueError as e:
        print(json.dumps({"error": f"Invalid parameter: {str(e)}"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"error": f"Command execution failed: {str(e)}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()