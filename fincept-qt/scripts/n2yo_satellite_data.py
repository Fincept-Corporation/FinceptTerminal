#!/usr/bin/env python3
"""
N2YO Satellite Tracking API Wrapper
Complete coverage of all N2YO.com REST API v1 endpoints for satellite tracking and orbital data.

API Documentation: https://www.n2yo.com/api/

FEATURES:
1. TLE (Two Line Elements) - Orbital element data
2. Satellite Positions - Future ground-track coordinates and viewing angles
3. Visual Passes - Optically visible satellite passages
4. Radio Passes - Communication windows for satellite contact
5. Above (What's Up) - All satellites currently visible from observer location

RATE LIMITS (per 60 minutes):
- TLE: 1,000 transactions
- Positions: 1,000 transactions
- Visual passes: 100 transactions
- Radio passes: 100 transactions
- Above: 100 transactions

Usage: python n2yo_satellite_data.py <command> [args]
"""

import sys
import json
import os
import requests
from typing import Dict, List, Any, Optional
from datetime import datetime, timezone
import time

# --- 1. CONFIGURATION ---

# API Configuration
API_KEY = os.environ.get('N2YO_API_KEY', '')
BASE_URL = "https://api.n2yo.com/rest/v1/satellite"

# Request settings
TIMEOUT = 30
MAX_RETRIES = 3
CACHE_DURATION = 300  # 5 minutes cache for frequently accessed data

# Cache for API responses
_api_cache = {}

# Popular satellite categories (from N2YO documentation)
SATELLITE_CATEGORIES = {
    0: "All Satellites",
    1: "Brightest",
    2: "ISS (International Space Station)",
    3: "Weather",
    4: "NOAA",
    5: "GOES",
    6: "Earth Resources",
    7: "Search & Rescue (SARSAT)",
    8: "Disaster Monitoring",
    9: "Tracking and Data Relay (TDRSS)",
    10: "Geostationary",
    11: "Intelsat",
    12: "Gorizont",
    13: "Raduga",
    14: "Molniya",
    15: "Iridium",
    16: "Orbcomm",
    17: "Globalstar",
    18: "Amateur Radio",
    19: "Experimental",
    20: "GPS Operational",
    21: "Glonass Operational",
    22: "Galileo",
    23: "Satellite-Based Augmentation System (WAAS/EGNOS/MSAS)",
    24: "Navy Navigation Satellite System (NNSS)",
    25: "Russian LEO Navigation",
    26: "Space & Earth Science",
    27: "Geodetic",
    28: "Engineering",
    29: "Education",
    30: "Military",
    31: "Radar Calibration",
    32: "CubeSats",
    33: "XM and Sirius",
    34: "TV",
    35: "Beidou Navigation System",
    36: "Yaogan",
    37: "Westford Needles",
    38: "Parus",
    39: "Gonets",
    40: "QZSS",
    41: "Cubesat",
    42: "Tsiklon",
    43: "Strela",
    44: "Lemur",
    45: "Flock",
    46: "Spire",
    47: "IRIDIUM NEXT",
    48: "OneWeb",
    49: "O3B Networks",
    50: "Swarm",
    51: "Planet",
    52: "Starlink",
    53: "ISS (zarya)"
}

# Popular satellites (NORAD IDs)
POPULAR_SATELLITES = {
    25544: "International Space Station (ISS)",
    20580: "Hubble Space Telescope",
    43013: "Tiangong Space Station",
    27424: "GOES 13 Weather Satellite",
    33591: "NOAA 19 Weather Satellite",
    28654: "GPS BIIR-8",
    37849: "IRIDIUM 106",
    40730: "WORLDVIEW-3",
    25994: "TRMM (Tropical Rainfall Measuring Mission)"
}

def _sanitize_url(url: str) -> str:
    """
    Remove API key from URL for safe logging/output

    Args:
        url: URL that may contain API key

    Returns:
        Sanitized URL with API key removed
    """
    if 'apiKey=' in url:
        return url.split('apiKey=')[0].rstrip('?&')
    return url

