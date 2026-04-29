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

The provider class is intentionally not re-exported here. See backtestingpy
__init__.py for the rationale (avoids RuntimeWarning when vectorbt_provider
is run via `python -m`).
"""
