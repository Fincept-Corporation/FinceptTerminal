"""
Fetch order book (bids/asks) for a symbol.
Used for depth visualization and slippage estimation.

Usage: python fetch_orderbook.py <exchange_id> <symbol> [limit]
Example: python fetch_orderbook.py binance BTC/USDT 20

Output JSON:
{
  "success": true,
  "data": {
    "symbol": "BTC/USDT",
    "bids": [[70990.49, 1.33], [70990.48, 0.0004], ...],
    "asks": [[70990.50, 1.56], [70990.51, 0.00079], ...],
    "timestamp": 1773466621013,
    "best_bid": 70990.49,
    "best_ask": 70990.50,
    "spread": 0.01,
    "spread_pct": 0.000014
  }
}
"""

import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling


@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_orderbook.py <exchange_id> <symbol> [limit]", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 20

    exchange = make_exchange(exchange_id)
    ob = exchange.fetch_order_book(symbol, limit=limit)

    best_bid = ob["bids"][0][0] if ob["bids"] else 0
    best_ask = ob["asks"][0][0] if ob["asks"] else 0
    spread = best_ask - best_bid if best_bid and best_ask else 0
    spread_pct = (spread / best_ask * 100) if best_ask else 0

    output_success({
        "symbol": ob.get("symbol", symbol),
        "bids": ob["bids"],
        "asks": ob["asks"],
        "timestamp": ob.get("timestamp"),
        "best_bid": best_bid,
        "best_ask": best_ask,
        "spread": spread,
        "spread_pct": round(spread_pct, 6),
    })


if __name__ == "__main__":
    main()
