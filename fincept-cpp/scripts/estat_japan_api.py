"""
Japanese Government e-Stat API Wrapper
Fetches Japanese official statistics data using hierarchical structure:
1. Statistics List (Catalogue) - Lists available statistical tables
2. Meta Information - Get structure and metadata for specific tables
3. Statistical Data - Get actual numerical data from tables

API Documentation: https://www.e-stat.go.jp/api/en/api-info
Portal URL: https://www.e-stat.go.jp/
Requires appId registration: https://www.e-stat.go.jp/api/en/api-info/user-guide

Usage: python estat_japan_api.py <command> [args]
"""

import sys
import json
import os
import requests
from typing import Dict, List, Optional, Any
from datetime import datetime
import urllib.parse

# --- 1. CONFIGURATION ---
BASE_URL = "https://api.e-stat.go.jp/rest/3.0/app"
API_KEY = os.environ.get('ESTAT_APP_ID')  # Required: Application ID from e-Stat
TIMEOUT = 30
RATE_LIMIT_DELAY = 1.0  # Be respectful to the public API

def _make_request(endpoint: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for e-Stat API

    Args:
        endpoint: API endpoint path (e.g., 'json/getStatsList')
        params: Query parameters for the request

    Returns:
        Dict with consistent structure: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    try:
        url = f"{BASE_URL}/{endpoint}"

        # Default parameters
        if params is None:
            params = {}

        # Add required appId parameter
        if not API_KEY:
            return {
                "data": [],
                "metadata": {
                    "source": "e-Stat (Japan)",
                    "endpoint": endpoint,
                    "parameters": params
                },
                "error": "e-Stat Application ID (ESTAT_APP_ID) is required. Set environment variable or register at https://www.e-stat.go.jp/api/en/api-info/user-guide"
            }

        params['appId'] = API_KEY

        # Setup headers
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Fincept-Terminal/1.0'
        }

        # Make request with timeout
        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        data = response.json()

        # Check for e-Stat API errors
        if isinstance(data, dict) and (data.get('GET_STATS_LIST') or data.get('GET_META_INFO') or data.get('GET_STATS_DATA')):
            # Valid e-Stat response structure
            result_data = data
        else:
            # Unexpected response format
            return {
                "data": [],
                "metadata": {
                    "source": "e-Stat (Japan)",
                    "endpoint": endpoint,
                    "parameters": params,
                    "http_status": response.status_code
                },
                "error": f"Unexpected response format from e-Stat API"
            }

        return {
            "data": result_data,
            "metadata": {
                "source": "e-Stat (Japan)",
                "endpoint": endpoint,
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
                "source": "e-Stat (Japan)",
                "endpoint": endpoint,
                "parameters": params,
                "http_status": e.response.status_code
            },
            "error": f"HTTP Error: {e.response.status_code} - {e.response.text}"
        }
    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {
                "source": "e-Stat (Japan)",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": "Request timeout. The e-Stat API is taking too long to respond."
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {
                "source": "e-Stat (Japan)",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": "Connection error. Could not connect to e-Stat API."
        }
    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {
                "source": "e-Stat (Japan)",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": f"Network or request error: {str(e)}"
        }
    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {
                "source": "e-Stat (Japan)",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": "Invalid JSON response from e-Stat API."
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "source": "e-Stat (Japan)",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": f"An unexpected error occurred: {str(e)}"
        }

def _parse_stats_list(raw_data: Dict[str, Any]) -> List[Dict[str, Any]]:
    """
    Parse raw e-Stat stats list response into enhanced format

    Args:
        raw_data: Raw response from getStatsList endpoint

    Returns:
        List of enhanced statistical table entries
    """
    try:
        stats_list = raw_data.get('GET_STATS_LIST', {}).get('STATISTICAL_DATA', [])

        enhanced_data = []
        for stat in stats_list:
            enhanced_stat = {
                "id": stat.get("@id"),
                "stat_name": stat.get("STAT_NAME"),
                "gov_org": stat.get("GOV_ORG"),
                "gov_org_name": stat.get("GOV_ORG_NAME"),
                "statistics_name": stat.get("STATISTICS_NAME"),
                "title": stat.get("TITLE"),
                "cycle": stat.get("CYCLE"),
                "survey_date": stat.get("SURVEY_DATE"),
                "open_date": stat.get("OPEN_DATE"),
                "small_area": stat.get("SMALL_AREA"),
                "main_category": stat.get("MAIN_CATEGORY"),
                "sub_category": stat.get("SUB_CATEGORY"),
                "overview": stat.get("OVERVIEW"),
                "collect_date": stat.get("COLLECT_DATE"),
                "total_number": stat.get("TOTAL_NUMBER"),
                "notes": stat.get("NOTES")
            }
            enhanced_data.append(enhanced_stat)

        return enhanced_data
    except Exception as e:
        return []

def _parse_meta_info(raw_data: Dict[str, Any]) -> Dict[str, Any]:
    """
    Parse raw e-Stat meta info response into enhanced format

    Args:
        raw_data: Raw response from getMetaInfo endpoint

    Returns:
        Enhanced metadata dictionary
    """
    try:
        meta_info = raw_data.get('GET_META_INFO', {}).get('METADATA_INF', {})

        enhanced_meta = {
            "table_id": meta_info.get("@id"),
            "table_name": meta_info.get("TABLE_NAME"),
            "table_name_en": meta_info.get("TABLE_NAME_EN"),
            "table_subtitle": meta_info.get("TABLE_SUB_TITLE"),
            "table_subtitle_en": meta_info.get("TABLE_SUB_TITLE_EN"),
            "gov_org": meta_info.get("GOV_ORG"),
            "gov_org_name": meta_info.get("GOV_ORG_NAME"),
            "statistics_name": meta_info.get("STATISTICS_NAME"),
            "title": meta_info.get("TITLE"),
            "title_en": meta_info.get("TITLE_EN"),
            "cycle": meta_info.get("CYCLE"),
            "survey_date": meta_info.get("SURVEY_DATE"),
            "open_date": meta_info.get("OPEN_DATE"),
            "small_area": meta_info.get("SMALL_AREA"),
            "main_category": meta_info.get("MAIN_CATEGORY"),
            "sub_category": meta_info.get("SUB_CATEGORY"),
            "overview": meta_info.get("OVERVIEW"),
            "overview_en": meta_info.get("OVERVIEW_EN"),
            "collect_date": meta_info.get("COLLECT_DATE"),
            "copyright": meta_info.get("COPYRIGHT"),
            "source": meta_info.get("SOURCE"),
            "source_en": meta_info.get("SOURCE_EN")
        }

        # Parse class objects (categories, areas, time)
        class_obj = meta_info.get("CLASS_INF", {}).get("CLASS_OBJ", [])

        if isinstance(class_obj, dict):
            class_obj = [class_obj]

        enhanced_meta["classifications"] = []
        for cls in class_obj:
            classification = {
                "id": cls.get("@id"),
                "name": cls.get("@name"),
                "name_en": cls.get("@nameEn"),
                "class_type": cls.get("@type"),
                "objects": []
            }

            class_objects = cls.get("CLASS", [])
            if isinstance(class_objects, dict):
                class_objects = [class_objects]

            for obj in class_objects:
                class_obj_data = {
                    "code": obj.get("@code"),
                    "name": obj.get("@name"),
                    "name_en": obj.get("@nameEn"),
                    "level": obj.get("@level"),
                    "unit": obj.get("@unit")
                }
                classification["objects"].append(class_obj_data)

            enhanced_meta["classifications"].append(classification)

        return enhanced_meta
    except Exception as e:
        return {}

def _parse_stats_data(raw_data: Dict[str, Any]) -> Dict[str, Any]:
    """
    Parse raw e-Stat stats data response into enhanced format

    Args:
        raw_data: Raw response from getStatsData endpoint

    Returns:
        Enhanced statistical data dictionary
    """
    try:
        stats_data = raw_data.get('GET_STATS_DATA', {}).get('STATISTICAL_DATA', {})

        enhanced_data = {
            "table_id": stats_data.get("@id"),
            "table_name": stats_data.get("TABLE_NAME"),
            "gov_org": stats_data.get("GOV_ORG"),
            "gov_org_name": stats_data.get("GOV_ORG_NAME"),
            "statistics_name": stats_data.get("STATISTICS_NAME"),
            "title": stats_data.get("TITLE"),
            "created": stats_data.get("DATE"),
            "data_values": [],
            "classifications": []
        }

        # Parse data values
        data_values = stats_data.get("DATA_INF", {}).get("VALUE", [])
        if isinstance(data_values, dict):
            data_values = [data_values]

        for value in data_values:
            data_point = {
                "tab": value.get("@tab"),
                "cat01": value.get("@cat01"),
                "cat02": value.get("@cat02"),
                "cat03": value.get("@cat03"),
                "area": value.get("@area"),
                "time": value.get("@time"),
                "value": value.get("$"),
                "unit": value.get("@unit")
            }
            enhanced_data["data_values"].append(data_point)

        # Parse classifications for reference
        class_inf = stats_data.get("CLASS_INF", {}).get("CLASS_OBJ", [])
        if isinstance(class_inf, dict):
            class_inf = [class_inf]

        for cls in class_inf:
            classification = {
                "id": cls.get("@id"),
                "name": cls.get("@name"),
                "name_en": cls.get("@nameEn"),
                "type": cls.get("@type")
            }
            enhanced_data["classifications"].append(classification)

        return enhanced_data
    except Exception as e:
        return {}

# --- 2. CORE FUNCTIONS (GROUPED BY API CATEGORY) ---

# ====== STATISTICS LIST (CATALOGUE) ======
def get_stats_list(search_word: Optional[str] = None, stats_code: Optional[str] = None,
                   survey_years: Optional[str] = None, limit: int = 100) -> Dict[str, Any]:
    """
    Get list of available statistical tables

    Args:
        search_word: Keyword to search for (e.g., "population", "GDP")
        stats_code: Five-digit code to filter by government ministry
        survey_years: Year the survey was conducted (e.g., "2020")
        limit: Number of results to return

    Returns:
        JSON response with statistics list
    """
    try:
        params = {'limit': limit}

        if search_word:
            params['searchWord'] = search_word
        if stats_code:
            params['statsCode'] = stats_code
        if survey_years:
            params['surveyYears'] = survey_years

        result = _make_request("json/getStatsList", params)

        if result["error"]:
            return result

        # Parse and enhance data
        parsed_data = _parse_stats_list(result["data"])
        result["data"] = parsed_data
        result["metadata"]["count"] = len(parsed_data)
        result["metadata"]["search_word"] = search_word
        result["metadata"]["stats_code"] = stats_code
        result["metadata"]["survey_years"] = survey_years
        result["metadata"]["description"] = f"Statistical tables list"

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching statistics list: {str(e)}"
        }