def _make_request(endpoint: str, params: Optional[Dict[str, str]] = None,
                   use_cache: bool = True) -> Dict[str, Any]:
    """
    Centralized request handler for N2YO API with enhanced error handling

    Args:
        endpoint: API endpoint path (e.g., "tle/25544")
        params: Optional query parameters (API key is added automatically)
        use_cache: Whether to use cached responses

    Returns:
        Response data or error information
    """
    try:
        # Check if API key is configured
        if not API_KEY:
            return {
                "data": [],
                "metadata": {"source": "N2YO"},
                "error": "N2YO API key not configured. Set N2YO_API_KEY environment variable."
            }

        # Build full URL
        full_url = f"{BASE_URL}/{endpoint}"

        # Check cache
        cache_key = f"{endpoint}_{json.dumps(params, sort_keys=True)}"
        if use_cache and cache_key in _api_cache:
            cached_data = _api_cache[cache_key]
            if time.time() - cached_data['timestamp'] < CACHE_DURATION:
                cached_data['response']['metadata']['cached'] = True
                return cached_data['response']

        # Setup headers and parameters
        headers = {
            'User-Agent': 'Fincept-Terminal/1.0 (N2YO API Wrapper)',
            'Accept': 'application/json'
        }

        if params is None:
            params = {}

        # Add API key to parameters
        params['apiKey'] = API_KEY

        # Make request with retries
        for attempt in range(MAX_RETRIES):
            try:
                response = requests.get(full_url, headers=headers, params=params, timeout=TIMEOUT)
                response.raise_for_status()

                data = response.json()

                # Check for API errors
                if 'error' in data:
                    return {
                        "data": [],
                        "metadata": {"source": "N2YO", "endpoint": endpoint},
                        "error": f"API Error: {data['error']}"
                    }

                result = {
                    "data": data,
                    "metadata": {
                        "source": "N2YO",
                        "endpoint": endpoint,
                        "timestamp": datetime.now(timezone.utc).isoformat(),
                        "cached": False,
                        "transactions_count": data.get('info', {}).get('transactionscount', 0)
                    },
                    "error": None
                }

                # Cache successful response
                if use_cache:
                    _api_cache[cache_key] = {
                        'response': result,
                        'timestamp': time.time()
                    }

                return result

            except requests.exceptions.Timeout:
                if attempt == MAX_RETRIES - 1:
                    return {
                        "data": [],
                        "metadata": {"source": "N2YO", "endpoint": endpoint},
                        "error": f"Request timeout after {TIMEOUT} seconds"
                    }
                continue

            except requests.exceptions.ConnectionError:
                if attempt == MAX_RETRIES - 1:
                    return {
                        "data": [],
                        "metadata": {"source": "N2YO", "endpoint": endpoint},
                        "error": "Connection error - unable to reach N2YO service"
                    }
                continue

            except requests.exceptions.HTTPError as e:
                error_details = f" - {e.response.reason}"
                if e.response.status_code == 404:
                    error_details += " (Resource not found - check satellite ID)"
                elif e.response.status_code == 400:
                    error_details += " (Bad request - check parameters)"
                elif e.response.status_code == 429:
                    error_details += " (Rate limit exceeded - check N2YO rate limits)"
                elif e.response.status_code == 500:
                    error_details += " (Server error - try again later)"

                return {
                    "data": [],
                    "metadata": {"source": "N2YO", "endpoint": endpoint},
                    "error": f"HTTP error: {e.response.status_code}{error_details}"
                }

    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {"source": "N2YO"},
            "error": "Failed to decode API response"
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {"source": "N2YO"},
            "error": f"Unexpected error: {str(e)}"
        }

def _validate_coordinates(lat: float, lng: float, alt: float) -> Dict[str, Any]:
    """
    Validate observer coordinates

    Args:
        lat: Latitude in decimal degrees
        lng: Longitude in decimal degrees
        alt: Altitude above sea level in meters

    Returns:
        Validation result
    """
    validation = {
        "is_valid": True,
        "errors": []
    }

    if not (-90 <= lat <= 90):
        validation["is_valid"] = False
        validation["errors"].append(f"Latitude {lat} out of range (-90 to 90)")

    if not (-180 <= lng <= 180):
        validation["is_valid"] = False
        validation["errors"].append(f"Longitude {lng} out of range (-180 to 180)")

    if alt < 0:
        validation["is_valid"] = False
        validation["errors"].append(f"Altitude {alt} cannot be negative")

    if alt > 8900:  # Approximate height of Mt. Everest
        validation["errors"].append(f"Warning: Altitude {alt}m is unusually high")

    return validation

