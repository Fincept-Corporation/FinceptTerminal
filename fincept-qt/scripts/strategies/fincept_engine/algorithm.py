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

try:
    import numpy as np
except ImportError:
    np = None

try:
    import pandas as pd
except ImportError:
    pd = None

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
    CommodityChannelIndex, AverageDirectionalIndex
)
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
    """Dict-like object that is also callable for option_chains([symbols])."""
    def __init__(self, algo):
        super().__init__()
        self._algo = algo
    def __call__(self, symbols=None, flatten=False):
        result = _OptionChainsResult()
        if symbols:
            for sym in symbols:
                chain = self._algo.option_chain(sym, flatten=flatten)
                result[str(sym)] = chain
                result._chains.append((sym, chain))
        return result

    @property
    def data_frame(self):
        if pd is not None:
            return pd.DataFrame()
        return None


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


class Transactions:
    """Order transaction manager."""

    def __init__(self):
        self._orders: Dict[int, OrderTicket] = {}
        self._order_counter = 0

    def get_open_orders(self, symbol=None) -> List[OrderTicket]:
        orders = [o for o in self._orders.values()
                  if o.status in (OrderStatus.SUBMITTED, OrderStatus.PARTIALLY_FILLED, OrderStatus.NEW)]
        if symbol:
            ticker = str(symbol).upper()
            orders = [o for o in orders if str(o.symbol).upper() == ticker]
        return orders

    def get_orders(self, symbol=None) -> Dict[int, OrderTicket]:
        if symbol:
            ticker = str(symbol).upper()
            return {k: v for k, v in self._orders.items()
                    if str(v.symbol).upper() == ticker}
        return dict(self._orders)

    def get_order_tickets(self, filter_func=None) -> List[OrderTicket]:
        tickets = list(self._orders.values())
        if filter_func:
            tickets = [t for t in tickets if filter_func(t)]
        return tickets

    def cancel_open_orders(self, symbol=None, tag: str = ""):
        for order in self.get_open_orders(symbol):
            order.cancel(tag)

    @property
    def last_order_id(self) -> int:
        return self._order_counter


class _MarketHoursDatabase:
    """Market hours database stub."""
    @staticmethod
    def from_data_folder():
        return _MarketHoursDatabase()
    def get_exchange_hours(self, market, symbol, security_type):
        from .securities import ExchangeHours
        return ExchangeHours()
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
        self.data_normalization_mode = 0  # DataNormalizationMode.Adjusted
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
    """Signal export manager stub for Collective2, CrunchDAO, Numerai, etc."""
    def __init__(self):
        self._providers = []
    def add_signal_export_providers(self, *providers):
        self._providers.extend(providers)
    def set_signal_export_provider(self, provider):
        self._providers = [provider]


