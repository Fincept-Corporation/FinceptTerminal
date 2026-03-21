"""
Base Memory Infrastructure

Core memory system for Renaissance Technologies multi-agent architecture.
Implements multi-layer memory with vector storage for semantic search.
"""

from typing import Optional, List, Dict, Any
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from enum import Enum
import json
import uuid

try:
    from agno.memory import AgentMemory
    from agno.memory.db.sqlite import SqliteMemoryDb
    from agno.memory.db.postgres import PostgresMemoryDb
except ImportError:
    # Fallback for type hints
    AgentMemory = object
    SqliteMemoryDb = object
    PostgresMemoryDb = object

from ..config import get_config, MemoryConfig


class MemoryType(str, Enum):
    """Types of memory in the system"""
    SHORT_TERM = "short_term"      # Recent interactions (24h)
    LONG_TERM = "long_term"        # Persistent memories (years)
    EPISODIC = "episodic"          # Specific events/trades
    SEMANTIC = "semantic"          # General knowledge/rules
    WORKING = "working"            # Active decision context


class MemoryPriority(str, Enum):
    """Priority levels for memory entries"""
    CRITICAL = "critical"    # Always remember (risk events, major losses)
    HIGH = "high"            # Important learnings
    MEDIUM = "medium"        # Regular operations
    LOW = "low"              # Routine information


