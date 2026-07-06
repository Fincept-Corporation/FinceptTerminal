# ============================================================================
# Fincept Terminal - Algorithm Imports Compatibility Layer
# Drop-in replacement for QuantConnect's AlgorithmImports.py
# Routes all imports to fincept_engine modules (pure Python, no .NET)
# ============================================================================

# --- Pre-register module intercepts to prevent pythonnet/QuantConnect crashes ---
import sys as _sys_boot
import types as _types_boot

# Block QuantConnect .NET imports - redirect to our stubs
def _register_stub_module(name, attrs=None):
    """Register a stub module in sys.modules to prevent .NET import crashes."""
    if name not in _sys_boot.modules or _should_override(name):
        mod = _types_boot.ModuleType(name)
        if attrs:
            for k, v in attrs.items():
                setattr(mod, k, v)
        # Make sub-attribute access return stubs
        mod.__path__ = []  # Mark as package so submodule imports work
        _sys_boot.modules[name] = mod
    return _sys_boot.modules[name]

def _should_override(name):
    """Check if existing module is broken (pythonnet) and should be overridden."""
    try:
        mod = _sys_boot.modules.get(name)
        if mod and hasattr(mod, '__file__') and mod.__file__ and 'pythonnet' in str(mod.__file__):
            return True
        if mod and hasattr(mod, '__loader__') and 'clr' in str(type(mod.__loader__)):
            return True
    except Exception:
        return True
    return False

# Pre-register QuantConnect stubs (prevents pythonnet .NET assembly load crashes)
for _qc_sub in ['QuantConnect', 'QuantConnect.Algorithm', 'QuantConnect.Algorithm.Framework',
                 'QuantConnect.Algorithm.Framework.Alphas', 'QuantConnect.Algorithm.Framework.Portfolio',
                 'QuantConnect.Algorithm.Framework.Execution', 'QuantConnect.Algorithm.Framework.Risk',
                 'QuantConnect.Algorithm.Framework.Selection',
                 'QuantConnect.Data', 'QuantConnect.Data.Custom', 'QuantConnect.Data.Custom.Tiingo',
                 'QuantConnect.Data.Custom.Intrinio',
                 'QuantConnect.Data.Market', 'QuantConnect.Data.UniverseSelection',
                 'QuantConnect.Data.Fundamental',
                 'QuantConnect.Orders', 'QuantConnect.Orders.Fees', 'QuantConnect.Orders.Fills',
                 'QuantConnect.Orders.Slippage', 'QuantConnect.Orders.OptionExercise',
                 'QuantConnect.Securities', 'QuantConnect.Securities.Option',
                 'QuantConnect.Securities.Option.StrategyMatcher',
                 'QuantConnect.Securities.Future',
                 'QuantConnect.Securities.Equity',
                 'QuantConnect.Securities.Positions',
                 'QuantConnect.Indicators',
                 'QuantConnect.Brokerages',
                 'QuantConnect.Python',
                 'QuantConnect.Util',
                 'QuantConnect.Data.Custom.IconicTypes',
                 'QuantConnect.Data.Custom.Tiingo',
                 'QuantConnect.Data.Auxiliary',
                 'QuantConnect.Algorithm.CSharp',
                 'QuantConnect.Lean.Engine.DataFeeds',
                 'QuantConnect.Logging']:
    _register_stub_module(_qc_sub)

# Populate specific QC modules with needed attributes
_qc_orders = _sys_boot.modules.get('QuantConnect.Orders')
if _qc_orders:
    _qc_orders.OrderEvent = type('OrderEvent', (), {
        '__init__': lambda self, *a, **kw: None
    })

# OptionExercise stubs
_qc_opt_exercise = _sys_boot.modules.get('QuantConnect.Orders.OptionExercise')
if _qc_opt_exercise:
    _qc_opt_exercise.OptionExerciseOrder = type('OptionExerciseOrder', (), {
        '__init__': lambda self, *a, **kw: [
            setattr(self, 'symbol', kw.get('symbol')),
            setattr(self, 'quantity', kw.get('quantity', 0)),
            setattr(self, 'tag', kw.get('tag', '')),
            setattr(self, 'type', 0),
            setattr(self, 'status', 0),
            setattr(self, 'direction', 0),
        ],
    })

_qc_sec_pos = _sys_boot.modules.get('QuantConnect.Securities.Positions')
if _qc_sec_pos:
    _qc_sec_pos.IPositionGroup = type('IPositionGroup', (), {})

# Tiingo data type
_qc_tiingo = _sys_boot.modules.get('QuantConnect.Data.Custom.Tiingo')
if _qc_tiingo:
    class _TiingoPrice:
        def __init__(self):
            self.symbol = None; self.time = None; self.close = 0; self.value = 0
            self.high = 0; self.low = 0; self.open = 0; self.volume = 0
    _TiingoPrice.set_auth_code = staticmethod(lambda code: None)
    _TiingoPrice.SetAuthCode = staticmethod(lambda code: None)
    _qc_tiingo.TiingoPrice = _TiingoPrice
    _qc_tiingo.TiingoDailyData = _TiingoPrice
    _qc_tiingo.Tiingo = _TiingoPrice
    _qc_tiingo.TiingoNews = type('TiingoNews', (), {
        '__init__': lambda self: None
    })

# IconicTypes data stubs
_qc_iconic = _sys_boot.modules.get('QuantConnect.Data.Custom.IconicTypes')
if _qc_iconic:
    class _LinkedData:
        requires_mapping_to_underlying = True  # Linked types need symbol cache lookup
        def __init__(self):
            self.symbol = None; self.time = None; self.value = 0
            self.count = 0; self.linked_symbol = None
    _qc_iconic.LinkedData = _LinkedData
    class _UnlinkedData:
        requires_mapping_to_underlying = False  # Unlinked types don't need cache lookup
        def __init__(self):
            self.symbol = None; self.time = None; self.value = 0
    _qc_iconic.UnlinkedData = _UnlinkedData
    _qc_iconic.UnlinkedDataTradeBar = type('UnlinkedDataTradeBar', (), {
        '__init__': lambda self: None
    })

# Auxiliary data stubs
_qc_aux = _sys_boot.modules.get('QuantConnect.Data.Auxiliary')
if _qc_aux:
    _qc_aux.MapFileResolver = type('MapFileResolver', (), {'Empty': None})

# Algorithm.CSharp stubs
_qc_csharp = _sys_boot.modules.get('QuantConnect.Algorithm.CSharp')
if _qc_csharp:
    _qc_csharp.CustomPartialFillModel = type('CustomPartialFillModel', (), {
        '__init__': lambda self, *a, **kw: None
    })
    _qc_csharp.TestOptionAssignmentModel = type('TestOptionAssignmentModel', (), {
        '__init__': lambda self, *a, **kw: None
    })
    _qc_csharp.TestOptionExerciseModel = type('TestOptionExerciseModel', (), {
        '__init__': lambda self, *a, **kw: None
    })
    # OptionAssignmentRegressionAlgorithm stub base class
    _qc_csharp.OptionAssignmentRegressionAlgorithm = type('OptionAssignmentRegressionAlgorithm', (), {
        '__init__': lambda self, *a, **kw: None
    })

# DataFeeds stubs
_qc_df = _sys_boot.modules.get('QuantConnect.Lean.Engine.DataFeeds')
if _qc_df:
    _qc_df.DefaultDataProvider = type('DefaultDataProvider', (), {
        '__init__': lambda self, *a, **kw: None
    })

