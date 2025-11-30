"""
Backtesting Base Module
Platform-independent base classes for backtesting providers
"""

from .base_provider import BacktestingProviderBase, BacktestResult, PerformanceMetrics

__all__ = ['BacktestingProviderBase', 'BacktestResult', 'PerformanceMetrics']
