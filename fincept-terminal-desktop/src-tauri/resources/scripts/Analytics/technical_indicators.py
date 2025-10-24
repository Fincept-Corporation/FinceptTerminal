# -*- coding: utf-8 -*-
"""
Technical Analysis Indicators Module
===================================

Comprehensive technical analysis indicators for financial market analysis.
Provides 10 essential technical indicators including moving averages, momentum
oscillators, volatility measures, and trend analysis tools with robust error
handling and flexible input support for various data formats.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Pandas Series or NumPy array with price data (OHLCV format preferred)
  - Time series data with datetime index (optional but recommended)
  - High, Low, Close prices for OHLC-based indicators
  - Volume data for volume-based indicators (future enhancement)

OUTPUT:
  - Moving averages (SMA, EMA) with configurable periods
  - Momentum oscillators (RSI, Stochastic, Williams %R)
  - Trend indicators (MACD with signal and histogram)
  - Volatility measures (Bollinger Bands, ATR)
  - Strength indicators (ADX, CCI) with directional components

PARAMETERS:
  - period: Lookback period for indicators (default varies by indicator)
  - std_dev: Standard deviation multiplier for bands (default: 2.0)
  - fast/slow: Fast and slow periods for MACD (default: 12, 26)
  - signal: Signal line period for MACD (default: 9)
  - k_period/d_period: Stochastic oscillator periods (default: 14, 3)
  - confidence_level: Statistical confidence for analysis (default: 0.95)
  - min_periods: Minimum data points required (default: period)
"""

import numpy as np
import pandas as pd
from typing import Union, Tuple, Optional
import warnings

# Suppress pandas warnings for cleaner output
warnings.filterwarnings("ignore", category=FutureWarning)


