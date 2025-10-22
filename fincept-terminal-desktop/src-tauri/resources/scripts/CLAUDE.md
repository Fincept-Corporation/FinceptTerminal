# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with API wrapper files in this repository.

## Overview

This directory contains Python API wrapper files that provide modular, fault-tolerant interfaces to various financial and economic data providers. Each wrapper follows a consistent design pattern and CLI interface for seamless integration with the Rust backend via Tauri commands.

## Architecture Pattern

### Core Design Principles

1. **Modular Endpoint Design**: Each API endpoint is implemented as an independent method with isolated error handling
2. **Fault Tolerance**: Failed endpoints don't crash the entire wrapper - errors are contained and reported
3. **CLI-First Interface**: All wrappers expose a command-line interface that outputs JSON for Rust integration
4. **Consistent Error Handling**: Standardized error format across all wrappers
5. **Rate Limiting**: Built-in rate limiting for APIs with usage restrictions
6. **Flexible Parameters**: Support for optional parameters with sensible defaults

### File Structure Pattern

```
<provider>_data.py
├── Header Documentation
├── Imports
├── Constants & Configuration
├── Error Classes
├── Main Wrapper Class
│   ├── Initialization & Configuration
│   ├── Private Helper Methods
│   ├── Individual Endpoint Methods
│   └── Composite Methods (optional)
└── CLI Interface (main function)
```

## Required Components

### 1. Header Documentation
```python
"""
Provider Name Data Fetcher
Brief description of the API and its capabilities
Returns JSON output for Rust integration

API Documentation:
- Base URL: https://api.example.com/v2
- Authentication: API key required
- Rate limits: X requests per minute
"""
```

### 2. Standard Imports
```python
import sys
import json
import os
import requests
from datetime import datetime, timedelta
from typing import Dict, Any, Optional, List, Union
```

### 3. Error Class Pattern
```python
class ProviderError:
    """Error handling wrapper for API responses"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success": False,
            "error": self.error,
            "endpoint": self.endpoint,
            "status_code": self.status_code,
            "timestamp": self.timestamp
        }
```

### 4. Main Wrapper Class
```python
class ProviderWrapper:
    """Modular Provider API wrapper with fault tolerance"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.environ.get('PROVIDER_API_KEY', '')
        self.base_url = "https://api.provider.com/v2"
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0',
            'Authorization': f'Bearer {self.api_key}' if self.api_key else ''
        })

        # Rate limiting (if applicable)
        self.last_request_time = 0
        self.min_request_interval = 1.0  # seconds

    def _rate_limit(self):
        """Implement rate limiting for API"""
        # Implementation varies by provider limits
        pass

    def _make_request(self, endpoint: str, params: Optional[Dict] = None) -> Dict[str, Any]:
        """Centralized request handler with comprehensive error handling"""
        # Standard implementation for all API calls
        pass
```

### 5. Endpoint Method Pattern

#### Individual Endpoint Method
```python
def get_<endpoint_name>(self, required_param: str, optional_param: str = "default") -> Dict[str, Any]:
    """Brief description of what this endpoint does

    Args:
        required_param: Description of required parameter
        optional_param: Description of optional parameter with default

    Returns:
        JSON response with data or error information
    """
    try:
        # Parameter validation
        if not required_param:
            return ProviderError('<endpoint_name>', 'Required parameter missing').to_dict()

        # Build request
        params = {
            'param1': required_param,
            'param2': optional_param
        }

        # Make API call
        result = self._make_request(endpoint, params)

        if "error" in result:
            return ProviderError('<endpoint_name>', result['error']).to_dict()

        # Process and format response data
        processed_data = self._process_response(result)

        return {
            "success": True,
            "endpoint": "<endpoint_name>",
            "data": processed_data,
            "parameters": {
                "required_param": required_param,
                "optional_param": optional_param
            },
            "timestamp": int(datetime.now().timestamp())
        }

    except Exception as e:
        return ProviderError('<endpoint_name>', str(e)).to_dict()
```

#### Composite Method Pattern
```python
def get_comprehensive_data(self, symbol: str) -> Dict[str, Any]:
    """Get comprehensive data from multiple endpoints - fails gracefully"""
    result = {
        "success": True,
        "symbol": symbol,
        "timestamp": int(datetime.now().timestamp()),
        "endpoints": {},
        "failed_endpoints": []
    }

    # Define endpoints to try
    endpoints = [
        ('quote', lambda: self.get_quote(symbol)),
        ('info', lambda: self.get_info(symbol)),
        ('historical', lambda: self.get_historical(symbol))
    ]

    overall_success = False

    for endpoint_name, endpoint_func in endpoints:
        try:
            endpoint_result = endpoint_func()
            result["endpoints"][endpoint_name] = endpoint_result

            if endpoint_result.get("success"):
                overall_success = True
            else:
                result["failed_endpoints"].append({
                    "endpoint": endpoint_name,
                    "error": endpoint_result.get("error", "Unknown error")
                })

        except Exception as e:
            result["failed_endpoints"].append({
                "endpoint": endpoint_name,
                "error": str(e)
            })

    result["success"] = overall_success
    return result
```

