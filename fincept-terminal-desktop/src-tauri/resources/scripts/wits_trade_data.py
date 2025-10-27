"""
World Bank WITS (World Integrated Trade Solution) API Wrapper
Fetches international trade, tariff, and non-tariff data from World Bank WITS platform
Returns JSON output for Rust integration

API Documentation: https://wits.worldbank.org/data/public/WITSAPI_UserGuide.pdf
Base URL: https://wits.worldbank.org/API/V1/

Usage:
    python wits_trade_data.py indicators
    python wits_trade_data.py trade-data --reporter=840 --partner=000 --year=2020
    python wits_trade_data.py tariff-data --reporter=840 --partner=000 --year=2020
    python wits_trade_data.py product-tariff --reporter=840 --partner=000 --product=020110 --year=2020
    python wits_trade_data.py catalog
"""

import sys
import json
import requests
import os
import xml.etree.ElementTree as ET
from typing import Dict, List, Optional, Any, Union
from datetime import datetime
import re

# Configuration
BASE_URL = "https://wits.worldbank.org/API/V1"
TIMEOUT = 30

# Common codes and mappings
PARTNER_CODES = {
    "000": "World",
    "840": "United States",
    "156": "China",
    "826": "United Kingdom",
    "276": "Germany",
    "124": "Canada",
    "392": "Japan",
    "380": "Italy",
    "250": "France"
}

REPORTER_CODES = {
    "000": "World",
    "840": "United States",
    "156": "China",
    "826": "United Kingdom",
    "276": "Germany",
    "124": "Canada",
    "392": "Japan",
    "380": "Italy",
    "250": "France"
}

DATASOURCES = {
    "tradestats-development": "Trade Statistics - Development",
    "trn": "Tariff Data",
    "tradestats-test": "Trade Statistics - Test"
}

PRODUCT_CATEGORIES = {
    "ALL": "All Products",
    "TOTAL": "Total Trade",
    "AG": "Agricultural Products",
    "FU": "Fuels",
    "MN": "Ores and Metals",
    "MF": "Manufactures"
}


def _make_request(url: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
    """
    Centralized request handler for WITS API calls with XML parsing

    Args:
        url: Complete API URL
        params: Query parameters (not used in URL-based WITS API)

    Returns:
        Standardized response dict with data, metadata, and error fields
    """
    try:
        headers = {
            'User-Agent': 'Fincept-Terminal/1.0 (wits-api-wrapper)',
            'Accept': 'application/xml,application/json'
        }

        response = requests.get(url, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        # Use bytes approach for XML parsing (handles BOM and encoding better)
        try:
            content_bytes = response.content
            # Remove UTF-8 BOM if present
            if content_bytes.startswith(b'\xef\xbb\xbf'):
                content_bytes = content_bytes[3:]

            root = ET.fromstring(content_bytes)

            # Extract data based on response type
            data = _parse_xml_response(root)

            if data is None:
                return {
                    "data": [],
                    "metadata": {
                        "source": "World Bank WITS",
                        "endpoint": url.split('/')[-1] if '/' in url else 'unknown',
                        "last_updated": datetime.now().isoformat(),
                        "action": f"fetched_{url.split('/')[-1]}",
                        "url": url
                    },
                    "error": "Failed to parse XML response"
                }

            return {
                "data": data,
                "metadata": {
                    "source": "World Bank WITS",
                    "endpoint": url.split('/')[-1] if '/' in url else 'unknown',
                    "last_updated": datetime.now().isoformat(),
                    "total_count": len(data) if isinstance(data, list) else 1,
                    "action": f"fetched_{url.split('/')[-1]}",
                    "url": url
                },
                "error": None
            }

        except ET.ParseError as e:
            return {
                "data": [],
                "metadata": {
                    "source": "World Bank WITS",
                    "endpoint": url.split('/')[-1] if '/' in url else 'unknown',
                    "last_updated": datetime.now().isoformat(),
                    "action": f"fetched_{url.split('/')[-1]}",
                    "url": url
                },
                "error": f"XML Parse Error: {str(e)}"
            }

    except requests.exceptions.HTTPError as e:
        error_msg = f"HTTP Error {e.response.status_code}"
        if e.response.status_code == 404:
            error_msg += " - Data not found or invalid parameters"
        elif e.response.status_code == 400:
            error_msg += " - Bad Request: Check your parameters"
        elif e.response.status_code >= 500:
            error_msg += " - Server error"

        return {
            "data": [],
            "metadata": {
                "source": "World Bank WITS",
                "endpoint": url.split('/')[-1] if '/' in url else 'unknown',
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{url.split('/')[-1]}",
                "url": url
            },
            "error": error_msg
        }

    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {
                "source": "World Bank WITS",
                "endpoint": url.split('/')[-1] if '/' in url else 'unknown',
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{url.split('/')[-1]}",
                "url": url
            },
            "error": f"Request timeout after {TIMEOUT} seconds"
        }

    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {
                "source": "World Bank WITS",
                "endpoint": url.split('/')[-1] if '/' in url else 'unknown',
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{url.split('/')[-1]}",
                "url": url
            },
            "error": f"Request failed: {str(e)}"
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "source": "World Bank WITS",
                "endpoint": url.split('/')[-1] if '/' in url else 'unknown',
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{url.split('/')[-1]}",
                "url": url
            },
            "error": f"Unexpected error: {str(e)}"
        }