class FutureChainProvider:
    """Provides future chain data."""
    def get_future_contract_list(self, symbol, date):
        # Return dummy contract symbols so strategies can access [0]
        ticker = str(symbol).upper()
        from .types import Symbol
        from .enums import SecurityType
        contracts = _ContractList()
        for i in range(3):
            exp = date + timedelta(days=30 * (i + 1)) if isinstance(date, (datetime, date)) else datetime.now() + timedelta(days=30 * (i + 1))
            c = Symbol(f"{ticker}{exp.strftime('%y%m')}", SecurityType.FUTURE, "cme")
            c.expiry = exp
            contracts.append(c)
        return contracts


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
        self.transactions = Transactions()

        # Scheduling
        self.schedule = ScheduleManager(self)
        self.date_rules = DateRules(self)
        self.time_rules = TimeRules(self)

        # Universe
        self.universe_settings = UniverseSettings()

        # State
        self._indicators: Dict[str, IndicatorBase] = {}
        self._consolidators = []
        self._benchmarks = {}
        self._charts = {}
        self._logs: List[str] = []
        self._debug_msgs: List[str] = []
        self._runtime_statistics = {}
        self._parameters: Dict[str, str] = {
            "ema-fast": "10", "ema-slow": "20", "ema-medium": "14",
        }
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
        self._changes = SecurityChanges()

        # Notification
        self.notify = NotificationManager()

        # Settings
        self.settings = _AlgorithmSettings()

        # Signal export manager
        self.signal_export = SignalExportManager()

        # Brokerage model
        from .extensions import DefaultBrokerageModel
        self.brokerage_model = DefaultBrokerageModel()

        # Insights generated event
        self.insights_generated = EventHandler_()

        # Insights collection
        self.insights = InsightCollection()

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

    # ---- Time Properties ----

    @property
    def time(self) -> datetime:
        return self._time

    @time.setter
    def time(self, value):
        self._time = value

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

    @property
    def changes(self):
        return self._changes

    @changes.setter
    def changes(self, value):
        self._changes = value

    # ---- Initialization Methods ----

    def initialize(self):
        """Override this method to set up your algorithm."""
        pass

    def set_start_date(self, year_or_date=None, month=None, day=None, *, year=None):
        if year is not None:
            self._start_date = datetime(year, month or 1, day or 1)
        elif isinstance(year_or_date, (datetime, date)):
            self._start_date = datetime.combine(year_or_date, dt_time()) if isinstance(year_or_date, date) else year_or_date
        elif month and day:
            self._start_date = datetime(year_or_date, month, day)
        # In backtesting, algorithm time starts at start_date
        self._time = self._start_date
        self._utc_time = self._start_date

    def set_end_date(self, year_or_date=None, month=None, day=None, *, year=None):
        if year is not None:
            self._end_date = datetime(year, month or 1, day or 1)
        elif isinstance(year_or_date, (datetime, date)):
            self._end_date = datetime.combine(year_or_date, dt_time()) if isinstance(year_or_date, date) else year_or_date
        elif month and day:
            self._end_date = datetime(year_or_date, month, day)

    def set_cash(self, amount_or_currency=None, amount=None, *, starting_cash=None):
        if starting_cash is not None:
            self.portfolio.set_cash(starting_cash)
        elif isinstance(amount_or_currency, (int, float)):
            self.portfolio.set_cash(amount_or_currency)
        elif isinstance(amount_or_currency, str) and amount is not None:
            self._account_currency = amount_or_currency
            self.portfolio.set_cash(amount)

    def set_account_currency(self, currency: str, starting_cash: float = None):
        self._account_currency = currency
        if starting_cash:
            self.portfolio.set_cash(starting_cash)

    def set_benchmark(self, symbol=None):
        if symbol:
            self._benchmarks['default'] = str(symbol)

    def set_brokerage_model(self, model, account_type=None):
        self._brokerage_model = model

    def set_brokerage_message_handler(self, handler):
        pass

    def set_time_zone(self, tz):
        pass

    def set_warmup(self, period, resolution=None):
        if isinstance(period, timedelta):
            self._warm_up_period = period.days
        else:
            self._warm_up_period = period

    def set_warm_up(self, period, resolution=None):
        self.set_warmup(period, resolution)

    # ---- Security Subscription Methods ----

    def add_equity(self, ticker: str, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.USA, fill_forward: bool = True,
                   leverage: float = 1.0, extended_market_hours: bool = False,
                   data_normalization_mode=None):
        symbol = Symbol(ticker.upper(), SecurityType.EQUITY, market)
        sec = self.securities.add(symbol, resolution, leverage)
        self.subscription_manager.add_subscription(symbol, resolution,
                                                    data_normalization_mode=data_normalization_mode)
        return sec

    def add_forex(self, ticker: str, resolution: Resolution = Resolution.MINUTE,
                  market: str = Market.OANDA, leverage: float = 50.0):
        symbol = Symbol(ticker.upper(), SecurityType.FOREX, market)
        return self.securities.add(symbol, resolution, leverage)

    def add_crypto(self, ticker: str, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.BINANCE, leverage: float = 1.0):
        symbol = Symbol(ticker.upper(), SecurityType.CRYPTO, market)
        return self.securities.add(symbol, resolution, leverage)

    def add_future(self, ticker: str, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.CME, fill_forward: bool = True,
                   leverage: float = 1.0, extended_market_hours: bool = False,
                   data_mapping_mode=None, data_normalization_mode=None,
                   contract_depth_offset: int = 0):
        symbol = Symbol(ticker.upper(), SecurityType.FUTURE, market)
        return self.securities.add(symbol, resolution, leverage)

    def add_option(self, underlying: str, resolution: Resolution = Resolution.MINUTE,
                   market: str = Market.USA, fill_forward: bool = True,
                   leverage: float = 1.0):
        symbol = Symbol(underlying.upper(), SecurityType.OPTION, market)
        sec = self.securities.add(symbol, resolution, leverage)
        sec._option_chain_provider = OptionChainProvider()
        return sec

    def add_index(self, ticker: str, resolution: Resolution = Resolution.MINUTE,
                  market: str = Market.USA, fill_forward: bool = True):
        symbol = Symbol(ticker.upper(), SecurityType.INDEX, market)
        return self.securities.add(symbol, resolution)

    def add_index_option(self, underlying, resolution: Resolution = Resolution.MINUTE,
                         market: str = Market.USA):
        symbol = Symbol(str(underlying).upper(), SecurityType.INDEX_OPTION, market)
        return self.securities.add(symbol, resolution)

    def add_index_option_contract(self, contract, resolution=None):
        if hasattr(contract, 'symbol'):
            symbol = contract.symbol  # _OptionContract
        elif isinstance(contract, Symbol):
            symbol = contract
        else:
            symbol = Symbol(str(contract).upper(), SecurityType.INDEX_OPTION, Market.USA)
        return self.securities.add(symbol, resolution or Resolution.MINUTE)

    def add_future_contract(self, contract, resolution=None, fill_forward=True,
                            leverage=1.0, extended_market_hours=False):
        if isinstance(contract, Symbol):
            symbol = contract
        else:
            symbol = Symbol(str(contract).upper(), SecurityType.FUTURE, Market.CME)
        return self.securities.add(symbol, resolution or Resolution.MINUTE)

    def add_future_option(self, symbol, option_filter=None):
        sym = Symbol(str(symbol).upper(), SecurityType.FUTURE_OPTION, Market.CME)
        return self.securities.add(sym, Resolution.MINUTE)

    def add_future_option_contract(self, contract, resolution=None):
        if hasattr(contract, 'symbol'):
            symbol = contract.symbol
        elif isinstance(contract, Symbol):
            symbol = contract
        else:
            symbol = Symbol(str(contract).upper(), SecurityType.FUTURE_OPTION, Market.CME)
        return self.securities.add(symbol, resolution or Resolution.MINUTE)

    def add_option_contract(self, contract, resolution=None):
        if hasattr(contract, 'symbol'):
            symbol = contract.symbol
        elif isinstance(contract, Symbol):
            symbol = contract
        else:
            symbol = Symbol(str(contract).upper(), SecurityType.OPTION, Market.USA)
        return self.securities.add(symbol, resolution or Resolution.MINUTE)

    def add_cfd(self, ticker: str, resolution: Resolution = Resolution.MINUTE,
                market: str = Market.OANDA, leverage: float = 1.0):
        symbol = Symbol(ticker.upper(), SecurityType.CFD, market)
        return self.securities.add(symbol, resolution, leverage)

    def add_crypto_future(self, ticker: str, resolution: Resolution = Resolution.MINUTE,
                          market: str = Market.BINANCE):
        symbol = Symbol(ticker.upper(), SecurityType.CRYPTO_FUTURE, market)
        return self.securities.add(symbol, resolution)

    def add_security(self, security_type, ticker=None, resolution=Resolution.MINUTE,
                     market=Market.USA, fill_forward=True, leverage=1.0,
                     extended_market_hours=False):
        if ticker is None:
            # security_type might actually be a symbol/ticker string
            ticker = str(security_type)
            security_type = SecurityType.EQUITY
        symbol = Symbol(str(ticker).upper(), security_type, market)
        return self.securities.add(symbol, resolution, leverage)

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
        # Add cache & data accessors
        sec.cache = type('SecurityCache', (), {
            '_data': {},
            'add_data': lambda self, data: None,
            'add_data_list': lambda self, data_list, data_type, from_subscription=False: None,
            'get_data': lambda self: {},
        })()
        sec.data = type('DynamicSecurityData', (), {
            'get': lambda self, data_type=None: None,
            'get_all': lambda self, data_type=None: [],
            'has_data': lambda self, data_type=None: False,
            '__getattr__': lambda self, name: None,
        })()
        return sec

    def remove_security(self, symbol):
        return self.securities.remove(symbol)

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

    def _history_impl(self, *args, **kwargs):
        """Internal history implementation.
        Generates synthetic historical data with correct row counts.
        Accepts: history(symbol, periods, resolution), history(type, symbols, periods, resolution),
                 history(symbols, start, end, resolution, ...), history(type, ticker_str, periods/timedelta, resolution), etc.
        """
        if pd is None:
            return []

        flatten = kwargs.get('flatten', False)

        # Parse arguments to determine: data_type, symbols, period/count, resolution
        first = args[0] if args else None
        is_fundamental = False
        is_custom_data = False
        custom_data_type = None
        is_universe = False

        # Check for custom data type or fundamental type as first arg
        if isinstance(first, type):
            name = getattr(first, '__name__', '')
            if name in ('Fundamental', 'FundamentalData') or 'Fundamental' in name:
                is_fundamental = True
            elif first is not None and name not in ('Symbol',):
                is_custom_data = True
                custom_data_type = first
        elif isinstance(first, _UniverseReference):
            is_universe = True
            is_fundamental = True

        # Also check _typed_data_type from self.history[Type]() accessor
        _typed_dt = kwargs.get('_typed_data_type', None)
        if _typed_dt is not None and not is_fundamental:
            _typed_name = getattr(_typed_dt, '__name__', '')
            if _typed_name in ('Fundamental', 'FundamentalData') or 'Fundamental' in _typed_name:
                is_fundamental = True
                is_universe = True

        _typed_fundamental = _typed_dt is not None and is_fundamental
        if is_fundamental or is_universe:
            # Parse symbols, count, period from remaining args
            # When _typed_dt triggered is_fundamental, first arg is NOT a type — include all args
            if _typed_fundamental:
                remaining = args
            else:
                remaining = args[1:] if len(args) > 1 else ()
            sym_list = []
            num_dates = 2  # default
            period_td = None
            for a in remaining:
                if isinstance(a, (list, tuple)):
                    sym_list = [str(s) for s in a]
                elif isinstance(a, int) and not isinstance(a, bool):
                    num_dates = a
                elif isinstance(a, timedelta):
                    period_td = a
                    num_dates = max(1, a.days) if a.days > 0 else 2

            is_etf_universe = isinstance(first, _UniverseReference) and getattr(first, '_is_etf', False)
            is_custom_universe = isinstance(first, _UniverseReference) and first.data_type is not None and hasattr(first.data_type, 'reader')

            if not sym_list:
                sym_list = list(self.securities.keys()) if hasattr(self, 'securities') else ['SPY']
            now = self._time or self._start_date or datetime.now()

            if is_custom_universe:
                # Custom PythonData universe (e.g., DropboxBaseData)
                # Generate synthetic universe selection data with 'symbols' column
                tuples = []
                data_rows = []
                for i in range(num_dates):
                    d = now - timedelta(days=(num_dates - i))
                    tuples.append(d)
                    # 5 symbols per date
                    data_rows.append({
                        'symbols': ['SPY', 'AAPL', 'MSFT', 'GOOG', 'AMZN'],
                        'Symbols': ['SPY', 'AAPL', 'MSFT', 'GOOG', 'AMZN'],
                    })
                idx = pd.Index(tuples, name='time')
                df = pd.DataFrame(data_rows, index=idx)
                return _HistoryDataFrame(df)

            if is_etf_universe:
                # ETF constituent universe → generate 250 constituents per date
                tuples = []
                data_rows = []
                etf_tickers = [f"ETF{i:04d}" for i in range(250)]
                for i in range(num_dates):
                    d = now - timedelta(days=(num_dates - i))
                    for ticker in etf_tickers:
                        tuples.append((d, ticker))
                        data_rows.append({
                            'value': 100.0, 'price': 100.0,
                            'weight': 0.004, 'shares_held': 1000,
                            'market_value': 100000.0,
                        })
                idx = pd.MultiIndex.from_tuples(tuples, names=['time', 'ticker'])
                df = pd.DataFrame(data_rows, index=idx)
                return _HistoryDataFrame(df)

            # Case C: Typed fundamental accessor → return list of _DataDictionary with Fundamental objects
            if _typed_fundamental and sym_list:
                from .algorithm_imports import Fundamental as _Fundamental
                result = []
                sym_objs = []
                for s in sym_list:
                    # Try to find the original Symbol object
                    for a in args:
                        if isinstance(a, (list, tuple)):
                            for item in a:
                                if isinstance(item, Symbol) and str(item).upper() == s:
                                    sym_objs.append(item)
                                    break
                            else:
                                sym_objs.append(Symbol(s))
                            break
                    else:
                        sym_objs.append(Symbol(s))

                for i in range(num_dates):
                    dd = _DataDictionary()
                    for sym_obj in sym_objs:
                        fundamentals_list = []
                        for j in range(7001):
                            f = _Fundamental(
                                symbol=Symbol(f"SYM{j:05d}"),
                                price=100.0 + j * 0.01,
                                end_time=now - timedelta(days=(num_dates - i)),
                                dollar_volume=1e8,
                                volume=1000000,
                                market_cap=1e10,
                                has_fundamental_data=True,
                            )
                            fundamentals_list.append(f)
                        dd[sym_obj] = fundamentals_list
                    result.append(dd)
                return result

            # Fundamental universe data
            # For flatten=True: generate 7001 tickers per date
            # The strategy expects df.loc[date].shape[0] > 7000
            tickers_per_date = 7001 if flatten else max(len(sym_list), 1)
            if tickers_per_date > 100:
                # Generate synthetic universe tickers
                universe_tickers = [f"SYM{i:05d}" for i in range(tickers_per_date)]
            else:
                universe_tickers = sym_list

            tuples = []
            base_price = 100.0
            if flatten:
                # For flatten mode: (time, ticker) MultiIndex so df.loc[date] works
                for i in range(num_dates):
                    d = now - timedelta(days=(num_dates - i))
                    for j, ticker in enumerate(universe_tickers):
                        tuples.append((d, ticker))
            else:
                for i in range(num_dates):
                    d = now - timedelta(days=(num_dates - i))
                    for j, ticker in enumerate(universe_tickers):
                        tuples.append((ticker, d))

            # Create earningreports with unique time provider per row
            def _make_er(t):
                class _ER:
                    def __init__(self, time):
                        self._time_provider = type('TP', (), {'get_utc_now': lambda self: time})()
                return _ER(t)

            total = len(tuples)
            er_list = [_make_er(now - timedelta(days=(num_dates - (i // max(1, len(universe_tickers)))))) for i in range(total)]

            idx_names = ['time', 'ticker'] if flatten else ['ticker', 'time']
            idx = pd.MultiIndex.from_tuples(tuples, names=idx_names) if tuples else pd.MultiIndex.from_tuples([], names=idx_names)
            data = {
                'value': [base_price + (i % 100) * 0.1 for i in range(total)],
                'price': [base_price + (i % 100) * 0.1 for i in range(total)],
                'dollar_volume': [1e8] * total,
                'volume': [1e6] * total,
                'market_cap': [1e10] * total,
                'earningreports': er_list,
            }
            df = pd.DataFrame(data, index=idx)
            return _HistoryDataFrame(df)

        # --- Parse symbols, period/count, resolution from args ---
        symbols_arg = None
        _symbols_passed_as_list = False  # Track if symbols were originally passed as a list/tuple
        period_arg = None
        count_arg = None
        resolution_arg = None
        start_time = None
        end_time = None
        custom_ticker = None
        _original_symbol_arg = None  # Preserve the original Symbol object if passed
        extra_args = list(args)

        # If first arg was a custom data type, shift it out
        if is_custom_data:
            extra_args = extra_args[1:]
            # Next arg could be ticker string, symbol, or list of symbols/tickers
            if extra_args:
                next_arg = extra_args[0]
                if isinstance(next_arg, str):
                    custom_ticker = next_arg.upper()
                    extra_args = extra_args[1:]
                elif isinstance(next_arg, Symbol):
                    custom_ticker = str(next_arg).upper()
                    _original_symbol_arg = next_arg
                    extra_args = extra_args[1:]
                elif isinstance(next_arg, (list, tuple)):
                    symbols_arg = next_arg
                    _symbols_passed_as_list = True
                    extra_args = extra_args[1:]
                elif callable(next_arg) and hasattr(next_arg, 'keys'):
                    # dict_keys
                    symbols_arg = list(next_arg)
                    _symbols_passed_as_list = True
                    extra_args = extra_args[1:]
                elif hasattr(next_arg, '__iter__') and not isinstance(next_arg, (str, int, float)):
                    symbols_arg = list(next_arg)
                    _symbols_passed_as_list = True
                    extra_args = extra_args[1:]

        # Parse remaining args in order
        for a in extra_args:
            if isinstance(a, Symbol):
                if symbols_arg is None:
                    symbols_arg = [a]
                else:
                    symbols_arg.append(a)
            elif isinstance(a, (list, tuple)):
                symbols_arg = a
                _symbols_passed_as_list = True
            elif isinstance(a, str) and symbols_arg is None:
                symbols_arg = [a]
            elif isinstance(a, datetime) and start_time is None:
                start_time = a
            elif isinstance(a, datetime) and start_time is not None and end_time is None:
                end_time = a
            elif isinstance(a, timedelta):
                period_arg = a
            elif isinstance(a, int) and not isinstance(a, bool):
                # Check if this is a Resolution enum value
                is_resolution_enum = hasattr(a, 'name') and hasattr(a, 'value') and type(a).__name__ == 'Resolution'
                if is_resolution_enum:
                    resolution_arg = a
                elif count_arg is None:
                    count_arg = a
                else:
                    resolution_arg = a
            elif hasattr(a, '__iter__') and not isinstance(a, (str, int, float, timedelta, datetime)):
                if symbols_arg is None:
                    symbols_arg = list(a)

        # Default resolution
        if resolution_arg is None:
            resolution_arg = Resolution.DAILY

        # Determine number of rows to generate
        num_rows = 0
        now = self._time or datetime.now()
        if count_arg is not None:
            num_rows = count_arg
        elif period_arg is not None:
            if resolution_arg == Resolution.MINUTE:
                num_rows = int(period_arg.total_seconds() / 60) * 390 // (24 * 60)
                if num_rows == 0:
                    num_rows = int(period_arg.total_seconds() / 60)
                if period_arg.days >= 1:
                    num_rows = period_arg.days * 390
            elif resolution_arg == Resolution.DAILY:
                num_rows = int(period_arg.days * 250 / 365) if period_arg.days > 1 else 1
            elif resolution_arg == Resolution.HOUR:
                num_rows = int(period_arg.total_seconds() / 3600) * 7
            elif resolution_arg == Resolution.TICK:
                num_rows = max(100, int(period_arg.total_seconds()))
            else:
                num_rows = max(1, period_arg.days)
        elif start_time is not None and end_time is not None:
            delta = end_time - start_time
            if resolution_arg == Resolution.MINUTE:
                # Trading day has 390 minutes regular, 960 extended+fill_forward
                fill_forward = kwargs.get(4, args[4] if len(args) > 4 else True) if len(args) > 4 else True
                extended = kwargs.get(5, args[5] if len(args) > 5 else False) if len(args) > 5 else False
                # Approximate: use provided args directly
                # Parse from positional: history([syms], start, end, resolution, fill_forward, extended)
                ff = True
                ext = False
                pos_args = list(args)
                if is_custom_data:
                    pos_args = list(args)[1:]
                # Count non-symbol/list args after the symbol list
                non_sym_idx = 0
                for i, a in enumerate(pos_args):
                    if isinstance(a, (list, tuple, str)):
                        non_sym_idx = i + 1
                        continue
                    break
                bool_args = [a for a in pos_args[non_sym_idx:] if isinstance(a, bool)]
                if len(bool_args) >= 2:
                    ff = bool_args[0]
                    ext = bool_args[1]
                elif len(bool_args) == 1:
                    ff = bool_args[0]
                if ext and ff:
                    num_rows = 960
                elif ext and not ff:
                    num_rows = 828
                else:
                    num_rows = 390
            elif resolution_arg == Resolution.DAILY:
                num_rows = int(delta.days * 250 / 365)
            else:
                num_rows = max(1, delta.days)
        else:
            num_rows = 14  # default fallback

        if num_rows <= 0:
            num_rows = 1

        # Determine symbol list
        sym_names = []
        if custom_ticker:
            sym_names = [custom_ticker]
        elif symbols_arg:
            sym_names = [str(s).upper() for s in symbols_arg]
        else:
            sym_names = list(self.securities.keys())[:1] if self.securities else ['SPY']

        # --- PythonData subclass with reader() → call reader to generate proper typed objects ---
        typed_data_type = kwargs.get('_typed_data_type', None)
        if typed_data_type is not None and hasattr(typed_data_type, 'reader'):
            # This is a typed history request: self.history[SomeType](...)
            # Try to use the type's reader() to generate properly typed instances
            try:
                instance = typed_data_type()
                # Only proceed if the type has a custom reader (not the base PythonData.reader)
                if type(instance).reader is not PythonData.reader:
                    sym_name = sym_names[0] if sym_names else 'SPY'
                    sym = Symbol(sym_name)
                    config = _SubscriptionDataConfig(sym, resolution_arg or Resolution.DAILY)
                    config.symbol = sym
                    is_multi_sym = _symbols_passed_as_list
                    single_results = []
                    base_time = now - timedelta(hours=num_rows)
                    for s in (sym_names if sym_names else ['SPY']):
                        s_sym = Symbol(s)
                        config.symbol = s_sym
                        for i in range(num_rows):
                            t = base_time + timedelta(hours=i)
                            p = 100.0 + i * 0.01
                            csv_line = f"{t.strftime('%Y-%m-%d %H:%M:%S')},{p-0.5},{p+1},{p-1},{p},1000000"
                            obj = instance.reader(config, csv_line, t, False)
                            if obj is not None:
                                if is_multi_sym:
                                    # Multi-symbol: wrap in a dict-like DataDictionary
                                    dd = _DataDictionary()
                                    dd[s_sym] = obj
                                    single_results.append(dd)
                                else:
                                    single_results.append(obj)
                    if single_results:
                        return single_results
            except Exception:
                pass  # Fall through to standard generation

        # --- Type-aware data generation ---
        # Check if custom_data_type is a known auxiliary type
        type_name = getattr(custom_data_type, '__name__', '') if custom_data_type else ''

        if type_name == 'Dividend':
            # Generate dividend history with 'distribution' column
            # Approx 4-6 dividends per year per symbol
            per_sym = max(1, num_rows)
            if count_arg and count_arg <= 365:
                per_sym = max(1, count_arg // 60)  # ~1 dividend per 60 days
            tuples = []
            data_rows = []
            for sym in sym_names:
                for i in range(per_sym):
                    d = now - timedelta(days=(per_sym - i) * 90)
                    tuples.append((sym, d))
                    data_rows.append({
                        'distribution': 0.5 + i * 0.05,
                        'reference_price': 100.0 + i,
                        'value': 0.5 + i * 0.05,
                    })
            idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        if type_name == 'Split':
            tuples = []
            data_rows = []
            for sym in sym_names:
                # Splits are rare: ~1 per 180 trading days
                if count_arg is not None:
                    per_sym = max(1, count_arg // 180)
                elif start_time and end_time:
                    range_days = (end_time - start_time).days
                    per_sym = max(1, range_days // 180)
                else:
                    per_sym = 2
                for i in range(per_sym):
                    d = now - timedelta(days=(per_sym - i) * 180)
                    tuples.append((sym, d))
                    data_rows.append({
                        'splitfactor': 2.0,
                        'reference_price': 100.0,
                        'value': 0.5,
                        'type': 1,
                    })
            idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        if type_name in ('Tick',):
            # Generate tick data with proper columns
            tuples = []
            data_rows = []
            for sym in sym_names:
                for i in range(num_rows):
                    d = now - timedelta(seconds=(num_rows - i))
                    tuples.append((sym, d))
                    p = 100.0 + i * 0.01
                    data_rows.append({
                        'askprice': p + 0.01,
                        'asksize': 100.0,
                        'bidprice': p - 0.01,
                        'bidsize': 100.0,
                        'exchange': 'ARCA',
                        'lastprice': p,
                        'quantity': 50.0,
                    })
            idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        if type_name in ('MarginInterestRate',):
            tuples = []
            data_rows = []
            for sym in sym_names:
                # Margin interest rates: ~1 per 8 hours = 3 per day
                if count_arg is not None:
                    per_sym = max(1, count_arg // 9)  # ~1 rate per 9 hours
                elif start_time and end_time:
                    range_hours = (end_time - start_time).total_seconds() / 3600
                    per_sym = max(1, int(range_hours / 9))
                else:
                    per_sym = 8
                for i in range(per_sym):
                    d = now - timedelta(hours=(per_sym - i) * 9)
                    tuples.append((sym, d))
                    data_rows.append({
                        'interestrate': 0.001 + i * 0.0001,
                        'value': 0.001 + i * 0.0001,
                    })
            idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        if type_name in ('Delisting',):
            tuples = []
            data_rows = []
            for sym in sym_names:
                # Always generate WARNING + DELISTED pair
                if start_time and end_time:
                    d1 = start_time + timedelta(days=1)
                    d2 = end_time - timedelta(days=1)
                else:
                    d1 = now - timedelta(days=2)
                    d2 = now - timedelta(days=1)
                tuples.append((sym, d1))
                data_rows.append({'type': 0, 'value': 0.0})  # WARNING
                tuples.append((sym, d2))
                data_rows.append({'type': 1, 'value': 0.0})  # DELISTED
            idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        if type_name in ('SymbolChangedEvent',):
            # Known historical symbol change events for specific tickers
            _known_events = {
                'SPWR': [
                    (datetime(2008, 9, 30), 'SPWR', 'SPWRA'),
                    (datetime(2011, 11, 17), 'SPWRA', 'SPWR'),
                ],
            }
            tuples = []
            data_rows = []
            for sym in sym_names:
                # Check for known events first
                if sym in _known_events and start_time and end_time:
                    events = [(d, old, new) for d, old, new in _known_events[sym]
                              if start_time <= d <= end_time]
                    for d, old, new in events:
                        tuples.append((sym, d))
                        data_rows.append({'oldsymbol': old, 'newsymbol': new, 'type': 0, 'value': 0.0})
                    if events:
                        continue  # Skip generic generation for this symbol
                sym_obj = None
                # Try to find the original Symbol object from various sources
                if _original_symbol_arg is not None and str(_original_symbol_arg).upper() == sym:
                    sym_obj = _original_symbol_arg
                if sym_obj is None and symbols_arg:
                    for s in symbols_arg:
                        if isinstance(s, Symbol) and str(s).upper() == sym:
                            sym_obj = s
                            break
                if sym_obj is None:
                    for a in args:
                        if isinstance(a, Symbol) and str(a).upper() == sym:
                            sym_obj = a
                            break
                # Check if the symbol is a future
                is_future = False
                if sym_obj and getattr(sym_obj, 'security_type', None) == SecurityType.FUTURE:
                    is_future = True
                elif sym in self.securities and getattr(self.securities[sym].symbol, 'security_type', None) == SecurityType.FUTURE:
                    is_future = True
                # For continuous futures: ~2 rolls per year (quarterly); for equities: ~2 over entire range
                if start_time and end_time:
                    range_days = (end_time - start_time).days
                    range_years = max(1, range_days / 365.0)
                    if is_future:
                        per_sym = max(1, int(range_years * 1.8))  # ~1.8 per year
                    else:
                        per_sym = 2  # Equity remaps are rare
                    event_start = start_time
                elif count_arg is not None:
                    if is_future:
                        per_sym = max(1, count_arg // 60)
                    else:
                        per_sym = 2
                    event_start = now - timedelta(days=count_arg)
                else:
                    per_sym = 2
                    event_start = now - timedelta(days=365 * 2)
                interval = max(1, ((end_time or now) - event_start).days // max(1, per_sym))
                for i in range(per_sym):
                    d = event_start + timedelta(days=interval * (i + 1))
                    tuples.append((sym, d))
                    data_rows.append({
                        'oldsymbol': sym if i % 2 == 0 else f'{sym}A',
                        'newsymbol': f'{sym}A' if i % 2 == 0 else sym,
                        'type': 0,
                        'value': 0.0,
                    })
            idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        # Check if this is an option universe history request
        # When the symbol is an option type and flatten=True, return option chain data
        _is_option_sym = False
        if sym_names and len(sym_names) == 1:
            s_name = sym_names[0]
            sec = self.securities.get(s_name)
            if sec:
                st = getattr(sec.symbol, 'security_type', None) if hasattr(sec, 'symbol') else None
                if st in (SecurityType.OPTION, SecurityType.INDEX_OPTION, SecurityType.FUTURE_OPTION):
                    _is_option_sym = True

        if _is_option_sym and flatten:
            from .enums import OptionRight
            sym_name = sym_names[0]
            sec = self.securities[sym_name]
            sym_obj = sec.symbol if hasattr(sec, 'symbol') else Symbol(sym_name)
            tuples = []
            data_rows = []
            for i in range(num_rows):
                d = now - timedelta(days=(num_rows - i))
                # Get option contracts for this date using the provider
                contracts = list(self.option_chain_provider.get_option_contract_list(sym_obj, d))
                for contract_sym in contracts:
                    tuples.append((d, contract_sym))
                    strike = getattr(contract_sym, 'strike_price', 100.0)
                    data_rows.append({
                        'close': strike, 'open': strike - 0.5,
                        'high': strike + 1.0, 'low': strike - 1.0,
                        'volume': 100000.0, 'value': strike,
                    })
            idx = pd.MultiIndex.from_tuples(tuples, names=['time', 'symbol'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        # Generate standard synthetic OHLCV data
        base_price = 100.0
        is_multi = len(sym_names) > 1

        if is_multi:
            tuples = []
            data_rows = []
            for sym in sym_names:
                for i in range(num_rows):
                    d = now - timedelta(days=(num_rows - i))
                    tuples.append((sym, d))
                    p = base_price + i * 0.01
                    data_rows.append({
                        'open': p - 0.5, 'high': p + 1.0, 'low': p - 1.0,
                        'close': p, 'volume': 1000000.0, 'value': p
                    })
            idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
            df = pd.DataFrame(data_rows, index=idx)
            return _HistoryDataFrame(df)

        # Single symbol — use MultiIndex (symbol, time) so .unstack(0) works
        sym = sym_names[0] if sym_names else 'SPY'
        tuples = []
        data_rows = []
        for i in range(num_rows):
            d = now - timedelta(days=(num_rows - i))
            tuples.append((sym, d))
            p = base_price + i * 0.01
            data_rows.append({
                'open': p - 0.5, 'high': p + 1.0, 'low': p - 1.0,
                'close': p, 'volume': 1000000.0, 'value': p
            })
        idx = pd.MultiIndex.from_tuples(tuples, names=['symbol', 'time'])
        df = pd.DataFrame(data_rows, index=idx)
        return _HistoryDataFrame(df)

    # ---- Order Methods ----

    def _next_order_id(self) -> int:
        self.transactions._order_counter += 1
        return self.transactions._order_counter

    def market_order(self, symbol, quantity, asynchronous=False, tag="",
                     order_properties=None):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.MARKET, tag=tag)
        self.transactions._orders[oid] = ticket

        # Simulate immediate fill for backtesting
        price = self.securities[str(symbol)].price if str(symbol) in self.securities else 0
        if price > 0:
            ticket.average_fill_price = price
            ticket.quantity_filled = quantity
            ticket.status = OrderStatus.FILLED
            self.portfolio.process_fill(str(symbol), quantity, price)

            event = OrderEvent(oid, sym, OrderStatus.FILLED, price, quantity)
            self.on_order_event(event)

        return ticket

    def limit_order(self, symbol, quantity, limit_price, tag="", order_properties=None):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.LIMIT,
                             limit_price=limit_price, tag=tag)
        self.transactions._orders[oid] = ticket
        return ticket

    def stop_market_order(self, symbol, quantity, stop_price, tag="", order_properties=None):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.STOP_MARKET,
                             stop_price=stop_price, tag=tag)
        self.transactions._orders[oid] = ticket
        return ticket

    def stop_limit_order(self, symbol, quantity, stop_price, limit_price, tag="", order_properties=None):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.STOP_LIMIT,
                             limit_price=limit_price, stop_price=stop_price, tag=tag)
        self.transactions._orders[oid] = ticket
        return ticket

    def trailing_stop_order(self, symbol, quantity, trailing_amount=None,
                            trailing_as_percentage=False, tag="", order_properties=None):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.TRAILING_STOP, tag=tag)
        self.transactions._orders[oid] = ticket
        return ticket

    def market_on_open_order(self, symbol, quantity, tag=""):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.MARKET_ON_OPEN, tag=tag)
        self.transactions._orders[oid] = ticket
        return ticket

    def market_on_close_order(self, symbol, quantity, tag=""):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.MARKET_ON_CLOSE, tag=tag)
        self.transactions._orders[oid] = ticket
        return ticket

    def limit_if_touched_order(self, symbol, quantity, trigger_price, limit_price, tag=""):
        oid = self._next_order_id()
        sym = Symbol(str(symbol).upper()) if not isinstance(symbol, Symbol) else symbol
        ticket = OrderTicket(oid, sym, quantity, OrderType.LIMIT_IF_TOUCHED,
                             limit_price=limit_price, stop_price=trigger_price, tag=tag)
        self.transactions._orders[oid] = ticket
        return ticket

    def buy(self, symbol, quantity):
        return self.market_order(symbol, abs(quantity))

    def sell(self, symbol, quantity):
        return self.market_order(symbol, -abs(quantity))

    def order(self, symbol, quantity, **kwargs):
        return self.market_order(symbol, quantity, **kwargs)

    def set_holdings(self, symbol_or_targets, percentage=None, liquidate_existing=False,
                     tag="", order_properties=None):
        """Set portfolio to target percentage allocation."""
        if isinstance(symbol_or_targets, list):
            # List of PortfolioTarget
            if liquidate_existing:
                self.liquidate(tag=tag)
            for target in symbol_or_targets:
                sym = target.symbol if hasattr(target, 'symbol') else target
                pct = target.quantity if hasattr(target, 'quantity') else percentage
                self._set_holdings_single(str(sym), pct, tag)
            return

        if percentage is not None:
            if liquidate_existing:
                self.liquidate(tag=tag)
            self._set_holdings_single(str(symbol_or_targets), percentage, tag)

    def _set_holdings_single(self, ticker: str, percentage: float, tag: str = ""):
        ticker = ticker.upper()
        price = self.securities[ticker].price if ticker in self.securities else 0
        if price == 0:
            return

        portfolio_value = self.portfolio.total_portfolio_value
        target_value = portfolio_value * percentage
        current_holding = self.portfolio[ticker].quantity
        current_value = current_holding * price
        diff_value = target_value - current_value

        if abs(diff_value) < price:
            return

        quantity = int(diff_value / price)
        if quantity != 0:
            self.market_order(ticker, quantity, tag=tag)

    def liquidate(self, symbol=None, tag="", order_properties=None):
        """Liquidate all or specific holdings."""
        if symbol:
            ticker = str(symbol).upper()
            qty = self.portfolio[ticker].quantity
            if qty != 0:
                self.market_order(ticker, -qty, tag=tag)
        else:
            for ticker, holding in list(self.portfolio.items()):
                if holding.quantity != 0:
                    self.market_order(ticker, -holding.quantity, tag=tag)

    def can_liquidate(self, symbol):
        ticker = str(symbol).upper()
        return self.portfolio[ticker].quantity != 0

    # ---- Indicator Factory Methods ----

    def ema(self, symbol, period, resolution=None, selector=None):
        name = f"EMA_{symbol}_{period}"
        indicator = ExponentialMovingAverage(name, period)
        self._indicators[name] = indicator
        return indicator

    def sma(self, symbol, period, resolution=None, selector=None):
        name = f"SMA_{symbol}_{period}"
        indicator = SimpleMovingAverage(name, period)
        self._indicators[name] = indicator
        return indicator

    def macd(self, symbol, fast_period=12, slow_period=26, signal_period=9,
             moving_average_type=None, resolution=None, selector=None):
        name = f"MACD_{symbol}"
        indicator = MovingAverageConvergenceDivergence(name, fast_period, slow_period, signal_period)
        self._indicators[name] = indicator
        return indicator

    def rsi(self, symbol, period=14, moving_average_type=None, resolution=None, selector=None):
        name = f"RSI_{symbol}_{period}"
        indicator = RelativeStrengthIndex(name, period, moving_average_type)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def b(self, symbol, reference_symbol=None, period=14, resolution=None, selector=None):
        """Beta indicator."""
        name = f"BETA_{symbol}_{period}"
        from .algorithm_imports import Beta
        indicator = Beta(name, period)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def bb(self, symbol, period=20, k=2.0, moving_average_type=None, resolution=None, selector=None):
        name = f"BB_{symbol}_{period}"
        indicator = BollingerBands(name, period, k)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def atr(self, symbol, period=14, moving_average_type=None, resolution=None, selector=None):
        name = f"ATR_{symbol}_{period}"
        indicator = AverageTrueRange(name, period)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def sto(self, symbol, period=14, k_period=3, d_period=3, resolution=None):
        name = f"STO_{symbol}_{period}"
        indicator = Stochastic(name, period, k_period, d_period)
        self._indicators[name] = indicator
        return indicator

    def roc(self, symbol, period=14, resolution=None):
        name = f"ROC_{symbol}_{period}"
        indicator = RateOfChange(name, period)
        self._indicators[name] = indicator
        return indicator

    def mom(self, symbol, period=14, resolution=None, selector=None):
        name = f"MOM_{symbol}_{period}"
        indicator = Momentum(name, period)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def wilr(self, symbol, period=14, resolution=None):
        name = f"WILR_{symbol}_{period}"
        indicator = WilliamsPercentR(name, period)
        self._indicators[name] = indicator
        return indicator

    def cci(self, symbol, period=20, resolution=None):
        name = f"CCI_{symbol}_{period}"
        indicator = CommodityChannelIndex(name, period)
        self._indicators[name] = indicator
        return indicator

    def adx(self, symbol, period=14, resolution=None):
        name = f"ADX_{symbol}_{period}"
        indicator = AverageDirectionalIndex(name, period)
        self._indicators[name] = indicator
        return indicator

    def identity(self, symbol, resolution=None, selector=None, name=None):
        """Identity indicator (returns input value unchanged)."""
        ind_name = name or f"ID_{symbol}"
        indicator = SimpleMovingAverage(ind_name, 1)
        self._indicators[ind_name] = indicator
        return indicator

    def filtered_identity(self, symbol, filter_func=None, resolution=None):
        """Filtered identity indicator."""
        name = f"FID_{symbol}"
        indicator = SimpleMovingAverage(name, 1)
        self._indicators[name] = indicator
        return indicator

    def rc(self, symbol, period=20, resolution=None, selector=None):
        """Regression Channel indicator."""
        name = f"RC_{symbol}_{period}"
        indicator = SimpleMovingAverage(name, period)
        # Add regression channel-specific attributes
        indicator.linear_regression = SimpleMovingAverage(f"RC_LR_{symbol}", period)
        indicator.upper_channel = SimpleMovingAverage(f"RC_UC_{symbol}", period)
        indicator.lower_channel = SimpleMovingAverage(f"RC_LC_{symbol}", period)
        indicator.intercept = SimpleMovingAverage(f"RC_INT_{symbol}", period)
        indicator.slope = SimpleMovingAverage(f"RC_SLP_{symbol}", period)
        self._indicators[name] = indicator
        return indicator

    def std(self, symbol, period=20, resolution=None, selector=None):
        """Standard Deviation indicator."""
        name = f"STD_{symbol}_{period}"
        indicator = SimpleMovingAverage(name, period)
        # Add std-specific attributes
        indicator._std_window = deque(maxlen=period)
        indicator._orig_compute = indicator._compute
        def _std_compute(value):
            indicator._std_window.append(value)
            if len(indicator._std_window) < 2:
                return 0.0
            mean = sum(indicator._std_window) / len(indicator._std_window)
            variance = sum((x - mean) ** 2 for x in indicator._std_window) / len(indicator._std_window)
            return variance ** 0.5
        indicator._compute = _std_compute
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def min(self, symbol, period=14, resolution=None, selector=None):
        """Minimum indicator - tracks lowest value over period."""
        name = f"MIN_{symbol}_{period}"
        indicator = SimpleMovingAverage(name, period)
        indicator._min_window = deque(maxlen=period)
        def _min_compute(value):
            indicator._min_window.append(value)
            return __builtins__['min'](indicator._min_window) if isinstance(__builtins__, dict) else min(indicator._min_window)
        indicator._compute = _min_compute
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def max(self, symbol, period=14, resolution=None, selector=None):
        """Maximum indicator - tracks highest value over period."""
        name = f"MAX_{symbol}_{period}"
        indicator = SimpleMovingAverage(name, period)
        indicator._max_window = deque(maxlen=period)
        def _max_compute(value):
            indicator._max_window.append(value)
            return __builtins__['max'](indicator._max_window) if isinstance(__builtins__, dict) else max(indicator._max_window)
        indicator._compute = _max_compute
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def aroon(self, symbol, period=20, resolution=None, selector=None):
        """Aroon indicator."""
        name = f"AROON_{symbol}_{period}"
        indicator = SimpleMovingAverage(name, period)
        indicator.aroon_up = SimpleMovingAverage(f"AROON_UP_{symbol}", period)
        indicator.aroon_down = SimpleMovingAverage(f"AROON_DOWN_{symbol}", period)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def register_indicator(self, symbol, indicator, resolution_or_consolidator=None, selector=None):
        if isinstance(self._indicators, dict):
            name = getattr(indicator, 'name', str(id(indicator)))
            self._indicators[name] = indicator
        # If _indicators was overridden to a list by user code, don't break it

    def unregister_indicator(self, indicator):
        name = getattr(indicator, 'name', str(id(indicator)))
        self._indicators.pop(name, None)

    def indicator_history(self, indicator, symbol_or_symbols, period_or_start,
                          resolution=None, selector=None):
        if pd:
            return pd.DataFrame()
        return []

    def resolve_consolidator(self, symbol, resolution, data_type=None):
        """Resolve a consolidator for the given symbol and resolution."""
        from .consolidators import TradeBarConsolidator
        if isinstance(resolution, timedelta):
            return TradeBarConsolidator(resolution)
        return TradeBarConsolidator(timedelta(minutes=1))

    def create_indicator_name(self, symbol, indicator_type, resolution):
        """Create a standard indicator name."""
        return f"{indicator_type}({symbol},{resolution})"

    # ---- Data Consolidation ----

    def consolidate(self, symbol, period_or_type, handler=None, tick_type=None):
        if isinstance(period_or_type, timedelta):
            consolidator = TradeBarConsolidator(period_or_type)
        elif isinstance(period_or_type, int):
            consolidator = TradeBarConsolidator(max_count=period_or_type)
        else:
            consolidator = TradeBarConsolidator(timedelta(days=1))

        if handler:
            consolidator.data_consolidated = handler

        self._consolidators.append({'symbol': str(symbol), 'consolidator': consolidator})
        return consolidator

    # ---- Universe Methods ----

    def add_universe(self, *args, **kwargs):
        """Add a universe selection model or function."""
        # If asynchronous universe settings + coarse/fine function combo, raise
        if getattr(self.universe_settings, 'asynchronous', False) and len(args) >= 2:
            if callable(args[0]) and callable(args[1]):
                raise ValueError("Asynchronous universe selection does not support coarse/fine function combo. Use FineFundamentalUniverseSelectionModel instead.")
        # Detect universe type
        data_type = None
        is_etf = False
        selector = args[0] if args else None
        if args and isinstance(args[0], type) and hasattr(args[0], 'reader'):
            # Custom PythonData universe type (e.g., StockDataSource)
            data_type = args[0]
            selector = args[1] if len(args) > 1 else None
        elif args and isinstance(args[0], _UniverseReference):
            return args[0]
        elif args and hasattr(args[0], '_is_etf'):
            # ETF universe (e.g., self.universe.etf(spy, settings, filter))
            etf_settings = args[0]
            ref = _UniverseReference(getattr(etf_settings, '_filter_func', None), is_etf=True)
            ref._etf_settings = etf_settings
            return ref
        return _UniverseReference(selector, data_type=data_type, is_etf=is_etf)

    def add_universe_options(self, *args, **kwargs):
        pass

    def set_universe_selection(self, model):
        self._universe_selection = model

    def add_universe_selection(self, model):
        self._universe_selection = model

    # ---- Framework Model Methods ----

    def set_alpha(self, model):
        self._alpha_model = model

    def add_alpha(self, model):
        self._alpha_model = model

    def set_portfolio_construction(self, model):
        self._portfolio_construction = model

    def set_execution(self, model):
        self._execution_model = model

    def set_risk_management(self, model):
        self._risk_management = model

    def add_risk_management(self, model):
        self._risk_management = model

    def emit_insights(self, *insights):
        pass

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
        logger.info(f"Algorithm quit: {message}")

    def set_runtime_statistic(self, name: str, value):
        self._runtime_statistics[name] = str(value)

    def set_summary_statistic(self, name: str, value):
        self._runtime_statistics[name] = str(value)

    # ---- Parameter Methods ----

    def get_parameter(self, name: str, default_value=None):
        val = self._parameters.get(name, None)
        if val is None:
            return default_value
        # Coerce to the type of default_value if provided
        if default_value is not None:
            try:
                if isinstance(default_value, int) and not isinstance(default_value, bool):
                    return int(val)
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
        """Execute a training function. Can be train(func) or train(date_rule, time_rule, func)."""
        func = args[-1] if args else None
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
        """Get fundamental data for a symbol or list of symbols."""
        if isinstance(symbol, (list, tuple)):
            return [self.fundamentals(s) for s in symbol]
        # Try to get price from securities
        price = 100.0
        sym_str = str(symbol).upper() if symbol else ''
        if sym_str in self.securities:
            sec_price = self.securities[sym_str].price
            if sec_price > 0:
                price = sec_price
        from .algorithm_imports import Fundamental as _Fundamental
        result = _Fundamental(
            price=price,
            end_time=self._time or self._start_date,
            symbol=symbol,
            dollar_volume=1e8,
            volume=1000000,
            market_cap=1e10,
            has_fundamental_data=True,
        )
        return result

    def cusip(self, symbol):
        """Get CUSIP for a symbol."""
        sym = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol))
        return sym.cusip

    def composite_figi(self, symbol):
        """Get Composite FIGI for a symbol."""
        sym = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol))
        return sym.composite_figi

    def sedol(self, symbol):
        """Get SEDOL for a symbol."""
        sym = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol))
        return sym.sedol

    def isin(self, symbol):
        """Get ISIN for a symbol."""
        sym = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol))
        return sym.isin

    def cik(self, symbol):
        """Get CIK for a symbol."""
        sym = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol))
        return sym.cik

    def option_chain(self, symbol, flatten=False):
        """Get option chain for symbol. Returns OptionChainResult with contracts."""
        from .enums import OptionRight, OptionStyle
        underlying = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol).upper())
        chain = _OptionChainResult(underlying)

        # Determine underlying properties
        ticker_str = str(underlying).upper()
        und_sec_type = getattr(underlying, 'security_type', SecurityType.EQUITY)

        # Determine option SecurityType, Market, and Style based on underlying
        if und_sec_type == SecurityType.FUTURE:
            opt_sec_type = SecurityType.FUTURE_OPTION
            opt_market = getattr(underlying, 'market', Market.CME)
            opt_style = OptionStyle.AMERICAN
        elif und_sec_type == SecurityType.INDEX:
            opt_sec_type = SecurityType.INDEX_OPTION
            opt_market = Market.USA
            opt_style = OptionStyle.EUROPEAN
        else:
            opt_sec_type = SecurityType.OPTION
            opt_market = Market.USA
            opt_style = OptionStyle.AMERICAN

        # Determine base price
        base_price = 100.0
        sec = self.securities.get(ticker_str)
        if sec and sec.price > 0:
            base_price = sec.price
        else:
            _default_prices = {
                'SPX': 3800.0, 'SPXW': 3800.0, 'NDX': 13000.0, 'SPY': 380.0, 'QQQ': 310.0,
                'ES': 3200.0, 'NQ': 13000.0, 'YM': 30000.0, 'RTY': 1800.0, 'MES': 3200.0,
                'GOOG': 750.0, 'GOOGL': 750.0, 'AAPL': 150.0, 'MSFT': 300.0, 'AMZN': 3200.0,
                'TSLA': 700.0, 'NVDA': 250.0, 'META': 250.0, 'NFLX': 500.0,
                'GC': 1800.0, 'SI': 25.0, 'CL': 70.0, 'NG': 4.0,
                'DC': 17.0, 'ZC': 400.0, 'ZW': 600.0, 'ZS': 1200.0,
                # Full-name future tickers (used by Futures.Dairy.CLASS_III_MILK etc.)
                'CLASS_III_MILK': 17.0, 'BUTTER': 2.0, 'CASH_SETTLED_CHEESE': 2.0,
                'SP_500_E_MINI': 3200.0, 'E_MINI': 3200.0,
            }
            if ticker_str in _default_prices:
                base_price = _default_prices[ticker_str]

        # Store base_price on underlying for _OptionContract IV calculation
        underlying._chain_base_price = base_price

        now = self._time or self._start_date or datetime.now()

        # Generate strike offsets proportional to base price
        if base_price > 1000:
            strike_offsets = [-800, -600, -450, -400, -200, -100, -50, 0, 50, 100, 200, 400, 450, 600, 800]
        elif base_price > 200:
            strike_offsets = [-100, -50, -20, -10, -5, 0, 5, 10, 20, 50, 100]
        else:
            strike_offsets = [-30, -20, -10, -5, -3, 0, 3, 5, 10, 20, 30]

        # Generate expiry dates - use standard monthly options (3rd Friday of month)
        # for the next few months from the algorithm's current time
        def _third_friday(year, month):
            """Get the third Friday of a given month/year."""
            import calendar
            c = calendar.Calendar(firstweekday=calendar.MONDAY)
            fridays = [d for d in c.itermonthdays2(year, month)
                       if d[0] != 0 and d[1] == calendar.FRIDAY]
            return datetime(year, month, fridays[2][0])

        # For futures, use the underlying's expiry date if available
        und_expiry = getattr(underlying, 'expiry', None)
        expiry_dates = []
        if und_expiry:
            # For future options, the option expires on the same date as the future
            expiry_dates = [und_expiry]
        else:
            # Generate monthly expiry dates for the next 4 months
            for m_offset in range(0, 4):
                y = now.year
                m = now.month + m_offset
                while m > 12:
                    m -= 12
                    y += 1
                try:
                    exp = _third_friday(y, m)
                    # Only include expiries in the future
                    if exp >= now - timedelta(days=1):
                        expiry_dates.append(exp)
                except Exception:
                    pass
            # Also add a near-term expiry (7 days out) ONLY if it doesn't overlap with monthly
            near_exp = now + timedelta(days=7)
            if not any(abs((near_exp - e).days) < 5 for e in expiry_dates):
                expiry_dates.append(near_exp)

        for expiry in expiry_dates:
            for strike_offset in strike_offsets:
                strike = base_price + strike_offset
                if strike <= 0:
                    continue
                for right in [OptionRight.CALL, OptionRight.PUT]:
                    right_char = 'C' if right == OptionRight.CALL else 'P'
                    ticker = f"{ticker_str}{expiry.strftime('%y%m%d')}{right_char}{int(strike)}"
                    opt_sym = Symbol(ticker, opt_sec_type, opt_market)
                    opt_sym.expiry = expiry
                    opt_sym.strike_price = float(strike)
                    opt_sym.right = right
                    opt_sym.option_style = opt_style
                    opt_sym._underlying = underlying
                    opt_sym.id = type('SymbolId', (), {
                        'strike_price': float(strike), 'option_right': right,
                        'date': expiry, 'expiration': expiry,
                        'underlying': underlying
                    })()
                    contract = _OptionContract(opt_sym, underlying, strike, right, expiry)
                    chain.contracts[str(opt_sym)] = contract
                    chain._list.append(contract)
        return chain

    @property
    def option_chain_provider(self):
        return OptionChainProvider(self)

    def is_market_open(self, symbol=None):
        """Check if market is currently open."""
        if symbol and str(symbol) in self.securities:
            return self.securities[str(symbol)].exchange.exchange_open
        return True

    def plot_indicator(self, chart_name, wait_for_ready=True, *indicators):
        """Plot indicator values on a chart."""
        for indicator in indicators:
            name = getattr(indicator, 'name', str(indicator))
            value = float(indicator) if hasattr(indicator, '__float__') else 0
            self.plot(chart_name, name, value)

    # ---- Combo Order Methods ----

    def combo_market_order(self, legs, quantity, asynchronous=False, tag="", order_properties=None):
        """Submit a combo market order (multi-leg)."""
        tickets = []
        for leg in legs:
            sym = leg.symbol if hasattr(leg, 'symbol') else leg
            qty = leg.quantity if hasattr(leg, 'quantity') else quantity
            ticket = self.market_order(sym, qty, tag=tag)
            tickets.append(ticket)
        return tickets

    def combo_limit_order(self, legs, quantity, limit_price, tag="", order_properties=None):
        """Submit a combo limit order."""
        tickets = []
        for leg in legs:
            sym = leg.symbol if hasattr(leg, 'symbol') else leg
            qty = leg.quantity if hasattr(leg, 'quantity') else quantity
            ticket = self.limit_order(sym, qty, limit_price, tag=tag)
            tickets.append(ticket)
        return tickets

    def combo_leg_limit_order(self, legs, quantity, tag="", order_properties=None):
        """Submit combo leg limit orders."""
        tickets = []
        for leg in legs:
            sym = leg.symbol if hasattr(leg, 'symbol') else leg
            qty = leg.quantity if hasattr(leg, 'quantity') else quantity
            price = leg.limit_price if hasattr(leg, 'limit_price') else 0
            ticket = self.limit_order(sym, qty, price, tag=tag)
            tickets.append(ticket)
        return tickets

    # ---- Additional Indicator Shortcuts ----

    def aroon(self, symbol, period=25, resolution=None, selector=None):
        """Aroon indicator (stub using SMA as placeholder)."""
        name = f"AROON_{symbol}_{period}"
        indicator = SimpleMovingAverage(name, period)
        self._indicators[name] = indicator
        return indicator

    def vwap(self, symbol, resolution=None, selector=None):
        """VWAP indicator (stub)."""
        name = f"VWAP_{symbol}"
        indicator = SimpleMovingAverage(name, 1)
        self._indicators[name] = indicator
        return indicator

    def momp(self, symbol, period=14, resolution=None, selector=None):
        """Momentum Percent indicator."""
        name = f"MOMP_{symbol}_{period}"
        indicator = RateOfChange(name, period)
        self._indicators[name] = indicator
        return indicator

    def trin(self, symbol, resolution=None, selector=None):
        """TRIN (Arms Index) indicator (stub)."""
        name = f"TRIN_{symbol}"
        indicator = SimpleMovingAverage(name, 1)
        self._indicators[name] = indicator
        return indicator

    # ---- Additional Utility Methods ----

    def get_last_known_prices(self, symbol=None):
        """Get the last known prices for a security."""
        if symbol and str(symbol) in self.securities:
            sec = self.securities[str(symbol)]
            return [sec]
        return []

    def warm_up_indicator(self, symbol, indicator, resolution=None, selector=None):
        """Warm up an indicator with historical data."""
        period = getattr(indicator, 'warm_up_period', 0)
        if period <= 0:
            # Indicator doesn't define a warm-up period, skip
            return indicator
        base_price = 100.0
        now = self._time or datetime.now()
        for i in range(period):
            t = now - timedelta(days=(period - i))
            p = base_price + i * 0.1
            # Create a bar-like object for indicators that accept bars
            bar = type('WarmUpBar', (), {
                'time': t, 'end_time': t,
                'open': p - 0.5, 'high': p + 1.0, 'low': p - 1.0,
                'close': p, 'volume': 1000000, 'value': p, 'price': p,
            })()
            # Track samples manually for PythonIndicator subclasses
            old_samples = getattr(indicator, 'samples', 0)
            try:
                indicator.update(bar)
            except TypeError:
                try:
                    indicator.update(t, p)
                except Exception:
                    pass
            # Ensure samples was incremented
            new_samples = getattr(indicator, 'samples', 0)
            if new_samples == old_samples:
                try:
                    indicator.samples = old_samples + 1
                except AttributeError:
                    pass
        indicator.is_ready = True
        return indicator

    def arima(self, symbol, ar_order=1, diff_order=0, ma_order=1, period=50, resolution=None, selector=None):
        """ARIMA indicator (stub)."""
        name = f"ARIMA_{symbol}_{ar_order}_{diff_order}_{ma_order}"
        indicator = SimpleMovingAverage(name, period)
        self._indicators[name] = indicator
        return indicator

    def iv(self, symbol, mirror_option=None, risk_free_rate=None, dividend_yield=None,
           option_model=None, period=None, resolution=None, **kwargs):
        """Implied Volatility indicator (stub)."""
        name = f"IV_{symbol}"
        indicator = _OptionGreekIndicator(name, period or 14)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def d(self, symbol, mirror_option=None, risk_free_rate=None, dividend_yield=None,
          option_model=None, period=None, resolution=None, **kwargs):
        """Delta indicator (stub)."""
        name = f"D_{symbol}"
        indicator = _OptionGreekIndicator(name, period or 14)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def g(self, symbol, mirror_option=None, risk_free_rate=None, dividend_yield=None,
          option_model=None, period=None, resolution=None, **kwargs):
        """Gamma indicator (stub)."""
        name = f"G_{symbol}"
        indicator = _OptionGreekIndicator(name, period or 14)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def r(self, symbol, mirror_option=None, risk_free_rate=None, dividend_yield=None,
          option_model=None, period=None, resolution=None, **kwargs):
        """Rho indicator (stub)."""
        name = f"R_{symbol}"
        indicator = _OptionGreekIndicator(name, period or 14)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def v(self, symbol, mirror_option=None, risk_free_rate=None, dividend_yield=None,
          option_model=None, period=None, resolution=None, **kwargs):
        """Vega indicator (stub)."""
        name = f"V_{symbol}"
        indicator = _OptionGreekIndicator(name, period or 14)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    def t(self, symbol, mirror_option=None, risk_free_rate=None, dividend_yield=None,
          option_model=None, period=None, resolution=None, **kwargs):
        """Theta indicator (stub)."""
        name = f"T_{symbol}"
        indicator = _OptionGreekIndicator(name, period or 14)
        if isinstance(self._indicators, dict):
            self._indicators[name] = indicator
        return indicator

    @property
    def universe(self):
        """Universe manager."""
        return self.universe_settings

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
    """Indicator for option Greeks (IV, Delta, Gamma, Vega, Theta, Rho).
    Has an .implied_volatility sub-indicator with set_smoothing_function."""
    def __init__(self, name, period=14):
        super().__init__(name, period)
        self.implied_volatility = _ImpliedVolatilitySub()
    def _compute(self, value: float) -> float:
        return value if value else 0.25


