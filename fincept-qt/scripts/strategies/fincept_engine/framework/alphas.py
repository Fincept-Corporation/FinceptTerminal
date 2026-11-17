# ============================================================================
# Fincept Terminal - Alpha Models
# Pre-built alpha models for the framework pipeline (LEAN Alphas/*.py semantics).
# Indicators are created through the algorithm (so the runner updates them with
# every bar) and warmed up from real history when a security is added.
# ============================================================================

from datetime import timedelta

from ..algorithm import AlphaModel, ConstantAlphaModel, CompositeAlphaModel
from ..enums import InsightDirection, Resolution
from ..errors import not_supported
from ..types import Insight, TradeBar


def _span(resolution, bars):
    td = resolution.to_timedelta() if hasattr(resolution, 'to_timedelta') else timedelta(days=1)
    return td * max(1, int(bars))


def _warm(algorithm, symbol, indicator, bars):
    """Feed the indicator the last `bars` real bars before now."""
    from ..indicators import feed_indicator
    try:
        hist = algorithm.history[TradeBar](symbol, int(bars))
    except Exception:
        return
    for bar in hist:
        feed_indicator(indicator, bar)


class RsiAlphaModel(AlphaModel):
    """UP when RSI drops below 30, DOWN when it rises above 70 (emitted on the state change;
    the state resets to the middle above 35 / below 65). Insight period = period bars."""

    def __init__(self, period=14, resolution=Resolution.DAILY):
        self.period = int(period)
        self.resolution = resolution if resolution is not None else Resolution.DAILY
        self.insight_period = _span(self.resolution, self.period)
        self._data = {}  # symbol -> [rsi, state]
        self.name = f"RsiAlphaModel({self.period},{self.resolution})"

    @staticmethod
    def _state(value, previous):
        if value > 70:
            return 'high'
        if value < 30:
            return 'low'
        if previous == 'low' and value > 35:
            return 'middle'
        if previous == 'high' and value < 65:
            return 'middle'
        return previous

    def update(self, algorithm, data):
        insights = []
        for symbol, sd in self._data.items():
            rsi, previous = sd
            state = self._state(rsi.current.value, previous)
            if state != previous and rsi.is_ready:
                if state == 'low':
                    insights.append(Insight.price(symbol, self.insight_period, InsightDirection.UP))
                elif state == 'high':
                    insights.append(Insight.price(symbol, self.insight_period, InsightDirection.DOWN))
            sd[1] = state
        return insights

    def on_securities_changed(self, algorithm, changes):
        for sec in changes.removed_securities:
            self._data.pop(sec.symbol, None)
        for sec in changes.added_securities:
            if sec.symbol in self._data:
                continue
            rsi = algorithm.rsi(sec.symbol, self.period, None, self.resolution)
            _warm(algorithm, sec.symbol, rsi, self.period + 1)
            self._data[sec.symbol] = [rsi, 'middle']


class EmaCrossAlphaModel(AlphaModel):
    """UP when the fast EMA crosses above the slow EMA, DOWN when it crosses below.
    Insight period = fast_period bars."""

    def __init__(self, fast_period=12, slow_period=26, resolution=Resolution.DAILY):
        self.fast_period = int(fast_period)
        self.slow_period = int(slow_period)
        self.resolution = resolution if resolution is not None else Resolution.DAILY
        self.prediction_interval = _span(self.resolution, self.fast_period)
        self._data = {}  # symbol -> [fast, slow, fast_over_slow or None]
        self.name = f"EmaCrossAlphaModel({self.fast_period},{self.slow_period},{self.resolution})"

    def update(self, algorithm, data):
        insights = []
        for symbol, sd in self._data.items():
            fast, slow, was_over = sd
            if not (fast.is_ready and slow.is_ready):
                continue
            over = fast.current.value > slow.current.value
            if was_over is not None and over != was_over:
                insights.append(Insight.price(symbol, self.prediction_interval,
                                              InsightDirection.UP if over else InsightDirection.DOWN))
            sd[2] = over
        return insights

    def on_securities_changed(self, algorithm, changes):
        for sec in changes.removed_securities:
            self._data.pop(sec.symbol, None)
        for sec in changes.added_securities:
            if sec.symbol in self._data:
                continue
            fast = algorithm.ema(sec.symbol, self.fast_period, self.resolution)
            slow = algorithm.ema(sec.symbol, self.slow_period, self.resolution)
            _warm(algorithm, sec.symbol, fast, self.slow_period)
            _warm(algorithm, sec.symbol, slow, self.slow_period)
            self._data[sec.symbol] = [fast, slow, None]


