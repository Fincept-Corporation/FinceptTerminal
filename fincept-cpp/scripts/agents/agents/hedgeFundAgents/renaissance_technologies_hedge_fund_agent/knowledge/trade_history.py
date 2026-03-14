"""
Trade History Knowledge

Knowledge base for historical trade data including:
- Trade outcomes
- Execution quality
- Lessons learned
- Pattern recognition from past trades
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
import uuid

from .base import (
    RenTechKnowledge,
    KnowledgeDocument,
    KnowledgeCategory,
    get_knowledge_base,
)


class TradeHistoryKnowledge:
    """
    Trade History Knowledge Manager.

    Provides structured access to historical trade information,
    outcomes, and lessons learned.
    """

    def __init__(self, knowledge_base: Optional[RenTechKnowledge] = None):
        self.kb = knowledge_base or get_knowledge_base()
        self.category = KnowledgeCategory.TRADE_HISTORY

    def add_trade_record(
        self,
        trade_id: str,
        ticker: str,
        direction: str,
        signal_type: str,
        entry_date: str,
        exit_date: Optional[str],
        entry_price: float,
        exit_price: Optional[float],
        quantity: int,
        pnl_bps: Optional[float],
        execution_metrics: Dict[str, Any],
        signal_metrics: Dict[str, Any],
        lessons: Optional[List[str]] = None,
    ) -> str:
        """
        Add a completed trade record to knowledge base.

        Args:
            trade_id: Unique trade identifier
            ticker: Stock ticker
            direction: long/short
            signal_type: Type of signal that generated trade
            entry_date: Entry date
            exit_date: Exit date (None if still open)
            entry_price: Entry price
            exit_price: Exit price (None if still open)
            quantity: Number of shares
            pnl_bps: P&L in basis points
            execution_metrics: Execution quality metrics
            signal_metrics: Signal performance metrics
            lessons: Lessons learned

        Returns:
            Document ID
        """
        status = "closed" if exit_date else "open"
        pnl_str = f"{pnl_bps:+.1f} bps" if pnl_bps else "N/A"
        outcome = "profitable" if (pnl_bps or 0) > 0 else "loss" if (pnl_bps or 0) < 0 else "break_even"

        content = f"""
# Trade Record: {trade_id}

## Summary
- **Ticker**: {ticker}
- **Direction**: {direction.upper()}
- **Signal Type**: {signal_type}
- **Status**: {status.upper()}
- **Outcome**: {outcome.upper()}

## Timing
- Entry Date: {entry_date}
- Exit Date: {exit_date or 'Still Open'}
- Holding Period: {self._calc_holding_period(entry_date, exit_date)}

## Prices
- Entry Price: ${entry_price:.2f}
- Exit Price: ${exit_price:.2f if exit_price else 'N/A'}
- Quantity: {quantity:,} shares
- Notional: ${quantity * entry_price:,.0f}

## Performance
- P&L: {pnl_str}
- Return: {(pnl_bps or 0) / 100:.2f}%

## Signal Metrics
- Signal Strength: {signal_metrics.get('strength', 'N/A')}
- Confidence: {signal_metrics.get('confidence', 'N/A')}
- P-value: {signal_metrics.get('p_value', 'N/A')}
- Expected Return: {signal_metrics.get('expected_return_bps', 'N/A')} bps
- Actual vs Expected: {((pnl_bps or 0) - signal_metrics.get('expected_return_bps', 0)):+.1f} bps

## Execution Metrics
- vs Arrival: {execution_metrics.get('vs_arrival_bps', 'N/A')} bps
- vs VWAP: {execution_metrics.get('vs_vwap_bps', 'N/A')} bps
- Market Impact: {execution_metrics.get('impact_bps', 'N/A')} bps
- Fill Rate: {execution_metrics.get('fill_rate', 'N/A')}
- Algorithm Used: {execution_metrics.get('algorithm', 'N/A')}

## Lessons Learned
{chr(10).join([f"- {l}" for l in (lessons or ['No lessons documented.'])])}

