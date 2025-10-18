#!/usr/bin/env python3
"""
French Government API Wrapper
Provides access to multiple French government APIs:
1. API Géo - Administrative boundaries and geographic data
2. API Catalogue - data.gouv.fr catalog search
3. API Tabulaire - Direct CSV/Excel file querying
4. API Entreprise - Company data (requires authentication)

Usage: python french_gov_api.py <command> [args]
"""

import sys
import json
import os
import requests
from typing import Dict, List, Optional, Any, Union
from datetime import datetime
import urllib.parse

# --- 1. CONFIGURATION ---
# API Géo Configuration
GEO_BASE_URL = "https://geo.api.gouv.fr"
GEO_TIMEOUT = 30

# API Catalogue Configuration
CATALOG_BASE_URL = "https://www.data.gouv.fr/api/1"
CATALOG_TIMEOUT = 30

# API Tabulaire Configuration
TABULAR_BASE_URL = "https://tabular-api.data.gouv.fr/api"
TABULAR_TIMEOUT = 30

# API Entreprise Configuration
ENTREPRISE_BASE_URL = "https://entreprise.api.gouv.fr"
ENTREPRISE_API_KEY = os.environ.get('ENTREPRISE_API_KEY')
ENTREPRISE_TIMEOUT = 30

# Request settings
DEFAULT_HEADERS = {
    'Content-Type': 'application/json',
    'User-Agent': 'Fincept-Terminal/1.0'
}