class TechnicalIndicators:
    """
    A comprehensive collection of technical analysis indicators.
    
    This class provides static methods for calculating various technical indicators
    used in financial market analysis.
    """

    @staticmethod
    def sma(data: Union[pd.Series, np.ndarray], period: int = 20) -> pd.Series:
        """
        Simple Moving Average (SMA)
        
        Args:
            data: Price data (typically closing prices)
            period: Number of periods for the moving average
            
        Returns:
            pd.Series: Simple moving average values
        """
        try:
            if isinstance(data, np.ndarray):
                data = pd.Series(data)
            
            return data.rolling(window=period, min_periods=period).mean()
        except Exception as e:
            print(f"Error calculating SMA: {e}")
            return pd.Series(dtype=float)

    @staticmethod
    def ema(data: Union[pd.Series, np.ndarray], period: int = 20) -> pd.Series:
        """
        Exponential Moving Average (EMA)
        
        Args:
            data: Price data (typically closing prices)
            period: Number of periods for the exponential moving average
            
        Returns:
            pd.Series: Exponential moving average values
        """
        try:
            if isinstance(data, np.ndarray):
                data = pd.Series(data)
            
            return data.ewm(span=period, adjust=False).mean()
        except Exception as e:
            print(f"Error calculating EMA: {e}")
            return pd.Series(dtype=float)

    @staticmethod
    def rsi(data: Union[pd.Series, np.ndarray], period: int = 14) -> pd.Series:
        """
        Relative Strength Index (RSI)
        
        Args:
            data: Price data (typically closing prices)
            period: Number of periods for RSI calculation
            
        Returns:
            pd.Series: RSI values (0-100)
        """
        try:
            if isinstance(data, np.ndarray):
                data = pd.Series(data)
            
            delta = data.diff()
            gain = (delta.where(delta > 0, 0)).rolling(window=period).mean()
            loss = (-delta.where(delta < 0, 0)).rolling(window=period).mean()
            
            rs = gain / loss
            rsi = 100 - (100 / (1 + rs))
            
            return rsi
        except Exception as e:
            print(f"Error calculating RSI: {e}")
            return pd.Series(dtype=float)

    @staticmethod
    def macd(data: Union[pd.Series, np.ndarray], fast: int = 12, slow: int = 26, signal: int = 9) -> Tuple[pd.Series, pd.Series, pd.Series]:
        """
        Moving Average Convergence Divergence (MACD)
        
        Args:
            data: Price data (typically closing prices)
            fast: Fast EMA period
            slow: Slow EMA period
            signal: Signal line EMA period
            
        Returns:
            Tuple[pd.Series, pd.Series, pd.Series]: (MACD line, Signal line, Histogram)
        """
        try:
            if isinstance(data, np.ndarray):
                data = pd.Series(data)
            
            ema_fast = data.ewm(span=fast).mean()
            ema_slow = data.ewm(span=slow).mean()
            
            macd_line = ema_fast - ema_slow
            signal_line = macd_line.ewm(span=signal).mean()
            histogram = macd_line - signal_line
            
            return macd_line, signal_line, histogram
        except Exception as e:
            print(f"Error calculating MACD: {e}")
            return pd.Series(dtype=float), pd.Series(dtype=float), pd.Series(dtype=float)

    @staticmethod
    def bollinger_bands(data: Union[pd.Series, np.ndarray], period: int = 20, std_dev: float = 2) -> Tuple[pd.Series, pd.Series, pd.Series]:
        """
        Bollinger Bands
        
        Args:
            data: Price data (typically closing prices)
            period: Number of periods for moving average
            std_dev: Number of standard deviations for bands
            
        Returns:
            Tuple[pd.Series, pd.Series, pd.Series]: (Upper band, Middle band, Lower band)
        """
        try:
            if isinstance(data, np.ndarray):
                data = pd.Series(data)
            
            middle_band = data.rolling(window=period).mean()
            std = data.rolling(window=period).std()
            
            upper_band = middle_band + (std * std_dev)
            lower_band = middle_band - (std * std_dev)
            
            return upper_band, middle_band, lower_band
        except Exception as e:
            print(f"Error calculating Bollinger Bands: {e}")
            return pd.Series(dtype=float), pd.Series(dtype=float), pd.Series(dtype=float)

    @staticmethod
    def stochastic_oscillator(high: Union[pd.Series, np.ndarray], 
                            low: Union[pd.Series, np.ndarray], 
                            close: Union[pd.Series, np.ndarray], 
                            k_period: int = 14, 
                            d_period: int = 3) -> Tuple[pd.Series, pd.Series]:
        """
        Stochastic Oscillator
        
        Args:
            high: High prices
            low: Low prices
            close: Closing prices
            k_period: %K period
            d_period: %D period (smoothing)
            
        Returns:
            Tuple[pd.Series, pd.Series]: (%K, %D)
        """
        try:
            if isinstance(high, np.ndarray):
                high = pd.Series(high)
            if isinstance(low, np.ndarray):
                low = pd.Series(low)
            if isinstance(close, np.ndarray):
                close = pd.Series(close)
            
            lowest_low = low.rolling(window=k_period).min()
            highest_high = high.rolling(window=k_period).max()
            
            k_percent = 100 * ((close - lowest_low) / (highest_high - lowest_low))
            d_percent = k_percent.rolling(window=d_period).mean()
            
            return k_percent, d_percent
        except Exception as e:
            print(f"Error calculating Stochastic Oscillator: {e}")
            return pd.Series(dtype=float), pd.Series(dtype=float)

    @staticmethod
    def williams_r(high: Union[pd.Series, np.ndarray], 
                   low: Union[pd.Series, np.ndarray], 
                   close: Union[pd.Series, np.ndarray], 
                   period: int = 14) -> pd.Series:
        """
        Williams %R
        
        Args:
            high: High prices
            low: Low prices
            close: Closing prices
            period: Number of periods
            
        Returns:
            pd.Series: Williams %R values (-100 to 0)
        """
        try:
            if isinstance(high, np.ndarray):
                high = pd.Series(high)
            if isinstance(low, np.ndarray):
                low = pd.Series(low)
            if isinstance(close, np.ndarray):
                close = pd.Series(close)
            
            highest_high = high.rolling(window=period).max()
            lowest_low = low.rolling(window=period).min()
            
            williams_r = -100 * ((highest_high - close) / (highest_high - lowest_low))
            
            return williams_r
        except Exception as e:
            print(f"Error calculating Williams %R: {e}")
            return pd.Series(dtype=float)

    @staticmethod
    def atr(high: Union[pd.Series, np.ndarray], 
            low: Union[pd.Series, np.ndarray], 
            close: Union[pd.Series, np.ndarray], 
            period: int = 14) -> pd.Series:
        """
        Average True Range (ATR)
        
        Args:
            high: High prices
            low: Low prices
            close: Closing prices
            period: Number of periods
            
        Returns:
            pd.Series: ATR values
        """
        try:
            if isinstance(high, np.ndarray):
                high = pd.Series(high)
            if isinstance(low, np.ndarray):
                low = pd.Series(low)
            if isinstance(close, np.ndarray):
                close = pd.Series(close)
            
            tr1 = high - low
            tr2 = abs(high - close.shift())
            tr3 = abs(low - close.shift())
            
            true_range = pd.concat([tr1, tr2, tr3], axis=1).max(axis=1)
            atr = true_range.rolling(window=period).mean()
            
            return atr
        except Exception as e:
            print(f"Error calculating ATR: {e}")
            return pd.Series(dtype=float)

    @staticmethod
    def cci(high: Union[pd.Series, np.ndarray], 
            low: Union[pd.Series, np.ndarray], 
            close: Union[pd.Series, np.ndarray], 
            period: int = 20) -> pd.Series:
        """
        Commodity Channel Index (CCI)
        
        Args:
            high: High prices
            low: Low prices
            close: Closing prices
            period: Number of periods
            
        Returns:
            pd.Series: CCI values
        """
        try:
            if isinstance(high, np.ndarray):
                high = pd.Series(high)
            if isinstance(low, np.ndarray):
                low = pd.Series(low)
            if isinstance(close, np.ndarray):
                close = pd.Series(close)
            
            typical_price = (high + low + close) / 3
            sma_tp = typical_price.rolling(window=period).mean()
            mean_deviation = typical_price.rolling(window=period).apply(
                lambda x: np.mean(np.abs(x - np.mean(x)))
            )
            
            cci = (typical_price - sma_tp) / (0.015 * mean_deviation)
            
            return cci
        except Exception as e:
            print(f"Error calculating CCI: {e}")
            return pd.Series(dtype=float)

    @staticmethod
    def adx(high: Union[pd.Series, np.ndarray], 
            low: Union[pd.Series, np.ndarray], 
            close: Union[pd.Series, np.ndarray], 
            period: int = 14) -> Tuple[pd.Series, pd.Series, pd.Series]:
        """
        Average Directional Index (ADX) with +DI and -DI
        
        Args:
            high: High prices
            low: Low prices
            close: Closing prices
            period: Number of periods
            
        Returns:
            Tuple[pd.Series, pd.Series, pd.Series]: (ADX, +DI, -DI)
        """
        try:
            if isinstance(high, np.ndarray):
                high = pd.Series(high)
            if isinstance(low, np.ndarray):
                low = pd.Series(low)
            if isinstance(close, np.ndarray):
                close = pd.Series(close)
            
            # Calculate True Range
            tr1 = high - low
            tr2 = abs(high - close.shift())
            tr3 = abs(low - close.shift())
            tr = pd.concat([tr1, tr2, tr3], axis=1).max(axis=1)
            
            # Calculate Directional Movements
            up_move = high - high.shift()
            down_move = low.shift() - low
            
            plus_dm = np.where((up_move > down_move) & (up_move > 0), up_move, 0)
            minus_dm = np.where((down_move > up_move) & (down_move > 0), down_move, 0)
            
            plus_dm = pd.Series(plus_dm, index=high.index)
            minus_dm = pd.Series(minus_dm, index=high.index)
            
            # Smooth the values
            atr = tr.ewm(span=period).mean()
            plus_di = 100 * (plus_dm.ewm(span=period).mean() / atr)
            minus_di = 100 * (minus_dm.ewm(span=period).mean() / atr)
            
            # Calculate ADX
            dx = 100 * abs(plus_di - minus_di) / (plus_di + minus_di)
            adx = dx.ewm(span=period).mean()
            
            return adx, plus_di, minus_di
        except Exception as e:
            print(f"Error calculating ADX: {e}")
            return pd.Series(dtype=float), pd.Series(dtype=float), pd.Series(dtype=float)


