# ============================================================================
# Fincept Terminal - Strategy Engine Consolidators
# Aggregate data into larger bars (minute->hour, tick->bar, renko, range)
# ============================================================================

from datetime import datetime, timedelta
from typing import Callable, Optional
from .types import TradeBar, QuoteBar, Tick


class ConsolidatorBase:
    """Base class for data consolidators."""

    def __init__(self):
        self.data_consolidated = None  # Callback
        self._working_bar = None
        self.consolidated = None

    def update(self, data):
        raise NotImplementedError

    def scan(self, time: datetime):
        pass


class TradeBarConsolidator(ConsolidatorBase):
    """Consolidates trade bars into larger period bars."""

    def __init__(self, period=None, max_count: int = None):
        super().__init__()
        if isinstance(period, timedelta):
            self._period = period
            self._max_count = None
        elif isinstance(period, int):
            self._period = None
            self._max_count = period
        else:
            self._period = period
            self._max_count = max_count

        self._count = 0
        self._period_start = None

    def update(self, bar: TradeBar):
        if self._working_bar is None:
            self._working_bar = TradeBar(
                time=bar.time, symbol=bar.symbol,
                open=bar.open, high=bar.high,
                low=bar.low, close=bar.close, volume=bar.volume
            )
            self._period_start = bar.time
            self._count = 1
        else:
            self._working_bar.high = max(self._working_bar.high, bar.high)
            self._working_bar.low = min(self._working_bar.low, bar.low)
            self._working_bar.close = bar.close
            self._working_bar.volume += bar.volume
            self._working_bar.end_time = bar.time
            self._count += 1

        should_emit = False
        if self._max_count and self._count >= self._max_count:
            should_emit = True
        elif self._period and self._period_start:
            if bar.time - self._period_start >= self._period:
                should_emit = True

        if should_emit:
            self.consolidated = self._working_bar
            if self.data_consolidated:
                self.data_consolidated(self, self._working_bar)
            self._working_bar = None
            self._count = 0
            self._period_start = None


class QuoteBarConsolidator(ConsolidatorBase):
    """Consolidates quote bars into larger period bars."""

    def __init__(self, period=None):
        super().__init__()
        self._period = period if isinstance(period, timedelta) else timedelta(minutes=period or 1)
        self._period_start = None

    def update(self, bar: QuoteBar):
        if self._working_bar is None:
            self._working_bar = bar
            self._period_start = bar.time
        else:
            self._working_bar.close = bar.close
            self._working_bar.value = bar.value

        if bar.time - self._period_start >= self._period:
            self.consolidated = self._working_bar
            if self.data_consolidated:
                self.data_consolidated(self, self._working_bar)
            self._working_bar = None
            self._period_start = None


class RenkoConsolidator(ConsolidatorBase):
    """Renko bar consolidator."""

    def __init__(self, bar_size: float, renko_type=None):
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
