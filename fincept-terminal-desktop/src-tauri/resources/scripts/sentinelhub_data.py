#!/usr/bin/env python3
"""
Sentinel Hub Data Fetcher
Access to satellite imagery for financial analysis and alternative data
Returns JSON output for Rust integration

API Documentation:
- Catalog API: Search for available satellite imagery
- Process API: Download processed satellite images
- Authentication: OAuth2 with Client ID and Client Secret
- Base URL: https://services.sentinel-hub.com/
"""

import sys
import json
import os
import requests
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import urllib.parse
import base64
import tempfile

# Configuration
BASE_URL = "https://services.sentinel-hub.com"
CATALOG_API_URL = f"{BASE_URL}/api/v1/catalog/1.0.0/search"
PROCESS_API_URL = f"{BASE_URL}/api/v1/process"
TOKEN_URL = f"{BASE_URL}/oauth/token"
TIMEOUT = 60  # Longer timeout for image processing

# Sentinel Hub Collections
SENTINEL_2_L2A = "sentinel-2-l2a"
SENTINEL_1_GRD = "sentinel-1-grd"
LANDSAT_8_L1 = "landsat-8-l1"
MODIS = "modis"

# Common evalscripts for different analyses
EVALSCRIPTS = {
    "true_color": """
//VERSION=3
function setup() {
    return {
        input: ["B02", "B03", "B04"],
        output: { bands: 3 }
    };
}
function evaluatePixel(sample) {
    return [2.5 * sample.B04, 2.5 * sample.B03, 2.5 * sample.B02];
}
""",
    "false_color": """
//VERSION=3
function setup() {
    return {
        input: ["B08", "B04", "B03"],
        output: { bands: 3 }
    };
}
function evaluatePixel(sample) {
    return [2.5 * sample.B08, 2.5 * sample.B04, 2.5 * sample.B03];
}
""",
    "ndvi": """
//VERSION=3
function setup() {
    return {
        input: ["B04", "B08"],
        output: { bands: 1 }
    };
}
function evaluatePixel(sample) {
    let ndvi = (sample.B08 - sample.B04) / (sample.B08 + sample.B04);
    return [ndvi];
}
""",
    "ndwi": """
//VERSION=3
function setup() {
    return {
        input: ["B03", "B08"],
        output: { bands: 1 }
    };
}
function evaluatePixel(sample) {
    let ndwi = (sample.B03 - sample.B08) / (sample.B03 + sample.B08);
    return [ndwi];
}
""",
    "urban_index": """
//VERSION=3
function setup() {
    return {
        input: ["B11", "B08"],
        output: { bands: 1 }
    };
}
function evaluatePixel(sample) {
    let ui = (sample.B11 - sample.B08) / (sample.B11 + sample.B08);
    return [ui];
}
"""
}

def get_auth_headers() -> Dict[str, str]:
    """Get authentication headers using OAuth2 access token"""
    # Get or refresh access token
    token_result = get_access_token()

    if token_result.get("error"):
        return {"error": token_result["error"]}

    access_token = token_result.get("access_token")
    return {
        "Authorization": f"Bearer {access_token}",
        "Content-Type": "application/json",
        "Accept": "application/json"
    }

