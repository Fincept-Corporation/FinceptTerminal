"""
Fetch account balance (requires API credentials via stdin).

Usage: echo '{"api_key":"...","secret":"..."}' | python fetch_balance.py <exchange_id>
Example: echo '{"api_key":"abc","secret":"xyz"}' | python fetch_balance.py binance

Output JSON:
{
  "success": true,
  "data": {
    "exchange": "binance",
    "balances": [
      {"currency": "BTC", "free": 0.5, "used": 0.1, "total": 0.6},
      {"currency": "USDT", "free": 10000.0, "used": 0.0, "total": 10000.0}
    ],
    "timestamp": 1773466621013
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
    if len(sys.argv) < 2:
        output_error("Usage: fetch_balance.py <exchange_id>", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    credentials = parse_credentials_from_stdin()

    if not credentials.get("api_key"):
        output_error("API key required. Pass credentials via stdin JSON.", "AUTH_ERROR")

    exchange = make_exchange(exchange_id, credentials)
    balance = exchange.fetch_balance()

    # Filter to non-zero balances
    balances = []
    for currency, data in balance.items():
        if isinstance(data, dict) and "total" in data:
            total = data.get("total", 0)
            if total and float(total) > 0:
                balances.append({
                    "currency": currency,
                    "free": float(data.get("free", 0) or 0),
                    "used": float(data.get("used", 0) or 0),
                    "total": float(total),
                })

    output_success({
        "exchange": exchange_id,
        "balances": balances,
        "timestamp": balance.get("timestamp"),
    })


if __name__ == "__main__":
    main()
