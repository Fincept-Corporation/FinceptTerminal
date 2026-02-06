"""
Renaissance Technologies Hedge Fund System

Main orchestrator for the multi-agent Renaissance Technologies system.
Implements Jim Simons' systematic quantitative approach.

Architecture:
    ┌─────────────────────────────────────────────────────────────┐
    │                    RenaissanceTechnologiesSystem            │
    │                                                             │
    │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
    │  │   Signal     │  │    Risk      │  │  Execution   │      │
    │  │ Generation   │  │  Modeling    │  │ Optimization │      │
    │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
    │         │                 │                 │               │
    │         │  ┌──────────────┴──────────────┐  │               │
    │         │  │     Statistical Arbitrage   │  │               │
    │         │  └──────────────┬──────────────┘  │               │
    │         │                 │                 │               │
    │         └────────────────┬┴─────────────────┘               │
    │                          │                                  │
    │                 ┌────────▼────────┐                         │
    │                 │    Medallion    │                         │
    │                 │   Synthesizer   │                         │
    │                 └────────┬────────┘                         │
    │                          │                                  │
    │                 ┌────────▼────────┐                         │
    │                 │  Final Decision │                         │
    │                 └─────────────────┘                         │
    └─────────────────────────────────────────────────────────────┘

Usage:
    from renaissance_technologies_hedge_fund_agent import RenaissanceTechnologiesSystem

    # Pure quantitative analysis (no LLM)
    system = RenaissanceTechnologiesSystem()
    result = system.analyze("AAPL")

    # With LLM enhancement
    system = RenaissanceTechnologiesSystem(
        model_provider="openai",
        api_key="sk-..."
    )
    result = system.analyze_with_llm("AAPL")
"""

from typing import Dict, Any, Optional, List
from dataclasses import dataclass
import json
import logging

# Analysis functions (pure Python, no LLM)
from .strategies import (
    run_signal_analysis,
    run_risk_analysis,
    run_execution_analysis,
    run_stat_arb_analysis,
    synthesize_decision,
)
# Agno agents (LLM-enhanced)
from .agents import (
    create_signal_scientist,
    create_risk_quant,
    create_execution_trader,
    create_quant_researcher,
    create_investment_committee_chair,
)
from .models.schemas import MedallionDecision, RenaissancePortfolioAnalysis, SignalType
from .utils import (
    fetch_stock_data_yfinance,
    fetch_market_benchmark,
    validate_data,
    prepare_analysis_inputs,
)

logger = logging.getLogger(__name__)


@dataclass
class AnalysisConfig:
    """Configuration for Renaissance analysis"""
    trade_value: float = 1_000_000  # Default $1M trade
    period: str = "1y"  # Historical data period
    include_peers: bool = False
    peer_ticker: Optional[str] = None
    expected_edge_bps: float = 20.0
    signal_decay_hours: float = 24.0


