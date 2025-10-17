"""
Alpha Vantage Data Fetcher
Fetches stock quotes and data from Alpha Vantage API
Returns JSON output for Rust integration
"""

import sys
import json
import os
import requests
from typing import Dict, Any

# API Configuration
API_KEY = os.environ.get('', '')
BASE_URL = "https://www.alphavantage.co/query"


def get_quote(symbol: str) -> Dict[str, Any]:
    """Fetch real-time quote for a stock symbol"""
    try:
        if not API_KEY:
            return {"error": "Alpha Vantage API key not configured"}

        params = {
            'function': 'GLOBAL_QUOTE',
            'symbol': symbol,
            'apikey': API_KEY
        }

        response = requests.get(BASE_URL, params=params, timeout=10)
        response.raise_for_status()

        data = response.json()

        if 'Global Quote' not in data:
            return {"error": "No data returned for symbol", "symbol": symbol}

        quote = data['Global Quote']

        result = {
            "symbol": symbol,
            "price": float(quote.get('05. price', 0)),
            "change": float(quote.get('09. change', 0)),
            "change_percent": quote.get('10. change percent', '0'),
            "volume": int(quote.get('06. volume', 0)),
            "open": float(quote.get('02. open', 0)),
            "high": float(quote.get('03. high', 0)),
            "low": float(quote.get('04. low', 0)),
            "previous_close": float(quote.get('08. previous close', 0)),
            "trading_day": quote.get('07. latest trading day', '')
        }

        return result

    except requests.exceptions.RequestException as e:
        return {"error": f"Network error: {str(e)}", "symbol": symbol}
    except Exception as e:
        return {"error": str(e), "symbol": symbol}


def main():
    """Main CLI entry point"""
    if len(sys.argv) < 3:
        print(json.dumps({
            "error": "Usage: python alphavantage_data.py quote <symbol>"
        }))
        sys.exit(1)

    command = sys.argv[1]
    symbol = sys.argv[2]

    if command == "quote":
        result = get_quote(symbol)
        print(json.dumps(result, indent=2))
    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))
        sys.exit(1)


if __name__ == "__main__":
    main()
