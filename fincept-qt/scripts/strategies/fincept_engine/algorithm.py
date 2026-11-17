# ============================================================================
# Fincept Terminal - Strategy Engine Core Algorithm
# Pure Python QCAlgorithm compatible base class
# This is the heart of the Fincept Strategy Engine — all strategies inherit
# from this class. It provides the same API surface as LEAN's QCAlgorithm
# but runs entirely in pure Python with no .NET dependency.
# ============================================================================

import json
import math
import logging
from datetime import datetime, timedelta, date, time as dt_time
from typing import Dict, List, Optional, Callable, Any
from collections import deque

# numpy / pandas are imported on first use (history frames): the strategy catalogue and
# strategies that never build a DataFrame do not pay for importing pandas.
from ._lazy import LazyModule as _LazyModule
np = _LazyModule('numpy')
pd = _LazyModule('pandas')

from .enums import (
    Resolution, SecurityType, OrderType, OrderStatus,
    Market, InsightDirection, InsightType, DataNormalizationMode
)
from .types import (
    Symbol, Slice, TradeBar, QuoteBar, Tick,
    SecurityHolding, OrderTicket, OrderEvent, UpdateOrderFields,
    Insight, PortfolioTarget
)
from .indicators import (
    IndicatorBase, ExponentialMovingAverage, SimpleMovingAverage,
    MovingAverageConvergenceDivergence, RelativeStrengthIndex,
    BollingerBands, AverageTrueRange, Stochastic,
    RateOfChange, Momentum, WilliamsPercentR,
    CommodityChannelIndex, AverageDirectionalIndex,
    AroonOscillator, VolumeWeightedAveragePrice, IntradayVwap, StandardDeviation,
    Minimum, Maximum, Identity, FilteredIdentity, RegressionChannel, MomentumPercent,
    Beta as _BetaIndicator, feed_indicator,
)
from .errors import NotSupportedByEngine, not_supported
from .broker import _DEFAULT_LEVERAGE, lot_size, round_to_lot
from .portfolio import SecurityPortfolioManager
from .securities import SecurityManager, Security
from .scheduling import ScheduleManager, DateRules, TimeRules
from .consolidators import (
    TradeBarConsolidator, QuoteBarConsolidator,
    RenkoConsolidator, RangeConsolidator, TickConsolidator
)
from .universe import UniverseSettings, Universe

logger = logging.getLogger("fincept.strategy")


class _AlgorithmMode:
    """Internal AlgorithmMode enum matching LEAN's AlgorithmMode."""
    BACKTESTING = 0; RESEARCH = 1; LIVE = 2
    Backtesting = 0; Research = 1; Live = 2
    def __eq__(self, other): return int(self) == int(other)
    def __repr__(self): return f"AlgorithmMode({self})"


class _OptionContract:
    """Represents a single option contract in a chain."""
    def __init__(self, symbol, underlying, strike, right, expiry):
        self.symbol = symbol
        self.underlying = underlying
        self.strike = strike
        self.strike_price = strike
        self.right = right
        self.expiry = expiry
        self.bid_price = 1.0
        self.ask_price = 1.5
        self.last_price = 1.25
        self.volume = 100
        self.open_interest = 500
        # Vary IV and greeks based on strike distance from underlying price
        base_price = getattr(underlying, '_chain_base_price', 100.0) if hasattr(underlying, '_chain_base_price') else 100.0
        moneyness = abs(strike - base_price) / max(base_price, 1) if base_price else 0
        self.implied_volatility = 0.3 + moneyness * 0.5 + 0.3  # Range roughly 0.6 to 1.1
        delta = max(0.05, min(0.95, 0.5 - moneyness * 1.5))
        self.greeks = type('Greeks', (), {
            'delta': delta, 'gamma': 0.05, 'vega': 0.1, 'theta': -0.02, 'rho': 0.01,
            'implied_volatility': self.implied_volatility
        })()
        self.id = type('ContractId', (), {
            'strike_price': strike, 'option_right': right,
            'date': expiry, 'expiration': expiry, 'underlying': underlying
        })()
        self.id.Date = expiry
        self.id.StrikePrice = strike
        self.id.OptionRight = right

    def __repr__(self):
        return f"OptionContract({self.symbol}, strike={self.strike}, right={self.right})"


class _OptionChainResult:
    """Result of option_chain() with .contracts dict-like access and list-like iteration."""
    def __init__(self, underlying=None):
        self.contracts = {}
        self.underlying = underlying
        self._list = []
    def __iter__(self):
        return iter(self._list)
    def __len__(self):
        return len(self._list)
    def __getitem__(self, idx):
        if isinstance(idx, int):
            return self._list[idx]
        return self.contracts.get(str(idx))
    def __bool__(self):
        return len(self._list) > 0
    @property
    def data_frame(self):
        if pd is not None:
            data = []
            for c in self._list:
                data.append({
                    'symbol': str(c.symbol),
                    'expiry': c.expiry,
                    'strike': c.strike,
                    'right': c.right,
                    'impliedvolatility': c.implied_volatility,
                    'delta': c.greeks.delta,
                    'gamma': c.greeks.gamma,
                    'vega': c.greeks.vega,
                    'theta': c.greeks.theta,
                    'rho': c.greeks.rho,
                    'bid_price': c.bid_price,
                    'ask_price': c.ask_price,
                    'last_price': c.last_price,
                    'volume': c.volume,
                    'open_interest': c.open_interest,
                })
            if data:
                df = pd.DataFrame(data)
                # Use the actual symbol objects as index so .name returns Symbol
                df.index = pd.Index([c.symbol for c in self._list], name=None)
                return df
            return pd.DataFrame()
        return None


class _OptionChainsAccessor(dict):
    """self.option_chains(symbols): the full model-priced chains of those underlyings / canonical
    options at the current time (OptionChains)."""
    def __init__(self, algo):
        super().__init__()
        self._algo = algo

    def __call__(self, symbols=None, flatten=False):
        return self._algo.option_chains_for(symbols, flatten)


class _OptionChainsResult(dict):
    """Result of option_chains() call with .data_frame support."""
    def __init__(self):
        super().__init__()
        self._chains = []

    @property
    def data_frame(self):
        if pd is not None:
            all_data = []
            all_index = []
            for canonical_sym, chain in self._chains:
                for c in chain._list:
                    row = {
                        'expiry': c.expiry,
                        'strike': c.strike,
                        'right': c.right,
                        'impliedvolatility': c.implied_volatility,
                        'delta': c.greeks.delta,
                        'gamma': c.greeks.gamma,
                        'vega': c.greeks.vega,
                        'theta': c.greeks.theta,
                        'rho': c.greeks.rho,
                        'bid_price': c.bid_price,
                        'ask_price': c.ask_price,
                        'last_price': c.last_price,
                        'volume': c.volume,
                        'open_interest': c.open_interest,
                    }
                    all_data.append(row)
                    all_index.append((canonical_sym, c.symbol))
            if all_data:
                idx = pd.MultiIndex.from_tuples(all_index, names=['canonical', 'contract'])
                return pd.DataFrame(all_data, index=idx)
            return pd.DataFrame()
        return None


class _UniverseReference:
    """Reference to an added universe, supporting .data_type, .symbol, etc."""
    def __init__(self, selector=None, data_type=None, is_etf=False):
        self._selector = selector
        self._is_etf = is_etf
        self.data_type = data_type or type('FundamentalData', (), {'__name__': 'FundamentalData'})
        self.symbol = Symbol('UNIVERSE', SecurityType.BASE, Market.USA) if Symbol else None
    def __repr__(self):
        return f"Universe(selector={self._selector})"


class _AlgorithmSettings:
    """Algorithm-level settings."""
    def __init__(self):
        self.free_portfolio_value = 0
        self.free_portfolio_value_percentage = 0.0025
        self.liquidate_enabled = True
        self.max_absolute_portfolio_target_percentage = 1000
        self.min_absolute_portfolio_target_percentage = 0
        self.rebalance_portfolio_on_security_changes = True
        self.rebalance_portfolio_on_insight_changes = True
        self.data_subscription_limit = 10000
        self.staleness_price_time_resolution = None
        self.warmup_resolution = None
        self.database_path = ""
        self.automatic_indicator_warm_up = False
        self.minimum_order_margin_portfolio_percentage = 0.0
        self.seed_initial_prices = False


class _OrderProperties:
    """Default properties applied to new orders (LEAN default_order_properties).
    time_in_force DAY makes an unfilled order expire after the first bar it could fill on."""
    def __init__(self, time_in_force=None, **kwargs):
        from .enums import TimeInForce
        self.time_in_force = time_in_force or TimeInForce.GOOD_TIL_CANCELED
        for k, v in kwargs.items():
            setattr(self, k, v)


_OPEN_ORDER_STATUSES = (OrderStatus.SUBMITTED, OrderStatus.PARTIALLY_FILLED, OrderStatus.NEW,
                        OrderStatus.UPDATE_SUBMITTED, OrderStatus.CANCEL_PENDING)


class Transactions:
    """Order transaction manager (LEAN SecurityTransactionManager)."""

    def __init__(self, algorithm=None):
        self._orders: Dict[int, OrderTicket] = {}
        self._order_counter = 0
        self._algo = algorithm

    # ---- requests (LEAN: transactions.add_order / process_request) ----
    def add_order(self, request):
        """Submit a SubmitOrderRequest; returns its ticket."""
        algo = self._algo
        if algo is None:
            raise not_supported("order requests outside a backtest run")
        return algo._submit_request(request)

    def process_request(self, request):
        from .orders import SubmitOrderRequest, UpdateOrderRequest, CancelOrderRequest
        if isinstance(request, SubmitOrderRequest):
            return self.add_order(request)
        ticket = self._orders.get(int(getattr(request, 'order_id', 0) or 0))
        if ticket is None:
            from .orders import OrderResponse, OrderResponseErrorCode
            request.set_response(OrderResponse.error(request, OrderResponseErrorCode.UNABLE_TO_FIND_ORDER,
                                                     f"Unable to locate order with id {getattr(request, 'order_id', 0)}"))
            return None
        if isinstance(request, CancelOrderRequest):
            ticket.cancel(request.tag)
            return ticket
        if isinstance(request, UpdateOrderRequest):
            fields = UpdateOrderFields(request.quantity, request.limit_price, request.stop_price,
                                       request.trigger_price, request.trailing_amount, request.tag)
            ticket.update(fields)
            return ticket
        raise not_supported(f"order request type {type(request).__name__}")

    def update_order(self, request):
        return self.process_request(request)

    def get_open_orders(self, symbol=None) -> List[OrderTicket]:
        orders = [o for o in self._orders.values() if o.status in _OPEN_ORDER_STATUSES]
        if symbol:
            ticker = str(symbol).upper()
            orders = [o for o in orders if str(o.symbol).upper() == ticker]
        return orders

    def get_orders(self, filter_or_symbol=None):
        """LEAN: an iterable of orders (optionally a filter function or a symbol)."""
        tickets = [self.get_order_by_id(t.order_id) for t in self._orders.values()]
        if callable(filter_or_symbol) and not isinstance(filter_or_symbol, (str, Symbol)):
            tickets = [t for t in tickets if filter_or_symbol(t)]
        elif filter_or_symbol is not None:
            ticker = str(filter_or_symbol).upper()
            tickets = [t for t in tickets if str(t.symbol).upper() == ticker]
        return _OrderList(tickets)

    def get_order_tickets(self, filter_func=None) -> List[OrderTicket]:
        tickets = list(self._orders.values())
        if filter_func:
            tickets = [t for t in tickets if filter_func(t)]
        return _OrderList(tickets)

    def get_open_order_tickets(self, filter_or_symbol=None):
        open_ = [t for t in self._orders.values() if t.status in _OPEN_ORDER_STATUSES]
        if callable(filter_or_symbol) and not isinstance(filter_or_symbol, (str, Symbol)):
            open_ = [t for t in open_ if filter_or_symbol(t)]
        elif filter_or_symbol is not None:
            ticker = str(filter_or_symbol).upper()
            open_ = [t for t in open_ if str(t.symbol).upper() == ticker]
        return _OrderList(open_)

    def get_order_by_id(self, order_id):
        """The order (LEAN Order subclass: MarketOrder, LimitOrder, ...), a live view of its ticket."""
        t = self._orders.get(int(getattr(order_id, 'order_id', order_id)))
        if t is None:
            return None
        from .orders import Order
        v = getattr(t, '_order_view', None)
        if v is None:
            v = Order.view(t)
            t._order_view = v
        return v

    def get_order_ticket(self, order_id):
        return self._orders.get(int(getattr(order_id, 'order_id', order_id)))

    def cancel_order(self, order_id, tag: str = ""):
        t = self._orders.get(int(order_id))
        if t is not None:
            t.cancel(tag)
        return t

    def cancel_open_orders(self, symbol=None, tag: str = ""):
        cancelled = []
        for order in self.get_open_orders(symbol):
            order.cancel(tag)
            cancelled.append(order)
        return _OrderList(cancelled)

    def get_open_orders_remaining_quantity(self, symbol=None) -> float:
        return sum(t.quantity - t.quantity_filled for t in self.get_open_orders(symbol))

    def get_order_events(self, order_id):
        t = self._orders.get(int(order_id))
        return list(t.order_events) if t is not None else []

    @property
    def orders_count(self) -> int:
        return len(self._orders)

    @property
    def last_order_id(self) -> int:
        return self._order_counter

    GetOrders = get_orders
    GetOrderTickets = get_order_tickets
    GetOpenOrders = get_open_orders
    GetOrderById = get_order_by_id
    CancelOpenOrders = cancel_open_orders
    AddOrder = add_order
    ProcessRequest = process_request


class _CallableList(list):
    """A list that can also be called (LEAN Python exposes .values / .keys as properties)."""
    def __call__(self):
        return self


class _ActiveSecurities:
    def __init__(self, manager):
        self._m = manager
    def __getitem__(self, key):
        return self._m[key]
    def __contains__(self, key):
        return key in self._m
    def __iter__(self):
        return iter(self._m)
    def __len__(self):
        return len(self._m)
    def get(self, key, default=None):
        return self._m.get(key, default)
    def contains_key(self, key):
        return key in self._m
    @property
    def values(self):
        return _CallableList(self._m.values())
    @property
    def keys(self):
        return _CallableList(s.symbol for s in self._m.values())
    def items(self):
        return [(s.symbol, s) for s in self._m.values()]
    @property
    def count(self):
        return len(self._m)


class _OrderList(list):
    """List of orders that also answers the dict-style calls older engine code made."""
    def values(self):
        return list(self)

    @property
    def count(self):
        return len(self)


class _MarketHoursDatabase:
    """Market hours by security type (market_hours.ExchangeHours: regular sessions, NYSE holidays)."""
    @staticmethod
    def from_data_folder():
        return _MarketHoursDatabase()

    def get_exchange_hours(self, market, symbol=None, security_type=None):
        """get_exchange_hours(market, symbol, security_type) or get_exchange_hours(subscription_config)."""
        from .market_hours import ExchangeHours
        if symbol is None and security_type is None and hasattr(market, 'symbol'):
            cfg = market
            sym = cfg.symbol
            security_type = getattr(cfg, 'security_type', None) or getattr(sym, 'security_type', None)
            market = getattr(cfg, 'market', None) or getattr(sym, 'market', None)
        if security_type is None:
            security_type = getattr(symbol, 'security_type', SecurityType.EQUITY)
        return ExchangeHours.for_security_type(security_type, market)

    GetExchangeHours = get_exchange_hours

    @staticmethod
    def get_database_symbol_key(symbol):
        """The database key of a symbol (LEAN: the ticker for custom data, the root otherwise)."""
        sid = getattr(symbol, 'id', None)
        return str(getattr(sid, 'symbol', None) or getattr(symbol, 'value', symbol))

    GetDatabaseSymbolKey = get_database_symbol_key
    def set_entry(self, market, symbol, security_type, exchange_hours, *args, **kwargs):
        pass
    def get_entry(self, market, symbol=None, security_type=None):
        from .securities import ExchangeHours, SymbolProperties
        return type('Entry', (), {
            'exchange_hours': ExchangeHours(),
            'data_time_zone': 'America/New_York',
            'symbol_properties': SymbolProperties(),
        })()
    SetEntry = set_entry
    GetEntry = get_entry


class _SymbolPropertiesDatabase:
    """Symbol properties database stub."""
    @staticmethod
    def from_data_folder():
        return _SymbolPropertiesDatabase()
    def get_symbol_properties(self, market, symbol, security_type, currency="USD"):
        from .securities import SymbolProperties
        return SymbolProperties()
    def set_entry(self, *args, **kwargs):
        pass
    SetEntry = set_entry
    GetSymbolProperties = get_symbol_properties


class _ObjectStoreKVP:
    """Key-value pair for ObjectStore iteration."""
    def __init__(self, key, value):
        self.key = key
        self.value = type('StoreValue', (), {
            'length': len(str(value)) if value else 0,
            '__str__': lambda self: str(value),
            '__repr__': lambda self: str(value),
        })() if value is not None else None
    Key = property(lambda self: self.key)
    Value = property(lambda self: self.value)


class ObjectStore:
    """Simple key-value object store."""

    def __init__(self):
        self._store: Dict[str, str] = {}

    def save(self, key: str, value: str):
        self._store[key] = value

    def read(self, key: str) -> str:
        return self._store.get(key, "")

    def save_json(self, key: str, obj):
        self._store[key] = json.dumps(obj)

    def read_json(self, key: str):
        val = self._store.get(key, "{}")
        return json.loads(val)

    def contains_key(self, key: str) -> bool:
        return key in self._store

    def delete(self, key: str):
        self._store.pop(key, None)

    def save_bytes(self, key: str, data: bytes):
        self._store[key] = data

    def read_bytes(self, key: str) -> bytes:
        return self._store.get(key, b"")

    def get_file_path(self, key: str) -> str:
        """Return a virtual file path for the key."""
        return f"/objectstore/{key}"

    def __getitem__(self, key):
        return self._store.get(key, self._store.get(str(key), ""))

    def __setitem__(self, key, value):
        self._store[key] = value

    def __contains__(self, key):
        return key in self._store

    def __iter__(self):
        """Iterate as key-value pairs with .key and .value attributes."""
        return iter([_ObjectStoreKVP(k, v) for k, v in self._store.items()])


