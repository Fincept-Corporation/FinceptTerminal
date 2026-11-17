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
        from .volatility_models import NullVolatilityModel
        self.volatility_model = NullVolatilityModel()
        self.settlement_model = None
        self.data_filter = None
        self.price_variation_model = None
        self.margin_interest_rate_model = None
        self._custom_properties = {}
        self._option_chain_provider = None
        self.subscriptions = []  # List of subscription data configs
        self._own_holdings = SecurityHolding_(symbol)
        self._manager = None      # SecurityManager (links to the portfolio and the clock)
        self._last_bar = None
        self.cache = _SecurityCache(self)
        self.data = _DynamicSecurityData()
        self.symbol_properties = SymbolProperties()
        self.quote_currency = QuoteCurrency()
        self.base_currency = QuoteCurrency()
        self.underlying = None
        self.contract_filter = None
        self.price_model = None
        self._init_done = True

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

    def set_volatility_model(self, model):
        self.volatility_model = model

    def set_shortable_provider(self, provider):
        self._shortable_provider = provider

    @property
    def fundamentals(self):
        """The security's Fundamental object as of now (universe_data; Yahoo snapshot, disclosed)."""
        algo = getattr(self._manager, '_algorithm', None) if self._manager is not None else None
        mgr = getattr(algo, '_universe_manager', None) if algo is not None else None
        if mgr is None or self.symbol.security_type != SecurityType.EQUITY:
            return None
        return mgr.fundamentals(self.symbol)

    Fundamentals = fundamentals

    @property
    def shortable_provider(self):
        return getattr(self, '_shortable_provider', None)

    @property
    def type(self):
        """LEAN Security.Type: the security type."""
        return getattr(self.symbol, 'security_type', None)

    @property
    def is_extended_market_hours(self):
        return bool(getattr(self, '_extended_market_hours', False))

    @property
    def style(self):
        """Option style of an option security (LEAN Option.Style)."""
        sid = getattr(self.symbol, 'id', None)
        return getattr(sid, 'option_style', None)

    @property
    def subscription_data_config(self):
        """The security's (trade bar) subscription config (LEAN Security.SubscriptionDataConfig)."""
        from datetime import timedelta
        from .algorithm import _SubscriptionDataConfig
        res = getattr(self, 'resolution', None)
        c = _SubscriptionDataConfig(self.symbol, res, 'trade')
        c.increment = {0: timedelta(0), 1: timedelta(seconds=1), 2: timedelta(minutes=1),
                       3: timedelta(hours=1)}.get(int(res) if res is not None else 4, timedelta(days=1))
        c.extended_market_hours = bool(getattr(self, '_extended_market_hours', False))
        mode = getattr(self, '_data_normalization_mode', None)
        if mode is not None:
            c.data_normalization_mode = mode
        c.data_mapping_mode = getattr(self, '_data_mapping_mode', None)
        c.contract_depth_offset = getattr(self, '_contract_depth_offset', 0)
        c.exchange_time_zone = c.data_time_zone = 'America/New_York'
        c.market = getattr(self.symbol, 'market', None)
        c.security_type = getattr(self.symbol, 'security_type', None)
        return c

    @property
    def option_exercise_model(self):
        return getattr(self, '_option_exercise_model', None)

    @property
    def option_assignment_model(self):
        return getattr(self, '_option_assignment_model', None)

    # ---- LEAN dynamic security properties: security.add("x", v), security["x"], security.x ----
    def __getattr__(self, name):
        if name.startswith('_'):
            raise AttributeError(name)
        props = self.__dict__.get('_custom_properties')
        if props is not None and name in props:
            return props[name]
        raise AttributeError(f"'{type(self).__name__}' object has no attribute '{name}'")

    def __setattr__(self, name, value):
        # After construction, a new public attribute is a LEAN dynamic property (security.x = v is
        # security["x"] = v); engine attributes and existing ones are set normally.
        d = self.__dict__
        if (d.get('_init_done') and not name.startswith('_') and name not in d
                and not hasattr(type(self), name)):
            d['_custom_properties'][name] = value
            return
        object.__setattr__(self, name, value)

    def get_property(self, key, default=None):
        return self._custom_properties.get(key, default)

    def remove(self, key):
        return self._custom_properties.pop(key, None) is not None

    def try_get(self, key):
        v = self._custom_properties.get(key)
        return (key in self._custom_properties, v)

    class _TypedGetter:
        def __init__(self, sec):
            self._sec = sec

        def __getitem__(self, _type):
            return lambda key, default=None: self._sec._custom_properties.get(key, default)

        def __call__(self, key, default=None):
            return self._sec._custom_properties.get(key, default)

    @property
    def get(self):
        """security.get("key") or security.get[Type]("key") (LEAN generic accessor)."""
        return Security._TypedGetter(self)

    def set_option_exercise_model(self, model):
        self._option_exercise_model = model

    def set_option_assignment_model(self, model):
        self._option_assignment_model = model

    def add(self, key, value):
        """Add custom property to security."""
        self._custom_properties[key] = value

    @property
    def key(self):
        """Security key for subscriptions."""
        return str(self.symbol)

    # PascalCase aliases
    def SetFilter(self, *args, **kwargs):
        return self.set_filter(*args, **kwargs)

    def SetLeverage(self, leverage):
        return self.set_leverage(leverage)

    def SetFeeModel(self, model):
        return self.set_fee_model(model)

    def SetFillModel(self, model):
        return self.set_fill_model(model)

    def SetSlippageModel(self, model):
        return self.set_slippage_model(model)

    def SetBuyingPowerModel(self, model):
        return self.set_buying_power_model(model)

    def SetVolatilityModel(self, model):
        return self.set_volatility_model(model)

    def set_filter(self, *args, **kwargs):
        """Option / future contract filter: a function of the filter universe, or LEAN's numeric
        forms (min_expiry, max_expiry) / (min_strike, max_strike, min_expiry, max_expiry)."""
        self._filter_args = (args, kwargs)
        self._filter_func = args[0] if args and callable(args[0]) else None
        return self

    def set_data_normalization_mode(self, mode):
        """Set data normalization mode."""
        self._data_normalization_mode = mode

    @property
    def holdings(self):
        """The portfolio's holding of this security (LEAN Security.Holdings)."""
        mgr = self._manager
        pf = getattr(mgr, '_portfolio', None) if mgr is not None else None
        if pf is not None:
            return pf[str(self.symbol).upper()]
        return self._own_holdings

    @holdings.setter
    def holdings(self, value):
        self._own_holdings = value

    @property
    def invested(self):
        return bool(getattr(self.holdings, 'invested', False))

    def get_last_data(self):
        return self._last_bar

    @property
    def mapped(self):
        """The contract a continuous future is mapped to now (itself for other securities)."""
        return self.__dict__.get('_mapped') or self.symbol

    @property
    def mappable(self):
        """Whether this security is mappable."""
        return True

    @property
    def settled_bar_count(self):
        return 0

    @property
    def local_time(self):
        algo = getattr(self._manager, '_algorithm', None) if self._manager is not None else None
        if algo is not None:
            return algo.time
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

    def set_market_price(self, data):
        """Update the price from a bar / tick / data point."""
        if data is None:
            return
        price = getattr(data, 'close', None) or getattr(data, 'price', None) or getattr(data, 'value', 0.0)
        self.update_price(float(price or 0.0), getattr(data, 'open', 0) or 0, getattr(data, 'high', 0) or 0,
                          getattr(data, 'low', 0) or 0, getattr(data, 'volume', 0) or 0)

    def __getitem__(self, key):
        if key not in self._custom_properties:
            raise KeyError(f"The security does not have a property with key '{key}'")
        return self._custom_properties[key]

    def __setitem__(self, key, value):
        self._custom_properties[key] = value

    def __contains__(self, key):
        return key in self._custom_properties

    @property
    def Symbol(self):
        return self.symbol

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

    def __init__(self, hours=None, clock=None):
        self.hours = hours if hours is not None else ExchangeHours()
        self.time_zone = getattr(self.hours, 'time_zone', "America/New_York")
        self._clock = clock          # callable -> the algorithm's current time
        self._forced_open = None

    @property
    def local_time(self):
        if self._clock is not None:
            return self._clock()
        from datetime import datetime
        return datetime.now()

    @property
    def exchange_open(self):
        if self._forced_open is not None:
            return self._forced_open
        return self.hours.is_open(self.local_time, False)

    @exchange_open.setter
    def exchange_open(self, value):
        self._forced_open = bool(value)

    @property
    def closing_soon(self):
        return False

    def date_is_open(self, d):
        return self.hours.is_date_open(d)

    def date_time_is_open(self, t):
        return self.hours.is_open(t, False)

    def set_market_hours(self, *args, **kwargs):
        pass

    ExchangeOpen = exchange_open
    Hours = property(lambda self: self.hours)


