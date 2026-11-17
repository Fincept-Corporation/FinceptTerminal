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
def _csharp_class(name):
    """A class of LEAN's C# regression suite (QuantConnect.Algorithm.CSharp): its code is not part of
    this catalogue, so using it is refused with that reason."""
    def _init(self, *a, **kw):
        from .errors import not_supported as _ns
        raise _ns(f"the C# class {name} from LEAN's C# regression algorithms (QuantConnect.Algorithm.CSharp)",
                  "Its code is not part of this strategy catalogue.")
    return type(name, (), {'__init__': _init})


if _qc_csharp:
    for _cs in ('CustomPartialFillModel', 'TestOptionAssignmentModel', 'TestOptionExerciseModel'):
        setattr(_qc_csharp, _cs, _csharp_class(_cs))
    del _cs

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

# keras / tensorflow are NOT stubbed: a strategy that imports them needs the real
# packages (a stub used to return a fixed fake prediction, i.e. an invented result).

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
    CompositeRiskManagementModel
)

# --- Framework Models: Universe Selection (framework/selection.py) ---
from .framework.selection import (
    ManualUniverseSelectionModel, FundamentalUniverseSelectionModel, ScheduledUniverseSelectionModel,
    CustomUniverseSelectionModel, CoarseFundamentalUniverseSelectionModel, FineFundamentalUniverseSelectionModel,
    EmaCrossUniverseSelectionModel, InceptionDateUniverseSelectionModel, LiquidETFUniverse,
    EnergyETFUniverse, MetalsETFUniverse, TechnologyETFUniverse, USTreasuriesETFUniverse, VolatilityETFUniverse,
    SP500SectorsETFUniverse
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
    UniverseSettings, Universe, CoarseFundamental, FineFundamental, Fundamental,
    ValuationRatios, OperationRatios, AssetClassification,
    CompanyReference, SecurityReference, ETFConstituentData, ETFConstituentUniverse, FundamentalUniverse,
    ConstituentsUniverse, ScheduledUniverse, MorningstarSectorCode, MorningstarIndustryGroupCode,
    MorningstarIndustryCode, MorningstarEconomySphereCode, UniverseDefinitions, CompanyProfile, EarningReports,
    EarningRatios, FinancialStatements, MultiPeriodField, FundamentalUniverseFactory, CoarseFundamentalUniverse,
    FineFundamentalFilteredUniverse, UserDefinedUniverse, ETFConstituentsUniverse
)

