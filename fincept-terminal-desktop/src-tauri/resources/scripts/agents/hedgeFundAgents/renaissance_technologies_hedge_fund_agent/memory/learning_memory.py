"""
Learning Memory

Memory system for organizational learning:
- Lessons learned from trades
- Strategy insights
- Market regime observations
- Best practices and anti-patterns
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
from dataclasses import dataclass, field
from enum import Enum
import uuid

from .base import (
    RenTechMemory,
    MemoryType,
    MemoryEntry,
    MemoryPriority,
    get_memory_system,
)


class LessonCategory(str, Enum):
    """Categories of lessons learned"""
    SIGNAL_QUALITY = "signal_quality"
    EXECUTION = "execution"
    RISK_MANAGEMENT = "risk_management"
    MARKET_REGIME = "market_regime"
    POSITION_SIZING = "position_sizing"
    TIMING = "timing"
    DATA_QUALITY = "data_quality"
    MODEL_BEHAVIOR = "model_behavior"
    OPERATIONAL = "operational"


class InsightType(str, Enum):
    """Types of insights"""
    PATTERN = "pattern"           # Recurring pattern observed
    ANOMALY = "anomaly"           # Unexpected behavior
    CORRELATION = "correlation"   # New correlation discovered
    ANTI_PATTERN = "anti_pattern" # What NOT to do
    BEST_PRACTICE = "best_practice"  # What TO do
    HYPOTHESIS = "hypothesis"     # Unconfirmed theory


@dataclass
class Lesson:
    """Structured lesson learned"""
    lesson_id: str
    category: LessonCategory
    title: str
    description: str

    # Evidence
    supporting_events: List[str]  # Trade IDs, decision IDs that support this
    evidence_strength: float  # 0-1, how well supported

    # Impact
    estimated_impact_bps: Optional[float]  # Estimated P&L impact
    frequency: str  # how often this applies (daily, weekly, rare)

    # Actions
    action_items: List[str]
    status: str = "active"  # active, implemented, deprecated


@dataclass
class MarketInsight:
    """Insight about market behavior"""
    insight_id: str
    insight_type: InsightType
    market_condition: str

    observation: str
    statistical_evidence: Dict[str, Any]

    trading_implications: List[str]
    valid_until: Optional[str] = None  # Some insights expire


class LearningMemory:
    """
    Learning-specific memory manager.

    Captures organizational learning to improve over time:

    1. Lessons Learned: What we've learned from experience
    2. Market Insights: Observations about market behavior
    3. Strategy Insights: What works and what doesn't
    4. Best Practices: Accumulated wisdom
    5. Anti-patterns: What to avoid

    This is the "institutional memory" of the fund.
    """

    def __init__(self, memory_system: Optional[RenTechMemory] = None):
        self.memory = memory_system or get_memory_system()

    def add_lesson(
        self,
        lesson: Lesson,
        learned_by: str,
        related_memories: Optional[List[str]] = None,
    ) -> str:
        """
        Add a lesson learned to memory.

        Args:
            lesson: The lesson to remember
            learned_by: Agent who identified the lesson
            related_memories: IDs of related memories

        Returns:
            Memory ID
        """
        # High-impact lessons get higher priority
        if lesson.estimated_impact_bps:
            if abs(lesson.estimated_impact_bps) > 100:
                priority = MemoryPriority.CRITICAL
            elif abs(lesson.estimated_impact_bps) > 50:
                priority = MemoryPriority.HIGH
            else:
                priority = MemoryPriority.MEDIUM
        else:
            priority = MemoryPriority.MEDIUM

        content = f"""
LESSON LEARNED: {lesson.title}
Category: {lesson.category.value}
Status: {lesson.status.upper()}

Description:
{lesson.description}

Evidence:
- Supporting Events: {len(lesson.supporting_events)} trades/decisions
- Evidence Strength: {lesson.evidence_strength:.0%}
- Estimated Impact: {lesson.estimated_impact_bps or 'Unknown'} bps per occurrence
- Frequency: {lesson.frequency}

Action Items:
{chr(10).join([f"- {a}" for a in lesson.action_items])}

