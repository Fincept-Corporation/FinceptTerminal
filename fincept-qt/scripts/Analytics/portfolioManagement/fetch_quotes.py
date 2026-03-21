"""
Fetch current quotes for a list of symbols using yfinance.
Input: JSON via stdin: {"symbols": ["AAPL", "MSFT", ...]}
Output: JSON to stdout: {"AAPL": {"price": 150.0, "prev_close": 148.0}, ...}
"""
import sys
import json


def main():
    data = json.loads(sys.stdin.read())
    symbols = data.get("symbols", [])
    if not symbols:
        print(json.dumps({}))
        return

    try:
        import yfinance as yf
    except ImportError:
        print(json.dumps({"error": "yfinance not installed"}))
        return

    result = {}
    for sym in symbols:
        try:
            t = yf.Ticker(sym)
            info = t.fast_info
            price = float(getattr(info, "last_price", 0) or 0)
            prev = float(getattr(info, "previous_close", 0) or 0)
            if price > 0:
                result[sym] = {"price": price, "prev_close": prev if prev > 0 else price}
        except Exception:
            pass

    print(json.dumps(result))


if __name__ == "__main__":
    main()
