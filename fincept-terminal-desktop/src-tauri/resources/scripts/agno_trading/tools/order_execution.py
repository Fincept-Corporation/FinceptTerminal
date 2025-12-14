"""
Order Execution Tools

Provides tools for executing trades. Currently supports paper trading mode.
"""

from typing import List, Dict, Any, Optional
from datetime import datetime
import uuid


class PaperTradingEngine:
    """Simple paper trading engine for testing"""

    def __init__(self, initial_capital: float = 10000.0):
        self.initial_capital = initial_capital
        self.cash = initial_capital
        self.positions: Dict[str, Dict[str, Any]] = {}
        self.orders: List[Dict[str, Any]] = []
        self.trades: List[Dict[str, Any]] = []

    def get_portfolio_value(self, current_prices: Dict[str, float]) -> float:
        """Calculate total portfolio value"""
        portfolio_value = self.cash

        for symbol, position in self.positions.items():
            if symbol in current_prices:
                portfolio_value += position['quantity'] * current_prices[symbol]

        return portfolio_value


def place_order(
    symbol: str,
    side: str,
    order_type: str,
    quantity: float,
    price: Optional[float] = None,
    mode: str = "paper"
) -> Dict[str, Any]:
    """
    Place a trading order

    Args:
        symbol: Trading symbol
        side: 'buy' or 'sell'
        order_type: 'market' or 'limit'
        quantity: Order quantity
        price: Limit price (for limit orders)
        mode: 'paper' or 'live' (currently only paper supported)

    Returns:
        Order confirmation
    """
    if mode != "paper":
        return {"error": "Only paper trading mode is currently supported"}

    order_id = str(uuid.uuid4())

    order = {
        "order_id": order_id,
        "symbol": symbol,
        "side": side.lower(),
        "type": order_type.lower(),
        "quantity": quantity,
        "price": price,
        "status": "submitted",
        "timestamp": datetime.now().isoformat(),
        "mode": mode
    }

    return {
        "success": True,
        "order": order,
        "message": f"Paper trading order placed: {side.upper()} {quantity} {symbol}"
    }


def cancel_order(order_id: str, mode: str = "paper") -> Dict[str, Any]:
    """Cancel an order"""
    return {
        "success": True,
        "order_id": order_id,
        "status": "cancelled",
        "message": "Order cancelled (paper trading)"
    }


def get_order_status(order_id: str, mode: str = "paper") -> Dict[str, Any]:
    """Get order status"""
    return {
        "order_id": order_id,
        "status": "filled",
        "message": "Order status retrieved (paper trading)"
    }


def get_order_execution_tools() -> List[Any]:
    """Get order execution tools"""
    return [
        place_order,
        cancel_order,
        get_order_status,
    ]


TOOL_DESCRIPTIONS = {
    "place_order": "Place a buy or sell order for a symbol. Supports market and limit orders in paper trading mode.",
    "cancel_order": "Cancel a pending order by order ID.",
    "get_order_status": "Check the status of an order (pending, filled, cancelled).",
}
