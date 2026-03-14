"""
Fetch open interest for a perpetual futures symbol.
Usage: python fetch_open_interest.py <exchange_id> <symbol>
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling

@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_open_interest.py <exchange_id> <symbol>", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]

    exchange = make_exchange(exchange_id)
    exchange.options["defaultType"] = "swap"

    try:
        oi = exchange.fetch_open_interest(symbol)
        output_success({
            "symbol": symbol,
            "open_interest": oi.get("openInterestAmount"),
            "open_interest_value": oi.get("openInterestValue"),
            "timestamp": oi.get("timestamp"),
        })
    except Exception as e:
        output_error(f"Open interest not available: {e}", "NOT_SUPPORTED")

if __name__ == "__main__":
    main()