from .market_hours import ExchangeHours  # noqa: E402  (per asset class sessions)


class SecurityManager:
    """Manages all subscribed securities. Dict-like access by ticker."""

    def __init__(self):
        self._securities: Dict[str, Security] = {}

    def copy(self):
        return {s.symbol: s for s in self._securities.values()}

    def clear(self):
        self._securities.clear()

    def pop(self, key, *default):
        t = str(key).upper()
        if t in self._securities:
            return self._securities.pop(t)
        if default:
            return default[0]
        raise KeyError(key)

    def __getitem__(self, key) -> Security:
        ticker = str(key).upper()
        if ticker not in self._securities:
            mgr = getattr(getattr(self, '_algorithm', None), '_option_manager', None)
            if mgr is not None:
                sec = mgr.on_demand(ticker)
                if sec is not None:
                    return sec
            sym = key if isinstance(key, Symbol) else Symbol(ticker)
            sec = Security(sym)
            sec._manager = self
            self._securities[ticker] = sec
        return self._securities[ticker]

    def __setitem__(self, key, value):
        self._securities[str(key).upper()] = value

    def __contains__(self, key) -> bool:
        t = str(key).upper()
        if t in self._securities:
            return True
        mgr = getattr(getattr(self, '_algorithm', None), '_option_manager', None)
        return mgr is not None and any(t in s for s in mgr.member_symbols.values())

    def __iter__(self):
        from .portfolio import _KeyValuePair
        return iter([_KeyValuePair(s.symbol, s) for s in self._securities.values()])

    def __len__(self):
        return len(self._securities)

    def keys(self):
        """The securities' Symbols (LEAN Securities.Keys)."""
        return [s.symbol for s in self._securities.values()]

    def values(self):
        return self._securities.values()

    def items(self):
        return self._securities.items()

    def add(self, symbol: Symbol, resolution: Resolution = Resolution.MINUTE,
            leverage: float = 1.0) -> Security:
        ticker = str(symbol).upper()
        sec = Security(symbol, resolution, leverage)
        sec._manager = self
        algo = getattr(self, '_algorithm', None)
        sec.exchange = Exchange(ExchangeHours.for_security_type(symbol.security_type, getattr(symbol, 'market', None)),
                                (lambda a=algo: a.time) if algo is not None else None)
        self._securities[ticker] = sec
        return sec

    def contains_key(self, key) -> bool:
        return str(key).upper() in self._securities

    def total(self):
        return list(self._securities.values())

    ContainsKey = contains_key
    # C# dictionary members (Securities.Values / Keys / Count)
    Values = property(lambda self: list(self._securities.values()))
    Keys = property(lambda self: [s.symbol for s in self._securities.values()])
    Count = property(lambda self: len(self._securities))

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
    def __init__(self, description="", quote_currency="USD", contract_multiplier=1.0,
                 minimum_price_variation=0.01, lot_size=1, market_ticker=None):
        self.description = description
        self.quote_currency_symbol = quote_currency
        self.contract_multiplier = contract_multiplier
        self.minimum_price_variation = minimum_price_variation
        self.lot_size = lot_size
        self.market_ticker = market_ticker or ""
        self.minimum_order_size = 1
        self.priceMagnifier = 1


