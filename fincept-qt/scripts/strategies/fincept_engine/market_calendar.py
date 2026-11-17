# ============================================================================
# Fincept Terminal - Strategy Engine Market Calendar
# NYSE trading days for any year, by rule (pure stdlib, no data files).
#
# Full-day holidays: New Year's Day, Martin Luther King Jr. Day, Washington's
# Birthday, Good Friday, Memorial Day, Juneteenth (from 2022), Independence Day,
# Labor Day, Thanksgiving and Christmas, with NYSE weekend observance:
#   - a holiday on Sunday is observed the following Monday;
#   - a holiday on Saturday is observed the preceding Friday, except New Year's
#     Day (NYSE Rule 7.2: no Friday close at the end of the yearly accounting
#     period, e.g. 2010-12-31 and 2021-12-31 were trading days);
# plus the unscheduled full-day closures since 1994 (national days of mourning,
# 9/11, Hurricane Sandy), listed in SPECIAL_CLOSURES.
#
# Early closes (13:00 ET) follow the modern rule: July 3 when it is Mon-Thu, the
# day after Thanksgiving, December 24 when it is Mon-Thu. Older irregular early
# closes are not modelled.
# ============================================================================

from datetime import date, datetime, time as dt_time, timedelta
from functools import lru_cache

MONDAY, TUESDAY, WEDNESDAY, THURSDAY, FRIDAY, SATURDAY, SUNDAY = range(7)

MARKET_OPEN = dt_time(9, 30)
MARKET_CLOSE = dt_time(16, 0)
EARLY_CLOSE = dt_time(13, 0)

# Unscheduled NYSE full-day closures (weekday dates).
SPECIAL_CLOSURES = frozenset({
    date(1994, 4, 27),                      # President Nixon funeral
    date(2001, 9, 11), date(2001, 9, 12), date(2001, 9, 13), date(2001, 9, 14),  # 9/11
    date(2004, 6, 11),                      # President Reagan funeral
    date(2007, 1, 2),                       # President Ford funeral
    date(2012, 10, 29), date(2012, 10, 30),  # Hurricane Sandy
    date(2018, 12, 5),                      # President G.H.W. Bush funeral
    date(2025, 1, 9),                       # President Carter funeral
})


def _as_date(d) -> date:
    if isinstance(d, datetime):
        return d.date()
    if isinstance(d, date):
        return d
    raise TypeError(f"expected a date or datetime, got {type(d).__name__}")


def easter_sunday(year: int) -> date:
    """Gregorian Easter Sunday (anonymous Gregorian algorithm)."""
    a = year % 19
    b, c = divmod(year, 100)
    d, e = divmod(b, 4)
    f = (b + 8) // 25
    g = (b - f + 1) // 3
    h = (19 * a + b - d - g + 15) % 30
    i, k = divmod(c, 4)
    l = (32 + 2 * e + 2 * i - h - k) % 7
    m = (a + 11 * h + 22 * l) // 451
    month, day = divmod(h + l - 7 * m + 114, 31)
    return date(year, month, day + 1)


def good_friday(year: int) -> date:
    return easter_sunday(year) - timedelta(days=2)


def nth_weekday(year: int, month: int, weekday: int, n: int) -> date:
    """The n-th `weekday` (0=Monday..6=Sunday) of the month; n=-1 is the last one, -2 the one before."""
    if n == 0:
        raise ValueError("nth_weekday: n must be non-zero")
    if n > 0:
        first = date(year, month, 1)
        d = first + timedelta(days=(weekday - first.weekday()) % 7 + 7 * (n - 1))
    else:
        last = _last_day_of_month(year, month)
        d = last - timedelta(days=(last.weekday() - weekday) % 7 + 7 * (-n - 1))
    if d.month != month:
        raise ValueError(f"nth_weekday: month {year}-{month:02d} has no weekday #{n} of type {weekday}")
    return d


def third_friday(year: int, month: int) -> date:
    return nth_weekday(year, month, FRIDAY, 3)


def third_wednesday(year: int, month: int) -> date:
    return nth_weekday(year, month, WEDNESDAY, 3)


def _last_day_of_month(year: int, month: int) -> date:
    if month == 12:
        return date(year, 12, 31)
    return date(year, month + 1, 1) - timedelta(days=1)


def _observed(d: date, saturday_to_friday: bool = True):
    wd = d.weekday()
    if wd == SUNDAY:
        return d + timedelta(days=1)
    if wd == SATURDAY:
        return d - timedelta(days=1) if saturday_to_friday else None
    return d


