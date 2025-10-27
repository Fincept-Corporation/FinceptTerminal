"""
U.S. Treasury FiscalData API Wrapper
Fetches U.S. government financial data from the Treasury's FiscalData platform
Returns JSON output for Rust integration

API Documentation: https://fiscaldata.treasury.gov/datasets/
Base URL: https://api.fiscaldata.treasury.gov/services/api/fiscal_service/

Usage:
    python fiscaldata_data.py debt-to-penny --fields="record_date,tot_pub_debt_out_amt" --limit=100
    python fiscaldata_data.py avg-interest-rates --security-type="Marketable" --limit=50
    python fiscaldata_data.py interest-expense --start-date="2024-01-01"
    python fiscaldata_data.py datasets
"""

import sys
import json
import requests
import os
from typing import Dict, List, Optional, Any
from datetime import datetime, timedelta

# Configuration
BASE_URL = "https://api.fiscaldata.treasury.gov/services/api/fiscal_service"
TIMEOUT = 30
DEFAULT_PAGE_SIZE = 100


def _make_request(endpoint: str, params: Dict[str, Any] = None) -> Dict[str, Any]:
    """
    Centralized request handler for FiscalData API calls

    Args:
        endpoint: API endpoint path
        params: Query parameters for the request

    Returns:
        Standardized response dict with data, metadata, and error fields
    """
    if params is None:
        params = {}

    # Set default values
    params.setdefault('format', 'json')
    params.setdefault('page[size]', str(DEFAULT_PAGE_SIZE))
    params.setdefault('page[number]', '1')

    try:
        url = f"{BASE_URL}/{endpoint}"

        headers = {
            'User-Agent': 'Fincept-Terminal/1.0 (fiscaldata-api-wrapper)',
            'Accept': 'application/json'
        }

        response = requests.get(url, params=params, headers=headers, timeout=TIMEOUT)
        response.raise_for_status()

        raw_data = response.json()

        # Extract data and metadata from the response
        if 'data' in raw_data:
            data = raw_data['data']
            metadata = raw_data.get('meta', {})

            return {
                "data": data,
                "metadata": {
                    "source": "U.S. Treasury FiscalData",
                    "endpoint": endpoint,
                    "last_updated": datetime.now().isoformat(),
                    "total_count": metadata.get('total-count', len(data)),
                    "count": metadata.get('count', len(data)),
                    "action": f"fetched_{endpoint.replace('/', '_')}",
                    "params": params
                },
                "error": None
            }
        else:
            return {
                "data": [],
                "metadata": {
                    "source": "U.S. Treasury FiscalData",
                    "endpoint": endpoint,
                    "last_updated": datetime.now().isoformat(),
                    "action": f"fetched_{endpoint.replace('/', '_')}",
                    "params": params
                },
                "error": "Unexpected response format: missing 'data' field"
            }

    except requests.exceptions.HTTPError as e:
        error_msg = f"HTTP Error {e.response.status_code}"
        if e.response.status_code == 400:
            error_msg += " - Bad Request: Check your parameters"
        elif e.response.status_code == 404:
            error_msg += " - Endpoint not found"
        elif e.response.status_code == 429:
            error_msg += " - Rate limit exceeded"
        elif e.response.status_code >= 500:
            error_msg += " - Server error"

        return {
            "data": [],
            "metadata": {
                "source": "U.S. Treasury FiscalData",
                "endpoint": endpoint,
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{endpoint.replace('/', '_')}",
                "params": params
            },
            "error": error_msg
        }

    except requests.exceptions.Timeout:
        return {
            "data": [],
            "metadata": {
                "source": "U.S. Treasury FiscalData",
                "endpoint": endpoint,
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{endpoint.replace('/', '_')}",
                "params": params
            },
            "error": f"Request timeout after {TIMEOUT} seconds"
        }

    except requests.exceptions.RequestException as e:
        return {
            "data": [],
            "metadata": {
                "source": "U.S. Treasury FiscalData",
                "endpoint": endpoint,
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{endpoint.replace('/', '_')}",
                "params": params
            },
            "error": f"Request failed: {str(e)}"
        }

    except json.JSONDecodeError:
        return {
            "data": [],
            "metadata": {
                "source": "U.S. Treasury FiscalData",
                "endpoint": endpoint,
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{endpoint.replace('/', '_')}",
                "params": params
            },
            "error": "Invalid JSON response"
        }

    except Exception as e:
        return {
            "data": [],
            "metadata": {
                "source": "U.S. Treasury FiscalData",
                "endpoint": endpoint,
                "last_updated": datetime.now().isoformat(),
                "action": f"fetched_{endpoint.replace('/', '_')}",
                "params": params
            },
            "error": f"Unexpected error: {str(e)}"
        }


