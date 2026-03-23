"""
Fetch all available trading pairs/markets from an exchange.
Used to populate symbol search and validate trading pairs.

Usage: python fetch_markets.py <exchange_id> [type]
Example: python fetch_markets.py binance spot
         python fetch_markets.py binance swap

Output JSON:
{
  "success": true,
  "data": {
    "exchange": "binance",
    "markets": [
      {
        "symbol": "BTC/USDT",
        "base": "BTC",
        "quote": "USDT",
        "type": "spot",
        "active": true,
        "maker_fee": 0.001,
        "taker_fee": 0.001,
        "precision_amount": 5,
        "precision_price": 2,
        "min_amount": 0.00001,
        "min_cost": 10.0
      },
      ...
    ],
    "count": 2000
  }
}
"""

import sys
from exchange_client import (
    make_exchange, output_success, output_error, run_with_error_handling,
    save_markets_cache,
)


@run_with_error_handling
def main():
    if len(sys.argv) < 2:
        output_error("Usage: fetch_markets.py <exchange_id> [type]", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    market_type = sys.argv[2] if len(sys.argv) > 2 else None

    exchange = make_exchange(exchange_id)
    exchange.load_markets()
    # Persist to disk cache so place_order.py / cancel_order.py can skip load_markets()
    save_markets_cache(exchange_id, exchange.markets)

    markets = []
    for symbol, market in exchange.markets.items():
        if market_type and market.get("type") != market_type:
            continue
        if not market.get("active", True):
            continue

        limits = market.get("limits", {})
        amount_limits = limits.get("amount", {})
        cost_limits = limits.get("cost", {})
        precision = market.get("precision", {})

        markets.append({
            "symbol": market["symbol"],
            "base": market.get("base"),
            "quote": market.get("quote"),
            "type": market.get("type"),
            "active": market.get("active"),
            "maker_fee": market.get("maker"),
            "taker_fee": market.get("taker"),
            "precision_amount": precision.get("amount"),
            "precision_price": precision.get("price"),
            "min_amount": amount_limits.get("min"),
            "min_cost": cost_limits.get("min"),
        })

    output_success({
        "exchange": exchange_id,
        "markets": markets,
        "count": len(markets),
    })


if __name__ == "__main__":
    main()
