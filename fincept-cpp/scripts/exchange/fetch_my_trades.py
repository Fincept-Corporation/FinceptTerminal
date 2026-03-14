"""
Fetch user's trade history.
Usage: python fetch_my_trades.py <exchange_id> <symbol> [limit]
Credentials via stdin JSON.
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling, parse_credentials_from_stdin

@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_my_trades.py <exchange_id> <symbol> [limit]", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 50

    creds = parse_credentials_from_stdin()
    exchange = make_exchange(exchange_id, credentials=creds)

    trades = exchange.fetch_my_trades(symbol, limit=limit)
    output_success({
        "trades": [
            {
                "id": t.get("id"),
                "order": t.get("order"),
                "symbol": t.get("symbol"),
                "side": t.get("side"),
                "price": t.get("price"),
                "amount": t.get("amount"),
                "cost": t.get("cost"),
                "fee": t.get("fee", {}).get("cost", 0),
                "fee_currency": t.get("fee", {}).get("currency", ""),
                "timestamp": t.get("timestamp"),
            }
            for t in trades
        ],
        "count": len(trades),
    })

if __name__ == "__main__":
    main()