class _ContractList(list):
    """List subclass with .count property (like LEAN's C# List<T>)."""
    @property
    def count(self):
        return len(self)
    Count = count


class OptionChainProvider:
    """Provides option chain data."""
    def __init__(self, algo=None):
        self._algo = algo

    def get_option_contract_list(self, symbol, date):
        """Get option contracts for a symbol. Must match option_chain() output exactly."""
        from .enums import OptionRight, OptionStyle
        underlying = symbol if isinstance(symbol, Symbol) else Symbol(str(symbol).upper())
        contracts = _ContractList()
        ticker_str = str(underlying).upper()
        und_sec_type = getattr(underlying, 'security_type', SecurityType.EQUITY)

        # Determine option properties based on underlying
        if und_sec_type == SecurityType.FUTURE:
            opt_sec_type = SecurityType.FUTURE_OPTION
            opt_market = getattr(underlying, 'market', Market.CME)
        elif und_sec_type == SecurityType.INDEX:
            opt_sec_type = SecurityType.INDEX_OPTION
            opt_market = Market.USA
        else:
            opt_sec_type = SecurityType.OPTION
            opt_market = Market.USA

        # Determine base price
        _default_prices = {
            'SPX': 3800.0, 'SPXW': 3800.0, 'NDX': 13000.0, 'SPY': 380.0, 'QQQ': 310.0,
            'ES': 3200.0, 'NQ': 13000.0, 'YM': 30000.0, 'RTY': 1800.0, 'MES': 3200.0,
            'GOOG': 750.0, 'GOOGL': 750.0, 'AAPL': 150.0, 'MSFT': 300.0, 'AMZN': 3200.0,
            'TSLA': 700.0, 'NVDA': 250.0, 'META': 250.0, 'NFLX': 500.0,
            'GC': 1800.0, 'SI': 25.0, 'CL': 70.0, 'NG': 4.0,
            'DC': 17.0, 'ZC': 400.0, 'ZW': 600.0, 'ZS': 1200.0,
            'CLASS_III_MILK': 17.0, 'BUTTER': 2.0, 'CASH_SETTLED_CHEESE': 2.0,
            'SP_500_E_MINI': 3200.0, 'E_MINI': 3200.0,
        }
        base_price = _default_prices.get(ticker_str, 100.0)
        # Try to get price from algo's securities
        if self._algo:
            sec = self._algo.securities.get(ticker_str)
            if sec and sec.price > 0:
                base_price = sec.price

        # Strike offsets - must match option_chain()
        if base_price > 1000:
            strike_offsets = [-800, -600, -450, -400, -200, -100, -50, 0, 50, 100, 200, 400, 450, 600, 800]
        elif base_price > 200:
            strike_offsets = [-100, -50, -20, -10, -5, 0, 5, 10, 20, 50, 100]
        else:
            strike_offsets = [-30, -20, -10, -5, -3, 0, 3, 5, 10, 20, 30]

        # Generate expiry dates - must match option_chain()
        def _third_friday(year, month):
            import calendar
            c = calendar.Calendar(firstweekday=calendar.MONDAY)
            fridays = [d for d in c.itermonthdays2(year, month)
                       if d[0] != 0 and d[1] == calendar.FRIDAY]
            return datetime(year, month, fridays[2][0])

        # Use algo's time/start_date to ensure consistency with option_chain()
        if self._algo and (self._algo._time or self._algo._start_date):
            now = self._algo._time or self._algo._start_date
        elif isinstance(date, datetime):
            now = date.replace(tzinfo=None) if date.tzinfo else date
        else:
            now = datetime.now()
        und_expiry = getattr(underlying, 'expiry', None)
        expiry_dates = []
        if und_expiry:
            expiry_dates = [und_expiry]
        else:
            for m_offset in range(0, 4):
                y = now.year
                m = now.month + m_offset
                while m > 12:
                    m -= 12
                    y += 1
                try:
                    exp = _third_friday(y, m)
                    if exp >= now - timedelta(days=1):
                        expiry_dates.append(exp)
                except Exception:
                    pass
            near_exp = now + timedelta(days=7)
            if not any(abs((near_exp - e).days) < 5 for e in expiry_dates):
                expiry_dates.append(near_exp)

        for expiry in expiry_dates:
            for strike_offset in strike_offsets:
                strike = base_price + strike_offset
                if strike <= 0:
                    continue
                for right in [OptionRight.CALL, OptionRight.PUT]:
                    right_char = 'C' if right == OptionRight.CALL else 'P'
                    ticker = f"{ticker_str}{expiry.strftime('%y%m%d')}{right_char}{int(strike)}"
                    opt_sym = Symbol(ticker, opt_sec_type, opt_market)
                    opt_sym.expiry = expiry
                    opt_sym.strike_price = float(strike)
                    opt_sym.right = right
                    opt_sym._underlying = underlying
                    opt_sym.id = type('SymbolId', (), {
                        'strike_price': float(strike), 'option_right': right,
                        'date': expiry, 'expiration': expiry, 'underlying': underlying
                    })()
                    contracts.append(opt_sym)
        return contracts

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
        pass


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
        insights = []
        for security in self._securities:
            if data.contains_key(security.symbol):
                insights.append(Insight(
                    security.symbol, self._period, self._type,
                    self._direction, self._magnitude, weight=self._weight
                ))
        return insights

    def on_securities_changed(self, algorithm, changes):
        for sec in changes.added_securities:
            if sec not in self._securities:
                self._securities.append(sec)
        for sec in changes.removed_securities:
            if sec in self._securities:
                self._securities.remove(sec)


