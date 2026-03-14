"""
Fetch current funding rate for a perpetual futures symbol.
Usage: python fetch_funding_rate.py <exchange_id> <symbol>
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling

@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_error("Usage: fetch_funding_rate.py <exchange_id> <symbol>", "INVALID_ARGS")

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]

    exchange = make_exchange(exchange_id)
    exchange.options["defaultType"] = "swap"

    try:
        rate = exchange.fetch_funding_rate(symbol)
        output_success({
            "symbol": symbol,
            "funding_rate": rate.get("fundingRate"),
            "funding_timestamp": rate.get("fundingTimestamp"),
            "next_funding_timestamp": rate.get("nextFundingTimestamp"),
            "mark_price": rate.get("markPrice"),
            "index_price": rate.get("indexPrice"),
            "interest_rate": rate.get("interestRate"),
        })
    except Exception as e:
        output_error(f"Funding rate not available: {e}", "NOT_SUPPORTED")

if __name__ == "__main__":
    main()
