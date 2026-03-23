"""
News Geolocation — Extract locations from headlines and geocode them.
Also queries nearby infrastructure via OpenStreetMap Overpass API.

Commands:
  extract_and_geocode <json_articles>
  nearby_infrastructure <lat> <lon> <radius_km>
"""
import sys
import json
import re
import time

# ── Location extraction patterns ─────────────────────────────────────────────

# Major cities with known coordinates (avoids geocoding API for common cases)
CITY_COORDS = {
    "new york": (40.7128, -74.0060), "washington": (38.9072, -77.0369),
    "london": (51.5074, -0.1278), "paris": (48.8566, 2.3522),
    "berlin": (52.5200, 13.4050), "moscow": (55.7558, 37.6173),
    "beijing": (39.9042, 116.4074), "tokyo": (35.6762, 139.6503),
    "mumbai": (19.0760, 72.8777), "delhi": (28.6139, 77.2090),
    "shanghai": (31.2304, 121.4737), "hong kong": (22.3193, 114.1694),
    "singapore": (1.3521, 103.8198), "dubai": (25.2048, 55.2708),
    "sydney": (-33.8688, 151.2093), "toronto": (43.6532, -79.3832),
    "seoul": (37.5665, 126.9780), "taipei": (25.0330, 121.5654),
    "istanbul": (41.0082, 28.9784), "cairo": (30.0444, 31.2357),
    "tel aviv": (32.0853, 34.7818), "jerusalem": (31.7683, 35.2137),
    "riyadh": (24.7136, 46.6753), "tehran": (35.6892, 51.3890),
    "baghdad": (33.3152, 44.3661), "damascus": (33.5138, 36.2765),
    "kabul": (34.5553, 69.2075), "kyiv": (50.4501, 30.5234),
    "kiev": (50.4501, 30.5234), "brussels": (50.8503, 4.3517),
    "geneva": (46.2044, 6.1432), "zurich": (47.3769, 8.5417),
    "rome": (41.9028, 12.4964), "madrid": (40.4168, -3.7038),
    "lisbon": (38.7223, -9.1393), "amsterdam": (52.3676, 4.9041),
    "stockholm": (59.3293, 18.0686), "oslo": (59.9139, 10.7522),
    "warsaw": (52.2297, 21.0122), "vienna": (48.2082, 16.3738),
    "athens": (37.9838, 23.7275), "ankara": (39.9334, 32.8597),
    "buenos aires": (-34.6037, -58.3816), "sao paulo": (-23.5505, -46.6333),
    "mexico city": (19.4326, -99.1332), "bogota": (4.7110, -74.0721),
    "lagos": (6.5244, 3.3792), "nairobi": (-1.2921, 36.8219),
    "johannesburg": (-26.2041, 28.0473), "addis ababa": (9.0192, 38.7525),
    "khartoum": (15.5007, 32.5599), "beirut": (33.8938, 35.5018),
    "gaza": (31.5, 34.47), "ramallah": (31.9038, 35.2034),
    "pyongyang": (39.0392, 125.7625), "hanoi": (21.0285, 105.8542),
    "bangkok": (13.7563, 100.5018), "jakarta": (-6.2088, 106.8456),
    "manila": (14.5995, 120.9842), "kuala lumpur": (3.1390, 101.6869),
}

# Country centroids (for country-level mentions)
COUNTRY_COORDS = {
    "US": (38.0, -97.0), "CN": (35.0, 105.0), "RU": (60.0, 100.0),
    "UA": (49.0, 32.0), "GB": (54.0, -2.0), "DE": (51.0, 9.0),
    "FR": (46.0, 2.0), "JP": (36.0, 138.0), "IN": (20.0, 77.0),
    "BR": (-14.0, -51.0), "CA": (56.0, -106.0), "AU": (-25.0, 133.0),
    "KR": (36.0, 128.0), "KP": (40.0, 127.0), "IR": (32.0, 53.0),
    "IL": (31.0, 34.8), "PS": (31.5, 34.5), "SA": (24.0, 45.0),
    "TR": (39.0, 35.0), "EG": (27.0, 30.0), "IQ": (33.0, 44.0),
    "SY": (35.0, 38.0), "AF": (33.0, 65.0), "YE": (15.0, 48.0),
    "LY": (27.0, 17.0), "LB": (34.0, 36.0), "SD": (13.0, 30.0),
    "SO": (6.0, 46.0), "MM": (22.0, 98.0), "ET": (9.0, 40.0),
    "VE": (7.0, -66.0), "CU": (22.0, -80.0),
}

# Country name to code mapping (subset for extraction)
LOCATION_PATTERNS = {
    "united states": "US", "china": "CN", "russia": "RU", "ukraine": "UA",
    "united kingdom": "GB", "germany": "DE", "france": "FR", "japan": "JP",
    "india": "IN", "brazil": "BR", "iran": "IR", "israel": "IL",
    "palestine": "PS", "gaza": "PS", "saudi arabia": "SA", "turkey": "TR",
    "egypt": "EG", "iraq": "IQ", "syria": "SY", "afghanistan": "AF",
    "yemen": "YE", "libya": "LY", "lebanon": "LB", "sudan": "SD",
    "somalia": "SO", "myanmar": "MM", "north korea": "KP", "south korea": "KR",
    "taiwan": "TW", "hong kong": "HK", "singapore": "SG",
}


