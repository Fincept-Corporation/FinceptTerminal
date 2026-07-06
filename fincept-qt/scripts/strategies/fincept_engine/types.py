# ============================================================================
# Fincept Terminal - Strategy Engine Types
# Core data types: Symbol, Slice, TradeBar, Holdings, Orders, Insights
# ============================================================================

from datetime import datetime, timedelta
from typing import Optional, Dict, List, Any
from .enums import SecurityType, OrderType, OrderStatus, InsightDirection, InsightType, Market


class IndicatorValue:
    """Wrapper for indicator current value."""
    def __init__(self, value: float = 0.0):
        self.value = value

    def __float__(self):
        return self.value

    def __repr__(self):
        return f"IndicatorValue({self.value})"


class Symbol:
    """Represents a security identifier."""

    def __init__(self, ticker: str, security_type: SecurityType = SecurityType.EQUITY,
                 market: str = Market.USA):
        self.value = ticker.upper()
        self.id = ticker.upper()
        self.security_type = security_type
        self.market = market
        # Stub industry-standard identifiers (non-empty for compatibility)
        self.cusip = f"{ticker.upper()[:6]:0<9}"[:9]
        self.sedol = f"{ticker.upper()[:5]:0<7}"[:7]
        self.isin = f"US{ticker.upper()[:6]:0<10}"[:12]
        self.figi = f"BBG{ticker.upper()[:6]:0<9}"[:12]
        self.composite_figi = f"BBG{ticker.upper()[:6]:0<9}"[:12]
        self.cik = abs(hash(ticker.upper())) % 9999999 + 1000000

    def __str__(self):
        return self.value

    def __repr__(self):
        return f"Symbol({self.value})"

    def __eq__(self, other):
        if isinstance(other, Symbol):
            return self.value == other.value
        if isinstance(other, str):
            return self.value == other.upper()
        return False

    def __hash__(self):
        return hash(self.value)

    def upper(self):
        """Allow Symbol to be used where string.upper() is called."""
        return self.value

    @staticmethod
    def create(ticker: str, security_type: SecurityType = SecurityType.EQUITY,
               market: str = Market.USA, *args, **kwargs):
        return Symbol(ticker, security_type, market)

    @staticmethod
    def create_future(ticker: str, market: str = "cme", *args, **kwargs):
        """Create a future symbol. Args: ticker, market, [expiry datetime]."""
        sym = Symbol(ticker.upper(), SecurityType.FUTURE, market)
        # Optional expiry datetime
        for a in args:
            if isinstance(a, datetime):
                sym.expiry = a
                break
        return sym

    @staticmethod
    def create_option(underlying, market: str = "usa", *args, **kwargs):
        """Create option symbol. Signature: (underlying, market, style, right, strike, expiry).
        underlying can be a string or Symbol."""
        from .enums import OptionRight as _OR
        underlying_sym = underlying if isinstance(underlying, Symbol) else Symbol(str(underlying).upper())
        underlying_ticker = str(underlying_sym).upper()
        # Parse positional args: style, right, strike, expiry
        style = args[0] if len(args) > 0 else 0  # AMERICAN=0
        right = args[1] if len(args) > 1 else _OR.CALL
        strike = args[2] if len(args) > 2 else 0
        expiry = args[3] if len(args) > 3 else None

        # Determine security type based on underlying
        sec_type = SecurityType.OPTION
        if underlying_sym.security_type == SecurityType.FUTURE:
            sec_type = SecurityType.FUTURE_OPTION
        elif underlying_sym.security_type == SecurityType.INDEX:
            sec_type = SecurityType.INDEX_OPTION

        # Generate a consistent ticker for the option
        right_char = 'C' if (hasattr(right, 'value') and right.value == 0) or right == 0 else 'P'
        if expiry:
            ticker_val = f"{underlying_ticker}{expiry.strftime('%y%m%d')}{right_char}{int(strike)}"
        else:
            ticker_val = f"{underlying_ticker}{right_char}{int(strike)}"

        sym = Symbol(ticker_val, sec_type, market)
        sym._underlying = underlying_sym
        sym.strike_price = float(strike)
        sym.right = right
        sym.expiry = expiry
        sym.option_style = style
        sym.id = type('SymbolId', (), {
            'strike_price': float(strike), 'option_right': right,
            'date': expiry, 'expiration': expiry,
            'underlying': underlying_sym
        })()
        return sym

    @staticmethod
    def create_canonical_option(underlying, market: str = "usa", alias=None):
        ticker = str(underlying).upper()
        return Symbol(ticker, SecurityType.OPTION, market)

    @property
    def underlying(self):
        """Get underlying symbol (for options/futures)."""
        return getattr(self, '_underlying', self)

    @underlying.setter
    def underlying(self, value):
        self._underlying = value

    @property
    def has_underlying(self):
        """Check if this symbol has an underlying. True for options/derivatives or if explicitly set."""
        if hasattr(self, '_underlying') and self._underlying is not self:
            return True
        return self.security_type in (SecurityType.OPTION, SecurityType.FUTURE_OPTION,
                                       SecurityType.INDEX_OPTION)

    @property
    def is_canonical(self):
        return not hasattr(self, 'strike_price') or not hasattr(self, 'expiry')