def _parse_compass_direction(degrees: float) -> str:
    """Convert azimuth degrees to compass direction"""
    directions = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                  "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"]
    index = int((degrees + 11.25) / 22.5) % 16
    return directions[index]

# --- 2. CORE FUNCTIONS (GROUPED BY API CATEGORY) ---

# ====== TLE (TWO LINE ELEMENTS) ======
def get_tle(satellite_id: int) -> Dict[str, Any]:
    """
    Get TLE (Two Line Elements) orbital data for a satellite

    Args:
        satellite_id: NORAD catalog number

    Returns:
        JSON response with TLE data
    """
    try:
        result = _make_request(f"tle/{satellite_id}")

        if not result["error"] and result.get("data"):
            data = result["data"]

            # Parse TLE lines
            tle_string = data.get("tle", "")
            tle_lines = tle_string.split("\r\n") if tle_string else []

            enhanced_data = {
                "satellite_id": data.get("info", {}).get("satid"),
                "satellite_name": data.get("info", {}).get("satname"),
                "tle_raw": tle_string,
                "tle_line1": tle_lines[0] if len(tle_lines) > 0 else "",
                "tle_line2": tle_lines[1] if len(tle_lines) > 1 else "",
                "transactions_count": data.get("info", {}).get("transactionscount")
            }

            result["data"] = enhanced_data
            result["metadata"]["satellite_id"] = satellite_id

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"satellite_id": satellite_id},
            "error": f"Error fetching TLE: {str(e)}"
        }

# ====== SATELLITE POSITIONS ======
def get_positions(satellite_id: int, observer_lat: float, observer_lng: float,
                  observer_alt: float, seconds: int) -> Dict[str, Any]:
    """
    Get future satellite positions and viewing angles from observer location

    Args:
        satellite_id: NORAD catalog number
        observer_lat: Observer latitude in decimal degrees
        observer_lng: Observer longitude in decimal degrees
        observer_alt: Observer altitude above sea level in meters
        seconds: Duration in seconds (max 300)

    Returns:
        JSON response with position predictions
    """
    try:
        # Validate inputs
        coord_validation = _validate_coordinates(observer_lat, observer_lng, observer_alt)
        if not coord_validation["is_valid"]:
            return {
                "data": [],
                "metadata": {},
                "error": f"Invalid coordinates: {', '.join(coord_validation['errors'])}"
            }

        if seconds <= 0 or seconds > 300:
            return {
                "data": [],
                "metadata": {},
                "error": f"Duration must be between 1 and 300 seconds (got {seconds})"
            }

        endpoint = f"positions/{satellite_id}/{observer_lat}/{observer_lng}/{observer_alt}/{seconds}"
        result = _make_request(endpoint)

        if not result["error"] and result.get("data"):
            data = result["data"]

            # Enhance position data
            positions = data.get("positions", [])
            enhanced_positions = []

            for pos in positions:
                enhanced_pos = {
                    "timestamp": pos.get("timestamp"),
                    "datetime": datetime.fromtimestamp(pos.get("timestamp", 0), tz=timezone.utc).isoformat(),
                    "satellite_latitude": pos.get("satlatitude"),
                    "satellite_longitude": pos.get("satlongitude"),
                    "satellite_altitude_km": pos.get("sataltitude"),
                    "azimuth": pos.get("azimuth"),
                    "azimuth_compass": _parse_compass_direction(pos.get("azimuth", 0)),
                    "elevation": pos.get("elevation"),
                    "right_ascension": pos.get("ra"),
                    "declination": pos.get("dec"),
                    "is_visible": pos.get("elevation", 0) > 0,
                    "is_sunlit": pos.get("eclipsed", 1) == 0 if "eclipsed" in pos else None
                }
                enhanced_positions.append(enhanced_pos)

            enhanced_data = {
                "satellite_id": data.get("info", {}).get("satid"),
                "satellite_name": data.get("info", {}).get("satname"),
                "observer_location": {
                    "latitude": observer_lat,
                    "longitude": observer_lng,
                    "altitude_m": observer_alt
                },
                "duration_seconds": seconds,
                "positions_count": len(enhanced_positions),
                "positions": enhanced_positions,
                "transactions_count": data.get("info", {}).get("transactionscount")
            }

            result["data"] = enhanced_data
            result["metadata"]["satellite_id"] = satellite_id
            result["metadata"]["observer_location"] = f"{observer_lat},{observer_lng}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"satellite_id": satellite_id},
            "error": f"Error fetching positions: {str(e)}"
        }

