"""
Scanner Engine

One-shot scanner: evaluates conditions across multiple symbols.
Returns list of symbols that match the given conditions.

Usage (called by C++ AlgoTradingService):
    python scanner_engine.py scan <json_params> --db <path>

JSON params:
    {
        "conditions": [...],
        "symbols": [...],
        "timeframe": "1d",
        "lookback_days": 365,
        "logic": "AND"
    }
"""

import json
import sys
import argparse
import sqlite3
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from condition_evaluator import load_candles_from_db, evaluate_condition_group


def get_db_path(arg_db: str) -> str:
    if arg_db and os.path.exists(arg_db):
        return arg_db
    appdata = os.environ.get('APPDATA', '')
    if appdata:
        candidate = os.path.join(appdata, 'Fincept', 'FinceptTerminal', 'fincept.db')
        if os.path.exists(candidate):
            return candidate
    return arg_db or ''


def main():
    parser = argparse.ArgumentParser(description='Algo Scanner Engine')
    parser.add_argument('command', choices=['scan'], help='Subcommand')
    parser.add_argument('payload', help='JSON params: conditions, symbols, timeframe, lookback_days, logic')
    parser.add_argument('--db', default=None, help='SQLite database path')
    args = parser.parse_args()

    db_path = get_db_path(args.db)

    try:
        params = json.loads(args.payload)
    except json.JSONDecodeError as e:
        print(json.dumps({'success': False, 'error': f'Invalid JSON: {e}'}))
        sys.exit(1)

    conditions = params.get('conditions', [])
    symbols = params.get('symbols', [])
    timeframe = params.get('timeframe', '1d')
    limit = int(params.get('lookback_days', 365))
    logic = params.get('logic', 'AND')

    if not conditions:
        print(json.dumps({'success': False, 'error': 'No conditions provided'}))
        sys.exit(1)

    if not isinstance(symbols, list) or not symbols:
        print(json.dumps({'success': False, 'error': 'symbols must be a non-empty array'}))
        sys.exit(1)

    matches = []
    errors = []
    scanned = 0
    debug_log = []

    debug_log.append(f'[scanner] db={db_path}')
    debug_log.append(f'[scanner] timeframe={timeframe}, symbols={len(symbols)}, conditions={len(conditions)}, logic={logic}')

    # Log candle_cache state for diagnostics
    try:
        conn_check = sqlite3.connect(db_path)
        cur = conn_check.cursor()
        cur.execute('SELECT COUNT(*) FROM candle_cache WHERE timeframe = ?', (timeframe,))
        total_rows = cur.fetchone()[0]
        debug_log.append(f'[scanner] candle_cache rows for tf={timeframe}: {total_rows}')
        conn_check.close()
    except Exception as e:
        debug_log.append(f'[scanner] DB check skipped: {e}')

    for symbol in symbols:
        scanned += 1
        try:
            df = load_candles_from_db(db_path, symbol, timeframe, limit)
            if df.empty or len(df) < 10:
                errors.append({'symbol': symbol, 'error': f'Insufficient data ({len(df)} candles)'})
                continue

            result = evaluate_condition_group(conditions, df)

            if result['result']:
                current_price = float(df['close'].iloc[-1])
                matches.append({
                    'symbol': symbol,
                    'price': current_price,
                    'conditions': result.get('details', []),
                })

        except Exception as e:
            errors.append({'symbol': symbol, 'error': str(e)})
            debug_log.append(f'[scanner] {symbol} exception: {e}')

    debug_log.append(f'[scanner] done: scanned={scanned}, matches={len(matches)}, errors={len(errors)}')

    print(json.dumps({
        'success': True,
        'matches': matches,
        'match_count': len(matches),
        'total_scanned': scanned,
        'condition_count': len(conditions),
        'errors': errors,
        'timeframe': timeframe,
        'scanner_debug': debug_log,
    }))


if __name__ == '__main__':
    main()
