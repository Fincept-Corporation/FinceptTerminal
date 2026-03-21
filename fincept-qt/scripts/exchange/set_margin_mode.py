"""
Set margin mode (cross/isolated) for a symbol.
Usage: python set_margin_mode.py <exchange_id> <symbol> <mode>
Credentials via stdin JSON.
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling, parse_credentials_from_stdin

@run_with_error_handling
def main():
    if len(sys.argv) < 4:
        output_error("Usage: set_margin_mode.py <exchange_id> <symbol> <mode>", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]
    mode = sys.argv[3]  # "cross" or "isolated"

    creds = parse_credentials_from_stdin()
    exchange = make_exchange(exchange_id, credentials=creds)
    exchange.options["defaultType"] = "swap"

    result = exchange.set_margin_mode(mode, symbol)
    output_success({"symbol": symbol, "margin_mode": mode, "result": result})

if __name__ == "__main__":
    main()