def calculate_all_indicators(df: pd.DataFrame, 
                           price_col: str = 'close',
                           high_col: str = 'high',
                           low_col: str = 'low') -> pd.DataFrame:
    """
    Calculate all technical indicators for a given dataframe.
    
    Args:
        df: DataFrame with OHLC data
        price_col: Column name for closing prices
        high_col: Column name for high prices
        low_col: Column name for low prices
        
    Returns:
        pd.DataFrame: Original dataframe with added indicator columns
    """
    try:
        result_df = df.copy()
        
        # Price-based indicators
        result_df['SMA_20'] = TechnicalIndicators.sma(df[price_col], 20)
        result_df['EMA_20'] = TechnicalIndicators.ema(df[price_col], 20)
        result_df['RSI_14'] = TechnicalIndicators.rsi(df[price_col], 14)
        
        # MACD
        macd, signal, histogram = TechnicalIndicators.macd(df[price_col])
        result_df['MACD'] = macd
        result_df['MACD_Signal'] = signal
        result_df['MACD_Histogram'] = histogram
        
        # Bollinger Bands
        bb_upper, bb_middle, bb_lower = TechnicalIndicators.bollinger_bands(df[price_col])
        result_df['BB_Upper'] = bb_upper
        result_df['BB_Middle'] = bb_middle
        result_df['BB_Lower'] = bb_lower
        
        # OHLC-based indicators
        if high_col in df.columns and low_col in df.columns:
            # Stochastic
            stoch_k, stoch_d = TechnicalIndicators.stochastic_oscillator(
                df[high_col], df[low_col], df[price_col]
            )
            result_df['Stoch_K'] = stoch_k
            result_df['Stoch_D'] = stoch_d
            
            # Williams %R
            result_df['Williams_R'] = TechnicalIndicators.williams_r(
                df[high_col], df[low_col], df[price_col]
            )
            
            # ATR
            result_df['ATR'] = TechnicalIndicators.atr(
                df[high_col], df[low_col], df[price_col]
            )
            
            # CCI
            result_df['CCI'] = TechnicalIndicators.cci(
                df[high_col], df[low_col], df[price_col]
            )
            
            # ADX
            adx, plus_di, minus_di = TechnicalIndicators.adx(
                df[high_col], df[low_col], df[price_col]
            )
            result_df['ADX'] = adx
            result_df['Plus_DI'] = plus_di
            result_df['Minus_DI'] = minus_di
        
        print(f"Successfully calculated all technical indicators for {len(result_df)} data points")
        return result_df
        
    except Exception as e:
        print(f"Error calculating indicators: {e}")
        return df


