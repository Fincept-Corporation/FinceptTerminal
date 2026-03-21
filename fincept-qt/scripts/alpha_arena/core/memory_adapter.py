"""
Memory Adapter for Alpha Arena

Provides conversation/memory capabilities for trading agents using
the existing session_module.py from finagent_core.
"""

import sys
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
from datetime import datetime

# Add paths for imports
scripts_dir = Path(__file__).parent.parent.parent
if str(scripts_dir) not in sys.path:
    sys.path.insert(0, str(scripts_dir))

finagent_path = scripts_dir / "agents" / "finagent_core"
if str(finagent_path) not in sys.path:
    sys.path.insert(0, str(finagent_path))

from alpha_arena.utils.logging import get_logger

logger = get_logger("memory_adapter")


@dataclass
class TradeMemory:
    """Memory of a trading decision and outcome."""
    timestamp: datetime
    symbol: str
    action: str
    quantity: float
    price: float
    reasoning: str
    outcome: Optional[str] = None  # "profit", "loss", "pending"
    pnl: Optional[float] = None
    lessons_learned: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "timestamp": self.timestamp.isoformat(),
            "symbol": self.symbol,
            "action": self.action,
            "quantity": self.quantity,
            "price": self.price,
            "reasoning": self.reasoning,
            "outcome": self.outcome,
            "pnl": self.pnl,
            "lessons_learned": self.lessons_learned,
        }

    def to_prompt_context(self) -> str:
        """Format for LLM context."""
        lines = [f"Trade on {self.timestamp.strftime('%Y-%m-%d %H:%M')}: {self.action} {self.quantity} {self.symbol} @ ${self.price:,.2f}"]
        lines.append(f"Reasoning: {self.reasoning}")
        if self.outcome:
            lines.append(f"Outcome: {self.outcome} (P&L: ${self.pnl:,.2f})" if self.pnl else f"Outcome: {self.outcome}")
        if self.lessons_learned:
            lines.append(f"Lesson: {self.lessons_learned}")
        return "\n".join(lines)


@dataclass
class MarketInsight:
    """An insight about market conditions."""
    timestamp: datetime
    symbol: str
    insight_type: str  # "pattern", "correlation", "trend", "news"
    content: str
    confidence: float = 0.5
    valid_until: Optional[datetime] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "timestamp": self.timestamp.isoformat(),
            "symbol": self.symbol,
            "insight_type": self.insight_type,
            "content": self.content,
            "confidence": self.confidence,
            "valid_until": self.valid_until.isoformat() if self.valid_until else None,
        }


