"""
Memory Module - Conversation history and user memory management

Provides:
- Conversation history management
- User memory storage
- Memory optimization strategies
- Session persistence
- Agentic memory (agent decides what to remember)
- Financial context memory
"""

from typing import Dict, Any, Optional, List, Callable
import logging
import json
from datetime import datetime

logger = logging.getLogger(__name__)


class MemoryModule:
    """
    Unified memory management for agents.

    Supports:
    - Agno MemoryManager for conversation history
    - User memory for persistent user preferences
    - Multiple optimization strategies
    """

    # Memory optimization strategies
    OPTIMIZATION_STRATEGIES = {
        "summarize": "SummarizeStrategy",
        "sliding_window": "SlidingWindowStrategy",
        "token_limit": "TokenLimitStrategy",
    }

    def __init__(
        self,
        db_path: Optional[str] = None,
        user_id: Optional[str] = None,
        enable_user_memory: bool = False,
        optimization_strategy: Optional[str] = None,
        **kwargs
    ):
        """
        Initialize memory module.

        Args:
            db_path: Path to SQLite database for persistence
            user_id: User ID for user-specific memory
            enable_user_memory: Enable persistent user memory
            optimization_strategy: Memory optimization strategy
            **kwargs: Additional configuration
        """
        self.db_path = db_path
        self.user_id = user_id
        self.enable_user_memory = enable_user_memory
        self.optimization_strategy = optimization_strategy
        self.config = kwargs

        self._memory_manager = None
        self._user_memory = None
        self._initialized = False

    def initialize(self) -> None:
        """Initialize memory systems"""
        if self._initialized:
            return

        try:
            # Initialize MemoryManager
            from agno.memory import MemoryManager

            manager_kwargs = {}

            # Add optimization strategy if specified
            if self.optimization_strategy:
                strategy = self._create_optimization_strategy()
                if strategy:
                    manager_kwargs["optimization_strategy"] = strategy

            self._memory_manager = MemoryManager(**manager_kwargs)

            # Initialize UserMemory if enabled
            if self.enable_user_memory and self.user_id:
                try:
                    from agno.memory import UserMemory
                    self._user_memory = UserMemory(
                        user_id=self.user_id,
                        db_path=self.db_path
                    )
                except ImportError:
                    logger.warning("UserMemory not available in this Agno version")

            self._initialized = True
            logger.debug("Memory module initialized")

        except ImportError as e:
            logger.warning(f"Memory module initialization failed: {e}")
        except Exception as e:
            logger.error(f"Memory module error: {e}")

    def _create_optimization_strategy(self) -> Optional[Any]:
        """Create memory optimization strategy"""
        if not self.optimization_strategy:
            return None

        try:
            from agno.memory import (
                MemoryOptimizationStrategyFactory,
                MemoryOptimizationStrategyType
            )

            strategy_map = {
                "summarize": MemoryOptimizationStrategyType.SUMMARIZE,
            }

            strategy_type = strategy_map.get(self.optimization_strategy.lower())
            if strategy_type:
                return MemoryOptimizationStrategyFactory.create(strategy_type)

        except ImportError:
            logger.warning("Memory optimization strategies not available")
        except Exception as e:
            logger.warning(f"Failed to create optimization strategy: {e}")

        return None

    def get_memory_manager(self) -> Optional[Any]:
        """Get the Agno MemoryManager instance"""
        self.initialize()
        return self._memory_manager

    def get_user_memory(self) -> Optional[Any]:
        """Get the UserMemory instance"""
        self.initialize()
        return self._user_memory

    def add_message(
        self,
        role: str,
        content: str,
        session_id: Optional[str] = None,
        **metadata
    ) -> None:
        """
        Add a message to memory.

        Args:
            role: Message role ('user', 'assistant', 'system')
            content: Message content
            session_id: Optional session ID
            **metadata: Additional metadata
        """
        self.initialize()

        if self._memory_manager:
            try:
                self._memory_manager.add_message(
                    role=role,
                    content=content,
                    session_id=session_id,
                    **metadata
                )
            except Exception as e:
                logger.error(f"Failed to add message to memory: {e}")

    def get_messages(
        self,
        session_id: Optional[str] = None,
        limit: Optional[int] = None
    ) -> List[Dict[str, Any]]:
        """
        Get messages from memory.

        Args:
            session_id: Optional session ID filter
            limit: Maximum number of messages

        Returns:
            List of message dictionaries
        """
        self.initialize()

        if self._memory_manager:
            try:
                return self._memory_manager.get_messages(
                    session_id=session_id,
                    limit=limit
                )
            except Exception as e:
                logger.error(f"Failed to get messages: {e}")

        return []

    def clear_session(self, session_id: str) -> None:
        """Clear messages for a session"""
        self.initialize()

        if self._memory_manager:
            try:
                self._memory_manager.clear_session(session_id)
            except Exception as e:
                logger.error(f"Failed to clear session: {e}")

    def store_user_preference(
        self,
        key: str,
        value: Any,
        category: Optional[str] = None
    ) -> None:
        """
        Store a user preference.

        Args:
            key: Preference key
            value: Preference value
            category: Optional category
        """
        if self._user_memory:
            try:
                self._user_memory.store(key=key, value=value, category=category)
            except Exception as e:
                logger.error(f"Failed to store user preference: {e}")

    def get_user_preference(
        self,
        key: str,
        default: Any = None
    ) -> Any:
        """
        Get a user preference.

        Args:
            key: Preference key
            default: Default value if not found

        Returns:
            Preference value or default
        """
        if self._user_memory:
            try:
                return self._user_memory.get(key=key, default=default)
            except Exception as e:
                logger.error(f"Failed to get user preference: {e}")

        return default

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert memory module to agent configuration.

        Returns:
            Dict to pass to Agent constructor
        """
        self.initialize()

        config = {}

        if self._memory_manager:
            config["memory"] = self._memory_manager

        if self._user_memory:
            config["user_memory"] = self._user_memory

        return config

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "MemoryModule":
        """
        Create MemoryModule from configuration dict.

        Args:
            config: Memory configuration

        Returns:
            MemoryModule instance
        """
        return cls(
            db_path=config.get("db_path"),
            user_id=config.get("user_id"),
            enable_user_memory=config.get("enable_user_memory", False),
            optimization_strategy=config.get("optimization_strategy"),
            **config.get("options", {})
        )

    @classmethod
    def list_strategies(cls) -> List[str]:
        """List available optimization strategies"""
        return list(cls.OPTIMIZATION_STRATEGIES.keys())


class AgenticMemoryModule:
    """
    Agentic memory - agent decides what to remember.

    Features:
    - Agent autonomously stores important information
    - Retrieves relevant memories for context
    - Financial-specific memory types
    """

    MEMORY_TYPES = {
        "fact": "Factual information about markets/securities",
        "preference": "User preferences and settings",
        "analysis": "Previous analysis results",
        "decision": "Trading decisions and rationale",
        "alert": "Price alerts and triggers",
        "watchlist": "Watched securities and reasons"
    }

    def __init__(
        self,
        user_id: str,
        db_path: Optional[str] = None,
        enable_auto_store: bool = True,
        max_memories: int = 1000
    ):
        """
        Initialize agentic memory.

        Args:
            user_id: User ID
            db_path: Database path
            enable_auto_store: Allow agent to auto-store
            max_memories: Maximum memories to keep
        """
        self.user_id = user_id
        self.db_path = db_path or f"./memories_{user_id}.db"
        self.enable_auto_store = enable_auto_store
        self.max_memories = max_memories

        self._memories: List[Dict[str, Any]] = []
        self._init_db()

    def _init_db(self) -> None:
        """Initialize database for memory persistence."""
        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("""
                CREATE TABLE IF NOT EXISTS memories (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id TEXT,
                    memory_type TEXT,
                    content TEXT,
                    metadata TEXT,
                    relevance_score REAL DEFAULT 1.0,
                    access_count INTEGER DEFAULT 0,
                    created_at TEXT,
                    last_accessed TEXT
                )
            """)

            cursor.execute("""
                CREATE INDEX IF NOT EXISTS idx_user_type
                ON memories(user_id, memory_type)
            """)

            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to initialize memory DB: {e}")

    def store(
        self,
        content: str,
        memory_type: str = "fact",
        metadata: Dict[str, Any] = None,
        relevance_score: float = 1.0
    ) -> int:
        """
        Store a memory.

        Args:
            content: Memory content
            memory_type: Type of memory
            metadata: Additional metadata
            relevance_score: Importance score (0-1)

        Returns:
            Memory ID
        """
        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            now = datetime.utcnow().isoformat()

            cursor.execute("""
                INSERT INTO memories
                (user_id, memory_type, content, metadata, relevance_score, created_at, last_accessed)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (
                self.user_id,
                memory_type,
                content,
                json.dumps(metadata or {}),
                relevance_score,
                now,
                now
            ))

            memory_id = cursor.lastrowid

            # Enforce max memories
            cursor.execute("""
                DELETE FROM memories
                WHERE user_id = ? AND id NOT IN (
                    SELECT id FROM memories
                    WHERE user_id = ?
                    ORDER BY relevance_score DESC, last_accessed DESC
                    LIMIT ?
                )
            """, (self.user_id, self.user_id, self.max_memories))

            conn.commit()
            conn.close()

            logger.debug(f"Stored memory {memory_id}: {content[:50]}...")
            return memory_id

        except Exception as e:
            logger.error(f"Failed to store memory: {e}")
            return -1

    def recall(
        self,
        query: str = None,
        memory_type: str = None,
        limit: int = 5
    ) -> List[Dict[str, Any]]:
        """
        Recall memories.

        Args:
            query: Search query (searches content)
            memory_type: Filter by type
            limit: Maximum results

        Returns:
            List of memory dicts
        """
        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            sql = "SELECT * FROM memories WHERE user_id = ?"
            params = [self.user_id]

            if memory_type:
                sql += " AND memory_type = ?"
                params.append(memory_type)

            if query:
                sql += " AND content LIKE ?"
                params.append(f"%{query}%")

            sql += " ORDER BY relevance_score DESC, last_accessed DESC LIMIT ?"
            params.append(limit)

            cursor.execute(sql, params)
            rows = cursor.fetchall()
            columns = [desc[0] for desc in cursor.description]

            # Update access counts
            if rows:
                ids = [row[0] for row in rows]
                placeholders = ",".join("?" * len(ids))
                cursor.execute(f"""
                    UPDATE memories
                    SET access_count = access_count + 1,
                        last_accessed = ?
                    WHERE id IN ({placeholders})
                """, [datetime.utcnow().isoformat()] + ids)
                conn.commit()

            conn.close()

            memories = []
            for row in rows:
                memory = dict(zip(columns, row))
                memory["metadata"] = json.loads(memory.get("metadata", "{}"))
                memories.append(memory)

            return memories

        except Exception as e:
            logger.error(f"Failed to recall memories: {e}")
            return []

    def update_relevance(self, memory_id: int, relevance_score: float) -> None:
        """Update memory relevance score."""
        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("""
                UPDATE memories SET relevance_score = ? WHERE id = ?
            """, (relevance_score, memory_id))

            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to update relevance: {e}")

    def forget(self, memory_id: int) -> None:
        """Delete a memory."""
        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("DELETE FROM memories WHERE id = ?", (memory_id,))

            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to forget memory: {e}")

    def get_context_for_query(self, query: str, limit: int = 3) -> str:
        """
        Get relevant memory context for a query.

        Args:
            query: User query
            limit: Max memories to include

        Returns:
            Formatted context string
        """
        memories = self.recall(query=query, limit=limit)

        if not memories:
            return ""

        context_parts = ["Relevant memories:"]
        for mem in memories:
            context_parts.append(f"- [{mem['memory_type']}] {mem['content']}")

        return "\n".join(context_parts)

    def store_financial_context(
        self,
        symbol: str,
        context_type: str,
        data: Dict[str, Any]
    ) -> int:
        """
        Store financial-specific context.

        Args:
            symbol: Financial symbol
            context_type: Type (analysis, alert, decision)
            data: Context data

        Returns:
            Memory ID
        """
        content = f"{symbol}: {json.dumps(data)}"
        metadata = {
            "symbol": symbol,
            "context_type": context_type,
            "timestamp": datetime.utcnow().isoformat()
        }

        return self.store(
            content=content,
            memory_type=context_type,
            metadata=metadata
        )

    def get_symbol_context(self, symbol: str, limit: int = 5) -> List[Dict]:
        """Get all memories related to a symbol."""
        return self.recall(query=symbol, limit=limit)

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert to Agno agent config.

        Returns config for Agent initialization.
        """
        return {
            "enable_agentic_memory": True,
            "enable_user_memories": True,
            "add_memories_to_context": True
        }

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "AgenticMemoryModule":
        """Create from configuration."""
        return cls(
            user_id=config.get("user_id", "default"),
            db_path=config.get("db_path"),
            enable_auto_store=config.get("enable_auto_store", True),
            max_memories=config.get("max_memories", 1000)
        )