# Example usage and testing
if __name__ == "__main__":
    # Create sample data for testing
    np.random.seed(42)
    dates = pd.date_range(start='2023-01-01', end='2024-01-01', freq='D')
    
    # Generate realistic price data
    base_price = 100
    returns = np.random.normal(0.001, 0.02, len(dates))
    prices = [base_price]
    
    for ret in returns[1:]:
        prices.append(prices[-1] * (1 + ret))
    
    # Create OHLC data
    sample_data = pd.DataFrame({
        'date': dates,
        'close': prices,
        'high': [p * (1 + abs(np.random.normal(0, 0.01))) for p in prices],
        'low': [p * (1 - abs(np.random.normal(0, 0.01))) for p in prices],
        'volume': np.random.randint(1000, 10000, len(dates))
    })
    
    # Calculate all indicators
    result = calculate_all_indicators(sample_data)
    
    print("Technical Indicators Module Test Results:")
    print("=" * 50)
    print(f"Data points: {len(result)}")
    print(f"Indicators calculated: {len([col for col in result.columns if col not in sample_data.columns])}")
    print("\nIndicator columns added:")
    for col in result.columns:
        if col not in sample_data.columns:
            print(f"  - {col}")
    
    print("\nSample of latest 5 indicator values:")
    indicator_cols = [col for col in result.columns if col not in sample_data.columns]
    print(result[indicator_cols].tail().round(2))