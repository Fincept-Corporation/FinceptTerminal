#!/usr/bin/env python3
"""
NASA GIBS (Global Imagery Browse Services) Comprehensive API Wrapper
Complete coverage of all NASA GIBS services with hierarchical structure:
1. Catalogue - Lists all available imagery layers across all services
2. Datasets - Lists available time periods for each layer
3. Series - Gets actual imagery (tiles/custom maps/vectors) for specific coordinates and time

SUPPORTED SERVICES:
- WMTS (Web Map Tile Service) - Fast pre-rendered tiles (primary service)
- WMS (Web Map Service) - Custom map images for any bounding box
- TWMS (Tiled Web Map Service) - Legacy hybrid service

SUPPORTED PROJECTIONS:
- EPSG:3857 (Web Mercator) - Standard web mapping
- EPSG:4326 (Geographic) - Standard lat/lon coordinates
- EPSG:3413 (Arctic Polar) - Arctic region mapping
- EPSG:3031 (Antarctic Polar) - Antarctic region mapping

Usage: python nasa_gibs_api.py <command> [args]

Complete API Documentation: https://nasa-gibs.github.io/gibs-api-docs/
Product Catalog: https://gibs.earthdata.nasa.gov/product-catalog/
Colormap API: https://gibs.earthdata.nasa.gov/colormaps/v1.3/
"""

import sys
import json
import os
import requests
import xml.etree.ElementTree as ET
from typing import Dict, List, Optional, Any, Tuple, Union
from datetime import datetime, timedelta
import re
from urllib.parse import quote
import hashlib
import base64
from io import BytesIO
import time

# --- 1. CONFIGURATION ---

# Complete GIBS API endpoints for all services and projections
GIBS_ENDPOINTS = {
    # WMTS (Web Map Tile Service) - Primary service
    'wmts': {
        'epsg3857': 'https://gibs.earthdata.nasa.gov/wmts/epsg3857/best',
        'epsg4326': 'https://gibs.earthdata.nasa.gov/wmts/epsg4326/best',
        'epsg3413': 'https://gibs.earthdata.nasa.gov/wmts/epsg3413/best',
        'epsg3031': 'https://gibs.earthdata.nasa.gov/wmts/epsg3031/best'
    },
    # WMS (Web Map Service) - Custom map images
    'wms': {
        'epsg3857': 'https://gibs.earthdata.nasa.gov/wms/epsg3857/best',
        'epsg4326': 'https://gibs.earthdata.nasa.gov/wms/epsg4326/best',
        'epsg3413': 'https://gibs.earthdata.nasa.gov/wms/epsg3413/best',
        'epsg3031': 'https://gibs.earthdata.nasa.gov/wms/epsg3031/best'
    },
    # TWMS (Tiled Web Map Service) - Legacy hybrid
    'twms': {
        'epsg3857': 'https://gibs.earthdata.nasa.gov/twms/epsg3857/best',
        'epsg4326': 'https://gibs.earthdata.nasa.gov/twms/epsg4326/best',
        'epsg3413': 'https://gibs.earthdata.nasa.gov/twms/epsg3413/best',
        'epsg3031': 'https://gibs.earthdata.nasa.gov/twms/epsg3031/best'
    }
}

# Projection configurations
PROJECTIONS = {
    'epsg3857': {
        'name': 'Web Mercator (EPSG:3857)',
        'description': 'Standard web mapping projection used by Google Maps, OpenStreetMap',
        'tile_matrix_sets': ['GoogleMapsCompatible_Level1', 'GoogleMapsCompatible_Level9', 'GoogleMapsCompatible_Level18'],
        'max_zoom': 18
    },
    'epsg4326': {
        'name': 'Geographic (EPSG:4326)',
        'description': 'WGS84 Geographic coordinate system (latitude/longitude)',
        'tile_matrix_sets': ['0.5deg', '1deg', '2deg', '4deg', '10deg'],
        'max_zoom': 10
    },
    'epsg3413': {
        'name': 'Arctic Polar (EPSG:3413)',
        'description': 'NSIDC Polar Stereographic projection optimized for Arctic region',
        'tile_matrix_sets': ['Arctic_1km', 'Arctic_2km', 'Arctic_4km', 'Arctic_8km', 'Arctic_16km'],
        'max_zoom': 16
    },
    'epsg3031': {
        'name': 'Antarctic Polar (EPSG:3031)',
        'description': 'NSIDC Polar Stereographic projection optimized for Antarctic region',
        'tile_matrix_sets': ['Antarctic_1km', 'Antarctic_2km', 'Antarctic_4km', 'Antarctic_8km', 'Antarctic_16km'],
        'max_zoom': 16
    }
}

# Supported image formats by service
SUPPORTED_FORMATS = {
    'wmts': ['png', 'jpeg', 'jpg', 'tif', 'tiff'],
    'wms': ['png', 'jpeg', 'jpg', 'gif', 'tif', 'tiff'],
    'twms': ['png', 'jpeg', 'jpg', 'gif', 'tif', 'tiff']
}

# Additional GIBS endpoints
COLORMAP_API = "https://gibs.earthdata.nasa.gov/colormaps/v1.3"
PRODUCT_CATALOG = "https://gibs.earthdata.nasa.gov/product-catalog/"

# Request settings
TIMEOUT = 30
MAX_RETRIES = 3
CACHE_DURATION = 3600  # 1 hour cache for capabilities

# Cache for capabilities responses
_capabilities_cache = {}

