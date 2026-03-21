"""
Database Manager for Agno Trading System
Handles all database operations for agent trades, performance, decisions, and evolution
"""

import sqlite3
import json
import os
from typing import Dict, List, Any, Optional, Tuple
from datetime import datetime, timedelta
import sys

class DatabaseManager:
    def __init__(self, db_path: Optional[str] = None):
        """Initialize database manager"""
        if db_path is None:
            # Use FINCEPT_DATA_DIR env var if set by Rust, otherwise use platform-specific app data
            data_dir = os.environ.get("FINCEPT_DATA_DIR")
            if data_dir:
                db_dir = os.path.join(data_dir, 'alpha_arena')
            else:
                # Fallback to platform-specific user data directory
                if os.name == "nt":  # Windows
                    base = os.environ.get("LOCALAPPDATA", os.path.expanduser("~/AppData/Local"))
                elif hasattr(os, 'uname') and os.uname().sysname == "Darwin":  # macOS
                    base = os.path.expanduser("~/Library/Application Support")
                else:  # Linux
                    base = os.path.expanduser("~/.local/share")
                db_dir = os.path.join(base, 'fincept-dev', 'alpha_arena')
            os.makedirs(db_dir, exist_ok=True)
            db_path = os.path.join(db_dir, 'agno_trading.db')
            print(f"[DatabaseManager] Using database at: {db_path}", file=sys.stderr)

        self.db_path = db_path
        self.initialize_database()

    def get_connection(self) -> sqlite3.Connection:
        """Get database connection"""
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def initialize_database(self):
        """Create tables if they don't exist"""
        schema_path = os.path.join(os.path.dirname(__file__), 'schema.sql')

        conn = self.get_connection()

        if os.path.exists(schema_path):
            with open(schema_path, 'r') as f:
                schema_sql = f.read()
            conn.executescript(schema_sql)

        # Initialize competition-related tables
        self._initialize_competition_tables(conn)

        conn.commit()
        conn.close()

    # ============================================================================
    # TRADE OPERATIONS
    # ============================================================================

    def record_trade(self, trade_data: Dict[str, Any]) -> int:
        """Record a new trade"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            INSERT INTO agent_trades (
                agent_id, model, symbol, side, order_type, quantity,
                entry_price, stop_loss, take_profit, entry_timestamp,
                reasoning, metadata, execution_time_ms, is_paper_trade
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            trade_data['agent_id'],
            trade_data['model'],
            trade_data['symbol'],
            trade_data['side'],
            trade_data.get('order_type', 'market'),
            trade_data['quantity'],
            trade_data['entry_price'],
            trade_data.get('stop_loss'),
            trade_data.get('take_profit'),
            trade_data.get('entry_timestamp', int(datetime.now().timestamp())),
            trade_data.get('reasoning'),
            json.dumps(trade_data.get('metadata', {})),
            trade_data.get('execution_time_ms', 0),
            trade_data.get('is_paper_trade', 1)
        ))

        trade_id = cursor.lastrowid
        conn.commit()
        conn.close()

        # Update performance metrics
        self.update_performance_metrics(trade_data['agent_id'])

        return trade_id

    def close_trade(self, trade_id: int, exit_price: float, exit_timestamp: Optional[int] = None):
        """Close an open trade"""
        if exit_timestamp is None:
            exit_timestamp = int(datetime.now().timestamp())

        conn = self.get_connection()
        cursor = conn.cursor()

        # Get trade details
        cursor.execute("SELECT * FROM agent_trades WHERE id = ?", (trade_id,))
        trade = cursor.fetchone()

        if not trade:
            conn.close()
            return

        # Calculate P&L
        quantity = trade['quantity']
        entry_price = trade['entry_price']
        side = trade['side']

        if side == 'buy':
            pnl = (exit_price - entry_price) * quantity
        else:  # sell/short
            pnl = (entry_price - exit_price) * quantity

        # Calculate P&L percentage based on total cost basis (including estimated fees)
        # This gives a more accurate return on capital deployed
        # Note: If fees are tracked in metadata, use them; otherwise estimate ~0.1% round-trip
        metadata = json.loads(trade['metadata']) if trade['metadata'] else {}
        entry_fee = metadata.get('entry_fee', entry_price * quantity * 0.0005)  # 0.05% default
        exit_fee = metadata.get('exit_fee', exit_price * quantity * 0.0005)  # 0.05% default
        total_fees = entry_fee + exit_fee

        # Actual P&L after fees
        pnl_after_fees = pnl - total_fees

        # Cost basis = entry cost + entry fee (capital actually deployed)
        cost_basis = (entry_price * quantity) + entry_fee

        # P&L percent based on cost basis
        pnl_percent = (pnl_after_fees / cost_basis) * 100 if cost_basis > 0 else 0

        # Update trade (store pnl_after_fees for accurate reporting)
        cursor.execute("""
            UPDATE agent_trades
            SET exit_price = ?, exit_timestamp = ?, pnl = ?, pnl_percent = ?, status = 'closed'
            WHERE id = ?
        """, (exit_price, exit_timestamp, pnl_after_fees, pnl_percent, trade_id))

        conn.commit()
        conn.close()

        # Update performance metrics
        self.update_performance_metrics(trade['agent_id'])

    def get_agent_trades(self, agent_id: str, limit: int = 100, status: Optional[str] = None) -> List[Dict]:
        """Get trades for an agent"""
        conn = self.get_connection()
        cursor = conn.cursor()

        if status:
            cursor.execute("""
                SELECT * FROM agent_trades
                WHERE agent_id = ? AND status = ?
                ORDER BY entry_timestamp DESC
                LIMIT ?
            """, (agent_id, status, limit))
        else:
            cursor.execute("""
                SELECT * FROM agent_trades
                WHERE agent_id = ?
                ORDER BY entry_timestamp DESC
                LIMIT ?
            """, (agent_id, limit))

        trades = [dict(row) for row in cursor.fetchall()]
        conn.close()

        return trades

    # ============================================================================
    # PERFORMANCE METRICS
    # ============================================================================

    def update_performance_metrics(self, agent_id: str):
        """Recalculate and update performance metrics for an agent"""
        conn = self.get_connection()
        cursor = conn.cursor()

        # Get all closed trades
        cursor.execute("""
            SELECT * FROM agent_trades
            WHERE agent_id = ? AND status = 'closed'
            ORDER BY entry_timestamp ASC
        """, (agent_id,))

        trades = cursor.fetchall()

        if not trades:
            conn.close()
            return

        # Calculate metrics
        total_trades = len(trades)
        winning_trades = len([t for t in trades if t['pnl'] > 0])
        losing_trades = len([t for t in trades if t['pnl'] < 0])

        total_pnl = sum(t['pnl'] for t in trades)

        # Time-based P&L
        now = datetime.now().timestamp()
        day_ago = now - 86400
        week_ago = now - 604800
        month_ago = now - 2592000

        daily_pnl = sum(t['pnl'] for t in trades if t['exit_timestamp'] and t['exit_timestamp'] >= day_ago)
        weekly_pnl = sum(t['pnl'] for t in trades if t['exit_timestamp'] and t['exit_timestamp'] >= week_ago)
        monthly_pnl = sum(t['pnl'] for t in trades if t['exit_timestamp'] and t['exit_timestamp'] >= month_ago)

        win_rate = (winning_trades / total_trades * 100) if total_trades > 0 else 0

        # Win/Loss stats
        wins = [t['pnl'] for t in trades if t['pnl'] > 0]
        losses = [abs(t['pnl']) for t in trades if t['pnl'] < 0]

        avg_win = sum(wins) / len(wins) if wins else 0
        avg_loss = sum(losses) / len(losses) if losses else 0

        largest_win = max(wins) if wins else 0
        largest_loss = max(losses) if losses else 0

        profit_factor = (sum(wins) / sum(losses)) if losses and sum(losses) > 0 else 0

        # Risk-free rate assumption (annualized, e.g., 4% = 0.04)
        risk_free_rate_annual = 0.04

        # Sharpe Ratio (properly annualized)
        # Convert pnl_percent from percentage (e.g., 5.0) to decimal (0.05)
        sharpe_ratio = 0.0
        sortino_ratio = 0.0

        if len(trades) >= 2:
            # Convert percentage returns to decimal returns
            returns = [t['pnl_percent'] / 100.0 for t in trades if t['pnl_percent'] is not None]

            if returns:
                avg_return = sum(returns) / len(returns)

                # Calculate standard deviation
                variance = sum((r - avg_return) ** 2 for r in returns) / len(returns)
                std_dev = variance ** 0.5

                # Annualization factor (assuming daily trades, ~252 trading days/year)
                # Adjust based on actual trading frequency
                annualization_factor = 252 ** 0.5

                # Daily risk-free rate
                risk_free_rate_daily = risk_free_rate_annual / 252

                # Sharpe Ratio: (avg_return - risk_free_rate) / std_dev * sqrt(252)
                if std_dev > 0:
                    sharpe_ratio = ((avg_return - risk_free_rate_daily) / std_dev) * annualization_factor
                else:
                    sharpe_ratio = 0.0

                # Sortino Ratio: Uses downside deviation instead of standard deviation
                # Only considers negative returns (downside risk)
                negative_returns = [r for r in returns if r < 0]
                if negative_returns:
                    downside_variance = sum(r ** 2 for r in negative_returns) / len(returns)
                    downside_deviation = downside_variance ** 0.5

                    if downside_deviation > 0:
                        sortino_ratio = ((avg_return - risk_free_rate_daily) / downside_deviation) * annualization_factor
                    else:
                        sortino_ratio = 0.0
                else:
                    # No negative returns - exceptional performance
                    sortino_ratio = sharpe_ratio * 1.5 if sharpe_ratio > 0 else 0.0

        # Drawdown calculation
        cumulative_pnl = []
        running_pnl = 0
        for trade in trades:
            running_pnl += trade['pnl']
            cumulative_pnl.append(running_pnl)

        peak = cumulative_pnl[0]
        max_drawdown = 0
        current_drawdown = 0

        for pnl in cumulative_pnl:
            if pnl > peak:
                peak = pnl
            drawdown = (peak - pnl) / abs(peak) if peak != 0 else 0
            max_drawdown = max(max_drawdown, drawdown)

        if cumulative_pnl:
            current_drawdown = (peak - cumulative_pnl[-1]) / abs(peak) if peak != 0 else 0

        # Consecutive wins/losses
        consecutive_wins = 0
        consecutive_losses = 0
        max_consecutive_wins = 0
        max_consecutive_losses = 0
        current_streak_wins = 0
        current_streak_losses = 0

        for trade in trades:
            if trade['pnl'] > 0:
                current_streak_wins += 1
                current_streak_losses = 0
                max_consecutive_wins = max(max_consecutive_wins, current_streak_wins)
            elif trade['pnl'] < 0:
                current_streak_losses += 1
                current_streak_wins = 0
                max_consecutive_losses = max(max_consecutive_losses, current_streak_losses)

        consecutive_wins = current_streak_wins
        consecutive_losses = current_streak_losses

        # Total volume
        total_volume = sum(t['quantity'] * t['entry_price'] for t in trades)

        # Open positions
        cursor.execute("SELECT COUNT(*) as count FROM agent_trades WHERE agent_id = ? AND status = 'open'", (agent_id,))
        positions = cursor.fetchone()['count']

        # Last trade timestamp
        last_trade_timestamp = trades[-1]['exit_timestamp'] if trades[-1]['exit_timestamp'] else trades[-1]['entry_timestamp']

        # Get model name
        cursor.execute("SELECT model FROM agent_trades WHERE agent_id = ? LIMIT 1", (agent_id,))
        result = cursor.fetchone()
        model = result['model'] if result else 'unknown'

        # Upsert performance metrics
        cursor.execute("""
            INSERT OR REPLACE INTO agent_performance (
                agent_id, model, total_trades, winning_trades, losing_trades,
                total_pnl, daily_pnl, weekly_pnl, monthly_pnl, win_rate,
                sharpe_ratio, sortino_ratio, max_drawdown, current_drawdown,
                avg_win, avg_loss, profit_factor, largest_win, largest_loss,
                consecutive_wins, consecutive_losses, max_consecutive_wins,
                max_consecutive_losses, total_volume, positions, last_trade_timestamp,
                last_updated
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            agent_id, model, total_trades, winning_trades, losing_trades,
            total_pnl, daily_pnl, weekly_pnl, monthly_pnl, win_rate,
            sharpe_ratio, sortino_ratio, max_drawdown, current_drawdown,
            avg_win, avg_loss, profit_factor, largest_win, largest_loss,
            consecutive_wins, consecutive_losses, max_consecutive_wins,
            max_consecutive_losses, total_volume, positions, last_trade_timestamp,
            int(datetime.now().timestamp())
        ))

        conn.commit()
        conn.close()

    def get_agent_performance(self, agent_id: str) -> Optional[Dict]:
        """Get performance metrics for an agent"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("SELECT * FROM agent_performance WHERE agent_id = ?", (agent_id,))
        result = cursor.fetchone()

        conn.close()

        return dict(result) if result else None

    def get_leaderboard(self, limit: int = 20) -> List[Dict]:
        """Get leaderboard of all agents sorted by total P&L"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM agent_performance
            ORDER BY total_pnl DESC
            LIMIT ?
        """, (limit,))

        leaderboard = [dict(row) for row in cursor.fetchall()]
        conn.close()

        return leaderboard

    # ============================================================================
    # DECISIONS LOG
    # ============================================================================

    def log_decision(self, decision_data: Dict[str, Any]):
        """Log an agent decision for real-time feed"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            INSERT INTO agent_decisions (
                agent_id, model, decision_type, symbol, decision,
                reasoning, confidence, timestamp, execution_time_ms
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            decision_data['agent_id'],
            decision_data['model'],
            decision_data['decision_type'],
            decision_data.get('symbol'),
            decision_data['decision'],
            decision_data.get('reasoning'),
            decision_data.get('confidence'),
            decision_data.get('timestamp', int(datetime.now().timestamp())),
            decision_data.get('execution_time_ms', 0)
        ))

        conn.commit()
        conn.close()

    def get_recent_decisions(self, limit: int = 50, agent_id: Optional[str] = None) -> List[Dict]:
        """Get recent decisions from all or specific agent"""
        conn = self.get_connection()
        cursor = conn.cursor()

        if agent_id:
            cursor.execute("""
                SELECT * FROM agent_decisions
                WHERE agent_id = ?
                ORDER BY timestamp DESC
                LIMIT ?
            """, (agent_id, limit))
        else:
            cursor.execute("""
                SELECT * FROM agent_decisions
                ORDER BY timestamp DESC
                LIMIT ?
            """, (limit,))

        decisions = [dict(row) for row in cursor.fetchall()]
        conn.close()

        return decisions

    # ============================================================================
    # EVOLUTION
    # ============================================================================

    def log_evolution(self, evolution_data: Dict[str, Any]):
        """Log agent evolution event"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            INSERT INTO agent_evolution (
                agent_id, model, evolution_trigger, old_instructions,
                new_instructions, metrics_before, metrics_after, timestamp, notes
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            evolution_data['agent_id'],
            evolution_data['model'],
            evolution_data['evolution_trigger'],
            evolution_data.get('old_instructions'),
            evolution_data['new_instructions'],
            json.dumps(evolution_data.get('metrics_before', {})),
            json.dumps(evolution_data.get('metrics_after', {})),
            evolution_data.get('timestamp', int(datetime.now().timestamp())),
            evolution_data.get('notes')
        ))

        conn.commit()
        conn.close()

    def get_evolution_history(self, agent_id: str, limit: int = 10) -> List[Dict]:
        """Get evolution history for an agent"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM agent_evolution
            WHERE agent_id = ?
            ORDER BY timestamp DESC
            LIMIT ?
        """, (agent_id, limit))

        history = [dict(row) for row in cursor.fetchall()]
        conn.close()

        return history

    # ============================================================================
    # DEBATE SESSIONS
    # ============================================================================

    def record_debate(self, debate_data: Dict[str, Any]) -> str:
        """Record a debate session"""
        conn = self.get_connection()
        cursor = conn.cursor()

        debate_id = debate_data.get('id', f"debate_{int(datetime.now().timestamp())}")

        cursor.execute("""
            INSERT OR REPLACE INTO debate_sessions (
                id, symbol, bull_model, bear_model, analyst_model,
                bull_argument, bear_argument, analyst_decision,
                final_action, confidence, timestamp, execution_time_ms
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            debate_id,
            debate_data['symbol'],
            debate_data['bull_model'],
            debate_data['bear_model'],
            debate_data['analyst_model'],
            debate_data.get('bull_argument'),
            debate_data.get('bear_argument'),
            debate_data.get('analyst_decision'),
            debate_data.get('final_action'),
            debate_data.get('confidence'),
            debate_data.get('timestamp', int(datetime.now().timestamp())),
            debate_data.get('execution_time_ms', 0)
        ))

        conn.commit()
        conn.close()

        return debate_id

    def get_recent_debates(self, limit: int = 10) -> List[Dict]:
        """Get recent debate sessions"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM debate_sessions
            ORDER BY timestamp DESC
            LIMIT ?
        """, (limit,))

        debates = [dict(row) for row in cursor.fetchall()]
        conn.close()

        return debates


    # ============================================================================
    # COMPETITION STATE MANAGEMENT
    # ============================================================================

    def save_competition_config(self, competition_id: str, name: str, models_json: str,
                                  symbol: str, mode: str, api_keys_json: str = "{}"):
        """Save competition configuration to database"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            INSERT OR REPLACE INTO competition_configs (
                competition_id, name, models_json, symbol, mode,
                api_keys_json, status, created_at
            ) VALUES (?, ?, ?, ?, ?, ?, 'created', ?)
        """, (
            competition_id, name, models_json, symbol, mode,
            api_keys_json, int(datetime.now().timestamp())
        ))

        conn.commit()
        conn.close()
        print(f"[DatabaseManager] Saved competition config: {competition_id}", file=sys.stderr)

    def get_competition_config(self, competition_id: str) -> Optional[Dict]:
        """Get competition configuration from database"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM competition_configs WHERE competition_id = ?
        """, (competition_id,))

        result = cursor.fetchone()
        conn.close()

        if result:
            return dict(result)
        return None

    def get_active_competition(self) -> Optional[Dict]:
        """Get the most recent active competition"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM competition_configs
            WHERE status IN ('created', 'running')
            ORDER BY created_at DESC LIMIT 1
        """)

        result = cursor.fetchone()
        conn.close()

        if result:
            return dict(result)
        return None

    def update_competition_status(self, competition_id: str, status: str):
        """Update competition status"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            UPDATE competition_configs SET status = ? WHERE competition_id = ?
        """, (status, competition_id))

        conn.commit()
        conn.close()

    def save_model_state(self, competition_id: str, model_name: str,
                         capital: float, positions_json: str, trades_count: int,
                         total_pnl: float = 0.0, portfolio_value: float = 0.0):
        """Save model state for competition"""
        conn = self.get_connection()
        cursor = conn.cursor()

        # Generate unique ID
        state_id = f"{competition_id}_{model_name}"

        cursor.execute("""
            INSERT OR REPLACE INTO alpha_model_states (
                id, competition_id, model_name, capital, positions_json,
                trades_count, total_pnl, portfolio_value, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            state_id, competition_id, model_name, capital, positions_json,
            trades_count, total_pnl, portfolio_value, int(datetime.now().timestamp())
        ))

        conn.commit()
        conn.close()

    def get_model_states(self, competition_id: str) -> List[Dict]:
        """Get all model states for a competition"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM alpha_model_states WHERE competition_id = ?
        """, (competition_id,))

        results = [dict(row) for row in cursor.fetchall()]
        conn.close()

        return results

    def _initialize_competition_tables(self, conn: sqlite3.Connection):
        """Create competition-related tables (called from initialize_database)"""
        # Competition config and model state tables
        conn.executescript("""
            CREATE TABLE IF NOT EXISTS competition_configs (
                competition_id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                models_json TEXT NOT NULL,
                symbol TEXT NOT NULL,
                mode TEXT NOT NULL,
                api_keys_json TEXT,
                status TEXT DEFAULT 'created',
                created_at INTEGER
            );

            CREATE TABLE IF NOT EXISTS alpha_performance_snapshots (
                id TEXT PRIMARY KEY,
                competition_id TEXT NOT NULL,
                cycle_number INTEGER NOT NULL,
                model_name TEXT NOT NULL,
                portfolio_value REAL NOT NULL,
                cash REAL NOT NULL,
                pnl REAL NOT NULL,
                return_pct REAL NOT NULL,
                positions_count INTEGER NOT NULL,
                trades_count INTEGER NOT NULL,
                timestamp TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS alpha_decision_logs (
                id TEXT PRIMARY KEY,
                competition_id TEXT NOT NULL,
                model_name TEXT NOT NULL,
                cycle_number INTEGER NOT NULL,
                action TEXT NOT NULL,
                symbol TEXT NOT NULL,
                quantity REAL,
                confidence REAL NOT NULL,
                reasoning TEXT NOT NULL,
                trade_executed TEXT NOT NULL,
                price_at_decision REAL,
                portfolio_value_before REAL,
                portfolio_value_after REAL,
                timestamp TEXT NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_competition_configs_status ON competition_configs(status);
            CREATE INDEX IF NOT EXISTS idx_alpha_snapshots_competition ON alpha_performance_snapshots(competition_id);
            CREATE INDEX IF NOT EXISTS idx_alpha_snapshots_cycle ON alpha_performance_snapshots(cycle_number);
            CREATE INDEX IF NOT EXISTS idx_alpha_decision_logs_competition ON alpha_decision_logs(competition_id);
            CREATE INDEX IF NOT EXISTS idx_alpha_decision_logs_model ON alpha_decision_logs(model_name);
            CREATE INDEX IF NOT EXISTS idx_alpha_decision_logs_cycle ON alpha_decision_logs(cycle_number);
        """)

    def save_decision_log(self, competition_id: str, model_name: str, cycle_number: int,
                         action: str, symbol: str, quantity: float, confidence: float,
                         reasoning: str, trade_executed: bool, price_at_decision: float,
                         portfolio_value_before: float, portfolio_value_after: float):
        """Save decision log to database"""
        conn = self.get_connection()
        cursor = conn.cursor()

        # Generate unique ID
        timestamp = int(datetime.now().timestamp() * 1000)
        decision_id = f"{competition_id}_{model_name}_{cycle_number}_{timestamp}"

        cursor.execute("""
            INSERT INTO alpha_decision_logs (
                id, competition_id, model_name, cycle_number, action, symbol, quantity,
                confidence, reasoning, trade_executed, price_at_decision,
                portfolio_value_before, portfolio_value_after, timestamp
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            decision_id, competition_id, model_name, cycle_number, action, symbol, quantity,
            confidence, reasoning, trade_executed, price_at_decision,
            portfolio_value_before, portfolio_value_after, timestamp
        ))

        conn.commit()
        conn.close()

    def save_performance_snapshot(self, competition_id: str, cycle_number: int,
                                 model_name: str, portfolio_value: float, cash: float,
                                 pnl: float, return_pct: float, positions_count: int,
                                 trades_count: int):
        """Save performance snapshot to database"""
        conn = self.get_connection()
        cursor = conn.cursor()

        # Generate unique ID
        timestamp = int(datetime.now().timestamp() * 1000)
        snapshot_id = f"{competition_id}_{model_name}_{cycle_number}_{timestamp}"

        cursor.execute("""
            INSERT INTO alpha_performance_snapshots (
                id, competition_id, cycle_number, model_name, portfolio_value, cash,
                pnl, return_pct, positions_count, trades_count, timestamp
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            snapshot_id, competition_id, cycle_number, model_name, portfolio_value, cash,
            pnl, return_pct, positions_count, trades_count, timestamp
        ))

        conn.commit()
        conn.close()

    def get_decision_logs(self, competition_id: str, limit: int = 50) -> List[Dict]:
        """Get recent decision logs for a competition"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute("""
            SELECT * FROM alpha_decision_logs
            WHERE competition_id = ?
            ORDER BY timestamp DESC
            LIMIT ?
        """, (competition_id, limit))

        results = [dict(row) for row in cursor.fetchall()]
        conn.close()

        return results


# Thread-safe singleton implementation
import threading

_db_manager = None
_db_manager_lock = threading.Lock()

def get_db_manager() -> DatabaseManager:
    """
    Get singleton database manager instance (thread-safe).

    Uses double-checked locking pattern to ensure thread safety
    while minimizing lock contention after initialization.
    """
    global _db_manager

    # First check without lock (fast path for already-initialized case)
    if _db_manager is not None:
        return _db_manager

    # Acquire lock for initialization
    with _db_manager_lock:
        # Double-check after acquiring lock (another thread may have initialized)
        if _db_manager is None:
            _db_manager = DatabaseManager()

    return _db_manager


def reset_db_manager() -> None:
    """
    Reset the singleton instance (useful for testing).
    Thread-safe reset of the database manager.
    """
    global _db_manager

    with _db_manager_lock:
        _db_manager = None
