"""
Database Persistence Layer

SQLite-based persistence for competition state.
"""

import json
import sqlite3
import os
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any
from contextlib import contextmanager

from alpha_arena.types.models import (
    Competition,
    CompetitionConfig,
    CompetitionStatus,
    ModelDecision,
    PerformanceSnapshot,
    LeaderboardEntry,
)
from alpha_arena.utils.logging import get_logger

logger = get_logger("database")


def get_main_db_path() -> Optional[Path]:
    """Get the path to the main Fincept Terminal database for paper trading integration."""
    possible_paths = [
        # Windows AppData
        Path(os.environ.get("APPDATA", "")) / "fincept-terminal" / "fincept_terminal.db",
        Path(os.environ.get("LOCALAPPDATA", "")) / "fincept-terminal" / "fincept_terminal.db",
        # Linux/Mac
        Path.home() / ".config" / "fincept-terminal" / "fincept_terminal.db",
        Path.home() / ".local" / "share" / "fincept-terminal" / "fincept_terminal.db",
    ]

    for path in possible_paths:
        if path.exists():
            return path

    return None


class AlphaArenaDatabase:
    """
    SQLite database for Alpha Arena persistence.
    """

    def __init__(self, db_path: Optional[str] = None):
        if db_path is None:
            # Default to user's app data directory
            app_data = Path.home() / ".fincept" / "alpha_arena"
            app_data.mkdir(parents=True, exist_ok=True)
            db_path = str(app_data / "alpha_arena.db")

        self.db_path = db_path
        self._initialize_db()

    @contextmanager
    def _get_connection(self):
        """Get a database connection."""
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        try:
            yield conn
            conn.commit()
        except Exception:
            conn.rollback()
            raise
        finally:
            conn.close()

    def _initialize_db(self):
        """Initialize database schema."""
        with self._get_connection() as conn:
            cursor = conn.cursor()

            # Competitions table
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS competitions (
                    competition_id TEXT PRIMARY KEY,
                    config TEXT NOT NULL,
                    status TEXT NOT NULL,
                    cycle_count INTEGER DEFAULT 0,
                    start_time TEXT,
                    end_time TEXT,
                    created_at TEXT NOT NULL,
                    updated_at TEXT NOT NULL
                )
            """)

            # Decisions table
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS decisions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    competition_id TEXT NOT NULL,
                    model_name TEXT NOT NULL,
                    cycle_number INTEGER NOT NULL,
                    action TEXT NOT NULL,
                    symbol TEXT NOT NULL,
                    quantity REAL NOT NULL,
                    confidence REAL NOT NULL,
                    reasoning TEXT,
                    price_at_decision REAL,
                    trade_status TEXT,
                    trade_pnl REAL,
                    timestamp TEXT NOT NULL,
                    FOREIGN KEY (competition_id) REFERENCES competitions(competition_id)
                )
            """)

            # Snapshots table
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS snapshots (
                    id TEXT PRIMARY KEY,
                    competition_id TEXT NOT NULL,
                    model_name TEXT NOT NULL,
                    cycle_number INTEGER NOT NULL,
                    portfolio_value REAL NOT NULL,
                    cash REAL NOT NULL,
                    pnl REAL NOT NULL,
                    return_pct REAL NOT NULL,
                    positions_count INTEGER DEFAULT 0,
                    trades_count INTEGER DEFAULT 0,
                    timestamp TEXT NOT NULL,
                    FOREIGN KEY (competition_id) REFERENCES competitions(competition_id)
                )
            """)

            # Leaderboard history table
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS leaderboard_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    competition_id TEXT NOT NULL,
                    cycle_number INTEGER NOT NULL,
                    leaderboard TEXT NOT NULL,
                    timestamp TEXT NOT NULL,
                    FOREIGN KEY (competition_id) REFERENCES competitions(competition_id)
                )
            """)

            # Create indexes
            cursor.execute("""
                CREATE INDEX IF NOT EXISTS idx_decisions_competition
                ON decisions(competition_id, model_name, cycle_number)
            """)

            cursor.execute("""
                CREATE INDEX IF NOT EXISTS idx_snapshots_competition
                ON snapshots(competition_id, model_name, cycle_number)
            """)

            logger.info(f"Database initialized at {self.db_path}")

    # ==================== Competition Methods ====================

    def save_competition(self, config: CompetitionConfig, status: CompetitionStatus) -> bool:
        """Save or update a competition."""
        now = datetime.now().isoformat()

        with self._get_connection() as conn:
            cursor = conn.cursor()

            cursor.execute("""
                INSERT OR REPLACE INTO competitions
                (competition_id, config, status, cycle_count, start_time, end_time, created_at, updated_at)
                VALUES (?, ?, ?, 0, NULL, NULL, ?, ?)
            """, (
                config.competition_id,
                json.dumps(config.model_dump()),
                status.value,
                now,
                now,
            ))

        logger.info(f"Saved competition: {config.competition_id}")
        return True

    def update_competition_status(
        self,
        competition_id: str,
        status: CompetitionStatus,
        cycle_count: Optional[int] = None,
        start_time: Optional[datetime] = None,
        end_time: Optional[datetime] = None,
    ) -> bool:
        """Update competition status."""
        now = datetime.now().isoformat()

        with self._get_connection() as conn:
            cursor = conn.cursor()

            updates = ["status = ?", "updated_at = ?"]
            values = [status.value, now]

            if cycle_count is not None:
                updates.append("cycle_count = ?")
                values.append(cycle_count)

            if start_time is not None:
                updates.append("start_time = ?")
                values.append(start_time.isoformat())

            if end_time is not None:
                updates.append("end_time = ?")
                values.append(end_time.isoformat())

            values.append(competition_id)

            cursor.execute(f"""
                UPDATE competitions
                SET {', '.join(updates)}
                WHERE competition_id = ?
            """, values)

        return True

    def get_competition(self, competition_id: str) -> Optional[Dict]:
        """Get competition by ID."""
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("""
                SELECT * FROM competitions WHERE competition_id = ?
            """, (competition_id,))
            row = cursor.fetchone()

            if row:
                return {
                    "competition_id": row["competition_id"],
                    "config": json.loads(row["config"]),
                    "status": row["status"],
                    "cycle_count": row["cycle_count"],
                    "start_time": row["start_time"],
                    "end_time": row["end_time"],
                    "created_at": row["created_at"],
                    "updated_at": row["updated_at"],
                }
            return None

    def list_competitions(self, limit: int = 50) -> List[Dict]:
        """List all competitions."""
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("""
                SELECT * FROM competitions
                ORDER BY created_at DESC
                LIMIT ?
            """, (limit,))

            return [
                {
                    "competition_id": row["competition_id"],
                    "config": json.loads(row["config"]),
                    "status": row["status"],
                    "cycle_count": row["cycle_count"],
                    "start_time": row["start_time"],
                    "end_time": row["end_time"],
                    "created_at": row["created_at"],
                }
                for row in cursor.fetchall()
            ]

    def delete_competition(self, competition_id: str) -> bool:
        """Delete a competition and all related data."""
        with self._get_connection() as conn:
            cursor = conn.cursor()

            # Delete related data
            cursor.execute("DELETE FROM decisions WHERE competition_id = ?", (competition_id,))
            cursor.execute("DELETE FROM snapshots WHERE competition_id = ?", (competition_id,))
            cursor.execute("DELETE FROM leaderboard_history WHERE competition_id = ?", (competition_id,))
            cursor.execute("DELETE FROM competitions WHERE competition_id = ?", (competition_id,))

        logger.info(f"Deleted competition: {competition_id}")
        return True

    # ==================== Decision Methods ====================

    def save_decision(self, decision: ModelDecision) -> int:
        """Save a trading decision to both local and main database."""
        with self._get_connection() as conn:
            cursor = conn.cursor()

            trade_status = None
            trade_pnl = None
            if decision.trade_executed:
                trade_status = decision.trade_executed.status
                trade_pnl = decision.trade_executed.pnl

            cursor.execute("""
                INSERT INTO decisions
                (competition_id, model_name, cycle_number, action, symbol, quantity,
                 confidence, reasoning, price_at_decision, trade_status, trade_pnl, timestamp)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                decision.competition_id,
                decision.model_name,
                decision.cycle_number,
                decision.action.value if hasattr(decision.action, 'value') else str(decision.action),
                decision.symbol,
                decision.quantity,
                decision.confidence,
                decision.reasoning,
                decision.price_at_decision,
                trade_status,
                trade_pnl,
                decision.timestamp.isoformat(),
            ))

            local_id = cursor.lastrowid

        # Also save to main Fincept database for UI integration
        self._save_decision_to_main_db(decision, trade_status, trade_pnl)

        return local_id

    def _save_decision_to_main_db(self, decision: ModelDecision, trade_status: Optional[str], trade_pnl: Optional[float]):
        """Save decision to main Fincept Terminal database."""
        main_db_path = get_main_db_path()
        if not main_db_path:
            logger.warning("Main database not found, skipping decision sync")
            return

        conn = None
        try:
            import uuid
            conn = sqlite3.connect(str(main_db_path), timeout=10.0)
            cursor = conn.cursor()

            decision_id = str(uuid.uuid4())
            action_value = decision.action.value if hasattr(decision.action, 'value') else str(decision.action)

            cursor.execute("""
                INSERT INTO alpha_arena_decisions
                (id, competition_id, model_name, cycle_number, symbol, action, quantity,
                 confidence, reasoning, trade_executed, price_at_decision,
                 portfolio_value_before, portfolio_value_after, pnl, timestamp)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                decision_id,
                decision.competition_id,
                decision.model_name,
                decision.cycle_number,
                decision.symbol,
                action_value,
                decision.quantity,
                decision.confidence,
                decision.reasoning,
                1 if trade_status == "executed" else 0,
                decision.price_at_decision,
                decision.portfolio_value_before,
                decision.portfolio_value_after,
                trade_pnl or 0,
                decision.timestamp.isoformat(),
            ))

            conn.commit()
            logger.debug(f"Decision synced to main database: {decision_id}")
        except Exception as e:
            logger.warning(f"Failed to sync decision to main database: {e}")
        finally:
            if conn:
                conn.close()

    def get_decisions(
        self,
        competition_id: str,
        model_name: Optional[str] = None,
        limit: int = 100,
    ) -> List[Dict]:
        """Get decisions for a competition."""
        with self._get_connection() as conn:
            cursor = conn.cursor()

            if model_name:
                cursor.execute("""
                    SELECT * FROM decisions
                    WHERE competition_id = ? AND model_name = ?
                    ORDER BY cycle_number DESC
                    LIMIT ?
                """, (competition_id, model_name, limit))
            else:
                cursor.execute("""
                    SELECT * FROM decisions
                    WHERE competition_id = ?
                    ORDER BY cycle_number DESC, model_name
                    LIMIT ?
                """, (competition_id, limit))

            return [dict(row) for row in cursor.fetchall()]

    # ==================== Snapshot Methods ====================

    def save_snapshot(self, snapshot: PerformanceSnapshot) -> bool:
        """Save a performance snapshot to both local and main database."""
        with self._get_connection() as conn:
            cursor = conn.cursor()

            cursor.execute("""
                INSERT OR REPLACE INTO snapshots
                (id, competition_id, model_name, cycle_number, portfolio_value,
                 cash, pnl, return_pct, positions_count, trades_count, timestamp)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                snapshot.id,
                snapshot.competition_id,
                snapshot.model_name,
                snapshot.cycle_number,
                snapshot.portfolio_value,
                snapshot.cash,
                snapshot.pnl,
                snapshot.return_pct,
                snapshot.positions_count,
                snapshot.trades_count,
                snapshot.timestamp.isoformat(),
            ))

        # Also save to main database
        self._save_snapshot_to_main_db(snapshot)

        return True

    def _save_snapshot_to_main_db(self, snapshot: PerformanceSnapshot):
        """Save snapshot to main Fincept Terminal database."""
        main_db_path = get_main_db_path()
        if not main_db_path:
            return

        conn = None
        try:
            conn = sqlite3.connect(str(main_db_path), timeout=10.0)
            cursor = conn.cursor()

            cursor.execute("""
                INSERT OR REPLACE INTO alpha_arena_snapshots
                (id, competition_id, model_name, cycle_number, portfolio_value,
                 cash, pnl, return_pct, positions_count, trades_count, timestamp)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                snapshot.id,
                snapshot.competition_id,
                snapshot.model_name,
                snapshot.cycle_number,
                snapshot.portfolio_value,
                snapshot.cash,
                snapshot.pnl,
                snapshot.return_pct,
                snapshot.positions_count,
                snapshot.trades_count,
                snapshot.timestamp.isoformat(),
            ))

            conn.commit()
        except Exception as e:
            logger.warning(f"Failed to sync snapshot to main database: {e}")
        finally:
            if conn:
                conn.close()

    def get_snapshots(
        self,
        competition_id: str,
        model_name: Optional[str] = None,
    ) -> List[Dict]:
        """Get snapshots for charting."""
        with self._get_connection() as conn:
            cursor = conn.cursor()

            if model_name:
                cursor.execute("""
                    SELECT * FROM snapshots
                    WHERE competition_id = ? AND model_name = ?
                    ORDER BY cycle_number
                """, (competition_id, model_name))
            else:
                cursor.execute("""
                    SELECT * FROM snapshots
                    WHERE competition_id = ?
                    ORDER BY cycle_number, model_name
                """, (competition_id,))

            return [dict(row) for row in cursor.fetchall()]

    # ==================== Leaderboard Methods ====================

    def save_leaderboard(
        self,
        competition_id: str,
        cycle_number: int,
        leaderboard: List[LeaderboardEntry],
    ) -> bool:
        """Save leaderboard snapshot to both local and main database."""
        import uuid
        leaderboard_json = json.dumps([e.model_dump() for e in leaderboard])
        now = datetime.now().isoformat()

        with self._get_connection() as conn:
            cursor = conn.cursor()

            cursor.execute("""
                INSERT INTO leaderboard_history
                (competition_id, cycle_number, leaderboard, timestamp)
                VALUES (?, ?, ?, ?)
            """, (
                competition_id,
                cycle_number,
                leaderboard_json,
                now,
            ))

        # Also save to main database
        self._save_leaderboard_to_main_db(competition_id, cycle_number, leaderboard_json, now)

        return True

    def _save_leaderboard_to_main_db(self, competition_id: str, cycle_number: int, leaderboard_json: str, timestamp: str):
        """Save leaderboard to main Fincept Terminal database."""
        main_db_path = get_main_db_path()
        if not main_db_path:
            return

        conn = None
        try:
            import uuid
            conn = sqlite3.connect(str(main_db_path), timeout=10.0)
            cursor = conn.cursor()

            leaderboard_id = str(uuid.uuid4())
            cursor.execute("""
                INSERT INTO alpha_arena_leaderboard
                (id, competition_id, cycle_number, leaderboard_json, timestamp)
                VALUES (?, ?, ?, ?, ?)
            """, (
                leaderboard_id,
                competition_id,
                cycle_number,
                leaderboard_json,
                timestamp,
            ))

            conn.commit()
        except Exception as e:
            logger.warning(f"Failed to sync leaderboard to main database: {e}")
        finally:
            if conn:
                conn.close()

    def get_latest_leaderboard(self, competition_id: str) -> Optional[List[Dict]]:
        """Get the latest leaderboard for a competition."""
        with self._get_connection() as conn:
            cursor = conn.cursor()

            cursor.execute("""
                SELECT leaderboard FROM leaderboard_history
                WHERE competition_id = ?
                ORDER BY cycle_number DESC
                LIMIT 1
            """, (competition_id,))

            row = cursor.fetchone()
            if row:
                return json.loads(row["leaderboard"])
            return None


# Singleton instance
_db: Optional[AlphaArenaDatabase] = None


def get_database(db_path: Optional[str] = None) -> AlphaArenaDatabase:
    """Get the database instance."""
    global _db

    if _db is None:
        _db = AlphaArenaDatabase(db_path)

    return _db
