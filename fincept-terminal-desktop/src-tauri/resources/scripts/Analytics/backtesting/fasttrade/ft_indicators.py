"""
Fast-Trade Indicators Module (FINTA TA Wrapper)

Wraps all 90+ technical indicators from the FINTA library used by fast-trade.
Each function takes a pandas DataFrame with OHLCV columns and returns indicator values.

Categories:
═══════════════════════════════════════════════════════════════════════

Moving Averages (14):
  SMA, EMA, DEMA, TEMA, TRIMA, WMA, HMA, KAMA, SMMA, SSMA, FRAMA, LWMA, ALMA, ZLEMA

Oscillators (16):
  RSI, STOCH, STOCHD, STOCHRSI, MACD, MOM, ROC, CMO, CCI, WILLIAMS, UO, TSI, AO,
  PPO, IFT_RSI, WTO

Volatility (8):
  BBANDS, BBWIDTH, ATR, TR, KC, APZ, DO, MOBO

Volume (9):
  OBV, WOBV, ADL, CHAIKIN, CMF/CFI, EFI, EMV, MFI, VFI

Trend (14):
  ADX, DMI, ICHIMOKU, PSAR, SAR, VORTEX, KST, TRIX, MI, SWI, SQZMI,
  ER, FISH, VIDYA

Bands & Channels (5):
  BBANDS, KC, APZ, DO, MOBO

Other (14):
  PIVOT, PIVOT_FIB, TP, BOP, COPP, DYMI, EBBP, EVWMA, EVSTC, EV_MACD,
  FVE, MAMA, MSD, PZO, QSTICK, VBM, VPT, VW_MACD, VZO, WAVEPM,
  WILLIAMS_FRACTAL, TMF, PERCENT_B, STC, VAMA, VC, VWAP,
  ROLLING_MAX, ROLLING_MIN

Transformer Map:
  Maps string names to FINTA indicator calls for fast-trade's JSON config.
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, Optional, List, Union, Tuple


# ============================================================================
# FINTA Internal Decorators
# ============================================================================

def inputvalidator(input_: str = 'ohlc'):
    """
    FINTA input validator decorator.

    Decorator used internally by the FINTA TA class to validate that
    the input DataFrame has the required columns (ohlc, ohlcv, etc.)
    before computing an indicator.

    Wraps fast_trade.finta.inputvalidator().

    Args:
        input_: Expected input type string.
            'ohlc' - requires open, high, low, close
            'ohlcv' - requires open, high, low, close, volume
            'c' - requires close only

    Returns:
        Decorator function that validates DataFrame inputs
    """
    try:
        from fast_trade.finta import inputvalidator as ft_validator
        return ft_validator(input_)
    except ImportError:
        def decorator(func):
            def wrapper(cls, ohlc, *args, **kwargs):
                required = {
                    'ohlc': ['open', 'high', 'low', 'close'],
                    'ohlcv': ['open', 'high', 'low', 'close', 'volume'],
                    'c': ['close'],
                }
                cols = required.get(input_, ['open', 'high', 'low', 'close'])
                ohlc_cols = [c.lower() for c in ohlc.columns]
                for col in cols:
                    if col not in ohlc_cols:
                        raise ValueError(f"Missing required column: {col}")
                return func(cls, ohlc, *args, **kwargs)
            return wrapper
        return decorator


# ============================================================================
# Helper: Get FINTA TA class
# ============================================================================

def _get_ta():
    """Import and return the FINTA TA class."""
    from fast_trade.finta import TA
    return TA


def _ensure_ohlcv(df: pd.DataFrame) -> pd.DataFrame:
    """Ensure DataFrame has required OHLCV columns (lowercase)."""
    col_map = {}
    for c in df.columns:
        cl = c.lower()
        if cl in ('open', 'high', 'low', 'close', 'volume'):
            col_map[c] = cl
    if col_map:
        df = df.rename(columns=col_map)
    return df


# ============================================================================
# Moving Averages
# ============================================================================

def sma(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Simple Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.SMA(ohlc, period, column)


def ema(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Exponential Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.EMA(ohlc, period, column)


def dema(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Double Exponential Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.DEMA(ohlc, period, column)


def tema(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Triple Exponential Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.TEMA(ohlc, period, column)


def trima(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Triangular Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.TRIMA(ohlc, period)


def wma(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Weighted Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.WMA(ohlc, period, column)


def hma(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Hull Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.HMA(ohlc, period)


def kama(df: pd.DataFrame, er_period: int = 10, fast: int = 2, slow: int = 30, column: str = 'close') -> pd.Series:
    """Kaufman Adaptive Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.KAMA(ohlc, er_period, fast, slow, column)


