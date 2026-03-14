"""
Seed Data for Knowledge Base

Populates the knowledge base with initial data including:
- Common trading strategies
- Risk policies
- Academic research summaries
- Market patterns
"""

from typing import Optional
from .base import get_knowledge_base, RenTechKnowledge
from .market_data import get_market_data_knowledge
from .research_papers import get_research_knowledge
from .strategy_docs import get_strategy_knowledge
from .trade_history import get_trade_history_knowledge


def seed_research_knowledge(research_kb=None):
    """Seed academic and internal research"""
    kb = research_kb or get_research_knowledge()

    # Academic papers
    kb.add_academic_paper(
        title="Returns to Buying Winners and Selling Losers",
        authors=["Jegadeesh", "Titman"],
        abstract="This paper documents that strategies which buy stocks that have performed well in the past and sell stocks that have performed poorly in the past generate significant positive returns over 3 to 12 month holding periods.",
        key_findings=[
            "Momentum strategies generate significant abnormal returns",
            "6-month formation and 6-month holding period is optimal",
            "Returns are not explained by systematic risk",
            "Profits are partially due to delayed reaction to firm-specific information",
        ],
        methodology="Cross-sectional momentum portfolios with various formation and holding periods",
        relevance_to_trading="Basis for our momentum signals. Use 12-1 month momentum with 1-month reversal adjustment.",
        publication_year=1993,
        tags=["momentum", "cross-sectional", "classic"],
    )

    kb.add_academic_paper(
        title="Mean Reversion in Stock Prices: Evidence and Implications",
        authors=["Poterba", "Summers"],
        abstract="This paper investigates whether stock prices exhibit mean reversion using variance ratio tests.",
        key_findings=[
            "Stock returns exhibit positive autocorrelation at short horizons",
            "Negative autocorrelation at longer horizons suggests mean reversion",
            "Mean reversion is stronger for smaller stocks",
            "Transaction costs may prevent full exploitation",
        ],
        methodology="Variance ratio tests on long-term stock returns",
        relevance_to_trading="Supports our mean reversion strategies. Focus on 3-5 day holding periods.",
        publication_year=1988,
        tags=["mean_reversion", "autocorrelation", "classic"],
    )

    kb.add_academic_paper(
        title="Statistical Arbitrage in the U.S. Equities Market",
        authors=["Avellaneda", "Lee"],
        abstract="We study the implementation of statistical arbitrage strategies using principal component analysis and mean-reverting residuals.",
        key_findings=[
            "PCA effectively identifies common factors in stock returns",
            "Residuals after factor adjustment exhibit mean reversion",
            "Optimal entry at 2 standard deviation moves",
            "Half-life of mean reversion is typically 10-20 days",
        ],
        methodology="PCA factor model with Ornstein-Uhlenbeck mean reversion",
        relevance_to_trading="Direct implementation guide for our stat arb strategies.",
        publication_year=2010,
        tags=["statistical_arbitrage", "pca", "mean_reversion"],
    )

    # Factor research
    kb.add_factor_research(
        factor_name="Momentum 12-1",
        description="Cross-sectional momentum based on past 12-month returns excluding the most recent month",
        construction="""
1. Calculate 12-month cumulative return for each stock
2. Exclude the most recent month (to avoid reversal)
3. Rank stocks by this return
4. Go long top decile, short bottom decile
5. Rebalance monthly
""",
        historical_performance={
            "annual_return": 8.5,
            "annual_vol": 18.0,
            "sharpe": 0.47,
            "max_dd": -52.0,
            "period": "1963-2023",
        },
        correlation_with_other_factors={
            "market": 0.05,
            "size": -0.15,
            "value": -0.25,
            "quality": 0.10,
        },
        implementation_notes="Use sector-neutral construction to reduce crash risk. Consider 50% Kelly fraction.",
    )

    kb.add_factor_research(
        factor_name="Quality (Profitability)",
        description="Long high-quality stocks (high ROE, low debt, stable earnings), short low-quality",
        construction="""
1. Calculate gross profitability (GP/Assets)
2. Calculate ROE
3. Calculate earnings stability (5-year std of earnings growth)
4. Combine into composite score
5. Long top quintile, short bottom quintile
""",
        historical_performance={
            "annual_return": 5.2,
            "annual_vol": 10.0,
            "sharpe": 0.52,
            "max_dd": -25.0,
            "period": "1963-2023",
        },
        correlation_with_other_factors={
            "market": -0.10,
            "size": -0.20,
            "value": -0.30,
            "momentum": 0.15,
        },
        implementation_notes="Quality provides defensive characteristics. Increase weight in volatile markets.",
    )

    # Internal research
    kb.add_internal_research(
        title="Intraday Mean Reversion in Liquid Stocks",
        researcher="Signal Scientist",
        summary="Analysis of intraday mean reversion patterns in S&P 500 stocks shows significant opportunities in the first and last hours of trading.",
        findings=[
            "Opening 30 minutes show strongest mean reversion (half-life ~15 min)",
            "Midday (11am-2pm) shows weakest patterns",
            "Last hour mean reversion tied to index rebalancing",
            "Pattern stronger on high-volume days",
        ],
        data_used=["1-minute bars for S&P 500", "2 years of data", "Order book snapshots"],
        statistical_results={
            "sample_size": 50000,
            "p_value": 0.003,
            "t_stat": 2.97,
            "sharpe": 1.8,
            "ic": 0.04,
            "needs_validation": False,
            "production_ready": True,
        },
        recommendations=[
            "Focus mean reversion signals on first/last hour",
            "Reduce activity during midday",
            "Increase size on high-volume days",
        ],
    )

    print("Research knowledge seeded successfully")