def get_stats_list_by_keyword(keyword: str, limit: int = 50) -> Dict[str, Any]:
    """
    Search statistical tables by keyword

    Args:
        keyword: Search keyword
        limit: Number of results to return

    Returns:
        JSON response with matching statistics
    """
    return get_stats_list(search_word=keyword, limit=limit)

def get_stats_list_by_ministry(stats_code: str, limit: int = 100) -> Dict[str, Any]:
    """
    Get statistical tables by government ministry code

    Args:
        stats_code: Five-digit ministry code
        limit: Number of results to return

    Returns:
        JSON response with ministry statistics
    """
    return get_stats_list(stats_code=stats_code, limit=limit)

def get_stats_list_by_year(year: str, limit: int = 100) -> Dict[str, Any]:
    """
    Get statistical tables by survey year

    Args:
        year: Survey year (e.g., "2020")
        limit: Number of results to return

    Returns:
        JSON response with year-specific statistics
    """
    return get_stats_list(survey_years=year, limit=limit)

# ====== META INFORMATION ======
def get_meta_info(stats_data_id: str) -> Dict[str, Any]:
    """
    Get metadata and structure for a specific statistical table

    Args:
        stats_data_id: Unique ID of the statistical table

    Returns:
        JSON response with table metadata
    """
    try:
        params = {'statsDataId': stats_data_id}
        result = _make_request("json/getMetaInfo", params)

        if result["error"]:
            return result

        # Parse and enhance data
        parsed_data = _parse_meta_info(result["data"])
        result["data"] = parsed_data
        result["metadata"]["stats_data_id"] = stats_data_id
        result["metadata"]["description"] = f"Metadata for statistical table {stats_data_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching meta info: {str(e)}"
        }