# Logging stubs
_qc_log = _sys_boot.modules.get('QuantConnect.Logging')
if _qc_log:
    _qc_log.Log = type('Log', (), {
        'trace': staticmethod(lambda msg: None),
        'debug': staticmethod(lambda msg: None),
        'error': staticmethod(lambda msg: None),
        'Trace': staticmethod(lambda msg: None),
        'Debug': staticmethod(lambda msg: None),
        'Error': staticmethod(lambda msg: None),
    })

# Pre-register System stubs with common types
_sys_mod = _register_stub_module('System', {
    'Action': type('Action', (), {'__init__': lambda self, *a: None, '__getitem__': classmethod(lambda cls, *a: cls)}),
    'Func': type('Func', (), {'__init__': lambda self, *a: None, '__getitem__': classmethod(lambda cls, *a: cls)}),
    'Nullable': type('Nullable', (), {'__getitem__': classmethod(lambda cls, *a: cls)}),
    'TimeSpan': type('TimeSpan', (), {}),
    'DateTime': type('DateTime', (), {}),
    'String': str,
    'Int32': int,
    'Double': float,
    'Boolean': bool,
    'Decimal': float,
    'Object': object,
})
# Make Action subscriptable: System.Action[Type1, Type2]
class _SubscriptableType:
    def __init__(self, name='Type'):
        self.__name__ = name
    def __getitem__(self, item):
        return self
    def __call__(self, *args, **kwargs):
        return None
    def __instancecheck__(cls, instance):
        return True
_sys_mod.Action = _SubscriptableType('Action')
_sys_mod.Func = _SubscriptableType('Func')
_sys_mod.Nullable = _SubscriptableType('Nullable')

_register_stub_module('System.Collections', {})
class _CSharpList(list):
    """C# List<T> compatible list with .add() method."""
    def add(self, item): self.append(item)
    def Add(self, item): self.append(item)
    def remove(self, item): super().remove(item)
    def Remove(self, item): super().remove(item)
    @property
    def count(self): return len(self)
    Count = count
_register_stub_module('System.Collections.Generic', {
    'List': _CSharpList,
    'Dictionary': dict,
    'IEnumerable': list,
    'HashSet': set,
})
_register_stub_module('System.Drawing', {'Color': type('Color', (), {
    'Red': 'red', 'Green': 'green', 'Blue': 'blue', 'White': 'white',
    'Black': 'black', 'Yellow': 'yellow', 'Orange': 'orange',
})})
_register_stub_module('System.IO', {})
_register_stub_module('System.Globalization', {'CultureInfo': type('CultureInfo', (), {
    'InvariantCulture': None
})})

# Pre-register clr stub (prevents pythonnet crash)
_register_stub_module('clr', {'AddReference': lambda *a: None})

# Pre-register Newtonsoft stub
_register_stub_module('Newtonsoft', {})
_jc = type('JsonConvert', (), {
    'SerializeObject': staticmethod(lambda obj, *a, **kw: '{}'),
    'DeserializeObject': staticmethod(lambda s, *a, **kw: {}),
    'serialize_object': staticmethod(lambda obj, *a, **kw: '{}'),
    'deserialize_object': staticmethod(lambda s, *a, **kw: {}),
})
_register_stub_module('Newtonsoft.Json', {'JsonConvert': _jc})

# Pre-register keras/tensorflow stubs if not available
if 'keras' not in _sys_boot.modules:
    try:
        import keras as _keras_test
    except ImportError:
        _keras_mod = _register_stub_module('keras')
        _keras_models = _register_stub_module('keras.models', {
            'Sequential': type('Sequential', (), {
                '__init__': lambda self, *a, **kw: setattr(self, '_layers', []),
                'add': lambda self, *a, **kw: self._layers.append(a[0]) if hasattr(self, '_layers') else None,
                'compile': lambda self, *a, **kw: None,
                'fit': lambda self, *a, **kw: type('History', (), {'history': {'loss': [0.1]}})(),
                'predict': lambda self, *a, **kw: [[0.0]],
                'save': lambda self, *a, **kw: None,
                'load_model': staticmethod(lambda *a, **kw: None),
                'train_on_batch': lambda self, *a, **kw: 0.1,
                'get_weights': lambda self: [[0.0]],
                'set_weights': lambda self, *a, **kw: None,
                '__getitem__': lambda self, idx: type('Layer', (), {
                    'output': None, 'input': None, 'name': f'layer_{idx}',
                    'get_weights': lambda self: [[0.0]],
                })(),
                '__len__': lambda self: len(getattr(self, '_layers', [])),
            })
        })
        _keras_layers = _register_stub_module('keras.layers', {
            'Dense': type('Dense', (), {'__init__': lambda self, *a, **kw: None}),
            'Activation': type('Activation', (), {'__init__': lambda self, *a, **kw: None}),
            'LSTM': type('LSTM', (), {'__init__': lambda self, *a, **kw: None}),
            'Dropout': type('Dropout', (), {'__init__': lambda self, *a, **kw: None}),
        })
        _keras_optimizers = _register_stub_module('keras.optimizers', {
            'SGD': type('SGD', (), {'__init__': lambda self, *a, **kw: None}),
            'Adam': type('Adam', (), {'__init__': lambda self, *a, **kw: None}),
        })
        _keras_mod.models = _keras_models
        _keras_mod.layers = _keras_layers
        _keras_mod.optimizers = _keras_optimizers

if 'tensorflow' not in _sys_boot.modules:
    try:
        import tensorflow as _tf_test
    except ImportError:
        _tf_mod = _register_stub_module('tensorflow')
        _tf_mod.keras = _sys_boot.modules.get('keras', _types_boot.ModuleType('keras'))
        _register_stub_module('tensorflow.keras', _tf_mod.keras.__dict__ if hasattr(_tf_mod.keras, '__dict__') else {})
        # tensorflow.compat.v1 stub
        _tf_compat = _register_stub_module('tensorflow.compat')
        _tf_v1 = _register_stub_module('tensorflow.compat.v1', {
            'Session': type('Session', (), {
                '__init__': lambda self, *a, **kw: None,
                'run': lambda self, *a, **kw: [],
                '__enter__': lambda self: self,
                '__exit__': lambda self, *a: None,
            }),
            'placeholder': lambda *a, **kw: None,
            'Variable': lambda *a, **kw: None,
            'global_variables_initializer': lambda: None,
            'train': type('train', (), {
                'GradientDescentOptimizer': type('GradientDescentOptimizer', (), {
                    '__init__': lambda self, *a, **kw: None,
                    'minimize': lambda self, *a, **kw: None,
                }),
            })(),
            'reduce_mean': lambda *a, **kw: None,
            'square': lambda *a, **kw: None,
            'matmul': lambda *a, **kw: None,
            'add': lambda *a, **kw: None,
            'nn': type('nn', (), {
                'relu': lambda *a, **kw: None,
                'sigmoid': lambda *a, **kw: None,
            })(),
            'float32': 'float32',
            'disable_v2_behavior': lambda: None,
        })
        _tf_compat.v1 = _tf_v1
        _tf_mod.compat = _tf_compat

del _sys_boot, _types_boot, _register_stub_module, _should_override

# --- Core Algorithm ---
from .algorithm import (
    QCAlgorithm, AlphaModel, PortfolioConstructionModel,
    ExecutionModel, RiskManagementModel, UniverseSelectionModel,
    NullRiskManagementModel, NullAlphaModel,
    Chart, Series, SeriesType, OptionChainProvider, SecurityChanges,
    Transactions, ObjectStore, NotificationManager, SubscriptionManager
)

# --- Framework Models: Alpha ---
from .algorithm import ConstantAlphaModel, CompositeAlphaModel

