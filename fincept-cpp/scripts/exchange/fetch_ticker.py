"""
Fetch ticker (last price, bid, ask, volume) for a symbol.
Used by C++ OrderMatcher to get live prices for paper trading fills.

Usage: python fetch_ticker.py <exchange_id> <symbol>
Example: python fetch_ticker.py binance BTC/USDT
         python fetch_ticker.py kraken ETH/USD

Output JSON:
{
  "success": true,
  "data": {
    "symbol": "BTC/USDT",
    "last": 70990.5,
    "bid": 70990.49,
    "ask": 70990.5,
    "high": 73913.74,
    "low": 70481.7,
    "open": 71568.0,
    "close": 70990.5,
    "change": -577.5,
    "percentage": -0.807,
    "base_volume": 33621.71,
    "quote_volume": 2419989519.12,
    "timestamp": 1773466621013
  }
}
"""

import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling


@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_ticker.py <exchange_id> <symbol>", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]

    exchange = make_exchange(exchange_id)
    ticker = exchange.fetch_ticker(symbol)

    output_success({
        "symbol": ticker["symbol"],
        "last": ticker.get("last"),
        "bid": ticker.get("bid"),
        "ask": ticker.get("ask"),
        "bid_volume": ticker.get("bidVolume"),
        "ask_volume": ticker.get("askVolume"),
        "high": ticker.get("high"),
        "low": ticker.get("low"),
        "open": ticker.get("open"),
        "close": ticker.get("close"),
        "change": ticker.get("change"),
        "percentage": ticker.get("percentage"),
        "vwap": ticker.get("vwap"),
        "base_volume": ticker.get("baseVolume"),
        "quote_volume": ticker.get("quoteVolume"),
        "timestamp": ticker.get("timestamp"),
    })


if __name__ == "__main__":
    main()
