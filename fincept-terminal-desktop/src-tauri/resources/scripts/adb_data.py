"""
Asian Development Bank (ADB) Key Indicators Database (KIDB) Data Fetcher
Fetches macroeconomic and social indicators from Asia-Pacific region
Returns JSON output for Rust integration

API Documentation: https://kidb.adb.org/api
Rate Limit: 30 queries per minute
"""

import sys
import json
import os
import requests
from typing import Dict, List, Optional, Any, Union
from datetime import datetime
import urllib.parse

# --- CONFIGURATION ---
BASE_URL = "https://kidb.adb.org/api/v4/sdmx"
TIMEOUT = 30
RATE_LIMIT_DELAY = 2.1  # ~28 queries per minute to stay under limit

# Common SDMX components
FREQUENCIES = {
    'A': 'Annual',
    'Q': 'Quarterly',
    'M': 'Monthly'
}

# Major dataflow codes (can be expanded)
DATAFLOWS = {
    'EO_NA': 'National Accounts',
    'PPL_POP': 'Population',
    'EO_FI': 'Financial Indicators',
    'EO_TR': 'Trade',
    'EO_PM': 'Price Management',
    'EO_GOV': 'Government Finance',
    'EO_EL': 'External Sector'
}

# Common economy codes (ISO 3-letter country codes)
ECONOMIES = {
    'PHI': 'Philippines',
    'SGP': 'Singapore',
    'JPN': 'Japan',
    'CHN': 'China',
    'IND': 'India',
    'KOR': 'Korea',
    'THA': 'Thailand',
    'MYS': 'Malaysia',
    'IDN': 'Indonesia',
    'VNM': 'Vietnam',
    'HKG': 'Hong Kong',
    'TWN': 'Taiwan',
    'AUS': 'Australia',
    'NZL': 'New Zealand',
    'all': 'All Economies'
}