# --- Framework Models: Portfolio Construction ---
from .algorithm import (
    EqualWeightingPortfolioConstructionModel,
    InsightWeightingPortfolioConstructionModel,
    MeanVarianceOptimizationPortfolioConstructionModel,
    BlackLittermanOptimizationPortfolioConstructionModel
)

# --- Framework Models: Execution ---
from .algorithm import (
    ImmediateExecutionModel,
    VolumeWeightedAveragePriceExecutionModel,
    StandardDeviationExecutionModel
)

# --- Framework Models: Risk Management ---
from .algorithm import (
    MaximumDrawdownPercentPortfolio,
    MaximumDrawdownPercentPerSecurity,
    MaximumUnrealizedProfitPercentPerSecurity,
    TrailingStopRiskManagementModel,
    MaximumSectorExposureRiskManagementModel,
    CompositeRiskManagementModel
)

# --- Framework Models: Universe Selection ---
from .algorithm import (
    ManualUniverseSelectionModel,
    FundamentalUniverseSelectionModel,
    ScheduledUniverseSelectionModel,
    CustomUniverseSelectionModel
)

# --- Data Types ---
from .algorithm import (
    PythonData, PythonQuandl, InsightCollection,
    Field, Futures, Globals, SecurityIdentifier, PortfolioBias
)

# --- Enums ---
from .enums import (
    Resolution, SecurityType, OrderType, OrderStatus,
    Market, InsightDirection, InsightType, OrderDirection,
    TimeInForce, DataNormalizationMode, MovingAverageType,
    OptionRight, OptionStyle, BrokerageName, AccountType,
    PositionSide, SettlementType
)

# --- Core Types ---
from .types import (
    Symbol, Slice, TradeBar, QuoteBar, Tick,
    SecurityHolding, OrderTicket, OrderEvent, UpdateOrderFields,
    Insight, InsightScore, PortfolioTarget, IndicatorValue
)

# --- Indicators ---
from .indicators import (
    IndicatorBase, IndicatorDataPoint,
    ExponentialMovingAverage, SimpleMovingAverage,
    MovingAverageConvergenceDivergence, RelativeStrengthIndex,
    BollingerBands, AverageTrueRange, Stochastic,
    RateOfChange, Momentum, WilliamsPercentR,
    CommodityChannelIndex, AverageDirectionalIndex
)

# --- Portfolio ---
from .portfolio import SecurityPortfolioManager

# --- Securities ---
from .securities import SecurityManager, Security, Exchange, ExchangeHours

# --- Scheduling ---
from .scheduling import (
    ScheduleManager, DateRules, TimeRules,
    ScheduledEvent, DateRule, TimeRule
)

# --- Consolidators ---
from .consolidators import (
    TradeBarConsolidator, QuoteBarConsolidator,
    RenkoConsolidator, RangeConsolidator, TickConsolidator,
    EventHandler
)

# --- Universe ---
from .universe import (
    UniverseSettings, Universe, CoarseFundamental, FineFundamental,
    ValuationRatios, OperationRatios, AssetClassification,
    CompanyReference, SecurityReference
)

# --- Extensions ---
from .extensions import (
    Time, TimeZones, Extensions, IndicatorExtensions,
    BuyingPowerModelExtensions, DefaultBrokerageModel, RateOfChangePercent
)

# --- Framework Models: Alpha (from framework/alphas.py) ---
from .framework.alphas import (
    RsiAlphaModel, HistoricalReturnsAlphaModel, EmaCrossAlphaModel,
    MacdAlphaModel, PairsTradingAlphaModel, BasePairsTradingAlphaModel,
    PearsonCorrelationPairsTradingAlphaModel
)

# --- Framework Models: Portfolio (from framework/portfolio_construction.py) ---
from .framework.portfolio_construction import UnconstrainedMeanVariancePortfolioOptimizer

# --- Framework Models: Execution (from framework/execution.py) ---
from .framework.execution import SpreadExecutionModel

# --- Framework Models: Selection (from framework/selection.py) ---
from .framework.selection import (
    OptionUniverseSelectionModel, FutureUniverseSelectionModel,
    ETFConstituentsUniverseSelectionModel, QC500UniverseSelectionModel
)

# --- Standard Library (matching LEAN's AlgorithmImports) ---
from datetime import date, time, datetime, timedelta
from typing import *
import math
import json
import random as _random_mod

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


# ============================================================================
# LEAN Compatibility Aliases & Stub Types
# These provide drop-in compatibility for QuantConnect/LEAN strategy code
# ============================================================================

# --- Algorithm aliases ---
QCAlgorithmFramework = QCAlgorithm
QCAlgorithmFrameworkBridge = QCAlgorithm
IAlgorithm = QCAlgorithm

# Fix up QC stub modules that need to reference QCAlgorithm (which wasn't available during early stub setup)
import sys as _sys_fixup
_qc_csharp_fix = _sys_fixup.modules.get('QuantConnect.Algorithm.CSharp')
if _qc_csharp_fix:
    _qc_csharp_fix.OptionAssignmentRegressionAlgorithm = type('OptionAssignmentRegressionAlgorithm', (QCAlgorithm,), {
        '__init__': lambda self, *a, **kw: QCAlgorithm.__init__(self),
    })
del _sys_fixup

# --- TimeSpan (C# System.TimeSpan) ---
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

# --- DateTime alias ---
DateTime = datetime

# --- DayOfWeek ---
class DayOfWeek:
    MONDAY = 0; TUESDAY = 1; WEDNESDAY = 2; THURSDAY = 3
    FRIDAY = 4; SATURDAY = 5; SUNDAY = 6
    Monday = 0; Tuesday = 1; Wednesday = 2; Thursday = 3
    Friday = 4; Saturday = 5; Sunday = 6

# --- TickType ---
class TickType:
    TRADE = 0; QUOTE = 1; OPEN_INTEREST = 2
    Trade = 0; Quote = 1; OpenInterest = 2

# --- OptionPricingModelType ---
class OptionPricingModelType:
    BLACK_SCHOLES = 0; BINOMIAL = 1; FORWARD_TREE = 2
    BINOMIAL_COX_ROSS_RUBINSTEIN = 1; CRANK_NICOLSON_FD = 3
    BlackScholes = 0; Binomial = 1; ForwardTree = 2
    BinomialCoxRossRubinstein = 1; CrankNicolsonFD = 3

# --- OptionPriceModels ---
class OptionPriceModels:
    @staticmethod
    def crank_nicolson_fd(): return None
    @staticmethod
    def black_scholes(): return None
    @staticmethod
    def additive_equiprobabilities(): return None
    @staticmethod
    def binomial_cox_ross_rubinstein(): return None
    @staticmethod
    def binomial_jarrow_rudd(): return None
    @staticmethod
    def barone_adesi_whaley(): return None
    CrankNicolsonFD = crank_nicolson_fd
    BlackScholes = black_scholes
    BaroneAdesiWhaley = barone_adesi_whaley

# --- OptionFilterUniverse ---
class OptionFilterUniverse:
    def __init__(self):
        self._strikes_down = 0
        self._strikes_up = 0
    def strikes(self, down, up): self._strikes_down = down; self._strikes_up = up; return self
    def expiration(self, min_days, max_days): return self
    def include_weeklys(self): return self
    def calls_only(self): return self
    def puts_only(self): return self

# --- Fill & Execution Models ---
class FillModel:
    def market_fill(self, security, order): return None
    def limit_fill(self, security, order): return None
    def stop_market_fill(self, security, order): return None

class ImmediateFillModel(FillModel):
    pass