def _parse_xml_response(root: ET.Element) -> Union[List[Dict[str, Any]], Dict[str, Any], None]:
    """
    Parse XML response based on the root element structure

    Args:
        root: XML root element

    Returns:
        Parsed data as list of dictionaries or single dictionary
    """
    # Check for different response types
    root_tag = root.tag.split('}')[-1] if '}' in root.tag else root.tag

    if root_tag == 'witsdata':
        # Trade statistics response
        return _parse_trade_statistics(root)
    elif root_tag == 'tariffdata':
        # Tariff data response
        return _parse_tariff_data(root)
    elif root_tag == 'data':
        # General data response
        return _parse_general_data(root)
    elif root_tag == 'Indicator':
        # Indicator response (capitalized)
        return _parse_indicator_data(root)
    elif root_tag == 'indicator':
        # Indicator response
        return _parse_indicator_data(root)
    elif root_tag == 'datasource':
        # WITS datasource response (contains indicators)
        return _parse_indicator_data(root)
    elif root_tag == 'GenericData':
        # Generic data response
        return _parse_generic_data(root)
    else:
        # Try generic parsing
        return _parse_generic_xml(root)


def _parse_trade_statistics(root: ET.Element) -> List[Dict[str, Any]]:
    """Parse trade statistics XML response"""
    data = []

    for record in root.findall('.//record'):
        item = {}
        for child in record:
            if child.text and child.text.strip():
                # Try to convert numeric values
                try:
                    # Handle decimal numbers
                    if '.' in child.text:
                        item[child.tag] = float(child.text)
                    else:
                        item[child.tag] = int(child.text)
                except ValueError:
                    # Keep as string if not numeric
                    item[child.tag] = child.text.strip()

        if item:
            # Add human-readable names for codes
            if 'reporter' in item:
                item['reporter_name'] = REPORTER_CODES.get(str(item['reporter']), f"Code {item['reporter']}")
            if 'partner' in item:
                item['partner_name'] = PARTNER_CODES.get(str(item['partner']), f"Code {item['partner']}")

            data.append(item)

    return data


