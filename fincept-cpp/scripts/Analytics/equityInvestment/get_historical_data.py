"""
Historical Data Fetcher
Fetches historical price data for stocks using yfinance
"""

import sys
import json
import yfinance as yf
from datetime import datetime


def main():
    try:
        if len(sys.argv) < 2:
            raise ValueError("Usage: python get_historical_data.py <symbol> [period] [interval]")

        symbol = sys.argv[1]
        period = sys.argv[2] if len(sys.argv) > 2 else '1y'
        interval = sys.argv[3] if len(sys.argv) > 3 else '1d'

        # Fetch data
        ticker = yf.Ticker(symbol)
        data = ticker.history(period=period, interval=interval)

        if data.empty:
            raise ValueError(f"No data found for symbol {symbol}")

        # Convert to JSON format
        result = []
        for idx, row in data.iterrows():
            result.append({
                'Date': idx.isoformat(),
                'timestamp': int(idx.timestamp()),
                'Open': float(row['Open']),
                'High': float(row['High']),
                'Low': float(row['Low']),
                'Close': float(row['Close']),
                'Volume': int(row['Volume'])
            })

        print(json.dumps(result))

    except Exception as e:
        error_msg = {'error': str(e)}
        print(json.dumps(error_msg))
        sys.exit(1)


if __name__ == '__main__':
    main()
