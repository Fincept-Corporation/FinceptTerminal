#!/usr/bin/env python3
"""
Universal CKAN Data Portals API Wrapper

A comprehensive wrapper for accessing multiple CKAN-based data portals worldwide.
Supports 8 major CKAN portals: US, UK, Australia, Italy, Brazil, Latvia, Slovenia, Uruguay.

Usage Examples:
    # List all datasets from US portal
    python universal_ckan_api.py --country us --action list_datasets

    # Search for climate data in Australia
    python universal_ckan_api.py --country au --action search_datasets --query "climate"

    # Get dataset details from UK portal
    python universal_ckan_api.py --country uk --action dataset_details --id "uk-some-dataset"

Supported Portals:
    - US (data.gov): https://catalog.data.gov/api/3/action
    - UK (data.gov.uk): https://data.gov.uk/api/3/action
    - AU (data.gov.au): https://data.gov.au/api/3/action
    - IT (dati.gov.it): https://www.dati.gov.it/api/3/action
    - BR (dados.gov.br): https://dados.gov.br/api/3/action
    - LV (data.gov.lv): https://data.gov.lv/api/3/action
    - SI (podatki.gov.si): https://podatki.gov.si/api/3/action
    - UY (catalogodatos.gub.uy): https://catalogodatos.gub.uy/api/3/action

API Documentation: https://docs.ckan.org/en/2.9/api/
"""

import sys
import json
import os
import requests
from datetime import datetime
from typing import Dict, List, Any, Optional

# --- 1. CONFIGURATION ---

# Supported CKAN portals with their base URLs
CKAN_PORTALS = {
    'us': {
        'name': 'United States (Data.gov)',
        'base_url': 'https://catalog.data.gov/api/3/action',
        'country_code': 'US'
    },
    'uk': {
        'name': 'United Kingdom (data.gov.uk)',
        'base_url': 'https://data.gov.uk/api/3/action',
        'country_code': 'UK'
    },
    'au': {
        'name': 'Australia (data.gov.au)',
        'base_url': 'https://data.gov.au/api/3/action',
        'country_code': 'AU'
    },
    'it': {
        'name': 'Italy (dati.gov.it)',
        'base_url': 'https://www.dati.gov.it/api/3/action',
        'country_code': 'IT'
    },
    'br': {
        'name': 'Brazil (dados.gov.br)',
        'base_url': 'https://dados.gov.br/api/3/action',
        'country_code': 'BR'
    },
    'lv': {
        'name': 'Latvia (data.gov.lv)',
        'base_url': 'https://data.gov.lv/api/3/action',
        'country_code': 'LV'
    },
    'si': {
        'name': 'Slovenia (podatki.gov.si)',
        'base_url': 'https://podatki.gov.si/api/3/action',
        'country_code': 'SI'
    },
    'uy': {
        'name': 'Uruguay (catalogodatos.gub.uy)',
        'base_url': 'https://catalogodatos.gub.uy/api/3/action',
        'country_code': 'UY'
    }
}

# Request configuration
TIMEOUT = 30
MAX_RETRIES = 3

# --- 2. PRIVATE HELPER FUNCTIONS ---

def _get_portal_config(country_code: str) -> Dict[str, str]:
    """
    Get portal configuration for the specified country.

    Args:
        country_code: Two-letter country code (us, uk, au, etc.)

    Returns:
        Dictionary containing portal configuration

    Raises:
        ValueError: If country code is not supported
    """
    country_code = country_code.lower()
    if country_code not in CKAN_PORTALS:
        supported_countries = ', '.join(CKAN_PORTALS.keys())
        raise ValueError(f"Unsupported country code '{country_code}'. Supported: {supported_countries}")

    return CKAN_PORTALS[country_code]