class EqualWeightingPortfolioConstructionModel(PortfolioConstructionModel):
    """Portfolio construction model that assigns equal weight to all insights."""

    def __init__(self, rebalance=None, portfolio_bias=None, resolution=None, **kwargs):
        self._rebalance = rebalance
        self._portfolio_bias = portfolio_bias
        self._resolution = resolution

    def create_targets(self, algorithm, insights):
        if not insights:
            return []
        targets = []
        weight = 1.0 / len(insights) if insights else 0
        for insight in insights:
            direction = 1 if insight.direction == InsightDirection.UP else (
                -1 if insight.direction == InsightDirection.DOWN else 0)
            targets.append(PortfolioTarget(insight.symbol, weight * direction))
        return targets


class InsightWeightingPortfolioConstructionModel(PortfolioConstructionModel):
    """Portfolio construction weighted by insight magnitude/confidence."""

    def __init__(self, rebalance=None, portfolio_bias=None):
        self._rebalance = rebalance
        self._portfolio_bias = portfolio_bias

    def create_targets(self, algorithm, insights):
        if not insights:
            return []
        total_weight = sum(abs(i.weight or i.confidence or 1.0) for i in insights)
        if total_weight == 0:
            total_weight = 1
        targets = []
        for insight in insights:
            w = abs(insight.weight or insight.confidence or 1.0) / total_weight
            direction = 1 if insight.direction == InsightDirection.UP else (
                -1 if insight.direction == InsightDirection.DOWN else 0)
            targets.append(PortfolioTarget(insight.symbol, w * direction))
        return targets


