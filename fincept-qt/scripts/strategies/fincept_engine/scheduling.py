# ============================================================================
# Fincept Terminal - Strategy Engine Scheduling
# Date rules, time rules, and scheduled events
#
# The engine runs on daily bars. A rule fires at most once per trading day (an
# intraday `every(interval)` rule collapses to one firing). Events whose time of
# day is before the close (16:00) run before that day's bar is known - they see
# the previous close and their market orders fill at that day's open. Events at or
# after the close run after the strategy has seen the day's bar and fill next day.
# Month/week/year start and end mean the first/last TRADING day of the period.
# ============================================================================

import bisect
from datetime import datetime, timedelta, time, date as _date
from typing import Callable, List, Optional

MARKET_OPEN = time(9, 30)
MARKET_CLOSE = time(16, 0)


def _as_int(x, default=0):
    try:
        return int(x)
    except (TypeError, ValueError):
        return default


def _split_symbol_offset(args, kwargs, offset_names=('days_offset', 'daysOffset')):
    """LEAN signatures are (symbol=None, days_offset=0); older ones were (days_offset).
    Accept both orders and keyword spellings."""
    symbol = kwargs.get('symbol')
    offset = None
    for n in offset_names:
        if n in kwargs:
            offset = kwargs[n]
    for a in args:
        if isinstance(a, bool):
            continue
        if isinstance(a, int) and offset is None:
            offset = a
        elif symbol is None and not isinstance(a, int):
            symbol = a
    return symbol, _as_int(offset, 0)


class TradingCalendar:
    """Position of each trading day inside its week / month / year."""

    def __init__(self, days):
        ds = sorted({datetime(d.year, d.month, d.day) for d in days})
        # Extend past the data with weekdays so the last (partial) month or week of the
        # data is not mistaken for the end of the period.
        if ds:
            d = ds[-1] + timedelta(days=1)
            stop = ds[-1] + timedelta(days=400)
            while d <= stop:
                if d.weekday() < 5:
                    ds.append(d)
                d += timedelta(days=1)
        self.days = ds
        self._pos = {}
        self._build(lambda d: (d.year, d.month), 'month')
        self._build(lambda d: tuple(d.isocalendar()[:2]), 'week')
        self._build(lambda d: d.year, 'year')

    def _build(self, keyf, name):
        groups = {}
        for d in self.days:
            groups.setdefault(keyf(d), []).append(d)
        for g in groups.values():
            n = len(g)
            for i, d in enumerate(g):
                self._pos.setdefault(d, {})[name] = (i, n)

    def position(self, d, period):
        """(index from the start, count) of trading day d within its period."""
        key = datetime(d.year, d.month, d.day)
        return self._pos.get(key, {}).get(period)


