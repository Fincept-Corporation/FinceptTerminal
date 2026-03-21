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

# Common codes and mappings (ISO3 codes for SDMX API)
PARTNER_CODES = {
    "wld": "World",
    "usa": "United States",
    "chn": "China",
    "gbr": "United Kingdom",
    "deu": "Germany",
    "can": "Canada",
    "jpn": "Japan",
    "ita": "Italy",
    "fra": "France",
    "ind": "India",
    "rus": "Russia",
    "bra": "Brazil"
}

REPORTER_CODES = {
    "wld": "World",
    "usa": "United States",
    "chn": "China",
    "gbr": "United Kingdom",
    "deu": "Germany",
    "can": "Canada",
    "jpn": "Japan",
    "ita": "Italy",
    "fra": "France",
    "ind": "India",
    "rus": "Russia",
    "bra": "Brazil"
}

# ISO3 to Numeric Code Mapping (for Tariff API) - Complete 266 countries
ISO3_TO_NUMERIC = {
    "999": "999", "abw": "533", "afg": "004", "ago": "024", "aia": "660", "alb": "008", "and": "020", "ant": "530",
    "are": "784", "arg": "032", "arm": "051", "asm": "016", "ata": "010", "atf": "260", "atg": "028", "aus": "036",
    "aut": "040", "aze": "031", "bat": "080", "bdi": "108", "bel": "056", "ben": "204", "bes": "535", "bfa": "854",
    "bgd": "050", "bgr": "100", "bhr": "048", "bhs": "044", "bih": "070", "blm": "652", "blr": "112", "blx": "058",
    "blz": "084", "bmu": "060", "bol": "068", "bra": "076", "brb": "052", "brn": "096", "btn": "064", "bun": "837",
    "bvt": "074", "bwa": "072", "caf": "140", "can": "124", "cck": "166", "che": "756", "chl": "152", "chn": "156",
    "civ": "384", "cmr": "120", "cog": "178", "cok": "184", "col": "170", "com": "174", "cpv": "132", "cri": "188",
    "csk": "200", "cub": "192", "cuw": "531", "cxr": "162", "cym": "136", "cyp": "196", "cze": "203", "ddr": "278",
    "deu": "276", "dji": "262", "dma": "212", "dnk": "208", "dom": "214", "dza": "012", "eas": "EAS", "ecs": "ECS",
    "ecu": "218", "egy": "818", "eri": "232", "esh": "732", "esp": "724", "est": "233", "etf": "230", "eth": "231",
    "fin": "246", "fji": "242", "flk": "238", "fra": "250", "fre": "838", "fro": "234", "fsm": "583", "gab": "266",
    "gbr": "826", "geo": "268", "gha": "288", "gib": "292", "gin": "324", "glp": "312", "gmb": "270", "gnb": "624",
    "gnq": "226", "grc": "300", "grd": "308", "grl": "304", "gtm": "320", "guf": "254", "gum": "316", "guy": "328",
    "hkg": "344", "hmd": "334", "hnd": "340", "hrv": "191", "hti": "332", "hun": "348", "idn": "360", "ind": "356",
    "iot": "086", "irl": "372", "irn": "364", "irq": "368", "isl": "352", "isr": "376", "ita": "380", "jam": "388",
    "jor": "400", "jpn": "392", "kaz": "398", "ken": "404", "kgz": "417", "khm": "116", "kir": "296", "kna": "659",
    "kor": "410", "kwt": "414", "lao": "418", "lbn": "422", "lbr": "430", "lby": "434", "lca": "662", "lcn": "LCN",
    "lka": "144", "lso": "426", "ltu": "440", "lux": "442", "lva": "428", "mac": "446", "mar": "504", "mco": "492",
    "mda": "498", "mdg": "450", "mdv": "462", "mea": "MEA", "mex": "484", "mhl": "584", "mkd": "807", "mli": "466",
    "mlt": "470", "mmr": "104", "mng": "496", "mnp": "580", "mnt": "499", "moz": "508", "mrt": "478", "msr": "500",
    "mtq": "474", "mus": "480", "mwi": "454", "mys": "458", "myt": "175", "nac": "NAC", "nam": "516", "ncl": "540",
    "ner": "562", "nfk": "574", "nga": "566", "nic": "558", "niu": "570", "nld": "528", "nor": "578", "npl": "524",
    "nru": "520", "nze": "536", "nzl": "554", "oas": "490", "omn": "512", "pak": "586", "pan": "591", "pce": "582",
    "pcn": "612", "per": "604", "phl": "608", "plw": "585", "png": "598", "pol": "616", "prk": "408", "prt": "620",
    "pry": "600", "pse": "275", "pyf": "258", "qat": "634", "reu": "638", "rom": "642", "rus": "643", "rwa": "646",
    "sas": "SAS", "sau": "682", "sdn": "736", "sen": "686", "ser": "891", "sgp": "702", "sgs": "239", "shn": "654",
    "slb": "090", "sle": "694", "slv": "222", "smr": "674", "som": "706", "spe": "839", "spm": "666", "ssd": "728",
    "ssf": "SSF", "stp": "678", "sud": "729", "sur": "740", "svk": "703", "svn": "705", "svu": "810", "swe": "752",
    "swz": "748", "sxm": "534", "syc": "690", "syr": "760", "tca": "796", "tcd": "148", "tgo": "768", "tha": "764",
    "tjk": "762", "tkl": "772", "tkm": "795", "tmp": "626", "ton": "776", "tto": "780", "tun": "788", "tur": "792",
    "tuv": "798", "tza": "834", "uga": "800", "ukr": "804", "umi": "581", "uns": "898", "ury": "858", "usa": "840",
    "usp": "849", "uzb": "860", "vat": "336", "vct": "670", "ven": "862", "vgb": "092", "vnm": "704", "vut": "548",
    "wld": "000", "wlf": "876", "wsm": "882", "ydr": "720", "yem": "887", "yug": "890", "zaf": "710", "zar": "180",
    "zmb": "894", "zwe": "716"
}

