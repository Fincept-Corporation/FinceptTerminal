"""
Condition Evaluator Engine

Short-lived subprocess called from Rust. Evaluates condition groups against candle data.

Usage:
    python condition_evaluator.py --mode once --conditions <json> --symbol <sym> --timeframe <tf> --db <path>

Input conditions JSON format:
    [
        {"indicator":"RSI","params":{"period":14},"field":"value","operator":"crosses_below","value":30},
        "AND",
        {"indicator":"MACD","params":{"fast":12,"slow":26,"signal":9},"field":"histogram","operator":">","value":0}
    ]

Output:
    {"success": true, "result": true/false, "details": [...]}
"""

import json
import sys
import argparse
import sqlite3
import pandas as pd
import math
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from indicators import get_latest_value, get_last_n_values


def load_candles_from_db(db_path: str, symbol: str, timeframe: str, limit: int = 200) -> pd.DataFrame:
    """Load candles from candle_cache table."""
    conn = sqlite3.connect(db_path)
    query = """
        SELECT open_time, o, h, l, c, volume
        FROM candle_cache
        WHERE symbol = ? AND timeframe = ? AND is_closed = 1
        ORDER BY open_time ASC
        LIMIT ?
    """
    df = pd.read_sql_query(query, conn, params=(symbol, timeframe, limit))
    conn.close()

    if df.empty:
        return pd.DataFrame(columns=['open', 'high', 'low', 'close', 'volume'])

    df.rename(columns={'o': 'open', 'h': 'high', 'l': 'low', 'c': 'close'}, inplace=True)
    return df


def evaluate_single_condition(condition: dict, df: pd.DataFrame) -> dict:
    """
    Evaluate a single condition against candle data.

    Returns: {"met": bool, "indicator": str, "field": str, "computed_value": float, "operator": str, "target": float}
    """
    indicator = condition.get('indicator', '')
    params = condition.get('params', {})
    field = condition.get('field', 'value')
    operator = condition.get('operator', '>')
    target_value = condition.get('value', 0)
    target_value2 = condition.get('value2', None)

    try:
        if operator in ('crosses_above', 'crosses_below'):
            # Need last 2 values for crossing detection
            values = get_last_n_values(indicator, df, params, field, n=2)
            if len(values) < 2 or any(math.isnan(v) for v in values):
                return {
                    'met': False,
                    'indicator': indicator,
                    'field': field,
                    'computed_value': values[-1] if values else None,
                    'operator': operator,
                    'target': target_value,
                    'error': 'Insufficient data for crossing detection'
                }

            prev_val, curr_val = values
            if operator == 'crosses_above':
                met = prev_val <= target_value and curr_val > target_value
            else:
                met = prev_val >= target_value and curr_val < target_value

            return {
                'met': met,
                'indicator': indicator,
                'field': field,
                'computed_value': curr_val,
                'prev_value': prev_val,
                'operator': operator,
                'target': target_value,
            }
        else:
            current_value = get_latest_value(indicator, df, params, field)

            if math.isnan(current_value):
                return {
                    'met': False,
                    'indicator': indicator,
                    'field': field,
                    'computed_value': None,
                    'operator': operator,
                    'target': target_value,
                    'error': 'Indicator returned NaN'
                }

            if operator == '>':
                met = current_value > target_value
            elif operator == '<':
                met = current_value < target_value
            elif operator == '>=':
                met = current_value >= target_value
            elif operator == '<=':
                met = current_value <= target_value
            elif operator == '==':
                met = abs(current_value - target_value) < 0.0001
            elif operator == 'between':
                if target_value2 is not None:
                    met = target_value <= current_value <= target_value2
                else:
                    met = False
            else:
                met = False

            return {
                'met': met,
                'indicator': indicator,
                'field': field,
                'computed_value': current_value,
                'operator': operator,
                'target': target_value,
            }

    except Exception as e:
        return {
            'met': False,
            'indicator': indicator,
            'field': field,
            'computed_value': None,
            'operator': operator,
            'target': target_value,
            'error': str(e)
        }


def evaluate_condition_group(conditions: list, df: pd.DataFrame) -> dict:
    """
    Evaluate a flat list of conditions with AND/OR logic.

    conditions format: [condition_dict, "AND"/"OR", condition_dict, ...]
    If no logic operators, default is AND.
    """
    if not conditions:
        return {'result': False, 'details': [], 'logic': 'AND'}

    # Parse into conditions and logic
    condition_items = []
    logic = 'AND'  # default

    for item in conditions:
        if isinstance(item, str) and item.upper() in ('AND', 'OR'):
            logic = item.upper()
        elif isinstance(item, dict):
            condition_items.append(item)

    if not condition_items:
        return {'result': False, 'details': [], 'logic': logic}

    details = []
    for cond in condition_items:
        result = evaluate_single_condition(cond, df)
        details.append(result)

    met_results = [d['met'] for d in details]

    if logic == 'AND':
        overall = all(met_results)
    else:
        overall = any(met_results)

    return {
        'result': overall,
        'details': details,
        'logic': logic,
    }


def main():
    parser = argparse.ArgumentParser(description='Condition Evaluator')
    parser.add_argument('--mode', default='once', choices=['once'])
    parser.add_argument('--conditions', required=True, help='JSON conditions array')
    parser.add_argument('--symbol', required=True)
    parser.add_argument('--timeframe', default='5m')
    parser.add_argument('--db', required=True, help='SQLite database path')
    parser.add_argument('--limit', type=int, default=200)
    args = parser.parse_args()

    try:
        conditions = json.loads(args.conditions)
    except json.JSONDecodeError as e:
        print(json.dumps({'success': False, 'error': f'Invalid conditions JSON: {e}'}))
        sys.exit(1)

    # Load candle data
    df = load_candles_from_db(args.db, args.symbol, args.timeframe, args.limit)
    if df.empty or len(df) < 5:
        print(json.dumps({
            'success': False,
            'error': f'Insufficient candle data for {args.symbol} ({len(df)} candles)'
        }))
        sys.exit(0)

    # Evaluate conditions
    result = evaluate_condition_group(conditions, df)

    print(json.dumps({
        'success': True,
        'symbol': args.symbol,
        'timeframe': args.timeframe,
        'candle_count': len(df),
        **result,
    }))


if __name__ == '__main__':
    main()
