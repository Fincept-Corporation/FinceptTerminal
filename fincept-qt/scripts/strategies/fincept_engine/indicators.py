# ============================================================================
# Fincept Terminal - Strategy Engine Indicators
# Pure Python technical indicators (no C#/LEAN dependency)
# ============================================================================

from collections import deque
from typing import Optional
import math


class _IndicatorEventHandler:
    """Supports += and -= for indicator event registration."""
    def __init__(self):
        self._handlers = []
    def __iadd__(self, handler):
        self._handlers.append(handler)
        return self
    def __isub__(self, handler):
        if handler in self._handlers:
            self._handlers.remove(handler)
        return self
    def __call__(self, *args, **kwargs):
        for h in self._handlers:
            h(*args, **kwargs)


class _IndicatorWindow:
    """Rolling window of an indicator's past values; [0] is the most recent (LEAN order)."""
    def __init__(self, size):
        self._size = max(int(size or 1), 2)
        self._data = deque(maxlen=self._size)
    def add(self, point):
        self._data.appendleft(point)
    def __getitem__(self, index):
        # LEAN RollingWindow: past the stored values but within the size -> an empty value; past the
        # size -> out of range (IndexError also ends Python's sequence iteration)
        if index < 0:
            index += len(self._data)
            if index < 0:
                raise IndexError(f"index {index} is out of range")
        if index < len(self._data):
            return self._data[index]
        if index < self._size:
            return IndicatorDataPoint(0.0)
        raise IndexError(f"index {index} is out of range of the window (size {self._size})")
    def __len__(self):
        return len(self._data)
    def __iter__(self):
        return iter(self._data)
    @property
    def count(self):
        return len(self._data)
    @property
    def size(self):
        return self._size
    @size.setter
    def size(self, value):
        self._size = value
        self._data = deque(self._data, maxlen=value)
    @property
    def is_ready(self):
        return len(self._data) >= self._size
    def reset(self):
        self._data.clear()


class IndicatorDataPoint:
    """Single indicator value at a point in time. Accepts the engine's (value, time) and LEAN's
    (time, value) and (symbol, time, value) argument orders."""
    def __init__(self, value=0.0, time=None, *rest):
        from datetime import datetime as _dt, date as _d
        if rest:                                   # (symbol, time, value)
            self.symbol, value, time = value, rest[0], time
            value, time = rest[0], time
        if isinstance(value, (_dt, _d)) and not isinstance(time, (_dt, _d)):
            value, time = time, value
        self.value = float(value) if value is not None else 0.0
        self.time = time
        self.window = _IndicatorWindow(1)

    @property
    def current(self):
        """Return self so that .current.value works on sub-band data points."""
        return self

    @property
    def end_time(self):
        return self.time

    @property
    def price(self):
        return self.value

    def __float__(self):
        return self.value

    def __repr__(self):
        return f"{self.value:.4f}"



def _name_period(name, period):
    """LEAN indicator constructors take (period) or (name, period): an int first argument is the period."""
    if isinstance(name, (int, float)) and not isinstance(name, bool):
        return None, int(name)
    return name, period


