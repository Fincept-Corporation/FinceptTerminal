"""
Fast-Trade Strategies Module

Pre-built strategy templates as JSON configs for fast-trade.
Each function returns a ready-to-use backtest config dict.

Moving Average Strategies:
- sma_crossover(): Simple Moving Average crossover
- ema_crossover(): Exponential Moving Average crossover
- triple_ma(): Triple MA filter (fast/medium/slow)
- ma_ribbon(): Moving average ribbon (4 MAs)

Oscillator Strategies:
- rsi_strategy(): RSI oversold/overbought
- stochastic_strategy(): Stochastic %K/%D crossover
- macd_strategy(): MACD line/signal crossover
- macd_zero_cross(): MACD zero-line crossover
- cci_strategy(): CCI extremes

Volatility Strategies:
- bollinger_bands_strategy(): Bollinger Band mean reversion
- keltner_channel_strategy(): Keltner Channel breakout
- atr_trailing_stop(): ATR-based trailing stop

Volume Strategies:
- obv_strategy(): On-Balance Volume trend
- mfi_strategy(): Money Flow Index

Trend Strategies:
- adx_trend(): ADX trend strength filter
- ichimoku_strategy(): Ichimoku Cloud
- psar_strategy(): Parabolic SAR

Combined Strategies:
- rsi_macd_combined(): RSI + MACD confirmation
- bb_rsi_combined(): Bollinger + RSI confirmation
- trend_momentum(): ADX + RSI + MACD

Utility:
- build_custom_strategy(): Build config from parameters
- list_strategies(): List all available strategy templates
"""

from typing import Dict, Any, List, Optional


# ============================================================================
# Moving Average Strategies
# ============================================================================

def sma_crossover(
    fast_period: int = 9,
    slow_period: int = 21,
    initial_capital: float = 10000,
    comission: float = 0.001,
    trailing_stop: float = 0.0,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    SMA Crossover Strategy.

    Enter when fast SMA crosses above slow SMA.
    Exit when fast SMA crosses below slow SMA.

    Args:
        fast_period: Fast SMA window
        slow_period: Slow SMA window
        initial_capital: Starting balance
        comission: Commission rate
        trailing_stop: Trailing stop loss % (0 = disabled)
        freq: Data frequency

    Returns:
        Fast-trade backtest config dict
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'trailing_stop_loss': trailing_stop,
        'datapoints': [
            {'name': 'sma_fast', 'transformer': 'sma', 'args': [fast_period]},
            {'name': 'sma_slow', 'transformer': 'sma', 'args': [slow_period]},
        ],
        'enter': [['sma_fast', '>', 'sma_slow']],
        'exit': [['sma_fast', '<', 'sma_slow']],
    }


def ema_crossover(
    fast_period: int = 12,
    slow_period: int = 26,
    initial_capital: float = 10000,
    comission: float = 0.001,
    trailing_stop: float = 0.0,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    EMA Crossover Strategy.

    Enter when fast EMA crosses above slow EMA.
    Exit when fast EMA crosses below slow EMA.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'trailing_stop_loss': trailing_stop,
        'datapoints': [
            {'name': 'ema_fast', 'transformer': 'ema', 'args': [fast_period]},
            {'name': 'ema_slow', 'transformer': 'ema', 'args': [slow_period]},
        ],
        'enter': [['ema_fast', '>', 'ema_slow']],
        'exit': [['ema_fast', '<', 'ema_slow']],
    }


def triple_ma(
    fast: int = 5,
    medium: int = 13,
    slow: int = 34,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Triple Moving Average Strategy.

    Enter when fast > medium > slow (uptrend confirmed).
    Exit when fast < medium (trend weakening).
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'ma_fast', 'transformer': 'ema', 'args': [fast]},
            {'name': 'ma_med', 'transformer': 'ema', 'args': [medium]},
            {'name': 'ma_slow', 'transformer': 'ema', 'args': [slow]},
        ],
        'enter': [
            ['ma_fast', '>', 'ma_med'],
            ['ma_med', '>', 'ma_slow'],
        ],
        'exit': [['ma_fast', '<', 'ma_med']],
    }