class TradeBar:
    """OHLCV bar data."""

    def __init__(self, time: datetime = None, symbol: Symbol = None,
                 open: float = 0, high: float = 0, low: float = 0,
                 close: float = 0, volume: float = 0):
        self.time = time or datetime.now()
        self.symbol = symbol
        self.open = open
        self.high = high
        self.low = low
        self.close = close
        self.volume = volume
        self.end_time = self.time
        self.value = close
        self.price = close
        self.period = timedelta(minutes=1)

    def __repr__(self):
        return f"TradeBar({self.symbol} O:{self.open} H:{self.high} L:{self.low} C:{self.close} V:{self.volume})"


class QuoteBar:
    """Quote (bid/ask) bar data."""

    def __init__(self, time: datetime = None, symbol: Symbol = None,
                 bid_open: float = 0, bid_high: float = 0,
                 bid_low: float = 0, bid_close: float = 0,
                 ask_open: float = 0, ask_high: float = 0,
                 ask_low: float = 0, ask_close: float = 0,
                 last_bid_size: float = 0, last_ask_size: float = 0):
        self.time = time or datetime.now()
        self.symbol = symbol
        self.bid = TradeBar(time, symbol, bid_open, bid_high, bid_low, bid_close)
        self.ask = TradeBar(time, symbol, ask_open, ask_high, ask_low, ask_close)
        self.last_bid_size = last_bid_size
        self.last_ask_size = last_ask_size
        self.value = (bid_close + ask_close) / 2 if (bid_close + ask_close) else 0
        self.price = self.value
        self.close = self.value


class Tick:
    """Single tick data point."""

    def __init__(self, time: datetime = None, symbol: Symbol = None,
                 bid_price: float = 0, ask_price: float = 0,
                 last_price: float = 0, quantity: float = 0):
        self.time = time or datetime.now()
        self.symbol = symbol
        self.bid_price = bid_price
        self.ask_price = ask_price
        self.last_price = last_price
        self.value = last_price
        self.price = last_price
        self.quantity = quantity


class Slice:
    """Time-sliced data container. Keyed by symbol."""

    def __init__(self, time: datetime = None):
        self.time = time or datetime.now()
        self._data: Dict[str, TradeBar] = {}
        self.bars = self._data
        self.quote_bars: Dict[str, QuoteBar] = {}
        self.ticks: Dict[str, List[Tick]] = {}
        self.has_data = False
        self.keys = []
        self.values = []

    def __getitem__(self, symbol):
        key = str(symbol).upper()
        return self._data.get(key)

    def __contains__(self, symbol):
        key = str(symbol).upper()
        return key in self._data

    def contains_key(self, symbol) -> bool:
        return str(symbol).upper() in self._data

    def get(self, symbol_type, default=None):
        return self._data

    def add(self, symbol: str, bar: TradeBar):
        key = symbol.upper()
        self._data[key] = bar
        self.has_data = True
        self.keys = list(self._data.keys())
        self.values = list(self._data.values())


class SecurityHolding:
    """Tracks holdings for a single security."""

    def __init__(self, symbol: Symbol, quantity: float = 0,
                 average_price: float = 0, market_price: float = 0):
        self.symbol = symbol
        self.quantity = quantity
        self.average_price = average_price
        self.market_price = market_price

    @property
    def invested(self) -> bool:
        return self.quantity != 0

    @property
    def is_long(self) -> bool:
        return self.quantity > 0

    @property
    def is_short(self) -> bool:
        return self.quantity < 0

    @property
    def holdings_value(self) -> float:
        return self.quantity * self.market_price

    @property
    def unrealized_profit(self) -> float:
        return self.quantity * (self.market_price - self.average_price)

    @property
    def unrealized_profit_percent(self) -> float:
        if self.average_price == 0:
            return 0
        return (self.market_price - self.average_price) / self.average_price

    @property
    def absolute_quantity(self) -> float:
        return abs(self.quantity)

    @property
    def total_close_profit(self) -> float:
        return self._total_close_profit

    def __init__(self, symbol: Symbol, quantity: float = 0,
                 average_price: float = 0, market_price: float = 0):
        self.symbol = symbol
        self.quantity = quantity
        self.average_price = average_price
        self.market_price = market_price
        self._total_close_profit = 0.0

    def update_market_price(self, price: float):
        self.market_price = price

    def __repr__(self):
        return f"Holding({self.symbol}: qty={self.quantity}, avg={self.average_price:.2f}, mkt={self.market_price:.2f})"


