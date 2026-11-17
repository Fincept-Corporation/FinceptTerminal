# ============================================================================
# Fincept Terminal - Strategy Engine: QCAlgorithm.indicator_history (LEAN IndicatorHistory)
#
# indicator_history(indicator, symbol, periods | timedelta | start[, end][, resolution][, selector]):
# the indicator is updated with the bars complete at the algorithm's current time (the run's own
# daily bars, no look-ahead) - enough extra bars before the window to warm it up - and the values it
# produced once ready are returned: .current, ["sub_indicator"], .data_frame, iteration, count.
# ============================================================================

from datetime import datetime, timedelta, date

from .types import TradeBar


def _sub_names(indicator):
    out = []
    for name, v in vars(indicator).items():
        if name.startswith('_') or name in ('current', 'previous', 'window', 'updated', 'name'):
            continue
        if hasattr(v, 'current') and hasattr(getattr(v, 'current'), 'value'):
            out.append(name)
        elif type(v).__name__ == 'IndicatorDataPoint':
            out.append(name)
    return out


def _value_of(v):
    cur = getattr(v, 'current', None)
    if cur is not None and hasattr(cur, 'value') and cur is not v:
        return cur
    return v


class IndicatorHistoryPoint:
    """One step: .current, .<sub_indicator>, ["sub_indicator"], .end_time."""

    def __init__(self, time, current, subs):
        self.end_time = self.time = time
        self.current = current
        self._subs = subs
        for k, v in subs.items():
            setattr(self, k, v)

    def __getitem__(self, name):
        if name == 'current':
            return self.current
        return self._subs[name]

    @property
    def value(self):
        return self.current.value

    def __eq__(self, other):
        if isinstance(other, (int, float)):
            return self.current.value == other
        return self is other

    __hash__ = object.__hash__

    def __float__(self):
        return float(self.current.value)

    def __repr__(self):
        return f"{self.time}: {self.current.value:.6g}"


class IndicatorHistory:
    def __init__(self, points, sub_names):
        self._points = points
        self._subs = sub_names

    @property
    def count(self):
        return len(self._points)

    def __len__(self):
        return len(self._points)

    def __iter__(self):
        return iter(self._points)

    @property
    def current(self):
        return [p.current for p in self._points]

    def __getitem__(self, key):
        if isinstance(key, int):
            return self._points[key]
        if key == 'current':
            return self.current
        return [p[key] for p in self._points]

    @property
    def data_frame(self):
        import pandas as pd
        rows = {'current': [p.current.value for p in self._points]}
        for s in self._subs:
            rows[s] = [p[s].value for p in self._points]
        return pd.DataFrame(rows, index=pd.DatetimeIndex([p.time for p in self._points], name='time'))

    Count = count
    DataFrame = data_frame

    def __repr__(self):
        return f"IndicatorHistory({len(self._points)} points)"


def indicator_history(algo, indicator, symbol, period_or_start, *args, resolution=None, selector=None):
    from .enums import Resolution, SecurityType
    from .errors import not_supported
    end = None
    for a in args:
        if isinstance(a, (datetime, date)) and end is None:
            end = a
        elif isinstance(a, Resolution):
            resolution = a
        elif callable(a):
            selector = a
    if isinstance(symbol, (list, tuple)):
        if len(symbol) != 1:
            raise not_supported("indicator_history() over several symbols")
        symbol = symbol[0]
    if resolution is not None and int(resolution) != int(Resolution.DAILY):
        note = "indicator_history() requests at intraday resolution were served from daily bars"
        if note not in algo._engine_notes:
            algo._engine_notes.append(note)
    key = str(getattr(symbol, 'value', symbol)).upper()
    sec = algo.securities.get(key)
    sym = getattr(sec, 'symbol', symbol)
    st = getattr(sym, 'security_type', SecurityType.EQUITY)
    series = algo._data.series(sym, st, getattr(sym, 'market', None)) if algo._data is not None else None
    if series is None:
        return IndicatorHistory([], [])
    now = algo.time
    n = series.count_known_at(now)
    warm = int(getattr(indicator, 'warm_up_period', 0) or getattr(indicator, 'period', 0) or 0)
    if isinstance(period_or_start, int):
        want = int(period_or_start)
        lo = max(0, n - want - warm)
        hi = n
    else:
        if isinstance(period_or_start, timedelta):
            s0 = now - period_or_start
        else:
            s0 = period_or_start if isinstance(period_or_start, datetime) else datetime.combine(period_or_start, datetime.min.time())
        e0 = end if end is not None else now
        first = next((i for i, d in enumerate(series.dates) if d >= datetime(s0.year, s0.month, s0.day)), n)
        hi = min(n, next((i for i, d in enumerate(series.dates) if d > datetime(e0.year, e0.month, e0.day)), n))
        lo = max(0, first - warm)
        want = max(0, hi - first)
    subs = _sub_names(indicator)
    points = []
    for i in range(lo, hi):
        bar = TradeBar(series.dates[i], sym, series.open[i], series.high[i], series.low[i], series.close[i],
                       series.volume[i], timedelta(days=1))
        bar.end_time = series._ends[i]
        x = selector(bar) if callable(selector) else bar
        indicator.update(x)
        if getattr(indicator, 'is_ready', False):
            cur = indicator.current
            snap = {s: _value_of(getattr(indicator, s)) for s in subs}
            points.append(IndicatorHistoryPoint(bar.end_time, cur, snap))
    return IndicatorHistory(points[-want:] if want else [], subs)
