"""
Repository Pattern - Clean database access layer

Provides abstraction over data storage for agents.
Similar to ValueCell's repository pattern.
"""

from abc import ABC, abstractmethod
from typing import Dict, Any, Optional, List, Generic, TypeVar
from dataclasses import dataclass, asdict
from datetime import datetime
import json
import sqlite3
from pathlib import Path
import logging

logger = logging.getLogger(__name__)

T = TypeVar('T')


class Repository(ABC, Generic[T]):
    """Abstract base repository"""

    @abstractmethod
    def create(self, entity: T) -> str:
        """Create a new entity"""
        pass

    @abstractmethod
    def get(self, id: str) -> Optional[T]:
        """Get entity by ID"""
        pass

    @abstractmethod
    def update(self, id: str, entity: T) -> bool:
        """Update an entity"""
        pass

    @abstractmethod
    def delete(self, id: str) -> bool:
        """Delete an entity"""
        pass

    @abstractmethod
    def list(self, **filters) -> List[T]:
        """List entities with optional filters"""
        pass


# =============================================================================
# Entity Models
# =============================================================================

@dataclass
class AgentSession:
    """Agent session entity"""
    id: str
    agent_id: str
    user_id: Optional[str]
    started_at: str
    ended_at: Optional[str] = None
    messages: List[Dict[str, Any]] = None
    context: Dict[str, Any] = None
    status: str = "active"

    def __post_init__(self):
        if self.messages is None:
            self.messages = []
        if self.context is None:
            self.context = {}


@dataclass
class AgentMemory:
    """Agent memory entity"""
    id: str
    agent_id: str
    user_id: Optional[str]
    memory_type: str  # "fact", "preference", "context", "long_term"
    content: str
    metadata: Dict[str, Any] = None
    created_at: str = ""
    importance: float = 0.5
    last_accessed: str = ""

    def __post_init__(self):
        if self.metadata is None:
            self.metadata = {}
        if not self.created_at:
            self.created_at = datetime.utcnow().isoformat()
        if not self.last_accessed:
            self.last_accessed = self.created_at


@dataclass
class TradeDecision:
    """Trading decision from AI agent"""
    id: str
    competition_id: str
    model_name: str
    cycle_number: int
    symbol: str
    action: str  # "buy", "sell", "hold"
    quantity: float
    price: Optional[float]
    reasoning: str
    confidence: float
    timestamp: str


@dataclass
class PerformanceSnapshot:
    """Performance snapshot at a point in time"""
    id: str
    competition_id: str
    model_name: str
    cycle_number: int
    portfolio_value: float
    cash: float
    positions_value: float
    total_pnl: float
    return_pct: float
    timestamp: str


# =============================================================================
# SQLite Repository Implementations
# =============================================================================

class SQLiteRepository(Repository[T]):
    """Base SQLite repository with connection management"""

    def __init__(self, db_path: Optional[Path] = None):
        if db_path is None:
            # Default to user data directory
            import os
            app_data = os.getenv('APPDATA', os.path.expanduser('~'))
            db_path = Path(app_data) / 'fincept-terminal' / 'agents.db'

        self.db_path = Path(db_path)
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _get_connection(self) -> sqlite3.Connection:
        conn = sqlite3.connect(str(self.db_path))
        conn.row_factory = sqlite3.Row
        return conn

    @abstractmethod
    def _init_db(self):
        """Initialize database tables"""
        pass

    def _execute(self, query: str, params: tuple = ()) -> List[sqlite3.Row]:
        with self._get_connection() as conn:
            cursor = conn.execute(query, params)
            return cursor.fetchall()

    def _execute_one(self, query: str, params: tuple = ()) -> Optional[sqlite3.Row]:
        rows = self._execute(query, params)
        return rows[0] if rows else None

    def _execute_write(self, query: str, params: tuple = ()) -> int:
        with self._get_connection() as conn:
            cursor = conn.execute(query, params)
            conn.commit()
            return cursor.lastrowid


