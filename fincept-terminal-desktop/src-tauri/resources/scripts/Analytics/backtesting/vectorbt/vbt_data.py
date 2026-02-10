"""
VBT Data Module

Covers all vectorbt data classes:
- Data (base class)
- YFData (Yahoo Finance)
- BinanceData (Binance crypto)
- CCXTData (CCXT multi-exchange)
- AlpacaData (Alpaca broker)
- GBMData / SyntheticData (Geometric Brownian Motion)

All classes support:
- download(symbols, start, end, **kwargs)
- update(symbols, **kwargs)
- concat(other)
- get(symbol) -> pd.DataFrame
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, List, Union
from datetime import datetime, timedelta


class Data:
    """
    Base data container matching vectorbt's Data interface.

    Stores OHLCV data for multiple symbols as a dict of DataFrames.
    """

    def __init__(self, data: Dict[str, pd.DataFrame], tz: Optional[str] = None):
        self._data = data
        self._tz = tz

    @property
    def data(self) -> Dict[str, pd.DataFrame]:
        return self._data

    @property
    def symbols(self) -> List[str]:
        return list(self._data.keys())

    def get(self, symbol: Optional[str] = None) -> pd.DataFrame:
        """Get data for a symbol. If single symbol, returns it directly."""
        if symbol is None:
            if len(self._data) == 1:
                return list(self._data.values())[0]
            return pd.concat(self._data.values(), axis=1, keys=self._data.keys())
        return self._data.get(symbol, pd.DataFrame())

    @classmethod
    def from_data(cls, data: Dict[str, pd.DataFrame]) -> 'Data':
        """Create Data from a dict of DataFrames."""
        return cls(data)

    def concat(self, other: 'Data') -> 'Data':
        """Concatenate two Data objects (append new rows)."""
        merged = {}
        all_symbols = set(self.symbols + other.symbols)
        for sym in all_symbols:
            dfs = []
            if sym in self._data:
                dfs.append(self._data[sym])
            if sym in other._data:
                dfs.append(other._data[sym])
            if dfs:
                combined = pd.concat(dfs)
                combined = combined[~combined.index.duplicated(keep='last')]
                combined = combined.sort_index()
                merged[sym] = combined
        return Data(merged, self._tz)

    def tz_localize(self, tz: str) -> 'Data':
        """Localize timezone."""
        new_data = {}
        for sym, df in self._data.items():
            new_data[sym] = df.copy()
            if not df.index.tzinfo:
                new_data[sym].index = df.index.tz_localize(tz)
        return Data(new_data, tz)

    def tz_convert(self, tz: str) -> 'Data':
        """Convert timezone."""
        new_data = {}
        for sym, df in self._data.items():
            new_data[sym] = df.copy()
            if df.index.tzinfo:
                new_data[sym].index = df.index.tz_convert(tz)
        return Data(new_data, tz)

    def align_index(self, method: str = 'ffill') -> 'Data':
        """Align all symbols to the same index."""
        if len(self._data) <= 1:
            return self
        all_idx = sorted(set().union(*[set(df.index) for df in self._data.values()]))
        new_data = {}
        for sym, df in self._data.items():
            reindexed = df.reindex(all_idx)
            if method == 'ffill':
                reindexed = reindexed.ffill()
            elif method == 'bfill':
                reindexed = reindexed.bfill()
            new_data[sym] = reindexed
        return Data(new_data, self._tz)

    def stats(self) -> Dict[str, Any]:
        """Basic statistics about the data."""
        result = {}
        for sym, df in self._data.items():
            result[sym] = {
                'rows': len(df),
                'columns': list(df.columns),
                'start': str(df.index[0]) if len(df) > 0 else None,
                'end': str(df.index[-1]) if len(df) > 0 else None,
                'missing': int(df.isnull().sum().sum()),
            }
        return result

    @classmethod
    def download(cls, symbols, start=None, end=None, **kwargs):
        """Override in subclass."""
        raise NotImplementedError("Use a specific data class (YFData, BinanceData, etc.)")

    def update(self, **kwargs):
        """Override in subclass."""
        raise NotImplementedError("Use a specific data class (YFData, BinanceData, etc.)")


class YFData(Data):
    """
    Yahoo Finance data downloader (mimics vbt.YFData).

    Uses yfinance library.
    """

    @classmethod
    def download(
        cls,
        symbols: Union[str, List[str]],
        start: Optional[str] = None,
        end: Optional[str] = None,
        period: str = '1y',
        interval: str = '1d',
        **kwargs,
    ) -> 'YFData':
        """
        Download OHLCV data from Yahoo Finance.

        Args:
            symbols: Ticker(s) - string or list
            start: Start date (YYYY-MM-DD)
            end: End date (YYYY-MM-DD)
            period: Period string (1d, 5d, 1mo, 3mo, 6mo, 1y, 2y, 5y, 10y, ytd, max)
            interval: Data interval (1m, 2m, 5m, 15m, 30m, 60m, 90m, 1h, 1d, 5d, 1wk, 1mo, 3mo)
        """
        import yfinance as yf

        if isinstance(symbols, str):
            symbols = [symbols]

        data = {}
        for sym in symbols:
            try:
                ticker = yf.Ticker(sym)
                if start and end:
                    df = ticker.history(start=start, end=end, interval=interval)
                else:
                    df = ticker.history(period=period, interval=interval)

                if len(df) > 0:
                    # Standardize column names
                    df.columns = [c.title() for c in df.columns]
                    # Keep OHLCV
                    cols = [c for c in ['Open', 'High', 'Low', 'Close', 'Volume'] if c in df.columns]
                    df = df[cols]
                    # Round to 4 decimal places to eliminate float32 rounding noise
                    for col in ['Open', 'High', 'Low', 'Close']:
                        if col in df.columns:
                            df[col] = df[col].round(4)
                    data[sym] = df
            except Exception:
                continue

        return cls(data)

    @classmethod
    def download_symbol(cls, symbol: str, **kwargs) -> pd.DataFrame:
        """Download a single symbol."""
        result = cls.download(symbol, **kwargs)
        return result.get(symbol)

    def update(self, **kwargs) -> 'YFData':
        """Update data with latest bars."""
        if not self._data:
            return self

        last_dates = {sym: df.index[-1] for sym, df in self._data.items() if len(df) > 0}
        if not last_dates:
            return self

        earliest_last = min(last_dates.values())
        start = str(earliest_last.date())
        new = YFData.download(self.symbols, start=start, **kwargs)
        return self.concat(new)


class BinanceData(Data):
    """
    Binance crypto data downloader (mimics vbt.BinanceData).

    Uses ccxt or requests to Binance API.
    """

    @classmethod
    def download(
        cls,
        symbols: Union[str, List[str]],
        start: Optional[str] = None,
        end: Optional[str] = None,
        timeframe: str = '1d',
        limit: int = 1000,
        **kwargs,
    ) -> 'BinanceData':
        """
        Download OHLCV data from Binance.

        Args:
            symbols: Trading pair(s) e.g. 'BTC/USDT'
            start: Start date
            end: End date
            timeframe: Candle timeframe (1m, 5m, 15m, 1h, 4h, 1d, 1w)
            limit: Max candles per request
        """
        try:
            import ccxt
            exchange = ccxt.binance({'enableRateLimit': True})
        except ImportError:
            # Fallback to requests
            return cls._download_via_api(symbols, start, end, timeframe, limit)

        if isinstance(symbols, str):
            symbols = [symbols]

        data = {}
        for sym in symbols:
            try:
                since = exchange.parse8601(start + 'T00:00:00Z') if start else None
                ohlcv = exchange.fetch_ohlcv(sym, timeframe, since=since, limit=limit)
                if ohlcv:
                    df = pd.DataFrame(ohlcv, columns=['Timestamp', 'Open', 'High', 'Low', 'Close', 'Volume'])
                    df['Timestamp'] = pd.to_datetime(df['Timestamp'], unit='ms')
                    df.set_index('Timestamp', inplace=True)
                    data[sym] = df
            except Exception:
                continue

        return cls(data)

    @classmethod
    def _download_via_api(cls, symbols, start, end, timeframe, limit):
        """Fallback download using requests."""
        import requests

        if isinstance(symbols, str):
            symbols = [symbols]

        tf_map = {'1m': '1m', '5m': '5m', '15m': '15m', '1h': '1h', '4h': '4h', '1d': '1d', '1w': '1w'}
        interval = tf_map.get(timeframe, '1d')

        data = {}
        for sym in symbols:
            try:
                api_sym = sym.replace('/', '')
                url = f'https://api.binance.com/api/v3/klines?symbol={api_sym}&interval={interval}&limit={limit}'
                if start:
                    ts = int(pd.Timestamp(start).timestamp() * 1000)
                    url += f'&startTime={ts}'
                resp = requests.get(url, timeout=30)
                klines = resp.json()
                if isinstance(klines, list) and len(klines) > 0:
                    df = pd.DataFrame(klines, columns=[
                        'Timestamp', 'Open', 'High', 'Low', 'Close', 'Volume',
                        'CloseTime', 'QuoteVol', 'Trades', 'TakerBuyBase', 'TakerBuyQuote', 'Ignore'
                    ])
                    df['Timestamp'] = pd.to_datetime(df['Timestamp'], unit='ms')
                    df.set_index('Timestamp', inplace=True)
                    for c in ['Open', 'High', 'Low', 'Close', 'Volume']:
                        df[c] = df[c].astype(float)
                    data[sym] = df[['Open', 'High', 'Low', 'Close', 'Volume']]
            except Exception:
                continue

        return cls(data)

    def update(self, **kwargs) -> 'BinanceData':
        """Update with latest candles."""
        if not self._data:
            return self
        last_dates = {sym: df.index[-1] for sym, df in self._data.items() if len(df) > 0}
        if not last_dates:
            return self
        earliest_last = min(last_dates.values())
        start = str(earliest_last.date())
        new = BinanceData.download(self.symbols, start=start, **kwargs)
        return self.concat(new)


class CCXTData(Data):
    """
    Multi-exchange crypto data via CCXT (mimics vbt.CCXTData).
    """

    @classmethod
    def download(
        cls,
        symbols: Union[str, List[str]],
        exchange_id: str = 'binance',
        start: Optional[str] = None,
        timeframe: str = '1d',
        limit: int = 1000,
        **kwargs,
    ) -> 'CCXTData':
        """Download from any CCXT-supported exchange."""
        import ccxt

        exchange_class = getattr(ccxt, exchange_id)
        exchange = exchange_class({'enableRateLimit': True})

        if isinstance(symbols, str):
            symbols = [symbols]

        data = {}
        for sym in symbols:
            try:
                since = exchange.parse8601(start + 'T00:00:00Z') if start else None
                ohlcv = exchange.fetch_ohlcv(sym, timeframe, since=since, limit=limit)
                if ohlcv:
                    df = pd.DataFrame(ohlcv, columns=['Timestamp', 'Open', 'High', 'Low', 'Close', 'Volume'])
                    df['Timestamp'] = pd.to_datetime(df['Timestamp'], unit='ms')
                    df.set_index('Timestamp', inplace=True)
                    data[sym] = df
            except Exception:
                continue

        return cls(data)

    def update(self, **kwargs) -> 'CCXTData':
        if not self._data:
            return self
        last_dates = {sym: df.index[-1] for sym, df in self._data.items() if len(df) > 0}
        if not last_dates:
            return self
        start = str(min(last_dates.values()).date())
        new = CCXTData.download(self.symbols, start=start, **kwargs)
        return self.concat(new)


class AlpacaData(Data):
    """
    Alpaca broker data downloader (mimics vbt.AlpacaData).
    """

    @classmethod
    def download(
        cls,
        symbols: Union[str, List[str]],
        start: Optional[str] = None,
        end: Optional[str] = None,
        timeframe: str = '1Day',
        api_key: Optional[str] = None,
        api_secret: Optional[str] = None,
        base_url: str = 'https://data.alpaca.markets',
        **kwargs,
    ) -> 'AlpacaData':
        """Download from Alpaca Markets API."""
        import requests

        if isinstance(symbols, str):
            symbols = [symbols]

        headers = {}
        if api_key and api_secret:
            headers = {
                'APCA-API-KEY-ID': api_key,
                'APCA-API-SECRET-KEY': api_secret,
            }

        data = {}
        for sym in symbols:
            try:
                url = f'{base_url}/v2/stocks/{sym}/bars'
                params = {'timeframe': timeframe, 'limit': 10000}
                if start:
                    params['start'] = start
                if end:
                    params['end'] = end

                resp = requests.get(url, headers=headers, params=params, timeout=30)
                result = resp.json()
                bars = result.get('bars', [])

                if bars:
                    df = pd.DataFrame(bars)
                    df['t'] = pd.to_datetime(df['t'])
                    df.set_index('t', inplace=True)
                    df = df.rename(columns={'o': 'Open', 'h': 'High', 'l': 'Low', 'c': 'Close', 'v': 'Volume'})
                    data[sym] = df[['Open', 'High', 'Low', 'Close', 'Volume']]
            except Exception:
                continue

        return cls(data)

    def update(self, **kwargs) -> 'AlpacaData':
        if not self._data:
            return self
        last_dates = {sym: df.index[-1] for sym, df in self._data.items() if len(df) > 0}
        if not last_dates:
            return self
        start = str(min(last_dates.values()).date())
        new = AlpacaData.download(self.symbols, start=start, **kwargs)
        return self.concat(new)


class GBMData(Data):
    """
    Geometric Brownian Motion synthetic data (mimics vbt.GBMData).

    Generates synthetic price paths for testing strategies without real data.
    """

    @classmethod
    def download(
        cls,
        symbols: Union[str, List[str], int] = 1,
        start: str = '2020-01-01',
        end: str = '2024-01-01',
        freq: str = '1D',
        s0: float = 100.0,
        mu: float = 0.1,
        sigma: float = 0.2,
        seed: Optional[int] = None,
        **kwargs,
    ) -> 'GBMData':
        """
        Generate synthetic GBM price paths.

        Args:
            symbols: Symbol name(s) or number of paths
            start: Start date
            end: End date
            freq: Frequency (1D, 1H, etc.)
            s0: Initial price
            mu: Annual drift (mean return)
            sigma: Annual volatility
            seed: Random seed
        """
        if seed is not None:
            np.random.seed(seed)

        dates = pd.date_range(start=start, end=end, freq=freq)
        n = len(dates)

        if isinstance(symbols, int):
            symbols = [f'SYM_{i}' for i in range(symbols)]
        elif isinstance(symbols, str):
            symbols = [symbols]

        dt = 1.0 / 252.0  # daily step

        data = {}
        for sym in symbols:
            z = np.random.randn(n)
            log_returns = (mu - 0.5 * sigma ** 2) * dt + sigma * np.sqrt(dt) * z
            prices = s0 * np.exp(np.cumsum(log_returns))

            # Generate OHLCV from close
            close = prices
            noise = sigma * np.sqrt(dt) * 0.3
            high = close * (1 + np.abs(np.random.randn(n) * noise))
            low = close * (1 - np.abs(np.random.randn(n) * noise))
            open_ = np.roll(close, 1)
            open_[0] = s0
            volume = np.random.lognormal(mean=15, sigma=1, size=n).astype(int)

            df = pd.DataFrame({
                'Open': open_,
                'High': high,
                'Low': low,
                'Close': close,
                'Volume': volume,
            }, index=dates)

            data[sym] = df

        return cls(data)

    @classmethod
    def generate_symbol(cls, **kwargs) -> pd.DataFrame:
        """Generate a single synthetic symbol."""
        result = cls.download(symbols='SYN', **kwargs)
        return result.get('SYN')

    def update(self, n_bars: int = 1, **kwargs) -> 'GBMData':
        """Extend each symbol with n_bars more synthetic data."""
        new_data = {}
        for sym, df in self._data.items():
            last_close = df['Close'].iloc[-1]
            last_date = df.index[-1]
            mu = kwargs.get('mu', 0.1)
            sigma = kwargs.get('sigma', 0.2)
            dt = 1.0 / 252.0

            new_dates = pd.date_range(start=last_date + pd.Timedelta(days=1), periods=n_bars, freq='1D')
            z = np.random.randn(n_bars)
            log_returns = (mu - 0.5 * sigma ** 2) * dt + sigma * np.sqrt(dt) * z
            prices = last_close * np.exp(np.cumsum(log_returns))

            noise = sigma * np.sqrt(dt) * 0.3
            new_df = pd.DataFrame({
                'Open': np.roll(prices, 1),
                'High': prices * (1 + np.abs(np.random.randn(n_bars) * noise)),
                'Low': prices * (1 - np.abs(np.random.randn(n_bars) * noise)),
                'Close': prices,
                'Volume': np.random.lognormal(mean=15, sigma=1, size=n_bars).astype(int),
            }, index=new_dates)
            new_df['Open'].iloc[0] = last_close

            new_data[sym] = pd.concat([df, new_df])

        return GBMData(new_data)


# Alias
SyntheticData = GBMData
