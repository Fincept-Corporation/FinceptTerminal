"""
Set leverage for a symbol on futures exchanges.
Usage: python set_leverage.py <exchange_id> <symbol> <leverage>
Credentials via stdin JSON.
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling, parse_credentials_from_stdin

@run_with_error_handling
def main():
    if len(sys.argv) < 4:
        output_error("Usage: set_leverage.py <exchange_id> <symbol> <leverage>", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]
    leverage = int(sys.argv[3])

    creds = parse_credentials_from_stdin()
    exchange = make_exchange(exchange_id, credentials=creds)
    exchange.options["defaultType"] = "swap"

    result = exchange.set_leverage(leverage, symbol)
    output_success({"symbol": symbol, "leverage": leverage, "result": result})

if __name__ == "__main__":
    main()
