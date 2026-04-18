"""
Trade Memory

Specialized memory system for trade-related information:
- Trade outcomes and P&L
- Execution quality
- Signal performance
- Market conditions during trades
"""

from typing import Optional, List, Dict, Any
from datetime import datetime, timedelta
from dataclasses import dataclass, field
import uuid

from .base import (
    RenTechMemory,
    MemoryType,
    MemoryEntry,
    MemoryPriority,
    get_memory_system,
)


@dataclass
class TradeOutcome:
    """Structured trade outcome for memory"""
    trade_id: str
    ticker: str
    direction: str  # long/short
    signal_type: str

    # Execution
    entry_price: float
    exit_price: Optional[float]
    entry_date: str
    exit_date: Optional[str]
    quantity: int

    # Performance
    pnl_bps: Optional[float]  # P&L in basis points
    pnl_dollars: Optional[float]
    vs_expected_bps: Optional[float]  # Actual vs predicted

    # Execution quality
    slippage_bps: Optional[float]
    market_impact_bps: Optional[float]
    execution_algorithm: str

    # Market conditions
    market_regime: str  # bull/bear/sideways/volatile
    sector_performance: Optional[float]  # Sector return during trade
    vix_level: Optional[float]

    # Metadata
    confidence_at_entry: float
    p_value_at_entry: Optional[float]


