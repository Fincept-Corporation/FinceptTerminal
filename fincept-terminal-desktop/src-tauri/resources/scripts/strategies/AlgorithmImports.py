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
    NullRiskManagementModel, Chart, Series, SeriesType,
    OptionChainProvider, SecurityChanges
)

# Enums
from fincept_engine.enums import (
    Resolution, SecurityType, OrderType, OrderStatus,
    Market, InsightDirection, InsightType, OrderDirection,
    TimeInForce, DataNormalizationMode
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

# Provide System.Collections.Generic stub
class List(list):
    """Stub for System.Collections.Generic.List"""
    def __init__(self, *args):
        if args and isinstance(args[0], type):
            super().__init__()
        else:
            super().__init__(args)