# --- PythonIndicator ---
class PythonIndicator:
    """Base class for custom Python indicators."""
    def __init__(self):
        self.name = self.__class__.__name__
        self.value = 0.0
        self.is_ready = False
        self.warm_up_period = 0
        self.current = type('obj', (object,), {'value': 0.0})()
        self.updated = EventHandler()
        self.samples = 0
        self.window = type('obj', (object,), {'count': 0, 'is_ready': False})()
    def __getattr__(self, name):
        # Ensure updated always exists even if subclass skips super().__init__
        if name == 'updated':
            self.updated = EventHandler()
            return self.updated
        if name == 'samples':
            self.samples = 0
            return 0
        if name == 'window':
            self.window = type('obj', (object,), {'count': 0, 'is_ready': False})()
            return self.window
        if name == 'current':
            self.current = type('obj', (object,), {'value': 0.0})()
            return self.current
        raise AttributeError(f"'{type(self).__name__}' object has no attribute '{name}'")
    def update(self, input_data): self.samples += 1; return False
    def __float__(self): return self.value

# --- Order ---
class Order:
    def __init__(self):
        self.id = 0; self.symbol = None; self.type = OrderType.MARKET
        self.status = OrderStatus.NEW; self.quantity = 0; self.price = 0
        self.tag = ""; self.direction = OrderDirection.BUY

# --- MarketOnCloseOrder / MarketOnOpenOrder ---
class MarketOnCloseOrder:
    def __init__(self, symbol=None, quantity=0, tag=""):
        self.symbol = symbol; self.quantity = quantity; self.tag = tag

class MarketOnOpenOrder:
    def __init__(self, symbol=None, quantity=0, tag=""):
        self.symbol = symbol; self.quantity = quantity; self.tag = tag

# --- Brokerage ---
class BrokerageModelSecurityInitializer:
    def __init__(self, brokerage_model=None, security_seeder=None):
        self._model = brokerage_model; self._seeder = security_seeder
    def initialize(self, security): pass

class SecuritySeeder:
    @staticmethod
    def null(): return SecuritySeeder()
    def seed_security(self, security): pass

SecuritySeeder.NULL = SecuritySeeder()
SecuritySeeder.Null = SecuritySeeder.NULL

class FuncSecuritySeeder:
    def __init__(self, func=None): self._func = func

class DefaultBrokerageMessageHandler:
    def __init__(self, *args, **kwargs): pass

class BrokerageMessageEvent:
    def __init__(self, type_=0, code=0, message=""):
        self.type = type_; self.code = code; self.message = message

class BrokerageMessageType:
    INFORMATION = 0; WARNING = 1; ERROR = 2; DISCONNECT = 3; RECONNECT = 4
    Information = 0; Warning = 1; Error = 2; Disconnect = 3; Reconnect = 4

# --- Fee Models ---
class FeeModel:
    def get_order_fee(self, parameters): return OrderFee(0)

class ConstantFeeModel(FeeModel):
    def __init__(self, fee=0): self._fee = fee
    def get_order_fee(self, parameters): return OrderFee(self._fee)

class OrderFee:
    def __init__(self, value=0, currency="USD"):
        self.value = CashAmount(value, currency)
    @staticmethod
    def zero(currency="USD"): return OrderFee(0, currency)

class CashAmount:
    def __init__(self, amount=0, currency="USD"):
        self.amount = amount; self.currency = currency
    Zero = None
CashAmount.Zero = CashAmount(0, "USD")
OrderFee.ZERO = OrderFee(0)
OrderFee.Zero = OrderFee.ZERO

# --- Slippage Models ---
class SlippageModel:
    def get_slippage_approximation(self, asset, order): return 0.0

class ConstantSlippageModel(SlippageModel):
    def __init__(self, slippage=0): self._slippage = slippage
    def get_slippage_approximation(self, asset, order): return self._slippage

# --- Volatility Model ---
class VolatilityModel:
    def __init__(self): self.volatility = 0.0
    @staticmethod
    def null(): return VolatilityModel()

# --- Buying Power / Margin ---
class BuyingPowerModel:
    def get_maximum_order_quantity_for_target_buying_power(self, *args):
        return type('obj', (object,), {'quantity': 0, 'is_successful': True})()
    def get_buying_power(self, *args):
        return type('obj', (object,), {'value': 0})()

class SecurityMarginModel(BuyingPowerModel):
    def __init__(self, leverage=1.0): self.leverage = leverage

# --- Settlement Models ---
class ImmediateSettlementModel:
    pass

class DelayedSettlementModel:
    def __init__(self, days=0, hours=0):
        self._days = days; self._hours = hours

# --- Update / Request Types ---
class UpdateOrderRequest:
    def __init__(self, *args, **kwargs):
        self.order_id = 0; self.limit_price = None; self.stop_price = None
        self.quantity = None; self.tag = None

# --- Currencies ---
class Currencies:
    USD = "USD"; EUR = "EUR"; GBP = "GBP"; JPY = "JPY"
    AUD = "AUD"; CAD = "CAD"; CHF = "CHF"; NullCurrency = ""

# --- Portfolio Construction Extras ---
class AccumulativeInsightPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs): pass
    def create_targets(self, algorithm, insights): return []

class ConfidenceWeightedPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs): pass
    def create_targets(self, algorithm, insights):
        if not insights: return []
        total = sum(abs(i.confidence or 1) for i in insights)
        if total == 0: total = 1
        return [PortfolioTarget(i.symbol, (i.confidence or 1)/total * (1 if i.direction == InsightDirection.UP else -1))
                for i in insights]

class SectorWeightingPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs): pass

# --- Security Data Filter ---
class SecurityDataFilter:
    def filter(self, vehicle, data): return True

# --- IPortfolioTarget alias ---
IPortfolioTarget = PortfolioTarget

# --- PortfolioTargetCollection ---
class PortfolioTargetCollection(dict):
    """Collection of portfolio targets, keyed by symbol."""
    def __init__(self, *args, **kwargs):
        super().__init__()
        self._targets = {}
    def add(self, target):
        self._targets[str(target.symbol)] = target
        self[str(target.symbol)] = target
    def add_range(self, targets):
        for t in targets:
            self.add(t)
    def clear_fulfilled(self, algorithm):
        fulfilled = [sym for sym, t in self._targets.items()
                     if not hasattr(algorithm, 'portfolio') or
                     algorithm.portfolio.get(sym, type('H', (), {'quantity': t.quantity})()).quantity == t.quantity]
        for sym in fulfilled:
            self._targets.pop(sym, None)
            self.pop(sym, None)
    def order_by_margin_impact(self, algorithm):
        return sorted(self._targets.values(), key=lambda t: abs(t.quantity), reverse=True)
    def contains(self, symbol):
        return str(symbol) in self._targets
    def is_empty(self):
        return len(self._targets) == 0
    @property
    def count(self):
        return len(self._targets)
    def __iter__(self):
        return iter(self._targets.values())
    def __len__(self):
        return len(self._targets)
    ClearFulfilled = clear_fulfilled
    OrderByMarginImpact = order_by_margin_impact
    Contains = contains
    IsEmpty = is_empty

# --- Provider stubs ---
class LocalDiskShortableProvider:
    def __init__(self, *args, **kwargs): pass

class ContractMultiplierProvider:
    pass

# --- Consolidator Extras ---
class ClassicRangeConsolidator:
    def __init__(self, range_size, data_type=None):
        self._range_size = range_size
        self.data_consolidated = EventHandler()

ClassicRenkoConsolidator = RenkoConsolidator

# --- Delistings / Dividends / Splits ---
class Delistings:
    def __init__(self): self._data = {}
    def __contains__(self, key): return str(key) in self._data
    def __getitem__(self, key): return self._data.get(str(key))
    def contains_key(self, key): return str(key) in self._data

class DelistingType:
    WARNING = 0; DELISTED = 1; Warning = 0; Delisted = 1