class OrderTicket:
    """Represents a submitted order."""

    def __init__(self, order_id: int, symbol: Symbol, quantity: float,
                 order_type: OrderType = OrderType.MARKET,
                 limit_price: float = 0, stop_price: float = 0,
                 tag: str = ""):
        self.order_id = order_id
        self.symbol = symbol
        self.quantity = quantity
        self.order_type = order_type
        self.limit_price = limit_price
        self.stop_price = stop_price
        self.tag = tag
        self.status = OrderStatus.SUBMITTED
        self.average_fill_price = 0.0
        self.quantity_filled = 0.0
        self.time = datetime.now()
        self.submit_request = self

    def update(self, fields):
        """Update order fields."""
        if hasattr(fields, 'limit_price') and fields.limit_price:
            self.limit_price = fields.limit_price
        if hasattr(fields, 'stop_price') and fields.stop_price:
            self.stop_price = fields.stop_price
        if hasattr(fields, 'quantity') and fields.quantity:
            self.quantity = fields.quantity
        if hasattr(fields, 'tag') and fields.tag:
            self.tag = fields.tag

    def cancel(self, tag: str = ""):
        self.status = OrderStatus.CANCELED
        self.tag = tag

    def get(self, field, default=None):
        return getattr(self, field, default)

    def __repr__(self):
        return f"OrderTicket(id={self.order_id}, {self.symbol}, qty={self.quantity}, status={self.status.name})"


class UpdateOrderFields:
    """Fields for updating an existing order."""
    def __init__(self):
        self.limit_price = None
        self.stop_price = None
        self.quantity = None
        self.tag = None


class OrderEvent:
    """Event fired when an order's status changes."""

    def __init__(self, order_id: int, symbol: Symbol, status: OrderStatus,
                 fill_price: float = 0, fill_quantity: float = 0,
                 direction: int = 0, message: str = ""):
        self.order_id = order_id
        self.symbol = symbol
        self.status = status
        self.fill_price = fill_price
        self.fill_quantity = fill_quantity
        self.direction = direction
        self.message = message
        self.utc_time = datetime.utcnow()
        self.is_assignment = False

    @property
    def absolute_fill_quantity(self):
        return abs(self.fill_quantity)


class Insight:
    """Alpha model signal."""

    def __init__(self, symbol, period: timedelta, insight_type: InsightType = InsightType.PRICE,
                 direction: InsightDirection = InsightDirection.FLAT,
                 magnitude: float = None, confidence: float = None,
                 source_model: str = "", weight: float = None):
        self.symbol = symbol
        self.period = period
        self.type = insight_type
        self.direction = direction
        self.magnitude = magnitude
        self.confidence = confidence
        self.source_model = source_model
        self.weight = weight
        self.generated_time_utc = datetime.utcnow()
        self.close_time_utc = self.generated_time_utc + period
        self.score = InsightScore()
        self.tag = ""

    @staticmethod
    def price(symbol, period, direction, magnitude=None, confidence=None,
              source_model="", weight=None):
        if isinstance(period, (int, float)):
            period = timedelta(days=period)
        return Insight(symbol, period, InsightType.PRICE, direction,
                       magnitude, confidence, source_model, weight)

    @staticmethod
    def group(insights):
        return insights


class InsightScore:
    """Score for an insight."""
    def __init__(self):
        self.direction = 0.0
        self.magnitude = 0.0
        self.final_score = 0.0
        self.is_final_score = False


class PortfolioTarget:
    """Target portfolio allocation."""

    def __init__(self, symbol, quantity: float, tag: str = ""):
        self.symbol = symbol
        self.quantity = quantity
        self.tag = tag

    @staticmethod
    def percent(algorithm, symbol, percent: float, return_delta_quantity: bool = False, tag: str = ""):
        portfolio_value = algorithm.portfolio.total_portfolio_value
        price = algorithm.securities[str(symbol)].price if str(symbol) in algorithm.securities else 0
        if price == 0:
            return PortfolioTarget(symbol, 0, tag)
        target_value = portfolio_value * percent
        target_qty = int(target_value / price)
        return PortfolioTarget(symbol, target_qty, tag)

    def __repr__(self):
        return f"PortfolioTarget({self.symbol}, {self.quantity})"
