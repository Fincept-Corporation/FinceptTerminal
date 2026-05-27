"""
Minimal candle data fetcher — data source only, no algo logic.
Returns OHLCV JSON for multiple symbols via yfinance.

Usage: python fetch_candles.py '{"symbols":["RELIANCE","TCS"],"timeframe":"1d","lookback_days":365}'
"""
import json
import sys

try:
    import yfinance as yf
except ImportError:
    print(json.dumps({"success": False, "error": "yfinance not installed"}))
    sys.exit(1)


TIMEFRAME_MAP = {
    "1m": "1m", "3m": "5m", "5m": "5m", "15m": "15m",
    "30m": "30m", "1h": "1h", "4h": "1h", "1d": "1d",
}

PERIOD_MAP = {
    "1m": "7d", "3m": "60d", "5m": "60d", "15m": "60d",
    "30m": "60d", "1h": "730d", "4h": "730d", "1d": "max",
}


def symbol_to_yf(symbol: str) -> str:
    s = symbol.strip().upper()
    if "." in s or "/" in s or "-" in s:
        return s
    return s + ".NS"


def fetch_symbol(symbol: str, timeframe: str, lookback_days: int):
    yf_symbol = symbol_to_yf(symbol)
    interval = TIMEFRAME_MAP.get(timeframe, "1d")
    period = PERIOD_MAP.get(timeframe, "max")

    if lookback_days and lookback_days < 365:
        period = f"{lookback_days}d"

    try:
        df = yf.download(yf_symbol, period=period, interval=interval, progress=False)
        if df.empty:
            return []

        if hasattr(df.columns, 'droplevel') and df.columns.nlevels > 1:
            df.columns = df.columns.droplevel(1)

        candles = []
        for idx, row in df.iterrows():
            candles.append({
                "t": int(idx.timestamp() * 1000),
                "o": round(float(row["Open"]), 4),
                "h": round(float(row["High"]), 4),
                "l": round(float(row["Low"]), 4),
                "c": round(float(row["Close"]), 4),
                "v": round(float(row.get("Volume", 0)), 0),
            })
        return candles
    except Exception as e:
        return []


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No params provided"}))
        sys.exit(1)

    try:
        params = json.loads(sys.argv[1])
    except json.JSONDecodeError as e:
        print(json.dumps({"success": False, "error": f"Invalid JSON: {e}"}))
        sys.exit(1)

    symbols = params.get("symbols", [])
    timeframe = params.get("timeframe", "1d")
    lookback_days = params.get("lookback_days", 365)

    data = {}
    for symbol in symbols:
        candles = fetch_symbol(symbol, timeframe, lookback_days)
        if candles:
            data[symbol] = candles

    print(json.dumps({"success": True, "data": data}))


if __name__ == "__main__":
    main()
