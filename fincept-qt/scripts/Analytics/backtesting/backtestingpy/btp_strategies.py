"""
Backtesting.py Strategies Module

All strategy class factories for backtesting.py.
Each function returns a Strategy subclass ready for Backtest().
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional


def _get_backtesting():
    """Lazy import backtesting module"""
    from backtesting import Strategy
    from backtesting.lib import crossover, cross, barssince, SignalStrategy, TrailingStrategy
    return Strategy, crossover, cross, barssince, SignalStrategy, TrailingStrategy


# ============================================================================
# Indicator helpers (used inside Strategy.init via self.I)
# ============================================================================

def _sma(values, n):
    return pd.Series(values).rolling(n).mean()

def _ema(values, n):
    return pd.Series(values).ewm(span=n, adjust=False).mean()

def _rsi(values, period):
    s = pd.Series(values)
    delta = s.diff()
    gain = delta.where(delta > 0, 0).rolling(window=period).mean()
    loss = (-delta.where(delta < 0, 0)).rolling(window=period).mean()
    rs = gain / loss
    return 100 - (100 / (1 + rs))

def _macd_line(values, fast, slow):
    s = pd.Series(values)
    return s.ewm(span=fast, adjust=False).mean() - s.ewm(span=slow, adjust=False).mean()

def _macd_signal(values, fast, slow, signal):
    ml = _macd_line(values, fast, slow)
    return ml.ewm(span=signal, adjust=False).mean()

def _macd_histogram(values, fast, slow, signal):
    ml = _macd_line(values, fast, slow)
    sl = ml.ewm(span=signal, adjust=False).mean()
    return ml - sl

def _bbands_upper(values, n, std_dev):
    s = pd.Series(values)
    return s.rolling(n).mean() + std_dev * s.rolling(n).std()

def _bbands_lower(values, n, std_dev):
    s = pd.Series(values)
    return s.rolling(n).mean() - std_dev * s.rolling(n).std()

def _zscore(values, n):
    s = pd.Series(values)
    mean = s.rolling(n).mean()
    std = s.rolling(n).std()
    return (s - mean) / std.replace(0, 1)

def _donchian_upper(values, n):
    return pd.Series(values).rolling(n).max()

def _donchian_lower(values, n):
    return pd.Series(values).rolling(n).min()

def _atr(high, low, close, n):
    h, l, c = pd.Series(high), pd.Series(low), pd.Series(close)
    prev_c = c.shift(1)
    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
    return tr.rolling(n).mean()

def _adx(high, low, close, period):
    h, l, c = pd.Series(high), pd.Series(low), pd.Series(close)
    prev_h, prev_l, prev_c = h.shift(1), l.shift(1), c.shift(1)
    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
    plus_dm = ((h - prev_h).where((h - prev_h) > (prev_l - l), 0)).clip(lower=0)
    minus_dm = ((prev_l - l).where((prev_l - l) > (h - prev_h), 0)).clip(lower=0)
    atr_v = tr.ewm(span=period, adjust=False).mean()
    plus_di = 100 * (plus_dm.ewm(span=period, adjust=False).mean() / atr_v.replace(0, 1))
    minus_di = 100 * (minus_dm.ewm(span=period, adjust=False).mean() / atr_v.replace(0, 1))
    dx = (abs(plus_di - minus_di) / (plus_di + minus_di).replace(0, 1)) * 100
    return dx.ewm(span=period, adjust=False).mean()

def _plus_di(high, low, close, period):
    h, l, c = pd.Series(high), pd.Series(low), pd.Series(close)
    prev_h, prev_l, prev_c = h.shift(1), l.shift(1), c.shift(1)
    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
    plus_dm = ((h - prev_h).where((h - prev_h) > (prev_l - l), 0)).clip(lower=0)
    atr_v = tr.ewm(span=period, adjust=False).mean()
    return 100 * (plus_dm.ewm(span=period, adjust=False).mean() / atr_v.replace(0, 1))

def _minus_di(high, low, close, period):
    h, l, c = pd.Series(high), pd.Series(low), pd.Series(close)
    prev_h, prev_l, prev_c = h.shift(1), l.shift(1), c.shift(1)
    tr = pd.concat([h - l, (h - prev_c).abs(), (l - prev_c).abs()], axis=1).max(axis=1)
    minus_dm = ((prev_l - l).where((prev_l - l) > (h - prev_h), 0)).clip(lower=0)
    atr_v = tr.ewm(span=period, adjust=False).mean()
    return 100 * (minus_dm.ewm(span=period, adjust=False).mean() / atr_v.replace(0, 1))

def _stoch_k(high, low, close, period):
    h, l, c = pd.Series(high), pd.Series(low), pd.Series(close)
    lowest = l.rolling(period).min()
    highest = h.rolling(period).max()
    return ((c - lowest) / (highest - lowest).replace(0, 1)) * 100

def _stoch_d(high, low, close, k_period, d_period):
    k = _stoch_k(high, low, close, k_period)
    return k.rolling(d_period).mean()

def _williams_r(high, low, close, period):
    h, l, c = pd.Series(high), pd.Series(low), pd.Series(close)
    highest = h.rolling(period).max()
    lowest = l.rolling(period).min()
    return -100 * (highest - c) / (highest - lowest).replace(0, 1)

def _cci(high, low, close, period):
    h, l, c = pd.Series(high), pd.Series(low), pd.Series(close)
    tp = (h + l + c) / 3
    sma_tp = tp.rolling(period).mean()
    mad = tp.rolling(period).apply(lambda x: np.abs(x - x.mean()).mean(), raw=True)
    return (tp - sma_tp) / (0.015 * mad.replace(0, 1))

def _obv(close, volume):
    c, v = pd.Series(close), pd.Series(volume)
    direction = np.sign(c.diff())
    direction.iloc[0] = 0
    return (v * direction).cumsum()

def _mfi(high, low, close, volume, period):
    h, l, c, v = pd.Series(high), pd.Series(low), pd.Series(close), pd.Series(volume)
    tp = (h + l + c) / 3
    rmf = tp * v
    pos = rmf.where(tp > tp.shift(1), 0).rolling(period).sum()
    neg = rmf.where(tp < tp.shift(1), 0).rolling(period).sum()
    return 100 - (100 / (1 + pos / neg.replace(0, 1)))

def _ichimoku_tenkan(high, low, period):
    h, l = pd.Series(high), pd.Series(low)
    return (h.rolling(period).max() + l.rolling(period).min()) / 2

def _ichimoku_kijun(high, low, period):
    return _ichimoku_tenkan(high, low, period)

def _psar(high, low, af_step, af_max):
    h, l = pd.Series(high).values, pd.Series(low).values
    n = len(h)
    out = np.zeros(n)
    af = af_step
    bull = True
    hp, lp = h[0], l[0]
    out[0] = h[0]
    for i in range(1, n):
        if bull:
            out[i] = out[i-1] + af * (hp - out[i-1])
            out[i] = min(out[i], l[i-1])
            if i >= 2: out[i] = min(out[i], l[i-2])
            if l[i] < out[i]:
                bull = False; out[i] = hp; lp = l[i]; af = af_step
            elif h[i] > hp:
                hp = h[i]; af = min(af + af_step, af_max)
        else:
            out[i] = out[i-1] + af * (lp - out[i-1])
            out[i] = max(out[i], h[i-1])
            if i >= 2: out[i] = max(out[i], h[i-2])
            if h[i] > out[i]:
                bull = True; out[i] = lp; hp = h[i]; af = af_step
            elif l[i] < lp:
                lp = l[i]; af = min(af + af_step, af_max)
    return out

def _lookback_return(values, n):
    return pd.Series(values).pct_change(periods=n)

def _keltner_upper(high, low, close, ema_period, atr_period, mult):
    mid = _ema(close, ema_period)
    a = _atr(high, low, close, atr_period)
    return mid + mult * a

def _keltner_lower(high, low, close, ema_period, atr_period, mult):
    mid = _ema(close, ema_period)
    a = _atr(high, low, close, atr_period)
    return mid - mult * a


# ============================================================================
# Strategy Builders
# ============================================================================

def build_sma_crossover(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        n1 = (opt_params or {}).get('n1', params.get('fastPeriod', 10))
        n2 = (opt_params or {}).get('n2', params.get('slowPeriod', 20))
        def init(self):
            self.sma1 = self.I(_sma, self.data.Close, self.n1)
            self.sma2 = self.I(_sma, self.data.Close, self.n2)
        def next(self):
            if crossover(self.sma1, self.sma2):
                if self.position.is_short: self.position.close()
                self.buy()
            elif crossover(self.sma2, self.sma1):
                if self.position.is_long: self.position.close()
                self.sell()
    return S


def build_ema_crossover(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        n1 = (opt_params or {}).get('n1', params.get('fastPeriod', 12))
        n2 = (opt_params or {}).get('n2', params.get('slowPeriod', 26))
        def init(self):
            self.ema1 = self.I(_ema, self.data.Close, self.n1)
            self.ema2 = self.I(_ema, self.data.Close, self.n2)
        def next(self):
            if crossover(self.ema1, self.ema2):
                if self.position.is_short: self.position.close()
                self.buy()
            elif crossover(self.ema2, self.ema1):
                if self.position.is_long: self.position.close()
                self.sell()
    return S


def build_rsi(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('period', 14)
        oversold = params.get('oversold', 30)
        overbought = params.get('overbought', 70)
        def init(self):
            self.rsi = self.I(_rsi, self.data.Close, self.period)
        def next(self):
            if self.rsi[-1] < self.oversold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.rsi[-1] > self.overbought:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_macd(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        fast_p = params.get('fastPeriod', 12)
        slow_p = params.get('slowPeriod', 26)
        signal_p = params.get('signalPeriod', 9)
        def init(self):
            self.macd_line = self.I(_macd_line, self.data.Close, self.fast_p, self.slow_p)
            self.signal_line = self.I(_macd_signal, self.data.Close, self.fast_p, self.slow_p, self.signal_p)
        def next(self):
            if crossover(self.macd_line, self.signal_line):
                if self.position.is_short: self.position.close()
                self.buy()
            elif crossover(self.signal_line, self.macd_line):
                if self.position.is_long: self.position.close()
                self.sell()
    return S


def build_bollinger_bands(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('period', 20)
        std_dev = params.get('stdDev', 2.0)
        def init(self):
            self.upper = self.I(_bbands_upper, self.data.Close, self.period, self.std_dev)
            self.lower = self.I(_bbands_lower, self.data.Close, self.period, self.std_dev)
        def next(self):
            if self.data.Close[-1] < self.lower[-1]:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.data.Close[-1] > self.upper[-1]:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_mean_reversion(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('window', params.get('period', 20))
        z_threshold = params.get('threshold', params.get('zThreshold', 2.0))
        def init(self):
            self.z = self.I(_zscore, self.data.Close, self.period)
        def next(self):
            if self.z[-1] < -self.z_threshold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.z[-1] > self.z_threshold:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
            elif abs(self.z[-1]) < 0.5:
                if self.position: self.position.close()
    return S


def build_momentum(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        lookback = params.get('period', params.get('lookback', 12))
        threshold = params.get('threshold', 0.02)
        def init(self):
            self.ret = self.I(_lookback_return, self.data.Close, self.lookback)
        def next(self):
            if self.ret[-1] > self.threshold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.ret[-1] < -self.threshold:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_breakout(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('period', 20)
        atr_mult = params.get('atrMult', 1.5)
        def init(self):
            self.upper = self.I(_donchian_upper, self.data.High, self.period)
            self.lower = self.I(_donchian_lower, self.data.Low, self.period)
            self.atr_val = self.I(_atr, self.data.High, self.data.Low, self.data.Close, self.period)
        def next(self):
            price = self.data.Close[-1]
            if price > self.upper[-1]:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    sl = price - self.atr_val[-1] * self.atr_mult if self.atr_val[-1] > 0 else None
                    self.buy(sl=sl)
            elif price < self.lower[-1]:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    sl = price + self.atr_val[-1] * self.atr_mult if self.atr_val[-1] > 0 else None
                    self.sell(sl=sl)
    return S


def build_stochastic(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        k_period = params.get('kPeriod', 14)
        d_period = params.get('dPeriod', 3)
        oversold = params.get('oversold', 20)
        overbought = params.get('overbought', 80)
        def init(self):
            self.k = self.I(_stoch_k, self.data.High, self.data.Low, self.data.Close, self.k_period)
            self.d = self.I(_stoch_d, self.data.High, self.data.Low, self.data.Close, self.k_period, self.d_period)
        def next(self):
            if self.k[-1] < self.oversold and crossover(self.k, self.d):
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.k[-1] > self.overbought and crossover(self.d, self.k):
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_adx_trend(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('adxPeriod', params.get('period', 14))
        threshold = params.get('adxThreshold', params.get('threshold', 25))
        def init(self):
            self.adx = self.I(_adx, self.data.High, self.data.Low, self.data.Close, self.period)
            self.pdi = self.I(_plus_di, self.data.High, self.data.Low, self.data.Close, self.period)
            self.mdi = self.I(_minus_di, self.data.High, self.data.Low, self.data.Close, self.period)
        def next(self):
            if self.adx[-1] > self.threshold:
                if self.pdi[-1] > self.mdi[-1]:
                    if not self.position.is_long:
                        if self.position.is_short: self.position.close()
                        self.buy()
                elif self.mdi[-1] > self.pdi[-1]:
                    if not self.position.is_short:
                        if self.position.is_long: self.position.close()
                        self.sell()
            else:
                if self.position: self.position.close()
    return S


def build_williams_r(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('period', 14)
        oversold = params.get('oversold', -80)
        overbought = params.get('overbought', -20)
        def init(self):
            self.wr = self.I(_williams_r, self.data.High, self.data.Low, self.data.Close, self.period)
        def next(self):
            if self.wr[-1] < self.oversold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.wr[-1] > self.overbought:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_cci(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('period', 20)
        threshold = params.get('threshold', 100)
        def init(self):
            self.cci = self.I(_cci, self.data.High, self.data.Low, self.data.Close, self.period)
        def next(self):
            if self.cci[-1] < -self.threshold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.cci[-1] > self.threshold:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_obv_trend(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        ma_period = params.get('maPeriod', 20)
        def init(self):
            self.obv_val = self.I(_obv, self.data.Close, self.data.Volume)
            self.obv_ma = self.I(lambda v, n: pd.Series(v).rolling(n).mean(), self.data.Close, self.ma_period)
            # Use price SMA for trend
            self.price_ma = self.I(_sma, self.data.Close, self.ma_period)
        def next(self):
            if self.data.Close[-1] > self.price_ma[-1]:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.data.Close[-1] < self.price_ma[-1]:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_keltner_breakout(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        ema_period = params.get('period', 20)
        atr_period = params.get('period', 10)
        mult = params.get('atrMultiplier', 2.0)
        def init(self):
            self.upper = self.I(_keltner_upper, self.data.High, self.data.Low, self.data.Close,
                                self.ema_period, self.atr_period, self.mult)
            self.lower = self.I(_keltner_lower, self.data.High, self.data.Low, self.data.Close,
                                self.ema_period, self.atr_period, self.mult)
        def next(self):
            price = self.data.Close[-1]
            if price > self.upper[-1]:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif price < self.lower[-1]:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_triple_ma(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        fast = params.get('fastPeriod', 10)
        med = params.get('mediumPeriod', 20)
        slow = params.get('slowPeriod', 50)
        def init(self):
            self.f = self.I(_sma, self.data.Close, self.fast)
            self.m = self.I(_sma, self.data.Close, self.med)
            self.s = self.I(_sma, self.data.Close, self.slow)
        def next(self):
            if self.f[-1] > self.m[-1] > self.s[-1]:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.f[-1] < self.m[-1] < self.s[-1]:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_dual_momentum(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        abs_period = params.get('absolutePeriod', params.get('lookback', 60))
        threshold = params.get('absThreshold', 0.0) / 100.0
        def init(self):
            self.abs_ret = self.I(_lookback_return, self.data.Close, self.abs_period)
        def next(self):
            if self.abs_ret[-1] > self.threshold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.abs_ret[-1] < -self.threshold:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_volatility_breakout(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        atr_period = params.get('atrPeriod', 14)
        atr_mult = params.get('atrMultiplier', params.get('atrMult', 2.0))
        lookback = params.get('lookback', 20)
        def init(self):
            self.atr_val = self.I(_atr, self.data.High, self.data.Low, self.data.Close, self.atr_period)
            self.atr_ma = self.I(lambda h, l, c, n: pd.Series(_atr(h, l, c, n)).rolling(self.lookback).mean(),
                                 self.data.High, self.data.Low, self.data.Close, self.atr_period)
        def next(self):
            if self.atr_val[-1] > self.atr_mult * self.atr_ma[-1]:
                if self.data.Close[-1] > self.data.Open[-1]:
                    if not self.position.is_long:
                        if self.position.is_short: self.position.close()
                        self.buy()
                else:
                    if not self.position.is_short:
                        if self.position.is_long: self.position.close()
                        self.sell()
    return S


def build_rsi_macd(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        rsi_period = params.get('rsiPeriod', 14)
        oversold = params.get('oversold', 30)
        overbought = params.get('overbought', 70)
        def init(self):
            self.rsi = self.I(_rsi, self.data.Close, self.rsi_period)
            self.macd_l = self.I(_macd_line, self.data.Close, 12, 26)
            self.macd_s = self.I(_macd_signal, self.data.Close, 12, 26, 9)
        def next(self):
            if self.rsi[-1] < self.oversold and crossover(self.macd_l, self.macd_s):
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.rsi[-1] > self.overbought and crossover(self.macd_s, self.macd_l):
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_macd_adx(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        fast_p = params.get('macdFast', params.get('fastPeriod', 12))
        slow_p = params.get('macdSlow', params.get('slowPeriod', 26))
        adx_threshold = params.get('adxThreshold', 25)
        def init(self):
            self.macd_l = self.I(_macd_line, self.data.Close, self.fast_p, self.slow_p)
            self.macd_s = self.I(_macd_signal, self.data.Close, self.fast_p, self.slow_p, 9)
            self.adx = self.I(_adx, self.data.High, self.data.Low, self.data.Close, 14)
        def next(self):
            if self.adx[-1] > self.adx_threshold:
                if crossover(self.macd_l, self.macd_s):
                    if self.position.is_short: self.position.close()
                    self.buy()
                elif crossover(self.macd_s, self.macd_l):
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_bollinger_rsi(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        bb_period = params.get('bbPeriod', 20)
        std_dev = params.get('stdDev', 2.0)
        rsi_period = params.get('rsiPeriod', 14)
        def init(self):
            self.upper = self.I(_bbands_upper, self.data.Close, self.bb_period, self.std_dev)
            self.lower = self.I(_bbands_lower, self.data.Close, self.bb_period, self.std_dev)
            self.rsi = self.I(_rsi, self.data.Close, self.rsi_period)
        def next(self):
            if self.data.Close[-1] < self.lower[-1] and self.rsi[-1] < 30:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.data.Close[-1] > self.upper[-1] and self.rsi[-1] > 70:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_ichimoku(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, crossover, *_ = _get_backtesting()
    class S(Strategy):
        tenkan_p = params.get('tenkanPeriod', 9)
        kijun_p = params.get('kijunPeriod', 26)
        def init(self):
            self.tenkan = self.I(_ichimoku_tenkan, self.data.High, self.data.Low, self.tenkan_p)
            self.kijun = self.I(_ichimoku_kijun, self.data.High, self.data.Low, self.kijun_p)
        def next(self):
            if crossover(self.tenkan, self.kijun):
                if self.position.is_short: self.position.close()
                self.buy()
            elif crossover(self.kijun, self.tenkan):
                if self.position.is_long: self.position.close()
                self.sell()
    return S


def build_psar_strategy(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        af_step = params.get('afStep', 0.02)
        af_max = params.get('afMax', 0.2)
        def init(self):
            self.sar = self.I(_psar, self.data.High, self.data.Low, self.af_step, self.af_max)
        def next(self):
            if self.data.Close[-1] > self.sar[-1]:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.data.Close[-1] < self.sar[-1]:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_mfi_strategy(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        period = params.get('period', 14)
        oversold = params.get('oversold', 20)
        overbought = params.get('overbought', 80)
        def init(self):
            self.mfi = self.I(_mfi, self.data.High, self.data.Low, self.data.Close, self.data.Volume, self.period)
        def next(self):
            if self.mfi[-1] < self.oversold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.mfi[-1] > self.overbought:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


def build_macd_zero_cross(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        fast_p = params.get('fastPeriod', 12)
        slow_p = params.get('slowPeriod', 26)
        def init(self):
            self.hist = self.I(_macd_histogram, self.data.Close, self.fast_p, self.slow_p, 9)
        def next(self):
            if len(self.hist) < 2: return
            if self.hist[-2] <= 0 and self.hist[-1] > 0:
                if self.position.is_short: self.position.close()
                self.buy()
            elif self.hist[-2] >= 0 and self.hist[-1] < 0:
                if self.position.is_long: self.position.close()
                self.sell()
    return S


def build_atr_trailing_stop(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        atr_period = params.get('atrPeriod', 14)
        atr_mult = params.get('atrMultiplier', 3.0)
        def init(self):
            self.atr_val = self.I(_atr, self.data.High, self.data.Low, self.data.Close, self.atr_period)
            self.price_ma = self.I(_sma, self.data.Close, 20)
        def next(self):
            price = self.data.Close[-1]
            if price > self.price_ma[-1]:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    sl = price - self.atr_val[-1] * self.atr_mult
                    self.buy(sl=sl)
            elif price < self.price_ma[-1]:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    sl = price + self.atr_val[-1] * self.atr_mult
                    self.sell(sl=sl)
    return S


def build_trend_momentum(params: Dict[str, Any], opt_params: Dict = None):
    Strategy, *_ = _get_backtesting()
    class S(Strategy):
        ma_period = params.get('maPeriod', 50)
        rsi_period = params.get('rsiPeriod', 14)
        rsi_threshold = params.get('rsiThreshold', 50)
        def init(self):
            self.ma = self.I(_sma, self.data.Close, self.ma_period)
            self.rsi = self.I(_rsi, self.data.Close, self.rsi_period)
        def next(self):
            if self.data.Close[-1] > self.ma[-1] and self.rsi[-1] > self.rsi_threshold:
                if not self.position.is_long:
                    if self.position.is_short: self.position.close()
                    self.buy()
            elif self.data.Close[-1] < self.ma[-1] and self.rsi[-1] < self.rsi_threshold:
                if not self.position.is_short:
                    if self.position.is_long: self.position.close()
                    self.sell()
    return S


# ============================================================================
# Strategy Registry
# ============================================================================

STRATEGY_BUILDERS = {
    'sma_crossover': build_sma_crossover,
    'ema_crossover': build_ema_crossover,
    'rsi': build_rsi,
    'macd': build_macd,
    'bollinger_bands': build_bollinger_bands,
    'mean_reversion': build_mean_reversion,
    'momentum': build_momentum,
    'breakout': build_breakout,
    'stochastic': build_stochastic,
    'adx_trend': build_adx_trend,
    'williams_r': build_williams_r,
    'cci': build_cci,
    'obv_trend': build_obv_trend,
    'keltner_breakout': build_keltner_breakout,
    'triple_ma': build_triple_ma,
    'dual_momentum': build_dual_momentum,
    'volatility_breakout': build_volatility_breakout,
    'rsi_macd': build_rsi_macd,
    'macd_adx': build_macd_adx,
    'bollinger_rsi': build_bollinger_rsi,
    'ichimoku': build_ichimoku,
    'psar': build_psar_strategy,
    'mfi': build_mfi_strategy,
    'macd_zero_cross': build_macd_zero_cross,
    'atr_trailing_stop': build_atr_trailing_stop,
    'trend_momentum': build_trend_momentum,
}


def get_strategy_class(strategy_type: str, params: Dict[str, Any],
                       opt_params: Dict = None):
    """Get a Strategy class for the given type and parameters."""
    builder = STRATEGY_BUILDERS.get(strategy_type)
    if builder:
        return builder(params, opt_params)
    return build_sma_crossover(params, opt_params)


def list_strategies() -> List[Dict[str, str]]:
    """Return list of available strategies for frontend catalog."""
    return [{'id': k, 'name': k.replace('_', ' ').title()} for k in STRATEGY_BUILDERS]
