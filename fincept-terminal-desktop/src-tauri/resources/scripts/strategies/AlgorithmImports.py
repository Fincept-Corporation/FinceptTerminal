# ============================================================================
# Fincept Terminal - AlgorithmImports Compatibility Layer
# Drop-in replacement for QuantConnect's AlgorithmImports.py
# Routes all imports to fincept_engine (pure Python, no .NET)
# ============================================================================

import os
import sys

# Ensure fincept_engine is importable
_dir = os.path.dirname(os.path.abspath(__file__))
if _dir not in sys.path:
    sys.path.insert(0, _dir)

# Core algorithm
from fincept_engine.algorithm import (
    QCAlgorithm, AlphaModel, PortfolioConstructionModel,
    ExecutionModel, RiskManagementModel, UniverseSelectionModel,
    NullRiskManagementModel, NullAlphaModel,
    Chart, Series, SeriesType, OptionChainProvider, SecurityChanges,
    Transactions, ObjectStore, NotificationManager, SubscriptionManager
)

# Framework models - Alpha
from fincept_engine.algorithm import (
    ConstantAlphaModel, CompositeAlphaModel
)

# Framework models - Portfolio Construction
from fincept_engine.algorithm import (
    EqualWeightingPortfolioConstructionModel,
    InsightWeightingPortfolioConstructionModel,
    MeanVarianceOptimizationPortfolioConstructionModel,
    BlackLittermanOptimizationPortfolioConstructionModel
)

# Framework models - Execution
from fincept_engine.algorithm import (
    ImmediateExecutionModel,
    VolumeWeightedAveragePriceExecutionModel,
    StandardDeviationExecutionModel
)

# Framework models - Risk Management
from fincept_engine.algorithm import (
    MaximumDrawdownPercentPortfolio,
    MaximumDrawdownPercentPerSecurity,
    MaximumUnrealizedProfitPercentPerSecurity,
    TrailingStopRiskManagementModel,
    MaximumSectorExposureRiskManagementModel,
    CompositeRiskManagementModel
)

# Framework models - Universe Selection
from fincept_engine.algorithm import (
    ManualUniverseSelectionModel,
    FundamentalUniverseSelectionModel,
    ScheduledUniverseSelectionModel,
    CustomUniverseSelectionModel
)

# Data types
from fincept_engine.algorithm import (
    PythonData, PythonQuandl, InsightCollection,
    Field, Futures, Globals, SecurityIdentifier, PortfolioBias
)

# Enums
from fincept_engine.enums import (
    Resolution, SecurityType, OrderType, OrderStatus,
    Market, InsightDirection, InsightType, OrderDirection,
    TimeInForce, DataNormalizationMode, MovingAverageType,
    OptionRight, OptionStyle, BrokerageName, AccountType,
    PositionSide, SettlementType
)

# Types
from fincept_engine.types import (
    Symbol, Slice, TradeBar, QuoteBar, Tick,
    SecurityHolding, OrderTicket, OrderEvent, UpdateOrderFields,
    Insight, InsightScore, PortfolioTarget, IndicatorValue
)

# Indicators
from fincept_engine.indicators import (
    IndicatorBase, IndicatorDataPoint,
    ExponentialMovingAverage, SimpleMovingAverage,
    MovingAverageConvergenceDivergence, RelativeStrengthIndex,
    BollingerBands, AverageTrueRange, Stochastic,
    RateOfChange, Momentum, WilliamsPercentR,
    CommodityChannelIndex, AverageDirectionalIndex
)

# Portfolio
from fincept_engine.portfolio import SecurityPortfolioManager

# Securities
from fincept_engine.securities import SecurityManager, Security, Exchange, ExchangeHours

# Scheduling
from fincept_engine.scheduling import (
    ScheduleManager, DateRules, TimeRules,
    ScheduledEvent, DateRule, TimeRule
)

# Consolidators
from fincept_engine.consolidators import (
    TradeBarConsolidator, QuoteBarConsolidator,
    RenkoConsolidator, RangeConsolidator, TickConsolidator
)

# Universe
from fincept_engine.universe import (
    UniverseSettings, Universe, CoarseFundamental, FineFundamental,
    ValuationRatios, OperationRatios, AssetClassification,
    CompanyReference, SecurityReference
)

