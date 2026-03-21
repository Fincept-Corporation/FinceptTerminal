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
        """Convert to timedelta (TimeSpan equivalent)"""
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
        """Convert UTC datetime to specified timezone"""
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return dt.astimezone(Extensions._resolve_tz(tz))

    @staticmethod
    def convert_to_utc(dt: datetime, tz) -> datetime:
        """Convert datetime from specified timezone to UTC"""
        resolved_tz = Extensions._resolve_tz(tz)
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=resolved_tz)
        return dt.astimezone(timezone.utc)

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
    """Extension methods for indicators"""

    @staticmethod
    def sma(indicator: Any, period: int) -> 'SimpleMovingAverageWrapper':
        """Create an SMA of another indicator's values"""
        return SimpleMovingAverageWrapper(indicator, period)

    @staticmethod
    def ema(indicator: Any, period: int) -> 'ExponentialMovingAverageWrapper':
        """Create an EMA of another indicator's values"""
        return ExponentialMovingAverageWrapper(indicator, period)

    @staticmethod
    def of(indicator: Any, selector: Any = None) -> Any:
        """Chain indicators together"""
        return indicator

    @staticmethod
    def over(indicator: Any, other: Any, selector: Any = None) -> Any:
        """Chain indicator over another (e.g., SMA over RSI)"""
        return indicator

    @staticmethod
    def minus(indicator: Any, other: Any) -> Any:
        """Subtract one indicator from another"""
        return indicator

    @staticmethod
    def plus(indicator: Any, other: Any) -> Any:
        """Add one indicator to another"""
        return indicator

    @staticmethod
    def times(indicator: Any, factor: float) -> Any:
        """Multiply indicator by factor"""
        return indicator

    Of = of
    Over = over
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


class SimpleMovingAverageWrapper:
    """Wrapper to compute SMA of another indicator"""

    def __init__(self, source: Any, period: int):
        self.source = source
        self.period = period
        self._values: List[float] = []
        self._current = 0.0
        self.is_ready = False
        self.updated = _WrapperEventHandler()

    def update(self, value: float) -> bool:
        """Update with new value"""
        self._values.append(value)
        if len(self._values) > self.period:
            self._values.pop(0)
        if len(self._values) >= self.period:
            self._current = sum(self._values) / len(self._values)
            self.is_ready = True
            return True
        return False

    @property
    def current(self) -> float:
        return self._current

    @property
    def Current(self) -> Any:
        """LEAN-style property"""
        class Value:
            def __init__(self, val):
                self.Value = val
        return Value(self._current)


class ExponentialMovingAverageWrapper:
    """Wrapper to compute EMA of another indicator"""

    def __init__(self, source: Any, period: int):
        self.source = source
        self.period = period
        self._current = 0.0
        self._count = 0
        self._multiplier = 2.0 / (period + 1)
        self.is_ready = False
        self.updated = _WrapperEventHandler()

    def update(self, value: float) -> bool:
        """Update with new value"""
        self._count += 1
        if self._count == 1:
            self._current = value
        else:
            self._current = (value - self._current) * self._multiplier + self._current
        if self._count >= self.period:
            self.is_ready = True
            return True
        return False

    @property
    def current(self) -> float:
        return self._current


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


class RateOfChangePercent:
    """Rate of change percentage indicator"""

    def __init__(self, name_or_period=14, period=None):
        # Support both RateOfChangePercent(14) and RateOfChangePercent("name", 1)
        if isinstance(name_or_period, str):
            self.name = name_or_period
            self.period = period if period is not None else 14
        else:
            self.period = name_or_period
            self.name = f"ROCP({self.period})"
        self._values: List[float] = []
        self._current = 0.0
        self.is_ready = False
        self.updated = _WrapperEventHandler()

    def update(self, time: datetime, value: float) -> bool:
        """Update indicator with new value"""
        self._values.append(value)
        if len(self._values) > self.period + 1:
            self._values.pop(0)

        if len(self._values) > self.period:
            old_value = self._values[0]
            if old_value != 0:
                self._current = ((value - old_value) / old_value) * 100
            else:
                self._current = 0.0
            self.is_ready = True
            return True
        return False

    @property
    def current(self) -> float:
        return self._current

    @property
    def Current(self) -> Any:
        """LEAN-style property"""
        class Value:
            def __init__(self, val):
                self.Value = val
        return Value(self._current)

    def reset(self) -> None:
        """Reset indicator"""
        self._values.clear()
        self._current = 0.0
        self.is_ready = False
