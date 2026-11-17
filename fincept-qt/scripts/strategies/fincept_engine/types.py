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


_OPTION_TYPES = (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION)

# LEAN's SecurityIdentifier.DefaultDate: the date of a canonical option/future and of
# securities whose listing date is unknown.
DEFAULT_DATE = datetime(1998, 1, 2)

_FUTURE_MONTH_CODES = 'FGHJKMNQUVXZ'


def _to_datetime(x):
    if x is None:
        return None
    if isinstance(x, datetime):
        return x
    from datetime import date as _date
    if isinstance(x, _date):
        return datetime(x.year, x.month, x.day)
    return x


class SecurityIdentifier:
    """LEAN SecurityIdentifier: what a symbol is (ticker, type, market, and for derivatives
    the expiry date, strike, right, style and the underlying's identifier)."""

    __slots__ = ('symbol', 'security_type', 'market', 'date', 'strike_price', 'option_right',
                 'option_style', 'underlying')

    def __init__(self, symbol='', security_type=SecurityType.EQUITY, market=Market.USA, date=None,
                 strike_price=0.0, option_right=None, option_style=None, underlying=None):
        from .enums import OptionRight as _OR, OptionStyle as _OS
        self.symbol = str(symbol).upper()
        self.security_type = security_type
        self.market = market
        self.date = _to_datetime(date) or DEFAULT_DATE
        self.strike_price = float(strike_price or 0.0)
        self.option_right = option_right if option_right is not None else _OR.CALL
        self.option_style = option_style if option_style is not None else _OS.AMERICAN
        self.underlying = underlying

    @property
    def has_underlying(self) -> bool:
        return self.underlying is not None

    @property
    def expiry(self):
        return self.date

    expiration = expiry

    def _key(self):
        u = self.underlying._key() if self.underlying is not None else None
        return (self.symbol, int(self.security_type), str(self.market).lower(), self.date,
                round(self.strike_price, 6), int(self.option_right), int(self.option_style), u)

    def __eq__(self, other):
        if isinstance(other, SecurityIdentifier):
            return self._key() == other._key()
        return NotImplemented

    def __hash__(self):
        return hash(self._key())

    def __str__(self):
        base = f"{self.symbol} {self.security_type.name}"
        if self.date != DEFAULT_DATE and (self.security_type in _OPTION_TYPES
                                          or self.security_type == SecurityType.FUTURE):
            base += f" {self.date:%Y%m%d}"
            if self.security_type in _OPTION_TYPES:
                base += f" {'C' if int(self.option_right) == 0 else 'P'}{self.strike_price:g}"
        return base

    def __repr__(self):
        return f"SecurityIdentifier({self})"

    def upper(self):
        return str(self).upper()

    # ---- LEAN static generators ----
    @staticmethod
    def generate_equity(ticker, market=Market.USA, map_symbol=True, *args, **kwargs):
        if isinstance(ticker, datetime):  # (date, ticker, market) overload
            ticker, market = market, (args[0] if args else Market.USA)
        return SecurityIdentifier(ticker, SecurityType.EQUITY, market)

    @staticmethod
    def generate_index(ticker, market=Market.USA, *args, **kwargs):
        return SecurityIdentifier(ticker, SecurityType.INDEX, market)

    @staticmethod
    def generate_forex(ticker, market=Market.OANDA, *args, **kwargs):
        return SecurityIdentifier(ticker, SecurityType.FOREX, market)

    @staticmethod
    def generate_crypto(ticker, market=Market.BINANCE, *args, **kwargs):
        return SecurityIdentifier(ticker, SecurityType.CRYPTO, market)

    @staticmethod
    def generate_cfd(ticker, market=Market.OANDA, *args, **kwargs):
        return SecurityIdentifier(ticker, SecurityType.CFD, market)

    @staticmethod
    def generate_base(data_type=None, ticker='', market=Market.USA, *args, **kwargs):
        return SecurityIdentifier(ticker, SecurityType.BASE, market)

    @staticmethod
    def generate_future(expiry, ticker, market=Market.CME, *args, **kwargs):
        if isinstance(expiry, str) and not isinstance(ticker, str):  # (ticker, market, expiry) order
            expiry, ticker = ticker, expiry
        return SecurityIdentifier(ticker, SecurityType.FUTURE, market, date=expiry)

    @staticmethod
    def generate_option(expiry, underlying, market=Market.USA, strike=0.0, right=None, style=None,
                        *args, **kwargs):
        u = underlying.id if isinstance(underlying, Symbol) else underlying
        if not isinstance(u, SecurityIdentifier):
            u = SecurityIdentifier(str(underlying), SecurityType.EQUITY, market)
        st = {SecurityType.INDEX: SecurityType.INDEX_OPTION,
              SecurityType.FUTURE: SecurityType.FUTURE_OPTION}.get(u.security_type, SecurityType.OPTION)
        return SecurityIdentifier(u.symbol, st, market, date=expiry, strike_price=strike,
                                  option_right=right, option_style=style, underlying=u)

    @staticmethod
    def generate_constituent_identifier(data_mapping_symbol, constituent_symbol=None, *args, **kwargs):
        return SecurityIdentifier(str(data_mapping_symbol), SecurityType.BASE)

    GenerateEquity = generate_equity
    GenerateIndex = generate_index
    GenerateFuture = generate_future
    GenerateOption = generate_option
    GenerateBase = generate_base
    GenerateConstituentIdentifier = generate_constituent_identifier
    Date = property(lambda self: self.date)
    StrikePrice = property(lambda self: self.strike_price)
    OptionRight = property(lambda self: self.option_right)
    OptionStyle = property(lambda self: self.option_style)
    Symbol = property(lambda self: self.symbol)
    Market = property(lambda self: self.market)
    SecurityType = property(lambda self: self.security_type)
    Underlying = property(lambda self: self.underlying)
    HasUnderlying = property(lambda self: self.has_underlying)


