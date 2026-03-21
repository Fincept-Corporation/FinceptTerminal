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

    Indicator-vs-indicator comparison:
    [
        {"indicator":"CLOSE","params":{},"field":"value","operator":"crosses_above",
         "compareMode":"indicator","compareIndicator":"EMA","compareParams":{"period":20},"compareField":"value"}
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
import logging

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from indicators import get_latest_value, get_last_n_values, get_value_at_offset

log = logging.getLogger('algo_runner.evaluator')


def _normalize_symbol(s: str) -> str:
    """Strip slashes, dashes, underscores, colons and uppercase for matching."""
    return s.replace('/', '').replace('-', '').replace('_', '').replace(':', '').upper()


def load_candles_from_db(db_path: str, symbol: str, timeframe: str, limit: int = 200) -> pd.DataFrame:
    """Load candles from candle_cache table.

    Tries exact symbol match first, then falls back to normalized matching
    to handle format differences (e.g., BTC/USD vs BTCUSD, NSE:RELIANCE vs RELIANCE).
    """
    conn = sqlite3.connect(db_path)

    # Try exact match first
    query = """
        SELECT open_time, o, h, l, c, volume
        FROM candle_cache
        WHERE symbol = ? AND timeframe = ? AND is_closed = 1
        ORDER BY open_time ASC
        LIMIT ?
    """
    df = pd.read_sql_query(query, conn, params=(symbol, timeframe, limit))
    log.debug(f"load_candles_from_db('{symbol}', '{timeframe}', limit={limit}) exact match => {len(df)} rows")

    if df.empty:
        # Fallback: find the actual symbol name in the cache that matches when normalized
        # Also handles broker truncation (e.g., Angel One truncates "AVANTIFEEDS" to "AVANTIFEED")
        norm = _normalize_symbol(symbol)
        try:
            cached_symbols = pd.read_sql_query(
                "SELECT DISTINCT symbol FROM candle_cache WHERE timeframe = ?",
                conn, params=(timeframe,)
            )
            log.debug(f"  Exact match failed. Trying normalized={norm}. Cached symbols for tf={timeframe}: {cached_symbols['symbol'].tolist()}")
            for _, row in cached_symbols.iterrows():
                cached_sym = row['symbol']
                cached_norm = _normalize_symbol(cached_sym)
                if cached_norm == norm or norm.startswith(cached_norm) or cached_norm.startswith(norm):
                    df = pd.read_sql_query(query, conn, params=(cached_sym, timeframe, limit))
                    log.debug(f"  Normalized/prefix match found: '{cached_sym}' => {len(df)} rows")
                    break
        except Exception as e:
            log.warning(f"  Fallback symbol search failed: {e}")

    conn.close()

    if df.empty:
        log.warning(f"  NO candle data found for '{symbol}' @ {timeframe}")
        return pd.DataFrame(columns=['open', 'high', 'low', 'close', 'volume'])

    df.rename(columns={'o': 'open', 'h': 'high', 'l': 'low', 'c': 'close'}, inplace=True)
    return df


def _get_target_value(condition: dict, df: pd.DataFrame):
    """
    Get the comparison target value(s). Supports both numeric and indicator comparison modes.

    Returns:
        For non-crossing operators: (current_target_value, None)
        For crossing operators: (current_target_value, previous_target_value)
        Also returns target_info dict for debug output.
    """
    compare_mode = condition.get('compareMode', 'value')
    operator = condition.get('operator', '>')
    compare_offset = condition.get('compareOffset', 0)

    if compare_mode == 'indicator':
        compare_indicator = condition.get('compareIndicator', '')
        compare_params = condition.get('compareParams', {})
        compare_field = condition.get('compareField', 'value')

        if not compare_indicator:
            raise ValueError('compareIndicator is required when compareMode is "indicator"')

        target_info = {
            'target_indicator': compare_indicator,
            'target_field': compare_field,
        }

        if operator in ('crosses_above', 'crosses_below'):
            # Need last 2 values for both sides
            target_values = get_last_n_values(compare_indicator, df, compare_params, compare_field, n=2)
            if len(target_values) < 2:
                raise ValueError(f'Insufficient data for {compare_indicator}')
            target_info['target_computed_value'] = target_values[-1]
            return target_values[-1], target_values[-2], target_info
        else:
            if compare_offset > 0:
                val = get_value_at_offset(compare_indicator, df, compare_params, compare_field, compare_offset)
            else:
                val = get_latest_value(compare_indicator, df, compare_params, compare_field)
            target_info['target_computed_value'] = val
            return val, None, target_info
    else:
        target_value = condition.get('value', 0)
        return target_value, None, {}


def evaluate_single_condition(condition: dict, df: pd.DataFrame) -> dict:
    """
    Evaluate a single condition against candle data.
    Supports indicator-vs-indicator comparison, offsets, and advanced operators.
    """
    indicator = condition.get('indicator', '')
    params = condition.get('params', {})
    field = condition.get('field', 'value')
    operator = condition.get('operator', '>')
    target_value_raw = condition.get('value', 0)
    target_value2 = condition.get('value2', None)
    offset = condition.get('offset', 0)
    compare_mode = condition.get('compareMode', 'value')
    lookback = condition.get('lookback', 5)

    try:
        # Get target value(s) — may be numeric or from another indicator
        target_current, target_prev, target_info = _get_target_value(condition, df)

        # ── Crossing operators ────────────────────────────────────────
        if operator in ('crosses_above', 'crosses_below'):
            values = get_last_n_values(indicator, df, params, field, n=2)
            if len(values) < 2 or any(math.isnan(v) for v in values):
                return {
                    'met': False,
                    'indicator': indicator,
                    'field': field,
                    'computed_value': values[-1] if values else None,
                    'operator': operator,
                    'target': target_current if isinstance(target_current, (int, float)) else 0,
                    'error': 'Insufficient data for crossing detection',
                    **target_info,
                }

            prev_val, curr_val = values

            if compare_mode == 'indicator' and target_prev is not None:
                # Crossing relative to another indicator
                if operator == 'crosses_above':
                    met = prev_val <= target_prev and curr_val > target_current
                else:
                    met = prev_val >= target_prev and curr_val < target_current
            else:
                # Crossing relative to a fixed value
                if operator == 'crosses_above':
                    met = prev_val <= target_current and curr_val > target_current
                else:
                    met = prev_val >= target_current and curr_val < target_current

            return {
                'met': met,
                'indicator': indicator,
                'field': field,
                'computed_value': curr_val,
                'prev_value': prev_val,
                'operator': operator,
                'target': target_current if isinstance(target_current, (int, float)) else 0,
                **target_info,
            }

        # ── Crossed within N bars ─────────────────────────────────────
        elif operator in ('crossed_above_within', 'crossed_below_within'):
            n = int(lookback) + 1
            values = get_last_n_values(indicator, df, params, field, n=n)
            if len(values) < 2:
                return {
                    'met': False, 'indicator': indicator, 'field': field,
                    'computed_value': values[-1] if values else None,
                    'operator': operator, 'target': target_current,
                    'error': 'Insufficient data',
                }

            # For indicator-vs-indicator, get N target values to compare bar-by-bar
            met = False
            if compare_mode == 'indicator':
                compare_indicator = condition.get('compareIndicator', '')
                compare_params_dict = condition.get('compareParams', {})
                compare_field_name = condition.get('compareField', 'value')
                target_values_list = get_last_n_values(compare_indicator, df, compare_params_dict, compare_field_name, n=n)
                if len(target_values_list) >= len(values):
                    for i in range(len(values) - 1):
                        if operator == 'crossed_above_within':
                            if values[i] <= target_values_list[i] and values[i + 1] > target_values_list[i + 1]:
                                met = True
                                break
                        else:
                            if values[i] >= target_values_list[i] and values[i + 1] < target_values_list[i + 1]:
                                met = True
                                break
            else:
                # Compare against fixed numeric target
                for i in range(len(values) - 1):
                    if operator == 'crossed_above_within':
                        if values[i] <= target_current and values[i + 1] > target_current:
                            met = True
                            break
                    else:
                        if values[i] >= target_current and values[i + 1] < target_current:
                            met = True
                            break

            return {
                'met': met,
                'indicator': indicator,
                'field': field,
                'computed_value': values[-1],
                'operator': operator,
                'target': target_current if isinstance(target_current, (int, float)) else 0,
                **target_info,
            }

        # ── Rising / Falling ──────────────────────────────────────────
        elif operator in ('rising', 'falling'):
            n = int(lookback)
            values = get_last_n_values(indicator, df, params, field, n=n)
            if len(values) < 2 or any(math.isnan(v) for v in values):
                return {
                    'met': False, 'indicator': indicator, 'field': field,
                    'computed_value': values[-1] if values else None,
                    'operator': operator, 'target': 0,
                    'error': 'Insufficient data',
                }

            if operator == 'rising':
                met = all(values[i] < values[i + 1] for i in range(len(values) - 1))
            else:
                met = all(values[i] > values[i + 1] for i in range(len(values) - 1))

            return {
                'met': met,
                'indicator': indicator,
                'field': field,
                'computed_value': values[-1],
                'operator': operator,
                'target': 0,
            }

        # ── Standard comparison operators ─────────────────────────────
        else:
            if offset > 0:
                current_value = get_value_at_offset(indicator, df, params, field, offset)
            else:
                current_value = get_latest_value(indicator, df, params, field)

            if math.isnan(current_value):
                return {
                    'met': False,
                    'indicator': indicator,
                    'field': field,
                    'computed_value': None,
                    'operator': operator,
                    'target': target_current if isinstance(target_current, (int, float)) else 0,
                    'error': 'Indicator returned NaN',
                    **target_info,
                }

            if math.isnan(target_current) if isinstance(target_current, float) else False:
                return {
                    'met': False,
                    'indicator': indicator,
                    'field': field,
                    'computed_value': current_value,
                    'operator': operator,
                    'target': 0,
                    'error': 'Target indicator returned NaN',
                    **target_info,
                }

            if operator == '>':
                met = current_value > target_current
            elif operator == '<':
                met = current_value < target_current
            elif operator == '>=':
                met = current_value >= target_current
            elif operator == '<=':
                met = current_value <= target_current
            elif operator == '==':
                met = abs(current_value - target_current) < 0.0001
            elif operator == 'between':
                if target_value2 is not None:
                    met = target_value_raw <= current_value <= target_value2
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
                'target': target_current if isinstance(target_current, (int, float)) else 0,
                **target_info,
            }

    except Exception as e:
        return {
            'met': False,
            'indicator': indicator,
            'field': field,
            'computed_value': None,
            'operator': operator,
            'target': target_value_raw,
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
    if df.empty or len(df) < 2:
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