## Tags
- Outcome: {outcome}
- Direction: {direction}
- Signal: {signal_type}
"""

        doc = KnowledgeDocument(
            id=f"trade_{trade_id}",
            category=self.category,
            title=f"Trade: {ticker} {direction} ({outcome})",
            content=content,
            metadata={
                "trade_id": trade_id,
                "ticker": ticker,
                "direction": direction,
                "signal_type": signal_type,
                "outcome": outcome,
                "pnl_bps": pnl_bps,
                "entry_date": entry_date,
                "exit_date": exit_date,
                "execution_metrics": execution_metrics,
                "signal_metrics": signal_metrics,
                "doc_type": "trade_record",
            },
            tags=[ticker, direction, signal_type, outcome, "trade"],
            importance=0.6 if outcome == "loss" else 0.5,  # Losses are more important to remember
        )

        return self.kb.add_document(doc)

    def _calc_holding_period(self, entry: str, exit: Optional[str]) -> str:
        """Calculate holding period string"""
        if not exit:
            return "Ongoing"
        try:
            entry_dt = datetime.fromisoformat(entry)
            exit_dt = datetime.fromisoformat(exit)
            days = (exit_dt - entry_dt).days
            return f"{days} days"
        except:
            return "Unknown"

    def add_trade_lesson(
        self,
        lesson_title: str,
        context: str,
        what_happened: str,
        what_we_learned: List[str],
        action_items: List[str],
        related_trades: List[str],
        severity: str = "medium",
    ) -> str:
        """
        Add a trade lesson learned document.

        Args:
            lesson_title: Title of the lesson
            context: Context/situation
            what_happened: What occurred
            what_we_learned: Key learnings
            action_items: Actions to take
            related_trades: Related trade IDs
            severity: Impact severity (low, medium, high, critical)

        Returns:
            Document ID
        """
        content = f"""
# Trade Lesson: {lesson_title}

## Severity: {severity.upper()}
## Date: {datetime.utcnow().strftime('%Y-%m-%d')}

## Context
{context}

## What Happened
{what_happened}

## What We Learned
{chr(10).join([f"- {l}" for l in what_we_learned])}

## Action Items
{chr(10).join([f"- [ ] {a}" for a in action_items])}

## Related Trades
{chr(10).join([f"- {t}" for t in related_trades])}

## Prevention
How to prevent this in the future:
{chr(10).join([f"- {l}" for l in what_we_learned[:3]])}

## Review
- Reviewed by: Pending
- Review date: Pending
- Status: Active lesson
"""

        doc = KnowledgeDocument(
            id=f"lesson_{lesson_title.replace(' ', '_')[:30]}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Lesson: {lesson_title}",
            content=content,
            metadata={
                "lesson_title": lesson_title,
                "severity": severity,
                "related_trades": related_trades,
                "action_items": action_items,
                "doc_type": "trade_lesson",
            },
            tags=["lesson", severity, "learning"],
            importance=0.95 if severity == "critical" else 0.8 if severity == "high" else 0.6,
        )

        return self.kb.add_document(doc)

    def add_pattern_observation(
        self,
        pattern_name: str,
        observation: str,
        supporting_trades: List[str],
        statistics: Dict[str, Any],
        recommendation: str,
    ) -> str:
        """Add a pattern observed across trades"""
        content = f"""
# Trade Pattern: {pattern_name}

## Observation
{observation}

## Supporting Evidence
- Number of trades: {len(supporting_trades)}
- Trade IDs: {', '.join(supporting_trades[:5])}{'...' if len(supporting_trades) > 5 else ''}

## Statistics
- Win Rate: {statistics.get('win_rate', 'N/A')}%
- Average P&L: {statistics.get('avg_pnl_bps', 'N/A')} bps
- Sample Size: {statistics.get('sample_size', 'N/A')}
- Confidence: {statistics.get('confidence', 'N/A')}%

## Recommendation
{recommendation}

