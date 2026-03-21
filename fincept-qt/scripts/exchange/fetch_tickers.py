"""
Fetch tickers for multiple symbols at once (batch price update).
Used by C++ price feed to update all watched symbols in one call.

Usage: python fetch_tickers.py <exchange_id> <symbol1> <symbol2> ...
Example: python fetch_tickers.py binance BTC/USDT ETH/USDT SOL/USDT

Output JSON:
{
  "success": true,
  "data": {
    "tickers": [
      {"symbol": "BTC/USDT", "last": 70990.5, "bid": 70990.49, "ask": 70990.5, ...},
      {"symbol": "ETH/USDT", "last": 2450.0, ...}
    ],
    "count": 2,
    "exchange": "binance"
  }
}
"""

import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling


def normalize_ticker(ticker: dict) -> dict:
    return {
        "symbol": ticker["symbol"],
        "last": ticker.get("last"),
        "bid": ticker.get("bid"),
        "ask": ticker.get("ask"),
        "high": ticker.get("high"),
        "low": ticker.get("low"),
        "open": ticker.get("open"),
        "close": ticker.get("close"),
        "change": ticker.get("change"),
        "percentage": ticker.get("percentage"),
        "base_volume": ticker.get("baseVolume"),
        "quote_volume": ticker.get("quoteVolume"),
        "timestamp": ticker.get("timestamp"),
    }


@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_tickers.py <exchange_id> <symbol1> [symbol2] ...", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbols = sys.argv[2:]

    exchange = make_exchange(exchange_id)
    tickers = exchange.fetch_tickers(symbols)

    result = []
    for symbol in symbols:
        if symbol in tickers:
            result.append(normalize_ticker(tickers[symbol]))

    output_success({
        "tickers": result,
        "count": len(result),
        "exchange": exchange_id,
    })


if __name__ == "__main__":
    main()
