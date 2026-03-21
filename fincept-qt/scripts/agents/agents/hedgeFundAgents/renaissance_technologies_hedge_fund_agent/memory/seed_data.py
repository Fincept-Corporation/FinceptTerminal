"""
Memory Seed Data

Initial memories to bootstrap the Renaissance Technologies
multi-agent system with institutional knowledge.
"""

from datetime import datetime, timedelta
from typing import List

from .base import (
    RenTechMemory,
    MemoryType,
    MemoryPriority,
    get_memory_system,
)
from .trade_memory import TradeMemory, TradeOutcome, get_trade_memory
from .decision_memory import (
    DecisionMemory,
    Decision,
    DecisionType,
    DecisionOutcome,
    get_decision_memory,
)
from .learning_memory import (
    LearningMemory,
    Lesson,
    LessonCategory,
    MarketInsight,
    InsightType,
    get_learning_memory,
)


def seed_lessons(learning: LearningMemory):
    """Seed organizational lessons learned"""

    # Lesson 1: Mean reversion timing
    lesson1 = Lesson(
        lesson_id="lesson_001",
        category=LessonCategory.TIMING,
        title="Mean Reversion Works Best in Low-Vol Regimes",
        description="""
Mean reversion signals have significantly higher win rates during
low volatility periods (VIX < 18). During high volatility, mean
reversion signals often get stopped out before reverting.

The p-value threshold should be adjusted based on regime:
- Low vol (VIX < 18): p < 0.02 is sufficient
- Medium vol (VIX 18-25): p < 0.01 required
- High vol (VIX > 25): p < 0.005 or skip entirely
""".strip(),
        supporting_events=[
            "trade_20240115_AAPL_MR_001",
            "trade_20240118_MSFT_MR_002",
            "trade_20240122_GOOGL_MR_003",
        ],
        evidence_strength=0.85,
        estimated_impact_bps=25,
        frequency="daily",
        action_items=[
            "Adjust mean reversion model p-value threshold by VIX regime",
            "Add regime filter to signal generation pipeline",
            "Backtest regime-adjusted thresholds",
        ],
        status="active",
    )
    learning.add_lesson(lesson1, learned_by="research_team")

    # Lesson 2: Execution timing
    lesson2 = Lesson(
        lesson_id="lesson_002",
        category=LessonCategory.EXECUTION,
        title="Avoid Trading First 15 Minutes",
        description="""
Execution costs are 3-4x higher in the first 15 minutes of market open.
Spreads are wider, order book is thin, and retail flow creates noise.

Unless we have urgent alpha decay concerns (urgency > 0.9), all
orders should use a 15-minute delay from market open.

Exception: Earnings reactions where alpha decays within minutes.
""".strip(),
        supporting_events=[
            "exec_analysis_Q4_2024",
            "trade_20240108_NVDA_001",
            "trade_20240109_AMD_002",
        ],
        evidence_strength=0.92,
        estimated_impact_bps=8,
        frequency="daily",
        action_items=[
            "Add 15-minute delay to default execution profile",
            "Create 'urgent' execution profile for exceptions",
            "Monitor first-15-min trades monthly",
        ],
        status="implemented",
    )
    learning.add_lesson(lesson2, learned_by="trading_team")

    # Lesson 3: Position sizing
    lesson3 = Lesson(
        lesson_id="lesson_003",
        category=LessonCategory.POSITION_SIZING,
        title="Kelly Criterion Overestimates in Correlated Positions",
        description="""
When multiple signals fire on correlated assets (e.g., AAPL/MSFT,
or multiple tech stocks), Kelly criterion overestimates appropriate
sizing because it doesn't account for correlation.

Implemented correlation penalty: If new position has >0.7 correlation
with existing book, reduce Kelly size by (1 - correlation^2).
""".strip(),
        supporting_events=[
            "drawdown_event_20240215",
            "risk_report_20240216",
        ],
        evidence_strength=0.88,
        estimated_impact_bps=150,
        frequency="weekly",
        action_items=[
            "Implement correlation penalty in position sizer",
            "Add correlation check to pre-trade risk",
            "Review sector concentration weekly",
        ],
        status="implemented",
    )
    learning.add_lesson(lesson3, learned_by="risk_team")

    # Lesson 4: Data quality
    lesson4 = Lesson(
        lesson_id="lesson_004",
        category=LessonCategory.DATA_QUALITY,
        title="Corporate Action Data Leads to False Signals",
        description="""
Stock splits, special dividends, and spin-offs cause false
mean reversion signals when price data isn't properly adjusted.

Lost 45 bps on NVDA position when 10:1 split wasn't reflected
in intraday data feed.

All signals must check for corporate actions in T-5 to T+5 window.
""".strip(),
        supporting_events=[
            "trade_20240610_NVDA_SPLIT_001",
            "data_incident_20240610",
        ],
        evidence_strength=0.95,
        estimated_impact_bps=45,
        frequency="rare",
        action_items=[
            "Add corporate action filter to signal pipeline",
            "Subscribe to real-time corporate action feed",
            "Create data quality dashboard",
        ],
        status="active",
    )
    learning.add_lesson(lesson4, learned_by="data_scientist")