def _make_request(url: str, headers: Optional[Dict[str, str]] = None, return_bytes: bool = False,
                   params: Optional[Dict[str, str]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for NASA GIBS API with enhanced error handling

    Args:
        url: Complete URL to request
        headers: Optional HTTP headers
        return_bytes: Whether to return raw bytes (for images) or text
        params: Optional query parameters for GET requests

    Returns:
        Response data or error information
    """
    try:
        # Setup headers
        if headers is None:
            headers = {
                'User-Agent': 'Fincept-Terminal/1.0 (NASA GIBS API Wrapper)',
                'Accept': 'application/xml,image/png,image/jpeg,image/tiff,image/gif,application/json'
            }

        # Make request with retries
        for attempt in range(MAX_RETRIES):
            try:
                response = requests.get(url, headers=headers, timeout=TIMEOUT, params=params)
                response.raise_for_status()

                # Return appropriate data type
                if return_bytes:
                    image_data = response.content

                    # Validate image content
                    image_validation = _validate_image_content(image_data, response.headers.get('content-type', ''))

                    return {
                        "data": image_data,
                        "metadata": {
                            "source": "NASA GIBS",
                            "url": str(response.url),
                            "content_type": response.headers.get('content-type', 'unknown'),
                            "content_length": len(image_data),
                            "last_updated": datetime.now().isoformat(),
                            "cache_control": response.headers.get('cache-control', ''),
                            "expires": response.headers.get('expires', ''),
                            "image_validation": image_validation
                        },
                        "error": None
                    }
                else:
                    return {
                        "data": response.text,
                        "metadata": {
                            "source": "NASA GIBS",
                            "url": str(response.url),
                            "content_type": response.headers.get('content-type', 'text/xml'),
                            "last_updated": datetime.now().isoformat(),
                            "content_length": len(response.text)
                        },
                        "error": None
                    }

            except requests.exceptions.Timeout:
                if attempt == MAX_RETRIES - 1:
                    return {
                        "data": [],
                        "metadata": {"source": "NASA GIBS", "url": url},
                        "error": f"Request timeout after {TIMEOUT} seconds"
                    }
                continue

            except requests.exceptions.ConnectionError:
                if attempt == MAX_RETRIES - 1:
                    return {
                        "data": [],
                        "metadata": {"source": "NASA GIBS", "url": url},
                        "error": "Connection error - unable to reach NASA GIBS service"
                    }
                continue

            except requests.exceptions.HTTPError as e:
                error_details = ""
                if hasattr(e, 'response') and e.response is not None:
                    error_details = f" - {e.response.reason}"
                    if e.response.status_code == 404:
                        error_details += " (Resource not found - check layer ID and parameters)"
                    elif e.response.status_code == 400:
                        error_details += " (Bad request - check parameters)"
                    elif e.response.status_code == 500:
                        error_details += " (Server error - try again later)"

                return {
                    "data": [],
                    "metadata": {"source": "NASA GIBS", "url": url},
                    "error": f"HTTP error: {e.response.status_code}{error_details}"
                }

    except Exception as e:
        return {
            "data": [],
            "metadata": {"source": "NASA GIBS"},
            "error": f"Unexpected error: {str(e)}"
        }

def _validate_image_content(image_data: bytes, content_type: str) -> Dict[str, Any]:
    """
    Validate image content and extract metadata

    Args:
        image_data: Raw image bytes
        content_type: HTTP content type header

    Returns:
        Image validation results
    """
    validation = {
        "is_valid_image": False,
        "detected_format": None,
        "file_size": len(image_data),
        "content_type_match": False,
        "error": None
    }

    try:
        # Check content type matches image
        content_type = content_type.lower()
        if any(img_type in content_type for img_type in ['image/png', 'image/jpeg', 'image/gif', 'image/tiff']):
            validation["content_type_match"] = True

        # Basic image format detection from magic bytes
        if len(image_data) >= 8:
            # PNG
            if image_data[:8] == b'\x89PNG\r\n\x1a\n':
                validation["is_valid_image"] = True
                validation["detected_format"] = "PNG"
            # JPEG
            elif image_data[:2] == b'\xff\xd8' and image_data[-2:] == b'\xff\xd9':
                validation["is_valid_image"] = True
                validation["detected_format"] = "JPEG"
            # GIF
            elif image_data[:6] in (b'GIF87a', b'GIF89a'):
                validation["is_valid_image"] = True
                validation["detected_format"] = "GIF"
            # TIFF
            elif image_data[:4] in (b'II*\x00', b'MM\x00*'):
                validation["is_valid_image"] = True
                validation["detected_format"] = "TIFF"

        # If no format detected but content type says image
        if not validation["is_valid_image"] and validation["content_type_match"]:
            validation["is_valid_image"] = True
            validation["detected_format"] = content_type.split('/')[-1].upper()

        if not validation["is_valid_image"]:
            validation["error"] = f"Invalid image format or corrupted data"

    except Exception as e:
        validation["error"] = f"Image validation error: {str(e)}"

    return validation

def _get_cached_capabilities(cache_key: str, url: str, service: str, projection: str) -> Dict[str, Any]:
    """
    Get capabilities with caching to reduce API calls

    Args:
        cache_key: Unique cache key
        url: URL to fetch capabilities from
        service: Service type (wmts, wms, twms)
        projection: Projection code

    Returns:
        Cached or fresh capabilities response
    """
    global _capabilities_cache

    current_time = time.time()

    # Check cache
    if cache_key in _capabilities_cache:
        cached_data = _capabilities_cache[cache_key]
        if current_time - cached_data['timestamp'] < CACHE_DURATION:
            return cached_data['response']

    # Fetch fresh data
    response = _make_request(url)

    # Cache the response
    _capabilities_cache[cache_key] = {
        'response': response,
        'timestamp': current_time,
        'service': service,
        'projection': projection
    }

    return response

def _parse_xml_capabilities(xml_text: str, service: str = 'wmts') -> Dict[str, Any]:
    """
    Enhanced XML parser for different GIBS service capabilities

    Args:
        xml_text: XML response from capabilities request
        service: Service type (wmts, wms, twms)

    Returns:
        Parsed layer information or error
    """
    try:
        root = ET.fromstring(xml_text)

        # Define XML namespaces for different services
        if service == 'wmts':
            namespaces = {
                'wmts': 'http://www.opengis.net/wmts/1.0',
                'ows': 'http://www.opengis.net/ows/1.1'
            }
        elif service == 'wms':
            namespaces = {
                'wms': 'http://www.opengis.net/wms',
                'xlink': 'http://www.w3.org/1999/xlink'
            }
        else:  # twms
            namespaces = {
                'wms': 'http://www.opengis.net/wms',
                'xlink': 'http://www.w3.org/1999/xlink'
            }

        layers = []

        # Different parsing strategies for different services
        if service == 'wmts':
            for layer in root.findall('.//wmts:Layer', namespaces):
                layer_info = _parse_wmts_layer(layer, namespaces)
                if layer_info:
                    layers.append(layer_info)
        else:
            for layer in root.findall('.//Layer', namespaces):
                layer_info = _parse_wms_layer(layer, namespaces, service)
                if layer_info:
                    layers.append(layer_info)

        return {
            "data": layers,
            "metadata": {
                "count": len(layers),
                "service": service,
                "source": f"NASA GIBS {service.upper()} GetCapabilities"
            },
            "error": None
        }

    except ET.ParseError as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Failed to parse XML response: {str(e)}"
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error parsing capabilities: {str(e)}"
        }

def _parse_wmts_layer(layer_element, namespaces) -> Optional[Dict[str, Any]]:
    """Parse WMTS layer information from XML element"""
    try:
        layer_info = {}

        # Basic layer info
        identifier = layer_element.find('ows:Identifier', namespaces)
        if identifier is not None:
            layer_info['id'] = identifier.text
            layer_info['name'] = identifier.text

        title = layer_element.find('ows:Title', namespaces)
        if title is not None:
            layer_info['title'] = title.text

        abstract = layer_element.find('ows:Abstract', namespaces)
        if abstract is not None:
            layer_info['description'] = abstract.text

        # Get available formats
        formats = []
        for fmt in layer_element.findall('.//wmts:Format', namespaces):
            if fmt.text:
                formats.append(fmt.text.strip())
        layer_info['formats'] = formats

        # Get time periods (if available)
        time_info = None
        for time_elem in layer_element.findall('.//wmts:Dimension', namespaces):
            if time_elem.get('name') == 'time':
                time_info = time_elem.text
                break

        layer_info['time_periods'] = _parse_time_periods(time_info) if time_info else []

        # Get tile matrix sets (zoom levels)
        tile_matrix_sets = []
        for tms in layer_element.findall('.//wmts:TileMatrixSetLink', namespaces):
            tms_id = tms.find('wmts:TileMatrixSet', namespaces)
            if tms_id is not None:
                tile_matrix_sets.append(tms_id.text)

        layer_info['tile_matrix_sets'] = tile_matrix_sets

        # Get style information
        styles = []
        for style in layer_element.findall('.//wmts:Style', namespaces):
            style_id = style.get('identifier')
            if style_id:
                styles.append(style_id)

        layer_info['styles'] = styles

        return layer_info

    except Exception:
        return None

def _parse_wms_layer(layer_element, namespaces, service: str) -> Optional[Dict[str, Any]]:
    """Parse WMS/TWMS layer information from XML element"""
    try:
        layer_info = {}

        # Basic layer info
        name = layer_element.find('Name', namespaces)
        if name is not None:
            layer_info['id'] = name.text
            layer_info['name'] = name.text

        title = layer_element.find('Title', namespaces)
        if title is not None:
            layer_info['title'] = title.text

        abstract = layer_element.find('Abstract', namespaces)
        if abstract is not None:
            layer_info['description'] = abstract.text

        # Get time periods (if available)
        time_info = None
        extent = layer_element.find('Extent', namespaces)
        if extent is not None and extent.get('name') == 'time':
            time_info = extent.text

        layer_info['time_periods'] = _parse_time_periods(time_info) if time_info else []

        # Get style information
        styles = []
        for style in layer_element.findall('.//Style', namespaces):
            style_name = style.find('Name', namespaces)
            if style_name is not None:
                styles.append(style_name.text)

        layer_info['styles'] = styles

        # WMS/TWMS specific information
        layer_info['service_type'] = service

        return layer_info

    except Exception:
        return None

def _parse_time_periods(time_string: str) -> List[str]:
    """
    Parse time periods from GIBS time dimension string

    Args:
        time_string: Time dimension string from capabilities

    Returns:
        List of available time periods
    """
    try:
        if not time_string:
            return []

        # Handle different time formats
        time_periods = []

        # Split by comma for multiple periods
        for period in time_string.split(','):
            period = period.strip()

            # Handle date ranges (e.g., "2023-01-01/2023-12-31/P1D")
            if '/' in period:
                parts = period.split('/')
                if len(parts) >= 2:
                    start_date = parts[0]
                    end_date = parts[1]
                    # Add just start and end dates for simplicity
                    time_periods.extend([start_date, end_date])
            else:
                # Single date
                time_periods.append(period)

        return sorted(list(set(time_periods)))  # Remove duplicates and sort

    except Exception:
        return []

def _get_tile_coordinates(lat: float, lon: float, zoom: int, projection: str = 'epsg3857') -> Tuple[int, int]:
    """
    Convert lat/lon to tile coordinates for different projections

    Args:
        lat: Latitude
        lon: Longitude
        zoom: Zoom level
        projection: Coordinate projection system

    Returns:
        Tuple of (tile_x, tile_y)
    """
    import math

    if projection == 'epsg3857':
        # Web Mercator tile calculation
        n = 2.0 ** zoom
        x = int((lon + 180.0) / 360.0 * n)
        y = int((1.0 - math.log(math.tan(math.radians(lat)) + 1.0 / math.cos(math.radians(lat))) / math.pi) / 2.0 * n)
    elif projection == 'epsg4326':
        # Geographic projection - simpler calculation
        n = 2.0 ** zoom
        x = int((lon + 180.0) / 360.0 * n)
        y = int((90.0 - lat) / 180.0 * n)
    else:
        # For polar projections, use a simplified approach
        # In practice, these would require complex polar stereographic math
        n = 2.0 ** zoom
        x = int((lon + 180.0) / 360.0 * n)
        y = int((90.0 - lat) / 180.0 * n)

    return x, y

def _tile_to_lat_lon(tile_x: int, tile_y: int, zoom: int, projection: str = 'epsg3857') -> Tuple[float, float]:
    """Convert tile coordinates to lat/lon of tile center"""
    import math

    if projection == 'epsg3857':
        # Web Mercator
        n = 2.0 ** zoom
        lon_deg = tile_x / n * 360.0 - 180.0
        lat_rad = math.atan(math.sinh(math.pi * (1 - 2 * tile_y / n)))
        lat_deg = math.degrees(lat_rad)
    elif projection == 'epsg4326':
        # Geographic
        n = 2.0 ** zoom
        lon_deg = tile_x / n * 360.0 - 180.0
        lat_deg = 90.0 - (tile_y / n * 180.0)
    else:
        # Simplified for polar projections
        n = 2.0 ** zoom
        lon_deg = tile_x / n * 360.0 - 180.0
        lat_deg = 90.0 - (tile_y / n * 180.0)

    return lat_deg, lon_deg

def _validate_coordinates(lat: float, lon: float, projection: str) -> Dict[str, Any]:
    """
    Validate coordinates for specific projection

    Args:
        lat: Latitude
        lon: Longitude
        projection: Projection code

    Returns:
        Validation result
    """
    validation = {
        "is_valid": False,
        "projection": projection,
        "error": None
    }

    try:
        if projection == 'epsg3857':
            # Web Mercator: latitude should be between -85.051129 and 85.051129
            if not (-85.051129 <= lat <= 85.051129):
                validation["error"] = f"Latitude {lat} out of range for Web Mercator (-85.051129 to 85.051129)"
            elif not (-180 <= lon <= 180):
                validation["error"] = f"Longitude {lon} out of range (-180 to 180)"
            else:
                validation["is_valid"] = True

        elif projection == 'epsg4326':
            # Geographic: standard lat/lon ranges
            if not (-90 <= lat <= 90):
                validation["error"] = f"Latitude {lat} out of range (-90 to 90)"
            elif not (-180 <= lon <= 180):
                validation["error"] = f"Longitude {lon} out of range (-180 to 180)"
            else:
                validation["is_valid"] = True

        elif projection in ['epsg3413', 'epsg3031']:
            # Polar projections - more lenient validation
            if not (-90 <= lat <= 90):
                validation["error"] = f"Latitude {lat} out of range (-90 to 90)"
            elif not (-180 <= lon <= 180):
                validation["error"] = f"Longitude {lon} out of range (-180 to 180)"
            else:
                validation["is_valid"] = True

        else:
            validation["error"] = f"Unknown projection: {projection}"

    except Exception as e:
        validation["error"] = f"Coordinate validation error: {str(e)}"

    return validation

def _parse_time_periods(time_string: str) -> List[str]:
    """
    Parse time periods from GIBS time dimension string

    Args:
        time_string: Time dimension string from capabilities

    Returns:
        List of available time periods
    """
    try:
        if not time_string:
            return []

        # Handle different time formats
        time_periods = []

        # Split by comma for multiple periods
        for period in time_string.split(','):
            period = period.strip()

            # Handle date ranges (e.g., "2023-01-01/2023-12-31/P1D")
            if '/' in period:
                parts = period.split('/')
                if len(parts) >= 2:
                    start_date = parts[0]
                    end_date = parts[1]
                    # Add just start and end dates for simplicity
                    time_periods.extend([start_date, end_date])
            else:
                # Single date
                time_periods.append(period)

        return sorted(list(set(time_periods)))  # Remove duplicates and sort

    except Exception:
        return []

def _get_tile_coordinates(lat: float, lon: float, zoom: int) -> Tuple[int, int]:
    """
    Convert lat/lon to tile coordinates for Web Mercator projection

    Args:
        lat: Latitude
        lon: Longitude
        zoom: Zoom level

    Returns:
        Tuple of (tile_x, tile_y)
    """
    import math

    # Web Mercator tile calculation
    n = 2.0 ** zoom
    x = int((lon + 180.0) / 360.0 * n)
    y = int((1.0 - math.log(math.tan(math.radians(lat)) + 1.0 / math.cos(math.radians(lat))) / math.pi) / 2.0 * n)

    return x, y

# --- 2. CORE FUNCTIONS (GROUPED BY API CATEGORY) ---

# ====== CATALOGUE (LAYERS) ======
def list_layers() -> Dict[str, Any]:
    """
    Get list of all available imagery layers from NASA GIBS

    Returns:
        JSON response with layer list
    """
    try:
        url = f"{BASE_URL}/wmts.cgi?request=GetCapabilities"
        result = _make_request(url)

        if result["error"]:
            return result

        # Parse XML capabilities
        parsed = _parse_xml_capabilities(result["data"])

        if parsed["error"]:
            return parsed

        # Enhance layer data
        enhanced_data = []
        for layer in parsed.get("data", []):
            enhanced_layer = {
                "id": layer.get("id", ""),
                "name": layer.get("name", ""),
                "title": layer.get("title", ""),
                "description": layer.get("description", ""),
                "formats": layer.get("formats", []),
                "time_periods_count": len(layer.get("time_periods", [])),
                "tile_matrix_sets": layer.get("tile_matrix_sets", []),
                "styles": layer.get("styles", [])
            }
            enhanced_data.append(enhanced_layer)

        return {
            "data": enhanced_data,
            "metadata": {
                "source": "NASA GIBS",
                "count": len(enhanced_data),
                "last_updated": datetime.now().isoformat(),
                "projection": "EPSG:3857 (Web Mercator)"
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching layers: {str(e)}"
        }

def get_layer_details(layer_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific layer

    Args:
        layer_id: The unique ID of the layer

    Returns:
        JSON response with layer details
    """
    try:
        # Get all layers first
        layers_result = list_layers()

        if layers_result["error"]:
            return layers_result

        # Find the specific layer
        layers = layers_result.get("data", [])
        layer = None

        for l in layers:
            if l.get("id") == layer_id or l.get("name") == layer_id:
                layer = l
                break

        if not layer:
            return {
                "data": {},
                "metadata": {},
                "error": f"Layer '{layer_id}' not found"
            }

        # Get full capabilities for this layer
        url = f"{BASE_URL}/wmts.cgi?request=GetCapabilities"
        result = _make_request(url)

        if not result["error"]:
            parsed = _parse_xml_capabilities(result["data"])
            if not parsed["error"]:
                # Find detailed info
                for detailed_layer in parsed.get("data", []):
                    if detailed_layer.get("id") == layer_id:
                        layer.update(detailed_layer)
                        break

        return {
            "data": layer,
            "metadata": {
                "layer_id": layer_id,
                "source": "NASA GIBS",
                "last_updated": datetime.now().isoformat()
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching layer details: {str(e)}"
        }

# ====== DATASETS (TIME PERIODS) ======
def get_layer_time_periods(layer_id: str, limit: int = 50) -> Dict[str, Any]:
    """
    Get available time periods for a specific layer

    Args:
        layer_id: The unique ID of the layer
        limit: Maximum number of time periods to return

    Returns:
        JSON response with available time periods
    """
    try:
        # Get layer details
        layer_result = get_layer_details(layer_id)

        if layer_result["error"]:
            return layer_result

        layer = layer_result.get("data", {})
        time_periods = layer.get("time_periods", [])

        # Limit results
        if limit and len(time_periods) > limit:
            time_periods = time_periods[:limit]

        # Enhance time period data
        enhanced_data = []
        for time_period in time_periods:
            enhanced_time = {
                "date": time_period,
                "layer_id": layer_id,
                "formatted_date": _format_date(time_period)
            }
            enhanced_data.append(enhanced_time)

        return {
            "data": enhanced_data,
            "metadata": {
                "layer_id": layer_id,
                "count": len(enhanced_data),
                "total_available": len(layer.get("time_periods", [])),
                "source": "NASA GIBS",
                "last_updated": datetime.now().isoformat()
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching time periods: {str(e)}"
        }

def _format_date(date_str: str) -> str:
    """Format date string for display"""
    try:
        # Try to parse and reformat date
        date_obj = datetime.strptime(date_str, "%Y-%m-%d")
        return date_obj.strftime("%B %d, %Y")
    except:
        return date_str

# ====== SERIES (TILES) ======
def get_tile(layer_id: str, date: str, zoom: int, lat: float, lon: float,
            format: str = "png", projection: str = "epsg3857") -> Dict[str, Any]:
    """
    Get a satellite image tile for specific coordinates and time

    Args:
        layer_id: The name of the product layer
        date: Date for the imagery (YYYY-MM-DD format)
        zoom: Zoom level (0-18 for Web Mercator)
        lat: Latitude coordinate
        lon: Longitude coordinate
        format: Image format (png, jpeg, etc.)
        projection: Coordinate projection system

    Returns:
        JSON response with tile data
    """
    try:
        # Validate inputs
        if projection not in PROJECTIONS:
            return {
                "data": {},
                "metadata": {},
                "error": f"Unsupported projection: {projection}. Supported: {list(PROJECTIONS.keys())}"
            }

        if format not in SUPPORTED_FORMATS:
            return {
                "data": {},
                "metadata": {},
                "error": f"Unsupported format: {format}. Supported: {SUPPORTED_FORMATS}"
            }

        # Get tile coordinates
        tile_x, tile_y = _get_tile_coordinates(lat, lon, zoom)

        # Build tile URL
        proj_config = PROJECTIONS[projection]
        tile_matrix_set = "GoogleMapsCompatible_Level9" if zoom <= 9 else "GoogleMapsCompatible_Level18"

        # URL encode layer name
        encoded_layer = quote(layer_id)

        tile_url = f"{proj_config['base_url']}/{encoded_layer}/default/{date}/{tile_matrix_set}/{zoom}/{tile_y}/{tile_x}.{format}"

        # Request the tile
        result = _make_request(tile_url, return_bytes=True)

        if not result["error"]:
            # Enhance tile metadata
            result["metadata"].update({
                "layer_id": layer_id,
                "date": date,
                "zoom": zoom,
                "latitude": lat,
                "longitude": lon,
                "tile_x": tile_x,
                "tile_y": tile_y,
                "format": format,
                "projection": projection,
                "tile_matrix_set": tile_matrix_set
            })

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching tile: {str(e)}"
        }

def get_tile_batch(layer_id: str, date: str, zoom: int, lat_min: float, lon_min: float,
                   lat_max: float, lon_max: float, format: str = "png",
                   projection: str = "epsg3857") -> Dict[str, Any]:
    """
    Get multiple tiles for a bounding box area

    Args:
        layer_id: The name of the product layer
        date: Date for the imagery (YYYY-MM-DD format)
        zoom: Zoom level
        lat_min, lon_min: Minimum latitude and longitude
        lat_max, lon_max: Maximum latitude and longitude
        format: Image format
        projection: Coordinate projection system

    Returns:
        JSON response with multiple tiles
    """
    try:
        # Get tile coordinates for bounding box
        tile_x_min, tile_y_max = _get_tile_coordinates(lat_min, lon_min, zoom)
        tile_x_max, tile_y_min = _get_tile_coordinates(lat_max, lon_max, zoom)

        tiles = []
        errors = []

        # Fetch tiles in the bounding box
        for tile_x in range(tile_x_min, tile_x_max + 1):
            for tile_y in range(tile_y_min, tile_y_max + 1):
                try:
                    # Convert tile coordinates back to lat/lon for the tile center
                    lat_center, lon_center = _tile_to_lat_lon(tile_x, tile_y, zoom)

                    tile_result = get_tile(layer_id, date, zoom, lat_center, lon_center, format, projection)

                    if not tile_result["error"]:
                        tile_data = tile_result["data"]
                        tile_metadata = tile_result["metadata"]

                        enhanced_tile = {
                            "tile_data": tile_data,
                            "tile_x": tile_x,
                            "tile_y": tile_y,
                            "lat_center": lat_center,
                            "lon_center": lon_center,
                            "metadata": tile_metadata
                        }
                        tiles.append(enhanced_tile)
                    else:
                        errors.append(f"Tile ({tile_x}, {tile_y}): {tile_result['error']}")

                except Exception as e:
                    errors.append(f"Tile ({tile_x}, {tile_y}): {str(e)}")

        return {
            "data": tiles,
            "metadata": {
                "layer_id": layer_id,
                "date": date,
                "zoom": zoom,
                "bounding_box": {
                    "lat_min": lat_min,
                    "lon_min": lon_min,
                    "lat_max": lat_max,
                    "lon_max": lon_max
                },
                "tile_range": {
                    "x_min": tile_x_min,
                    "x_max": tile_x_max,
                    "y_min": tile_y_min,
                    "y_max": tile_y_max
                },
                "total_tiles_requested": (tile_x_max - tile_x_min + 1) * (tile_y_max - tile_y_min + 1),
                "successful_tiles": len(tiles),
                "failed_tiles": len(errors),
                "source": "NASA GIBS",
                "last_updated": datetime.now().isoformat()
            },
            "error": None if errors else f"Partial success with {len(errors)} failed tiles",
            "errors": errors if errors else None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching tile batch: {str(e)}"
        }

def _tile_to_lat_lon(tile_x: int, tile_y: int, zoom: int) -> Tuple[float, float]:
    """Convert tile coordinates to lat/lon of tile center"""
    import math

    n = 2.0 ** zoom
    lon_deg = tile_x / n * 360.0 - 180.0
    lat_rad = math.atan(math.sinh(math.pi * (1 - 2 * tile_y / n)))
    lat_deg = math.degrees(lat_rad)

    return lat_deg, lon_deg

# ====== UTILITY FUNCTIONS ======
def get_supported_projections() -> Dict[str, Any]:
    """
    Get list of supported coordinate projections

    Returns:
        JSON response with supported projections
    """
    try:
        projections = []
        for code, config in PROJECTIONS.items():
            projections.append({
                "code": code,
                "name": config["name"],
                "base_url": config["base_url"],
                "tile_matrix_sets": config["tile_matrix_sets"]
            })

        return {
            "data": projections,
            "metadata": {
                "count": len(projections),
                "source": "NASA GIBS API Wrapper",
                "last_updated": datetime.now().isoformat()
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching projections: {str(e)}"
        }

def get_popular_layers(limit: int = 20) -> Dict[str, Any]:
    """
    Get popular imagery layers based on common usage

    Args:
        limit: Maximum number of layers to return

    Returns:
        JSON response with popular layers
    """
    try:
        # Get all layers
        layers_result = list_layers()

        if layers_result["error"]:
            return layers_result

        all_layers = layers_result.get("data", [])

        # Popular layers based on common usage
        popular_keywords = [
            "MODIS", "Terra", "Aqua", "TrueColor", "CorrectedReflectance",
            "VIIRS", "SNPP", "Suomi-NPP", "NightLights", "Thermal",
            "Landsat", "Sentinel", "ASTER", "GPM", "AIRS"
        ]

        scored_layers = []
        for layer in all_layers:
            score = 0
            layer_name = layer.get("name", "").upper()
            layer_title = layer.get("title", "").upper()

            for keyword in popular_keywords:
                if keyword.upper() in layer_name:
                    score += 3
                if keyword.upper() in layer_title:
                    score += 2

            if score > 0:
                layer["popularity_score"] = score
                scored_layers.append(layer)

        # Sort by score and limit results
        scored_layers.sort(key=lambda x: x["popularity_score"], reverse=True)
        popular_layers = scored_layers[:limit] if limit else scored_layers

        return {
            "data": popular_layers,
            "metadata": {
                "count": len(popular_layers),
                "total_layers": len(all_layers),
                "limit": limit,
                "source": "NASA GIBS",
                "last_updated": datetime.now().isoformat()
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching popular layers: {str(e)}"
        }

def search_layers(query: str, limit: int = 50) -> Dict[str, Any]:
    """
    Search for layers by keyword

    Args:
        query: Search query string
        limit: Maximum number of results to return

    Returns:
        JSON response with search results
    """
    try:
        # Get all layers
        layers_result = list_layers()

        if layers_result["error"]:
            return layers_result

        all_layers = layers_result.get("data", [])
        query_lower = query.lower()

        # Search layers
        matched_layers = []
        for layer in all_layers:
            name_match = query_lower in layer.get("name", "").lower()
            title_match = query_lower in layer.get("title", "").lower()
            desc_match = query_lower in layer.get("description", "").lower()

            if name_match or title_match or desc_match:
                layer["search_score"] = (3 if name_match else 0) + (2 if title_match else 0) + (1 if desc_match else 0)
                matched_layers.append(layer)

        # Sort by relevance score and limit results
        matched_layers.sort(key=lambda x: x["search_score"], reverse=True)
        results = matched_layers[:limit] if limit else matched_layers

        return {
            "data": results,
            "metadata": {
                "query": query,
                "count": len(results),
                "total_matches": len(matched_layers),
                "limit": limit,
                "source": "NASA GIBS",
                "last_updated": datetime.now().isoformat()
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching layers: {str(e)}"
        }

# --- 3. AUTO-TESTING FUNCTION ---
def test_all_endpoints() -> Dict[str, Any]:
    """
    Test all API endpoints individually to verify functionality

    Returns:
        JSON response with test results for all endpoints
    """
    try:
        test_results = []

        # Test 1: List layers
        try:
            result = list_layers()
            test_results.append({
                "endpoint": "list_layers",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "data_count": len(result.get("data", [])),
                "response_time": result.get("metadata", {}).get("last_updated")
            })
        except Exception as e:
            test_results.append({
                "endpoint": "list_layers",
                "status": "error",
                "error": str(e)
            })

        # Test 2: Get supported projections
        try:
            result = get_supported_projections()
            test_results.append({
                "endpoint": "get_supported_projections",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "data_count": len(result.get("data", []))
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_supported_projections",
                "status": "error",
                "error": str(e)
            })

        # Test 3: Search layers
        try:
            result = search_layers("MODIS", 5)
            test_results.append({
                "endpoint": "search_layers",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "data_count": len(result.get("data", []))
            })
        except Exception as e:
            test_results.append({
                "endpoint": "search_layers",
                "status": "error",
                "error": str(e)
            })

        # Test 4: Get popular layers
        try:
            result = get_popular_layers(5)
            test_results.append({
                "endpoint": "get_popular_layers",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "data_count": len(result.get("data", []))
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_popular_layers",
                "status": "error",
                "error": str(e)
            })

        # Test 5: Layer details (requires a layer from list_layers)
        try:
            layers_result = list_layers()
            if not layers_result["error"] and layers_result.get("data"):
                first_layer = layers_result["data"][0]
                layer_id = first_layer.get("id", "")
                if layer_id:
                    result = get_layer_details(layer_id)
                    test_results.append({
                        "endpoint": "get_layer_details",
                        "status": "passed" if not result["error"] else "failed",
                        "error": result.get("error"),
                        "test_layer_id": layer_id
                    })
                else:
                    test_results.append({
                        "endpoint": "get_layer_details",
                        "status": "skipped",
                        "error": "No layer ID available for testing"
                    })
            else:
                test_results.append({
                    "endpoint": "get_layer_details",
                    "status": "skipped",
                    "error": "Could not get layers for testing"
                })
        except Exception as e:
            test_results.append({
                "endpoint": "get_layer_details",
                "status": "error",
                "error": str(e)
            })

        # Test 6: Time periods (requires a layer)
        try:
            layers_result = list_layers()
            if not layers_result["error"] and layers_result.get("data"):
                first_layer = layers_result["data"][0]
                layer_id = first_layer.get("id", "")
                if layer_id:
                    result = get_layer_time_periods(layer_id, 10)
                    test_results.append({
                        "endpoint": "get_layer_time_periods",
                        "status": "passed" if not result["error"] else "failed",
                        "error": result.get("error"),
                        "test_layer_id": layer_id,
                        "data_count": len(result.get("data", []))
                    })
                else:
                    test_results.append({
                        "endpoint": "get_layer_time_periods",
                        "status": "skipped",
                        "error": "No layer ID available for testing"
                    })
            else:
                test_results.append({
                    "endpoint": "get_layer_time_periods",
                    "status": "skipped",
                    "error": "Could not get layers for testing"
                })
        except Exception as e:
            test_results.append({
                "endpoint": "get_layer_time_periods",
                "status": "error",
                "error": str(e)
            })

        # Test 7: Get tile (coordinates for a small area)
        try:
            # Test with coordinates for a small area (New York)
            result = get_tile(
                layer_id="MODIS_Terra_CorrectedReflectance_TrueColor",
                date="2024-01-01",
                zoom=6,
                lat=40.7128,  # New York latitude
                lon=-74.0060, # New York longitude
                format="png"
            )
            test_results.append({
                "endpoint": "get_tile",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "test_coords": "40.7128, -74.0060 (New York)"
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_tile",
                "status": "error",
                "error": str(e)
            })

        # Calculate summary
        passed_tests = sum(1 for test in test_results if test["status"] == "passed")
        failed_tests = sum(1 for test in test_results if test["status"] == "failed")
        error_tests = sum(1 for test in test_results if test["status"] == "error")
        skipped_tests = sum(1 for test in test_results if test["status"] == "skipped")

        return {
            "data": test_results,
            "metadata": {
                "total_tests": len(test_results),
                "passed": passed_tests,
                "failed": failed_tests,
                "errors": error_tests,
                "skipped": skipped_tests,
                "success_rate": f"{(passed_tests / len(test_results) * 100):.1f}%" if test_results else "0%",
                "source": "NASA GIBS API Wrapper",
                "test_time": datetime.now().isoformat()
            },
            "error": None if failed_tests == 0 and error_tests == 0 else f"Some tests failed: {failed_tests} failed, {error_tests} errors"
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error running tests: {str(e)}"
        }

# --- 4. CLI INTERFACE ---
def print_usage():
    """Print usage instructions."""
    print("NASA GIBS (Global Imagery Browse Services) API Wrapper")
    print("=" * 60)
    print()
    print("Usage Examples:")
    print("  # List all available imagery layers")
    print(f"  python {sys.argv[0]} list-layers")
    print()
    print("  # Search for MODIS layers")
    print(f"  python {sys.argv[0]} search-layers \"MODIS\"")
    print()
    print("  # Get details for a specific layer")
    print(f"  python {sys.argv[0]} layer-details \"MODIS_Terra_CorrectedReflectance_TrueColor\"")
    print()
    print("  # Get time periods for a layer")
    print(f"  python {sys.argv[0]} time-periods \"MODIS_Terra_CorrectedReflectance_TrueColor\"")
    print()
    print("  # Get a satellite image tile")
    print(f"  python {sys.argv[0]} get-tile \"MODIS_Terra_CorrectedReflectance_TrueColor\" \"2024-01-01\" 6 40.7128 -74.0060")
    print()
    print("  # Test all endpoints")
    print(f"  python {sys.argv[0]} test-all")
    print()
    print("Available Commands:")
    commands = [
        "list-layers", "layer-details <layer_id>", "search-layers <query>",
        "time-periods <layer_id>", "get-tile <layer_id> <date> <zoom> <lat> <lon>",
        "get-tile-batch <layer_id> <date> <zoom> <lat_min> <lon_min> <lat_max> <lon_max>",
        "popular-layers", "supported-projections", "test-all"
    ]
    for command in commands:
        print(f"  - {command}")
    print()

def main():
    """Main CLI function."""
    if len(sys.argv) < 2 or '--help' in sys.argv or '-h' in sys.argv:
        print_usage()
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "list-layers":
            result = list_layers()

        elif command == "layer-details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: layer-details <layer_id>"}))
                sys.exit(1)
            layer_id = sys.argv[2]
            result = get_layer_details(layer_id)

        elif command == "search-layers":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: search-layers <query>"}))
                sys.exit(1)
            query = sys.argv[2]
            result = search_layers(query)

        elif command == "time-periods":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: time-periods <layer_id>"}))
                sys.exit(1)
            layer_id = sys.argv[2]
            result = get_layer_time_periods(layer_id)

        elif command == "get-tile":
            if len(sys.argv) < 7:
                print(json.dumps({"error": "Usage: get-tile <layer_id> <date> <zoom> <lat> <lon>"}))
                sys.exit(1)
            layer_id = sys.argv[2]
            date = sys.argv[3]
            zoom = int(sys.argv[4])
            lat = float(sys.argv[5])
            lon = float(sys.argv[6])
            result = get_tile(layer_id, date, zoom, lat, lon)

        elif command == "get-tile-batch":
            if len(sys.argv) < 9:
                print(json.dumps({"error": "Usage: get-tile-batch <layer_id> <date> <zoom> <lat_min> <lon_min> <lat_max> <lon_max>"}))
                sys.exit(1)
            layer_id = sys.argv[2]
            date = sys.argv[3]
            zoom = int(sys.argv[4])
            lat_min = float(sys.argv[5])
            lon_min = float(sys.argv[6])
            lat_max = float(sys.argv[7])
            lon_max = float(sys.argv[8])
            result = get_tile_batch(layer_id, date, zoom, lat_min, lon_min, lat_max, lon_max)

        elif command == "popular-layers":
            result = get_popular_layers()

        elif command == "supported-projections":
            result = get_supported_projections()

        elif command == "test-all":
            result = test_all_endpoints()

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "list-layers", "layer-details <layer_id>", "search-layers <query>",
                    "time-periods <layer_id>", "get-tile <layer_id> <date> <zoom> <lat> <lon>",
                    "get-tile-batch <layer_id> <date> <zoom> <lat_min> <lon_min> <lat_max> <lon_max>",
                    "popular-layers", "supported-projections", "test-all"
                ]
            }

        # Output result as JSON
        print(json.dumps(result, indent=2, ensure_ascii=False))

        # Exit with error code if API call failed
        if result.get('error'):
            sys.exit(1)

    except Exception as e:
        error_result = {
            "data": [],
            "metadata": {},
            "error": f"CLI Error: {str(e)}",
            "command": command,
            "timestamp": datetime.now().isoformat()
        }
        print(json.dumps(error_result, indent=2))
        sys.exit(1)

if __name__ == "__main__":
    main()