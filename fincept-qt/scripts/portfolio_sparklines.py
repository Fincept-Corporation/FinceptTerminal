"""
Portfolio Sparklines — batch fetch 5-day hourly close prices for multiple symbols.
Input (argv[1]): JSON string {"symbols": ["AAPL", "MSFT", ...]}
Output (stdout): JSON {"AAPL": [170.1, 171.3, ...], "MSFT": [375.0, ...], ...}
Each list is chronological close prices (up to ~35 data points, 5d x 7h).
"""
import sys
import json
import yfinance as yf


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "No input"}))
        return

    try:
        params = json.loads(sys.argv[1])
    except Exception as e:
        print(json.dumps({"error": f"JSON parse error: {e}"}))
        return

    symbols = params.get("symbols", [])
    if not symbols:
        print(json.dumps({"error": "No symbols"}))
        return

    try:
        # Single batch download — 5 days, 1h interval
        data = yf.download(
            symbols,
            period="5d",
            interval="1h",
            progress=False,
            auto_adjust=True,
        )

        if data is None or data.empty:
            print(json.dumps({"error": "No data returned"}))
            return

        close = data["Close"] if "Close" in data else data

        # Normalise to DataFrame even for single symbol
        import pandas as pd
        if isinstance(close, pd.Series):
            close = pd.DataFrame({symbols[0]: close})

        result = {}
        for sym in symbols:
            if sym not in close.columns:
                continue
            series = close[sym].dropna()
            if series.empty:
                continue
            result[sym] = [round(float(v), 4) for v in series.tolist()]

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"error": str(e)}))


if __name__ == "__main__":
    main()
