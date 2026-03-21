"""
Paper Trading Engine

Simulates trade execution for competition models.
"""

from datetime import datetime
from typing import Dict, Optional, List
from decimal import Decimal, ROUND_DOWN

from alpha_arena.types.models import (
    MarketData,
    ModelDecision,
    PortfolioState,
    PositionInfo,
    TradeAction,
    TradeResult,
)
from alpha_arena.utils.logging import get_logger

logger = get_logger("paper_trading")


class PaperTradingEngine:
    """
    Paper trading engine for simulating trade execution.

    Handles:
    - Long and short positions
    - Position sizing and margin
    - P&L calculation
    - Portfolio state management
    """

    def __init__(
        self,
        model_name: str,
        initial_capital: float = 10000.0,
        maker_fee: float = 0.001,
        taker_fee: float = 0.002,
        margin_requirement: float = 0.3,
        max_position_pct: float = 0.9,
    ):
        self.model_name = model_name
        self.initial_capital = initial_capital
        self.maker_fee = maker_fee
        self.taker_fee = taker_fee
        self.margin_requirement = margin_requirement
        self.max_position_pct = max_position_pct

        # Portfolio state
        self.cash = initial_capital
        self.positions: Dict[str, PositionInfo] = {}
        self.realized_pnl = 0.0
        self.trades_count = 0
        self.trade_history: List[TradeResult] = []

    def get_portfolio_state(self, current_prices: Dict[str, float] = None) -> PortfolioState:
        """
        Get current portfolio state.

        Args:
            current_prices: Current prices for position valuation

        Returns:
            PortfolioState with current values
        """
        current_prices = current_prices or {}

        # Calculate unrealized P&L
        unrealized_pnl = 0.0
        positions_value = 0.0

        for symbol, position in self.positions.items():
            current_price = current_prices.get(symbol, position.entry_price)

            if position.side == "long":
                position_value = position.quantity * current_price
                position_pnl = (current_price - position.entry_price) * position.quantity
            else:  # short
                position_value = position.margin_reserved or 0
                position_pnl = (position.entry_price - current_price) * position.quantity

            positions_value += position_value
            unrealized_pnl += position_pnl

            # Update position's unrealized P&L
            position.unrealized_pnl = position_pnl

        portfolio_value = self.cash + positions_value + unrealized_pnl

        return PortfolioState(
            model_name=self.model_name,
            cash=self.cash,
            portfolio_value=portfolio_value,
            positions=self.positions.copy(),
            total_pnl=self.realized_pnl + unrealized_pnl,
            unrealized_pnl=unrealized_pnl,
            trades_count=self.trades_count,
        )

    def execute_decision(
        self,
        decision: ModelDecision,
        market_data: MarketData,
    ) -> TradeResult:
        """
        Execute a trading decision.

        Args:
            decision: The trading decision to execute
            market_data: Current market data

        Returns:
            TradeResult with execution details
        """
        symbol = decision.symbol
        action = decision.action
        requested_quantity = decision.quantity

        # Validate action
        if isinstance(action, str):
            try:
                action = TradeAction(action.lower())
            except ValueError:
                return TradeResult(
                    status="rejected",
                    action=TradeAction.HOLD,
                    symbol=symbol,
                    quantity=0,
                    price=market_data.price,
                    reason=f"Invalid action: {action}",
                )

        # Handle HOLD
        if action == TradeAction.HOLD:
            return TradeResult(
                status="executed",
                action=TradeAction.HOLD,
                symbol=symbol,
                quantity=0,
                price=market_data.price,
                reason="Hold - no trade",
            )

        # Determine execution price (use ask for buys, bid for sells)
        if action == TradeAction.BUY:
            execution_price = market_data.ask
        else:
            execution_price = market_data.bid

        # Execute based on action
        if action == TradeAction.BUY:
            return self._execute_buy(symbol, requested_quantity, execution_price)
        elif action == TradeAction.SELL:
            return self._execute_sell(symbol, requested_quantity, execution_price)
        elif action == TradeAction.SHORT:
            return self._execute_short(symbol, requested_quantity, execution_price)
        else:
            return TradeResult(
                status="rejected",
                action=action,
                symbol=symbol,
                quantity=0,
                price=execution_price,
                reason=f"Unknown action: {action}",
            )

    def _execute_buy(
        self,
        symbol: str,
        quantity: float,
        price: float,
    ) -> TradeResult:
        """Execute a buy order."""
        # Check for existing short position
        if symbol in self.positions and self.positions[symbol].side == "short":
            return self._close_short(symbol, quantity, price)

        # Calculate cost
        cost = quantity * price
        fee = cost * self.taker_fee
        total_cost = cost + fee

        # Check if we have enough cash
        if total_cost > self.cash:
            # Adjust quantity to fit available cash
            max_cost = self.cash * self.max_position_pct
            quantity = (max_cost / (1 + self.taker_fee)) / price
            quantity = float(Decimal(str(quantity)).quantize(Decimal('0.00001'), rounding=ROUND_DOWN))

            if quantity <= 0:
                return TradeResult(
                    status="rejected",
                    action=TradeAction.BUY,
                    symbol=symbol,
                    quantity=0,
                    price=price,
                    reason="Insufficient funds",
                )

            cost = quantity * price
            fee = cost * self.taker_fee
            total_cost = cost + fee

        # Execute trade
        self.cash -= total_cost

        if symbol in self.positions:
            # Add to existing position (average in)
            existing = self.positions[symbol]
            total_quantity = existing.quantity + quantity
            avg_price = (
                (existing.entry_price * existing.quantity) + (price * quantity)
            ) / total_quantity

            self.positions[symbol] = PositionInfo(
                symbol=symbol,
                quantity=total_quantity,
                entry_price=avg_price,
                side="long",
            )
        else:
            # New position
            self.positions[symbol] = PositionInfo(
                symbol=symbol,
                quantity=quantity,
                entry_price=price,
                side="long",
            )

        self.trades_count += 1

        result = TradeResult(
            status="executed",
            action=TradeAction.BUY,
            symbol=symbol,
            quantity=quantity,
            price=price,
            cost=total_cost,
        )
        self.trade_history.append(result)

        logger.info(f"{self.model_name}: BUY {quantity} {symbol} @ ${price:.2f} (cost: ${total_cost:.2f})")
        return result

    def _execute_sell(
        self,
        symbol: str,
        quantity: float,
        price: float,
    ) -> TradeResult:
        """Execute a sell order."""
        if symbol not in self.positions:
            # No position to sell - could open short
            return TradeResult(
                status="rejected",
                action=TradeAction.SELL,
                symbol=symbol,
                quantity=0,
                price=price,
                reason="No position to sell",
            )

        position = self.positions[symbol]

        if position.side == "short":
            # Close short position
            return self._close_short(symbol, quantity, price)

        # Sell long position
        sell_quantity = min(quantity, position.quantity)
        proceeds = sell_quantity * price
        fee = proceeds * self.taker_fee
        net_proceeds = proceeds - fee

        # Calculate P&L
        pnl = (price - position.entry_price) * sell_quantity - fee

        self.cash += net_proceeds
        self.realized_pnl += pnl

        if sell_quantity >= position.quantity:
            # Full close
            del self.positions[symbol]
        else:
            # Partial close
            self.positions[symbol] = PositionInfo(
                symbol=symbol,
                quantity=position.quantity - sell_quantity,
                entry_price=position.entry_price,
                side="long",
            )

        self.trades_count += 1

        result = TradeResult(
            status="executed",
            action=TradeAction.SELL,
            symbol=symbol,
            quantity=sell_quantity,
            price=price,
            proceeds=net_proceeds,
            pnl=pnl,
        )
        self.trade_history.append(result)

        logger.info(
            f"{self.model_name}: SELL {sell_quantity} {symbol} @ ${price:.2f} "
            f"(proceeds: ${net_proceeds:.2f}, P&L: ${pnl:+.2f})"
        )
        return result

    def _execute_short(
        self,
        symbol: str,
        quantity: float,
        price: float,
    ) -> TradeResult:
        """Execute a short sell order."""
        if symbol in self.positions:
            position = self.positions[symbol]
            if position.side == "long":
                # Close long first
                close_result = self._execute_sell(symbol, position.quantity, price)
                if close_result.status != "executed":
                    return close_result

        # Calculate margin required
        position_value = quantity * price
        margin_required = position_value * self.margin_requirement
        fee = position_value * self.taker_fee

        if margin_required + fee > self.cash:
            # Adjust quantity
            available_margin = self.cash * self.max_position_pct
            quantity = (available_margin / self.margin_requirement) / price
            quantity = float(Decimal(str(quantity)).quantize(Decimal('0.00001'), rounding=ROUND_DOWN))

            if quantity <= 0:
                return TradeResult(
                    status="rejected",
                    action=TradeAction.SHORT,
                    symbol=symbol,
                    quantity=0,
                    price=price,
                    reason="Insufficient margin",
                )

            position_value = quantity * price
            margin_required = position_value * self.margin_requirement
            fee = position_value * self.taker_fee

        # Execute short
        self.cash -= (margin_required + fee)

        self.positions[symbol] = PositionInfo(
            symbol=symbol,
            quantity=quantity,
            entry_price=price,
            side="short",
            margin_reserved=margin_required,
        )

        self.trades_count += 1

        result = TradeResult(
            status="executed",
            action=TradeAction.SHORT,
            symbol=symbol,
            quantity=quantity,
            price=price,
            margin_reserved=margin_required,
        )
        self.trade_history.append(result)

        logger.info(f"{self.model_name}: SHORT {quantity} {symbol} @ ${price:.2f} (margin: ${margin_required:.2f})")
        return result

    def _close_short(
        self,
        symbol: str,
        quantity: float,
        price: float,
    ) -> TradeResult:
        """Close a short position."""
        if symbol not in self.positions or self.positions[symbol].side != "short":
            return TradeResult(
                status="rejected",
                action=TradeAction.BUY,
                symbol=symbol,
                quantity=0,
                price=price,
                reason="No short position to close",
            )

        position = self.positions[symbol]
        close_quantity = min(quantity, position.quantity)

        # Calculate P&L (profit if price fell)
        pnl = (position.entry_price - price) * close_quantity

        # Return margin
        margin_return = (position.margin_reserved or 0) * (close_quantity / position.quantity)

        # Deduct cover cost
        cover_cost = close_quantity * price
        fee = cover_cost * self.taker_fee

        self.cash += margin_return + pnl - fee
        self.realized_pnl += pnl

        if close_quantity >= position.quantity:
            del self.positions[symbol]
        else:
            remaining_margin = (position.margin_reserved or 0) - margin_return
            self.positions[symbol] = PositionInfo(
                symbol=symbol,
                quantity=position.quantity - close_quantity,
                entry_price=position.entry_price,
                side="short",
                margin_reserved=remaining_margin,
            )

        self.trades_count += 1

        result = TradeResult(
            status="executed",
            action=TradeAction.BUY,
            symbol=symbol,
            quantity=close_quantity,
            price=price,
            pnl=pnl,
            reason="Closed short position",
        )
        self.trade_history.append(result)

        logger.info(
            f"{self.model_name}: CLOSE SHORT {close_quantity} {symbol} @ ${price:.2f} "
            f"(P&L: ${pnl:+.2f})"
        )
        return result

    def reset(self):
        """Reset the trading engine to initial state."""
        self.cash = self.initial_capital
        self.positions = {}
        self.realized_pnl = 0.0
        self.trades_count = 0
        self.trade_history = []
        logger.info(f"{self.model_name}: Trading engine reset")

    def get_trade_history(self, limit: int = 50) -> List[TradeResult]:
        """Get recent trade history."""
        return self.trade_history[-limit:]