class Dividends(dict):
    pass

class Splits(dict):
    pass

class SymbolChangedEvents(dict):
    pass

# --- Singular data event types (for history() requests) ---
class Split:
    def __init__(self):
        self.symbol = None; self.time = None; self.split_factor = 1.0
        self.reference_price = 0.0; self.type = 0; self.value = 0.0

class MarginInterestRate:
    def __init__(self):
        self.symbol = None; self.time = None; self.interest_rate = 0.0; self.value = 0.0

class Delisting:
    def __init__(self):
        self.symbol = None; self.time = None; self.type = 0; self.value = 0.0

class SymbolChangedEvent:
    def __init__(self):
        self.symbol = None; self.time = None; self.old_symbol = ""; self.new_symbol = ""
        self.type = 0; self.value = 0.0

# --- Algorithm Mode / Deployment Target ---
class AlgorithmMode:
    BACKTESTING = 0; RESEARCH = 1; LIVE = 2
    Backtesting = 0; Research = 1; Live = 2

class DeploymentTarget:
    LOCAL = 0; CLOUD = 1; LOCAL_PLATFORM = 0; CLOUD_PLATFORM = 1
    Local = 0; Cloud = 1; LocalPlatform = 0; CloudPlatform = 1

# --- Market Hours / Symbol Cache ---
class MarketHoursDatabase:
    @staticmethod
    def from_data_folder(): return MarketHoursDatabase()
    def get_exchange_hours(self, market, symbol, security_type): return ExchangeHours()

class SymbolCache:
    @staticmethod
    def get_symbol(ticker): return Symbol(ticker)

class DataMappingMode:
    LAST_TRADING_DAY = 0; FIRST_DAY_MONTH = 1; OPEN_INTEREST = 2; OPEN_INTEREST_ANNUAL = 3
    LastTradingDay = 0; FirstDayMonth = 1; OpenInterest = 2; OpenInterestAnnual = 3

# --- BaseData ---
class BaseData:
    def __init__(self):
        self.symbol = None; self.time = None; self.end_time = None; self.value = 0.0

# --- Option type ---
class Option:
    def __init__(self, symbol=None):
        self.symbol = symbol; self.underlying = None; self.contract_filter = None
        self.price_model = None; self.strike_price = 0.0; self.expiry = None; self.right = None
    def set_filter(self, *args, **kwargs): return self
    def SetFilter(self, *args, **kwargs): return self.set_filter(*args, **kwargs)

# --- EquityExchange ---
class EquityExchange:
    def __init__(self):
        self.hours = ExchangeHours(); self.time_zone = "America/New_York"

# --- IndiaOrderProperties ---
class IndiaOrderProperties:
    def __init__(self, exchange=None): self.exchange = exchange or ""

# --- ETFConstituentData ---
class ETFConstituentData:
    def __init__(self):
        self.symbol = None; self.weight = 0.0; self.shares_held = 0; self.market_value = 0.0

# --- Fundamental data class (used by self.history(Fundamental, ...) and type checks) ---
class Fundamental:
    """Fundamental data point for a security."""
    def __init__(self, **kwargs):
        self.symbol = kwargs.get('symbol', None)
        self.price = kwargs.get('price', 0.0)
        self.end_time = kwargs.get('end_time', None)
        self.dollar_volume = kwargs.get('dollar_volume', 0.0)
        self.volume = kwargs.get('volume', 0)
        self.market_cap = kwargs.get('market_cap', 0.0)
        self.has_fundamental_data = kwargs.get('has_fundamental_data', True)
        self.valuation_ratios = kwargs.get('valuation_ratios', type('VR', (), {'pe_ratio': 0})())

# --- .NET Object replacement with reference_equals ---
class Object:
    """Python replacement for .NET System.Object."""
    @staticmethod
    def reference_equals(a, b):
        """Check if two objects are the same reference (like .NET ReferenceEquals)."""
        return a is b
    ReferenceEquals = reference_equals

# --- Fundamental Universe Selection Aliases ---
CoarseFundamentalUniverseSelectionModel = FundamentalUniverseSelectionModel
FineFundamentalUniverseSelectionModel = FundamentalUniverseSelectionModel

# --- Random ---
Random = _random_mod.Random

# --- Delay indicator stub ---
class Delay:
    def __init__(self, period=1):
        self._period = period; self.is_ready = False
        self.current = type('obj', (object,), {'value': 0.0})()
    def update(self, *args): self.is_ready = True; return True

# --- Margin Call Model ---
class MarginCallModel:
    NULL = None  # set after class definition
    @staticmethod
    def null(): return MarginCallModel()
    def get_margin_call_orders(self, *args): return []

class DefaultMarginCallModel(MarginCallModel):
    def __init__(self, *args, **kwargs): pass

MarginCallModel.NULL = MarginCallModel()
MarginCallModel.Null = MarginCallModel.NULL

# --- SecurityPositionGroupModel ---
class SecurityPositionGroupModel:
    NULL = None  # set after class definition
    def __init__(self, *args, **kwargs): pass
    def get_buying_power_for_strategy(self, *args):
        return type('obj', (object,), {'value': 0})()

SecurityPositionGroupModel.NULL = SecurityPositionGroupModel()
SecurityPositionGroupModel.Null = SecurityPositionGroupModel.NULL

# --- SubscriptionDataSource ---
class SubscriptionDataSource:
    def __init__(self, source="", transport_medium=0, format_type=0):
        self.source = source; self.transport_medium = transport_medium; self.format = format_type

class SubscriptionTransportMedium:
    LOCAL_FILE = 0; REMOTE_FILE = 1; REST = 2; OBJECT_STORE = 3
    LocalFile = 0; RemoteFile = 1; Rest = 2; ObjectStore = 3

class FileFormat:
    CSV = 0; BINARY = 1; COLLECTION = 2; Csv = 0; UnfoldingBrowser = 3

# --- System.Collections.Generic stub ---
class List(list):
    def __init__(self, *args):
        if args and isinstance(args[0], type):
            super().__init__()
        else:
            super().__init__(args)
    def add(self, item):
        self.append(item)
    def Add(self, item):
        self.append(item)

# --- RollingWindow ---
class _RollingWindowBase:
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

    def __getitem__(self, index): return self._data[index]
    def __len__(self): return len(self._data)

    @property
    def count(self): return len(self._data)
    @property
    def is_ready(self): return len(self._data) >= self._size
    @property
    def most_recently_removed(self): return None

    def __iter__(self): return iter(self._data)
    def reset(self): self._data.clear()

    Count = property(lambda self: self.count)
    IsReady = property(lambda self: self.is_ready)
    MostRecentlyRemoved = property(lambda self: self.most_recently_removed)
    def Add(self, item): return self.add(item)
    def Reset(self): return self.reset()

class _RollingWindowMeta(type):
    """Metaclass to support RollingWindow[float](5) syntax."""
    def __getitem__(cls, item): return cls

RollingWindow = _RollingWindowMeta('RollingWindow', (_RollingWindowBase,), dict(_RollingWindowBase.__dict__))

# --- ConstituentsUniverse ---
class ConstituentsUniverse:
    def __init__(self, symbol, universe_filter_func=None):
        self.symbol = symbol
        self._filter = universe_filter_func

ETFConstituentUniverse = ConstituentsUniverse

# --- SequentialConsolidator ---
class SequentialConsolidator:
    def __init__(self, first, second):
        self.first = first
        self.second = second
        self.data_consolidated = EventHandler()
    def update(self, data):
        pass

# --- VolumeRenkoConsolidator ---
class VolumeRenkoConsolidator:
    def __init__(self, bar_size, *args):
        self._bar_size = bar_size
        self.data_consolidated = EventHandler()
    def update(self, data):
        pass

