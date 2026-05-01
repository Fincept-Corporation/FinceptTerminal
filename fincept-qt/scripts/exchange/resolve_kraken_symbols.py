"""
Resolve a list of Fincept symbols (e.g. ['BTC/USDT','MATIC/USDT']) to actual
Kraken-listed symbols using ccxt.kraken.load_markets(). One-shot helper for
the native KrakenWsClient — runs once at WS startup.

Usage: python resolve_kraken_symbols.py SYM1 SYM2 SYM3 ...
Output: one JSON object on stdout with shape:
  {"resolved": {"BTC/USDT": "BTC/USDT", "MATIC/USDT": "POL/USD", ...}}
Symbols with no match map to null.

Resolution strategy (in order):
  1. Exact match in k.symbols
  2. Same base, /USD quote
  3. Known rebrand (MATIC -> POL)
  4. Same base, any /USD-stable quote (USDC, USDT)
  5. Same base, any quote (lowest-volume tiebreaker by alphabetical)
"""
import json
import sys

# Known rebrands. Kraken occasionally renames a base asset (e.g. Polygon's
# MATIC → POL on Sep 2024). When we ask for the old name and it's gone, fall
# back to the new one BEFORE the generic /USD search.
REBRANDS = {
    "MATIC": "POL",
    # add more here as Kraken keeps renaming things
}


def resolve_one(symbol: str, market_symbols: set, markets: dict) -> str | None:
    if symbol in market_symbols:
        return symbol

    base, _, quote = symbol.partition("/")
    base = base.upper()
    if not base:
        return None

    # Strip any settle suffix the caller might have included (BTC/USDT:USDT).
    quote = quote.split(":")[0].upper() if quote else "USDT"

    candidates = [
        f"{base}/USD",
        f"{base}/USDT",
        f"{base}/USDC",
    ]

    rebranded = REBRANDS.get(base)
    if rebranded:
        candidates = [f"{rebranded}/USD", f"{rebranded}/USDT", f"{rebranded}/USDC"] + candidates

    for c in candidates:
        if c in market_symbols:
            return c

    # Final fallback: any /USD-stable, prefer the rebranded base if any.
    bases = [base]
    if rebranded:
        bases.insert(0, rebranded)
    for b in bases:
        for s in sorted(market_symbols):
            if s.startswith(b + "/"):
                return s
    return None


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({"error": "usage: resolve_kraken_symbols.py SYM ..."}))
        sys.exit(1)

    requested = sys.argv[1:]

    try:
        import ccxt
    except ImportError:
        print(json.dumps({"error": "ccxt not installed"}))
        sys.exit(2)

    try:
        kraken = ccxt.kraken({"enableRateLimit": True})
        kraken.load_markets()
    except Exception as e:
        print(json.dumps({"error": f"load_markets failed: {e}"}))
        sys.exit(3)

    market_symbols = set(kraken.symbols or [])
    markets = kraken.markets or {}

    resolved = {sym: resolve_one(sym, market_symbols, markets) for sym in requested}
    print(json.dumps({"resolved": resolved}))


if __name__ == "__main__":
    main()
