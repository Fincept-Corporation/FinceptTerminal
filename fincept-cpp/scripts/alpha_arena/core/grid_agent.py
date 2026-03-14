"""
Grid Strategy Agent for Alpha Arena

Implements rule-based grid trading strategy that doesn't require LLM.
Uses the same grid calculation logic as the TypeScript GridEngine.
"""

from typing import Dict, List, Optional, Any, AsyncGenerator
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum

from alpha_arena.core.base_agent import BaseTradingAgent
from alpha_arena.types.models import (
    MarketData,
    ModelDecision,
    PortfolioState,
    TradeAction,
)
from alpha_arena.types.responses import (
    StreamResponse,
    decision_started,
    decision_completed,
    reasoning,
    done,
)
from alpha_arena.utils.logging import get_logger

logger = get_logger("grid_agent")


class GridType(Enum):
    ARITHMETIC = "arithmetic"  # Equal price gaps
    GEOMETRIC = "geometric"    # Equal percentage gaps


class GridDirection(Enum):
    NEUTRAL = "neutral"  # Buy below, sell above
    LONG = "long"        # Bias towards buying
    SHORT = "short"      # Bias towards selling


@dataclass
class GridConfig:
    """Grid strategy configuration"""
    upper_price: float
    lower_price: float
    grid_levels: int = 10
    grid_type: GridType = GridType.ARITHMETIC
    direction: GridDirection = GridDirection.NEUTRAL
    total_investment: float = 10000.0
    quantity_per_grid: Optional[float] = None
    stop_loss: Optional[float] = None
    take_profit: Optional[float] = None


@dataclass
class GridLevel:
    """A single grid level"""
    index: int
    price: float
    side: str  # "buy" or "sell"
    quantity: float
    status: str = "pending"  # pending, triggered, filled
    triggered_at: Optional[datetime] = None


@dataclass
class GridState:
    """Current state of the grid strategy"""
    config: GridConfig
    levels: List[GridLevel]
    current_price: float
    position: float = 0.0
    average_entry: float = 0.0
    realized_pnl: float = 0.0
    grid_profit: float = 0.0
    total_buys: int = 0
    total_sells: int = 0
    last_triggered_index: Optional[int] = None