# --- CandlestickSeries ---
class CandlestickSeries:
    def __init__(self, name="", index=0, unit=""):
        self.name = name

# --- PythonConsolidator ---
class PythonConsolidator:
    def __init__(self):
        self.input_type = None
        self.output_type = None
        self.data_consolidated = EventHandler()
        self.consolidated = None
        self.working_data = None
    def __getattr__(self, name):
        if name == 'data_consolidated':
            self.data_consolidated = EventHandler()
            return self.data_consolidated
        if name == 'consolidated':
            self.consolidated = None
            return None
        if name == 'working_data':
            self.working_data = None
            return None
        raise AttributeError(f"'{type(self).__name__}' object has no attribute '{name}'")
    def update(self, data): pass
    def scan(self, time): pass

# --- IDataConsolidator alias ---
IDataConsolidator = PythonConsolidator

# --- IBaseDataBar / IBaseData ---
class IBaseDataBar:
    pass

class IBaseData:
    pass

# --- ScheduledUniverse ---
class ScheduledUniverse:
    def __init__(self, date_rule, time_rule, selector, settings=None):
        self._date_rule = date_rule
        self._time_rule = time_rule
        self._selector = selector

# --- OptionChainedUniverseSelectionModel ---
class OptionChainedUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, universe_selection_model, option_filter=None):
        self._inner = universe_selection_model
        self._filter = option_filter

# --- OptionChains ---
class OptionChains(dict):
    @property
    def data_frame(self):
        try:
            import pandas as pd
            return pd.DataFrame()
        except ImportError:
            return None

# --- OpenInterestFutureUniverseSelectionModel ---
class OpenInterestFutureUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, *args, **kwargs): pass

# --- GeneratedInsightsCollection ---
class GeneratedInsightsCollection:
    def __init__(self, *args):
        self._insights = list(args) if args else []
    def __iter__(self):
        return iter(self._insights)
    def __len__(self):
        return len(self._insights)

# --- AlphaStreamsBrokerageModel ---
class AlphaStreamsBrokerageModel:
    def __init__(self, *args, **kwargs): pass

# --- InteractiveBrokersOrderProperties ---
class InteractiveBrokersOrderProperties:
    def __init__(self):
        self.time_in_force = None
        self.outside_regular_trading_hours = False

# --- CircularQueue ---
class _CircularQueueMeta(type):
    def __getitem__(cls, item): return cls

class CircularQueue(metaclass=_CircularQueueMeta):
    def __init__(self, items_or_size=None):
        from .consolidators import EventHandler
        if isinstance(items_or_size, (list, tuple)):
            self._data = list(items_or_size)
            self._size = len(self._data)
            self._index = 0
        else:
            self._size = items_or_size or 0
            self._data = []
            self._index = 0
        self.circle_completed = EventHandler()
    def enqueue(self, item):
        self._data.append(item)
        if len(self._data) > self._size:
            self._data.pop(0)
    def dequeue(self):
        if not self._data:
            return None
        item = self._data[self._index]
        self._index = (self._index + 1) % len(self._data)
        if self._index == 0:
            self.circle_completed(self, None)
        return item
    @property
    def current(self):
        if self._data:
            return self._data[self._index % len(self._data)]
        return None
    def __iter__(self):
        return iter(self._data)
    def __len__(self):
        return len(self._data)

# --- EquityFillModel ---
class EquityFillModel(FillModel):
    pass

# --- FuncRiskFreeRateInterestRateModel ---
class FuncRiskFreeRateInterestRateModel:
    def __init__(self, func=None):
        self._func = func

# --- MassIndex indicator ---
class MassIndex:
    def __init__(self, *args, **kwargs):
        self.name = "MASS"
        self.is_ready = False
        self.current = type('obj', (object,), {'value': 0.0})()
        self.warm_up_period = 25
    def update(self, *args): return True
    def __float__(self): return self.current.value

# --- StandardDeviationOfReturnsVolatilityModel ---
class StandardDeviationOfReturnsVolatilityModel:
    def __init__(self, *args, **kwargs):
        self._periods = args[0] if args else kwargs.get('periods', 252)
        self.volatility = 0.0

# --- MarketImpactSlippageModel ---
class MarketImpactSlippageModel:
    def __init__(self, *args, **kwargs):
        pass
    def get_slippage_approximation(self, asset, order):
        return 0.0

# --- Collective2/CrunchDAO/Numerai Signal Export ---
class Collective2SignalExport:
    def __init__(self, api_key="", system_id=0):
        self.api_key = api_key
        self.system_id = system_id

class Collective2PortfolioSignalExport:
    def __init__(self, api_key="", system_id=0):
        self.api_key = api_key

class CrunchDaoSignalExport:
    def __init__(self, api_key="", model="", submission_name="", comment=""):
        pass

class CrunchDaoSkeleton(PythonData):
    pass

class NumeraiSignalExport:
    def __init__(self, *args, **kwargs):
        pass

# --- NullShortableProvider ---
class NullShortableProvider:
    def __init__(self, *args, **kwargs): pass
    def shortable_quantity(self, symbol, local_time):
        return float('inf')

# --- SymbolProperties / MarginInterestRateParameters ---
class SymbolProperties:
    def __init__(self, description="", quote_currency="USD", contract_multiplier=1,
                 minimum_price_variation=0.01, lot_size=1, market_ticker=""):
        self.description = description
        self.quote_currency = quote_currency
        self.contract_multiplier = contract_multiplier
        self.minimum_price_variation = minimum_price_variation
        self.lot_size = lot_size
        self.market_ticker = market_ticker
        self.minimum_order_size = 1
        self.priceMagnifier = 1

class MarginInterestRateParameters:
    def __init__(self, *args):
        self.security = None
        self.position_group = None

# --- InvalidOperationException ---
class InvalidOperationException(Exception):
    pass

# --- RegressionTestException ---
class RegressionTestException(Exception):
    pass

# --- SubscriptionDataConfig ---
class SubscriptionDataConfig:
    def __init__(self, *args, **kwargs):
        self.symbol = None
        self.resolution = None
        self.type = None

# --- Func stub (for C# Func<> delegates) ---
class _FuncMeta(type):
    def __getitem__(cls, item): return cls

class Func(metaclass=_FuncMeta):
    def __init__(self, func=None):
        self._func = func
    def __call__(self, *args, **kwargs):
        return self._func(*args, **kwargs) if self._func else None

# --- Command ---
class Command:
    def __init__(self, *args, **kwargs):
        self.name = ""
        self.parameters = {}

    def run(self, algo) -> bool:
        """Override this method to implement command logic."""
        return True

    def my_custom_method(self):
        return False

    def __str__(self):
        return self.__class__.__name__

# --- AddReference (stub for C# assembly references) ---
def AddReference(*args):
    pass

# --- Dividend ---
class Dividend:
    def __init__(self):
        self.symbol = None
        self.distribution = 0.0
        self.reference_price = 0.0
        self.time = None

# --- Orders module stub ---
class _OrdersModule:
    """Stub for 'from Orders import ...' pattern."""
    OrderDirection = OrderDirection
    OrderType = OrderType
    OrderStatus = OrderStatus
import sys as _sys
_orders_mod = type(_sys)('Orders')
_orders_mod.OrderDirection = OrderDirection
_orders_mod.OrderType = OrderType
_orders_mod.OrderStatus = OrderStatus
_orders_mod.Fees = type('Fees', (), {'OrderFee': OrderFee, 'CashAmount': CashAmount})()
_sys.modules['Orders'] = _orders_mod
_sys.modules['Orders.Fees'] = _orders_mod.Fees