@lru_cache(maxsize=None)
def nyse_holidays(year: int) -> frozenset:
    """NYSE full-day closures (weekday dates) in `year`, scheduled holidays plus SPECIAL_CLOSURES."""
    hs = set()

    def add(d):
        if d is not None and d.year == year and d.weekday() < 5:
            hs.add(d)

    add(_observed(date(year, 1, 1), saturday_to_friday=False))
    if year >= 1998:
        add(nth_weekday(year, 1, MONDAY, 3))          # Martin Luther King Jr. Day
    add(nth_weekday(year, 2, MONDAY, 3))              # Washington's Birthday (Presidents Day)
    add(good_friday(year))
    add(nth_weekday(year, 5, MONDAY, -1))             # Memorial Day
    if year >= 2022:
        add(_observed(date(year, 6, 19)))             # Juneteenth
    add(_observed(date(year, 7, 4)))                  # Independence Day
    add(nth_weekday(year, 9, MONDAY, 1))              # Labor Day
    add(nth_weekday(year, 11, THURSDAY, 4))           # Thanksgiving
    add(_observed(date(year, 12, 25)))                # Christmas
    hs.update(d for d in SPECIAL_CLOSURES if d.year == year)
    return frozenset(hs)


def is_holiday(d) -> bool:
    d = _as_date(d)
    return d in nyse_holidays(d.year)


def is_trading_day(d) -> bool:
    """True when the NYSE has a regular (possibly shortened) session on `d`."""
    d = _as_date(d)
    return d.weekday() < 5 and d not in nyse_holidays(d.year)


def _like(src, d: date):
    """Return `d` as the same kind (date or datetime at 00:00) as `src`."""
    return datetime(d.year, d.month, d.day) if isinstance(src, datetime) else d


def previous_trading_day(d, inclusive: bool = False):
    """The last trading day before `d` (or on `d` when inclusive). Same type as `d` (datetime -> 00:00)."""
    x = _as_date(d)
    if not inclusive:
        x -= timedelta(days=1)
    while not is_trading_day(x):
        x -= timedelta(days=1)
    return _like(d, x)


def next_trading_day(d, inclusive: bool = False):
    """The first trading day after `d` (or on `d` when inclusive). Same type as `d` (datetime -> 00:00)."""
    x = _as_date(d)
    if not inclusive:
        x += timedelta(days=1)
    while not is_trading_day(x):
        x += timedelta(days=1)
    return _like(d, x)


def add_trading_days(d, n: int):
    """`d` moved by n trading days (n<0 goes back). n=0 returns `d` itself if it trades, else the next trading day."""
    x = _as_date(d)
    if n == 0:
        return next_trading_day(d, inclusive=True)
    step = 1 if n > 0 else -1
    left = abs(n)
    while left:
        x += timedelta(days=step)
        if is_trading_day(x):
            left -= 1
    return _like(d, x)


def trading_days(start, end) -> list:
    """Trading days in [start, end], inclusive, as dates."""
    s, e = _as_date(start), _as_date(end)
    out = []
    x = s
    while x <= e:
        if is_trading_day(x):
            out.append(x)
        x += timedelta(days=1)
    return out


def last_business_day(year: int, month: int) -> date:
    """Last NYSE trading day of the month."""
    return previous_trading_day(_last_day_of_month(year, month), inclusive=True)


def nth_last_business_day(year: int, month: int, n: int) -> date:
    """n=1 is the last trading day of the month, n=2 the one before, ..."""
    if n < 1:
        raise ValueError("nth_last_business_day: n must be >= 1")
    d = last_business_day(year, month)
    for _ in range(n - 1):
        d = previous_trading_day(d)
    if d.month != month:
        raise ValueError(f"month {year}-{month:02d} has fewer than {n} trading days")
    return d


def nth_business_day(year: int, month: int, n: int) -> date:
    """n=1 is the first trading day of the month."""
    if n < 1:
        raise ValueError("nth_business_day: n must be >= 1")
    d = next_trading_day(date(year, month, 1), inclusive=True)
    for _ in range(n - 1):
        d = next_trading_day(d)
    if d.month != month:
        raise ValueError(f"month {year}-{month:02d} has fewer than {n} trading days")
    return d


def early_close(d):
    """13:00 for an NYSE early-close day (modern rule, see module header), else None."""
    d = _as_date(d)
    if not is_trading_day(d):
        return None
    wd = d.weekday()
    if d.month == 7 and d.day == 3 and wd <= THURSDAY:
        return EARLY_CLOSE
    if d.month == 11 and wd == FRIDAY and d - timedelta(days=1) == nth_weekday(d.year, 11, THURSDAY, 4):
        return EARLY_CLOSE
    if d.month == 12 and d.day == 24 and wd <= THURSDAY:
        return EARLY_CLOSE
    return None


def market_close_time(d):
    """Regular session close time on `d` (16:00, or 13:00 on early-close days); None when closed."""
    if not is_trading_day(d):
        return None
    return early_close(d) or MARKET_CLOSE
