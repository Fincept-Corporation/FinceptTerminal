"""
Fast-Trade Backtest Module

Core backtesting engine wrapper covering:
- run_backtest(): Main backtest execution with full result + summary
- validate_backtest(): Validate strategy config before execution
- prepare_new_backtest(): Normalize and prepare backtest config
- determine_action(): Evaluate entry/exit logic per frame
- take_action(): Process logic and determine trade action
- process_single_frame(): Evaluate all logics for a single bar
- process_single_logic(): Evaluate one logic condition
- process_logic_and_generate_actions(): Full signal generation pipeline
- apply_backtest_to_df(): Apply backtest logic to a DataFrame
- clean_field_type(): Normalize field types in logic definitions

Error Classes:
- BacktestKeyError: Missing required keys in config
- MissingData: No data provided or data is empty
"""

import pandas as pd
import numpy as np
from typing import Dict, Any, List, Optional, Tuple, Union
from datetime import datetime


# ============================================================================
# Error Classes
# ============================================================================

class BacktestKeyError(Exception):
    """Raised when required keys are missing from backtest config."""
    pass


class MissingData(Exception):
    """Raised when no data is provided or data is empty."""
    pass


# ============================================================================
# Config Validation
# ============================================================================

def match_field_type_to_value(field: Any) -> Any:
    """
    Match a field type to its expected value type for validation.

    Internal validation helper used by validate_backtest to check
    that logic fields have valid types (string column names or numeric literals).

    Wraps fast_trade.validate_backtest.match_field_type_to_value().

    Args:
        field: A field from a logic condition (string, int, or float)

    Returns:
        The validated/coerced field value
    """
    try:
        from fast_trade.validate_backtest import match_field_type_to_value as ft_match
        return ft_match(field)
    except ImportError:
        # Fallback: try numeric coercion, otherwise return as-is
        if isinstance(field, (int, float)):
            return field
        try:
            return float(field)
        except (ValueError, TypeError):
            return field


def validate_backtest(backtest: Dict[str, Any]) -> Dict[str, Any]:
    """
    Validate a backtest configuration dictionary.

    Wraps fast_trade.validate_backtest.validate_backtest().
    Checks for required fields (enter, exit, datapoints, etc.)

    Args:
        backtest: Strategy configuration dict with keys:
            - enter: list of entry logic conditions
            - exit: list of exit logic conditions
            - datapoints: list of indicator definitions
            - base_balance: starting capital (optional, default 1000)
            - comission: commission rate (optional, default 0)

    Returns:
        dict: {'valid': bool, 'errors': list[str], 'warnings': list[str]}
    """
    try:
        from fast_trade.validate_backtest import validate_backtest as ft_validate
        result = ft_validate(backtest)
        # fast-trade returns a dict or raises
        if isinstance(result, dict):
            return {
                'valid': not result.get('errors', []),
                'errors': result.get('errors', []),
                'warnings': result.get('warnings', [])
            }
        return {'valid': True, 'errors': [], 'warnings': []}
    except Exception as e:
        return {'valid': False, 'errors': [str(e)], 'warnings': []}


def validate_backtest_with_df(
    backtest: Dict[str, Any],
    df: pd.DataFrame
) -> Dict[str, Any]:
    """
    Validate backtest config against actual data.

    Ensures all referenced columns/indicators exist or can be computed.

    Args:
        backtest: Strategy config
        df: DataFrame with OHLCV data

    Returns:
        dict with 'valid', 'errors', 'warnings'
    """
    try:
        from fast_trade.validate_backtest import validate_backtest_with_df as ft_validate_df
        ft_validate_df(backtest, df)
        return {'valid': True, 'errors': [], 'warnings': []}
    except Exception as e:
        return {'valid': False, 'errors': [str(e)], 'warnings': []}


# ============================================================================
# Backtest Preparation
# ============================================================================

def prepare_new_backtest(backtest: Dict[str, Any]) -> Dict[str, Any]:
    """
    Normalize and prepare backtest config with defaults.

    Wraps fast_trade.run_backtest.prepare_new_backtest().
    Sets defaults for base_balance, comission, trailing_stop_loss, etc.

    Args:
        backtest: Raw strategy config

    Returns:
        Normalized config dict with all defaults applied
    """
    try:
        from fast_trade.run_backtest import prepare_new_backtest as ft_prepare
        return ft_prepare(backtest)
    except ImportError:
        # Fallback: apply defaults manually
        defaults = {
            'base_balance': 1000,
            'comission': 0,
            'enter': [],
            'exit': [],
            'datapoints': [],
            'trailing_stop_loss': 0,
            'any_enter': False,
            'any_exit': False,
        }
        merged = {**defaults, **backtest}
        return merged


