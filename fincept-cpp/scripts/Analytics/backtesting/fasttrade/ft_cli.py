"""
Fast-Trade CLI Module

Command-line interface helpers and entry points.
Wraps fast_trade.cli and fast_trade.cli_helpers.

CLI Entry Points:
- main(): Main CLI entry point (argparse-based)
- backtest_helper(): Run backtest from CLI arguments
- validate_helper(): Validate strategy file from CLI

CLI Helpers:
- open_strat_file(): Load strategy JSON file from disk
- create_plot(): Generate matplotlib plot from backtest results
- save(): Save backtest results to disk (JSON/CSV)

Error Classes:
- MissingStrategyFile: Raised when strategy file is not found
"""

from typing import Dict, Any, Optional, List
from pathlib import Path


# ============================================================================
# Error Classes
# ============================================================================

class MissingStrategyFile(Exception):
    """
    Raised when a strategy file cannot be found at the specified path.

    Wraps fast_trade.cli_helpers.MissingStrategyFile.
    """
    pass


# ============================================================================
# CLI Entry Points
# ============================================================================

def main() -> None:
    """
    Main CLI entry point for fast-trade.

    Parses command-line arguments and dispatches to backtest or validate.
    Wraps fast_trade.cli.main().

    Usage:
        fast_trade backtest <strategy_file> [--csv <data_file>] [--plot]
        fast_trade validate <strategy_file>
    """
    from fast_trade.cli import main as ft_main
    ft_main()


def backtest_helper(*args, **kwargs) -> None:
    """
    Run a backtest from CLI arguments.

    Parses the strategy file, loads data, runs backtest, and optionally
    generates plots and saves results.

    Wraps fast_trade.cli.backtest_helper().

    Args:
        *args: Positional arguments from argparse
        **kwargs: Keyword arguments from argparse
            Expected keys:
            - strat_file: Path to strategy JSON file
            - csv: Path to OHLCV CSV file (optional)
            - plot: Whether to generate a plot (optional)
            - save: Whether to save results (optional)
    """
    from fast_trade.cli import backtest_helper as ft_backtest_helper
    ft_backtest_helper(*args, **kwargs)


def validate_helper(args) -> None:
    """
    Validate a strategy file from CLI arguments.

    Loads the strategy JSON and runs validation checks without
    executing the backtest.

    Wraps fast_trade.cli.validate_helper().

    Args:
        args: Argparse namespace with 'strat_file' attribute
    """
    from fast_trade.cli import validate_helper as ft_validate_helper
    ft_validate_helper(args)


# ============================================================================
# CLI Helpers
# ============================================================================

def open_strat_file(fp: str) -> Dict[str, Any]:
    """
    Load a strategy JSON file from disk.

    Reads and parses a JSON strategy configuration file used by fast-trade.

    Wraps fast_trade.cli_helpers.open_strat_file().

    Args:
        fp: File path to the strategy JSON file

    Returns:
        Parsed strategy configuration dict

    Raises:
        MissingStrategyFile: If the file does not exist
        json.JSONDecodeError: If the file is not valid JSON
    """
    try:
        from fast_trade.cli_helpers import open_strat_file as ft_open
        return ft_open(fp)
    except ImportError:
        import json
        path = Path(fp)
        if not path.exists():
            raise MissingStrategyFile(f"Strategy file not found: {fp}")
        with open(path, 'r') as f:
            return json.load(f)


def create_plot(df, trade_df) -> None:
    """
    Generate a matplotlib plot from backtest results.

    Creates a visualization showing:
    - Price chart with entry/exit markers
    - Equity curve
    - Indicator overlays

    Wraps fast_trade.cli_helpers.create_plot().

    Args:
        df: Backtest result DataFrame (with close, indicators, equity)
        trade_df: Trade log DataFrame (with action, close columns)
    """
    from fast_trade.cli_helpers import create_plot as ft_plot
    ft_plot(df, trade_df)


def save(result: Dict[str, Any]) -> None:
    """
    Save backtest results to disk.

    Writes the backtest result dict to a JSON file and optionally
    exports the DataFrame to CSV.

    Wraps fast_trade.cli_helpers.save().

    Args:
        result: Complete backtest result dict from run_backtest()
            Contains 'df', 'trade_df', 'summary' keys
    """
    from fast_trade.cli_helpers import save as ft_save
    ft_save(result)