# Numeric to ISO3 Code Mapping
NUMERIC_TO_ISO3 = {v: k for k, v in ISO3_TO_NUMERIC.items()}

DATASOURCES = {
    "tradestats-development": "Trade Statistics - Development",
    "trn": "Tariff Data",
    "tradestats-test": "Trade Statistics - Test"
}

PRODUCT_CATEGORIES = {
    "total": "Total Trade",
    "all": "All Products",
    "ag": "Agricultural Products",
    "fuels": "Fuels",
    "mn": "Ores and Metals",
    "mf": "Manufactures"
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

    if root_tag == 'StructureSpecificData':
        # SDMX V2.1 format
        return _parse_sdmx_v21(root)
    elif root_tag == 'witsdata':
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


def _parse_sdmx_v21(root: ET.Element) -> List[Dict[str, Any]]:
    """Parse SDMX V2.1 StructureSpecificData response"""
    data = []

    # Find all Series elements
    for series in root.findall('.//{*}Series'):
        series_attrs = dict(series.attrib)

        # Find all Obs (observations) within this series
        for obs in series.findall('.//{*}Obs'):
            item = {}

            # Add series attributes
            for key, value in series_attrs.items():
                item[key.lower()] = value

            # Add observation attributes
            for key, value in obs.attrib.items():
                item[key.lower()] = value

            # Try to convert numeric values
            if 'obs_value' in item:
                try:
                    item['obs_value'] = float(item['obs_value'])
                except ValueError:
                    pass

            data.append(item)

    return data


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


def get_trade_data(reporter: str, partner: str, product: str = "Total", year: str = None,
                  indicator: str = "XPRT-TRD-VL") -> Dict[str, Any]:
    """
    Get trade statistics data between countries using SDMX format

    Args:
        reporter: Reporter country ISO3 code (e.g., "usa" for USA)
        partner: Partner country ISO3 code (e.g., "wld" for World)
        product: Product code (e.g., "total" for all products, "fuels", etc.)
        year: Year for data (e.g., "2020")
        indicator: Trade indicator code (e.g., "XPRT-TRD-VL" for export value)

    Returns:
        Dict containing trade data, metadata, and error information
    """
    # Default to current year if not specified
    if year is None:
        year = str(datetime.now().year - 1)  # Last full year

    # Convert codes for API (country lowercase, product capitalized)
    reporter = reporter.lower()
    partner = partner.lower()
    # Product code should be capitalized (Total, not total)
    if product.lower() in ["total", "all", "ag", "fuels", "mn", "mf"]:
        product = product.capitalize()

    # Use SDMX format for tradestats-trade
    url = f"{BASE_URL}/SDMX/V21/datasource/tradestats-trade/reporter/{reporter}/year/{year}/partner/{partner}/product/{product}/indicator/{indicator}"

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
            'indicator': indicator,
            'datasource': 'tradestats-trade',
            'datasource_name': 'Trade Statistics - Trade'
        }

    return result


