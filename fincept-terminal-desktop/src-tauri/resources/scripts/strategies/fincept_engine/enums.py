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
    BACKWARDS_RATIO = 4
    BACKWARDS_PANAMA_CANAL = 5
    FORWARD_PANAMA_CANAL = 6

    # Aliases
    Raw = 0
    Adjusted = 1
    SplitAdjusted = 2
    TotalReturn = 3
    BackwardsRatio = 4
    BackwardsPanamaCanal = 5
    ForwardPanamaCanal = 6


class MovingAverageType(IntEnum):
    """Type of moving average for indicators"""
    SIMPLE = 0
    EXPONENTIAL = 1
    WILDERS = 2
    LINEAR_WEIGHTED = 3
    DOUBLE_EXPONENTIAL = 4
    TRIPLE_EXPONENTIAL = 5
    TRIANGULAR = 6
    T3 = 7
    KAMA = 8
    HULL = 9
    ALMA = 10

    # Aliases
    Simple = 0
    Exponential = 1
    Wilders = 2
    LinearWeightedMovingAverage = 3
    DoubleExponential = 4
    TripleExponential = 5
    Triangular = 6
    Kama = 8
    Hull = 9


class OptionRight(IntEnum):
    """Option right (call or put)"""
    CALL = 0
    PUT = 1

    # Aliases
    Call = 0
    Put = 1


class OptionStyle(IntEnum):
    """Option exercise style"""
    AMERICAN = 0
    EUROPEAN = 1

    # Aliases
    American = 0
    European = 1


class BrokerageName(IntEnum):
    """Brokerage names for configuration"""
    DEFAULT = 0
    INTERACTIVE_BROKERS_BROKERAGE = 1
    TRADIER_BROKERAGE = 2
    OANDA_BROKERAGE = 3
    FXCM_BROKERAGE = 4
    BITFINEX_BROKERAGE = 5
    BINANCE_BROKERAGE = 6
    GDAX_BROKERAGE = 7
    ALPACA_BROKERAGE = 8
    ZERODHA_BROKERAGE = 9
    SAMCO_BROKERAGE = 10
    KRAKEN_BROKERAGE = 11
    FYERS_BROKERAGE = 12
    BYBIT = 13
    BYBIT_BROKERAGE = 13
    COINBASE = 14
    AXOS = 15
    AXOS_BROKERAGE = 15
    GDAX = 7
    BINANCE_FUTURES = 16
    BINANCE_COIN_FUTURES = 17
    EXANTE = 18
    FTX = 19
    FTX_US = 20
    WOLVERINE = 21
    TD_AMERITRADE = 22
    TERMINAL_LINK = 23
    TRADING_TECHNOLOGIES = 24
    QUANTCONNECT = 25
    ALPHA_STREAMS = 26
    BINANCE = 6

    # Aliases
    Default = 0
    InteractiveBrokersBrokerage = 1
    TradierBrokerage = 2
    OandaBrokerage = 3
    FxcmBrokerage = 4
    BitfinexBrokerage = 5
    BinanceBrokerage = 6
    GDAXBrokerage = 7
    AlpacaBrokerage = 8
    ZerodhaBrokerage = 9
    SamcoBrokerage = 10
    KrakenBrokerage = 11
    FyersBrokerage = 12
    BybitBrokerage = 13
    CoinbaseBrokerage = 14
    AxosBrokerage = 15
    BinanceFuturesBrokerage = 16
    BinanceCoinFuturesBrokerage = 17
    ExanteBrokerage = 18
    FTXBrokerage = 19
    FTXUSBrokerage = 20
    WolverineBrokerage = 21
    TDAmeritradeBrokerage = 22
    TerminalLinkBrokerage = 23
    TradingTechnologiesBrokerage = 24
    QuantConnectBrokerage = 25
    AlphaStreamsBrokerage = 26
    Binance = 6


class AccountType(IntEnum):
    """Account type for trading"""
    MARGIN = 0
    CASH = 1

    # Aliases
    Margin = 0
    Cash = 1


class PositionSide(IntEnum):
    """Position side"""
    LONG = 0
    SHORT = 1

    # Aliases
    Long = 0
    Short = 1


class SettlementType(IntEnum):
    """Settlement type for trades"""
    IMMEDIATE = 0
    DELAYED = 1

    # Aliases
    Immediate = 0
    Delayed = 1