### 6. CLI Interface Pattern

```python
def main():
    """CLI interface for Provider Data Fetcher"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python provider_data.py <command> <args>",
            "available_commands": [
                "quote <symbol>",
                "info <symbol>",
                "historical <symbol> <start_date> <end_date>",
                "comprehensive <symbol>",
                "search <query>"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = ProviderWrapper()

    try:
        if command == "quote":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python provider_data.py quote <symbol>"}))
                sys.exit(1)

            symbol = sys.argv[2]
            result = wrapper.get_quote(symbol)
            print(json.dumps(result, indent=2))

        elif command == "info":
            # Similar pattern for other commands
            pass

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "quote <symbol>",
                    "info <symbol>",
                    "historical <symbol> <start_date> <end_date>",
                    "comprehensive <symbol>",
                    "search <query>"
                ]
            }))
            sys.exit(1)

    except KeyboardInterrupt:
        print(json.dumps({"error": "Operation cancelled by user"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"error": f"Unexpected error: {str(e)}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()
```

## Response Format Standards

### Success Response
```json
{
    "success": true,
    "endpoint": "endpoint_name",
    "data": [
        {
            "symbol": "AAPL",
            "price": 150.25,
            "change": 1.50,
            "timestamp": 1694918400
        }
    ],
    "parameters": {
        "symbol": "AAPL"
    },
    "metadata": {
        "total_records": 1,
        "source": "Provider API"
    },
    "timestamp": 1694918400
}
```

### Error Response
```json
{
    "success": false,
    "error": "API rate limit exceeded",
    "endpoint": "quote",
    "status_code": 429,
    "timestamp": 1694918400
}
```

## Common Endpoint Types

### 1. Quote/Price Data
- Real-time or delayed pricing information
- Volume, change, percentage change
- High/low/open/close data

### 2. Historical Data
- Time series data with date ranges
- OHLCV data for financial instruments
- Economic indicator historical values

### 3. Company/Entity Information
- Company profiles, descriptions
- Sector, industry classifications
- Market cap, employee counts

### 4. Search/Discovery
- Symbol lookup by name or ISIN
- Instrument filtering by criteria
- Market/sector browsing

### 5. Economic Data
- GDP, inflation, unemployment rates
- Central bank data
- Trade balance, interest rates

## Implementation Guidelines

### Authentication
- Use environment variables for API keys: `os.environ.get('PROVIDER_API_KEY', '')`
- Support both API key and bearer token authentication
- Include API key in session headers for all requests

### Rate Limiting
- Implement provider-specific rate limits
- Use time-based throttling when required
- Track last request time to avoid limits

### Data Normalization
- Convert all timestamps to Unix timestamps
- Standardize currency codes (ISO 4217)
- Normalize country/region codes
- Handle missing/null values consistently

### Error Handling
- Catch all exceptions and return structured errors
- Include HTTP status codes when available
- Provide meaningful error messages
- Never let exceptions propagate to CLI

### Date Handling
- Accept dates in YYYY-MM-DD format
- Support relative dates (e.g., "1y", "6m")
- Convert all dates to Unix timestamps in output
- Handle timezone conversions when needed

## Testing & Validation

### Command-Line Testing
```bash
# Test basic functionality
python provider_data.py quote AAPL

# Test with parameters
python provider_data.py historical AAPL 2023-01-01 2023-12-31

# Test error handling
python provider_data.py quote INVALID_SYMBOL
```

### Expected Output
- All commands output valid JSON
- Success responses include "success": true
- Error responses include "success": false and error details
- Output is properly formatted with indentation

## Integration with Rust Backend

### Tauri Command Pattern
```rust
#[tauri::command]
pub async fn fetch_provider_data(symbol: String) -> Result<String, String> {
    let output = Command::new("python")
        .arg("resources/scripts/provider_data.py")
        .arg("quote")
        .arg(symbol)
        .output()
        .map_err(|e| e.to_string())?;

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}
```

### Error Propagation
- Rust should parse JSON responses
- Handle both success and error cases
- Log errors appropriately
- Return user-friendly error messages

## Best Practices

1. **Documentation**: Include comprehensive docstrings for all methods
2. **Validation**: Validate all input parameters before API calls
3. **Caching**: Consider local caching for expensive API calls
4. **Logging**: Add debug logging for troubleshooting (optional)
5. **Type Hints**: Use proper type hints for all method signatures
6. **Constants**: Define API URLs and constants at module level
7. **Dependencies**: Keep external dependencies minimal
8. **Compatibility**: Ensure Python 3.7+ compatibility

## Example Implementation Template

See existing files for complete examples:
- `yfinance_data.py` - Basic market data wrapper
- `alphavantage_data.py` - Advanced wrapper with rate limiting
- `fred_data.py` - Economic data wrapper
- `worldbank_data.py` - International data wrapper
- `imf_data.py` - Complex multi-dataset wrapper

Each new wrapper should follow these patterns while adapting to the specific API's requirements and data structures.