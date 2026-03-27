"""
Algo Manager — deployment management subcommands.

Called by the C++ AlgoTradingService for deployment control operations.
algo_live_runner.py only handles the long-running strategy loop; this script
handles all read/write management queries against the DB.

Usage:
    python algo_manager.py list_deployments --db <path>
    python algo_manager.py stop <deploy_id> --db <path>
    python algo_manager.py stop_all --db <path>
"""

import json
import sys
import os
import argparse
import sqlite3


def get_db_path(args_db: str) -> str:
    """Resolve DB path: use provided --db, or fall back to AppData location."""
    if args_db and os.path.exists(args_db):
        return args_db
    # Windows fallback
    appdata = os.environ.get('APPDATA', '')
    if appdata:
        candidate = os.path.join(appdata, 'Fincept', 'FinceptTerminal', 'fincept.db')
        if os.path.exists(candidate):
            return candidate
    # Last resort: same dir as script
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'fincept.db')


def open_db(db_path: str):
    conn = sqlite3.connect(db_path, timeout=10)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA busy_timeout=5000")
    return conn


def cmd_list_deployments(db_path: str):
    """Return all deployments with their live metrics."""
    try:
        conn = open_db(db_path)
        rows = conn.execute("""
            SELECT
                d.id, d.strategy_id, d.symbol, d.mode, d.status,
                d.timeframe, d.quantity, d.error_message,
                d.created_at, d.updated_at,
                s.name AS strategy_name,
                COALESCE(m.total_pnl, 0)                AS total_pnl,
                COALESCE(m.unrealized_pnl, 0)           AS unrealized_pnl,
                COALESCE(m.total_trades, 0)              AS total_trades,
                COALESCE(m.win_rate, 0)                  AS win_rate,
                COALESCE(m.max_drawdown, 0)              AS max_drawdown,
                COALESCE(m.current_position_qty, 0)      AS current_position_qty,
                COALESCE(m.current_position_side, '')    AS current_position_side,
                COALESCE(m.current_position_entry, 0)    AS current_position_entry
            FROM algo_deployments d
            LEFT JOIN algo_strategies s ON s.id = d.strategy_id
            LEFT JOIN algo_metrics m ON m.deployment_id = d.id
            ORDER BY d.created_at DESC
        """).fetchall()
        conn.close()

        deployments = [dict(r) for r in rows]
        print(json.dumps({'success': True, 'deployments': deployments}))

    except sqlite3.OperationalError as e:
        # Table may not exist yet (first run before any deployment)
        if 'no such table' in str(e).lower():
            print(json.dumps({'success': True, 'deployments': []}))
        else:
            print(json.dumps({'success': False, 'error': str(e), 'deployments': []}))
    except Exception as e:
        print(json.dumps({'success': False, 'error': str(e), 'deployments': []}))


def cmd_stop(deploy_id: str, db_path: str):
    """Mark a deployment as stopped in the DB."""
    try:
        conn = open_db(db_path)
        conn.execute(
            "UPDATE algo_deployments SET status = 'stopped', updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (deploy_id,)
        )
        conn.commit()
        conn.close()
        print(json.dumps({'success': True, 'deployment_id': deploy_id}))
    except Exception as e:
        print(json.dumps({'success': False, 'error': str(e)}))


def cmd_stop_all(db_path: str):
    """Mark all running/starting deployments as stopped."""
    try:
        conn = open_db(db_path)
        conn.execute(
            "UPDATE algo_deployments SET status = 'stopped', updated_at = CURRENT_TIMESTAMP "
            "WHERE status IN ('running', 'starting', 'pending')"
        )
        conn.commit()
        conn.close()
        print(json.dumps({'success': True}))
    except Exception as e:
        print(json.dumps({'success': False, 'error': str(e)}))


def main():
    parser = argparse.ArgumentParser(description='Algo Deployment Manager')
    parser.add_argument('command', choices=['list_deployments', 'stop', 'stop_all'])
    parser.add_argument('deploy_id', nargs='?', default=None,
                        help='Deployment ID (required for stop)')
    parser.add_argument('--db', default=None, help='SQLite database path')

    args = parser.parse_args()
    db_path = get_db_path(args.db)

    if args.command == 'list_deployments':
        cmd_list_deployments(db_path)
    elif args.command == 'stop':
        if not args.deploy_id:
            print(json.dumps({'success': False, 'error': 'deploy_id required for stop'}))
            sys.exit(1)
        cmd_stop(args.deploy_id, db_path)
    elif args.command == 'stop_all':
        cmd_stop_all(db_path)


if __name__ == '__main__':
    main()