def smma(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Smoothed Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.SMMA(ohlc, period, column)


def ssma(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Smoothed Simple Moving Average (alias)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.SSMA(ohlc, period, column)


def frama(df: pd.DataFrame, period: int = 14, batch: int = 10) -> pd.Series:
    """Fractal Adaptive Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.FRAMA(ohlc, period, batch)


def lwma(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Linear Weighted Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.LWMA(ohlc, period, column)


def alma(df: pd.DataFrame, period: int = 14, sigma: float = 6.0, offset: float = 0.85) -> pd.Series:
    """Arnaud Legoux Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ALMA(ohlc, period, sigma, offset)


def zlema(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Zero Lag Exponential Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ZLEMA(ohlc, period)


def vama(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Volume Adjusted Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VAMA(ohlc, period)


def vidya(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Variable Index Dynamic Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VIDYA(ohlc, period)


# ============================================================================
# Oscillators
# ============================================================================

def rsi(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Relative Strength Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.RSI(ohlc, period, column)


def stoch(df: pd.DataFrame, period: int = 14) -> pd.DataFrame:
    """Stochastic Oscillator (%K and %D)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.STOCH(ohlc, period)


def stochd(df: pd.DataFrame, period: int = 3, stoch_period: int = 14) -> pd.Series:
    """Stochastic %D (smoothed %K)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.STOCHD(ohlc, period, stoch_period)


def stochrsi(df: pd.DataFrame, rsi_period: int = 14, stoch_period: int = 14) -> pd.Series:
    """Stochastic RSI."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.STOCHRSI(ohlc, rsi_period, stoch_period)


def macd(df: pd.DataFrame, fast: int = 12, slow: int = 26, signal: int = 9) -> pd.DataFrame:
    """
    MACD (Moving Average Convergence Divergence).

    Returns DataFrame with columns: MACD, SIGNAL, HISTOGRAM
    """
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.MACD(ohlc, fast, slow, signal)


def mom(df: pd.DataFrame, period: int = 10, column: str = 'close') -> pd.Series:
    """Momentum."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.MOM(ohlc, period, column)


def roc(df: pd.DataFrame, period: int = 10, column: str = 'close') -> pd.Series:
    """Rate of Change."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ROC(ohlc, period, column)


def cmo(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Chande Momentum Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.CMO(ohlc, period)


def cci(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Commodity Channel Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.CCI(ohlc, period)


def williams(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Williams %R."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.WILLIAMS(ohlc, period)


def uo(df: pd.DataFrame, s: int = 7, m: int = 14, l: int = 28) -> pd.Series:
    """Ultimate Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.UO(ohlc, s, m, l)


def tsi(df: pd.DataFrame, long: int = 25, short: int = 13) -> pd.Series:
    """True Strength Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.TSI(ohlc, long, short)


def ao(df: pd.DataFrame, s: int = 5, l: int = 34) -> pd.Series:
    """Awesome Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.AO(ohlc, s, l)


def ppo(df: pd.DataFrame, fast: int = 12, slow: int = 26) -> pd.DataFrame:
    """Percentage Price Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.PPO(ohlc, fast, slow)


def ift_rsi(df: pd.DataFrame, rsi_period: int = 5, wma_period: int = 9) -> pd.Series:
    """Inverse Fisher Transform on RSI."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.IFT_RSI(ohlc, rsi_period, wma_period)


def wto(df: pd.DataFrame, channel_length: int = 10, avg_length: int = 21) -> pd.DataFrame:
    """Wave Trend Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.WTO(ohlc, channel_length, avg_length)


# ============================================================================
# Volatility
# ============================================================================

def bbands(df: pd.DataFrame, period: int = 20, std_multiplier: float = 2.0, column: str = 'close') -> pd.DataFrame:
    """
    Bollinger Bands.

    Returns DataFrame with: upper, middle, lower bands.
    """
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.BBANDS(ohlc, period, std_multiplier, column)


def bbwidth(df: pd.DataFrame, period: int = 20, std_multiplier: float = 2.0, column: str = 'close') -> pd.Series:
    """Bollinger Band Width."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.BBWIDTH(ohlc, period, std_multiplier, column)


def percent_b(df: pd.DataFrame, period: int = 20, std_multiplier: float = 2.0, column: str = 'close') -> pd.Series:
    """Bollinger %B."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.PERCENT_B(ohlc, period, std_multiplier, column)


def atr(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Average True Range."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ATR(ohlc, period)


def tr(df: pd.DataFrame) -> pd.Series:
    """True Range."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.TR(ohlc)


def kc(df: pd.DataFrame, period: int = 20, atr_period: int = 10, multiplier: float = 2.0) -> pd.DataFrame:
    """
    Keltner Channels.

    Returns DataFrame with: upper, middle, lower channels.
    """
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.KC(ohlc, period, atr_period, multiplier)


def apz(df: pd.DataFrame, period: int = 21, dev_factor: float = 2.0) -> pd.DataFrame:
    """Adaptive Price Zone."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.APZ(ohlc, period, dev_factor)


def donchian(df: pd.DataFrame, upper_period: int = 20, lower_period: int = 20) -> pd.DataFrame:
    """
    Donchian Channels.

    Returns DataFrame with: upper, middle, lower channels.
    """
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.DO(ohlc, upper_period, lower_period)


def mobo(df: pd.DataFrame, period: int = 10, std_multiplier: float = 0.8) -> pd.DataFrame:
    """Momentum Bands."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.MOBO(ohlc, period, std_multiplier)


def chandelier(df: pd.DataFrame, period: int = 22, multiplier: float = 3.0) -> pd.DataFrame:
    """Chandelier Exit."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.CHANDELIER(ohlc, period, multiplier)


def msd(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Mean Standard Deviation."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.MSD(ohlc, period)


# ============================================================================
# Volume Indicators
# ============================================================================

def obv(df: pd.DataFrame) -> pd.Series:
    """On-Balance Volume."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.OBV(ohlc)


def wobv(df: pd.DataFrame) -> pd.Series:
    """Weighted On-Balance Volume."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.WOBV(ohlc)


def adl(df: pd.DataFrame) -> pd.Series:
    """Accumulation/Distribution Line."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ADL(ohlc)


def chaikin(df: pd.DataFrame, fast: int = 3, slow: int = 10) -> pd.Series:
    """Chaikin Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.CHAIKIN(ohlc, fast, slow)


def cfi(df: pd.DataFrame) -> pd.Series:
    """Cumulative Force Index / Chaikin Money Flow."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.CFI(ohlc)


def efi(df: pd.DataFrame, period: int = 13) -> pd.Series:
    """Elder's Force Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.EFI(ohlc, period)


def emv(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Ease of Movement."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.EMV(ohlc, period)


def mfi(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Money Flow Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.MFI(ohlc, period)


def vfi(df: pd.DataFrame, period: int = 130, smoothing: int = 3, coef: float = 0.2, vol_coef: float = 2.5) -> pd.Series:
    """Volume Flow Indicator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VFI(ohlc, period, smoothing, coef, vol_coef)


def vpt(df: pd.DataFrame) -> pd.Series:
    """Volume Price Trend."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VPT(ohlc)


def fve(df: pd.DataFrame, period: int = 22) -> pd.Series:
    """Finite Volume Elements."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.FVE(ohlc, period)


def evwma(df: pd.DataFrame, period: int = 20) -> pd.Series:
    """Elastic Volume Weighted Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.EVWMA(ohlc, period)


def vwap(df: pd.DataFrame) -> pd.Series:
    """Volume Weighted Average Price."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VWAP(ohlc)


def tmf(df: pd.DataFrame, period: int = 21) -> pd.Series:
    """Twiggs Money Flow."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.TMF(ohlc, period)


def vzo(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Volume Zone Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VZO(ohlc, period)


def pzo(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Price Zone Oscillator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.PZO(ohlc, period)


# ============================================================================
# Trend Indicators
# ============================================================================

def adx(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Average Directional Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ADX(ohlc, period)


def dmi(df: pd.DataFrame, period: int = 14) -> pd.DataFrame:
    """Directional Movement Index (+DI, -DI)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.DMI(ohlc, period)


def ichimoku(
    df: pd.DataFrame,
    tenkan: int = 9,
    kijun: int = 26,
    senkou: int = 52,
    chikou: int = 26
) -> pd.DataFrame:
    """
    Ichimoku Cloud.

    Returns DataFrame with: TENKAN, KIJUN, SENKOU_SPAN_A, SENKOU_SPAN_B, CHIKOU.
    """
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ICHIMOKU(ohlc, tenkan, kijun, senkou, chikou)


def psar(df: pd.DataFrame, iaf: float = 0.02, maxaf: float = 0.2) -> pd.DataFrame:
    """Parabolic SAR."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.PSAR(ohlc, iaf, maxaf)


def sar(df: pd.DataFrame, iaf: float = 0.02, maxaf: float = 0.2) -> pd.Series:
    """SAR (simplified)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.SAR(ohlc, iaf, maxaf)


def vortex(df: pd.DataFrame, period: int = 14) -> pd.DataFrame:
    """Vortex Indicator (+VI, -VI)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VORTEX(ohlc, period)


def kst(df: pd.DataFrame) -> pd.DataFrame:
    """Know Sure Thing."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.KST(ohlc)


def trix(df: pd.DataFrame, period: int = 15) -> pd.Series:
    """TRIX (Triple Exponential Average)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.TRIX(ohlc, period)


def mi(df: pd.DataFrame, period: int = 9) -> pd.Series:
    """Mass Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.MI(ohlc, period)


def swi(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Stochastic Williams Indicator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.SWI(ohlc, period)


def sqzmi(df: pd.DataFrame, period: int = 20, multiplier: float = 2.0) -> pd.Series:
    """Squeeze Momentum Indicator."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.SQZMI(ohlc, period, multiplier)


def er(df: pd.DataFrame, period: int = 10) -> pd.Series:
    """Efficiency Ratio (Kaufman)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ER(ohlc, period)


def fish(df: pd.DataFrame, period: int = 10) -> pd.Series:
    """Fisher Transform."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.FISH(ohlc, period)


def copp(df: pd.DataFrame, roc1: int = 14, roc2: int = 11, wma_period: int = 10) -> pd.Series:
    """Coppock Curve."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.COPP(ohlc, roc1, roc2, wma_period)


# ============================================================================
# Candlestick & Misc Indicators
# ============================================================================

def tp(df: pd.DataFrame) -> pd.Series:
    """Typical Price ((H+L+C)/3)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.TP(ohlc)


def bop(df: pd.DataFrame) -> pd.Series:
    """Balance of Power."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.BOP(ohlc)


def pivot(df: pd.DataFrame) -> pd.DataFrame:
    """Pivot Points (Standard)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.PIVOT(ohlc)


def pivot_fib(df: pd.DataFrame) -> pd.DataFrame:
    """Fibonacci Pivot Points."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.PIVOT_FIB(ohlc)


def dymi(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Dynamic Momentum Index."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.DYMI(ohlc, period)


def ebbp(df: pd.DataFrame, period: int = 13) -> pd.DataFrame:
    """Elder Bull Bear Power."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.EBBP(ohlc, period)


def ev_macd(df: pd.DataFrame, fast: int = 20, slow: int = 40) -> pd.DataFrame:
    """Elastic Volume MACD."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.EV_MACD(ohlc, fast, slow)


def evstc(df: pd.DataFrame, fast: int = 12, slow: int = 26) -> pd.Series:
    """Elastic Volume STC."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.EVSTC(ohlc, fast, slow)


def stc(df: pd.DataFrame, fast: int = 23, slow: int = 50, length: int = 10) -> pd.Series:
    """Schaff Trend Cycle."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.STC(ohlc, fast, slow, length)


def mama(df: pd.DataFrame, fast_limit: float = 0.5, slow_limit: float = 0.05) -> pd.DataFrame:
    """MESA Adaptive Moving Average."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.MAMA(ohlc, fast_limit, slow_limit)


def qstick(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """QStick."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.QSTICK(ohlc, period)


def vbm(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Volatility-Based Momentum."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VBM(ohlc, period)


def vc(df: pd.DataFrame, period: int = 5) -> pd.DataFrame:
    """Value Chart."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VC(ohlc, period)


def vw_macd(df: pd.DataFrame, fast: int = 12, slow: int = 26, signal: int = 9) -> pd.DataFrame:
    """Volume-Weighted MACD."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.VW_MACD(ohlc, fast, slow, signal)


def wavepm(df: pd.DataFrame, period: int = 14) -> pd.Series:
    """Wave PM."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.WAVEPM(ohlc, period)


def williams_fractal(df: pd.DataFrame, period: int = 2) -> pd.DataFrame:
    """Williams Fractal."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.WILLIAMS_FRACTAL(ohlc, period)


def basp(df: pd.DataFrame, period: int = 40) -> pd.DataFrame:
    """Buy and Sell Pressure."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.BASP(ohlc, period)


def baspn(df: pd.DataFrame, period: int = 40) -> pd.DataFrame:
    """Buy and Sell Pressure (Normalized)."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.BASPN(ohlc, period)


def rolling_max(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Rolling Maximum."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ROLLING_MAX(ohlc, period, column)


def rolling_min(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Rolling Minimum."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.ROLLING_MIN(ohlc, period, column)


def smm(df: pd.DataFrame, period: int = 14, column: str = 'close') -> pd.Series:
    """Simple Moving Median."""
    TA = _get_ta()
    ohlc = _ensure_ohlcv(df)
    return TA.SMM(ohlc, period, column)


# ============================================================================
# Transformer Map (for fast-trade JSON config)
# ============================================================================

def get_transformer_map() -> Dict[str, Any]:
    """
    Get the fast-trade transformer map.

    Maps string names to indicator functions used in JSON strategy configs.
    E.g. 'sma' -> SMA function, 'rsi' -> RSI function, etc.

    Returns:
        dict mapping transformer name strings to callables
    """
    try:
        from fast_trade.transformers_map import transformers_map
        return transformers_map
    except ImportError:
        # Return our own map as fallback
        return {
            'sma': sma, 'ema': ema, 'dema': dema, 'tema': tema,
            'wma': wma, 'hma': hma, 'kama': kama, 'smma': smma,
            'frama': frama, 'lwma': lwma, 'alma': alma, 'zlema': zlema,
            'rsi': rsi, 'stoch': stoch, 'macd': macd, 'mom': mom,
            'roc': roc, 'cmo': cmo, 'cci': cci, 'williams': williams,
            'uo': uo, 'tsi': tsi, 'ao': ao, 'ppo': ppo,
            'bbands': bbands, 'atr': atr, 'tr': tr, 'kc': kc,
            'obv': obv, 'adl': adl, 'chaikin': chaikin, 'mfi': mfi,
            'adx': adx, 'dmi': dmi, 'ichimoku': ichimoku, 'psar': psar,
            'sar': sar, 'vortex': vortex, 'fish': fish, 'tp': tp,
            'bop': bop, 'pivot': pivot, 'vwap': vwap,
        }


def apply_transformer(
    df: pd.DataFrame,
    name: str,
    transformer: str,
    args: Optional[List] = None,
    column: Optional[str] = None
) -> pd.DataFrame:
    """
    Apply a single transformer/indicator to a DataFrame.

    Used by fast-trade's datapoints system to add computed columns.

    Args:
        df: OHLCV DataFrame
        name: Output column name
        transformer: Indicator function name (e.g. 'sma', 'rsi')
        args: Positional arguments for the indicator
        column: Specific output column if indicator returns DataFrame

    Returns:
        DataFrame with new column added
    """
    try:
        from fast_trade.build_data_frame import apply_transformers_to_dataframe
        datapoints = [{'name': name, 'transformer': transformer, 'args': args or []}]
        if column:
            datapoints[0]['column'] = column
        return apply_transformers_to_dataframe(df, datapoints)
    except ImportError:
        tmap = get_transformer_map()
        func = tmap.get(transformer)
        if func is None:
            raise ValueError(f"Unknown transformer: {transformer}")
        result = func(df, *(args or []))
        if isinstance(result, pd.DataFrame) and column:
            df[name] = result[column]
        elif isinstance(result, pd.Series):
            df[name] = result
        else:
            df[name] = result
        return df


def list_available_indicators() -> List[str]:
    """
    List all available indicator/transformer names.

    Returns:
        Sorted list of indicator name strings
    """
    return sorted(get_transformer_map().keys())
