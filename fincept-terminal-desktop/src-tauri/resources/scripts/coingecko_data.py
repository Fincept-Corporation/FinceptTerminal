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

# 1. CONFIGURATION
API_KEY = os.environ.get('COINGECKO_API_KEY')
if API_KEY:
    BASE_URL = "https://pro-api.coingecko.com/api/v3"
else:
    BASE_URL = "https://api.coingecko.com/api/v3"

def _make_request(endpoint: str, params: Dict[str, Any] = None) -> Any:
    """A private helper function to handle all API requests and errors."""
    full_url = f"{BASE_URL}/{endpoint}"
    
    if API_KEY:
        if params is None:
            params = {}
        params['x_cg_pro_api_key'] = API_KEY

    try:
        response = requests.get(full_url, params=params, timeout=15)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP Error: {e.response.status_code} - {e.response.text}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Network or request error: {str(e)}"}
    except json.JSONDecodeError:
        return {"error": "Failed to decode API response."}

# 2. CORE FUNCTIONS

def get_ping() -> Dict[str, Any]:
    """1. Checks if the CoinGecko API is responsive."""
    return _make_request("ping")

def get_prices(coin_ids: List[str]) -> Dict[str, Any]:
    """2. Fetches the current price for one or more cryptocurrencies."""
    if not coin_ids:
        return {"error": "No coin IDs provided."}
    params = {'ids': ",".join(coin_ids), 'vs_currencies': 'usd'}
    data = _make_request("simple/price", params=params)

    if data and "error" not in data:
        results = [{"id": coin_id, "price_usd": prices.get('usd', 0)} for coin_id, prices in data.items()]
        return {"data": results}
    return data

def get_market_info(coin_id: str) -> Dict[str, Any]:
    """3. Fetches detailed market information for a single cryptocurrency."""
    params = {'vs_currency': 'usd', 'ids': coin_id}
    data = _make_request("coins/markets", params=params)

    if data and "error" not in data and isinstance(data, list) and len(data) > 0:
        return data[0] # The raw API response is already well-formatted
    elif isinstance(data, list) and len(data) == 0:
        return {"error": f"No market data found for ID: {coin_id}"}
    return data

def get_historical_data(coin_id: str, date: str) -> Dict[str, Any]:
    """4. Fetches historical data for a coin on a specific date."""
    params = {'date': date} # API expects DD-MM-YYYY format
    data = _make_request(f"coins/{coin_id}/history", params=params)
    
    if data and "error" not in data:
        # Standardize the response
        return {
            "id": data.get('id'),
            "symbol": data.get('symbol'),
            "date": date,
            "price_usd": data.get('market_data', {}).get('current_price', {}).get('usd'),
            "market_cap_usd": data.get('market_data', {}).get('market_cap', {}).get('usd'),
            "volume_usd": data.get('market_data', {}).get('total_volume', {}).get('usd')
        }
    return data

def get_market_chart(coin_id: str, days: int) -> Dict[str, Any]:
    """5. Fetches market chart data (prices, market caps, volumes) for a coin."""
    params = {'vs_currency': 'usd', 'days': days}
    data = _make_request(f"coins/{coin_id}/market_chart", params=params)
    return data # The raw response is already well-structured for charting

def get_trending_coins() -> Dict[str, Any]:
    """6. Fetches the top-7 trending coins on CoinGecko."""
    data = _make_request("search/trending")
    if data and "error" not in data:
        # The API returns a list of items inside a 'coins' key
        return {"trending": data.get("coins", [])}
    return data
    
def search_coins(query: str) -> Dict[str, Any]:
    """7. Searches for coins by name or symbol."""
    params = {'query': query}
    data = _make_request("search", params=params)
    if data and "error" not in data:
        return {"results": data.get("coins", [])}
    return data

def get_global_market_data() -> Dict[str, Any]:
    """8. Fetches global crypto market data (e.g., BTC dominance)."""
    response = _make_request("global")
    if response and "error" not in response:
        return response.get("data", {}) # Data is nested under a 'data' key
    return response

# 3. CLI INTERFACE

def main():
    """Main Command-Line Interface entry point."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python coingecko_data.py <command> [args]",
            "commands": [
                "ping", 
                "price <id1> [id2...]", 
                "info <id>", 
                "history <id> <dd-mm-yyyy>",
                "chart <id> <days>",
                "trending",
                "search <query>",
                "global"
            ]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1].lower()
    result = {}

    try:
        if command == "ping":
            result = get_ping()
        elif command == "price":
            result = get_prices(sys.argv[2:])
        elif command == "info":
            result = get_market_info(sys.argv[2])
        elif command == "history":
            result = get_historical_data(sys.argv[2], sys.argv[3])
        elif command == "chart":
            result = get_market_chart(sys.argv[2], int(sys.argv[3]))
        elif command == "trending":
            result = get_trending_coins()
        elif command == "search":
            result = search_coins(" ".join(sys.argv[2:]))
        elif command == "global":
            result = get_global_market_data()
        else:
            result = {"error": f"Unknown command: {command}"}
    except IndexError:
        result = {"error": "Missing arguments for command."}
    except Exception as e:
        result = {"error": f"An unexpected error occurred: {e}"}

    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()