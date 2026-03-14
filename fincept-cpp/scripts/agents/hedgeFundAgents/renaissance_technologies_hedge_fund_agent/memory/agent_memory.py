"""
Agent Memory Manager

Unified memory management for individual agents:
- Per-agent memory isolation
- Memory sharing between agents
- Context building for agent decisions
- Memory lifecycle management
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
from dataclasses import dataclass
import uuid

try:
    from agno.memory import AgentMemory
    from agno.memory.db.sqlite import SqliteMemoryDb
except ImportError:
    AgentMemory = object
    SqliteMemoryDb = object

from .base import (
    RenTechMemory,
    MemoryType,
    MemoryEntry,
    MemoryPriority,
    get_memory_system,
)
from .trade_memory import TradeMemory, get_trade_memory
from .decision_memory import DecisionMemory, get_decision_memory
from .learning_memory import LearningMemory, get_learning_memory
from ..config import get_config


@dataclass
class AgentContext:
    """
    Context object provided to an agent for decision making.

    Contains relevant memories, learnings, and current state.
    """
    agent_id: str
    timestamp: str

    # Current working context
    current_task: Optional[str] = None
    current_ticker: Optional[str] = None
    current_signal: Optional[Dict[str, Any]] = None

    # Relevant memories
    recent_trades: List[Dict[str, Any]] = None
    similar_past_decisions: List[Dict[str, Any]] = None
    relevant_lessons: List[Dict[str, Any]] = None
    anti_patterns_to_avoid: List[Dict[str, Any]] = None

    # Performance context
    recent_performance: Optional[Dict[str, Any]] = None
    decision_accuracy: Optional[Dict[str, Any]] = None

    # Market context
    market_insights: List[Dict[str, Any]] = None

    def __post_init__(self):
        self.recent_trades = self.recent_trades or []
        self.similar_past_decisions = self.similar_past_decisions or []
        self.relevant_lessons = self.relevant_lessons or []
        self.anti_patterns_to_avoid = self.anti_patterns_to_avoid or []
        self.market_insights = self.market_insights or []

    def to_prompt_context(self) -> str:
        """Convert to a string context for LLM prompts"""
        sections = []

        if self.current_task:
            sections.append(f"CURRENT TASK: {self.current_task}")

        if self.current_ticker:
            sections.append(f"ANALYZING: {self.current_ticker}")

        if self.recent_trades:
            trades_str = "\n".join([
                f"- {t.get('ticker', 'N/A')}: {t.get('outcome', 'N/A')} ({t.get('pnl_bps', 0):+.1f} bps)"
                for t in self.recent_trades[:5]
            ])
            sections.append(f"RECENT TRADES:\n{trades_str}")

        if self.relevant_lessons:
            lessons_str = "\n".join([
                f"- {l.get('title', 'N/A')}"
                for l in self.relevant_lessons[:3]
            ])
            sections.append(f"RELEVANT LESSONS:\n{lessons_str}")

        if self.anti_patterns_to_avoid:
            patterns_str = "\n".join([
                f"- AVOID: {p.get('title', 'N/A')}"
                for p in self.anti_patterns_to_avoid[:3]
            ])
            sections.append(f"ANTI-PATTERNS TO AVOID:\n{patterns_str}")

        if self.decision_accuracy:
            sections.append(
                f"YOUR RECENT ACCURACY: {self.decision_accuracy.get('accuracy', 0):.1%}"
            )

        return "\n\n".join(sections)


class AgentMemoryManager:
    """
    Unified memory manager for all agents.

    Provides:
    1. Per-agent memory isolation
    2. Cross-agent memory sharing (when appropriate)
    3. Context building for decisions
    4. Memory lifecycle management
    5. Integration with Agno's AgentMemory
    """

    def __init__(
        self,
        memory_system: Optional[RenTechMemory] = None,
        trade_memory: Optional[TradeMemory] = None,
        decision_memory: Optional[DecisionMemory] = None,
        learning_memory: Optional[LearningMemory] = None,
    ):
        self.memory = memory_system or get_memory_system()
        self.trades = trade_memory or get_trade_memory()
        self.decisions = decision_memory or get_decision_memory()
        self.learning = learning_memory or get_learning_memory()

        self.config = get_config()

        # Cache of agent contexts
        self._agent_contexts: Dict[str, AgentContext] = {}

        # Shared memory pool (for team-wide knowledge)
        self._shared_memories: List[MemoryEntry] = []

    def create_agent_memory(self, agent_id: str) -> AgentMemory:
        """
        Create an Agno AgentMemory instance for an agent.

        This integrates with Agno's memory system while using
        our custom storage backend.
        """
        try:
            db = SqliteMemoryDb(
                table_name=f"agent_{agent_id}_memory",
                db_file=self.config.database.sqlite_path,
            )

            return AgentMemory(
                db=db,
                create_user_memories=True,
                create_session_summary=True,
            )
        except Exception as e:
            print(f"Warning: Could not create Agno memory for {agent_id}: {e}")
            return None

    def build_context(
        self,
        agent_id: str,
        task: Optional[str] = None,
        ticker: Optional[str] = None,
        signal: Optional[Dict[str, Any]] = None,
        market_condition: Optional[str] = None,
    ) -> AgentContext:
        """
        Build a context object for an agent's decision making.

        Gathers relevant memories from all memory systems to
        provide comprehensive context.
        """
        context = AgentContext(
            agent_id=agent_id,
            timestamp=datetime.utcnow().isoformat(),
            current_task=task,
            current_ticker=ticker,
            current_signal=signal,
        )

        # Get recent trades
        if ticker:
            similar_trades = self.trades.recall_similar_trades(
                ticker=ticker,
                signal_type=signal.get("type", "") if signal else "",
                direction=signal.get("direction", "") if signal else "",
                market_regime=market_condition,
                limit=5,
            )
            context.recent_trades = [
                m.context.get("trade_outcome", {})
                for m in similar_trades
            ]

            # Get recent losses for this ticker
            losses = self.trades.recall_losses(ticker=ticker, limit=3)
            for loss in losses:
                trade_ctx = loss.context.get("trade_outcome", {})
                if trade_ctx and trade_ctx not in context.recent_trades:
                    context.recent_trades.append(trade_ctx)

        # Get similar past decisions
        if task:
            from .decision_memory import DecisionType
            decision_type = self._infer_decision_type(task)
            if decision_type:
                similar_decisions = self.decisions.recall_similar_decisions(
                    decision_type=decision_type,
                    subject=ticker or task,
                    limit=5,
                )
                context.similar_past_decisions = [
                    m.context.get("decision", {})
                    for m in similar_decisions
                ]

        # Get relevant lessons
        situation = f"{task} {ticker}" if ticker else task
        if situation:
            lessons = self.learning.recall_lessons_for_situation(
                situation=situation,
                limit=3,
            )
            context.relevant_lessons = [
                m.context.get("lesson", {})
                for m in lessons
            ]

        # Get anti-patterns to avoid
        anti_patterns = self.learning.recall_anti_patterns(
            situation=situation,
            limit=3,
        )
        context.anti_patterns_to_avoid = [
            m.context.get("anti_pattern", {})
            for m in anti_patterns
        ]

        # Get market insights
        if market_condition:
            insights = self.learning.recall_insights_for_market_condition(
                market_condition=market_condition,
                limit=3,
            )
            context.market_insights = [
                m.context.get("insight", {})
                for m in insights
            ]

        # Get agent's decision accuracy
        context.decision_accuracy = self.decisions.get_agent_decision_accuracy(
            agent_id=agent_id
        )

        # Cache the context
        self._agent_contexts[agent_id] = context

        return context

    def _infer_decision_type(self, task: str) -> Optional[Any]:
        """Infer decision type from task description"""
        from .decision_memory import DecisionType

        task_lower = task.lower()

        if "approv" in task_lower or "reject" in task_lower or "ic " in task_lower:
            return DecisionType.IC_APPROVAL
        elif "risk" in task_lower:
            return DecisionType.RISK_OVERRIDE
        elif "size" in task_lower or "position" in task_lower:
            return DecisionType.POSITION_SIZING
        elif "execut" in task_lower:
            return DecisionType.EXECUTION_CHOICE
        elif "signal" in task_lower:
            return DecisionType.SIGNAL_GENERATION

        return None

    def share_memory(
        self,
        memory_id: str,
        from_agent: str,
        to_agents: List[str],
    ):
        """
        Share a memory between agents.

        Used when one agent discovers something relevant to others.
        """
        # Find the memory
        memory = None
        for mt in MemoryType:
            if memory_id in self.memory._memories[mt]:
                memory = self.memory._memories[mt][memory_id]
                break

        if not memory:
            return

        # Add to shared pool
        if memory not in self._shared_memories:
            self._shared_memories.append(memory)

        # Create references in target agents' memories
        for agent_id in to_agents:
            self.memory.add_memory(
                content=f"[SHARED from {from_agent}] {memory.content[:200]}...",
                memory_type=MemoryType.SHORT_TERM,
                agent_id=agent_id,
                context={
                    "shared_from": from_agent,
                    "original_memory_id": memory_id,
                },
                priority=memory.priority,
                tags=memory.tags + ["shared"],
                related_to=[memory_id],
            )

    def broadcast_to_team(
        self,
        content: str,
        from_agent: str,
        team: str,
        priority: MemoryPriority = MemoryPriority.MEDIUM,
        tags: Optional[List[str]] = None,
    ) -> str:
        """
        Broadcast information to all agents in a team.

        Used for important announcements that everyone should know.
        """
        memory_id = self.memory.add_memory(
            content=f"[TEAM BROADCAST - {team}]\n{content}",
            memory_type=MemoryType.SHORT_TERM,
            agent_id=from_agent,
            context={
                "broadcast": True,
                "team": team,
                "from_agent": from_agent,
            },
            priority=priority,
            tags=(tags or []) + ["broadcast", team],
        )

        return memory_id

    def remember_interaction(
        self,
        agent_id: str,
        user_message: str,
        agent_response: str,
        outcome: Optional[str] = None,
    ) -> str:
        """
        Remember an agent interaction for learning.
        """
        content = f"""