def seed_strategy_knowledge(strategy_kb=None):
    """Seed strategy documentation"""
    kb = strategy_kb or get_strategy_knowledge()

    # Mean reversion strategy
    kb.add_strategy_spec(
        strategy_name="Equity Mean Reversion",
        strategy_type="mean_reversion",
        description="Short-term mean reversion in liquid US equities. Exploits temporary price dislocations using statistical signals.",
        universe=["S&P 500 constituents", "Liquid ETFs", "Min ADV $10M"],
        signals_used=["Z-score of returns", "RSI divergence", "Bollinger band deviation"],
        entry_rules=[
            "Z-score > 2.0 for short, < -2.0 for long",
            "RSI confirms (>70 for short, <30 for long)",
            "No earnings in next 5 days",
            "Not on restricted list",
        ],
        exit_rules=[
            "Z-score returns to ±0.5",
            "Max holding period 5 days",
            "Stop loss at 3% adverse move",
            "Profit target at 2% favorable move",
        ],
        risk_parameters={
            "max_position_pct": 2.0,
            "max_sector_pct": 15.0,
            "stop_loss_pct": 3.0,
            "take_profit_pct": 2.0,
            "max_drawdown_pct": 10.0,
            "max_leverage": 4.0,
            "turnover_target": 50.0,
            "execution_urgency": "normal",
            "participation_rate": 5.0,
            "default_algo": "TWAP",
            "target_sharpe": 2.0,
            "target_return": 15.0,
        },
        allocation=25.0,
        status="active",
    )

    # Momentum strategy
    kb.add_strategy_spec(
        strategy_name="Cross-Sectional Momentum",
        strategy_type="momentum",
        description="Medium-term momentum strategy buying recent winners and shorting losers. Sector-neutral construction.",
        universe=["Russell 1000", "Min market cap $1B", "Min ADV $5M"],
        signals_used=["12-1 month return", "Earnings momentum", "Analyst revision momentum"],
        entry_rules=[
            "Top decile by 12-1 month return (long)",
            "Bottom decile by 12-1 month return (short)",
            "Sector neutral (equal weight per sector)",
            "No recent earnings surprise > 3 std",
        ],
        exit_rules=[
            "Monthly rebalance",
            "Remove if drops out of top/bottom decile",
            "Remove if major news event",
            "Stop loss at 15% drawdown per position",
        ],
        risk_parameters={
            "max_position_pct": 1.5,
            "max_sector_pct": 20.0,
            "stop_loss_pct": 15.0,
            "max_drawdown_pct": 25.0,
            "max_leverage": 2.0,
            "turnover_target": 12.0,
            "execution_urgency": "patient",
            "participation_rate": 3.0,
            "default_algo": "VWAP",
            "target_sharpe": 0.8,
            "target_return": 12.0,
        },
        allocation=20.0,
        status="active",
    )

    # Risk policy
    kb.add_risk_policy(
        policy_name="Position Limits Policy",
        policy_type="position_limits",
        description="Defines maximum position sizes to ensure diversification and limit single-name risk.",
        rules=[
            "No single position may exceed 5% of NAV",
            "No sector may exceed 25% of NAV",
            "Aggregate short exposure may not exceed 100% of NAV",
            "Aggregate long exposure may not exceed 150% of NAV",
        ],
        limits={
            "single_position": {"soft": 3.0, "hard": 5.0},
            "sector_exposure": {"soft": 20.0, "hard": 25.0},
            "short_exposure": {"soft": 80.0, "hard": 100.0},
            "long_exposure": {"soft": 120.0, "hard": 150.0},
        },
        escalation_procedure=[
            "Soft limit breach: Alert risk team, monitor for 24h",
            "Hard limit breach: Immediately reduce to soft limit",
            "Repeated breaches: Escalate to Investment Committee",
        ],
        effective_date="2024-01-01",
    )

    kb.add_risk_policy(
        policy_name="Drawdown Management Policy",
        policy_type="drawdown",
        description="Procedures for managing portfolio drawdowns to protect capital.",
        rules=[
            "At 10% drawdown: Review all positions, reduce risk by 25%",
            "At 15% drawdown: Emergency IC meeting, reduce risk by 50%",
            "At 20% drawdown: Halt new trades, orderly liquidation",
        ],
        limits={
            "drawdown_warning": {"soft": 7.0, "hard": 10.0},
            "drawdown_action": {"soft": 10.0, "hard": 15.0},
            "drawdown_halt": {"soft": 15.0, "hard": 20.0},
        },
        escalation_procedure=[
            "Warning level: Daily report to IC Chair",
            "Action level: Emergency IC call within 2 hours",
            "Halt level: Full trading halt, board notification",
        ],
        effective_date="2024-01-01",
    )

    # Position sizing guide
    kb.add_position_sizing_guide(
        guide_name="Signal-Based Kelly Sizing",
        methodology="Fractional Kelly Criterion",
        description="Position sizing based on signal strength and confidence using Kelly criterion with safety margin.",
        formula="""
Kelly Fraction = (p * b - q) / b

Where:
- p = win probability (from signal confidence)
- q = 1 - p
- b = win/loss ratio (from expected return / stop loss)

Final Size = Kelly Fraction * 0.25 * Base Size

The 0.25 multiplier (quarter Kelly) accounts for:
- Estimation error in parameters
- Fat tails in return distribution
- Correlation with existing positions
""",
        parameters={
            "kelly_fraction": 0.25,
            "base_size_pct": 2.0,
            "min_confidence": 0.55,
            "max_position_pct": 5.0,
        },
        examples=[
            {"signal_strength": 0.8, "confidence": 0.6, "volatility": 25, "size_pct": 1.5},
            {"signal_strength": 0.5, "confidence": 0.55, "volatility": 20, "size_pct": 0.8},
            {"signal_strength": 0.9, "confidence": 0.7, "volatility": 30, "size_pct": 2.0},
        ],
    )

    # Execution playbook
    kb.add_execution_playbook(
        playbook_name="Large Order Execution",
        scenario="Orders > 5% of ADV",
        description="Execution approach for large orders that require careful handling to minimize market impact.",
        steps=[
            "Assess current market conditions (spread, depth, volatility)",
            "Choose appropriate algorithm (typically Arrival Price or VWAP)",
            "Set participation rate at 3-5% of volume",
            "Spread execution over 2-4 hours",
            "Use dark pools for 20-30% of order",
            "Monitor real-time and adjust if conditions change",
            "Complete post-trade analysis",
        ],
        algorithms=["Arrival Price", "Adaptive VWAP", "Implementation Shortfall"],
        parameters={
            "participation_rate": 5.0,
            "duration_minutes": 180,
            "urgency": "patient",
            "max_spread_bps": 10,
            "limit_offset_bps": 5,
            "primary_venue": "Auto-route",
            "dark_pool_pct": 25,
            "cancel_condition": "Price moves 1% adverse",
            "pause_condition": "Unusual volume spike",
        },
        examples=[
            "100k shares of AAPL (0.1% of ADV) - use VWAP over 2 hours",
            "500k shares of MSFT (0.5% of ADV) - use Arrival Price over 3 hours",
            "1M shares of small cap (5% of ADV) - use Adaptive over full day",
        ],
    )

    print("Strategy knowledge seeded successfully")