def seed_market_insights(learning: LearningMemory):
    """Seed market behavior insights"""

    # Insight 1: Post-FOMC pattern
    insight1 = MarketInsight(
        insight_id="insight_001",
        insight_type=InsightType.PATTERN,
        market_condition="fomc_day",
        observation="""
Market tends to reverse initial FOMC reaction within 24 hours
approximately 65% of the time. Initial spike up often followed
by profit-taking, initial drop often followed by dip-buying.
""".strip(),
        statistical_evidence={
            "sample_size": 48,
            "win_rate": 0.65,
            "avg_reversal_bps": 45,
            "t_statistic": 2.8,
            "p_value": 0.007,
        },
        trading_implications=[
            "Consider fading large initial FOMC moves",
            "Size smaller on FOMC day directional bets",
            "Look for reversal signals in first 24 hours post-FOMC",
        ],
    )
    learning.add_market_insight(insight1, discovered_by="quant_researcher")

    # Insight 2: Sector rotation
    insight2 = MarketInsight(
        insight_id="insight_002",
        insight_type=InsightType.PATTERN,
        market_condition="rising_rates",
        observation="""
When 10-year Treasury yield rises >10bps in a week, growth stocks
underperform value stocks by 150-200 bps on average over the
following 2 weeks.
""".strip(),
        statistical_evidence={
            "sample_size": 85,
            "correlation": 0.45,
            "avg_spread_bps": 175,
            "t_statistic": 3.2,
            "p_value": 0.002,
        },
        trading_implications=[
            "Reduce growth exposure when rates spike",
            "Consider growth/value pair trade",
            "Monitor Treasury auctions for rate signals",
        ],
    )
    learning.add_market_insight(insight2, discovered_by="signal_scientist")


def seed_best_practices(learning: LearningMemory):
    """Seed operational best practices"""

    learning.add_best_practice(
        title="Pre-Trade Risk Check Protocol",
        description="""
Before executing any trade, the following checks must pass:
1. Position size within limits (single name, sector, portfolio)
2. Correlation check against existing book
3. Liquidity check (order < 1% of ADV)
4. Market regime check (not halted, not circuit breaker)
5. Compliance check (restricted list, insider window)
""".strip(),
        when_to_use="Before every trade execution",
        examples=[
            "Caught oversized TSLA position that would breach sector limit",
            "Prevented trade during earnings blackout window",
            "Reduced size on illiquid small-cap to meet ADV limit",
        ],
        discovered_by="compliance_officer",
    )

    learning.add_best_practice(
        title="Signal Validation 4-Eye Principle",
        description="""
Every new signal type requires validation by at least two
independent researchers before production deployment:
1. Primary researcher develops and backtests
2. Secondary researcher validates on different data period
3. Risk team reviews for potential edge cases
4. IC approves for production with size limits
""".strip(),
        when_to_use="When deploying any new signal type",
        examples=[
            "Momentum signal validated by both Sarah and Chen",
            "Stat arb signal caught overfitting in second validation",
            "Risk team identified correlation issue before deployment",
        ],
        discovered_by="research_lead",
    )


def seed_anti_patterns(learning: LearningMemory):
    """Seed things to avoid"""

    learning.add_anti_pattern(
        title="Chasing Performance After Drawdowns",
        description="""
After a significant drawdown (>5%), there's a tendency to
increase position sizes to 'make back' losses. This usually
leads to larger losses.
""".strip(),
        why_bad="""
1. Drawdowns often occur during regime changes
2. Increased sizing increases risk of further drawdown
3. Emotional decision-making impairs judgment
4. Creates negative feedback loop
""".strip(),
        what_to_do_instead="""
1. Reduce position sizes by 20% after 5% drawdown
2. Review regime indicators before resuming normal sizing
3. Focus on signal quality, not P&L recovery
4. Let position sizing model, not emotions, determine size
""".strip(),
        examples=[
            "Feb 2024: Increased size after 6% drawdown, lost additional 4%",
            "Aug 2023: Discipline after drawdown, recovered within 2 weeks",
        ],
        discovered_by="investment_committee",
    )

    learning.add_anti_pattern(
        title="Overriding Model on 'Gut Feel'",
        description="""
Manually overriding signal models because 'this time is different'
or 'the market doesn't feel right' has negative expected value.
""".strip(),
        why_bad="""
1. Gut feel is systematically overconfident
2. Models incorporate more data than intuition
3. Creates inconsistent track record
4. Undermines quantitative discipline
""".strip(),
        what_to_do_instead="""
1. If model seems wrong, investigate the data
2. Add new factors to model rather than overriding
3. Document every override for later analysis
4. Review override outcomes quarterly
""".strip(),
        examples=[
            "Override on AAPL 2024-01: Lost 35 bps vs model signal",
            "Override on market regime: Missed 2% rally by going defensive",
        ],
        discovered_by="investment_committee",
    )


