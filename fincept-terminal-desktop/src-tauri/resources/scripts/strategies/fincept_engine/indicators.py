# ============================================================================
# Fincept Terminal - Strategy Engine Indicators
# Pure Python technical indicators (no C#/LEAN dependency)
# ============================================================================

from collections import deque
from typing import Optional
import math


class IndicatorDataPoint:
    """Single indicator value at a point in time."""
    def __init__(self, value: float = 0.0, time=None):
        self.value = value
        self.time = time

    def __float__(self):
        return self.value

    def __repr__(self):
        return f"{self.value:.4f}"


class IndicatorBase:
    """Base class for all indicators."""

    def __init__(self, name: str, period: int):
        self.name = name
        self.period = period
        self.current = IndicatorDataPoint(0.0)
        self.previous = IndicatorDataPoint(0.0)
        self._samples = 0
        self._window = deque(maxlen=period)
        self.warm_up_period = period

    @property
    def is_ready(self) -> bool:
        return self._samples >= self.period

    @property
    def samples(self) -> int:
        return self._samples

    def update(self, time, value: float) -> bool:
        self.previous = IndicatorDataPoint(self.current.value, self.current.time)
        self._window.append(value)
        self._samples += 1
        result = self._compute(value)
        self.current = IndicatorDataPoint(result, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        raise NotImplementedError

    def reset(self):
        self._samples = 0
        self._window.clear()
        self.current = IndicatorDataPoint(0.0)
        self.previous = IndicatorDataPoint(0.0)

    def __float__(self):
        return self.current.value

    def __gt__(self, other):
        if isinstance(other, IndicatorBase):
            return self.current.value > other.current.value
        return self.current.value > float(other)

    def __lt__(self, other):
        if isinstance(other, IndicatorBase):
            return self.current.value < other.current.value
        return self.current.value < float(other)

    def __ge__(self, other):
        if isinstance(other, IndicatorBase):
            return self.current.value >= other.current.value
        return self.current.value >= float(other)

    def __le__(self, other):
        if isinstance(other, IndicatorBase):
            return self.current.value <= other.current.value
        return self.current.value <= float(other)

    def __sub__(self, other):
        if isinstance(other, IndicatorBase):
            return self.current.value - other.current.value
        return self.current.value - float(other)

    def __add__(self, other):
        if isinstance(other, IndicatorBase):
            return self.current.value + other.current.value
        return self.current.value + float(other)

    def __mul__(self, other):
        return self.current.value * float(other)

    def __truediv__(self, other):
        divisor = float(other)
        if divisor == 0:
            return 0
        return self.current.value / divisor

    def __repr__(self):
        return f"{self.name}({self.current.value:.4f})"


class SimpleMovingAverage(IndicatorBase):
    """SMA indicator."""

    def __init__(self, name: str = "SMA", period: int = 14):
        super().__init__(name, period)

    def _compute(self, value: float) -> float:
        if len(self._window) == 0:
            return 0
        return sum(self._window) / len(self._window)


class ExponentialMovingAverage(IndicatorBase):
    """EMA indicator."""

    def __init__(self, name: str = "EMA", period: int = 14, smoothing_factor: float = None):
        super().__init__(name, period)
        self._k = smoothing_factor or (2.0 / (period + 1))
        self._ema = None

    def _compute(self, value: float) -> float:
        if self._ema is None:
            self._ema = value
        else:
            self._ema = value * self._k + self._ema * (1 - self._k)
        return self._ema

    def reset(self):
        super().reset()
        self._ema = None


class MovingAverageConvergenceDivergence(IndicatorBase):
    """MACD indicator with signal and histogram."""

    def __init__(self, name: str = "MACD", fast_period: int = 12,
                 slow_period: int = 26, signal_period: int = 9):
        super().__init__(name, slow_period)
        self.fast = ExponentialMovingAverage("MACD_Fast", fast_period)
        self.slow = ExponentialMovingAverage("MACD_Slow", slow_period)
        self.signal = ExponentialMovingAverage("MACD_Signal", signal_period)
        self.histogram = IndicatorDataPoint(0.0)
        self.warm_up_period = slow_period + signal_period

    @property
    def is_ready(self) -> bool:
        return self.slow.is_ready

    def _compute(self, value: float) -> float:
        self.fast.update(None, value)
        self.slow.update(None, value)
        macd_val = self.fast.current.value - self.slow.current.value
        self.signal.update(None, macd_val)
        self.histogram = IndicatorDataPoint(macd_val - self.signal.current.value)
        return macd_val

    def reset(self):
        super().reset()
        self.fast.reset()
        self.slow.reset()
        self.signal.reset()


class RelativeStrengthIndex(IndicatorBase):
    """RSI indicator."""

    def __init__(self, name: str = "RSI", period: int = 14):
        super().__init__(name, period)
        self._avg_gain = 0
        self._avg_loss = 0
        self._prev_value = None
        self._gains = deque(maxlen=period)
        self._losses = deque(maxlen=period)

    def _compute(self, value: float) -> float:
        if self._prev_value is None:
            self._prev_value = value
            return 50.0

        change = value - self._prev_value
        self._prev_value = value

        gain = max(change, 0)
        loss = abs(min(change, 0))
        self._gains.append(gain)
        self._losses.append(loss)

        if len(self._gains) < self.period:
            return 50.0

        avg_gain = sum(self._gains) / len(self._gains)
        avg_loss = sum(self._losses) / len(self._losses)

        if avg_loss == 0:
            return 100.0

        rs = avg_gain / avg_loss
        return 100.0 - (100.0 / (1.0 + rs))


class BollingerBands(IndicatorBase):
    """Bollinger Bands indicator."""

    def __init__(self, name: str = "BB", period: int = 20, k: float = 2.0):
        super().__init__(name, period)
        self._k = k
        self.middle_band = IndicatorDataPoint(0.0)
        self.upper_band = IndicatorDataPoint(0.0)
        self.lower_band = IndicatorDataPoint(0.0)
        self.band_width = IndicatorDataPoint(0.0)
        self.percent_b = IndicatorDataPoint(0.0)
        self.standard_deviation = IndicatorDataPoint(0.0)

    def _compute(self, value: float) -> float:
        if len(self._window) < 2:
            self.middle_band = IndicatorDataPoint(value)
            self.upper_band = IndicatorDataPoint(value)
            self.lower_band = IndicatorDataPoint(value)
            return value

        mean = sum(self._window) / len(self._window)
        variance = sum((x - mean) ** 2 for x in self._window) / len(self._window)
        std = math.sqrt(variance)

        self.middle_band = IndicatorDataPoint(mean)
        self.upper_band = IndicatorDataPoint(mean + self._k * std)
        self.lower_band = IndicatorDataPoint(mean - self._k * std)
        self.standard_deviation = IndicatorDataPoint(std)

        bw = self.upper_band.value - self.lower_band.value
        self.band_width = IndicatorDataPoint(bw / mean if mean != 0 else 0)

        if bw != 0:
            self.percent_b = IndicatorDataPoint((value - self.lower_band.value) / bw)
        else:
            self.percent_b = IndicatorDataPoint(0.5)

        return mean


class AverageTrueRange(IndicatorBase):
    """ATR indicator."""

    def __init__(self, name: str = "ATR", period: int = 14):
        super().__init__(name, period)
        self._prev_close = None
        self._tr_values = deque(maxlen=period)

    def update_bar(self, time, high: float, low: float, close: float) -> bool:
        if self._prev_close is None:
            tr = high - low
        else:
            tr = max(high - low,
                     abs(high - self._prev_close),
                     abs(low - self._prev_close))

        self._prev_close = close
        self._tr_values.append(tr)
        self._samples += 1

        if len(self._tr_values) > 0:
            atr = sum(self._tr_values) / len(self._tr_values)
        else:
            atr = 0

        self.previous = IndicatorDataPoint(self.current.value, self.current.time)
        self.current = IndicatorDataPoint(atr, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


class Stochastic(IndicatorBase):
    """Stochastic oscillator."""

    def __init__(self, name: str = "STO", period: int = 14, k_period: int = 3, d_period: int = 3):
        super().__init__(name, period)
        self._highs = deque(maxlen=period)
        self._lows = deque(maxlen=period)
        self._k_values = deque(maxlen=k_period)
        self.fast_stoch = IndicatorDataPoint(0.0)
        self.stoch_k = IndicatorDataPoint(0.0)
        self.stoch_d = ExponentialMovingAverage("StochD", d_period)

    def update_bar(self, time, high: float, low: float, close: float) -> bool:
        self._highs.append(high)
        self._lows.append(low)
        self._samples += 1

        highest = max(self._highs)
        lowest = min(self._lows)
        range_val = highest - lowest

        if range_val > 0:
            k = ((close - lowest) / range_val) * 100
        else:
            k = 50.0

        self.fast_stoch = IndicatorDataPoint(k, time)
        self._k_values.append(k)
        k_avg = sum(self._k_values) / len(self._k_values)
        self.stoch_k = IndicatorDataPoint(k_avg, time)
        self.stoch_d.update(time, k_avg)

        self.current = self.stoch_k
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


class RateOfChange(IndicatorBase):
    """Rate of Change indicator."""

    def __init__(self, name: str = "ROC", period: int = 14):
        super().__init__(name, period)

    def _compute(self, value: float) -> float:
        if len(self._window) < self.period:
            return 0
        old_value = self._window[0]
        if old_value == 0:
            return 0
        return ((value - old_value) / old_value) * 100


class Momentum(IndicatorBase):
    """Momentum indicator."""

    def __init__(self, name: str = "MOM", period: int = 14):
        super().__init__(name, period)

    def _compute(self, value: float) -> float:
        if len(self._window) < self.period:
            return 0
        return value - self._window[0]


class WilliamsPercentR(IndicatorBase):
    """Williams %R indicator."""

    def __init__(self, name: str = "WILLR", period: int = 14):
        super().__init__(name, period)
        self._highs = deque(maxlen=period)
        self._lows = deque(maxlen=period)

    def update_bar(self, time, high: float, low: float, close: float) -> bool:
        self._highs.append(high)
        self._lows.append(low)
        self._samples += 1

        highest = max(self._highs)
        lowest = min(self._lows)
        range_val = highest - lowest

        if range_val > 0:
            wr = ((highest - close) / range_val) * -100
        else:
            wr = -50.0

        self.previous = IndicatorDataPoint(self.current.value, self.current.time)
        self.current = IndicatorDataPoint(wr, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


class CommodityChannelIndex(IndicatorBase):
    """CCI indicator."""

    def __init__(self, name: str = "CCI", period: int = 20):
        super().__init__(name, period)
        self._tp_values = deque(maxlen=period)

    def update_bar(self, time, high: float, low: float, close: float) -> bool:
        tp = (high + low + close) / 3.0
        self._tp_values.append(tp)
        self._samples += 1

        if len(self._tp_values) < self.period:
            self.current = IndicatorDataPoint(0, time)
            return False

        mean_tp = sum(self._tp_values) / len(self._tp_values)
        mean_dev = sum(abs(x - mean_tp) for x in self._tp_values) / len(self._tp_values)

        if mean_dev > 0:
            cci = (tp - mean_tp) / (0.015 * mean_dev)
        else:
            cci = 0

        self.previous = IndicatorDataPoint(self.current.value, self.current.time)
        self.current = IndicatorDataPoint(cci, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


class AverageDirectionalIndex(IndicatorBase):
    """ADX indicator."""

    def __init__(self, name: str = "ADX", period: int = 14):
        super().__init__(name, period)
        self._prev_high = None
        self._prev_low = None
        self._prev_close = None
        self._plus_dm = deque(maxlen=period)
        self._minus_dm = deque(maxlen=period)
        self._tr = deque(maxlen=period)
        self._dx_values = deque(maxlen=period)
        self.positive_directional_index = IndicatorDataPoint(0.0)
        self.negative_directional_index = IndicatorDataPoint(0.0)

    def update_bar(self, time, high: float, low: float, close: float) -> bool:
        if self._prev_high is None:
            self._prev_high = high
            self._prev_low = low
            self._prev_close = close
            self._samples += 1
            return False

        up_move = high - self._prev_high
        down_move = self._prev_low - low

        plus_dm = up_move if (up_move > down_move and up_move > 0) else 0
        minus_dm = down_move if (down_move > up_move and down_move > 0) else 0

        tr = max(high - low,
                 abs(high - self._prev_close),
                 abs(low - self._prev_close))

        self._plus_dm.append(plus_dm)
        self._minus_dm.append(minus_dm)
        self._tr.append(tr)
        self._samples += 1

        self._prev_high = high
        self._prev_low = low
        self._prev_close = close

        if len(self._tr) < self.period:
            return False

        atr = sum(self._tr) / len(self._tr)
        if atr == 0:
            return False

        plus_di = (sum(self._plus_dm) / len(self._plus_dm)) / atr * 100
        minus_di = (sum(self._minus_dm) / len(self._minus_dm)) / atr * 100

        self.positive_directional_index = IndicatorDataPoint(plus_di, time)
        self.negative_directional_index = IndicatorDataPoint(minus_di, time)

        di_sum = plus_di + minus_di
        if di_sum == 0:
            dx = 0
        else:
            dx = abs(plus_di - minus_di) / di_sum * 100

        self._dx_values.append(dx)
        adx = sum(self._dx_values) / len(self._dx_values)

        self.previous = IndicatorDataPoint(self.current.value, self.current.time)
        self.current = IndicatorDataPoint(adx, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value
