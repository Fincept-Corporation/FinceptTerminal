"""
BT Data Loading

Fetches data via yfinance and converts to pandas DataFrames suitable for
bt.Backtest(). Falls back to synthetic GBM data when yfinance unavailable.
"""

import sys
import numpy as np
import pandas as pd
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional


# Module-level flag to track synthetic data usage across calls
_last_fetch_used_synthetic = False


def fetch_data(
    symbols: List[str],
    start_date: str,
    end_date: str,
    timeframe: str = '1d',
) -> pd.DataFrame:
    """
    Fetch close-price data for bt.

    bt expects a DataFrame with DatetimeIndex and one column per symbol
    (close prices). Returns that format directly.

    Falls back to synthetic data if yfinance is not available.
    Sets module-level _last_fetch_used_synthetic flag.
    """
    global _last_fetch_used_synthetic
    _last_fetch_used_synthetic = False

    try:
        import yfinance as yf
    except ImportError:
        print('[BT-DATA] WARNING: yfinance not installed, using SYNTHETIC data. '
              'Install yfinance: pip install yfinance', file=sys.stderr)
        _last_fetch_used_synthetic = True
        return _generate_synthetic(symbols, start_date, end_date)

    frames = {}
    any_synthetic = False
    for sym in symbols:
        try:
            print(f'[BT-DATA] Fetching {sym} from {start_date} to {end_date}', file=sys.stderr)
            ticker = yf.Ticker(sym)
            df = ticker.history(start=start_date, end=end_date, interval=timeframe)

            if df.empty:
                print(f'[BT-DATA] WARNING: No data for {sym}, using SYNTHETIC', file=sys.stderr)
                synth = _generate_synthetic([sym], start_date, end_date)
                frames[sym] = synth[sym]
                any_synthetic = True
                continue

            # Normalize columns
            df.columns = [c.lower().replace(' ', '_') for c in df.columns]

            if df.index.tz is None:
                df.index = df.index.tz_localize('UTC')
            else:
                df.index = df.index.tz_convert('UTC')

            # Round to 4 decimal places to eliminate float32 rounding noise
            # from Yahoo Finance API (slightly different values across requests)
            frames[sym] = df['close'].round(4)
            print(f'[BT-DATA] {sym}: {len(df)} bars loaded', file=sys.stderr)

        except Exception as e:
            print(f'[BT-DATA] WARNING: Error fetching {sym}: {e}, using SYNTHETIC', file=sys.stderr)
            synth = _generate_synthetic([sym], start_date, end_date)
            frames[sym] = synth[sym]
            any_synthetic = True

    _last_fetch_used_synthetic = any_synthetic
    result = pd.DataFrame(frames).dropna()
    return result


def was_last_fetch_synthetic() -> bool:
    """Return whether the last fetch_data call used any synthetic data."""
    return _last_fetch_used_synthetic


def fetch_ohlcv(
    symbols: List[str],
    start_date: str,
    end_date: str,
    timeframe: str = '1d',
) -> Dict[str, pd.DataFrame]:
    """
    Fetch full OHLCV data per symbol (for indicators that need H/L/V).
    """
    try:
        import yfinance as yf
    except ImportError:
        return _generate_ohlcv_synthetic(symbols, start_date, end_date)

    result = {}
    for sym in symbols:
        try:
            ticker = yf.Ticker(sym)
            df = ticker.history(start=start_date, end=end_date, interval=timeframe)
            if df.empty:
                synth = _generate_ohlcv_synthetic([sym], start_date, end_date)
                result[sym] = synth[sym]
                continue
            df.columns = [c.lower().replace(' ', '_') for c in df.columns]
            keep = ['open', 'high', 'low', 'close', 'volume']
            available = [c for c in keep if c in df.columns]
            df = df[available].copy()
            if df.index.tz is None:
                df.index = df.index.tz_localize('UTC')
            else:
                df.index = df.index.tz_convert('UTC')
            df.index.name = 'date'
            df = df.dropna()
            # Round to 4 decimal places to eliminate float32 rounding noise
            for col in ['open', 'high', 'low', 'close']:
                if col in df.columns:
                    df[col] = df[col].round(4)
            result[sym] = df
        except Exception as e:
            synth = _generate_ohlcv_synthetic([sym], start_date, end_date)
            result[sym] = synth[sym]
    return result


def _generate_synthetic(
    symbols: List[str],
    start_date: str,
    end_date: str,
) -> pd.DataFrame:
    """Generate synthetic GBM close prices as fallback.

    WARNING: This produces fake data. Results are NOT based on real market data.
    Uses deterministic seed based on symbol char codes (not hash() which varies per-run).
    """
    print('[BT-DATA] WARNING: Generating SYNTHETIC data. Results will NOT reflect '
          'real market conditions. Install yfinance: pip install yfinance', file=sys.stderr)
    dates = pd.bdate_range(start=start_date, end=end_date, tz='UTC')
    frames = {}
    for sym in symbols:
        # Use deterministic seed: sum of char codes (NOT hash() which is randomized per-run)
        seed = sum(ord(c) for c in sym) % (2**31)
        np.random.seed(seed)
        n = len(dates)
        price = 100.0
        prices = []
        for _ in range(n):
            ret = np.random.normal(0.0003, 0.015)
            price *= (1 + ret)
            prices.append(price)
        frames[sym] = pd.Series(prices, index=dates[:n], name=sym)
    return pd.DataFrame(frames)


def _generate_ohlcv_synthetic(
    symbols: List[str],
    start_date: str,
    end_date: str,
) -> Dict[str, pd.DataFrame]:
    """Generate synthetic OHLCV data as fallback.

    WARNING: This produces fake data. Results are NOT based on real market data.
    Uses deterministic seed based on symbol char codes (not hash() which varies per-run).
    """
    print('[BT-DATA] WARNING: Generating SYNTHETIC OHLCV data. Results will NOT reflect '
          'real market conditions. Install yfinance: pip install yfinance', file=sys.stderr)
    dates = pd.bdate_range(start=start_date, end=end_date, tz='UTC')
    result = {}
    for sym in symbols:
        # Use deterministic seed: sum of char codes (NOT hash() which is randomized per-run)
        seed = sum(ord(c) for c in sym) % (2**31)
        np.random.seed(seed)
        n = len(dates)
        price = 100.0
        prices = []
        for _ in range(n):
            ret = np.random.normal(0.0003, 0.015)
            price *= (1 + ret)
            prices.append(price)
        close = np.array(prices)
        high = close * (1 + np.abs(np.random.normal(0, 0.005, n)))
        low = close * (1 - np.abs(np.random.normal(0, 0.005, n)))
        opn = close * (1 + np.random.normal(0, 0.003, n))
        volume = np.random.randint(100000, 5000000, n).astype(float)
        df = pd.DataFrame({
            'open': opn, 'high': high, 'low': low,
            'close': close, 'volume': volume,
        }, index=dates[:n])
        df.index.name = 'date'
        result[sym] = df
    return result


def data_to_records(data: pd.DataFrame) -> List[Dict[str, Any]]:
    """Convert close-price DataFrame to list-of-dicts for JSON output."""
    records = []
    for idx, row in data.iterrows():
        date_str = idx.strftime('%Y-%m-%d') if hasattr(idx, 'strftime') else str(idx)
        for col in data.columns:
            records.append({
                'symbol': col,
                'date': date_str,
                'close': float(row[col]),
            })
    return records
