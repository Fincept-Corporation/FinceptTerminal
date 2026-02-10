# ============================================================================
# Fincept Terminal - Strategy Engine Securities Manager
# Manages subscribed securities and their properties
# ============================================================================

from typing import Dict, Optional
from .enums import SecurityType, Resolution, Market
from .types import Symbol


class Security:
    """Represents a subscribed security."""

    def __init__(self, symbol: Symbol, resolution: Resolution = Resolution.MINUTE,
                 leverage: float = 1.0):
        self.symbol = symbol
        self.resolution = resolution
        self.leverage = leverage
        self.price = 0.0
        self.close = 0.0
        self.open = 0.0
        self.high = 0.0
        self.low = 0.0
        self.volume = 0.0
        self.bid_price = 0.0
        self.ask_price = 0.0
        self.bid_size = 0.0
        self.ask_size = 0.0
        self.is_tradable = True
        self.is_delisted = False
        self.has_data = False
        self.exchange = Exchange()
        self.fee_model = None
        self.fill_model = None
        self.slippage_model = None
        self.margin_model = None
        self.buying_power_model = None
        self.volatility_model = None
        self.settlement_model = None
        self.data_filter = None
        self.price_variation_model = None
        self.margin_interest_rate_model = None
        self._custom_properties = {}
        self._option_chain_provider = None
        self.holdings = SecurityHolding_(symbol)
        self.symbol_properties = SymbolProperties()
        self.quote_currency = QuoteCurrency()
        self.base_currency = QuoteCurrency()
        self.fundamentals = None

    def set_leverage(self, leverage: float):
        self.leverage = leverage

    def set_fee_model(self, model):
        self.fee_model = model

    def set_fill_model(self, model):
        self.fill_model = model

    def set_slippage_model(self, model):
        self.slippage_model = model

    def set_buying_power_model(self, model):
        self.buying_power_model = model

    def set_margin_model(self, model):
        self.margin_model = model

    def set_data_filter(self, data_filter):
        self.data_filter = data_filter

    def set_settlement_model(self, model):
        self.settlement_model = model

    def set_margin_interest_rate_model(self, model):
        self.margin_interest_rate_model = model

    def set_filter(self, *args, **kwargs):
        """Set option/future chain filter. Accepts filter function or min/max params."""
        if args and callable(args[0]):
            self._filter_func = args[0]
        else:
            self._filter_func = None
        return self

    def set_data_normalization_mode(self, mode):
        """Set data normalization mode."""
        self._data_normalization_mode = mode

    @property
    def mapped(self):
        """Get mapped symbol (for continuous futures)."""
        return self.symbol

    @property
    def mappable(self):
        """Whether this security is mappable."""
        return True

    @property
    def settled_bar_count(self):
        return 0

    @property
    def local_time(self):
        from datetime import datetime
        return datetime.now()

    def update_price(self, price: float, open_: float = 0, high: float = 0,
                     low: float = 0, volume: float = 0):
        self.price = price
        self.close = price
        if open_:
            self.open = open_
        if high:
            self.high = high
        if low:
            self.low = low
        if volume:
            self.volume = volume
        self.has_data = True

    def __getitem__(self, key):
        return self._custom_properties.get(key)

    def __setitem__(self, key, value):
        self._custom_properties[key] = value

    def __repr__(self):
        return f"Security({self.symbol}, price={self.price:.2f})"


class Exchange:
    """Security exchange information."""

    # Exchange constants
    NSE = "NSE"
    BSE = "BSE"
    NYSE = "NYSE"
    NASDAQ = "NASDAQ"
    AMEX = "AMEX"
    CME = "CME"
    COMEX = "COMEX"
    NYMEX = "NYMEX"
    CBOT = "CBOT"
    ICE = "ICE"
    EUREX = "EUREX"
    LSE = "LSE"
    TSE = "TSE"
    HKEX = "HKEX"
    SGX = "SGX"
    ASX = "ASX"

    def __init__(self):
        self.exchange_open = True
        self.hours = ExchangeHours()

    @property
    def local_time(self):
        from datetime import datetime
        return datetime.now()


class ExchangeHours:
    """Exchange trading hours."""
    def __init__(self):
        self.is_open = True
        self.regular_market_duration = None

    def is_date_open(self, date) -> bool:
        return True

    def get_next_market_open(self, time, extended_market=False):
        return time

    def get_next_market_close(self, time, extended_market=False):
        from datetime import timedelta
        return time + timedelta(hours=6, minutes=30)


class SecurityManager:
    """Manages all subscribed securities. Dict-like access by ticker."""

    def __init__(self):
        self._securities: Dict[str, Security] = {}

    def __getitem__(self, key) -> Security:
        ticker = str(key).upper()
        if ticker not in self._securities:
            sym = Symbol(ticker)
            self._securities[ticker] = Security(sym)
        return self._securities[ticker]

    def __setitem__(self, key, value):
        self._securities[str(key).upper()] = value

    def __contains__(self, key) -> bool:
        return str(key).upper() in self._securities

    def __iter__(self):
        return iter(self._securities.values())

    def __len__(self):
        return len(self._securities)

    def keys(self):
        return self._securities.keys()

    def values(self):
        return self._securities.values()

    def items(self):
        return self._securities.items()

    def add(self, symbol: Symbol, resolution: Resolution = Resolution.MINUTE,
            leverage: float = 1.0) -> Security:
        ticker = str(symbol).upper()
        sec = Security(symbol, resolution, leverage)
        self._securities[ticker] = sec
        return sec

    def remove(self, symbol) -> bool:
        ticker = str(symbol).upper()
        if ticker in self._securities:
            del self._securities[ticker]
            return True
        return False

    def get(self, key, default=None):
        return self._securities.get(str(key).upper(), default)


class SecurityHolding_:
    """Lightweight holdings tracker on Security object."""
    def __init__(self, symbol):
        self.symbol = symbol
        self.quantity = 0
        self.average_price = 0.0
        self.invested = False
        self.is_long = False
        self.is_short = False
        self.unrealized_profit = 0.0
        self.unrealized_profit_percent = 0.0


class SymbolProperties:
    """Symbol properties like lot size, minimum price variation."""
    def __init__(self):
        self.lot_size = 1
        self.minimum_price_variation = 0.01
        self.minimum_order_size = 1
        self.contract_multiplier = 1.0
        self.priceMagnifier = 1
        self.description = ""


class QuoteCurrency:
    """Quote currency info for a security."""
    def __init__(self):
        self.symbol = "USD"
        self.conversion_rate = 1.0
        self.amount = 0.0

    def set_amount(self, amount):
        self.amount = amount

    def SetAmount(self, amount):
        self.amount = amount
