"""
Alpha Arena Competition

Main competition orchestration class.
"""

import asyncio
import json
import time
from datetime import datetime
from typing import AsyncGenerator, Dict, List, Optional, Any

from alpha_arena.types.models import (
    Competition,
    CompetitionConfig,
    CompetitionStatus,
    CompetitionModel,
    CycleResult,
    LeaderboardEntry,
    MarketData,
    ModelDecision,
    PortfolioState,
    PerformanceSnapshot,
)
from alpha_arena.types.responses import (
    StreamResponse,
    decision_started,
    decision_completed,
    trade_executed,
    portfolio_update,
    reasoning,
    market_data as market_data_response,
    leaderboard_update,
    cycle_completed,
    error,
    done,
    SystemResponseEvent,
)
from alpha_arena.core.base_agent import BaseTradingAgent, LLMTradingAgent
from alpha_arena.core.paper_trading_bridge import PaperTradingBridge, create_paper_trading_bridge
from alpha_arena.core.market_data import MarketDataProvider, get_market_data_provider
from alpha_arena.core.agent_manager import AgentManager, get_agent_manager
from alpha_arena.config.agent_cards import get_agent_card
from alpha_arena.utils.logging import get_logger
from alpha_arena.utils.uuid import generate_snapshot_id

logger = get_logger("competition")