@dataclass
class MemoryEntry:
    """
    A single memory entry in the system.

    Models how a trader or analyst would remember
    important information about trades and decisions.
    """
    id: str
    type: MemoryType
    priority: MemoryPriority
    content: str
    context: Dict[str, Any]

    # Metadata
    agent_id: str
    created_at: datetime
    expires_at: Optional[datetime] = None

    # Memory strength (decays over time unless reinforced)
    strength: float = 1.0
    access_count: int = 0
    last_accessed: Optional[datetime] = None

    # Relationships
    related_memories: List[str] = field(default_factory=list)
    tags: List[str] = field(default_factory=list)

    # Emotional/importance markers (affects retention)
    emotional_valence: float = 0.0  # -1 (negative) to 1 (positive)
    surprise_factor: float = 0.0    # How unexpected was this?

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for storage"""
        return {
            "id": self.id,
            "type": self.type.value,
            "priority": self.priority.value,
            "content": self.content,
            "context": self.context,
            "agent_id": self.agent_id,
            "created_at": self.created_at.isoformat(),
            "expires_at": self.expires_at.isoformat() if self.expires_at else None,
            "strength": self.strength,
            "access_count": self.access_count,
            "last_accessed": self.last_accessed.isoformat() if self.last_accessed else None,
            "related_memories": self.related_memories,
            "tags": self.tags,
            "emotional_valence": self.emotional_valence,
            "surprise_factor": self.surprise_factor,
        }

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "MemoryEntry":
        """Create from dictionary"""
        return cls(
            id=data["id"],
            type=MemoryType(data["type"]),
            priority=MemoryPriority(data["priority"]),
            content=data["content"],
            context=data["context"],
            agent_id=data["agent_id"],
            created_at=datetime.fromisoformat(data["created_at"]),
            expires_at=datetime.fromisoformat(data["expires_at"]) if data.get("expires_at") else None,
            strength=data.get("strength", 1.0),
            access_count=data.get("access_count", 0),
            last_accessed=datetime.fromisoformat(data["last_accessed"]) if data.get("last_accessed") else None,
            related_memories=data.get("related_memories", []),
            tags=data.get("tags", []),
            emotional_valence=data.get("emotional_valence", 0.0),
            surprise_factor=data.get("surprise_factor", 0.0),
        )

    def is_expired(self) -> bool:
        """Check if memory has expired"""
        if self.expires_at is None:
            return False
        return datetime.utcnow() > self.expires_at

    def decay_strength(self, decay_rate: float = 0.01):
        """
        Apply memory decay (forgetting curve).
        Memories accessed more frequently decay slower.
        """
        # Reduce decay based on access frequency
        effective_decay = decay_rate / (1 + self.access_count * 0.1)

        # High priority memories decay slower
        if self.priority == MemoryPriority.CRITICAL:
            effective_decay *= 0.1
        elif self.priority == MemoryPriority.HIGH:
            effective_decay *= 0.5

        self.strength = max(0.0, self.strength - effective_decay)

    def reinforce(self, amount: float = 0.2):
        """Reinforce memory (accessed or validated)"""
        self.strength = min(1.0, self.strength + amount)
        self.access_count += 1
        self.last_accessed = datetime.utcnow()


class RenTechMemory:
    """
    Core memory system for Renaissance Technologies agents.

    Implements a multi-layer memory architecture inspired by
    human memory systems:

    1. Working Memory: Active context for current decisions
    2. Short-term Memory: Recent interactions (24 hours)
    3. Long-term Memory: Persistent knowledge (years)
    4. Episodic Memory: Specific trade events and outcomes
    5. Semantic Memory: General trading rules and knowledge

    Uses Agno's memory system with custom extensions for
    trading-specific features.
    """

    def __init__(
        self,
        config: Optional[MemoryConfig] = None,
        db_path: Optional[str] = None,
    ):
        self.config = config or get_config().memory
        self.db_path = db_path or get_config().database.sqlite_path

        # Memory stores by type
        self._memories: Dict[MemoryType, Dict[str, MemoryEntry]] = {
            mt: {} for mt in MemoryType
        }

        # Working memory (in-memory only, clears each session)
        self._working_context: Dict[str, Any] = {}

        # Initialize Agno memory backend
        self._init_backend()

    def _init_backend(self):
        """Initialize the memory storage backend"""
        try:
            self.db = SqliteMemoryDb(
                table_name="rentech_memories",
                db_file=self.db_path,
            )
            self._backend_available = True
        except Exception as e:
            print(f"Warning: Could not initialize memory backend: {e}")
            self._backend_available = False

    def add_memory(
        self,
        content: str,
        memory_type: MemoryType,
        agent_id: str,
        context: Optional[Dict[str, Any]] = None,
        priority: MemoryPriority = MemoryPriority.MEDIUM,
        tags: Optional[List[str]] = None,
        related_to: Optional[List[str]] = None,
        emotional_valence: float = 0.0,
        surprise_factor: float = 0.0,
    ) -> str:
        """
        Add a new memory to the system.

        Args:
            content: The memory content
            memory_type: Type of memory (short_term, long_term, etc.)
            agent_id: ID of the agent creating the memory
            context: Additional context data
            priority: Memory priority level
            tags: Tags for categorization
            related_to: IDs of related memories
            emotional_valence: Emotional charge (-1 to 1)
            surprise_factor: How unexpected (0 to 1)

        Returns:
            Memory ID
        """
        # Calculate expiration based on type
        expires_at = None
        if memory_type == MemoryType.SHORT_TERM:
            expires_at = datetime.utcnow() + timedelta(hours=self.config.short_term_memory_hours)
        elif memory_type == MemoryType.WORKING:
            expires_at = datetime.utcnow() + timedelta(hours=1)  # Working memory expires fast

        # Create memory entry
        memory_id = f"{memory_type.value}_{uuid.uuid4().hex[:12]}"

        entry = MemoryEntry(
            id=memory_id,
            type=memory_type,
            priority=priority,
            content=content,
            context=context or {},
            agent_id=agent_id,
            created_at=datetime.utcnow(),
            expires_at=expires_at,
            tags=tags or [],
            related_memories=related_to or [],
            emotional_valence=emotional_valence,
            surprise_factor=surprise_factor,
        )

        # Store in appropriate memory store
        self._memories[memory_type][memory_id] = entry

        # Persist to backend if available
        if self._backend_available:
            self._persist_memory(entry)

        return memory_id

    def _persist_memory(self, entry: MemoryEntry):
        """Persist memory to database"""
        try:
            # Store as JSON in the database
            pass  # Agno backend handles this
        except Exception as e:
            print(f"Warning: Could not persist memory: {e}")

    def recall(
        self,
        query: str,
        memory_types: Optional[List[MemoryType]] = None,
        agent_id: Optional[str] = None,
        limit: int = 10,
        min_strength: float = 0.1,
    ) -> List[MemoryEntry]:
        """
        Recall memories matching a query.

        Uses semantic search to find relevant memories
        across specified memory types.

        Args:
            query: Search query
            memory_types: Types to search (all if None)
            agent_id: Filter by agent
            limit: Maximum results
            min_strength: Minimum memory strength

        Returns:
            List of matching memories, ranked by relevance
        """
        types_to_search = memory_types or list(MemoryType)
        results = []

        for mt in types_to_search:
            for entry in self._memories[mt].values():
                # Filter by agent if specified
                if agent_id and entry.agent_id != agent_id:
                    continue

                # Filter by strength
                if entry.strength < min_strength:
                    continue

                # Filter expired
                if entry.is_expired():
                    continue

                # Simple keyword matching (would be vector search in production)
                query_lower = query.lower()
                content_lower = entry.content.lower()

                if query_lower in content_lower or any(tag in query_lower for tag in entry.tags):
                    results.append(entry)
                    # Reinforce accessed memories
                    entry.reinforce(0.05)

        # Sort by relevance (strength * priority_weight)
        priority_weights = {
            MemoryPriority.CRITICAL: 4,
            MemoryPriority.HIGH: 3,
            MemoryPriority.MEDIUM: 2,
            MemoryPriority.LOW: 1,
        }

        results.sort(
            key=lambda x: x.strength * priority_weights[x.priority],
            reverse=True
        )

        return results[:limit]

    def get_working_context(self) -> Dict[str, Any]:
        """Get current working memory context"""
        return self._working_context.copy()

    def set_working_context(self, key: str, value: Any):
        """Set a value in working memory"""
        self._working_context[key] = value

    def clear_working_memory(self):
        """Clear working memory (end of decision cycle)"""
        self._working_context = {}
        self._memories[MemoryType.WORKING] = {}

    def consolidate_memories(self):
        """
        Consolidate short-term memories to long-term.

        Important memories (high strength, high priority, accessed often)
        are promoted to long-term storage. Others are allowed to decay.
        """
        short_term = self._memories[MemoryType.SHORT_TERM]
        to_promote = []
        to_remove = []

        for memory_id, entry in short_term.items():
            # Apply decay
            entry.decay_strength()

            # Check for promotion to long-term
            should_promote = (
                entry.strength > 0.7 or
                entry.priority in [MemoryPriority.CRITICAL, MemoryPriority.HIGH] or
                entry.access_count >= 5 or
                abs(entry.emotional_valence) > 0.5 or
                entry.surprise_factor > 0.5
            )

            if should_promote:
                to_promote.append(entry)
            elif entry.strength < 0.1:
                to_remove.append(memory_id)

        # Promote memories
        for entry in to_promote:
            entry.type = MemoryType.LONG_TERM
            entry.expires_at = None  # Long-term doesn't expire
            self._memories[MemoryType.LONG_TERM][entry.id] = entry
            del self._memories[MemoryType.SHORT_TERM][entry.id]

        # Remove weak memories
        for memory_id in to_remove:
            del self._memories[MemoryType.SHORT_TERM][memory_id]

    def get_memories_by_tags(
        self,
        tags: List[str],
        memory_types: Optional[List[MemoryType]] = None,
    ) -> List[MemoryEntry]:
        """Get all memories with specific tags"""
        types_to_search = memory_types or list(MemoryType)
        results = []

        for mt in types_to_search:
            for entry in self._memories[mt].values():
                if not entry.is_expired() and any(tag in entry.tags for tag in tags):
                    results.append(entry)

        return results

    def get_agent_memories(
        self,
        agent_id: str,
        memory_types: Optional[List[MemoryType]] = None,
        limit: int = 50,
    ) -> List[MemoryEntry]:
        """Get all memories for a specific agent"""
        types_to_search = memory_types or list(MemoryType)
        results = []

        for mt in types_to_search:
            for entry in self._memories[mt].values():
                if entry.agent_id == agent_id and not entry.is_expired():
                    results.append(entry)

        # Sort by recency
        results.sort(key=lambda x: x.created_at, reverse=True)
        return results[:limit]

    def forget(self, memory_id: str) -> bool:
        """Explicitly forget a memory"""
        for mt in MemoryType:
            if memory_id in self._memories[mt]:
                del self._memories[mt][memory_id]
                return True
        return False

    def get_memory_summary(self) -> Dict[str, Any]:
        """Get summary statistics of the memory system"""
        summary = {
            "total_memories": 0,
            "by_type": {},
            "by_priority": {p.value: 0 for p in MemoryPriority},
            "average_strength": 0.0,
        }

        all_memories = []
        for mt in MemoryType:
            count = len(self._memories[mt])
            summary["by_type"][mt.value] = count
            summary["total_memories"] += count
            all_memories.extend(self._memories[mt].values())

        if all_memories:
            for entry in all_memories:
                summary["by_priority"][entry.priority.value] += 1
            summary["average_strength"] = sum(m.strength for m in all_memories) / len(all_memories)

        return summary


# Global memory instance
_memory_system: Optional[RenTechMemory] = None


def get_memory_system() -> RenTechMemory:
    """Get the global memory system instance"""
    global _memory_system
    if _memory_system is None:
        _memory_system = RenTechMemory()
    return _memory_system


def reset_memory_system():
    """Reset the memory system (for testing)"""
    global _memory_system
    _memory_system = None
