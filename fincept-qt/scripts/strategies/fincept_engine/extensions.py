# ============================================================================
# Fincept Terminal - Strategy Engine Extensions
# Pure Python replacement for LEAN C# extension methods
# ============================================================================

from datetime import datetime, timedelta, time as dt_time, timezone
from typing import Optional, List, Any
import math


class Time:
    """Utility class for time-related operations (replaces LEAN's Time class)"""

    # Common time spans
    ONE_MINUTE = timedelta(minutes=1)
    ONE_HOUR = timedelta(hours=1)
    ONE_DAY = timedelta(days=1)
    ONE_SECOND = timedelta(seconds=1)

    @staticmethod
    def multiply(td: timedelta, factor: float) -> timedelta:
        """Multiply a timedelta by a factor"""
        return timedelta(seconds=td.total_seconds() * factor)

    @staticmethod
    def parse(time_str: str) -> dt_time:
        """Parse a time string to time object"""
        if ':' in time_str:
            parts = time_str.split(':')
            hour = int(parts[0])
            minute = int(parts[1]) if len(parts) > 1 else 0
            second = int(parts[2]) if len(parts) > 2 else 0
            return dt_time(hour, minute, second)
        return dt_time(int(time_str), 0, 0)

    @staticmethod
    def end_of_day(dt: datetime) -> datetime:
        """Get end of day for a datetime"""
        return dt.replace(hour=23, minute=59, second=59, microsecond=999999)

    @staticmethod
    def start_of_day(dt: datetime) -> datetime:
        """Get start of day for a datetime"""
        return dt.replace(hour=0, minute=0, second=0, microsecond=0)

    @staticmethod
    def each_tradeable_day_in_time_zone(exchange_hours, start, end, time_zone=None, include_extended=False):
        """Yield each tradeable day between start and end."""
        current = start
        while current <= end:
            if current.weekday() < 5:  # Mon-Fri
                yield current
            current += timedelta(days=1)

    EachTradeableDayInTimeZone = each_tradeable_day_in_time_zone


class Expiry:
    """LEAN Expiry helpers: functions from an insight's generation time to its expiry time."""

    @staticmethod
    def _add_months(dt, n):
        import calendar
        m = dt.month - 1 + n
        y = dt.year + m // 12
        m = m % 12 + 1
        d = min(dt.day, calendar.monthrange(y, m)[1])
        return dt.replace(year=y, month=m, day=d)

    @staticmethod
    def ONE_MONTH(dt):
        return Expiry._add_months(dt, 1)

    @staticmethod
    def ONE_QUARTER(dt):
        return Expiry._add_months(dt, 3)

    @staticmethod
    def ONE_YEAR(dt):
        return Expiry._add_months(dt, 12)

    @staticmethod
    def END_OF_DAY(dt):
        return datetime(dt.year, dt.month, dt.day) + timedelta(days=1)

    @staticmethod
    def END_OF_WEEK(dt):
        # .NET DayOfWeek: Sunday = 0 ... Saturday = 6; next Monday at midnight
        dow = (dt.weekday() + 1) % 7
        value = 8 - dow
        if value == 8:
            value = 1
        return datetime(dt.year, dt.month, dt.day) + timedelta(days=value)

    @staticmethod
    def END_OF_MONTH(dt):
        nxt = Expiry._add_months(dt, 1)
        return datetime(nxt.year, nxt.month, 1)

    @staticmethod
    def END_OF_QUARTER(dt):
        first = (dt.month - 1) // 3 * 3 + 1
        return Expiry._add_months(datetime(dt.year, first, 1), 3)

    @staticmethod
    def END_OF_YEAR(dt):
        return datetime(dt.year + 1, 1, 1)

    OneMonth = ONE_MONTH
    OneQuarter = ONE_QUARTER
    OneYear = ONE_YEAR
    EndOfDay = END_OF_DAY
    EndOfWeek = END_OF_WEEK
    EndOfMonth = END_OF_MONTH
    EndOfQuarter = END_OF_QUARTER
    EndOfYear = END_OF_YEAR