def future_ticker(root: str, expiry: datetime) -> str:
    """LEAN future contract ticker: root + expiry day + month code + 2-digit year (ES20Z13)."""
    return f"{root}{expiry.day:02d}{_FUTURE_MONTH_CODES[expiry.month - 1]}{expiry.year % 100:02d}"


def option_ticker_osi(underlying: str, right, strike: float, expiry: datetime) -> str:
    """OSI option ticker as LEAN writes it: underlying padded to 6, yymmdd, C/P, strike*1000."""
    sym = str(underlying).upper()
    if len(sym) > 5:
        sym += ' '
    rc = 'C' if int(right) == 0 else 'P'
    return f"{sym:<6}{expiry:%y%m%d}{rc}{int(round(float(strike) * 1000)):08d}"


_FUTURE_OPTION_ROOTS = {'GC': 'OG', 'SI': 'SO', 'HG': 'HXE', 'CL': 'LO', 'NG': 'ON', 'ZC': 'OZC',
                        'ZS': 'OZS', 'ZW': 'OZW', 'ZN': 'OZN', 'ZB': 'OZB', 'ZF': 'OZF', 'ZT': 'OZT'}


def future_option_root(future_root: str) -> str:
    """Option root of a future's options (LEAN FuturesOptionsSymbolMappings, common roots)."""
    return _FUTURE_OPTION_ROOTS.get(future_root.upper(), future_root.upper())


_MARKETS = {'usa', 'cme', 'cbot', 'nymex', 'comex', 'ice', 'india', 'eurex', 'cboe', 'cfe', 'nse',
            'bse', 'oanda', 'binance', 'gdax', 'coinbase', 'kraken', 'bybit', 'fxcm', 'sgx', 'hkfe'}