def get_table_classifications(stats_data_id: str) -> Dict[str, Any]:
    """
    Get classification structure for a statistical table

    Args:
        stats_data_id: Unique ID of the statistical table

    Returns:
        JSON response with table classifications
    """
    meta_result = get_meta_info(stats_data_id)

    if meta_result["error"]:
        return meta_result

    classifications = meta_result["data"].get("classifications", [])

    return {
        "data": classifications,
        "metadata": {
            "source": "e-Stat (Japan)",
            "stats_data_id": stats_data_id,
            "count": len(classifications),
            "description": f"Classifications for table {stats_data_id}"
        },
        "error": None
    }

# ====== STATISTICAL DATA ======
def get_stats_data(stats_data_id: str, cd_cat01: Optional[str] = None,
                   cd_cat02: Optional[str] = None, cd_cat03: Optional[str] = None,
                   cd_area: Optional[str] = None, cd_time: Optional[str] = None,
                   start_position: Optional[int] = None) -> Dict[str, Any]:
    """
    Get actual numerical data from a statistical table

    Args:
        stats_data_id: Unique ID of the statistical table
        cd_cat01: Filter by category code 01
        cd_cat02: Filter by category code 02
        cd_cat03: Filter by category code 03
        cd_area: Filter by area/region code
        cd_time: Filter by time period code
        start_position: For pagination through large datasets

    Returns:
        JSON response with statistical data
    """
    try:
        params = {'statsDataId': stats_data_id}

        if cd_cat01:
            params['cdCat01'] = cd_cat01
        if cd_cat02:
            params['cdCat02'] = cd_cat02
        if cd_cat03:
            params['cdCat03'] = cd_cat03
        if cd_area:
            params['cdArea'] = cd_area
        if cd_time:
            params['cdTime'] = cd_time
        if start_position:
            params['startPosition'] = start_position

        result = _make_request("json/getStatsData", params)

        if result["error"]:
            return result

        # Parse and enhance data
        parsed_data = _parse_stats_data(result["data"])
        result["data"] = parsed_data
        result["metadata"]["stats_data_id"] = stats_data_id
        result["metadata"]["filters"] = {
            "cd_cat01": cd_cat01,
            "cd_cat02": cd_cat02,
            "cd_cat03": cd_cat03,
            "cd_area": cd_area,
            "cd_time": cd_time,
            "start_position": start_position
        }
        result["metadata"]["data_points_count"] = len(parsed_data.get("data_values", []))
        result["metadata"]["description"] = f"Statistical data for table {stats_data_id}"

        return result

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching stats data: {str(e)}"
        }