class DateRules:
    """Rules for when scheduled events fire (date component)."""

    def __init__(self, algorithm=None):
        self._algorithm = algorithm

    def every_day(self, symbol=None, *args, **kwargs):
        return DateRule("every_day", symbol)

    def every(self, *days):
        flat = []
        for d in days:
            if isinstance(d, (list, tuple, set)):
                flat.extend(d)
            else:
                flat.append(d)
        return DateRule("every", days=[_as_int(d, -1) for d in flat])

    def month_start(self, *args, **kwargs):
        sym, off = _split_symbol_offset(args, kwargs)
        return DateRule("month_start", sym, days_offset=off)

    def month_end(self, *args, **kwargs):
        sym, off = _split_symbol_offset(args, kwargs)
        return DateRule("month_end", sym, days_offset=off)

    def week_start(self, *args, **kwargs):
        sym, off = _split_symbol_offset(args, kwargs)
        return DateRule("week_start", sym, days_offset=off)

    def week_end(self, *args, **kwargs):
        sym, off = _split_symbol_offset(args, kwargs)
        return DateRule("week_end", sym, days_offset=off)

    def year_start(self, *args, **kwargs):
        sym, off = _split_symbol_offset(args, kwargs)
        return DateRule("year_start", sym, days_offset=off)

    def year_end(self, *args, **kwargs):
        sym, off = _split_symbol_offset(args, kwargs)
        return DateRule("year_end", sym, days_offset=off)

    def on(self, *args):
        dates = []
        if len(args) == 3 and all(isinstance(a, int) for a in args):
            dates.append(datetime(args[0], args[1], args[2]))
        else:
            for a in args:
                if isinstance(a, (list, tuple)):
                    dates.extend(a)
                elif isinstance(a, (datetime, _date)):
                    dates.append(a)
        return DateRule("on", dates=[datetime(d.year, d.month, d.day) for d in dates])

    @property
    def today(self):
        now = getattr(self._algorithm, 'time', None) or datetime.now()
        return DateRule("on", dates=[datetime(now.year, now.month, now.day)])

    @property
    def tomorrow(self):
        now = getattr(self._algorithm, 'time', None) or datetime.now()
        n = now + timedelta(days=1)
        return DateRule("on", dates=[datetime(n.year, n.month, n.day)])

    # PascalCase aliases
    EveryDay = every_day
    Every = every
    MonthStart = month_start
    MonthEnd = month_end
    WeekStart = week_start
    WeekEnd = week_end
    YearStart = year_start
    YearEnd = year_end
    On = on


class DateRule:
    """A specific date rule."""
    def __init__(self, rule_type: str, symbol=None, days_offset: int = 0,
                 date: datetime = None, days=None, dates=None):
        self.rule_type = rule_type
        self.symbol = symbol
        self.days_offset = days_offset
        self.days = days
        self.dates = dates or ([date] if date else [])
        self.date = self.dates[0] if self.dates else None
        self.name = rule_type

    def matches_day(self, d: datetime, cal: Optional[TradingCalendar]) -> bool:
        rt = self.rule_type
        if rt == "every_day":
            return True
        if rt == "every":
            return d.weekday() in (self.days or [])
        if rt == "on":
            return any(x.date() == d.date() for x in self.dates)
        period = {'month_start': 'month', 'month_end': 'month', 'week_start': 'week',
                  'week_end': 'week', 'year_start': 'year', 'year_end': 'year'}.get(rt)
        if period is None:
            return False
        pos = cal.position(d, period) if cal is not None else None
        if pos is None:
            return False
        i, n = pos
        off = min(max(self.days_offset, 0), n - 1)
        return i == off if rt.endswith('_start') else (n - 1 - i) == off

    def matches(self, dt: datetime) -> bool:
        """Calendar-free check kept for callers outside the runner (weekday approximation)."""
        return self.matches_day(dt, None) if self.rule_type in ("every_day", "every", "on") else True


class TimeRules:
    """Rules for when scheduled events fire (time component)."""

    def __init__(self, algorithm=None):
        self._algorithm = algorithm

    def at(self, hour: int, minute: int = 0, second_or_tz=0, *args):
        sec = second_or_tz if isinstance(second_or_tz, int) else 0
        return TimeRule("at", time=time(int(hour), int(minute), int(sec)))

    def after_market_open(self, symbol=None, minutes_after_open: float = 0, extended_market_open=False, **kwargs):
        minutes = kwargs.get('minutes_after_open', kwargs.get('minutes_after', minutes_after_open))
        t = (datetime(2000, 1, 3, MARKET_OPEN.hour, MARKET_OPEN.minute) + timedelta(minutes=float(minutes or 0))).time()
        return TimeRule("after_market_open", time=t, symbol=symbol)

    def before_market_close(self, symbol=None, minutes_before_close: float = 0, extended_market_close=False, **kwargs):
        minutes = kwargs.get('minutes_before_close', kwargs.get('minutes_before', minutes_before_close))
        t = (datetime(2000, 1, 3, MARKET_CLOSE.hour, MARKET_CLOSE.minute) - timedelta(minutes=float(minutes or 0))).time()
        return TimeRule("before_market_close", time=t, symbol=symbol)

    def after_market_close(self, symbol=None, minutes_after_close: float = 0, *args, **kwargs):
        t = (datetime(2000, 1, 3, MARKET_CLOSE.hour, MARKET_CLOSE.minute) + timedelta(minutes=float(minutes_after_close or 0))).time()
        return TimeRule("after_market_close", time=t, symbol=symbol)

    def every(self, interval: timedelta):
        return TimeRule("every", time=MARKET_OPEN, interval=interval)

    @property
    def midnight(self):
        return TimeRule("at", time=time(0, 0))

    @property
    def noon(self):
        return TimeRule("at", time=time(12, 0))

    @property
    def market_open(self):
        return TimeRule("at", time=MARKET_OPEN)

    @property
    def market_close(self):
        return TimeRule("at", time=MARKET_CLOSE)

    @property
    def now(self):
        now = getattr(self._algorithm, 'time', None) or datetime.now()
        return TimeRule("at", time=now.time())

    # PascalCase aliases
    At = at
    AfterMarketOpen = after_market_open
    BeforeMarketClose = before_market_close
    Every = every


