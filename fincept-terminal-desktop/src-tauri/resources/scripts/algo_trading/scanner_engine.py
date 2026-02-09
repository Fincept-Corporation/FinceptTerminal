"""
Scanner Engine

One-shot scanner: evaluates conditions across multiple symbols.
Returns list of symbols that match the given conditions.

Usage:
    python scanner_engine.py --conditions <json> --symbols <json_array> --timeframe <tf> --db <path>
"""

import json
import sys
import argparse
import sqlite3
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from condition_evaluator import load_candles_from_db, evaluate_condition_group


def main():
    parser = argparse.ArgumentParser(description='Algo Scanner Engine')
    parser.add_argument('--conditions', required=True, help='JSON conditions array')
    parser.add_argument('--symbols', required=True, help='JSON array of symbols')
    parser.add_argument('--timeframe', default='5m')
    parser.add_argument('--db', required=True, help='SQLite database path')
    parser.add_argument('--limit', type=int, default=200)
    args = parser.parse_args()

    try:
        conditions = json.loads(args.conditions)
        symbols = json.loads(args.symbols)
    except json.JSONDecodeError as e:
        print(json.dumps({'success': False, 'error': f'Invalid JSON: {e}'}))
        sys.exit(1)

    if not isinstance(symbols, list) or not symbols:
        print(json.dumps({'success': False, 'error': 'symbols must be a non-empty array'}))
        sys.exit(1)

    matches = []
    errors = []
    scanned = 0
    debug_log = []

    debug_log.append(f'[scanner.py] db={args.db}')
    debug_log.append(f'[scanner.py] timeframe={args.timeframe}, symbols={len(symbols)}, conditions={len(conditions)}')

    # Check DB connectivity and candle_cache state
    try:
        conn_check = sqlite3.connect(args.db)
        cur = conn_check.cursor()
        cur.execute('SELECT COUNT(*) FROM candle_cache WHERE timeframe = ?', (args.timeframe,))
        total_rows = cur.fetchone()[0]
        debug_log.append(f'[scanner.py] candle_cache total rows for tf={args.timeframe}: {total_rows}')

        for sym in symbols:
            cur.execute('SELECT COUNT(*) FROM candle_cache WHERE symbol = ? AND timeframe = ?', (sym, args.timeframe))
            sym_count = cur.fetchone()[0]
            debug_log.append(f'[scanner.py] {sym}: {sym_count} candles in cache')
        conn_check.close()
    except Exception as e:
        debug_log.append(f'[scanner.py] DB check error: {e}')

    for symbol in symbols:
        scanned += 1
        try:
            df = load_candles_from_db(args.db, symbol, args.timeframe, args.limit)
            if df.empty or len(df) < 10:
                errors.append({'symbol': symbol, 'error': f'Insufficient data ({len(df)} candles)'})
                continue

            result = evaluate_condition_group(conditions, df)

            if result['result']:
                # Get current price from last candle
                current_price = float(df['close'].iloc[-1])
                matches.append({
                    'symbol': symbol,
                    'price': current_price,
                    'details': result['details'],
                })

        except Exception as e:
            errors.append({'symbol': symbol, 'error': str(e)})
            debug_log.append(f'[scanner.py] {symbol} exception: {e}')

    debug_log.append(f'[scanner.py] summary: scanned={scanned}, matches={len(matches)}, errors={len(errors)}')

    print(json.dumps({
        'success': True,
        'matches': matches,
        'match_count': len(matches),
        'scanned': scanned,
        'errors': errors,
        'timeframe': args.timeframe,
        'scanner_debug': debug_log,
    }))


if __name__ == '__main__':
    main()