def clean_field_type(field: Any, row: Optional[pd.Series] = None) -> Any:
    """
    Normalize field types in logic definitions.

    If field is a string matching a column name in row, returns the column value.
    Otherwise returns the field as-is (int/float).

    Args:
        field: A logic field (column name string or numeric literal)
        row: Current DataFrame row (optional)

    Returns:
        Resolved numeric value
    """
    try:
        from fast_trade.run_backtest import clean_field_type as ft_clean
        return ft_clean(field, row)
    except ImportError:
        if row is not None and isinstance(field, str) and field in row.index:
            return row[field]
        try:
            return float(field)
        except (ValueError, TypeError):
            return field


# ============================================================================
# Signal Logic Processing
# ============================================================================

def process_single_logic(logic: List, row: pd.Series) -> bool:
    """
    Evaluate one logic condition against a data row.

    Logic format: [left_field, operator, right_field]
    Operators: '>', '<', '=', '>=', '<=', '!='

    Args:
        logic: [field_or_column, operator_str, field_or_column]
        row: DataFrame row with indicator values

    Returns:
        True if condition is met
    """
    try:
        from fast_trade.run_backtest import process_single_logic as ft_logic
        return ft_logic(logic, row)
    except ImportError:
        if len(logic) != 3:
            return False
        left = clean_field_type(logic[0], row)
        op = logic[1]
        right = clean_field_type(logic[2], row)
        try:
            left = float(left)
            right = float(right)
        except (ValueError, TypeError):
            return False

        ops = {
            '>': lambda a, b: a > b,
            '<': lambda a, b: a < b,
            '=': lambda a, b: a == b,
            '>=': lambda a, b: a >= b,
            '<=': lambda a, b: a <= b,
            '!=': lambda a, b: a != b,
        }
        return ops.get(op, lambda a, b: False)(left, right)


def process_single_frame(
    logics: List[List],
    row: pd.Series,
    require_any: bool = False
) -> bool:
    """
    Evaluate all logic conditions for a single bar.

    Args:
        logics: List of logic conditions [[field, op, field], ...]
        row: DataFrame row
        require_any: If True, any condition passing is enough (OR logic).
                     If False, all must pass (AND logic).

    Returns:
        True if entry/exit conditions are met
    """
    try:
        from fast_trade.run_backtest import process_single_frame as ft_frame
        return ft_frame(logics, row, require_any)
    except ImportError:
        results = [process_single_logic(logic, row) for logic in logics]
        if not results:
            return False
        return any(results) if require_any else all(results)


def determine_action(
    frame: pd.DataFrame,
    backtest: Dict[str, Any],
    last_frames: Optional[List] = None
) -> str:
    """
    Determine trade action for current frame.

    Evaluates enter/exit logic and returns action string.

    Args:
        frame: Current data frame/row
        backtest: Strategy config with 'enter' and 'exit' logic
        last_frames: Previous frames for lookback (optional)

    Returns:
        'e' (enter), 'x' (exit), or '' (hold)
    """
    try:
        from fast_trade.run_backtest import determine_action as ft_determine
        return ft_determine(frame, backtest, last_frames or [])
    except ImportError:
        row = frame.iloc[-1] if isinstance(frame, pd.DataFrame) else frame

        enter_logics = backtest.get('enter', [])
        exit_logics = backtest.get('exit', [])
        any_enter = backtest.get('any_enter', False)
        any_exit = backtest.get('any_exit', False)

        if exit_logics and process_single_frame(exit_logics, row, any_exit):
            return 'x'
        if enter_logics and process_single_frame(enter_logics, row, any_enter):
            return 'e'
        return ''


def take_action(
    current_frame: pd.Series,
    logics: List[List],
    last_frames: Optional[List] = None,
    require_any: bool = False
) -> bool:
    """
    Process logic and determine if action should be taken.

    Args:
        current_frame: Current bar data
        logics: List of logic conditions
        last_frames: Previous frames for lookback
        require_any: OR vs AND logic

    Returns:
        True if action should be taken
    """
    try:
        from fast_trade.run_backtest import take_action as ft_take
        return ft_take(current_frame, logics, last_frames or [], require_any)
    except ImportError:
        return process_single_frame(logics, current_frame, require_any)


