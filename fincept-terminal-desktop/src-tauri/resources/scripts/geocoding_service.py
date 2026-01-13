"""
Geocoding Service using geopy
Provides location search, autocomplete, and reverse geocoding
Returns JSON output for Rust integration

Uses Nominatim (OpenStreetMap) - no API key required
Rate Limit: 1 request per second (handled with delays)
"""

import sys
import json
from typing import Dict, List, Optional, Any
from geopy.geocoders import Nominatim
from geopy.extra.rate_limiter import RateLimiter
import time

# Initialize geocoder with user agent
geolocator = Nominatim(user_agent="fincept-terminal/3.0")
# Rate limiter: 1 call per second
geocode = RateLimiter(geolocator.geocode, min_delay_seconds=1)
reverse = RateLimiter(geolocator.reverse, min_delay_seconds=1)

def search_location(query: str, limit: int = 5) -> Dict[str, Any]:
    """Search for locations matching query with autocomplete suggestions"""
    try:
        # Get multiple results for autocomplete
        results = geolocator.geocode(
            query,
            exactly_one=False,
            limit=limit,
            addressdetails=True,
            language='en'
        )

        if not results:
            return {
                "success": False,
                "error": "No locations found",
                "suggestions": []
            }

        suggestions = []
        for location in results:
            # Extract bounding box from raw data if available
            bbox = location.raw.get('boundingbox', [])

            suggestion = {
                "name": location.address,
                "latitude": location.latitude,
                "longitude": location.longitude,
                "bbox": {
                    "min_lat": float(bbox[0]) if len(bbox) >= 4 else location.latitude - 0.1,
                    "max_lat": float(bbox[1]) if len(bbox) >= 4 else location.latitude + 0.1,
                    "min_lng": float(bbox[2]) if len(bbox) >= 4 else location.longitude - 0.1,
                    "max_lng": float(bbox[3]) if len(bbox) >= 4 else location.longitude + 0.1
                },
                "type": location.raw.get('type', 'unknown'),
                "importance": location.raw.get('importance', 0)
            }
            suggestions.append(suggestion)

        return {
            "success": True,
            "query": query,
            "count": len(suggestions),
            "suggestions": suggestions
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "suggestions": []
        }

def reverse_geocode(lat: float, lng: float) -> Dict[str, Any]:
    """Reverse geocode coordinates to location name"""
    try:
        location = geolocator.reverse(f"{lat}, {lng}", exactly_one=True)

        if not location:
            return {
                "success": False,
                "error": "No location found for coordinates"
            }

        bbox = location.raw.get('boundingbox', [])

        return {
            "success": True,
            "name": location.address,
            "latitude": location.latitude,
            "longitude": location.longitude,
            "bbox": {
                "min_lat": float(bbox[0]) if len(bbox) >= 4 else lat - 0.1,
                "max_lat": float(bbox[1]) if len(bbox) >= 4 else lat + 0.1,
                "min_lng": float(bbox[2]) if len(bbox) >= 4 else lng - 0.1,
                "max_lng": float(bbox[3]) if len(bbox) >= 4 else lng + 0.1
            },
            "type": location.raw.get('type', 'unknown')
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }

def main():
    """Main entry point for CLI usage"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python geocoding_service.py <command> [args...]"
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "search":
            if len(sys.argv) < 3:
                result = {"success": False, "error": "Missing query parameter"}
            else:
                query = sys.argv[2]
                limit = int(sys.argv[3]) if len(sys.argv) > 3 else 5
                result = search_location(query, limit)

        elif command == "reverse":
            if len(sys.argv) < 4:
                result = {"success": False, "error": "Missing lat/lng parameters"}
            else:
                lat = float(sys.argv[2])
                lng = float(sys.argv[3])
                result = reverse_geocode(lat, lng)

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)

if __name__ == "__main__":
    main()