# Extensions and utilities
from fincept_engine.extensions import (
    Time, TimeZones, Extensions, IndicatorExtensions,
    BuyingPowerModelExtensions, DefaultBrokerageModel, RateOfChangePercent
)

# Standard library (matching LEAN's AlgorithmImports)
from datetime import date, time, datetime, timedelta
from typing import *
import math
import json

try:
    import numpy as np
except ImportError:
    np = None

try:
    import pandas as pd
except ImportError:
    pd = None

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
except ImportError:
    plt = None

# Aliases for LEAN compatibility
QCAlgorithmFramework = QCAlgorithm
QCAlgorithmFrameworkBridge = QCAlgorithm

# TimeSpan alias for .NET compatibility
TimeSpan = timedelta

# DayOfWeek enum
class DayOfWeek:
    MONDAY = 0
    TUESDAY = 1
    WEDNESDAY = 2
    THURSDAY = 3
    FRIDAY = 4
    SATURDAY = 5
    SUNDAY = 6
    Monday = 0
    Tuesday = 1
    Wednesday = 2
    Thursday = 3
    Friday = 4
    Saturday = 5
    Sunday = 6

# TickType enum
class TickType:
    TRADE = 0
    QUOTE = 1
    OPEN_INTEREST = 2
    Trade = 0
    Quote = 1
    OpenInterest = 2

# OptionPriceModels
class OptionPriceModels:
    @staticmethod
    def crank_nicolson_fd():
        return None
    @staticmethod
    def black_scholes():
        return None
    @staticmethod
    def additive_equiprobabilities():
        return None
    @staticmethod
    def binomial_cox_ross_rubinstein():
        return None
    @staticmethod
    def binomial_jarrow_rudd():
        return None
    CrankNicolsonFD = crank_nicolson_fd
    BlackScholes = black_scholes

# OptionFilterUniverse
class OptionFilterUniverse:
    def __init__(self):
        self._strikes_down = 0
        self._strikes_up = 0
    def strikes(self, down, up):
        self._strikes_down = down
        self._strikes_up = up
        return self
    def expiration(self, min_days, max_days):
        return self
    def include_weeklys(self):
        return self
    def calls_only(self):
        return self
    def puts_only(self):
        return self

# FillModel stub
class FillModel:
    def market_fill(self, security, order):
        return None
    def limit_fill(self, security, order):
        return None
    def stop_market_fill(self, security, order):
        return None

# PythonIndicator base
class PythonIndicator:
    """Base class for custom Python indicators."""
    def __init__(self):
        self.name = self.__class__.__name__
        self.value = 0.0
        self.is_ready = False
        self.warm_up_period = 0
        self.current = type('obj', (object,), {'value': 0.0})()

    def update(self, input_data):
        return False

    def __float__(self):
        return self.value

# Order class (for type checking)
class Order:
    def __init__(self):
        self.id = 0
        self.symbol = None
        self.type = OrderType.MARKET
        self.status = OrderStatus.NEW
        self.quantity = 0
        self.price = 0
        self.tag = ""
        self.direction = OrderDirection.BUY

# BrokerageModelSecurityInitializer
class BrokerageModelSecurityInitializer:
    def __init__(self, brokerage_model=None, security_seeder=None):
        self._model = brokerage_model
        self._seeder = security_seeder

    def initialize(self, security):
        pass

# SecuritySeeder
class SecuritySeeder:
    @staticmethod
    def null():
        return SecuritySeeder()

    def seed_security(self, security):
        pass

class FuncSecuritySeeder:
    def __init__(self, func=None):
        self._func = func

# DefaultBrokerageMessageHandler
class DefaultBrokerageMessageHandler:
    pass

# Currencies
class Currencies:
    USD = "USD"
    EUR = "EUR"
    GBP = "GBP"
    JPY = "JPY"
    AUD = "AUD"
    CAD = "CAD"
    CHF = "CHF"
    NullCurrency = ""

# AccumulativeInsightPortfolioConstructionModel
class AccumulativeInsightPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs):
        pass
    def create_targets(self, algorithm, insights):
        return []

# ConfidenceWeightedPortfolioConstructionModel
class ConfidenceWeightedPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs):
        pass
    def create_targets(self, algorithm, insights):
        if not insights:
            return []
        total = sum(abs(i.confidence or 1) for i in insights)
        if total == 0:
            total = 1
        return [PortfolioTarget(i.symbol, (i.confidence or 1)/total * (1 if i.direction == InsightDirection.UP else -1))
                for i in insights]