class _SubscriptionDataConfig:
    """Represents a single data subscription."""
    def __init__(self, symbol, resolution=None, tick_type=None):
        self.symbol = symbol
        self.resolution = resolution
        self.tick_type = tick_type
        self.consolidator = None
        self.consolidators = _OrderList()
        self.data_normalization_mode = 1  # DataNormalizationMode.ADJUSTED (RAW is 0)
        self.data_type = None
        self.is_custom_data = False

    def __eq__(self, other):
        if isinstance(other, _SubscriptionDataConfig):
            return str(self.symbol) == str(other.symbol)
        return False

    def __getattr__(self, name):
        # Fallback for any missing attribute
        if name.startswith('_'):
            raise AttributeError(name)
        return None


class SubscriptionManager:
    """Manages data subscriptions."""

    def __init__(self):
        self._subscriptions = []

    def add_subscription(self, symbol, resolution=None, data_normalization_mode=None):
        """Add a data subscription for a symbol."""
        # Add trade + quote subscriptions (like LEAN does for equities)
        config = _SubscriptionDataConfig(symbol, resolution, 'trade')
        if data_normalization_mode is not None:
            config.data_normalization_mode = data_normalization_mode
        self._subscriptions.append(config)
        config2 = _SubscriptionDataConfig(symbol, resolution, 'quote')
        if data_normalization_mode is not None:
            config2.data_normalization_mode = data_normalization_mode
        self._subscriptions.append(config2)

    def add_consolidator(self, symbol, consolidator):
        config = _SubscriptionDataConfig(symbol)
        config.consolidator = consolidator
        self._subscriptions.append(config)

    def remove_consolidator(self, symbol, consolidator):
        self._subscriptions = [
            s for s in self._subscriptions
            if not (str(s.symbol) == str(symbol) and s.consolidator is consolidator)
        ]

    @property
    def subscriptions(self):
        return self._subscriptions

    def get_subscription_data_configs(self, symbol=None, *args, **kwargs):
        if symbol:
            return [s for s in self._subscriptions if str(s.symbol) == str(symbol)]
        return self._subscriptions

    @property
    def subscription_data_config_service(self):
        return self


class SecurityChanges:
    """Tracks securities added/removed from universe."""

    def __init__(self, added=None, removed=None):
        self.added_securities = added or []
        self.removed_securities = removed or []

    @property
    def count(self):
        return len(self.added_securities) + len(self.removed_securities)

    @staticmethod
    def none():
        return SecurityChanges()


class _DataDictionary(dict):
    """Dict-like object keyed by symbol for typed history multi-symbol results.
    Supports .values()[0] style access."""
    def values(self):
        return list(super().values())


class _HistoryRow:
    """A row from history iteration that supports both attribute and dict-style access.
    When accessed as a Slice (data[symbol]), returns self for column data.
    """
    def __init__(self):
        self._data = {}
        self.time = None
        self.end_time = None
        self.value = 0.0
        self.price = 0.0
    def __getitem__(self, key):
        # Support both column access ('close') and symbol access (Symbol object)
        if isinstance(key, str) and key in self._data:
            return self._data.get(key, 0.0)
        # If key is a Symbol or string that's not a column, return self as the bar
        return self
    def __setitem__(self, key, value):
        self._data[key] = value
        setattr(self, key, value)
    def __contains__(self, key):
        return True  # Pretend all symbols are present
    def keys(self):
        """Return empty keys to avoid iteration over columns when used as Slice."""
        return []
    def values(self):
        return self._data.values()
    def __repr__(self):
        return f"HistoryRow(time={self.time}, {self._data})"


class _SafeLoc:
    """Wraps DataFrame.loc to return empty DataFrame on KeyError instead of raising."""
    def __init__(self, df):
        self._df = df
    def __getitem__(self, key):
        try:
            result = self._df.loc[key]
            if isinstance(result, pd.DataFrame):
                return _HistoryDataFrame(result)
            return result
        except KeyError:
            return _HistoryDataFrame(pd.DataFrame(columns=self._df.columns))


class _HistoryDataFrame:
    """Wraps a pandas DataFrame so iteration yields TradeBar-like row objects
    instead of column-name strings.  Also supports .loc, .index, .unstack, len(), etc."""
    def __init__(self, df):
        self._df = df
    @property
    def loc(self):
        return _SafeLoc(self._df)
    # delegate attribute access to underlying DF
    def __getattr__(self, name):
        return getattr(self._df, name)
    def __len__(self):
        return len(self._df)
    def __bool__(self):
        return len(self._df) > 0
    def __iter__(self):
        """Iterate rows as Slice-like namespace objects with .time, .open, .close, etc."""
        from .types import TradeBar, Symbol
        for idx, row in self._df.iterrows():
            bar = _HistoryRow()
            bar.time = idx if not isinstance(idx, (int, float)) else datetime.now()
            bar.end_time = bar.time
            for col in row.index:
                setattr(bar, col, row[col])
                bar._data[col] = row[col]
            if 'close' in row.index:
                bar.value = row.get('close', 0)
                bar.price = row.get('close', 0)
            yield bar
    def __getitem__(self, key):
        result = self._df[key]
        if isinstance(result, pd.DataFrame):
            return _HistoryDataFrame(result)
        return result
    def __repr__(self):
        return repr(self._df)
    def __str__(self):
        return str(self._df)


class _HistoryAccessor:
    """Makes self.history both callable and subscriptable."""
    def __init__(self, algo, data_type=None):
        self._algo = algo
        self._data_type = data_type
    def __call__(self, *args, **kwargs):
        # Validate: period-based tick history is not allowed
        if self._data_type is not None:
            from .types import Tick, Symbol
            dt_name = getattr(self._data_type, '__name__', '')
            if dt_name == 'Tick' or self._data_type is Tick:
                # Find the period arg (skip symbol/list args at start)
                non_sym_args = []
                for a in args:
                    if isinstance(a, (Symbol, list, tuple)):
                        continue
                    if isinstance(a, str):
                        continue
                    non_sym_args.append(a)
                # First non-symbol arg: could be int count, timedelta, resolution, etc.
                period_arg = non_sym_args[0] if non_sym_args else None
                if isinstance(period_arg, int) and not isinstance(period_arg, bool):
                    from .algorithm_imports import InvalidOperationException
                    raise InvalidOperationException(
                        "Period-based history requests are not allowed with Tick resolution")
                kwargs['_force_resolution'] = Resolution.TICK
            # Pass the data type for custom data handling
            kwargs['_typed_data_type'] = self._data_type
        return self._algo._history_impl(*args, **kwargs)
    def __getitem__(self, data_type):
        # self.history[TradeBar](...) → returns a callable with type info
        return _HistoryAccessor(self._algo, data_type)
    def __repr__(self):
        return "<HistoryAccessor>"


class EventHandler_:
    """Simple event handler that supports += and -= syntax."""
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


class SignalExportManager:
    """Signal exports (Collective2, CrunchDAO, Numerai...). As in LEAN, targets are only sent in live
    trading: in a backtest set_target_portfolio does nothing and reports success."""
    def __init__(self, algorithm=None):
        self._providers = []
        self._algorithm = algorithm
        self.automatic_export_time_span = None

    def add_signal_export_providers(self, *providers):
        self._providers.extend(providers)

    def add_signal_export_provider(self, provider):
        self._providers.append(provider)

    def set_signal_export_provider(self, provider):
        self._providers = [provider]

    def set_target_portfolio(self, *targets):
        return True

    def set_target_portfolio_from_portfolio(self):
        algo = self._algorithm
        if algo is not None and algo.portfolio.total_portfolio_value <= 0:
            return False
        return True

    def flush(self, *args):
        pass

    SetTargetPortfolio = set_target_portfolio
    SetTargetPortfolioFromPortfolio = set_target_portfolio_from_portfolio


class _FuncBenchmark:
    """set_benchmark(lambda dt: value)."""
    def __init__(self, fn):
        self._fn = fn

    def evaluate(self, time):
        return float(self._fn(time))

    Evaluate = evaluate


class _SecurityBenchmark:
    """set_benchmark(symbol): the benchmark value is the security's price at the time asked (the
    last bar complete then; the security is priced from the run's data)."""
    def __init__(self, algorithm, symbol):
        self._algo = algorithm
        self.symbol = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol).upper())

    @property
    def security(self):
        return self._algo.securities.get(str(self.symbol).upper())

    Security = security

    def evaluate(self, time):
        data = self._algo._data
        if data is None:
            return 0.0
        s = data.series(self.symbol, getattr(self.symbol, 'security_type', SecurityType.EQUITY),
                        getattr(self.symbol, 'market', None))
        if s is None:
            return 0.0
        n = s.count_known_at(time)
        return float(s.close[n - 1]) if n > 0 else 0.0

    Evaluate = evaluate


class FutureChainProvider:
    """Listed futures contracts of a root on a date (exchange calendar; see futures.py)."""
    def get_future_contract_list(self, symbol, date=None):
        from .futures import contract_expiries, contract_symbol
        root = getattr(getattr(symbol, 'id', None), 'symbol', None) or str(symbol).lstrip('/')
        d = (date or datetime.now())
        d = d.date() if isinstance(d, datetime) else d
        return [contract_symbol(root, e, getattr(symbol, 'market', 'cme')) for e in contract_expiries(root, d)]
    GetFutureContractList = get_future_contract_list


# The parameter set a run starts with when the request sets none (LEAN's default config.json "parameters"):
# get_parameter() returns these before the strategy's own default. The catalogue reports them as the defaults.
DEFAULT_PARAMETERS = {"ema-fast": "10", "ema-slow": "20", "ema-medium": "14"}


