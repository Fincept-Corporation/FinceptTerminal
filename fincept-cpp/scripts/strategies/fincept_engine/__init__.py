# ============================================================================
# Fincept Terminal - Strategy Engine Core
# Copyright (c) 2024-2026 Fincept Corporation. All rights reserved.
# Licensed under the MIT License.
#
# Package structure:
#   fincept_engine/
#   ├── __init__.py              ← This file (public API)
#   ├── algorithm.py             ← QCAlgorithm base class + framework models
#   ├── algorithm_imports.py     ← Full LEAN compatibility layer (stubs & aliases)
#   ├── consolidators.py         ← Data consolidators (TradeBar, Renko, Range, etc.)
#   ├── enums.py                 ← All enumerations
#   ├── extensions.py            ← Utility extensions (TimeZones, etc.)
#   ├── indicators.py            ← Technical indicators (SMA, EMA, RSI, etc.)
#   ├── portfolio.py             ← Portfolio manager
#   ├── scheduling.py            ← Scheduling system (DateRules, TimeRules)
#   ├── securities.py            ← Security manager & Exchange
#   ├── types.py                 ← Core data types (Symbol, TradeBar, Insight, etc.)
#   ├── universe.py              ← Universe selection & fundamental data
#   └── framework/               ← Framework pipeline models
#       ├── __init__.py
#       ├── alphas.py            ← Alpha models (RSI, EMA Cross, MACD, etc.)
#       ├── portfolio_construction.py  ← Portfolio optimizers
#       ├── execution.py         ← Execution models (Spread, etc.)
#       ├── risk.py              ← Risk model re-exports
#       └── selection.py         ← Universe selection models (Option, Future, ETF)
# ============================================================================

# --- Enums ---
from .enums import (
    Resolution, SecurityType, OrderType, Market,
    InsightDirection, InsightType, OrderStatus,
    TimeInForce, DataNormalizationMode, MovingAverageType,
    OptionRight, OptionStyle, BrokerageName, AccountType,
    OrderDirection, PositionSide, SettlementType
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

# --- Portfolio & Securities ---
from .portfolio import SecurityPortfolioManager
from .securities import SecurityManager, Security, Exchange, ExchangeHours

# --- Algorithm (core + all framework models) ---
from .algorithm import (
    QCAlgorithm, AlphaModel, PortfolioConstructionModel,
    ExecutionModel, RiskManagementModel, UniverseSelectionModel,
    NullRiskManagementModel, NullAlphaModel,
    Chart, Series, SeriesType, OptionChainProvider, SecurityChanges,
    Transactions, ObjectStore, NotificationManager, SubscriptionManager,
    # Framework models
    ConstantAlphaModel, CompositeAlphaModel,
    EqualWeightingPortfolioConstructionModel,
    InsightWeightingPortfolioConstructionModel,
    MeanVarianceOptimizationPortfolioConstructionModel,
    BlackLittermanOptimizationPortfolioConstructionModel,
    ImmediateExecutionModel,
    VolumeWeightedAveragePriceExecutionModel,
    StandardDeviationExecutionModel,
    MaximumDrawdownPercentPortfolio,
    MaximumDrawdownPercentPerSecurity,
    MaximumUnrealizedProfitPercentPerSecurity,
    TrailingStopRiskManagementModel,
    MaximumSectorExposureRiskManagementModel,
    CompositeRiskManagementModel,
    ManualUniverseSelectionModel,
    FundamentalUniverseSelectionModel,
    ScheduledUniverseSelectionModel,
    CustomUniverseSelectionModel,
    # Data types
    PythonData, PythonQuandl, InsightCollection,
    Field, Futures, Globals, SecurityIdentifier, PortfolioBias,
)

# --- Consolidators ---
from .consolidators import (
    TradeBarConsolidator, QuoteBarConsolidator,
    RenkoConsolidator, RangeConsolidator,
    TickConsolidator, EventHandler
)

# --- Scheduling ---
from .scheduling import (
    DateRules, TimeRules, ScheduleManager,
    ScheduledEvent, DateRule, TimeRule
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

# --- Framework sub-package ---
from .framework.alphas import (
    RsiAlphaModel, HistoricalReturnsAlphaModel, EmaCrossAlphaModel,
    MacdAlphaModel, PairsTradingAlphaModel, BasePairsTradingAlphaModel,
    PearsonCorrelationPairsTradingAlphaModel
)
from .framework.portfolio_construction import UnconstrainedMeanVariancePortfolioOptimizer
from .framework.execution import SpreadExecutionModel
from .framework.selection import (
    OptionUniverseSelectionModel, FutureUniverseSelectionModel,
    ETFConstituentsUniverseSelectionModel, QC500UniverseSelectionModel
)