class IndicatorBase:
    """Base class for all indicators."""

    def __init__(self, name, period: int = None):
        name, period = _name_period(name, period)
        # QuantConnect compat: if name is an int, treat it as period
        if name is None:
            name = type(self).__name__
        if isinstance(name, (int, float)) and period is None:
            period = int(name)
            name = type(self).__name__
        elif period is None:
            period = 14  # Default period
        self.name = str(name)
        self.period = period
        self.current = IndicatorDataPoint(0.0)
        self.previous = IndicatorDataPoint(0.0)
        self._samples = 0
        self._window = deque(maxlen=period)
        self.warm_up_period = period
        self._is_ready_override = False
        self.updated = _IndicatorEventHandler()
        self.window = _IndicatorWindow(period)

    @property
    def is_ready(self) -> bool:
        return self._is_ready_override or self._samples >= self.period

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    @property
    def samples(self) -> int:
        return self._samples

    def update(self, time_or_input, value: float = None) -> bool:
        # Support both update(time, value) and update(bar) patterns
        if value is not None:
            time = time_or_input
            val = float(value)
        elif isinstance(time_or_input, (int, float)):
            time = None
            val = float(time_or_input)
        elif hasattr(time_or_input, 'close'):
            # TradeBar / bar-like object
            time = getattr(time_or_input, 'time', getattr(time_or_input, 'end_time', None))
            val = float(time_or_input.close)
        elif hasattr(time_or_input, 'value'):
            time = getattr(time_or_input, 'time', None)
            val = float(time_or_input.value)
        elif hasattr(time_or_input, 'price'):
            time = getattr(time_or_input, 'time', None)
            val = float(time_or_input.price)
        else:
            time = None
            try:
                val = float(time_or_input)
            except (TypeError, ValueError):
                val = 0.0
        self.previous = IndicatorDataPoint(self.current.value, self.current.time)
        self._window.append(val)
        self._samples += 1
        self._update_time = time
        result = self._compute(val)
        self.current = IndicatorDataPoint(result, time)
        self._post_update()
        return self.is_ready

    def _post_update(self):
        """Record the new value in `window` and fire `updated` (LEAN: Updated(sender, point))."""
        try:
            self.window.add(self.current)
        except AttributeError:
            pass
        handlers = getattr(self, 'updated', None)
        if handlers:
            handlers(self, self.current)

    def _set_value(self, value, time):
        """For bar indicators that compute outside _compute()."""
        self.previous = IndicatorDataPoint(self.current.value, self.current.time)
        self.current = IndicatorDataPoint(value, time)
        self._post_update()

    def _compute(self, value: float) -> float:
        raise NotImplementedError

    def reset(self):
        self._samples = 0
        self._window.clear()
        self.current = IndicatorDataPoint(0.0)
        self.previous = IndicatorDataPoint(0.0)
        try:
            self.window.reset()
        except AttributeError:
            pass

    def __float__(self):
        return self.current.value

    def __getitem__(self, index):
        """indicator[0] is the current value, [1] the previous, ... (LEAN's indicator window)."""
        return self.window[index]

    def __iter__(self):
        """The stored window values, most recent first (LEAN: an indicator enumerates its window)."""
        return iter(list(self.window))

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
        name, period = _name_period(name, period)
        super().__init__(name, period)

    def _compute(self, value: float) -> float:
        if len(self._window) == 0:
            return 0
        return sum(self._window) / len(self._window)


class ExponentialMovingAverage(IndicatorBase):
    """EMA indicator."""

    def __init__(self, name: str = "EMA", period: int = 14, smoothing_factor: float = None):
        name, period = _name_period(name, period)
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
        return self._is_ready_override or (self.slow.is_ready and self.signal.is_ready)

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

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


class _Wilder:
    """Wilder smoothing: seeded with the simple mean of the first `period` values,
    then avg = (avg * (period - 1) + x) / period."""
    def __init__(self, period):
        self.period = period
        self.n = 0
        self._sum = 0.0
        self.value = 0.0

    def add(self, x):
        self.n += 1
        if self.n <= self.period:
            self._sum += x
            self.value = self._sum / self.n
        else:
            self.value = (self.value * (self.period - 1) + x) / self.period
        return self.value

    @property
    def ready(self):
        return self.n >= self.period


class RelativeStrengthIndex(IndicatorBase):
    """RSI (LEAN default: Wilder smoothing of gains and losses; Simple when asked).

    RSI = 100 - 100 / (1 + avg_gain / avg_loss). Ready after period + 1 inputs
    (period price changes).
    """

    def __init__(self, name: str = "RSI", period: int = 14, moving_average_type=None):
        name, period = _name_period(name, period)
        super().__init__(name, period)
        self._prev_value = None
        mat = int(moving_average_type) if moving_average_type is not None else 2  # Wilders
        self._simple = (mat == 0)
        self._gains = deque(maxlen=period)
        self._losses = deque(maxlen=period)
        self._wg = _Wilder(period)
        self._wl = _Wilder(period)
        self.warm_up_period = period + 1
        # LEAN exposes the two averages as sub-indicators
        self.average_gain = IndicatorDataPoint(0.0)
        self.average_loss = IndicatorDataPoint(0.0)

    @property
    def is_ready(self) -> bool:
        return self._is_ready_override or self._samples > self.period

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    def _compute(self, value: float) -> float:
        if self._prev_value is None:
            self._prev_value = value
            return 0.0
        change = value - self._prev_value
        self._prev_value = value
        gain = max(change, 0.0)
        loss = max(-change, 0.0)
        if self._simple:
            self._gains.append(gain)
            self._losses.append(loss)
            avg_gain = sum(self._gains) / len(self._gains)
            avg_loss = sum(self._losses) / len(self._losses)
        else:
            avg_gain = self._wg.add(gain)
            avg_loss = self._wl.add(loss)
        self.average_gain = IndicatorDataPoint(avg_gain)
        self.average_loss = IndicatorDataPoint(avg_loss)
        if avg_loss == 0:
            return 100.0  # LEAN: no losses -> 100
        rs = avg_gain / avg_loss
        return 100.0 - (100.0 / (1.0 + rs))

    def reset(self):
        super().reset()
        self._prev_value = None
        self._gains.clear()
        self._losses.clear()
        self._wg = _Wilder(self.period)
        self._wl = _Wilder(self.period)