class AgentSessionRepository(SQLiteRepository[AgentSession]):
    """Repository for agent sessions"""

    def _init_db(self):
        with self._get_connection() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS agent_sessions (
                    id TEXT PRIMARY KEY,
                    agent_id TEXT NOT NULL,
                    user_id TEXT,
                    started_at TEXT NOT NULL,
                    ended_at TEXT,
                    messages TEXT NOT NULL DEFAULT '[]',
                    context TEXT NOT NULL DEFAULT '{}',
                    status TEXT DEFAULT 'active'
                )
            """)
            conn.execute("CREATE INDEX IF NOT EXISTS idx_sessions_agent ON agent_sessions(agent_id)")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_sessions_user ON agent_sessions(user_id)")

    def create(self, entity: AgentSession) -> str:
        self._execute_write(
            """INSERT INTO agent_sessions (id, agent_id, user_id, started_at, ended_at, messages, context, status)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
            (entity.id, entity.agent_id, entity.user_id, entity.started_at, entity.ended_at,
             json.dumps(entity.messages), json.dumps(entity.context), entity.status)
        )
        return entity.id

    def get(self, id: str) -> Optional[AgentSession]:
        row = self._execute_one("SELECT * FROM agent_sessions WHERE id = ?", (id,))
        if row:
            return AgentSession(
                id=row['id'],
                agent_id=row['agent_id'],
                user_id=row['user_id'],
                started_at=row['started_at'],
                ended_at=row['ended_at'],
                messages=json.loads(row['messages']),
                context=json.loads(row['context']),
                status=row['status']
            )
        return None

    def update(self, id: str, entity: AgentSession) -> bool:
        self._execute_write(
            """UPDATE agent_sessions SET ended_at = ?, messages = ?, context = ?, status = ? WHERE id = ?""",
            (entity.ended_at, json.dumps(entity.messages), json.dumps(entity.context), entity.status, id)
        )
        return True

    def delete(self, id: str) -> bool:
        self._execute_write("DELETE FROM agent_sessions WHERE id = ?", (id,))
        return True

    def list(self, agent_id: str = None, user_id: str = None, status: str = None, limit: int = 50) -> List[AgentSession]:
        query = "SELECT * FROM agent_sessions WHERE 1=1"
        params = []

        if agent_id:
            query += " AND agent_id = ?"
            params.append(agent_id)
        if user_id:
            query += " AND user_id = ?"
            params.append(user_id)
        if status:
            query += " AND status = ?"
            params.append(status)

        query += " ORDER BY started_at DESC LIMIT ?"
        params.append(limit)

        rows = self._execute(query, tuple(params))
        return [AgentSession(
            id=row['id'],
            agent_id=row['agent_id'],
            user_id=row['user_id'],
            started_at=row['started_at'],
            ended_at=row['ended_at'],
            messages=json.loads(row['messages']),
            context=json.loads(row['context']),
            status=row['status']
        ) for row in rows]

    def add_message(self, session_id: str, role: str, content: str) -> bool:
        """Add a message to session"""
        session = self.get(session_id)
        if session:
            session.messages.append({
                "role": role,
                "content": content,
                "timestamp": datetime.utcnow().isoformat()
            })
            return self.update(session_id, session)
        return False