class TimeRule:
    """A specific time rule."""
    def __init__(self, rule_type: str, time: time = None,
                 interval: timedelta = None, symbol=None):
        self.rule_type = rule_type
        self.time = time or MARKET_OPEN
        self.interval = interval
        self.symbol = symbol
        self.name = rule_type

    def matches(self, dt: datetime) -> bool:
        return dt.hour == self.time.hour and dt.minute == self.time.minute


class ScheduledEvent:
    """A scheduled event with date rule, time rule, and callback."""
    def __init__(self, name: str, date_rule: DateRule, time_rule: TimeRule,
                 callback: Callable):
        self.name = name
        self.date_rule = date_rule
        self.time_rule = time_rule
        self.callback = callback
        self.enabled = True
        self.fire_count = 0

    def should_fire(self, dt: datetime) -> bool:
        if not self.enabled:
            return False
        return self.date_rule.matches(dt) and self.time_rule.matches(dt)

    def time_of_day(self) -> time:
        return getattr(self.time_rule, 'time', None) or MARKET_OPEN


class ScheduleManager:
    """Manages scheduled events."""

    def __init__(self, algorithm=None):
        self._algorithm = algorithm
        self._events: List[ScheduledEvent] = []
        self.date_rules = DateRules(algorithm)
        self.time_rules = TimeRules(algorithm)

    def on(self, date_rule, time_rule, callback: Callable = None, name: str = None):
        """Register a scheduled event. LEAN also accepts on(name, date_rule, time_rule, callback)."""
        if isinstance(date_rule, str) and callback is not None and not callable(time_rule):
            # on(name, date_rule, time_rule, callback) - shift arguments
            name, date_rule, time_rule, callback = date_rule, time_rule, callback, name
        event_name = name or f"event_{len(self._events)}"
        event = ScheduledEvent(event_name, date_rule, time_rule, callback)
        self._events.append(event)
        return event

    def event(self, name: str, date_rule, time_rule, callback: Callable):
        return self.on(date_rule, time_rule, callback, name)

    def due_on(self, day: datetime, calendar: Optional[TradingCalendar]):
        """[(fire datetime, event)] for trading day `day`, in time order."""
        out = []
        for ev in self._events:
            if not ev.enabled or not callable(ev.callback):
                continue
            dr = ev.date_rule
            ok = dr.matches_day(day, calendar) if hasattr(dr, 'matches_day') else True
            if ok:
                tod = ev.time_of_day()
                out.append((datetime(day.year, day.month, day.day, tod.hour, tod.minute, tod.second), ev))
        out.sort(key=lambda x: x[0])
        return out

    def check_and_fire(self, dt: datetime):
        """Legacy exact-time check (kept for direct callers)."""
        for event in self._events:
            if event.should_fire(dt):
                event.callback()

    def remove(self, event: ScheduledEvent):
        if event in self._events:
            self._events.remove(event)

    # PascalCase aliases
    On = on
