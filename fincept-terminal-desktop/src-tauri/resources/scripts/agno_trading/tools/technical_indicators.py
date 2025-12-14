"""
Technical Analysis Tools

Provides technical indicators and analysis tools for trading agents.
"""

import json
from typing import List, Dict, Any, Optional
import math

try:
    import pandas as pd
    import numpy as np
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False

try:
    import talib
    TALIB_AVAILABLE = True
except ImportError:
    TALIB_AVAILABLE = False


def calculate_sma(prices: List[float], period: int = 20) -> Dict[str, Any]:
    """
    Calculate Simple Moving Average

    Args:
        prices: List of prices
        period: Period for SMA calculation

    Returns:
        Dictionary with SMA values
    """
    try:
        if not PANDAS_AVAILABLE:
            # Manual calculation
            if len(prices) < period:
                return {"error": f"Not enough data. Need at least {period} prices."}

            sma_values = []
            for i in range(len(prices) - period + 1):
                window = prices[i:i+period]
                sma = sum(window) / period
                sma_values.append(sma)

            return {
                "indicator": "SMA",
                "period": period,
                "values": sma_values,
                "current": sma_values[-1] if sma_values else None,
                "count": len(sma_values)
            }

        else:
            # Use pandas
            series = pd.Series(prices)
            sma = series.rolling(window=period).mean()

            return {
                "indicator": "SMA",
                "period": period,
                "values": sma.dropna().tolist(),
                "current": float(sma.iloc[-1]) if not pd.isna(sma.iloc[-1]) else None,
                "count": len(sma.dropna())
            }

    except Exception as e:
        return {"error": f"SMA calculation failed: {str(e)}"}


def calculate_ema(prices: List[float], period: int = 20) -> Dict[str, Any]:
    """
    Calculate Exponential Moving Average

    Args:
        prices: List of prices
        period: Period for EMA calculation

    Returns:
        Dictionary with EMA values
    """
    try:
        if TALIB_AVAILABLE:
            prices_array = np.array(prices)
            ema = talib.EMA(prices_array, timeperiod=period)

            return {
                "indicator": "EMA",
                "period": period,
                "values": ema[~np.isnan(ema)].tolist(),
                "current": float(ema[-1]) if not np.isnan(ema[-1]) else None,
                "source": "talib"
            }

        elif PANDAS_AVAILABLE:
            series = pd.Series(prices)
            ema = series.ewm(span=period, adjust=False).mean()

            return {
                "indicator": "EMA",
                "period": period,
                "values": ema.tolist(),
                "current": float(ema.iloc[-1]),
                "source": "pandas"
            }

        else:
            return {"error": "pandas or talib required for EMA calculation"}

    except Exception as e:
        return {"error": f"EMA calculation failed: {str(e)}"}


def calculate_rsi(prices: List[float], period: int = 14) -> Dict[str, Any]:
    """
    Calculate Relative Strength Index

    Args:
        prices: List of prices
        period: Period for RSI calculation

    Returns:
        Dictionary with RSI values and interpretation
    """
    try:
        if TALIB_AVAILABLE:
            prices_array = np.array(prices)
            rsi = talib.RSI(prices_array, timeperiod=period)

            current_rsi = float(rsi[-1]) if not np.isnan(rsi[-1]) else None

            # Interpret RSI
            interpretation = ""
            if current_rsi:
                if current_rsi > 70:
                    interpretation = "Overbought - Potential sell signal"
                elif current_rsi < 30:
                    interpretation = "Oversold - Potential buy signal"
                else:
                    interpretation = "Neutral"

            return {
                "indicator": "RSI",
                "period": period,
                "values": rsi[~np.isnan(rsi)].tolist(),
                "current": current_rsi,
                "interpretation": interpretation,
                "source": "talib"
            }

        else:
            return {"error": "talib required for RSI calculation. Install with: pip install TA-Lib"}

    except Exception as e:
        return {"error": f"RSI calculation failed: {str(e)}"}


def calculate_macd(
    prices: List[float],
    fast_period: int = 12,
    slow_period: int = 26,
    signal_period: int = 9
) -> Dict[str, Any]:
    """
    Calculate MACD (Moving Average Convergence Divergence)

    Args:
        prices: List of prices
        fast_period: Fast EMA period
        slow_period: Slow EMA period
        signal_period: Signal line period

    Returns:
        Dictionary with MACD values
    """
    try:
        if TALIB_AVAILABLE:
            prices_array = np.array(prices)
            macd, signal, histogram = talib.MACD(
                prices_array,
                fastperiod=fast_period,
                slowperiod=slow_period,
                signalperiod=signal_period
            )

            # Clean NaN values
            valid_indices = ~(np.isnan(macd) | np.isnan(signal) | np.isnan(histogram))

            current_macd = float(macd[-1]) if not np.isnan(macd[-1]) else None
            current_signal = float(signal[-1]) if not np.isnan(signal[-1]) else None
            current_histogram = float(histogram[-1]) if not np.isnan(histogram[-1]) else None

            # Interpret MACD
            interpretation = ""
            if current_macd and current_signal:
                if current_macd > current_signal:
                    interpretation = "Bullish - MACD above signal line"
                else:
                    interpretation = "Bearish - MACD below signal line"

            return {
                "indicator": "MACD",
                "fast_period": fast_period,
                "slow_period": slow_period,
                "signal_period": signal_period,
                "macd": macd[valid_indices].tolist(),
                "signal": signal[valid_indices].tolist(),
                "histogram": histogram[valid_indices].tolist(),
                "current_macd": current_macd,
                "current_signal": current_signal,
                "current_histogram": current_histogram,
                "interpretation": interpretation,
                "source": "talib"
            }

        else:
            return {"error": "talib required for MACD calculation"}

    except Exception as e:
        return {"error": f"MACD calculation failed: {str(e)}"}


