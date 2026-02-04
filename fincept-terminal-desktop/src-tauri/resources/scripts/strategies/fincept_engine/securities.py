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
