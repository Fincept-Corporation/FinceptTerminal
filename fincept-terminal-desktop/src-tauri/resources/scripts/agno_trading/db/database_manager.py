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
            # Default to agno_trading.db in same directory
            db_path = os.path.join(os.path.dirname(__file__), 'agno_trading.db')

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

        if os.path.exists(schema_path):
            with open(schema_path, 'r') as f:
                schema_sql = f.read()

            conn = self.get_connection()
            conn.executescript(schema_sql)
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

        pnl_percent = (pnl / (entry_price * quantity)) * 100 if (entry_price * quantity) > 0 else 0

        # Update trade
        cursor.execute("""
            UPDATE agent_trades
            SET exit_price = ?, exit_timestamp = ?, pnl = ?, pnl_percent = ?, status = 'closed'
            WHERE id = ?
        """, (exit_price, exit_timestamp, pnl, pnl_percent, trade_id))

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

        # Sharpe Ratio (simplified)
        if len(trades) >= 2:
            returns = [t['pnl_percent'] for t in trades]
            avg_return = sum(returns) / len(returns)
            std_dev = (sum((r - avg_return) ** 2 for r in returns) / len(returns)) ** 0.5
            sharpe_ratio = (avg_return / std_dev) if std_dev > 0 else 0
        else:
            sharpe_ratio = 0

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
            sharpe_ratio, sharpe_ratio, max_drawdown, current_drawdown,
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


# Global instance
_db_manager = None

def get_db_manager() -> DatabaseManager:
    """Get singleton database manager instance"""
    global _db_manager
    if _db_manager is None:
        _db_manager = DatabaseManager()
    return _db_manager
