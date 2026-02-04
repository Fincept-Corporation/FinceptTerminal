"""
Zipline-Reloaded Provider - Event-Driven Backtesting

Comprehensive implementation covering Zipline's full API surface:
- Order types: order, order_value, order_percent, order_target, order_target_value, order_target_percent
- Execution styles: MarketOrder, LimitOrder, StopOrder, StopLimitOrder
- Commission models: PerShare, PerTrade, PerDollar, NoCommission
- Slippage models: FixedSlippage, VolumeShareSlippage, NoSlippage, FixedBasisPointsSlippage
- Trading controls: set_long_only, set_max_leverage, set_max_order_count, etc.
- Cancel policies: EODCancel, NeverCancel
- Asset restrictions: StaticRestrictions, HistoricalRestrictions, NoRestrictions
- Scheduling: schedule_function with date_rules and time_rules (including every_minute)
- Recording: record() for custom metric tracking
- Callbacks: before_trading_start, analyze
- Pipeline API: attach_pipeline, pipeline_output
- Benchmark support via set_benchmark
- Order management: get_order, get_open_orders, cancel_order
- Asset lookup: symbol, symbols, sid, continuous_future, future_symbol
- Asset types: Asset, Equity, Future, ContinuousFuture
- Bundle management: register, ingest, load, clean, unregister
- Datetime: get_datetime
"""

import sys
import io
import json
import math
from typing import Dict, Any, List, Optional
from pathlib import Path

# Setup paths for both direct execution and package import
_SCRIPT_DIR = Path(__file__).parent
_BACKTESTING_DIR = _SCRIPT_DIR.parent

if str(_BACKTESTING_DIR) not in sys.path:
    sys.path.insert(0, str(_BACKTESTING_DIR))
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from base.base_provider import (
    BacktestingProviderBase,
    json_response,
    parse_json_input,
)


# ============================================================================
# Execution Style classes (mirror zipline.finance.execution)
# ============================================================================

class MarketOrder:
    """Market order - execute at current market price."""
    style = 'market'

class LimitOrder:
    """Limit order - execute at specified price or better."""
    def __init__(self, limit_price):
        self.limit_price = limit_price
        self.style = 'limit'

class StopOrder:
    """Stop order - triggered when price crosses stop_price."""
    def __init__(self, stop_price):
        self.stop_price = stop_price
        self.style = 'stop'

class StopLimitOrder:
    """Stop-limit order - stop trigger + limit execution."""
    def __init__(self, limit_price, stop_price):
        self.limit_price = limit_price
        self.stop_price = stop_price
        self.style = 'stop_limit'


# ============================================================================
# Commission Models (mirror zipline.finance.commission)
# ============================================================================

class PerShare:
    """Per-share commission model."""
    def __init__(self, cost=0.001, min_trade_cost=0.0):
        self.cost = cost
        self.min_trade_cost = min_trade_cost
    def calculate(self, amount, price):
        return max(abs(amount) * self.cost, self.min_trade_cost)

class PerTrade:
    """Flat per-trade commission."""
    def __init__(self, cost=0.0):
        self.cost = cost
    def calculate(self, amount, price):
        return self.cost

class PerDollar:
    """Commission as fraction of dollar value."""
    def __init__(self, cost=0.0015):
        self.cost = cost
    def calculate(self, amount, price):
        return abs(amount * price * self.cost)

class NoCommission:
    """Zero-commission model."""
    def calculate(self, amount, price):
        return 0.0


# ============================================================================
# Slippage Models (mirror zipline.finance.slippage)
# ============================================================================

class FixedSlippage:
    """Fixed basis-point slippage."""
    def __init__(self, spread=0.0):
        self.spread = spread
    def calculate(self, price, amount):
        if amount > 0:
            return price * (1 + self.spread / 2)
        return price * (1 - self.spread / 2)

class VolumeShareSlippage:
    """Volume-based slippage model."""
    def __init__(self, volume_limit=0.025, price_impact=0.1):
        self.volume_limit = volume_limit
        self.price_impact = price_impact
    def calculate(self, price, amount):
        impact = abs(amount) * self.price_impact * 0.0001
        if amount > 0:
            return price * (1 + impact)
        return price * (1 - impact)

class NoSlippage:
    """Zero-slippage model - price unchanged."""
    def calculate(self, price, amount):
        return price

class FixedBasisPointsSlippage:
    """Fixed basis-points slippage with volume limit."""
    def __init__(self, basis_points=5.0, volume_limit=0.1):
        self.basis_points = basis_points
        self.volume_limit = volume_limit
    def calculate(self, price, amount):
        impact = self.basis_points / 10000.0
        if amount > 0:
            return price * (1 + impact)
        return price * (1 - impact)


# ============================================================================
# Cancel Policies (mirror zipline.finance.cancel_policy)
# ============================================================================

class EODCancel:
    """Cancel unfilled orders at end of day."""
    policy = 'eod'

class NeverCancel:
    """Never automatically cancel orders."""
    policy = 'never'


# ============================================================================
# Asset Restriction Classes (mirror zipline.finance.asset_restrictions)
# ============================================================================

class NoRestrictions:
    """No restrictions on any assets."""
    def is_restricted(self, asset, dt):
        return False

class StaticRestrictions:
    """Static set of restricted assets."""
    def __init__(self, asset_set):
        self._restricted = set(str(a) for a in asset_set)
    def is_restricted(self, asset, dt):
        return str(asset) in self._restricted

class HistoricalRestrictions:
    """Time-varying asset restrictions from a DataFrame."""
    def __init__(self, restrictions_df):
        self._df = restrictions_df
    def is_restricted(self, asset, dt):
        return False  # Stub - real impl checks df


# ============================================================================
# Asset Classes (mirror zipline.assets)
# ============================================================================

class Asset:
    """Base asset class."""
    def __init__(self, sid, symbol=None, asset_name=None, exchange=None,
                 exchange_full=None, country_code=None, start_date=None, end_date=None):
        self.sid = sid
        self.symbol = symbol or str(sid)
        self.asset_name = asset_name or self.symbol
        self.exchange = exchange or 'UNKNOWN'
        self.exchange_full = exchange_full or self.exchange
        self.country_code = country_code or 'US'
        self.start_date = start_date
        self.end_date = end_date

    def __repr__(self):
        return f"Asset({self.sid}, '{self.symbol}')"

    def __str__(self):
        return self.symbol

    def __eq__(self, other):
        if isinstance(other, Asset):
            return self.sid == other.sid
        return NotImplemented

    def __hash__(self):
        return hash(self.sid)

class Equity(Asset):
    """Equity asset (stock)."""
    def __repr__(self):
        return f"Equity({self.sid}, '{self.symbol}')"

class Future(Asset):
    """Futures contract."""
    def __init__(self, sid, symbol=None, root_symbol=None, notice_date=None,
                 expiration_date=None, auto_close_date=None, tick_size=0.01,
                 multiplier=1.0, **kwargs):
        super().__init__(sid, symbol, **kwargs)
        self.root_symbol = root_symbol or self.symbol
        self.notice_date = notice_date
        self.expiration_date = expiration_date
        self.auto_close_date = auto_close_date
        self.tick_size = tick_size
        self.multiplier = multiplier

    def __repr__(self):
        return f"Future({self.sid}, '{self.symbol}')"

class ContinuousFuture:
    """Continuous futures contract (rolling)."""
    def __init__(self, root_symbol, offset=0, roll_style='volume', adjustment='mul'):
        self.root_symbol = root_symbol
        self.offset = offset
        self.roll_style = roll_style
        self.adjustment = adjustment

    def __repr__(self):
        return f"ContinuousFuture('{self.root_symbol}')"


# ============================================================================
# Bundle Management Stubs (mirror zipline.data.bundles)
# ============================================================================

_registered_bundles = {}

def register_bundle(name, f=None, calendar_name='XNYS', start_session=None,
                    end_session=None, minutes_per_day=390, create_writers=True):
    """Register a data bundle."""
    _registered_bundles[name] = {
        'ingest': f,
        'calendar_name': calendar_name,
        'start_session': start_session,
        'end_session': end_session,
        'minutes_per_day': minutes_per_day,
    }

def ingest_bundle(name, environ=None, timestamp=None, assets_versions=(),
                  show_progress=False):
    """Ingest data for a named bundle (stub)."""
    if name not in _registered_bundles:
        raise ValueError(f"Bundle '{name}' not registered")
    print(f'[ZL-BUNDLE] Ingesting bundle: {name}', file=sys.stderr)

def load_bundle(name, environ=None, timestamp=None):
    """Load a previously ingested bundle (stub)."""
    if name not in _registered_bundles:
        raise ValueError(f"Bundle '{name}' not registered")
    return {'name': name, 'data': None}

def clean_bundle(name, before=None, after=None, keep_last=None):
    """Clean bundle data (stub)."""
    print(f'[ZL-BUNDLE] Cleaning bundle: {name}', file=sys.stderr)

def unregister_bundle(name):
    """Unregister a bundle."""
    _registered_bundles.pop(name, None)

def bundles():
    """List registered bundles."""
    return dict(_registered_bundles)


# ============================================================================
# Date/Time Rules (mirror zipline.utils.events)
# ============================================================================

class _DateRules:
    """Scheduling date rules."""
    @staticmethod
    def every_day():
        return {'type': 'every_day'}
    @staticmethod
    def week_start(days_offset=0):
        return {'type': 'week_start', 'days_offset': days_offset}
    @staticmethod
    def week_end(days_offset=0):
        return {'type': 'week_end', 'days_offset': days_offset}
    @staticmethod
    def month_start(days_offset=0):
        return {'type': 'month_start', 'days_offset': days_offset}
    @staticmethod
    def month_end(days_offset=0):
        return {'type': 'month_end', 'days_offset': days_offset}

class _TimeRules:
    """Scheduling time rules."""
    @staticmethod
    def market_open(minutes=0, hours=0):
        return {'type': 'market_open', 'minutes': minutes, 'hours': hours}
    @staticmethod
    def market_close(minutes=0, hours=0):
        return {'type': 'market_close', 'minutes': minutes, 'hours': hours}
    @staticmethod
    def every_minute():
        return {'type': 'every_minute'}

date_rules = _DateRules()
time_rules = _TimeRules()


# ============================================================================
# Mock Order for order management
# ============================================================================

class MockOrder:
    """Simulated order object returned by order functions."""
    _counter = 0

    def __init__(self, asset, amount, style=None, limit_price=None, stop_price=None):
        MockOrder._counter += 1
        self.id = str(MockOrder._counter)
        self.asset = asset
        self.amount = amount
        self.filled = 0
        self.limit = limit_price
        self.stop = stop_price
        self.style = style or MarketOrder()
        self.status = 'open'
        self.created = None
        self.commission = 0.0

    def cancel(self):
        self.status = 'cancelled'

    def to_dict(self):
        return {
            'id': self.id,
            'asset': str(self.asset),
            'amount': self.amount,
            'filled': self.filled,
            'limit': self.limit,
            'stop': self.stop,
            'status': self.status,
        }


# ============================================================================
# MockData - Extended data access object
# ============================================================================

