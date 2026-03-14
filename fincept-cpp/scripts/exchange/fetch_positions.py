"""
Fetch open positions (futures/margin).
Usage: python fetch_positions.py <exchange_id> [symbol]
Credentials via stdin JSON.
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling, parse_credentials_from_stdin

@run_with_error_handling
def main():
    if len(sys.argv) < 2:
        output_error("Usage: fetch_positions.py <exchange_id> [symbol]", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbols = [sys.argv[2]] if len(sys.argv) > 2 else None

    creds = parse_credentials_from_stdin()
    exchange = make_exchange(exchange_id, credentials=creds)
    exchange.options["defaultType"] = "swap"

    positions = exchange.fetch_positions(symbols)
    output_success({
        "positions": [
            {
                "symbol": p.get("symbol"),
                "side": p.get("side"),
                "contracts": p.get("contracts"),
                "contractSize": p.get("contractSize"),
                "notional": p.get("notional"),
                "leverage": p.get("leverage"),
                "unrealizedPnl": p.get("unrealizedPnl"),
                "realizedPnl": p.get("realizedPnl", 0),
                "entryPrice": p.get("entryPrice"),
                "markPrice": p.get("markPrice"),
                "liquidationPrice": p.get("liquidationPrice"),
                "marginMode": p.get("marginMode"),
                "marginRatio": p.get("marginRatio"),
                "collateral": p.get("collateral"),
                "timestamp": p.get("timestamp"),
            }
            for p in positions if p.get("contracts") and float(p["contracts"]) != 0
        ],
        "count": len(positions),
    })

if __name__ == "__main__":
    main()