class TradeMemory:
    """
    Trade-specific memory manager.

    Remembers:
    - Successful trades (for pattern replication)
    - Failed trades (to avoid similar mistakes)
    - Execution quality (for algorithm selection)
    - Market condition correlations
    """

    def __init__(self, memory_system: Optional[RenTechMemory] = None):
        self.memory = memory_system or get_memory_system()

    def remember_trade(
        self,
        outcome: TradeOutcome,
        agent_id: str,
        lessons_learned: Optional[List[str]] = None,
    ) -> str:
        """
        Remember a completed trade.

        Args:
            outcome: Trade outcome data
            agent_id: Agent who executed/analyzed the trade
            lessons_learned: Key takeaways from this trade

        Returns:
            Memory ID
        """
        # Determine memory priority based on outcome
        pnl = outcome.pnl_bps or 0

        if abs(pnl) > 100:  # Significant trade (>1%)
            priority = MemoryPriority.HIGH
        elif abs(pnl) > 50:  # Notable trade
            priority = MemoryPriority.MEDIUM
        else:
            priority = MemoryPriority.LOW

        # Exceptional outcomes get critical priority
        if abs(pnl) > 500 or abs(outcome.vs_expected_bps or 0) > 200:
            priority = MemoryPriority.CRITICAL

        # Build memory content
        outcome_str = "profitable" if pnl > 0 else "loss" if pnl < 0 else "break-even"
        execution_quality = self._assess_execution(outcome)

        content = f"""
Trade {outcome.trade_id}: {outcome.ticker} {outcome.direction.upper()}
Signal: {outcome.signal_type} | Outcome: {outcome_str.upper()}
P&L: {pnl:+.1f} bps (${outcome.pnl_dollars or 0:,.0f})
Expected: {outcome.confidence_at_entry:.1%} confidence, p-value: {outcome.p_value_at_entry or 'N/A'}
Actual vs Expected: {outcome.vs_expected_bps or 0:+.1f} bps

Execution Quality: {execution_quality}
- Slippage: {outcome.slippage_bps or 0:.1f} bps
- Market Impact: {outcome.market_impact_bps or 0:.1f} bps
- Algorithm: {outcome.execution_algorithm}

Market Conditions:
- Regime: {outcome.market_regime}
- VIX: {outcome.vix_level or 'N/A'}

Lessons:
{chr(10).join([f"- {l}" for l in (lessons_learned or ['None documented'])])}
""".strip()

        # Emotional valence based on outcome
        emotional_valence = 0.0
        if pnl > 0:
            emotional_valence = min(0.8, pnl / 200)  # Cap at 0.8
        elif pnl < 0:
            emotional_valence = max(-0.8, pnl / 200)

        # Surprise factor based on deviation from expected
        vs_expected = abs(outcome.vs_expected_bps or 0)
        surprise_factor = min(1.0, vs_expected / 100)

        # Create context with structured data
        context = {
            "trade_outcome": {
                "trade_id": outcome.trade_id,
                "ticker": outcome.ticker,
                "direction": outcome.direction,
                "signal_type": outcome.signal_type,
                "pnl_bps": outcome.pnl_bps,
                "vs_expected_bps": outcome.vs_expected_bps,
                "execution_quality": execution_quality,
                "market_regime": outcome.market_regime,
            },
            "lessons": lessons_learned or [],
        }

        # Tags for easy retrieval
        tags = [
            outcome.ticker,
            outcome.direction,
            outcome.signal_type,
            outcome_str,
            outcome.market_regime,
            f"algo_{outcome.execution_algorithm}",
        ]

        # Determine memory type
        # Exceptional trades go to long-term immediately
        if priority == MemoryPriority.CRITICAL:
            memory_type = MemoryType.EPISODIC  # Specific events
        else:
            memory_type = MemoryType.SHORT_TERM

        return self.memory.add_memory(
            content=content,
            memory_type=memory_type,
            agent_id=agent_id,
            context=context,
            priority=priority,
            tags=tags,
            emotional_valence=emotional_valence,
            surprise_factor=surprise_factor,
        )

    def _assess_execution(self, outcome: TradeOutcome) -> str:
        """Assess execution quality"""
        total_cost = (outcome.slippage_bps or 0) + (outcome.market_impact_bps or 0)

        if total_cost <= 2:
            return "excellent"
        elif total_cost <= 5:
            return "good"
        elif total_cost <= 10:
            return "acceptable"
        elif total_cost <= 20:
            return "poor"
        else:
            return "very_poor"

    def recall_similar_trades(
        self,
        ticker: str,
        signal_type: str,
        direction: str,
        market_regime: Optional[str] = None,
        limit: int = 5,
    ) -> List[MemoryEntry]:
        """
        Recall similar historical trades.

        Used before entering a new position to learn from
        similar past situations.
        """
        # Build search query
        query = f"{ticker} {signal_type} {direction}"
        if market_regime:
            query += f" {market_regime}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.SHORT_TERM, MemoryType.LONG_TERM, MemoryType.EPISODIC],
            limit=limit * 2,  # Get extra, filter down
        )

        # Filter to relevant trades
        relevant = []
        for m in memories:
            ctx = m.context.get("trade_outcome", {})
            if ctx.get("signal_type") == signal_type:
                relevant.append(m)

        return relevant[:limit]

    def recall_losses(
        self,
        ticker: Optional[str] = None,
        signal_type: Optional[str] = None,
        min_loss_bps: float = 50,
        limit: int = 10,
    ) -> List[MemoryEntry]:
        """
        Recall losing trades for risk awareness.

        Helps avoid repeating mistakes.
        """
        query = "loss"
        if ticker:
            query += f" {ticker}"
        if signal_type:
            query += f" {signal_type}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.SHORT_TERM, MemoryType.LONG_TERM, MemoryType.EPISODIC],
            limit=limit * 2,
        )

        # Filter to actual losses above threshold
        losses = []
        for m in memories:
            ctx = m.context.get("trade_outcome", {})
            pnl = ctx.get("pnl_bps", 0)
            if pnl is not None and pnl < -min_loss_bps:
                losses.append(m)

        # Sort by magnitude of loss
        losses.sort(
            key=lambda x: x.context.get("trade_outcome", {}).get("pnl_bps", 0)
        )

        return losses[:limit]

    def recall_by_market_regime(
        self,
        regime: str,
        limit: int = 10,
    ) -> List[MemoryEntry]:
        """Recall trades from a specific market regime"""
        return self.memory.get_memories_by_tags(
            tags=[regime],
            memory_types=[MemoryType.SHORT_TERM, MemoryType.LONG_TERM, MemoryType.EPISODIC],
        )[:limit]

    def get_signal_performance_summary(
        self,
        signal_type: str,
    ) -> Dict[str, Any]:
        """
        Get performance summary for a signal type.

        Used to assess signal quality over time.
        """
        memories = self.memory.recall(
            query=signal_type,
            memory_types=[MemoryType.SHORT_TERM, MemoryType.LONG_TERM, MemoryType.EPISODIC],
            limit=100,
        )

        # Filter to matching signal type
        trades = []
        for m in memories:
            ctx = m.context.get("trade_outcome", {})
            if ctx.get("signal_type") == signal_type:
                trades.append(ctx)

        if not trades:
            return {"signal_type": signal_type, "sample_size": 0}

        # Calculate statistics
        pnls = [t.get("pnl_bps", 0) or 0 for t in trades]
        wins = [p for p in pnls if p > 0]
        losses = [p for p in pnls if p < 0]

        return {
            "signal_type": signal_type,
            "sample_size": len(trades),
            "win_rate": len(wins) / len(trades) if trades else 0,
            "avg_pnl_bps": sum(pnls) / len(pnls) if pnls else 0,
            "avg_win_bps": sum(wins) / len(wins) if wins else 0,
            "avg_loss_bps": sum(losses) / len(losses) if losses else 0,
            "total_pnl_bps": sum(pnls),
        }

    def get_execution_performance(
        self,
        algorithm: str,
    ) -> Dict[str, Any]:
        """
        Get execution performance for an algorithm.

        Helps choose the best execution algorithm.
        """
        memories = self.memory.recall(
            query=f"algo_{algorithm}",
            memory_types=[MemoryType.SHORT_TERM, MemoryType.LONG_TERM, MemoryType.EPISODIC],
            limit=100,
        )

        slippages = []
        impacts = []

        for m in memories:
            ctx = m.context.get("trade_outcome", {})
            if ctx.get("execution_algorithm") == algorithm:
                # Would need to parse from content in real implementation
                pass

        return {
            "algorithm": algorithm,
            "sample_size": len(memories),
            "avg_slippage_bps": sum(slippages) / len(slippages) if slippages else None,
            "avg_impact_bps": sum(impacts) / len(impacts) if impacts else None,
        }


# Global trade memory instance
_trade_memory: Optional[TradeMemory] = None


def get_trade_memory() -> TradeMemory:
    """Get the global trade memory instance"""
    global _trade_memory
    if _trade_memory is None:
        _trade_memory = TradeMemory()
    return _trade_memory
