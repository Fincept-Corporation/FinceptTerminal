# ============================================================================
# Fincept Terminal - Strategy Engine Scheduling
# Date rules, time rules, and scheduled events
# ============================================================================

from datetime import datetime, timedelta, time
from typing import Callable, List, Optional


class DateRules:
    """Rules for when scheduled events fire (date component)."""

    def __init__(self, algorithm=None):
        self._algorithm = algorithm

    def every_day(self, symbol=None):
        return DateRule("every_day", symbol)

    def every(self, *days):
        return DateRule("every", days=days)

    def month_start(self, days_offset: int = 0):
        return DateRule("month_start", days_offset=days_offset)

    def month_end(self, days_offset: int = 0):
        return DateRule("month_end", days_offset=days_offset)

    def week_start(self, days_offset: int = 0):
        return DateRule("week_start", days_offset=days_offset)

    def week_end(self, days_offset: int = 0):
        return DateRule("week_end", days_offset=days_offset)

    def on(self, year: int, month: int, day: int):
        return DateRule("on", date=datetime(year, month, day))

    def today(self):
        return DateRule("today")

    def tomorrow(self):
        return DateRule("tomorrow")

    def year_start(self, days_offset: int = 0):
        return DateRule("year_start", days_offset=days_offset)

    def year_end(self, days_offset: int = 0):
        return DateRule("year_end", days_offset=days_offset)


class DateRule:
    """A specific date rule."""
    def __init__(self, rule_type: str, symbol=None, days_offset: int = 0,
                 date: datetime = None, days=None):
        self.rule_type = rule_type
        self.symbol = symbol
        self.days_offset = days_offset
        self.date = date
        self.days = days

    def matches(self, dt: datetime) -> bool:
        if self.rule_type == "every_day":
            return dt.weekday() < 5  # Mon-Fri
        elif self.rule_type == "month_start":
            return dt.day == 1 + self.days_offset
        elif self.rule_type == "month_end":
            next_month = dt.replace(day=28) + timedelta(days=4)
            last_day = (next_month - timedelta(days=next_month.day)).day
            return dt.day == last_day - self.days_offset
        elif self.rule_type == "week_start":
            return dt.weekday() == 0 + self.days_offset
        elif self.rule_type == "week_end":
            return dt.weekday() == 4 - self.days_offset
        elif self.rule_type == "on":
            return dt.date() == self.date.date()
        elif self.rule_type == "today":
            return dt.date() == datetime.now().date()
        return True


class TimeRules:
    """Rules for when scheduled events fire (time component)."""

    def __init__(self, algorithm=None):
        self._algorithm = algorithm

    def at(self, hour: int, minute: int = 0, second: int = 0):
        return TimeRule("at", time=time(hour, minute, second))

    def after_market_open(self, symbol=None, minutes_after: float = 0):
        # Default market open: 9:30 AM
        open_time = time(9, 30)
        actual = datetime.combine(datetime.today(), open_time) + timedelta(minutes=minutes_after)
        return TimeRule("after_market_open", time=actual.time(), symbol=symbol)

    def before_market_close(self, symbol=None, minutes_before: float = 0):
        # Default market close: 4:00 PM
        close_time = time(16, 0)
        actual = datetime.combine(datetime.today(), close_time) - timedelta(minutes=minutes_before)
        return TimeRule("before_market_close", time=actual.time(), symbol=symbol)

    def every(self, interval: timedelta):
        return TimeRule("every", interval=interval)

    @property
    def midnight(self):
        return TimeRule("at", time=time(0, 0))

    @property
    def noon(self):
        return TimeRule("at", time=time(12, 0))

    @property
    def market_open(self):
        return TimeRule("at", time=time(9, 30))

    @property
    def market_close(self):
        return TimeRule("at", time=time(16, 0))


class TimeRule:
    """A specific time rule."""
    def __init__(self, rule_type: str, time: time = None,
                 interval: timedelta = None, symbol=None):
        self.rule_type = rule_type
        self.time = time
        self.interval = interval
        self.symbol = symbol

    def matches(self, dt: datetime) -> bool:
        if self.rule_type == "at":
            return dt.hour == self.time.hour and dt.minute == self.time.minute
        elif self.rule_type == "every" and self.interval:
            return True  # Handled by scheduler
        return False


class ScheduledEvent:
    """A scheduled event with date rule, time rule, and callback."""
    def __init__(self, name: str, date_rule: DateRule, time_rule: TimeRule,
                 callback: Callable):
        self.name = name
        self.date_rule = date_rule
        self.time_rule = time_rule
        self.callback = callback
        self.enabled = True

    def should_fire(self, dt: datetime) -> bool:
        if not self.enabled:
            return False
        return self.date_rule.matches(dt) and self.time_rule.matches(dt)


class ScheduleManager:
    """Manages scheduled events."""

    def __init__(self, algorithm=None):
        self._algorithm = algorithm
        self._events: List[ScheduledEvent] = []
        self.date_rules = DateRules(algorithm)
        self.time_rules = TimeRules(algorithm)

    def on(self, date_rule, time_rule, callback: Callable, name: str = None):
        """Register a scheduled event."""
        event_name = name or f"event_{len(self._events)}"
        event = ScheduledEvent(event_name, date_rule, time_rule, callback)
        self._events.append(event)
        return event

    def event(self, name: str, date_rule, time_rule, callback: Callable):
        return self.on(date_rule, time_rule, callback, name)

    def check_and_fire(self, dt: datetime):
        """Check all events and fire those that match."""
        for event in self._events:
            if event.should_fire(dt):
                event.callback()

    def remove(self, event: ScheduledEvent):
        if event in self._events:
            self._events.remove(event)