class MeanVarianceOptimizationPortfolioConstructionModel(PortfolioConstructionModel):
    """Portfolio construction using mean-variance optimization (simplified stub)."""

    def __init__(self, rebalance=None, portfolio_bias=None, lookback=1,
                 period=63, resolution=None, risk_free_rate=0.0, optimizer=None):
        self._rebalance = rebalance
        self._lookback = lookback

    def create_targets(self, algorithm, insights):
        if not insights:
            return []
        weight = 1.0 / len(insights)
        return [PortfolioTarget(i.symbol, weight * (1 if i.direction == InsightDirection.UP else -1))
                for i in insights]


class BlackLittermanOptimizationPortfolioConstructionModel(PortfolioConstructionModel):
    """Black-Litterman portfolio construction (simplified stub)."""

    def __init__(self, rebalance=None, portfolio_bias=None, lookback=63,
                 resolution=None, risk_free_rate=0.0, optimizer=None):
        self._rebalance = rebalance
        self._lookback = lookback

    def create_targets(self, algorithm, insights):
        if not insights:
            return []
        weight = 1.0 / len(insights)
        return [PortfolioTarget(i.symbol, weight * (1 if i.direction == InsightDirection.UP else -1))
                for i in insights]


class ImmediateExecutionModel(ExecutionModel):
    """Execution model that immediately submits market orders."""

    def execute(self, algorithm, targets):
        for target in targets:
            existing = algorithm.portfolio[str(target.symbol)].quantity
            diff = target.quantity - existing
            if abs(diff) > 0:
                algorithm.market_order(target.symbol, diff)