def _make_request(endpoint: str, params: Optional[Dict[str, Any]] = None, format_type: str = "sdmx-json") -> Dict[str, Any]:
    """
    Centralized request handler with comprehensive error handling

    Args:
        endpoint: API endpoint path
        params: Query parameters
        format_type: Response format (sdmx-json, sdmx-csv, or xml)

    Returns:
        Dict with consistent structure: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    try:
        url = f"{BASE_URL}/{endpoint}"

        # Add format parameter if not already present
        if params is None:
            params = {}
        if 'format' not in params:
            params['format'] = format_type

        # Make request with timeout
        response = requests.get(url, params=params, timeout=TIMEOUT)
        response.raise_for_status()

        # Process response based on format
        if format_type == "sdmx-json":
            data = response.json()
        else:
            data = response.text

        # Return structured success response
        return {
            "data": data,
            "metadata": {
                "source": "Asian Development Bank (ADB) - Key Indicators Database",
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
                "source": "Asian Development Bank (ADB) - Key Indicators Database",
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
                "source": "Asian Development Bank (ADB) - Key Indicators Database",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": "Request timeout. The ADB API is taking too long to respond."
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {
                "source": "Asian Development Bank (ADB) - Key Indicators Database",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": "Connection error. Could not connect to ADB API."
        }
    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {
                "source": "Asian Development Bank (ADB) - Key Indicators Database",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": f"Request error: {str(e)}"
        }
    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {
                "source": "Asian Development Bank (ADB) - Key Indicators Database",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": "Invalid JSON response from ADB API."
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "source": "Asian Development Bank (ADB) - Key Indicators Database",
                "endpoint": endpoint,
                "parameters": params
            },
            "error": f"An unexpected error occurred: {str(e)}"
        }

def _parse_sdmx_data(sdmx_data: Dict[str, Any]) -> List[Dict[str, Any]]:
    """
    Parse SDMX-JSON data into a more usable format

    Args:
        sdmx_data: Raw SDMX-JSON response

    Returns:
        List of parsed data points
    """
    if not sdmx_data or 'data' not in sdmx_data:
        return []

    parsed_data = []

    try:
        # Extract data from SDMX structure
        data_sets = sdmx_data.get('data', {}).get('dataSets', [])
        series = sdmx_data.get('data', {}).get('series', {})
        structure = sdmx_data.get('data', {}).get('structure', {})

        if not data_sets or not series:
            return parsed_data

        # Get series information
        for series_key, series_data in series.items():
            observations = series_data.get('observations', {})
            series_attributes = series_data.get('seriesAttributes', {})

            # Extract dimensions from series key
            dimensions = series_key.split(':') if ':' in series_key else [series_key]

            # Parse each observation
            for obs_key, obs_value in observations.items():
                if obs_value and len(obs_value) > 0:
                    time_period = obs_key  # Usually in format like "2020"
                    value = obs_value[0] if isinstance(obs_value, list) else obs_value

                    data_point = {
                        "series_key": series_key,
                        "time_period": time_period,
                        "value": value,
                        "dimensions": dimensions,
                        "attributes": series_attributes
                    }
                    parsed_data.append(data_point)

    except Exception as e:
        print(f"Error parsing SDMX data: {e}")
        # Return empty list if parsing fails
        pass

    return parsed_data

# --- CORE API FUNCTIONS ---

def get_all_dataflows() -> Dict[str, Any]:
    """
    Get all available dataflows from ADB KIDB
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    endpoint = "structure/ADB,all,latest"
    result = _make_request(endpoint)

    # Enhance metadata
    if result['metadata']:
        result['metadata']['description'] = "All available dataflows in ADB Key Indicators Database"
        result['metadata']['total_dataflows'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def get_all_codelists() -> Dict[str, Any]:
    """
    Get all available codelists from ADB KIDB
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    endpoint = "codelist/ADB,all,latest"
    result = _make_request(endpoint)

    # Enhance metadata
    if result['metadata']:
        result['metadata']['description'] = "All available codelists in ADB Key Indicators Database"
        result['metadata']['total_codelists'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def get_dataflow_details(dataflow_code: str) -> Dict[str, Any]:
    """
    Get detailed information for a specific dataflow
    Args:
        dataflow_code: Dataflow code (e.g., 'EO_NA', 'PPL_POP')
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    endpoint = f"dataflow/ADB/{dataflow_code}/latest"
    result = _make_request(endpoint)

    # Enhance metadata
    if result['metadata']:
        result['metadata']['dataflow_code'] = dataflow_code
        result['metadata']['dataflow_name'] = DATAFLOWS.get(dataflow_code, 'Unknown')
        result['metadata']['description'] = f"Details for dataflow {dataflow_code} ({DATAFLOWS.get(dataflow_code, 'Unknown')})"

    return result

def get_population_data(economy: str = "all", start_period: Optional[str] = None, end_period: Optional[str] = None) -> Dict[str, Any]:
    """
    Get population data for specified economy/economies
    Args:
        economy: Economy code (e.g., 'PHI', 'SGP', or 'all')
        start_period: Start year (e.g., '2010')
        end_period: End year (e.g., '2020')
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    # Build SDMX key: A.PPL_POP.FREQUENCY.all.ECONOMY
    sdmx_key = f"A..{economy}"
    endpoint = f"data/ADB,PPL_POP/{sdmx_key}"

    params = {}
    if start_period:
        params['startPeriod'] = start_period
    if end_period:
        params['endPeriod'] = end_period

    result = _make_request(endpoint, params)

    # Parse and enhance data
    if result['data'] and not result['error']:
        parsed_data = _parse_sdmx_data(result['data'])
        result['data'] = parsed_data

    if result['metadata']:
        result['metadata']['economy'] = economy
        result['metadata']['economy_name'] = ECONOMIES.get(economy, 'Unknown')
        result['metadata']['start_period'] = start_period
        result['metadata']['end_period'] = end_period
        result['metadata']['dataflow'] = 'PPL_POP'
        result['metadata']['description'] = f"Population data for {ECONOMIES.get(economy, economy)}"
        result['metadata']['total_records'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def get_gdp_data(economy: str, indicator: str = "NGDP_XDC", start_period: Optional[str] = None, end_period: Optional[str] = None) -> Dict[str, Any]:
    """
    Get GDP data for specified economy and indicator
    Args:
        economy: Economy code (e.g., 'PHI', 'SGP')
        indicator: Indicator code (default: 'NGDP_XDC' - Gross Domestic Product, current prices)
        start_period: Start year (e.g., '2010')
        end_period: End year (e.g., '2020')
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    # Build SDMX key: A.EO_NA.INDICATOR.ECONOMY
    sdmx_key = f"A.{indicator}.{economy}"
    endpoint = f"data/ADB,EO_NA/{sdmx_key}"

    params = {}
    if start_period:
        params['startPeriod'] = start_period
    if end_period:
        params['endPeriod'] = end_period

    result = _make_request(endpoint, params)

    # Parse and enhance data
    if result['data'] and not result['error']:
        parsed_data = _parse_sdmx_data(result['data'])
        result['data'] = parsed_data

    if result['metadata']:
        result['metadata']['economy'] = economy
        result['metadata']['economy_name'] = ECONOMIES.get(economy, 'Unknown')
        result['metadata']['indicator'] = indicator
        result['metadata']['start_period'] = start_period
        result['metadata']['end_period'] = end_period
        result['metadata']['dataflow'] = 'EO_NA'
        result['metadata']['description'] = f"GDP data for {ECONOMIES.get(economy, economy)} - {indicator}"
        result['metadata']['total_records'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def get_multiple_indicators(economy: str, indicators: List[str], start_period: Optional[str] = None, end_period: Optional[str] = None) -> Dict[str, Any]:
    """
    Get multiple indicators for specified economy
    Args:
        economy: Economy code (e.g., 'PHI', 'SGP')
        indicators: List of indicator codes (e.g., ['NGDP_XDC', 'NGDPVA_XDC'])
        start_period: Start year (e.g., '2010')
        end_period: End year (e.g., '2020')
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    if not indicators:
        return {
            "data": [],
            "metadata": {},
            "error": "At least one indicator must be specified"
        }

    # Build SDMX key: A.EO_NA.INDICATOR1+INDICATOR2.ECONOMY
    indicators_str = "+".join(indicators)
    sdmx_key = f"A.{indicators_str}.{economy}"
    endpoint = f"data/ADB,EO_NA/{sdmx_key}"

    params = {}
    if start_period:
        params['startPeriod'] = start_period
    if end_period:
        params['endPeriod'] = end_period

    result = _make_request(endpoint, params)

    # Parse and enhance data
    if result['data'] and not result['error']:
        parsed_data = _parse_sdmx_data(result['data'])
        result['data'] = parsed_data

    if result['metadata']:
        result['metadata']['economy'] = economy
        result['metadata']['economy_name'] = ECONOMIES.get(economy, 'Unknown')
        result['metadata']['indicators'] = indicators
        result['metadata']['indicators_count'] = len(indicators)
        result['metadata']['start_period'] = start_period
        result['metadata']['end_period'] = end_period
        result['metadata']['dataflow'] = 'EO_NA'
        result['metadata']['description'] = f"Multiple indicators for {ECONOMIES.get(economy, economy)}: {', '.join(indicators)}"
        result['metadata']['total_records'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def get_multiple_economies_data(indicator: str, economies: List[str], start_period: Optional[str] = None, end_period: Optional[str] = None) -> Dict[str, Any]:
    """
    Get specific indicator for multiple economies
    Args:
        indicator: Indicator code (e.g., 'NGDP_XDC')
        economies: List of economy codes (e.g., ['PHI', 'SGP', 'JPN'])
        start_period: Start year (e.g., '2010')
        end_period: End year (e.g., '2020')
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    if not economies:
        return {
            "data": [],
            "metadata": {},
            "error": "At least one economy must be specified"
        }

    # Build SDMX key: A.EO_NA.INDICATOR.ECONOMY1+ECONOMY2
    economies_str = "+".join(economies)
    sdmx_key = f"A.{indicator}.{economies_str}"
    endpoint = f"data/ADB,EO_NA/{sdmx_key}"

    params = {}
    if start_period:
        params['startPeriod'] = start_period
    if end_period:
        params['endPeriod'] = end_period

    result = _make_request(endpoint, params)

    # Parse and enhance data
    if result['data'] and not result['error']:
        parsed_data = _parse_sdmx_data(result['data'])
        result['data'] = parsed_data

    if result['metadata']:
        result['metadata']['indicator'] = indicator
        result['metadata']['economies'] = economies
        result['metadata']['economies_count'] = len(economies)
        result['metadata']['economy_names'] = [ECONOMIES.get(econ, econ) for econ in economies]
        result['metadata']['start_period'] = start_period
        result['metadata']['end_period'] = end_period
        result['metadata']['dataflow'] = 'EO_NA'
        result['metadata']['description'] = f"Indicator {indicator} for economies: {', '.join([ECONOMIES.get(econ, econ) for econ in economies])}"
        result['metadata']['total_records'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def get_financial_indicators(economy: str, start_period: Optional[str] = None, end_period: Optional[str] = None) -> Dict[str, Any]:
    """
    Get financial indicators for specified economy
    Args:
        economy: Economy code (e.g., 'PHI', 'SGP')
        start_period: Start year (e.g., '2010')
        end_period: End year (e.g., '2020')
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    # Build SDMX key: A.EO_FI.all.ECONOMY
    sdmx_key = f"A..{economy}"
    endpoint = f"data/ADB,EO_FI/{sdmx_key}"

    params = {}
    if start_period:
        params['startPeriod'] = start_period
    if end_period:
        params['endPeriod'] = end_period

    result = _make_request(endpoint, params)

    # Parse and enhance data
    if result['data'] and not result['error']:
        parsed_data = _parse_sdmx_data(result['data'])
        result['data'] = parsed_data

    if result['metadata']:
        result['metadata']['economy'] = economy
        result['metadata']['economy_name'] = ECONOMIES.get(economy, 'Unknown')
        result['metadata']['start_period'] = start_period
        result['metadata']['end_period'] = end_period
        result['metadata']['dataflow'] = 'EO_FI'
        result['metadata']['description'] = f"Financial indicators for {ECONOMIES.get(economy, economy)}"
        result['metadata']['total_records'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def get_trade_data(economy: str, start_period: Optional[str] = None, end_period: Optional[str] = None) -> Dict[str, Any]:
    """
    Get trade data for specified economy
    Args:
        economy: Economy code (e.g., 'PHI', 'SGP')
        start_period: Start year (e.g., '2010')
        end_period: End year (e.g., '2020')
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    # Build SDMX key: A.EO_TR.all.ECONOMY
    sdmx_key = f"A..{economy}"
    endpoint = f"data/ADB,EO_TR/{sdmx_key}"

    params = {}
    if start_period:
        params['startPeriod'] = start_period
    if end_period:
        params['endPeriod'] = end_period

    result = _make_request(endpoint, params)

    # Parse and enhance data
    if result['data'] and not result['error']:
        parsed_data = _parse_sdmx_data(result['data'])
        result['data'] = parsed_data

    if result['metadata']:
        result['metadata']['economy'] = economy
        result['metadata']['economy_name'] = ECONOMIES.get(economy, 'Unknown')
        result['metadata']['start_period'] = start_period
        result['metadata']['end_period'] = end_period
        result['metadata']['dataflow'] = 'EO_TR'
        result['metadata']['description'] = f"Trade data for {ECONOMIES.get(economy, economy)}"
        result['metadata']['total_records'] = len(result['data']) if isinstance(result['data'], list) else 0

    return result

def search_datasets(keyword: str) -> Dict[str, Any]:
    """
    Search for datasets containing keyword (basic implementation)
    Args:
        keyword: Search keyword
    Returns: {"data": [...], "metadata": {...}, "error": None/error_msg}
    """
    # This is a basic search - in reality would need more sophisticated logic
    # For now, we'll get all dataflows and filter by keyword
    dataflows_result = get_all_dataflows()

    if dataflows_result['error']:
        return dataflows_result

    # Filter dataflows by keyword
    matching_dataflows = []
    if isinstance(dataflows_result['data'], dict) and 'dataflows' in dataflows_result['data']:
        all_dataflows = dataflows_result['data']['dataflows']
        for dataflow in all_dataflows:
            if 'name' in dataflow and keyword.lower() in str(dataflow['name']).lower():
                matching_dataflows.append(dataflow)
            elif 'description' in dataflow and keyword.lower() in str(dataflow['description']).lower():
                matching_dataflows.append(dataflow)

    return {
        "data": matching_dataflows,
        "metadata": {
            "source": "Asian Development Bank (ADB) - Key Indicators Database",
            "search_keyword": keyword,
            "total_matches": len(matching_dataflows),
            "description": f"Dataflows matching keyword: {keyword}",
            "last_updated": datetime.now().isoformat()
        },
        "error": None
    }

# --- CLI INTERFACE ---

def main():
    """Main CLI entry point for ADB KIDB Data Fetcher"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python adb_data.py <command> <args>",
            "available_commands": [
                "get_dataflows",
                "get_codelists",
                "get_dataflow_details <dataflow_code>",
                "get_population <economy_code> [start_year] [end_year]",
                "get_gdp <economy_code> [indicator] [start_year] [end_year]",
                "get_multiple_indicators <economy_code> <indicator1,indicator2,...> [start_year] [end_year]",
                "get_multiple_economies <indicator> <economy1,economy2,...> [start_year] [end_year]",
                "get_financial <economy_code> [start_year] [end_year]",
                "get_trade <economy_code> [start_year] [end_year]",
                "search <keyword>"
            ],
            "examples": [
                "python adb_data.py get_dataflows",
                "python adb_data.py get_population PHI 2010 2020",
                "python adb_data.py get_gdp SGP NGDP_XDC 2015 2020",
                "python adb_data.py get_multiple_indicators PHI NGDP_XDC,NGDPVA_XDC 2010 2020",
                "python adb_data.py get_multiple_economies NGDP_XDC PHI,SGP,JPN 2015 2020",
                "python adb_data.py search population"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "get_dataflows":
            result = get_all_dataflows()

        elif command == "get_codelists":
            result = get_all_codelists()

        elif command == "get_dataflow_details":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python adb_data.py get_dataflow_details <dataflow_code>"}))
                sys.exit(1)
            dataflow_code = sys.argv[2]
            result = get_dataflow_details(dataflow_code)

        elif command == "get_population":
            economy = sys.argv[2] if len(sys.argv) > 2 else "all"
            start_period = sys.argv[3] if len(sys.argv) > 3 else None
            end_period = sys.argv[4] if len(sys.argv) > 4 else None
            result = get_population_data(economy, start_period, end_period)

        elif command == "get_gdp":
            economy = sys.argv[2] if len(sys.argv) > 2 else None
            indicator = sys.argv[3] if len(sys.argv) > 3 else "NGDP_XDC"
            start_period = sys.argv[4] if len(sys.argv) > 4 else None
            end_period = sys.argv[5] if len(sys.argv) > 5 else None
            if not economy:
                print(json.dumps({"error": "Economy code is required for GDP data"}))
                sys.exit(1)
            result = get_gdp_data(economy, indicator, start_period, end_period)

        elif command == "get_multiple_indicators":
            economy = sys.argv[2] if len(sys.argv) > 2 else None
            indicators_str = sys.argv[3] if len(sys.argv) > 3 else None
            start_period = sys.argv[4] if len(sys.argv) > 4 else None
            end_period = sys.argv[5] if len(sys.argv) > 5 else None
            if not economy or not indicators_str:
                print(json.dumps({"error": "Economy code and indicators are required"}))
                sys.exit(1)
            indicators = [ind.strip() for ind in indicators_str.split(',')]
            result = get_multiple_indicators(economy, indicators, start_period, end_period)

        elif command == "get_multiple_economies":
            indicator = sys.argv[2] if len(sys.argv) > 2 else None
            economies_str = sys.argv[3] if len(sys.argv) > 3 else None
            start_period = sys.argv[4] if len(sys.argv) > 4 else None
            end_period = sys.argv[5] if len(sys.argv) > 5 else None
            if not indicator or not economies_str:
                print(json.dumps({"error": "Indicator and economies are required"}))
                sys.exit(1)
            economies = [econ.strip() for econ in economies_str.split(',')]
            result = get_multiple_economies_data(indicator, economies, start_period, end_period)

        elif command == "get_financial":
            economy = sys.argv[2] if len(sys.argv) > 2 else None
            start_period = sys.argv[3] if len(sys.argv) > 3 else None
            end_period = sys.argv[4] if len(sys.argv) > 4 else None
            if not economy:
                print(json.dumps({"error": "Economy code is required for financial data"}))
                sys.exit(1)
            result = get_financial_indicators(economy, start_period, end_period)

        elif command == "get_trade":
            economy = sys.argv[2] if len(sys.argv) > 2 else None
            start_period = sys.argv[3] if len(sys.argv) > 3 else None
            end_period = sys.argv[4] if len(sys.argv) > 4 else None
            if not economy:
                print(json.dumps({"error": "Economy code is required for trade data"}))
                sys.exit(1)
            result = get_trade_data(economy, start_period, end_period)

        elif command == "search":
            keyword = sys.argv[2] if len(sys.argv) > 2 else None
            if not keyword:
                print(json.dumps({"error": "Search keyword is required"}))
                sys.exit(1)
            result = search_datasets(keyword)

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "get_dataflows", "get_codelists", "get_dataflow_details",
                    "get_population", "get_gdp", "get_multiple_indicators",
                    "get_multiple_economies", "get_financial", "get_trade", "search"
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