# ====== VISUAL PASSES ======
def get_visual_passes(satellite_id: int, observer_lat: float, observer_lng: float,
                      observer_alt: float, days: int, min_visibility: int = 0) -> Dict[str, Any]:
    """
    Get predicted optically visible satellite passes

    Args:
        satellite_id: NORAD catalog number
        observer_lat: Observer latitude in decimal degrees
        observer_lng: Observer longitude in decimal degrees
        observer_alt: Observer altitude above sea level in meters
        days: Prediction window in days (max 10)
        min_visibility: Minimum visible duration in seconds

    Returns:
        JSON response with visual pass predictions
    """
    try:
        # Validate inputs
        coord_validation = _validate_coordinates(observer_lat, observer_lng, observer_alt)
        if not coord_validation["is_valid"]:
            return {
                "data": [],
                "metadata": {},
                "error": f"Invalid coordinates: {', '.join(coord_validation['errors'])}"
            }

        if days <= 0 or days > 10:
            return {
                "data": [],
                "metadata": {},
                "error": f"Days must be between 1 and 10 (got {days})"
            }

        endpoint = f"visualpasses/{satellite_id}/{observer_lat}/{observer_lng}/{observer_alt}/{days}/{min_visibility}"
        result = _make_request(endpoint)

        if not result["error"] and result.get("data"):
            data = result["data"]

            # Enhance pass data
            passes = data.get("passes", [])
            enhanced_passes = []

            for pass_data in passes:
                enhanced_pass = {
                    "start": {
                        "timestamp": pass_data.get("startUTC"),
                        "datetime": datetime.fromtimestamp(pass_data.get("startUTC", 0), tz=timezone.utc).isoformat(),
                        "azimuth": pass_data.get("startAz"),
                        "azimuth_compass": pass_data.get("startAzCompass"),
                        "elevation": pass_data.get("startEl", 0)
                    },
                    "max": {
                        "timestamp": pass_data.get("maxUTC"),
                        "datetime": datetime.fromtimestamp(pass_data.get("maxUTC", 0), tz=timezone.utc).isoformat(),
                        "azimuth": pass_data.get("maxAz"),
                        "azimuth_compass": pass_data.get("maxAzCompass"),
                        "elevation": pass_data.get("maxEl")
                    },
                    "end": {
                        "timestamp": pass_data.get("endUTC"),
                        "datetime": datetime.fromtimestamp(pass_data.get("endUTC", 0), tz=timezone.utc).isoformat(),
                        "azimuth": pass_data.get("endAz"),
                        "azimuth_compass": pass_data.get("endAzCompass"),
                        "elevation": pass_data.get("endEl", 0)
                    },
                    "duration_seconds": pass_data.get("duration"),
                    "magnitude": pass_data.get("mag"),
                    "brightness_description": _get_magnitude_description(pass_data.get("mag", 0))
                }
                enhanced_passes.append(enhanced_pass)

            enhanced_data = {
                "satellite_id": data.get("info", {}).get("satid"),
                "satellite_name": data.get("info", {}).get("satname"),
                "observer_location": {
                    "latitude": observer_lat,
                    "longitude": observer_lng,
                    "altitude_m": observer_alt
                },
                "prediction_days": days,
                "min_visibility_seconds": min_visibility,
                "passes_count": len(enhanced_passes),
                "passes": enhanced_passes,
                "transactions_count": data.get("info", {}).get("transactionscount")
            }

            result["data"] = enhanced_data
            result["metadata"]["satellite_id"] = satellite_id
            result["metadata"]["prediction_days"] = days

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"satellite_id": satellite_id},
            "error": f"Error fetching visual passes: {str(e)}"
        }