class Symbol:
    """A security identifier: `value` (the ticker string LEAN shows), `id` (SecurityIdentifier),
    `security_type`, `market`, and for derivatives `underlying` and `canonical`.

    Equality and hashing use the value string, so a Symbol and its ticker string address the same
    security everywhere in the engine (securities, portfolio, slices). Option contracts use the OSI
    ticker ('GOOG  151224C00745000'), canonical options '?GOOG', future contracts 'ES20Z13' and
    canonical futures '/ES', as in LEAN."""

    def __init__(self, ticker: str, security_type: SecurityType = SecurityType.EQUITY,
                 market: str = Market.USA, sid: 'SecurityIdentifier' = None, underlying: 'Symbol' = None):
        ticker = str(ticker)
        t = ticker.upper()
        self.value = t
        self.security_type = security_type
        self.market = market
        self.id = sid if sid is not None else SecurityIdentifier(t.lstrip('?/'), security_type, market)
        if underlying is not None:
            self._underlying = underlying

    # Placeholder industry identifiers (non-empty for compatibility; not real CUSIP/ISIN codes)
    @property
    def cusip(self):
        return f"{self.value[:6]:0<9}"[:9]

    @property
    def sedol(self):
        return f"{self.value[:5]:0<7}"[:7]

    @property
    def isin(self):
        return f"US{self.value[:6]:0<10}"[:12]

    @property
    def figi(self):
        return f"BBG{self.value[:6]:0<9}"[:12]

    composite_figi = figi

    @property
    def cik(self):
        import zlib
        return zlib.crc32(self.value.encode()) % 9999999 + 1000000

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

    def __lt__(self, other):
        return self.value < str(other).upper()

    def upper(self):
        """Allow Symbol to be used where string.upper() is called."""
        return self.value

    def lower(self):
        return self.value.lower()

    @property
    def Value(self):
        return self.value

    @property
    def ID(self):
        return self.id

    # ---- derivative details (LEAN reads them from symbol.id; these are shortcuts) ----
    @property
    def strike_price(self):
        return self.id.strike_price

    @property
    def expiry(self):
        return self.id.date if self.id.date != DEFAULT_DATE else None

    @property
    def right(self):
        return self.id.option_right

    @property
    def option_style(self):
        return self.id.option_style

    # ---- factories ----
    @staticmethod
    def create(ticker: str, security_type: SecurityType = SecurityType.EQUITY,
               market: str = Market.USA, alias=None, *args, **kwargs):
        st = security_type if security_type is not None else SecurityType.EQUITY
        market = market or Market.USA
        if st == SecurityType.OPTION:
            return Symbol.create_canonical_option(Symbol(str(ticker), SecurityType.EQUITY, market), market, alias)
        if st == SecurityType.INDEX_OPTION:
            return Symbol.create_canonical_option(Symbol(str(ticker), SecurityType.INDEX, market), market, alias)
        if st == SecurityType.FUTURE:
            t = str(ticker).upper().lstrip('/')
            return Symbol(alias or f"/{t}", SecurityType.FUTURE, market,
                          sid=SecurityIdentifier(t, SecurityType.FUTURE, market))
        return Symbol(alias or str(ticker), st, market)

    @staticmethod
    def create_base(data_type=None, underlying=None, market=Market.USA, *args, **kwargs):
        """A custom-data symbol linked to `underlying` (same ticker value)."""
        if underlying is None:
            return Symbol(getattr(data_type, '__name__', 'BASE'), SecurityType.BASE, market)
        u = underlying if isinstance(underlying, Symbol) else Symbol(str(underlying))
        m = market or Market.USA
        return Symbol(u.value, SecurityType.BASE, m, sid=SecurityIdentifier(u.id.symbol, SecurityType.BASE, m),
                      underlying=u)

    @staticmethod
    def create_future(ticker: str, market: str = "cme", expiry=None, alias=None, *args, **kwargs):
        """A future contract (with an expiry) or, without one, the canonical future."""
        root = str(ticker).upper().lstrip('/')
        exp = _to_datetime(expiry)
        if exp is None or exp == DEFAULT_DATE:
            return Symbol.create(root, SecurityType.FUTURE, market)
        sid = SecurityIdentifier(root, SecurityType.FUTURE, market, date=exp)
        return Symbol(alias or future_ticker(root, exp), SecurityType.FUTURE, market, sid=sid)

    @staticmethod
    def create_option(underlying, *args, **kwargs):
        """LEAN overloads:
            create_option(underlying, market, style, right, strike, expiry[, alias])
            create_option(underlying_symbol, target_option, market, style, right, strike, expiry[, alias])
        `underlying` is a Symbol or an equity ticker string."""
        from .enums import OptionRight as _OR, OptionStyle as _OS
        a = list(args)
        target = None
        if len(a) >= 6 and isinstance(a[0], str) and isinstance(a[1], str):
            target = a.pop(0)
        market = a[0] if len(a) > 0 else kwargs.get('market', Market.USA)
        style = a[1] if len(a) > 1 else kwargs.get('style', kwargs.get('option_style'))
        right = a[2] if len(a) > 2 else kwargs.get('right', kwargs.get('option_right', _OR.CALL))
        strike = a[3] if len(a) > 3 else kwargs.get('strike', kwargs.get('strike_price', 0.0))
        expiry = a[4] if len(a) > 4 else kwargs.get('expiry', kwargs.get('expiration'))
        alias = a[5] if len(a) > 5 else kwargs.get('alias')
        market = market or Market.USA
        u = underlying if isinstance(underlying, Symbol) else Symbol(str(underlying), SecurityType.EQUITY, market)
        st = {SecurityType.INDEX: SecurityType.INDEX_OPTION,
              SecurityType.FUTURE: SecurityType.FUTURE_OPTION}.get(u.security_type, SecurityType.OPTION)
        if style is None:
            style = _OS.EUROPEAN if st == SecurityType.INDEX_OPTION else _OS.AMERICAN
        if right is None:
            right = _OR.CALL
        exp = _to_datetime(expiry)
        canonical = exp is None or exp == DEFAULT_DATE
        if target:
            name = str(target).upper()
        elif st == SecurityType.FUTURE_OPTION:
            name = future_option_root(u.id.symbol)
        else:
            name = u.id.symbol
        sid = SecurityIdentifier(name, st, market, date=None if canonical else exp,
                                 strike_price=0.0 if canonical else strike,
                                 option_right=right, option_style=style, underlying=u.id)
        if canonical:
            value = alias or f"?{name}"
            if not alias and st == SecurityType.FUTURE_OPTION and u.security_type == SecurityType.FUTURE                     and not u.is_canonical():
                # one canonical option per futures contract: this engine keys symbols by their value, so the
                # contract is part of it (LEAN tells them apart by the underlying in the identifier)
                value = f"?{name}.{u.value}"
        else:
            shown = name if (target or name != u.id.symbol) else u.value
            value = alias or option_ticker_osi(shown, right, strike, exp)
        return Symbol(value, st, market, sid=sid, underlying=u)

    @staticmethod
    def create_canonical_option(underlying, *args, **kwargs):
        """create_canonical_option(underlying[, market][, alias]) or
        create_canonical_option(underlying, target_option, market[, alias])."""
        a = list(args)
        target = kwargs.get('target_option')
        if len(a) >= 2 and isinstance(a[0], str) and isinstance(a[1], str) and a[1].lower() in _MARKETS:
            target = a.pop(0)
        market = a[0] if a else kwargs.get('market')
        alias = a[1] if len(a) > 1 else kwargs.get('alias')
        u = underlying if isinstance(underlying, Symbol) else Symbol(str(underlying), SecurityType.EQUITY,
                                                                      market or Market.USA)
        rest = [market or u.market or Market.USA, None, None, 0.0, None, alias]
        return Symbol.create_option(u, *(([target] if target else []) + rest))

    # ---- derivative relationships ----
    @property
    def underlying(self):
        """The underlying symbol (options, linked custom data); None when there is none."""
        return getattr(self, '_underlying', None)

    @underlying.setter
    def underlying(self, value):
        self._underlying = value

    @property
    def has_underlying(self):
        return getattr(self, '_underlying', None) is not None

    def is_canonical(self) -> bool:
        """True for a canonical option ('?SPY') or canonical future ('/ES')."""
        return ((self.security_type == SecurityType.FUTURE or
                 (self.security_type in _OPTION_TYPES and self.has_underlying))
                and self.id.date == DEFAULT_DATE)

    def has_canonical(self) -> bool:
        return not self.is_canonical() and (self.security_type in _OPTION_TYPES or
                                            self.security_type == SecurityType.FUTURE)

    @property
    def canonical(self):
        if self.is_canonical():
            return self
        if self.security_type in _OPTION_TYPES:
            c = getattr(self, '_canonical_cache', None)
            if c is None:
                u = self.underlying
                target = self.id.symbol
                default_name = (future_option_root(u.id.symbol) if self.security_type == SecurityType.FUTURE_OPTION
                                else u.id.symbol)
                args = ([target] if target != default_name else []) + [self.market]
                c = Symbol.create_canonical_option(u, *args)
                self._canonical_cache = c
            return c
        if self.security_type == SecurityType.FUTURE:
            return Symbol.create(self.id.symbol, SecurityType.FUTURE, self.market)
        raise ValueError(f"Canonical symbol is not defined for {self.value} ({self.security_type.name})")

    def has_underlying_symbol(self, symbol) -> bool:
        cur = self
        while cur.has_underlying:
            if cur.underlying == symbol:
                return True
            cur = cur.underlying
        return False

    IsCanonical = is_canonical
    HasCanonical = has_canonical
    HasUnderlying = has_underlying
    Underlying = underlying
    Canonical = canonical
    CreateOption = create_option
    CreateFuture = create_future
    CreateCanonicalOption = create_canonical_option


