"""
Fetch current funding rate for a perpetual futures symbol.
Works with both spot symbols (BTC/USDT) and perp symbols (BTC/USDT:USDT).
For spot symbols on exchanges that use unified format, tries the perp equivalent.

Usage: python fetch_funding_rate.py <exchange_id> <symbol>
"""
import sys
from exchange_client import make_exchange, output_success, run_with_error_handling


def to_perp_symbol(symbol: str) -> str:
    """Convert spot symbol to linear perp equivalent if needed.
    BTC/USDT -> BTC/USDT:USDT (Binance unified format)
    """
    if ":" not in symbol and "/" in symbol:
        quote = symbol.split("/")[1]
        return f"{symbol}:{quote}"
    return symbol


@run_with_error_handling
def main():
    if len(sys.argv) < 3:
        output_success({
            "symbol": "",
            "funding_rate": None,
            "funding_timestamp": None,
            "next_funding_timestamp": None,
            "mark_price": None,
            "index_price": None,
        })
        return

    exchange_id = sys.argv[1]
    symbol = sys.argv[2]

    exchange = make_exchange(exchange_id)
    exchange.load_markets()

    # Try perp symbol first, fall back to original
    perp_symbol = to_perp_symbol(symbol)
    tried = []

    for sym in ([perp_symbol, symbol] if perp_symbol != symbol else [symbol]):
        tried.append(sym)
        try:
            # Use swap market type for perp symbols
            exchange.options["defaultType"] = "swap"
            rate = exchange.fetch_funding_rate(sym)
            output_success({
                "symbol": sym,
                "funding_rate": rate.get("fundingRate"),
                "funding_timestamp": rate.get("fundingTimestamp"),
                "next_funding_timestamp": rate.get("nextFundingTimestamp"),
                "mark_price": rate.get("markPrice"),
                "index_price": rate.get("indexPrice"),
                "interest_rate": rate.get("interestRate"),
            })
            return
        except Exception:
            continue

    # Not available for this symbol/exchange (common for spot-only exchanges)
    output_success({
        "symbol": symbol,
        "funding_rate": None,
        "funding_timestamp": None,
        "next_funding_timestamp": None,
        "mark_price": None,
        "index_price": None,
    })


if __name__ == "__main__":
    main()
