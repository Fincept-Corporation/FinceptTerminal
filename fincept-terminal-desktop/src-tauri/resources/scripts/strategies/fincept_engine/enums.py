# ============================================================================
# Fincept Terminal - Strategy Engine Enums
# Pure Python replacement for LEAN C# enums
# ============================================================================

from enum import Enum, IntEnum


class Resolution(IntEnum):
    TICK = 0
    SECOND = 1
    MINUTE = 2
    HOUR = 3
    DAILY = 4

    # Aliases for compatibility
    @classmethod
    def _missing_(cls, value):
        aliases = {
            'tick': cls.TICK, 'second': cls.SECOND,
            'minute': cls.MINUTE, 'hour': cls.HOUR,
            'daily': cls.DAILY, 'day': cls.DAILY
        }
        if isinstance(value, str):
            return aliases.get(value.lower())
        return None

    def to_timedelta(self):
        from datetime import timedelta
        mapping = {
            Resolution.TICK: timedelta(milliseconds=1),
            Resolution.SECOND: timedelta(seconds=1),
            Resolution.MINUTE: timedelta(minutes=1),
            Resolution.HOUR: timedelta(hours=1),
            Resolution.DAILY: timedelta(days=1),
        }
        return mapping[self]

    def to_pandas_freq(self):
        mapping = {
            Resolution.SECOND: '1s',
            Resolution.MINUTE: '1min',
            Resolution.HOUR: '1h',
            Resolution.DAILY: '1D',
        }
        return mapping.get(self, '1min')


class SecurityType(IntEnum):
    BASE = 0
    EQUITY = 1
    OPTION = 2
    COMMODITY = 3
    FOREX = 4
    FUTURE = 5
    CFD = 6
    CRYPTO = 7
    INDEX = 8
    FUTURE_OPTION = 9
    INDEX_OPTION = 10
    CRYPTO_FUTURE = 11

    Base = 0


class OrderType(IntEnum):
    MARKET = 0
    LIMIT = 1
    STOP_MARKET = 2
    STOP_LIMIT = 3
    MARKET_ON_OPEN = 4
    MARKET_ON_CLOSE = 5
    LIMIT_IF_TOUCHED = 6
    COMBO_MARKET = 7
    COMBO_LIMIT = 8
    COMBO_LEG_LIMIT = 9
    TRAILING_STOP = 10


class OrderStatus(IntEnum):
    NEW = 0
    SUBMITTED = 1
    PARTIALLY_FILLED = 2
    FILLED = 3
    CANCELED = 5
    INVALID = 6
    CANCEL_PENDING = 7
    UPDATE_SUBMITTED = 8


class OrderDirection(IntEnum):
    BUY = 0
    SELL = 1
    HOLD = 2


class Market:
    USA = "usa"
    INDIA = "india"
    NSE = "nse"
    BSE = "bse"
    CME = "cme"
    COMEX = "comex"
    EUREX = "eurex"
    OANDA = "oanda"
    BINANCE = "binance"
    GDAX = "gdax"
    INTERACTIVE_BROKERS = "interactive_brokers"
    KRAKEN = "kraken"
    BYBIT = "bybit"
    MCX = "mcx"
    NFO = "nfo"


class InsightDirection(IntEnum):
    UP = 1
    DOWN = -1
    FLAT = 0


class InsightType(IntEnum):
    PRICE = 0
    VOLATILITY = 1


class TimeInForce:
    GOOD_TIL_CANCELED = "gtc"
    DAY = "day"
    GOOD_TIL_DATE = "gtd"
    IMMEDIATE_OR_CANCEL = "ioc"
    FILL_OR_KILL = "fok"

    @staticmethod
    def good_til_canceled():
        return TimeInForce.GOOD_TIL_CANCELED

    @staticmethod
    def day():
        return TimeInForce.DAY


class DataNormalizationMode(IntEnum):
    RAW = 0
    ADJUSTED = 1
    SPLIT_ADJUSTED = 2
    TOTAL_RETURN = 3
