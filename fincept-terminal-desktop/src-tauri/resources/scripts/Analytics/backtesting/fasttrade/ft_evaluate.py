"""
Fast-Trade Evaluate Module

Rule evaluation engine for post-backtest filtering and analysis.
Allows defining pass/fail criteria on backtest results.

Functions:
- evaluate_rules(): Evaluate a list of rules against backtest results
- handle_rule(): Evaluate a single rule condition
- extract_error_messages(): Format error messages from validation
"""

from typing import Dict, Any, List, Tuple, Optional


# ============================================================================
# Rule Evaluation
# ============================================================================

def evaluate_rules(
    result: Dict[str, Any],
    rules: List[list]
) -> Tuple[bool, Dict[str, Any]]:
    """
    Evaluate a list of rules against backtest results.

    Rules are conditions that the backtest results must satisfy.
    Used for filtering: e.g. "only accept strategies with Sharpe > 1.5"

    Rule format: [metric_path, operator, threshold]
    Example rules:
        ['summary.sharpe_ratio', '>', 1.0]
        ['summary.return_perc', '>', 0]
        ['summary.max_drawdown', '>', -20]
        ['summary.num_trades', '>=', 10]

    Args:
        result: Backtest result dict (from run_backtest)
        rules: List of rule conditions

    Returns:
        Tuple of (all_passed: bool, details: dict)
        details contains:
            - passed: list of passed rules
            - failed: list of failed rules
            - errors: list of error messages
    """
    try:
        from fast_trade.evaluate import evaluate_rules as ft_evaluate
        return ft_evaluate(result, rules)
    except ImportError:
        passed = []
        failed = []
        errors = []

        for rule in rules:
            try:
                rule_passed = handle_rule(result, rule)
                if rule_passed:
                    passed.append(rule)
                else:
                    failed.append(rule)
            except Exception as e:
                errors.append(f"Rule {rule}: {str(e)}")
                failed.append(rule)

        all_passed = len(failed) == 0 and len(errors) == 0
        return all_passed, {
            'passed': passed,
            'failed': failed,
            'errors': errors,
        }


def handle_rule(result: Dict[str, Any], rule: list) -> bool:
    """
    Evaluate a single rule condition against backtest results.

    Rule format: [metric_path, operator, threshold]

    Supported operators: '>', '<', '=', '>=', '<=', '!='

    The metric_path uses dot notation to access nested values:
    e.g. 'summary.sharpe_ratio' accesses result['summary']['sharpe_ratio']

    Args:
        result: Backtest result dict
        rule: [metric_path, operator, threshold]

    Returns:
        True if rule condition is satisfied

    Raises:
        ValueError: If rule format is invalid
        KeyError: If metric path not found in results
    """
    try:
        from fast_trade.evaluate import handle_rule as ft_handle
        return ft_handle(result, rule)
    except ImportError:
        if len(rule) != 3:
            raise ValueError(f"Rule must have 3 elements [path, op, value], got: {rule}")

        path, operator, threshold = rule

        # Navigate dot-separated path
        value = result
        for key in path.split('.'):
            if isinstance(value, dict):
                value = value.get(key)
            else:
                raise KeyError(f"Cannot navigate path '{path}' in result")

        if value is None:
            return False

        try:
            value = float(value)
            threshold = float(threshold)
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

        op_func = ops.get(operator)
        if op_func is None:
            raise ValueError(f"Unknown operator: {operator}")

        return op_func(value, threshold)


def extract_error_messages(error_dict: Dict[str, Any]) -> str:
    """
    Format error messages from validation/evaluation results.

    Recursively extracts error strings from nested dict structures.

    Args:
        error_dict: Dict containing error information

    Returns:
        Formatted error message string
    """
    try:
        from fast_trade.evaluate import extract_error_messages as ft_extract
        return ft_extract(error_dict)
    except ImportError:
        if isinstance(error_dict, str):
            return error_dict
        if isinstance(error_dict, dict):
            messages = []
            for key, value in error_dict.items():
                if isinstance(value, str):
                    messages.append(f"{key}: {value}")
                elif isinstance(value, list):
                    messages.append(f"{key}: {', '.join(str(v) for v in value)}")
                elif isinstance(value, dict):
                    messages.append(f"{key}: {extract_error_messages(value)}")
            return '; '.join(messages)
        return str(error_dict)
