"""
Fetch recent public trades for a symbol.

Usage: python fetch_trades.py <exchange_id> <symbol> [limit]
Example: python fetch_trades.py binance BTC/USDT 50

Output JSON:
{
  "success": true,
  "data": {
    "symbol": "BTC/USDT",
    "trades": [
      {"id": "3904295246", "timestamp": 1773466627395, "side": "sell", "price": 70967.38, "amount": 0.00016, "cost": 11.35},
      ...
    ],
    "count": 50
  }
}
"""

import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling


@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_trades.py <exchange_id> <symbol> [limit]", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 50

    exchange = make_exchange(exchange_id)
    trades = exchange.fetch_trades(symbol, limit=limit)

    output_success({
        "symbol": symbol,
        "trades": [
            {
                "id": t.get("id"),
                "timestamp": t.get("timestamp"),
                "side": t.get("side"),
                "price": t.get("price"),
                "amount": t.get("amount"),
                "cost": t.get("cost"),
            }
            for t in trades
        ],
        "count": len(trades),
    })


if __name__ == "__main__":
    main()