class TradeBar:
    """OHLCV bar data."""

    def __init__(self, time: datetime = None, symbol: Symbol = None,
                 open: float = 0, high: float = 0, low: float = 0,
                 close: float = 0, volume: float = 0, period: timedelta = None):
        self.time = time or datetime.now()
        self.symbol = symbol
        self.open = open
        self.high = high
        self.low = low
        self.close = close
        self.volume = volume
        self.period = period if isinstance(period, timedelta) else timedelta(minutes=1)
        # LEAN TradeBar(time, symbol, o, h, l, c, v, period): the bar ends a period after it starts
        self.end_time = self.time + period if isinstance(period, timedelta) else self.time
        self.value = close
        self.price = close
        self.is_fill_forward = False

    def __repr__(self):
        return f"TradeBar({self.symbol} O:{self.open} H:{self.high} L:{self.low} C:{self.close} V:{self.volume})"

    def __str__(self):
        return f"{self.symbol}: O: {self.open:g} H: {self.high:g} L: {self.low:g} C: {self.close:g} V: {self.volume:g}"


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


class KeyValuePair:
    """An item of a LEAN dictionary iterated in Python: .key / .value, unpacks as (key, value), and
    otherwise behaves like its key (str, ==, hash) so `for symbol in data.bars` keeps working."""
    __slots__ = ('key', 'value')

    def __init__(self, key, value):
        self.key = key
        self.value = value

    def __iter__(self):
        yield self.key
        yield self.value

    def __str__(self):
        return str(self.key)

    def __repr__(self):
        return f"[{self.key}, {self.value!r}]"

    def __eq__(self, other):
        return self.key == (other.key if isinstance(other, KeyValuePair) else other)

    def __hash__(self):
        return hash(self.key)

    def upper(self):
        return str(self.key).upper()

    def __getattr__(self, name):
        if name.startswith('__'):
            raise AttributeError(name)
        return getattr(self.value, name)

    Key = property(lambda self: self.key)
    Value = property(lambda self: self.value)


class SymbolDict(dict):
    """dict keyed by Symbol that also accepts ticker strings (any case) as keys. Iterating yields
    key/value pairs (LEAN's C# dictionaries do); .keys() / .values() / .items() as in Python."""

    @staticmethod
    def _k(key):
        if isinstance(key, KeyValuePair):
            key = key.key
        return key if isinstance(key, Symbol) else str(key).upper()

    def __iter__(self):
        return iter([KeyValuePair(k, v) for k, v in dict.items(self)])

    def __setitem__(self, key, value):
        dict.__setitem__(self, self._k(key), value)

    def __getitem__(self, key):
        return super().__getitem__(self._k(key))

    def __contains__(self, key):
        return super().__contains__(self._k(key))

    def get(self, key, default=None):
        return super().get(self._k(key), default)

    def contains_key(self, key) -> bool:
        return self._k(key) in self

    def try_get_value(self, key):
        v = self.get(key)
        return (v is not None, v)

    @property
    def count(self):
        return len(self)

    ContainsKey = contains_key
    TryGetValue = try_get_value
    Count = count


def _unsupported(feature, detail=""):
    from .errors import unsupported_attribute
    return unsupported_attribute(feature, detail)


class _NoTickDict(SymbolDict):
    """slice.ticks: the engine has no tick data; asking for a symbol's ticks says so."""
    def __getitem__(self, key):
        if dict.__contains__(self, self._k(key)):
            return dict.__getitem__(self, self._k(key))
        from .errors import not_supported
        raise not_supported("tick data", "The engine steps through daily bars; there are no ticks.")


class _NoQuoteDict(SymbolDict):
    """slice.quote_bars: daily Yahoo bars are trade bars; asking for a symbol's quote bar says so."""
    def __getitem__(self, key):
        if dict.__contains__(self, self._k(key)):
            return dict.__getitem__(self, self._k(key))
        from .errors import not_supported
        raise not_supported("bid/ask quote bars", "The engine's daily bars are trade (OHLC) bars only.")


