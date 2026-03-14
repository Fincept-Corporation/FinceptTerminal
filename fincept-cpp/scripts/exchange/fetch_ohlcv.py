"""
Fetch OHLCV candlestick data for charting and technical analysis.

Usage: python fetch_ohlcv.py <exchange_id> <symbol> [timeframe] [limit]
Example: python fetch_ohlcv.py binance BTC/USDT 1h 100

Output JSON:
{
  "success": true,
  "data": {
    "symbol": "BTC/USDT",
    "timeframe": "1h",
    "candles": [
      {"timestamp": 1773457200000, "open": 70783.27, "high": 71185.23, "low": 70783.26, "close": 71081.3, "volume": 944.72},
      ...
    ],
    "count": 100
  }
}
"""

import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling


@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_ohlcv.py <exchange_id> <symbol> [timeframe] [limit]", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]
    timeframe = sys.argv[3] if len(sys.argv) > 3 else "1h"
    limit = int(sys.argv[4]) if len(sys.argv) > 4 else 100

    exchange = make_exchange(exchange_id)
    candles = exchange.fetch_ohlcv(symbol, timeframe, limit=limit)

    output_success({
        "symbol": symbol,
        "timeframe": timeframe,
        "candles": [
            {
                "timestamp": c[0],
                "open": c[1],
                "high": c[2],
                "low": c[3],
                "close": c[4],
                "volume": c[5],
            }
            for c in candles
        ],
        "count": len(candles),
    })


if __name__ == "__main__":
    main()
