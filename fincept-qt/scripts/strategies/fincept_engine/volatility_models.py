# ============================================================================
# Fincept Terminal - Strategy Engine: security volatility models (LEAN IVolatilityModel)
#
# security.volatility_model is updated with every bar the security receives (session.publish).
# Default: the null model (volatility 0), as in LEAN. StandardDeviationOfReturnsVolatilityModel:
# sample standard deviation of the last `periods` simple returns, annualised with sqrt(252);
# RelativeStandardDeviationVolatilityModel: standard deviation / mean of the last prices;
# IndicatorVolatilityModel: an indicator's current value. Computed from the run's real bars.
# ============================================================================

import math
from collections import deque
from datetime import timedelta

from .errors import ArgumentOutOfRangeException


class BaseVolatilityModel:
    """Base class: override update(security, data) and the volatility property."""

    def __init__(self, *args, **kwargs):
        self._volatility = 0.0

    @property
    def volatility(self):
        return self._volatility

    @volatility.setter
    def volatility(self, value):
        self._volatility = float(value)

    def update(self, security, data):
        pass

    def get_history_requirements(self, security, utc_time, *args):
        return []

    def set_subscription_data_config_provider(self, provider):
        pass

    def warm_up(self, *args, **kwargs):
        pass

    Volatility = property(lambda self: self.volatility)
    Update = update
    GetHistoryRequirements = get_history_requirements


class NullVolatilityModel(BaseVolatilityModel):
    """LEAN VolatilityModel.Null: volatility is always 0."""


class VolatilityModel(BaseVolatilityModel):
    """LEAN's VolatilityModel name (static NULL); strategies subclass it for custom models."""

    @staticmethod
    def null():
        return NullVolatilityModel()


VolatilityModel.NULL = NullVolatilityModel()
VolatilityModel.Null = VolatilityModel.NULL


def _periods_in(resolution):
    r = int(resolution) if resolution is not None else 4
    return {0: 1440 * 60, 1: 3600 * 7, 2: 390 * 2, 3: 7 * 30, 4: 30}.get(r, 30)


def _span_of(resolution):
    r = int(resolution) if resolution is not None else 4
    return {0: timedelta(0), 1: timedelta(seconds=1), 2: timedelta(minutes=1),
            3: timedelta(hours=1)}.get(r, timedelta(days=1))


def _price(data):
    for attr in ('price', 'close', 'value'):
        v = getattr(data, attr, None)
        if v is not None:
            try:
                return float(v)
            except (TypeError, ValueError):
                pass
    return 0.0


def _end_time(data):
    return getattr(data, 'end_time', None) or getattr(data, 'time', None)


class StandardDeviationOfReturnsVolatilityModel(BaseVolatilityModel):
    """StandardDeviationOfReturnsVolatilityModel(periods[, resolution, update_frequency]) or
    (resolution[, update_frequency])."""

    def __init__(self, periods=None, resolution=None, update_frequency=None):
        super().__init__()
        from .enums import Resolution
        if isinstance(periods, Resolution):
            periods, resolution, update_frequency = _periods_in(periods), periods, resolution
        periods = 30 if periods is None else int(periods)
        if periods < 2:
            raise ArgumentOutOfRangeException("'periods' must be greater than or equal to 2.")
        self._window = deque(maxlen=periods)
        self._resolution = resolution
        self._span = update_frequency if isinstance(update_frequency, timedelta) else _span_of(resolution)
        self._last_update = None
        self._last_price = 0.0

    @property
    def volatility(self):
        w = self._window
        n = len(w)
        if n < 2:
            return 0.0
        m = sum(w) / n
        return math.sqrt(sum((x - m) ** 2 for x in w) / (n - 1)) * math.sqrt(252.0)

    def update(self, security, data):
        t = _end_time(data)
        px = _price(data)
        if px <= 0 or t is None:
            return
        if self._last_update is not None and t - self._last_update < self._span:
            return
        if self._last_price > 0:
            self._window.append(px / self._last_price - 1.0)
        self._last_update = t
        self._last_price = px

    def reset(self):
        self._window.clear()
        self._last_update = None
        self._last_price = 0.0


class RelativeStandardDeviationVolatilityModel(BaseVolatilityModel):
    """Standard deviation of the last `periods` prices divided by their mean."""

    def __init__(self, period_span=None, periods=30):
        super().__init__()
        if isinstance(period_span, int) and not isinstance(period_span, bool):
            period_span, periods = None, period_span
        self._span = period_span if isinstance(period_span, timedelta) else timedelta(days=1)
        self._window = deque(maxlen=int(periods))
        self._last_update = None

    @property
    def volatility(self):
        w = self._window
        n = len(w)
        if n < 2:
            return 0.0
        m = sum(w) / n
        if m == 0:
            return 0.0
        return math.sqrt(sum((x - m) ** 2 for x in w) / (n - 1)) / m

    def update(self, security, data):
        t = _end_time(data)
        px = _price(data)
        if px <= 0 or t is None:
            return
        if self._last_update is not None and t - self._last_update < self._span:
            return
        self._window.append(px)
        self._last_update = t


class IndicatorVolatilityModel(BaseVolatilityModel):
    """The volatility is an indicator's current value (the indicator is updated by the strategy)."""

    def __init__(self, indicator, indicator_update=None):
        super().__init__()
        self._indicator = indicator
        self._indicator_update = indicator_update

    @property
    def volatility(self):
        cur = getattr(self._indicator, 'current', None)
        return float(getattr(cur, 'value', 0.0) or 0.0)

    def update(self, security, data):
        if callable(self._indicator_update):
            self._indicator_update(security, data, self._indicator)