class _SliceGetter:
    """slice.get(TradeBar), slice.get(TradeBar, symbol), slice.get(symbol) and slice.get[TradeBar]()."""
    __slots__ = ('_s',)

    def __init__(self, s):
        self._s = s

    def __call__(self, type_or_symbol=None, symbol=None, default=None):
        return self._s._get(type_or_symbol, symbol, default)

    def __getitem__(self, data_type):
        return lambda symbol=None, default=None: self._s._get(data_type, symbol, default)


class Slice:
    """Data of one time step: the bars of every symbol that has one, keyed by Symbol, plus the
    event collections (splits, dividends, delistings, symbol changes), option and future chains
    and custom data the engine produced for that step (empty when there were none)."""

    margin_interest_rates = _unsupported("margin interest rate data")

    def __init__(self, time: datetime = None):
        self.time = time or datetime.now()
        self.utc_time = self.time
        self.bars = SymbolDict()
        self._data = self.bars
        self.quote_bars = _NoQuoteDict()
        self.ticks = _NoTickDict()
        self.splits = SymbolDict()
        self.dividends = SymbolDict()
        self.delistings = SymbolDict()
        self.symbol_changed_events = SymbolDict()
        self.option_chains = None      # set by the option manager (OptionChains)
        self.future_chains = None      # set by the futures manager (FuturesChains)
        self.custom = SymbolDict()     # custom data points (add_data)
        self.has_data = False

    @property
    def futures_chains(self):
        return self.future_chains

    def _ensure_chains(self):
        if self.option_chains is None or self.future_chains is None:
            from .chains import empty_option_chains, empty_future_chains
            if self.option_chains is None:
                self.option_chains = empty_option_chains()
            if self.future_chains is None:
                self.future_chains = empty_future_chains()

    OptionChains = property(lambda self: self.option_chains)
    FutureChains = property(lambda self: self.future_chains)
    Delistings = property(lambda self: self.delistings)
    SymbolChangedEvents = property(lambda self: self.symbol_changed_events)
    Splits = property(lambda self: self.splits)
    Dividends = property(lambda self: self.dividends)

    def __getitem__(self, symbol):
        v = self.bars.get(symbol)
        if v is None:
            v = self.custom.get(symbol)
        return v

    def __contains__(self, symbol):
        return symbol in self.bars or symbol in self.custom

    def __len__(self):
        return len(self.bars) + len(self.custom)

    def __iter__(self):
        return iter(list(self.bars.keys()) + list(self.custom.keys()))

    def contains_key(self, symbol) -> bool:
        return symbol in self

    def keys(self):
        return list(self.bars.keys()) + list(self.custom.keys())

    def values(self):
        return list(self.bars.values()) + list(self.custom.values())

    def items(self):
        return list(self.bars.items()) + list(self.custom.items())

    @property
    def get(self):
        return _SliceGetter(self)

    _EVENT_TYPES = {'Split': 'splits', 'Splits': 'splits', 'Dividend': 'dividends', 'Dividends': 'dividends',
                    'Delisting': 'delistings', 'Delistings': 'delistings',
                    'SymbolChangedEvent': 'symbol_changed_events', 'SymbolChangedEvents': 'symbol_changed_events'}

    def _get(self, type_or_symbol=None, symbol=None, default=None):
        """data.get(TradeBar) -> bars; data.get(TradeBar, sym) -> that bar; data.get(sym) -> bar."""
        if isinstance(type_or_symbol, type):
            name = getattr(type_or_symbol, '__name__', '')
            if name in ('TradeBar', 'BaseData', 'IBaseDataBar', 'IBaseData'):
                coll = self.bars
            elif name == 'QuoteBar':
                coll = self.quote_bars
            elif name == 'Tick':
                coll = self.ticks
            elif name in self._EVENT_TYPES:
                coll = getattr(self, self._EVENT_TYPES[name])
            elif name == 'MarginInterestRate':
                from .errors import not_supported
                raise not_supported("margin interest rate data")
            else:
                coll = SymbolDict((k, v) for k, v in self.custom.items() if isinstance(v, type_or_symbol))
            return coll.get(symbol, default) if symbol is not None else coll
        if type_or_symbol is None:
            return default
        v = self.bars.get(type_or_symbol)
        if v is None:
            v = self.custom.get(type_or_symbol, default)
        return v

    def add(self, symbol, bar):
        key = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol).upper())
        self.bars[key] = bar
        self.has_data = True

    @property
    def Bars(self):
        return self.bars

    @property
    def Time(self):
        return self.time

    ContainsKey = contains_key
    Keys = property(lambda self: self.keys())
    Values = property(lambda self: self.values())


