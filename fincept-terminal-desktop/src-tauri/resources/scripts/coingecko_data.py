"""
CoinGecko Data Fetcher
Fetches cryptocurrency price and market data from the CoinGecko API.
Returns JSON output for Rust integration.
"""

import sys
import json
import os
import requests
from typing import Dict, Any, List

# 1. CONFIGURATION (from your guide)
# The public CoinGecko API doesn't require a key, but we'll use their pro URL if a key is found.
API_KEY = os.environ.get('COINGECKO_API_KEY')
if API_KEY:
    BASE_URL = "https://pro-api.coingecko.com/api/v3"
else:
    BASE_URL = "https://api.coingecko.com/api/v3"

def _make_request(endpoint: str, params: Dict[str, Any] = None) -> Any:
    """A private helper function to handle all API requests and errors."""
    full_url = f"{BASE_URL}/{endpoint}"
    
    # Add API key to params if using the pro version
    if API_KEY:
        if params is None:
            params = {}
        params['x_cg_pro_api_key'] = API_KEY

    try:
        response = requests.get(full_url, params=params, timeout=10)
        response.raise_for_status()  # Raise an exception for bad status codes (4xx or 5xx)
        return response.json()
    except requests.exceptions.HTTPError as e:
        # Return a standardized error format
        return {"error": f"HTTP Error: {e.response.status_code} - {e.response.text}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Network or request error: {str(e)}"}
    except json.JSONDecodeError:
        return {"error": "Failed to decode API response."}

# 2. CORE FUNCTIONS (from your guide)

def get_ping() -> Dict[str, Any]:
    """Checks if the CoinGecko API is responsive."""
    return _make_request("ping")

def get_prices(coin_ids: List[str]) -> Dict[str, Any]:
    """
    Fetches the current price for one or more cryptocurrencies.

    Args:
        coin_ids: A list of coin IDs (e.g., ['bitcoin', 'ethereum']).

    Returns:
        A dictionary with formatted price data or an error.
    """
    if not coin_ids:
        return {"error": "No coin IDs provided."}

    params = {
        'ids': ",".join(coin_ids),  # API expects a comma-separated string
        'vs_currencies': 'usd'
    }
    
    data = _make_request("simple/price", params=params)

    if data and "error" not in data:
        # Format the data into the standardized response format
        results = []
        for coin_id, prices in data.items():
            results.append({
                "id": coin_id,
                "price_usd": prices.get('usd', 0)
            })
        return {"data": results}
    return data # Return the error response if something went wrong

def get_market_info(coin_id: str) -> Dict[str, Any]:
    """
    Fetches detailed market information for a single cryptocurrency.

    Args:
        coin_id: The ID of the coin (e.g., 'bitcoin').

    Returns:
        A dictionary with formatted market data or an error.
    """
    params = {
        'vs_currency': 'usd',
        'ids': coin_id
    }
    data = _make_request("coins/markets", params=params)

    # API returns a list, we only want the first item
    if data and "error" not in data and isinstance(data, list) and len(data) > 0:
        coin_data = data[0]
        # Format into a standardized response
        return {
            "id": coin_data.get('id'),
            "symbol": coin_data.get('symbol'),
            "name": coin_data.get('name'),
            "image": coin_data.get('image'),
            "current_price": coin_data.get('current_price'),
            "market_cap": coin_data.get('market_cap'),
            "total_volume": coin_data.get('total_volume'),
            "price_change_24h": coin_data.get('price_change_percentage_24h')
        }
    elif isinstance(data, list) and len(data) == 0:
        return {"error": f"No market data found for ID: {coin_id}"}
    return data # Return original error or empty list

# 3. CLI INTERFACE (from your guide)

def main():
    """Main Command-Line Interface entry point."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python coingecko_data.py <command> [args]",
            "commands": ["ping", "price <id1> [id2...]", "info <id>"]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1].lower()
    result = {}

    if command == "ping":
        result = get_ping()
    elif command == "price":
        if len(sys.argv) < 3:
            result = {"error": "Usage: python coingecko_data.py price <coin_id_1> [coin_id_2]..."}
        else:
            coin_ids = sys.argv[2:]
            result = get_prices(coin_ids)
    elif command == "info":
        if len(sys.argv) != 3:
            result = {"error": "Usage: python coingecko_data.py info <coin_id>"}
        else:
            coin_id = sys.argv[2]
            result = get_market_info(coin_id)
    else:
        result = {"error": f"Unknown command: {command}"}

    # Print the final JSON output
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()