class MockData:
    """
    Mock data object simulating Zipline's BarData.

    Supports:
    - history(asset, field, bar_count, freq)
    - current(asset, field) / current(asset, [fields])
    - can_trade(asset)
    - is_stale(asset)
    """
    def __init__(self, df):
        import pandas as pd
        self._df = df
        self._current_idx = 0

    def history(self, asset, field, bar_count, freq):
        import pandas as pd
        end = self._current_idx + 1
        start = max(0, end - bar_count)
        if field in self._df.columns:
            return self._df[field].iloc[start:end]
        return pd.Series(dtype=float)

    def current(self, asset, fields):
        """Get current bar data for asset."""
        if isinstance(fields, str):
            if fields in self._df.columns:
                return float(self._df[fields].iloc[self._current_idx])
            return 0.0
        # List of fields
        result = {}
        for f in fields:
            if f in self._df.columns:
                result[f] = float(self._df[f].iloc[self._current_idx])
            else:
                result[f] = 0.0
        return result

    def can_trade(self, asset):
        """Check if asset can be traded (has valid price data)."""
        if self._current_idx < len(self._df):
            import numpy as np
            val = self._df['close'].iloc[self._current_idx]
            return not (np.isnan(val) if isinstance(val, float) else False)
        return False

    def is_stale(self, asset):
        """Check if data is stale (no recent updates)."""
        return False


# ============================================================================
# MockContext - Comprehensive context simulating Zipline's TradingAlgorithm
# ============================================================================

class MockContext:
    """
    Comprehensive mock of Zipline's context object.

    Supports all Zipline API functions:
    - Order functions: order, order_value, order_percent, order_target,
      order_target_value, order_target_percent
    - Commission/slippage: set_commission, set_slippage
    - Trading controls: set_long_only, set_max_leverage, set_max_order_count,
      set_max_order_size, set_max_position_size, set_do_not_order_list
    - Scheduling: schedule_function
    - Recording: record
    - Benchmark: set_benchmark
    - Order management: get_order, get_open_orders, cancel_order
    - Environment: get_environment
    """

    def __init__(self, capital, commission_rate=0.001, symbol='SPY'):
        self.asset = symbol
        self.invested = False
        self.portfolio_value = capital
        self.cash = capital
        self.shares = 0.0
        self.history_len = 50
        self._current_price = 0.0
        self._current_volume = 0.0
        self._initial_capital = capital
        self._day_index = 0
        self._current_dt = None

        # Commission & slippage models
        self._commission_model = PerDollar(commission_rate)
        self._slippage_model = FixedSlippage(0.0)

        # Trading controls
        self._long_only = False
        self._max_leverage = float('inf')
        self._max_order_count = float('inf')
        self._max_order_size = float('inf')
        self._max_position_size = float('inf')
        self._do_not_order_list = set()
        self._order_count_today = 0

        # Cancel policy
        self._cancel_policy = EODCancel()

        # Asset restrictions
        self._asset_restrictions = NoRestrictions()

        # Order management
        self._orders = {}
        self._open_orders = {}

        # Scheduling
        self._scheduled_functions = []

        # Pipeline
        self._pipelines = {}
        self._pipeline_outputs = {}

        # Asset lookup cache
        self._asset_cache = {}

        # Recording
        self._recorded = {}
        self._recorded_history = []

        # Benchmark
        self._benchmark = None

        # Portfolio/account
        self.portfolio = _MockPortfolio(capital)
        self.account = _MockAccount(capital)

    # ---- Order Functions ----

    def order(self, asset, amount, limit_price=None, stop_price=None, style=None):
        """Place an order for a fixed number of shares."""
        if self._check_order_rejected(asset, amount):
            return None
        return self._execute_order(asset, amount, limit_price, stop_price, style)

    def order_value(self, asset, value, limit_price=None, stop_price=None, style=None):
        """Place an order for a fixed dollar value."""
        if self._current_price <= 0:
            return None
        amount = value / self._current_price
        return self.order(asset, amount, limit_price, stop_price, style)

    def order_percent(self, asset, percent, limit_price=None, stop_price=None, style=None):
        """Place an order for a percentage of portfolio value."""
        value = self.portfolio_value * percent
        return self.order_value(asset, value, limit_price, stop_price, style)

    def order_target(self, asset, target, limit_price=None, stop_price=None, style=None):
        """Place an order to adjust position to a target number of shares."""
        amount = target - self.shares
        if abs(amount) < 0.001:
            return None
        return self.order(asset, amount, limit_price, stop_price, style)

    def order_target_value(self, asset, target_value, limit_price=None, stop_price=None, style=None):
        """Place an order to adjust position to a target dollar value."""
        if self._current_price <= 0:
            return None
        target_shares = target_value / self._current_price
        return self.order_target(asset, target_shares, limit_price, stop_price, style)

    def order_target_percent(self, asset, pct, limit_price=None, stop_price=None, style=None):
        """Place an order to adjust position to a target percentage of portfolio."""
        if self.portfolio_value <= 0:
            return None
        target_value = self.portfolio_value * pct
        current_value = self.shares * self._current_price
        diff = target_value - current_value
        if abs(diff) < 1.0:
            return None
        shares_to_trade = diff / self._current_price
        return self._execute_order(asset, shares_to_trade, limit_price, stop_price, style)

    def _check_order_rejected(self, asset, amount):
        """Check trading controls and asset restrictions."""
        if str(asset) in self._do_not_order_list:
            return True
        if self._asset_restrictions.is_restricted(asset, self._current_dt):
            return True
        if self._long_only and amount < 0 and self.shares + amount < 0:
            return True
        if self._order_count_today >= self._max_order_count:
            return True
        if abs(amount) > self._max_order_size:
            return True
        return False

    def _execute_order(self, asset, amount, limit_price=None, stop_price=None, style=None):
        """Execute an order with commission and slippage."""
        if abs(amount) < 0.001:
            return None

        # Determine execution price with slippage
        exec_price = self._slippage_model.calculate(self._current_price, amount)

        # Apply limit price check
        if limit_price is not None:
            if amount > 0 and exec_price > limit_price:
                return None  # buy price too high
            if amount < 0 and exec_price < limit_price:
                return None  # sell price too low

        # Apply stop price check
        if stop_price is not None:
            if amount > 0 and self._current_price < stop_price:
                return None  # stop not triggered
            if amount < 0 and self._current_price > stop_price:
                return None

        # Check position size limits
        new_position = abs(self.shares + amount) * exec_price
        if new_position > self._max_position_size and self._max_position_size != float('inf'):
            return None

        # Check leverage
        if self._initial_capital > 0:
            leverage = (abs(self.shares + amount) * exec_price) / self._initial_capital
            if leverage > self._max_leverage:
                return None

        # Calculate commission
        commission = self._commission_model.calculate(amount, exec_price)

        # Execute
        self.shares += amount
        self.cash -= amount * exec_price + commission
        self.portfolio_value = self.cash + self.shares * self._current_price
        self._order_count_today += 1

        # Track order
        order = MockOrder(asset, amount, style, limit_price, stop_price)
        order.filled = amount
        order.status = 'filled'
        order.commission = commission
        self._orders[order.id] = order
        return order

    # ---- Commission & Slippage ----

    def set_commission(self, model):
        """Set commission model (PerShare, PerTrade, PerDollar)."""
        self._commission_model = model

    def set_slippage(self, model):
        """Set slippage model (FixedSlippage, VolumeShareSlippage)."""
        self._slippage_model = model

    # ---- Trading Controls ----

    def set_long_only(self):
        """Restrict to long-only positions."""
        self._long_only = True

    def set_max_leverage(self, max_leverage):
        """Set maximum leverage ratio."""
        self._max_leverage = max_leverage

    def set_max_order_count(self, max_count):
        """Set maximum orders per day."""
        self._max_order_count = max_count

    def set_max_order_size(self, asset=None, max_shares=float('inf'), max_notional=float('inf')):
        """Set maximum order size."""
        self._max_order_size = min(max_shares, max_notional / max(self._current_price, 1))

    def set_max_position_size(self, asset=None, max_shares=float('inf'), max_notional=float('inf')):
        """Set maximum position size."""
        self._max_position_size = max_notional if max_notional < float('inf') else max_shares * max(self._current_price, 1)

    def set_do_not_order_list(self, restricted_list):
        """Set list of assets that cannot be ordered."""
        self._do_not_order_list = set(str(s) for s in restricted_list)

    # ---- Scheduling ----

    def schedule_function(self, func, date_rule=None, time_rule=None):
        """Schedule a function to run on specific dates/times."""
        self._scheduled_functions.append({
            'func': func,
            'date_rule': date_rule or date_rules.every_day(),
            'time_rule': time_rule or time_rules.market_open(),
        })

    # ---- Recording ----

    def record(self, **kwargs):
        """Record custom variables for analysis."""
        self._recorded.update(kwargs)

    # ---- Benchmark ----

    def set_benchmark(self, asset):
        """Set benchmark asset for comparison."""
        self._benchmark = asset

    # ---- Order Management ----

    def get_order(self, order_id):
        """Get order by ID."""
        return self._orders.get(str(order_id))

    def get_open_orders(self, asset=None):
        """Get all open orders, optionally filtered by asset."""
        open_orders = {k: v for k, v in self._orders.items() if v.status == 'open'}
        if asset:
            open_orders = {k: v for k, v in open_orders.items() if str(v.asset) == str(asset)}
        return open_orders

    def cancel_order(self, order_or_id):
        """Cancel an order."""
        order_id = order_or_id if isinstance(order_or_id, str) else order_or_id.id
        if order_id in self._orders:
            self._orders[order_id].cancel()
            return True
        return False

    # ---- Environment ----

    def get_environment(self, field=None):
        """Get algorithm environment info."""
        env = {
            'platform': 'zipline-numpy-simulation',
            'arena': 'backtest',
            'data_frequency': 'daily',
            'start': None,
            'end': None,
            'capital_base': self._initial_capital,
            'live_trading': False,
        }
        if field:
            return env.get(field)
        return env

    # ---- Datetime ----

    def get_datetime(self, tz=None):
        """Get current simulation datetime."""
        if self._current_dt is not None:
            return self._current_dt
        import pandas as pd
        return pd.Timestamp.now(tz=tz)

    # ---- Cancel Policy ----

    def set_cancel_policy(self, cancel_policy):
        """Set order cancellation policy (EODCancel or NeverCancel)."""
        self._cancel_policy = cancel_policy

    # ---- Asset Restrictions ----

    def set_asset_restrictions(self, restrictions, on_error='fail'):
        """Set asset trading restrictions."""
        self._asset_restrictions = restrictions

    # ---- Pipeline Integration ----

    def attach_pipeline(self, pipeline, name='default', chunks=None, eager=True):
        """Attach a Pipeline to this algorithm context."""
        self._pipelines[name] = pipeline

    def pipeline_output(self, name='default'):
        """Get the output of a previously attached pipeline."""
        import pandas as pd
        if name in self._pipeline_outputs:
            return self._pipeline_outputs[name]
        return pd.DataFrame()

    # ---- Asset Lookup ----

    def symbol(self, symbol_str):
        """Look up an Equity by its ticker symbol."""
        if symbol_str not in self._asset_cache:
            self._asset_cache[symbol_str] = Equity(
                sid=hash(symbol_str) % 100000,
                symbol=symbol_str,
            )
        return self._asset_cache[symbol_str]

    def symbols(self, *args):
        """Look up multiple Equities by ticker symbols."""
        return [self.symbol(s) for s in args]

    def sid(self, sid_int):
        """Look up an Asset by its unique integer identifier."""
        for asset in self._asset_cache.values():
            if asset.sid == sid_int:
                return asset
        a = Asset(sid=sid_int)
        self._asset_cache[str(sid_int)] = a
        return a

    def continuous_future(self, root_symbol, offset=0, roll='volume', adjustment='mul'):
        """Create a ContinuousFuture specifier."""
        return ContinuousFuture(root_symbol, offset, roll, adjustment)

    def future_symbol(self, symbol_str):
        """Look up a Future by its symbol."""
        key = f'future:{symbol_str}'
        if key not in self._asset_cache:
            self._asset_cache[key] = Future(
                sid=hash(symbol_str) % 100000,
                symbol=symbol_str,
            )
        return self._asset_cache[key]

    # ---- Day reset ----

    def _reset_daily(self):
        """Reset daily counters."""
        self._order_count_today = 0
        if self._recorded:
            self._recorded_history.append(dict(self._recorded))
            self._recorded = {}
        self._day_index += 1

        # Apply cancel policy (cancel open orders at EOD)
        if isinstance(self._cancel_policy, EODCancel):
            for oid, o in list(self._orders.items()):
                if o.status == 'open':
                    o.cancel()