class GridStrategyAgent(BaseTradingAgent):
    """
    Grid Trading Strategy Agent.

    A rule-based agent that:
    1. Creates a grid of buy/sell levels
    2. Triggers orders when price crosses levels
    3. Profits from price oscillation within the range

    This agent doesn't use LLM - it's purely algorithmic.
    """

    def __init__(
        self,
        name: str = "GridBot",
        config: Optional[GridConfig] = None,
        auto_adjust: bool = False,
    ):
        super().__init__(name=name, provider="rule_based", model_id="grid_v1")
        self.config = config
        self.state: Optional[GridState] = None
        self.auto_adjust = auto_adjust
        self._initialized = False

    async def initialize(self) -> bool:
        """Initialize the grid agent"""
        self._initialized = True
        logger.info(f"Grid agent '{self.name}' initialized")
        return True

    def configure(
        self,
        upper_price: float,
        lower_price: float,
        grid_levels: int = 10,
        grid_type: str = "arithmetic",
        direction: str = "neutral",
        total_investment: float = 10000.0,
        quantity_per_grid: Optional[float] = None,
        stop_loss: Optional[float] = None,
        take_profit: Optional[float] = None,
    ):
        """Configure the grid strategy"""
        self.config = GridConfig(
            upper_price=upper_price,
            lower_price=lower_price,
            grid_levels=grid_levels,
            grid_type=GridType(grid_type),
            direction=GridDirection(direction),
            total_investment=total_investment,
            quantity_per_grid=quantity_per_grid,
            stop_loss=stop_loss,
            take_profit=take_profit,
        )
        logger.info(f"Grid configured: {lower_price} - {upper_price}, {grid_levels} levels")

    def auto_configure(
        self,
        current_price: float,
        price_range_pct: float = 10.0,
        grid_levels: int = 10,
        total_investment: float = 10000.0,
    ):
        """Auto-configure grid based on current price"""
        half_range = current_price * (price_range_pct / 100) / 2
        self.configure(
            upper_price=current_price + half_range,
            lower_price=current_price - half_range,
            grid_levels=grid_levels,
            total_investment=total_investment,
        )
        logger.info(f"Grid auto-configured around ${current_price:,.2f}")

    def _calculate_grid_levels(self, current_price: float) -> List[GridLevel]:
        """Calculate grid price levels"""
        if not self.config:
            raise ValueError("Grid not configured")

        cfg = self.config
        levels = []

        # Calculate price levels
        if cfg.grid_type == GridType.ARITHMETIC:
            step = (cfg.upper_price - cfg.lower_price) / (cfg.grid_levels - 1)
            prices = [cfg.lower_price + step * i for i in range(cfg.grid_levels)]
        else:  # GEOMETRIC
            ratio = (cfg.upper_price / cfg.lower_price) ** (1 / (cfg.grid_levels - 1))
            prices = [cfg.lower_price * (ratio ** i) for i in range(cfg.grid_levels)]

        # Calculate quantity per grid
        avg_price = (cfg.upper_price + cfg.lower_price) / 2
        qty = cfg.quantity_per_grid or (cfg.total_investment / cfg.grid_levels / avg_price)

        # Create levels with buy/sell assignments
        for i, price in enumerate(prices):
            if cfg.direction == GridDirection.NEUTRAL:
                side = "buy" if price < current_price else "sell"
            elif cfg.direction == GridDirection.LONG:
                side = "buy" if price <= current_price else "sell"
            else:  # SHORT
                side = "sell" if price >= current_price else "buy"

            levels.append(GridLevel(
                index=i,
                price=round(price, 8),
                side=side,
                quantity=round(qty, 8),
                status="pending",
            ))

        return levels

    def _initialize_state(self, current_price: float):
        """Initialize grid state"""
        levels = self._calculate_grid_levels(current_price)
        self.state = GridState(
            config=self.config,
            levels=levels,
            current_price=current_price,
        )

    def _check_triggered_levels(self, new_price: float) -> List[GridLevel]:
        """Check which levels were triggered by price movement"""
        if not self.state:
            return []

        triggered = []
        old_price = self.state.current_price

        for level in self.state.levels:
            if level.status != "pending":
                continue

            # Check if price crossed this level
            crossed = False
            if old_price <= level.price <= new_price:
                crossed = True
            elif old_price >= level.price >= new_price:
                crossed = True

            if crossed:
                level.status = "triggered"
                level.triggered_at = datetime.now()
                triggered.append(level)

        return triggered

    def _process_triggered_level(self, level: GridLevel, price: float):
        """Process a triggered grid level"""
        if level.side == "buy":
            # Buying
            cost = level.quantity * price
            old_position = self.state.position
            self.state.position += level.quantity
            self.state.total_buys += 1

            # Update average entry
            if self.state.position > 0:
                if old_position > 0:
                    old_value = old_position * self.state.average_entry
                    new_value = level.quantity * price
                    self.state.average_entry = (old_value + new_value) / self.state.position
                else:
                    self.state.average_entry = price
        else:
            # Selling
            if self.state.position > 0 and self.state.average_entry > 0:
                # Realize profit
                pnl = (price - self.state.average_entry) * level.quantity
                self.state.realized_pnl += pnl
                self.state.grid_profit += pnl

            self.state.position -= level.quantity
            self.state.total_sells += 1

        level.status = "filled"
        self.state.last_triggered_index = level.index

        # Flip the level for next trigger
        level.side = "sell" if level.side == "buy" else "buy"
        level.status = "pending"

    async def make_decision(
        self,
        market_data: MarketData,
        portfolio: PortfolioState,
        cycle_number: int,
    ) -> ModelDecision:
        """Make trading decision based on grid logic"""

        price = market_data.price
        symbol = market_data.symbol

        # Auto-configure if not set
        if not self.config:
            self.auto_configure(price)

        # Initialize state if needed
        if not self.state:
            self._initialize_state(price)

        # Check stop conditions
        if self.config.stop_loss and price <= self.config.stop_loss:
            return ModelDecision(
                competition_id="",
                model_name=self.name,
                cycle_number=cycle_number,
                action=TradeAction.SELL,
                symbol=symbol,
                quantity=self.state.position if self.state.position > 0 else 0,
                confidence=1.0,
                reasoning="Stop loss triggered - closing all positions",
                price_at_decision=price,
            )

        if self.config.take_profit and price >= self.config.take_profit:
            return ModelDecision(
                competition_id="",
                model_name=self.name,
                cycle_number=cycle_number,
                action=TradeAction.SELL,
                symbol=symbol,
                quantity=self.state.position if self.state.position > 0 else 0,
                confidence=1.0,
                reasoning="Take profit triggered - closing all positions",
                price_at_decision=price,
            )

        # Check for triggered levels
        triggered = self._check_triggered_levels(price)
        self.state.current_price = price

        if not triggered:
            # No levels triggered - hold
            return ModelDecision(
                competition_id="",
                model_name=self.name,
                cycle_number=cycle_number,
                action=TradeAction.HOLD,
                symbol=symbol,
                quantity=0,
                confidence=0.5,
                reasoning=f"Price ${price:,.2f} - no grid levels triggered. Position: {self.state.position:.4f}",
                price_at_decision=price,
            )

        # Process triggered levels
        for level in triggered:
            self._process_triggered_level(level, price)

        # Determine aggregate action
        net_buys = sum(1 for l in triggered if l.side == "sell")  # Flipped, so original was buy
        net_sells = sum(1 for l in triggered if l.side == "buy")  # Flipped, so original was sell
        total_qty = sum(l.quantity for l in triggered)

        if net_buys > net_sells:
            action = TradeAction.BUY
            reasoning = f"Grid BUY: {net_buys} buy level(s) triggered at ${price:,.2f}"
        elif net_sells > net_buys:
            action = TradeAction.SELL
            reasoning = f"Grid SELL: {net_sells} sell level(s) triggered at ${price:,.2f}"
        else:
            action = TradeAction.HOLD
            reasoning = f"Grid: Equal buy/sell levels triggered at ${price:,.2f}"

        reasoning += f" | Grid P&L: ${self.state.grid_profit:+,.2f} | Position: {self.state.position:.4f}"

        return ModelDecision(
            competition_id="",
            model_name=self.name,
            cycle_number=cycle_number,
            action=action,
            symbol=symbol,
            quantity=total_qty,
            confidence=0.9,
            reasoning=reasoning,
            price_at_decision=price,
        )

    async def stream(
        self,
        query: str,
        competition_id: str,
        cycle_number: int,
        dependencies: Optional[Dict] = None,
    ) -> AsyncGenerator[StreamResponse, None]:
        """Stream decision process"""
        dependencies = dependencies or {}
        market_data = dependencies.get("market_data")
        portfolio = dependencies.get("portfolio")

        if not market_data or not portfolio:
            yield StreamResponse(content="Missing market_data or portfolio", event="error")
            return

        yield decision_started(self.name, market_data.symbol)

        decision = await self.make_decision(market_data, portfolio, cycle_number)

        yield reasoning(self.name, decision.reasoning)

        yield decision_completed(
            model_name=self.name,
            action=decision.action.value,
            symbol=decision.symbol,
            quantity=decision.quantity,
            confidence=decision.confidence,
            reasoning=decision.reasoning,
        )

        yield done()

    def get_grid_status(self) -> Dict[str, Any]:
        """Get current grid status for UI display"""
        if not self.state:
            return {"status": "not_initialized"}

        return {
            "status": "active",
            "config": {
                "upper_price": self.config.upper_price,
                "lower_price": self.config.lower_price,
                "grid_levels": self.config.grid_levels,
                "grid_type": self.config.grid_type.value,
                "direction": self.config.direction.value,
            },
            "current_price": self.state.current_price,
            "position": self.state.position,
            "average_entry": self.state.average_entry,
            "realized_pnl": self.state.realized_pnl,
            "grid_profit": self.state.grid_profit,
            "total_buys": self.state.total_buys,
            "total_sells": self.state.total_sells,
            "levels": [
                {
                    "index": l.index,
                    "price": l.price,
                    "side": l.side,
                    "quantity": l.quantity,
                    "status": l.status,
                }
                for l in self.state.levels
            ],
        }

    def reset(self):
        """Reset grid state"""
        self.state = None
        logger.info(f"Grid agent '{self.name}' reset")