def get_all_data_for_table(stats_data_id: str) -> Dict[str, Any]:
    """
    Get all data for a statistical table (no filters)

    Args:
        stats_data_id: Unique ID of the statistical table

    Returns:
        JSON response with all table data
    """
    return get_stats_data(stats_data_id)

def get_data_by_area(stats_data_id: str, area_code: str) -> Dict[str, Any]:
    """
    Get statistical data for a specific area

    Args:
        stats_data_id: Unique ID of the statistical table
        area_code: Area/region code

    Returns:
        JSON response with area-specific data
    """
    return get_stats_data(stats_data_id, cd_area=area_code)

def get_data_by_time(stats_data_id: str, time_code: str) -> Dict[str, Any]:
    """
    Get statistical data for a specific time period

    Args:
        stats_data_id: Unique ID of the statistical table
        time_code: Time period code

    Returns:
        JSON response with time-specific data
    """
    return get_stats_data(stats_data_id, cd_time=time_code)

# ====== UTILITY FUNCTIONS ======
def search_statistics(keyword: str, limit: int = 20) -> Dict[str, Any]:
    """
    Comprehensive search across all statistics

    Args:
        keyword: Search keyword
        limit: Number of results to return

    Returns:
        JSON response with search results
    """
    return get_stats_list(search_word=keyword, limit=limit)

def get_population_data(limit: int = 10) -> Dict[str, Any]:
    """
    Get population-related statistics

    Args:
        limit: Number of results to return

    Returns:
        JSON response with population statistics
    """
    return get_stats_list(search_word="population", limit=limit)

def get_gdp_data(limit: int = 10) -> Dict[str, Any]:
    """
    Get GDP-related statistics

    Args:
        limit: Number of results to return

    Returns:
        JSON response with GDP statistics
    """
    return get_stats_list(search_word="GDP", limit=limit)

def get_labor_data(limit: int = 10) -> Dict[str, Any]:
    """
    Get labor/employment statistics

    Args:
        limit: Number of results to return

    Returns:
        JSON response with labor statistics
    """
    return get_stats_list(search_word="labor", limit=limit)