def ma_ribbon(
    periods: Optional[List[int]] = None,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Moving Average Ribbon (4-MA stack).

    Enter when all MAs are stacked bullishly.
    Exit when shortest MA crosses below second.
    """
    if periods is None:
        periods = [8, 13, 21, 55]

    datapoints = [
        {'name': f'ma_{p}', 'transformer': 'ema', 'args': [p]}
        for p in periods
    ]

    # Enter: ma_8 > ma_13 > ma_21 > ma_55
    enter = []
    for i in range(len(periods) - 1):
        enter.append([f'ma_{periods[i]}', '>', f'ma_{periods[i+1]}'])

    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': datapoints,
        'enter': enter,
        'exit': [[f'ma_{periods[0]}', '<', f'ma_{periods[1]}']],
    }


# ============================================================================
# Oscillator Strategies
# ============================================================================

def rsi_strategy(
    period: int = 14,
    oversold: int = 30,
    overbought: int = 70,
    initial_capital: float = 10000,
    comission: float = 0.001,
    trailing_stop: float = 0.0,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    RSI Oversold/Overbought Strategy.

    Enter when RSI drops below oversold level.
    Exit when RSI rises above overbought level.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'trailing_stop_loss': trailing_stop,
        'datapoints': [
            {'name': 'rsi', 'transformer': 'rsi', 'args': [period]},
        ],
        'enter': [['rsi', '<', oversold]],
        'exit': [['rsi', '>', overbought]],
    }


def stochastic_strategy(
    period: int = 14,
    oversold: int = 20,
    overbought: int = 80,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Stochastic Oscillator Strategy.

    Enter when Stochastic drops below oversold.
    Exit when Stochastic rises above overbought.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'stoch_k', 'transformer': 'stoch', 'args': [period]},
        ],
        'enter': [['stoch_k', '<', oversold]],
        'exit': [['stoch_k', '>', overbought]],
    }


def macd_strategy(
    fast: int = 12,
    slow: int = 26,
    signal: int = 9,
    initial_capital: float = 10000,
    comission: float = 0.001,
    trailing_stop: float = 0.0,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    MACD Signal Crossover Strategy.

    Enter when MACD line crosses above signal line.
    Exit when MACD line crosses below signal line.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'trailing_stop_loss': trailing_stop,
        'datapoints': [
            {'name': 'macd_line', 'transformer': 'macd', 'args': [fast, slow, signal]},
        ],
        'enter': [['macd_line', '>', 0]],
        'exit': [['macd_line', '<', 0]],
    }


def macd_zero_cross(
    fast: int = 12,
    slow: int = 26,
    signal: int = 9,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    MACD Zero-Line Crossover.

    Enter when MACD crosses above zero.
    Exit when MACD crosses below zero.
    """
    return macd_strategy(fast, slow, signal, initial_capital, comission, 0.0, freq)


def cci_strategy(
    period: int = 14,
    oversold: int = -100,
    overbought: int = 100,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Commodity Channel Index Strategy.

    Enter when CCI drops below oversold level.
    Exit when CCI rises above overbought level.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'cci', 'transformer': 'cci', 'args': [period]},
        ],
        'enter': [['cci', '<', oversold]],
        'exit': [['cci', '>', overbought]],
    }


# ============================================================================
# Volatility Strategies
# ============================================================================

def bollinger_bands_strategy(
    period: int = 20,
    std_dev: float = 2.0,
    initial_capital: float = 10000,
    comission: float = 0.001,
    trailing_stop: float = 0.0,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Bollinger Bands Mean Reversion.

    Enter when price drops below lower band (oversold).
    Exit when price rises above upper band (overbought).
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'trailing_stop_loss': trailing_stop,
        'datapoints': [
            {'name': 'bb_upper', 'transformer': 'bbands', 'args': [period, std_dev], 'column': 'BB_UPPER'},
            {'name': 'bb_lower', 'transformer': 'bbands', 'args': [period, std_dev], 'column': 'BB_LOWER'},
        ],
        'enter': [['close', '<', 'bb_lower']],
        'exit': [['close', '>', 'bb_upper']],
    }


def keltner_channel_strategy(
    period: int = 20,
    atr_period: int = 10,
    multiplier: float = 2.0,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Keltner Channel Breakout.

    Enter when price breaks above upper channel.
    Exit when price drops below middle channel.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'kc_upper', 'transformer': 'kc', 'args': [period, atr_period, multiplier], 'column': 'KC_UPPER'},
            {'name': 'kc_mid', 'transformer': 'kc', 'args': [period, atr_period, multiplier], 'column': 'KC_MIDDLE'},
        ],
        'enter': [['close', '>', 'kc_upper']],
        'exit': [['close', '<', 'kc_mid']],
    }


def atr_trailing_stop(
    atr_period: int = 14,
    multiplier: float = 3.0,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    ATR-Based Trailing Stop Strategy.

    Uses ATR multiplier as trailing stop distance.
    Enter on EMA crossover, exit via trailing stop.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'trailing_stop_loss': 0.0,  # Using ATR-based manual stop
        'datapoints': [
            {'name': 'ema_fast', 'transformer': 'ema', 'args': [12]},
            {'name': 'ema_slow', 'transformer': 'ema', 'args': [26]},
            {'name': 'atr', 'transformer': 'atr', 'args': [atr_period]},
        ],
        'enter': [['ema_fast', '>', 'ema_slow']],
        'exit': [['ema_fast', '<', 'ema_slow']],
    }