class SecurityHolding:
    """Holdings of one security. Values include the contract multiplier (options 100, futures per
    contract spec); money amounts are in the security's quote currency."""

    _active = None      # the portfolio's set of keys with a non-zero quantity (kept by the setter)
    _key = None

    @property
    def quantity(self):
        return self._quantity

    @quantity.setter
    def quantity(self, value):
        self._quantity = value
        act = self._active
        if act is not None:
            if value:
                act.add(self._key)
            else:
                act.discard(self._key)

    def __init__(self, symbol: Symbol, quantity: float = 0,
                 average_price: float = 0, market_price: float = 0):
        self.symbol = symbol
        self.quantity = quantity
        self.average_price = average_price
        self.market_price = market_price
        self._multiplier = 1.0
        self._total_close_profit = 0.0
        self.total_sale_volume = 0.0
        self.total_fees = 0.0
        self._security = None

    @property
    def multiplier(self) -> float:
        sec = self._security
        if sec is not None:
            try:
                return float(getattr(sec.symbol_properties, 'contract_multiplier', 1.0) or 1.0)
            except (TypeError, ValueError, AttributeError):
                return 1.0
        return self._multiplier

    @multiplier.setter
    def multiplier(self, value):
        self._multiplier = float(value)

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
        return self.quantity * self.market_price * self.multiplier

    @property
    def unrealized_profit(self) -> float:
        return self.quantity * (self.market_price - self.average_price) * self.multiplier

    unrealised_profit = unrealized_profit

    @property
    def unrealized_profit_percent(self) -> float:
        if self.average_price == 0:
            return 0
        pct = (self.market_price - self.average_price) / self.average_price
        return -pct if self.quantity < 0 else pct

    @property
    def absolute_quantity(self) -> float:
        return abs(self.quantity)

    @property
    def total_close_profit(self) -> float:
        return self._total_close_profit

    @property
    def leverage(self) -> float:
        sec = self._security
        return float(getattr(sec, 'leverage', 1.0) or 1.0) if sec is not None else 1.0

    def update_market_price(self, price: float):
        self.market_price = price

    def get_quantity_value(self, quantity, price=None):
        """LEAN: the value of `quantity` units at the current (or given) price, with the cash
        balance it is denominated in (.amount, .cash)."""
        p = self.market_price if price is None else float(price)
        cash = None
        sec = self._security
        pf = getattr(getattr(sec, '_manager', None), '_portfolio', None) if sec is not None else None
        if pf is not None:
            cash = pf.cash_book.ensure(pf.quote_currency(str(self.symbol).upper()))
        from types import SimpleNamespace
        return SimpleNamespace(amount=float(quantity) * p * self.multiplier, cash=cash,
                               currency=getattr(cash, 'symbol', 'USD'))

    @property
    def hold_stock(self) -> bool:
        return self.quantity != 0

    @property
    def holdings_cost(self) -> float:
        return self.quantity * self.average_price * self.multiplier

    @property
    def absolute_holdings_value(self) -> float:
        return abs(self.holdings_value)

    @property
    def absolute_holdings_cost(self) -> float:
        return abs(self.holdings_cost)

    @property
    def price(self) -> float:
        return self.market_price

    @property
    def net_profit(self) -> float:
        return self._total_close_profit - self.total_fees

    @property
    def profit(self) -> float:
        return self._total_close_profit

    def __repr__(self):
        return f"Holding({self.symbol}: qty={self.quantity}, avg={self.average_price:.2f}, mkt={self.market_price:.2f})"

    def __str__(self):
        return f"{self.symbol}: {self.quantity:g} @ {self.average_price:g}"

    Quantity = property(lambda self: self.quantity)
    AveragePrice = property(lambda self: self.average_price)
    Invested = property(lambda self: self.invested)
    HoldingsValue = property(lambda self: self.holdings_value)
    UnrealizedProfit = property(lambda self: self.unrealized_profit)


_OPEN_STATUSES = (OrderStatus.NEW, OrderStatus.SUBMITTED, OrderStatus.PARTIALLY_FILLED,
                  OrderStatus.UPDATE_SUBMITTED, OrderStatus.CANCEL_PENDING)


def is_open_status(status) -> bool:
    return status in _OPEN_STATUSES