class Math:
    """C# System.Math (LEAN's AlgorithmImports brings it in)."""
    PI = math.pi
    E = math.e

    @staticmethod
    def _r(x, digits=0):
        return round(x, digits)

    sin = staticmethod(math.sin)
    cos = staticmethod(math.cos)
    tan = staticmethod(math.tan)
    asin = staticmethod(math.asin)
    acos = staticmethod(math.acos)
    atan = staticmethod(math.atan)
    atan2 = staticmethod(math.atan2)
    sqrt = staticmethod(math.sqrt)
    exp = staticmethod(math.exp)
    log10 = staticmethod(math.log10)
    floor = staticmethod(math.floor)
    ceiling = staticmethod(math.ceil)
    abs = staticmethod(abs)
    max = staticmethod(max)
    min = staticmethod(min)
    pow = staticmethod(math.pow)

    @staticmethod
    def log(x, base=None):
        return math.log(x) if base is None else math.log(x, base)

    @staticmethod
    def round(x, digits=0):
        return round(x, digits)

    @staticmethod
    def sign(x):
        return (x > 0) - (x < 0)

    @staticmethod
    def truncate(x):
        return float(int(x))

    Sin, Cos, Tan, Sqrt, Exp, Log, Log10, Floor, Ceiling, Abs, Max, Min, Pow, Round, Sign, Truncate = (
        sin, cos, tan, sqrt, exp, log, log10, floor, ceiling, abs, max, min, pow, round, sign, truncate)


class TimeZones:
    """Common timezone definitions (replaces LEAN's TimeZones)"""

    NEW_YORK = timezone(timedelta(hours=-5))
    CHICAGO = timezone(timedelta(hours=-6))
    LOS_ANGELES = timezone(timedelta(hours=-8))
    LONDON = timezone(timedelta(hours=0))
    TOKYO = timezone(timedelta(hours=9))
    HONG_KONG = timezone(timedelta(hours=8))
    SINGAPORE = timezone(timedelta(hours=8))
    SYDNEY = timezone(timedelta(hours=10))
    MUMBAI = timezone(timedelta(hours=5, minutes=30))
    UTC = timezone.utc

    # Aliases
    NewYork = NEW_YORK
    Chicago = CHICAGO
    LosAngeles = LOS_ANGELES
    London = LONDON
    Tokyo = TOKYO
    HongKong = HONG_KONG
    Singapore = SINGAPORE
    Sydney = SYDNEY
    Mumbai = MUMBAI
    Utc = UTC

    @staticmethod
    def get_timezone(name: str) -> timezone:
        """Get timezone by name"""
        tz_map = {
            'new_york': TimeZones.NEW_YORK,
            'chicago': TimeZones.CHICAGO,
            'los_angeles': TimeZones.LOS_ANGELES,
            'london': TimeZones.LONDON,
            'tokyo': TimeZones.TOKYO,
            'hong_kong': TimeZones.HONG_KONG,
            'singapore': TimeZones.SINGAPORE,
            'sydney': TimeZones.SYDNEY,
            'mumbai': TimeZones.MUMBAI,
            'utc': TimeZones.UTC,
        }
        return tz_map.get(name.lower().replace(' ', '_'), TimeZones.UTC)