def _parse_tariff_data(root: ET.Element) -> List[Dict[str, Any]]:
    """Parse tariff data XML response"""
    data = []

    for record in root.findall('.//record'):
        item = {}
        for child in record:
            if child.text and child.text.strip():
                try:
                    # Handle percentage values
                    if '%' in child.text:
                        item[child.tag] = float(child.text.replace('%', ''))
                    elif '.' in child.text:
                        item[child.tag] = float(child.text)
                    else:
                        item[child.tag] = int(child.text)
                except ValueError:
                    item[child.tag] = child.text.strip()

        if item:
            # Add human-readable names
            if 'reporter' in item:
                item['reporter_name'] = REPORTER_CODES.get(str(item['reporter']), f"Code {item['reporter']}")
            if 'partner' in item:
                item['partner_name'] = PARTNER_CODES.get(str(item['partner']), f"Code {item['partner']}")

            data.append(item)

    return data


def _parse_general_data(root: ET.Element) -> List[Dict[str, Any]]:
    """Parse general data XML response"""
    data = []

    for record in root.findall('.//record'):
        item = {}
        for child in record:
            if child.text and child.text.strip():
                item[child.tag] = child.text.strip()

        if item:
            data.append(item)

    return data


def _parse_indicator_data(root: ET.Element) -> List[Dict[str, Any]]:
    """Parse indicator data XML response"""
    data = []

    # Extract namespace from root tag
    namespace = {}
    if root.tag.startswith('{'):
        namespace_uri = root.tag.split('}')[0][1:]
        namespace = {'wits': namespace_uri}
    else:
        namespace = {'wits': 'http://wits.worldbank.org'}

    # Look for indicators using the correct namespace
    indicators = root.findall('.//wits:indicator', namespace)

  
    for indicator in indicators:
        item = {}

        # Extract attributes
        for attr in ['indicatorcode', 'ispartnerequired', 'SDMX_partnervalue', 'isproductrequired', 'SDMX_productvalue']:
            value = indicator.get(attr)
            if value:
                item[attr] = value.strip()

        # Extract text content from child elements
        for child in indicator:
            if child.text and child.text.strip():
                # Remove namespace from tag name if present
                tag_name = child.tag.split('}')[-1] if '}' in child.tag else child.tag
                item[tag_name] = child.text.strip()

        if item:  # Only add if we got some data
            data.append(item)

    # If no indicators found, try to parse as generic data
    if not data:
        return _parse_generic_data(root)

    return data


def _parse_generic_data(root: ET.Element) -> List[Dict[str, Any]]:
    """Parse generic data XML response"""
    data = []

    # Look for records, entries, or data elements
    for record in root.findall('.//record') or root.findall('.//entry') or root.findall('.//data'):
        item = {}
        for child in record:
            if child.text and child.text.strip():
                item[child.tag] = child.text.strip()
        if item:
            data.append(item)

    return data


def _parse_generic_xml(root: ET.Element) -> List[Dict[str, Any]]:
    """Generic XML parser for unknown response structures"""
    data = []

    # Find all direct children that might be records
    for child in root:
        if child.tag and child.text and child.text.strip():
            item = {child.tag: child.text.strip()}
            data.append(item)

    # If no data found, try deeper parsing
    if not data:
        return _parse_generic_data(root)

    return data


def get_indicators(datasource: str = "tradestats-development") -> Dict[str, Any]:
    """
    Get all available trade indicators from WITS

    Args:
        datasource: Data source to query (default: tradestats-development)

    Returns:
        Dict containing indicators data, metadata, and error information
    """
    url = f"{BASE_URL}/wits/datasource/{datasource}/indicator/ALL"
    return _make_request(url)


def get_trade_data(reporter: str, partner: str, product: str = "ALL", year: str = None,
                  datasource: str = "tradestats-development") -> Dict[str, Any]:
    """
    Get trade statistics data between countries

    Args:
        reporter: Reporter country code (e.g., "840" for USA)
        partner: Partner country code (e.g., "000" for World)
        product: Product code (e.g., "ALL" for all products)
        year: Year for data (e.g., "2020")
        datasource: Data source to query

    Returns:
        Dict containing trade data, metadata, and error information
    """
    # Default to current year if not specified
    if year is None:
        year = str(datetime.now().year - 1)  # Last full year

    url = f"{BASE_URL}/wits/datasource/{datasource}/country/{reporter}/partner/{partner}/product/{product}/year/{year}"

    # Add context to metadata
    result = _make_request(url)
    if result['metadata']:
        result['metadata']['context'] = {
            'reporter_code': reporter,
            'reporter_name': REPORTER_CODES.get(reporter, f"Code {reporter}"),
            'partner_code': partner,
            'partner_name': PARTNER_CODES.get(partner, f"Code {partner}"),
            'product_code': product,
            'year': year,
            'datasource': datasource,
            'datasource_name': DATASOURCES.get(datasource, datasource)
        }

    return result


