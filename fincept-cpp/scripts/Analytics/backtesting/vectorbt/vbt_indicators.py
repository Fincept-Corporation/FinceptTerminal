"""
VBT Indicators & Signal Generation Module

Covers all VectorBT built-in indicators:
- MA (SMA/EMA with configurable window and ewm flag)
- MSTD (Moving Standard Deviation)
- BBANDS (Bollinger Bands: upper, middle, lower, bandwidth, %B)
- RSI (Relative Strength Index)
- STOCH (Stochastic Oscillator: %K, %D)
- MACD (MACD line, Signal line, Histogram)
- ATR (Average True Range)
- OBV (On-Balance Volume)

Signal Generators:
- RAND (Random entry/exit signals for benchmarking)
- STX (Stop/take-profit from close-only data)
- OHLCSTX (Stop/take-profit from OHLC data)

Custom Indicators:
- ADX (Average Directional Index)
- Donchian Channels
- Z-Score
- Momentum (ROC)
- Williams %R
- CCI (Commodity Channel Index)
- Keltner Channels

IndicatorFactory:
- from_talib: TA-Lib wrapper
- from_pandas_ta: pandas-ta wrapper
- from_ta: ta-lib wrapper
- run_combs: Combinatorial indicator runs
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, Tuple, List, Callable
from itertools import product


# ============================================================================
# VBT Native Indicators
# ============================================================================

def calculate_ma(
    vbt, close: np.ndarray, period: int, ewm: bool = False
) -> np.ndarray:
    """
    Calculate Moving Average.

    Args:
        vbt: vectorbt module (unused, kept for API compatibility)
        close: Price array
        period: MA window
        ewm: If True, use exponential weighting (EMA)

    Returns:
        MA values array
    """
    s = pd.Series(close)
    if ewm:
        return s.ewm(span=period, adjust=False).mean().values
    return s.rolling(window=period).mean().values


def calculate_mstd(
    vbt, close: np.ndarray, period: int, ewm: bool = False
) -> np.ndarray:
    """
    Calculate Moving Standard Deviation.

    Args:
        vbt: vectorbt module (unused, kept for API compatibility)
        close: Price array
        period: Window size
        ewm: Use exponential weighting

    Returns:
        MSTD values array
    """
    s = pd.Series(close)
    if ewm:
        return s.ewm(span=period, adjust=False).std().values
    return s.rolling(window=period).std().values


def calculate_bbands(
    vbt, close: np.ndarray, period: int = 20, alpha: float = 2.0
) -> Dict[str, np.ndarray]:
    """
    Calculate Bollinger Bands.

    Returns dict with: upper, middle, lower, bandwidth, percent_b
    """
    s = pd.Series(close)
    middle = s.rolling(window=period).mean().values
    std = s.rolling(window=period).std().values
    upper = middle + alpha * std
    lower = middle - alpha * std

    # Bandwidth = (upper - lower) / middle
    bandwidth = np.where(middle > 0, (upper - lower) / middle, 0)

    # %B = (close - lower) / (upper - lower)
    band_width = upper - lower
    percent_b = np.where(band_width > 0, (close - lower) / band_width, 0.5)

    return {
        'upper': upper,
        'middle': middle,
        'lower': lower,
        'bandwidth': bandwidth,
        'percent_b': percent_b,
    }


def calculate_rsi(
    vbt, close: np.ndarray, period: int = 14
) -> np.ndarray:
    """Calculate RSI (Relative Strength Index)."""
    s = pd.Series(close)
    delta = s.diff()
    gain = delta.where(delta > 0, 0.0)
    loss = (-delta).where(delta < 0, 0.0)
    avg_gain = gain.ewm(alpha=1.0 / period, min_periods=period, adjust=False).mean()
    avg_loss = loss.ewm(alpha=1.0 / period, min_periods=period, adjust=False).mean()
    rs = avg_gain / avg_loss.replace(0, np.nan)
    rsi = 100.0 - (100.0 / (1.0 + rs))
    return rsi.fillna(50.0).values


def calculate_stoch(
    vbt, high: np.ndarray, low: np.ndarray, close: np.ndarray,
    k_period: int = 14, d_period: int = 3
) -> Dict[str, np.ndarray]:
    """
    Calculate Stochastic Oscillator.

    Returns dict with: k (fast), d (slow)
    """
    h = pd.Series(high)
    l = pd.Series(low)
    c = pd.Series(close)
    lowest_low = l.rolling(window=k_period).min()
    highest_high = h.rolling(window=k_period).max()
    denom = highest_high - lowest_low
    denom = denom.replace(0, np.nan)
    percent_k = ((c - lowest_low) / denom * 100).fillna(50.0)
    percent_d = percent_k.rolling(window=d_period).mean().fillna(50.0)
    return {
        'k': percent_k.values,
        'd': percent_d.values,
    }


def calculate_macd(
    vbt, close: np.ndarray,
    fast_period: int = 12, slow_period: int = 26, signal_period: int = 9
) -> Dict[str, np.ndarray]:
    """
    Calculate MACD.

    Returns dict with: macd, signal, histogram
    """
    s = pd.Series(close)
    fast_ema = s.ewm(span=fast_period, adjust=False).mean()
    slow_ema = s.ewm(span=slow_period, adjust=False).mean()
    macd_line = fast_ema - slow_ema
    signal_line = macd_line.ewm(span=signal_period, adjust=False).mean()
    macd_vals = macd_line.values
    signal_vals = signal_line.values
    histogram = macd_vals - signal_vals

    return {
        'macd': macd_vals,
        'signal': signal_vals,
        'histogram': histogram,
    }


def calculate_atr(
    vbt, high: np.ndarray, low: np.ndarray, close: np.ndarray,
    period: int = 14
) -> np.ndarray:
    """
    Calculate Average True Range.

    Note: If only close available, caller should approximate high/low from close.
    """
    h = pd.Series(high)
    l = pd.Series(low)
    c = pd.Series(close)
    prev_close = c.shift(1)
    tr = pd.concat([
        h - l,
        (h - prev_close).abs(),
        (l - prev_close).abs(),
    ], axis=1).max(axis=1)
    return tr.ewm(span=period, adjust=False).mean().values


def calculate_obv(
    vbt, close: np.ndarray, volume: np.ndarray
) -> np.ndarray:
    """
    Calculate On-Balance Volume.

    Args:
        vbt: vectorbt module (unused, kept for API compatibility)
        close: Price array
        volume: Volume array

    Returns:
        OBV values array
    """
    direction = np.sign(np.diff(close, prepend=close[0]))
    return np.cumsum(direction * volume)


# ============================================================================
# Custom Indicators (not in VBT natively)
# ============================================================================

def calculate_adx(
    close: np.ndarray, high: Optional[np.ndarray] = None,
    low: Optional[np.ndarray] = None, period: int = 14
) -> Dict[str, np.ndarray]:
    """
    Calculate ADX (Average Directional Index).

    If high/low not available, approximates from close.

    Returns dict with: adx, plus_di, minus_di
    """
    n = len(close)
    if high is None:
        high = close * 1.005
    if low is None:
        low = close * 0.995

    # True Range
    tr = np.zeros(n)
    for i in range(1, n):
        tr[i] = max(
            high[i] - low[i],
            abs(high[i] - close[i - 1]),
            abs(low[i] - close[i - 1])
        )

    # Directional Movement
    plus_dm = np.zeros(n)
    minus_dm = np.zeros(n)
    for i in range(1, n):
        up_move = high[i] - high[i - 1]
        down_move = low[i - 1] - low[i]
        if up_move > down_move and up_move > 0:
            plus_dm[i] = up_move
        if down_move > up_move and down_move > 0:
            minus_dm[i] = down_move

    # Smooth with EMA
    atr = pd.Series(tr).ewm(span=period, adjust=False).mean().values
    plus_di_raw = pd.Series(plus_dm).ewm(span=period, adjust=False).mean().values
    minus_di_raw = pd.Series(minus_dm).ewm(span=period, adjust=False).mean().values

    # Directional Indicators
    plus_di = np.where(atr > 0, (plus_di_raw / atr) * 100, 0)
    minus_di = np.where(atr > 0, (minus_di_raw / atr) * 100, 0)

    # DX and ADX
    di_sum = plus_di + minus_di
    dx = np.where(di_sum > 0, np.abs(plus_di - minus_di) / di_sum * 100, 0)
    adx = pd.Series(dx).ewm(span=period, adjust=False).mean().values

    return {
        'adx': adx,
        'plus_di': plus_di,
        'minus_di': minus_di,
    }


def calculate_donchian(
    close: np.ndarray, period: int = 20
) -> Dict[str, np.ndarray]:
    """
    Calculate Donchian Channels (breakout indicator).

    Returns dict with: upper, lower, middle
    """
    upper = pd.Series(close).rolling(period).max().values
    lower = pd.Series(close).rolling(period).min().values
    middle = (upper + lower) / 2

    return {
        'upper': upper,
        'lower': lower,
        'middle': middle,
    }


def calculate_zscore(
    close: np.ndarray, period: int = 20
) -> np.ndarray:
    """
    Calculate Z-Score (number of standard deviations from mean).

    Z = (close - SMA) / STD
    """
    sma = pd.Series(close).rolling(period).mean().values
    std = pd.Series(close).rolling(period).std().values
    std = np.where(std < 1e-10, 1.0, std)
    return (close - sma) / std


def calculate_momentum(
    close: np.ndarray, lookback: int = 20
) -> np.ndarray:
    """
    Calculate Momentum (Rate of Change).

    ROC = (close - close[n]) / close[n]
    """
    return pd.Series(close).pct_change(periods=lookback).values


def calculate_williams_r(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    period: int = 14
) -> np.ndarray:
    """
    Calculate Williams %R.

    %R = (Highest High - Close) / (Highest High - Lowest Low) * -100
    """
    highest = pd.Series(high).rolling(period).max().values
    lowest = pd.Series(low).rolling(period).min().values
    denom = highest - lowest
    denom = np.where(denom < 1e-10, 1.0, denom)
    return ((highest - close) / denom) * -100


def calculate_cci(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    period: int = 20
) -> np.ndarray:
    """
    Calculate Commodity Channel Index.

    CCI = (Typical Price - SMA(TP)) / (0.015 * Mean Deviation)
    """
    tp = (high + low + close) / 3
    sma_tp = pd.Series(tp).rolling(period).mean().values
    mean_dev = pd.Series(tp).rolling(period).apply(
        lambda x: np.mean(np.abs(x - x.mean())), raw=True
    ).values
    mean_dev = np.where(mean_dev < 1e-10, 1.0, mean_dev)
    return (tp - sma_tp) / (0.015 * mean_dev)


def calculate_keltner(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    ema_period: int = 20, atr_period: int = 10, multiplier: float = 2.0
) -> Dict[str, np.ndarray]:
    """
    Calculate Keltner Channels.

    Middle = EMA(close)
    Upper = EMA + multiplier * ATR
    Lower = EMA - multiplier * ATR
    """
    ema = pd.Series(close).ewm(span=ema_period, adjust=False).mean().values

    # ATR calculation
    tr = np.zeros(len(close))
    for i in range(1, len(close)):
        tr[i] = max(
            high[i] - low[i],
            abs(high[i] - close[i - 1]),
            abs(low[i] - close[i - 1])
        )
    atr = pd.Series(tr).ewm(span=atr_period, adjust=False).mean().values

    return {
        'upper': ema + multiplier * atr,
        'middle': ema,
        'lower': ema - multiplier * atr,
    }


def calculate_vwap(
    high: np.ndarray, low: np.ndarray, close: np.ndarray,
    volume: np.ndarray
) -> np.ndarray:
    """
    Calculate Volume Weighted Average Price (VWAP).

    VWAP = cumsum(TP * Volume) / cumsum(Volume)
    """
    tp = (high + low + close) / 3
    cumvol = np.cumsum(volume)
    cumvol = np.where(cumvol > 0, cumvol, 1)
    return np.cumsum(tp * volume) / cumvol


# ============================================================================
# Signal Generation Helpers
# ============================================================================

def generate_crossover_signals(
    fast: np.ndarray, slow: np.ndarray, index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate entry/exit signals from indicator crossover.

    Entry: fast crosses above slow (was below, now above)
    Exit: fast crosses below slow (was above, now below)
    """
    above = fast > slow
    # Detect edges: cross-up where previous bar was not above and current is
    cross_up = np.zeros(len(above), dtype=bool)
    cross_down = np.zeros(len(above), dtype=bool)
    for i in range(1, len(above)):
        if above[i] and not above[i - 1]:
            cross_up[i] = True
        elif not above[i] and above[i - 1]:
            cross_down[i] = True
    entries = pd.Series(cross_up, index=index)
    exits = pd.Series(cross_down, index=index)
    return entries, exits


