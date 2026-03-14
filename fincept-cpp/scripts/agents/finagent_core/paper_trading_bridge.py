"""
Paper Trading Bridge - Connects Alpha Arena to Rust paper trading module

Provides Python interface to the Rust paper trading commands via Tauri IPC.
Used by AI agents to execute simulated trades.
"""

from typing import Dict, Any, Optional, List
from dataclasses import dataclass, asdict
from datetime import datetime
import json
import logging
import subprocess
import os

logger = logging.getLogger(__name__)


@dataclass
class Portfolio:
    """Paper trading portfolio"""
    id: str
    name: str
    initial_balance: float
    balance: float
    currency: str = "USD"
    leverage: float = 1.0
    margin_mode: str = "cross"
    fee_rate: float = 0.001
    created_at: str = ""


@dataclass
class Order:
    """Paper trading order"""
    id: str
    portfolio_id: str
    symbol: str
    side: str  # "buy" | "sell"
    order_type: str  # "market" | "limit" | "stop" | "stop_limit"
    quantity: float
    price: Optional[float] = None
    stop_price: Optional[float] = None
    filled_qty: float = 0
    avg_price: Optional[float] = None
    status: str = "pending"
    reduce_only: bool = False
    created_at: str = ""
    filled_at: Optional[str] = None


@dataclass
class Position:
    """Paper trading position"""
    id: str
    portfolio_id: str
    symbol: str
    side: str  # "long" | "short"
    quantity: float
    entry_price: float
    current_price: float
    unrealized_pnl: float = 0
    realized_pnl: float = 0
    leverage: float = 1.0
    liquidation_price: Optional[float] = None
    opened_at: str = ""


@dataclass
class Trade:
    """Executed trade"""
    id: str
    portfolio_id: str
    order_id: str
    symbol: str
    side: str
    price: float
    quantity: float
    fee: float
    pnl: float
    timestamp: str


@dataclass
class Stats:
    """Portfolio statistics"""
    total_pnl: float
    win_rate: float
    total_trades: int
    winning_trades: int
    losing_trades: int
    largest_win: float
    largest_loss: float