# SectorWeightingPortfolioConstructionModel
class SectorWeightingPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs):
        pass

# LocalDiskShortableProvider
class LocalDiskShortableProvider:
    pass

# ClassicRangeConsolidator
class ClassicRangeConsolidator:
    def __init__(self, range_size, data_type=None):
        self._range_size = range_size
        self.data_consolidated = None

# Delistings
class Delistings:
    def __init__(self):
        self._data = {}
    def __contains__(self, key):
        return str(key) in self._data
    def __getitem__(self, key):
        return self._data.get(str(key))
    def contains_key(self, key):
        return str(key) in self._data

class DelistingType:
    WARNING = 0
    DELISTED = 1
    Warning = 0
    Delisted = 1

# Dividends
class Dividends(dict):
    pass

class Splits(dict):
    pass

# AlgorithmMode
class AlgorithmMode:
    BACKTESTING = 0
    RESEARCH = 1
    LIVE = 2
    Backtesting = 0
    Research = 1
    Live = 2

# DeploymentTarget
class DeploymentTarget:
    LOCAL = 0
    CLOUD = 1
    Local = 0
    Cloud = 1

# MarketHoursDatabase
class MarketHoursDatabase:
    @staticmethod
    def from_data_folder():
        return MarketHoursDatabase()
    def get_exchange_hours(self, market, symbol, security_type):
        return ExchangeHours()

# SymbolCache
class SymbolCache:
    @staticmethod
    def get_symbol(ticker):
        return Symbol(ticker)

# DataMapping
class DataMappingMode:
    LAST_TRADING_DAY = 0
    FIRST_DAY_MONTH = 1
    OPEN_INTEREST = 2
    OPEN_INTEREST_ANNUAL = 3

    LastTradingDay = 0
    FirstDayMonth = 1
    OpenInterest = 2
    OpenInterestAnnual = 3

# ContractMultiplierProvider
class ContractMultiplierProvider:
    pass

# DateTime alias for .NET compatibility
DateTime = datetime

# BaseData base class
class BaseData:
    """Base class for all data types."""
    def __init__(self):
        self.symbol = None
        self.time = None
        self.end_time = None
        self.value = 0.0

# FeeModel base
class FeeModel:
    """Fee model base class."""
    def get_order_fee(self, parameters):
        return OrderFee(0)

class ConstantFeeModel(FeeModel):
    def __init__(self, fee=0):
        self._fee = fee
    def get_order_fee(self, parameters):
        return OrderFee(self._fee)

class OrderFee:
    def __init__(self, value=0, currency="USD"):
        self.value = CashAmount(value, currency)
    @staticmethod
    def zero(currency="USD"):
        return OrderFee(0, currency)

class CashAmount:
    def __init__(self, amount=0, currency="USD"):
        self.amount = amount
        self.currency = currency
    Zero = None
CashAmount.Zero = CashAmount(0, "USD")

# UpdateOrderRequest
class UpdateOrderRequest:
    def __init__(self, *args, **kwargs):
        self.order_id = 0
        self.limit_price = None
        self.stop_price = None
        self.quantity = None
        self.tag = None

# SlippageModel
class SlippageModel:
    def get_slippage_approximation(self, asset, order):
        return 0.0

class ConstantSlippageModel(SlippageModel):
    def __init__(self, slippage=0):
        self._slippage = slippage
    def get_slippage_approximation(self, asset, order):
        return self._slippage

# VolatilityModel
class VolatilityModel:
    def __init__(self):
        self.volatility = 0.0
    @staticmethod
    def null():
        return VolatilityModel()

# BuyingPowerModel
class BuyingPowerModel:
    def get_maximum_order_quantity_for_target_buying_power(self, *args):
        return type('obj', (object,), {'quantity': 0, 'is_successful': True})()
    def get_buying_power(self, *args):
        return type('obj', (object,), {'value': 0})()

class SecurityMarginModel(BuyingPowerModel):
    def __init__(self, leverage=1.0):
        self.leverage = leverage

# SettlementModel
class ImmediateSettlementModel:
    pass