## Action
- If pattern is positive: Consider increasing allocation
- If pattern is negative: Investigate root cause
- Document any model changes
"""

        doc = KnowledgeDocument(
            id=f"pattern_{pattern_name.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Pattern: {pattern_name}",
            content=content,
            metadata={
                "pattern_name": pattern_name,
                "supporting_trades": supporting_trades,
                "statistics": statistics,
                "doc_type": "pattern_observation",
            },
            tags=["pattern", "analysis"],
            importance=0.75,
        )

        return self.kb.add_document(doc)

    def add_execution_analysis(
        self,
        period: str,
        summary: str,
        statistics: Dict[str, Any],
        best_practices: List[str],
        areas_for_improvement: List[str],
    ) -> str:
        """Add execution quality analysis"""
        content = f"""
# Execution Analysis: {period}

## Summary
{summary}

## Statistics
- Total Trades: {statistics.get('total_trades', 'N/A')}
- Total Notional: ${statistics.get('total_notional', 0):,.0f}
- Average Cost: {statistics.get('avg_cost_bps', 'N/A')} bps
- vs Arrival (avg): {statistics.get('avg_vs_arrival', 'N/A')} bps
- vs VWAP (avg): {statistics.get('avg_vs_vwap', 'N/A')} bps
- Fill Rate (avg): {statistics.get('avg_fill_rate', 'N/A')}%

## Best Performing
- Best Algorithm: {statistics.get('best_algo', 'N/A')}
- Best Time of Day: {statistics.get('best_time', 'N/A')}
- Best Venue: {statistics.get('best_venue', 'N/A')}

## Worst Performing
- Worst Algorithm: {statistics.get('worst_algo', 'N/A')}
- Worst Time of Day: {statistics.get('worst_time', 'N/A')}

## Best Practices Confirmed
{chr(10).join([f"- {b}" for b in best_practices])}

## Areas for Improvement
{chr(10).join([f"- {a}" for a in areas_for_improvement])}

## Recommendations
Based on this analysis, consider:
1. Increase usage of {statistics.get('best_algo', 'best performing algo')}
2. Prefer trading during {statistics.get('best_time', 'optimal hours')}
3. Reduce usage of {statistics.get('worst_algo', 'underperforming algo')}
"""

        doc = KnowledgeDocument(
            id=f"exec_analysis_{period.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Execution Analysis: {period}",
            content=content,
            metadata={
                "period": period,
                "statistics": statistics,
                "doc_type": "execution_analysis",
            },
            tags=["execution", "analysis", period],
            importance=0.7,
        )

        return self.kb.add_document(doc)

    def search_trades(
        self,
        query: str,
        ticker: Optional[str] = None,
        outcome: Optional[str] = None,
        signal_type: Optional[str] = None,
        limit: int = 10,
    ) -> List[Dict[str, Any]]:
        """Search trade history"""
        results = self.kb.search(
            query=query,
            category=self.category,
            limit=limit * 2,  # Get more, then filter
        )

        if ticker:
            results = [r for r in results if r.get("metadata", {}).get("ticker") == ticker]
        if outcome:
            results = [r for r in results if r.get("metadata", {}).get("outcome") == outcome]
        if signal_type:
            results = [r for r in results if r.get("metadata", {}).get("signal_type") == signal_type]

        return results[:limit]

    def get_similar_trades(
        self,
        ticker: str,
        signal_type: str,
        direction: str,
        limit: int = 5,
    ) -> List[KnowledgeDocument]:
        """Get similar historical trades for reference"""
        query = f"{ticker} {signal_type} {direction} trade"
        results = self.search_trades(
            query=query,
            ticker=ticker,
            signal_type=signal_type,
            limit=limit,
        )
        return [self.kb.get_document(r["id"]) for r in results if r.get("id")]

    def get_lessons_for_situation(
        self,
        situation: str,
        limit: int = 5,
    ) -> List[Dict[str, Any]]:
        """Get relevant lessons for a given situation"""
        return self.kb.search(
            query=f"lesson {situation}",
            category=self.category,
            limit=limit,
        )


# Global instance
_trade_history_knowledge: Optional[TradeHistoryKnowledge] = None


def get_trade_history_knowledge() -> TradeHistoryKnowledge:
    """Get the global trade history knowledge instance"""
    global _trade_history_knowledge
    if _trade_history_knowledge is None:
        _trade_history_knowledge = TradeHistoryKnowledge()
    return _trade_history_knowledge
