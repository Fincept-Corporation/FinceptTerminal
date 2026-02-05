"""
Fast-Trade Data Module

Data loading, preparation, and archive access:

DataFrame Operations:
- load_basic_df_from_csv(): Load OHLCV CSV into DataFrame
- standardize_df(): Normalize column names and index
- build_data_frame(): Load CSV + apply transformers from backtest config
- prepare_df(): Apply freq, start/stop time filters
- apply_charting_to_df(): Resample + time window filter
- apply_transformers_to_dataframe(): Apply indicator datapoints to df
- infer_frequency(): Detect data frequency from index
- detect_time_unit(): Parse time unit from string
- calculate_perc_missing(): Calculate % of missing data
- process_res_df(): Internal DataFrame result processor for transformer outputs

Error Classes:
- TransformerError: Raised when a transformer/indicator fails to apply

Yahoo Finance:
- load_yfinance_data(): Load data from Yahoo Finance (yfinance)

Archive (Data Downloading):
- Binance API: get_binance_klines, get_available_symbols, get_exchange_info,
               binance_kline_to_df, get_oldest_date_available
- Coinbase API: get_product_candles, get_asset_ids, get_products,
                df_from_candles, get_single_candle, get_oldest_day
- DB Helpers: get_kline, update_klines_to_db, get_local_assets, connect_to_db,
              standardize_df (db variant)
- Archive CLI: download_asset, get_assets
- Update: update_archive, update_single_archive, update_kline

Synthetic Data:
- generate_synthetic_ohlcv(): Generate random OHLCV data for testing
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, Optional, List, Union, Tuple
from datetime import datetime, timedelta
from pathlib import Path


# ============================================================================
# Error Classes
# ============================================================================

class TransformerError(Exception):
    """
    Raised when a transformer/indicator fails to apply to the DataFrame.

    Wraps fast_trade.build_data_frame.TransformerError.
    Typically occurs when:
    - Transformer name is not found in the transformer map
    - Transformer arguments are invalid
    - Required columns are missing from the DataFrame
    """
    pass


# ============================================================================
# DataFrame Loading & Standardization
# ============================================================================

def load_basic_df_from_csv(csv_path: str) -> pd.DataFrame:
    """
    Load OHLCV data from CSV into a standardized DataFrame.

    Wraps fast_trade.build_data_frame.load_basic_df_from_csv().
    Expects columns: date, open, high, low, close, volume.

    Args:
        csv_path: Path to CSV file

    Returns:
        DataFrame with DatetimeIndex and OHLCV columns
    """
    try:
        from fast_trade.build_data_frame import load_basic_df_from_csv as ft_load
        return ft_load(csv_path)
    except ImportError:
        df = pd.read_csv(csv_path)
        return standardize_df(df)


def standardize_df(df: pd.DataFrame) -> pd.DataFrame:
    """
    Standardize DataFrame column names and index.

    - Lowercase column names
    - Set 'date' column as DatetimeIndex
    - Ensure OHLCV columns exist

    Args:
        df: Raw DataFrame

    Returns:
        Standardized DataFrame
    """
    try:
        from fast_trade.build_data_frame import standardize_df as ft_std
        return ft_std(df)
    except ImportError:
        df = df.copy()
        df.columns = [c.lower().strip() for c in df.columns]

        if 'date' in df.columns:
            df['date'] = pd.to_datetime(df['date'])
            df = df.set_index('date')
        elif 'datetime' in df.columns:
            df['datetime'] = pd.to_datetime(df['datetime'])
            df = df.set_index('datetime')

        if not isinstance(df.index, pd.DatetimeIndex):
            df.index = pd.to_datetime(df.index)

        return df


def infer_frequency(df: pd.DataFrame) -> str:
    """
    Detect data frequency from DataFrame index.

    Analyzes time gaps between rows to determine frequency.

    Args:
        df: DataFrame with DatetimeIndex

    Returns:
        Frequency string ('1Min', '5Min', '1H', '1D', etc.)
    """
    try:
        from fast_trade.build_data_frame import infer_frequency as ft_infer
        return ft_infer(df)
    except ImportError:
        if len(df) < 2:
            return '1D'
        diffs = pd.Series(df.index).diff().dropna()
        median_diff = diffs.median()
        minutes = median_diff.total_seconds() / 60

        if minutes <= 1:
            return '1Min'
        elif minutes <= 5:
            return '5Min'
        elif minutes <= 15:
            return '15Min'
        elif minutes <= 30:
            return '30Min'
        elif minutes <= 60:
            return '1H'
        elif minutes <= 240:
            return '4H'
        elif minutes <= 1440:
            return '1D'
        elif minutes <= 10080:
            return '1W'
        else:
            return '1M'


def detect_time_unit(str_or_int: str) -> Tuple[int, str]:
    """
    Parse time unit from string (e.g. '5Min', '1H', '1D').

    Args:
        str_or_int: Time unit string

    Returns:
        Tuple of (value, unit) e.g. (5, 'Min')
    """
    try:
        from fast_trade.build_data_frame import detect_time_unit as ft_detect
        return ft_detect(str_or_int)
    except ImportError:
        import re
        match = re.match(r'^(\d+)(\w+)$', str(str_or_int))
        if match:
            return int(match.group(1)), match.group(2)
        return 1, str(str_or_int)


# ============================================================================
# DataFrame Preparation & Transformation
# ============================================================================

def prepare_df(df: pd.DataFrame, backtest: Dict[str, Any]) -> pd.DataFrame:
    """
    Prepare DataFrame for backtesting.

    Applies frequency resampling and time window filters from backtest config.

    Args:
        df: Raw OHLCV DataFrame
        backtest: Config with optional 'freq', 'start', 'stop' keys

    Returns:
        Filtered and resampled DataFrame
    """
    try:
        from fast_trade.build_data_frame import prepare_df as ft_prepare
        return ft_prepare(df, backtest)
    except ImportError:
        df = df.copy()
        start = backtest.get('start')
        stop = backtest.get('stop')
        if start:
            df = df[df.index >= pd.Timestamp(start)]
        if stop:
            df = df[df.index <= pd.Timestamp(stop)]
        return df


def apply_charting_to_df(
    df: pd.DataFrame,
    freq: str,
    start_time: Optional[str] = None,
    stop_time: Optional[str] = None
) -> pd.DataFrame:
    """
    Apply charting-specific transformations.

    Resamples data to target frequency and filters by time window.

    Args:
        df: OHLCV DataFrame
        freq: Target frequency string
        start_time: Start time filter
        stop_time: Stop time filter

    Returns:
        Transformed DataFrame
    """
    try:
        from fast_trade.build_data_frame import apply_charting_to_df as ft_chart
        return ft_chart(df, freq, start_time, stop_time)
    except ImportError:
        # Simple passthrough
        return prepare_df(df, {'freq': freq, 'start': start_time, 'stop': stop_time})


def apply_transformers_to_dataframe(
    df: pd.DataFrame,
    transformers: List[Dict[str, Any]]
) -> pd.DataFrame:
    """
    Apply list of indicator/transformer definitions to DataFrame.

    Each transformer is a dict: {'name': str, 'transformer': str, 'args': list}
    Adds computed columns to the DataFrame.

    Args:
        df: OHLCV DataFrame
        transformers: List of datapoint definitions

    Returns:
        DataFrame with indicator columns added
    """
    try:
        from fast_trade.build_data_frame import apply_transformers_to_dataframe as ft_apply
        return ft_apply(df, transformers)
    except ImportError:
        from .ft_indicators import apply_transformer
        for t in transformers:
            name = t.get('name', '')
            transformer = t.get('transformer', '')
            args = t.get('args', [])
            column = t.get('column')
            df = apply_transformer(df, name, transformer, args, column)
        return df


def build_data_frame(backtest: Dict[str, Any], csv_path: str) -> pd.DataFrame:
    """
    Full data pipeline: load CSV + apply transformers.

    Loads CSV, standardizes, applies frequency/time filters,
    then applies all datapoint transformers.

    Args:
        backtest: Strategy config with 'datapoints', 'freq', 'start', 'stop'
        csv_path: Path to OHLCV CSV file

    Returns:
        Fully prepared DataFrame with indicators
    """
    try:
        from fast_trade.build_data_frame import build_data_frame as ft_build
        return ft_build(backtest, csv_path)
    except ImportError:
        df = load_basic_df_from_csv(csv_path)
        df = prepare_df(df, backtest)
        datapoints = backtest.get('datapoints', [])
        if datapoints:
            df = apply_transformers_to_dataframe(df, datapoints)
        return df


def process_res_df(
    df: pd.DataFrame,
    ind: int,
    trans_res: Any
) -> pd.DataFrame:
    """
    Process transformer result and merge into main DataFrame.

    Internal function used by apply_transformers_to_dataframe to handle
    the output of a single transformer and add it as column(s) to the df.

    Args:
        df: Main OHLCV DataFrame
        ind: Index of the transformer in the datapoints list
        trans_res: Result from the transformer (Series, DataFrame, or scalar)

    Returns:
        DataFrame with transformer result merged in
    """
    try:
        from fast_trade.build_data_frame import process_res_df as ft_process
        return ft_process(df, ind, trans_res)
    except ImportError:
        if isinstance(trans_res, pd.Series):
            df[f'transformer_{ind}'] = trans_res
        elif isinstance(trans_res, pd.DataFrame):
            for col in trans_res.columns:
                df[f'transformer_{ind}_{col}'] = trans_res[col]
        else:
            df[f'transformer_{ind}'] = trans_res
        return df


def calculate_perc_missing(df: pd.DataFrame) -> float:
    """
    Calculate percentage of missing data in DataFrame.

    Args:
        df: DataFrame to analyze

    Returns:
        Percentage (0-100) of missing values
    """
    try:
        from fast_trade.calculate_perc_missing import calculate_perc_missing as ft_calc
        return ft_calc(df)
    except ImportError:
        if df.empty:
            return 100.0
        total = df.size
        missing = df.isna().sum().sum()
        return (missing / total) * 100


# ============================================================================
# Archive: Binance API
# ============================================================================

def binance_kline_to_df(klines: List) -> pd.DataFrame:
    """
    Convert raw Binance kline response to a pandas DataFrame.

    Internal helper that transforms the raw list-of-lists kline format
    returned by the Binance API into a standardized OHLCV DataFrame.

    Args:
        klines: Raw kline data from Binance API.
            Each element is a list:
            [open_time, open, high, low, close, volume, close_time, ...]

    Returns:
        DataFrame with columns: date, open, high, low, close, volume
        and DatetimeIndex.
    """
    from fast_trade.archive.binance_api import binance_kline_to_df as ft_kline_to_df
    return ft_kline_to_df(klines)


def get_binance_klines(
    symbol: str,
    start_date: datetime,
    end_date: datetime,
    tld: str = 'us'
) -> pd.DataFrame:
    """
    Fetch kline/candlestick data from Binance.

    Args:
        symbol: Trading pair (e.g. 'BTCUSDT')
        start_date: Start datetime
        end_date: End datetime
        tld: Top-level domain ('us' or 'com')

    Returns:
        DataFrame with OHLCV data
    """
    from fast_trade.archive.binance_api import get_binance_klines as ft_binance
    return ft_binance(symbol, start_date, end_date, tld=tld)


def get_binance_symbols(tld: str = 'us') -> List[str]:
    """
    Get available Binance trading symbols.

    Args:
        tld: 'us' or 'com'

    Returns:
        List of symbol strings
    """
    from fast_trade.archive.binance_api import get_available_symbols
    return get_available_symbols(tld=tld)


def get_binance_exchange_info(tld: str = 'us') -> Dict[str, Any]:
    """
    Get Binance exchange information.

    Args:
        tld: 'us' or 'com'

    Returns:
        Exchange info dict
    """
    from fast_trade.archive.binance_api import get_exchange_info
    return get_exchange_info(tld=tld)


def get_binance_oldest_date(symbol: str, tld: str = 'us') -> datetime:
    """
    Get oldest available date for a Binance symbol.

    Args:
        symbol: Trading pair
        tld: 'us' or 'com'

    Returns:
        Oldest available datetime
    """
    from fast_trade.archive.binance_api import get_oldest_date_available
    return get_oldest_date_available(symbol, tld=tld)


# ============================================================================
# Archive: Coinbase API
# ============================================================================

def df_from_candles(klines: List) -> pd.DataFrame:
    """
    Convert raw Coinbase candle response to a pandas DataFrame.

    Internal helper that transforms the raw candle data from the Coinbase API
    into a standardized OHLCV DataFrame.

    Args:
        klines: Raw candle data from Coinbase API.
            Each element is a list: [time, low, high, open, close, volume]

    Returns:
        DataFrame with OHLCV columns and DatetimeIndex.
    """
    from fast_trade.archive.coinbase_api import df_from_candles as ft_df_candles
    return ft_df_candles(klines)


def get_single_candle(
    product_id: str,
    params: Optional[Dict[str, Any]] = None,
    df: Optional[pd.DataFrame] = None
) -> pd.DataFrame:
    """
    Fetch a single candle/batch of candles from Coinbase for a given product.

    Low-level helper used internally by get_product_candles to fetch
    one page of candle data at a time.

    Args:
        product_id: Product ID (e.g. 'BTC-USD')
        params: Query parameters dict (start, end, granularity)
        df: Existing DataFrame to append to (optional)

    Returns:
        DataFrame with candle data appended
    """
    from fast_trade.archive.coinbase_api import get_single_candle as ft_single
    return ft_single(product_id, params=params or {}, df=df if df is not None else pd.DataFrame())


def get_coinbase_candles(
    product_id: str,
    start: Optional[datetime] = None,
    end: Optional[datetime] = None
) -> pd.DataFrame:
    """
    Fetch candle data from Coinbase.

    Args:
        product_id: Product ID (e.g. 'BTC-USD')
        start: Start datetime (optional)
        end: End datetime (optional)

    Returns:
        DataFrame with OHLCV data
    """
    from fast_trade.archive.coinbase_api import get_product_candles
    return get_product_candles(product_id, start=start, end=end)


def get_coinbase_assets() -> List[str]:
    """
    Get available Coinbase asset IDs.

    Returns:
        List of asset ID strings
    """
    from fast_trade.archive.coinbase_api import get_asset_ids
    return get_asset_ids()


def get_coinbase_products() -> List[Dict[str, Any]]:
    """
    Get all Coinbase products.

    Returns:
        List of product dicts
    """
    from fast_trade.archive.coinbase_api import get_products
    return get_products()


def get_coinbase_oldest_day(product_id: str) -> datetime:
    """
    Get oldest available day for a Coinbase product.

    Args:
        product_id: Product ID (e.g. 'BTC-USD')

    Returns:
        Oldest available datetime
    """
    from fast_trade.archive.coinbase_api import get_oldest_day
    return get_oldest_day(product_id)


# ============================================================================
# Archive: Local Database
# ============================================================================

def connect_to_db(db_path: str, create: bool = False):
    """
    Connect to the local SQLite archive database.

    Opens a connection to the SQLite database used for storing
    downloaded kline/candle data locally.

    Args:
        db_path: Path to SQLite database file
        create: If True, create the database if it doesn't exist

    Returns:
        sqlite3.Connection object
    """
    from fast_trade.archive.db_helpers import connect_to_db as ft_connect
    return ft_connect(db_path, create=create)


def get_kline(
    symbol: str,
    exchange: str,
    start_date: Optional[datetime] = None,
    end_date: Optional[datetime] = None,
    freq: str = '1Min'
) -> pd.DataFrame:
    """
    Get kline data from local SQLite database.

    Args:
        symbol: Symbol name
        exchange: Exchange name ('binance', 'coinbase', 'local')
        start_date: Start filter
        end_date: End filter
        freq: Frequency for resampling

    Returns:
        DataFrame with OHLCV data
    """
    from fast_trade.archive.db_helpers import get_kline as ft_kline
    return ft_kline(symbol, exchange, start_date, end_date, freq)


def get_local_assets() -> List[Tuple[str, str]]:
    """
    Get list of locally stored assets.

    Returns:
        List of (symbol, exchange) tuples
    """
    from fast_trade.archive.db_helpers import get_local_assets as ft_local
    return ft_local()


def update_klines_to_db(df: pd.DataFrame, symbol: str, exchange: str) -> str:
    """
    Store/update kline data in local SQLite database.

    Args:
        df: OHLCV DataFrame
        symbol: Symbol name
        exchange: Exchange name

    Returns:
        Status message
    """
    from fast_trade.archive.db_helpers import update_klines_to_db as ft_update
    return ft_update(df, symbol, exchange)


# ============================================================================
# Archive: Download & Update
# ============================================================================

def download_asset(
    symbol: str,
    exchange: str,
    start: Optional[Union[str, datetime]] = None,
    end: Optional[Union[str, datetime]] = None
) -> None:
    """
    Download asset data from exchange and store locally.

    Args:
        symbol: Symbol to download
        exchange: Exchange name ('binance', 'coinbase')
        start: Start date
        end: End date
    """
    from fast_trade.archive.cli import download_asset as ft_download
    ft_download(symbol, exchange, start=start, end=end)


def get_assets(exchange: str = 'local') -> List[str]:
    """
    Get available assets for an exchange.

    Args:
        exchange: Exchange name or 'local' for stored assets

    Returns:
        List of asset symbols
    """
    from fast_trade.archive.cli import get_assets as ft_assets
    return ft_assets(exchange)


def update_archive() -> None:
    """Update all locally stored archive data."""
    from fast_trade.archive.update_archive import update_archive as ft_update
    ft_update()


def update_single_archive(symbol: str, exchange: str) -> None:
    """
    Update archive for a single symbol.

    Args:
        symbol: Symbol to update
        exchange: Exchange name
    """
    from fast_trade.archive.update_archive import update_single_archive as ft_update
    ft_update(symbol, exchange)


def update_kline(
    symbol: str,
    exchange: str,
    start_date: Optional[datetime] = None,
    end_date: Optional[datetime] = None
) -> None:
    """
    Update kline data for a symbol.

    Args:
        symbol: Symbol
        exchange: Exchange name
        start_date: Start date
        end_date: End date
    """
    from fast_trade.archive.update_kline import update_kline as ft_update
    ft_update(symbol, exchange, start_date, end_date)


# ============================================================================
# Yahoo Finance Data Loading
# ============================================================================

def load_yfinance_data(
    symbol: str,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    interval: str = '1d'
) -> pd.DataFrame:
    """
    Load data from Yahoo Finance using yfinance.

    Args:
        symbol: Ticker symbol (e.g. 'AAPL', 'SPY')
        start_date: Start date string (YYYY-MM-DD) or None for default
        end_date: End date string (YYYY-MM-DD) or None for today
        interval: Data interval ('1m', '5m', '15m', '1h', '1d', '1wk', '1mo')

    Returns:
        DataFrame with date index and open, high, low, close, volume columns
        (lowercase, fast-trade format)
    """
    try:
        import yfinance as yf

        ticker = yf.Ticker(symbol)
        df = ticker.history(
            start=start_date or '2020-01-01',
            end=end_date,
            interval=interval,
            auto_adjust=True
        )

        if df is not None and len(df) > 0:
            # Standardize to fast-trade format (lowercase columns)
            df.columns = [c.lower() for c in df.columns]
            df.index.name = 'date'

            # Ensure required columns exist
            required = ['open', 'high', 'low', 'close', 'volume']
            available = [col for col in required if col in df.columns]

            return df[available]

        return pd.DataFrame()
    except Exception as e:
        print(f"yfinance error for {symbol}: {e}")
        return pd.DataFrame()


# ============================================================================
# Synthetic Data Generation
# ============================================================================

def generate_synthetic_ohlcv(
    periods: int = 1000,
    start_date: str = '2023-01-01',
    freq: str = '1H',
    initial_price: float = 100.0,
    volatility: float = 0.02,
    drift: float = 0.0002,
    seed: Optional[int] = 42
) -> pd.DataFrame:
    """
    Generate synthetic OHLCV data for testing.

    Uses geometric Brownian motion to simulate realistic price movements.

    Args:
        periods: Number of bars to generate
        start_date: Start date string
        freq: Frequency ('1Min', '5Min', '1H', '1D', etc.)
        initial_price: Starting price
        volatility: Price volatility (std of returns)
        drift: Mean return per period
        seed: Random seed for reproducibility

    Returns:
        DataFrame with date index and open, high, low, close, volume columns
    """
    if seed is not None:
        np.random.seed(seed)

    # Normalize deprecated uppercase freq aliases
    freq_norm = freq.replace('H', 'h').replace('Min', 'min').replace('D', 'D')
    dates = pd.date_range(start=start_date, periods=periods, freq=freq_norm)
    returns = np.random.normal(drift, volatility, periods)
    price = initial_price * np.exp(np.cumsum(returns))

    df = pd.DataFrame({
        'open': price * (1 + np.random.uniform(-0.005, 0.005, periods)),
        'high': price * (1 + np.random.uniform(0, 0.015, periods)),
        'low': price * (1 - np.random.uniform(0, 0.015, periods)),
        'close': price,
        'volume': np.random.uniform(500000, 5000000, periods),
    }, index=dates)

    # Ensure high >= max(open, close) and low <= min(open, close)
    df['high'] = df[['open', 'high', 'close']].max(axis=1)
    df['low'] = df[['open', 'low', 'close']].min(axis=1)
    df.index.name = 'date'

    return df