def _get_all_pages(endpoint: str, params: Dict[str, Any] = None, max_pages: int = 50) -> Dict[str, Any]:
    """
    Helper function to retrieve all paginated results

    Args:
        endpoint: API endpoint path
        params: Query parameters
        max_pages: Maximum number of pages to retrieve

    Returns:
        Combined results from all pages
    """
    all_data = []
    metadata = None

    if params is None:
        params = {}

    original_page_size = params.get('page[size]', str(DEFAULT_PAGE_SIZE))

    # Override pagination parameters for internal use
    params['page[size]'] = original_page_size

    for page in range(1, max_pages + 1):
        params['page[number]'] = str(page)

        result = _make_request(endpoint, params)

        if result['error']:
            # Return error but include any data collected so far
            return {
                "data": all_data,
                "metadata": metadata or {
                    "source": "U.S. Treasury FiscalData",
                    "endpoint": endpoint,
                    "last_updated": datetime.now().isoformat(),
                    "action": f"fetched_all_{endpoint.replace('/', '_')}",
                    "params": params
                },
                "error": result['error']
            }

        if result['data']:
            all_data.extend(result['data'])
            metadata = result['metadata']

            # Check if we've got all the data
            total_count = result['metadata'].get('total_count', 0)
            if len(all_data) >= total_count or len(result['data']) < int(original_page_size):
                break
        else:
            break

    # Update metadata with final counts
    if metadata:
        metadata['total_count'] = len(all_data)
        metadata['pages_retrieved'] = page
        metadata['action'] = f"fetched_all_{endpoint.replace('/', '_')}"

    return {
        "data": all_data,
        "metadata": metadata or {
            "source": "U.S. Treasury FiscalData",
            "endpoint": endpoint,
            "last_updated": datetime.now().isoformat(),
            "total_count": len(all_data),
            "action": f"fetched_all_{endpoint.replace('/', '_')}",
            "params": params
        },
        "error": None
    }


def get_debt_to_penny(fields: str = None, start_date: str = None, end_date: str = None,
                      limit: int = None, all_pages: bool = False) -> Dict[str, Any]:
    """
    Get Debt to the Penny data - Daily public debt outstanding

    Args:
        fields: Comma-separated list of fields to return
        start_date: Filter for records on or after this date (YYYY-MM-DD)
        end_date: Filter for records on or before this date (YYYY-MM-DD)
        limit: Maximum number of records to return
        all_pages: If True, retrieve all available records

    Returns:
        Dict containing debt data, metadata, and error information
    """
    params = {}

    if fields:
        params['fields'] = fields
    else:
        # Default fields for debt data (using correct field names)
        params['fields'] = 'record_date,tot_pub_debt_out_amt,debt_held_public_amt,intragov_hold_amt'

    # Build filter string
    filters = []
    if start_date:
        filters.append(f"record_date:gte:{start_date}")
    if end_date:
        filters.append(f"record_date:lte:{end_date}")

    if filters:
        params['filter'] = ','.join(filters)

    # Sort by most recent date first
    params['sort'] = '-record_date'

    if limit:
        params['page[size]'] = str(min(limit, 1000))

    if all_pages and not limit:
        return _get_all_pages('v2/accounting/od/debt_to_penny', params)
    else:
        result = _make_request('v2/accounting/od/debt_to_penny', params)

        if limit and result['data'] and len(result['data']) > limit:
            result['data'] = result['data'][:limit]
            result['metadata']['total_count'] = len(result['data'])

        return result