def _make_request(url: str, headers: Optional[Dict[str, str]] = None,
                  timeout: int = 30, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for French government APIs

    Args:
        url: Complete API URL
        headers: Optional request headers
        timeout: Request timeout in seconds
        params: Optional query parameters

    Returns:
        JSON response data or error information
    """
    try:
        if headers is None:
            headers = DEFAULT_HEADERS.copy()

        response = requests.get(url, params=params, headers=headers, timeout=timeout)
        response.raise_for_status()

        # Handle different response formats
        content_type = response.headers.get('content-type', '').lower()
        if 'application/json' in content_type:
            data = response.json()
        else:
            # Handle non-JSON responses (like GeoJSON)
            try:
                data = response.json()
            except:
                data = {"raw_response": response.text}

        return {
            "data": data,
            "metadata": {
                "source": urllib.parse.urlparse(url).netloc,
                "last_updated": datetime.now().isoformat(),
                "response_time_ms": response.elapsed.microseconds // 1000 if hasattr(response, 'elapsed') else 0
            },
            "error": None
        }

    except requests.exceptions.HTTPError as e:
        error_msg = f"HTTP Error: {e.response.status_code}"
        if e.response.status_code == 429:
            error_msg += " - Rate limit exceeded"
        elif e.response.status_code == 401:
            error_msg += " - Authentication required"
        elif e.response.status_code == 403:
            error_msg += " - Access forbidden"
        elif e.response.status_code == 404:
            error_msg += " - Resource not found"

        return {
            "data": [],
            "metadata": {},
            "error": f"{error_msg} - {e.response.text[:200]}" if hasattr(e, 'response') else error_msg
        }

    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {},
            "error": f"Request timeout. The API took more than {timeout} seconds to respond."
        }

    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {},
            "error": "Connection error. Unable to connect to the French government API."
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
            "error": "Invalid JSON response from French government API"
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"An unexpected error occurred: {str(e)}"
        }

# ============================================================================
# API GÉO - GEOGRAPHIC AND ADMINISTRATIVE DATA
# ============================================================================

def get_communes(nom: Optional[str] = None, codePostal: Optional[str] = None,
                lat: Optional[float] = None, lon: Optional[float] = None,
                format: str = "json", geometry: Optional[str] = None,
                limit: int = 20, boost: str = "population") -> Dict[str, Any]:
    """
    Get French municipalities (communes) with various search criteria

    Args:
        nom: Search by commune name
        codePostal: Search by postal code
        lat: Search by latitude (with lon)
        lon: Search by longitude (with lat)
        format: Response format (json or geojson)
        geometry: Include geometry (contour, centre)
        limit: Maximum number of results
        boost: Boost results by population or default

    Returns:
        JSON response with commune data
    """
    try:
        url = f"{GEO_BASE_URL}/communes"
        params = {
            "limit": limit,
            "boost": boost
        }

        if nom:
            params["nom"] = nom
        if codePostal:
            params["codePostal"] = codePostal
        if lat is not None and lon is not None:
            params["lat"] = lat
            params["lon"] = lon
        if format != "json":
            params["format"] = format
        if geometry:
            params["geometry"] = geometry

        result = _make_request(url, timeout=GEO_TIMEOUT, params=params)

        if not result["error"]:
            # Enhance commune data
            communes = result.get("data", [])
            if isinstance(communes, list):
                enhanced_data = []
                for commune in communes:
                    enhanced_commune = {
                        "code": commune.get("code"),
                        "nom": commune.get("nom"),
                        "codesPostaux": commune.get("codesPostaux", []),
                        "codeDepartement": commune.get("codeDepartement"),
                        "codeRegion": commune.get("codeRegion"),
                        "population": commune.get("population"),
                        "centre": commune.get("centre"),
                        "contour": commune.get("contour"),
                        "surface": commune.get("surface"),
                        "_search_score": commune.get("_score", 0)
                    }
                    enhanced_data.append(enhanced_commune)
                result["data"] = enhanced_data
                result["metadata"]["count"] = len(enhanced_data)
                result["metadata"]["search_criteria"] = {
                    "nom": nom, "codePostal": codePostal, "lat": lat, "lon": lon,
                    "format": format, "geometry": geometry, "limit": limit
                }

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching communes: {str(e)}"
        }

def get_departements(nom: Optional[str] = None, code: Optional[str] = None,
                    format: str = "json", limit: int = 20) -> Dict[str, Any]:
    """
    Get French departments

    Args:
        nom: Search by department name
        code: Search by department code
        format: Response format (json or geojson)
        limit: Maximum number of results

    Returns:
        JSON response with department data
    """
    try:
        url = f"{GEO_BASE_URL}/departements"
        params = {"limit": limit}

        if nom:
            params["nom"] = nom
        if code:
            params["code"] = code
        if format != "json":
            params["format"] = format

        result = _make_request(url, timeout=GEO_TIMEOUT, params=params)

        if not result["error"]:
            # Enhance department data
            departments = result.get("data", [])
            if isinstance(departments, list):
                enhanced_data = []
                for dept in departments:
                    enhanced_dept = {
                        "code": dept.get("code"),
                        "nom": dept.get("nom"),
                        "codeRegion": dept.get("codeRegion"),
                        "centre": dept.get("centre"),
                        "contour": dept.get("contour"),
                        "_search_score": dept.get("_score", 0)
                    }
                    enhanced_data.append(enhanced_dept)
                result["data"] = enhanced_data
                result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching departments: {str(e)}"
        }

def get_regions(nom: Optional[str] = None, code: Optional[str] = None,
               format: str = "json", limit: int = 20) -> Dict[str, Any]:
    """
    Get French regions

    Args:
        nom: Search by region name
        code: Search by region code
        format: Response format (json or geojson)
        limit: Maximum number of results

    Returns:
        JSON response with region data
    """
    try:
        url = f"{GEO_BASE_URL}/regions"
        params = {"limit": limit}

        if nom:
            params["nom"] = nom
        if code:
            params["code"] = code
        if format != "json":
            params["format"] = format

        result = _make_request(url, timeout=GEO_TIMEOUT, params=params)

        if not result["error"]:
            # Enhance region data
            regions = result.get("data", [])
            if isinstance(regions, list):
                enhanced_data = []
                for region in regions:
                    enhanced_region = {
                        "code": region.get("code"),
                        "nom": region.get("nom"),
                        "centre": region.get("centre"),
                        "contour": region.get("contour"),
                        "_search_score": region.get("_score", 0)
                    }
                    enhanced_data.append(enhanced_region)
                result["data"] = enhanced_data
                result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching regions: {str(e)}"
        }

# ============================================================================
# API CATALOGUE - DATA.GOUV.FR CATALOG SEARCH
# ============================================================================

def search_dataservices(q: Optional[str] = None, page: int = 1,
                       page_size: int = 20) -> Dict[str, Any]:
    """
    Search for dataservices (APIs) in data.gouv.fr catalog

    Args:
        q: Search query string
        page: Page number for pagination
        page_size: Number of results per page

    Returns:
        JSON response with dataservice search results
    """
    try:
        url = f"{CATALOG_BASE_URL}/dataservices/"
        params = {
            "page": page,
            "page_size": page_size
        }

        if q:
            params["q"] = q

        result = _make_request(url, timeout=CATALOG_TIMEOUT, params=params)

        if not result["error"]:
            # Enhance dataservice data
            dataservices_data = result.get("data", {})
            dataservices = dataservices_data.get("data", [])
            if isinstance(dataservices, list):
                enhanced_data = []
                for ds in dataservices:
                    enhanced_ds = {
                        "id": ds.get("id"),
                        "title": ds.get("title"),
                        "description": ds.get("description"),
                        "url": ds.get("url"),
                        "owner": ds.get("owner", {}).get("name") if ds.get("owner") else None,
                        "metrics": ds.get("metrics", {}),
                        "created_at": ds.get("created_at"),
                        "last_modified": ds.get("last_modified"),
                        "followers": ds.get("followers", 0),
                        "views": ds.get("views", 0)
                    }
                    enhanced_data.append(enhanced_ds)

                result["data"] = enhanced_data
                result["metadata"]["pagination"] = {
                    "page": page,
                    "page_size": page_size,
                    "total": dataservices_data.get("total", 0),
                    "returned": len(enhanced_data)
                }
                result["metadata"]["search_query"] = q

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching dataservices: {str(e)}"
        }

def search_datasets(q: Optional[str] = None, page: int = 1,
                   page_size: int = 20) -> Dict[str, Any]:
    """
    Search for datasets in data.gouv.fr catalog

    Args:
        q: Search query string
        page: Page number for pagination
        page_size: Number of results per page

    Returns:
        JSON response with dataset search results
    """
    try:
        url = f"{CATALOG_BASE_URL}/datasets/"
        params = {
            "page": page,
            "page_size": page_size
        }

        if q:
            params["q"] = q

        result = _make_request(url, timeout=CATALOG_TIMEOUT, params=params)

        if not result["error"]:
            # Enhance dataset data
            datasets_data = result.get("data", {})
            datasets = datasets_data.get("data", [])
            if isinstance(datasets, list):
                enhanced_data = []
                for dataset in datasets:
                    enhanced_dataset = {
                        "id": dataset.get("id"),
                        "title": dataset.get("title"),
                        "description": dataset.get("description"),
                        "url": dataset.get("page"),
                        "owner": dataset.get("owner", {}).get("name") if dataset.get("owner") else None,
                        "organization": dataset.get("organization", {}).get("name") if dataset.get("organization") else None,
                        "metrics": dataset.get("metrics", {}),
                        "created_at": dataset.get("created_at"),
                        "last_modified": dataset.get("last_modified"),
                        "tags": [tag for tag in dataset.get("tags", [])],
                        "frequency": dataset.get("frequency"),
                        "temporal_coverage": dataset.get("temporal_coverage"),
                        "spatial": dataset.get("spatial"),
                        "license": dataset.get("license"),
                        "resources_count": len(dataset.get("resources", []))
                    }
                    enhanced_data.append(enhanced_dataset)

                result["data"] = enhanced_data
                result["metadata"]["pagination"] = {
                    "page": page,
                    "page_size": page_size,
                    "total": datasets_data.get("total", 0),
                    "returned": len(enhanced_data)
                }
                result["metadata"]["search_query"] = q

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching datasets: {str(e)}"
        }

# ============================================================================
# API TABULAIRE - DIRECT CSV/EXCEL FILE QUERYING
# ============================================================================

def get_resource_profile(resource_id: str) -> Dict[str, Any]:
    """
    Get the schema/profile of a tabular resource (CSV/Excel file)

    Args:
        resource_id: The unique identifier of the resource

    Returns:
        JSON response with resource schema information
    """
    try:
        url = f"{TABULAR_BASE_URL}/resources/{resource_id}/profile/"

        result = _make_request(url, timeout=TABULAR_TIMEOUT)

        if not result["error"]:
            # Enhance profile data
            profile_data = result.get("data", {})
            enhanced_profile = {
                "resource_id": resource_id,
                "name": profile_data.get("name"),
                "title": profile_data.get("title"),
                "description": profile_data.get("description"),
                "format": profile_data.get("format"),
                "mime": profile_data.get("mime"),
                "size": profile_data.get("size"),
                "hash": profile_data.get("hash"),
                "created_at": profile_data.get("created_at"),
                "modified_at": profile_data.get("modified_at"),
                "columns": [],
                "total_rows": profile_data.get("total_rows"),
                "encoding": profile_data.get("encoding"),
                "delimiter": profile_data.get("delimiter"),
                "has_header": profile_data.get("has_header", True)
            }

            # Process column information
            if "columns" in profile_data:
                for col in profile_data["columns"]:
                    enhanced_col = {
                        "name": col.get("name"),
                        "title": col.get("title"),
                        "type": col.get("type"),
                        "format": col.get("format"),
                        "description": col.get("description"),
                        "constraints": col.get("constraints", {})
                    }
                    enhanced_profile["columns"].append(enhanced_col)

            result["data"] = enhanced_profile
            result["metadata"]["column_count"] = len(enhanced_profile["columns"])

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching resource profile: {str(e)}"
        }

def get_resource_lines(resource_id: str, page: int = 1,
                      page_size: int = 20) -> Dict[str, Any]:
    """
    Get actual data lines from a tabular resource

    Args:
        resource_id: The unique identifier of the resource
        page: Page number for pagination
        page_size: Number of rows per page

    Returns:
        JSON response with tabular data
    """
    try:
        url = f"{TABULAR_BASE_URL}/resources/{resource_id}/lines/"
        params = {
            "page": page,
            "page_size": page_size
        }

        result = _make_request(url, timeout=TABULAR_TIMEOUT, params=params)

        if not result["error"]:
            # Enhance data response
            lines_data = result.get("data", {})
            enhanced_data = {
                "resource_id": resource_id,
                "lines": lines_data.get("data", []),
                "total_rows": lines_data.get("total_rows", 0),
                "page": page,
                "page_size": page_size,
                "has_more": lines_data.get("has_more", False),
                "next_page": lines_data.get("next_page"),
                "prev_page": lines_data.get("prev_page")
            }

            result["data"] = enhanced_data
            result["metadata"]["pagination"] = {
                "page": page,
                "page_size": page_size,
                "total_rows": lines_data.get("total_rows", 0),
                "returned_rows": len(lines_data.get("data", [])),
                "has_more": lines_data.get("has_more", False)
            }

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching resource lines: {str(e)}"
        }

# ============================================================================
# API ENTREPRISE - COMPANY DATA (REQUIRES AUTHENTICATION)
# ============================================================================

def search_company(siret: Optional[str] = None, siren: Optional[str] = None,
                  matching_size: int = 1) -> Dict[str, Any]:
    """
    Search for French company information (requires authentication)

    Args:
        siret: SIRET number (14 digits)
        siren: SIREN number (9 digits)
        matching_size: Number of matches to return

    Returns:
        JSON response with company information
    """
    try:
        if not ENTREPRISE_API_KEY:
            return {
                "data": {},
                "metadata": {},
                "error": "ENTREPRISE_API_KEY environment variable is required for this endpoint"
            }

        if not siret and not siren:
            return {
                "data": {},
                "metadata": {},
                "error": "Either siret or siren parameter is required"
            }

        url = f"{ENTREPRISE_BASE_URL}/v3/search"
        headers = DEFAULT_HEADERS.copy()
        headers["Authorization"] = f"Bearer {ENTREPRISE_API_KEY}"

        params = {
            "matching_size": matching_size
        }

        if siret:
            params["siret"] = siret
        if siren:
            params["siren"] = siren

        result = _make_request(url, timeout=ENTREPRISE_TIMEOUT, headers=headers, params=params)

        if not result["error"]:
            # Enhance company data
            companies_data = result.get("data", {}).get("results", [])
            enhanced_data = []

            for company in companies_data:
                enhanced_company = {
                    "siren": company.get("siren"),
                    "siret": company.get("siret"),
                    "nom_complet": company.get("nom_complet"),
                    "nom_raison_sociale": company.get("nom_raison_sociale"),
                    "sigle": company.get("sigle"),
                    "categorie_entreprise": company.get("categorie_entreprise"),
                    "date_creation": company.get("date_creation"),
                    "date_cessation": company.get("date_cessation"),
                    "date_immatriculation": company.get("date_immatriculation"),
                    "etat_administratif": company.get("etat_administratif"),
                    "libelle_nature_juridique": company.get("libelle_nature_juridique"),
                    "forme_juridique_code": company.get("forme_juridique_code"),
                    "activite_principale": company.get("activite_principale"),
                    "tranche_effectif_salarie": company.get("tranche_effectif_salarie"),
                    "adresse": company.get("adresse"),
                    "comptes_annuels": company.get("comptes_annuels"),
                    "score_matching": company.get("score_matching")
                }
                enhanced_data.append(enhanced_company)

            result["data"] = enhanced_data
            result["metadata"]["companies_count"] = len(enhanced_data)
            result["metadata"]["search_criteria"] = {
                "siret": siret, "siren": siren, "matching_size": matching_size
            }

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error searching company: {str(e)}"
        }

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

def test_all_api_connectivity() -> Dict[str, Any]:
    """
    Test connectivity to all French government APIs

    Returns:
        JSON response with connectivity test results
    """
    results = {}

    # Test API Géo
    try:
        geo_result = get_communes(limit=1)
        results["api_geo"] = {
            "status": "connected" if not geo_result["error"] else "error",
            "message": geo_result["error"] or "Successfully connected to API Géo",
            "response_time_ms": geo_result.get("metadata", {}).get("response_time_ms", 0)
        }
    except Exception as e:
        results["api_geo"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    # Test API Catalogue
    try:
        catalog_result = search_datasets(page_size=1)
        results["api_catalogue"] = {
            "status": "connected" if not catalog_result["error"] else "error",
            "message": catalog_result["error"] or "Successfully connected to API Catalogue",
            "response_time_ms": catalog_result.get("metadata", {}).get("response_time_ms", 0)
        }
    except Exception as e:
        results["api_catalogue"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    # Test API Tabulaire
    try:
        # Use a known resource ID for testing
        tabular_result = get_resource_profile("1c5075ec-7ce1-49cb-ab89-94f507812daf")
        results["api_tabulaire"] = {
            "status": "connected" if not tabular_result["error"] else "error",
            "message": tabular_result["error"] or "Successfully connected to API Tabulaire",
            "response_time_ms": tabular_result.get("metadata", {}).get("response_time_ms", 0)
        }
    except Exception as e:
        results["api_tabulaire"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    # Test API Entreprise
    try:
        if ENTREPRISE_API_KEY:
            # Use a dummy SIREN for testing
            entreprise_result = search_company(siren="000000000")
            results["api_entreprise"] = {
                "status": "connected" if not entreprise_result["error"] or "not found" in entreprise_result["error"].lower() else "error",
                "message": "Successfully connected to API Entreprise (test call made)",
                "response_time_ms": entreprise_result.get("metadata", {}).get("response_time_ms", 0)
            }
        else:
            results["api_entreprise"] = {
                "status": "skipped",
                "message": "API key not configured - test skipped",
                "response_time_ms": 0
            }
    except Exception as e:
        results["api_entreprise"] = {
            "status": "error",
            "message": str(e),
            "response_time_ms": 0
        }

    return {
        "data": results,
        "metadata": {
            "test_timestamp": datetime.now().isoformat(),
            "api_keys_configured": {
                "entreprise_api": bool(ENTREPRISE_API_KEY)
            }
        },
        "error": None
    }

# ============================================================================
# CLI INTERFACE
# ============================================================================

def main():
    """CLI interface for French Government API"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python french_gov_api.py <command> <args>",
            "available_commands": [
                "# API Géo (Geographic Data)",
                "communes [nom|codePostal|lat lon] [limit]",
                "departements [nom|code] [limit]",
                "regions [nom|code] [limit]",
                "",
                "# API Catalogue (data.gouv.fr)",
                "dataservices [query] [page] [page_size]",
                "datasets [query] [page] [page_size]",
                "",
                "# API Tabulaire",
                "profile <resource_id>",
                "lines <resource_id> [page] [page_size]",
                "",
                "# API Entreprise (requires API key)",
                "company <siren|siret>",
                "",
                "# Utility",
                "test-connectivity"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        # API Géo Commands
        if command == "communes":
            if len(sys.argv) > 2:
                arg = sys.argv[2]
                # Check if it's a name or postal code
                if arg.isdigit():
                    result = get_communes(codePostal=arg, limit=int(sys.argv[3]) if len(sys.argv) > 3 else 20)
                elif "," in arg:
                    coords = arg.split(",")
                    lat, lon = float(coords[0]), float(coords[1])
                    result = get_communes(lat=lat, lon=lon, limit=int(sys.argv[3]) if len(sys.argv) > 3 else 20)
                else:
                    result = get_communes(nom=arg, limit=int(sys.argv[3]) if len(sys.argv) > 3 else 20)
            else:
                result = get_communes(limit=20)

        elif command == "departements":
            if len(sys.argv) > 2:
                arg = sys.argv[2]
                if arg.isdigit():
                    result = get_departements(code=arg, limit=int(sys.argv[3]) if len(sys.argv) > 3 else 20)
                else:
                    result = get_departements(nom=arg, limit=int(sys.argv[3]) if len(sys.argv) > 3 else 20)
            else:
                result = get_departements(limit=20)

        elif command == "regions":
            if len(sys.argv) > 2:
                arg = sys.argv[2]
                if arg.isdigit():
                    result = get_regions(code=arg, limit=int(sys.argv[3]) if len(sys.argv) > 3 else 20)
                else:
                    result = get_regions(nom=arg, limit=int(sys.argv[3]) if len(sys.argv) > 3 else 20)
            else:
                result = get_regions(limit=20)

        # API Catalogue Commands
        elif command == "dataservices":
            query = sys.argv[2] if len(sys.argv) > 2 else None
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 else 20
            result = search_dataservices(q=query, page=page, page_size=page_size)

        elif command == "datasets":
            query = sys.argv[2] if len(sys.argv) > 2 else None
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 else 20
            result = search_datasets(q=query, page=page, page_size=page_size)

        # API Tabulaire Commands
        elif command == "profile":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: profile <resource_id>"}))
                sys.exit(1)
            resource_id = sys.argv[2]
            result = get_resource_profile(resource_id)

        elif command == "lines":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: lines <resource_id> [page] [page_size]"}))
                sys.exit(1)
            resource_id = sys.argv[2]
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 else 20
            result = get_resource_lines(resource_id, page, page_size)

        # API Entreprise Commands
        elif command == "company":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: company <siren|siret>"}))
                sys.exit(1)
            identifier = sys.argv[2]
            if len(identifier) == 9:
                result = search_company(siren=identifier)
            elif len(identifier) == 14:
                result = search_company(siret=identifier)
            else:
                result = {"error": "Invalid identifier. Must be 9-digit SIREN or 14-digit SIRET"}

        # Utility Commands
        elif command == "test-connectivity":
            result = test_all_api_connectivity()

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "communes [nom|codePostal|lat lon] [limit]",
                    "departements [nom|code] [limit]",
                    "regions [nom|code] [limit]",
                    "dataservices [query] [page] [page_size]",
                    "datasets [query] [page] [page_size]",
                    "profile <resource_id>",
                    "lines <resource_id> [page] [page_size]",
                    "company <siren|siret>",
                    "test-connectivity"
                ]
            }

        print(json.dumps(result, indent=2, ensure_ascii=False))

    except ValueError as e:
        print(json.dumps({"error": f"Invalid parameter: {str(e)}"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"error": f"Command execution failed: {str(e)}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()