def get_tariff_data(reporter: str, partner: str = "000", product: str = "ALL", year: str = None) -> Dict[str, Any]:
    """
    Get tariff data for imports

    Args:
        reporter: Reporter country code (e.g., "840" for USA)
        partner: Partner country code (default: "000" for World)
        product: Product code (default: "ALL" for all products)
        year: Year for data (e.g., "2020")

    Returns:
        Dict containing tariff data, metadata, and error information
    """
    # Default to current year if not specified
    if year is None:
        year = str(datetime.now().year - 1)  # Last full year

    url = f"{BASE_URL}/wits/datasource/trn/reporter/{reporter}/partner/{partner}/product/{product}/year/{year}"

    # Add context to metadata
    result = _make_request(url)
    if result['metadata']:
        result['metadata']['context'] = {
            'reporter_code': reporter,
            'reporter_name': REPORTER_CODES.get(reporter, f"Code {reporter}"),
            'partner_code': partner,
            'partner_name': PARTNER_CODES.get(partner, f"Code {partner}"),
            'product_code': product,
            'year': year,
            'datasource': 'trn',
            'datasource_name': 'Tariff Data'
        }

    return result


def get_product_tariff(reporter: str, partner: str, product: str, year: str,
                       datatype: str = "reported") -> Dict[str, Any]:
    """
    Get specific product tariff data using SDMX format

    Args:
        reporter: Reporter country code (e.g., "840" for USA)
        partner: Partner country code (e.g., "000" for World)
        product: Product HS code (e.g., "020110")
        year: Year for data (e.g., "2000")
        datatype: Data type (default: "reported")

    Returns:
        Dict containing product tariff data, metadata, and error information
    """
    url = f"{BASE_URL}/SDMX/V21/datasource/TRN/reporter/{reporter}/partner/{partner}/product/{product}/year/{year}/datatype/{datatype}"

    # Add context to metadata
    result = _make_request(url)
    if result['metadata']:
        result['metadata']['context'] = {
            'reporter_code': reporter,
            'reporter_name': REPORTER_CODES.get(reporter, f"Code {reporter}"),
            'partner_code': partner,
            'partner_name': PARTNER_CODES.get(partner, f"Code {partner}"),
            'product_code': product,
            'year': year,
            'datatype': datatype,
            'datasource': 'TRN',
            'datasource_name': 'Tariff Data (SDMX)'
        }

    return result


def get_catalog() -> Dict[str, Any]:
    """
    Get complete catalog of available data sources, countries, and products

    Returns:
        Dict containing catalog information, metadata, and error information
    """
    catalog = {
        "datasources": [
            {"code": code, "name": name} for code, name in DATASOURCES.items()
        ],
        "reporter_countries": [
            {"code": code, "name": name} for code, name in REPORTER_CODES.items()
        ],
        "partner_countries": [
            {"code": code, "name": name} for code, name in PARTNER_CODES.items()
        ],
        "product_categories": [
            {"code": code, "name": name} for code, name in PRODUCT_CATEGORIES.items()
        ]
    }

    return {
        "data": catalog,
        "metadata": {
            "source": "World Bank WITS",
            "endpoint": "catalog",
            "last_updated": datetime.now().isoformat(),
            "total_count": 1,
            "action": "catalog_retrieved"
        },
        "error": None
    }