INTERACTION:
User: {user_message[:500]}...

Agent Response: {agent_response[:500]}...

Outcome: {outcome or 'Not evaluated'}
""".strip()

        return self.memory.add_memory(
            content=content,
            memory_type=MemoryType.EPISODIC,
            agent_id=agent_id,
            context={
                "interaction": {
                    "user_message": user_message,
                    "agent_response": agent_response,
                    "outcome": outcome,
                }
            },
            priority=MemoryPriority.LOW,
            tags=["interaction"],
        )

    def get_working_memory(self, agent_id: str) -> Dict[str, Any]:
        """
        Get the current working memory for an agent.
        """
        agent_working = {}

        for memory_id, entry in self.memory._memories[MemoryType.WORKING].items():
            if entry.agent_id == agent_id:
                agent_working[memory_id] = entry.content

        return agent_working

    def set_working_memory(
        self,
        agent_id: str,
        key: str,
        value: Any,
    ):
        """
        Set a value in an agent's working memory.
        """
        self.memory.add_memory(
            content=f"{key}: {value}",
            memory_type=MemoryType.WORKING,
            agent_id=agent_id,
            context={"key": key, "value": value},
            priority=MemoryPriority.LOW,
            tags=["working_memory", key],
        )

    def end_decision_cycle(self, agent_id: str):
        """
        End a decision cycle for an agent.

        Clears working memory and consolidates important memories.
        """
        # Clear working memory for this agent
        to_remove = []
        for memory_id, entry in self.memory._memories[MemoryType.WORKING].items():
            if entry.agent_id == agent_id:
                to_remove.append(memory_id)

        for memory_id in to_remove:
            del self.memory._memories[MemoryType.WORKING][memory_id]

        # Clear cached context
        if agent_id in self._agent_contexts:
            del self._agent_contexts[agent_id]

    def consolidate_all(self):
        """
        Run memory consolidation for the entire system.

        Should be called periodically (e.g., end of trading day).
        """
        self.memory.consolidate_memories()

    def get_system_stats(self) -> Dict[str, Any]:
        """
        Get overall memory system statistics.
        """
        memory_summary = self.memory.get_memory_summary()
        learning_summary = self.learning.get_learning_summary()

        return {
            "memory": memory_summary,
            "learning": learning_summary,
            "shared_memories": len(self._shared_memories),
            "cached_contexts": len(self._agent_contexts),
        }


# Global agent memory manager instance
_agent_memory_manager: Optional[AgentMemoryManager] = None


def get_agent_memory_manager() -> AgentMemoryManager:
    """Get the global agent memory manager instance"""
    global _agent_memory_manager
    if _agent_memory_manager is None:
        _agent_memory_manager = AgentMemoryManager()
    return _agent_memory_manager


def reset_agent_memory_manager():
    """Reset the agent memory manager (for testing)"""
    global _agent_memory_manager
    _agent_memory_manager = None