class _CacheGetter:
    """security.cache.get_data(), .get_data[Type]() (LEAN generic)."""
    def __init__(self, cache):
        self._c = cache

    def __call__(self, data_type=None):
        return self._c._latest(data_type)

    def __getitem__(self, data_type):
        return lambda: self._c._latest(data_type)


class _SecurityCache:
    """Security data cache: the latest data of each type the security received (bars, custom data,
    ETF constituent data from universes)."""
    def __init__(self, security=None):
        self._data = {}
        self._security = security

    def _items(self, data_type):
        if data_type is None:
            sec = self._security
            last = getattr(sec, '_last_bar', None) if sec is not None else None
            return [last] if last is not None else []
        name = getattr(data_type, '__name__', str(data_type))
        for k, v in self._data.items():
            if k is data_type or getattr(k, '__name__', None) == name:
                return v
        sec = self._security
        if sec is not None and name in ('ETFConstituentData', 'ETFConstituentUniverse', 'ETFConstituentsUniverse'):
            mgr = getattr(getattr(getattr(sec, '_manager', None), '_algorithm', None), '_universe_manager', None)
            if mgr is not None:
                d = mgr.constituent_data(sec.symbol)
                return [d] if d is not None else []
        if sec is not None and name in ('TradeBar', 'BaseData'):
            last = getattr(sec, '_last_bar', None)
            return [last] if last is not None else []
        return []

    def _latest(self, data_type=None):
        v = self._items(data_type)
        return v[-1] if v else None

    def add_data(self, data):
        self._data.setdefault(type(data), []).append(data)

    def add_data_list(self, data_list, data_type=None, from_subscription=False):
        if data_type is not None:
            self._data[data_type] = list(data_list) if data_list else []

    @property
    def get_data(self):
        return _CacheGetter(self)

    def get_all(self, data_type=None):
        return list(self._items(data_type))

    def has_data(self, data_type=None):
        return bool(self._items(data_type))

    GetData = get_data
    HasData = has_data


class _DynamicSecurityData:
    """Dynamic security data providing access to custom data types."""
    def __init__(self):
        self._store = {}
    def get(self, data_type=None):
        if data_type and data_type in self._store:
            items = self._store[data_type]
            return items[-1] if items else None
        return None
    def get_all(self, data_type=None):
        if data_type and data_type in self._store:
            return self._store[data_type]
        return []
    def has_data(self, data_type=None):
        if data_type:
            return data_type in self._store and len(self._store[data_type]) > 0
        return len(self._store) > 0
    def __getattr__(self, name):
        if name.startswith('_'):
            raise AttributeError(name)
        return None

    # PascalCase aliases
    Get = get
    GetAll = get_all
    HasData = has_data


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