def process_logic_and_generate_actions(
    df: pd.DataFrame,
    backtest: Dict[str, Any]
) -> pd.DataFrame:
    """
    Process all logic conditions and generate action column.

    Iterates through the DataFrame and applies entry/exit logic
    to generate a column of actions ('e', 'x', '').

    Args:
        df: DataFrame with OHLCV + indicator columns
        backtest: Strategy config

    Returns:
        DataFrame with 'action' column added
    """
    try:
        from fast_trade.run_backtest import process_logic_and_generate_actions as ft_gen
        return ft_gen(df, backtest)
    except ImportError:
        actions = []
        for i in range(len(df)):
            row = df.iloc[i]
            action = determine_action(row, backtest)
            actions.append(action)
        df = df.copy()
        df['action'] = actions
        return df


# ============================================================================
# Backtest Application
# ============================================================================

def apply_backtest_to_df(
    df: pd.DataFrame,
    backtest: Dict[str, Any]
) -> pd.DataFrame:
    """
    Apply full backtest logic to a DataFrame.

    Adds action column, then simulates portfolio equity changes.
    Wraps fast_trade.run_backtest.apply_backtest_to_df().

    Args:
        df: DataFrame with OHLCV + indicators
        backtest: Strategy config with enter/exit/balance/comission

    Returns:
        DataFrame with action, equity, and trade columns
    """
    try:
        from fast_trade.run_backtest import apply_backtest_to_df as ft_apply
        return ft_apply(df, backtest)
    except ImportError:
        df = process_logic_and_generate_actions(df, backtest)
        return df


# ============================================================================
# Main Backtest Runner
# ============================================================================

def run_backtest(
    backtest: Dict[str, Any],
    df: Optional[pd.DataFrame] = None,
    summary: bool = True
) -> Dict[str, Any]:
    """
    Execute a full backtest.

    Main entry point. Accepts a JSON-like config dict and optional DataFrame.
    Returns complete results including DataFrame, trade log, and summary.

    Args:
        backtest: Strategy config dict containing:
            - enter: list of entry conditions [[field, op, value], ...]
            - exit: list of exit conditions
            - datapoints: list of indicator definitions
                [{'name': str, 'transformer': str, 'args': list}, ...]
            - base_balance: starting capital (default 1000)
            - comission: commission per trade (default 0)
            - trailing_stop_loss: trailing stop percentage (default 0)
            - any_enter: bool, use OR logic for enter (default False)
            - any_exit: bool, use OR logic for exit (default False)
            - freq: timeframe string ('1Min', '5Min', '1H', '1D', etc.)
            - start: start datetime filter (optional)
            - stop: stop datetime filter (optional)
            - data: pd.DataFrame or csv_path can be used
        df: Pre-loaded DataFrame (optional, overrides backtest['data'])
        summary: Whether to compute summary statistics (default True)

    Returns:
        dict with keys:
            - df: DataFrame with all columns (close, indicators, action, equity, etc.)
            - trade_df: DataFrame of individual trades
            - summary: dict of performance metrics (if summary=True)

    Raises:
        BacktestKeyError: If required config keys are missing
        MissingData: If no data is provided
    """
    try:
        from fast_trade.run_backtest import run_backtest as ft_run
        if df is not None and not df.empty:
            return ft_run(backtest, df=df, summary=summary)
        return ft_run(backtest, summary=summary)
    except ImportError:
        raise ImportError(
            "fast-trade is not installed. Run: pip install fast-trade"
        )


def run_backtest_from_csv(
    backtest: Dict[str, Any],
    csv_path: str,
    summary: bool = True
) -> Dict[str, Any]:
    """
    Run backtest loading data from a CSV file.

    Convenience wrapper that loads CSV then runs backtest.

    Args:
        backtest: Strategy config (same as run_backtest)
        csv_path: Path to OHLCV CSV file
        summary: Compute summary stats

    Returns:
        Same as run_backtest()
    """
    from fast_trade.build_data_frame import load_basic_df_from_csv
    df = load_basic_df_from_csv(csv_path)
    return run_backtest(backtest, df=df, summary=summary)


def run_multiple_backtests(
    configs: List[Dict[str, Any]],
    df: Optional[pd.DataFrame] = None,
    summary: bool = True
) -> List[Dict[str, Any]]:
    """
    Run multiple backtests with different configurations.

    Useful for parameter sweeps since fast-trade doesn't have native optimization.

    Args:
        configs: List of strategy config dicts
        df: Shared DataFrame (optional)
        summary: Compute summaries

    Returns:
        List of backtest results
    """
    results = []
    for config in configs:
        try:
            result = run_backtest(config, df=df, summary=summary)
            results.append({
                'config': {k: v for k, v in config.items() if k != 'data'},
                'result': result,
                'success': True
            })
        except Exception as e:
            results.append({
                'config': {k: v for k, v in config.items() if k != 'data'},
                'error': str(e),
                'success': False
            })
    return results