# --- FuncSecurityInitializer ---
class FuncSecurityInitializer:
    def __init__(self, func=None):
        self._func = func
    def initialize(self, security):
        if self._func:
            self._func(security)

# --- CompositeSecurityInitializer ---
class CompositeSecurityInitializer:
    def __init__(self, *initializers):
        self._initializers = list(initializers)
    def initialize(self, security):
        for init in self._initializers:
            if hasattr(init, 'initialize'):
                init.initialize(security)

# --- CrunchDAO case alias ---
CrunchDAOSignalExport = CrunchDaoSignalExport

# --- LiquidETFUniverse ---
class LiquidETFUniverse:
    SP500 = "SPY"
    SP500_SECTORS = ["XLB", "XLE", "XLF", "XLI", "XLK", "XLP", "XLRE", "XLU", "XLV", "XLY"]

# --- InceptionDateUniverseSelectionModel ---
class InceptionDateUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, *args, **kwargs): pass

# --- SecurityExchangeHours ---
SecurityExchangeHours = ExchangeHours

# --- Compression ---
class Compression:
    UNCOMPRESSED = 0; ZIP = 1; GZIP = 2; ZIP_BYTES = 3
    Uncompressed = 0; Zip = 1; GZip = 2; ZipBytes = 3

    @staticmethod
    def zip_bytes(data, entry_name='data'):
        """Compress data bytes into a zip archive."""
        import io, zipfile
        buf = io.BytesIO()
        with zipfile.ZipFile(buf, 'w', zipfile.ZIP_DEFLATED) as zf:
            zf.writestr(entry_name, data if isinstance(data, bytes) else str(data).encode('utf-8'))
        return buf.getvalue()

    @staticmethod
    def unzip_bytes(data, entry_name=None):
        """Decompress zip archive bytes."""
        import io, zipfile
        buf = io.BytesIO(data)
        with zipfile.ZipFile(buf, 'r') as zf:
            names = zf.namelist()
            target = entry_name if entry_name else names[0]
            return zf.read(target)

    ZipBytes = zip_bytes
    UnzipBytes = unzip_bytes

# --- IntrinioConfig ---
class IntrinioConfig:
    def __init__(self):
        self.api_key = ""
    @staticmethod
    def set_user_and_password(user="", password=""):
        pass
    @staticmethod
    def set_api_key(key=""):
        pass
    @staticmethod
    def set_time_interval_between_calls(interval=None):
        pass
    SetTimeIntervalBetweenCalls = set_time_interval_between_calls

# --- FillModelParameters ---
class FillModelParameters:
    def __init__(self, *args, **kwargs):
        self.security = None
        self.order = None
        self.config_provider = None

# --- EmaCrossUniverseSelectionModel (Selection submodule) ---
class EmaCrossUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, *args, **kwargs): pass

# --- MeanReversionPortfolioConstructionModel ---
class MeanReversionPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs): pass

# --- RiskParityPortfolioConstructionModel ---
class RiskParityPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs): pass

# --- VolumeShareSlippageModel ---
class VolumeShareSlippageModel(SlippageModel):
    def __init__(self, *args, **kwargs): pass