def generate_threshold_signals(
    indicator: np.ndarray, lower: float, upper: float,
    index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate entry/exit signals from threshold crossings.

    Entry: indicator crosses below lower threshold
    Exit: indicator crosses above upper threshold
    """
    below_lower = indicator < lower
    above_upper = indicator > upper
    cross_below = np.zeros(len(indicator), dtype=bool)
    cross_above = np.zeros(len(indicator), dtype=bool)
    for i in range(1, len(indicator)):
        if below_lower[i] and not below_lower[i - 1]:
            cross_below[i] = True
        if above_upper[i] and not above_upper[i - 1]:
            cross_above[i] = True
    entries = pd.Series(cross_below, index=index)
    exits = pd.Series(cross_above, index=index)
    return entries, exits


def generate_breakout_signals(
    close: np.ndarray, upper: np.ndarray, lower: np.ndarray,
    index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate breakout signals (Donchian-style).

    Entry: close breaks above upper channel
    Exit: close breaks below lower channel
    """
    entries = pd.Series(close > np.roll(upper, 1), index=index)
    exits = pd.Series(close < np.roll(lower, 1), index=index)
    # First bar has no previous channel value
    entries.iloc[0] = False
    exits.iloc[0] = False
    return entries, exits


def generate_mean_reversion_signals(
    zscore: np.ndarray, z_entry: float, z_exit: float,
    index: pd.DatetimeIndex
) -> Tuple[pd.Series, pd.Series]:
    """
    Generate mean-reversion signals from Z-score.

    Entry: Z-score crosses below -z_entry (oversold)
    Exit: Z-score crosses above z_exit (mean reversion complete)
    """
    below_entry = zscore < -z_entry
    above_exit = zscore > z_exit
    cross_below = np.zeros(len(zscore), dtype=bool)
    cross_above = np.zeros(len(zscore), dtype=bool)
    for i in range(1, len(zscore)):
        if below_entry[i] and not below_entry[i - 1]:
            cross_below[i] = True
        if above_exit[i] and not above_exit[i - 1]:
            cross_above[i] = True
    entries = pd.Series(cross_below, index=index)
    exits = pd.Series(cross_above, index=index)
    return entries, exits


def apply_signal_filter(
    entries: pd.Series, exits: pd.Series,
    filter_indicator: np.ndarray, filter_threshold: float,
    filter_type: str = 'above'
) -> Tuple[pd.Series, pd.Series]:
    """
    Apply a filter to entry signals (e.g., only trade when ADX > 25).

    Args:
        entries: Original entry signals
        exits: Original exit signals
        filter_indicator: Filter indicator values
        filter_threshold: Threshold for filter
        filter_type: 'above' or 'below'

    Returns:
        Filtered entries and exits
    """
    if filter_type == 'above':
        mask = pd.Series(filter_indicator > filter_threshold, index=entries.index)
    else:
        mask = pd.Series(filter_indicator < filter_threshold, index=entries.index)

    filtered_entries = entries & mask
    return filtered_entries, exits


# ============================================================================
# Indicator Catalog (for frontend display)
# ============================================================================

def get_indicator_catalog() -> Dict[str, Any]:
    """
    Return the full catalog of available indicators.

    Used by the frontend to populate indicator selection dropdowns.
    """
    return {
        'categories': [
            {
                'name': 'Trend',
                'indicators': [
                    {'id': 'sma', 'name': 'SMA', 'params': ['period'], 'description': 'Simple Moving Average'},
                    {'id': 'ema', 'name': 'EMA', 'params': ['period'], 'description': 'Exponential Moving Average'},
                    {'id': 'macd', 'name': 'MACD', 'params': ['fastPeriod', 'slowPeriod', 'signalPeriod'], 'description': 'MACD Line, Signal, Histogram'},
                    {'id': 'adx', 'name': 'ADX', 'params': ['period'], 'description': 'Average Directional Index'},
                    {'id': 'keltner', 'name': 'Keltner', 'params': ['emaPeriod', 'atrPeriod', 'multiplier'], 'description': 'Keltner Channels'},
                    {'id': 'vwap', 'name': 'VWAP', 'params': [], 'description': 'Volume Weighted Average Price'},
                ]
            },
            {
                'name': 'Momentum',
                'indicators': [
                    {'id': 'rsi', 'name': 'RSI', 'params': ['period'], 'description': 'Relative Strength Index'},
                    {'id': 'stochastic', 'name': 'Stochastic', 'params': ['kPeriod', 'dPeriod'], 'description': 'Stochastic Oscillator (%K, %D)'},
                    {'id': 'momentum', 'name': 'Momentum', 'params': ['lookback'], 'description': 'Rate of Change'},
                    {'id': 'williams_r', 'name': "Williams %R", 'params': ['period'], 'description': 'Williams Percent Range'},
                    {'id': 'cci', 'name': 'CCI', 'params': ['period'], 'description': 'Commodity Channel Index'},
                ]
            },
            {
                'name': 'Volatility',
                'indicators': [
                    {'id': 'bbands', 'name': 'Bollinger Bands', 'params': ['period', 'stdDev'], 'description': 'Bollinger Bands (Upper, Middle, Lower)'},
                    {'id': 'atr', 'name': 'ATR', 'params': ['period'], 'description': 'Average True Range'},
                    {'id': 'mstd', 'name': 'MSTD', 'params': ['period'], 'description': 'Moving Standard Deviation'},
                    {'id': 'donchian', 'name': 'Donchian', 'params': ['period'], 'description': 'Donchian Channels (Breakout)'},
                ]
            },
            {
                'name': 'Volume',
                'indicators': [
                    {'id': 'obv', 'name': 'OBV', 'params': [], 'description': 'On-Balance Volume'},
                ]
            },
            {
                'name': 'Statistical',
                'indicators': [
                    {'id': 'zscore', 'name': 'Z-Score', 'params': ['period'], 'description': 'Standard deviations from mean'},
                ]
            },
        ]
    }


# ============================================================================
# Indicator Factory (mimics vbt.IndicatorFactory)
# ============================================================================

class IndicatorFactory:
    """
    Factory for creating custom indicators and wrapping external TA libraries.

    Mimics vectorbt's IndicatorFactory with:
    - from_custom_func: Create indicator from a custom function
    - from_apply_func: Create indicator from an apply function
    - from_talib: Wrap TA-Lib indicators
    - from_pandas_ta: Wrap pandas-ta indicators
    - from_ta: Wrap ta library indicators
    - run_combs: Run combinatorial parameter sweeps
    """

    def __init__(
        self,
        input_names: List[str] = None,
        param_names: List[str] = None,
        output_names: List[str] = None,
        short_name: str = 'custom',
    ):
        self._input_names = input_names or ['close']
        self._param_names = param_names or []
        self._output_names = output_names or ['output']
        self._short_name = short_name
        self._custom_func = None
        self._apply_func = None

    @property
    def short_name(self) -> str:
        return self._short_name

    @property
    def input_names(self) -> List[str]:
        return self._input_names

    @property
    def param_names(self) -> List[str]:
        return self._param_names

    @property
    def output_names(self) -> List[str]:
        return self._output_names

    @classmethod
    def from_custom_func(
        cls,
        custom_func: Callable,
        input_names: List[str] = None,
        param_names: List[str] = None,
        output_names: List[str] = None,
        short_name: str = 'custom',
    ) -> 'IndicatorFactory':
        """
        Create indicator from a custom function.

        custom_func(*inputs, *params) -> tuple of output arrays
        """
        factory = cls(input_names, param_names, output_names, short_name)
        factory._custom_func = custom_func
        return factory

    @classmethod
    def from_apply_func(
        cls,
        apply_func: Callable,
        input_names: List[str] = None,
        param_names: List[str] = None,
        output_names: List[str] = None,
        short_name: str = 'custom',
    ) -> 'IndicatorFactory':
        """
        Create indicator from an apply function.

        apply_func(close, *params) -> output array or tuple
        """
        factory = cls(input_names, param_names, output_names, short_name)
        factory._apply_func = apply_func
        return factory

    def run(self, *inputs, **params) -> Dict[str, np.ndarray]:
        """Run indicator with given inputs and parameters."""
        func = self._custom_func or self._apply_func
        if func is None:
            raise ValueError("No function defined. Use from_custom_func or from_apply_func.")

        param_vals = [params.get(p, None) for p in self._param_names]
        print(f"[IndicatorFactory.run] param_names={self._param_names}, params={params}, param_vals={param_vals}")
        result = func(*inputs, *param_vals)

        if isinstance(result, tuple):
            return {name: arr for name, arr in zip(self._output_names, result)}
        elif isinstance(result, dict):
            return result
        else:
            return {self._output_names[0]: result}

    def run_combs(
        self,
        *inputs,
        param_ranges: Dict[str, List] = None,
        **kwargs,
    ) -> List[Dict[str, Any]]:
        """
        Run indicator across all parameter combinations.

        Args:
            *inputs: Input arrays
            param_ranges: Dict of param_name -> list of values
                e.g. {'period': [10, 20, 30], 'alpha': [1.5, 2.0, 2.5]}

        Returns:
            List of dicts with 'params' and 'output' keys
        """
        if param_ranges is None:
            return [{'params': {}, 'output': self.run(*inputs)}]

        names = list(param_ranges.keys())
        values = list(param_ranges.values())
        results = []

        for combo in product(*values):
            params = dict(zip(names, combo))
            try:
                output = self.run(*inputs, **params)
                results.append({'params': params, 'output': output})
            except Exception as e:
                print(f"[IndicatorFactory.run_combs] Exception for params {params}: {e}")
                import traceback
                traceback.print_exc()
                continue

        return results

    # ------------------------------------------------------------------
    # TA Library Wrappers
    # ------------------------------------------------------------------

    @staticmethod
    def from_talib(func_name: str, **kwargs) -> 'IndicatorFactory':
        """
        Wrap a TA-Lib indicator function.

        Requires talib to be installed.
        Usage: factory = IndicatorFactory.from_talib('RSI')
        """
        try:
            import talib
            func = getattr(talib, func_name.upper())
        except (ImportError, AttributeError) as e:
            raise ImportError(f"TA-Lib not available or function '{func_name}' not found: {e}")

        def wrapper(*inputs, **params):
            return func(*inputs, **params)

        return IndicatorFactory.from_custom_func(
            wrapper,
            short_name=f'talib_{func_name}',
            output_names=[func_name.lower()],
            **kwargs,
        )

    @staticmethod
    def from_pandas_ta(func_name: str, **kwargs) -> 'IndicatorFactory':
        """
        Wrap a pandas-ta indicator.

        Requires pandas_ta to be installed.
        Usage: factory = IndicatorFactory.from_pandas_ta('rsi')
        """
        try:
            import pandas_ta
        except ImportError:
            raise ImportError("pandas_ta is not installed")

        def wrapper(close, **params):
            s = pd.Series(close) if not isinstance(close, pd.Series) else close
            result = getattr(pandas_ta, func_name)(s, **params)
            if isinstance(result, pd.DataFrame):
                return tuple(result[col].values for col in result.columns)
            return result.values if isinstance(result, pd.Series) else result

        return IndicatorFactory.from_apply_func(
            wrapper,
            short_name=f'pta_{func_name}',
            output_names=[func_name],
            **kwargs,
        )

    @staticmethod
    def from_ta(indicator_class, **kwargs) -> 'IndicatorFactory':
        """
        Wrap a ta library indicator class.

        Requires ta to be installed.
        Usage: factory = IndicatorFactory.from_ta(ta.momentum.RSIIndicator)
        """
        def wrapper(close, **params):
            s = pd.Series(close) if not isinstance(close, pd.Series) else close
            df = pd.DataFrame({'Close': s})
            ind = indicator_class(close=s, **params)
            # Get all public methods that return series
            outputs = {}
            for attr in dir(ind):
                if not attr.startswith('_') and callable(getattr(ind, attr)):
                    try:
                        val = getattr(ind, attr)()
                        if isinstance(val, pd.Series):
                            outputs[attr] = val.values
                    except Exception:
                        pass
            if outputs:
                return outputs
            return np.zeros(len(close))

        return IndicatorFactory.from_apply_func(
            wrapper,
            short_name=kwargs.get('short_name', 'ta_indicator'),
            **kwargs,
        )

    @staticmethod
    def get_talib_indicators() -> List[str]:
        """List all available TA-Lib functions."""
        try:
            import talib
            return talib.get_function_groups()
        except ImportError:
            return []

    @staticmethod
    def get_pandas_ta_indicators() -> List[str]:
        """List all available pandas-ta indicators."""
        try:
            import pandas_ta
            return list(pandas_ta.Category.keys()) if hasattr(pandas_ta, 'Category') else []
        except ImportError:
            return []
