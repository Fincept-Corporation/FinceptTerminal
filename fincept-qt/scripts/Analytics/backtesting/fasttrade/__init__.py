"""
Fast-Trade Backtesting Provider Module

Complete wrapper for the fast-trade library.
JSON-based backtesting with 90+ FINTA technical indicators.

Modules:
- fasttrade_provider: Main orchestrator (CLI entry point)
- ft_backtest: Core backtesting engine (run_backtest, validate, signal logic)
- ft_indicators: All 90+ FINTA TA indicators + transformer map + inputvalidator
- ft_data: Data loading, preparation, archive (Binance/Coinbase/DB), synthetic data
- ft_summary: Performance metrics, trade analysis, risk metrics, drawdowns
- ft_analysis: Trade execution logic (enter/exit, fees, position management)
- ft_evaluate: Rule evaluation engine for post-backtest filtering
- ft_utils: Utilities (resample, trend detection, data conversion)
- ft_strategies: 20+ pre-built strategy templates
- ft_cli: CLI entry points and helpers (backtest_helper, create_plot, save)

The provider class is intentionally not re-exported here. See backtestingpy
__init__.py for the rationale (avoids RuntimeWarning when fasttrade_provider
is run via `python -m`).
"""
