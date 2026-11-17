# ============================================================================
# Fincept Terminal - Strategy Engine: market hours (LEAN MarketHoursDatabase, simplified)
#
# Regular and extended sessions per asset class, in exchange-local time:
#   US equities / ETFs / US index options: 09:30-16:00 regular, 04:00-20:00 extended, NYSE
#     holidays and early closes (market_calendar.py)
#   US indices: 09:30-16:15 (the cash index prints until 16:15 for options)
#   CME-style futures (and future options): Sunday 18:00 - Friday 17:00 with a daily
#     17:00-18:00 break; the regular session is 09:30-17:00 for equity-index roots
#   Forex / CFDs: Sunday 17:00 - Friday 17:00 (New York)
#   Crypto: always open
#   India (NSE/BSE): 09:15-15:30 IST on weekdays
# The engine steps on daily bars: on_data runs at the close of the session (16:00 New York for
# equities), and that moment counts as inside the session (the close is inclusive), so a
# strategy that checks `is_market_open` from on_data can act on the closing price.
# ============================================================================

from datetime import datetime, time, timedelta, date

from .enums import SecurityType

_EQ_REG = (time(9, 30), time(16, 0))
_EQ_EXT = (time(4, 0), time(20, 0))
_IDX_REG = (time(9, 30), time(16, 15))
_IN_REG = (time(9, 15), time(15, 30))


class ExchangeHours:
    """Exchange hours of one asset class (is_open, is_date_open, next open/close)."""

    def __init__(self, kind='equity', time_zone='America/New_York'):
        self.kind = kind
        self.time_zone = time_zone

    # ---- factories ----
    @staticmethod
    def always_open(time_zone=None):
        return ExchangeHours('always', time_zone or 'UTC')

    AlwaysOpen = always_open

    @staticmethod
    def for_security_type(security_type, market=None):
        st = security_type
        m = str(market or '').lower()
        if st in (SecurityType.CRYPTO, SecurityType.CRYPTO_FUTURE):
            return ExchangeHours('always', 'UTC')
        if st in (SecurityType.FOREX, SecurityType.CFD):
            return ExchangeHours('fx')
        if st in (SecurityType.FUTURE, SecurityType.FUTURE_OPTION):
            return ExchangeHours('futures', 'America/Chicago')
        if st == SecurityType.INDEX or st == SecurityType.INDEX_OPTION:
            return ExchangeHours('index')
        if m in ('india', 'nse', 'bse', 'nfo', 'mcx'):
            return ExchangeHours('india', 'Asia/Kolkata')
        if st == SecurityType.BASE:
            return ExchangeHours('always')
        return ExchangeHours('equity')

    # ---- calendar ----
    def _trading_day(self, d) -> bool:
        if self.kind == 'always':
            return True
        dd = d.date() if isinstance(d, datetime) else d
        if self.kind == 'fx':
            return dd.weekday() < 5 or dd.weekday() == 6
        if self.kind == 'futures':
            return dd.weekday() < 5 or dd.weekday() == 6
        if self.kind == 'india':
            return dd.weekday() < 5
        from .market_calendar import is_trading_day
        return is_trading_day(dd)

    def is_date_open(self, d, extended_market_hours=False) -> bool:
        if self.kind in ('fx', 'futures'):
            dd = d.date() if isinstance(d, datetime) else d
            return dd.weekday() < 5
        return self._trading_day(d)

    def _session(self, d, extended):
        if self.kind == 'equity':
            return _EQ_EXT if extended else _EQ_REG
        if self.kind == 'index':
            return _IDX_REG
        if self.kind == 'india':
            return _IN_REG
        return None

    def is_open(self, t: datetime, extended_market_hours=False, *args) -> bool:
        """Whether the exchange trades at local time t (the session close counts as open)."""
        if isinstance(t, datetime) is False:
            return True
        if self.kind == 'always':
            return True
        wd = t.weekday()
        tod = t.time()
        if self.kind == 'fx':
            if wd == 5:
                return False
            if wd == 6:
                return tod >= time(17, 0)
            if wd == 4:
                return tod <= time(17, 0)
            return True
        if self.kind == 'futures':
            if wd == 5:
                return False
            if wd == 6:
                return tod >= time(18, 0)
            if wd == 4 and tod > time(17, 0):
                return False
            if not extended_market_hours:
                return time(9, 30) <= tod <= time(17, 0)
            return not (time(17, 0) < tod < time(18, 0))
        if not self._trading_day(t):
            return False
        a, b = self._session(t, extended_market_hours)
        return a <= tod <= b

    def get_next_market_open(self, t: datetime, extended_market_hours=False) -> datetime:
        x = t
        for _ in range(60 * 24 * 10 // 30):
            x = x + timedelta(minutes=30) if x != t else x + timedelta(minutes=1)
            if self.is_open(x, extended_market_hours) and not self.is_open(x - timedelta(minutes=1),
                                                                           extended_market_hours):
                return x
        return t

    def get_next_market_close(self, t: datetime, extended_market_hours=False) -> datetime:
        if self.kind == 'always':
            return datetime(t.year, t.month, t.day) + timedelta(days=1)
        d = datetime(t.year, t.month, t.day)
        for _ in range(10):
            if self._trading_day(d):
                s = self._session(d, extended_market_hours)
                close = d + timedelta(hours=s[1].hour, minutes=s[1].minute) if s else d + timedelta(hours=17)
                if close > t:
                    return close
            d += timedelta(days=1)
        return t

    def get_previous_trading_day(self, d):
        x = d - timedelta(days=1)
        while not self._trading_day(x):
            x -= timedelta(days=1)
        return x

    def get_next_trading_day(self, d):
        x = d + timedelta(days=1)
        while not self._trading_day(x):
            x += timedelta(days=1)
        return x

    @property
    def regular_market_duration(self):
        s = self._session(datetime(2020, 1, 2), False)
        if s is None:
            return timedelta(hours=24) if self.kind == 'always' else timedelta(hours=7, minutes=30)
        return datetime.combine(date(2020, 1, 2), s[1]) - datetime.combine(date(2020, 1, 2), s[0])

    IsOpen = is_open
    IsDateOpen = is_date_open
    GetNextMarketOpen = get_next_market_open
    GetNextMarketClose = get_next_market_close