class AlphaArenaCompetition:
    """
    Main competition orchestration class.

    Manages the competition lifecycle:
    - Agent initialization
    - Cycle execution
    - Trade execution
    - Leaderboard updates
    - Performance tracking
    """

    def __init__(self, config: CompetitionConfig):
        self.config = config
        self.competition_id = config.competition_id
        self.status = CompetitionStatus.CREATED
        self.cycle_count = 0
        self.start_time: Optional[datetime] = None
        self.end_time: Optional[datetime] = None

        # Agents and trading engines (now using database-backed bridge)
        self._agents: Dict[str, BaseTradingAgent] = {}
        self._engines: Dict[str, PaperTradingBridge] = {}
        self._market_provider: Optional[MarketDataProvider] = None

        # Tracking
        self._decisions: List[ModelDecision] = []
        self._snapshots: List[PerformanceSnapshot] = []
        self._cycle_results: List[CycleResult] = []
        self._is_running = False
        self._stop_requested = False

    async def initialize(self, api_keys: Dict[str, str]) -> bool:
        """
        Initialize the competition.

        Args:
            api_keys: Dict of provider -> api_key

        Returns:
            True if initialization successful
        """
        # Check if already initialized
        if self._agents and self._engines:
            logger.info(f"Competition {self.competition_id} already initialized with {len(self._agents)} agents")
            return True

        try:
            logger.info(f"Initializing competition {self.competition_id}")
            logger.info(f"API keys available for providers: {list(api_keys.keys())}")

            # Initialize market data provider with timeout
            try:
                self._market_provider = await asyncio.wait_for(
                    get_market_data_provider(self.config.exchange_id),
                    timeout=30.0
                )
                logger.info(f"Market data provider initialized for {self.config.exchange_id}")
            except asyncio.TimeoutError:
                logger.error(f"Market data provider initialization timed out for {self.config.exchange_id}")
                self.status = CompetitionStatus.FAILED
                return False

            # Create agents and trading engines for each model
            for model in self.config.models:
                logger.info(f"Initializing model: {model.name} (provider: {model.provider})")

                # Get agent card
                card = get_agent_card(model.name)

                if card:
                    provider = card.provider
                    model_id = card.model_id
                    logger.info(f"Found agent card for {model.name}: {provider}/{model_id}")
                else:
                    provider = model.provider
                    model_id = model.model_id
                    logger.info(f"Using model config: {provider}/{model_id}")

                # Get API key - try model-specific first, then provider-level
                api_key = model.api_key or api_keys.get(provider) or api_keys.get(provider.lower())

                if not api_key:
                    logger.warning(f"No API key found for provider '{provider}' - agent will use mock mode")

                # Get trading style if specified in model config
                trading_style = None
                if hasattr(model, 'trading_style'):
                    trading_style = model.trading_style
                elif hasattr(model, 'metadata') and model.metadata:
                    trading_style = model.metadata.get('trading_style')

                # Build instructions from custom system prompt (from Settings tab)
                custom_instructions = None
                if self.config.custom_prompt:
                    custom_instructions = [self.config.custom_prompt]

                # Create agent with optional trading style
                # Temperature and system prompt come from Settings > LLM Config global settings
                agent_capital = model.initial_capital or self.config.initial_capital
                agent = LLMTradingAgent(
                    name=model.name,
                    provider=provider,
                    model_id=model_id,
                    api_key=api_key,
                    temperature=self.config.temperature,
                    instructions=custom_instructions,
                    mode=self.config.mode.value if hasattr(self.config.mode, 'value') else self.config.mode,
                    trading_style=trading_style,
                    initial_capital=agent_capital,
                )

                # Initialize with timeout - agent MUST succeed with real LLM
                try:
                    init_success = await asyncio.wait_for(
                        agent.initialize(),
                        timeout=30.0
                    )
                    if init_success:
                        logger.info(f"Agent {model.name} initialized successfully with real LLM")
                        self._agents[model.name] = agent
                    else:
                        init_error = getattr(agent, 'init_error', 'unknown')
                        logger.error(f"Agent {model.name} failed to initialize: {init_error}")
                        continue  # Skip this agent
                except asyncio.TimeoutError:
                    logger.error(f"Agent {model.name} initialization timed out - skipping")
                    continue  # Skip this agent
                except Exception as agent_init_error:
                    logger.error(f"Agent {model.name} initialization error: {agent_init_error}")
                    continue  # Skip this agent

                # Create trading engine (database-backed bridge to main paper trading)
                engine = create_paper_trading_bridge(
                    competition_id=self.competition_id,
                    model_name=model.name,
                    initial_capital=model.initial_capital or self.config.initial_capital,
                    fee_rate=0.001,
                )
                self._engines[model.name] = engine
                logger.info(f"Trading engine (DB-backed) created for {model.name} with capital ${model.initial_capital or self.config.initial_capital}")

            # Validate enough agents initialized with real LLMs
            if len(self._agents) < 2:
                failed_count = len(self.config.models) - len(self._agents)
                logger.error(
                    f"Only {len(self._agents)} agent(s) initialized successfully out of {len(self.config.models)}. "
                    f"Need at least 2 agents for a competition."
                )
                self.status = CompetitionStatus.FAILED
                return False

            self.status = CompetitionStatus.CREATED
            logger.info(f"Competition {self.competition_id} initialized with {len(self._agents)} agents")
            return True

        except Exception as e:
            logger.exception(f"Failed to initialize competition: {e}")
            self.status = CompetitionStatus.FAILED
            return False

    async def run_cycle(self) -> AsyncGenerator[StreamResponse, None]:
        """
        Run a single competition cycle.

        Yields streaming responses for:
        - Market data updates
        - Decision progress
        - Trade executions
        - Leaderboard updates
        """
        if self.status == CompetitionStatus.FAILED:
            yield error("Competition is in failed state")
            return

        # Auto-transition to RUNNING if not already
        if self.status in [CompetitionStatus.CREATED, CompetitionStatus.PAUSED]:
            self.status = CompetitionStatus.RUNNING
            self._is_running = True
            if self.start_time is None:
                self.start_time = datetime.now()
            logger.info(f"Competition {self.competition_id} auto-transitioned to RUNNING state")

        self.cycle_count += 1
        cycle_number = self.cycle_count
        cycle_start = time.time()

        logger.info(f"Starting cycle {cycle_number}")

        # Start cycle notification
        yield StreamResponse(
            content=f"Starting cycle {cycle_number}",
            event=SystemResponseEvent.CYCLE_STARTED,
            metadata={"cycle_number": cycle_number},
        )

        try:
            # Fetch market data
            symbol = self.config.symbols[0] if self.config.symbols else "BTC/USD"

            # Ensure market provider is initialized
            if self._market_provider is None:
                yield error("Market data provider not initialized. Cannot run cycle without live market data.")
                return

            market = await self._market_provider.get_ticker(symbol)

            yield market_data_response(
                symbol=market.symbol,
                price=market.price,
                change_pct=0.0,  # Calculate if we have history
            )

            # Get decisions from each agent
            decisions: Dict[str, ModelDecision] = {}
            errors_list: List[str] = []

            for model_name, agent in self._agents.items():
                engine = self._engines[model_name]
                portfolio = engine.get_portfolio_state({symbol: market.price})

                yield decision_started(model_name, symbol)

                try:
                    # Get decision from agent
                    decision = await agent.make_decision(
                        market_data=market,
                        portfolio=portfolio,
                        cycle_number=cycle_number,
                    )
                    decision.competition_id = self.competition_id
                    decision.price_at_decision = market.price
                    decision.portfolio_value_before = portfolio.portfolio_value

                    # Emit reasoning
                    if decision.reasoning:
                        yield reasoning(model_name, decision.reasoning)

                    # Execute trade
                    trade_result = engine.execute_decision(decision, market)
                    decision.trade_executed = trade_result

                    # Get updated portfolio
                    new_portfolio = engine.get_portfolio_state({symbol: market.price})
                    decision.portfolio_value_after = new_portfolio.portfolio_value

                    decisions[model_name] = decision
                    self._decisions.append(decision)

                    # Emit decision completed
                    yield decision_completed(
                        model_name=model_name,
                        action=decision.action.value if hasattr(decision.action, 'value') else str(decision.action),
                        symbol=decision.symbol,
                        quantity=decision.quantity,
                        confidence=decision.confidence,
                        reasoning=decision.reasoning,
                    )

                    # Emit trade if executed
                    action_value = decision.action.value if hasattr(decision.action, 'value') else str(decision.action)
                    if trade_result.status == "executed" and action_value != "hold":
                        yield trade_executed(
                            model_name=model_name,
                            action=trade_result.action.value if hasattr(trade_result.action, 'value') else str(trade_result.action),
                            symbol=trade_result.symbol,
                            quantity=trade_result.quantity,
                            price=trade_result.price,
                            pnl=trade_result.pnl,
                        )

                    # Emit real-time portfolio update after trade
                    yield portfolio_update(
                        model_name=model_name,
                        portfolio_value=new_portfolio.portfolio_value,
                        cash=new_portfolio.cash,
                        total_pnl=new_portfolio.total_pnl,
                        positions=[
                            {
                                "symbol": p.get("symbol", ""),
                                "side": p.get("side", ""),
                                "quantity": p.get("quantity", 0),
                                "entry_price": p.get("entry_price", 0),
                                "current_price": p.get("current_price", 0),
                                "unrealized_pnl": p.get("unrealized_pnl", 0),
                            }
                            for p in new_portfolio.positions
                        ] if isinstance(new_portfolio.positions, list) else [],
                        trades_count=new_portfolio.trades_count,
                        cycle_number=cycle_number,
                    )

                except Exception as e:
                    error_msg = f"Error processing {model_name}: {str(e)}"
                    logger.exception(error_msg)
                    errors_list.append(error_msg)
                    yield error(error_msg)

            # Create snapshots for charting
            await self._create_snapshots(cycle_number, market.price)

            # Update leaderboard
            leaderboard = self._calculate_leaderboard({symbol: market.price})
            yield leaderboard_update(
                leaderboard=[entry.model_dump() for entry in leaderboard],
                cycle_number=cycle_number,
            )

            # Create cycle result
            cycle_time = time.time() - cycle_start
            cycle_result = CycleResult(
                cycle_number=cycle_number,
                market_data=market,
                decisions=decisions,
                cycle_time_seconds=cycle_time,
                errors=errors_list,
            )
            self._cycle_results.append(cycle_result)

            yield cycle_completed(cycle_number, cycle_time)
            yield done()

        except Exception as e:
            logger.exception(f"Cycle {cycle_number} failed: {e}")
            yield error(f"Cycle failed: {str(e)}")

    async def _create_snapshots(self, cycle_number: int, current_price: float):
        """Create performance snapshots for all models."""
        symbol = self.config.symbols[0] if self.config.symbols else "BTC/USD"

        for model_name, engine in self._engines.items():
            portfolio = engine.get_portfolio_state({symbol: current_price})

            snapshot = PerformanceSnapshot(
                id=generate_snapshot_id(self.competition_id, model_name, cycle_number),
                competition_id=self.competition_id,
                cycle_number=cycle_number,
                model_name=model_name,
                portfolio_value=portfolio.portfolio_value,
                cash=portfolio.cash,
                pnl=portfolio.total_pnl,
                return_pct=((portfolio.portfolio_value / self.config.initial_capital) - 1) * 100,
                positions_count=len(portfolio.positions),
                trades_count=portfolio.trades_count,
            )
            self._snapshots.append(snapshot)

    def _calculate_leaderboard(self, prices: Dict[str, float]) -> List[LeaderboardEntry]:
        """Calculate current leaderboard."""
        entries = []

        for model_name, engine in self._engines.items():
            portfolio = engine.get_portfolio_state(prices)

            return_pct = ((portfolio.portfolio_value / self.config.initial_capital) - 1) * 100

            entries.append(LeaderboardEntry(
                rank=0,  # Will be set after sorting
                model_name=model_name,
                portfolio_value=portfolio.portfolio_value,
                total_pnl=portfolio.total_pnl,
                return_pct=return_pct,
                trades_count=portfolio.trades_count,
                cash=portfolio.cash,
                positions_count=len(portfolio.positions),
            ))

        # Sort by portfolio value
        entries.sort(key=lambda x: x.portfolio_value, reverse=True)

        # Set ranks
        for i, entry in enumerate(entries):
            entry.rank = i + 1

        return entries

    async def start(self, api_keys: Dict[str, str]) -> bool:
        """Start the competition."""
        # Allow restarting from RUNNING state (e.g., after browser refresh)
        if self.status not in [CompetitionStatus.CREATED, CompetitionStatus.PAUSED, CompetitionStatus.RUNNING]:
            logger.warning(f"Cannot start competition in state: {self.status}")
            return False

        if not await self.initialize(api_keys):
            return False

        self.status = CompetitionStatus.RUNNING
        if self.start_time is None:
            self.start_time = datetime.now()
        self._is_running = True
        self._stop_requested = False

        logger.info(f"Competition {self.competition_id} started (status: {self.status})")
        return True

    async def stop(self):
        """Stop the competition."""
        self._stop_requested = True
        self._is_running = False
        self.status = CompetitionStatus.PAUSED
        logger.info(f"Competition {self.competition_id} stopped")

    async def complete(self):
        """Complete the competition."""
        self._stop_requested = True
        self._is_running = False
        self.status = CompetitionStatus.COMPLETED
        self.end_time = datetime.now()
        logger.info(f"Competition {self.competition_id} completed")

    def get_leaderboard(self) -> List[LeaderboardEntry]:
        """Get current leaderboard."""
        symbol = self.config.symbols[0] if self.config.symbols else "BTC/USD"
        # Use last known prices or estimate
        prices = {}
        if self._cycle_results:
            last_cycle = self._cycle_results[-1]
            prices = {symbol: last_cycle.market_data.price}
        else:
            # No cycles run yet - use initial capital as portfolio value
            # This allows showing a leaderboard even before the first cycle
            prices = {symbol: 0.0}  # Price doesn't matter for initial state
        return self._calculate_leaderboard(prices)

    def get_decisions(self, model_name: Optional[str] = None, limit: int = 50) -> List[ModelDecision]:
        """Get recent decisions."""
        decisions = self._decisions
        if model_name:
            decisions = [d for d in decisions if d.model_name == model_name]
        return decisions[-limit:]

    def get_snapshots(self, model_name: Optional[str] = None) -> List[PerformanceSnapshot]:
        """Get performance snapshots."""
        snapshots = self._snapshots
        if model_name:
            snapshots = [s for s in snapshots if s.model_name == model_name]
        return snapshots

    def get_portfolio_states(self) -> Dict[str, PortfolioState]:
        """Get current portfolio states for all models."""
        symbol = self.config.symbols[0] if self.config.symbols else "BTC/USD"
        prices = {}
        if self._cycle_results:
            last_cycle = self._cycle_results[-1]
            prices = {symbol: last_cycle.market_data.price}

        return {
            name: engine.get_portfolio_state(prices)
            for name, engine in self._engines.items()
        }

    def to_dict(self) -> Dict[str, Any]:
        """Convert competition state to dictionary."""
        return {
            "competition_id": self.competition_id,
            "config": self.config.model_dump(),
            "status": self.status.value,
            "cycle_count": self.cycle_count,
            "start_time": self.start_time.isoformat() if self.start_time else None,
            "end_time": self.end_time.isoformat() if self.end_time else None,
            "leaderboard": [e.model_dump() for e in self.get_leaderboard()],
        }


# Competition registry
_competitions: Dict[str, AlphaArenaCompetition] = {}


def get_competition(competition_id: str) -> Optional[AlphaArenaCompetition]:
    """Get a competition by ID."""
    return _competitions.get(competition_id)


def create_competition(config: CompetitionConfig) -> AlphaArenaCompetition:
    """Create a new competition."""
    competition = AlphaArenaCompetition(config)
    _competitions[config.competition_id] = competition
    return competition


def list_competitions() -> List[AlphaArenaCompetition]:
    """List all competitions."""
    return list(_competitions.values())


def delete_competition(competition_id: str) -> bool:
    """Delete a competition."""
    if competition_id in _competitions:
        del _competitions[competition_id]
        return True
    return False
