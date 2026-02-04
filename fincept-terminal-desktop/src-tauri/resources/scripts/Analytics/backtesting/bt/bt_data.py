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
    """
    try:
        import yfinance as yf
    except ImportError:
        print('[BT-DATA] yfinance not installed, using synthetic data', file=sys.stderr)
        return _generate_synthetic(symbols, start_date, end_date)

    frames = {}
    for sym in symbols:
        try:
            print(f'[BT-DATA] Fetching {sym} from {start_date} to {end_date}', file=sys.stderr)
            ticker = yf.Ticker(sym)
            df = ticker.history(start=start_date, end=end_date, interval=timeframe)

            if df.empty:
                print(f'[BT-DATA] No data for {sym}, using synthetic', file=sys.stderr)
                synth = _generate_synthetic([sym], start_date, end_date)
                frames[sym] = synth[sym]
                continue

            # Normalize columns
            df.columns = [c.lower().replace(' ', '_') for c in df.columns]

            if df.index.tz is None:
                df.index = df.index.tz_localize('UTC')
            else:
                df.index = df.index.tz_convert('UTC')

            frames[sym] = df['close']
            print(f'[BT-DATA] {sym}: {len(df)} bars loaded', file=sys.stderr)

        except Exception as e:
            print(f'[BT-DATA] Error fetching {sym}: {e}, using synthetic', file=sys.stderr)
            synth = _generate_synthetic([sym], start_date, end_date)
            frames[sym] = synth[sym]

    result = pd.DataFrame(frames).dropna()
    return result


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
    """Generate synthetic GBM close prices as fallback."""
    dates = pd.bdate_range(start=start_date, end=end_date, tz='UTC')
    frames = {}
    for sym in symbols:
        np.random.seed(hash(sym) % 2**31)
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
    """Generate synthetic OHLCV data as fallback."""
    dates = pd.bdate_range(start=start_date, end=end_date, tz='UTC')
    result = {}
    for sym in symbols:
        np.random.seed(hash(sym) % 2**31)
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