def get_tariff_data(reporter: str, partner: str = "wld", product: str = "total", year: str = None,
                   datatype: str = "reported") -> Dict[str, Any]:
    """
    Get tariff data for imports using SDMX format

    Args:
        reporter: Reporter country code - ISO3 (e.g., "usa") or numeric (e.g., "840")
        partner: Partner country code - ISO3 (e.g., "wld") or numeric (default: "wld" for World)
        product: Product HS code (e.g., "020110") or "total" for aggregated
        year: Year for data (e.g., "2020")
        datatype: Data type (default: "reported")

    Returns:
        Dict containing tariff data, metadata, and error information
    """
    # Default to current year if not specified
    if year is None:
        year = str(datetime.now().year - 1)  # Last full year

    # Convert ISO3 to numeric codes for tariff API
    reporter_original = reporter
    partner_original = partner

    reporter = reporter.lower()
    partner = partner.lower()

    # Convert to numeric if ISO3 code
    if reporter in ISO3_TO_NUMERIC:
        reporter = ISO3_TO_NUMERIC[reporter]
    if partner in ISO3_TO_NUMERIC:
        partner = ISO3_TO_NUMERIC[partner]

    # Use SDMX format for tariff data
    url = f"{BASE_URL}/SDMX/V21/datasource/TRN/reporter/{reporter}/partner/{partner}/product/{product}/year/{year}/datatype/{datatype}"

    # Add context to metadata
    result = _make_request(url)
    if result['metadata']:
        result['metadata']['context'] = {
            'reporter_code': reporter,
            'reporter_name': REPORTER_CODES.get(reporter_original.lower(), REPORTER_CODES.get(NUMERIC_TO_ISO3.get(reporter, ""), f"Code {reporter}")),
            'partner_code': partner,
            'partner_name': PARTNER_CODES.get(partner_original.lower(), PARTNER_CODES.get(NUMERIC_TO_ISO3.get(partner, ""), f"Code {partner}")),
            'product_code': product,
            'year': year,
            'datatype': datatype,
            'datasource': 'TRN',
            'datasource_name': 'Tariff Data (SDMX)'
        }

    return result


def get_product_tariff(reporter: str, partner: str, product: str, year: str,
                       datatype: str = "reported") -> Dict[str, Any]:
    """
    Get specific product tariff data using SDMX format

    Args:
        reporter: Reporter country code - ISO3 (e.g., "usa") or numeric (e.g., "840")
        partner: Partner country code - ISO3 (e.g., "wld") or numeric (e.g., "000")
        product: Product HS code (e.g., "020110")
        year: Year for data (e.g., "2020")
        datatype: Data type (default: "reported")

    Returns:
        Dict containing product tariff data, metadata, and error information
    """
    # Convert ISO3 to numeric codes for tariff API
    reporter_original = reporter
    partner_original = partner

    reporter = reporter.lower()
    partner = partner.lower()

    # Convert to numeric if ISO3 code
    if reporter in ISO3_TO_NUMERIC:
        reporter = ISO3_TO_NUMERIC[reporter]
    if partner in ISO3_TO_NUMERIC:
        partner = ISO3_TO_NUMERIC[partner]

    url = f"{BASE_URL}/SDMX/V21/datasource/TRN/reporter/{reporter}/partner/{partner}/product/{product}/year/{year}/datatype/{datatype}"

    # Add context to metadata
    result = _make_request(url)
    if result['metadata']:
        result['metadata']['context'] = {
            'reporter_code': reporter,
            'reporter_name': REPORTER_CODES.get(reporter_original.lower(), REPORTER_CODES.get(NUMERIC_TO_ISO3.get(reporter, ""), f"Code {reporter}")),
            'partner_code': partner,
            'partner_name': PARTNER_CODES.get(partner_original.lower(), PARTNER_CODES.get(NUMERIC_TO_ISO3.get(partner, ""), f"Code {partner}")),
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
        result = get_trade_data("usa", "wld", "Total", "2020")
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

    # Test 4: Get tariff data (US, 2020) - using specific product code
    print("Testing: get_tariff_data(US, 2020)...")
    try:
        result = get_tariff_data("usa", "wld", "020110", "2020")
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
        result = get_product_tariff("usa", "wld", "020110", "2020")
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
        indicator = "XPRT-TRD-VL"

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
            elif arg.startswith("--indicator="):
                indicator = arg.split("=", 1)[1]

        if not reporter or not partner:
            print(json.dumps({
                "error": "Missing required parameters: --reporter and --partner are required",
                "example": "python wits_trade_data.py trade-data --reporter=840 --partner=000 --year=2020"
            }))
            sys.exit(1)

        result = get_trade_data(reporter, partner, product, year, indicator)

    elif command == "tariff-data":
        reporter = None
        partner = "wld"
        product = "total"
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
                "example": "python wits_trade_data.py tariff-data --reporter=usa --year=2020"
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