class AgentMemoryRepository(SQLiteRepository[AgentMemory]):
    """Repository for agent memories"""

    def _init_db(self):
        with self._get_connection() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS agent_memories (
                    id TEXT PRIMARY KEY,
                    agent_id TEXT NOT NULL,
                    user_id TEXT,
                    memory_type TEXT NOT NULL,
                    content TEXT NOT NULL,
                    metadata TEXT NOT NULL DEFAULT '{}',
                    created_at TEXT NOT NULL,
                    importance REAL DEFAULT 0.5,
                    last_accessed TEXT
                )
            """)
            conn.execute("CREATE INDEX IF NOT EXISTS idx_memories_agent ON agent_memories(agent_id)")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_memories_user ON agent_memories(user_id)")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_memories_type ON agent_memories(memory_type)")

    def create(self, entity: AgentMemory) -> str:
        self._execute_write(
            """INSERT INTO agent_memories (id, agent_id, user_id, memory_type, content, metadata, created_at, importance, last_accessed)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (entity.id, entity.agent_id, entity.user_id, entity.memory_type, entity.content,
             json.dumps(entity.metadata), entity.created_at, entity.importance, entity.last_accessed)
        )
        return entity.id

    def get(self, id: str) -> Optional[AgentMemory]:
        row = self._execute_one("SELECT * FROM agent_memories WHERE id = ?", (id,))
        if row:
            return AgentMemory(
                id=row['id'],
                agent_id=row['agent_id'],
                user_id=row['user_id'],
                memory_type=row['memory_type'],
                content=row['content'],
                metadata=json.loads(row['metadata']),
                created_at=row['created_at'],
                importance=row['importance'],
                last_accessed=row['last_accessed']
            )
        return None

    def update(self, id: str, entity: AgentMemory) -> bool:
        self._execute_write(
            """UPDATE agent_memories SET content = ?, metadata = ?, importance = ?, last_accessed = ? WHERE id = ?""",
            (entity.content, json.dumps(entity.metadata), entity.importance, datetime.utcnow().isoformat(), id)
        )
        return True

    def delete(self, id: str) -> bool:
        self._execute_write("DELETE FROM agent_memories WHERE id = ?", (id,))
        return True

    def list(self, agent_id: str = None, user_id: str = None, memory_type: str = None, limit: int = 100) -> List[AgentMemory]:
        query = "SELECT * FROM agent_memories WHERE 1=1"
        params = []

        if agent_id:
            query += " AND agent_id = ?"
            params.append(agent_id)
        if user_id:
            query += " AND user_id = ?"
            params.append(user_id)
        if memory_type:
            query += " AND memory_type = ?"
            params.append(memory_type)

        query += " ORDER BY importance DESC, last_accessed DESC LIMIT ?"
        params.append(limit)

        rows = self._execute(query, tuple(params))
        return [AgentMemory(
            id=row['id'],
            agent_id=row['agent_id'],
            user_id=row['user_id'],
            memory_type=row['memory_type'],
            content=row['content'],
            metadata=json.loads(row['metadata']),
            created_at=row['created_at'],
            importance=row['importance'],
            last_accessed=row['last_accessed']
        ) for row in rows]

    def search(self, query: str, agent_id: str = None, limit: int = 10) -> List[AgentMemory]:
        """Simple text search in memories"""
        sql = "SELECT * FROM agent_memories WHERE content LIKE ?"
        params = [f"%{query}%"]

        if agent_id:
            sql += " AND agent_id = ?"
            params.append(agent_id)

        sql += " ORDER BY importance DESC LIMIT ?"
        params.append(limit)

        rows = self._execute(sql, tuple(params))
        return [AgentMemory(
            id=row['id'],
            agent_id=row['agent_id'],
            user_id=row['user_id'],
            memory_type=row['memory_type'],
            content=row['content'],
            metadata=json.loads(row['metadata']),
            created_at=row['created_at'],
            importance=row['importance'],
            last_accessed=row['last_accessed']
        ) for row in rows]


class TradeDecisionRepository(SQLiteRepository[TradeDecision]):
    """Repository for trade decisions"""

    def _init_db(self):
        with self._get_connection() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS trade_decisions (
                    id TEXT PRIMARY KEY,
                    competition_id TEXT NOT NULL,
                    model_name TEXT NOT NULL,
                    cycle_number INTEGER NOT NULL,
                    symbol TEXT NOT NULL,
                    action TEXT NOT NULL,
                    quantity REAL NOT NULL,
                    price REAL,
                    reasoning TEXT,
                    confidence REAL DEFAULT 0.5,
                    timestamp TEXT NOT NULL
                )
            """)
            conn.execute("CREATE INDEX IF NOT EXISTS idx_decisions_competition ON trade_decisions(competition_id)")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_decisions_model ON trade_decisions(model_name)")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_decisions_cycle ON trade_decisions(cycle_number)")

    def create(self, entity: TradeDecision) -> str:
        self._execute_write(
            """INSERT INTO trade_decisions (id, competition_id, model_name, cycle_number, symbol, action, quantity, price, reasoning, confidence, timestamp)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (entity.id, entity.competition_id, entity.model_name, entity.cycle_number, entity.symbol,
             entity.action, entity.quantity, entity.price, entity.reasoning, entity.confidence, entity.timestamp)
        )
        return entity.id

    def get(self, id: str) -> Optional[TradeDecision]:
        row = self._execute_one("SELECT * FROM trade_decisions WHERE id = ?", (id,))
        if row:
            return TradeDecision(**dict(row))
        return None

    def update(self, id: str, entity: TradeDecision) -> bool:
        self._execute_write(
            """UPDATE trade_decisions SET action = ?, quantity = ?, price = ?, reasoning = ?, confidence = ? WHERE id = ?""",
            (entity.action, entity.quantity, entity.price, entity.reasoning, entity.confidence, id)
        )
        return True

    def delete(self, id: str) -> bool:
        self._execute_write("DELETE FROM trade_decisions WHERE id = ?", (id,))
        return True

    def list(self, competition_id: str = None, model_name: str = None, cycle_number: int = None, limit: int = 100) -> List[TradeDecision]:
        query = "SELECT * FROM trade_decisions WHERE 1=1"
        params = []

        if competition_id:
            query += " AND competition_id = ?"
            params.append(competition_id)
        if model_name:
            query += " AND model_name = ?"
            params.append(model_name)
        if cycle_number is not None:
            query += " AND cycle_number = ?"
            params.append(cycle_number)

        query += " ORDER BY timestamp DESC LIMIT ?"
        params.append(limit)

        rows = self._execute(query, tuple(params))
        return [TradeDecision(**dict(row)) for row in rows]