def get_access_token() -> Dict[str, Any]:
    """
    Get OAuth2 access token using client credentials

    Returns:
        Dict with access token or error information
    """
    try:
        client_id = os.environ.get('SENTINELHUB_CLIENT_ID')
        client_secret = os.environ.get('SENTINELHUB_CLIENT_SECRET')

        if not client_id or not client_secret:
            return {
                "error": "Missing Sentinel Hub credentials. Set SENTINELHUB_CLIENT_ID and SENTINELHUB_CLIENT_SECRET environment variables."
            }

        # Prepare authentication request
        auth_string = f"{client_id}:{client_secret}"
        auth_bytes = auth_string.encode('ascii')
        auth_b64 = base64.b64encode(auth_bytes).decode('ascii')

        headers = {
            "Authorization": f"Basic {auth_b64}",
            "Content-Type": "application/x-www-form-urlencoded"
        }

        data = "grant_type=client_credentials"

        response = requests.post(TOKEN_URL, headers=headers, data=data, timeout=TIMEOUT)
        response.raise_for_status()

        token_data = response.json()

        return {
            "access_token": token_data.get("access_token"),
            "expires_in": token_data.get("expires_in"),
            "token_type": token_data.get("token_type"),
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        error_msg = f"Authentication failed: {e.response.status_code}"
        if e.response.status_code == 401:
            error_msg = "Invalid client credentials"
        elif e.response.status_code == 403:
            error_msg = "Access forbidden"
        return {"error": f"{error_msg}: {str(e)}"}

    except requests.exceptions.Timeout:
        return {"error": "Authentication timeout"}

    except requests.exceptions.ConnectionError:
        return {"error": "Connection error during authentication"}

    except Exception as e:
        return {"error": f"Authentication error: {str(e)}"}

def _make_catalog_request(search_params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Centralized request handler for Catalog API

    Args:
        search_params: Search parameters for the catalog API

    Returns:
        Dict with 'data', 'metadata', and 'error' keys
    """
    try:
        headers = get_auth_headers()

        if "error" in headers:
            return {
                "data": [],
                "metadata": {},
                "error": headers["error"]
            }

        response = requests.post(CATALOG_API_URL,
                               headers=headers,
                               json=search_params,
                               timeout=TIMEOUT)
        response.raise_for_status()

        raw_data = response.json()

        # Process GeoJSON FeatureCollection
        features = raw_data.get("features", [])

        enhanced_features = []
        for feature in features:
            enhanced_feature = {
                "id": feature.get("id"),
                "collection": feature.get("collection"),
                "datetime": feature.get("properties", {}).get("datetime"),
                "cloud_cover": feature.get("properties", {}).get("eo:cloud_cover", 0),
                "bbox": feature.get("bbox"),
                "geometry": feature.get("geometry"),
                "properties": feature.get("properties", {}),
                "assets": feature.get("assets", {}),
                "links": feature.get("links", [])
            }
            enhanced_features.append(enhanced_feature)

        # Sort by datetime (newest first) and cloud cover (clearest first)
        enhanced_features.sort(key=lambda x: (
            x.get("datetime", ""),
            x.get("cloud_cover", 100)
        ), reverse=True)

        return {
            "data": enhanced_features,
            "metadata": {
                "source": "Sentinel Hub Catalog API",
                "total_scenes": len(enhanced_features),
                "search_params": search_params,
                "timestamp": datetime.utcnow().isoformat(),
                "description": "Available satellite imagery scenes"
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        error_msg = f"Catalog API Error {e.response.status_code}"
        if e.response.status_code == 401:
            error_msg = "Authentication expired - please check credentials"
        elif e.response.status_code == 403:
            error_msg = "Insufficient permissions for catalog access"
        elif e.response.status_code == 429:
            error_msg = "Rate limit exceeded - please try again later"

        return {
            "data": [],
            "metadata": {},
            "error": f"{error_msg}: {str(e)}"
        }

    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {},
            "error": "Catalog API timeout"
        }

    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {},
            "error": "Connection error to catalog API"
        }

    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {},
            "error": "Invalid JSON response from catalog API"
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Catalog API error: {str(e)}"
        }

def _make_process_request(process_params: Dict[str, Any],
                         save_to_file: bool = False) -> Dict[str, Any]:
    """
    Centralized request handler for Process API

    Args:
        process_params: Process parameters for the Process API
        save_to_file: Whether to save the image to a temporary file

    Returns:
        Dict with 'data', 'metadata', and 'error' keys
    """
    try:
        headers = get_auth_headers()

        if "error" in headers:
            return {
                "data": {},
                "metadata": {},
                "error": headers["error"]
            }

        # Update headers for image response
        process_headers = headers.copy()
        process_headers["Accept"] = "image/*"

        response = requests.post(PROCESS_API_URL,
                               headers=process_headers,
                               json=process_params,
                               timeout=TIMEOUT)
        response.raise_for_status()

        # Handle image response
        content_type = response.headers.get('content-type', '')

        if save_to_file:
            # Save to temporary file
            with tempfile.NamedTemporaryFile(delete=False, suffix='.png') as tmp_file:
                tmp_file.write(response.content)
                file_path = tmp_file.name

            return {
                "data": {
                    "image_file": file_path,
                    "content_type": content_type,
                    "size_bytes": len(response.content)
                },
                "metadata": {
                    "source": "Sentinel Hub Process API",
                    "process_params": process_params,
                    "timestamp": datetime.utcnow().isoformat(),
                    "description": "Processed satellite image saved to file"
                },
                "error": None
            }
        else:
            # Return base64 encoded image
            image_b64 = base64.b64encode(response.content).decode('utf-8')

            return {
                "data": {
                    "image_base64": image_b64,
                    "content_type": content_type,
                    "size_bytes": len(response.content)
                },
                "metadata": {
                    "source": "Sentinel Hub Process API",
                    "process_params": process_params,
                    "timestamp": datetime.utcnow().isoformat(),
                    "description": "Processed satellite image (base64 encoded)"
                },
                "error": None
            }

    except requests.exceptions.HTTPError as e:
        error_msg = f"Process API Error {e.response.status_code}"
        if e.response.status_code == 401:
            error_msg = "Authentication expired - please check credentials"
        elif e.response.status_code == 400:
            error_msg = "Invalid process parameters"
        elif e.response.status_code == 429:
            error_msg = "Rate limit exceeded - please try again later"

        return {
            "data": {},
            "metadata": {},
            "error": f"{error_msg}: {str(e)}"
        }

    except requests.exceptions.Timeout:
        return {
            "data": {},
            "metadata": {},
            "error": "Process API timeout - image processing took too long"
        }

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Process API error: {str(e)}"
        }

# ============================================================================
# CATALOG API ENDPOINTS
# ============================================================================

def search_imagery(bbox: List[float],
                   datetime_range: str,
                   collections: Optional[List[str]] = None,
                   max_cloud_cover: float = 30.0,
                   limit: int = 10) -> Dict[str, Any]:
    """
    Search for available satellite imagery using the Catalog API

    Args:
        bbox: Bounding box [min_lon, min_lat, max_lon, max_lat]
        datetime_range: ISO datetime range "YYYY-MM-DDTHH:MM:SSZ/YYYY-MM-DDTHH:MM:SSZ"
        collections: List of satellite collections to search
        max_cloud_cover: Maximum cloud coverage percentage (default: 30%)
        limit: Maximum number of scenes to return (default: 10)

    Returns:
        Dict with 'data', 'metadata', and 'error' keys containing search results
    """
    try:
        if not bbox or len(bbox) != 4:
            return {
                "data": [],
                "metadata": {},
                "error": "Bounding box must be a list of 4 coordinates: [min_lon, min_lat, max_lon, max_lat]"
            }

        if not datetime_range:
            return {
                "data": [],
                "metadata": {},
                "error": "Date range is required in ISO format"
            }

        # Default collections if not specified
        if not collections:
            collections = [SENTINEL_2_L2A]

        # Build search parameters
        search_params = {
            "bbox": bbox,
            "datetime": datetime_range,
            "collections": collections,
            "limit": limit,
            "query": {
                "eo:cloud_cover": {
                    "lt": max_cloud_cover
                }
            }
        }

        result = _make_catalog_request(search_params)

        if result.get("error"):
            return result

        # Add filtering information to metadata
        result["metadata"].update({
            "bbox": bbox,
            "datetime_range": datetime_range,
            "collections": collections,
            "max_cloud_cover": max_cloud_cover,
            "limit": limit
        })

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching imagery: {str(e)}"
        }

def search_imagery_by_coordinates(lat: float, lon: float,
                                 radius_km: float = 10.0,
                                 start_date: str = None,
                                 end_date: str = None,
                                 collections: Optional[List[str]] = None,
                                 max_cloud_cover: float = 30.0,
                                 limit: int = 10) -> Dict[str, Any]:
    """
    Search for satellite imagery by center coordinates and radius

    Args:
        lat: Latitude of center point
        lon: Longitude of center point
        radius_km: Search radius in kilometers (default: 10km)
        start_date: Start date in YYYY-MM-DD format (default: 30 days ago)
        end_date: End date in YYYY-MM-DD format (default: today)
        collections: List of satellite collections to search
        max_cloud_cover: Maximum cloud coverage percentage (default: 30%)
        limit: Maximum number of scenes to return (default: 10)

    Returns:
        Dict with 'data', 'metadata', and 'error' keys containing search results
    """
    try:
        # Calculate bounding box from coordinates and radius
        # Approximate conversion: 1 degree lat = ~111km, 1 degree lon = 111km * cos(lat)
        lat_delta = radius_km / 111.0
        lon_delta = radius_km / (111.0 * abs(lat) if lat != 0 else 111.0)

        bbox = [
            lon - lon_delta,  # min_lon
            lat - lat_delta,  # min_lat
            lon + lon_delta,  # max_lon
            lat + lat_delta   # max_lat
        ]

        # Default date range if not specified
        if not end_date:
            end_date = datetime.utcnow().strftime("%Y-%m-%d")
        if not start_date:
            start_date = (datetime.utcnow() - timedelta(days=30)).strftime("%Y-%m-%d")

        datetime_range = f"{start_date}T00:00:00Z/{end_date}T23:59:59Z"

        result = search_imagery(bbox, datetime_range, collections, max_cloud_cover, limit)

        if result.get("error"):
            return result

        # Add coordinate search info to metadata
        result["metadata"].update({
            "search_center": {"lat": lat, "lon": lon},
            "search_radius_km": radius_km,
            "search_type": "coordinate_based"
        })

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching by coordinates: {str(e)}"
        }

# ============================================================================
# PROCESS API ENDPOINTS
# ============================================================================

def process_imagery(bbox: List[float],
                   datetime_range: str,
                   evalscript: str = None,
                   evalscript_type: str = "true_color",
                   width: int = 512,
                   height: int = 512,
                   format_type: str = "image/png",
                   save_to_file: bool = False) -> Dict[str, Any]:
    """
    Process satellite imagery using the Process API

    Args:
        bbox: Bounding box [min_lon, min_lat, max_lon, max_lat]
        datetime_range: ISO datetime range
        evalscript: Custom evalscript (overrides evalscript_type)
        evalscript_type: Predefined evalscript type (true_color, false_color, ndvi, etc.)
        width: Output image width (default: 512)
        height: Output image height (default: 512)
        format_type: Output format (image/png, image/tiff, etc.)
        save_to_file: Whether to save image to temporary file (default: False)

    Returns:
        Dict with 'data', 'metadata', and 'error' keys containing processed image
    """
    try:
        if not bbox or len(bbox) != 4:
            return {
                "data": {},
                "metadata": {},
                "error": "Bounding box must be a list of 4 coordinates"
            }

        if not datetime_range:
            return {
                "data": {},
                "metadata": {},
                "error": "Date range is required"
            }

        # Use predefined evalscript if custom one not provided
        if not evalscript and evalscript_type in EVALSCRIPTS:
            evalscript = EVALSCRIPTS[evalscript_type]
        elif not evalscript:
            evalscript = EVALSCRIPTS["true_color"]

        # Build process parameters
        process_params = {
            "input": {
                "bounds": {
                    "bbox": bbox
                },
                "data": [{
                    "type": SENTINEL_2_L2A,
                    "dataFilter": {
                        "timeRange": {
                            "from": datetime_range.split("/")[0],
                            "to": datetime_range.split("/")[1]
                        },
                        "maxCloudCoverage": max(0, min(100, 30))  # Default 30% max cloud
                    }
                }]
            },
            "output": {
                "width": width,
                "height": height,
                "responses": [{
                    "identifier": "default",
                    "format": {"type": format_type}
                }]
            },
            "evalscript": evalscript
        }

        result = _make_process_request(process_params, save_to_file)

        if result.get("error"):
            return result

        # Add processing info to metadata
        result["metadata"].update({
            "bbox": bbox,
            "datetime_range": datetime_range,
            "evalscript_type": evalscript_type,
            "width": width,
            "height": height,
            "format": format_type
        })

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error processing imagery: {str(e)}"
        }

def process_imagery_by_scene_id(scene_id: str,
                               evalscript: str = None,
                               evalscript_type: str = "true_color",
                               width: int = 512,
                               height: int = 512,
                               format_type: str = "image/png",
                               save_to_file: bool = False) -> Dict[str, Any]:
    """
    Process satellite imagery using a specific scene ID

    Args:
        scene_id: Scene ID from catalog search
        evalscript: Custom evalscript (overrides evalscript_type)
        evalscript_type: Predefined evalscript type
        width: Output image width
        height: Output image height
        format_type: Output format
        save_to_file: Whether to save image to temporary file

    Returns:
        Dict with 'data', 'metadata', and 'error' keys containing processed image
    """
    try:
        if not scene_id:
            return {
                "data": {},
                "metadata": {},
                "error": "Scene ID is required"
            }

        # Extract scene info from ID (S2A_MSIL2A_20191210T100311...)
        scene_datetime = None
        bbox = None

        # Try to extract datetime from scene ID
        import re
        date_match = re.search(r'_(\d{8}T\d{6})_', scene_id)
        if date_match:
            scene_datetime = date_match.group(1)

        # For now, we'll need to search for the scene to get its bbox
        # In a real implementation, you might cache this info or use a different endpoint

        if not scene_datetime:
            return {
                "data": {},
                "metadata": {},
                "error": "Could not extract datetime from scene ID"
            }

        # Create a small date range around the scene time
        scene_time = datetime.strptime(scene_datetime, "%Y%m%dT%H%M%S")
        datetime_range = f"{scene_time.strftime('%Y-%m-%dT%H:%M:%SZ')}/{scene_time.strftime('%Y-%m-%dT%H:%M:%SZ')}"

        # Search for the scene to get its bbox
        search_result = search_imagery(
            bbox=[-180, -90, 180, 90],  # Global search
            datetime_range=datetime_range,
            limit=1
        )

        if search_result.get("error") or not search_result.get("data"):
            return {
                "data": {},
                "metadata": {},
                "error": f"Could not find scene with ID: {scene_id}"
            }

        scene_data = search_result["data"][0]
        bbox = scene_data.get("bbox")

        if not bbox:
            return {
                "data": {},
                "metadata": {},
                "error": "Could not determine bounding box for scene"
            }

        result = process_imagery(
            bbox=bbox,
            datetime_range=datetime_range,
            evalscript=evalscript,
            evalscript_type=evalscript_type,
            width=width,
            height=height,
            format_type=format_type,
            save_to_file=save_to_file
        )

        if result.get("error"):
            return result

        # Add scene-specific info to metadata
        result["metadata"].update({
            "scene_id": scene_id,
            "processing_method": "scene_id_based"
        })

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error processing scene {scene_id}: {str(e)}"
        }

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def get_available_collections() -> Dict[str, Any]:
    """
    Get list of available satellite collections with descriptions

    Returns:
        Dict with collection information
    """
    return {
        "data": [
            {
                "id": SENTINEL_2_L2A,
                "name": "Sentinel-2 Level-2A",
                "description": "High-resolution optical imagery with atmospheric correction",
                "resolution": "10m, 20m, 60m",
                "revisit_time": "5 days",
                "bands": ["B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B8A", "B09", "B11", "B12"],
                "use_cases": ["Vegetation monitoring", "Land cover", "Coastal areas"]
            },
            {
                "id": SENTINEL_1_GRD,
                "name": "Sentinel-1 Ground Range Detected",
                "description": "Radar imagery (C-band) - works day and night, through clouds",
                "resolution": "5x20m",
                "revisit_time": "1-3 days",
                "bands": ["VV", "VH", "HH", "HV"],
                "use_cases": ["Flood monitoring", "Oil spill detection", "Ship detection"]
            },
            {
                "id": LANDSAT_8_L1,
                "name": "Landsat 8 Level-1",
                "description": "Medium-resolution optical imagery",
                "resolution": "15m, 30m, 100m",
                "revisit_time": "16 days",
                "bands": ["B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10", "B11"],
                "use_cases": ["Land cover change", "Urban development", "Agriculture"]
            },
            {
                "id": MODIS,
                "name": "MODIS",
                "description": "Daily global coverage for environmental monitoring",
                "resolution": "250m, 500m, 1000m",
                "revisit_time": "Daily",
                "bands": ["Multiple spectral bands"],
                "use_cases": ["Climate monitoring", "Vegetation indices", "Disaster monitoring"]
            }
        ],
        "metadata": {
            "source": "Sentinel Hub",
            "timestamp": datetime.utcnow().isoformat(),
            "description": "Available satellite collections"
        },
        "error": None
    }

def get_evalscript_types() -> Dict[str, Any]:
    """
    Get list of available evalscript types with descriptions

    Returns:
        Dict with evalscript information
    """
    return {
        "data": [
            {
                "id": "true_color",
                "name": "True Color",
                "description": "Natural color image as seen by human eye",
                "bands": ["B04 (Red)", "B03 (Green)", "B02 (Blue)"],
                "use_cases": ["General visualization", "Human geography"]
            },
            {
                "id": "false_color",
                "name": "False Color (Infrared)",
                "description": "Infrared composite highlighting vegetation",
                "bands": ["B08 (NIR)", "B04 (Red)", "B03 (Green)"],
                "use_cases": ["Vegetation health", "Forest monitoring"]
            },
            {
                "id": "ndvi",
                "name": "NDVI (Normalized Difference Vegetation Index)",
                "description": "Vegetation health indicator",
                "formula": "(NIR - Red) / (NIR + Red)",
                "range": "-1 to 1",
                "use_cases": ["Crop monitoring", "Drought assessment", "Yield prediction"]
            },
            {
                "id": "ndwi",
                "name": "NDWI (Normalized Difference Water Index)",
                "description": "Water body detection and monitoring",
                "formula": "(Green - NIR) / (Green + NIR)",
                "range": "-1 to 1",
                "use_cases": ["Flood monitoring", "Water resource management", "Coastal monitoring"]
            },
            {
                "id": "urban_index",
                "name": "Urban Index",
                "description": "Built-up area detection",
                "formula": "(SWIR - NIR) / (SWIR + NIR)",
                "range": "-1 to 1",
                "use_cases": ["Urban sprawl monitoring", "Construction tracking", "Infrastructure planning"]
            }
        ],
        "metadata": {
            "source": "Sentinel Hub",
            "timestamp": datetime.utcnow().isoformat(),
            "description": "Available image processing types"
        },
        "error": None
    }

def test_api_connectivity() -> Dict[str, Any]:
    """
    Test connectivity to Sentinel Hub APIs

    Returns:
        Dict with connectivity test results
    """
    results = {}

    # Test authentication
    try:
        auth_result = get_access_token()
        results["authentication"] = {
            "status": "success" if not auth_result.get("error") else "error",
            "message": auth_result.get("error") or "Authentication successful",
            "token_expires_in": auth_result.get("expires_in")
        }
    except Exception as e:
        results["authentication"] = {
            "status": "error",
            "message": str(e)
        }

    # Test catalog API with a small search
    try:
        test_bbox = [13.0, 45.0, 13.1, 45.1]  # Small area in Italy
        test_datetime = f"{(datetime.utcnow() - timedelta(days=60)).strftime('%Y-%m-%d')}T00:00:00Z/{datetime.utcnow().strftime('%Y-%m-%d')}T23:59:59Z"

        catalog_result = search_imagery(
            bbox=test_bbox,
            datetime_range=test_datetime,
            limit=1
        )

        results["catalog_api"] = {
            "status": "success" if not catalog_result.get("error") else "error",
            "message": catalog_result.get("error") or "Catalog API working",
            "scenes_found": len(catalog_result.get("data", []))
        }
    except Exception as e:
        results["catalog_api"] = {
            "status": "error",
            "message": str(e)
        }

    return {
        "data": results,
        "metadata": {
            "test_timestamp": datetime.utcnow().isoformat(),
            "base_url": BASE_URL,
            "credentials_configured": bool(os.environ.get('SENTINELHUB_CLIENT_ID') and os.environ.get('SENTINELHUB_CLIENT_SECRET'))
        },
        "error": None
    }

# ============================================================================
# CLI INTERFACE
# ============================================================================

def main():
    """Command-line interface for Sentinel Hub API wrapper"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python sentinelhub_data.py <command> [args]",
            "available_commands": [
                "search <bbox> <datetime_range> [collections] [max_cloud] [limit]",
                "search-coords <lat> <lon> <radius_km> [start_date] [end_date] [collections]",
                "process <bbox> <datetime_range> [evalscript_type] [width] [height] [format] [save_to_file]",
                "process-scene <scene_id> [evalscript_type] [width] [height] [format] [save_to_file]",
                "collections",
                "evalscripts",
                "test-connectivity"
            ],
            "examples": [
                "sentinelhub_data.py search \"13.0,45.0,14.0,46.0\" \"2019-12-10T00:00:00Z/2019-12-10T23:59:59Z\" sentinel-2-l2a 20 5",
                "sentinelhub_data.py search-coords 45.5 13.6 10 2019-12-01 2019-12-31",
                "sentinelhub_data.py process \"13.0,45.0,14.0,46.0\" \"2019-12-10T00:00:00Z/2019-12-10T23:59:59Z\" ndvi 1024 1024",
                "sentinelhub_data.py process-scene S2A_MSIL2A_20191210T100311_N0213_R122_T33TUE_20191210T121921 true_color",
                "sentinelhub_data.py collections",
                "sentinelhub_data.py evalscripts",
                "sentinelhub_data.py test-connectivity"
            ]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "search":
            if len(sys.argv) < 4:
                result = {"error": "Usage: search <bbox> <datetime_range> [collections] [max_cloud] [limit]"}
            else:
                bbox = json.loads(sys.argv[2])  # Parse as JSON array
                datetime_range = sys.argv[3]
                collections = json.loads(sys.argv[4]) if len(sys.argv) > 4 else None
                max_cloud = float(sys.argv[5]) if len(sys.argv) > 5 else 30.0
                limit = int(sys.argv[6]) if len(sys.argv) > 6 else 10
                result = search_imagery(bbox, datetime_range, collections, max_cloud, limit)

        elif command == "search-coords":
            if len(sys.argv) < 4:
                result = {"error": "Usage: search-coords <lat> <lon> <radius_km> [start_date] [end_date] [collections]"}
            else:
                lat = float(sys.argv[2])
                lon = float(sys.argv[3])
                radius = float(sys.argv[4])
                start_date = sys.argv[5] if len(sys.argv) > 5 else None
                end_date = sys.argv[6] if len(sys.argv) > 6 else None
                collections = json.loads(sys.argv[7]) if len(sys.argv) > 7 else None
                result = search_imagery_by_coordinates(lat, lon, radius, start_date, end_date, collections)

        elif command == "process":
            if len(sys.argv) < 4:
                result = {"error": "Usage: process <bbox> <datetime_range> [evalscript_type] [width] [height] [format] [save_to_file]"}
            else:
                bbox = json.loads(sys.argv[2])
                datetime_range = sys.argv[3]
                evalscript_type = sys.argv[4] if len(sys.argv) > 4 else "true_color"
                width = int(sys.argv[5]) if len(sys.argv) > 5 else 512
                height = int(sys.argv[6]) if len(sys.argv) > 6 else 512
                format_type = sys.argv[7] if len(sys.argv) > 7 else "image/png"
                save_to_file = sys.argv[8].lower() == "true" if len(sys.argv) > 8 else False
                result = process_imagery(bbox, datetime_range, None, evalscript_type, width, height, format_type, save_to_file)

        elif command == "process-scene":
            if len(sys.argv) < 3:
                result = {"error": "Usage: process-scene <scene_id> [evalscript_type] [width] [height] [format] [save_to_file]"}
            else:
                scene_id = sys.argv[2]
                evalscript_type = sys.argv[3] if len(sys.argv) > 3 else "true_color"
                width = int(sys.argv[4]) if len(sys.argv) > 4 else 512
                height = int(sys.argv[5]) if len(sys.argv) > 5 else 512
                format_type = sys.argv[6] if len(sys.argv) > 6 else "image/png"
                save_to_file = sys.argv[7].lower() == "true" if len(sys.argv) > 7 else False
                result = process_imagery_by_scene_id(scene_id, None, evalscript_type, width, height, format_type, save_to_file)

        elif command == "collections":
            result = get_available_collections()

        elif command == "evalscripts":
            result = get_evalscript_types()

        elif command == "test-connectivity":
            result = test_api_connectivity()

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "search <bbox> <datetime_range> [collections] [max_cloud] [limit]",
                    "search-coords <lat> <lon> <radius_km> [start_date] [end_date] [collections]",
                    "process <bbox> <datetime_range> [evalscript_type] [width] [height] [format] [save_to_file]",
                    "process-scene <scene_id> [evalscript_type] [width] [height] [format] [save_to_file]",
                    "collections",
                    "evalscripts",
                    "test-connectivity"
                ]
            }

        print(json.dumps(result, indent=2))

    except json.JSONDecodeError as e:
        print(json.dumps({"error": f"Invalid JSON parameter: {str(e)}"}))
        sys.exit(1)
    except ValueError as e:
        print(json.dumps({"error": f"Invalid parameter: {str(e)}"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"error": f"Command execution failed: {str(e)}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()