def get_avg_interest_rates(start_date: str = None, end_date: str = None,
                          security_type: str = None, limit: int = None,
                          all_pages: bool = False) -> Dict[str, Any]:
    """
    Get Average Interest Rates data - Average interest rates on Treasury securities

    Args:
        start_date: Filter for records on or after this date (YYYY-MM-DD)
        end_date: Filter for records on or before this date (YYYY-MM-DD)
        security_type: Filter by security type (e.g., 'Marketable', 'Non-marketable')
        limit: Maximum number of records to return
        all_pages: If True, retrieve all available records

    Returns:
        Dict containing average interest rates data, metadata, and error information
    """
    params = {}

    # Default fields for average interest rates
    params['fields'] = 'record_date,security_type_desc,security_desc,avg_interest_rate_amt'

    # Build filter string
    filters = []
    if start_date:
        filters.append(f"record_date:gte:{start_date}")
    if end_date:
        filters.append(f"record_date:lte:{end_date}")
    if security_type:
        filters.append(f"security_type_desc:eq:{security_type}")

    if filters:
        params['filter'] = ','.join(filters)

    # Sort by most recent date first
    params['sort'] = '-record_date'

    if limit:
        params['page[size]'] = str(min(limit, 1000))

    if all_pages and not limit:
        return _get_all_pages('v2/accounting/od/avg_interest_rates', params)
    else:
        result = _make_request('v2/accounting/od/avg_interest_rates', params)

        if limit and result['data'] and len(result['data']) > limit:
            result['data'] = result['data'][:limit]
            result['metadata']['total_count'] = len(result['data'])

        return result


def get_interest_expense(start_date: str = None, end_date: str = None,
                        expense_category: str = None, limit: int = None,
                        all_pages: bool = False) -> Dict[str, Any]:
    """
    Get Interest Expense data - Monthly interest expense on Treasury securities

    Args:
        start_date: Filter for records on or after this date (YYYY-MM-DD)
        end_date: Filter for records on or before this date (YYYY-MM-DD)
        expense_category: Filter by expense category (e.g., 'INTEREST EXPENSE ON PUBLIC ISSUES')
        limit: Maximum number of records to return
        all_pages: If True, retrieve all available records

    Returns:
        Dict containing interest expense data, metadata, and error information
    """
    params = {}

    # Default fields for interest expense
    params['fields'] = 'record_date,expense_catg_desc,expense_group_desc,expense_type_desc,month_expense_amt,fytd_expense_amt'

    # Build filter string
    filters = []
    if start_date:
        filters.append(f"record_date:gte:{start_date}")
    if end_date:
        filters.append(f"record_date:lte:{end_date}")
    if expense_category:
        filters.append(f"expense_catg_desc:eq:{expense_category}")

    if filters:
        params['filter'] = ','.join(filters)

    # Sort by most recent date first
    params['sort'] = '-record_date'

    if limit:
        params['page[size]'] = str(min(limit, 1000))

    if all_pages and not limit:
        return _get_all_pages('v2/accounting/od/interest_expense', params)
    else:
        result = _make_request('v2/accounting/od/interest_expense', params)

        if limit and result['data'] and len(result['data']) > limit:
            result['data'] = result['data'][:limit]
            result['metadata']['total_count'] = len(result['data'])

        return result


def get_datasets() -> Dict[str, Any]:
    """
    Get available datasets from FiscalData (working endpoints only)

    Returns:
        Dict containing information about available endpoints and datasets
    """
    # Return only working endpoints from our testing
    datasets = [
        {
            "endpoint": "v2/accounting/od/debt_to_penny",
            "name": "Debt to Penny",
            "description": "Daily public debt outstanding amounts",
            "function": "get_debt_to_penny",
            "status": "working"
        },
        {
            "endpoint": "v2/accounting/od/avg_interest_rates",
            "name": "Average Interest Rates",
            "description": "Average interest rates on Treasury securities",
            "function": "get_avg_interest_rates",
            "status": "working"
        },
        {
            "endpoint": "v2/accounting/od/interest_expense",
            "name": "Interest Expense",
            "description": "Monthly interest expense on Treasury securities",
            "function": "get_interest_expense",
            "status": "working"
        },
        {
            "endpoint": "v2/accounting/od/rates_of_exchange",
            "name": "Exchange Rates",
            "description": "Daily currency exchange rates",
            "function": "get_exchange_rates",
            "status": "not_available"
        },
        {
            "endpoint": "v2/debt/to_the_summary",
            "name": "Federal Debt Summary",
            "description": "Summary of federal debt holdings",
            "function": "get_federal_debt_summary",
            "status": "not_available"
        }
    ]

    working_count = sum(1 for d in datasets if d.get("status") == "working")

    return {
        "data": datasets,
        "metadata": {
            "source": "U.S. Treasury FiscalData",
            "last_updated": datetime.now().isoformat(),
            "action": "listed_datasets",
            "total_count": len(datasets),
            "working_count": working_count
        },
        "error": None
    }