class _SubIndicator:
    """A sub-indicator of a composite indicator (Bollinger middle/upper/lower band, ...): the same
    object for the whole run, with current / previous values and its own rolling window, as LEAN's
    sub-indicators (bb.middle_band.window, bb.middle_band[0], iteration)."""

    def __init__(self, name, window_size=2):
        self.name = name
        self.current = IndicatorDataPoint(0.0)
        self.previous = IndicatorDataPoint(0.0)
        self.window = _IndicatorWindow(window_size)
        self.updated = _IndicatorEventHandler()
        self._samples = 0

    def _set(self, value, time):
        self.previous = self.current
        self.current = IndicatorDataPoint(value, time)
        self._samples += 1
        self.window.add(self.current)
        if self.updated:
            self.updated(self, self.current)

    @property
    def value(self):
        return self.current.value

    @property
    def end_time(self):
        return self.current.time

    @property
    def time(self):
        return self.current.time

    @property
    def price(self):
        return self.current.value

    @property
    def samples(self):
        return self._samples

    @property
    def is_ready(self):
        return self._samples > 0

    def reset(self):
        self.current = IndicatorDataPoint(0.0)
        self.previous = IndicatorDataPoint(0.0)
        self.window.reset()
        self._samples = 0

    def __getitem__(self, index):
        return self.window[index]

    def __iter__(self):
        return iter(list(self.window))

    def __float__(self):
        return self.current.value

    def __repr__(self):
        return f"{self.current.value:.4f}"

    def _cmp(self, other):
        return other.current.value if hasattr(other, 'current') else float(other)

    def __gt__(self, other):
        return self.current.value > self._cmp(other)

    def __lt__(self, other):
        return self.current.value < self._cmp(other)

    def __ge__(self, other):
        return self.current.value >= self._cmp(other)

    def __le__(self, other):
        return self.current.value <= self._cmp(other)

    def __sub__(self, other):
        return self.current.value - self._cmp(other)

    def __add__(self, other):
        return self.current.value + self._cmp(other)

    def __mul__(self, other):
        return self.current.value * self._cmp(other)

    def __truediv__(self, other):
        return self.current.value / self._cmp(other)

    __rsub__ = lambda self, other: float(other) - self.current.value
    __radd__ = __add__
    __rmul__ = __mul__


class BollingerBands(IndicatorBase):
    """Bollinger Bands: middle = simple mean of the last `period` values, upper/lower = middle +- k x
    population standard deviation. LEAN forms: BollingerBands(period, k[, ma_type]) and
    BollingerBands(name, period, k[, ma_type]). The bands are sub-indicators with their own windows."""

    _BANDS = ('price', 'middle_band', 'upper_band', 'lower_band', 'band_width', 'percent_b', 'standard_deviation')

    def __init__(self, name="BB", period=None, k=None, moving_average_type=None):
        if isinstance(name, (int, float)) and not isinstance(name, bool):
            # (period, k[, moving_average_type])
            name, period, k, moving_average_type = None, int(name), period, k
        period = 20 if period is None else int(period)
        k = 2.0 if k is None else float(k)
        super().__init__(name, period)
        self._k = k
        self.moving_average_type = moving_average_type
        for b in self._BANDS:
            setattr(self, b, _SubIndicator(f"{self.name}_{b}", 2))

    def _compute(self, value: float) -> float:
        t = getattr(self, '_update_time', None)
        self.price._set(value, t)
        n = len(self._window)
        mean = sum(self._window) / n
        if n < 2:
            std = 0.0
        else:
            std = math.sqrt(sum((x - mean) ** 2 for x in self._window) / n)
        upper, lower = mean + self._k * std, mean - self._k * std
        self.middle_band._set(mean, t)
        self.upper_band._set(upper, t)
        self.lower_band._set(lower, t)
        self.standard_deviation._set(std, t)
        bw = upper - lower
        self.band_width._set(bw / mean if mean != 0 else 0.0, t)
        self.percent_b._set((value - lower) / bw if bw != 0 else 0.0, t)   # LEAN: 0 when the bands meet
        return mean

    def reset(self):
        super().reset()
        for b in self._BANDS:
            getattr(self, b).reset()


