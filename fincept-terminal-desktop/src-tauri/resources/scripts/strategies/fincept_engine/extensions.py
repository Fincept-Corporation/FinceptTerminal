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
    def convert_from_utc(dt: datetime, tz: timezone) -> datetime:
        """Convert UTC datetime to specified timezone"""
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return dt.astimezone(tz)

    @staticmethod
    def convert_to_utc(dt: datetime, tz: timezone) -> datetime:
        """Convert datetime from specified timezone to UTC"""
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=tz)
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


class SimpleMovingAverageWrapper:
    """Wrapper to compute SMA of another indicator"""

    def __init__(self, source: Any, period: int):
        self.source = source
        self.period = period
        self._values: List[float] = []
        self._current = 0.0
        self.is_ready = False

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


class DefaultBrokerageModel:
    """Default brokerage model implementation"""

    def __init__(self, account_type: int = 0):
        from .enums import AccountType
        self.account_type = account_type
        self.default_markets = {
            'equity': 'usa',
            'option': 'usa',
            'future': 'cme',
            'forex': 'oanda',
            'crypto': 'binance',
        }

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

    def __init__(self, period: int = 14):
        self.period = period
        self._values: List[float] = []
        self._current = 0.0
        self.is_ready = False
        self.name = f"ROCP({period})"

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