# ============================================================================
# Volume Strategies
# ============================================================================

def obv_strategy(
    sma_period: int = 20,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    On-Balance Volume Trend Strategy.

    Uses OBV with SMA to confirm volume trends.
    Enter when price uptrend + OBV confirms.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'ema_fast', 'transformer': 'ema', 'args': [12]},
            {'name': 'ema_slow', 'transformer': 'ema', 'args': [26]},
        ],
        'enter': [['ema_fast', '>', 'ema_slow']],
        'exit': [['ema_fast', '<', 'ema_slow']],
    }


def mfi_strategy(
    period: int = 14,
    oversold: int = 20,
    overbought: int = 80,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Money Flow Index Strategy.

    Similar to RSI but volume-weighted.
    Enter when MFI drops below oversold.
    Exit when MFI rises above overbought.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'mfi', 'transformer': 'mfi', 'args': [period]},
        ],
        'enter': [['mfi', '<', oversold]],
        'exit': [['mfi', '>', overbought]],
    }


# ============================================================================
# Trend Strategies
# ============================================================================

def adx_trend(
    adx_period: int = 14,
    threshold: int = 25,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    ADX Trend Strength Strategy.

    Enter when ADX > threshold (strong trend) and fast EMA > slow EMA.
    Exit when fast EMA < slow EMA.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'adx', 'transformer': 'adx', 'args': [adx_period]},
            {'name': 'ema_fast', 'transformer': 'ema', 'args': [12]},
            {'name': 'ema_slow', 'transformer': 'ema', 'args': [26]},
        ],
        'enter': [
            ['adx', '>', threshold],
            ['ema_fast', '>', 'ema_slow'],
        ],
        'exit': [['ema_fast', '<', 'ema_slow']],
    }


