# ============================================================================
# Fincept Terminal - Strategy Engine Consolidators
# Aggregate data into larger bars (minute->hour, tick->bar, renko, range)
# ============================================================================

from datetime import datetime, timedelta
from typing import Callable, Optional
from .types import TradeBar, QuoteBar, Tick


class EventHandler:
    """Supports += and -= for event registration like C# events."""
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

    def __bool__(self):
        return len(self._handlers) > 0


class ConsolidatorBase:
    """Base class for data consolidators."""

    def __init__(self):
        self.data_consolidated = EventHandler()
        self._working_bar = None
        self.consolidated = None

    def __getattr__(self, name):
        # Ensure data_consolidated always exists even if subclass skips super().__init__
        if name == 'data_consolidated':
            self.data_consolidated = EventHandler()
            return self.data_consolidated
        if name == '_working_bar':
            self._working_bar = None
            return None
        if name == 'consolidated':
            self.consolidated = None
            return None
        raise AttributeError(f"'{type(self).__name__}' object has no attribute '{name}'")

    def update(self, data):
        raise NotImplementedError

    def scan(self, time: datetime):
        pass


def _calendar_key(fn, t):
    """Period key for LEAN Calendar.* consolidation functions (weekly/monthly/...)."""
    name = getattr(fn, '__name__', '').lower()
    if 'week' in name:
        return tuple(t.isocalendar()[:2])
    if 'quarter' in name:
        return (t.year, (t.month - 1) // 3)
    if 'year' in name:
        return t.year
    if 'month' in name:
        return (t.year, t.month)
    return None


def _as_period(p):
    """Resolution enum -> timedelta; timedelta unchanged; anything else None."""
    if isinstance(p, timedelta):
        return p
    to_td = getattr(p, 'to_timedelta', None)
    if callable(to_td):
        try:
            return to_td()
        except Exception:
            return None
    return None


class TradeBarConsolidator(ConsolidatorBase):
    """Consolidates trade bars into larger bars by time period, bar count or calendar.

    A working bar is emitted as soon as the incoming bar completes its period
    (bar.time + bar.period reaches the period end), so a 1-day consolidator over
    daily bars emits every bar, and a weekly one emits on the week's last bar.
    """

    def __init__(self, period=None, max_count: int = None):
        super().__init__()
        self._calendar = None
        self._period = None
        self._max_count = max_count
        if isinstance(period, int) and not hasattr(period, 'to_timedelta'):
            self._max_count = period
        elif _as_period(period) is not None:
            self._period = _as_period(period)
        elif callable(period):
            self._calendar = period
        self._count = 0
        self._period_start = None
        self._key = None

    @property
    def period(self):
        return self._period

    def _emit(self):
        bar = self._working_bar
        self._working_bar = None
        self._count = 0
        self._period_start = None
        self._key = None
        if bar is None:
            return
        self.consolidated = bar
        if self.data_consolidated:
            self.data_consolidated(self, bar)

    def update(self, bar: TradeBar):
        bar_period = getattr(bar, 'period', None) or timedelta(0)
        if self._calendar is not None:
            key = _calendar_key(self._calendar, bar.time)
            if self._working_bar is not None and key != self._key:
                self._emit()
            if self._working_bar is None:
                self._key = key
        elif self._period is not None and self._working_bar is not None:
            if bar.time >= self._period_start + self._period:
                self._emit()

        if self._working_bar is None:
            self._working_bar = TradeBar(
                time=bar.time, symbol=bar.symbol,
                open=bar.open, high=bar.high,
                low=bar.low, close=bar.close, volume=bar.volume
            )
            self._working_bar.period = self._period or bar_period
            self._period_start = bar.time
            self._count = 1
        else:
            self._working_bar.high = max(self._working_bar.high, bar.high)
            self._working_bar.low = min(self._working_bar.low, bar.low)
            self._working_bar.close = bar.close
            self._working_bar.value = bar.close
            self._working_bar.price = bar.close
            self._working_bar.volume += bar.volume
            self._working_bar.end_time = getattr(bar, 'end_time', bar.time)
            self._count += 1

        if self._max_count and self._count >= self._max_count:
            self._emit()
        elif self._period is not None:
            if bar.time + bar_period >= self._period_start + self._period:
                self._emit()
        elif self._calendar is not None:
            # Emit on the period's last weekday; otherwise the next period's first bar emits it.
            nxt = bar.time + timedelta(days=1)
            while nxt.weekday() >= 5:
                nxt += timedelta(days=1)
            if _calendar_key(self._calendar, nxt) != self._key:
                self._emit()


class QuoteBarConsolidator(TradeBarConsolidator):
    """Quote-bar consolidation. The engine's daily data has no separate bid/ask series
    (Yahoo FX/crypto bars are mid prices), so this consolidates the same OHLC bars."""

    def __init__(self, period=None, max_count: int = None):
        super().__init__(period, max_count)


class RenkoConsolidator(ConsolidatorBase):
    """Renko bar consolidator."""

    def __init__(self, bar_size: float, renko_type=None, force_classic=None):
        super().__init__()
        self._bar_size = bar_size
        self._last_close = None

    def update(self, data):
        price = data.close if hasattr(data, 'close') else float(data)

        if self._last_close is None:
            self._last_close = price
            return

        diff = price - self._last_close
        bricks = int(abs(diff) / self._bar_size)

        for _ in range(bricks):
            direction = 1 if diff > 0 else -1
            new_close = self._last_close + (direction * self._bar_size)
            bar = TradeBar(
                time=data.time if hasattr(data, 'time') else datetime.now(),
                symbol=data.symbol if hasattr(data, 'symbol') else None,
                open=self._last_close,
                high=max(self._last_close, new_close),
                low=min(self._last_close, new_close),
                close=new_close,
                volume=data.volume if hasattr(data, 'volume') else 0
            )
            self.consolidated = bar
            if self.data_consolidated:
                self.data_consolidated(self, bar)
            self._last_close = new_close


class RangeConsolidator(ConsolidatorBase):
    """Range bar consolidator."""

    def __init__(self, range_size: float):
        super().__init__()
        self._range_size = range_size

    def update(self, data):
        price = data.close if hasattr(data, 'close') else float(data)

        if self._working_bar is None:
            self._working_bar = TradeBar(
                time=data.time if hasattr(data, 'time') else datetime.now(),
                symbol=data.symbol if hasattr(data, 'symbol') else None,
                open=price, high=price, low=price, close=price,
                volume=data.volume if hasattr(data, 'volume') else 0
            )
            return

        self._working_bar.high = max(self._working_bar.high, price)
        self._working_bar.low = min(self._working_bar.low, price)
        self._working_bar.close = price
        self._working_bar.volume += data.volume if hasattr(data, 'volume') else 0

        if self._working_bar.high - self._working_bar.low >= self._range_size:
            self.consolidated = self._working_bar
            if self.data_consolidated:
                self.data_consolidated(self, self._working_bar)
            self._working_bar = None


class TickConsolidator(ConsolidatorBase):
    """Consolidates ticks into trade bars."""

    def __init__(self, max_count: int):
        super().__init__()
        self._max_count = max_count
        self._count = 0

    def update(self, tick: Tick):
        if self._working_bar is None:
            self._working_bar = TradeBar(
                time=tick.time, symbol=tick.symbol,
                open=tick.price, high=tick.price,
                low=tick.price, close=tick.price,
                volume=tick.quantity
            )
            self._count = 1
        else:
            self._working_bar.high = max(self._working_bar.high, tick.price)
            self._working_bar.low = min(self._working_bar.low, tick.price)
            self._working_bar.close = tick.price
            self._working_bar.volume += tick.quantity
            self._count += 1

        if self._count >= self._max_count:
            self.consolidated = self._working_bar
            if self.data_consolidated:
                self.data_consolidated(self, self._working_bar)
            self._working_bar = None
            self._count = 0