class MacdAlphaModel(AlphaModel):
    """Direction from the MACD signal line normalised by price: UP above +bounce_threshold
    (1%), DOWN below -1%, FLAT otherwise; emitted when the direction changes.
    Insight period = fast_period bars."""

    def __init__(self, fast_period=12, slow_period=26, signal_period=9,
                 moving_average_type=None, resolution=Resolution.DAILY):
        self.fast_period = int(fast_period)
        self.slow_period = int(slow_period)
        self.signal_period = int(signal_period)
        self.resolution = resolution if resolution is not None else Resolution.DAILY
        self.insight_period = _span(self.resolution, self.fast_period)
        self.bounce_threshold_percent = 0.01
        self._data = {}  # symbol -> [macd, previous direction]
        self.name = f"MacdAlphaModel({self.fast_period},{self.slow_period},{self.signal_period})"

    def update(self, algorithm, data):
        insights = []
        for symbol, sd in self._data.items():
            macd, previous = sd
            price = algorithm.securities[symbol].price if symbol in algorithm.securities else 0.0
            if not macd.is_ready or price <= 0:
                continue
            normalized = macd.signal.current.value / price
            if normalized > self.bounce_threshold_percent:
                direction = InsightDirection.UP
            elif normalized < -self.bounce_threshold_percent:
                direction = InsightDirection.DOWN
            else:
                direction = InsightDirection.FLAT
            if direction == previous:
                continue
            insights.append(Insight.price(symbol, self.insight_period, direction))
            sd[1] = direction
        return insights

    def on_securities_changed(self, algorithm, changes):
        for sec in changes.removed_securities:
            self._data.pop(sec.symbol, None)
        for sec in changes.added_securities:
            if sec.symbol in self._data:
                continue
            macd = algorithm.macd(sec.symbol, self.fast_period, self.slow_period, self.signal_period,
                                  None, self.resolution)
            _warm(algorithm, sec.symbol, macd, self.slow_period + self.signal_period)
            self._data[sec.symbol] = [macd, None]


class HistoricalReturnsAlphaModel(AlphaModel):
    """Every bar, per symbol: direction = sign of the `lookback`-bar rate of change,
    magnitude = that return. Insight period = lookback bars."""

    def __init__(self, lookback=1, resolution=Resolution.DAILY):
        self.lookback = int(lookback)
        self.resolution = resolution if resolution is not None else Resolution.DAILY
        self.prediction_interval = _span(self.resolution, self.lookback)
        self._data = {}  # symbol -> [roc, last emitted sample]
        self.name = f"HistoricalReturnsAlphaModel({self.lookback},{self.resolution})"

    def update(self, algorithm, data):
        insights = []
        for symbol, sd in self._data.items():
            roc, last = sd
            if not roc.is_ready or roc.samples == last:
                continue
            sd[1] = roc.samples
            r = roc.current.value
            direction = InsightDirection.UP if r > 0 else (InsightDirection.DOWN if r < 0 else InsightDirection.FLAT)
            insights.append(Insight.price(symbol, self.prediction_interval, direction, r))
        return insights

    def on_securities_changed(self, algorithm, changes):
        for sec in changes.removed_securities:
            self._data.pop(sec.symbol, None)
        for sec in changes.added_securities:
            if sec.symbol in self._data:
                continue
            roc = algorithm.roc(sec.symbol, self.lookback, self.resolution)
            _warm(algorithm, sec.symbol, roc, self.lookback + 1)
            self._data[sec.symbol] = [roc, -1]


class _UnsupportedAlpha(AlphaModel):
    _feature = "this alpha model"

    def __init__(self, *args, **kwargs):
        raise not_supported(self._feature)


class PairsTradingAlphaModel(_UnsupportedAlpha):
    _feature = "the pairs-trading alpha model"


class BasePairsTradingAlphaModel(_UnsupportedAlpha):
    _feature = "the pairs-trading alpha model"


class PearsonCorrelationPairsTradingAlphaModel(_UnsupportedAlpha):
    _feature = "the Pearson-correlation pairs-trading alpha model"