class RenaissanceTechnologiesSystem:
    """
    Renaissance Technologies Multi-Agent System.

    Implements Jim Simons' quantitative approach with:
    - Signal Generation (mean reversion, momentum, patterns)
    - Risk Modeling (volatility, VaR, factor decomposition)
    - Execution Optimization (market impact, TCA)
    - Statistical Arbitrage (mispricing, regime detection)
    - Medallion Synthesis (final systematic decision)
    """

    def __init__(
        self,
        model_provider: Optional[str] = None,
        model_id: str = "gpt-4o",
        api_key: Optional[str] = None
    ):
        """
        Initialize Renaissance Technologies System.

        Args:
            model_provider: LLM provider (openai, anthropic, groq) - None for pure quant
            model_id: Model identifier
            api_key: API key for LLM provider
        """
        self.model_provider = model_provider
        self.model_id = model_id
        self.api_key = api_key

        # Create Agno agents if LLM enabled
        self._agents = None
        if model_provider:
            self._agents = self._create_agents()

    def _create_agents(self) -> Dict[str, Any]:
        """
        Create Agno agents for LLM-enhanced analysis.

        Now uses dynamic model factory that reads from frontend SQLite config.
        """
        from .config import get_config
        cfg = get_config()

        # Agents will use model factory internally
        # No need to pass model provider info - it's loaded from frontend DB
        return {
            "signal": create_signal_scientist(cfg),
            "risk": create_risk_quant(cfg),
            "execution": create_execution_trader(cfg),
            "stat_arb": create_quant_researcher(cfg),
            "medallion": create_investment_committee_chair(cfg),
        }

    def analyze(
        self,
        ticker: str,
        config: Optional[AnalysisConfig] = None
    ) -> MedallionDecision:
        """
        Run pure quantitative analysis (no LLM).

        This is the core Renaissance approach: mathematical models only.

        Args:
            ticker: Stock ticker to analyze
            config: Analysis configuration

        Returns:
            MedallionDecision with signal and recommendations
        """
        config = config or AnalysisConfig()

        # Fetch data
        logger.info(f"Fetching data for {ticker}...")
        data = fetch_stock_data_yfinance(ticker, config.period)

        # Validate data
        is_valid, warnings = validate_data(data)
        if not is_valid:
            logger.warning(f"Data validation failed: {warnings}")
            return self._create_error_decision(ticker, warnings)

        for warning in warnings:
            logger.warning(warning)

        # Prepare inputs
        inputs = prepare_analysis_inputs(data, config.trade_value)

        # Fetch market benchmark for risk analysis
        market_prices = fetch_market_benchmark(config.period)

        # Run all analyses in parallel (conceptually)
        logger.info("Running signal generation analysis...")
        signal_analysis = run_signal_analysis(
            ticker=ticker,
            prices=inputs["prices"],
            revenues=inputs["revenues"],
            earnings=inputs["earnings"],
            metrics=inputs["metrics"]
        )

        logger.info("Running risk modeling analysis...")
        risk_analysis = run_risk_analysis(
            ticker=ticker,
            prices=inputs["prices"],
            market_prices=market_prices,
            market_cap=inputs["market_cap"],
            signal_strength=signal_analysis["aggregate_signal"],
            signal_confidence=signal_analysis["statistical_significance"] * 100
        )

        logger.info("Running execution analysis...")
        execution_analysis = run_execution_analysis(
            ticker=ticker,
            trade_value=config.trade_value,
            market_cap=inputs["market_cap"],
            avg_daily_volume=inputs["avg_daily_volume"],
            volatility=inputs["volatility"],
            expected_edge_bps=config.expected_edge_bps,
            signal_decay_hours=config.signal_decay_hours
        )

        logger.info("Running statistical arbitrage analysis...")
        stat_arb_analysis = run_stat_arb_analysis(
            ticker=ticker,
            prices=inputs["prices"],
            intrinsic_estimates=None,  # Would need analyst estimates
            peer_prices=None,  # Would need peer selection
            metrics=inputs["metrics"]
        )

        # Synthesize final decision
        logger.info("Synthesizing Medallion decision...")
        decision = synthesize_decision(
            ticker=ticker,
            signal_analysis=signal_analysis,
            risk_analysis=risk_analysis,
            execution_analysis=execution_analysis,
            stat_arb_analysis=stat_arb_analysis
        )

        return decision

    def analyze_portfolio(
        self,
        tickers: List[str],
        config: Optional[AnalysisConfig] = None
    ) -> RenaissancePortfolioAnalysis:
        """
        Analyze multiple tickers as a portfolio.

        Args:
            tickers: List of stock tickers
            config: Analysis configuration

        Returns:
            Portfolio analysis with all individual decisions
        """
        config = config or AnalysisConfig()

        decisions = {}
        for ticker in tickers:
            try:
                decision = self.analyze(ticker, config)
                decisions[ticker] = decision
            except Exception as e:
                logger.error(f"Error analyzing {ticker}: {e}")
                decisions[ticker] = self._create_error_decision(ticker, [str(e)])

        # Calculate portfolio-level metrics
        signals = [d.signal for d in decisions.values()]
        confidences = [d.confidence for d in decisions.values()]

        bullish_count = sum(1 for s in signals if s == SignalType.BULLISH)
        bearish_count = sum(1 for s in signals if s == SignalType.BEARISH)

        if bullish_count > bearish_count * 1.5:
            portfolio_signal = SignalType.BULLISH
        elif bearish_count > bullish_count * 1.5:
            portfolio_signal = SignalType.BEARISH
        else:
            portfolio_signal = SignalType.NEUTRAL

        portfolio_confidence = sum(confidences) / len(confidences) if confidences else 0

        summary = self._generate_portfolio_summary(decisions, portfolio_signal)

        return RenaissancePortfolioAnalysis(
            decisions=decisions,
            portfolio_signal=portfolio_signal,
            portfolio_confidence=portfolio_confidence,
            correlation_matrix=None,  # Would need to calculate
            portfolio_var_95=None,    # Would need to calculate
            summary=summary
        )

    def analyze_with_llm(
        self,
        ticker: str,
        query: Optional[str] = None,
        config: Optional[AnalysisConfig] = None
    ) -> Dict[str, Any]:
        """
        Run analysis with LLM enhancement.

        Combines pure quant analysis with LLM reasoning.

        Args:
            ticker: Stock ticker
            query: Optional specific question
            config: Analysis configuration

        Returns:
            Enhanced analysis with LLM insights
        """
        if not self._agents:
            raise ValueError("LLM not configured. Provide model_provider in constructor.")

        # First run pure quant analysis
        decision = self.analyze(ticker, config)

        # Enhance with LLM
        query = query or f"Provide Renaissance Technologies-style analysis for {ticker}"

        # Run LLM agents
        llm_response = self._agents["medallion"].run(
            f"""Based on the following quantitative analysis, provide your assessment:

Ticker: {ticker}
Signal: {decision.signal.value}
Confidence: {decision.confidence}%
Statistical Edge: {decision.statistical_edge}

Key Factors: {', '.join(decision.key_factors)}
Risks: {', '.join(decision.risks)}

Reasoning: {decision.reasoning}

Question: {query}
"""
        )

        return {
            "quant_decision": decision,
            "llm_enhancement": llm_response.content if hasattr(llm_response, 'content') else str(llm_response)
        }

    def _create_error_decision(
        self,
        ticker: str,
        errors: List[str]
    ) -> MedallionDecision:
        """Create a neutral decision when analysis fails."""
        from .models.schemas import PositionSizing

        return MedallionDecision(
            ticker=ticker,
            signal=SignalType.NEUTRAL,
            confidence=0.0,
            statistical_edge=0.0,
            signal_score=5.0,
            risk_score=5.0,
            execution_score=5.0,
            arbitrage_score=5.0,
            position_sizing=PositionSizing(
                recommended_size_pct=0.0,
                kelly_fraction=0.0,
                max_position_size_pct=0.0,
                leverage_recommendation=1.0
            ),
            signal_strength={
                "mean_reversion": 0.0,
                "momentum": 0.0,
                "statistical_arbitrage": 0.0,
                "risk_adjusted_return": 0.0
            },
            reasoning=f"Analysis failed: {'; '.join(errors)}",
            key_factors=["Insufficient data for analysis"],
            risks=errors
        )

    def _generate_portfolio_summary(
        self,
        decisions: Dict[str, MedallionDecision],
        portfolio_signal: SignalType
    ) -> str:
        """Generate portfolio summary text."""
        total = len(decisions)
        bullish = sum(1 for d in decisions.values() if d.signal == SignalType.BULLISH)
        bearish = sum(1 for d in decisions.values() if d.signal == SignalType.BEARISH)
        neutral = total - bullish - bearish

        avg_confidence = sum(d.confidence for d in decisions.values()) / total if total else 0

        return (
            f"Portfolio Analysis: {total} securities analyzed. "
            f"Signal distribution: {bullish} bullish, {bearish} bearish, {neutral} neutral. "
            f"Overall signal: {portfolio_signal.value.upper()}. "
            f"Average confidence: {avg_confidence:.1f}%."
        )