class QCAlgorithm:
    """
    Fincept Terminal Strategy Engine - QCAlgorithm Compatible Base Class

    All strategy files inherit from this class. Provides the same API as
    QuantConnect's QCAlgorithm but runs in pure Python.

    Lifecycle:
        1. initialize() - Set dates, cash, add securities, indicators
        2. on_data(data) - Called on each new data point
        3. on_end_of_algorithm() - Called when backtest completes

    Supports:
        - Equity, Futures, Options, Crypto, Forex, Index, CFD
        - 13+ built-in indicators (SMA, EMA, MACD, RSI, BB, ATR, etc.)
        - Portfolio management with P&L tracking
        - Order management (market, limit, stop, trailing)
        - Scheduled events (date/time rules)
        - Data consolidation (time, renko, range, tick)
        - Universe selection (coarse/fine fundamental)
        - Alpha model framework (insights, portfolio targets)
        - Indian market support (NSE, BSE, MCX, NFO)
    """

    def __init__(self):
        # Time
        self._start_date = datetime(2020, 1, 1)
        self._end_date = datetime(2024, 1, 1)
        self._time = datetime.now()
        self._utc_time = datetime.utcnow()

        # Portfolio & Securities
        self.portfolio = SecurityPortfolioManager(100000)
        self.securities = SecurityManager()
        self.securities._portfolio = self.portfolio
        self.securities._algorithm = self
        self.portfolio._securities = self.securities
        self.portfolio._algo = self
        self.transactions = Transactions(self)
        self._benchmark = None
        self.current_slice = None
        from .statistics import AlgorithmStatistics
        self.statistics = AlgorithmStatistics(self)

        # Scheduling
        self.schedule = ScheduleManager(self)
        self.date_rules = DateRules(self)
        self.time_rules = TimeRules(self)

        # Universe (universe_selection.UniverseManager: coarse/fundamental/ETF/custom selectors and
        # the framework universe models; data from universe_data.UniverseDataProvider)
        self.universe_settings = UniverseSettings()
        from .universe_selection import UniverseManager
        self._universe_manager = UniverseManager(self, None, self._universe_subscribe, self._universe_held)
        self._universe_definitions = None

        # State
        self._indicators: Dict[str, IndicatorBase] = {}
        self._consolidators = []
        self._benchmarks = {}
        self._charts = {}
        self._logs: List[str] = []
        self._debug_msgs: List[str] = []
        self._runtime_statistics = {}
        self._parameters: Dict[str, str] = dict(DEFAULT_PARAMETERS)
        self._warm_up_period = 0
        self._is_warming_up = False
        self._account_currency = "USD"
        self._brokerage_model = None
        self._algorithm_mode = _AlgorithmMode.BACKTESTING
        self._deployment_target = 0  # DeploymentTarget.LOCAL_PLATFORM
        self._live_mode = False
        self._name = self.__class__.__name__

        # Object store
        self.object_store = ObjectStore()

        # Market hours database
        self.market_hours_database = _MarketHoursDatabase()
        self.symbol_properties_database = _SymbolPropertiesDatabase()

        # Subscription manager
        self.subscription_manager = SubscriptionManager()

        # Framework models
        self._alpha_model = None
        self._portfolio_construction = None
        self._execution_model = None
        self._risk_management = None
        self._universe_selection = None

        # Security changes
        # The latest SecurityChanges (engine-internal). LEAN's QCAlgorithm has no `changes`
        # attribute: strategies commonly keep their own self.changes / self._changes.
        self._fe_changes = SecurityChanges()

        # Notification
        self.notify = NotificationManager()

        # Settings
        self.settings = _AlgorithmSettings()

        # Signal export manager
        self.signal_export = SignalExportManager(self)

        # Brokerage model
        from .extensions import DefaultBrokerageModel
        self.brokerage_model = DefaultBrokerageModel()

        # Insights generated event
        self.insights_generated = EventHandler_()

        # Insights collection
        self.insights = InsightCollection()
        self.insights._algorithm = self

        # Future/Option chain providers
        self.future_chain_provider = FutureChainProvider()

        # Option chains (dict-like and callable)
        self.option_chains = _OptionChainsAccessor(self)

        # Identity indicator cache
        self._identity_indicators = {}

        # Max tags
        self.MAX_TAGS_COUNT = 20

        # Security initializer
        self.security_initializer = None

        # ---- Backtest engine wiring (set by the runner) ----
        # Every indicator created through a factory / register_indicator, with the
        # symbol(s) whose bars update it: [{'indicator', 'symbols', 'selector'}]
        self._registered_indicators = []
        self._broker = None             # fincept_engine.broker.BacktestBroker
        self._data = None               # fincept_engine.data.DataSource
        self._forced_start = None       # the run's dates win over set_start_date()
        self._forced_end = None
        self._strategy_dates = {}       # what the strategy itself asked for
        self._next_fill_date = None     # first session an order placed now may fill on
        self._warmup_bars = 0
        self._warmup_timedelta = None
        self._custom_data = []          # tickers subscribed through add_data()
        self._dynamic_universe = None   # description of a universe the engine cannot run
        self._engine_notes = []         # disclosures collected during the run
        self._quit_requested = False
        self._pending_insights = []
        self.default_order_properties = _OrderProperties()

    # ---- Time Properties ----

    @property
    def time(self) -> datetime:
        return self._time

    @time.setter
    def time(self, value):
        self._time = value
        self._utc_time = value

    @property
    def utc_time(self) -> datetime:
        return self._utc_time

    @property
    def start_date(self) -> datetime:
        return self._start_date

    @property
    def end_date(self) -> datetime:
        return self._end_date

    @property
    def is_warming_up(self) -> bool:
        return self._is_warming_up

    @property
    def live_mode(self) -> bool:
        return self._live_mode

    @property
    def algorithm_mode(self):
        return self._algorithm_mode

    @property
    def deployment_target(self):
        return self._deployment_target

    @property
    def name(self) -> str:
        return self._name

    @name.setter
    def name(self, value):
        self._name = value

    # ---- Initialization Methods ----

    def initialize(self):
        """Override this method to set up your algorithm."""
        pass

    @staticmethod
    def _parse_date_args(year_or_date=None, month=None, day=None, year=None):
        if year is not None:
            return datetime(year, month or 1, day or 1)
        if isinstance(year_or_date, datetime):
            return year_or_date
        if isinstance(year_or_date, date):
            return datetime.combine(year_or_date, dt_time())
        if year_or_date is not None and month and day:
            return datetime(year_or_date, month, day)
        return None

    def set_start_date(self, year_or_date=None, month=None, day=None, *, year=None):
        d = self._parse_date_args(year_or_date, month, day, year)
        if d is None:
            return
        self._strategy_dates['start'] = d
        if self._forced_start is not None:
            return  # the backtest request's dates win; the strategy's are only recorded
        self._start_date = d
        # In backtesting, algorithm time starts at start_date
        self._time = self._start_date
        self._utc_time = self._start_date

    def set_end_date(self, year_or_date=None, month=None, day=None, *, year=None):
        d = self._parse_date_args(year_or_date, month, day, year)
        if d is None:
            return
        self._strategy_dates['end'] = d
        if self._forced_end is not None:
            return
        self._end_date = d

    def set_cash(self, amount_or_currency=None, amount=None, conversion_rate=None, *, starting_cash=None):
        """set_cash(amount) - the account currency balance; set_cash("EUR", amount[, conversion_rate]) -
        another currency's balance in the cash book."""
        if starting_cash is not None:
            self.portfolio.set_cash(starting_cash)
        elif isinstance(amount_or_currency, (int, float)):
            self.portfolio.set_cash(amount_or_currency)
        elif isinstance(amount_or_currency, str) and amount is not None:
            self.portfolio.set_cash(amount_or_currency, amount, conversion_rate)
            if amount_or_currency.upper() != self.portfolio.cash_book.account_currency:
                self._multi_currency = True

    def set_account_currency(self, currency: str, starting_cash: float = None):
        self._account_currency = str(currency).upper()
        self.portfolio.set_account_currency(currency, starting_cash)

    @property
    def account_currency(self):
        return self.portfolio.cash_book.account_currency

    def set_benchmark(self, symbol=None, *args):
        """A ticker/Symbol (its price is the benchmark) or a function of time."""
        if symbol is None:
            return
        self._benchmark_spec = symbol
        if callable(symbol) and not isinstance(symbol, (str, Symbol)):
            self._benchmark = _FuncBenchmark(symbol)
        else:
            self._benchmarks['default'] = str(symbol)
            self._benchmark = _SecurityBenchmark(self, symbol)

    @property
    def benchmark(self):
        if self._benchmark is None:
            model = self.brokerage_model
            getter = getattr(model, 'get_benchmark', None)
            if getter is not None:
                try:
                    b = getter(self.securities)
                except TypeError:
                    b = None
                if b is not None:
                    self._benchmark = b
        if self._benchmark is None:
            self._benchmark = _SecurityBenchmark(self, 'SPY')
        return self._benchmark

    def set_brokerage_model(self, model, account_type=None):
        """A BrokerageName (its fee schedule is not applied: the run's commission rate is) or a
        brokerage model object (its can_submit_order / can_update_order / get_benchmark are used)."""
        from .extensions import DefaultBrokerageModel
        if isinstance(model, int) and not hasattr(model, 'can_submit_order'):
            name = getattr(model, 'name', str(model))
            obj = DefaultBrokerageModel(account_type if account_type is not None else 0)
            obj.brokerage_name = name
            self._brokerage_model = obj
            self.brokerage_model = obj
            return
        self._brokerage_model = model
        self.brokerage_model = model
        if account_type is not None:
            try:
                model.account_type = account_type
            except Exception:
                pass

    # ---- pre-trade checks the broker asks for (brokerage model, shortable quantity) ----
    def _brokerage_refusal(self, method, *args):
        from .extensions import DefaultBrokerageModel
        model = self.brokerage_model
        fn = getattr(model, method, None)
        if fn is None or getattr(type(model), method, None) is getattr(DefaultBrokerageModel, method, None):
            return None
        try:
            res = fn(*args, None)
        except TypeError:
            res = fn(*args)
        ok, msg = (res if isinstance(res, tuple) else (res, None))
        if ok:
            return None
        return str(msg) if msg is not None else 'refused by the brokerage model'

    def _shortable_refusal(self, symbol, quantity_after, order_id=None):
        sec = self.securities.get(str(symbol).upper())
        provider = getattr(sec, 'shortable_provider', None) if sec is not None else None
        if provider is None or quantity_after >= 0:
            return None
        try:
            allowed = provider.shortable_quantity(sec.symbol, self.time)
        except Exception as exc:
            # Fails open: the order is allowed without the short-availability check.
            logger.warning("Shortable check failed, order allowed symbol=%s error=%s", sec.symbol, type(exc).__name__)
            return None
        if allowed is None:
            return None
        if -quantity_after > float(allowed):
            return (f"Order exceeds shortable quantity {float(allowed):g} for Symbol {sec.symbol} "
                    f"requested {quantity_after:g})")
        return None

    def _open_quantity(self, ticker, exclude=None):
        return sum(t.quantity - t.quantity_filled for t in self.transactions.get_open_orders(ticker)
                   if t is not exclude)

    def _pre_submit_check(self, ticket):
        from .orders import OrderResponseErrorCode as EC
        sec = self.securities.get(str(ticket.symbol).upper())
        if sec is not None and sec.symbol.security_type in (SecurityType.FUTURE, SecurityType.OPTION,
                                                            SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION) \
                and sec.symbol.is_canonical():
            return (EC.NON_TRADABLE_SECURITY,
                    f"Order Error: {sec.symbol} is a canonical (continuous) symbol and cannot be traded; "
                    f"trade a contract (e.g. security.mapped)")
        if sec is not None and getattr(sec, 'is_delisted', False):
            return (EC.NON_TRADABLE_SECURITY, f"Order Error: {sec.symbol} is delisted (expired)")
        msg = self._brokerage_refusal('can_submit_order', sec, self.transactions.get_order_by_id(ticket.order_id) or ticket)
        if msg is not None:
            return (EC.BROKERAGE_MODEL_REFUSED_TO_SUBMIT_ORDER,
                    f"BrokerageModel declared unable to submit order: [{ticket.order_id}] {msg}")
        ticker = str(ticket.symbol).upper()
        held = self.portfolio[ticker].quantity if ticker in self.portfolio else 0.0
        if ticket.quantity < 0:
            m = self._shortable_refusal(ticket.symbol, held + self._open_quantity(ticker, ticket) + ticket.quantity)
            if m:
                return (EC.EXCEEDS_SHORTABLE_QUANTITY, m)
        return None

    def _pre_update_check(self, ticket, request):
        from .orders import OrderResponseErrorCode as EC
        sec = self.securities.get(str(ticket.symbol).upper())
        msg = self._brokerage_refusal('can_update_order', sec, self.transactions.get_order_by_id(ticket.order_id) or ticket,
                                      request)
        if msg is not None:
            return (EC.BROKERAGE_MODEL_REFUSED_TO_UPDATE_ORDER, f"OrderID: {ticket.order_id} {msg}")
        if request.quantity is not None and float(request.quantity) < 0:
            ticker = str(ticket.symbol).upper()
            held = self.portfolio[ticker].quantity if ticker in self.portfolio else 0.0
            m = self._shortable_refusal(ticket.symbol, held + self._open_quantity(ticker, ticket) + float(request.quantity))
            if m:
                return (EC.EXCEEDS_SHORTABLE_QUANTITY, m)
        return None

    def set_brokerage_message_handler(self, handler):
        pass

    def set_time_zone(self, tz):
        pass

    def set_warmup(self, period, resolution=None):
        """Warm up with `period` bars (int) or the bars inside a time span (timedelta).
        The engine runs on daily bars, so a bar count means daily bars."""
        if isinstance(period, timedelta):
            self._warmup_timedelta = period
            self._warmup_bars = 0
            self._warm_up_period = period.days
        else:
            try:
                self._warmup_bars = max(0, int(period))
            except (TypeError, ValueError):
                self._warmup_bars = 0
            self._warmup_timedelta = None
            self._warm_up_period = self._warmup_bars

    def set_warm_up(self, period, resolution=None):
        self.set_warmup(period, resolution)

    # ---- Security Subscription Methods ----

    def _add(self, symbol, resolution, leverage=None):
        """Register a security. Leverage defaults by security type (equity 2x margin,
        forex 50x, crypto 1x, ...); the broker caps it at the type's maximum. Adding a security that
        was removed brings it back."""
        old = self.securities.get(str(symbol).upper())
        if old is not None and getattr(old, '_removed', False):
            old._removed = False
            old.is_tradable = True
            return old
        if leverage in (None, 0):
            leverage = _DEFAULT_LEVERAGE.get(symbol.security_type, 1.0)
        sec = self.securities.add(symbol, resolution, leverage)
        mkt = str(getattr(symbol, 'market', '') or '').lower()
        if symbol.security_type in (SecurityType.EQUITY, SecurityType.INDEX) and mkt in ('india', 'nse', 'bse'):
            sec.quote_currency.symbol = 'INR'
        if symbol.security_type in (SecurityType.FOREX, SecurityType.CRYPTO, SecurityType.CRYPTO_FUTURE,
                                    SecurityType.CFD):
            from .cashbook import split_pair
            base, quote = split_pair(str(symbol), symbol.security_type)
            if base and quote:
                sec.quote_currency.symbol = quote
                sec.base_currency.symbol = base
                if symbol.security_type in (SecurityType.FOREX, SecurityType.CRYPTO):
                    self.portfolio.cash_book.ensure(base)
                    if self.portfolio.quote_currency(str(symbol).upper()) != self.portfolio.cash_book.account_currency:
                        self.portfolio.cash_book.ensure(quote)
        if self.security_initializer is not None:
            init = getattr(self.security_initializer, 'initialize', self.security_initializer)
            if callable(init):
                init(sec)
        return sec

    def add_equity(self, ticker, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.USA, fill_forward: bool = True,
                   leverage: float = None, extended_market_hours: bool = False,
                   data_normalization_mode=None, **kwargs):
        symbol = ticker if isinstance(ticker, Symbol) else Symbol(str(ticker).upper(), SecurityType.EQUITY, market)
        sec = self._add(symbol, resolution, leverage)
        self.subscription_manager.add_subscription(symbol, resolution,
                                                    data_normalization_mode=data_normalization_mode)
        return sec

    def add_forex(self, ticker, resolution: Resolution = Resolution.MINUTE,
                  market: str = Market.OANDA, fill_forward: bool = True, leverage: float = None, **kwargs):
        symbol = ticker if isinstance(ticker, Symbol) else Symbol(str(ticker).upper(), SecurityType.FOREX, market)
        return self._add(symbol, resolution, leverage)

    def add_crypto(self, ticker, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.BINANCE, fill_forward: bool = True, leverage: float = None, **kwargs):
        symbol = ticker if isinstance(ticker, Symbol) else Symbol(str(ticker).upper(), SecurityType.CRYPTO, market)
        return self._add(symbol, resolution, leverage)

    def add_future(self, ticker, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.CME, fill_forward: bool = True,
                   leverage: float = None, extended_market_hours: bool = False,
                   data_mapping_mode=None, data_normalization_mode=None,
                   contract_depth_offset: int = 0, **kwargs):
        """A continuous future ('/ES'): priced from Yahoo's continuous series, mapped to one contract
        (security.mapped) by data_mapping_mode / contract_depth_offset; set_filter adds a contract
        universe (slice.future_chains). See futures.py."""
        if isinstance(resolution, str) and not isinstance(market, str):
            resolution, market = Resolution.MINUTE, resolution
        symbol = ticker if isinstance(ticker, Symbol) else Symbol.create(str(ticker).upper(), SecurityType.FUTURE,
                                                                          market or Market.CME)
        sec = self._add(symbol, resolution, leverage)
        sec._data_mapping_mode = data_mapping_mode
        sec._contract_depth_offset = int(contract_depth_offset or 0)
        sec._data_normalization_mode = data_normalization_mode
        sec._extended_market_hours = bool(extended_market_hours)
        self.subscription_manager.add_subscription(symbol, resolution, data_normalization_mode=data_normalization_mode)
        return sec

    # ---- options (model-priced: see options.py / option_model.py) ----

    def _add_option_canonical(self, canonical, resolution, leverage=None):
        sec = self._add(canonical, resolution, leverage if leverage not in (None, 0) else 1.0)
        from .options import canonical_multiplier
        sec.symbol_properties.contract_multiplier = canonical_multiplier(canonical)
        u = canonical.underlying
        if u is not None and str(u).upper() not in self.securities and u.security_type != SecurityType.FUTURE:
            self._add(u, resolution, None)
        if u is not None and str(u).upper() in self.securities:
            sec.underlying = self.securities[str(u).upper()]
        mgr = getattr(self, '_option_manager', None)
        if mgr is not None and getattr(mgr, '_session', None) is not None:
            mgr.used = True
            mgr._prepare_security(mgr._session, sec)
        return sec

    def add_option(self, underlying, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.USA, fill_forward: bool = True,
                   leverage: float = 1.0, **kwargs):
        """The option chain of an equity (or index) underlying; the underlying is subscribed too."""
        if isinstance(resolution, str) and not isinstance(market, str):
            resolution, market = Resolution.MINUTE, resolution
        if isinstance(underlying, Symbol):
            if underlying.security_type in (SecurityType.OPTION, SecurityType.INDEX_OPTION) and underlying.is_canonical():
                return self._add_option_canonical(underlying, resolution, leverage)
            u = underlying
        else:
            key = str(underlying).upper()
            u = self.securities[key].symbol if key in self.securities else Symbol(key, SecurityType.EQUITY,
                                                                                    market or Market.USA)
        if u.security_type == SecurityType.INDEX:
            return self.add_index_option(u, None, resolution, market)
        target = kwargs.get('target_option')
        args = ([target] if target else []) + [market or Market.USA]
        return self._add_option_canonical(Symbol.create_canonical_option(u, *args), resolution, leverage)

    def add_index(self, ticker, resolution: Resolution = Resolution.MINUTE,
                  market: str = Market.USA, fill_forward: bool = True, **kwargs):
        symbol = ticker if isinstance(ticker, Symbol) else Symbol(str(ticker).upper(), SecurityType.INDEX, market)
        return self._add(symbol, resolution, None)

    def add_index_option(self, underlying, target_option=None, resolution: Resolution = Resolution.MINUTE,
                         market: str = Market.USA, fill_forward: bool = True, **kwargs):
        """add_index_option(index[, target_option e.g. "SPXW"][, resolution][, market])."""
        if not isinstance(target_option, str) and target_option is not None:
            target_option, resolution = None, target_option
        if isinstance(resolution, str) and resolution not in ('usa',):
            target_option, resolution = resolution, Resolution.MINUTE
        u = underlying if isinstance(underlying, Symbol) else Symbol(str(underlying).upper(), SecurityType.INDEX,
                                                                      market or Market.USA)
        if u.security_type != SecurityType.INDEX:
            u = Symbol(u.value, SecurityType.INDEX, market or Market.USA)
        if str(u).upper() not in self.securities:
            self.add_index(u, resolution if isinstance(resolution, Resolution) else Resolution.MINUTE, market)
        args = ([target_option] if target_option else []) + [market or Market.USA]
        return self._add_option_canonical(Symbol.create_canonical_option(u, *args), resolution)

    def _add_option_contract(self, contract, resolution=None, kind=SecurityType.OPTION):
        symbol = contract if isinstance(contract, Symbol) else getattr(contract, 'symbol', None)
        if not isinstance(symbol, Symbol):
            raise ValueError(f"An option contract Symbol is required, got {contract!r}")
        from .options import canonical_multiplier
        sec = self._add(symbol, resolution or Resolution.MINUTE, 1.0)
        sec.symbol_properties.contract_multiplier = canonical_multiplier(symbol)
        u = symbol.underlying
        if u is not None and str(u).upper() not in self.securities:
            if u.security_type == SecurityType.FUTURE:
                self.add_future_contract(u, resolution)
            else:
                self._add(u, resolution or Resolution.MINUTE, None)
        if u is not None:
            sec.underlying = self.securities.get(str(u).upper())
        mgr = getattr(self, '_option_manager', None)
        if mgr is None and getattr(self, '_session', None) is not None:
            mgr = self._ensure_option_manager()
        if mgr is not None and getattr(mgr, '_session', None) is not None:
            mgr.used = True
            mgr._prepare_security(mgr._session, sec)
        return sec

    def add_index_option_contract(self, contract, resolution=None, fill_forward=True):
        return self._add_option_contract(contract, resolution, SecurityType.INDEX_OPTION)

    def add_future_contract(self, contract, resolution=None, fill_forward=True,
                            leverage=None, extended_market_hours=False):
        """One futures contract (a Symbol from the chain or Symbol.create_future)."""
        symbol = getattr(contract, 'symbol', contract) if not isinstance(contract, Symbol) else contract
        if not isinstance(symbol, Symbol):
            raise ValueError(f"add_future_contract needs a contract Symbol, got {contract!r}")
        sec = self._add(symbol, resolution or Resolution.MINUTE, leverage)
        self._ensure_futures_manager().configure(sec)
        return sec

    def _ensure_futures_manager(self):
        """The run's FuturesManager; created on first use so that contracts added in initialize() are
        priced already there (option chains on a futures contract, history)."""
        mgr = getattr(self, '_futures_manager', None)
        if mgr is None:
            from .futures import FuturesManager
            mgr = self._futures_manager = FuturesManager()
            session = getattr(self, '_session', None)
            if session is not None:
                mgr.after_initialize(session)
                session.extensions.append(mgr)
            elif self._data is not None:
                from types import SimpleNamespace
                early = SimpleNamespace(data=self._data, algo=self)
                self._data.register_provider(mgr.is_contract, lambda sym, _s=early: mgr._contract_series(_s, sym))
        return mgr

    def add_future_option(self, symbol, option_filter=None):
        """Options on a future: on a contract Symbol, that contract's options; on a continuous
        future, the options of the contracts its chain selects (Black-76, see option_model.py)."""
        fut = symbol if isinstance(symbol, Symbol) else Symbol.create(str(symbol), SecurityType.FUTURE, Market.CME)
        canonical = Symbol.create_canonical_option(fut, getattr(fut, 'market', Market.CME))
        sec = self._add_option_canonical(canonical, Resolution.MINUTE)
        if option_filter is not None:
            sec.set_filter(option_filter)
        self._future_option_roots = getattr(self, '_future_option_roots', []) + [sec]
        return sec

    def add_future_option_contract(self, contract, resolution=None, fill_forward=True):
        return self._add_option_contract(contract, resolution, SecurityType.FUTURE_OPTION)

    def add_option_contract(self, contract, resolution=None, fill_forward=True, leverage=None, *args, **kwargs):
        return self._add_option_contract(contract, resolution, SecurityType.OPTION)

    def exercise_option(self, option_symbol, quantity, asynchronous=False, tag="", order_properties=None):
        """Exercise `quantity` of a long American option position now (settled at the current
        underlying price, like an expiry exercise)."""
        mgr = self._ensure_option_manager()
        mgr.exercise(mgr._session, option_symbol, quantity)
        return None

    def add_cfd(self, ticker, resolution: Resolution = Resolution.MINUTE,
                market: str = Market.OANDA, fill_forward: bool = True, leverage: float = None, **kwargs):
        symbol = ticker if isinstance(ticker, Symbol) else Symbol(str(ticker).upper(), SecurityType.CFD, market)
        return self._add(symbol, resolution, leverage)

    def add_crypto_future(self, ticker, resolution: Resolution = Resolution.MINUTE,
                          market: str = Market.BINANCE, fill_forward: bool = True, leverage: float = None, **kwargs):
        symbol = ticker if isinstance(ticker, Symbol) else Symbol(str(ticker).upper(), SecurityType.CRYPTO_FUTURE, market)
        return self._add(symbol, resolution, leverage)

    def add_security(self, security_type, ticker=None, resolution=Resolution.MINUTE,
                     market=None, fill_forward=True, leverage=None,
                     extended_market_hours=False, **kwargs):
        if isinstance(security_type, Symbol) and ticker is None:
            sym = security_type
            return self._add(sym, resolution, leverage)
        if ticker is None:
            # security_type might actually be a symbol/ticker string
            ticker = str(security_type)
            security_type = SecurityType.EQUITY
        if security_type == SecurityType.FUTURE:
            return self.add_future(ticker, resolution, market or Market.CME, fill_forward, leverage, extended_market_hours)
        if security_type in (SecurityType.OPTION, SecurityType.INDEX_OPTION):
            return self.add_option(ticker, resolution, market or Market.USA) if security_type == SecurityType.OPTION \
                else self.add_index_option(ticker, resolution, market or Market.USA)
        symbol = Symbol(str(ticker).upper(), security_type, market or Market.USA)
        return self._add(symbol, resolution, leverage)

    def add_data(self, data_type, ticker, *args, resolution=None, properties=None,
                 leverage=1.0, fill_forward=True, **kwargs):
        # args may contain: resolution, properties, exchange_hours in various positions
        sym_properties = properties
        exchange_hours = None
        res = resolution
        underlying_symbol = None
        for arg in args:
            if isinstance(arg, Resolution) or (isinstance(arg, int) and not isinstance(arg, bool)):
                if res is None:
                    res = arg
            elif hasattr(arg, 'lot_size'):
                sym_properties = arg
            elif hasattr(arg, 'is_open') or (hasattr(arg, 'always_open') and callable(getattr(arg, 'always_open', None))):
                exchange_hours = arg
        # When ticker is a Symbol object, preserve the underlying linkage
        if isinstance(ticker, Symbol):
            underlying_symbol = ticker
            symbol = Symbol(str(ticker).upper(), SecurityType.BASE, Market.USA)
            symbol._underlying = underlying_symbol
        else:
            # For linked data types, the string ticker must resolve from existing securities
            requires_mapping = getattr(data_type, 'requires_mapping_to_underlying', False)
            if requires_mapping and isinstance(ticker, str):
                # Look up ticker in the symbol cache (existing securities)
                found = ticker.upper() in self.securities
                if not found:
                    from .algorithm_imports import InvalidOperationException
                    raise InvalidOperationException(
                        f"'{ticker}' wasn't found in the SymbolCache. "
                        f"Linked data types require the underlying security to be added first."
                    )
                # Resolve to the existing equity symbol as underlying
                underlying_symbol = self.securities[ticker.upper()].symbol
            symbol = Symbol(ticker.upper(), SecurityType.BASE, Market.USA)
            if underlying_symbol:
                symbol._underlying = underlying_symbol
        sec = self.securities.add(symbol, res or Resolution.DAILY, leverage)
        # Read by custom_data.CustomDataManager with the type's own get_source / reader.
        if not isinstance(data_type, type) or not hasattr(data_type, 'reader'):
            self._custom_data.append(f"{getattr(data_type, '__name__', str(data_type))}:{symbol}")
        else:
            self._custom_subscriptions = getattr(self, '_custom_subscriptions', {})
            self._custom_subscriptions[str(symbol).upper()] = (data_type, sec)
        if sym_properties is not None:
            sec.symbol_properties = sym_properties
        if exchange_hours is not None:
            sec.exchange.hours = exchange_hours
        # Populate subscriptions with a data config so sec.subscriptions[0].type works
        config = _SubscriptionDataConfig(symbol, res or Resolution.DAILY)
        config.data_type = data_type
        config.type = data_type
        config.is_custom_data = True
        sec.subscriptions = [config]
        return sec

    def remove_security(self, symbol, tag=None):
        """Cancel the security's open orders, liquidate its position and remove it from the algorithm's
        universe (LEAN). The security object stays known (prices, history) until it is added again."""
        key = str(symbol).upper()
        sec = self.securities.get(key)
        if sec is None:
            return False
        if self._broker is not None:
            self._broker.cancel_open(key, 'Removed')
            h = self.portfolio[key] if key in self.portfolio else None
            if h is not None and h.quantity and not self._is_warming_up:
                self.market_order(sec.symbol, -h.quantity, tag=tag or 'Liquidated')
        sec.is_tradable = False
        sec._removed = True
        self._removed_securities = getattr(self, '_removed_securities', []) + [key]
        return True

    # ---- Data Methods ----

    def on_data(self, data: Slice):
        """Override to handle incoming data."""
        pass

    def on_securities_changed(self, changes):
        """Called when securities are added/removed."""
        pass

    def on_end_of_day(self, symbol=None):
        """Called at end of each trading day."""
        pass

    def on_end_of_algorithm(self):
        """Called when algorithm finishes."""
        pass

    def on_warmup_finished(self):
        """Called when warmup period completes."""
        pass

    def on_order_event(self, order_event: OrderEvent):
        """Called when order status changes."""
        pass

    def on_dividends_paid(self, dividends):
        pass

    def on_splits(self, splits):
        pass

    def on_delistings(self, delistings):
        pass

    def on_margin_call(self, requests):
        return requests

    def on_assignment_order_event(self, assignment_event):
        pass

    @property
    def history(self):
        """Get historical data. Returns a callable & subscriptable object."""
        return _HistoryAccessor(self)

    # Data types history() cannot serve, and why.
    _HISTORY_UNSUPPORTED = {
        'QuoteBar': ("quote (bid/ask) bar history", "Only daily OHLCV trade bars are available."),
        'Tick': ("tick data", "The engine runs on daily bars."),
        'OpenInterest': ("open-interest data", ""),
        'Dividend': ("dividend events", "Prices are split- and dividend-adjusted; corporate-action events are not delivered."),
        'Split': ("split events", "Prices are split- and dividend-adjusted; corporate-action events are not delivered."),
        'Delisting': ("delisting events", ""),
        'SymbolChangedEvent': ("symbol-changed events", ""),
        'MarginInterestRate': ("margin interest rate data", ""),
        'ETFConstituentData': ("ETF constituent data", ""),
    }

    def _history_symbols(self, arg):
        """Normalise the symbol argument of history() to a list of Symbol/str."""
        if arg is None:
            return []
        if isinstance(arg, (Symbol, str)):
            return [arg]
        if isinstance(arg, dict):
            return list(arg.keys())
        try:
            return [x.symbol if hasattr(x, 'symbol') and not isinstance(x, (Symbol, str)) else x for x in arg]
        except TypeError:
            return [arg]

    def _history_impl(self, *args, **kwargs):
        """history() served from the run's real daily bars (fincept_engine.data).

        Accepts the LEAN forms: history(symbol(s), periods[, resolution]),
        history(symbol(s), timedelta[, resolution]), history(symbol(s), start, end[, resolution]),
        history(TradeBar, symbol(s), ...), self.history[TradeBar](...). Only bars that are
        complete at the algorithm's current time are returned (no look-ahead). Fundamental,
        universe, tick, quote, corporate-action and custom data are refused with a clear
        error - they used to be invented.
        """
        typed = kwargs.pop('_typed_data_type', None)
        force_res = kwargs.pop('_force_resolution', None)
        kwargs.pop('flatten', None)
        args = list(args)

        data_type = None
        if args and isinstance(args[0], type):
            data_type = args.pop(0)
        elif args and (isinstance(args[0], _UniverseReference) or type(args[0]).__name__.endswith('Universe')
                       or hasattr(args[0], 'select_symbols')):
            raise not_supported("universe history (constituent or fundamental data)")
        if typed is not None:
            data_type = typed
        want_objects = typed is not None
        cdm = getattr(self, '_custom_manager', None)
        if data_type is not None and cdm is not None and isinstance(data_type, type) and hasattr(data_type, 'reader'):
            return self._custom_history(cdm, data_type, args, kwargs, want_objects=typed is not None)
        if data_type is not None:
            name = getattr(data_type, '__name__', str(data_type))
            if 'Fundamental' in name:
                raise not_supported("historical fundamental data (history of Fundamental objects)",
                                    "Fundamentals come from a current Yahoo Finance snapshot; there is no history of them.")
            if name in self._HISTORY_UNSUPPORTED:
                raise not_supported(*self._HISTORY_UNSUPPORTED[name])
            if name not in ('TradeBar', 'BaseData', 'IBaseDataBar', 'IBaseData'):
                raise not_supported(f"custom data history ({name})")
        if force_res is not None and int(force_res) == int(Resolution.TICK):
            raise not_supported("tick data", "The engine runs on daily bars.")
        if self._data is None:
            raise not_supported("history() outside a backtest run")

        # ---- arguments ----
        symbols = None
        count = period = start = end = resolution = None
        if 'symbol' in kwargs or 'symbols' in kwargs:
            symbols = self._history_symbols(kwargs.pop('symbol', None) or kwargs.pop('symbols', None))
        for key in ('periods', 'bar_count'):
            if key in kwargs:
                count = kwargs.pop(key)
        if 'start' in kwargs:
            start = kwargs.pop('start')
        if 'end' in kwargs:
            end = kwargs.pop('end')
        if 'resolution' in kwargs:
            resolution = kwargs.pop('resolution')
        depth = kwargs.pop('contract_depth_offset', None)
        mmode = kwargs.pop('data_mapping_mode', None)
        for a in args:
            if type(a).__name__ == 'DataMappingMode':
                mmode = a
        dnm = kwargs.pop('data_normalization_mode', None)
        for a in args:
            if type(a).__name__ == 'DataNormalizationMode':
                dnm = a
        if dnm is not None and int(dnm) not in (1, 3):  # ADJUSTED / TOTAL_RETURN serve adjusted bars
            want = 'raw' if int(dnm) == 0 else ('split_adjusted' if int(dnm) == 2 else None)
            syms0 = kwargs.get('symbol') or (args[0] if args and not isinstance(args[0], (int, timedelta, datetime)) else None)
            have = self._data.mode_of(syms0) if (self._data is not None and syms0 is not None and
                                                 not isinstance(syms0, (list, tuple))) else 'adjusted'
            if want is None or want != have:
                raise not_supported(f"history in {getattr(dnm, 'name', dnm)} price mode",
                                    "History is served in the price mode the security is subscribed with.")
        for a in args:
            if isinstance(a, bool) or a is None or type(a).__name__ in ('DataNormalizationMode', 'DataMappingMode'):
                continue
            if isinstance(a, Resolution):
                resolution = a
            elif isinstance(a, int):
                count = a
            elif isinstance(a, timedelta):
                period = a
            elif isinstance(a, (datetime, date)):
                d = a if isinstance(a, datetime) else datetime.combine(a, dt_time())
                if start is None:
                    start = d
                else:
                    end = d
            elif symbols is None and not isinstance(a, float):
                symbols = self._history_symbols(a)
        if symbols is None or len(symbols) == 0:
            symbols = [sec.symbol for sec in self.securities.values()]
        if resolution is not None and int(resolution) != int(Resolution.DAILY):
            note = "history() requests at intraday resolution were served from daily bars"
            if note not in self._engine_notes:
                self._engine_notes.append(note)

        now = self._time
        rows, objs = [], []
        expiries = {}                 # row number -> futures expiry (futures history has an expiry level)
        multi = len(symbols) > 1
        all_futures = all(getattr(s if isinstance(s, Symbol) else getattr(self.securities.get(str(s).upper()), 'symbol', None),
                                  'security_type', None) == SecurityType.FUTURE for s in symbols)
        for sym in symbols:
            key = str(sym).upper()
            sec = self.securities.get(key)
            sym_obj = sym if isinstance(sym, Symbol) else (sec.symbol if sec is not None else Symbol(key))
            st = getattr(sym_obj, 'security_type', SecurityType.EQUITY)
            if st in (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION):
                raise not_supported("option price history")
            mapped_of = None
            if st == SecurityType.FUTURE and sym_obj.is_canonical():
                mapped_of = self._continuous_mapping(sym_obj, sec, depth, mmode)
            if mapped_of is not None and (depth is not None or mmode is not None):
                series = self._continuous_series(sym_obj, mapped_of)
            else:
                series = self._data.series(sym_obj, st, getattr(sym_obj, 'market', None))
            if series is None:
                continue
            n_known = series.count_known_at(now)
            if count is not None:
                lo, hi = max(0, n_known - int(count)), n_known
            elif period is not None:
                cutoff = now - period
                lo = n_known
                while lo > 0 and series._ends[lo - 1] > cutoff:
                    lo -= 1
                hi = n_known
            elif start is not None:
                s0 = datetime(start.year, start.month, start.day)
                e0 = end if end is not None else now
                e0 = datetime(e0.year, e0.month, e0.day)
                lo = next((i for i, d in enumerate(series.dates) if d >= s0), len(series))
                hi = min(n_known, next((i for i, d in enumerate(series.dates) if d > e0), len(series)))
            else:
                lo, hi = max(0, n_known - 1), n_known
            for i in range(lo, max(lo, hi)):
                t_end = series._ends[i]
                if want_objects:
                    bar = TradeBar(time=series.dates[i], symbol=sym_obj, open=series.open[i],
                                   high=series.high[i], low=series.low[i], close=series.close[i],
                                   volume=series.volume[i])
                    bar.end_time = t_end
                    bar.period = timedelta(days=1)
                    objs.append((t_end, sym_obj, bar))
                else:
                    if st == SecurityType.FUTURE and all_futures:
                        if mapped_of is not None:
                            m = mapped_of(series.dates[i].date())
                            expiries[len(rows)] = m.id.date if m is not None else None
                        else:
                            expiries[len(rows)] = sym_obj.id.date
                    rows.append((key, t_end, series.open[i], series.high[i], series.low[i],
                                 series.close[i], series.volume[i]))

        if want_objects:
            if not multi:
                return [b for _, _, b in objs]
            by_time = {}
            for t, so, b in objs:
                by_time.setdefault(t, _DataDictionary())[so] = b
            return [by_time[t] for t in sorted(by_time)]
        if pd is None:
            raise not_supported("pandas history frames (pandas is not installed)")
        cols = ['open', 'high', 'low', 'close', 'volume']
        if rows:
            # Build the (symbol, time) index from levels + codes: MultiIndex.from_tuples
            # factorises every call and made history() ~10x slower.
            syms = list(dict.fromkeys(r[0] for r in rows))
            times = sorted({r[1] for r in rows})
            spos = {s: i for i, s in enumerate(syms)}
            tpos = {t: i for i, t in enumerate(times)}
            if expiries and len(expiries) == len(rows):
                # futures: LEAN's (expiry, symbol, time) index
                idx = pd.MultiIndex.from_arrays(
                    [pd.DatetimeIndex([expiries[k] for k in range(len(rows))]), [r[0] for r in rows],
                     pd.DatetimeIndex([r[1] for r in rows])], names=['expiry', 'symbol', 'time'])
            else:
                idx = pd.MultiIndex(levels=[syms, pd.DatetimeIndex(times)],
                                    codes=[[spos[r[0]] for r in rows], [tpos[r[1]] for r in rows]],
                                    names=['symbol', 'time'], verify_integrity=False)
            df = pd.DataFrame([r[2:] for r in rows], index=idx, columns=cols, dtype=float)
        else:
            df = pd.DataFrame(columns=cols,
                              index=pd.MultiIndex.from_tuples([], names=['symbol', 'time']))
        return _HistoryDataFrame(df)

    def _continuous_mapping(self, canonical, sec, depth=None, mode=None):
        """day -> the contract a continuous future maps to (its own mapping settings unless overridden)."""
        from types import SimpleNamespace
        mgr = self._ensure_futures_manager()
        ns = SimpleNamespace(
            symbol=canonical,
            _data_mapping_mode=mode if mode is not None else getattr(sec, '_data_mapping_mode', None),
            _contract_depth_offset=int(depth) if depth is not None else int(getattr(sec, '_contract_depth_offset', 0) or 0))
        cache = {}

        def mapped(day):
            if day not in cache:
                try:
                    cache[day] = mgr._mapped_for(ns, day)
                except Exception:
                    cache[day] = None
            return cache[day]
        return mapped

    def _continuous_series(self, canonical, mapped):
        """Daily bars of a continuous future built from the contract it maps to each day (derived
        contract prices, futures.py) - history(..., contract_depth_offset= / data_mapping_mode=)."""
        from .data import BarSeries
        base = self._data.series(canonical, SecurityType.FUTURE, getattr(canonical, 'market', None))
        if base is None:
            return None
        dates, o, h, l, c, v = [], [], [], [], [], []
        cache = {}
        for d in base.dates:
            m = mapped(d.date())
            if m is None:
                continue
            k = str(m)
            if k not in cache:
                cache[k] = self._data.series(m, SecurityType.FUTURE, getattr(m, 'market', None))
            s = cache[k]
            i = s.index_of_date(d) if s is not None else None
            if i is None:
                continue
            dates.append(d)
            o.append(s.open[i]); h.append(s.high[i]); l.append(s.low[i]); c.append(s.close[i]); v.append(s.volume[i])
        if not dates:
            return None
        return BarSeries.from_columns(str(canonical).upper(), f"{base.yahoo} (mapped contracts)", dates, o, h, l, c, v)

    def _custom_history(self, cdm, data_type, args, kwargs, want_objects):
        """history(CustomType, symbol(s), periods | timedelta | start, end): the custom points known now."""
        syms = kwargs.pop('symbol', None) or kwargs.pop('symbols', None)
        count = period = start = None
        for a in args:
            if isinstance(a, bool) or a is None:
                continue
            if isinstance(a, (Symbol, str)) and syms is None:
                syms = a
            elif isinstance(a, (list, tuple)) and syms is None:
                syms = list(a)
            elif isinstance(a, int):
                count = a
            elif isinstance(a, timedelta):
                period = a
            elif isinstance(a, (datetime, date)) and start is None:
                start = a if isinstance(a, datetime) else datetime.combine(a, dt_time())
        if syms is None:
            syms = list(cdm.feeds.keys())
        syms = syms if isinstance(syms, (list, tuple)) else [syms]
        now = self._time
        if period is not None:
            start = now - period
        out = []
        for s in syms:
            out.extend(cdm.history(str(s).upper(), now, count, start))
        if want_objects:
            return out
        import pandas as _pd
        rows, idx = [], []
        for p in out:
            d = dict(getattr(p, '_data', {}) or {})
            d['value'] = getattr(p, 'value', 0.0)
            rows.append({str(k).lower(): v for k, v in d.items()})
            idx.append((str(getattr(p, 'symbol', '')).upper(), getattr(p, 'end_time', None) or p.time))
        if not rows:
            return _HistoryDataFrame(_pd.DataFrame())
        mi = _pd.MultiIndex.from_tuples(idx, names=['symbol', 'time'])
        return _HistoryDataFrame(_pd.DataFrame(rows, index=mi))

    # ---- Order Methods ----
    # In a backtest every order goes to the broker (fincept_engine.broker): it is checked
    # against buying power and filled on a LATER bar (next open for market orders), with the
    # run's commission and slippage. Without a broker (no runner) orders are refused.

    def _next_order_id(self) -> int:
        self.transactions._order_counter += 1
        return self.transactions._order_counter

    def _make_ticket(self, symbol, quantity, order_type, tag="", limit_price=0, stop_price=0,
                     order_properties=None, trigger_price=0, trailing_amount=0, trailing_as_percentage=False,
                     request=None):
        from .orders import SubmitOrderRequest
        oid = self._next_order_id()
        if isinstance(symbol, Symbol):
            sym = symbol
        else:
            sec = self.securities.get(str(symbol).upper())
            sym = sec.symbol if sec is not None else Symbol(str(symbol).upper())
        ticket = OrderTicket(oid, sym, float(quantity), order_type,
                             limit_price=limit_price or 0, stop_price=stop_price or 0, tag=tag or "",
                             trigger_price=trigger_price or 0, trailing_amount=trailing_amount or 0,
                             trailing_as_percentage=trailing_as_percentage)
        props = order_properties or self.default_order_properties
        ticket.properties = props
        ticket.time_in_force = getattr(props, 'time_in_force', None)
        ticket.time = self._time
        if request is None:
            request = SubmitOrderRequest(order_type, sym.security_type, sym, float(quantity), stop_price or 0,
                                         limit_price or 0, trigger_price or 0, trailing_amount or 0,
                                         bool(trailing_as_percentage), self._utc_time, tag or "", props)
        request.order_id = oid
        ticket.submit_request = request
        self.transactions._orders[oid] = ticket
        return ticket

    def _submit(self, symbol, quantity, order_type, tag="", limit_price=0, stop_price=0,
                order_properties=None, trigger_price=0, trailing_amount=0, trailing_as_percentage=False,
                request=None):
        key = str(symbol).upper()
        if key not in self.securities._securities and key in self.securities:
            self.securities[key]          # a chain contract: subscribed on demand
        if key not in self.securities._securities:
            # LEAN raises for an order on a security the algorithm never added.
            raise Exception(f"This asset symbol ({str(symbol).upper()} 0) was not found in your security list. "
                            f"Please add this security or check it exists before using it with "
                            f"'Securities.ContainsKey(\"{str(symbol).upper()}\")'")
        ticket = self._make_ticket(symbol, quantity, order_type, tag, limit_price, stop_price,
                                   order_properties, trigger_price, trailing_amount, trailing_as_percentage,
                                   request)
        if self._broker is None:
            raise not_supported("order placement outside a backtest run")
        return self._broker.submit(ticket)

    def _forget_order(self, ticket):
        """A request refused by a pre-order check is not an order (LEAN): drop it from the book."""
        oid = ticket.order_id
        if self.transactions._orders.get(oid) is ticket:
            del self.transactions._orders[oid]
            if self.transactions._order_counter == oid:
                self.transactions._order_counter -= 1

    def _submit_request(self, request):
        """transactions.add_order(SubmitOrderRequest)."""
        sym = request.symbol
        key = str(sym).upper()
        if key in self.securities:
            sym = self.securities[key].symbol
        return self._submit(sym, request.quantity, request.order_type, request.tag, request.limit_price,
                            request.stop_price, request.order_properties, request.trigger_price,
                            request.trailing_amount, request.trailing_as_percentage, request)

    def market_order(self, symbol, quantity, asynchronous=False, tag="", order_properties=None, **kwargs):
        return self._submit(symbol, quantity, OrderType.MARKET, tag, order_properties=order_properties)

    def limit_order(self, symbol, quantity, limit_price, tag="", order_properties=None, **kwargs):
        return self._submit(symbol, quantity, OrderType.LIMIT, tag, limit_price=limit_price,
                            order_properties=order_properties)

    def stop_market_order(self, symbol, quantity, stop_price, tag="", order_properties=None, **kwargs):
        return self._submit(symbol, quantity, OrderType.STOP_MARKET, tag, stop_price=stop_price,
                            order_properties=order_properties)

    def stop_limit_order(self, symbol, quantity, stop_price, limit_price, tag="", order_properties=None, **kwargs):
        return self._submit(symbol, quantity, OrderType.STOP_LIMIT, tag, limit_price=limit_price,
                            stop_price=stop_price, order_properties=order_properties)

    def trailing_stop_order(self, symbol, quantity, *args, **kwargs):
        """LEAN: trailing_stop_order(symbol, quantity, trailing_amount, trailing_as_percentage[, tag, props])
        or trailing_stop_order(symbol, quantity, stop_price, trailing_amount, trailing_as_percentage[, ...])."""
        a = list(args)
        stop_price = kwargs.pop('stop_price', None)
        if len(a) >= 3 and isinstance(a[2], bool):
            stop_price = a.pop(0)
        trailing_amount = a.pop(0) if a else kwargs.pop('trailing_amount', 0)
        as_pct = a.pop(0) if a else kwargs.pop('trailing_as_percentage', False)
        tag = a.pop(0) if a else kwargs.pop('tag', "")
        props = a.pop(0) if a else kwargs.pop('order_properties', None)
        return self._submit(symbol, quantity, OrderType.TRAILING_STOP, tag, stop_price=stop_price or 0,
                            order_properties=props, trailing_amount=trailing_amount or 0,
                            trailing_as_percentage=bool(as_pct))

    def market_on_open_order(self, symbol, quantity, tag="", order_properties=None, **kwargs):
        return self._submit(symbol, quantity, OrderType.MARKET_ON_OPEN, tag, order_properties=order_properties)

    def market_on_close_order(self, symbol, quantity, tag="", order_properties=None, **kwargs):
        return self._submit(symbol, quantity, OrderType.MARKET_ON_CLOSE, tag, order_properties=order_properties)

    def limit_if_touched_order(self, symbol, quantity, trigger_price, limit_price, tag="", order_properties=None, **kwargs):
        return self._submit(symbol, quantity, OrderType.LIMIT_IF_TOUCHED, tag, limit_price=limit_price,
                            trigger_price=trigger_price, order_properties=order_properties)

    def _strategy_order(self, strategy, quantity, tag="", order_properties=None):
        """An OptionStrategy (option_strategies.OptionStrategies.*) as one combo market order."""
        from .option_strategies import strategy_to_legs
        legs = strategy_to_legs(strategy, 1)
        for leg in legs:
            if str(leg.symbol).upper() not in self.securities:
                if leg.symbol.security_type in (SecurityType.OPTION, SecurityType.INDEX_OPTION,
                                                SecurityType.FUTURE_OPTION):
                    self._add_option_contract(leg.symbol)
                else:
                    self._add(leg.symbol, Resolution.MINUTE, None)
        return self.combo_market_order(legs, quantity, tag=tag, order_properties=order_properties)

    def buy(self, symbol, quantity, *args, **kwargs):
        if hasattr(symbol, 'option_legs'):
            return self._strategy_order(symbol, abs(quantity), kwargs.get('tag', ''))
        return self.market_order(symbol, abs(quantity), *args, **kwargs)

    def sell(self, symbol, quantity, *args, **kwargs):
        if hasattr(symbol, 'option_legs'):
            return self._strategy_order(symbol, -abs(quantity), kwargs.get('tag', ''))
        return self.market_order(symbol, -abs(quantity), *args, **kwargs)

    def order(self, symbol, quantity, *args, **kwargs):
        if hasattr(symbol, 'option_legs'):
            return self._strategy_order(symbol, quantity, kwargs.get('tag', ''))
        return self.market_order(symbol, quantity, *args, **kwargs)

    def _unordered_quantity(self, ticker: str) -> float:
        """Holdings plus quantities of market orders not yet filled (LEAN: open orders count)."""
        held = self.portfolio[ticker].quantity if ticker in self.portfolio else 0.0
        pending = self._broker.pending_quantity(ticker) if self._broker is not None else 0.0
        return held + pending

    def calculate_order_quantity(self, symbol, target, *args, **kwargs) -> float:
        """Quantity to order so `symbol` becomes `target` (fraction of portfolio value).

        target value = (portfolio value - free buffer) * target, priced at the last close and
        grossed up by the run's commission + slippage; open market orders are counted; the
        result is rounded toward zero to the lot size (whole shares, 1e-8 crypto). A buying-power
        model the strategy set on the security (get_maximum_order_quantity_for_target_buying_power)
        decides instead."""
        ticker = str(symbol).upper()
        sec = self.securities.get(ticker)
        model = (getattr(sec, 'buying_power_model', None) or getattr(sec, 'margin_model', None)) if sec else None
        if model is not None:
            from .models import (BuyingPowerModel, is_overridden,
                                 GetMaximumOrderQuantityForTargetBuyingPowerParameters)
            if is_overridden(model, BuyingPowerModel, 'get_maximum_order_quantity_for_target_buying_power'):
                res = model.get_maximum_order_quantity_for_target_buying_power(
                    GetMaximumOrderQuantityForTargetBuyingPowerParameters(self.portfolio, sec, float(target)))
                return float(getattr(res, 'quantity', res) or 0.0)
        return self._engine_order_quantity(symbol, target)

    def _engine_order_quantity(self, symbol, target) -> float:
        ticker = str(symbol).upper()
        sec = self.securities.get(ticker)
        price = float(getattr(sec, 'price', 0.0) or 0.0) if sec is not None else 0.0
        if price <= 0:
            return 0.0
        tpv = self.portfolio.total_portfolio_value
        buffer = tpv * float(getattr(self.settings, 'free_portfolio_value_percentage', 0.0) or 0.0)
        target_value = (tpv - buffer) * float(target)
        cost = 1.0
        mult = 1.0
        if self._broker is not None:
            cost += self._broker.commission_rate + self._broker.slippage_rate
            mult = self._broker.multiplier(ticker)
            rate = self.portfolio._rate(self.portfolio.quote_currency(ticker))
            if rate and rate != 1.0:
                mult *= rate
        current = self._unordered_quantity(ticker)
        desired = target_value / (price * mult * cost)
        lot = lot_size(getattr(getattr(sec, 'symbol', None), 'security_type', None))
        delta = round_to_lot(desired - current, lot)
        return delta

    def set_holdings(self, symbol_or_targets=None, percentage=None, liquidate_existing_holdings=False,
                     tag="", order_properties=None, **kwargs):
        """Move `symbol` to `percentage` of portfolio value (or apply a list of PortfolioTargets).
        liquidate_existing_holdings closes every OTHER position first."""
        if symbol_or_targets is None:
            symbol_or_targets = kwargs.pop('symbol', None)
        if percentage is None:
            percentage = kwargs.pop('target', kwargs.pop('percent', None))
        liquidate_existing_holdings = (liquidate_existing_holdings or kwargs.pop('liquidate_existing', False)
                                       or kwargs.pop('liquidateExistingHoldings', False))
        if isinstance(symbol_or_targets, (list, tuple)):
            targets = [(str(t.symbol if hasattr(t, 'symbol') else t).upper(),
                        t.quantity if hasattr(t, 'quantity') else percentage) for t in symbol_or_targets]
            if liquidate_existing_holdings:
                keep = {s for s, _ in targets}
                for ticker, holding in list(self.portfolio.items()):
                    if holding.quantity != 0 and ticker not in keep:
                        self.liquidate(ticker, tag=tag)

            def _adds_exposure(item):
                ticker, pct = item
                if pct is None:
                    return 1
                cur = self._unordered_quantity(ticker)
                return 0 if abs(cur + self.calculate_order_quantity(ticker, pct)) < abs(cur) else 1
            # Like LEAN: orders that reduce a position go first, so they free margin for the rest.
            targets.sort(key=_adds_exposure)
            for ticker, pct in targets:
                if pct is not None:
                    self._set_holdings_single(ticker, pct, tag, order_properties)
            return
        if percentage is None:
            return
        ticker = str(symbol_or_targets).upper()
        if liquidate_existing_holdings:
            for other, holding in list(self.portfolio.items()):
                if holding.quantity != 0 and other != ticker:
                    self.liquidate(other, tag=tag)
        self._set_holdings_single(ticker, percentage, tag, order_properties)

    def _set_holdings_single(self, ticker: str, percentage: float, tag: str = "", order_properties=None):
        ticker = str(ticker).upper()
        quantity = self.calculate_order_quantity(ticker, percentage)
        if quantity != 0:
            self.market_order(ticker, quantity, tag=tag, order_properties=order_properties)

    def liquidate(self, symbol=None, tag="Liquidated", order_properties=None, **kwargs):
        """Cancel open orders and close the position(s) with market orders."""
        tickers = [str(symbol).upper()] if symbol is not None else \
            [t for t, h in list(self.portfolio.items()) if h.quantity != 0]
        if symbol is None and self._broker is not None:
            self._broker.cancel_open(None, tag)
        tickets = []
        for ticker in tickers:
            if self._broker is not None and symbol is not None:
                self._broker.cancel_open(ticker, tag)
            qty = self.portfolio[ticker].quantity if ticker in self.portfolio else 0
            if qty != 0:
                tickets.append(self.market_order(ticker, -qty, tag=tag, order_properties=order_properties))
        return tickets

    def can_liquidate(self, symbol):
        ticker = str(symbol).upper()
        return self.portfolio[ticker].quantity != 0

    # ---- Indicator Factory Methods ----
    # Every factory registers the indicator with the symbol(s) whose bars update it
    # (self._registered_indicators); the runner feeds each daily bar to the indicators
    # of its symbol before calling on_data, like LEAN's automatic indicator updates.

    def _register(self, indicator, symbols, selector=None, name=None):
        syms = [str(s).upper() for s in (symbols if isinstance(symbols, (list, tuple)) else [symbols])
                if s is not None]
        entry = {'indicator': indicator, 'symbols': syms, 'selector': selector}
        self._registered_indicators.append(entry)
        indicator._fincept_symbols = syms
        if isinstance(self._indicators, dict):
            self._indicators[name or getattr(indicator, 'name', str(id(indicator)))] = indicator
        return indicator

    @staticmethod
    def _sel(resolution, selector):
        """Factories accept (..., resolution, selector); a selector passed in the resolution
        slot (callable, not a Resolution) is taken as the selector."""
        if selector is None and callable(resolution) and not isinstance(resolution, (int, timedelta)):
            return resolution
        return selector

    def ema(self, symbol, period, smoothing_factor=None, resolution=None, selector=None, **kwargs):
        if callable(smoothing_factor) or isinstance(smoothing_factor, Resolution):
            smoothing_factor, resolution = None, smoothing_factor
        sf = smoothing_factor if isinstance(smoothing_factor, float) and 0 < smoothing_factor <= 1 else None
        ind = ExponentialMovingAverage(f"EMA({symbol},{period})", int(period), sf)
        return self._register(ind, symbol, self._sel(resolution, selector))

    def sma(self, symbol, period, resolution=None, selector=None, **kwargs):
        ind = SimpleMovingAverage(f"SMA({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def macd(self, symbol, fast_period=12, slow_period=26, signal_period=9,
             moving_average_type=None, resolution=None, selector=None, **kwargs):
        ind = MovingAverageConvergenceDivergence(f"MACD({symbol},{fast_period},{slow_period},{signal_period})",
                                                 int(fast_period), int(slow_period), int(signal_period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def rsi(self, symbol, period=14, moving_average_type=None, resolution=None, selector=None, **kwargs):
        if isinstance(moving_average_type, Resolution):
            moving_average_type, resolution = None, moving_average_type
        ind = RelativeStrengthIndex(f"RSI({symbol},{period})", int(period), moving_average_type)
        return self._register(ind, symbol, self._sel(resolution, selector))

    def b(self, target, reference, period=14, resolution=None, selector=None, **kwargs):
        """Beta of target vs reference over `period` daily returns."""
        ind = _BetaIndicator(f"B({target},{reference},{period})", target, reference, int(period))
        return self._register(ind, [target, reference], None)

    beta = b

    def bb(self, symbol, period=20, k=2.0, moving_average_type=None, resolution=None, selector=None, **kwargs):
        if isinstance(moving_average_type, Resolution):
            moving_average_type, resolution = None, moving_average_type
        ind = BollingerBands(f"BB({symbol},{period},{k})", int(period), float(k))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def atr(self, symbol, period=14, moving_average_type=None, resolution=None, selector=None, **kwargs):
        if isinstance(moving_average_type, Resolution):
            moving_average_type, resolution = None, moving_average_type
        ind = AverageTrueRange(f"ATR({symbol},{period})", int(period), moving_average_type)
        return self._register(ind, symbol, self._sel(resolution, selector))

    def sto(self, symbol, period=14, k_period=3, d_period=3, resolution=None, selector=None, **kwargs):
        ind = Stochastic(f"STO({symbol},{period})", int(period), int(k_period), int(d_period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def roc(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = RateOfChange(f"ROC({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def rocp(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = MomentumPercent(f"ROCP({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def mom(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = Momentum(f"MOM({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def momp(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = MomentumPercent(f"MOMP({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def wilr(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = WilliamsPercentR(f"WILR({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def cci(self, symbol, period=20, moving_average_type=None, resolution=None, selector=None, **kwargs):
        ind = CommodityChannelIndex(f"CCI({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def adx(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = AverageDirectionalIndex(f"ADX({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def identity(self, symbol, resolution=None, selector=None, name=None, **kwargs):
        """Identity indicator (the input value: close, or the selector's value)."""
        if isinstance(selector, str) and name is None:
            selector, name = None, selector
        ind = Identity(name or f"ID({symbol})")
        return self._register(ind, symbol, self._sel(resolution, selector), name=name)

    def filtered_identity(self, symbol, filter_func=None, resolution=None, selector=None, name=None, **kwargs):
        filt = filter_func if filter_func is not None else kwargs.get('filter')
        ind = FilteredIdentity(name or f"FID({symbol})", filt)
        return self._register(ind, symbol, self._sel(resolution, selector), name=name)

    def rc(self, symbol, period=20, k=2.0, resolution=None, selector=None, **kwargs):
        """Regression Channel: least-squares line +/- k standard deviations."""
        if isinstance(k, Resolution):
            k, resolution = 2.0, k
        ind = RegressionChannel(f"RC({symbol},{period},{k})", int(period), float(k))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def std(self, symbol, period=20, resolution=None, selector=None, **kwargs):
        ind = StandardDeviation(f"STD({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def min(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = Minimum(f"MIN({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def max(self, symbol, period=14, resolution=None, selector=None, **kwargs):
        ind = Maximum(f"MAX({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def aroon(self, symbol, up_period=25, down_period=None, resolution=None, selector=None, **kwargs):
        """Aroon oscillator (aroon_up / aroon_down sub-values), LEAN AROON(symbol, period[, downPeriod])."""
        if isinstance(down_period, Resolution):
            down_period, resolution = None, down_period
        dp = int(down_period) if down_period else int(up_period)
        ind = AroonOscillator(f"AROON({symbol},{up_period},{dp})", int(up_period), dp)
        return self._register(ind, symbol, self._sel(resolution, selector))

    def vwap(self, symbol, period=None, resolution=None, selector=None, **kwargs):
        """vwap(symbol, period): rolling VWAP over `period` bars; vwap(symbol): session VWAP."""
        if isinstance(period, Resolution) or period is None:
            if isinstance(period, Resolution):
                resolution = period
            ind = IntradayVwap(f"VWAP({symbol})")
        else:
            ind = VolumeWeightedAveragePrice(f"VWAP({symbol},{period})", int(period))
        return self._register(ind, symbol, self._sel(resolution, selector))

    def trin(self, *args, **kwargs):
        raise not_supported("the TRIN (Arms) index", "It needs market-wide advance/decline volume data.")

    def arima(self, *args, **kwargs):
        raise not_supported("the ARIMA indicator")

    def _greek(self, kind, symbol, *args, **kwargs):
        """Option greek / implied-volatility indicator of a contract, updated at each close from the
        option model (the same quotes the contract trades at)."""
        from .indicators import Identity
        sym = symbol if isinstance(symbol, Symbol) else getattr(symbol, 'symbol', None)
        if sym is None or sym.security_type not in (SecurityType.OPTION, SecurityType.INDEX_OPTION,
                                                    SecurityType.FUTURE_OPTION):
            raise ValueError(f"{kind} indicator needs an option contract Symbol, got {symbol!r}")
        ind = _OptionGreekIndicator(f"{kind.upper()}({sym})", kind)
        if str(sym).upper() not in self.securities:
            self._add_option_contract(sym)
        mgr = getattr(self, '_option_manager', None)
        if mgr is not None:
            mgr.register_indicator(ind, sym, kind)
        else:
            self._pending_option_indicators = getattr(self, '_pending_option_indicators', []) + [(ind, sym, kind)]
        return ind

    def iv(self, symbol, *args, **kwargs):
        return self._greek('iv', symbol, *args, **kwargs)

    def d(self, symbol, *args, **kwargs):
        return self._greek('delta', symbol, *args, **kwargs)

    def g(self, symbol, *args, **kwargs):
        return self._greek('gamma', symbol, *args, **kwargs)

    def r(self, symbol, *args, **kwargs):
        return self._greek('rho', symbol, *args, **kwargs)

    def v(self, symbol, *args, **kwargs):
        return self._greek('vega', symbol, *args, **kwargs)

    def t(self, symbol, *args, **kwargs):
        return self._greek('theta', symbol, *args, **kwargs)

    def register_indicator(self, symbol, indicator, resolution_or_consolidator=None, selector=None):
        """Update `indicator` with `symbol`'s bars - or with the bars of a consolidator /
        a time period, when one is given."""
        roc = resolution_or_consolidator
        if roc is not None and hasattr(roc, 'data_consolidated') and hasattr(roc, 'update'):
            self._wire_consolidated(symbol, roc, indicator, selector, register_consolidator=True)
            return indicator
        if isinstance(roc, timedelta) and roc > timedelta(days=1):
            cons = TradeBarConsolidator(roc)
            self._wire_consolidated(symbol, cons, indicator, selector, register_consolidator=True)
            return indicator
        if selector is None and callable(roc) and not isinstance(roc, (int, timedelta)):
            selector = roc
        # re-registering the same indicator replaces its feed
        self._registered_indicators = [e for e in self._registered_indicators if e['indicator'] is not indicator]
        return self._register(indicator, symbol, selector)

    def _wire_consolidated(self, symbol, consolidator, indicator, selector, register_consolidator):
        def _on(sender, bar):
            feed_indicator(indicator, selector(bar) if selector else bar)
        consolidator.data_consolidated += _on
        if register_consolidator and not any(c['consolidator'] is consolidator for c in self._consolidators):
            self._consolidators.append({'symbol': str(symbol).upper(), 'consolidator': consolidator})

    def unregister_indicator(self, indicator):
        self._registered_indicators = [e for e in self._registered_indicators if e['indicator'] is not indicator]
        name = getattr(indicator, 'name', str(id(indicator)))
        if isinstance(self._indicators, dict):
            self._indicators.pop(name, None)

    def indicator_history(self, indicator, symbol_or_symbols, period_or_start, *args,
                          resolution=None, selector=None):
        """LEAN indicator_history: updates `indicator` with the bars known now (see indicator_history.py)
        and returns the values it produced."""
        from .indicator_history import indicator_history
        return indicator_history(self, indicator, symbol_or_symbols, period_or_start, *args,
                                 resolution=resolution, selector=selector)

    def warm_up_indicator(self, symbol, indicator, resolution=None, selector=None, **kwargs):
        """Feed `indicator` the real bars before now (as many as its warm-up period needs)."""
        period = int(getattr(indicator, 'warm_up_period', 0) or 0)
        if period <= 0:
            return indicator  # LEAN skips indicators that do not declare a warm-up period
        bars = self.history[TradeBar](symbol if not isinstance(symbol, (list, tuple)) else symbol[0], period)
        sel = self._sel(resolution, selector)
        for bar in bars:
            feed_indicator(indicator, sel(bar) if sel else bar)
        return indicator

    def resolve_consolidator(self, symbol, resolution, data_type=None):
        """A consolidator for `resolution` (Resolution or timedelta)."""
        return TradeBarConsolidator(resolution if resolution is not None else timedelta(days=1))

    def create_indicator_name(self, symbol, indicator_type, resolution):
        """Create a standard indicator name."""
        return f"{indicator_type}({symbol},{resolution})"

    # ---- Data Consolidation ----

    def consolidate(self, symbol, period, *args, **kwargs):
        """LEAN consolidate(symbol, period, [tick_type,] handler): `handler(bar)` receives each
        consolidated bar. period: timedelta, Resolution, bar count or Calendar.* function."""
        handler = kwargs.get('handler')
        for a in args:
            if callable(a) and not isinstance(a, (int, timedelta)):
                handler = a
        if isinstance(period, Resolution) and period == Resolution.TICK:
            raise not_supported("tick data", "The engine runs on daily bars.")
        consolidator = TradeBarConsolidator(period)
        if handler is not None:
            consolidator.data_consolidated += (lambda sender, bar, _h=handler: _h(bar))
        self._consolidators.append({'symbol': str(symbol).upper(), 'consolidator': consolidator})
        return consolidator

    # ---- Universe Methods ----
    # Universes (manual lists, coarse/fine/fundamental selectors, ETF constituents, scheduled and
    # custom selectors, framework universe models) run in universe_selection.UniverseManager over a
    # fixed, disclosed pool of US stocks and ETFs (universe_data.py); option/future universe models
    # subscribe the chains their selector returns.

    # ---- universe plumbing (callbacks of the UniverseManager) ----
    def _universe_subscribe(self, symbol, settings):
        """Add a security a universe selected; None when no data source can price it."""
        from .data import TRADABLE_TYPES
        key = str(symbol).upper()
        res = getattr(settings, 'resolution', None) or Resolution.DAILY
        lev = getattr(settings, 'leverage', None)
        lev = lev if isinstance(lev, (int, float)) and lev > 0 else None
        existed = key in self.securities._securities
        sec = self._add(symbol if isinstance(symbol, Symbol) else Symbol(key), res, lev)
        mode = getattr(settings, 'data_normalization_mode', None)
        if mode is not None and int(mode) != 1 and not existed:
            sec.set_data_normalization_mode(mode)
        if self._data is not None and sec.symbol.security_type in TRADABLE_TYPES and not existed:
            session = getattr(self, '_session', None)
            if session is not None:
                session.apply_price_mode(sec)
            if self._data.series(sec.symbol, sec.symbol.security_type, getattr(sec.symbol, 'market', None)) is None:
                self.securities.remove(key)
                return None
        return sec

    def _universe_held(self, symbol):
        key = str(symbol).upper()
        h = self.portfolio._holdings.get(key)
        if h is not None and h.quantity:
            return True
        return bool(self.transactions.get_open_orders(key))

    def add_universe(self, *args, **kwargs):
        """LEAN add_universe overloads (fundamental / coarse+fine functions, ETF constituents,
        Universe objects, user-defined selectors, framework models); see universe_selection.py."""
        if args and isinstance(args[0], UniverseSelectionModel):
            self.add_universe_selection(args[0])
            return args[0]
        return self._universe_manager.add_universe(*args, **kwargs)

    def add_universe_options(self, universe, option_filter=None, *args, **kwargs):
        """Options on the members of a universe (LEAN AddUniverseOptions): each selected equity gets its
        option chain with this filter."""
        self._universe_option_filters = getattr(self, '_universe_option_filters', []) + [(universe, option_filter)]
        return universe

    def _derivative_universe_model(self, model):
        """OptionUniverseSelectionModel / FutureUniverseSelectionModel: the canonical symbols the model's
        selector returns are subscribed (option chains / futures with the model's filter); the selector
        runs again every refresh_interval (universe_ext.UniverseExtension)."""
        if model not in getattr(self, '_derivative_models', []):
            self._derivative_models = getattr(self, '_derivative_models', []) + [model]
        if hasattr(model, 'select_chain_symbols'):
            syms = model.select_chain_symbols(self.utc_time)
        else:
            sel = getattr(model, '_option_chain_symbol_selector', None) or \
                getattr(model, '_future_chain_symbol_selector', None)
            if sel is None:
                raise not_supported(f"universe selection ({type(model).__name__})")
            try:
                syms = list(sel(self.utc_time) or [])
            except TypeError:
                syms = list(sel() or [])
        flt = getattr(model, 'filter', None)
        done = getattr(self, '_derivative_subscribed', set())
        self._derivative_subscribed = done
        for s in syms:
            if str(s).upper() in done:
                continue
            done.add(str(s).upper())
            sym = s if isinstance(s, Symbol) else Symbol(str(s))
            if sym.security_type == SecurityType.FUTURE:
                sec = self.add_future(sym.id.symbol, self.universe_settings.resolution, sym.market)
                if callable(flt):
                    sec.set_filter(lambda u, _m=model: _m.filter(u))
            elif sym.security_type in (SecurityType.OPTION, SecurityType.INDEX_OPTION):
                sec = self._add_option_canonical(sym if sym.is_canonical() else sym.canonical,
                                                 self.universe_settings.resolution)
                if callable(flt):
                    sec.set_filter(lambda u, _m=model: _m.filter(u))
        return model

    def set_universe_selection(self, model):
        self._universe_selection = model
        from .framework.selection import OptionUniverseSelectionModel, FutureUniverseSelectionModel
        if isinstance(model, (OptionUniverseSelectionModel, FutureUniverseSelectionModel)):
            return self._derivative_universe_model(model)
        self._universe_manager.set_universe_selection(model)

    def add_universe_selection(self, model, _replace=False):
        if not _replace:
            self._universe_selection = model
        from .framework.selection import OptionUniverseSelectionModel, FutureUniverseSelectionModel
        if isinstance(model, (OptionUniverseSelectionModel, FutureUniverseSelectionModel)) or \
                hasattr(model, '_future_chain_symbol_selector') or hasattr(model, '_option_chain_symbol_selector'):
            return self._derivative_universe_model(model)
        self._universe_manager.add_universe_selection(model)

    # ---- Framework Model Methods ----
    # The runner drives alpha -> portfolio construction -> risk -> execution once per bar.

    def set_alpha(self, model):
        self._alpha_model = model

    def add_alpha(self, model):
        if self._alpha_model is None:
            self._alpha_model = model
        elif isinstance(self._alpha_model, CompositeAlphaModel):
            self._alpha_model._models.append(model)
        else:
            self._alpha_model = CompositeAlphaModel(self._alpha_model, model)

    def set_portfolio_construction(self, model):
        self._portfolio_construction = model

    def set_execution(self, model):
        self._execution_model = model

    def set_risk_management(self, model):
        self._risk_management = model

    def add_risk_management(self, model):
        if self._risk_management is None:
            self._risk_management = model
        elif isinstance(self._risk_management, CompositeRiskManagementModel):
            self._risk_management._models.append(model)
        else:
            self._risk_management = CompositeRiskManagementModel(self._risk_management, model)

    def emit_insights(self, *insights):
        """Insights emitted by the algorithm itself; processed with the alpha model's."""
        for i in insights:
            if isinstance(i, (list, tuple)):
                self._pending_insights.extend(i)
            elif i is not None:
                self._pending_insights.append(i)

    # ---- Charting & Logging ----

    def plot(self, chart_name: str, series_name: str = None,
             value: float = None, *args):
        if chart_name not in self._charts:
            self._charts[chart_name] = {}
        if series_name and value is not None:
            if series_name not in self._charts[chart_name]:
                self._charts[chart_name][series_name] = []
            self._charts[chart_name][series_name].append({
                'time': self._time.isoformat(),
                'value': value
            })

    def add_chart(self, chart):
        name = chart.name if hasattr(chart, 'name') else str(chart)
        self._charts[name] = {}

    def log(self, message: str):
        entry = f"[{self._time}] {message}"
        self._logs.append(entry)
        logger.info(entry)

    def debug(self, message: str):
        self._debug_msgs.append(f"[{self._time}] {message}")
        logger.debug(message)

    def error(self, message: str):
        logger.error(f"[{self._time}] {message}")

    def quit(self, message: str = ""):
        """Stop the algorithm: the runner ends the backtest after the current step."""
        logger.info(f"Algorithm quit: {message}")
        self._quit_requested = True
        self._quit_message = message

    def set_runtime_statistic(self, name: str, value):
        self._runtime_statistics[name] = str(value)

    def set_summary_statistic(self, name: str, value):
        self._runtime_statistics[name] = str(value)
        self.statistics._custom[str(name)] = str(value)

    # ---- Parameter Methods ----

    def get_parameter(self, name: str, default_value=None):
        val = self._parameters.get(name, None)
        if val is None:
            return default_value
        # Coerce to the type of default_value if provided
        if default_value is not None:
            try:
                if isinstance(default_value, int) and not isinstance(default_value, bool):
                    try:
                        return int(val)
                    except (ValueError, TypeError):
                        f = float(val)          # '100.0' from an optimiser grid
                        if f.is_integer():
                            return int(f)
                        raise
                elif isinstance(default_value, float):
                    return float(val)
            except (ValueError, TypeError):
                pass
        return val

    def set_parameters(self, params: dict):
        self._parameters.update(params)

    # ---- Security Initializer ----

    def set_security_initializer(self, initializer):
        self.security_initializer = initializer

    # ---- Utility Methods ----

    def symbol(self, ticker: str) -> Symbol:
        return Symbol(ticker.upper())

    def download(self, url: str, headers=None) -> str:
        try:
            import urllib.request
            req = urllib.request.Request(url)
            if headers:
                for k, v in headers.items():
                    req.add_header(k, v)
            with urllib.request.urlopen(req) as resp:
                return resp.read().decode('utf-8')
        except Exception as e:
            self.error(f"Download failed: {e}")
            return ""

    def train(self, *args):
        """train(func) runs func now; train(date_rule, time_rule, func) schedules it."""
        func = args[-1] if args else None
        if len(args) >= 3 and callable(func):
            return self.schedule.on(args[0], args[1], func)
        if callable(func):
            func()

    def add_command(self, command_type, handler=None):
        """Register a command handler. Validates the command type has a 'run' method."""
        if isinstance(command_type, type) and not hasattr(command_type, 'run'):
            raise ValueError(f"Command type '{command_type.__name__}' does not have a 'run' method")

    def link(self, command):
        """Generate a command link URL string for a command object."""
        import urllib.parse

        def _encode_value(key, val, parts):
            if isinstance(val, dict):
                for k, v in val.items():
                    parts.append(f"command[{key}][{k}]={urllib.parse.quote_plus(str(v))}")
            elif isinstance(val, (list, tuple)):
                for i, v in enumerate(val):
                    parts.append(f"command[{key}][{i}]={urllib.parse.quote_plus(str(v))}")
            elif val is None:
                pass  # skip None values
            else:
                parts.append(f"command[{key}]={urllib.parse.quote_plus(str(val))}")

        parts = []

        if isinstance(command, dict):
            for attr, val in command.items():
                _encode_value(attr, val, parts)
            return "&" + "&".join(parts)

        # Object command — collect all public non-callable attrs
        # Sort alphabetically to match LEAN's serialization order
        type_name = command.__class__.__name__
        skip_attrs = {'name'}
        attrs_map = {}

        # Collect from class body
        for attr in command.__class__.__dict__:
            if attr.startswith('_') or attr in skip_attrs:
                continue
            val = getattr(command, attr)
            if callable(val):
                continue
            attrs_map[attr] = val

        # Collect from instance
        for attr in vars(command):
            if attr.startswith('_') or attr in skip_attrs:
                continue
            val = getattr(command, attr)
            if callable(val):
                continue
            attrs_map[attr] = val

        # Sort alphabetically
        for attr in sorted(attrs_map.keys()):
            _encode_value(attr, attrs_map[attr], parts)
        parts.append(f"command[$type]={type_name}")
        return "&" + "&".join(parts)

    def fundamentals(self, symbol=None):
        """Fundamental objects as of now (Yahoo snapshot; see universe_data.py and its disclosure)."""
        return self._universe_manager.fundamentals(symbol)

    def _identifier(self, what):
        raise not_supported(f"security identifier lookups ({what})")

    def cusip(self, symbol):
        self._identifier("CUSIP")

    def composite_figi(self, symbol):
        self._identifier("FIGI")

    def sedol(self, symbol):
        self._identifier("SEDOL")

    def isin(self, symbol):
        self._identifier("ISIN")

    def cik(self, symbol):
        self._identifier("CIK")

    def _ensure_option_manager(self):
        mgr = getattr(self, '_option_manager', None)
        if mgr is None:
            session = getattr(self, '_session', None)
            from .options import OptionManager
            mgr = OptionManager()
            if session is None:
                if self._data is None:
                    raise not_supported("option chains outside a backtest run")
                mgr.attach_early(self)       # joins the run's session when it starts
            else:
                mgr.after_initialize(session)
                session.extensions.append(mgr)
        return mgr

    def option_chain(self, symbol, flatten=False):
        """The full option chain (every listed contract, no filter) of an underlying or canonical option
        at the current time, model-priced (see option_model.py)."""
        mgr = self._ensure_option_manager()
        canonical = self._canonical_option_for(symbol)
        return mgr.full_chain(canonical, self.time)

    def _canonical_option_for(self, symbol):
        sym = symbol if isinstance(symbol, Symbol) else (self.securities[str(symbol).upper()].symbol
                                                         if str(symbol).upper() in self.securities else
                                                         Symbol(str(symbol).upper()))
        if sym.security_type in (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION):
            return sym.canonical
        if sym.security_type == SecurityType.FUTURE:
            return Symbol.create_canonical_option(sym, sym.market)
        if sym.security_type == SecurityType.INDEX:
            return Symbol.create_canonical_option(sym, sym.market)
        return Symbol.create_canonical_option(sym, Market.USA)

    def option_chains_for(self, symbols, flatten=False):
        from .option_chain import OptionChains
        out = OptionChains(self.time)
        for s in (symbols if isinstance(symbols, (list, tuple, set)) else [symbols]):
            ch = self.option_chain(s)
            out[ch.symbol] = ch
        return out

    def futures_chain(self, symbol, flatten=False):
        """Listed contracts of a future at the current time (prices known now)."""
        from .futures import contract_expiries, contract_symbol, FuturesChain, FuturesContract
        root = getattr(getattr(symbol, 'id', None), 'symbol', None) or str(symbol).lstrip('/')
        canonical = Symbol.create(root, SecurityType.FUTURE, Market.CME)
        contracts = []
        for e in contract_expiries(root, self.time.date()):
            cs = contract_symbol(root, e)
            bar = None
            if self._data is not None:
                s = self._data.series(cs, SecurityType.FUTURE, 'cme')
                if s is not None:
                    n = s.count_known_at(self.time)
                    bar = {'close': s.close[n - 1]} if n > 0 else None
            contracts.append(FuturesContract(cs, canonical, datetime(e.year, e.month, e.day), bar, self.time))
        return FuturesChain(canonical, self.time, contracts)

    future_chain = futures_chain

    @property
    def option_chain_provider(self):
        return OptionChainProvider(self)

    def is_market_open(self, symbol=None):
        """Whether the security's exchange trades now (on_data runs at the session close, which counts
        as open; see market_hours.py)."""
        if symbol is not None and str(symbol).upper() in self.securities:
            return self.securities[str(symbol).upper()].exchange.exchange_open
        return True

    def plot_indicator(self, chart_name, wait_for_ready=True, *indicators):
        """Plot indicator values on a chart."""
        if not isinstance(wait_for_ready, bool):
            indicators = (wait_for_ready,) + tuple(indicators)
            wait_for_ready = True
        for indicator in indicators:
            if wait_for_ready and not getattr(indicator, 'is_ready', True):
                continue
            name = getattr(indicator, 'name', str(indicator))
            value = float(indicator) if hasattr(indicator, '__float__') else 0
            self.plot(chart_name, name, value)

    # ---- Combo Order Methods ----
    # All legs fill together on the same bar or not at all; buying power is checked for the legs as a
    # group (a spread is margined as a spread).

    def _combo(self, legs, quantity, order_type, tag="", order_properties=None, limit_price=None):
        if self._broker is None:
            raise not_supported("order placement outside a backtest run")
        group = {'type': order_type, 'quantity': float(quantity), 'limit_price': limit_price, 'legs': []}
        tickets = []
        for leg in legs:
            sym = getattr(leg, 'symbol', leg)
            key = str(sym).upper()
            if key not in self.securities:
                raise Exception(f"This asset symbol ({key} 0) was not found in your security list. Please add "
                                f"this security or check it exists before using it with "
                                f"'Securities.ContainsKey(\"{key}\")'")
            q = float(getattr(leg, 'quantity', 1)) * float(quantity)
            leg_limit = getattr(leg, 'order_price', None) if order_type == OrderType.COMBO_LEG_LIMIT else None
            sec_leg = self.securities[key]
            if getattr(self, '_session', None) is not None and sec_leg.symbol.security_type in (
                    SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION):
                self._ensure_option_manager()
            tk = self._make_ticket(self.securities[key].symbol, q, order_type, tag,
                                   limit_price=leg_limit if leg_limit is not None else (limit_price or 0),
                                   order_properties=order_properties)
            tk._combo = group
            group['legs'].append(tk)
            tickets.append(tk)
        self._broker.submit_combo(tickets)
        return tickets

    def combo_market_order(self, legs, quantity, asynchronous=False, tag="", order_properties=None):
        return self._combo(legs, quantity, OrderType.COMBO_MARKET, tag, order_properties)

    def combo_limit_order(self, legs, quantity, limit_price, tag="", order_properties=None):
        return self._combo(legs, quantity, OrderType.COMBO_LIMIT, tag, order_properties, limit_price)

    def combo_leg_limit_order(self, legs, quantity, tag="", order_properties=None):
        return self._combo(legs, quantity, OrderType.COMBO_LEG_LIMIT, tag, order_properties)

    # ---- Additional Utility Methods ----

    def get_last_known_price(self, security_or_symbol):
        """The last bar of a security known now (its last published bar, else history)."""
        sec = security_or_symbol if hasattr(security_or_symbol, 'get_last_data') else \
            self.securities.get(str(security_or_symbol).upper())
        last = sec.get_last_data() if sec is not None else None
        if last is None:
            got = self.get_last_known_prices(security_or_symbol)
            last = got[-1] if got else None
        return last

    def get_last_known_prices(self, security_or_symbol=None):
        """[the last daily bar complete at the current time] of a security (empty when none)."""
        if security_or_symbol is None:
            return []
        sym = getattr(security_or_symbol, 'symbol', security_or_symbol)
        if isinstance(sym, Symbol) and sym.security_type in (SecurityType.BASE,):
            return []
        try:
            bars = self.history[TradeBar](sym, 1)
        except Exception as exc:
            logger.warning("History request failed symbol=%s error=%s", sym, type(exc).__name__)
            return []
        return list(bars)[-1:] if bars else []

    @property
    def active_securities(self):
        """LEAN ActiveSecurities: `.values` works with and without a call."""
        return _ActiveSecurities(self.securities)

    @property
    def universe(self):
        """LEAN's universe definitions (self.universe.etf(...), .qc_500, .top(n), .dollar_volume.top(n))."""
        if self._universe_definitions is None:
            from .universe import UniverseDefinitions
            self._universe_definitions = UniverseDefinitions(self)
        return self._universe_definitions

    @property
    def universe_manager(self):
        return self._universe_manager.view()

    # ---- PascalCase Aliases for LEAN/C# Compatibility ----

    def SetStartDate(self, *args, **kwargs):
        return self.set_start_date(*args, **kwargs)

    def SetEndDate(self, *args, **kwargs):
        return self.set_end_date(*args, **kwargs)

    def SetCash(self, *args, **kwargs):
        return self.set_cash(*args, **kwargs)

    def SetBenchmark(self, *args, **kwargs):
        return self.set_benchmark(*args, **kwargs)

    def SetBrokerageModel(self, *args, **kwargs):
        return self.set_brokerage_model(*args, **kwargs)

    def SetWarmUp(self, *args, **kwargs):
        return self.set_warmup(*args, **kwargs)

    def SetWarmup(self, *args, **kwargs):
        return self.set_warmup(*args, **kwargs)

    def AddEquity(self, *args, **kwargs):
        return self.add_equity(*args, **kwargs)

    def AddForex(self, *args, **kwargs):
        return self.add_forex(*args, **kwargs)

    def AddCrypto(self, *args, **kwargs):
        return self.add_crypto(*args, **kwargs)

    def AddFuture(self, *args, **kwargs):
        return self.add_future(*args, **kwargs)

    def AddOption(self, *args, **kwargs):
        return self.add_option(*args, **kwargs)

    def AddIndex(self, *args, **kwargs):
        return self.add_index(*args, **kwargs)

    def AddData(self, *args, **kwargs):
        return self.add_data(*args, **kwargs)

    def AddSecurity(self, *args, **kwargs):
        return self.add_security(*args, **kwargs)

    def MarketOrder(self, *args, **kwargs):
        return self.market_order(*args, **kwargs)

    def LimitOrder(self, *args, **kwargs):
        return self.limit_order(*args, **kwargs)

    def StopMarketOrder(self, *args, **kwargs):
        return self.stop_market_order(*args, **kwargs)

    def StopLimitOrder(self, *args, **kwargs):
        return self.stop_limit_order(*args, **kwargs)

    def SetHoldings(self, *args, **kwargs):
        return self.set_holdings(*args, **kwargs)

    def Liquidate(self, *args, **kwargs):
        return self.liquidate(*args, **kwargs)

    def Log(self, *args, **kwargs):
        return self.log(*args, **kwargs)

    def Debug(self, *args, **kwargs):
        return self.debug(*args, **kwargs)

    def Error(self, *args, **kwargs):
        return self.error(*args, **kwargs)

    def Plot(self, *args, **kwargs):
        return self.plot(*args, **kwargs)

    def History(self, *args, **kwargs):
        return self.history(*args, **kwargs)

    def Buy(self, *args, **kwargs):
        return self.buy(*args, **kwargs)

    def Sell(self, *args, **kwargs):
        return self.sell(*args, **kwargs)

    def Order(self, *args, **kwargs):
        return self.order(*args, **kwargs)

    def EMA(self, *args, **kwargs):
        return self.ema(*args, **kwargs)

    def SMA(self, *args, **kwargs):
        return self.sma(*args, **kwargs)

    def MACD(self, *args, **kwargs):
        return self.macd(*args, **kwargs)

    def RSI(self, *args, **kwargs):
        return self.rsi(*args, **kwargs)

    def BB(self, *args, **kwargs):
        return self.bb(*args, **kwargs)

    def ATR(self, *args, **kwargs):
        return self.atr(*args, **kwargs)

    def ROC(self, *args, **kwargs):
        return self.roc(*args, **kwargs)

    def MOM(self, *args, **kwargs):
        return self.mom(*args, **kwargs)

    def WILR(self, *args, **kwargs):
        return self.wilr(*args, **kwargs)

    def CCI(self, *args, **kwargs):
        return self.cci(*args, **kwargs)

    def ADX(self, *args, **kwargs):
        return self.adx(*args, **kwargs)

    def AROON(self, *args, **kwargs):
        return self.aroon(*args, **kwargs)

    def VWAP(self, *args, **kwargs):
        return self.vwap(*args, **kwargs)

    def MOMP(self, *args, **kwargs):
        return self.momp(*args, **kwargs)

    def GetParameter(self, *args, **kwargs):
        return self.get_parameter(*args, **kwargs)

    def SetParameters(self, *args, **kwargs):
        return self.set_parameters(*args, **kwargs)

    def Train(self, *args, **kwargs):
        return self.train(*args, **kwargs)

    def Download(self, *args, **kwargs):
        return self.download(*args, **kwargs)

    def Quit(self, *args, **kwargs):
        return self.quit(*args, **kwargs)

    def SetAlpha(self, *args, **kwargs):
        return self.set_alpha(*args, **kwargs)

    def AddAlpha(self, *args, **kwargs):
        return self.add_alpha(*args, **kwargs)

    def SetPortfolioConstruction(self, *args, **kwargs):
        return self.set_portfolio_construction(*args, **kwargs)

    def SetExecution(self, *args, **kwargs):
        return self.set_execution(*args, **kwargs)

    def SetRiskManagement(self, *args, **kwargs):
        return self.set_risk_management(*args, **kwargs)

    def AddRiskManagement(self, *args, **kwargs):
        return self.add_risk_management(*args, **kwargs)

    def SetUniverseSelection(self, *args, **kwargs):
        return self.set_universe_selection(*args, **kwargs)

    def AddUniverse(self, *args, **kwargs):
        return self.add_universe(*args, **kwargs)

    def AddChart(self, *args, **kwargs):
        return self.add_chart(*args, **kwargs)

    def SetSecurityInitializer(self, *args, **kwargs):
        return self.set_security_initializer(*args, **kwargs)

    def SetAccountCurrency(self, *args, **kwargs):
        return self.set_account_currency(*args, **kwargs)

    def SetTimeZone(self, *args, **kwargs):
        return self.set_time_zone(*args, **kwargs)

    def RegisterIndicator(self, *args, **kwargs):
        return self.register_indicator(*args, **kwargs)

    def Consolidate(self, *args, **kwargs):
        return self.consolidate(*args, **kwargs)

    def Initialize(self):
        return self.initialize()

    def GetLastKnownPrices(self, *args, **kwargs):
        return self.get_last_known_prices(*args, **kwargs)

    def WarmUpIndicator(self, *args, **kwargs):
        return self.warm_up_indicator(*args, **kwargs)

    def PlotIndicator(self, *args, **kwargs):
        return self.plot_indicator(*args, **kwargs)

    def FilteredIdentity(self, *args, **kwargs):
        return self.filtered_identity(*args, **kwargs)

    def CreateIndicatorName(self, *args, **kwargs):
        return self.create_indicator_name(*args, **kwargs)

    def IV(self, *args, **kwargs):
        return self.iv(*args, **kwargs)

    def D(self, *args, **kwargs):
        return self.d(*args, **kwargs)

    def G(self, *args, **kwargs):
        return self.g(*args, **kwargs)

    def T(self, *args, **kwargs):
        return self.t(*args, **kwargs)

    def R(self, *args, **kwargs):
        return self.r(*args, **kwargs)

    def V(self, *args, **kwargs):
        return self.v(*args, **kwargs)

    # ---- Properties with PascalCase aliases ----

    @property
    def Time(self):
        return self._time

    @property
    def UtcTime(self):
        return self._utc_time

    @property
    def StartDate(self):
        return self._start_date

    @property
    def EndDate(self):
        return self._end_date

    @property
    def IsWarmingUp(self):
        return self._is_warming_up

    @property
    def LiveMode(self):
        return self._live_mode

    @property
    def Portfolio(self):
        return self.portfolio

    @property
    def Securities(self):
        return self.securities

    @property
    def Name(self):
        return self._name

    @Name.setter
    def Name(self, value):
        self._name = value

    def __repr__(self):
        return f"FinceptStrategy({self._name})"


class _ImpliedVolatilitySub:
    """Sub-object for implied_volatility on Greek indicators."""
    def __init__(self):
        self.current = type('Val', (), {'value': 0.25})()
        self.is_ready = True
        self._smoothing_func = None
    def set_smoothing_function(self, func):
        self._smoothing_func = func
    def SetSmoothingFunction(self, func):
        self._smoothing_func = func
    def update(self, *args, **kwargs):
        return True


class _OptionGreekIndicator(IndicatorBase):
    """An option greek or implied volatility as an indicator; the option manager updates it each close."""
    def __init__(self, name, kind='iv', period=1):
        super().__init__(name, period)
        self.kind = kind
        self.implied_volatility = self if kind == 'iv' else _ImpliedVolatilitySub()

    def _compute(self, value: float) -> float:
        return value

    def set_smoothing_function(self, func):
        self._smoothing = func

    SetSmoothingFunction = set_smoothing_function


class _ContractList(list):
    """List subclass with .count property (like LEAN's C# List<T>)."""
    @property
    def count(self):
        return len(self)
    Count = count


class OptionChainProvider:
    """Listed option contracts of an underlying on a date (model chain from the price known that day)."""
    def __init__(self, algo=None):
        self._algo = algo

    def get_option_contract_list(self, symbol, date=None):
        a = self._algo
        if a is None:
            raise not_supported("option chains outside a backtest run")
        mgr = a._ensure_option_manager()
        t = date or a.time
        if not isinstance(t, datetime):
            t = datetime(t.year, t.month, t.day)
        t = min(t, a.time) if t.date() == a.time.date() else t
        canonical = a._canonical_option_for(symbol)
        syms, _S = mgr.chain_symbols(canonical, t)
        return syms

    get_option_chain = get_option_contract_list
    GetOptionContractList = get_option_contract_list


class Chart:
    """Chart container for plotting."""
    def __init__(self, name: str):
        self.name = name
        self.series = {}

    def add_series(self, series):
        self.series[series.name if hasattr(series, 'name') else str(series)] = series


class Series:
    """Chart data series."""
    def __init__(self, name: str, series_type=0, unit="$"):
        self.name = name
        self.series_type = series_type
        self.unit = unit
        self.values = []


class SeriesType:
    LINE = 0
    SCATTER = 1
    CANDLE = 2
    BAR = 3
    FLAG = 4
    STACKED_AREA = 5
    PIE = 6
    TREEMAP = 7


class NotificationManager:
    """Notification stub."""
    def email(self, to, subject, body):
        pass

    def sms(self, phone, message):
        pass

    def web(self, url, data=None):
        pass


# Framework model base classes

class AlphaModel:
    """Base class for alpha models."""
    def update(self, algorithm, data):
        return []

    def on_securities_changed(self, algorithm, changes):
        pass


class PortfolioConstructionModel:
    """Base class for portfolio construction."""
    def create_targets(self, algorithm, insights):
        return []

    def on_securities_changed(self, algorithm, changes):
        """LEAN: insights of removed securities are expired."""
        removed = [s.symbol for s in getattr(changes, 'removed_securities', None) or []]
        if removed and algorithm is not None and hasattr(algorithm, 'insights'):
            algorithm.insights.expire(removed)


class ExecutionModel:
    """Base class for execution models."""
    def execute(self, algorithm, targets):
        pass

    def on_securities_changed(self, algorithm, changes):
        pass


class RiskManagementModel:
    """Base class for risk management."""
    def manage_risk(self, algorithm, targets):
        return targets

    def on_securities_changed(self, algorithm, changes):
        pass


class UniverseSelectionModel:
    """Base class for universe selection."""
    def create_universes(self, algorithm):
        return []

    def get_next_refresh_time_utc(self):
        return datetime.utcnow() + timedelta(days=1)


class NullRiskManagementModel(RiskManagementModel):
    """No-op risk management."""
    pass


class NullAlphaModel(AlphaModel):
    """No-op alpha model."""
    pass


class ConstantAlphaModel(AlphaModel):
    """Alpha model that emits constant insights for all securities."""

    def __init__(self, insight_type=None, direction=None, period=None, magnitude=None,
                 weight=None, confidence=None):
        self._type = insight_type or InsightType.PRICE
        self._direction = direction or InsightDirection.UP
        self._period = period or timedelta(days=1)
        self._magnitude = magnitude
        self._weight = weight
        self._confidence = confidence
        self._securities = []

    def update(self, algorithm, data):
        """Emit one insight per security, again only after the previous one expired."""
        insights = []
        now = algorithm.utc_time
        if not hasattr(self, '_last_close'):
            self._last_close = {}
        period = self._period if isinstance(self._period, timedelta) else (
            self._period.to_timedelta() if hasattr(self._period, 'to_timedelta') else timedelta(days=1))
        for security in self._securities:
            key = str(security.symbol).upper()
            if not data.contains_key(security.symbol):
                continue
            last = self._last_close.get(key)
            if last is not None and now < last:
                continue
            insights.append(Insight(security.symbol, period, self._type, self._direction,
                                    self._magnitude, self._confidence, weight=self._weight))
            self._last_close[key] = now + period
        return insights

    def on_securities_changed(self, algorithm, changes):
        for sec in changes.added_securities:
            if sec not in self._securities:
                self._securities.append(sec)
        for sec in changes.removed_securities:
            if sec in self._securities:
                self._securities.remove(sec)


def _insight_sign(insight):
    d = getattr(insight, 'direction', 0)
    try:
        d = int(d)
    except (TypeError, ValueError):
        d = 0
    return 1 if d > 0 else (-1 if d < 0 else 0)


def _percent_target(algorithm, symbol, percent):
    """PortfolioTarget whose quantity is the units for `percent` of portfolio value."""
    return PortfolioTarget.percent(algorithm, symbol, percent)


class EqualWeightingPortfolioConstructionModel(PortfolioConstructionModel):
    """Equal weight across the active insights: each non-flat insight gets
    direction * 1/N of portfolio value (N = active non-flat insights); flat -> 0."""

    def __init__(self, rebalance=None, portfolio_bias=None, resolution=None, **kwargs):
        self._rebalance = rebalance
        self._portfolio_bias = portfolio_bias
        self._resolution = resolution

    def _allowed(self, sign):
        bias = self._portfolio_bias
        if bias is None or int(bias) == 2:  # LONG_SHORT
            return sign
        if int(bias) == 0 and sign < 0:     # LONG only
            return 0
        if int(bias) == 1 and sign > 0:     # SHORT only
            return 0
        return sign

    def determine_target_percent(self, insights):
        active = [i for i in insights if _insight_sign(i) != 0]
        n = len(active)
        pct = 1.0 / n if n else 0.0
        return {i: self._allowed(_insight_sign(i)) * pct for i in insights}

    def create_targets(self, algorithm, insights):
        if not insights:
            return []
        return [_percent_target(algorithm, i.symbol, p) for i, p in self.determine_target_percent(insights).items()]


class InsightWeightingPortfolioConstructionModel(EqualWeightingPortfolioConstructionModel):
    """Weight each insight by |weight| (falls back to confidence, then 1); scaled down so the
    total does not exceed 100%."""

    def determine_target_percent(self, insights):
        ws = {i: abs(i.weight if i.weight is not None else (i.confidence if i.confidence is not None else 1.0))
              for i in insights}
        total = sum(w for i, w in ws.items() if _insight_sign(i) != 0)
        scale = 1.0 / total if total > 1.0 else 1.0
        return {i: self._allowed(_insight_sign(i)) * w * scale for i, w in ws.items()}


class _UnsupportedPortfolioModel(PortfolioConstructionModel):
    _feature = "this portfolio construction model"

    def __init__(self, *args, **kwargs):
        raise not_supported(self._feature)


class BlackLittermanOptimizationPortfolioConstructionModel(_UnsupportedPortfolioModel):
    _feature = "Black-Litterman portfolio optimisation"


class ImmediateExecutionModel(ExecutionModel):
    """Send market orders for the difference between each target and holdings plus open orders."""

    def execute(self, algorithm, targets):
        for target in targets:
            ticker = str(target.symbol).upper()
            sec = algorithm.securities.get(ticker)
            lot = lot_size(getattr(getattr(sec, 'symbol', None), 'security_type', None))
            diff = round_to_lot(float(target.quantity) - algorithm._unordered_quantity(ticker), lot)
            if diff != 0:
                algorithm.market_order(target.symbol, diff)


class VolumeWeightedAveragePriceExecutionModel(ExecutionModel):
    def __init__(self, *args, **kwargs):
        raise not_supported("VWAP order execution", "It slices orders over intraday volume; the engine runs on daily bars.")


class StandardDeviationExecutionModel(ExecutionModel):
    def __init__(self, *args, **kwargs):
        raise not_supported("standard-deviation order execution", "It waits for intraday price deviations; the engine runs on daily bars.")


def _cancel_insights(algorithm, symbol):
    ins = getattr(algorithm, 'insights', None)
    if ins is not None and hasattr(ins, 'cancel'):
        ins.cancel([symbol])


class MaximumDrawdownPercentPortfolio(RiskManagementModel):
    """Liquidates every targeted security (and cancels their insights) when the portfolio value falls
    more than `maximum_drawdown_percent` below its starting value - or below its high when
    is_trailing - then starts measuring again (LEAN). Returns only its override targets."""

    def __init__(self, maximum_drawdown_percent=0.05, is_trailing=False):
        self._max_dd = -abs(maximum_drawdown_percent)
        self._is_trailing = bool(is_trailing)
        self._initialised = False
        self._high = 0.0

    def manage_risk(self, algorithm, targets):
        value = algorithm.portfolio.total_portfolio_value
        if not self._initialised:
            self._high = value
            self._initialised = True
        if self._is_trailing and self._high < value:
            self._high = value
            return []
        pnl = value / self._high - 1.0 if self._high else 0.0
        if pnl < self._max_dd and targets:
            self._initialised = False
            out = []
            for t in targets:
                _cancel_insights(algorithm, t.symbol)
                out.append(PortfolioTarget(t.symbol, 0))
            return out
        return []


class MaximumDrawdownPercentPerSecurity(RiskManagementModel):
    """Liquidates a security whose unrealized loss exceeds `maximum_drawdown_percent` (LEAN)."""

    def __init__(self, maximum_drawdown_percent=0.05):
        self._max_dd = -abs(maximum_drawdown_percent)

    def manage_risk(self, algorithm, targets):
        out = []
        for ticker, holding in list(algorithm.portfolio.items()):
            if holding.invested and holding.unrealized_profit_percent < self._max_dd:
                sym = algorithm.securities[ticker].symbol if ticker in algorithm.securities else ticker
                _cancel_insights(algorithm, sym)
                out.append(PortfolioTarget(sym, 0))
        return out


class MaximumUnrealizedProfitPercentPerSecurity(RiskManagementModel):
    """Takes the profit of a security whose unrealized gain exceeds the limit (LEAN)."""

    def __init__(self, maximum_unrealized_profit_percent=0.05):
        self._max_profit = abs(maximum_unrealized_profit_percent)

    def manage_risk(self, algorithm, targets):
        out = []
        for ticker, holding in list(algorithm.portfolio.items()):
            if holding.invested and holding.unrealized_profit_percent > self._max_profit:
                sym = algorithm.securities[ticker].symbol if ticker in algorithm.securities else ticker
                _cancel_insights(algorithm, sym)
                out.append(PortfolioTarget(sym, 0))
        return out


class TrailingStopRiskManagementModel(RiskManagementModel):
    """Liquidates a position whose absolute holdings value falls `maximum_drawdown_percent` from its
    best level since it was opened (high for longs, low for shorts), as LEAN's model does."""

    def __init__(self, maximum_drawdown_percent=0.05):
        self._max_dd = abs(maximum_drawdown_percent)
        self._state = {}          # ticker -> [is_long, best absolute holdings value]

    def manage_risk(self, algorithm, targets):
        out = []
        for ticker, holding in list(algorithm.portfolio.items()):
            if not holding.invested:
                self._state.pop(ticker, None)
                continue
            is_long = holding.quantity > 0
            value = abs(holding.holdings_value)
            st = self._state.get(ticker)
            if st is None or st[0] != is_long:
                st = self._state[ticker] = [is_long, abs(holding.quantity * holding.average_price * holding.multiplier)]
            best = st[1]
            if (is_long and best < value) or (not is_long and best > value):
                st[1] = value
                continue
            drawdown = abs((best - value) / best) if best else 0.0
            if self._max_dd < drawdown:
                sym = algorithm.securities[ticker].symbol if ticker in algorithm.securities else ticker
                _cancel_insights(algorithm, sym)
                self._state.pop(ticker, None)
                out.append(PortfolioTarget(sym, 0))
        return out


def merge_risk_targets(overrides, targets):
    """LEAN: risk-model targets first, then the portfolio targets, distinct by symbol (overrides win)."""
    out, seen = [], set()
    for t in list(overrides or []) + list(targets or []):
        if t is None:
            continue
        k = str(t.symbol).upper()
        if k not in seen:
            seen.add(k)
            out.append(t)
    return out


class CompositeRiskManagementModel(RiskManagementModel):
    """Combines multiple risk management models."""

    def __init__(self, *models):
        self._models = list(models)

    def add_risk_management(self, model):
        self._models.append(model)

    def manage_risk(self, algorithm, targets):
        """LEAN: each model sees the targets so far; its overrides win (distinct by symbol)."""
        targets = list(targets or [])
        for model in self._models:
            targets = merge_risk_targets(model.manage_risk(algorithm, targets), targets)
        return targets










class CompositeAlphaModel(AlphaModel):
    """Combines multiple alpha models."""

    def __init__(self, *models):
        self._models = list(models)

    def update(self, algorithm, data):
        all_insights = []
        for model in self._models:
            insights = model.update(algorithm, data)
            if insights:
                all_insights.extend(insights)
        return all_insights

    def on_securities_changed(self, algorithm, changes):
        for model in self._models:
            model.on_securities_changed(algorithm, changes)


class PythonData:
    """Base class of custom data types (LEAN PythonData). A subclass implements get_source and
    reader; properties set by the reader (data["Close"] = x or data.close = x) are dynamic and
    case-insensitive, as in LEAN. end_time defaults to time (+ period when one is set)."""

    _CORE = ('symbol', 'time', 'value', '_data', 'period', '_end_time', 'data_type', 'is_fill_forward')

    def __init__(self):
        object.__setattr__(self, '_data', {})
        self.symbol = None
        self.time = None
        self.value = 0.0
        self.period = None
        object.__setattr__(self, '_end_time', None)

    def get_source(self, config, date, is_live_mode):
        raise NotImplementedError(f"{type(self).__name__}.get_source is not implemented")

    def reader(self, config, line, date, is_live_mode):
        raise NotImplementedError(f"{type(self).__name__}.reader is not implemented")

    def default_resolution(self):
        return Resolution.DAILY

    def supported_resolutions(self):
        return [Resolution.DAILY]

    def is_sparse_data(self):
        return False

    def requires_mapping(self):
        return False

    def clone(self):
        import copy
        return copy.copy(self)

    @property
    def end_time(self):
        e = self.__dict__.get('_end_time')
        if e is not None:
            return e
        t = self.__dict__.get('time')
        p = self.__dict__.get('period')
        return (t + p) if (t is not None and isinstance(p, timedelta)) else t

    @end_time.setter
    def end_time(self, v):
        object.__setattr__(self, '_end_time', v)

    @property
    def price(self):
        return self.value

    def __getattr__(self, name):
        if name.startswith('_'):
            raise AttributeError(name)
        d = self.__dict__.get('_data', {})
        if name.lower() in d:
            return d[name.lower()]
        raise AttributeError(f"'{type(self).__name__}' object has no attribute '{name}'")

    def __setattr__(self, name, value):
        if name in PythonData._CORE or name == 'end_time' or hasattr(type(self), name):
            object.__setattr__(self, name, value)
        else:
            self.__dict__.setdefault('_data', {})[name.lower()] = value

    def __getitem__(self, key):
        return self.__dict__.get('_data', {}).get(str(key).lower(), 0)

    def __setitem__(self, key, value):
        self.__dict__.setdefault('_data', {})[str(key).lower()] = value

    def get_storage_dictionary(self):
        return dict(self.__dict__.get('_data', {}))

    def __repr__(self):
        return f"{self.symbol}: {self.value}"

    GetSource = get_source
    Reader = reader


class PythonQuandl(PythonData):
    """Quandl data loader (stub)."""

    def __init__(self):
        super().__init__()

    def get_source(self, config, date, is_live_mode):
        return None


class InsightCollection:
    """Collection of insights with filtering capabilities."""

    def __init__(self):
        self._insights = []

    def add(self, insight):
        self._insights.append(insight)

    def add_range(self, insights):
        self._insights.extend(insights)

    def clear(self):
        self._insights.clear()

    def get_active_insights(self, utc_time):
        return [i for i in self._insights if i.close_time_utc > utc_time]

    def has_active_insights(self, symbol, utc_time):
        return any(i for i in self._insights
                   if str(i.symbol) == str(symbol) and i.close_time_utc > utc_time)

    @property
    def total_count(self):
        return len(self._insights)

    @property
    def count(self):
        return len(self._insights)

    def get_insights(self, filter_func=None):
        return _OrderList(i for i in self._insights if filter_func is None or filter_func(i))

    def contains_key(self, symbol):
        return any(str(i.symbol) == str(symbol) for i in self._insights)

    def remove_expired_insights(self, utc_time):
        self._insights = [i for i in self._insights if i.close_time_utc > utc_time]

    def expire(self, what):
        """Expire insights now (LEAN InsightManager.Expire): a symbol, symbols, or insights."""
        algo = getattr(self, '_algorithm', None)
        now = getattr(algo, 'utc_time', None)
        if now is None or what is None:
            return
        items = what if isinstance(what, (list, tuple, set)) or hasattr(what, '__iter__') and             not isinstance(what, (str, Insight)) else [what]
        keys, objs = set(), []
        for x in items:
            if isinstance(x, Insight):
                objs.append(x)
            else:
                keys.add(str(getattr(x, 'value', x)).upper())
        for i in self._insights:
            if i in objs or (keys and str(getattr(i.symbol, 'value', i.symbol)).upper() in keys):
                i.expire(now)

    cancel = expire
    Expire = expire
    Cancel = expire

    def set_insight_score_function(self, func):
        """Set a custom scoring function for insights."""
        self._score_function = func

    def __iter__(self):
        return iter(self._insights)

    def __len__(self):
        return len(self._insights)


class Field:
    """Universe selection field for sorting/filtering."""

    @staticmethod
    def dollar_volume(x):
        return getattr(x, 'dollar_volume', 0)

    @staticmethod
    def price(x):
        return getattr(x, 'price', 0)

    @staticmethod
    def volume(x):
        return getattr(x, 'volume', 0)

    CLOSE = staticmethod(lambda x: getattr(x, 'close', getattr(x, 'price', 0)))
    OPEN = staticmethod(lambda x: getattr(x, 'open', 0))
    HIGH = staticmethod(lambda x: getattr(x, 'high', 0))
    LOW = staticmethod(lambda x: getattr(x, 'low', 0))
    VOLUME = staticmethod(lambda x: getattr(x, 'volume', 0))
    AVERAGE = staticmethod(lambda x: getattr(x, 'close', getattr(x, 'price', 0)))

    # Lowercase aliases (used by many strategies)
    close = CLOSE
    open = OPEN
    high = HIGH
    low = LOW
    average = AVERAGE

    SEVEN_BAR = staticmethod(lambda x: getattr(x, 'close', getattr(x, 'price', 0)))
    BID_CLOSE = staticmethod(lambda x: getattr(x, 'bid_close', getattr(x, 'close', 0)))
    ASK_CLOSE = staticmethod(lambda x: getattr(x, 'ask_close', getattr(x, 'close', 0)))
    BID_OPEN = staticmethod(lambda x: getattr(x, 'bid_open', getattr(x, 'open', 0)))
    ASK_OPEN = staticmethod(lambda x: getattr(x, 'ask_open', getattr(x, 'open', 0)))
    BID_HIGH = staticmethod(lambda x: getattr(x, 'bid_high', getattr(x, 'high', 0)))
    ASK_HIGH = staticmethod(lambda x: getattr(x, 'ask_high', getattr(x, 'high', 0)))
    BID_LOW = staticmethod(lambda x: getattr(x, 'bid_low', getattr(x, 'low', 0)))
    ASK_LOW = staticmethod(lambda x: getattr(x, 'ask_low', getattr(x, 'low', 0)))
    SevenBar = SEVEN_BAR
    BidClose = BID_CLOSE
    AskClose = ASK_CLOSE
    BidOpen = BID_OPEN
    AskOpen = ASK_OPEN
    BidHigh = BID_HIGH
    AskHigh = ASK_HIGH
    BidLow = BID_LOW
    AskLow = ASK_LOW

    DollarVolume = staticmethod(lambda x: getattr(x, 'dollar_volume', 0))
    Price = staticmethod(lambda x: getattr(x, 'price', 0))
    Volume = staticmethod(lambda x: getattr(x, 'volume', 0))
    Close = CLOSE
    Open = OPEN
    High = HIGH
    Low = LOW
    Average = AVERAGE


class Futures:
    """Futures contract specifications."""

    class Indices:
        SP_500_E_MINI = "ES"
        NASDAQ_100_E_MINI = "NQ"
        DOW_30_E_MINI = "YM"
        RUSSELL_2000_E_MINI = "RTY"
        VIX = "VX"
        SP500 = "ES"
        MICRO_SP_500_E_MINI = "MES"
        EURO_STOXX_50 = "FESX"
        NIKKEI_225 = "NK"
        FTSE_100 = "Z"
        DAX = "FDAX"
        HANG_SENG = "HSI"
        MSCI_EMERGING_MARKETS = "MXEF"

    class Metals:
        GOLD = "GC"
        SILVER = "SI"
        PLATINUM = "PL"
        PALLADIUM = "PA"
        COPPER = "HG"
        MICRO_GOLD = "MGC"
        MICRO_SILVER = "SIL"

    class Energies:
        CRUDE_OIL_WTI = "CL"
        NATURAL_GAS = "NG"
        HEATING_OIL = "HO"
        GASOLINE = "RB"
        BRENT_CRUDE = "BZ"
        MICRO_CRUDE_OIL_WTI = "MCL"

    class Grains:
        CORN = "ZC"
        WHEAT = "ZW"
        SOYBEANS = "ZS"
        SOYBEAN_MEAL = "ZM"
        SOYBEAN_OIL = "ZL"
        OATS = "ZO"

    class Currencies:
        EUR = "6E"
        GBP = "6B"
        JPY = "6J"
        AUD = "6A"
        CAD = "6C"
        CHF = "6S"

    class Financials:
        Y_2_TREASURY_NOTE = "ZT"
        Y_5_TREASURY_NOTE = "ZF"
        Y_10_TREASURY_NOTE = "ZN"
        Y_30_TREASURY_BOND = "ZB"
        EURODOLLAR = "GE"

    class Meats:
        LIVE_CATTLE = "LE"
        FEEDER_CATTLE = "GF"
        LEAN_HOGS = "HE"

    class Softs:
        SUGAR = "SB"
        COTTON = "CT"
        COFFEE = "KC"
        COCOA = "CC"


class Globals:
    """Global settings."""
    DataFolder = ""
    Cache = ""


from .types import SecurityIdentifier  # noqa: E402  (LEAN identifiers live with Symbol)


class PortfolioBias:
    """Portfolio bias setting."""
    LONG = 0
    SHORT = 1
    LONG_SHORT = 2

    Long = 0
    Short = 1
    LongShort = 2


class Resolution_:
    """Namespace aliases for Resolution to support both Resolution.Daily and Resolution.DAILY"""
    pass