def _get_magnitude_description(magnitude: float) -> str:
    """Convert visual magnitude to brightness description"""
    if magnitude < -4:
        return "Extremely bright (brighter than Venus)"
    elif magnitude < -2:
        return "Very bright (similar to Venus)"
    elif magnitude < 0:
        return "Bright (easily visible)"
    elif magnitude < 2:
        return "Moderate (visible to naked eye)"
    elif magnitude < 4:
        return "Faint (visible in dark skies)"
    else:
        return "Very faint (binoculars recommended)"

# ====== RADIO PASSES ======
def get_radio_passes(satellite_id: int, observer_lat: float, observer_lng: float,
                     observer_alt: float, days: int, min_elevation: int = 0) -> Dict[str, Any]:
    """
    Get predicted radio communication windows

    Args:
        satellite_id: NORAD catalog number
        observer_lat: Observer latitude in decimal degrees
        observer_lng: Observer longitude in decimal degrees
        observer_alt: Observer altitude above sea level in meters
        days: Prediction window in days (max 10)
        min_elevation: Minimum elevation threshold in degrees

    Returns:
        JSON response with radio pass predictions
    """
    try:
        # Validate inputs
        coord_validation = _validate_coordinates(observer_lat, observer_lng, observer_alt)
        if not coord_validation["is_valid"]:
            return {
                "data": [],
                "metadata": {},
                "error": f"Invalid coordinates: {', '.join(coord_validation['errors'])}"
            }

        if days <= 0 or days > 10:
            return {
                "data": [],
                "metadata": {},
                "error": f"Days must be between 1 and 10 (got {days})"
            }

        if min_elevation < 0 or min_elevation > 90:
            return {
                "data": [],
                "metadata": {},
                "error": f"Minimum elevation must be between 0 and 90 degrees (got {min_elevation})"
            }

        endpoint = f"radiopasses/{satellite_id}/{observer_lat}/{observer_lng}/{observer_alt}/{days}/{min_elevation}"
        result = _make_request(endpoint)

        if not result["error"] and result.get("data"):
            data = result["data"]

            # Enhance pass data
            passes = data.get("passes", [])
            enhanced_passes = []

            for pass_data in passes:
                enhanced_pass = {
                    "start": {
                        "timestamp": pass_data.get("startUTC"),
                        "datetime": datetime.fromtimestamp(pass_data.get("startUTC", 0), tz=timezone.utc).isoformat(),
                        "azimuth": pass_data.get("startAz"),
                        "azimuth_compass": pass_data.get("startAzCompass")
                    },
                    "max": {
                        "timestamp": pass_data.get("maxUTC"),
                        "datetime": datetime.fromtimestamp(pass_data.get("maxUTC", 0), tz=timezone.utc).isoformat(),
                        "azimuth": pass_data.get("maxAz"),
                        "azimuth_compass": pass_data.get("maxAzCompass"),
                        "elevation": pass_data.get("maxEl")
                    },
                    "end": {
                        "timestamp": pass_data.get("endUTC"),
                        "datetime": datetime.fromtimestamp(pass_data.get("endUTC", 0), tz=timezone.utc).isoformat(),
                        "azimuth": pass_data.get("endAz"),
                        "azimuth_compass": pass_data.get("endAzCompass")
                    }
                }
                enhanced_passes.append(enhanced_pass)

            enhanced_data = {
                "satellite_id": data.get("info", {}).get("satid"),
                "satellite_name": data.get("info", {}).get("satname"),
                "observer_location": {
                    "latitude": observer_lat,
                    "longitude": observer_lng,
                    "altitude_m": observer_alt
                },
                "prediction_days": days,
                "min_elevation_degrees": min_elevation,
                "passes_count": len(enhanced_passes),
                "passes": enhanced_passes,
                "transactions_count": data.get("info", {}).get("transactionscount")
            }

            result["data"] = enhanced_data
            result["metadata"]["satellite_id"] = satellite_id
            result["metadata"]["prediction_days"] = days

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {"satellite_id": satellite_id},
            "error": f"Error fetching radio passes: {str(e)}"
        }