def _make_request(country_code: str, action: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """
    Centralized request handler for CKAN API calls.

    Args:
        country_code: Two-letter country code
        action: CKAN action name (e.g., 'package_search', 'package_show')
        params: Additional parameters for the API call

    Returns:
        Formatted response with data, metadata, and error fields
    """
    try:
        portal_config = _get_portal_config(country_code)
        base_url = portal_config['base_url']

        # Prepare request parameters
        request_params = params.copy() if params else {}

        # Prepare headers
        headers = {
            'Content-Type': 'application/json',
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json'
        }

        # Make the request with retries
        url = f"{base_url}/{action}"

        for attempt in range(MAX_RETRIES):
            try:
                response = requests.post(
                    url,
                    json=request_params,
                    headers=headers,
                    timeout=TIMEOUT
                )
                response.raise_for_status()

                # Parse JSON response
                api_response = response.json()

                # Check if CKAN API returned success
                if not api_response.get('success', False):
                    error_msg = api_response.get('error', {}).get('message', 'Unknown CKAN API error')
                    return {
                        "data": [],
                        "metadata": {
                            "source": portal_config['name'],
                            "country": portal_config['country_code'],
                            "action": action,
                            "last_updated": datetime.now().isoformat(),
                            "count": 0,
                            "url": url
                        },
                        "error": f"CKAN API Error: {error_msg}"
                    }

                # Return successful response
                result_data = api_response.get('result', [])

                # Handle different response formats
                if isinstance(result_data, dict) and 'results' in result_data:
                    # Search results format
                    data = result_data['results']
                    count = result_data.get('count', len(data))
                else:
                    # Direct result format
                    data = result_data if isinstance(result_data, list) else [result_data]
                    count = len(data)

                return {
                    "data": data,
                    "metadata": {
                        "source": portal_config['name'],
                        "country": portal_config['country_code'],
                        "action": action,
                        "last_updated": datetime.now().isoformat(),
                        "count": count,
                        "url": url,
                        "params": request_params
                    },
                    "error": None
                }

            except requests.exceptions.Timeout:
                if attempt == MAX_RETRIES - 1:
                    return {
                        "data": [],
                        "metadata": {
                            "source": portal_config['name'],
                            "country": portal_config['country_code'],
                            "action": action,
                            "last_updated": datetime.now().isoformat(),
                            "count": 0,
                            "url": url
                        },
                        "error": f"Request timeout after {TIMEOUT} seconds"
                    }
                continue

            except requests.exceptions.ConnectionError:
                if attempt == MAX_RETRIES - 1:
                    return {
                        "data": [],
                        "metadata": {
                            "source": portal_config['name'],
                            "country": portal_config['country_code'],
                            "action": action,
                            "last_updated": datetime.now().isoformat(),
                            "count": 0,
                            "url": url
                        },
                        "error": "Connection error - unable to reach the portal"
                    }
                continue

            except requests.exceptions.HTTPError as e:
                return {
                    "data": [],
                    "metadata": {
                        "source": portal_config['name'],
                        "country": portal_config['country_code'],
                        "action": action,
                        "last_updated": datetime.now().isoformat(),
                        "count": 0,
                        "url": url
                    },
                    "error": f"HTTP error: {e.response.status_code} - {e.response.reason}"
                }

    except Exception as e:
        return {
            "data": [],
            "metadata": {},
            "error": f"Unexpected error: {str(e)}"
        }

def _enhance_dataset_metadata(dataset: Dict[str, Any]) -> Dict[str, Any]:
    """
    Enhance dataset with additional computed metadata.

    Args:
        dataset: Raw dataset from CKAN API

    Returns:
        Enhanced dataset with additional fields
    """
    enhanced = dataset.copy()

    # Add computed fields
    enhanced['num_resources'] = len(dataset.get('resources', []))
    enhanced['has_tags'] = len(dataset.get('tags', [])) > 0
    enhanced['organization_name'] = dataset.get('organization', {}).get('title', 'Unknown')
    enhanced['metadata_created'] = dataset.get('metadata_created', '')
    enhanced['metadata_modified'] = dataset.get('metadata_modified', '')

    # Extract resource types
    resource_types = []
    for resource in dataset.get('resources', []):
        format_type = resource.get('format', '').lower()
        if format_type and format_type not in resource_types:
            resource_types.append(format_type)
    enhanced['resource_types'] = resource_types

    return enhanced

# --- 3. ORGANIZATION/CATALOGUE FUNCTIONS ---

def list_organizations(country_code: str, limit: Optional[int] = None) -> Dict[str, Any]:
    """
    List all organizations (catalogues) in the specified portal.

    Args:
        country_code: Two-letter country code
        limit: Maximum number of organizations to return

    Returns:
        Formatted response containing list of organizations
    """
    params = {}
    if limit:
        params['limit'] = limit

    result = _make_request(country_code, 'organization_list', params)

    if not result['error']:
        # Enhance organization data
        enhanced_orgs = []
        for org_name in result['data']:
            enhanced_orgs.append({
                'name': org_name,
                'display_name': org_name.replace('-', ' ').title(),
                'country': country_code.upper()
            })
        result['data'] = enhanced_orgs
        result['metadata']['count'] = len(enhanced_orgs)
        result['metadata']['action'] = 'list_organizations'

    return result

def get_organization_details(country_code: str, organization_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific organization.

    Args:
        country_code: Two-letter country code
        organization_id: ID or name of the organization

    Returns:
        Formatted response containing organization details
    """
    params = {'id': organization_id}
    result = _make_request(country_code, 'organization_show', params)

    if not result['error'] and result['data']:
        # Handle different response formats
        org_data = result['data']

        # Ensure we have a dictionary, not a list
        if isinstance(org_data, list) and len(org_data) > 0:
            org_data = org_data[0]
        elif isinstance(org_data, list):
            # Empty list, return error
            result['error'] = f"Organization '{organization_id}' not found"
            result['data'] = {}
            return result

        # Ensure it's a dictionary
        if isinstance(org_data, dict):
            # Enhance organization data
            org_data['package_count'] = len(org_data.get('packages', []))
            org_data['country'] = country_code.upper()
            result['data'] = org_data
        else:
            result['error'] = f"Unexpected data format for organization '{organization_id}'"
            result['data'] = {}

    return result

# --- 4. DATASET FUNCTIONS ---

def list_datasets(country_code: str, limit: Optional[int] = 50) -> Dict[str, Any]:
    """
    List all datasets in the specified portal.

    Args:
        country_code: Two-letter country code
        limit: Maximum number of datasets to return

    Returns:
        Formatted response containing list of datasets
    """
    params = {'limit': limit}
    result = _make_request(country_code, 'package_list', params)

    if not result['error']:
        # Handle different response formats from different CKAN implementations
        enhanced_datasets = []

        if isinstance(result['data'], list) and len(result['data']) > 0:
            # Check if it's a simple list of names or a search result format
            first_item = result['data'][0]

            if isinstance(first_item, str):
                # Simple list of names (standard CKAN package_list)
                for dataset_name in result['data']:
                    enhanced_datasets.append({
                        'name': dataset_name,
                        'display_name': dataset_name.replace('-', ' ').title(),
                        'country': country_code.upper()
                    })
            elif isinstance(first_item, dict) and 'name' in first_item:
                # Search result format (some implementations return this)
                for dataset in result['data']:
                    enhanced_dataset = {
                        'name': dataset.get('name', ''),
                        'display_name': dataset.get('title', dataset.get('name', '')).replace('-', ' ').title(),
                        'country': country_code.upper(),
                        'title': dataset.get('title', ''),
                        'notes': dataset.get('notes', ''),
                        'organization': dataset.get('organization', {}).get('title', ''),
                        'num_resources': dataset.get('num_resources', 0),
                        'metadata_created': dataset.get('metadata_created', ''),
                        'metadata_modified': dataset.get('metadata_modified', '')
                    }
                    enhanced_datasets.append(enhanced_dataset)

        result['data'] = enhanced_datasets
        result['metadata']['count'] = len(enhanced_datasets)
        result['metadata']['action'] = 'list_datasets'

    return result

def search_datasets(country_code: str, query: str = "", rows: int = 50,
                   organization: Optional[str] = None,
                   tags: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    Search for datasets in the specified portal.

    Args:
        country_code: Two-letter country code
        query: Search query string
        rows: Number of results to return
        organization: Filter by organization name
        tags: Filter by tags

    Returns:
        Formatted response containing search results
    """
    params = {
        'rows': rows
    }

    if query:
        params['q'] = query

    if organization:
        params['fq'] = f'organization:{organization}'

    if tags:
        tag_filter = ' OR '.join([f'tags:{tag}' for tag in tags])
        if 'fq' in params:
            params['fq'] += f' AND ({tag_filter})'
        else:
            params['fq'] = tag_filter

    result = _make_request(country_code, 'package_search', params)

    if not result['error']:
        # Enhance dataset metadata
        enhanced_datasets = []
        for dataset in result['data']:
            enhanced_dataset = _enhance_dataset_metadata(dataset)
            enhanced_datasets.append(enhanced_dataset)

        result['data'] = enhanced_datasets
        result['metadata']['count'] = len(enhanced_datasets)
        result['metadata']['query'] = query

    return result

def get_dataset_details(country_code: str, dataset_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific dataset.

    Args:
        country_code: Two-letter country code
        dataset_id: ID or name of the dataset

    Returns:
        Formatted response containing dataset details
    """
    params = {'id': dataset_id}
    result = _make_request(country_code, 'package_show', params)

    if not result['error'] and result['data']:
        # Handle different response formats
        dataset_data = result['data']

        # Ensure we have a dictionary, not a list
        if isinstance(dataset_data, list) and len(dataset_data) > 0:
            dataset_data = dataset_data[0]
        elif isinstance(dataset_data, list):
            # Empty list, return error
            result['error'] = f"Dataset '{dataset_id}' not found"
            result['data'] = {}
            return result

        # Ensure it's a dictionary
        if isinstance(dataset_data, dict):
            # Enhance dataset metadata
            enhanced_dataset = _enhance_dataset_metadata(dataset_data)
            result['data'] = enhanced_dataset
        else:
            result['error'] = f"Unexpected data format for dataset '{dataset_id}'"
            result['data'] = {}

    return result

def get_datasets_by_organization(country_code: str, organization_id: str,
                                limit: Optional[int] = None) -> Dict[str, Any]:
    """
    Get all datasets belonging to a specific organization.

    Args:
        country_code: Two-letter country code
        organization_id: ID or name of the organization
        limit: Maximum number of datasets to return

    Returns:
        Formatted response containing organization datasets
    """
    # First get organization details to get the list of packages
    org_result = get_organization_details(country_code, organization_id)

    if org_result['error']:
        return org_result

    # Get the list of packages from the organization
    packages = org_result['data'].get('packages', [])

    if limit:
        packages = packages[:limit]

    # Get details for each package
    datasets = []
    errors = []

    for package_name in packages:
        try:
            dataset_result = get_dataset_details(country_code, package_name)
            if not dataset_result['error']:
                datasets.append(dataset_result['data'])
            else:
                errors.append(f"Failed to get dataset {package_name}: {dataset_result['error']}")
        except Exception as e:
            errors.append(f"Error getting dataset {package_name}: {str(e)}")

    portal_config = _get_portal_config(country_code)

    return {
        "data": datasets,
        "metadata": {
            "source": portal_config['name'],
            "country": portal_config['country_code'],
            "organization": organization_id,
            "last_updated": datetime.now().isoformat(),
            "count": len(datasets),
            "errors": errors if errors else None
        },
        "error": None if errors else f"Partial success with {len(errors)} errors"
    }

# --- 5. RESOURCE/SERIES FUNCTIONS ---

def get_dataset_resources(country_code: str, dataset_id: str) -> Dict[str, Any]:
    """
    Get all resources (data files) for a specific dataset.

    Args:
        country_code: Two-letter country code
        dataset_id: ID or name of the dataset

    Returns:
        Formatted response containing dataset resources
    """
    # First get dataset details
    dataset_result = get_dataset_details(country_code, dataset_id)

    if dataset_result['error']:
        return dataset_result

    # Extract and enhance resources
    resources = dataset_result['data'].get('resources', [])
    enhanced_resources = []

    for resource in resources:
        enhanced_resource = resource.copy()
        enhanced_resource['dataset_name'] = dataset_result['data'].get('name', '')
        enhanced_resource['dataset_title'] = dataset_result['data'].get('title', '')
        enhanced_resource['organization'] = dataset_result['data'].get('organization', {}).get('name', '')
        enhanced_resources.append(enhanced_resource)

    portal_config = _get_portal_config(country_code)

    return {
        "data": enhanced_resources,
        "metadata": {
            "source": portal_config['name'],
            "country": portal_config['country_code'],
            "dataset_id": dataset_id,
            "dataset_name": dataset_result['data'].get('name', ''),
            "last_updated": datetime.now().isoformat(),
            "count": len(enhanced_resources)
        },
        "error": None
    }

def get_resource_details(country_code: str, resource_id: str) -> Dict[str, Any]:
    """
    Get detailed information about a specific resource.

    Args:
        country_code: Two-letter country code
        resource_id: ID of the resource

    Returns:
        Formatted response containing resource details
    """
    params = {'id': resource_id}
    result = _make_request(country_code, 'resource_show', params)

    return result

# --- 6. UTILITY FUNCTIONS ---

def get_supported_countries() -> Dict[str, Any]:
    """
    Get list of supported countries and their portal information.

    Returns:
        Formatted response containing supported countries
    """
    countries = []
    for code, config in CKAN_PORTALS.items():
        countries.append({
            'code': code.upper(),
            'name': config['name'],
            'base_url': config['base_url'],
            'country_code': config['country_code']
        })

    return {
        "data": countries,
        "metadata": {
            "source": "Universal CKAN API Wrapper",
            "country": "ALL",
            "action": "get_supported_countries",
            "last_updated": datetime.now().isoformat(),
            "count": len(countries)
        },
        "error": None
    }

def test_portal_connection(country_code: str) -> Dict[str, Any]:
    """
    Test connection to a specific CKAN portal.

    Args:
        country_code: Two-letter country code

    Returns:
        Formatted response containing connection test results
    """
    try:
        portal_config = _get_portal_config(country_code)

        # Test with a simple API call
        result = _make_request(country_code, 'package_list', {'limit': 1})

        if not result['error']:
            return {
                "data": {
                    "status": "connected",
                    "portal": result['metadata']['source'],
                    "country": result['metadata']['country']
                },
                "metadata": {
                    "source": portal_config['name'],
                    "country": portal_config['country_code'],
                    "action": "test_portal_connection",
                    "last_updated": datetime.now().isoformat(),
                    "test_time": datetime.now().isoformat(),
                    "count": 1
                },
                "error": None
            }
        else:
            return {
                "data": {
                    "status": "failed",
                    "portal": portal_config['name'],
                    "country": portal_config['country_code'],
                    "error": result['error']
                },
                "metadata": {
                    "source": portal_config['name'],
                    "country": portal_config['country_code'],
                    "action": "test_portal_connection",
                    "last_updated": datetime.now().isoformat(),
                    "test_time": datetime.now().isoformat(),
                    "count": 0
                },
                "error": result['error']
            }

    except Exception as e:
        try:
            portal_config = _get_portal_config(country_code)
        except ValueError:
            portal_config = {'name': f'Portal {country_code.upper()}', 'country_code': country_code.upper()}

        return {
            "data": {
                "status": "failed",
                "portal": portal_config['name'],
                "country": portal_config['country_code'],
                "error": str(e)
            },
            "metadata": {
                "source": portal_config['name'],
                "country": portal_config['country_code'],
                "action": "test_portal_connection",
                "last_updated": datetime.now().isoformat(),
                "test_time": datetime.now().isoformat(),
                "count": 0
            },
            "error": f"Connection test failed: {str(e)}"
        }

# --- 7. CLI INTERFACE ---

def print_usage():
    """Print usage instructions."""
    print("Universal CKAN Data Portals API Wrapper")
    print("=" * 50)
    print()
    print("Usage Examples:")
    print("  # List all datasets from US portal")
    print(f"  python {sys.argv[0]} --country us --action list_datasets")
    print()
    print("  # Search for climate data in Australia")
    print(f"  python {sys.argv[0]} --country au --action search_datasets --query 'climate'")
    print()
    print("  # Get dataset details from UK portal")
    print(f"  python {sys.argv[0]} --country uk --action dataset_details --id 'dataset-name'")
    print()
    print("Supported Countries:")
    for code in CKAN_PORTALS.keys():
        print(f"  {code.upper()}: {CKAN_PORTALS[code]['name']}")
    print()
    print("Available Actions:")
    actions = [
        "list_organizations", "get_organization_details",
        "list_datasets", "search_datasets", "get_dataset_details",
        "get_datasets_by_organization", "get_dataset_resources",
        "get_resource_details", "test_connection", "supported_countries"
    ]
    for action in actions:
        print(f"  - {action}")
    print()

def main():
    """Main CLI function."""
    if len(sys.argv) < 4 or '--help' in sys.argv or '-h' in sys.argv:
        print_usage()
        sys.exit(1)

    # Parse command line arguments
    country_code = None
    action = None
    dataset_id = None
    organization_id = None
    query = ""
    limit = 50

    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == '--country' and i + 1 < len(sys.argv):
            country_code = sys.argv[i + 1]
            i += 2
        elif arg == '--action' and i + 1 < len(sys.argv):
            action = sys.argv[i + 1]
            i += 2
        elif arg == '--id' and i + 1 < len(sys.argv):
            dataset_id = sys.argv[i + 1]
            i += 2
        elif arg == '--org' and i + 1 < len(sys.argv):
            organization_id = sys.argv[i + 1]
            i += 2
        elif arg == '--query' and i + 1 < len(sys.argv):
            query = sys.argv[i + 1]
            i += 2
        elif arg == '--limit' and i + 1 < len(sys.argv):
            try:
                limit = int(sys.argv[i + 1])
            except ValueError:
                print("Error: Limit must be a number")
                sys.exit(1)
            i += 2
        else:
            i += 1

    # Validate required arguments
    if not country_code or not action:
        print("Error: --country and --action are required")
        print_usage()
        sys.exit(1)

    # Execute the requested action
    try:
        if action == 'list_organizations':
            result = list_organizations(country_code, limit)
        elif action == 'get_organization_details':
            if not organization_id:
                print("Error: --org is required for get_organization_details")
                sys.exit(1)
            result = get_organization_details(country_code, organization_id)
        elif action == 'list_datasets':
            result = list_datasets(country_code, limit)
        elif action == 'search_datasets':
            result = search_datasets(country_code, query, limit, organization_id)
        elif action == 'get_dataset_details':
            if not dataset_id:
                print("Error: --id is required for get_dataset_details")
                sys.exit(1)
            result = get_dataset_details(country_code, dataset_id)
        elif action == 'get_datasets_by_organization':
            if not organization_id:
                print("Error: --org is required for get_datasets_by_organization")
                sys.exit(1)
            result = get_datasets_by_organization(country_code, organization_id, limit)
        elif action == 'get_dataset_resources':
            if not dataset_id:
                print("Error: --id is required for get_dataset_resources")
                sys.exit(1)
            result = get_dataset_resources(country_code, dataset_id)
        elif action == 'get_resource_details':
            if not dataset_id:
                print("Error: --id is required for get_resource_details")
                sys.exit(1)
            result = get_resource_details(country_code, dataset_id)
        elif action == 'test_connection':
            result = test_portal_connection(country_code)
        elif action == 'supported_countries':
            result = get_supported_countries()
        else:
            print(f"Error: Unknown action '{action}'")
            print_usage()
            sys.exit(1)

        # Output result as JSON
        print(json.dumps(result, indent=2, ensure_ascii=False))

        # Exit with error code if API call failed
        if result.get('error'):
            sys.exit(1)

    except Exception as e:
        error_result = {
            "data": [],
            "metadata": {},
            "error": f"CLI Error: {str(e)}"
        }
        print(json.dumps(error_result, indent=2))
        sys.exit(1)

if __name__ == "__main__":
    main()