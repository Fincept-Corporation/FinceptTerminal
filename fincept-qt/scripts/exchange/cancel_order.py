"""
Cancel an open order on an exchange.

Usage: echo '{"api_key":"...","secret":"..."}' | python cancel_order.py <exchange_id> <order_id> <symbol>
Example: echo '{"api_key":"...","secret":"..."}' | python cancel_order.py binance 12345678 BTC/USDT

Output JSON:
{
  "success": true,
  "data": {
    "id": "12345678",
    "symbol": "BTC/USDT",
    "status": "canceled",
    "exchange": "binance"
  }
}
"""

import sys
from exchange_client import (
    make_exchange, output_success, output_error,
    parse_credentials_from_stdin, run_with_error_handling,
)


@run_with_error_handling
def main():
    if len(sys.argv) < 4:
        output_error("Usage: cancel_order.py <exchange_id> <order_id> <symbol>", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    order_id = sys.argv[2]
    symbol = sys.argv[3]

    credentials = parse_credentials_from_stdin()
    if not credentials.get("api_key"):
        output_error("API key required. Pass credentials via stdin JSON.", "AUTH_ERROR")
    if not credentials.get("secret"):
        output_error("API secret required. Pass credentials via stdin JSON.", "AUTH_ERROR")

    exchange = make_exchange(exchange_id, credentials)
    result = exchange.cancel_order(order_id, symbol)

    status = result.get("status", "")
    if status not in ("canceled", "cancelled"):
        output_error(f"Order cancellation not confirmed — exchange returned status: '{status}'", "CANCEL_FAILED")

    output_success({
        "id": result.get("id", order_id),
        "symbol": result.get("symbol", symbol),
        "status": status or "canceled",
        "exchange": exchange_id,
    })


if __name__ == "__main__":
    main()