class AverageTrueRange(IndicatorBase):
    """ATR: Wilder-smoothed true range (LEAN default; Simple when asked).
    TR = max(high - low, |high - prev close|, |low - prev close|); the first bar's TR is high - low."""

    def __init__(self, name: str = "ATR", period: int = 14, moving_average_type=None):
        name, period = _name_period(name, period)
        super().__init__(name, period)
        self._prev_close = None
        self._tr_values = deque(maxlen=period)
        mat = int(moving_average_type) if moving_average_type is not None else 2
        self._simple = (mat == 0)
        self._wilder = _Wilder(period)
        self.true_range = IndicatorDataPoint(0.0)

    def update(self, time_or_input, value: float = None) -> bool:
        # Support update(bar) where bar has high/low/close
        if value is None and hasattr(time_or_input, 'high') and hasattr(time_or_input, 'low') and hasattr(time_or_input, 'close'):
            bar = time_or_input
            return self.update_bar(
                getattr(bar, 'time', getattr(bar, 'end_time', None)),
                float(bar.high), float(bar.low), float(bar.close)
            )
        return super().update(time_or_input, value)

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
        self.true_range = IndicatorDataPoint(tr, time)

        if self._simple:
            atr = sum(self._tr_values) / len(self._tr_values)
        else:
            atr = self._wilder.add(tr)

        self._set_value(atr, time)
        return self.is_ready

    def reset(self):
        super().reset()
        self._prev_close = None
        self._tr_values.clear()
        self._wilder = _Wilder(self.period)

    def _compute(self, value: float) -> float:
        return self.current.value