# --- Extensions ---
from .extensions import (
    Time, TimeZones, Extensions, IndicatorExtensions,
    BuyingPowerModelExtensions, DefaultBrokerageModel, RateOfChangePercent, Expiry, Math
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
from enum import Enum, IntEnum


def _enum_get_values(enum_type):
    """System.Enum.GetValues(type) as LEAN strategies call it (Enum.get_values(DataMappingMode))."""
    try:
        return [m.value for m in enum_type]
    except TypeError:                     # a plain constants class
        vals = [v for k, v in vars(enum_type).items()
                if not k.startswith('_') and k.isupper() and isinstance(v, int) and not isinstance(v, bool)]
        return sorted(dict.fromkeys(vals))


Enum.get_values = staticmethod(_enum_get_values)
Enum.GetValues = staticmethod(_enum_get_values)
from .enums import OrderField, OrderRequestStatus
import math
import json
import random as _random_mod

# numpy / pandas are exported (LEAN's AlgorithmImports does) but imported on first use:
# importing pandas eagerly cost every run ~1-2 s of cold start before any work began.
# (matplotlib used to be imported here too; no strategy uses it.)
from ._lazy import LazyModule as _LazyModule
np = _LazyModule('numpy')
pd = _LazyModule('pandas')


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
    def _csharp_base_initialize(self):
        from .errors import not_supported as _ns
        raise _ns("the C# base algorithm OptionAssignmentRegressionAlgorithm from LEAN's C# regression "
                  "algorithms (QuantConnect.Algorithm.CSharp)", "Its code is not part of this strategy catalogue.")
    _qc_csharp_fix.OptionAssignmentRegressionAlgorithm = type('OptionAssignmentRegressionAlgorithm', (QCAlgorithm,), {
        'initialize': _csharp_base_initialize,
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

# --- OptionFilterUniverse ---

# --- Fill & Execution Models ---


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
    def on_updated(self, point):
        """Publish a new value (custom indicators call this from update)."""
        v = float(getattr(point, 'value', point) or 0.0)
        self.samples = getattr(self, 'samples', 0) + 1
        cur = self.current
        try:
            cur.value = v
            cur.time = getattr(point, 'time', None)
        except AttributeError:
            pass
        self.updated(self, point)
    def __float__(self): return float(self.value)

# --- Order ---

# --- Reality models, order classes, statistics, cash book, corporate-action events (real
# implementations; see models.py / orders.py / statistics.py / cashbook.py / corporate_actions.py) ---
from .models import (CashAmount, OrderFee, OrderFeeParameters, FeeModel, ConstantFeeModel,
                     InteractiveBrokersFeeModel, SlippageModel, NullSlippageModel, ConstantSlippageModel,
                     VolumeShareSlippageModel, MarketImpactSlippageModel, AlphaStreamsSlippageModel,
                     FillModel, ImmediateFillModel, EquityFillModel, FutureFillModel, FutureOptionFillModel,
                     Fill, FillModelParameters, BuyingPowerModel, SecurityMarginModel, CashBuyingPowerModel,
                     ConstantBuyingPowerModel, NullBuyingPowerModel, PatternDayTradingMarginModel,
                     GetMaximumOrderQuantityForTargetBuyingPowerParameters, GetMaximumOrderQuantityResult,
                     HasSufficientBuyingPowerForOrderParameters, HasSufficientBuyingPowerForOrderResult,
                     MaintenanceMargin, InitialMargin, BuyingPower, ReservedBuyingPowerForPosition,
                     ReservedBuyingPowerForPositionParameters, MarginInterestRateParameters,
                     MarginInterestRateModel, MarginCallModel, DefaultMarginCallModel,
                     ImmediateSettlementModel, DelayedSettlementModel, BrokerageMessageType, BrokerageMessageEvent)
from .orders import (Order, MarketOrder, LimitOrder, StopMarketOrder, StopLimitOrder, LimitIfTouchedOrder,
                     TrailingStopOrder, ComboMarketOrder, ComboLimitOrder, ComboLegLimitOrder,
                     OptionExerciseOrder)
from .statistics import PerformanceMetrics
from .cashbook import Cash, CashBook
from .corporate_actions import Split, Dividend, SplitType
from .types import SymbolDict as _SymbolDict, KeyValuePair
from .extensions import DefaultBrokerageModel as BrokerageModel


class Delistings(_SymbolDict):
    pass


class Dividends(_SymbolDict):
    pass


class Splits(_SymbolDict):
    pass


class SymbolChangedEvents(_SymbolDict):
    pass


class NullShortableProvider:
    """No shortable-quantity limit (LEAN's null provider): every quantity is shortable."""
    def __init__(self, *args, **kwargs):
        pass

    def shortable_quantity(self, symbol, local_time):
        return None

    def fee_rate(self, symbol, local_time):
        return 0.0

    def rebate_rate(self, symbol, local_time):
        return 0.0

# --- Options and futures (model-priced chains: options.py, option_chain.py, option_model.py,
# option_strategies.py; derived futures contracts: futures.py) ---
from .option_chain import (OptionFilterUniverse, OptionChain, OptionChains, OptionContract, Greeks)
from .option_strategies import OptionStrategies, OptionStrategy, Leg
from .futures import (FutureMarginModel, FuturesChain, FuturesContract, FutureFilterUniverse, Delisting,
                      SymbolChangedEvent)


class DelistingType:
    WARNING = 0
    DELISTED = 1
    Warning = 0
    Delisted = 1


class _PriceModelChoice:
    """What option.price_model was set to. The engine prices every contract with its own model
    (Black-Scholes-Merton / Black-76, option_model.py); the choice is recorded and disclosed, and as
    in LEAN a model that does not support the contract's style (Black-Scholes and the integral
    engine: European only; Barone-Adesi-Whaley and Bjerksund-Stensland: American only) makes
    greeks / implied volatility raise ArgumentException."""
    _STYLES = {'BlackScholes': (1,), 'Integral': (1,), 'BaroneAdesiWhaley': (0,), 'BjerksundStensland': (0,)}

    def __init__(self, name):
        self.name = name
        self.allowed_styles = self._STYLES.get(name, (0, 1))      # OptionStyle: AMERICAN 0, EUROPEAN 1

    def __repr__(self):
        return f"OptionPriceModel({self.name})"


class OptionPriceModels:
    @staticmethod
    def black_scholes(): return _PriceModelChoice('BlackScholes')
    @staticmethod
    def crank_nicolson_fd(): return _PriceModelChoice('CrankNicolsonFD')
    @staticmethod
    def barone_adesi_whaley(): return _PriceModelChoice('BaroneAdesiWhaley')
    @staticmethod
    def bjerksund_stensland(): return _PriceModelChoice('BjerksundStensland')
    @staticmethod
    def binomial_cox_ross_rubinstein(): return _PriceModelChoice('BinomialCoxRossRubinstein')
    @staticmethod
    def binomial_jarrow_rudd(): return _PriceModelChoice('BinomialJarrowRudd')
    @staticmethod
    def additive_equiprobabilities(): return _PriceModelChoice('AdditiveEquiprobabilities')
    @staticmethod
    def binomial_tian(): return _PriceModelChoice('BinomialTian')
    @staticmethod
    def binomial_leisen_reimer(): return _PriceModelChoice('BinomialLeisenReimer')
    @staticmethod
    def binomial_trigeorgis(): return _PriceModelChoice('BinomialTrigeorgis')
    @staticmethod
    def integral(): return _PriceModelChoice('Integral')
    @staticmethod
    def black_scholes_merton(): return _PriceModelChoice('BlackScholes')
    BlackScholes = black_scholes
    CrankNicolsonFD = crank_nicolson_fd
    BaroneAdesiWhaley = barone_adesi_whaley
    BjerksundStensland = bjerksund_stensland
    BinomialCoxRossRubinstein = binomial_cox_ross_rubinstein


def _greek_indicator_class(kind):
    from .algorithm import _OptionGreekIndicator

    class _G(_OptionGreekIndicator):
        def __init__(self, name=None, option=None, *args, **kwargs):
            sym = option if option is not None else (name if not isinstance(name, str) else None)
            label = name if isinstance(name, str) else f"{kind.upper()}({sym})"
            super().__init__(label, kind)
            self.option_symbol = sym

        def update(self, *args, **kwargs):
            # values come from the option model each close (fed by the option manager when the indicator
            # is registered with algorithm.iv/d/g/v/t/r); manual feeding uses IndicatorBase.update
            return super().update(*args, **kwargs)
    _G.__name__ = {'iv': 'ImpliedVolatility', 'delta': 'Delta', 'gamma': 'Gamma', 'vega': 'Vega',
                   'theta': 'Theta', 'rho': 'Rho'}[kind]
    return _G


ImpliedVolatility = _greek_indicator_class('iv')
Delta = _greek_indicator_class('delta')
Gamma = _greek_indicator_class('gamma')
Vega = _greek_indicator_class('vega')
Theta = _greek_indicator_class('theta')
Rho = _greek_indicator_class('rho')
CustomImpliedVolatility = ImpliedVolatility


class DefaultExerciseModel:
    """LEAN's default exercise model: in-the-money options are exercised automatically at expiry
    (options.py implements it)."""
    def option_exercise(self, option, order):
        return []

from .custom_data import SubscriptionDataSource, SubscriptionTransportMedium, FileFormat

from .portfolio import OptionStrategyPositionGroupBuyingPowerModel, SecurityPositionGroupBuyingPowerModel

# --- Order requests / responses (fincept_engine.orders) ---
from .orders import (MarketOnCloseOrder, MarketOnOpenOrder, SubmitOrderRequest, UpdateOrderRequest,
                     CancelOrderRequest, OrderRequest, OrderResponse, OrderResponseErrorCode)

# --- Brokerage ---
class SecuritySeeder:
    """Seeds a new security with its last known price (LEAN ISecuritySeeder)."""
    @staticmethod
    def null(): return SecuritySeeder()
    def seed_security(self, security):
        return False

SecuritySeeder.NULL = SecuritySeeder()
SecuritySeeder.Null = SecuritySeeder.NULL

class FuncSecuritySeeder(SecuritySeeder):
    """Seeds with the data a function returns (typically algorithm.get_last_known_prices): the last
    bar complete at the algorithm's current time."""
    def __init__(self, func=None):
        self._func = func

    def seed_security(self, security):
        if self._func is None:
            return False
        data = self._func(security)
        items = data if isinstance(data, (list, tuple)) else ([data] if data is not None else [])
        seeded = False
        for item in items:
            if item is None:
                continue
            security.set_market_price(item)
            security._last_bar = item
            seeded = True
        return seeded

    SeedSecurity = seed_security

class BrokerageModelSecurityInitializer:
    """Applies the brokerage model's leverage to a new security and seeds its price."""
    def __init__(self, brokerage_model=None, security_seeder=None):
        self._model = brokerage_model; self._seeder = security_seeder
    def initialize(self, security):
        if self._seeder is not None and hasattr(self._seeder, 'seed_security'):
            self._seeder.seed_security(security)
    Initialize = initialize

class DefaultBrokerageMessageHandler:
    def __init__(self, *args, **kwargs): pass



# --- Fee Models ---




# --- Slippage Models ---


# --- Volatility Model ---
from .volatility_models import (VolatilityModel, BaseVolatilityModel, NullVolatilityModel,  # noqa: E402
                                StandardDeviationOfReturnsVolatilityModel,
                                RelativeStandardDeviationVolatilityModel, IndicatorVolatilityModel)

# --- Buying Power / Margin ---


# --- Settlement Models ---



# --- Currencies ---
class Currencies:
    USD = "USD"; EUR = "EUR"; GBP = "GBP"; JPY = "JPY"
    AUD = "AUD"; CAD = "CAD"; CHF = "CHF"; NullCurrency = ""

# --- Portfolio Construction Extras ---
from .errors import not_supported as _not_supported
from .algorithm import EqualWeightingPortfolioConstructionModel as _EqualWeightingPCM


class AccumulativeInsightPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs):
        raise _not_supported("the accumulative-insight portfolio construction model")

class ConfidenceWeightedPortfolioConstructionModel(_EqualWeightingPCM):
    """Weight each insight by its confidence (scaled down when the total exceeds 100%)."""
    def determine_target_percent(self, insights):
        conf = {i: abs(i.confidence) if i.confidence is not None else 0.0 for i in insights}
        total = sum(conf.values())
        scale = 1.0 / total if total > 1.0 else 1.0
        sign = lambda i: 1 if int(i.direction) > 0 else (-1 if int(i.direction) < 0 else 0)
        return {i: self._allowed(sign(i)) * c * scale for i, c in conf.items()}

from .framework.sector import (SectorWeightingPortfolioConstructionModel,  # noqa: E402
                               MaximumSectorExposureRiskManagementModel)

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
    """LEAN reads shortable quantities from its data folder; this engine has no such files."""
    def __init__(self, brokerage=None, *args, **kwargs):
        self.brokerage = brokerage

    def shortable_quantity(self, symbol, local_time=None):
        raise _not_supported("shortable-quantity files from LEAN's data folder (LocalDiskShortableProvider)")

    ShortableQuantity = shortable_quantity

class ContractMultiplierProvider:
    pass

# --- Consolidator Extras ---
class ClassicRangeConsolidator:
    def __init__(self, range_size, data_type=None):
        self._range_size = range_size
        self.data_consolidated = EventHandler()

ClassicRenkoConsolidator = RenkoConsolidator

# --- Delistings / Dividends / Splits ---





# --- Singular data event types (for history() requests) ---

class MarginInterestRate:
    def __init__(self):
        self.symbol = None; self.time = None; self.interest_rate = 0.0; self.value = 0.0



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
    """Symbols known to the run (subscribed securities)."""
    _known = {}
    @staticmethod
    def get_symbol(ticker):
        return SymbolCache._known.get(str(ticker).upper()) or Symbol(ticker)
    @staticmethod
    def try_get_symbol(ticker, symbol=None):
        s = SymbolCache._known.get(str(ticker).upper())
        return (s is not None, s)
    GetSymbol = get_symbol
    TryGetSymbol = try_get_symbol

class DataMappingMode(IntEnum):
    LAST_TRADING_DAY = 0
    FIRST_DAY_MONTH = 1
    OPEN_INTEREST = 2
    OPEN_INTEREST_ANNUAL = 3
    LastTradingDay = 0
    FirstDayMonth = 1
    OpenInterest = 2
    OpenInterestAnnual = 3

# --- BaseData ---
class BaseData:
    def __init__(self):
        self.symbol = None; self.time = None; self.end_time = None; self.value = 0.0

# --- Option type ---

# --- EquityExchange ---
class EquityExchange:
    def __init__(self):
        self.hours = ExchangeHours(); self.time_zone = "America/New_York"

# --- IndiaOrderProperties ---
class IndiaOrderProperties:
    def __init__(self, exchange=None): self.exchange = exchange or ""

# --- ETFConstituentData ---

# --- Fundamental data class (used by self.history(Fundamental, ...) and type checks) ---

# --- .NET Object replacement with reference_equals ---
class Object:
    """Python replacement for .NET System.Object."""
    @staticmethod
    def reference_equals(a, b):
        """Check if two objects are the same reference (like .NET ReferenceEquals)."""
        return a is b
    ReferenceEquals = reference_equals

# --- Fundamental Universe Selection Aliases ---

# --- Random (C# System.Random API on Python's generator) ---
class Random(_random_mod.Random):
    def next(self, a=None, b=None):
        """next() -> [0, 2^31); next(max) -> [0, max); next(min, max) -> [min, max)."""
        if a is None:
            return self.randrange(0, 2 ** 31 - 1)
        if b is None:
            return self.randrange(0, int(a)) if int(a) > 0 else 0
        return self.randrange(int(a), int(b)) if int(b) > int(a) else int(a)

    def next_double(self):
        return self.random()

    Next = next
    NextDouble = next_double

# --- Delay / Identity / Beta / MassIndex: real indicators (they used to be stubs
# returning 0 / a constant beta of 1.0) ---
from .indicators import Delay, Identity, MassIndex
from .indicators import Beta as _EngineBeta

# --- Margin Call Model ---



# --- SecurityPositionGroupModel ---
class SecurityPositionGroupModel:
    NULL = None  # set after class definition
    def __init__(self, *args, **kwargs): pass
    def get_buying_power_for_strategy(self, *args):
        return type('obj', (object,), {'value': 0})()

SecurityPositionGroupModel.NULL = SecurityPositionGroupModel()
SecurityPositionGroupModel.Null = SecurityPositionGroupModel.NULL

# --- SubscriptionDataSource ---



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

# --- OptionChainedUniverseSelectionModel ---
class OptionChainedUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, universe_selection_model, option_filter=None):
        self._inner = universe_selection_model
        self._filter = option_filter

# --- OptionChains ---

# --- OpenInterestFutureUniverseSelectionModel ---
class OpenInterestFutureUniverseSelectionModel(UniverseSelectionModel):
    def __init__(self, *args, **kwargs): pass

# --- GeneratedInsightsCollection ---
class GeneratedInsightsCollection:
    """The insights_generated event argument: .insights and .date_time_utc (LEAN)."""
    def __init__(self, *args):
        self._insights = list(args) if args else []
        self.date_time_utc = getattr(self._insights[0], 'generated_time_utc', None) if self._insights else None

    @property
    def insights(self):
        return list(self._insights)

    Insights = insights
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

# --- FuncRiskFreeRateInterestRateModel ---
class FuncRiskFreeRateInterestRateModel:
    def __init__(self, func=None):
        self._func = func
    def get_interest_rate(self, date):
        return self._func(date) if callable(self._func) else 0.0
    GetInterestRate = get_interest_rate


# --- StandardDeviationOfReturnsVolatilityModel ---

# --- MarketImpactSlippageModel ---

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

# --- SymbolProperties / MarginInterestRateParameters ---
class SymbolProperties:
    @staticmethod
    def get_default(quote_currency="USD"):
        """LEAN SymbolProperties.GetDefault(quoteCurrency)."""
        return SymbolProperties("", quote_currency, 1, 0.01, 1, "")

    GetDefault = get_default

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


# --- InvalidOperationException ---
class InvalidOperationException(Exception):
    pass


from .errors import (ArgumentException, ArgumentOutOfRangeException, AggregateException,  # noqa: E402
                     KeyNotFoundException, NotSupportedException, NullReferenceException)

# Security classes by type (LEAN Equity, Option, Future...): this engine has one Security class;
# the names exist for type hints and isinstance checks.
from .securities import Security as _Security  # noqa: E402
Equity = Option = Future = Forex = Crypto = Index = Cfd = FutureOption = IndexOption = CryptoFuture = _Security

# every indicator class of the engine (StandardDeviation, ...), unless defined above
from . import indicators as _indicators_module  # noqa: E402
for _n, _v in list(vars(_indicators_module).items()):
    if isinstance(_v, type) and not _n.startswith('_') and _v.__module__ == _indicators_module.__name__ \
            and _n not in globals():
        globals()[_n] = _v
del _n, _v

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

# --- InceptionDateUniverseSelectionModel ---

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

# --- EmaCrossUniverseSelectionModel (Selection submodule) ---

# --- MeanReversionPortfolioConstructionModel ---
class MeanReversionPortfolioConstructionModel(PortfolioConstructionModel):
    def __init__(self, *args, **kwargs):
        raise _not_supported("the mean-reversion portfolio construction model")

# --- Optimiser-based portfolio construction (framework/portfolio_construction.py) ---
from .framework.portfolio_construction import (  # noqa: E402
    MeanVarianceOptimizationPortfolioConstructionModel, RiskParityPortfolioConstructionModel,
    MinimumVariancePortfolioOptimizer, UnconstrainedMeanVariancePortfolioOptimizer,
    MaximumSharpeRatioPortfolioOptimizer, RiskParityPortfolioOptimizer)

# --- VolumeShareSlippageModel ---

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
    'MeanVarianceOptimizationPortfolioConstructionModel': {
        'MeanVarianceOptimizationPortfolioConstructionModel': MeanVarianceOptimizationPortfolioConstructionModel},
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

# --- Wilder swing indices: not implemented (they used to echo the input) ---
class WilderAccumulativeSwingIndex(IndicatorBase):
    def __init__(self, *args, **kwargs):
        raise _not_supported("the Wilder accumulative swing index indicator")

class WilderSwingIndex(IndicatorBase):
    def __init__(self, *args, **kwargs):
        raise _not_supported("the Wilder swing index indicator")

# --- Beta: LEAN's Beta(name, period, target, reference) or the engine's
# Beta(name, target, reference, period) ---
class Beta(_EngineBeta):
    def __init__(self, name="BETA", *args, **kwargs):
        if args and isinstance(args[0], int):
            period, target, reference = args[0], args[1] if len(args) > 1 else None, args[2] if len(args) > 2 else None
        else:
            target = args[0] if len(args) > 0 else kwargs.get('target')
            reference = args[1] if len(args) > 1 else kwargs.get('reference')
            period = args[2] if len(args) > 2 else kwargs.get('period', 20)
        super().__init__(name, target, reference, int(period))

# --- IntrinioEconomicData ---
class IntrinioEconomicData(PythonData):
    pass

# --- FundamentalUniverse ---

# --- Futures.Dairy patch ---
class FuturesDairy:
    CASH_SETTLED_BUTTER = BUTTER = "CB"; CASH_SETTLED_CHEESE = "CSC"; CLASS_III_MILK = "DC"; DRY_WHEY = "DY"
    CLASS_IV_MILK = "GDK"; NONFAT_DRY_MILK = "GNF"
    CashSettledButter = Butter = "CB"; CashSettledCheese = "CSC"; ClassIIIMilk = "DC"; DryWhey = "DY"
    ClassIVMilk = "GDK"; NonfatDryMilk = "GNF"

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

# --- BrokerageModel ---


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
class OptionAssignmentModel:
    """Option assignment models. This engine assigns short options only at expiry (in the money);
    early assignment before expiry is not simulated, whatever the model (disclosed in the run notes)."""
    def __init__(self, *args, **kwargs): pass
    def get_assignment(self, parameters):
        return type('OptionAssignmentResult', (), {'is_assigned': False, 'quantity': 0, 'tag': ''})()
    GetAssignment = get_assignment


class DefaultOptionAssignmentModel(OptionAssignmentModel):
    pass


class NullOptionAssignmentModel(OptionAssignmentModel):
    """LEAN NullOptionAssignmentModel: no early assignment (what this engine does anyway)."""

# --- DefaultExerciseModel ---

# --- ImpliedVolatility: needs option prices (it used to return a constant 25%) ---

# --- OptionExerciseOrder ---

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