class OrderTicket:
    """A submitted order: LEAN's OrderTicket and Order in one object (transactions.get_order_by_id
    and get_order_ticket return the same object). Updates and cancels go through the broker, which
    validates them and emits the order events, as LEAN's transaction handler does."""

    def __init__(self, order_id: int, symbol: Symbol, quantity: float,
                 order_type: OrderType = OrderType.MARKET,
                 limit_price: float = 0, stop_price: float = 0,
                 tag: str = "", trigger_price: float = 0, trailing_amount: float = 0,
                 trailing_as_percentage: bool = False):
        self.order_id = order_id
        self.symbol = symbol
        self.quantity = quantity
        self.order_type = order_type
        self.limit_price = limit_price
        self.stop_price = stop_price
        self.trigger_price = trigger_price
        self.trailing_amount = trailing_amount
        self.trailing_as_percentage = bool(trailing_as_percentage)
        self.tag = tag
        self.status = OrderStatus.NEW
        self.average_fill_price = 0.0
        self.quantity_filled = 0.0
        self.time = datetime.now()
        self.created_time = None
        self.last_fill_time = None
        self.last_update_time = None
        self.canceled_time = None
        self.submit_request = None
        self.update_requests = []
        self.cancel_request = None
        self.order_events = []
        self.properties = None
        self.time_in_force = None
        self.message = ''
        self.trigger_touched = False
        self._broker = None

    def __int__(self):  # LEAN converts a ticket to its order id implicitly
        return int(self.order_id)

    __index__ = __int__

    # ---- Order view ----
    @property
    def id(self):
        return self.order_id

    @property
    def type(self):
        return self.order_type

    @property
    def price(self):
        return self.average_fill_price

    @property
    def security_type(self):
        return getattr(self.symbol, 'security_type', None)

    @property
    def quantity_remaining(self):
        return self.quantity - self.quantity_filled

    @property
    def absolute_quantity(self):
        return abs(self.quantity)

    @property
    def direction(self):
        return 0 if self.quantity > 0 else (1 if self.quantity < 0 else 2)

    @property
    def value(self):
        return self.quantity * (self.average_fill_price or 0.0)

    def get_value(self, security):
        """LEAN Order.GetValue: quantity x the security's current price x contract multiplier."""
        mult = float(getattr(getattr(security, 'symbol_properties', None), 'contract_multiplier', 1.0) or 1.0)
        return self.quantity * float(getattr(security, 'price', 0.0) or 0.0) * mult

    GetValue = get_value

    @property
    def stop_triggered(self):
        return bool(getattr(self, '_stop_triggered', False))

    @property
    def order_closed(self):
        return not is_open_status(self.status)

    @property
    def has_order(self):
        return True

    # ---- requests ----
    def update(self, fields):
        """Update quantity / prices / tag of the order (LEAN UpdateOrderFields). Returns OrderResponse."""
        from .orders import UpdateOrderRequest
        req = UpdateOrderRequest(getattr(self._broker, 'now', lambda: datetime.now())(), self.order_id, fields)
        self.update_requests.append(req)
        if self._broker is None:
            from .orders import OrderResponse, OrderResponseErrorCode
            req.set_response(OrderResponse.error(req, OrderResponseErrorCode.PROCESSING_ERROR,
                                                 'Order updates need a running backtest'))
            return req.response
        return self._broker.update(self, req)

    def _update_one(self, tag=None, **kw):
        fields = UpdateOrderFields()
        for k, v in kw.items():
            setattr(fields, k, v)
        if tag is not None:
            fields.tag = tag
        return self.update(fields)

    def update_quantity(self, quantity, tag=None):
        return self._update_one(tag, quantity=quantity)

    def update_limit_price(self, price, tag=None):
        return self._update_one(tag, limit_price=price)

    def update_stop_price(self, price, tag=None):
        return self._update_one(tag, stop_price=price)

    def update_trigger_price(self, price, tag=None):
        return self._update_one(tag, trigger_price=price)

    def update_trailing_amount(self, amount, tag=None):
        return self._update_one(tag, trailing_amount=amount)

    def update_tag(self, tag):
        return self._update_one(tag)

    def cancel(self, tag: str = ""):
        """Cancel the order. Returns the OrderResponse (LEAN returns it from ticket.cancel)."""
        from .orders import CancelOrderRequest, OrderResponse, OrderResponseErrorCode
        if self.cancel_request is not None and self.cancel_request.response.is_success:
            return OrderResponse.error(self.cancel_request, OrderResponseErrorCode.REQUEST_CANCELED,
                                       f"Order {self.order_id} has already received a cancellation request.")
        req = CancelOrderRequest(getattr(self._broker, 'now', lambda: datetime.now())(), self.order_id, tag or '')
        self.cancel_request = req
        if self._broker is None:
            if is_open_status(self.status):
                self.status = OrderStatus.CANCELED
                req.set_response(OrderResponse.success(req))
            else:
                req.set_response(OrderResponse.error(req, OrderResponseErrorCode.INVALID_ORDER_STATUS,
                                                     f"Order {self.order_id} is not open ({self.status.name})"))
            return req.response
        return self._broker.cancel(self, req)

    def get(self, field, default=None):
        name = str(getattr(field, 'name', field)).upper()
        key = {'LIMIT_PRICE': 'limit_price', 'LIMITPRICE': 'limit_price', 'STOP_PRICE': 'stop_price',
               'STOPPRICE': 'stop_price', 'TRIGGER_PRICE': 'trigger_price', 'TRIGGERPRICE': 'trigger_price',
               'TRAILING_AMOUNT': 'trailing_amount', 'TRAILING_AS_PERCENTAGE': 'trailing_as_percentage',
               'QUANTITY': 'quantity'}.get(name, name.lower())
        return getattr(self, key, default)

    def to_string(self):
        return str(self)

    def __str__(self):
        return (f"OrderTicket: OrderId: {self.order_id} Symbol: {self.symbol} "
                f"Type: {self.order_type.name} Status: {self.status.name} Quantity: {self.quantity:g} "
                f"Filled: {self.quantity_filled:g} AverageFillPrice: {self.average_fill_price:g}"
                + (f" Tag: {self.tag}" if self.tag else ''))

    def __repr__(self):
        return f"OrderTicket(id={self.order_id}, {self.symbol}, qty={self.quantity}, status={self.status.name})"

    # PascalCase
    Update = update
    Cancel = cancel
    Get = get
    UpdateQuantity = update_quantity
    UpdateLimitPrice = update_limit_price
    UpdateStopPrice = update_stop_price
    UpdateTag = update_tag
    OrderId = property(lambda self: self.order_id)
    Status = property(lambda self: self.status)
    QuantityFilled = property(lambda self: self.quantity_filled)
    AverageFillPrice = property(lambda self: self.average_fill_price)


# kept for code that imported it
def _OrderResponse(success, message=''):
    from .orders import OrderResponse
    return OrderResponse(0, 0 if success else -9, message)


class UpdateOrderFields:
    """Fields for updating an existing order (None = unchanged)."""
    def __init__(self, quantity=None, limit_price=None, stop_price=None, trigger_price=None,
                 trailing_amount=None, tag=None):
        self.quantity = quantity
        self.limit_price = limit_price
        self.stop_price = stop_price
        self.trigger_price = trigger_price
        self.trailing_amount = trailing_amount
        self.tag = tag