def extract_and_geocode(articles_json):
    """Extract locations from article text and return coordinates."""
    try:
        articles = json.loads(articles_json) if isinstance(articles_json, str) else articles_json
    except json.JSONDecodeError:
        return {"success": False, "error": "Invalid JSON"}

    results = []
    all_locations = {}

    for article in articles:
        text = (article.get("headline", "") + " " + article.get("summary", "")).lower()
        locations = []

        # Check cities first (more specific)
        for city, (lat, lon) in CITY_COORDS.items():
            if city in text:
                loc = {"name": city.title(), "lat": lat, "lon": lon, "type": "city"}
                locations.append(loc)
                all_locations[city] = loc

        # Check countries
        for pattern, code in LOCATION_PATTERNS.items():
            if pattern in text and code in COUNTRY_COORDS:
                lat, lon = COUNTRY_COORDS[code]
                if not any(l["name"].lower() == pattern for l in locations):
                    loc = {"name": pattern.title(), "code": code, "lat": lat, "lon": lon, "type": "country"}
                    locations.append(loc)
                    all_locations[pattern] = loc

        if locations:
            results.append({
                "id": article.get("id", ""),
                "locations": locations[:5],
                "primary_lat": locations[0]["lat"],
                "primary_lon": locations[0]["lon"],
            })

    return {
        "success": True,
        "geolocated_articles": results,
        "unique_locations": list(all_locations.values()),
        "coverage": f"{len(results)}/{len(articles)} articles geolocated",
    }


def nearby_infrastructure(lat, lon, radius_km):
    """Query OpenStreetMap Overpass API for nearby infrastructure."""
    try:
        lat = float(lat)
        lon = float(lon)
        radius_km = int(radius_km)
    except (ValueError, TypeError):
        return {"success": False, "error": "Invalid coordinates or radius"}

    radius_m = radius_km * 1000

    # Overpass query for airports, military bases, power plants, ports
    query = f"""
    [out:json][timeout:10];
    (
      node["aeroway"="aerodrome"](around:{radius_m},{lat},{lon});
      node["military"](around:{radius_m},{lat},{lon});
      node["power"="plant"](around:{radius_m},{lat},{lon});
      node["harbour"](around:{radius_m},{lat},{lon});
      way["aeroway"="aerodrome"](around:{radius_m},{lat},{lon});
      way["military"](around:{radius_m},{lat},{lon});
    );
    out center 20;
    """

    try:
        import urllib.request
        import urllib.parse

        url = "https://overpass-api.de/api/interpreter"
        data = urllib.parse.urlencode({"data": query}).encode()
        req = urllib.request.Request(url, data=data)
        req.add_header("User-Agent", "FinceptTerminal/4.0")

        with urllib.request.urlopen(req, timeout=15) as resp:
            result = json.loads(resp.read().decode())

        infrastructure = []
        for elem in result.get("elements", []):
            tags = elem.get("tags", {})
            e_lat = elem.get("lat") or elem.get("center", {}).get("lat")
            e_lon = elem.get("lon") or elem.get("center", {}).get("lon")

            if not e_lat or not e_lon:
                continue

            name = tags.get("name", "Unknown")
            infra_type = "unknown"
            if "aeroway" in tags:
                infra_type = "airport"
            elif "military" in tags:
                infra_type = "military"
            elif "power" in tags:
                infra_type = "power_plant"
            elif "harbour" in tags:
                infra_type = "port"

            # Calculate approximate distance
            from math import radians, cos, sin, sqrt, atan2
            R = 6371
            dlat = radians(float(e_lat) - lat)
            dlon = radians(float(e_lon) - lon)
            a = sin(dlat/2)**2 + cos(radians(lat)) * cos(radians(float(e_lat))) * sin(dlon/2)**2
            distance_km = R * 2 * atan2(sqrt(a), sqrt(1-a))

            infrastructure.append({
                "name": name,
                "type": infra_type,
                "lat": float(e_lat),
                "lon": float(e_lon),
                "distance_km": round(distance_km, 1),
            })

        infrastructure.sort(key=lambda x: x["distance_km"])

        return {
            "success": True,
            "infrastructure": infrastructure[:15],
            "center": {"lat": lat, "lon": lon},
            "radius_km": radius_km,
            "count": len(infrastructure),
        }

    except Exception as e:
        return {"success": False, "error": f"Overpass API error: {str(e)}"}


def main(args=None):
    if args is None:
        args = sys.argv[1:]

    if len(args) < 2:
        print(json.dumps({"success": False, "error": "Usage: news_geolocation.py <command> <args...>"}))
        return

    command = args[0]

    if command == "extract_and_geocode":
        result = extract_and_geocode(args[1])
    elif command == "nearby_infrastructure":
        if len(args) < 4:
            result = {"success": False, "error": "Usage: nearby_infrastructure <lat> <lon> <radius_km>"}
        else:
            result = nearby_infrastructure(args[1], args[2], args[3])
    else:
        result = {"success": False, "error": f"Unknown command: {command}"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
