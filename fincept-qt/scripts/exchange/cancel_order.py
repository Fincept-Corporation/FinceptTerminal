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

output_error("Missing API key in credentials", "AUTH_ERROR")

    if not credentials.get("secret"):
        output_error("Missing API secret in credentials", "AUTH_ERROR")

    exchange = make_exchange(exchange_id, credentials)

    try:
        result = exchange.cancel_order(order_id, symbol)
    except Exception as e:
        output_error(f"Failed to cancel order: {str(e)}", "EXCHANGE_ERROR")

    if not result or result.get("status") not in ("canceled", "cancelled"):
        output_error("Order cancellation not confirmed", "CANCEL_FAILED")

    output_success({
        "id": result.get("id", order_id),
        "symbol": result.get("symbol", symbol),
        "status": result.get("status", "canceled"),
        "exchange": exchange_id
    })
        output_error("API key required. Pass credentials via stdin JSON.", "AUTH_ERROR")

    exchange = make_exchange(exchange_id, credentials)
    result = exchange.cancel_order(order_id, symbol)

    output_success({
        "id": result.get("id", order_id),
        "symbol": result.get("symbol", symbol),
        "status": result.get("status", "canceled"),
        "exchange": exchange_id,
    })


if __name__ == "__main__":
    main()