def calculate_bollinger_bands(
    prices: List[float],
    period: int = 20,
    std_dev: float = 2.0
) -> Dict[str, Any]:
    """
    Calculate Bollinger Bands

    Args:
        prices: List of prices
        period: Period for calculation
        std_dev: Standard deviation multiplier

    Returns:
        Dictionary with Bollinger Bands
    """
    try:
        if TALIB_AVAILABLE:
            prices_array = np.array(prices)
            upper, middle, lower = talib.BBANDS(
                prices_array,
                timeperiod=period,
                nbdevup=std_dev,
                nbdevdn=std_dev,
                matype=0
            )

            current_price = prices[-1]
            current_upper = float(upper[-1]) if not np.isnan(upper[-1]) else None
            current_middle = float(middle[-1]) if not np.isnan(middle[-1]) else None
            current_lower = float(lower[-1]) if not np.isnan(lower[-1]) else None

            # Interpret position
            interpretation = ""
            if current_upper and current_lower:
                if current_price > current_upper:
                    interpretation = "Price above upper band - Potentially overbought"
                elif current_price < current_lower:
                    interpretation = "Price below lower band - Potentially oversold"
                else:
                    interpretation = "Price within bands - Normal range"

            return {
                "indicator": "Bollinger Bands",
                "period": period,
                "std_dev": std_dev,
                "upper_band": upper[~np.isnan(upper)].tolist(),
                "middle_band": middle[~np.isnan(middle)].tolist(),
                "lower_band": lower[~np.isnan(lower)].tolist(),
                "current_upper": current_upper,
                "current_middle": current_middle,
                "current_lower": current_lower,
                "current_price": current_price,
                "interpretation": interpretation,
                "source": "talib"
            }

        else:
            return {"error": "talib required for Bollinger Bands calculation"}

    except Exception as e:
        return {"error": f"Bollinger Bands calculation failed: {str(e)}"}


def detect_support_resistance(
    prices: List[float],
    window: int = 5,
    threshold: float = 0.02
) -> Dict[str, Any]:
    """
    Detect support and resistance levels

    Args:
        prices: List of prices
        window: Window size for peak detection
        threshold: Minimum price change threshold

    Returns:
        Dictionary with support and resistance levels
    """
    try:
        if not PANDAS_AVAILABLE:
            return {"error": "pandas required for support/resistance detection"}

        series = pd.Series(prices)

        # Find local maxima (resistance)
        resistance_levels = []
        for i in range(window, len(prices) - window):
            if all(prices[i] >= prices[i-j] for j in range(1, window+1)) and \
               all(prices[i] >= prices[i+j] for j in range(1, window+1)):
                resistance_levels.append(prices[i])

        # Find local minima (support)
        support_levels = []
        for i in range(window, len(prices) - window):
            if all(prices[i] <= prices[i-j] for j in range(1, window+1)) and \
               all(prices[i] <= prices[i+j] for j in range(1, window+1)):
                support_levels.append(prices[i])

        current_price = prices[-1]

        # Find nearest levels
        nearest_resistance = min(
            [r for r in resistance_levels if r > current_price],
            default=None
        )
        nearest_support = max(
            [s for s in support_levels if s < current_price],
            default=None
        )

        return {
            "current_price": current_price,
            "support_levels": sorted(set(support_levels)),
            "resistance_levels": sorted(set(resistance_levels)),
            "nearest_support": nearest_support,
            "nearest_resistance": nearest_resistance,
            "support_count": len(support_levels),
            "resistance_count": len(resistance_levels)
        }

    except Exception as e:
        return {"error": f"Support/Resistance detection failed: {str(e)}"}


def get_technical_analysis_tools() -> List[Any]:
    """
    Get list of technical analysis tools for Agno agents

    Returns:
        List of tool functions
    """
    return [
        calculate_sma,
        calculate_ema,
        calculate_rsi,
        calculate_macd,
        calculate_bollinger_bands,
        detect_support_resistance,
    ]


# Tool descriptions for LLM
TOOL_DESCRIPTIONS = {
    "calculate_sma": "Calculate Simple Moving Average to identify trend direction and smooth out price action.",
    "calculate_ema": "Calculate Exponential Moving Average which gives more weight to recent prices.",
    "calculate_rsi": "Calculate Relative Strength Index to identify overbought/oversold conditions (>70 overbought, <30 oversold).",
    "calculate_macd": "Calculate MACD indicator to identify trend changes and momentum shifts.",
    "calculate_bollinger_bands": "Calculate Bollinger Bands to measure volatility and identify potential reversal points.",
    "detect_support_resistance": "Detect support and resistance levels where price tends to reverse or consolidate.",
}