class Stochastic(IndicatorBase):
    """Stochastic oscillator."""

    def __init__(self, name: str = "STO", period: int = 14, k_period: int = 3, d_period: int = 3):
        name, period = _name_period(name, period)
        super().__init__(name, period)
        self._highs = deque(maxlen=period)
        self._lows = deque(maxlen=period)
        self._k_values = deque(maxlen=k_period)
        self.fast_stoch = IndicatorDataPoint(0.0)
        self.stoch_k = IndicatorDataPoint(0.0)
        self.stoch_d = ExponentialMovingAverage("StochD", d_period)
        self.warm_up_period = period
        self._k_period = k_period

    @property
    def is_ready(self) -> bool:
        # Stochastic produces valid output once it has enough data in the window
        # LEAN's Stochastic is ready when all sub-components have data
        return self._is_ready_override or self._samples >= max(self._k_period, self.period - 1)

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    def update(self, time_or_input, value: float = None) -> bool:
        # Support update(bar) where bar has high/low/close
        if value is None and hasattr(time_or_input, 'high') and hasattr(time_or_input, 'low') and hasattr(time_or_input, 'close'):
            bar = time_or_input
            return self.update_bar(
                getattr(bar, 'time', getattr(bar, 'end_time', None)),
                float(bar.high), float(bar.low), float(bar.close)
            )
        return super().update(time_or_input, value)

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

        self._set_value(k_avg, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


class _LagIndicator(IndicatorBase):
    """Base for indicators comparing the input with the input `period` samples ago
    (window of period + 1 values; ready after period + 1 samples, like LEAN)."""

    def __init__(self, name, period):
        name, period = _name_period(name, period)
        super().__init__(name, period)
        self._lag = deque(maxlen=int(self.period) + 1)
        self.warm_up_period = int(self.period) + 1

    @property
    def is_ready(self) -> bool:
        return self._is_ready_override or self._samples > self.period

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    def _compute(self, value: float) -> float:
        self._lag.append(value)
        if len(self._lag) <= self.period:
            return 0.0
        return self._from_lag(value, self._lag[0])

    def _from_lag(self, value, old):
        raise NotImplementedError

    def reset(self):
        super().reset()
        self._lag.clear()


class RateOfChange(_LagIndicator):
    """ROC = (value - value[period ago]) / value[period ago], a FRACTION (LEAN)."""

    def __init__(self, name: str = "ROC", period: int = 14):
        name, period = _name_period(name, period)
        super().__init__(name, period)

    def _from_lag(self, value, old):
        return (value - old) / old if old else 0.0


class MomentumPercent(_LagIndicator):
    """MOMP = 100 * (value - value[period ago]) / value[period ago] (LEAN RateOfChangePercent)."""

    def __init__(self, name: str = "MOMP", period: int = 14):
        name, period = _name_period(name, period)
        super().__init__(name, period)

    def _from_lag(self, value, old):
        return 100.0 * (value - old) / old if old else 0.0


class Momentum(_LagIndicator):
    """MOM = value - value[period ago]."""

    def __init__(self, name: str = "MOM", period: int = 14):
        name, period = _name_period(name, period)
        super().__init__(name, period)

    def _from_lag(self, value, old):
        return value - old


class Delay(_LagIndicator):
    """Delay(period): the input value from `period` samples ago."""

    def __init__(self, name="DELAY", period: int = 1):
        if isinstance(name, (int, float)) and not isinstance(name, bool):
            name, period = f"DELAY{int(name)}", int(name)
        super().__init__(name, period)

    def _from_lag(self, value, old):
        return old


class WilliamsPercentR(IndicatorBase):
    """Williams %R indicator."""

    def __init__(self, name: str = "WILLR", period: int = 14):
        name, period = _name_period(name, period)
        super().__init__(name, period)
        self._highs = deque(maxlen=period)
        self._lows = deque(maxlen=period)

    def update(self, time_or_input, value: float = None) -> bool:
        if value is None and hasattr(time_or_input, 'high') and hasattr(time_or_input, 'low') and hasattr(time_or_input, 'close'):
            bar = time_or_input
            return self.update_bar(
                getattr(bar, 'time', getattr(bar, 'end_time', None)),
                float(bar.high), float(bar.low), float(bar.close)
            )
        return super().update(time_or_input, value)

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

        self._set_value(wr, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


class CommodityChannelIndex(IndicatorBase):
    """CCI indicator."""

    def __init__(self, name: str = "CCI", period: int = 20):
        name, period = _name_period(name, period)
        super().__init__(name, period)
        self._tp_values = deque(maxlen=period)

    def update(self, time_or_input, value: float = None) -> bool:
        if value is None and hasattr(time_or_input, 'high') and hasattr(time_or_input, 'low') and hasattr(time_or_input, 'close'):
            bar = time_or_input
            return self.update_bar(
                getattr(bar, 'time', getattr(bar, 'end_time', None)),
                float(bar.high), float(bar.low), float(bar.close)
            )
        return super().update(time_or_input, value)

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

        self._set_value(cci, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


class AverageDirectionalIndex(IndicatorBase):
    """ADX indicator."""

    def __init__(self, name: str = "ADX", period: int = 14):
        name, period = _name_period(name, period)
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

    def update(self, time_or_input, value: float = None) -> bool:
        if value is None and hasattr(time_or_input, 'high') and hasattr(time_or_input, 'low') and hasattr(time_or_input, 'close'):
            bar = time_or_input
            return self.update_bar(
                getattr(bar, 'time', getattr(bar, 'end_time', None)),
                float(bar.high), float(bar.low), float(bar.close)
            )
        return super().update(time_or_input, value)

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

        self._set_value(adx, time)
        return self.is_ready

    def _compute(self, value: float) -> float:
        return self.current.value


# ============================================================================
# Indicators that used to be stubs (an SMA standing in for them). Each follows the
# standard / LEAN definition; see the docstrings for the formula.
# ============================================================================

def _bar_fields(time_or_input, value=None):
    """(time, open, high, low, close, volume) from a bar, a data point or (time, value)."""
    if value is not None:
        v = float(value)
        return time_or_input, v, v, v, v, 0.0
    x = time_or_input
    if hasattr(x, 'close') and hasattr(x, 'high'):
        t = getattr(x, 'time', getattr(x, 'end_time', None))
        c = float(x.close)
        return (t, float(getattr(x, 'open', c) or c), float(x.high), float(x.low), c,
                float(getattr(x, 'volume', 0.0) or 0.0))
    if hasattr(x, 'value'):
        v = float(x.value)
        return getattr(x, 'time', None), v, v, v, v, 0.0
    v = float(x)
    return None, v, v, v, v, 0.0


class AroonOscillator(IndicatorBase):
    """Aroon. Over the last period + 1 bars:
      AroonUp   = 100 * (period - bars since the highest high) / period
      AroonDown = 100 * (period - bars since the lowest low)  / period
      value     = AroonUp - AroonDown   (ready after period + 1 bars)."""

    def __init__(self, name="AROON", up_period: int = 25, down_period: int = None):
        if isinstance(name, (int, float)) and not isinstance(name, bool):
            name, up_period, down_period = "AROON", int(name), (up_period if down_period is None else down_period)
        up_period = int(up_period)
        down_period = int(down_period) if down_period else up_period
        super().__init__(name, max(up_period, down_period))
        self._up_p, self._down_p = up_period, down_period
        self._highs = deque(maxlen=up_period + 1)
        self._lows = deque(maxlen=down_period + 1)
        self.aroon_up = IndicatorDataPoint(0.0)
        self.aroon_down = IndicatorDataPoint(0.0)
        self.warm_up_period = max(up_period, down_period) + 1

    @property
    def is_ready(self) -> bool:
        return self._is_ready_override or (len(self._highs) > self._up_p and len(self._lows) > self._down_p)

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    def update(self, time_or_input, value: float = None) -> bool:
        t, _o, h, l, _c, _v = _bar_fields(time_or_input, value)
        self._highs.append(h)
        self._lows.append(l)
        self._samples += 1
        hs, ls = list(self._highs), list(self._lows)
        # bars since the extreme (the most recent occurrence wins ties)
        hi, lo = max(hs), min(ls)
        since_hi = len(hs) - 1 - max(i for i, x in enumerate(hs) if x == hi)
        since_lo = len(ls) - 1 - max(i for i, x in enumerate(ls) if x == lo)
        up = 100.0 * (self._up_p - since_hi) / self._up_p
        down = 100.0 * (self._down_p - since_lo) / self._down_p
        self.aroon_up = IndicatorDataPoint(up, t)
        self.aroon_down = IndicatorDataPoint(down, t)
        self._set_value(up - down, t)
        return self.is_ready

    def reset(self):
        super().reset()
        self._highs.clear()
        self._lows.clear()


class VolumeWeightedAveragePrice(IndicatorBase):
    """Rolling VWAP over `period` bars (LEAN VolumeWeightedAveragePriceIndicator):
    sum(price_i * volume_i) / sum(volume_i), price_i = (open + high + low + close) / 4.
    With zero volume in the window the plain average price is used."""

    def __init__(self, name="VWAP", period: int = 14):
        super().__init__(name, period)
        self._pv = deque(maxlen=int(self.period))

    def update(self, time_or_input, value: float = None) -> bool:
        t, o, h, l, c, v = _bar_fields(time_or_input, value)
        price = (o + h + l + c) / 4.0
        self._pv.append((price, v))
        self._samples += 1
        vol = sum(x[1] for x in self._pv)
        vwap = (sum(p * q for p, q in self._pv) / vol) if vol > 0 else sum(p for p, _ in self._pv) / len(self._pv)
        self._set_value(vwap, t)
        return self.is_ready

    def reset(self):
        super().reset()
        self._pv.clear()


class IntradayVwap(IndicatorBase):
    """Session VWAP that restarts every day: sum(typical * volume) / sum(volume) over the
    day's bars, typical = (high + low + close) / 3. On daily bars a session has one bar,
    so the value is that day's typical price."""

    def __init__(self, name="VWAP"):
        super().__init__(name, 1)
        self._day = None
        self._pv = 0.0
        self._v = 0.0

    def update(self, time_or_input, value: float = None) -> bool:
        t, _o, h, l, c, v = _bar_fields(time_or_input, value)
        day = t.date() if hasattr(t, 'date') else None
        if day != self._day:
            self._day, self._pv, self._v = day, 0.0, 0.0
        tp = (h + l + c) / 3.0
        self._pv += tp * v
        self._v += v
        self._samples += 1
        self._set_value(self._pv / self._v if self._v > 0 else tp, t)
        return self.is_ready


class StandardDeviation(IndicatorBase):
    """Population standard deviation of the last `period` inputs (LEAN StandardDeviation)."""

    def __init__(self, name="STD", period: int = 20):
        super().__init__(name, period)

    def _compute(self, value: float) -> float:
        n = len(self._window)
        if n < 2:
            return 0.0
        mean = sum(self._window) / n
        return math.sqrt(max(sum((x - mean) ** 2 for x in self._window) / n, 0.0))


class Minimum(IndicatorBase):
    """Lowest input over the last `period` samples (bars: the low)."""

    def __init__(self, name="MIN", period: int = 14):
        super().__init__(name, period)

    def update(self, time_or_input, value: float = None) -> bool:
        t, _o, _h, l, _c, _v = _bar_fields(time_or_input, value)
        self._window.append(l)
        self._samples += 1
        self._set_value(min(self._window), t)
        return self.is_ready


class Maximum(IndicatorBase):
    """Highest input over the last `period` samples (bars: the high)."""

    def __init__(self, name="MAX", period: int = 14):
        super().__init__(name, period)

    def update(self, time_or_input, value: float = None) -> bool:
        t, _o, h, _l, _c, _v = _bar_fields(time_or_input, value)
        self._window.append(h)
        self._samples += 1
        self._set_value(max(self._window), t)
        return self.is_ready


class Identity(IndicatorBase):
    """Passes the input through (bars: the close, or the registered selector)."""

    def __init__(self, name="ID", period: int = 1):
        if isinstance(name, (int, float)) and not isinstance(name, bool):
            name = "ID"
        super().__init__(name, 1)

    def _compute(self, value: float) -> float:
        return value


class FilteredIdentity(Identity):
    """Identity that ignores inputs for which filter(input) is false."""

    def __init__(self, name="FID", filter_func=None):
        super().__init__(name)
        self._filter = filter_func

    def update(self, time_or_input, value: float = None) -> bool:
        if self._filter is not None and value is None and not self._filter(time_or_input):
            return self.is_ready
        return super().update(time_or_input, value)


class LeastSquaresMovingAverage(IndicatorBase):
    """Least-squares line through the last `period` inputs at x = 1..period;
    value = intercept + slope * period (the fitted value at the newest point)."""

    def __init__(self, name="LSMA", period: int = 20):
        super().__init__(name, period)
        self.slope = IndicatorDataPoint(0.0)
        self.intercept = IndicatorDataPoint(0.0)

    def _compute(self, value: float) -> float:
        ys = list(self._window)
        n = len(ys)
        if n < 2:
            return value
        xs = list(range(1, n + 1))
        mx, my = sum(xs) / n, sum(ys) / n
        sxx = sum((x - mx) ** 2 for x in xs)
        b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx if sxx else 0.0
        a = my - b * mx
        self.slope = IndicatorDataPoint(b)
        self.intercept = IndicatorDataPoint(a)
        return a + b * n


class RegressionChannel(IndicatorBase):
    """LEAN RegressionChannel(period, k): linear_regression = least-squares fitted value,
    upper/lower_channel = linear_regression +/- k * population std of the last period inputs."""

    def __init__(self, name="RC", period: int = 20, k: float = 2.0):
        super().__init__(name, period)
        self._k = float(k)
        self.linear_regression = LeastSquaresMovingAverage(f"{name}_LR", period)
        self._std = StandardDeviation(f"{name}_STD", period)
        self.upper_channel = IndicatorDataPoint(0.0)
        self.lower_channel = IndicatorDataPoint(0.0)
        self.intercept = IndicatorDataPoint(0.0)
        self.slope = IndicatorDataPoint(0.0)

    def _compute(self, value: float) -> float:
        self.linear_regression.update(None, value)
        self._std.update(None, value)
        lr = self.linear_regression.current.value
        sd = self._std.current.value
        self.upper_channel = IndicatorDataPoint(lr + self._k * sd)
        self.lower_channel = IndicatorDataPoint(lr - self._k * sd)
        self.intercept = self.linear_regression.intercept
        self.slope = self.linear_regression.slope
        return lr


class Beta(IndicatorBase):
    """Beta of `target` against `reference` over `period` simple returns of their closes
    on common dates: cov(r_target, r_ref) / var(r_ref). Fed the bars of both symbols."""

    def __init__(self, name="BETA", target=None, reference=None, period: int = 20):
        if isinstance(target, int) and not isinstance(period, int):
            target, reference, period = reference, period, target  # (name, period, target, reference)
        super().__init__(name if isinstance(name, str) else "BETA", int(period))
        self.target = str(target).upper() if target is not None else None
        self.reference = str(reference).upper() if reference is not None else None
        self._pending = {}  # time -> {symbol: close}
        self._prev = None   # (target close, reference close) of the previous common date
        self._rt = deque(maxlen=int(self.period))
        self._rr = deque(maxlen=int(self.period))
        self.warm_up_period = int(self.period) + 1

    @property
    def is_ready(self) -> bool:
        return self._is_ready_override or len(self._rr) >= self.period

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    def update(self, time_or_input, value: float = None) -> bool:
        sym = str(getattr(time_or_input, 'symbol', '') or '').upper()
        t, _o, _h, _l, c, _v = _bar_fields(time_or_input, value)
        if sym not in (self.target, self.reference) or t is None:
            return self.is_ready
        row = self._pending.setdefault(t, {})
        row[sym] = c
        if self.target in row and self.reference in row:
            ct, cr = row[self.target], row[self.reference]
            for k in [k for k in self._pending if k <= t]:
                del self._pending[k]
            prev, self._prev = self._prev, (ct, cr)
            self._samples += 1
            if prev and prev[0] and prev[1]:
                self._rt.append(ct / prev[0] - 1.0)
                self._rr.append(cr / prev[1] - 1.0)
            n = len(self._rr)
            beta = 0.0
            if n >= 2:
                mt, mr = sum(self._rt) / n, sum(self._rr) / n
                var = sum((x - mr) ** 2 for x in self._rr)
                cov = sum((a - mt) * (b - mr) for a, b in zip(self._rt, self._rr))
                beta = cov / var if var > 0 else 0.0
            self._set_value(beta, t)
        return self.is_ready


class MassIndex(IndicatorBase):
    """Mass Index (Dorsey): sum over `sum_period` of EMA(high - low) / EMA(EMA(high - low)),
    both EMAs of `ema_period`. Ready once the sum window is full of ratios."""

    def __init__(self, name="MASS", ema_period: int = 9, sum_period: int = 25):
        if isinstance(name, (int, float)) and not isinstance(name, bool):
            name, ema_period, sum_period = "MASS", int(name), int(ema_period if ema_period != 9 else 25)
        super().__init__(name, int(sum_period))
        self._ema1 = ExponentialMovingAverage(f"{name}_EMA1", int(ema_period))
        self._ema2 = ExponentialMovingAverage(f"{name}_EMA2", int(ema_period))
        self._ratios = deque(maxlen=int(sum_period))
        self.warm_up_period = 2 * int(ema_period) + int(sum_period) - 2

    @property
    def is_ready(self) -> bool:
        return self._is_ready_override or len(self._ratios) >= self.period

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    def update(self, time_or_input, value: float = None) -> bool:
        t, _o, h, l, _c, _v = _bar_fields(time_or_input, value)
        self._samples += 1
        self._ema1.update(t, h - l)
        if self._ema1.is_ready:
            self._ema2.update(t, self._ema1.current.value)
            if self._ema2.is_ready and self._ema2.current.value:
                self._ratios.append(self._ema1.current.value / self._ema2.current.value)
        self._set_value(sum(self._ratios), t)
        return self.is_ready


class CompositeIndicator(IndicatorBase):
    """op(left, right), recomputed once both sides have updated (LEAN CompositeIndicator)."""

    def __init__(self, name, left, right, op):
        super().__init__(name, 1)
        self.left, self.right, self._op = left, right, op
        self._new_left = self._new_right = False
        if hasattr(left, 'updated'):
            left.updated += self._on_left
        if hasattr(right, 'updated'):
            right.updated += self._on_right
        else:
            self._new_right = True  # constant right side

    @property
    def is_ready(self) -> bool:
        return self._is_ready_override or (bool(getattr(self.left, 'is_ready', False)) and
                                           bool(getattr(self.right, 'is_ready', True)))

    @is_ready.setter
    def is_ready(self, value: bool):
        self._is_ready_override = value

    def _on_left(self, sender, point):
        self._new_left = True
        self._try(point)

    def _on_right(self, sender, point):
        self._new_right = True
        self._try(point)

    def _try(self, point):
        if self._new_left and self._new_right:
            self._new_left = False
            self._new_right = not hasattr(self.right, 'updated')
            lv = _current_value(self.left)
            rv = _current_value(self.right)
            self._samples += 1
            self._set_value(self._op(lv, rv), getattr(point, 'time', None))


class ConstantIndicator(IndicatorBase):
    """A fixed value, always ready (the right side of `times(indicator, 2)`)."""

    def __init__(self, value, name="CONST"):
        super().__init__(name, 1)
        self.current = IndicatorDataPoint(float(value))
        self._is_ready_override = True


def _current_value(ind) -> float:
    cur = getattr(ind, 'current', None)
    if cur is not None and hasattr(cur, 'value'):
        return float(cur.value)
    if cur is not None:
        try:
            return float(cur)
        except (TypeError, ValueError):
            pass
    return float(getattr(ind, 'value', 0.0) or 0.0)


def feed_indicator(ind, data, time=None):
    """Update an engine indicator or a strategy-defined (PythonIndicator-style) one.

    Engine indicators fire their own `updated` event. A strategy's custom indicator only
    implements update() and sets `value`; LEAN's wrapper then refreshes `current`, counts
    the sample and fires `updated` - done here so chained indicators and handlers work.
    """
    if isinstance(ind, IndicatorBase):
        return ind.update(data)
    result = ind.update(data)
    # LEAN's wrapper of PythonIndicator subclasses counts the samples and takes IsReady from
    # update()'s return value; other duck-typed indicators keep their own bookkeeping.
    wrapped = any(c.__name__ == 'PythonIndicator' for c in type(ind).__mro__)
    if wrapped and isinstance(result, bool) and not isinstance(
            getattr(type(ind), 'is_ready', None), property):
        try:
            ind.is_ready = result
        except AttributeError:
            pass
    val = getattr(ind, 'value', None)
    if val is None:
        val = _current_value(ind)
    t = time if time is not None else getattr(data, 'end_time', getattr(data, 'time', None))
    try:
        fval = float(val)
    except (TypeError, ValueError):
        fval = 0.0
    point = IndicatorDataPoint(fval, t)
    cur = getattr(ind, 'current', None)
    try:
        if cur is not None and hasattr(cur, 'value') and not isinstance(cur, IndicatorDataPoint):
            cur.value = fval
        else:
            ind.current = point
    except AttributeError:
        pass
    if wrapped:
        try:
            ind.samples = int(getattr(ind, 'samples', 0) or 0) + 1
        except (AttributeError, TypeError):
            pass
    handlers = getattr(ind, 'updated', None)
    if handlers:
        handlers(ind, point)
    return result