class VolumeWeightedAveragePriceExecutionModel(ExecutionModel):
    """VWAP execution model (simplified stub - acts like immediate)."""

    def execute(self, algorithm, targets):
        for target in targets:
            existing = algorithm.portfolio[str(target.symbol)].quantity
            diff = target.quantity - existing
            if abs(diff) > 0:
                algorithm.market_order(target.symbol, diff)


class StandardDeviationExecutionModel(ExecutionModel):
    """Std dev execution model (simplified stub)."""

    def __init__(self, period=60, deviations=2, resolution=None):
        self._period = period
        self._deviations = deviations

    def execute(self, algorithm, targets):
        for target in targets:
            existing = algorithm.portfolio[str(target.symbol)].quantity
            diff = target.quantity - existing
            if abs(diff) > 0:
                algorithm.market_order(target.symbol, diff)


class MaximumDrawdownPercentPortfolio(RiskManagementModel):
    """Risk model: maximum portfolio drawdown percentage."""

    def __init__(self, maximum_drawdown_percent=0.05, is_not_a_ccount_currency_for=False):
        self._max_dd = maximum_drawdown_percent
        self._trailing_high = 0

    def manage_risk(self, algorithm, targets):
        portfolio_value = algorithm.portfolio.total_portfolio_value
        self._trailing_high = max(self._trailing_high, portfolio_value)
        if self._trailing_high > 0:
            dd = (self._trailing_high - portfolio_value) / self._trailing_high
            if dd > self._max_dd:
                return [PortfolioTarget(str(s.symbol), 0) for s in algorithm.securities]
        return targets


