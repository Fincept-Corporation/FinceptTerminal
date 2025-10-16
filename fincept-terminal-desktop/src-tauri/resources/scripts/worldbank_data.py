#!/usr/bin/env python3
"""
World Bank Data Fetcher
Fetches economic, development, and commodity data from the World Bank Open Data API
Returns JSON output for Rust integration

API Documentation:
- Base URL: https://api.worldbank.org/v2
- Main endpoints: countries, indicators, sources
- Format: Returns XML by default, but we request JSON
- No API key required for public data
"""

import sys
import json
import requests
from typing import Dict, List, Optional, Any, Union
from datetime import datetime, timedelta
import os

# Configuration
BASE_URL = "https://api.worldbank.org/v2"
DEFAULT_FORMAT = "json"
TIMEOUT = 30

# Common World Bank Indicator Codes
GDP_PER_CAPITA = "NY.GDP.PCAP.CD"
GDP_GROWTH = "NY.GDP.MKTP.KD.ZG"
INFLATION = "FP.CPI.TOTL.ZG"
UNEMPLOYMENT = "SL.UEM.TOTL.ZS"
POPULATION = "SP.POP.TOTL"
LIFE_EXPECTANCY = "SP.DYN.LE00.IN"

# Commodity Indicator Codes (Pink Sheet)
CRUDE_OIL_BRENT = "PCRUDE_BRENT"
CRUDE_OIL_DUBAI = "PCRUDE_DUBAI"
CRUDE_OIL_WTI = "PCRUDE_WTI"
COPPER = "PCOPPER"
GOLD = "PGOLD"
COFFEE_ROBUSTA = "PCOFF_ROBUSTA"
COFFEE_ARABICA = "PCOFF_ARABICA"
WHEAT = "PWHEAMT"
CORN = "PMAIZMT"
RICE = "PRICENPQ"

# Region Codes
AFRICA = "AFR"
EAST_ASIA_PACIFIC = "EAP"
EUROPE_CENTRAL_ASIA = "ECA"
LATIN_AMERICA_CARIBBEAN = "LAC"
MIDDLE_EAST_NORTH_AFRICA = "MENA"
SOUTH_ASIA = "SAS"
NORTH_AMERICA = "NAC"