class PaperTradingBridge:
    """
    Bridge to Rust paper trading module.

    In Tauri context, this calls the pt_* commands.
    For testing/standalone, can simulate locally.
    """

    def __init__(self, use_tauri: bool = True):
        """
        Initialize bridge.

        Args:
            use_tauri: If True, calls Rust commands. If False, uses local simulation.
        """
        self.use_tauri = use_tauri
        self._local_portfolios: Dict[str, Portfolio] = {}
        self._local_orders: Dict[str, Order] = {}
        self._local_positions: Dict[str, Position] = {}
        self._local_trades: List[Trade] = []

    # =========================================================================
    # Portfolio Operations
    # =========================================================================

    def create_portfolio(
        self,
        name: str,
        balance: float = 100000.0,
        currency: str = "USD",
        leverage: float = 1.0,
        margin_mode: str = "cross",
        fee_rate: float = 0.001
    ) -> Portfolio:
        """Create a new paper trading portfolio"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_create_portfolio", {
                "name": name,
                "balance": balance,
                "currency": currency,
                "leverage": leverage,
                "margin_mode": margin_mode,
                "fee_rate": fee_rate
            })
            return Portfolio(**result)
        else:
            # Local simulation
            import uuid
            portfolio = Portfolio(
                id=str(uuid.uuid4()),
                name=name,
                initial_balance=balance,
                balance=balance,
                currency=currency,
                leverage=leverage,
                margin_mode=margin_mode,
                fee_rate=fee_rate,
                created_at=datetime.utcnow().isoformat()
            )
            self._local_portfolios[portfolio.id] = portfolio
            return portfolio

    def get_portfolio(self, portfolio_id: str) -> Optional[Portfolio]:
        """Get portfolio by ID"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_get_portfolio", {"id": portfolio_id})
            return Portfolio(**result) if result else None
        else:
            return self._local_portfolios.get(portfolio_id)

    def list_portfolios(self) -> List[Portfolio]:
        """List all portfolios"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_list_portfolios", {})
            return [Portfolio(**p) for p in result]
        else:
            return list(self._local_portfolios.values())

    def delete_portfolio(self, portfolio_id: str) -> bool:
        """Delete a portfolio"""
        if self.use_tauri:
            self._invoke_tauri("pt_delete_portfolio", {"id": portfolio_id})
            return True
        else:
            if portfolio_id in self._local_portfolios:
                del self._local_portfolios[portfolio_id]
                return True
            return False

    def reset_portfolio(self, portfolio_id: str) -> Portfolio:
        """Reset portfolio to initial state"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_reset_portfolio", {"id": portfolio_id})
            return Portfolio(**result)
        else:
            portfolio = self._local_portfolios.get(portfolio_id)
            if portfolio:
                portfolio.balance = portfolio.initial_balance
                # Clear positions and orders for this portfolio
                self._local_orders = {k: v for k, v in self._local_orders.items()
                                      if v.portfolio_id != portfolio_id}
                self._local_positions = {k: v for k, v in self._local_positions.items()
                                         if v.portfolio_id != portfolio_id}
                return portfolio
            raise ValueError(f"Portfolio not found: {portfolio_id}")

    # =========================================================================
    # Order Operations
    # =========================================================================

    def place_order(
        self,
        portfolio_id: str,
        symbol: str,
        side: str,
        order_type: str = "market",
        quantity: float = 1.0,
        price: Optional[float] = None,
        stop_price: Optional[float] = None,
        reduce_only: bool = False
    ) -> Order:
        """Place a paper trading order"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_place_order", {
                "portfolio_id": portfolio_id,
                "symbol": symbol,
                "side": side,
                "order_type": order_type,
                "quantity": quantity,
                "price": price,
                "stop_price": stop_price,
                "reduce_only": reduce_only
            })
            return Order(**result)
        else:
            import uuid
            order = Order(
                id=str(uuid.uuid4()),
                portfolio_id=portfolio_id,
                symbol=symbol,
                side=side,
                order_type=order_type,
                quantity=quantity,
                price=price,
                stop_price=stop_price,
                reduce_only=reduce_only,
                created_at=datetime.utcnow().isoformat()
            )
            self._local_orders[order.id] = order
            return order

    def cancel_order(self, order_id: str) -> bool:
        """Cancel an order"""
        if self.use_tauri:
            self._invoke_tauri("pt_cancel_order", {"order_id": order_id})
            return True
        else:
            order = self._local_orders.get(order_id)
            if order and order.status == "pending":
                order.status = "cancelled"
                return True
            return False

    def get_orders(self, portfolio_id: str, status: Optional[str] = None) -> List[Order]:
        """Get orders for portfolio"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_get_orders", {
                "portfolio_id": portfolio_id,
                "status": status
            })
            return [Order(**o) for o in result]
        else:
            orders = [o for o in self._local_orders.values()
                      if o.portfolio_id == portfolio_id]
            if status:
                orders = [o for o in orders if o.status == status]
            return orders

    def fill_order(
        self,
        order_id: str,
        fill_price: float,
        fill_qty: Optional[float] = None
    ) -> Trade:
        """Fill an order at given price"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_fill_order", {
                "order_id": order_id,
                "fill_price": fill_price,
                "fill_qty": fill_qty
            })
            return Trade(**result)
        else:
            order = self._local_orders.get(order_id)
            if not order:
                raise ValueError(f"Order not found: {order_id}")

            import uuid
            qty = fill_qty or (order.quantity - order.filled_qty)
            fee = qty * fill_price * 0.001

            trade = Trade(
                id=str(uuid.uuid4()),
                portfolio_id=order.portfolio_id,
                order_id=order_id,
                symbol=order.symbol,
                side=order.side,
                price=fill_price,
                quantity=qty,
                fee=fee,
                pnl=0,  # Would calculate based on position
                timestamp=datetime.utcnow().isoformat()
            )

            order.filled_qty += qty
            order.avg_price = fill_price
            order.status = "filled" if order.filled_qty >= order.quantity else "partial"
            order.filled_at = trade.timestamp

            self._local_trades.append(trade)
            return trade

    # =========================================================================
    # Position Operations
    # =========================================================================

    def get_positions(self, portfolio_id: str) -> List[Position]:
        """Get positions for portfolio"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_get_positions", {"portfolio_id": portfolio_id})
            return [Position(**p) for p in result]
        else:
            return [p for p in self._local_positions.values()
                    if p.portfolio_id == portfolio_id]

    def update_position_price(self, portfolio_id: str, symbol: str, price: float) -> bool:
        """Update current price for a position"""
        if self.use_tauri:
            self._invoke_tauri("pt_update_price", {
                "portfolio_id": portfolio_id,
                "symbol": symbol,
                "price": price
            })
            return True
        else:
            for pos in self._local_positions.values():
                if pos.portfolio_id == portfolio_id and pos.symbol == symbol:
                    pos.current_price = price
                    if pos.side == "long":
                        pos.unrealized_pnl = (price - pos.entry_price) * pos.quantity
                    else:
                        pos.unrealized_pnl = (pos.entry_price - price) * pos.quantity
                    return True
            return False

    # =========================================================================
    # Trade & Stats Operations
    # =========================================================================

    def get_trades(self, portfolio_id: str, limit: int = 50) -> List[Trade]:
        """Get trades for portfolio"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_get_trades", {
                "portfolio_id": portfolio_id,
                "limit": limit
            })
            return [Trade(**t) for t in result]
        else:
            trades = [t for t in self._local_trades if t.portfolio_id == portfolio_id]
            return trades[-limit:]

    def get_stats(self, portfolio_id: str) -> Stats:
        """Get portfolio statistics"""
        if self.use_tauri:
            result = self._invoke_tauri("pt_get_stats", {"portfolio_id": portfolio_id})
            return Stats(**result)
        else:
            trades = [t for t in self._local_trades if t.portfolio_id == portfolio_id]
            if not trades:
                return Stats(
                    total_pnl=0,
                    win_rate=0,
                    total_trades=0,
                    winning_trades=0,
                    losing_trades=0,
                    largest_win=0,
                    largest_loss=0
                )

            total_pnl = sum(t.pnl for t in trades)
            winning = [t for t in trades if t.pnl > 0]
            losing = [t for t in trades if t.pnl < 0]

            return Stats(
                total_pnl=total_pnl,
                win_rate=(len(winning) / len(trades) * 100) if trades else 0,
                total_trades=len(trades),
                winning_trades=len(winning),
                losing_trades=len(losing),
                largest_win=max((t.pnl for t in trades), default=0),
                largest_loss=min((t.pnl for t in trades), default=0)
            )

    # =========================================================================
    # Helper Methods
    # =========================================================================

    def _invoke_tauri(self, command: str, args: Dict[str, Any]) -> Any:
        """
        Invoke Tauri command.

        In actual Tauri context, this is handled via IPC.
        For Python-only execution, we simulate or raise.
        """
        # This would be called from Rust via tauri::invoke
        # For now, log and simulate
        logger.debug(f"Tauri invoke: {command} with args: {args}")

        # In real implementation, Rust calls Python with the result
        # For standalone Python testing, raise to indicate Tauri needed
        raise NotImplementedError(
            f"Tauri command {command} requires Rust runtime. "
            "Use use_tauri=False for local simulation."
        )


# =============================================================================
# Convenience Functions for Alpha Arena
# =============================================================================

_bridge: Optional[PaperTradingBridge] = None


def get_bridge(use_tauri: bool = False) -> PaperTradingBridge:
    """Get singleton bridge instance"""
    global _bridge
    if _bridge is None:
        _bridge = PaperTradingBridge(use_tauri=use_tauri)
    return _bridge


def execute_trade(
    portfolio_id: str,
    symbol: str,
    action: str,
    quantity: float,
    current_price: float
) -> Dict[str, Any]:
    """
    Execute a trade for Alpha Arena.

    Args:
        portfolio_id: Portfolio to trade in
        symbol: Symbol to trade
        action: "buy" or "sell"
        quantity: Quantity to trade
        current_price: Current market price

    Returns:
        Trade result dict
    """
    bridge = get_bridge()

    try:
        # Place and immediately fill market order
        order = bridge.place_order(
            portfolio_id=portfolio_id,
            symbol=symbol,
            side=action,
            order_type="market",
            quantity=quantity
        )

        trade = bridge.fill_order(order.id, current_price)

        return {
            "success": True,
            "order_id": order.id,
            "trade_id": trade.id,
            "symbol": symbol,
            "side": action,
            "quantity": quantity,
            "price": current_price,
            "pnl": trade.pnl,
            "fee": trade.fee
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "symbol": symbol,
            "action": action
        }


def get_portfolio_value(portfolio_id: str) -> Dict[str, Any]:
    """Get current portfolio value for Alpha Arena"""
    bridge = get_bridge()

    try:
        portfolio = bridge.get_portfolio(portfolio_id)
        positions = bridge.get_positions(portfolio_id)

        if not portfolio:
            return {"success": False, "error": "Portfolio not found"}

        positions_value = sum(p.quantity * p.current_price for p in positions)
        total_value = portfolio.balance + positions_value

        return {
            "success": True,
            "portfolio_id": portfolio_id,
            "cash": portfolio.balance,
            "positions_value": positions_value,
            "total_value": total_value,
            "initial_balance": portfolio.initial_balance,
            "total_pnl": total_value - portfolio.initial_balance,
            "return_pct": ((total_value / portfolio.initial_balance) - 1) * 100
        }

    except Exception as e:
        return {"success": False, "error": str(e)}


def get_positions_summary(portfolio_id: str) -> Dict[str, Any]:
    """Get positions summary for Alpha Arena"""
    bridge = get_bridge()

    try:
        positions = bridge.get_positions(portfolio_id)

        return {
            "success": True,
            "portfolio_id": portfolio_id,
            "positions_count": len(positions),
            "positions": [
                {
                    "symbol": p.symbol,
                    "side": p.side,
                    "quantity": p.quantity,
                    "entry_price": p.entry_price,
                    "current_price": p.current_price,
                    "unrealized_pnl": p.unrealized_pnl,
                    "return_pct": ((p.current_price / p.entry_price) - 1) * 100 if p.side == "long"
                                  else ((p.entry_price / p.current_price) - 1) * 100
                }
                for p in positions
            ]
        }

    except Exception as e:
        return {"success": False, "error": str(e)}