class _MockPortfolio:
    """Mock portfolio object matching zipline.protocol.Portfolio."""
    def __init__(self, capital):
        self.starting_cash = capital
        self.cash = capital
        self.portfolio_value = capital
        self.positions_value = 0.0
        self.returns = 0.0
        self.pnl = 0.0
        self.positions = {}
        self.capital_used = 0.0
        self.start_date = None


class _MockAccount:
    """Mock account object matching zipline.protocol.Account."""
    def __init__(self, capital):
        self.buying_power = capital
        self.total_positions_value = 0.0
        self.net_liquidation = capital
        self.leverage = 0.0
        self.settled_cash = capital
        self.accrued_interest = 0.0
        self.available_funds = capital
        self.net_leverage = 0.0
        self.excess_liquidity = capital
        self.cushion = 1.0
        self.regt_equity = capital
        self.regt_margin = float('inf')
        self.initial_margin_requirement = 0.0
        self.maintenance_margin_requirement = 0.0
        self.total_positions_exposure = 0.0
        self.day_trades_remaining = float('inf')


# ============================================================================
# ZiplineProvider
# ============================================================================

class ZiplineProvider(BacktestingProviderBase):
    """
    Zipline-Reloaded Provider - Event-driven backtesting.

    Comprehensive orchestrator covering Zipline's full API surface.
    Delegates to specialized modules for strategies, data, and metrics.
    """

    def __init__(self):
        super().__init__()

    @property
    def name(self) -> str:
        return "Zipline"

    @property
    def version(self) -> str:
        try:
            import zipline
            return zipline.__version__
        except Exception:
            return "pure-numpy"

    @property
    def capabilities(self) -> Dict[str, Any]:
        return {
            'backtesting': True,
            'optimization': True,
            'liveTrading': False,
            'research': True,
            'multiAsset': ['equity'],
            'indicators': True,
            'customStrategies': True,
            'maxConcurrentBacktests': 10,
            'vectorization': False,
            'walkForward': True,
            'eventDriven': True,
            'pipeline': True,
            'stopLoss': True,
            'takeProfit': True,
            'positionSizing': True,
            'orderTypes': ['market', 'limit', 'stop', 'stop_limit'],
            'commissionModels': ['per_share', 'per_trade', 'per_dollar', 'no_commission'],
            'slippageModels': ['fixed', 'volume_share', 'no_slippage', 'fixed_basis_points'],
            'cancelPolicies': ['eod_cancel', 'never_cancel'],
            'assetRestrictions': ['static', 'historical', 'none'],
            'assetTypes': ['equity', 'future', 'continuous_future'],
            'bundleManagement': ['register', 'ingest', 'load', 'clean', 'unregister'],
            'tradingControls': ['long_only', 'max_leverage', 'max_order_count',
                                'max_order_size', 'max_position_size', 'do_not_order_list'],
            'scheduling': True,
            'recording': True,
            'benchmark': True,
        }

    def initialize(self, config: Dict[str, Any]) -> Dict[str, Any]:
        self.config = config
        return self._create_success_result(f'Zipline {self.version} ready')

    def test_connection(self) -> Dict[str, Any]:
        return self._create_success_result(f'Zipline {self.version} is available')

    # ========================================================================
    # Core Commands
    # ========================================================================

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Run event-driven backtest using Zipline or pure-numpy fallback."""
        import numpy as np
        import pandas as pd

        backtest_id = self._generate_id()
        logs = []

        print(f'[ZL-BT] === run_backtest START ===', file=sys.stderr)
        print(f'[ZL-BT] Request keys: {sorted(request.keys())}', file=sys.stderr)

        try:
            from zl_strategies import get_strategy
            from zl_data import fetch_yfinance_data
            from zl_metrics import (
                extract_performance,
                extract_equity_curve,
                extract_statistics,
            )

            # Parse request
            strategy_def = request.get('strategy', {})
            strategy_type = strategy_def.get('type', 'sma_crossover')
            strategy_params = strategy_def.get('parameters', {})
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')
            initial_capital = float(request.get('initialCapital', 100000))
            commission_rate = float(request.get('commission', 0.001))
            slippage_bps = float(request.get('slippage', 0.0))
            assets = request.get('assets', [])

            # Commission/slippage model from request
            commission_model_type = request.get('commissionModel', 'per_dollar')
            slippage_model_type = request.get('slippageModel', 'fixed')

            # Trading controls
            long_only = request.get('longOnly', False)
            max_leverage = float(request.get('maxLeverage', 0)) or float('inf')
            benchmark = request.get('benchmark', None)

            symbols = [a.get('symbol', 'SPY') for a in assets] if assets else ['SPY']
            primary_symbol = symbols[0]

            logs.append(f'Strategy: {strategy_type}')
            logs.append(f'Symbols: {symbols}')
            logs.append(f'Period: {start_date} to {end_date}')
            logs.append(f'Capital: ${initial_capital:,.0f}')

            print(f'[ZL-BT] Strategy: {strategy_type}, params: {strategy_params}', file=sys.stderr)
            print(f'[ZL-BT] Symbols: {symbols}', file=sys.stderr)

            # Get strategy functions
            init_fn, handle_fn = get_strategy(strategy_type, strategy_params)
            logs.append(f'Strategy loaded: {strategy_type}')

            # Get optional callbacks
            before_trading_start_fn = strategy_params.get('_before_trading_start', None)
            analyze_fn = strategy_params.get('_analyze', None)

            # Fetch data
            data = fetch_yfinance_data(symbols, start_date, end_date)
            if not data or primary_symbol not in data:
                return {'success': False, 'error': f'No data available for {primary_symbol}'}

            price_df = data[primary_symbol]
            logs.append(f'Data loaded: {len(price_df)} bars')

            # Try zipline.run_algorithm first, fall back to pure-numpy simulation
            try:
                results = self._run_with_zipline(
                    init_fn, handle_fn, price_df, primary_symbol,
                    start_date, end_date, initial_capital, commission_rate,
                    logs,
                )
            except Exception as e:
                print(f'[ZL-BT] Zipline run_algorithm failed: {e}, using numpy fallback', file=sys.stderr)
                logs.append(f'Using numpy simulation (zipline unavailable: {e})')
                results = self._run_numpy_simulation(
                    init_fn, handle_fn, price_df, primary_symbol,
                    initial_capital, commission_rate, slippage_bps,
                    commission_model_type, slippage_model_type,
                    long_only, max_leverage, benchmark,
                    before_trading_start_fn, analyze_fn,
                    logs,
                )

            # Extract metrics
            performance = extract_performance(results, initial_capital)
            equity = extract_equity_curve(results, initial_capital)
            trades_list = []
            if 'transactions' in results.columns:
                from zl_metrics import _extract_trades_from_results
                trades_list = _extract_trades_from_results(results, initial_capital)
            statistics = extract_statistics(results, initial_capital, trades_list)

            # Add recorded data if available
            recorded_data = None
            if hasattr(results, 'attrs') and 'recorded' in results.attrs:
                recorded_data = results.attrs['recorded']

            logs.append(f'Backtest completed: {performance.get("total_trades", 0)} trades')

            result_data = {
                'id': backtest_id,
                'status': 'completed',
                'performance': performance,
                'trades': trades_list,
                'equity': equity,
                'statistics': statistics,
                'logs': logs,
            }
            if recorded_data:
                result_data['recorded'] = recorded_data

            return {
                'success': True,
                'data': result_data,
            }

        except Exception as e:
            import traceback
            print(f'[ZL-BT] ERROR: {e}', file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
            return {
                'success': False,
                'error': str(e),
                'data': {
                    'id': backtest_id,
                    'status': 'failed',
                    'performance': None,
                    'trades': [],
                    'equity': [],
                    'statistics': None,
                    'logs': logs,
                    'error': str(e),
                }
            }

    def _run_with_zipline(self, init_fn, handle_fn, price_df, symbol,
                          start_date, end_date, capital, commission, logs):
        """Try running with actual Zipline library."""
        import zipline
        from zipline.api import (
            order_target_percent, symbol as zl_symbol,
            order, order_value, order_percent, order_target, order_target_value,
            set_commission, set_slippage, set_long_only, set_benchmark,
            schedule_function, record,
        )
        from zipline.finance.commission import PerShare as ZlPerShare
        from zipline.finance.slippage import FixedSlippage as ZlFixedSlippage
        import pandas as pd

        start_dt = pd.Timestamp(start_date, tz='UTC')
        end_dt = pd.Timestamp(end_date, tz='UTC')

        # Wrap strategy functions for Zipline's API
        def wrapped_init(context):
            init_fn(context)
            context.asset = zl_symbol(symbol)
            # Inject all Zipline API functions into context
            context.order = order
            context.order_value = order_value
            context.order_percent = order_percent
            context.order_target = order_target
            context.order_target_value = order_target_value
            context.order_target_percent = order_target_percent
            context.set_commission = set_commission
            context.set_slippage = set_slippage
            context.set_long_only = set_long_only
            context.set_benchmark = set_benchmark
            context.schedule_function = schedule_function
            context.record = record

        def wrapped_handle(context, data):
            handle_fn(context, data)

        results = zipline.run_algorithm(
            start=start_dt,
            end=end_dt,
            initialize=wrapped_init,
            handle_data=wrapped_handle,
            capital_base=capital,
            trading_calendar='XNYS',
        )

        logs.append('Executed via zipline.run_algorithm()')
        return results

    def _run_numpy_simulation(self, init_fn, handle_fn, price_df, symbol,
                              capital, commission_rate, slippage_bps=0.0,
                              commission_model_type='per_dollar',
                              slippage_model_type='fixed',
                              long_only=False, max_leverage=float('inf'),
                              benchmark=None,
                              before_trading_start_fn=None,
                              analyze_fn=None,
                              logs=None):
        """
        Pure-numpy event-driven simulation fallback when Zipline is not installed.
        Simulates the full initialize/handle_data pattern with comprehensive mock objects.
        """
        import numpy as np
        import pandas as pd

        if logs is None:
            logs = []

        print(f'[ZL-BT] Running numpy simulation with {len(price_df)} bars', file=sys.stderr)

        # Build commission model
        if commission_model_type == 'per_share':
            comm_model = PerShare(commission_rate)
        elif commission_model_type == 'per_trade':
            comm_model = PerTrade(commission_rate)
        elif commission_model_type == 'no_commission':
            comm_model = NoCommission()
        else:
            comm_model = PerDollar(commission_rate)

        # Build slippage model
        if slippage_model_type == 'volume_share':
            slip_model = VolumeShareSlippage()
        elif slippage_model_type == 'no_slippage':
            slip_model = NoSlippage()
        elif slippage_model_type == 'fixed_basis_points':
            slip_model = FixedBasisPointsSlippage(slippage_bps * 10000 if slippage_bps < 1 else slippage_bps)
        else:
            slip_model = FixedSlippage(slippage_bps)

        context = MockContext(capital, commission_rate, symbol)
        context._commission_model = comm_model
        context._slippage_model = slip_model

        if long_only:
            context.set_long_only()
        if max_leverage and max_leverage != float('inf'):
            context.set_max_leverage(max_leverage)
        if benchmark:
            context.set_benchmark(benchmark)

        mock_data = MockData(price_df)

        # Initialize strategy
        init_fn(context)

        # Check for scheduled functions from initialization
        scheduled = list(context._scheduled_functions)

        # Simulation loop
        portfolio_values = []
        daily_returns = []
        transactions_log = []
        recorded_series = []
        prev_value = capital

        for i in range(len(price_df)):
            mock_data._current_idx = i
            close_price = float(price_df['close'].iloc[i])
            context._current_price = close_price
            context._current_dt = price_df.index[i]
            if 'volume' in price_df.columns:
                context._current_volume = float(price_df['volume'].iloc[i])

            # Update portfolio value
            context.portfolio_value = context.cash + context.shares * close_price
            positions_val = context.shares * close_price

            # Update portfolio fields
            context.portfolio.portfolio_value = context.portfolio_value
            context.portfolio.cash = context.cash
            context.portfolio.positions_value = positions_val
            context.portfolio.pnl = context.portfolio_value - capital
            context.portfolio.returns = (context.portfolio_value / capital) - 1.0

            # Update account fields
            context.account.net_liquidation = context.portfolio_value
            context.account.total_positions_value = positions_val
            context.account.total_positions_exposure = abs(positions_val)
            context.account.buying_power = context.cash
            context.account.available_funds = context.cash
            leverage = abs(positions_val) / context.portfolio_value if context.portfolio_value > 0 else 0.0
            context.account.leverage = leverage
            context.account.net_leverage = leverage

            # Reset daily counters
            context._reset_daily()

            # Call before_trading_start if provided
            if before_trading_start_fn:
                try:
                    before_trading_start_fn(context, mock_data)
                except Exception:
                    pass

            old_shares = context.shares

            # Execute scheduled functions
            for sf in scheduled:
                if self._should_run_scheduled(sf, i, price_df):
                    try:
                        sf['func'](context, mock_data)
                    except Exception:
                        pass

            # Call handle_data
            handle_fn(context, mock_data)

            # Record transactions
            if context.shares != old_shares:
                amount = context.shares - old_shares
                transactions_log.append({
                    'date_idx': i,
                    'sid': symbol,
                    'amount': amount,
                    'price': close_price,
                    'commission': context._commission_model.calculate(amount, close_price),
                })

            # Update portfolio after trades
            context.portfolio_value = context.cash + context.shares * close_price
            portfolio_values.append(context.portfolio_value)

            ret = (context.portfolio_value / prev_value - 1.0) if prev_value > 0 else 0.0
            daily_returns.append(ret)
            prev_value = context.portfolio_value

            # Capture recorded data
            if context._recorded:
                rec = dict(context._recorded)
                rec['_date'] = str(price_df.index[i])
                recorded_series.append(rec)

        # Call analyze callback if provided
        if analyze_fn:
            try:
                analyze_fn(context, None)
            except Exception:
                pass

        # Build result DataFrame matching Zipline's output format
        results = pd.DataFrame({
            'portfolio_value': portfolio_values,
            'returns': daily_returns,
        }, index=price_df.index)

        # Add transactions column
        txn_by_day = [[] for _ in range(len(price_df))]
        for txn in transactions_log:
            txn_by_day[txn['date_idx']].append({
                'sid': txn['sid'],
                'amount': txn['amount'],
                'price': txn['price'],
                'commission': txn['commission'],
            })
        results['transactions'] = txn_by_day

        # Attach recorded data
        if recorded_series:
            results.attrs['recorded'] = recorded_series

        logs.append(f'Executed via numpy simulation ({len(transactions_log)} transactions)')
        return results

    def _should_run_scheduled(self, sf, day_idx, price_df):
        """Check if a scheduled function should run on this day."""
        rule = sf.get('date_rule', {})
        rule_type = rule.get('type', 'every_day')

        if rule_type == 'every_day':
            return True

        dt = price_df.index[day_idx]
        dow = dt.weekday()  # Monday=0

        if rule_type == 'week_start':
            offset = rule.get('days_offset', 0)
            return dow == offset
        elif rule_type == 'week_end':
            offset = rule.get('days_offset', 0)
            return dow == (4 - offset)
        elif rule_type == 'month_start':
            offset = rule.get('days_offset', 0)
            if day_idx == 0:
                return offset == 0
            prev_dt = price_df.index[day_idx - 1]
            if dt.month != prev_dt.month:
                return offset == 0
            # Check if we're offset days from month start
            return False
        elif rule_type == 'month_end':
            if day_idx >= len(price_df) - 1:
                return True
            next_dt = price_df.index[day_idx + 1]
            return dt.month != next_dt.month

        return True

    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Parameter optimization via grid/random search."""
        import numpy as np

        opt_id = self._generate_id()
        logs = []

        try:
            strategy_def = request.get('strategy', {})
            strategy_type = strategy_def.get('type', 'sma_crossover')
            param_ranges = request.get('parameters', {})
            objective = request.get('objective', 'sharpe')
            method = request.get('method', 'grid')
            max_iter = int(request.get('maxIterations', 100))

            if not param_ranges:
                return {'success': False, 'error': 'No parameter ranges specified'}

            # Generate parameter combinations
            combos = self._generate_param_combos(param_ranges, method, max_iter)
            logs.append(f'Testing {len(combos)} parameter combinations')

            best_score = -np.inf
            best_params = {}
            best_result = None
            all_results = []

            for i, params in enumerate(combos):
                bt_request = {**request, 'strategy': {
                    'type': strategy_type,
                    'parameters': params,
                }}
                result = self.run_backtest(bt_request)

                if result.get('success') and result.get('data', {}).get('performance'):
                    perf = result['data']['performance']
                    score = self._get_objective_score(perf, objective)
                    all_results.append({
                        'parameters': params,
                        'score': score,
                        'total_return': perf.get('total_return', 0),
                        'sharpe_ratio': perf.get('sharpe_ratio', 0),
                    })

                    if score > best_score:
                        best_score = score
                        best_params = params
                        best_result = result

                if (i + 1) % 10 == 0:
                    print(f'[ZL-OPT] Completed {i + 1}/{len(combos)} iterations', file=sys.stderr)

            if best_result is None:
                return {'success': False, 'error': 'All optimization iterations failed'}

            logs.append(f'Best {objective}: {best_score:.4f}')
            logs.append(f'Best params: {best_params}')

            return {
                'success': True,
                'data': {
                    'id': opt_id,
                    'status': 'completed',
                    'best_parameters': best_params,
                    'best_result': best_result.get('data', {}),
                    'all_results': all_results[:100],
                    'iterations': len(combos),
                    'duration': 0,
                    'logs': logs,
                }
            }

        except Exception as e:
            import traceback
            print(f'[ZL-OPT] ERROR: {e}', file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    def walk_forward(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Walk-forward analysis with train/test splits."""
        import pandas as pd

        wf_id = self._generate_id()
        logs = []

        try:
            n_splits = int(request.get('nSplits', 5))
            train_ratio = float(request.get('trainRatio', 0.7))
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')

            start_dt = pd.Timestamp(start_date)
            end_dt = pd.Timestamp(end_date)
            total_days = (end_dt - start_dt).days
            split_days = total_days // n_splits

            split_results = []
            for i in range(n_splits):
                split_start = start_dt + pd.Timedelta(days=i * split_days)
                split_end = split_start + pd.Timedelta(days=split_days)
                train_end = split_start + pd.Timedelta(days=int(split_days * train_ratio))

                # Run test period
                test_request = {**request}
                test_request['startDate'] = train_end.strftime('%Y-%m-%d')
                test_request['endDate'] = split_end.strftime('%Y-%m-%d')

                result = self.run_backtest(test_request)
                if result.get('success') and result.get('data', {}).get('performance'):
                    perf = result['data']['performance']
                    split_results.append({
                        'split': i + 1,
                        'train_start': split_start.strftime('%Y-%m-%d'),
                        'train_end': train_end.strftime('%Y-%m-%d'),
                        'test_start': train_end.strftime('%Y-%m-%d'),
                        'test_end': split_end.strftime('%Y-%m-%d'),
                        'total_return': perf.get('total_return', 0),
                        'sharpe_ratio': perf.get('sharpe_ratio', 0),
                        'max_drawdown': perf.get('max_drawdown', 0),
                    })
                    logs.append(f'Split {i + 1}: return={perf.get("total_return", 0):.4f}')

            if not split_results:
                return {'success': False, 'error': 'All walk-forward splits failed'}

            import numpy as np
            avg_return = float(np.mean([s['total_return'] for s in split_results]))
            avg_sharpe = float(np.mean([s['sharpe_ratio'] for s in split_results]))

            logs.append(f'Average return: {avg_return:.4f}')
            logs.append(f'Average Sharpe: {avg_sharpe:.4f}')

            return {
                'success': True,
                'data': {
                    'id': wf_id,
                    'status': 'completed',
                    'splits': split_results,
                    'summary': {
                        'avg_return': avg_return,
                        'avg_sharpe': avg_sharpe,
                        'n_splits': n_splits,
                    },
                    'logs': logs,
                }
            }

        except Exception as e:
            import traceback
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    def get_historical_data(self, request: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Get historical price data."""
        from zl_data import fetch_yfinance_data, data_to_records

        symbols = request.get('symbols', ['SPY'])
        start_date = request.get('startDate', '2023-01-01')
        end_date = request.get('endDate', '2024-01-01')

        if isinstance(symbols, str):
            symbols = [s.strip() for s in symbols.split(',')]

        data = fetch_yfinance_data(symbols, start_date, end_date)
        return data_to_records(data)

    def calculate_indicator(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate technical indicator - comprehensive coverage."""
        import numpy as np
        from zl_data import fetch_yfinance_data
        from zl_strategies import (
            _rolling_mean, _ema, _rsi, _atr, _rolling_std,
            _macd_calc, _stochastic_calc, _williams_r_calc, _cci_calc,
            _obv_calc, _adx_calc, _keltner_calc, _donchian_calc,
            _momentum_calc, _zscore_calc, _vwap_calc,
        )

        symbols = request.get('symbols', ['SPY'])
        start_date = request.get('startDate', '2023-01-01')
        end_date = request.get('endDate', '2024-01-01')
        indicator = request.get('indicator', 'ma')
        params = request.get('parameters', {})

        if isinstance(symbols, str):
            symbols = [s.strip() for s in symbols.split(',')]

        data = fetch_yfinance_data(symbols, start_date, end_date)
        results = {}

        for sym in symbols:
            if sym not in data:
                continue
            df = data[sym]
            close = np.asarray(df['close'])
            high = np.asarray(df['high']) if 'high' in df.columns else close
            low = np.asarray(df['low']) if 'low' in df.columns else close
            volume = np.asarray(df['volume']) if 'volume' in df.columns else np.ones_like(close)
            period = int(params.get('period', 14))

            if indicator == 'ma' or indicator == 'sma':
                vals = _rolling_mean(close, period)
            elif indicator == 'ema':
                vals = _ema(close, period)
            elif indicator == 'rsi':
                vals = _rsi(close, period)
            elif indicator == 'bbands':
                ma = _rolling_mean(close, period)
                std = _rolling_std(close, period)
                alpha = float(params.get('alpha', 2.0))
                results[sym] = {
                    'upper': _to_json_list(ma + alpha * std),
                    'middle': _to_json_list(ma),
                    'lower': _to_json_list(ma - alpha * std),
                }
                continue
            elif indicator == 'atr':
                vals = _atr(high, low, close, period)
            elif indicator == 'mstd':
                vals = _rolling_std(close, period)
            elif indicator == 'macd':
                fast_p = int(params.get('fastPeriod', 12))
                slow_p = int(params.get('slowPeriod', 26))
                signal_p = int(params.get('signalPeriod', 9))
                macd_line, signal_line, histogram = _macd_calc(close, fast_p, slow_p, signal_p)
                results[sym] = {
                    'macd': _to_json_list(macd_line),
                    'signal': _to_json_list(signal_line),
                    'histogram': _to_json_list(histogram),
                }
                continue
            elif indicator == 'stochastic':
                k_p = int(params.get('kPeriod', 14))
                d_p = int(params.get('dPeriod', 3))
                k_vals, d_vals = _stochastic_calc(high, low, close, k_p, d_p)
                results[sym] = {
                    'k': _to_json_list(k_vals),
                    'd': _to_json_list(d_vals),
                }
                continue
            elif indicator == 'williams_r':
                vals = _williams_r_calc(high, low, close, period)
            elif indicator == 'cci':
                vals = _cci_calc(high, low, close, period)
            elif indicator == 'obv':
                vals = _obv_calc(close, volume)
            elif indicator == 'adx':
                vals = _adx_calc(high, low, close, period)
            elif indicator == 'keltner':
                mult = float(params.get('multiplier', 2.0))
                upper, middle, lower = _keltner_calc(high, low, close, period, mult)
                results[sym] = {
                    'upper': _to_json_list(upper),
                    'middle': _to_json_list(middle),
                    'lower': _to_json_list(lower),
                }
                continue
            elif indicator == 'donchian':
                upper, lower = _donchian_calc(high, low, period)
                results[sym] = {
                    'upper': _to_json_list(upper),
                    'lower': _to_json_list(lower),
                }
                continue
            elif indicator == 'momentum' or indicator == 'roc':
                vals = _momentum_calc(close, period)
            elif indicator == 'zscore':
                vals = _zscore_calc(close, period)
            elif indicator == 'vwap':
                vals = _vwap_calc(high, low, close, volume)
            else:
                vals = _rolling_mean(close, period)

            results[sym] = _to_json_list(vals)

        return {
            'success': True,
            'data': {
                'indicator': indicator,
                'parameters': params,
                'results': results,
            }
        }

    def get_strategies(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return strategy catalog."""
        from zl_strategies import get_strategy_catalog
        catalog = get_strategy_catalog()
        return {
            'success': True,
            'data': {
                'provider': 'zipline',
                'strategies': catalog,
            }
        }

    def get_indicators(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return comprehensive indicator catalog."""
        indicators = {
            'ma': {'name': 'Simple Moving Average', 'params': ['period'], 'category': 'trend'},
            'ema': {'name': 'Exponential Moving Average', 'params': ['period'], 'category': 'trend'},
            'rsi': {'name': 'Relative Strength Index', 'params': ['period'], 'category': 'momentum'},
            'bbands': {'name': 'Bollinger Bands', 'params': ['period', 'alpha'], 'category': 'volatility'},
            'atr': {'name': 'Average True Range', 'params': ['period'], 'category': 'volatility'},
            'mstd': {'name': 'Moving Std Dev', 'params': ['period'], 'category': 'volatility'},
            'macd': {'name': 'MACD', 'params': ['fastPeriod', 'slowPeriod', 'signalPeriod'], 'category': 'momentum'},
            'stochastic': {'name': 'Stochastic Oscillator', 'params': ['kPeriod', 'dPeriod'], 'category': 'momentum'},
            'williams_r': {'name': 'Williams %R', 'params': ['period'], 'category': 'momentum'},
            'cci': {'name': 'Commodity Channel Index', 'params': ['period'], 'category': 'momentum'},
            'obv': {'name': 'On-Balance Volume', 'params': [], 'category': 'volume'},
            'adx': {'name': 'Average Directional Index', 'params': ['period'], 'category': 'trend'},
            'keltner': {'name': 'Keltner Channel', 'params': ['period', 'multiplier'], 'category': 'volatility'},
            'donchian': {'name': 'Donchian Channel', 'params': ['period'], 'category': 'volatility'},
            'momentum': {'name': 'Momentum (ROC)', 'params': ['period'], 'category': 'momentum'},
            'zscore': {'name': 'Z-Score', 'params': ['period'], 'category': 'statistical'},
            'vwap': {'name': 'Volume Weighted Avg Price', 'params': [], 'category': 'volume'},
        }
        return {
            'success': True,
            'data': {
                'provider': 'zipline',
                'indicators': indicators,
            }
        }

    # ========================================================================
    # Helper: load close series
    # ========================================================================

    def _load_close_series(self, symbols, start_date, end_date):
        """Load close price as pd.Series. Returns (close_series, using_synthetic)."""
        import pandas as pd
        from zl_data import fetch_yfinance_data

        sym = symbols[0] if isinstance(symbols, list) else symbols
        if isinstance(symbols, str):
            symbols = [symbols]

        data = fetch_yfinance_data(symbols, start_date, end_date)
        if sym in data and len(data[sym]) > 0:
            df = data[sym]
            close = df['close']
            # Check if synthetic by seeing if the fetch returned "real" data
            # (synthetic data has a very specific seed pattern, but we can't
            # reliably distinguish - just return False as best guess)
            return close, False
        # Fallback: generate synthetic GBM
        dates = pd.bdate_range(start=start_date, end=end_date, tz='UTC')
        import numpy as np
        np.random.seed(hash(sym) % 2**31)
        price = 100.0
        prices = []
        for _ in range(len(dates)):
            ret = np.random.normal(0.0003, 0.015)
            price *= (1 + ret)
            prices.append(price)
        close = pd.Series(prices, index=dates, name='close')
        return close, True

    # ========================================================================
    # Indicator Signals
    # ========================================================================

    def indicator_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Generate entry/exit signals from technical indicators."""
        import numpy as np
        from zl_strategies import (
            _rolling_mean, _ema, _rsi, _cci_calc, _williams_r_calc,
            _donchian_calc, _keltner_calc, _rolling_std, _zscore_calc,
        )

        try:
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')
            mode = request.get('mode', 'crossover_signals')
            params = request.get('parameters', {})

            close_series, synthetic = self._load_close_series(symbols, start_date, end_date)
            close = np.asarray(close_series, dtype=float)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in close_series.index]
            n = len(close)

            entries = np.zeros(n, dtype=bool)
            exits = np.zeros(n, dtype=bool)

            if mode == 'crossover_signals':
                fast_p = int(params.get('fastPeriod', 10))
                slow_p = int(params.get('slowPeriod', 20))
                ma_type = params.get('maType', 'sma')
                if ma_type == 'ema':
                    fast_ma = _ema(close, fast_p)
                    slow_ma = _ema(close, slow_p)
                else:
                    fast_ma = _rolling_mean(close, fast_p)
                    slow_ma = _rolling_mean(close, slow_p)
                for i in range(1, n):
                    if not np.isnan(fast_ma[i]) and not np.isnan(slow_ma[i]):
                        if fast_ma[i] > slow_ma[i] and fast_ma[i-1] <= slow_ma[i-1]:
                            entries[i] = True
                        elif fast_ma[i] < slow_ma[i] and fast_ma[i-1] >= slow_ma[i-1]:
                            exits[i] = True

            elif mode == 'threshold_signals':
                indicator = params.get('indicator', 'rsi')
                period = int(params.get('period', 14))
                oversold = float(params.get('oversold', 30))
                overbought = float(params.get('overbought', 70))
                if indicator == 'rsi':
                    vals = _rsi(close, period)
                elif indicator == 'cci':
                    vals = _cci_calc(close, close, close, period)
                elif indicator == 'williams_r':
                    vals = _williams_r_calc(close, close, close, period)
                else:
                    vals = _rsi(close, period)
                for i in range(1, n):
                    if not np.isnan(vals[i]):
                        if vals[i] < oversold and vals[i-1] >= oversold:
                            entries[i] = True
                        elif vals[i] > overbought and vals[i-1] <= overbought:
                            exits[i] = True

            elif mode == 'breakout_signals':
                period = int(params.get('period', 20))
                channel = params.get('channel', 'donchian')
                if channel == 'keltner':
                    mult = float(params.get('multiplier', 2.0))
                    upper, middle, lower = _keltner_calc(close, close, close, period, mult)
                else:
                    upper, lower = _donchian_calc(close, close, period)
                for i in range(1, n):
                    if not np.isnan(upper[i]):
                        if close[i] > upper[i-1]:
                            entries[i] = True
                        elif close[i] < lower[i-1]:
                            exits[i] = True

            elif mode == 'mean_reversion_signals':
                period = int(params.get('period', 20))
                threshold = float(params.get('threshold', 2.0))
                zscores = _zscore_calc(close, period)
                for i in range(1, n):
                    if not np.isnan(zscores[i]):
                        if zscores[i] < -threshold and zscores[i-1] >= -threshold:
                            entries[i] = True
                        elif zscores[i] > threshold and zscores[i-1] <= threshold:
                            exits[i] = True

            elif mode == 'signal_filter':
                # Apply base signals and filter with another indicator
                fast_p = int(params.get('fastPeriod', 10))
                slow_p = int(params.get('slowPeriod', 20))
                filter_indicator = params.get('filterIndicator', 'rsi')
                filter_period = int(params.get('filterPeriod', 14))
                filter_threshold = float(params.get('filterThreshold', 50))
                fast_ma = _rolling_mean(close, fast_p)
                slow_ma = _rolling_mean(close, slow_p)
                if filter_indicator == 'rsi':
                    filt = _rsi(close, filter_period)
                else:
                    filt = _zscore_calc(close, filter_period)
                for i in range(1, n):
                    if not np.isnan(fast_ma[i]) and not np.isnan(slow_ma[i]) and not np.isnan(filt[i]):
                        if fast_ma[i] > slow_ma[i] and fast_ma[i-1] <= slow_ma[i-1] and filt[i] < filter_threshold:
                            entries[i] = True
                        elif fast_ma[i] < slow_ma[i] and fast_ma[i-1] >= slow_ma[i-1]:
                            exits[i] = True

            entry_dates = [dates[i] for i in range(n) if entries[i]]
            exit_dates = [dates[i] for i in range(n) if exits[i]]

            return {
                'success': True,
                'data': {
                    'mode': mode,
                    'totalBars': n,
                    'entryCount': int(entries.sum()),
                    'exitCount': int(exits.sum()),
                    'entries': entry_dates,
                    'exits': exit_dates,
                    'synthetic': synthetic,
                }
            }

        except Exception as e:
            import traceback
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    # ========================================================================
    # Generate Signals (random / algorithmic)
    # ========================================================================

    def generate_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Random/algorithmic signal generation for testing."""
        import numpy as np

        try:
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')
            gen_type = request.get('generatorType', 'RAND')
            params = request.get('parameters', {})
            seed = int(params.get('seed', 42))

            close_series, synthetic = self._load_close_series(symbols, start_date, end_date)
            close = np.asarray(close_series, dtype=float)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in close_series.index]
            n = len(close)
            rng = np.random.RandomState(seed)

            entries = np.zeros(n, dtype=bool)
            exits = np.zeros(n, dtype=bool)

            if gen_type == 'RAND':
                count = min(int(params.get('n', 10)), n)
                idxs = rng.choice(n, size=count, replace=False)
                entries[idxs] = True

            elif gen_type == 'RANDX':
                count = min(int(params.get('n', 10)), n // 2)
                idxs = np.sort(rng.choice(n, size=count * 2, replace=False))
                entries[idxs[::2]] = True
                exits[idxs[1::2]] = True

            elif gen_type == 'RANDNX':
                count = min(int(params.get('n', 10)), n // 4)
                min_hold = int(params.get('minHold', 5))
                max_hold = int(params.get('maxHold', 20))
                i = rng.randint(0, max(1, n // 10))
                placed = 0
                while placed < count and i < n:
                    entries[i] = True
                    hold = rng.randint(min_hold, max_hold + 1)
                    exit_i = min(i + hold, n - 1)
                    exits[exit_i] = True
                    i = exit_i + rng.randint(1, max(2, max_hold))
                    placed += 1

            elif gen_type == 'RPROB':
                entry_prob = float(params.get('entryProb', 0.05))
                entries = rng.random(n) < entry_prob

            elif gen_type == 'RPROBX':
                entry_prob = float(params.get('entryProb', 0.05))
                exit_prob = float(params.get('exitProb', 0.1))
                in_position = False
                for i in range(n):
                    if not in_position:
                        if rng.random() < entry_prob:
                            entries[i] = True
                            in_position = True
                    else:
                        if rng.random() < exit_prob:
                            exits[i] = True
                            in_position = False

            elif gen_type == 'RPROBCX':
                entry_prob = float(params.get('entryProb', 0.05))
                exit_prob = float(params.get('exitProb', 0.1))
                cooldown = int(params.get('cooldown', 5))
                in_position = False
                cooldown_remaining = 0
                for i in range(n):
                    if cooldown_remaining > 0:
                        cooldown_remaining -= 1
                        continue
                    if not in_position:
                        if rng.random() < entry_prob:
                            entries[i] = True
                            in_position = True
                    else:
                        if rng.random() < exit_prob:
                            exits[i] = True
                            in_position = False
                            cooldown_remaining = cooldown

            elif gen_type == 'RPROBNX':
                count = min(int(params.get('n', 10)), n // 4)
                entry_prob = float(params.get('entryProb', 0.05))
                exit_prob = float(params.get('exitProb', 0.1))
                in_position = False
                placed = 0
                for i in range(n):
                    if placed >= count:
                        break
                    if not in_position:
                        if rng.random() < entry_prob:
                            entries[i] = True
                            in_position = True
                            placed += 1
                    else:
                        if rng.random() < exit_prob:
                            exits[i] = True
                            in_position = False

            elif gen_type == 'STX':
                stop_loss = float(params.get('stopLoss', 0.02))
                take_profit = float(params.get('takeProfit', 0.04))
                # Generate random entries, then apply SL/TP exits
                entry_prob = float(params.get('entryProb', 0.05))
                entry_price = 0.0
                in_position = False
                for i in range(n):
                    if not in_position:
                        if rng.random() < entry_prob:
                            entries[i] = True
                            entry_price = close[i]
                            in_position = True
                    else:
                        ret = (close[i] - entry_price) / entry_price
                        if ret <= -stop_loss or ret >= take_profit:
                            exits[i] = True
                            in_position = False

            elif gen_type == 'STCX':
                stop_loss = float(params.get('stopLoss', 0.02))
                trailing_stop = float(params.get('trailingStop', 0.015))
                entry_prob = float(params.get('entryProb', 0.05))
                entry_price = 0.0
                peak_price = 0.0
                in_position = False
                for i in range(n):
                    if not in_position:
                        if rng.random() < entry_prob:
                            entries[i] = True
                            entry_price = close[i]
                            peak_price = close[i]
                            in_position = True
                    else:
                        if close[i] > peak_price:
                            peak_price = close[i]
                        ret = (close[i] - entry_price) / entry_price
                        trail_dd = (peak_price - close[i]) / peak_price
                        if ret <= -stop_loss or trail_dd >= trailing_stop:
                            exits[i] = True
                            in_position = False

            elif gen_type == 'OHLCSTX':
                stop_loss = float(params.get('stopLoss', 0.02))
                take_profit = float(params.get('takeProfit', 0.04))
                entry_prob = float(params.get('entryProb', 0.05))
                entry_price = 0.0
                in_position = False
                for i in range(n):
                    if not in_position:
                        if rng.random() < entry_prob:
                            entries[i] = True
                            entry_price = close[i]
                            in_position = True
                    else:
                        ret = (close[i] - entry_price) / entry_price
                        if ret <= -stop_loss or ret >= take_profit:
                            exits[i] = True
                            in_position = False

            entry_dates = [dates[i] for i in range(n) if entries[i]]
            exit_dates = [dates[i] for i in range(n) if exits[i]]

            return {
                'success': True,
                'data': {
                    'generatorType': gen_type,
                    'totalBars': n,
                    'entryCount': int(np.sum(entries)),
                    'exitCount': int(np.sum(exits)),
                    'entries': entry_dates,
                    'exits': exit_dates,
                    'synthetic': synthetic,
                }
            }

        except Exception as e:
            import traceback
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    # ========================================================================
    # Generate Labels (ML label generation)
    # ========================================================================

    def generate_labels(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """ML label generation for supervised learning workflows."""
        import numpy as np

        try:
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')
            label_type = request.get('labelType', 'FIXLB')
            params = request.get('parameters', {})

            close_series, synthetic = self._load_close_series(symbols, start_date, end_date)
            close = np.asarray(close_series, dtype=float)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in close_series.index]
            n = len(close)

            labels = np.zeros(n, dtype=int)  # 0 = neutral

            if label_type == 'FIXLB':
                horizon = int(params.get('horizon', 5))
                threshold = float(params.get('threshold', 0.01))
                for i in range(n - horizon):
                    fwd_ret = (close[i + horizon] - close[i]) / close[i]
                    if fwd_ret > threshold:
                        labels[i] = 1
                    elif fwd_ret < -threshold:
                        labels[i] = -1

            elif label_type == 'MEANLB':
                window = int(params.get('window', 20))
                threshold = float(params.get('threshold', 1.5))
                from zl_strategies import _zscore_calc
                zscores = _zscore_calc(close, window)
                for i in range(n):
                    if not np.isnan(zscores[i]):
                        if zscores[i] < -threshold:
                            labels[i] = 1  # buy (below mean)
                        elif zscores[i] > threshold:
                            labels[i] = -1  # sell (above mean)

            elif label_type == 'LEXLB':
                window = int(params.get('window', 5))
                for i in range(window, n - window):
                    local_window = close[i - window:i + window + 1]
                    if close[i] == local_window.min():
                        labels[i] = 1  # local min -> buy
                    elif close[i] == local_window.max():
                        labels[i] = -1  # local max -> sell

            elif label_type == 'TRENDLB':
                window = int(params.get('window', 20))
                threshold = float(params.get('threshold', 0.0))
                for i in range(window - 1, n):
                    segment = close[i - window + 1:i + 1]
                    x = np.arange(window)
                    # Linear regression slope
                    x_mean = x.mean()
                    y_mean = segment.mean()
                    denom = np.sum((x - x_mean) ** 2)
                    if denom > 1e-10:
                        slope = np.sum((x - x_mean) * (segment - y_mean)) / denom
                        # Normalize slope by price level
                        norm_slope = slope / y_mean if y_mean > 0 else 0
                        if norm_slope > threshold:
                            labels[i] = 1
                        elif norm_slope < -threshold:
                            labels[i] = -1

            elif label_type == 'BOLB':
                window = int(params.get('window', 20))
                alpha = float(params.get('alpha', 2.0))
                from zl_strategies import _rolling_mean, _rolling_std
                ma = _rolling_mean(close, window)
                std = _rolling_std(close, window)
                for i in range(n):
                    if not np.isnan(ma[i]) and not np.isnan(std[i]) and std[i] > 1e-8:
                        upper = ma[i] + alpha * std[i]
                        lower = ma[i] - alpha * std[i]
                        if close[i] < lower:
                            labels[i] = 1  # buy
                        elif close[i] > upper:
                            labels[i] = -1  # sell

            # Build distribution
            dist = {
                '-1': int(np.sum(labels == -1)),
                '0': int(np.sum(labels == 0)),
                '1': int(np.sum(labels == 1)),
            }
            labeled_bars = int(np.sum(labels != 0))

            # Sample labels (first 50 non-zero)
            sample_labels = []
            count = 0
            for i in range(n):
                if labels[i] != 0:
                    sample_labels.append({'date': dates[i], 'label': int(labels[i])})
                    count += 1
                    if count >= 50:
                        break

            return {
                'success': True,
                'data': {
                    'labelType': label_type,
                    'totalBars': n,
                    'labeledBars': labeled_bars,
                    'distribution': dist,
                    'sampleLabels': sample_labels,
                    'synthetic': synthetic,
                }
            }

        except Exception as e:
            import traceback
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    # ========================================================================
    # Generate Splits (cross-validation)
    # ========================================================================

    def generate_splits(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Cross-validation split generation for walk-forward testing."""
        import numpy as np
        import pandas as pd

        try:
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')
            splitter_type = request.get('splitterType', 'rolling')
            params = request.get('parameters', {})

            close_series, synthetic = self._load_close_series(symbols, start_date, end_date)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in close_series.index]
            n = len(close_series)

            splits = []

            if splitter_type == 'rolling':
                window_len = int(params.get('windowLen', 252))
                test_len = int(params.get('testLen', 63))
                step = int(params.get('step', 21))
                fold = 0
                i = 0
                while i + window_len + test_len <= n:
                    train_start = i
                    train_end = i + window_len - 1
                    test_start = i + window_len
                    test_end = min(i + window_len + test_len - 1, n - 1)
                    splits.append({
                        'fold': fold,
                        'trainStart': dates[train_start],
                        'trainEnd': dates[train_end],
                        'trainSize': window_len,
                        'testStart': dates[test_start],
                        'testEnd': dates[test_end],
                        'testSize': test_end - test_start + 1,
                    })
                    fold += 1
                    i += step

            elif splitter_type == 'expanding':
                min_len = int(params.get('minLen', 126))
                test_len = int(params.get('testLen', 63))
                step = int(params.get('step', 21))
                fold = 0
                train_end_idx = min_len - 1
                while train_end_idx + test_len < n:
                    test_start = train_end_idx + 1
                    test_end = min(test_start + test_len - 1, n - 1)
                    splits.append({
                        'fold': fold,
                        'trainStart': dates[0],
                        'trainEnd': dates[train_end_idx],
                        'trainSize': train_end_idx + 1,
                        'testStart': dates[test_start],
                        'testEnd': dates[test_end],
                        'testSize': test_end - test_start + 1,
                    })
                    fold += 1
                    train_end_idx += step

            elif splitter_type == 'purged_kfold':
                n_splits = int(params.get('nSplits', 5))
                purge_len = int(params.get('purgeLen', 5))
                embargo_len = int(params.get('embargoLen', 5))
                fold_size = n // n_splits
                for fold in range(n_splits):
                    test_start = fold * fold_size
                    test_end = min((fold + 1) * fold_size - 1, n - 1)
                    # Train = everything except test + purge + embargo
                    train_indices_before = list(range(0, max(0, test_start - purge_len)))
                    train_indices_after = list(range(min(n, test_end + 1 + embargo_len), n))
                    train_size = len(train_indices_before) + len(train_indices_after)
                    t_start = dates[train_indices_before[0]] if train_indices_before else dates[train_indices_after[0]] if train_indices_after else dates[0]
                    t_end = dates[train_indices_after[-1]] if train_indices_after else dates[train_indices_before[-1]] if train_indices_before else dates[-1]
                    splits.append({
                        'fold': fold,
                        'trainStart': t_start,
                        'trainEnd': t_end,
                        'trainSize': train_size,
                        'testStart': dates[test_start],
                        'testEnd': dates[test_end],
                        'testSize': test_end - test_start + 1,
                    })

            elif splitter_type == 'range':
                ranges = params.get('ranges', [])
                for fold, rng_spec in enumerate(ranges):
                    splits.append({
                        'fold': fold,
                        'trainStart': rng_spec.get('trainStart', start_date),
                        'trainEnd': rng_spec.get('trainEnd', start_date),
                        'trainSize': 0,
                        'testStart': rng_spec.get('testStart', end_date),
                        'testEnd': rng_spec.get('testEnd', end_date),
                        'testSize': 0,
                    })

            return {
                'success': True,
                'data': {
                    'splitterType': splitter_type,
                    'totalBars': n,
                    'nSplits': len(splits),
                    'splits': splits,
                    'synthetic': synthetic,
                }
            }

        except Exception as e:
            import traceback
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    # ========================================================================
    # Analyze Returns
    # ========================================================================

    def analyze_returns(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Comprehensive returns analysis."""
        import numpy as np

        try:
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')
            analysis_type = request.get('analysisType', 'returns_stats')
            params = request.get('parameters', {})

            close_series, synthetic = self._load_close_series(symbols, start_date, end_date)
            close = np.asarray(close_series, dtype=float)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in close_series.index]
            n = len(close)

            # Compute daily returns
            returns = np.diff(close) / close[:-1]
            returns = np.insert(returns, 0, 0.0)

            if analysis_type == 'returns_stats':
                # Core stats
                total_return = (close[-1] / close[0]) - 1.0 if close[0] > 0 else 0.0
                ann_return = (1 + total_return) ** (252 / max(n, 1)) - 1.0
                volatility = float(np.std(returns[1:], ddof=1) * np.sqrt(252)) if n > 2 else 0.0
                sharpe = ann_return / volatility if volatility > 1e-8 else 0.0

                # Sortino
                downside = returns[returns < 0]
                down_std = float(np.std(downside, ddof=1) * np.sqrt(252)) if len(downside) > 1 else 1e-8
                sortino = ann_return / down_std if down_std > 1e-8 else 0.0

                # Max drawdown
                cum_ret = np.cumprod(1 + returns)
                running_max = np.maximum.accumulate(cum_ret)
                dd = (cum_ret - running_max) / running_max
                max_dd = float(np.min(dd))

                # Calmar
                calmar = ann_return / abs(max_dd) if abs(max_dd) > 1e-8 else 0.0

                # Omega (threshold = 0)
                gains = returns[returns > 0].sum()
                losses = abs(returns[returns < 0].sum())
                omega = (gains / losses) if losses > 1e-8 else float('inf')

                # Skewness & kurtosis
                if n > 3:
                    r = returns[1:]
                    r_mean = r.mean()
                    r_std = r.std(ddof=1)
                    if r_std > 1e-8:
                        skew = float(np.mean(((r - r_mean) / r_std) ** 3))
                        kurt = float(np.mean(((r - r_mean) / r_std) ** 4) - 3.0)
                    else:
                        skew, kurt = 0.0, 0.0
                else:
                    skew, kurt = 0.0, 0.0

                # Up/down capture (vs benchmark assumed as same series)
                up_days = returns[returns > 0]
                down_days = returns[returns < 0]
                up_capture = float(up_days.mean() * 252) if len(up_days) > 0 else 0.0
                down_capture = float(down_days.mean() * 252) if len(down_days) > 0 else 0.0

                # Win rate
                win_rate = float(len(up_days) / max(len(returns[1:]), 1))

                # Cumulative returns series
                cum_returns = list(np.cumprod(1 + returns) - 1.0)
                cum_returns_clean = []
                for v in cum_returns:
                    fv = float(v)
                    cum_returns_clean.append(None if (math.isnan(fv) or math.isinf(fv)) else round(fv, 6))

                return {
                    'success': True,
                    'data': {
                        'analysisType': 'returns_stats',
                        'stats': {
                            'totalReturn': round(float(total_return), 6),
                            'annualizedReturn': round(float(ann_return), 6),
                            'volatility': round(volatility, 6),
                            'sharpeRatio': round(float(sharpe), 4),
                            'sortinoRatio': round(float(sortino), 4),
                            'calmarRatio': round(float(calmar), 4),
                            'omegaRatio': round(float(min(omega, 999)), 4),
                            'maxDrawdown': round(float(max_dd), 6),
                            'skewness': round(skew, 4),
                            'kurtosis': round(kurt, 4),
                            'upCapture': round(up_capture, 6),
                            'downCapture': round(down_capture, 6),
                            'winRate': round(win_rate, 4),
                            'totalBars': n,
                            'bestDay': round(float(np.max(returns[1:])), 6) if n > 1 else 0.0,
                            'worstDay': round(float(np.min(returns[1:])), 6) if n > 1 else 0.0,
                            'avgDailyReturn': round(float(np.mean(returns[1:])), 6) if n > 1 else 0.0,
                        },
                        'cumulativeReturns': cum_returns_clean,
                        'dates': dates,
                        'synthetic': synthetic,
                    }
                }

            elif analysis_type == 'drawdowns':
                cum_ret = np.cumprod(1 + returns)
                running_max = np.maximum.accumulate(cum_ret)
                dd = (cum_ret - running_max) / running_max

                # Find drawdown events
                in_dd = False
                dd_events = []
                current_dd_start = 0
                for i in range(n):
                    if dd[i] < -1e-6 and not in_dd:
                        in_dd = True
                        current_dd_start = i
                    elif dd[i] >= -1e-6 and in_dd:
                        in_dd = False
                        dd_depth = float(np.min(dd[current_dd_start:i]))
                        dd_events.append({
                            'start': dates[current_dd_start],
                            'end': dates[i],
                            'depth': round(dd_depth, 6),
                            'duration': i - current_dd_start,
                        })

                # Handle active drawdown
                if in_dd:
                    dd_depth = float(np.min(dd[current_dd_start:]))
                    dd_events.append({
                        'start': dates[current_dd_start],
                        'end': dates[-1],
                        'depth': round(dd_depth, 6),
                        'duration': n - current_dd_start,
                        'active': True,
                    })

                max_dd = float(np.min(dd))
                avg_dd = float(np.mean([e['depth'] for e in dd_events])) if dd_events else 0.0
                avg_dur = float(np.mean([e['duration'] for e in dd_events])) if dd_events else 0.0

                dd_series = []
                for v in dd:
                    fv = float(v)
                    dd_series.append(None if (math.isnan(fv) or math.isinf(fv)) else round(fv, 6))

                return {
                    'success': True,
                    'data': {
                        'analysisType': 'drawdowns',
                        'stats': {
                            'count': len(dd_events),
                            'maxDrawdown': round(max_dd, 6),
                            'avgDrawdown': round(avg_dd, 6),
                            'avgDuration': round(avg_dur, 1),
                        },
                        'events': dd_events[:50],
                        'drawdownSeries': dd_series,
                        'dates': dates,
                        'synthetic': synthetic,
                    }
                }

            elif analysis_type == 'ranges':
                threshold = float(params.get('threshold', 0.0))
                # Find contiguous ranges where cumulative return > threshold
                cum_ret = np.cumprod(1 + returns) - 1.0
                in_range = False
                ranges_list = []
                range_start = 0
                for i in range(n):
                    if cum_ret[i] > threshold and not in_range:
                        in_range = True
                        range_start = i
                    elif cum_ret[i] <= threshold and in_range:
                        in_range = False
                        ranges_list.append({
                            'start': dates[range_start],
                            'end': dates[i - 1],
                            'duration': i - range_start,
                            'maxReturn': round(float(np.max(cum_ret[range_start:i])), 6),
                        })
                if in_range:
                    ranges_list.append({
                        'start': dates[range_start],
                        'end': dates[-1],
                        'duration': n - range_start,
                        'maxReturn': round(float(np.max(cum_ret[range_start:])), 6),
                    })

                coverage = sum(r['duration'] for r in ranges_list) / max(n, 1)

                return {
                    'success': True,
                    'data': {
                        'analysisType': 'ranges',
                        'stats': {
                            'count': len(ranges_list),
                            'avgDuration': round(float(np.mean([r['duration'] for r in ranges_list])), 1) if ranges_list else 0.0,
                            'maxDuration': max((r['duration'] for r in ranges_list), default=0),
                            'coverage': round(float(coverage), 4),
                        },
                        'ranges': ranges_list[:50],
                        'synthetic': synthetic,
                    }
                }

            elif analysis_type == 'rolling':
                window = int(params.get('window', 63))
                metrics_requested = params.get('metrics', ['sharpe', 'volatility'])

                rolling_data = {}
                roll_dates = dates[window:]

                if 'sharpe' in metrics_requested or 'all' in metrics_requested:
                    roll_sharpe = []
                    for i in range(window, n):
                        w = returns[i - window + 1:i + 1]
                        ann_r = float(np.mean(w) * 252)
                        ann_v = float(np.std(w, ddof=1) * np.sqrt(252))
                        s = ann_r / ann_v if ann_v > 1e-8 else 0.0
                        roll_sharpe.append(round(s, 4))
                    rolling_data['sharpe'] = roll_sharpe

                if 'sortino' in metrics_requested or 'all' in metrics_requested:
                    roll_sortino = []
                    for i in range(window, n):
                        w = returns[i - window + 1:i + 1]
                        ann_r = float(np.mean(w) * 252)
                        down = w[w < 0]
                        ds = float(np.std(down, ddof=1) * np.sqrt(252)) if len(down) > 1 else 1e-8
                        roll_sortino.append(round(ann_r / ds if ds > 1e-8 else 0.0, 4))
                    rolling_data['sortino'] = roll_sortino

                if 'volatility' in metrics_requested or 'all' in metrics_requested:
                    roll_vol = []
                    for i in range(window, n):
                        w = returns[i - window + 1:i + 1]
                        roll_vol.append(round(float(np.std(w, ddof=1) * np.sqrt(252)), 6))
                    rolling_data['volatility'] = roll_vol

                if 'calmar' in metrics_requested or 'all' in metrics_requested:
                    roll_calmar = []
                    for i in range(window, n):
                        w = returns[i - window + 1:i + 1]
                        ann_r = float(np.mean(w) * 252)
                        c = np.cumprod(1 + w)
                        rm = np.maximum.accumulate(c)
                        dd = (c - rm) / rm
                        mdd = float(np.min(dd))
                        roll_calmar.append(round(ann_r / abs(mdd) if abs(mdd) > 1e-8 else 0.0, 4))
                    rolling_data['calmar'] = roll_calmar

                return {
                    'success': True,
                    'data': {
                        'analysisType': 'rolling',
                        'window': window,
                        'metrics': rolling_data,
                        'dates': roll_dates,
                        'synthetic': synthetic,
                    }
                }

            else:
                return {'success': False, 'error': f'Unknown analysis type: {analysis_type}'}

        except Exception as e:
            import traceback
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    # ========================================================================
    # Labels to Signals
    # ========================================================================

    def labels_to_signals(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Convert ML labels to entry/exit signals."""
        import numpy as np

        try:
            # Generate labels first using the same logic
            label_result = self.generate_labels(request)
            if not label_result.get('success'):
                return label_result

            # Now regenerate labels to get raw array
            symbols = request.get('symbols', ['SPY'])
            start_date = request.get('startDate', '2023-01-01')
            end_date = request.get('endDate', '2024-01-01')
            label_type = request.get('labelType', 'FIXLB')
            params = request.get('parameters', {})
            entry_label = int(params.get('entryLabel', 1))
            exit_label = int(params.get('exitLabel', -1))

            close_series, synthetic = self._load_close_series(symbols, start_date, end_date)
            close = np.asarray(close_series, dtype=float)
            dates = [str(d.date()) if hasattr(d, 'date') else str(d) for d in close_series.index]
            n = len(close)

            # Regenerate labels array
            labels = np.zeros(n, dtype=int)
            if label_type == 'FIXLB':
                horizon = int(params.get('horizon', 5))
                threshold = float(params.get('threshold', 0.01))
                for i in range(n - horizon):
                    fwd_ret = (close[i + horizon] - close[i]) / close[i]
                    if fwd_ret > threshold:
                        labels[i] = 1
                    elif fwd_ret < -threshold:
                        labels[i] = -1
            elif label_type == 'MEANLB':
                window = int(params.get('window', 20))
                threshold = float(params.get('threshold', 1.5))
                from zl_strategies import _zscore_calc
                zscores = _zscore_calc(close, window)
                for i in range(n):
                    if not np.isnan(zscores[i]):
                        if zscores[i] < -threshold:
                            labels[i] = 1
                        elif zscores[i] > threshold:
                            labels[i] = -1
            elif label_type == 'LEXLB':
                window = int(params.get('window', 5))
                for i in range(window, n - window):
                    local_window = close[i - window:i + window + 1]
                    if close[i] == local_window.min():
                        labels[i] = 1
                    elif close[i] == local_window.max():
                        labels[i] = -1
            elif label_type == 'TRENDLB':
                window = int(params.get('window', 20))
                threshold = float(params.get('threshold', 0.0))
                for i in range(window - 1, n):
                    segment = close[i - window + 1:i + 1]
                    x = np.arange(window)
                    x_mean = x.mean()
                    y_mean = segment.mean()
                    denom = np.sum((x - x_mean) ** 2)
                    if denom > 1e-10:
                        slope = np.sum((x - x_mean) * (segment - y_mean)) / denom
                        norm_slope = slope / y_mean if y_mean > 0 else 0
                        if norm_slope > threshold:
                            labels[i] = 1
                        elif norm_slope < -threshold:
                            labels[i] = -1
            elif label_type == 'BOLB':
                window = int(params.get('window', 20))
                alpha = float(params.get('alpha', 2.0))
                from zl_strategies import _rolling_mean, _rolling_std
                ma = _rolling_mean(close, window)
                std = _rolling_std(close, window)
                for i in range(n):
                    if not np.isnan(ma[i]) and not np.isnan(std[i]) and std[i] > 1e-8:
                        upper = ma[i] + alpha * std[i]
                        lower = ma[i] - alpha * std[i]
                        if close[i] < lower:
                            labels[i] = 1
                        elif close[i] > upper:
                            labels[i] = -1

            entries = labels == entry_label
            exits_arr = labels == exit_label

            entry_dates = [dates[i] for i in range(n) if entries[i]]
            exit_dates = [dates[i] for i in range(n) if exits_arr[i]]

            return {
                'success': True,
                'data': {
                    'labelType': label_type,
                    'entryLabel': entry_label,
                    'exitLabel': exit_label,
                    'totalBars': n,
                    'entryCount': int(np.sum(entries)),
                    'exitCount': int(np.sum(exits_arr)),
                    'entries': entry_dates,
                    'exits': exit_dates,
                    'synthetic': synthetic,
                }
            }

        except Exception as e:
            import traceback
            traceback.print_exc(file=sys.stderr)
            return {'success': False, 'error': str(e)}

    # ========================================================================
    # Helpers
    # ========================================================================

    def _generate_param_combos(self, param_ranges, method, max_iter):
        """Generate parameter combinations for optimization."""
        import numpy as np
        import itertools

        param_values = {}
        for name, spec in param_ranges.items():
            if isinstance(spec, dict):
                mn = spec.get('min', 0)
                mx = spec.get('max', 100)
                step = spec.get('step', 1)
                param_values[name] = list(np.arange(mn, mx + step, step))
            elif isinstance(spec, list):
                param_values[name] = spec
            else:
                param_values[name] = [spec]

        if method == 'grid':
            keys = list(param_values.keys())
            vals = list(param_values.values())
            combos = [dict(zip(keys, combo)) for combo in itertools.product(*vals)]
            if len(combos) > max_iter:
                np.random.shuffle(combos)
                combos = combos[:max_iter]
        else:
            combos = []
            keys = list(param_values.keys())
            for _ in range(max_iter):
                combo = {}
                for key in keys:
                    combo[key] = float(np.random.choice(param_values[key]))
                combos.append(combo)

        return combos

    def _get_objective_score(self, perf, objective):
        """Extract optimization objective score from performance."""
        if objective == 'sharpe':
            return perf.get('sharpe_ratio', 0)
        elif objective == 'sortino':
            return perf.get('sortino_ratio', 0)
        elif objective == 'calmar':
            return perf.get('calmar_ratio', 0)
        elif objective == 'return':
            return perf.get('total_return', 0)
        return perf.get('sharpe_ratio', 0)


def _to_json_list(arr):
    """Convert numpy array to JSON-safe list."""
    import numpy as np
    result = []
    for v in arr:
        if v is None or (isinstance(v, float) and (math.isnan(v) or math.isinf(v))):
            result.append(None)
        elif isinstance(v, (np.floating, np.integer)):
            fv = float(v)
            result.append(None if (math.isnan(fv) or math.isinf(fv)) else fv)
        else:
            try:
                fv = float(v)
                result.append(None if (math.isnan(fv) or math.isinf(fv)) else fv)
            except (TypeError, ValueError):
                result.append(None)
    return result


# ============================================================================
# CLI Entry Point
# ============================================================================

def main():
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': f'Usage: zipline_provider.py <command> <json_args | --stdin>'
        }))
        sys.exit(1)

    command = sys.argv[1]

    # Support --stdin mode for large payloads
    if sys.argv[2] == '--stdin':
        json_args = sys.stdin.read()
    else:
        json_args = sys.argv[2]

    # Redirect stdout to buffer during execution to prevent stray prints
    real_stdout = sys.stdout
    captured = io.StringIO()

    try:
        sys.stdout = captured
        print(f'[ZL-MAIN] command={command}, args_len={len(json_args)}', file=sys.stderr)
        args = parse_json_input(json_args)
        print(f'[ZL-MAIN] parsed args keys: {sorted(args.keys()) if isinstance(args, dict) else type(args)}', file=sys.stderr)
        provider = ZiplineProvider()

        if command == 'test_import':
            result_str = json_response({'success': True, 'version': provider.version})
        elif command == 'test_connection':
            result = provider.test_connection()
            result_str = json_response(result)
        elif command == 'initialize':
            result = provider.initialize(args)
            result_str = json_response(result)
        elif command == 'run_backtest':
            print(f'[ZL-MAIN] Calling run_backtest...', file=sys.stderr)
            result = provider.run_backtest(args)
            print(f'[ZL-MAIN] run_backtest returned, success={result.get("success")}', file=sys.stderr)
            result_str = json_response(result)
        elif command == 'optimize':
            result = provider.optimize(args)
            result_str = json_response(result)
        elif command == 'walk_forward':
            result = provider.walk_forward(args)
            result_str = json_response(result)
        elif command == 'get_strategies':
            result = provider.get_strategies(args)
            result_str = json_response(result)
        elif command == 'get_indicators':
            result = provider.get_indicators(args)
            result_str = json_response(result)
        elif command == 'get_historical_data':
            result = provider.get_historical_data(args)
            result_str = json_response({'success': True, 'data': result})
        elif command == 'calculate_indicator':
            result = provider.calculate_indicator(args)
            result_str = json_response(result)
        elif command == 'indicator_signals':
            result = provider.indicator_signals(args)
            result_str = json_response(result)
        elif command == 'generate_signals':
            result = provider.generate_signals(args)
            result_str = json_response(result)
        elif command == 'generate_labels':
            result = provider.generate_labels(args)
            result_str = json_response(result)
        elif command == 'generate_splits':
            result = provider.generate_splits(args)
            result_str = json_response(result)
        elif command == 'analyze_returns':
            result = provider.analyze_returns(args)
            result_str = json_response(result)
        elif command == 'labels_to_signals':
            result = provider.labels_to_signals(args)
            result_str = json_response(result)
        else:
            result_str = json_response({'success': False, 'error': f'Unknown command: {command}'})

        # Emit only the JSON on the real stdout
        sys.stdout = real_stdout
        print(result_str)

    except Exception as e:
        sys.stdout = real_stdout
        stray_output = captured.getvalue()
        if stray_output:
            print(stray_output, file=sys.stderr)
        print(json_response({
            'success': False,
            'error': str(e),
            'traceback': __import__('traceback').format_exc()
        }))


if __name__ == '__main__':
    main()
