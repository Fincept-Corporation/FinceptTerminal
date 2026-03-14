"""
Technical Analysis Main CLI
Command-line interface for calculating technical indicators
Supports multiple data sources: yfinance, CSV files, and JSON data
"""

import sys
import os
import json
import pandas as pd
import yfinance as yf
from datetime import datetime

# Add current directory to Python path for module imports
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Import indicator modules
try:
    from momentum_indicators import calculate_all_momentum_indicators
    from volume_indicators import calculate_all_volume_indicators
    from volatility_indicators import calculate_all_volatility_indicators
    from trend_indicators import calculate_all_trend_indicators
    from others_indicators import calculate_all_others_indicators
except ImportError:
    # Try with absolute imports if running as a package
    from technicals.momentum_indicators import calculate_all_momentum_indicators
    from technicals.volume_indicators import calculate_all_volume_indicators
    from technicals.volatility_indicators import calculate_all_volatility_indicators
    from technicals.trend_indicators import calculate_all_trend_indicators
    from technicals.others_indicators import calculate_all_others_indicators


def load_data_from_yfinance(symbol, period="1y", start_date=None, end_date=None):
    """
    Load data from Yahoo Finance

    Args:
        symbol: Stock symbol (e.g., 'AAPL')
        period: Period for historical data (e.g., '1d', '5d', '1mo', '1y', 'max')
        start_date: Start date in YYYY-MM-DD format (optional)
        end_date: End date in YYYY-MM-DD format (optional)

    Returns:
        DataFrame with OHLCV data
    """
    try:
        ticker = yf.Ticker(symbol)
        if start_date and end_date:
            df = ticker.history(start=start_date, end=end_date)
        else:
            df = ticker.history(period=period)

        if df.empty:
            raise ValueError(f"No data found for symbol: {symbol}")

        # Normalize column names to lowercase
        df.columns = df.columns.str.lower()
        df = df.reset_index()

        # Rename 'date' column to 'timestamp' if it exists
        if 'date' in df.columns:
            df['timestamp'] = pd.to_datetime(df['date']).astype(int) // 10**9
            df = df.drop('date', axis=1)

        return df
    except Exception as e:
        raise Exception(f"Error loading data from yfinance: {str(e)}")


def load_data_from_csv(filepath):
    """
    Load data from CSV file

    Args:
        filepath: Path to CSV file

    Returns:
        DataFrame with OHLCV data

    Expected CSV format:
        - Columns: date/timestamp, open, high, low, close, volume
        - Date can be in various formats (will be auto-parsed)
    """
    try:
        df = pd.read_csv(filepath)

        # Normalize column names to lowercase
        df.columns = df.columns.str.lower()

        # Try to find date/timestamp column
        date_cols = ['date', 'datetime', 'timestamp', 'time']
        date_col = None
        for col in date_cols:
            if col in df.columns:
                date_col = col
                break

        if date_col:
            df['timestamp'] = pd.to_datetime(df[date_col]).astype(int) // 10**9
            if date_col != 'timestamp':
                df = df.drop(date_col, axis=1)

        # Validate required columns
        required_cols = ['open', 'high', 'low', 'close']
        missing_cols = [col for col in required_cols if col not in df.columns]
        if missing_cols:
            raise ValueError(f"Missing required columns: {missing_cols}")

        return df
    except Exception as e:
        raise Exception(f"Error loading data from CSV: {str(e)}")


def load_data_from_json(json_str):
    """
    Load data from JSON string

    Args:
        json_str: JSON string with OHLCV data

    Returns:
        DataFrame with OHLCV data

    Expected JSON format:
        [
            {"timestamp": 1234567890, "open": 100, "high": 105, "low": 99, "close": 102, "volume": 10000},
            ...
        ]
        OR
        {
            "timestamp": [1234567890, ...],
            "open": [100, ...],
            "high": [105, ...],
            "low": [99, ...],
            "close": [102, ...],
            "volume": [10000, ...]
        }
    """
    try:
        data = json.loads(json_str)
        df = pd.DataFrame(data)

        # Normalize column names to lowercase
        df.columns = df.columns.str.lower()

        # Validate required columns
        required_cols = ['open', 'high', 'low', 'close']
        missing_cols = [col for col in required_cols if col not in df.columns]
        if missing_cols:
            raise ValueError(f"Missing required columns: {missing_cols}")

        return df
    except Exception as e:
        raise Exception(f"Error loading data from JSON: {str(e)}")


def calculate_indicators(df, categories=None, specific_indicators=None, params=None):
    """
    Calculate technical indicators

    Args:
        df: DataFrame with OHLCV data
        categories: List of categories to calculate ['momentum', 'volume', 'volatility', 'trend', 'others']
                   If None, calculates all categories
        specific_indicators: List of specific indicator names to calculate
                            If provided, overrides categories parameter
        params: Dict of parameters for indicators (optional)

    Returns:
        DataFrame with calculated indicators
    """
    result_df = df.copy()
    params = params or {}

    if specific_indicators:
        # Calculate only specific indicators
        # This would require mapping indicator names to functions
        # For now, we'll use categories
        pass

    # Determine which categories to calculate
    if categories is None:
        categories = ['momentum', 'volume', 'volatility', 'trend', 'others']

    # Calculate indicators by category
    if 'momentum' in categories:
        result_df = calculate_all_momentum_indicators(result_df, **params.get('momentum', {}))

    if 'volume' in categories and 'volume' in result_df.columns:
        result_df = calculate_all_volume_indicators(result_df, **params.get('volume', {}))

    if 'volatility' in categories:
        result_df = calculate_all_volatility_indicators(result_df, **params.get('volatility', {}))

    if 'trend' in categories:
        result_df = calculate_all_trend_indicators(result_df, **params.get('trend', {}))

    if 'others' in categories:
        result_df = calculate_all_others_indicators(result_df, **params.get('others', {}))

    return result_df


