"""
Fetch open orders.
Usage: python fetch_open_orders.py <exchange_id> [symbol]
Credentials via stdin JSON.
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling, parse_credentials_from_stdin

@run_with_error_handling
def main():
    if len(sys.argv) < 2:
        output_error("Usage: fetch_open_orders.py <exchange_id> [symbol]", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2] if len(sys.argv) > 2 else None

    creds = parse_credentials_from_stdin()
    exchange = make_exchange(exchange_id, credentials=creds)

    orders = exchange.fetch_open_orders(symbol)
    output_success({
        "orders": [
            {
                "id": o.get("id"),
                "symbol": o.get("symbol"),
                "type": o.get("type"),
                "side": o.get("side"),
                "price": o.get("price"),
                "amount": o.get("amount"),
                "filled": o.get("filled"),
                "remaining": o.get("remaining"),
                "status": o.get("status"),
                "timestamp": o.get("timestamp"),
                "stopPrice": o.get("stopPrice"),
                "takeProfitPrice": o.get("takeProfitPrice"),
                "stopLossPrice": o.get("stopLossPrice"),
            }
            for o in orders
        ],
        "count": len(orders),
    })

if __name__ == "__main__":
    main()
