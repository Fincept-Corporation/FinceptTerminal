"""
VectorBT Provider Module
Ultra-fast vectorized backtesting

Modules:
- vectorbt_provider: Main orchestrator (CLI entry point)
- vbt_strategies: 20+ strategy implementations
- vbt_indicators: All VBT indicators + custom indicators + IndicatorFactory
- vbt_portfolio: Portfolio construction (from_signals, from_order_func, stops, sizing)
- vbt_metrics: Full metrics extraction (100+ metrics)
- vbt_optimization: Vectorized parameter optimization + walk-forward
- vbt_returns: Returns Accessor (30+ rolling/risk-adjusted methods)
- vbt_data: Data classes (YFData, BinanceData, CCXTData, AlpacaData, GBMData)
- vbt_signals: Signal factory + random generators + stop/take-profit generators
- vbt_labels: Label generators (FIXLB, MEANLB, LEXLB, TRENDLB, BOLB)
- vbt_splitters: Cross-validation splitters (Rolling, Expanding, Range, PurgedKFold)
- vbt_generic: Drawdowns and Ranges analysis classes
"""

from .vectorbt_provider import VectorBTProvider

__all__ = [
    'VectorBTProvider',
    # Sub-modules importable as:
    # from vectorbt.vbt_portfolio import SimplePortfolio, build_portfolio, build_portfolio_from_order_func
    # from vectorbt.vbt_returns import ReturnsAccessor
    # from vectorbt.vbt_data import YFData, BinanceData, CCXTData, AlpacaData, GBMData, Data
    # from vectorbt.vbt_signals import SignalFactory, RAND, RANDX, STX, OHLCSTX, ...
    # from vectorbt.vbt_labels import FIXLB, MEANLB, LEXLB, TRENDLB, BOLB
    # from vectorbt.vbt_indicators import IndicatorFactory, calculate_rsi, ...
    # from vectorbt.vbt_splitters import RollingSplitter, ExpandingSplitter, RangeSplitter, PurgedKFoldSplitter
    # from vectorbt.vbt_generic import Drawdowns, Ranges
    # from vectorbt.vbt_metrics import extract_full_metrics
    # from vectorbt.vbt_optimization import optimize, walk_forward_optimize
]
