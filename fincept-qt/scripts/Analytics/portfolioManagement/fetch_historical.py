"""
Fetch historical price data for symbols using yfinance.
Input: JSON via stdin: {"symbols": ["AAPL", "MSFT"], "period": "6mo", "interval": "1d"}
Output: JSON to stdout: {"AAPL": [{"date": "2024-01-01", "close": 150.0}, ...], ...}
"""
import sys
import json


def main():
    data = json.loads(sys.stdin.read())
    symbols = data.get("symbols", [])
    period = data.get("period", "6mo")
    interval = data.get("interval", "1d")

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
            hist = yf.download(sym, period=period, interval=interval, progress=False)
            if hist is not None and not hist.empty:
                records = []
                for date, row in hist.iterrows():
                    close_val = row["Close"]
                    if hasattr(close_val, "item"):
                        close_val = close_val.item()
                    records.append({
                        "date": date.strftime("%Y-%m-%d"),
                        "close": float(close_val)
                    })
                result[sym] = records
        except Exception:
            pass

    print(json.dumps(result))


if __name__ == "__main__":
    main()