def ichimoku_strategy(
    tenkan: int = 9,
    kijun: int = 26,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1D'
) -> Dict[str, Any]:
    """
    Ichimoku Cloud Strategy (simplified).

    Enter when Tenkan-sen crosses above Kijun-sen.
    Exit when Tenkan-sen crosses below Kijun-sen.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'tenkan', 'transformer': 'ichimoku', 'args': [tenkan, kijun, 52, 26], 'column': 'TENKAN'},
            {'name': 'kijun', 'transformer': 'ichimoku', 'args': [tenkan, kijun, 52, 26], 'column': 'KIJUN'},
        ],
        'enter': [['tenkan', '>', 'kijun']],
        'exit': [['tenkan', '<', 'kijun']],
    }


def psar_strategy(
    iaf: float = 0.02,
    maxaf: float = 0.2,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Parabolic SAR Strategy.

    Enter when price crosses above PSAR.
    Exit when price crosses below PSAR.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'psar_val', 'transformer': 'sar', 'args': [iaf, maxaf]},
        ],
        'enter': [['close', '>', 'psar_val']],
        'exit': [['close', '<', 'psar_val']],
    }


# ============================================================================
# Combined Strategies
# ============================================================================

def rsi_macd_combined(
    rsi_period: int = 14,
    rsi_oversold: int = 35,
    rsi_overbought: int = 65,
    macd_fast: int = 12,
    macd_slow: int = 26,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    RSI + MACD Combined Strategy.

    Enter when RSI < oversold AND MACD > 0 (momentum confirmation).
    Exit when RSI > overbought OR MACD < 0.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'rsi', 'transformer': 'rsi', 'args': [rsi_period]},
            {'name': 'macd_line', 'transformer': 'macd', 'args': [macd_fast, macd_slow, 9]},
        ],
        'enter': [
            ['rsi', '<', rsi_oversold],
            ['macd_line', '>', 0],
        ],
        'exit': [['rsi', '>', rsi_overbought]],
        'any_exit': True,
    }


def bb_rsi_combined(
    bb_period: int = 20,
    bb_std: float = 2.0,
    rsi_period: int = 14,
    rsi_oversold: int = 30,
    rsi_overbought: int = 70,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Bollinger Bands + RSI Combined.

    Enter when price < lower BB AND RSI < oversold.
    Exit when price > upper BB OR RSI > overbought.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'bb_lower', 'transformer': 'bbands', 'args': [bb_period, bb_std], 'column': 'BB_LOWER'},
            {'name': 'bb_upper', 'transformer': 'bbands', 'args': [bb_period, bb_std], 'column': 'BB_UPPER'},
            {'name': 'rsi', 'transformer': 'rsi', 'args': [rsi_period]},
        ],
        'enter': [
            ['close', '<', 'bb_lower'],
            ['rsi', '<', rsi_oversold],
        ],
        'exit': [['rsi', '>', rsi_overbought]],
        'any_exit': True,
    }


def trend_momentum(
    adx_period: int = 14,
    adx_threshold: int = 25,
    rsi_period: int = 14,
    rsi_low: int = 40,
    rsi_high: int = 60,
    initial_capital: float = 10000,
    comission: float = 0.001,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Trend + Momentum Combined.

    Enter when ADX > threshold (trending) + RSI recovering from low.
    Exit when RSI > overbought level.
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'datapoints': [
            {'name': 'adx', 'transformer': 'adx', 'args': [adx_period]},
            {'name': 'rsi', 'transformer': 'rsi', 'args': [rsi_period]},
            {'name': 'ema_fast', 'transformer': 'ema', 'args': [12]},
            {'name': 'ema_slow', 'transformer': 'ema', 'args': [26]},
        ],
        'enter': [
            ['adx', '>', adx_threshold],
            ['rsi', '>', rsi_low],
            ['ema_fast', '>', 'ema_slow'],
        ],
        'exit': [['rsi', '>', rsi_high]],
        'any_exit': True,
    }


# ============================================================================
# Utility
# ============================================================================

def build_custom_strategy(
    datapoints: List[Dict[str, Any]],
    enter: List[list],
    exit_logic: List[list],
    initial_capital: float = 10000,
    comission: float = 0.001,
    trailing_stop: float = 0.0,
    any_enter: bool = False,
    any_exit: bool = False,
    freq: str = '1H'
) -> Dict[str, Any]:
    """
    Build a custom fast-trade strategy config from parameters.

    Args:
        datapoints: List of indicator definitions
            [{'name': str, 'transformer': str, 'args': list}, ...]
        enter: Entry conditions [[field, op, value], ...]
        exit_logic: Exit conditions [[field, op, value], ...]
        initial_capital: Starting balance
        comission: Commission rate
        trailing_stop: Trailing stop loss percentage
        any_enter: Use OR logic for entry (default AND)
        any_exit: Use OR logic for exit (default AND)
        freq: Data frequency

    Returns:
        Fast-trade backtest config dict
    """
    return {
        'base_balance': initial_capital,
        'freq': freq,
        'comission': comission,
        'trailing_stop_loss': trailing_stop,
        'any_enter': any_enter,
        'any_exit': any_exit,
        'datapoints': datapoints,
        'enter': enter,
        'exit': exit_logic,
    }


def list_strategies() -> List[Dict[str, str]]:
    """
    List all available pre-built strategy templates.

    Returns:
        List of dicts with 'name', 'category', 'description'
    """
    return [
        {'name': 'sma_crossover', 'category': 'Moving Average', 'description': 'Simple Moving Average crossover'},
        {'name': 'ema_crossover', 'category': 'Moving Average', 'description': 'Exponential Moving Average crossover'},
        {'name': 'triple_ma', 'category': 'Moving Average', 'description': 'Triple MA trend confirmation'},
        {'name': 'ma_ribbon', 'category': 'Moving Average', 'description': '4-MA ribbon stack'},
        {'name': 'rsi_strategy', 'category': 'Oscillator', 'description': 'RSI oversold/overbought'},
        {'name': 'stochastic_strategy', 'category': 'Oscillator', 'description': 'Stochastic %K extremes'},
        {'name': 'macd_strategy', 'category': 'Oscillator', 'description': 'MACD zero crossover'},
        {'name': 'macd_zero_cross', 'category': 'Oscillator', 'description': 'MACD zero-line crossover'},
        {'name': 'cci_strategy', 'category': 'Oscillator', 'description': 'CCI extremes'},
        {'name': 'bollinger_bands_strategy', 'category': 'Volatility', 'description': 'Bollinger Band mean reversion'},
        {'name': 'keltner_channel_strategy', 'category': 'Volatility', 'description': 'Keltner Channel breakout'},
        {'name': 'atr_trailing_stop', 'category': 'Volatility', 'description': 'ATR-based trailing stop'},
        {'name': 'obv_strategy', 'category': 'Volume', 'description': 'OBV trend confirmation'},
        {'name': 'mfi_strategy', 'category': 'Volume', 'description': 'Money Flow Index'},
        {'name': 'adx_trend', 'category': 'Trend', 'description': 'ADX trend strength filter'},
        {'name': 'ichimoku_strategy', 'category': 'Trend', 'description': 'Ichimoku Cloud TK crossover'},
        {'name': 'psar_strategy', 'category': 'Trend', 'description': 'Parabolic SAR flip'},
        {'name': 'rsi_macd_combined', 'category': 'Combined', 'description': 'RSI + MACD confirmation'},
        {'name': 'bb_rsi_combined', 'category': 'Combined', 'description': 'Bollinger + RSI confirmation'},
        {'name': 'trend_momentum', 'category': 'Combined', 'description': 'ADX + RSI + EMA trend momentum'},
    ]