# ====== ABOVE (WHAT'S UP) ======
def get_above(observer_lat: float, observer_lng: float, observer_alt: float,
              search_radius: int, category_id: int = 0) -> Dict[str, Any]:
    """
    Get all satellites currently above observer location

    Args:
        observer_lat: Observer latitude in decimal degrees
        observer_lng: Observer longitude in decimal degrees
        observer_alt: Observer altitude above sea level in meters
        search_radius: Sky search radius in degrees (0-90)
        category_id: Satellite category ID (0 for all)

    Returns:
        JSON response with satellites currently overhead
    """
    try:
        # Validate inputs
        coord_validation = _validate_coordinates(observer_lat, observer_lng, observer_alt)
        if not coord_validation["is_valid"]:
            return {
                "data": [],
                "metadata": {},
                "error": f"Invalid coordinates: {', '.join(coord_validation['errors'])}"
            }

        if search_radius < 0 or search_radius > 90:
            return {
                "data": [],
                "metadata": {},
                "error": f"Search radius must be between 0 and 90 degrees (got {search_radius})"
            }

        endpoint = f"above/{observer_lat}/{observer_lng}/{observer_alt}/{search_radius}/{category_id}"
        result = _make_request(endpoint, use_cache=False)  # Don't cache - real-time data

        if not result["error"] and result.get("data"):
            data = result["data"]

            # Enhance satellite data
            satellites = data.get("above", [])
            enhanced_satellites = []

            for sat in satellites:
                enhanced_sat = {
                    "satellite_id": sat.get("satid"),
                    "satellite_name": sat.get("satname"),
                    "international_designator": sat.get("intDesignator"),
                    "launch_date": sat.get("launchDate"),
                    "footprint": {
                        "latitude": sat.get("satlat"),
                        "longitude": sat.get("satlng"),
                        "altitude_km": sat.get("satalt")
                    }
                }
                enhanced_satellites.append(enhanced_sat)

            enhanced_data = {
                "observer_location": {
                    "latitude": observer_lat,
                    "longitude": observer_lng,
                    "altitude_m": observer_alt
                },
                "search_radius_degrees": search_radius,
                "category_id": category_id,
                "category_name": SATELLITE_CATEGORIES.get(category_id, "Unknown"),
                "satellites_count": len(enhanced_satellites),
                "satellites": enhanced_satellites,
                "transactions_count": data.get("info", {}).get("transactionscount")
            }

            result["data"] = enhanced_data
            result["metadata"]["observer_location"] = f"{observer_lat},{observer_lng}"
            result["metadata"]["search_radius"] = search_radius

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching satellites above: {str(e)}"
        }

