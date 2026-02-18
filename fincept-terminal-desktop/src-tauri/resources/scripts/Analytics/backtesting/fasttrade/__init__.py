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
"""

# Fix: Avoid relative imports for CLI compatibility
try:
    from .fasttrade_provider import FastTradeProvider
except ImportError:
    from fasttrade_provider import FastTradeProvider

__all__ = [
    'FastTradeProvider',
    # Sub-modules importable as:
    # from fasttrade.ft_backtest import run_backtest, validate_backtest, run_multiple_backtests
    # from fasttrade.ft_indicators import sma, ema, rsi, macd, bbands, atr, ichimoku, ...
    # from fasttrade.ft_data import load_basic_df_from_csv, generate_synthetic_ohlcv, get_binance_klines, ...
    # from fasttrade.ft_summary import build_summary, calculate_drawdown_metrics, calculate_trade_quality, ...
    # from fasttrade.ft_analysis import enter_position, exit_position, apply_logic_to_df, calculate_fee
    # from fasttrade.ft_evaluate import evaluate_rules, handle_rule
    # from fasttrade.ft_utils import resample, trending_up, trending_down, to_dataframe
    # from fasttrade.ft_strategies import sma_crossover, rsi_strategy, bollinger_bands_strategy, ...
    # from fasttrade.ft_cli import backtest_helper, validate_helper, open_strat_file, create_plot, save, MissingStrategyFile
]
