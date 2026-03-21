"""
Fetch trading fee rates.
Usage: python fetch_trading_fees.py <exchange_id> [symbol]
"""
import sys
from exchange_client import make_exchange, output_success, output_error, run_with_error_handling

@run_with_error_handling
def main():
    if len(sys.argv) < 2:
        output_error("Usage: fetch_trading_fees.py <exchange_id> [symbol]", "INVALID_ARGS")

    exchange_id = sys.argv[1]

    exchange = make_exchange(exchange_id)

    if len(sys.argv) > 2:
        symbol = sys.argv[2]
        fee = exchange.fetch_trading_fee(symbol)
        output_success({
            "symbol": symbol,
            "maker": fee.get("maker"),
            "taker": fee.get("taker"),
            "percentage": fee.get("percentage", True),
        })
    else:
        fees = exchange.fetch_trading_fees()
        result = []
        for symbol, fee in list(fees.items())[:50]:
            if isinstance(fee, dict) and "maker" in fee:
                result.append({
                    "symbol": symbol,
                    "maker": fee.get("maker"),
                    "taker": fee.get("taker"),
                })
        output_success({"fees": result, "count": len(result)})

if __name__ == "__main__":
    main()