def get_table_summary(stats_data_id: str) -> Dict[str, Any]:
    """
    Get summary information about a statistical table

    Args:
        stats_data_id: Unique ID of the statistical table

    Returns:
        JSON response with table summary
    """
    try:
        # Get metadata first
        meta_result = get_meta_info(stats_data_id)
        if meta_result["error"]:
            return meta_result

        # Get sample data (first few records)
        data_result = get_stats_data(stats_data_id, start_position=1)

        summary = {
            "metadata": meta_result["data"],
            "sample_data": {
                "data_points_count": data_result["data"].get("data_values_count", 0),
                "sample_values": data_result["data"].get("data_values", [])[:5] if data_result["data"] else []
            }
        }

        return {
            "data": summary,
            "metadata": {
                "source": "e-Stat (Japan)",
                "stats_data_id": stats_data_id,
                "description": f"Summary for statistical table {stats_data_id}"
            },
            "error": None
        }

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error generating table summary: {str(e)}"
        }

# --- 3. CLI INTERFACE ---
def main():
    """CLI interface for e-Stat API"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python estat_japan_api.py <command> <args>",
            "available_commands": [
                "stats-list [keyword] [limit]",
                "stats-by-ministry <stats_code> [limit]",
                "stats-by-year <year> [limit]",
                "meta-info <stats_data_id>",
                "classifications <stats_data_id>",
                "stats-data <stats_data_id> [area_code] [time_code]",
                "all-data <stats_data_id>",
                "search <keyword> [limit]",
                "population [limit]",
                "gdp [limit]",
                "labor [limit]",
                "table-summary <stats_data_id>"
            ],
            "examples": [
                "python estat_japan_api.py stats-list population 10",
                "python estat_japan_api.py stats-by-ministry 00200 5",
                "python estat_japan_api.py meta-info 0003410379",
                "python estat_japan_api.py stats-data 0003410379 00000 2020",
                "python estat_japan_api.py search GDP 15",
                "python estat_japan_api.py population 5",
                "python estat_japan_api.py table-summary 0003410379"
            ],
            "note": "Set ESTAT_APP_ID environment variable with your application ID from https://www.e-stat.go.jp/api/en/api-info/user-guide"
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "stats-list":
            keyword = sys.argv[2] if len(sys.argv) > 2 else None
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = get_stats_list(search_word=keyword, limit=limit)

        elif command == "stats-by-ministry":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: stats-by-ministry <stats_code> [limit]"}))
                sys.exit(1)
            stats_code = sys.argv[2]
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = get_stats_list_by_ministry(stats_code, limit)

        elif command == "stats-by-year":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: stats-by-year <year> [limit]"}))
                sys.exit(1)
            year = sys.argv[2]
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 100
            result = get_stats_list_by_year(year, limit)

        elif command == "meta-info":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: meta-info <stats_data_id>"}))
                sys.exit(1)
            stats_data_id = sys.argv[2]
            result = get_meta_info(stats_data_id)

        elif command == "classifications":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: classifications <stats_data_id>"}))
                sys.exit(1)
            stats_data_id = sys.argv[2]
            result = get_table_classifications(stats_data_id)

        elif command == "stats-data":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: stats-data <stats_data_id> [area_code] [time_code]"}))
                sys.exit(1)
            stats_data_id = sys.argv[2]
            area_code = sys.argv[3] if len(sys.argv) > 3 else None
            time_code = sys.argv[4] if len(sys.argv) > 4 else None
            result = get_stats_data(stats_data_id, cd_area=area_code, cd_time=time_code)

        elif command == "all-data":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: all-data <stats_data_id>"}))
                sys.exit(1)
            stats_data_id = sys.argv[2]
            result = get_all_data_for_table(stats_data_id)

        elif command == "search":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: search <keyword> [limit]"}))
                sys.exit(1)
            keyword = sys.argv[2]
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 20
            result = search_statistics(keyword, limit)

        elif command == "population":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 10
            result = get_population_data(limit)

        elif command == "gdp":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 10
            result = get_gdp_data(limit)

        elif command == "labor":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else 10
            result = get_labor_data(limit)

        elif command == "table-summary":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: table-summary <stats_data_id>"}))
                sys.exit(1)
            stats_data_id = sys.argv[2]
            result = get_table_summary(stats_data_id)

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "stats-list", "stats-by-ministry", "stats-by-year", "meta-info",
                    "classifications", "stats-data", "all-data", "search",
                    "population", "gdp", "labor", "table-summary"
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