def main():
    """CLI entry point for Renaissance Technologies analysis."""
    import sys
    import argparse

    parser = argparse.ArgumentParser(
        description="Renaissance Technologies Quantitative Analysis System"
    )
    parser.add_argument("ticker", help="Stock ticker to analyze")
    parser.add_argument(
        "--trade-value",
        type=float,
        default=1_000_000,
        help="Trade value in dollars (default: 1000000)"
    )
    parser.add_argument(
        "--period",
        default="1y",
        choices=["1mo", "3mo", "6mo", "1y", "2y", "5y"],
        help="Historical data period (default: 1y)"
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output as JSON"
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Verbose output"
    )

    args = parser.parse_args()

    # Configure logging
    logging.basicConfig(
        level=logging.INFO if args.verbose else logging.WARNING,
        format="%(asctime)s - %(levelname)s - %(message)s"
    )

    # Run analysis
    system = RenaissanceTechnologiesSystem()
    config = AnalysisConfig(
        trade_value=args.trade_value,
        period=args.period
    )

    try:
        decision = system.analyze(args.ticker, config)

        if args.json:
            # Output as JSON
            print(json.dumps(decision.model_dump(), indent=2, default=str))
        else:
            # Human-readable output
            print("\n" + "=" * 60)
            print(f"RENAISSANCE TECHNOLOGIES ANALYSIS: {args.ticker}")
            print("=" * 60)
            print(f"\nSignal: {decision.signal.value.upper()}")
            print(f"Confidence: {decision.confidence:.1f}%")
            print(f"Statistical Edge: {decision.statistical_edge:.4f}")
            print(f"\nPosition Sizing:")
            print(f"  Recommended: {decision.position_sizing.recommended_size_pct:.2f}% of portfolio")
            print(f"  Kelly Fraction: {decision.position_sizing.kelly_fraction:.2f}")
            print(f"  Leverage: {decision.position_sizing.leverage_recommendation:.1f}x")
            print(f"\nComponent Scores:")
            print(f"  Signal: {decision.signal_score:.1f}/10")
            print(f"  Risk: {decision.risk_score:.1f}/10")
            print(f"  Execution: {decision.execution_score:.1f}/10")
            print(f"  Arbitrage: {decision.arbitrage_score:.1f}/10")
            print(f"\nKey Factors:")
            for factor in decision.key_factors:
                print(f"  - {factor}")
            print(f"\nRisks:")
            for risk in decision.risks:
                print(f"  - {risk}")
            print(f"\nReasoning:\n{decision.reasoning}")
            print("\n" + "=" * 60)

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