class Extensions:
    """Extension methods for various LEAN types (static utility class)"""

    @staticmethod
    def to_time_span(minutes: int = 0, hours: int = 0, days: int = 0,
                     seconds: int = 0) -> timedelta:
        """Convert to timedelta (TimeSpan equivalent); Extensions.to_time_span(resolution) is the
        resolution's bar period (LEAN Resolution.ToTimeSpan)."""
        if type(minutes).__name__ == 'Resolution':
            return {0: timedelta(0), 1: timedelta(seconds=1), 2: timedelta(minutes=1),
                    3: timedelta(hours=1), 4: timedelta(days=1)}[int(minutes)]
        return timedelta(days=days, hours=hours, minutes=minutes, seconds=seconds)

    @staticmethod
    def _resolve_tz(tz):
        """Resolve timezone: accept string or tzinfo object."""
        if isinstance(tz, str):
            try:
                from zoneinfo import ZoneInfo
                return ZoneInfo(tz)
            except (ImportError, KeyError):
                return timezone.utc
        return tz if tz is not None else timezone.utc

    @staticmethod
    def convert_from_utc(dt: datetime, tz) -> datetime:
        """UTC -> exchange time. The engine keeps one naive clock (algorithm.utc_time == algorithm.time,
        New York exchange time), so a naive time is returned unchanged and an aware one is converted and
        made naive - comparisons with bar times never mix naive and aware datetimes."""
        if dt is None or dt.tzinfo is None:
            return dt
        return dt.astimezone(Extensions._resolve_tz(tz)).replace(tzinfo=None)

    @staticmethod
    def convert_to_utc(dt: datetime, tz) -> datetime:
        """Exchange time -> UTC on the engine's single naive clock (see convert_from_utc)."""
        if dt is None or dt.tzinfo is None:
            return dt
        return dt.astimezone(timezone.utc).replace(tzinfo=None)

    @staticmethod
    def read_lines(file_path: str) -> List[str]:
        """Read all lines from a file"""
        try:
            with open(file_path, 'r') as f:
                return f.readlines()
        except Exception:
            return []

    @staticmethod
    def round_to_significant_digits(value: float, digits: int) -> float:
        """Round a number to specified significant digits"""
        if value == 0:
            return 0
        return round(value, digits - int(math.floor(math.log10(abs(value)))) - 1)

    @staticmethod
    def is_null_or_empty(s: Optional[str]) -> bool:
        """Check if string is null or empty"""
        return s is None or len(s) == 0

    @staticmethod
    def safe_multiply(a: float, b: float) -> float:
        """Safe multiplication handling edge cases"""
        if math.isnan(a) or math.isnan(b):
            return float('nan')
        if math.isinf(a) or math.isinf(b):
            return float('inf') if (a > 0) == (b > 0) else float('-inf')
        return a * b

    @staticmethod
    def is_option(security_type) -> bool:
        """Check if security type is an option type."""
        val = int(security_type) if hasattr(security_type, '__int__') else security_type
        return val in (2, 9, 10)  # OPTION, FUTURE_OPTION, INDEX_OPTION

    @staticmethod
    def get_order_direction(quantity) -> int:
        """Get order direction from quantity."""
        if quantity > 0:
            return 0  # BUY
        elif quantity < 0:
            return 1  # SELL
        return 2  # HOLD

    # PascalCase aliases
    IsOption = is_option
    GetOrderDirection = get_order_direction


class IndicatorExtensions:
    """LEAN IndicatorExtensions: chain and combine indicators.

    of(second, first)      -> `second` is updated with every new value of `first`; returns second
    sma/ema(indicator, n)  -> an SMA/EMA of the indicator's values
    over/minus/plus/times  -> composite indicators, recomputed when both inputs have updated
    """

    @staticmethod
    def _wire(target, source, wait_for_first_to_ready=False):
        from .indicators import feed_indicator, IndicatorDataPoint

        def _on(sender, point):
            if wait_for_first_to_ready and not getattr(source, 'is_ready', True):
                return
            feed_indicator(target, IndicatorDataPoint(float(point.value), getattr(point, 'time', None)))
        source.updated += _on
        return target

    @staticmethod
    def of(second: Any, first: Any, wait_for_first_to_ready: bool = False) -> Any:
        return IndicatorExtensions._wire(second, first, wait_for_first_to_ready)

    @staticmethod
    def sma(indicator: Any, period: int, wait_for_first_to_ready: bool = False) -> Any:
        from .indicators import SimpleMovingAverage
        name = f"SMA{period}_{getattr(indicator, 'name', 'IND')}"
        return IndicatorExtensions._wire(SimpleMovingAverage(name, int(period)), indicator, wait_for_first_to_ready)

    @staticmethod
    def ema(indicator: Any, period: int, smoothing_factor: float = None, wait_for_first_to_ready: bool = False) -> Any:
        from .indicators import ExponentialMovingAverage
        name = f"EMA{period}_{getattr(indicator, 'name', 'IND')}"
        return IndicatorExtensions._wire(ExponentialMovingAverage(name, int(period), smoothing_factor),
                                         indicator, wait_for_first_to_ready)

    @staticmethod
    def _composite(left: Any, right: Any, op, sym: str) -> Any:
        from .indicators import CompositeIndicator, ConstantIndicator
        if isinstance(right, (int, float)):
            right = ConstantIndicator(right)
        name = f"{getattr(left, 'name', 'L')}{sym}{getattr(right, 'name', 'R')}"
        return CompositeIndicator(name, left, right, op)

    @staticmethod
    def over(left: Any, right: Any, name: str = None) -> Any:
        return IndicatorExtensions._composite(left, right, lambda a, b: a / b if b else 0.0, '/')

    @staticmethod
    def minus(left: Any, right: Any, name: str = None) -> Any:
        return IndicatorExtensions._composite(left, right, lambda a, b: a - b, '-')

    @staticmethod
    def plus(left: Any, right: Any, name: str = None) -> Any:
        return IndicatorExtensions._composite(left, right, lambda a, b: a + b, '+')

    @staticmethod
    def times(left: Any, right: Any, name: str = None) -> Any:
        return IndicatorExtensions._composite(left, right, lambda a, b: a * b, '*')

    Of = of
    Over = over
    Minus = minus
    Plus = plus
    Times = times
    SMA = sma
    EMA = ema