def seed_historical_trades(trades: TradeMemory):
    """Seed some historical trade memories"""

    # Profitable mean reversion trade
    outcome1 = TradeOutcome(
        trade_id="trade_20240115_AAPL_MR_001",
        ticker="AAPL",
        direction="long",
        signal_type="mean_reversion",
        entry_price=185.50,
        exit_price=188.25,
        entry_date="2024-01-15",
        exit_date="2024-01-17",
        quantity=5000,
        pnl_bps=148,
        pnl_dollars=13750,
        vs_expected_bps=28,
        slippage_bps=2.1,
        market_impact_bps=3.5,
        execution_algorithm="twap",
        market_regime="low_vol",
        sector_performance=1.2,
        vix_level=14.5,
        confidence_at_entry=0.58,
        p_value_at_entry=0.008,
    )
    trades.remember_trade(
        outcome1,
        agent_id="execution_trader",
        lessons_learned=[
            "Mean reversion worked well in low VIX environment",
            "TWAP execution kept market impact low",
        ],
    )

    # Loss on momentum trade
    outcome2 = TradeOutcome(
        trade_id="trade_20240120_TSLA_MOM_001",
        ticker="TSLA",
        direction="long",
        signal_type="momentum",
        entry_price=225.00,
        exit_price=218.50,
        entry_date="2024-01-20",
        exit_date="2024-01-22",
        quantity=2000,
        pnl_bps=-289,
        pnl_dollars=-13000,
        vs_expected_bps=-189,
        slippage_bps=5.2,
        market_impact_bps=8.0,
        execution_algorithm="vwap",
        market_regime="high_vol",
        sector_performance=-2.5,
        vix_level=22.0,
        confidence_at_entry=0.52,
        p_value_at_entry=0.015,
    )
    trades.remember_trade(
        outcome2,
        agent_id="execution_trader",
        lessons_learned=[
            "Momentum signal failed in high VIX environment",
            "P-value was borderline (0.015 vs 0.01 threshold)",
            "Should have reduced size given elevated VIX",
        ],
    )


def seed_historical_decisions(decisions: DecisionMemory):
    """Seed some historical IC decisions"""

    # Approved trade that worked
    decision1 = Decision(
        decision_id="dec_20240115_001",
        decision_type=DecisionType.IC_APPROVAL,
        decision_maker="investment_committee",
        timestamp="2024-01-15T09:30:00",
        subject="AAPL long mean_reversion",
        action="APPROVED with full size",
        rationale=[
            "Strong statistical significance (p=0.008)",
            "Low VIX environment favorable for mean reversion",
            "AAPL has high liquidity, easy to execute",
            "Within all risk limits",
        ],
        signal_strength=0.72,
        confidence=0.58,
        risk_score=0.3,
        market_conditions={"vix": 14.5, "regime": "low_vol"},
        outcome=DecisionOutcome.CORRECT,
        outcome_notes="Trade returned +148 bps, exceeded expectations",
        outcome_pnl_bps=148,
    )
    decisions.remember_decision(decision1)

    # Rejected trade (correct rejection)
    decision2 = Decision(
        decision_id="dec_20240120_002",
        decision_type=DecisionType.IC_REJECTION,
        decision_maker="investment_committee",
        timestamp="2024-01-20T09:30:00",
        subject="NVDA short stat_arb",
        action="REJECTED - insufficient evidence",
        rationale=[
            "P-value borderline at 0.02",
            "High VIX environment increases risk",
            "NVDA has high momentum, shorting risky",
            "Earnings in 2 weeks adds uncertainty",
        ],
        signal_strength=0.45,
        confidence=0.51,
        risk_score=0.7,
        market_conditions={"vix": 22.0, "regime": "high_vol"},
        outcome=DecisionOutcome.CORRECT,
        outcome_notes="NVDA rose 15% after rejection, correct to avoid",
    )
    decisions.remember_decision(decision2)


def seed_all_memories():
    """Seed all memory systems with initial data"""
    print("Seeding memory systems...")

    # Get memory instances
    learning = get_learning_memory()
    trades = get_trade_memory()
    decisions = get_decision_memory()

    # Seed each component
    print("  - Seeding lessons learned...")
    seed_lessons(learning)

    print("  - Seeding market insights...")
    seed_market_insights(learning)

    print("  - Seeding best practices...")
    seed_best_practices(learning)

    print("  - Seeding anti-patterns...")
    seed_anti_patterns(learning)

    print("  - Seeding historical trades...")
    seed_historical_trades(trades)

    print("  - Seeding historical decisions...")
    seed_historical_decisions(decisions)

    print("Memory seeding complete!")

    # Return summary
    memory = get_memory_system()
    return memory.get_memory_summary()


if __name__ == "__main__":
    summary = seed_all_memories()
    print(f"\nMemory Summary: {summary}")