def format_output(df, output_format='json'):
    """
    Format output data

    Args:
        df: DataFrame with data
        output_format: Output format ('json', 'csv')

    Returns:
        Formatted string
    """
    if output_format == 'csv':
        return df.to_csv(index=False)
    else:  # json
        # Replace NaN with None for proper JSON encoding
        df = df.replace({pd.np.nan: None}) if hasattr(pd, 'np') else df.where(pd.notnull(df), None)
        return df.to_json(orient='records', date_format='iso')


def main():
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python technical_analysis.py <command> <args>",
            "commands": {
                "yfinance": "python technical_analysis.py yfinance <symbol> [period] [categories]",
                "csv": "python technical_analysis.py csv <filepath> [categories]",
                "json": "python technical_analysis.py json <json_data> [categories]",
                "help": "python technical_analysis.py help"
            }
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "help":
            help_text = {
                "description": "Technical Analysis CLI - Calculate technical indicators from various data sources",
                "commands": {
                    "yfinance": {
                        "description": "Load data from Yahoo Finance and calculate indicators",
                        "usage": "python technical_analysis.py yfinance <symbol> [period] [categories]",
                        "arguments": {
                            "symbol": "Stock symbol (e.g., AAPL)",
                            "period": "Period for historical data (e.g., 1d, 5d, 1mo, 1y, max) - default: 1y",
                            "categories": "Comma-separated list of categories (momentum,volume,volatility,trend,others) - default: all"
                        },
                        "examples": [
                            "python technical_analysis.py yfinance AAPL",
                            "python technical_analysis.py yfinance AAPL 6mo",
                            "python technical_analysis.py yfinance AAPL 1y momentum,trend"
                        ]
                    },
                    "csv": {
                        "description": "Load data from CSV file and calculate indicators",
                        "usage": "python technical_analysis.py csv <filepath> [categories]",
                        "arguments": {
                            "filepath": "Path to CSV file with OHLCV data",
                            "categories": "Comma-separated list of categories - default: all"
                        },
                        "expected_format": "CSV with columns: date/timestamp, open, high, low, close, volume",
                        "examples": [
                            "python technical_analysis.py csv data.csv",
                            "python technical_analysis.py csv data.csv momentum,volatility"
                        ]
                    },
                    "json": {
                        "description": "Load data from JSON and calculate indicators",
                        "usage": "python technical_analysis.py json '<json_data>' [categories]",
                        "arguments": {
                            "json_data": "JSON string with OHLCV data",
                            "categories": "Comma-separated list of categories - default: all"
                        },
                        "expected_format": "JSON array with objects containing: timestamp, open, high, low, close, volume",
                        "examples": [
                            'python technical_analysis.py json \'[{"timestamp": 1234567890, "open": 100, "high": 105, "low": 99, "close": 102, "volume": 10000}]\''
                        ]
                    }
                },
                "categories": {
                    "momentum": "RSI, Stochastic, Williams %R, KAMA, ROC, TSI, Ultimate Oscillator, PPO, PVO",
                    "volume": "ADI, OBV, CMF, Force Index, EoM, VPT, NVI, VWAP, MFI",
                    "volatility": "ATR, Bollinger Bands, Keltner Channel, Donchian Channel, Ulcer Index",
                    "trend": "SMA, EMA, WMA, MACD, TRIX, Mass Index, Ichimoku, KST, DPO, CCI, ADX, Vortex, PSAR, STC, Aroon",
                    "others": "Daily Return, Daily Log Return, Cumulative Return"
                }
            }
            print(json.dumps(help_text, indent=2))

        elif command == "yfinance":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python technical_analysis.py yfinance <symbol> [period] [categories]"}))
                sys.exit(1)

            symbol = sys.argv[2]
            period = sys.argv[3] if len(sys.argv) > 3 and not ',' in sys.argv[3] else "1y"
            categories = sys.argv[4].split(',') if len(sys.argv) > 4 else sys.argv[3].split(',') if len(sys.argv) > 3 and ',' in sys.argv[3] else None

            df = load_data_from_yfinance(symbol, period=period)
            result_df = calculate_indicators(df, categories=categories)
            output = format_output(result_df, output_format='json')
            print(output)

        elif command == "csv":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python technical_analysis.py csv <filepath> [categories]"}))
                sys.exit(1)

            filepath = sys.argv[2]
            categories = sys.argv[3].split(',') if len(sys.argv) > 3 else None

            df = load_data_from_csv(filepath)
            result_df = calculate_indicators(df, categories=categories)
            output = format_output(result_df, output_format='json')
            print(output)

        elif command == "json":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python technical_analysis.py json '<json_data>' [categories]"}))
                sys.exit(1)

            json_data = sys.argv[2]
            categories = sys.argv[3].split(',') if len(sys.argv) > 3 else None

            df = load_data_from_json(json_data)
            result_df = calculate_indicators(df, categories=categories)
            output = format_output(result_df, output_format='json')
            print(output)

        else:
            print(json.dumps({"error": f"Unknown command: {command}. Use 'help' for available commands."}))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