# --- Register additional Selection submodules ---
import types as _types
_sel_extras = {
    'EmaCrossUniverseSelectionModel': {'EmaCrossUniverseSelectionModel': EmaCrossUniverseSelectionModel},
    'ConstituentsUniverse': {'ConstituentsUniverse': ConstituentsUniverse},
    'ScheduledUniverse': {'ScheduledUniverse': ScheduledUniverse},
    'OptionChainedUniverseSelectionModel': {'OptionChainedUniverseSelectionModel': OptionChainedUniverseSelectionModel},
    'InceptionDateUniverseSelectionModel': {'InceptionDateUniverseSelectionModel': InceptionDateUniverseSelectionModel},
}
for _name, _attrs in _sel_extras.items():
    _mod = _types.ModuleType(f'Selection.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    import sys as _sys2
    _sys2.modules[f'Selection.{_name}'] = _mod

# --- Register Portfolio submodules ---
_port_extras = {
    'MeanReversionPortfolioConstructionModel': {'MeanReversionPortfolioConstructionModel': MeanReversionPortfolioConstructionModel},
    'RiskParityPortfolioConstructionModel': {'RiskParityPortfolioConstructionModel': RiskParityPortfolioConstructionModel},
}
for _name, _attrs in _port_extras.items():
    _mod = _types.ModuleType(f'Portfolio.{_name}')
    for _attr_name, _attr_val in _attrs.items():
        setattr(_mod, _attr_name, _attr_val)
    _sys2.modules[f'Portfolio.{_name}'] = _mod

# --- Register Orders submodules ---
_orders_slippage = _types.ModuleType('Orders.Slippage')
_orders_slippage.VolumeShareSlippageModel = VolumeShareSlippageModel
_sys2.modules['Orders.Slippage'] = _orders_slippage

# Also register submodule path so 'from Orders.Slippage.VolumeShareSlippageModel import ...' works
_orders_parent = _types.ModuleType('Orders')
_orders_parent.Slippage = _orders_slippage
_sys2.modules['Orders'] = _orders_parent
_vsm_mod = _types.ModuleType('Orders.Slippage.VolumeShareSlippageModel')
_vsm_mod.VolumeShareSlippageModel = VolumeShareSlippageModel
_sys2.modules['Orders.Slippage.VolumeShareSlippageModel'] = _vsm_mod

# --- Calendar (for consolidator CalendarType) ---
class Calendar:
    """Calendar-based consolidation types."""
    @staticmethod
    def weekly(dt):
        return dt.weekday() == 4  # Friday
    @staticmethod
    def monthly(dt):
        import calendar as _cal
        _, last_day = _cal.monthrange(dt.year, dt.month)
        return dt.day == last_day
    Weekly = weekly
    Monthly = monthly
    WEEKLY = weekly
    MONTHLY = monthly

    @staticmethod
    def quarterly(dt):
        import calendar as _cal
        _, last_day = _cal.monthrange(dt.year, dt.month)
        return dt.month in (3, 6, 9, 12) and dt.day == last_day
    QUARTERLY = quarterly
    Quarterly = quarterly

    @staticmethod
    def yearly(dt):
        return dt.month == 12 and dt.day == 31
    YEARLY = yearly
    Yearly = yearly

# --- WilderAccumulativeSwingIndex indicator ---
class WilderAccumulativeSwingIndex(IndicatorBase):
    def __init__(self, name="WASI", limit_move=0):
        super().__init__(name, limit_move or 14)
    def _compute(self, value: float) -> float:
        return value

# --- Beta indicator ---
class Beta(IndicatorBase):
    def __init__(self, *args, **kwargs):
        name = args[0] if args else kwargs.get('name', 'BETA')
        period = args[1] if len(args) > 1 else kwargs.get('period', 20)
        if isinstance(name, str):
            super().__init__(name, period)
        else:
            super().__init__('BETA', 20)
    def _compute(self, value: float) -> float:
        return 1.0  # Default beta = 1

# --- WilderSwingIndex indicator ---
class WilderSwingIndex(IndicatorBase):
    def __init__(self, name="WSI", limit_move=0):
        super().__init__(name, limit_move or 14)
    def _compute(self, value: float) -> float:
        return value

# --- Identity indicator ---
class Identity(IndicatorBase):
    def __init__(self, name="ID"):
        super().__init__(name, 1)
    def _compute(self, value: float) -> float:
        return value

# --- IntrinioEconomicData ---
class IntrinioEconomicData(PythonData):
    pass

# --- FundamentalUniverse ---
class FundamentalUniverse:
    @staticmethod
    def usa(*args, **kwargs):
        return None
    USA = usa

# --- Futures.Dairy patch ---
class FuturesDairy:
    BUTTER = "butter"; CLASS_III_MILK = "class_iii_milk"; CASH_SETTLED_CHEESE = "cash_settled_cheese"
    Butter = "butter"; ClassIIIMilk = "class_iii_milk"; CashSettledCheese = "cash_settled_cheese"

if not hasattr(Futures, 'Dairy'):
    Futures.Dairy = FuturesDairy

# --- SecurityIdentifier.upper() / generate_constituent_identifier ---
if not hasattr(SecurityIdentifier, 'upper'):
    SecurityIdentifier.upper = lambda self: str(self)

if not hasattr(SecurityIdentifier, 'generate_constituent_identifier'):
    SecurityIdentifier.generate_constituent_identifier = staticmethod(
        lambda data_mapping_symbol, constituent_symbol, *args: SecurityIdentifier()
    )
    SecurityIdentifier.GenerateConstituentIdentifier = SecurityIdentifier.generate_constituent_identifier

# --- ImpliedVolatility indicator ---
class ImpliedVolatility(IndicatorBase):
    def __init__(self, *args, **kwargs):
        name = args[0] if args and isinstance(args[0], str) else 'IV'
        super().__init__(name, 1)
    def _compute(self, value: float) -> float:
        return 0.25  # Default IV

# --- Tiingo alias ---
class Tiingo:
    """Tiingo data type stub."""
    def __init__(self):
        self.symbol = None; self.time = None; self.close = 0; self.value = 0
        self.high = 0; self.low = 0; self.open = 0; self.volume = 0

class TiingoPrice(Tiingo):
    pass

class TiingoDailyData(Tiingo):
    pass

# --- LeanData ---
class LeanData:
    @staticmethod
    def generate_zip_file_name(symbol, date, resolution, tick_type=None):
        return f"{str(symbol).lower()}.zip"
    @staticmethod
    def generate_zip_entry_name(symbol, date, resolution, tick_type=None):
        return f"{str(symbol).lower()}.csv"
    GenerateZipFileName = generate_zip_file_name
    GenerateZipEntryName = generate_zip_entry_name

# --- SymbolPropertiesDatabase ---
class SymbolPropertiesDatabase:
    @staticmethod
    def from_data_folder():
        return SymbolPropertiesDatabase()
    def get_symbol_properties(self, market, symbol, security_type, currency="USD"):
        from .securities import SymbolProperties as SP
        return SP()
    def set_entry(self, *args, **kwargs):
        pass
    SetEntry = set_entry
    GetSymbolProperties = get_symbol_properties

# --- MarketHoursDatabase (alias from algorithm.py) ---
from .algorithm import _MarketHoursDatabase, _SymbolPropertiesDatabase

# --- InterestRateProvider ---
class InterestRateProvider:
    def __init__(self, *args, **kwargs):
        pass
    def get_interest_rate(self, date):
        return 0.05  # 5% default rate

# --- Tiingo top-level alias ---
class Tiingo:
    def __init__(self):
        self.symbol = None; self.time = None; self.close = 0; self.value = 0
    @staticmethod
    def set_auth_code(code):
        pass
    SetAuthCode = set_auth_code

class TiingoPrice(Tiingo):
    pass

# --- SymbolPropertiesDatabase alias ---
SymbolPropertiesDatabase = _SymbolPropertiesDatabase

# --- OptionChain (single underlying chain result type) ---
OptionChain = type('OptionChain', (), {
    '__init__': lambda self, *a, **kw: None,
    'underlying': None,
    'contracts': {},
    '__iter__': lambda self: iter([]),
    '__len__': lambda self: 0,
})

# --- BrokerageModel ---
class BrokerageModel:
    def __init__(self, *args, **kwargs): pass
    def get_fee_model(self, security): return FeeModel()
    def get_fill_model(self, security): return FillModel()
    def get_slippage_model(self, security): return SlippageModel()
    def can_submit_order(self, security, order, message=None): return True

class DefaultBrokerageModel2(BrokerageModel):
    pass

# --- CustomData alias (for IndicatorSuiteAlgorithm) ---
CustomData = type('CustomData', (), {
    '__init__': lambda self: None,
    'symbol': None, 'time': None, 'value': 0,
    'get_source': lambda self, *a: None,
    'reader': lambda self, *a: None,
})

# --- DividendYieldProvider ---
class DividendYieldProvider:
    def __init__(self, *args, **kwargs): pass
    def get_dividend_yield(self, date, *args): return 0.02

# --- LocalDiskMapFileProvider ---
class LocalDiskMapFileProvider:
    def __init__(self, *args, **kwargs): pass
    def initialize(self, *args, **kwargs): pass
    def get(self, *args, **kwargs): return None

# --- OptionAssignmentRegressionAlgorithm (referenced by subclass strategies) ---
# Stub base class so subclasses can inherit
class OptionAssignmentRegressionAlgorithm(QCAlgorithm):
    """Stub base for option assignment regression tests."""
    pass

# --- BaseVolatilityModel ---
class BaseVolatilityModel:
    """Base volatility model for securities."""
    def __init__(self, *args, **kwargs):
        self.volatility = 0.0
    def update(self, security, data): pass
    def get_history_requirements(self, security, utc_time):
        return []
    @staticmethod
    def null():
        return BaseVolatilityModel()

# --- DefaultOptionAssignmentModel ---
class DefaultOptionAssignmentModel:
    """Default option assignment model."""
    def __init__(self, *args, **kwargs): pass
    def get_assignment(self, parameters):
        return type('AssignmentResult', (), {'is_assigned': False})()

# --- DefaultExerciseModel ---
class DefaultExerciseModel:
    """Default option exercise model."""
    def __init__(self, *args, **kwargs): pass
    def get_exercise(self, parameters):
        return []

# --- ImpliedVolatility with set_smoothing_function ---
# Override the one defined earlier
class ImpliedVolatility(IndicatorBase):
    def __init__(self, *args, **kwargs):
        name = args[0] if args and isinstance(args[0], str) else 'IV'
        super().__init__(name, 1)
        self._smoothing_func = None
    def _compute(self, value: float) -> float:
        return 0.25
    def set_smoothing_function(self, func):
        self._smoothing_func = func
    def SetSmoothingFunction(self, func):
        self._smoothing_func = func

# --- OptionExerciseOrder ---
class OptionExerciseOrder:
    """Represents an option exercise order."""
    def __init__(self, symbol=None, quantity=0, tag=""):
        self.symbol = symbol
        self.quantity = quantity
        self.tag = tag
        self.type = 0
        self.status = 0
        self.direction = 0

# --- LocalDiskFactorFileProvider ---
class _FactorFile:
    """Stub factor file."""
    def get_price_scale_factor(self, time):
        return 1.0
    def GetPriceScaleFactor(self, time):
        return 1.0

class LocalDiskFactorFileProvider:
    """Local disk factor file provider stub."""
    def __init__(self, *args, **kwargs): pass
    def initialize(self, *args, **kwargs): pass
    def get(self, *args, **kwargs): return _FactorFile()

# --- CustomImpliedVolatility (for OptionIndicatorsMirror strategies) ---
class CustomImpliedVolatility(IndicatorBase):
    """Custom IV indicator that accepts option, mirror, rate model, yield model."""
    def __init__(self, *args, **kwargs):
        name = 'CustomIV'
        super().__init__(name, 1)
        self._smoothing_func = None
    def _compute(self, value: float) -> float:
        return 0.25
    def set_smoothing_function(self, func):
        self._smoothing_func = func
    def SetSmoothingFunction(self, func):
        self._smoothing_func = func