Supporting Events:
{chr(10).join([f"- {e}" for e in lesson.supporting_events[:5]])}
{"..." if len(lesson.supporting_events) > 5 else ""}
""".strip()

        context = {
            "lesson": {
                "id": lesson.lesson_id,
                "category": lesson.category.value,
                "title": lesson.title,
                "evidence_strength": lesson.evidence_strength,
                "estimated_impact_bps": lesson.estimated_impact_bps,
                "action_items": lesson.action_items,
                "status": lesson.status,
            },
            "supporting_events": lesson.supporting_events,
        }

        tags = [
            "lesson",
            lesson.category.value,
            lesson.status,
            f"freq_{lesson.frequency}",
        ]

        # Lessons go to long-term/semantic memory
        return self.memory.add_memory(
            content=content,
            memory_type=MemoryType.SEMANTIC,
            agent_id=learned_by,
            context=context,
            priority=priority,
            tags=tags,
            related_to=related_memories,
        )

    def add_market_insight(
        self,
        insight: MarketInsight,
        discovered_by: str,
    ) -> str:
        """
        Add a market insight to memory.

        Market insights are observations about market behavior
        that can inform trading decisions.
        """
        # Determine priority based on insight type
        if insight.insight_type == InsightType.ANOMALY:
            priority = MemoryPriority.HIGH  # Anomalies are important
        elif insight.insight_type == InsightType.ANTI_PATTERN:
            priority = MemoryPriority.HIGH  # Anti-patterns prevent losses
        elif insight.insight_type == InsightType.BEST_PRACTICE:
            priority = MemoryPriority.HIGH
        else:
            priority = MemoryPriority.MEDIUM

        content = f"""
MARKET INSIGHT: {insight.insight_type.value.upper()}
Condition: {insight.market_condition}
Valid Until: {insight.valid_until or 'Indefinite'}

Observation:
{insight.observation}

Statistical Evidence:
{chr(10).join([f"- {k}: {v}" for k, v in insight.statistical_evidence.items()])}

Trading Implications:
{chr(10).join([f"- {i}" for i in insight.trading_implications])}
""".strip()

        context = {
            "insight": {
                "id": insight.insight_id,
                "type": insight.insight_type.value,
                "condition": insight.market_condition,
                "evidence": insight.statistical_evidence,
                "implications": insight.trading_implications,
            }
        }

        tags = [
            "insight",
            insight.insight_type.value,
            insight.market_condition,
        ]

        return self.memory.add_memory(
            content=content,
            memory_type=MemoryType.SEMANTIC,
            agent_id=discovered_by,
            context=context,
            priority=priority,
            tags=tags,
        )

    def add_strategy_insight(
        self,
        strategy_name: str,
        insight: str,
        performance_data: Dict[str, Any],
        recommendation: str,
        discovered_by: str,
    ) -> str:
        """
        Add an insight about strategy performance.
        """
        content = f"""
STRATEGY INSIGHT: {strategy_name}

Observation:
{insight}

Performance Data:
{chr(10).join([f"- {k}: {v}" for k, v in performance_data.items()])}

Recommendation:
{recommendation}
""".strip()

        context = {
            "strategy_insight": {
                "strategy": strategy_name,
                "insight": insight,
                "performance": performance_data,
                "recommendation": recommendation,
            }
        }

        tags = ["strategy_insight", strategy_name.lower().replace(" ", "_")]

        return self.memory.add_memory(
            content=content,
            memory_type=MemoryType.SEMANTIC,
            agent_id=discovered_by,
            context=context,
            priority=MemoryPriority.HIGH,
            tags=tags,
        )

    def add_best_practice(
        self,
        title: str,
        description: str,
        when_to_use: str,
        examples: List[str],
        discovered_by: str,
    ) -> str:
        """
        Add a best practice to organizational memory.
        """
        content = f"""
BEST PRACTICE: {title}

Description:
{description}

When to Use:
{when_to_use}

Examples:
{chr(10).join([f"- {e}" for e in examples])}
""".strip()

        context = {
            "best_practice": {
                "title": title,
                "description": description,
                "when_to_use": when_to_use,
                "examples": examples,
            }
        }

        return self.memory.add_memory(
            content=content,
            memory_type=MemoryType.SEMANTIC,
            agent_id=discovered_by,
            context=context,
            priority=MemoryPriority.HIGH,
            tags=["best_practice"],
        )

    def add_anti_pattern(
        self,
        title: str,
        description: str,
        why_bad: str,
        what_to_do_instead: str,
        examples: List[str],
        discovered_by: str,
    ) -> str:
        """
        Add an anti-pattern (what NOT to do) to memory.

        Anti-patterns are critical for avoiding repeated mistakes.
        """
        content = f"""
ANTI-PATTERN: {title}