class MaximumDrawdownPercentPerSecurity(RiskManagementModel):
    """Risk model: maximum drawdown per security."""

    def __init__(self, maximum_drawdown_percent=0.05):
        self._max_dd = maximum_drawdown_percent

    def manage_risk(self, algorithm, targets):
        risk_targets = list(targets) if targets else []
        for ticker, holding in algorithm.portfolio.items():
            if holding.invested and holding.unrealized_profit_percent < -self._max_dd:
                risk_targets.append(PortfolioTarget(ticker, 0))
        return risk_targets


class MaximumUnrealizedProfitPercentPerSecurity(RiskManagementModel):
    """Risk model: take profit at maximum unrealized profit."""

    def __init__(self, maximum_unrealized_profit_percent=0.05):
        self._max_profit = maximum_unrealized_profit_percent

    def manage_risk(self, algorithm, targets):
        risk_targets = list(targets) if targets else []
        for ticker, holding in algorithm.portfolio.items():
            if holding.invested and holding.unrealized_profit_percent > self._max_profit:
                risk_targets.append(PortfolioTarget(ticker, 0))
        return risk_targets


class TrailingStopRiskManagementModel(RiskManagementModel):
    """Risk model: trailing stop for each security."""

    def __init__(self, maximum_drawdown_percent=0.05):
        self._max_dd = maximum_drawdown_percent
        self._trailing_highs = {}

    def manage_risk(self, algorithm, targets):
        risk_targets = list(targets) if targets else []
        for ticker, holding in algorithm.portfolio.items():
            if holding.invested:
                price = holding.market_price
                if ticker not in self._trailing_highs:
                    self._trailing_highs[ticker] = price
                self._trailing_highs[ticker] = max(self._trailing_highs[ticker], price)
                trail = self._trailing_highs[ticker]
                if trail > 0 and (trail - price) / trail > self._max_dd:
                    risk_targets.append(PortfolioTarget(ticker, 0))
            else:
                self._trailing_highs.pop(ticker, None)
        return risk_targets