class DelayedSettlementModel:
    def __init__(self, days=0, hours=0):
        self._days = days
        self._hours = hours

# ImmediateFillModel
class ImmediateFillModel(FillModel):
    pass

# SecurityDataFilter
class SecurityDataFilter:
    def filter(self, vehicle, data):
        return True

# IPortfolioTarget alias
IPortfolioTarget = PortfolioTarget

# BrokerageMessageEvent
class BrokerageMessageEvent:
    def __init__(self, type_=0, code=0, message=""):
        self.type = type_
        self.code = code
        self.message = message

class BrokerageMessageType:
    INFORMATION = 0
    WARNING = 1
    ERROR = 2
    DISCONNECT = 3
    RECONNECT = 4
    Information = 0
    Warning = 1
    Error = 2
    Disconnect = 3
    Reconnect = 4

# SymbolChangedEvents
class SymbolChangedEvents(dict):
    pass

# Provide System.Collections.Generic stub
class List(list):
    """Stub for System.Collections.Generic.List"""
    def __init__(self, *args):
        if args and isinstance(args[0], type):
            super().__init__()
        else:
            super().__init__(args)

# TimeSpan - custom class with from_days/from_hours etc.
class TimeSpan(timedelta):
    """timedelta subclass with LEAN-compatible static factory methods."""

    @staticmethod
    def from_days(days):
        return timedelta(days=days)

    @staticmethod
    def from_hours(hours):
        return timedelta(hours=hours)

    @staticmethod
    def from_minutes(minutes):
        return timedelta(minutes=minutes)

    @staticmethod
    def from_seconds(seconds):
        return timedelta(seconds=seconds)

    @staticmethod
    def from_milliseconds(ms):
        return timedelta(milliseconds=ms)

    FromDays = from_days
    FromHours = from_hours
    FromMinutes = from_minutes
    FromSeconds = from_seconds
    FromMilliseconds = from_milliseconds

# RollingWindow - circular buffer
class RollingWindow:
    """Rolling window (circular buffer) for storing last N values."""
    def __init__(self, size_or_type=None, size=None):
        if size is not None:
            self._size = size
        elif isinstance(size_or_type, int):
            self._size = size_or_type
        else:
            self._size = 10
        self._data = []
        self._count = 0

    def add(self, item):
        self._data.insert(0, item)
        if len(self._data) > self._size:
            self._data.pop()
        self._count += 1

    def __getitem__(self, index):
        return self._data[index]

    def __len__(self):
        return len(self._data)

    @property
    def count(self):
        return len(self._data)

    @property
    def is_ready(self):
        return len(self._data) >= self._size

    @property
    def most_recently_removed(self):
        return None

    def __iter__(self):
        return iter(self._data)

    def reset(self):
        self._data.clear()

    Count = property(lambda self: self.count)
    IsReady = property(lambda self: self.is_ready)
    MostRecentlyRemoved = property(lambda self: self.most_recently_removed)
    def Add(self, item): return self.add(item)
    def Reset(self): return self.reset()

# ETFConstituentData
class ETFConstituentData:
    def __init__(self):
        self.symbol = None
        self.weight = 0.0
        self.shares_held = 0
        self.market_value = 0.0

# CoarseFundamentalUniverseSelectionModel alias
CoarseFundamentalUniverseSelectionModel = FundamentalUniverseSelectionModel

# FineFundamentalUniverseSelectionModel alias
FineFundamentalUniverseSelectionModel = FundamentalUniverseSelectionModel

# Random number (Python's random)
import random as _random_mod
Random = _random_mod.Random

# Delay stub
class Delay:
    def __init__(self, period=1):
        self._period = period
        self.is_ready = False
        self.current = type('obj', (object,), {'value': 0.0})()
    def update(self, *args):
        self.is_ready = True
        return True

# Option type
class Option:
    def __init__(self, symbol=None):
        self.symbol = symbol
        self.underlying = None
        self.contract_filter = None
        self.price_model = None
        self.strike_price = 0.0
        self.expiry = None
        self.right = None

    def set_filter(self, *args, **kwargs):
        return self

    def SetFilter(self, *args, **kwargs):
        return self.set_filter(*args, **kwargs)

# IAlgorithm alias
IAlgorithm = QCAlgorithm