# ====== UTILITY FUNCTIONS ======
def list_categories() -> Dict[str, Any]:
    """
    List all available satellite categories

    Returns:
        JSON response with category list
    """
    try:
        categories = []
        for cat_id, cat_name in SATELLITE_CATEGORIES.items():
            categories.append({
                "id": cat_id,
                "name": cat_name
            })

        return {
            "data": categories,
            "metadata": {
                "source": "N2YO",
                "count": len(categories)
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error listing categories: {str(e)}"
        }

def list_popular_satellites() -> Dict[str, Any]:
    """
    List popular satellites with their NORAD IDs

    Returns:
        JSON response with popular satellite list
    """
    try:
        satellites = []
        for sat_id, sat_name in POPULAR_SATELLITES.items():
            satellites.append({
                "id": sat_id,
                "name": sat_name
            })

        return {
            "data": satellites,
            "metadata": {
                "source": "N2YO",
                "count": len(satellites)
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error listing popular satellites: {str(e)}"
        }

# --- 3. AUTO-TESTING FUNCTION ---
def test_all_endpoints() -> Dict[str, Any]:
    """
    Test all API endpoints to verify functionality

    Returns:
        JSON response with test results
    """
    try:
        test_results = []

        # Test coordinates (New York City)
        test_lat, test_lng, test_alt = 40.7128, -74.0060, 10
        test_sat_id = 25544  # ISS

        # Test 1: Get TLE
        try:
            result = get_tle(test_sat_id)
            test_results.append({
                "endpoint": "get_tle",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "test_satellite": "ISS (25544)"
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_tle",
                "status": "error",
                "error": str(e)
            })

        # Test 2: Get Positions
        try:
            result = get_positions(test_sat_id, test_lat, test_lng, test_alt, 60)
            test_results.append({
                "endpoint": "get_positions",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "positions_count": result.get("data", {}).get("positions_count", 0)
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_positions",
                "status": "error",
                "error": str(e)
            })

        # Test 3: Get Visual Passes
        try:
            result = get_visual_passes(test_sat_id, test_lat, test_lng, test_alt, 1)
            test_results.append({
                "endpoint": "get_visual_passes",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "passes_count": result.get("data", {}).get("passes_count", 0)
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_visual_passes",
                "status": "error",
                "error": str(e)
            })

        # Test 4: Get Radio Passes
        try:
            result = get_radio_passes(test_sat_id, test_lat, test_lng, test_alt, 1)
            test_results.append({
                "endpoint": "get_radio_passes",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "passes_count": result.get("data", {}).get("passes_count", 0)
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_radio_passes",
                "status": "error",
                "error": str(e)
            })

        # Test 5: Get Above
        try:
            result = get_above(test_lat, test_lng, test_alt, 90, 0)
            test_results.append({
                "endpoint": "get_above",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "satellites_count": result.get("data", {}).get("satellites_count", 0)
            })
        except Exception as e:
            test_results.append({
                "endpoint": "get_above",
                "status": "error",
                "error": str(e)
            })

        # Test 6: List Categories
        try:
            result = list_categories()
            test_results.append({
                "endpoint": "list_categories",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "categories_count": len(result.get("data", []))
            })
        except Exception as e:
            test_results.append({
                "endpoint": "list_categories",
                "status": "error",
                "error": str(e)
            })

        # Test 7: List Popular Satellites
        try:
            result = list_popular_satellites()
            test_results.append({
                "endpoint": "list_popular_satellites",
                "status": "passed" if not result["error"] else "failed",
                "error": result.get("error"),
                "satellites_count": len(result.get("data", []))
            })
        except Exception as e:
            test_results.append({
                "endpoint": "list_popular_satellites",
                "status": "error",
                "error": str(e)
            })

        # Calculate summary
        passed_tests = sum(1 for test in test_results if test["status"] == "passed")
        failed_tests = sum(1 for test in test_results if test["status"] == "failed")
        error_tests = sum(1 for test in test_results if test["status"] == "error")

        return {
            "data": test_results,
            "metadata": {
                "total_tests": len(test_results),
                "passed": passed_tests,
                "failed": failed_tests,
                "errors": error_tests,
                "success_rate": f"{(passed_tests / len(test_results) * 100):.1f}%" if test_results else "0%",
                "source": "N2YO API Wrapper",
                "test_timestamp": datetime.now(timezone.utc).isoformat()
            },
            "error": None if (failed_tests == 0 and error_tests == 0) else f"Some tests failed: {failed_tests} failed, {error_tests} errors"
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error running tests: {str(e)}"
        }

# --- 4. CLI INTERFACE ---
def print_usage():
    """Print usage instructions"""
    print("N2YO Satellite Tracking API Wrapper")
    print("=" * 60)
    print()
    print("Usage Examples:")
    print("  # Get TLE for ISS")
    print(f"  python {sys.argv[0]} tle 25544")
    print()
    print("  # Get satellite positions for next 60 seconds")
    print(f"  python {sys.argv[0]} positions 25544 40.7128 -74.0060 10 60")
    print()
    print("  # Get visual passes for next 5 days")
    print(f"  python {sys.argv[0]} visual-passes 25544 40.7128 -74.0060 10 5 0")
    print()
    print("  # Get radio passes for next 3 days")
    print(f"  python {sys.argv[0]} radio-passes 25544 40.7128 -74.0060 10 3 10")
    print()
    print("  # Get satellites above location (90° radius, all categories)")
    print(f"  python {sys.argv[0]} above 40.7128 -74.0060 10 90 0")
    print()
    print("  # List satellite categories")
    print(f"  python {sys.argv[0]} categories")
    print()
    print("  # List popular satellites")
    print(f"  python {sys.argv[0]} popular")
    print()
    print("  # Test all endpoints")
    print(f"  python {sys.argv[0]} test-all")
    print()
    print("Available Commands:")
    commands = [
        "tle <satellite_id>",
        "positions <satellite_id> <lat> <lng> <alt> <seconds>",
        "visual-passes <satellite_id> <lat> <lng> <alt> <days> <min_visibility>",
        "radio-passes <satellite_id> <lat> <lng> <alt> <days> <min_elevation>",
        "above <lat> <lng> <alt> <radius> <category_id>",
        "categories",
        "popular",
        "test-all"
    ]
    for command in commands:
        print(f"  - {command}")
    print()
    print("Environment Variables:")
    print("  N2YO_API_KEY - Your N2YO API key (required)")
    print()

def main():
    """Main CLI function"""
    if len(sys.argv) < 2 or '--help' in sys.argv or '-h' in sys.argv:
        print_usage()
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "tle":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: tle <satellite_id>"}))
                sys.exit(1)
            satellite_id = int(sys.argv[2])
            result = get_tle(satellite_id)

        elif command == "positions":
            if len(sys.argv) < 7:
                print(json.dumps({"error": "Usage: positions <satellite_id> <lat> <lng> <alt> <seconds>"}))
                sys.exit(1)
            satellite_id = int(sys.argv[2])
            lat = float(sys.argv[3])
            lng = float(sys.argv[4])
            alt = float(sys.argv[5])
            seconds = int(sys.argv[6])
            result = get_positions(satellite_id, lat, lng, alt, seconds)

        elif command == "visual-passes":
            if len(sys.argv) < 8:
                print(json.dumps({"error": "Usage: visual-passes <satellite_id> <lat> <lng> <alt> <days> <min_visibility>"}))
                sys.exit(1)
            satellite_id = int(sys.argv[2])
            lat = float(sys.argv[3])
            lng = float(sys.argv[4])
            alt = float(sys.argv[5])
            days = int(sys.argv[6])
            min_visibility = int(sys.argv[7])
            result = get_visual_passes(satellite_id, lat, lng, alt, days, min_visibility)

        elif command == "radio-passes":
            if len(sys.argv) < 8:
                print(json.dumps({"error": "Usage: radio-passes <satellite_id> <lat> <lng> <alt> <days> <min_elevation>"}))
                sys.exit(1)
            satellite_id = int(sys.argv[2])
            lat = float(sys.argv[3])
            lng = float(sys.argv[4])
            alt = float(sys.argv[5])
            days = int(sys.argv[6])
            min_elevation = int(sys.argv[7])
            result = get_radio_passes(satellite_id, lat, lng, alt, days, min_elevation)

        elif command == "above":
            if len(sys.argv) < 7:
                print(json.dumps({"error": "Usage: above <lat> <lng> <alt> <radius> <category_id>"}))
                sys.exit(1)
            lat = float(sys.argv[2])
            lng = float(sys.argv[3])
            alt = float(sys.argv[4])
            radius = int(sys.argv[5])
            category_id = int(sys.argv[6])
            result = get_above(lat, lng, alt, radius, category_id)

        elif command == "categories":
            result = list_categories()

        elif command == "popular":
            result = list_popular_satellites()

        elif command == "test-all":
            result = test_all_endpoints()

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "tle", "positions", "visual-passes", "radio-passes",
                    "above", "categories", "popular", "test-all"
                ]
            }

        # Output result as JSON
        print(json.dumps(result, indent=2, ensure_ascii=False))

        # Exit with error code if API call failed
        if result.get('error'):
            sys.exit(1)

    except ValueError as e:
        error_result = {
            "data": [],
            "metadata": {},
            "error": f"Invalid parameter value: {str(e)}",
            "command": command
        }
        print(json.dumps(error_result, indent=2))
        sys.exit(1)
    except Exception as e:
        error_result = {
            "data": [],
            "metadata": {},
            "error": f"CLI Error: {str(e)}",
            "command": command,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }
        print(json.dumps(error_result, indent=2))
        sys.exit(1)

if __name__ == "__main__":
    main()
