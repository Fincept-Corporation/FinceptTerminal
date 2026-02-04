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

    def add_security(self, security_type, ticker, resolution=Resolution.MINUTE,
                     market=Market.USA, fill_forward=True, leverage=1.0,
                     extended_market_hours=False):
        symbol = Symbol(ticker.upper(), security_type, market)
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

    def train(self, func):
        """Execute a training function."""
        if callable(func):
            func()

    def add_command(self, command_type, handler=None):
        pass

    def option_chain(self, symbol):
        return []

    def option_chain_provider(self):
        return OptionChainProvider()

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