def seed_market_knowledge(market_kb=None):
    """Seed market data knowledge"""
    kb = market_kb or get_market_data_knowledge()

    # Price patterns
    kb.add_price_pattern(
        ticker="AAPL",
        pattern_name="Post-Earnings Mean Reversion",
        description="AAPL shows consistent mean reversion 3-5 days after earnings announcements when the initial move exceeds 5%.",
        statistics={
            "mean_return": 1.5,
            "std_return": 3.2,
            "sharpe_ratio": 1.2,
            "win_rate": 0.58,
            "p_value": 0.008,
            "sample_size": 45,
            "lookback": 5,
            "holding_period": 3,
            "entry_threshold": 5.0,
            "exit_threshold": 0.5,
            "bull_sharpe": 1.5,
            "bear_sharpe": 0.8,
            "high_vol_sharpe": 1.8,
        },
        tags=["AAPL", "earnings", "mean_reversion", "tech"],
    )

    # Volume profile
    kb.add_volume_profile(
        ticker="AAPL",
        profile_data={
            "adv_20": 75000000,
            "adv_60": 72000000,
            "open_pct": 8,
            "morning_pct": 35,
            "midday_pct": 20,
            "afternoon_pct": 27,
            "close_pct": 10,
            "spread_bps": 1.5,
            "depth_1pct": 50000000,
            "kyle_lambda": 0.0008,
            "opt_participation": 3,
            "best_time": "10:00-11:30",
            "avoid_time": "12:00-13:30",
        },
    )

    # Sector analysis
    kb.add_sector_analysis(
        sector="Technology",
        analysis={
            "avg_beta": 1.15,
            "avg_vol": 28,
            "ytd_return": 25,
            "spy_corr": 0.85,
            "sector_beta": 1.2,
            "top_holdings": [
                {"ticker": "AAPL", "weight": 20},
                {"ticker": "MSFT", "weight": 18},
                {"ticker": "NVDA", "weight": 12},
            ],
            "risk_off_behavior": "Underperforms, high correlation increase",
            "risk_on_behavior": "Outperforms, leads market rallies",
            "assessment": "Currently extended, momentum strong but watch for reversal signals",
            "mean_rev_score": 6,
            "momentum_score": 8,
        },
    )

    # Microstructure insight
    kb.add_microstructure_insight(
        topic="Opening Auction Dynamics",
        insight="The opening auction contains significant price discovery. Stocks that gap up more than 2% tend to continue in the same direction for the first 30 minutes but then mean-revert.",
        evidence={
            "period": "2020-2024",
            "sample_size": 15000,
            "significance": "p < 0.01",
            "findings": [
                "Gap-up > 2%: 65% continue for 30min, then 55% mean revert",
                "Gap-down > 2%: 60% continue for 30min, then 50% mean revert",
                "Opening volume predicts intraday volume with R² = 0.7",
            ],
            "implications": "Trade with momentum in first 30min, then fade",
            "actions": [
                "Set momentum signals for 9:30-10:00 window",
                "Set mean reversion signals for 10:00-11:00 window",
                "Size based on opening volume",
            ],
        },
    )

    print("Market knowledge seeded successfully")


