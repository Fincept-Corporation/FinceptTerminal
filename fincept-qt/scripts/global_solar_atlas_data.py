"""
Global Solar Atlas Data Fetcher
Global Solar Atlas (World Bank/Solargis): long-term solar irradiance (GHI, DNI, DIF), optimal-tilt irradiance, PV output potential and air temperature for any location worldwide (keyless).
"""
import sys
import json
import requests
from typing import Dict, Any

# Long-term-average point data endpoint used by globalsolaratlas.info (the old /api/v1 paths were removed).
BASE_URL = "https://api.globalsolaratlas.info/data"

UNITS = {
    "GHI": "kWh/m2 (global horizontal irradiation)",
    "DNI": "kWh/m2 (direct normal irradiation)",
    "DIF": "kWh/m2 (diffuse horizontal irradiation)",
    "GTI_opta": "kWh/m2 (global tilted irradiation at optimum angle)",
    "PVOUT_csi": "kWh/kWp (specific PV output, crystalline-silicon, optimum tilt)",
    "OPTA": "degrees (optimum tilt of PV modules)",
    "TEMP": "degC (air temperature)",
    "ELE": "m (terrain elevation)",
}
ALIASES = {"PVOUT": "PVOUT_csi", "GTI": "GTI_opta"}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _lta(lat: str, lon: str) -> Any:
    try:
        lat_f, lon_f = float(lat), float(lon)
    except (TypeError, ValueError):
        return {"error": "lat and lon must be numbers"}
    try:
        response = session.get(f"{BASE_URL}/lta", params={"loc": f"{lat_f},{lon_f}"}, timeout=30)
        if response.status_code in (400, 404):
            return {"error": f"No Global Solar Atlas data for {lat_f},{lon_f} (outside coverage, e.g. polar or open sea)"}
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if not isinstance(data, dict) or "annual" not in data:
        return {"error": "Unexpected response from Global Solar Atlas"}
    return data


def _annual(lat: str, lon: str, variable: str) -> Any:
    data = _lta(lat, lon)
    if "error" in data:
        return data
    var = ALIASES.get(variable, variable)
    annual = data["annual"].get("data", {})
    monthly = (data.get("monthly") or {}).get("data", {}).get(var)
    if annual.get(var) is None and not monthly:
        return {"error": f"No {var} value for {lat},{lon} (outside coverage, e.g. open sea, or unknown variable)"}
    out ={"lat": float(lat), "lon": float(lon), "variable": var, "unit": UNITS.get(var),
           "annual": annual.get(var), "source": "Global Solar Atlas (World Bank / Solargis), long-term average"}
    if monthly:
        out["monthly"] = monthly
    return out


def get_ghi(lat: str, lon: str) -> Any:
    return _annual(lat, lon, "GHI")


def get_dni(lat: str, lon: str) -> Any:
    return _annual(lat, lon, "DNI")


def get_pvout(lat: str, lon: str, system_type: str = "fixed") -> Any:
    out = _annual(lat, lon, "PVOUT_csi")
    if "error" not in out:
        out["system_type"] = "fixed, optimum tilt"
        if system_type and system_type != "fixed":
            out["note"] = "Only the fixed optimum-tilt system is available from the public API"
    return out


def get_dif(lat: str, lon: str) -> Any:
    return _annual(lat, lon, "DIF")


def get_site_summary(lat: str, lon: str) -> Any:
    """All long-term annual values for one site."""
    data = _lta(lat, lon)
    if "error" in data:
        return data
    annual = data["annual"].get("data", {})
    return {"lat": float(lat), "lon": float(lon),
            "annual": {k: {"value": v, "unit": UNITS.get(k)} for k, v in annual.items()},
            "source": "Global Solar Atlas (World Bank / Solargis), long-term average"}


def get_monthly_data(lat: str, lon: str, variable: str) -> Any:
    out = _annual(lat, lon, variable)
    if "error" in out:
        return out
    if "monthly" not in out:
        return {"error": f"No monthly series for {variable}. Options: GHI, DNI, DIF, GTI_opta, PVOUT_csi, TEMP"}
    return out


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "ghi":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        result = get_ghi(lat, lon)
    elif command == "dni":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        result = get_dni(lat, lon)
    elif command == "pvout":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        system_type = args[3] if len(args) > 3 else "fixed"
        result = get_pvout(lat, lon, system_type)
    elif command == "dif":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        result = get_dif(lat, lon)
    elif command == "summary":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        result = get_site_summary(lat, lon)
    elif command == "monthly":
        lat = args[1] if len(args) > 1 else "48.8"
        lon = args[2] if len(args) > 2 else "2.3"
        variable = args[3] if len(args) > 3 else "GHI"
        result = get_monthly_data(lat, lon, variable)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