class _WrapperEventHandler:
    """Supports += and -= for wrapper event registration."""
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


class BuyingPowerModelExtensions:
    """Extension methods for buying power models"""

    @staticmethod
    def get_leverage(security: Any) -> float:
        """Get leverage for a security"""
        if hasattr(security, 'leverage'):
            return security.leverage
        return 1.0

    @staticmethod
    def set_leverage(security: Any, leverage: float) -> None:
        """Set leverage for a security"""
        if hasattr(security, 'set_leverage'):
            security.set_leverage(leverage)


class _BrokerageMarkets(dict):
    """Dict that supports both SecurityType enum and string keys for default_markets."""
    _STR_MAP = {
        'equity': 1, 'option': 2, 'forex': 4, 'future': 5, 'crypto': 7,
        'index': 6, 'index_option': 12, 'future_option': 13, 'cfd': 8,
    }

    def __getitem__(self, key):
        # Try direct lookup first
        try:
            return super().__getitem__(key)
        except KeyError:
            pass
        # Try by string name
        if isinstance(key, str):
            for k, v in super().items():
                if str(k).lower().replace('securitytype.', '') == key.lower():
                    return v
        # Try by int value
        if isinstance(key, int):
            for k, v in super().items():
                if getattr(k, 'value', k) == key:
                    return v
        raise KeyError(key)

    def __contains__(self, key):
        try:
            self[key]
            return True
        except KeyError:
            return False


class DefaultBrokerageModel:
    """Default brokerage model implementation"""

    def __init__(self, account_type: int = 0):
        from .enums import AccountType, SecurityType, Market
        self.account_type = account_type
        # Support both string keys ('equity') and SecurityType enum keys
        self.default_markets = _BrokerageMarkets({
            SecurityType.EQUITY: Market.USA,
            SecurityType.OPTION: Market.USA,
            SecurityType.FUTURE: Market.CME,
            SecurityType.FOREX: Market.OANDA,
            SecurityType.CRYPTO: Market.BINANCE,
            SecurityType.INDEX: Market.USA,
            SecurityType.INDEX_OPTION: Market.USA,
            SecurityType.FUTURE_OPTION: Market.CME,
            SecurityType.CFD: Market.OANDA,
        })

    def get_leverage(self, security: Any) -> float:
        """Get default leverage for security type"""
        leverage_map = {
            1: 2.0,   # EQUITY
            2: 1.0,   # OPTION
            4: 50.0,  # FOREX
            5: 1.0,   # FUTURE
            7: 1.0,   # CRYPTO
        }
        sec_type = getattr(security, 'security_type', 1)
        return leverage_map.get(sec_type, 1.0)

    def can_submit_order(self, security: Any, order: Any) -> bool:
        """Check if order can be submitted"""
        return True

    def can_update_order(self, security: Any, order: Any) -> bool:
        """Check if order can be updated"""
        return True


from .indicators import MomentumPercent as _MomentumPercent  # noqa: E402


class RateOfChangePercent(_MomentumPercent):
    """ROCP = 100 * (value - value[period ago]) / value[period ago].
    Accepts RateOfChangePercent(period) and RateOfChangePercent(name, period)."""

    def __init__(self, name_or_period=14, period=None):
        if isinstance(name_or_period, str):
            super().__init__(name_or_period, period if period is not None else 14)
        else:
            super().__init__(f"ROCP({name_or_period})", int(name_or_period))