def test_all_endpoints() -> Dict[str, Any]:
    """
    Test all major endpoints to ensure functionality

    Returns:
        Dict containing test results for all endpoints
    """
    test_results = {
        "tests": {},
        "summary": {"total": 0, "passed": 0, "failed": 0},
        "timestamp": datetime.now().isoformat()
    }

    # Test 1: Get indicators
    print("Testing: get_indicators()...")
    try:
        result = get_indicators()
        test_results["tests"]["indicators"] = {
            "status": "passed" if not result.get("error") else "failed",
            "error": result.get("error"),
            "data_count": len(result.get("data", [])) if isinstance(result.get("data"), list) else 1
        }
        test_results["summary"]["total"] += 1
        if not result.get("error"):
            test_results["summary"]["passed"] += 1
        else:
            test_results["summary"]["failed"] += 1
    except Exception as e:
        test_results["tests"]["indicators"] = {"status": "failed", "error": str(e)}
        test_results["summary"]["total"] += 1
        test_results["summary"]["failed"] += 1

    # Test 2: Get catalog
    print("Testing: get_catalog()...")
    try:
        result = get_catalog()
        test_results["tests"]["catalog"] = {
            "status": "passed" if not result.get("error") else "failed",
            "error": result.get("error"),
            "data_count": 1
        }
        test_results["summary"]["total"] += 1
        if not result.get("error"):
            test_results["summary"]["passed"] += 1
        else:
            test_results["summary"]["failed"] += 1
    except Exception as e:
        test_results["tests"]["catalog"] = {"status": "failed", "error": str(e)}
        test_results["summary"]["total"] += 1
        test_results["summary"]["failed"] += 1

    # Test 3: Get trade data (US to World, 2020)
    print("Testing: get_trade_data(US to World, 2020)...")
    try:
        result = get_trade_data("840", "000", "ALL", "2020")
        test_results["tests"]["trade_data"] = {
            "status": "passed" if not result.get("error") else "failed",
            "error": result.get("error"),
            "data_count": len(result.get("data", [])) if isinstance(result.get("data"), list) else 1
        }
        test_results["summary"]["total"] += 1
        if not result.get("error"):
            test_results["summary"]["passed"] += 1
        else:
            test_results["summary"]["failed"] += 1
    except Exception as e:
        test_results["tests"]["trade_data"] = {"status": "failed", "error": str(e)}
        test_results["summary"]["total"] += 1
        test_results["summary"]["failed"] += 1

    # Test 4: Get tariff data (US, 2020)
    print("Testing: get_tariff_data(US, 2020)...")
    try:
        result = get_tariff_data("840", "000", "ALL", "2020")
        test_results["tests"]["tariff_data"] = {
            "status": "passed" if not result.get("error") else "failed",
            "error": result.get("error"),
            "data_count": len(result.get("data", [])) if isinstance(result.get("data"), list) else 1
        }
        test_results["summary"]["total"] += 1
        if not result.get("error"):
            test_results["summary"]["passed"] += 1
        else:
            test_results["summary"]["failed"] += 1
    except Exception as e:
        test_results["tests"]["tariff_data"] = {"status": "failed", "error": str(e)}
        test_results["summary"]["total"] += 1
        test_results["summary"]["failed"] += 1

    # Test 5: Get product tariff (Beef from US to World, 2020)
    print("Testing: get_product_tariff(Beef from US to World, 2020)...")
    try:
        result = get_product_tariff("840", "000", "020110", "2020")
        test_results["tests"]["product_tariff"] = {
            "status": "passed" if not result.get("error") else "failed",
            "error": result.get("error"),
            "data_count": len(result.get("data", [])) if isinstance(result.get("data"), list) else 1
        }
        test_results["summary"]["total"] += 1
        if not result.get("error"):
            test_results["summary"]["passed"] += 1
        else:
            test_results["summary"]["failed"] += 1
    except Exception as e:
        test_results["tests"]["product_tariff"] = {"status": "failed", "error": str(e)}
        test_results["summary"]["total"] += 1
        test_results["summary"]["failed"] += 1

    return test_results


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python wits_trade_data.py <command> [options]",
            "available_commands": [
                "indicators [--datasource=tradestats-development]",
                "trade-data --reporter=<code> --partner=<code> [--product=<code>] [--year=<year>] [--datasource=<source>]",
                "tariff-data --reporter=<code> [--partner=<code>] [--product=<code>] [--year=<year>]",
                "product-tariff --reporter=<code> --partner=<code> --product=<code> --year=<year> [--datatype=<type>]",
                "catalog",
                "test-all"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    result = None

    if command == "indicators":
        datasource = "tradestats-development"

        # Parse arguments
        for arg in sys.argv[2:]:
            if arg.startswith("--datasource="):
                datasource = arg.split("=", 1)[1]

        result = get_indicators(datasource)

    elif command == "trade-data":
        reporter = None
        partner = None
        product = "ALL"
        year = None
        datasource = "tradestats-development"

        # Parse arguments
        for arg in sys.argv[2:]:
            if arg.startswith("--reporter="):
                reporter = arg.split("=", 1)[1]
            elif arg.startswith("--partner="):
                partner = arg.split("=", 1)[1]
            elif arg.startswith("--product="):
                product = arg.split("=", 1)[1]
            elif arg.startswith("--year="):
                year = arg.split("=", 1)[1]
            elif arg.startswith("--datasource="):
                datasource = arg.split("=", 1)[1]

        if not reporter or not partner:
            print(json.dumps({
                "error": "Missing required parameters: --reporter and --partner are required",
                "example": "python wits_trade_data.py trade-data --reporter=840 --partner=000 --year=2020"
            }))
            sys.exit(1)

        result = get_trade_data(reporter, partner, product, year, datasource)

    elif command == "tariff-data":
        reporter = None
        partner = "000"
        product = "ALL"
        year = None

        # Parse arguments
        for arg in sys.argv[2:]:
            if arg.startswith("--reporter="):
                reporter = arg.split("=", 1)[1]
            elif arg.startswith("--partner="):
                partner = arg.split("=", 1)[1]
            elif arg.startswith("--product="):
                product = arg.split("=", 1)[1]
            elif arg.startswith("--year="):
                year = arg.split("=", 1)[1]

        if not reporter:
            print(json.dumps({
                "error": "Missing required parameter: --reporter is required",
                "example": "python wits_trade_data.py tariff-data --reporter=840 --year=2020"
            }))
            sys.exit(1)

        result = get_tariff_data(reporter, partner, product, year)

    elif command == "product-tariff":
        reporter = None
        partner = None
        product = None
        year = None
        datatype = "reported"

        # Parse arguments
        for arg in sys.argv[2:]:
            if arg.startswith("--reporter="):
                reporter = arg.split("=", 1)[1]
            elif arg.startswith("--partner="):
                partner = arg.split("=", 1)[1]
            elif arg.startswith("--product="):
                product = arg.split("=", 1)[1]
            elif arg.startswith("--year="):
                year = arg.split("=", 1)[1]
            elif arg.startswith("--datatype="):
                datatype = arg.split("=", 1)[1]

        if not all([reporter, partner, product, year]):
            print(json.dumps({
                "error": "Missing required parameters: --reporter, --partner, --product, and --year are required",
                "example": "python wits_trade_data.py product-tariff --reporter=840 --partner=000 --product=020110 --year=2020"
            }))
            sys.exit(1)

        result = get_product_tariff(reporter, partner, product, year, datatype)

    elif command == "catalog":
        result = get_catalog()

    elif command == "test-all":
        result = test_all_endpoints()

    else:
        print(json.dumps({
            "error": f"Unknown command: {command}",
            "available_commands": [
                "indicators", "trade-data", "tariff-data", "product-tariff", "catalog", "test-all"
            ]
        }))
        sys.exit(1)

    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()