def _parse_date_range(date_str: str) -> str:
    """
    Helper function to parse date range strings and convert to YYYY-MM-DD format

    Args:
        date_str: Date string (can be YYYY-MM-DD, or relative like '30d', '1y', '3m')

    Returns:
        Formatted date string in YYYY-MM-DD format
    """
    if not date_str:
        return None

    # Handle relative dates
    if date_str.endswith('d'):
        days = int(date_str[:-1])
        target_date = datetime.now() - timedelta(days=days)
        return target_date.strftime('%Y-%m-%d')
    elif date_str.endswith('m'):
        months = int(date_str[:-1])
        target_date = datetime.now() - timedelta(days=months * 30)
        return target_date.strftime('%Y-%m-%d')
    elif date_str.endswith('y'):
        years = int(date_str[:-1])
        target_date = datetime.now() - timedelta(days=years * 365)
        return target_date.strftime('%Y-%m-%d')

    # Assume it's already in YYYY-MM-DD format
    return date_str


def main():
    """Main CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python fiscaldata_data.py <command> [options]",
            "available_commands": [
                "debt-to-penny [--fields=field1,field2] [--start-date=YYYY-MM-DD] [--end-date=YYYY-MM-DD] [--limit=N] [--all]",
                "avg-interest-rates [--start-date=YYYY-MM-DD] [--end-date=YYYY-MM-DD] [--security-type=type] [--limit=N] [--all]",
                "interest-expense [--start-date=YYYY-MM-DD] [--end-date=YYYY-MM-DD] [--expense-category=category] [--limit=N] [--all]",
                "datasets"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    result = None

    if command == "debt-to-penny":
        fields = None
        start_date = None
        end_date = None
        limit = None
        all_pages = False

        # Parse arguments
        for arg in sys.argv[2:]:
            if arg.startswith("--fields="):
                fields = arg.split("=", 1)[1]
            elif arg.startswith("--start-date="):
                start_date = _parse_date_range(arg.split("=", 1)[1])
            elif arg.startswith("--end-date="):
                end_date = _parse_date_range(arg.split("=", 1)[1])
            elif arg.startswith("--limit="):
                limit = int(arg.split("=", 1)[1])
            elif arg == "--all":
                all_pages = True

        result = get_debt_to_penny(fields, start_date, end_date, limit, all_pages)

    elif command == "avg-interest-rates":
        start_date = None
        end_date = None
        security_type = None
        limit = None
        all_pages = False

        for arg in sys.argv[2:]:
            if arg.startswith("--start-date="):
                start_date = _parse_date_range(arg.split("=", 1)[1])
            elif arg.startswith("--end-date="):
                end_date = _parse_date_range(arg.split("=", 1)[1])
            elif arg.startswith("--security-type="):
                security_type = arg.split("=", 1)[1]
            elif arg.startswith("--limit="):
                limit = int(arg.split("=", 1)[1])
            elif arg == "--all":
                all_pages = True

        result = get_avg_interest_rates(start_date, end_date, security_type, limit, all_pages)

    elif command == "interest-expense":
        start_date = None
        end_date = None
        expense_category = None
        limit = None
        all_pages = False

        for arg in sys.argv[2:]:
            if arg.startswith("--start-date="):
                start_date = _parse_date_range(arg.split("=", 1)[1])
            elif arg.startswith("--end-date="):
                end_date = _parse_date_range(arg.split("=", 1)[1])
            elif arg.startswith("--expense-category="):
                expense_category = arg.split("=", 1)[1]
            elif arg.startswith("--limit="):
                limit = int(arg.split("=", 1)[1])
            elif arg == "--all":
                all_pages = True

        result = get_interest_expense(start_date, end_date, expense_category, limit, all_pages)

    elif command == "datasets":
        result = get_datasets()

    else:
        print(json.dumps({
            "error": f"Unknown command: {command}",
            "available_commands": [
                "debt-to-penny", "avg-interest-rates", "interest-expense", "datasets"
            ]
        }))
        sys.exit(1)

    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()