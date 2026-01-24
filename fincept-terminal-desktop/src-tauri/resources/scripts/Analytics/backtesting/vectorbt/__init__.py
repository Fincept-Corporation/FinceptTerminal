"""
VectorBT Provider Module
Ultra-fast vectorized backtesting

Modules:
- vectorbt_provider: Main orchestrator (CLI entry point)
- vbt_strategies: 20+ strategy implementations
- vbt_indicators: All VBT indicators + custom indicators
- vbt_portfolio: Portfolio construction (from_signals, stops, sizing)
- vbt_metrics: Full metrics extraction (100+ metrics)
- vbt_optimization: Vectorized parameter optimization + walk-forward
"""

from .vectorbt_provider import VectorBTProvider

__all__ = ['VectorBTProvider']
