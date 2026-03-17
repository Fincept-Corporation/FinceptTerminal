"""
Fetch open interest for a perpetual futures symbol.
Works with both spot symbols (BTC/USDT) and perp symbols (BTC/USDT:USDT).

Usage: python fetch_open_interest.py <exchange_id> <symbol>
"""
import sys
from exchange_client import make_exchange, output_success, run_with_error_handling


def to_perp_symbol(symbol: str) -> str:
    """Convert spot symbol to linear perp equivalent if needed."""
    if ":" not in symbol and "/" in symbol:
        quote = symbol.split("/")[1]
        return f"{symbol}:{quote}"
    return symbol


@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_success({"symbol": "", "open_interest": None, "open_interest_value": None, "timestamp": None})
        return

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]

    exchange = make_exchange(exchange_id)
    exchange.load_markets()

    perp_symbol = to_perp_symbol(symbol)

    for sym in ([perp_symbol, symbol] if perp_symbol != symbol else [symbol]):
        try:
            exchange.options["defaultType"] = "swap"
            oi = exchange.fetch_open_interest(sym)
            output_success({
                "symbol": sym,
                "open_interest": oi.get("openInterestAmount"),
                "open_interest_value": oi.get("openInterestValue"),
                "timestamp": oi.get("timestamp"),
            })
            return
        except Exception:
            continue

    # Not available for this symbol/exchange
    output_success({
        "symbol": symbol,
        "open_interest": None,
        "open_interest_value": None,
        "timestamp": None,
    })


if __name__ == "__main__":
    main()
