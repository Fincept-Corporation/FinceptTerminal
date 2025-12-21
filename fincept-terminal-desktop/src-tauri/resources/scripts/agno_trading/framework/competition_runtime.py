"""
Competition runtime for managing Alpha Arena competitions.
"""

import asyncio
from typing import List, Dict, Optional
from .decision_coordinator import DecisionCoordinator
from .llm_composer import LLMComposer, ModelConfig
from .paper_execution import PaperExecutionGateway
from .portfolio_service import InMemoryPortfolioService
from .market_data_source import CCXTMarketDataSource
from .features_pipeline import TechnicalFeaturesPipeline
from .types import DecisionCycleResult


class CompetitionRuntime:
    """
    Runtime for managing a multi-model trading competition.

    Responsibilities:
    - Initialize all components (composer, execution, portfolios, etc.)
    - Run trading cycles at specified intervals
    - Persist state to database
    - Stream updates to UI
    """

    def __init__(
        self,
        competition_id: str,
        models: List[ModelConfig],
        symbols: List[str],
        initial_capital: float = 10000.0,
        exchange_id: str = "kraken",
        custom_prompt: Optional[str] = None
    ):
        """
        Initialize competition runtime.

        Args:
            competition_id: Unique competition ID
            models: List of competing LLM models
            symbols: List of trading symbols
            initial_capital: Starting capital per model
            exchange_id: Exchange for market data
            custom_prompt: Optional custom trading instructions
        """
        self.competition_id = competition_id
        self.models = models
        self.symbols = symbols
        self.initial_capital = initial_capital
        self.is_running = False

        # Create components
        self.market_data_source = CCXTMarketDataSource(
            exchange_id=exchange_id,
            testnet=False,
            rate_limit=True
        )

        self.features_pipeline = TechnicalFeaturesPipeline(
            market_data_source=self.market_data_source,
            symbols=symbols,
            interval="1h",  # 1-hour candles for better technical analysis
            lookback=168    # 168 hours = 7 days of hourly data
        )

        self.execution_gateway = PaperExecutionGateway(
            fee_bps=10.0,
            default_slippage_bps=5.0
        )

        # Create portfolio for each model
        self.portfolios: Dict[str, InMemoryPortfolioService] = {}
        model_names = [model.name for model in models]

        for model_name in model_names:
            self.portfolios[model_name] = InMemoryPortfolioService(
                initial_capital=initial_capital
            )

        # Create composer
        self.composer = LLMComposer(
            models=models,
            custom_prompt=custom_prompt,
            max_positions=3,
            max_leverage=10.0
        )

        # Create coordinator
        self.coordinator = DecisionCoordinator(
            strategy_id=competition_id,
            composer=self.composer,
            execution_gateway=self.execution_gateway,
            features_pipeline=self.features_pipeline,
            portfolios=self.portfolios,
            model_names=model_names
        )

    async def run_cycle(self) -> DecisionCycleResult:
        """
        Run a single trading cycle.

        Returns:
            DecisionCycleResult with all decisions and executions
        """
        result = await self.coordinator.run_once()
        return result

    async def run_continuous(self, cycle_interval_seconds: int = 900):
        """
        Run continuous trading cycles.

        Args:
            cycle_interval_seconds: Seconds between cycles (default: 900 = 15 minutes)
        """
        self.is_running = True

        while self.is_running:
            try:
                # Run cycle
                result = await self.run_cycle()

                print(f"Cycle {result.cycle_number} complete", flush=True)

                # Wait for next cycle
                await asyncio.sleep(cycle_interval_seconds)

            except Exception as e:
                print(f"Error in trading cycle: {e}", flush=True)
                # Continue despite errors
                await asyncio.sleep(cycle_interval_seconds)

    def stop(self):
        """Stop the competition."""
        self.is_running = False

    async def get_leaderboard(self) -> List[Dict]:
        """
        Get current leaderboard.

        Returns:
            List of model rankings
        """
        leaderboard = []

        for model_name, portfolio in self.portfolios.items():
            view = await portfolio.get_portfolio_view()
            leaderboard.append({
                "model_name": model_name,
                "portfolio_value": view.total_value,
                "total_pnl": view.total_realized_pnl + view.total_unrealized_pnl,
                "return_pct": ((view.total_value - self.initial_capital) / self.initial_capital) * 100.0,
                "trades_count": view.trades_count,
                "cash": view.cash,
                "positions_count": len(view.positions)
            })

        # Sort by portfolio value
        leaderboard.sort(key=lambda x: x["portfolio_value"], reverse=True)

        # Add rank
        for i, entry in enumerate(leaderboard):
            entry["rank"] = i + 1

        return leaderboard

    async def get_portfolio_snapshot(self, model_name: str) -> Optional[Dict]:
        """Get portfolio snapshot for a specific model."""
        if model_name not in self.portfolios:
            return None

        view = await self.portfolios[model_name].get_portfolio_view()
        return view.to_dict()

    async def close(self):
        """Clean up resources."""
        await self.market_data_source.close()
        await self.execution_gateway.close()