Description (What to Avoid):
{description}

Why This Is Bad:
{why_bad}

What To Do Instead:
{what_to_do_instead}

Examples of This Anti-Pattern:
{chr(10).join([f"- {e}" for e in examples])}

REMEMBER: Avoiding this saves money!
""".strip()

        context = {
            "anti_pattern": {
                "title": title,
                "description": description,
                "why_bad": why_bad,
                "alternative": what_to_do_instead,
                "examples": examples,
            }
        }

        return self.memory.add_memory(
            content=content,
            memory_type=MemoryType.SEMANTIC,
            agent_id=discovered_by,
            context=context,
            priority=MemoryPriority.CRITICAL,  # Anti-patterns are critical
            tags=["anti_pattern", "avoid"],
            emotional_valence=-0.5,  # Negative association helps remember
        )

    def recall_lessons_for_situation(
        self,
        situation: str,
        category: Optional[LessonCategory] = None,
        limit: int = 5,
    ) -> List[MemoryEntry]:
        """
        Recall relevant lessons for a given situation.
        """
        query = f"lesson {situation}"
        if category:
            query += f" {category.value}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.SEMANTIC, MemoryType.LONG_TERM],
            limit=limit * 2,
        )

        # Filter to actual lessons
        lessons = []
        for m in memories:
            if "lesson" in m.tags or m.context.get("lesson"):
                lessons.append(m)

        return lessons[:limit]

    def recall_insights_for_market_condition(
        self,
        market_condition: str,
        limit: int = 5,
    ) -> List[MemoryEntry]:
        """
        Recall market insights relevant to current conditions.
        """
        memories = self.memory.recall(
            query=f"insight {market_condition}",
            memory_types=[MemoryType.SEMANTIC, MemoryType.LONG_TERM],
            limit=limit * 2,
        )

        # Filter to actual insights
        insights = []
        for m in memories:
            if "insight" in m.tags or m.context.get("insight"):
                insights.append(m)

        return insights[:limit]

    def recall_anti_patterns(
        self,
        situation: Optional[str] = None,
        limit: int = 5,
    ) -> List[MemoryEntry]:
        """
        Recall anti-patterns to avoid.

        Should be called before major decisions to remember
        what NOT to do.
        """
        query = "anti_pattern avoid"
        if situation:
            query += f" {situation}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.SEMANTIC, MemoryType.LONG_TERM],
            limit=limit,
        )

        # Filter to actual anti-patterns
        anti_patterns = []
        for m in memories:
            if "anti_pattern" in m.tags or m.context.get("anti_pattern"):
                anti_patterns.append(m)

        return anti_patterns[:limit]

    def recall_best_practices(
        self,
        situation: Optional[str] = None,
        limit: int = 5,
    ) -> List[MemoryEntry]:
        """
        Recall best practices for a situation.
        """
        query = "best_practice"
        if situation:
            query += f" {situation}"

        memories = self.memory.recall(
            query=query,
            memory_types=[MemoryType.SEMANTIC, MemoryType.LONG_TERM],
            limit=limit,
        )

        # Filter to actual best practices
        best_practices = []
        for m in memories:
            if "best_practice" in m.tags or m.context.get("best_practice"):
                best_practices.append(m)

        return best_practices[:limit]

    def get_learning_summary(
        self,
        category: Optional[LessonCategory] = None,
    ) -> Dict[str, Any]:
        """
        Get summary of organizational learning.
        """
        all_memories = []

        for mt in [MemoryType.SEMANTIC, MemoryType.LONG_TERM]:
            all_memories.extend(self.memory._memories[mt].values())

        lessons = [m for m in all_memories if "lesson" in m.tags]
        insights = [m for m in all_memories if "insight" in m.tags]
        best_practices = [m for m in all_memories if "best_practice" in m.tags]
        anti_patterns = [m for m in all_memories if "anti_pattern" in m.tags]

        return {
            "total_learnings": len(all_memories),
            "lessons_count": len(lessons),
            "insights_count": len(insights),
            "best_practices_count": len(best_practices),
            "anti_patterns_count": len(anti_patterns),
            "categories": {cat.value: 0 for cat in LessonCategory},  # Would count
        }


# Global learning memory instance
_learning_memory: Optional[LearningMemory] = None


def get_learning_memory() -> LearningMemory:
    """Get the global learning memory instance"""
    global _learning_memory
    if _learning_memory is None:
        _learning_memory = LearningMemory()
    return _learning_memory