def seed_trade_history(history_kb=None):
    """Seed example trade history"""
    kb = history_kb or get_trade_history_knowledge()

    # Example profitable trade
    kb.add_trade_record(
        trade_id="TRD_20240115_001",
        ticker="AAPL",
        direction="long",
        signal_type="mean_reversion",
        entry_date="2024-01-15",
        exit_date="2024-01-18",
        entry_price=185.50,
        exit_price=189.25,
        quantity=10000,
        pnl_bps=202,
        execution_metrics={
            "vs_arrival_bps": -2.5,
            "vs_vwap_bps": 1.2,
            "impact_bps": 3.0,
            "fill_rate": 0.98,
            "algorithm": "TWAP",
        },
        signal_metrics={
            "strength": 0.75,
            "confidence": 0.68,
            "p_value": 0.007,
            "expected_return_bps": 150,
        },
        lessons=[
            "Signal worked well in low-vol environment",
            "Entry timing was good (near daily low)",
            "Could have held longer for additional gain",
        ],
    )

    # Example loss trade
    kb.add_trade_record(
        trade_id="TRD_20240120_002",
        ticker="NVDA",
        direction="short",
        signal_type="mean_reversion",
        entry_date="2024-01-20",
        exit_date="2024-01-22",
        entry_price=615.00,
        exit_price=642.50,
        quantity=2000,
        pnl_bps=-447,
        execution_metrics={
            "vs_arrival_bps": 1.5,
            "vs_vwap_bps": -0.8,
            "impact_bps": 5.0,
            "fill_rate": 0.95,
            "algorithm": "Arrival Price",
        },
        signal_metrics={
            "strength": 0.65,
            "confidence": 0.58,
            "p_value": 0.015,
            "expected_return_bps": 100,
        },
        lessons=[
            "Mean reversion failed during strong momentum",
            "Should have checked sector momentum before entry",
            "P-value was borderline (0.015 > 0.01 threshold)",
            "Stop loss saved from larger loss",
        ],
    )

    # Trade lesson
    kb.add_trade_lesson(
        lesson_title="Avoid Mean Reversion Shorts in Strong Momentum",
        context="Several mean reversion short signals failed during Q1 2024 tech rally",
        what_happened="Mean reversion signals correctly identified overbought conditions but momentum was too strong. Multiple trades hit stop losses.",
        what_we_learned=[
            "Check sector momentum before mean reversion shorts",
            "Avoid shorting stocks with 20+ day momentum > 10%",
            "Mean reversion works better in range-bound markets",
            "Consider reducing short exposure in trending markets",
        ],
        action_items=[
            "Add momentum filter to mean reversion signal",
            "Create market regime indicator",
            "Reduce mean reversion allocation in trending regimes",
        ],
        related_trades=["TRD_20240120_002", "TRD_20240122_003", "TRD_20240125_004"],
        severity="high",
    )

    print("Trade history seeded successfully")


def seed_all_knowledge():
    """Seed all knowledge bases with initial data"""
    print("Seeding Renaissance Technologies Knowledge Base...")
    print("-" * 50)

    seed_research_knowledge()
    seed_strategy_knowledge()
    seed_market_knowledge()
    seed_trade_history()

    print("-" * 50)
    print("Knowledge base seeding complete!")

    # Print statistics
    kb = get_knowledge_base()
    stats = kb.get_statistics()
    print(f"\nTotal documents: {stats['total_documents']}")
    for cat, count in stats['by_category'].items():
        if count > 0:
            print(f"  - {cat}: {count}")


if __name__ == "__main__":
    seed_all_knowledge()
