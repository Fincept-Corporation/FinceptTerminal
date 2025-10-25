# Universal CKAN Data Portals API Wrapper

A comprehensive Python wrapper for accessing multiple CKAN-based data portals worldwide. This wrapper supports 8 major CKAN portals with a unified interface, making it easy to access open government data from different countries.

## Supported Portals

| Country | Code | Portal Name | Base URL |
|---------|------|-------------|----------|
| United States | `us` | Data.gov | `https://catalog.data.gov/api/3/action` |
| United Kingdom | `uk` | data.gov.uk | `https://data.gov.uk/api/3/action` |
| Australia | `au` | data.gov.au | `https://data.gov.au/api/3/action` |
| Italy | `it` | dati.gov.it | `https://www.dati.gov.it/api/3/action` |
| Brazil | `br` | dados.gov.br | `https://dados.gov.br/api/3/action` |
| Latvia | `lv` | data.gov.lv | `https://data.gov.lv/api/3/action` |
| Slovenia | `si` | podatki.gov.si | `https://podatki.gov.si/api/3/action` |
| Uruguay | `uy` | catalogodatos.gub.uy | `https://catalogodatos.gub.uy/api/3/action` |

## Features

- **Multi-country Support**: Access data from 8 different CKAN portals with a single interface
- **Hierarchical Data Access**: Navigate from organizations → datasets → resources
- **Comprehensive Error Handling**: Graceful degradation with detailed error messages
- **Modular Design**: Each endpoint works independently
- **Enhanced Metadata**: Additional computed fields for better data understanding
- **CLI Interface**: Command-line access to all functionality
- **Connection Testing**: Verify portal availability before making requests

## Installation

No external dependencies required beyond the Python standard library and `requests`.

```bash
pip install requests
```

## Quick Start

### Command Line Usage

```bash
# List all datasets from US portal
python universal_ckan_api.py --country us --action list_datasets

# Search for climate data in Australia
python universal_ckan_api.py --country au --action search_datasets --query "climate"

# Get dataset details from UK portal
python universal_ckan_api.py --country uk --action dataset_details --id "dataset-name"

# Test connection to a portal
python universal_ckan_api.py --country it --action test_connection

# List supported countries
python universal_ckan_api.py --country us --action supported_countries
```

### Python Usage

```python
from universal_ckan_api import (
    search_datasets,
    get_dataset_details,
    get_dataset_resources,
    list_organizations
)

# Search for datasets in the US portal
result = search_datasets('us', query='climate', rows=10)
if not result['error']:
    print(f"Found {result['metadata']['count']} datasets")
    for dataset in result['data']:
        print(f"- {dataset['title']}")

# Get detailed information about a dataset
dataset_info = get_dataset_details('us', 'consumer-complaint-database')
if not dataset_info['error']:
    print(f"Dataset: {dataset_info['data']['title']}")
    print(f"Resources: {dataset_info['data']['num_resources']}")

# Get all resources for a dataset
resources = get_dataset_resources('us', 'consumer-complaint-database')
if not resources['error']:
    for resource in resources['data']:
        print(f"- {resource['name']} ({resource['format']})")

# List organizations
orgs = list_organizations('au', limit=20)
if not orgs['error']:
    for org in orgs['data']:
        print(f"- {org['display_name']}")
```

## API Reference

### Organization Functions

#### `list_organizations(country_code, limit=None)`
List all organizations in the specified portal.

**Parameters:**
- `country_code` (str): Two-letter country code (us, uk, au, etc.)
- `limit` (int, optional): Maximum number of organizations to return

**Returns:** Dictionary with `data`, `metadata`, and `error` fields

#### `get_organization_details(country_code, organization_id)`
Get detailed information about a specific organization.

**Parameters:**
- `country_code` (str): Two-letter country code
- `organization_id` (str): ID or name of the organization

**Returns:** Dictionary with organization details

### Dataset Functions

#### `list_datasets(country_code, limit=50)`
List all datasets in the specified portal.

**Parameters:**
- `country_code` (str): Two-letter country code
- `limit` (int): Maximum number of datasets to return

**Returns:** Dictionary with list of datasets

#### `search_datasets(country_code, query="", rows=50, organization=None, tags=None)`
Search for datasets in the specified portal.

**Parameters:**
- `country_code` (str): Two-letter country code
- `query` (str): Search query string
- `rows` (int): Number of results to return
- `organization` (str, optional): Filter by organization name
- `tags` (list, optional): Filter by tags

**Returns:** Dictionary with search results

#### `get_dataset_details(country_code, dataset_id)`
Get detailed information about a specific dataset.

**Parameters:**
- `country_code` (str): Two-letter country code
- `dataset_id` (str): ID or name of the dataset

**Returns:** Dictionary with dataset details

#### `get_datasets_by_organization(country_code, organization_id, limit=None)`
Get all datasets belonging to a specific organization.

**Parameters:**
- `country_code` (str): Two-letter country code
- `organization_id` (str): ID or name of the organization
- `limit` (int, optional): Maximum number of datasets to return

**Returns:** Dictionary with organization datasets

### Resource Functions

#### `get_dataset_resources(country_code, dataset_id)`
Get all resources (data files) for a specific dataset.