def _make_request(endpoint: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for World Bank API

    Args:
        endpoint: API endpoint path
        params: Query parameters for the request

    Returns:
        JSON response data or error information
    """
    try:
        full_url = f"{BASE_URL}/{endpoint}"

        # Default parameters
        if params is None:
            params = {}

        params['format'] = DEFAULT_FORMAT

        # Make request with timeout
        response = requests.get(full_url, params=params, timeout=TIMEOUT)
        response.raise_for_status()

        data = response.json()

        # World Bank API returns array where first element contains metadata
        if isinstance(data, list) and len(data) >= 2:
            metadata = data[0] if data[0] else {}
            actual_data = data[1] if len(data) > 1 else []

            return {
                "data": actual_data,
                "metadata": {
                    "page": metadata.get("page", 1),
                    "pages": metadata.get("pages", 1),
                    "per_page": metadata.get("per_page", 50),
                    "total": metadata.get("total", 0),
                    "source": metadata.get("source", "World Bank"),
                    "last_updated": metadata.get("lastupdated", datetime.now().isoformat())
                },
                "error": None
            }
        else:
            return {
                "data": data,
                "metadata": {"source": "World Bank", "last_updated": datetime.now().isoformat()},
                "error": None
            }

    except requests.exceptions.HTTPError as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"HTTP Error: {e.response.status_code} - {e.response.text}"
        }
    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {},
            "error": "Request timeout. The World Bank API is taking too long to respond."
        }
    except requests.exceptions.ConnectionError:
        return {
            "data": [],
            "metadata": {},
            "error": "Connection error. Unable to connect to World Bank API."
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
            "error": "Invalid JSON response from World Bank API"
        }
    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"An unexpected error occurred: {str(e)}"
        }

def normalize_country_codes(countries: Union[str, List[str]]) -> str:
    """
    Normalize country codes to World Bank format

    Args:
        countries: Country code(s) as string or list

    Returns:
        Semicolon-separated country codes string
    """
    if isinstance(countries, list):
        return ";".join(countries)
    return countries

def get_countries(region: Optional[str] = None, income_level: Optional[str] = None) -> Dict[str, Any]:
    """
    Get list of countries available in World Bank database

    Args:
        region: Optional region code (e.g., 'AFR', 'LAC')
        income_level: Optional income level filter

    Returns:
        JSON response with country information
    """
    try:
        endpoint = "countries"
        params = {}

        if region:
            params['region'] = region
        if income_level:
            params['incomeLevel'] = income_level

        params['per_page'] = 300  # Get all countries

        result = _make_request(endpoint, params)

        if result["error"]:
            return result

        # Enhance country data with additional info
        enhanced_data = []
        for country in result.get("data", []):
            enhanced_country = {
                "id": country.get("id"),
                "iso2Code": country.get("iso2Code"),
                "name": country.get("name"),
                "region": country.get("region", {}).get("value"),
                "income_level": country.get("incomeLevel", {}).get("value"),
                "lending_type": country.get("lendingType", {}).get("value"),
                "capital_city": country.get("capitalCity"),
                "longitude": country.get("longitude"),
                "latitude": country.get("latitude")
            }
            enhanced_data.append(enhanced_country)

        result["data"] = enhanced_data
        result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching countries: {str(e)}"
        }

def get_sources() -> Dict[str, Any]:
    """
    Get list of all data sources available in World Bank database

    Returns:
        JSON response with source information
    """
    try:
        endpoint = "sources"
        params = {"per_page": 100}  # Get all sources

        result = _make_request(endpoint, params)

        if result["error"]:
            return result

        # Enhance source data
        enhanced_data = []
        for source in result.get("data", []):
            enhanced_source = {
                "id": source.get("id"),
                "name": source.get("name"),
                "description": source.get("description"),
                "url": source.get("url"),
                "data_coverage": source.get("datacoverage"),
                "last_updated": source.get("lastupdated")
            }
            enhanced_data.append(enhanced_source)

        result["data"] = enhanced_data
        result["metadata"]["count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching sources: {str(e)}"
        }

def get_indicators(country_code: str = "all", indicator: str = GDP_PER_CAPITA,
                  date_range: Optional[str] = None, per_page: int = 1000) -> Dict[str, Any]:
    """
    Get indicator data for specified countries and time period

    Args:
        country_code: Country code(s) (default: 'all')
        indicator: World Bank indicator code (default: GDP per capita)
        date_range: Date range in format 'YYYY:YYYY' (optional)
        per_page: Number of results per page (default: 1000)

    Returns:
        JSON response with indicator data
    """
    try:
        # Normalize country codes
        normalized_countries = normalize_country_codes(country_code)

        # Build endpoint
        endpoint = f"country/{normalized_countries}/indicator/{indicator}"

        params = {"per_page": per_page}

        if date_range:
            params['date'] = date_range

        result = _make_request(endpoint, params)

        if result["error"]:
            return result

        # Enhance indicator data
        enhanced_data = []
        for item in result.get("data", []):
            enhanced_item = {
                "indicator_id": item.get("indicator", {}).get("id"),
                "indicator_name": item.get("indicator", {}).get("value"),
                "country_id": item.get("country", {}).get("id"),
                "country_name": item.get("country", {}).get("value"),
                "date": item.get("date"),
                "value": item.get("value"),
                "unit": item.get("unit"),
                "obs_status": item.get("obs_status"),
                "decimal": item.get("decimal")
            }
            enhanced_data.append(enhanced_item)

        result["data"] = enhanced_data
        result["metadata"]["indicator"] = indicator
        result["metadata"]["countries"] = country_code
        result["metadata"]["observation_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching indicator data: {str(e)}"
        }

def get_gdp_per_capita(countries: str = "USA,CHN,IND", years: int = 20) -> Dict[str, Any]:
    """
    Get GDP per capita data for specified countries

    Args:
        countries: Comma-separated country codes
        years: Number of years to look back

    Returns:
        JSON response with GDP per capita data
    """
    try:
        # Calculate date range
        current_year = datetime.now().year
        start_year = current_year - years
        date_range = f"{start_year}:{current_year}"

        return get_indicators(countries, GDP_PER_CAPITA, date_range)

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching GDP per capita: {str(e)}"
        }

def get_commodity_prices(commodity_code: str = CRUDE_OIL_BRENT, years: int = 10) -> Dict[str, Any]:
    """
    Get commodity price data (Pink Sheet)

    Args:
        commodity_code: World Bank commodity indicator code
        years: Number of years to look back

    Returns:
        JSON response with commodity price data
    """
    try:
        # Calculate date range
        current_year = datetime.now().year
        start_year = current_year - years
        date_range = f"{start_year}:{current_year}"

        # Commodity data uses a different endpoint format
        endpoint = f"country/all/indicator/{commodity_code}"
        params = {
            "date": date_range,
            "per_page": 1000,
            "frequency": "M"  # Monthly data for commodities
        }

        result = _make_request(endpoint, params)

        if result["error"]:
            return result

        # Enhance commodity data
        enhanced_data = []
        for item in result.get("data", []):
            enhanced_item = {
                "commodity_code": commodity_code,
                "date": item.get("date"),
                "value": item.get("value"),
                "unit": item.get("unit", "USD"),
                "obs_status": item.get("obs_status"),
                "decimal": item.get("decimal")
            }
            enhanced_data.append(enhanced_item)

        result["data"] = enhanced_data
        result["metadata"]["commodity"] = commodity_code
        result["metadata"]["observation_count"] = len(enhanced_data)

        return result

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching commodity prices: {str(e)}"
        }

def get_economic_snapshot(country_code: str = "USA") -> Dict[str, Any]:
    """
    Get comprehensive economic snapshot for a country

    Args:
        country_code: World Bank country code

    Returns:
        JSON response with multiple economic indicators
    """
    try:
        # Key economic indicators
        indicators = [
            GDP_PER_CAPITA,
            GDP_GROWTH,
            INFLATION,
            UNEMPLOYMENT,
            POPULATION,
            LIFE_EXPECTANCY
        ]

        # Get recent data (last 5 years)
        current_year = datetime.now().year
        start_year = current_year - 5
        date_range = f"{start_year}:{current_year}"

        snapshot_data = {}
        errors = []

        for indicator in indicators:
            try:
                result = get_indicators(country_code, indicator, date_range, 100)
                if result["error"]:
                    errors.append(f"{indicator}: {result['error']}")
                else:
                    # Get the most recent value
                    data = result.get("data", [])
                    if data:
                        latest = data[0]  # Data is typically returned in descending date order
                        snapshot_data[indicator] = {
                            "name": latest.get("indicator_name"),
                            "value": latest.get("value"),
                            "date": latest.get("date"),
                            "unit": latest.get("unit")
                        }
            except Exception as e:
                errors.append(f"{indicator}: {str(e)}")

        return {
            "data": snapshot_data,
            "metadata": {
                "country_code": country_code,
                "date_range": date_range,
                "indicators_requested": len(indicators),
                "indicators_success": len(snapshot_data),
                "last_updated": datetime.now().isoformat()
            },
            "error": "; ".join(errors) if errors else None
        }

    except Exception as e:
        return {
            "data": {},
            "metadata": {},
            "error": f"Error fetching economic snapshot: {str(e)}"
        }

def get_regional_comparison(region_code: str, indicator: str = GDP_PER_CAPITA, years: int = 5) -> Dict[str, Any]:
    """
    Get indicator data for all countries in a region

    Args:
        region_code: World Bank region code
        indicator: Indicator code to compare
        years: Number of years to look back

    Returns:
        JSON response with regional comparison data
    """
    try:
        # First get all countries in the region
        countries_result = get_countries(region=region_code)

        if countries_result["error"]:
            return countries_result

        countries = countries_result.get("data", [])
        country_codes = [country["id"] for country in countries]

        if not country_codes:
            return {
                "data": [],
                "metadata": {},
                "error": f"No countries found for region: {region_code}"
            }

        # Get indicator data for all countries in the region
        current_year = datetime.now().year
        start_year = current_year - years
        date_range = f"{start_year}:{current_year}"

        # World Bank API has limits on country codes per request, so we'll batch them
        batch_size = 20
        all_data = []
        errors = []

        for i in range(0, len(country_codes), batch_size):
            batch = country_codes[i:i + batch_size]
            country_batch = ";".join(batch)

            result = get_indicators(country_batch, indicator, date_range, 2000)
            if result["error"]:
                errors.append(f"Batch {i//batch_size + 1}: {result['error']}")
            else:
                all_data.extend(result.get("data", []))

        return {
            "data": all_data,
            "metadata": {
                "region_code": region_code,
                "indicator": indicator,
                "date_range": date_range,
                "countries_in_region": len(country_codes),
                "observation_count": len(all_data),
                "last_updated": datetime.now().isoformat()
            },
            "error": "; ".join(errors) if errors else None
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Error fetching regional comparison: {str(e)}"
        }

def main():
    """CLI interface for World Bank Data Fetcher"""

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python worldbank_data.py <command> <args>",
            "available_commands": [
                "countries [region_code]",
                "sources",
                "indicators <country_code> <indicator_code> [date_range]",
                "gdp_per_capita <country_codes> [years]",
                "commodity_prices <commodity_code> [years]",
                "economic_snapshot <country_code>",
                "regional_comparison <region_code> <indicator_code> [years]"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "countries":
            region = sys.argv[2] if len(sys.argv) > 2 else None
            result = get_countries(region)

        elif command == "sources":
            result = get_sources()

        elif command == "indicators":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: indicators <country_code> <indicator_code> [date_range]"}))
                sys.exit(1)

            country_code = sys.argv[2]
            indicator = sys.argv[3]
            date_range = sys.argv[4] if len(sys.argv) > 4 else None
            result = get_indicators(country_code, indicator, date_range)

        elif command == "gdp_per_capita":
            countries = sys.argv[2] if len(sys.argv) > 2 else "USA,CHN,IND"
            years = int(sys.argv[3]) if len(sys.argv) > 3 else 20
            result = get_gdp_per_capita(countries, years)

        elif command == "commodity_prices":
            commodity = sys.argv[2] if len(sys.argv) > 2 else CRUDE_OIL_BRENT
            years = int(sys.argv[3]) if len(sys.argv) > 3 else 10
            result = get_commodity_prices(commodity, years)

        elif command == "economic_snapshot":
            country = sys.argv[2] if len(sys.argv) > 2 else "USA"
            result = get_economic_snapshot(country)

        elif command == "regional_comparison":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: regional_comparison <region_code> <indicator_code> [years]"}))
                sys.exit(1)

            region = sys.argv[2]
            indicator = sys.argv[3]
            years = int(sys.argv[4]) if len(sys.argv) > 4 else 5
            result = get_regional_comparison(region, indicator, years)

        else:
            result = {
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "countries [region_code]",
                    "sources",
                    "indicators <country_code> <indicator_code> [date_range]",
                    "gdp_per_capita <country_codes> [years]",
                    "commodity_prices <commodity_code> [years]",
                    "economic_snapshot <country_code>",
                    "regional_comparison <region_code> <indicator_code> [years]"
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