# MarketOnCloseOrder / MarketOnOpenOrder
class MarketOnCloseOrder:
    def __init__(self, symbol=None, quantity=0, tag=""):
        self.symbol = symbol
        self.quantity = quantity
        self.tag = tag

class MarketOnOpenOrder:
    def __init__(self, symbol=None, quantity=0, tag=""):
        self.symbol = symbol
        self.quantity = quantity
        self.tag = tag

# EquityExchange
class EquityExchange:
    def __init__(self):
        self.hours = ExchangeHours()
        self.time_zone = "America/New_York"

# IndiaOrderProperties - fix to accept exchange arg
class IndiaOrderProperties:
    def __init__(self, exchange=None):
        self.exchange = exchange or ""

# Futures.Dairy etc.
class FuturesDairy:
    BUTTER = "butter"
    CLASS_III_MILK = "class_iii_milk"
    CASH_SETTLED_CHEESE = "cash_settled_cheese"
    Butter = "butter"
    ClassIIIMilk = "class_iii_milk"
    CashSettledCheese = "cash_settled_cheese"

if not hasattr(Futures, 'Dairy'):
    Futures.Dairy = FuturesDairy

# OptionPriceModels.barone_adesi_whaley
if not hasattr(OptionPriceModels, 'barone_adesi_whaley'):
    OptionPriceModels.barone_adesi_whaley = staticmethod(lambda: None)
    OptionPriceModels.BaroneAdesiWhaley = staticmethod(lambda: None)

# UniverseSettings.schedule attribute
if not hasattr(UniverseSettings, 'schedule'):
    pass  # handled in universe.py

# SecurityIdentifier.generate_constituent_identifier
if not hasattr(SecurityIdentifier, 'generate_constituent_identifier'):
    SecurityIdentifier.generate_constituent_identifier = staticmethod(
        lambda data_mapping_symbol, constituent_symbol, *args: SecurityIdentifier()
    )
    SecurityIdentifier.GenerateConstituentIdentifier = SecurityIdentifier.generate_constituent_identifier

# EventHandler export for consolidators
from fincept_engine.consolidators import EventHandler

# Make RollingWindow subscriptable: RollingWindow[float](5)
class _RollingWindowMeta(type):
    def __getitem__(cls, item):
        return cls
RollingWindow = _RollingWindowMeta('RollingWindow', (RollingWindow,), dict(RollingWindow.__dict__))

# MarginCallModel stub
class MarginCallModel:
    @staticmethod
    def null():
        return MarginCallModel()
    def get_margin_call_orders(self, *args):
        return []

class DefaultMarginCallModel(MarginCallModel):
    def __init__(self, *args, **kwargs):
        pass

# SecurityPositionGroupModel stub
class SecurityPositionGroupModel:
    def __init__(self, *args, **kwargs):
        pass
    def get_buying_power_for_strategy(self, *args):
        return type('obj', (object,), {'value': 0})()

# ClassicRenkoConsolidator alias
from fincept_engine.consolidators import RenkoConsolidator
ClassicRenkoConsolidator = RenkoConsolidator

# HistoricalReturnsAlphaModel - ensure available at top level
try:
    from Alphas import HistoricalReturnsAlphaModel as _HRAM
except ImportError:
    _HRAM = None

# SecurityIdentifier.upper() support
_orig_si_init = SecurityIdentifier.__init__ if hasattr(SecurityIdentifier, '__init__') else None
def _si_upper(self):
    return str(self)
SecurityIdentifier.upper = _si_upper

# option_chain_provider fix - should return OptionChainProvider instance, not the method
# (handled by making it a property returning an instance)

# AlgorithmMode comparison fix
AlgorithmMode.BACKTESTING = 'backtesting'
AlgorithmMode.Backtesting = 'backtesting'

# SubscriptionDataSource for custom data
class SubscriptionDataSource:
    def __init__(self, source="", transport_medium=0, format_type=0):
        self.source = source
        self.transport_medium = transport_medium
        self.format = format_type

class SubscriptionTransportMedium:
    LOCAL_FILE = 0
    REMOTE_FILE = 1
    REST = 2
    LocalFile = 0
    RemoteFile = 1
    Rest = 2

class FileFormat:
    CSV = 0
    BINARY = 1
    COLLECTION = 2
    Csv = 0
    UnfoldingBrowser = 3