class OrderEvent:
    """Event fired when an order's status changes."""

    _next_id = {}

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
        self.is_in_the_money = False
        self.fill_price_currency = 'USD'
        self.limit_price = None
        self.stop_price = None
        self.trigger_price = None
        self.trailing_amount = None
        self.quantity = 0.0
        self.id = 0
        self.order_fee = None
        self.ticket = None

    @property
    def absolute_fill_quantity(self):
        return abs(self.fill_quantity)

    def to_string(self):
        return str(self)

    def __str__(self):
        fee = getattr(getattr(self.order_fee, 'value', None), 'amount', 0.0) or 0.0
        s = (f"Time: {self.utc_time} OrderID: {self.order_id} EventID: {self.id} Symbol: {self.symbol} "
             f"Status: {getattr(self.status, 'name', self.status)} Quantity: {self.quantity:g}")
        if self.fill_quantity:
            s += f" FillQuantity: {self.fill_quantity:g} FillPrice: {self.fill_price:g} {self.fill_price_currency}"
        if self.limit_price:
            s += f" LimitPrice: {self.limit_price:g}"
        if self.stop_price:
            s += f" StopPrice: {self.stop_price:g}"
        if fee:
            s += f" OrderFee: {fee:g} USD"
        if self.message:
            s += f" Message: {self.message}"
        return s

    __repr__ = __str__
    OrderId = property(lambda self: self.order_id)
    Status = property(lambda self: self.status)
    FillPrice = property(lambda self: self.fill_price)
    FillQuantity = property(lambda self: self.fill_quantity)
    Symbol = property(lambda self: self.symbol)
    IsAssignment = property(lambda self: self.is_assignment)


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
        if callable(period) and not isinstance(period, timedelta):
            self.close_time_utc = period(self.generated_time_utc)   # Expiry function
        else:
            self.close_time_utc = self.generated_time_utc + period
        self.score = InsightScore()
        self.tag = ""

    @staticmethod
    def price(symbol, period, direction, magnitude=None, confidence=None,
              source_model="", weight=None, *args, **kwargs):
        """Insight.price(symbol, timedelta|days, direction, ...) or
        Insight.price(symbol, resolution, bar_count, direction, ...)."""
        to_td = getattr(period, 'to_timedelta', None)
        if callable(period) and not isinstance(period, timedelta) and to_td is None:
            pass  # an Expiry function: the close time is computed when the insight is emitted
        elif callable(to_td):
            # (symbol, resolution, bar_count, direction, magnitude, confidence, source_model, weight)
            bars = int(direction) if direction is not None else 1
            direction, magnitude, confidence, source_model, weight = (
                magnitude, confidence, (source_model if source_model != "" else None),
                (weight or ""), (args[0] if args else None))
            period = to_td() * bars
        elif isinstance(period, (int, float)):
            period = timedelta(days=period)
        return Insight(symbol, period, InsightType.PRICE, direction,
                       magnitude, confidence, source_model, weight)

    @staticmethod
    def group(insights):
        return insights

    def is_active(self, utc_time):
        """LEAN: active until its close time (exclusive)."""
        return not self.is_expired(utc_time)

    def is_expired(self, utc_time):
        close = self.close_time_utc
        return close is not None and utc_time is not None and close < utc_time

    def expire(self, utc_time):
        """Close the insight now (LEAN Insight.Expire / cancel)."""
        if self.is_active(utc_time):
            self.close_time_utc = utc_time - timedelta(seconds=1)
            try:
                self.period = self.close_time_utc - self.generated_time_utc
            except TypeError:
                pass

    def cancel(self, utc_time):
        self.expire(utc_time)

    IsActive = is_active
    IsExpired = is_expired


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
        """Target in units for `percent` of portfolio value (same sizing as set_holdings);
        None when the security has no price yet (LEAN returns null)."""
        from .broker import lot_size, round_to_lot
        ticker = str(symbol).upper()
        sec = algorithm.securities.get(ticker) if ticker in algorithm.securities else None
        price = float(getattr(sec, 'price', 0.0) or 0.0) if sec is not None else 0.0
        if price <= 0:
            return None
        tpv = algorithm.portfolio.total_portfolio_value
        settings = getattr(algorithm, 'settings', None)
        buffer = tpv * float(getattr(settings, 'free_portfolio_value_percentage', 0.0) or 0.0)
        broker = getattr(algorithm, '_broker', None)
        cost = 1.0 + ((broker.commission_rate + broker.slippage_rate) if broker is not None else 0.0)
        lot = lot_size(getattr(getattr(sec, 'symbol', None), 'security_type', None))
        qty = round_to_lot((tpv - buffer) * float(percent) / (price * cost), lot)
        if return_delta_quantity and hasattr(algorithm, '_unordered_quantity'):
            qty = round_to_lot(qty - algorithm._unordered_quantity(ticker), lot)
        return PortfolioTarget(sec.symbol if sec is not None else symbol, qty, tag)

    def __repr__(self):
        return f"PortfolioTarget({self.symbol}, {self.quantity})"


def _install_quote_guards():
    """Daily bars are trade (OHLC) bars: reading .bid / .ask raises a clear error instead of
    an AttributeError (a zero-spread copy of the trade prices would be invented data)."""
    from .errors import unsupported_attribute
    TradeBar.bid = unsupported_attribute("bid/ask quote bars", "The engine's daily bars are trade (OHLC) bars only.")
    TradeBar.ask = unsupported_attribute("bid/ask quote bars", "The engine's daily bars are trade (OHLC) bars only.")


_install_quote_guards()
