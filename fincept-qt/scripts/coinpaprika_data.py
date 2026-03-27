"""
CoinPaprika Data Fetcher
71000+ assets, market caps, OHLCV, exchanges, events.
No API key required for public endpoints.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.coinpaprika.com/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_global() -> Any:
    """Get global cryptocurrency market overview."""
    data = _make_request("global")
    return data


def get_coins(limit: int = 100) -> Any:
    """Get list of all coins with basic info."""
    data = _make_request("coins")
    if isinstance(data, list):
        return {"coins": data[:limit], "total_count": len(data)}
    return data


def get_coin(coin_id: str) -> Any:
    """Get detailed info for a specific coin (e.g. btc-bitcoin, eth-ethereum)."""
    return _make_request(f"coins/{coin_id}")


def get_coin_ohlcv(coin_id: str, start: str = None, end: str = None, limit: int = 365) -> Any:
    """Get OHLCV daily data for a coin.
    start/end format: YYYY-MM-DD. Default: last 365 days.
    """
    params = {"limit": min(limit, 366)}
    if start:
        params["start"] = start
    if end:
        params["end"] = end
    data = _make_request(f"coins/{coin_id}/ohlcv/historical", params=params)
    if isinstance(data, list):
        return {"coin_id": coin_id, "ohlcv": data, "count": len(data)}
    return data


def get_coin_today_ohlcv(coin_id: str, quote: str = "usd") -> Any:
    """Get today's OHLCV for a coin."""
    params = {"quote": quote}
    data = _make_request(f"coins/{coin_id}/ohlcv/today", params=params)
    if isinstance(data, list):
        return {"coin_id": coin_id, "quote": quote, "today": data}
    return data


def get_exchanges() -> Any:
    """Get list of all exchanges with volume data."""
    data = _make_request("exchanges")
    if isinstance(data, list):
        return {"exchanges": data[:100], "total_count": len(data)}
    return data


def get_exchange(exchange_id: str) -> Any:
    """Get detailed data for a specific exchange (e.g. binance, coinbase-pro)."""
    return _make_request(f"exchanges/{exchange_id}")


def get_markets(coin_id: str) -> Any:
    """Get markets (trading pairs) for a specific coin."""
    params = {"quotes": "USD,BTC"}
    data = _make_request(f"coins/{coin_id}/markets", params=params)
    if isinstance(data, list):
        return {"coin_id": coin_id, "markets": data[:50], "count": len(data)}
    return data


def get_tickers(limit: int = 100, quotes: str = "USD") -> Any:
    """Get price tickers for top coins with market data."""
    params = {"limit": min(limit, 250), "quotes": quotes}
    data = _make_request("tickers", params=params)
    if isinstance(data, list):
        return {"tickers": data, "count": len(data)}
    return data


def get_coin_events(coin_id: str) -> Any:
    """Get upcoming and past events for a coin."""
    data = _make_request(f"coins/{coin_id}/events")
    if isinstance(data, list):
        return {"coin_id": coin_id, "events": data, "count": len(data)}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: global, coins, coin, ohlcv, today_ohlcv, exchanges, exchange, markets, tickers, events"}))
        return

    command = args[0]

    if command == "global":
        result = get_global()
    elif command == "coins":
        limit = int(args[1]) if len(args) > 1 else 100
        result = get_coins(limit)
    elif command == "coin":
        if len(args) < 2:
            result = {"error": "Usage: coin <coin_id> (e.g. btc-bitcoin, eth-ethereum)"}
        else:
            result = get_coin(args[1])
    elif command == "ohlcv":
        if len(args) < 2:
            result = {"error": "Usage: ohlcv <coin_id> [start_date] [end_date] [limit]"}
        else:
            start = args[2] if len(args) > 2 else None
            end = args[3] if len(args) > 3 else None
            limit = int(args[4]) if len(args) > 4 else 365
            result = get_coin_ohlcv(args[1], start, end, limit)
    elif command == "today_ohlcv":
        if len(args) < 2:
            result = {"error": "Usage: today_ohlcv <coin_id> [quote]"}
        else:
            quote = args[2] if len(args) > 2 else "usd"
            result = get_coin_today_ohlcv(args[1], quote)
    elif command == "exchanges":
        result = get_exchanges()
    elif command == "exchange":
        if len(args) < 2:
            result = {"error": "Usage: exchange <exchange_id> (e.g. binance, coinbase-pro)"}
        else:
            result = get_exchange(args[1])
    elif command == "markets":
        if len(args) < 2:
            result = {"error": "Usage: markets <coin_id>"}
        else:
            result = get_markets(args[1])
    elif command == "tickers":
        limit = int(args[1]) if len(args) > 1 else 100
        quotes = args[2] if len(args) > 2 else "USD"
        result = get_tickers(limit, quotes)
    elif command == "events":
        if len(args) < 2:
            result = {"error": "Usage: events <coin_id>"}
        else:
            result = get_coin_events(args[1])
    else:
        result = {"error": f"Unknown command: {command}. Available: global, coins, coin, ohlcv, today_ohlcv, exchanges, exchange, markets, tickers, events"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