class PerformanceSnapshotRepository(SQLiteRepository[PerformanceSnapshot]):
    """Repository for performance snapshots"""

    def _init_db(self):
        with self._get_connection() as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS performance_snapshots (
                    id TEXT PRIMARY KEY,
                    competition_id TEXT NOT NULL,
                    model_name TEXT NOT NULL,
                    cycle_number INTEGER NOT NULL,
                    portfolio_value REAL NOT NULL,
                    cash REAL NOT NULL,
                    positions_value REAL NOT NULL,
                    total_pnl REAL DEFAULT 0,
                    return_pct REAL DEFAULT 0,
                    timestamp TEXT NOT NULL
                )
            """)
            conn.execute("CREATE INDEX IF NOT EXISTS idx_snapshots_competition ON performance_snapshots(competition_id)")
            conn.execute("CREATE INDEX IF NOT EXISTS idx_snapshots_model ON performance_snapshots(model_name)")

    def create(self, entity: PerformanceSnapshot) -> str:
        self._execute_write(
            """INSERT INTO performance_snapshots (id, competition_id, model_name, cycle_number, portfolio_value, cash, positions_value, total_pnl, return_pct, timestamp)
               VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
            (entity.id, entity.competition_id, entity.model_name, entity.cycle_number, entity.portfolio_value,
             entity.cash, entity.positions_value, entity.total_pnl, entity.return_pct, entity.timestamp)
        )
        return entity.id

    def get(self, id: str) -> Optional[PerformanceSnapshot]:
        row = self._execute_one("SELECT * FROM performance_snapshots WHERE id = ?", (id,))
        if row:
            return PerformanceSnapshot(**dict(row))
        return None

    def update(self, id: str, entity: PerformanceSnapshot) -> bool:
        self._execute_write(
            """UPDATE performance_snapshots SET portfolio_value = ?, cash = ?, positions_value = ?, total_pnl = ?, return_pct = ? WHERE id = ?""",
            (entity.portfolio_value, entity.cash, entity.positions_value, entity.total_pnl, entity.return_pct, id)
        )
        return True

    def delete(self, id: str) -> bool:
        self._execute_write("DELETE FROM performance_snapshots WHERE id = ?", (id,))
        return True

    def list(self, competition_id: str = None, model_name: str = None, limit: int = 500) -> List[PerformanceSnapshot]:
        query = "SELECT * FROM performance_snapshots WHERE 1=1"
        params = []

        if competition_id:
            query += " AND competition_id = ?"
            params.append(competition_id)
        if model_name:
            query += " AND model_name = ?"
            params.append(model_name)

        query += " ORDER BY cycle_number ASC LIMIT ?"
        params.append(limit)

        rows = self._execute(query, tuple(params))
        return [PerformanceSnapshot(**dict(row)) for row in rows]


# =============================================================================
# Repository Factory
# =============================================================================

class RepositoryFactory:
    """Factory for creating repository instances"""

    _instances: Dict[str, Repository] = {}

    @classmethod
    def get_session_repository(cls, db_path: Path = None) -> AgentSessionRepository:
        key = f"session:{db_path}"
        if key not in cls._instances:
            cls._instances[key] = AgentSessionRepository(db_path)
        return cls._instances[key]

    @classmethod
    def get_memory_repository(cls, db_path: Path = None) -> AgentMemoryRepository:
        key = f"memory:{db_path}"
        if key not in cls._instances:
            cls._instances[key] = AgentMemoryRepository(db_path)
        return cls._instances[key]

    @classmethod
    def get_trade_decision_repository(cls, db_path: Path = None) -> TradeDecisionRepository:
        key = f"trade_decision:{db_path}"
        if key not in cls._instances:
            cls._instances[key] = TradeDecisionRepository(db_path)
        return cls._instances[key]

    @classmethod
    def get_performance_repository(cls, db_path: Path = None) -> PerformanceSnapshotRepository:
        key = f"performance:{db_path}"
        if key not in cls._instances:
            cls._instances[key] = PerformanceSnapshotRepository(db_path)
        return cls._instances[key]

    @classmethod
    def clear_instances(cls):
        """Clear all cached instances"""
        cls._instances.clear()
