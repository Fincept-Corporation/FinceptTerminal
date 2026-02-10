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

    def __getitem__(self, key):
        return self._store[key]

    def __setitem__(self, key, value):
        self._store[key] = value

    def __contains__(self, key):
        return key in self._store


class SubscriptionManager:
    """Manages data subscriptions."""

    def __init__(self):
        self._subscriptions = []

    def add_consolidator(self, symbol, consolidator):
        self._subscriptions.append({
            'symbol': str(symbol),
            'consolidator': consolidator
        })

    def remove_consolidator(self, symbol, consolidator):
        self._subscriptions = [
            s for s in self._subscriptions
            if not (str(s['symbol']) == str(symbol) and s['consolidator'] is consolidator)
        ]

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
        self._parameters: Dict[str, str] = {}
        self._warm_up_period = 0
        self._is_warming_up = False
        self._account_currency = "USD"
        self._brokerage_model = None
        self._algorithm_mode = "backtesting"
        self._deployment_target = "local"
        self._live_mode = False
        self._name = self.__class__.__name__

        # Object store
        self.object_store = ObjectStore()

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

    # ---- Initialization Methods ----

    def initialize(self):
        """Override this method to set up your algorithm."""
        pass

    def set_start_date(self, year_or_date, month=None, day=None):
        if isinstance(year_or_date, (datetime, date)):
            self._start_date = datetime.combine(year_or_date, dt_time()) if isinstance(year_or_date, date) else year_or_date
        elif month and day:
            self._start_date = datetime(year_or_date, month, day)

    def set_end_date(self, year_or_date, month=None, day=None):
        if isinstance(year_or_date, (datetime, date)):
            self._end_date = datetime.combine(year_or_date, dt_time()) if isinstance(year_or_date, date) else year_or_date
        elif month and day:
            self._end_date = datetime(year_or_date, month, day)

    def set_cash(self, amount_or_currency=None, amount=None):
        if isinstance(amount_or_currency, (int, float)):
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
                   leverage: float = 1.0, extended_market_hours: bool = False):
        symbol = Symbol(ticker.upper(), SecurityType.EQUITY, market)
        sec = self.securities.add(symbol, resolution, leverage)
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
        symbol = Symbol(str(contract).upper(), SecurityType.INDEX_OPTION, Market.USA)
        return self.securities.add(symbol, resolution or Resolution.MINUTE)

    def add_future_contract(self, contract, resolution=None):
        symbol = Symbol(str(contract).upper(), SecurityType.FUTURE, Market.CME)
        return self.securities.add(symbol, resolution or Resolution.MINUTE)

    def add_future_option(self, symbol, option_filter=None):
        sym = Symbol(str(symbol).upper(), SecurityType.FUTURE_OPTION, Market.CME)
        return self.securities.add(sym, Resolution.MINUTE)

    def add_future_option_contract(self, contract, resolution=None):
        symbol = Symbol(str(contract).upper(), SecurityType.FUTURE_OPTION, Market.CME)
        return self.securities.add(symbol, resolution or Resolution.MINUTE)

    def add_option_contract(self, contract, resolution=None):
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

    def add_data(self, data_type, ticker, resolution=None, *args, **kwargs):
        symbol = Symbol(ticker.upper() if isinstance(ticker, str) else str(ticker), SecurityType.BASE, Market.USA)
        return self.securities.add(symbol, resolution or Resolution.DAILY)

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

    def history(self, symbol_or_symbols, periods_or_start=None, resolution=None, **kwargs):
        """Get historical data. Returns pandas DataFrame when available."""
        if pd is None:
            return []
        # Return empty DataFrame with standard columns
        return pd.DataFrame(columns=['open', 'high', 'low', 'close', 'volume'])

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
             resolution=None, selector=None):
        name = f"MACD_{symbol}"
        indicator = MovingAverageConvergenceDivergence(name, fast_period, slow_period, signal_period)
        self._indicators[name] = indicator
        return indicator

    def rsi(self, symbol, period=14, resolution=None, selector=None):
        name = f"RSI_{symbol}_{period}"
        indicator = RelativeStrengthIndex(name, period)
        self._indicators[name] = indicator
        return indicator

    def bb(self, symbol, period=20, k=2.0, resolution=None, selector=None):
        name = f"BB_{symbol}_{period}"
        indicator = BollingerBands(name, period, k)
        self._indicators[name] = indicator
        return indicator

    def atr(self, symbol, period=14, resolution=None, selector=None):
        name = f"ATR_{symbol}_{period}"
        indicator = AverageTrueRange(name, period)
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

    def mom(self, symbol, period=14, resolution=None):
        name = f"MOM_{symbol}_{period}"
        indicator = Momentum(name, period)
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

    def register_indicator(self, symbol, indicator, resolution_or_consolidator=None, selector=None):
        name = getattr(indicator, 'name', str(id(indicator)))
        self._indicators[name] = indicator

    def unregister_indicator(self, indicator):
        name = getattr(indicator, 'name', str(id(indicator)))
        self._indicators.pop(name, None)

    def indicator_history(self, indicator, symbol_or_symbols, period_or_start,
                          resolution=None, selector=None):
        if pd:
            return pd.DataFrame()
        return []

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
        pass

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
        return self._parameters.get(name, default_value)

    def set_parameters(self, params: dict):
        self._parameters.update(params)

    # ---- Security Initializer ----

    def set_security_initializer(self, initializer):
        pass

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
        pass

    def option_chain(self, symbol):
        return []

    @property
    def option_chain_provider(self):
        return OptionChainProvider()

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
        """Warm up an indicator with historical data (stub)."""
        indicator.is_ready = True
        return indicator

    def arima(self, symbol, ar_order=1, diff_order=0, ma_order=1, period=50, resolution=None, selector=None):
        """ARIMA indicator (stub)."""
        name = f"ARIMA_{symbol}_{ar_order}_{diff_order}_{ma_order}"
        indicator = SimpleMovingAverage(name, period)
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


class OptionChainProvider:
    """Provides option chain data."""
    def get_option_contract_list(self, symbol, date):
        return []


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

    def __init__(self, insight_type=None, direction=None, period=None, magnitude=None, weight=None):
        self._type = insight_type or InsightType.PRICE
        self._direction = direction or InsightDirection.UP
        self._period = period or timedelta(days=1)
        self._magnitude = magnitude
        self._weight = weight
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

    def __init__(self, rebalance=None, portfolio_bias=None):
        self._rebalance = rebalance
        self._portfolio_bias = portfolio_bias

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

    def __init__(self, filter_fine_universe=True, universe_settings=None):
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

    DollarVolume = staticmethod(lambda x: getattr(x, 'dollar_volume', 0))
    Price = staticmethod(lambda x: getattr(x, 'price', 0))
    Volume = staticmethod(lambda x: getattr(x, 'volume', 0))


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
