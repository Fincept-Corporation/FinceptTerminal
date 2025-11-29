"""
Technical Analysis Package
A comprehensive technical analysis library using the ta library
"""

from .momentum_indicators import (
    calculate_rsi,
    calculate_stochastic,
    calculate_stoch_rsi,
    calculate_williams_r,
    calculate_awesome_oscillator,
    calculate_kama,
    calculate_roc,
    calculate_tsi,
    calculate_ultimate_oscillator,
    calculate_ppo,
    calculate_pvo,
    calculate_all_momentum_indicators
)

from .volume_indicators import (
    calculate_adi,
    calculate_obv,
    calculate_cmf,
    calculate_force_index,
    calculate_eom,
    calculate_vpt,
    calculate_nvi,
    calculate_vwap,
    calculate_mfi,
    calculate_all_volume_indicators
)

from .volatility_indicators import (
    calculate_atr,
    calculate_bollinger_bands,
    calculate_keltner_channel,
    calculate_donchian_channel,
    calculate_ulcer_index,
    calculate_all_volatility_indicators
)

from .trend_indicators import (
    calculate_sma,
    calculate_ema,
    calculate_wma,
    calculate_macd,
    calculate_trix,
    calculate_mass_index,
    calculate_ichimoku,
    calculate_kst,
    calculate_dpo,
    calculate_cci,
    calculate_adx,
    calculate_vortex,
    calculate_psar,
    calculate_stc,
    calculate_aroon,
    calculate_all_trend_indicators
)

from .others_indicators import (
    calculate_daily_return,
    calculate_daily_log_return,
    calculate_cumulative_return,
    calculate_all_others_indicators
)

__all__ = [
    # Momentum
    'calculate_rsi',
    'calculate_stochastic',
    'calculate_stoch_rsi',
    'calculate_williams_r',
    'calculate_awesome_oscillator',
    'calculate_kama',
    'calculate_roc',
    'calculate_tsi',
    'calculate_ultimate_oscillator',
    'calculate_ppo',
    'calculate_pvo',
    'calculate_all_momentum_indicators',

    # Volume
    'calculate_adi',
    'calculate_obv',
    'calculate_cmf',
    'calculate_force_index',
    'calculate_eom',
    'calculate_vpt',
    'calculate_nvi',
    'calculate_vwap',
    'calculate_mfi',
    'calculate_all_volume_indicators',

    # Volatility
    'calculate_atr',
    'calculate_bollinger_bands',
    'calculate_keltner_channel',
    'calculate_donchian_channel',
    'calculate_ulcer_index',
    'calculate_all_volatility_indicators',

    # Trend
    'calculate_sma',
    'calculate_ema',
    'calculate_wma',
    'calculate_macd',
    'calculate_trix',
    'calculate_mass_index',
    'calculate_ichimoku',
    'calculate_kst',
    'calculate_dpo',
    'calculate_cci',
    'calculate_adx',
    'calculate_vortex',
    'calculate_psar',
    'calculate_stc',
    'calculate_aroon',
    'calculate_all_trend_indicators',

    # Others
    'calculate_daily_return',
    'calculate_daily_log_return',
    'calculate_cumulative_return',
    'calculate_all_others_indicators',
]

__version__ = '1.0.0'
