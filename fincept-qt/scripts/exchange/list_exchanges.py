"""
List all supported exchanges with feature availability.

Usage: python list_exchanges.py

Output JSON:
{
  "success": true,
  "data": {
    "exchanges": [
      {
        "id": "binance",
        "name": "Binance",
        "countries": ["JP"],
        "has_fetch_ticker": true,
        "has_fetch_order_book": true,
        "has_fetch_ohlcv": true,
        "has_create_order": true,
        "has_fetch_balance": true,
        "has_fetch_positions": true,
        "has_set_leverage": true
      },
      ...
    ],
    "count": 110,
    "version": "4.5.28"
  }
}
"""

import ccxt
from exchange_client import output_success, run_with_error_handling


@run_with_error_handling
def main():
    exchanges = []

    for exchange_id in sorted(ccxt.exchanges):
        try:
            exchange_class = getattr(ccxt, exchange_id)
            ex = exchange_class()
            desc = ex.describe()
            has = ex.has

            exchanges.append({
                "id": exchange_id,
                "name": desc.get("name", exchange_id),
                "countries": desc.get("countries", []),
                "has_fetch_ticker": bool(has.get("fetchTicker")),
                "has_fetch_order_book": bool(has.get("fetchOrderBook")),
                "has_fetch_ohlcv": bool(has.get("fetchOHLCV")),
                "has_create_order": bool(has.get("createOrder")),
                "has_fetch_balance": bool(has.get("fetchBalance")),
                "has_fetch_positions": bool(has.get("fetchPositions")),
                "has_set_leverage": bool(has.get("setLeverage")),
            })
        except Exception:
            exchanges.append({"id": exchange_id, "name": exchange_id, "error": True})

    output_success({
        "exchanges": exchanges,
        "count": len(exchanges),
        "version": ccxt.__version__,
    })


if __name__ == "__main__":
    main()
