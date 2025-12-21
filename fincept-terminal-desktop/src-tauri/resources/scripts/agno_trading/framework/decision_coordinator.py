"""
Decision coordinator for orchestrating trading cycles.
"""

import time
import uuid
from typing import List, Dict
from .base_composer import BaseComposer
from .base_execution import BaseExecutionGateway
from .base_portfolio import BasePortfolioService
from .base_features import BaseFeaturesPipeline
from .types import (
    ComposeContext,
    DecisionCycleResult,
    PortfolioView,
    TradeDigest,
    ComposeResult
)


class DecisionCoordinator:
    """
    Coordinates a single trading cycle.

    Flow:
    1. Build features (market data + technical indicators)
    2. Get portfolio state
    3. Build performance digest
    4. LLM decision via composer
    5. Execute trades
    6. Update portfolio
    7. Return results
    """

    def __init__(
        self,
        strategy_id: str,
        composer: BaseComposer,
        execution_gateway: BaseExecutionGateway,
        features_pipeline: BaseFeaturesPipeline,
        portfolios: Dict[str, BasePortfolioService],  # One per model
        model_names: List[str]
    ):
        """
        Initialize decision coordinator.

        Args:
            strategy_id: Competition/strategy ID
            composer: Decision composer (LLM-based)
            execution_gateway: Execution gateway (paper or live)
            features_pipeline: Features pipeline
            portfolios: Dict of model_name -> portfolio service
            model_names: List of competing model names
        """
        self.strategy_id = strategy_id
        self.composer = composer
        self.execution_gateway = execution_gateway
        self.features_pipeline = features_pipeline
        self.portfolios = portfolios
        self.model_names = model_names
        self.cycle_count = 0

    async def run_once(self) -> DecisionCycleResult:
        """
        Execute one trading cycle for all models.

        Returns:
            DecisionCycleResult with all decisions and executions
        """
        self.cycle_count += 1
        timestamp_ms = int(time.time() * 1000)
        cycle_id = f"cycle_{uuid.uuid4().hex[:8]}"

        # 1. Build features
        features_result = await self.features_pipeline.build()

        # 2. Get all decisions from competing models
        compose_results: List[ComposeResult] = []
        all_instructions = []

        for model_name in self.model_names:
            # Get portfolio for this model
            portfolio = self.portfolios[model_name]
            portfolio_view = await portfolio.get_portfolio_view()

            # Build digest (simplified - can be enhanced)
            digest = TradeDigest(
                total_trades=portfolio_view.trades_count,
                total_realized_pnl=portfolio_view.total_realized_pnl,
                overall_win_rate=0.0,  # TODO: Calculate from trade history
                sharpe_ratio=0.0,  # TODO: Calculate
                max_drawdown=0.0,  # TODO: Calculate
                by_instrument={}
            )

            # Create context
            context = ComposeContext(
                ts=timestamp_ms,
                compose_id=f"{cycle_id}_{model_name}",
                strategy_id=self.strategy_id,
                model_name=model_name,
                features=features_result.features,
                portfolio=portfolio_view,
                digest=digest
            )

            # Get decision from LLM
            compose_result = await self.composer.compose(context)
            compose_results.append(compose_result)

            # Collect instructions
            all_instructions.extend(compose_result.instructions)

        # 3. Execute all trades
        tx_results = await self.execution_gateway.execute(
            all_instructions,
            features_result.features
        )

        # 4. Apply trades to respective portfolios
        for tx in tx_results:
            model_name = tx.instruction.model_name
            if model_name and model_name in self.portfolios:
                await self.portfolios[model_name].apply_trade(tx)

        # 5. Mark portfolios to market
        current_prices = {
            feat.symbol: feat.close
            for feat in features_result.features
        }

        for portfolio in self.portfolios.values():
            await portfolio.mark_to_market(current_prices)

        # 6. Get updated portfolio snapshots
        portfolio_snapshots = {}
        for model_name in self.model_names:
            portfolio_snapshots[model_name] = await self.portfolios[model_name].get_portfolio_view()

        # 7. Build leaderboard
        leaderboard = []
        for model_name in self.model_names:
            portfolio_view = portfolio_snapshots[model_name]
            leaderboard.append({
                "model_name": model_name,
                "portfolio_value": portfolio_view.total_value,
                "total_pnl": portfolio_view.total_realized_pnl + portfolio_view.total_unrealized_pnl,
                "trades_count": portfolio_view.trades_count,
                "cash": portfolio_view.cash
            })

        # Sort by portfolio value (descending)
        leaderboard.sort(key=lambda x: x["portfolio_value"], reverse=True)

        # Add rank
        for i, entry in enumerate(leaderboard):
            entry["rank"] = i + 1

        # 8. Return complete cycle result
        return DecisionCycleResult(
            cycle_id=cycle_id,
            cycle_number=self.cycle_count,
            timestamp_ms=timestamp_ms,
            compose_results=compose_results,
            tx_results=tx_results,
            portfolio_snapshots=portfolio_snapshots,
            leaderboard=leaderboard
        )