class AgentMemory:
    """
    Memory system for Alpha Arena agents.

    Stores:
    - Trade history with outcomes
    - Market insights and patterns
    - Conversation context
    - Learning from successes/failures
    """

    def __init__(
        self,
        agent_name: str,
        competition_id: str,
        storage_path: Optional[str] = None,
        max_trade_memory: int = 100,
        max_insights: int = 50,
    ):
        self.agent_name = agent_name
        self.competition_id = competition_id
        self.storage_path = storage_path
        self.max_trade_memory = max_trade_memory
        self.max_insights = max_insights

        self._trade_memories: List[TradeMemory] = []
        self._insights: List[MarketInsight] = []
        self._conversation_context: List[Dict[str, Any]] = []
        self._session_module = None
        self._session_id: Optional[str] = None

        # Initialize session module
        self._init_session()

    def _init_session(self):
        """Initialize session module from finagent_core."""
        try:
            from modules.session_module import SessionModule

            self._session_module = SessionModule(
                storage_path=self.storage_path or f"./alpha_arena_sessions/{self.competition_id}.db",
                auto_save=True,
            )

            # Create or resume session for this agent
            self._session_id = self._session_module.create_session(
                session_id=f"{self.competition_id}_{self.agent_name}",
                metadata={
                    "competition_id": self.competition_id,
                    "agent_name": self.agent_name,
                    "created_at": datetime.now().isoformat(),
                }
            )

            logger.info(f"Session initialized for {self.agent_name}")

        except ImportError:
            logger.warning("SessionModule not available, using in-memory storage only")
        except Exception as e:
            logger.warning(f"Failed to initialize session: {e}")

    def record_trade(
        self,
        symbol: str,
        action: str,
        quantity: float,
        price: float,
        reasoning: str,
    ) -> TradeMemory:
        """
        Record a trading decision.

        Args:
            symbol: Trading symbol
            action: Trade action (buy, sell, hold)
            quantity: Trade quantity
            price: Execution price
            reasoning: Decision reasoning

        Returns:
            TradeMemory object
        """
        memory = TradeMemory(
            timestamp=datetime.now(),
            symbol=symbol,
            action=action,
            quantity=quantity,
            price=price,
            reasoning=reasoning,
        )

        self._trade_memories.append(memory)

        # Trim to max
        if len(self._trade_memories) > self.max_trade_memory:
            self._trade_memories = self._trade_memories[-self.max_trade_memory:]

        # Add to session conversation
        if self._session_module and self._session_id:
            self._session_module.add_message(
                self._session_id,
                role="assistant",
                content=f"TRADE: {action} {quantity} {symbol} @ ${price:.2f}\nReason: {reasoning}",
                trade_data=memory.to_dict(),
            )

        return memory

    def update_trade_outcome(
        self,
        trade_index: int,
        outcome: str,
        pnl: float,
        lessons_learned: Optional[str] = None,
    ):
        """
        Update a trade with its outcome.

        Args:
            trade_index: Index of trade in memory
            outcome: "profit", "loss", or "breakeven"
            pnl: Profit/Loss amount
            lessons_learned: Optional lessons from this trade
        """
        if 0 <= trade_index < len(self._trade_memories):
            memory = self._trade_memories[trade_index]
            memory.outcome = outcome
            memory.pnl = pnl
            memory.lessons_learned = lessons_learned

            # Update session state
            if self._session_module and self._session_id:
                stats = self.get_performance_stats()
                self._session_module.update_session_state(
                    self._session_id,
                    {"performance_stats": stats},
                )

    def add_insight(
        self,
        symbol: str,
        insight_type: str,
        content: str,
        confidence: float = 0.5,
        valid_hours: int = 24,
    ) -> MarketInsight:
        """
        Add a market insight.

        Args:
            symbol: Related symbol
            insight_type: Type of insight
            content: Insight content
            confidence: Confidence level (0-1)
            valid_hours: Hours until insight expires

        Returns:
            MarketInsight object
        """
        from datetime import timedelta

        insight = MarketInsight(
            timestamp=datetime.now(),
            symbol=symbol,
            insight_type=insight_type,
            content=content,
            confidence=confidence,
            valid_until=datetime.now() + timedelta(hours=valid_hours),
        )

        self._insights.append(insight)

        # Trim to max
        if len(self._insights) > self.max_insights:
            self._insights = self._insights[-self.max_insights:]

        return insight

    def get_relevant_context(
        self,
        symbol: str,
        max_trades: int = 5,
        max_insights: int = 3,
    ) -> str:
        """
        Get relevant context for a symbol.

        Args:
            symbol: Trading symbol
            max_trades: Max recent trades to include
            max_insights: Max insights to include

        Returns:
            Formatted context string for LLM
        """
        lines = [f"AGENT MEMORY FOR {symbol}:"]

        # Recent trades for this symbol
        symbol_trades = [t for t in self._trade_memories if t.symbol == symbol]
        recent_trades = symbol_trades[-max_trades:] if symbol_trades else []

        if recent_trades:
            lines.append("\nRecent Trades:")
            for trade in recent_trades:
                lines.append(f"  - {trade.to_prompt_context()}")

        # Relevant insights
        now = datetime.now()
        valid_insights = [
            i for i in self._insights
            if (i.symbol == symbol or i.symbol == "MARKET")
            and (i.valid_until is None or i.valid_until > now)
        ]
        recent_insights = valid_insights[-max_insights:] if valid_insights else []

        if recent_insights:
            lines.append("\nMarket Insights:")
            for insight in recent_insights:
                lines.append(f"  - [{insight.insight_type}] {insight.content} (confidence: {insight.confidence:.0%})")

        # Performance stats
        stats = self.get_performance_stats()
        if stats["total_trades"] > 0:
            lines.append(f"\nPerformance: {stats['win_rate']:.0%} win rate, {stats['total_pnl']:.2f} total P&L")

        return "\n".join(lines)

    def get_performance_stats(self) -> Dict[str, Any]:
        """Get performance statistics from memory."""
        completed_trades = [t for t in self._trade_memories if t.outcome]

        if not completed_trades:
            return {
                "total_trades": len(self._trade_memories),
                "completed_trades": 0,
                "wins": 0,
                "losses": 0,
                "win_rate": 0.0,
                "total_pnl": 0.0,
                "avg_pnl": 0.0,
            }

        wins = sum(1 for t in completed_trades if t.outcome == "profit")
        losses = sum(1 for t in completed_trades if t.outcome == "loss")
        total_pnl = sum(t.pnl or 0 for t in completed_trades)

        return {
            "total_trades": len(self._trade_memories),
            "completed_trades": len(completed_trades),
            "wins": wins,
            "losses": losses,
            "win_rate": wins / len(completed_trades) if completed_trades else 0.0,
            "total_pnl": total_pnl,
            "avg_pnl": total_pnl / len(completed_trades) if completed_trades else 0.0,
        }

    def get_lessons_learned(self, symbol: Optional[str] = None) -> List[str]:
        """Get lessons learned from trades."""
        if symbol:
            trades = [t for t in self._trade_memories if t.symbol == symbol and t.lessons_learned]
        else:
            trades = [t for t in self._trade_memories if t.lessons_learned]

        return [t.lessons_learned for t in trades]

    def add_conversation(self, role: str, content: str):
        """Add a conversation message to memory."""
        self._conversation_context.append({
            "role": role,
            "content": content,
            "timestamp": datetime.now().isoformat(),
        })

        # Also add to session module
        if self._session_module and self._session_id:
            self._session_module.add_message(self._session_id, role, content)

    def get_conversation_history(self, limit: int = 10) -> List[Dict[str, Any]]:
        """Get recent conversation history."""
        if self._session_module and self._session_id:
            return self._session_module.get_messages(self._session_id, limit=limit)
        return self._conversation_context[-limit:]

    def search_memory(self, query: str, limit: int = 5) -> List[Dict[str, Any]]:
        """Search memory for relevant information."""
        results = []
        query_lower = query.lower()

        # Search trade memories
        for i, trade in enumerate(self._trade_memories):
            if query_lower in trade.reasoning.lower() or query_lower in trade.symbol.lower():
                results.append({
                    "type": "trade",
                    "index": i,
                    "data": trade.to_dict(),
                })
                if len(results) >= limit:
                    return results

        # Search insights
        for i, insight in enumerate(self._insights):
            if query_lower in insight.content.lower() or query_lower in insight.symbol.lower():
                results.append({
                    "type": "insight",
                    "index": i,
                    "data": insight.to_dict(),
                })
                if len(results) >= limit:
                    return results

        # Search session history
        if self._session_module and self._session_id:
            session_results = self._session_module.search_history(query, self._session_id, limit=limit - len(results))
            results.extend([{"type": "conversation", **r} for r in session_results])

        return results[:limit]

    def get_summary(self) -> str:
        """Get a summary of agent memory."""
        stats = self.get_performance_stats()
        insights_count = len(self._insights)

        return (
            f"Agent: {self.agent_name} | Competition: {self.competition_id}\n"
            f"Trades: {stats['total_trades']} ({stats['wins']}W/{stats['losses']}L)\n"
            f"Win Rate: {stats['win_rate']:.0%} | Total P&L: ${stats['total_pnl']:.2f}\n"
            f"Active Insights: {insights_count}"
        )

    def clear_memory(self, keep_lessons: bool = True):
        """Clear memory, optionally keeping lessons learned."""
        if keep_lessons:
            lessons = self.get_lessons_learned()
        else:
            lessons = []

        self._trade_memories.clear()
        self._insights.clear()

        # Re-add lessons as insights
        for lesson in lessons:
            self.add_insight("MARKET", "lesson", lesson, confidence=0.8, valid_hours=168)

    def to_dict(self) -> Dict[str, Any]:
        """Export memory to dictionary."""
        return {
            "agent_name": self.agent_name,
            "competition_id": self.competition_id,
            "trades": [t.to_dict() for t in self._trade_memories],
            "insights": [i.to_dict() for i in self._insights],
            "stats": self.get_performance_stats(),
        }


# Factory function
_agent_memories: Dict[str, AgentMemory] = {}


def get_agent_memory(
    agent_name: str,
    competition_id: str,
    storage_path: Optional[str] = None,
) -> AgentMemory:
    """Get or create memory for an agent."""
    key = f"{competition_id}_{agent_name}"

    if key not in _agent_memories:
        _agent_memories[key] = AgentMemory(
            agent_name=agent_name,
            competition_id=competition_id,
            storage_path=storage_path,
        )

    return _agent_memories[key]


def clear_all_memories():
    """Clear all agent memories."""
    _agent_memories.clear()