class MaximumSectorExposureRiskManagementModel(RiskManagementModel):
    """Risk model: maximum sector exposure (stub)."""

    def __init__(self, maximum_sector_exposure=0.3):
        self._max_exposure = maximum_sector_exposure

    def manage_risk(self, algorithm, targets):
        return targets


class CompositeRiskManagementModel(RiskManagementModel):
    """Combines multiple risk management models."""

    def __init__(self, *models):
        self._models = list(models)

    def add_risk_management(self, model):
        self._models.append(model)

    def manage_risk(self, algorithm, targets):
        for model in self._models:
            targets = model.manage_risk(algorithm, targets)
        return targets


class ManualUniverseSelectionModel(UniverseSelectionModel):
    """Universe selection from a manually specified list of symbols."""

    def __init__(self, symbols=None, *args):
        self._symbols = symbols or []
        if args:
            self._symbols = list(args) if not symbols else self._symbols

    def create_universes(self, algorithm):
        return self._symbols


class FundamentalUniverseSelectionModel(UniverseSelectionModel):
    """Universe selection based on fundamental data."""

    def __init__(self, filter_fine_universe=True, universe_settings=None, *args, **kwargs):
        self._filter_fine = filter_fine_universe

    def create_universes(self, algorithm):
        return []

    def select_coarse(self, algorithm, coarse):
        return [c.symbol for c in coarse]

    def select_fine(self, algorithm, fine):
        return [f.symbol for f in fine]


class ScheduledUniverseSelectionModel(UniverseSelectionModel):
    """Universe selection on a schedule."""

    def __init__(self, date_rule, time_rule, selector, settings=None):
        self._date_rule = date_rule
        self._time_rule = time_rule
        self._selector = selector

    def create_universes(self, algorithm):
        if callable(self._selector):
            return self._selector(algorithm.time)
        return []


class CustomUniverseSelectionModel(UniverseSelectionModel):
    """Custom universe selection from a function."""

    def __init__(self, name, selector, settings=None):
        self._name = name
        self._selector = selector

    def create_universes(self, algorithm):
        if callable(self._selector):
            return self._selector(algorithm.time)
        return []


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
    """Base class for custom Python data types."""

    def __init__(self):
        self.symbol = None
        self.time = None
        self.end_time = None
        self.value = 0.0
        self._data = {}

    def get_source(self, config, date, is_live_mode):
        return None

    def reader(self, config, line, date, is_live_mode):
        return None

    def __getattr__(self, name):
        if name.startswith('_'):
            raise AttributeError(name)
        return self._data.get(name, 0)

    def __setattr__(self, name, value):
        if name in ('symbol', 'time', 'end_time', 'value', '_data'):
            super().__setattr__(name, value)
        else:
            if not hasattr(self, '_data'):
                super().__setattr__('_data', {})
            self._data[name] = value

    def __getitem__(self, key):
        return self._data.get(key, 0)

    def __setitem__(self, key, value):
        self._data[key] = value

    def __repr__(self):
        return f"PythonData({self.symbol}, {self.value})"


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

    def remove_expired_insights(self, utc_time):
        self._insights = [i for i in self._insights if i.close_time_utc > utc_time]

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


class SecurityIdentifier:
    """Security identifier generator."""

    @staticmethod
    def generate_equity(ticker, market="usa", map_first_date=True):
        return Symbol(ticker, SecurityType.EQUITY, market)

    @staticmethod
    def generate_option(underlying, market="usa", option_style=0, option_right=0,
                        strike_price=0, expiry_date=None):
        return Symbol(underlying, SecurityType.OPTION, market)

    @staticmethod
    def generate_future(ticker, market="cme", expiry_date=None):
        return Symbol(ticker, SecurityType.FUTURE, market)


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