**Parameters:**
- `country_code` (str): Two-letter country code
- `dataset_id` (str): ID or name of the dataset

**Returns:** Dictionary with dataset resources

#### `get_resource_details(country_code, resource_id)`
Get detailed information about a specific resource.

**Parameters:**
- `country_code` (str): Two-letter country code
- `resource_id` (str): ID of the resource

**Returns:** Dictionary with resource details

### Utility Functions

#### `get_supported_countries()`
Get list of supported countries and their portal information.

**Returns:** Dictionary with supported countries

#### `test_portal_connection(country_code)`
Test connection to a specific CKAN portal.

**Parameters:**
- `country_code` (str): Two-letter country code

**Returns:** Dictionary with connection test results

## Response Format

All API functions return a consistent dictionary format:

```python
{
    "data": [...],           # Actual API response data
    "metadata": {            # Additional context information
        "source": "Portal Name",
        "country": "US",
        "action": "package_search",
        "last_updated": "2024-01-01T12:00:00",
        "count": 10,
        "url": "https://...",
        # ... other metadata fields
    },
    "error": None           # Error message if failed, None if successful
}
```

## Error Handling

The wrapper includes comprehensive error handling:

- **Connection Errors**: Network issues, timeouts
- **HTTP Errors**: Invalid responses, status codes
- **API Errors**: CKAN-specific errors
- **Validation Errors**: Invalid parameters, unsupported countries

All errors are returned in the `error` field with descriptive messages, allowing the application to continue functioning even when individual endpoints fail.

## Data Enhancement

The wrapper automatically enhances raw CKAN data with additional computed fields:

### Dataset Enhancements
- `num_resources`: Number of resources in the dataset
- `has_tags`: Whether the dataset has tags
- `organization_name`: Human-readable organization name
- `resource_types`: List of unique resource formats
- `metadata_created`: Dataset creation timestamp
- `metadata_modified`: Dataset modification timestamp

### Organization Enhancements
- `package_count`: Number of datasets/packages
- `country`: Country code

## CLI Reference

### Basic Syntax

```bash
python universal_ckan_api.py --country <CODE> --action <ACTION> [OPTIONS]
```

### Options

- `--country <code>`: Two-letter country code (required)
- `--action <action>`: Action to perform (required)
- `--id <id>`: Dataset or resource ID (for specific actions)
- `--org <org>`: Organization ID or name
- `--query <query>`: Search query string
- `--limit <number>`: Maximum results to return

### Available Actions

- `list_organizations`: List all organizations
- `get_organization_details`: Get organization details
- `list_datasets`: List all datasets
- `search_datasets`: Search datasets
- `get_dataset_details`: Get dataset details
- `get_datasets_by_organization`: Get datasets by organization
- `get_dataset_resources`: Get dataset resources
- `get_resource_details`: Get resource details
- `test_connection`: Test portal connection
- `supported_countries`: List supported countries

## Examples

### Example 1: Climate Data Research

```python
# Search for climate data across multiple countries
countries = ['us', 'uk', 'au', 'ca']
climate_datasets = []

for country in countries:
    result = search_datasets(country, query='climate change', rows=20)
    if not result['error']:
        for dataset in result['data']:
            climate_datasets.append({
                'title': dataset['title'],
                'country': country.upper(),
                'organization': dataset['organization_name'],
                'resources': dataset['num_resources']
            })

print(f"Found {len(climate_datasets)} climate datasets")
```

### Example 2: Bulk Dataset Analysis

```python
# Analyze datasets from a specific organization
org_datasets = get_datasets_by_organization('us', 'census-bureau')

if not org_datasets['error']:
    total_resources = 0
    formats = {}

    for dataset in org_datasets['data']:
        total_resources += dataset.get('num_resources', 0)

        for resource in dataset.get('resources', []):
            format_type = resource.get('format', 'unknown').lower()
            formats[format_type] = formats.get(format_type, 0) + 1

    print(f"Total datasets: {len(org_datasets['data'])}")
    print(f"Total resources: {total_resources}")
    print("Resource formats:", formats)
```

### Example 3: Portal Health Check

```python
# Test all supported portals
health_status = {}

for country in CKAN_PORTALS.keys():
    result = test_portal_connection(country)
    health_status[country] = {
        'status': result['data']['status'],
        'portal': result['data']['portal']
    }

print("Portal Health Status:")
for country, status in health_status.items():
    print(f"{country.upper()}: {status['status']} - {status['portal']}")
```

## Rate Limiting

While this wrapper doesn't implement rate limiting internally, it's important to be mindful of the individual portal's rate limits when making multiple requests. Consider implementing delays between requests for bulk operations.

## Contributing

To add support for additional CKAN portals:

1. Add the portal configuration to the `CKAN_PORTALS` dictionary
2. Update the documentation with the new portal
3. Test the implementation with the new portal

## License

This wrapper is provided as-is for educational and research purposes. Please respect the terms of service of the individual data portals when using this tool.

## Official Documentation

- [CKAN API Documentation](https://docs.ckan.org/en/2.9/api/)
- [CKAN User Guide](https://docs.ckan.org/en/2.9/user-guide.html)