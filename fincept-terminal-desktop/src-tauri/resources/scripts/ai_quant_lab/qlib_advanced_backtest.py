"""
AI Quant Lab - Advanced Backtest Module
Realistic backtesting with market microstructure:
- Order book simulation
- Market impact modeling
- Realistic slippage
- Transaction costs
- Execution algorithms (TWAP, VWAP, Limit orders)
- Liquidity constraints
"""

import json
import sys
from typing import Dict, List, Any, Optional, Union, Tuple
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

try:
    import pandas as pd
    import numpy as np
    from collections import deque
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False
    pd = None
    np = None


class OrderBook:
    """
    Simplified order book simulation.

    Models bid-ask spread and depth.
    """

    def __init__(self,
                 initial_mid_price: float,
                 spread_bps: float = 10.0,
                 depth: float = 1000000):
        """
        Args:
            initial_mid_price: Initial mid price
            spread_bps: Bid-ask spread in basis points
            depth: Liquidity depth at best bid/ask
        """
        self.mid_price = initial_mid_price
        self.spread_bps = spread_bps
        self.depth = depth

        # Calculate bid-ask
        spread = initial_mid_price * (spread_bps / 10000)
        self.best_bid = initial_mid_price - spread / 2
        self.best_ask = initial_mid_price + spread / 2

        # Order book levels (price -> quantity)
        self.bids = {}
        self.asks = {}

        self._initialize_book()

    def _initialize_book(self):
        """Initialize order book with realistic depth"""
        # 5 levels on each side
        tick_size = self.mid_price * 0.001  # 0.1% tick size

        for i in range(5):
            bid_price = self.best_bid - i * tick_size
            ask_price = self.best_ask + i * tick_size

            # Depth decreases with distance from mid
            level_depth = self.depth * (1 - i * 0.15)

            self.bids[bid_price] = level_depth
            self.asks[ask_price] = level_depth

    def get_execution_price(self,
                           side: str,
                           quantity: float,
                           aggressive: bool = True) -> Tuple[float, float]:
        """
        Get execution price for an order.

        Args:
            side: 'buy' or 'sell'
            quantity: Order quantity
            aggressive: If True, crosses spread (market order)

        Returns:
            (avg_execution_price, total_cost)
        """
        if side == 'buy':
            book = sorted(self.asks.items())  # Ascending price
            base_price = self.best_ask if aggressive else self.best_bid
        else:
            book = sorted(self.bids.items(), reverse=True)  # Descending price
            base_price = self.best_bid if aggressive else self.best_ask

        if not aggressive:
            # Passive order, no immediate execution
            return base_price, base_price * quantity

        # Walk through order book
        remaining = quantity
        total_cost = 0.0

        for price, available in book:
            if remaining <= 0:
                break

            fill_qty = min(remaining, available)
            total_cost += price * fill_qty
            remaining -= fill_qty

        # If not enough liquidity, use last price + penalty
        if remaining > 0:
            penalty_price = book[-1][0] * 1.01 if side == 'buy' else book[-1][0] * 0.99
            total_cost += penalty_price * remaining

        avg_price = total_cost / quantity
        return avg_price, total_cost

    def update_price(self, new_mid_price: float):
        """Update order book after price movement"""
        self.mid_price = new_mid_price
        spread = new_mid_price * (self.spread_bps / 10000)
        self.best_bid = new_mid_price - spread / 2
        self.best_ask = new_mid_price + spread / 2
        self._initialize_book()


class MarketImpactModel:
    """
    Market impact model based on academic research.

    Implements square-root impact law.
    """

    def __init__(self,
                 impact_coefficient: float = 0.1,
                 temporary_decay_rate: float = 0.5):
        """
        Args:
            impact_coefficient: Market impact sensitivity
            temporary_decay_rate: Rate of temporary impact decay
        """
        self.impact_coef = impact_coefficient
        self.decay_rate = temporary_decay_rate

    def calculate_impact(self,
                        order_size: float,
                        adv: float,  # Average daily volume
                        volatility: float,
                        participation_rate: Optional[float] = None) -> Dict[str, float]:
        """
        Calculate permanent and temporary market impact.

        Uses square-root impact model:
        Impact = σ * (size / ADV)^0.5

        Args:
            order_size: Order size in shares
            adv: Average daily volume
            volatility: Daily volatility
            participation_rate: Order size as % of volume

        Returns:
            Dict with permanent and temporary impact
        """
        if adv <= 0 or order_size <= 0:
            return {"permanent": 0.0, "temporary": 0.0, "total": 0.0}

        # Participation rate
        if participation_rate is None:
            participation_rate = order_size / adv

        # Square-root impact
        base_impact = volatility * np.sqrt(participation_rate)

        # Permanent impact (50% of total)
        permanent_impact = self.impact_coef * base_impact * 0.5

        # Temporary impact (50% of total, decays)
        temporary_impact = self.impact_coef * base_impact * 0.5

        total_impact = permanent_impact + temporary_impact

        return {
            "permanent": float(permanent_impact),
            "temporary": float(temporary_impact),
            "total": float(total_impact),
            "participation_rate": float(participation_rate),
            "base_impact_bps": float(base_impact * 10000)
        }

    def calculate_optimal_execution_time(self,
                                        order_size: float,
                                        adv: float,
                                        max_participation: float = 0.10) -> int:
        """
        Calculate optimal execution time to minimize impact.

        Args:
            order_size: Total order size
            adv: Average daily volume
            max_participation: Max participation rate (e.g., 0.10 = 10%)

        Returns:
            Number of time periods (intervals) to execute over
        """
        participation = order_size / adv

        if participation <= max_participation:
            return 1  # Execute immediately

        # Split into chunks
        num_periods = int(np.ceil(participation / max_participation))
        return num_periods


class SlippageModel:
    """
    Realistic slippage model.

    Combines bid-ask spread, market impact, and volatility.
    """

    def __init__(self,
                 base_spread_bps: float = 10.0,
                 volatility_factor: float = 0.5,
                 size_penalty_factor: float = 0.1):
        """
        Args:
            base_spread_bps: Base bid-ask spread
            volatility_factor: How much volatility affects slippage
            size_penalty_factor: Penalty for large orders
        """
        self.base_spread = base_spread_bps
        self.vol_factor = volatility_factor
        self.size_factor = size_penalty_factor

    def calculate_slippage(self,
                          order_size: float,
                          adv: float,
                          volatility: float,
                          order_type: str = "market") -> Dict[str, float]:
        """
        Calculate expected slippage.

        Args:
            order_size: Order size
            adv: Average daily volume
            volatility: Daily volatility
            order_type: 'market', 'limit', or 'midpoint'

        Returns:
            Slippage in basis points and dollars
        """
        # Base spread cost
        if order_type == "market":
            spread_cost = self.base_spread / 2  # Cross full spread
        elif order_type == "limit":
            spread_cost = 0  # Provide liquidity
        else:  # midpoint
            spread_cost = self.base_spread / 4

        # Volatility component
        vol_cost = self.vol_factor * volatility * 10000  # Convert to bps

        # Size component (square root of participation)
        participation = order_size / adv if adv > 0 else 1.0
        size_cost = self.size_factor * np.sqrt(participation) * 10000

        # Total slippage
        total_slippage_bps = spread_cost + vol_cost + size_cost

        return {
            "total_bps": float(total_slippage_bps),
            "spread_bps": float(spread_cost),
            "volatility_bps": float(vol_cost),
            "size_bps": float(size_cost),
            "participation_rate": float(participation)
        }


class ExecutionAlgorithm:
    """
    Execution algorithms for optimal trade execution.
    """

    def __init__(self, impact_model: MarketImpactModel):
        self.impact_model = impact_model

    def twap(self,
             total_quantity: float,
             num_intervals: int,
             interval_minutes: int = 5) -> List[Dict[str, Any]]:
        """
        Time-Weighted Average Price (TWAP) algorithm.

        Splits order equally across time intervals.

        Args:
            total_quantity: Total order size
            num_intervals: Number of execution intervals
            interval_minutes: Minutes per interval

        Returns:
            List of child orders
        """
        slice_size = total_quantity / num_intervals

        orders = []
        for i in range(num_intervals):
            orders.append({
                "interval": i,
                "quantity": slice_size,
                "execution_time": i * interval_minutes,
                "strategy": "TWAP",
                "aggressive": True  # Market orders
            })

        return orders

    def vwap(self,
             total_quantity: float,
             volume_profile: List[float]) -> List[Dict[str, Any]]:
        """
        Volume-Weighted Average Price (VWAP) algorithm.

        Allocates order proportional to expected volume.

        Args:
            total_quantity: Total order size
            volume_profile: Expected volume distribution (sums to 1.0)

        Returns:
            List of child orders
        """
        if abs(sum(volume_profile) - 1.0) > 0.01:
            # Normalize
            volume_profile = [v / sum(volume_profile) for v in volume_profile]

        orders = []
        for i, vol_pct in enumerate(volume_profile):
            orders.append({
                "interval": i,
                "quantity": total_quantity * vol_pct,
                "volume_participation": vol_pct,
                "strategy": "VWAP",
                "aggressive": True
            })

        return orders

    def implementation_shortfall(self,
                                 total_quantity: float,
                                 adv: float,
                                 urgency: float = 0.5) -> List[Dict[str, Any]]:
        """
        Implementation Shortfall algorithm.

        Balances market impact vs. timing risk.

        Args:
            total_quantity: Total order size
            adv: Average daily volume
            urgency: Urgency level (0=passive, 1=aggressive)

        Returns:
            Optimal execution schedule
        """
        # Calculate optimal execution rate based on urgency
        max_participation = 0.05 + urgency * 0.15  # 5-20%

        num_intervals = self.impact_model.calculate_optimal_execution_time(
            total_quantity, adv, max_participation
        )

        # Front-load if urgent
        if urgency > 0.7:
            # Execute 50% immediately, rest over time
            immediate_qty = total_quantity * 0.5
            remaining_qty = total_quantity * 0.5

            orders = [{
                "interval": 0,
                "quantity": immediate_qty,
                "strategy": "IS-Aggressive",
                "aggressive": True
            }]

            # Split remaining
            slice_size = remaining_qty / (num_intervals - 1) if num_intervals > 1 else remaining_qty
            for i in range(1, num_intervals):
                orders.append({
                    "interval": i,
                    "quantity": slice_size,
                    "strategy": "IS-Passive",
                    "aggressive": False
                })
        else:
            # Use TWAP-like distribution
            orders = self.twap(total_quantity, num_intervals)
            for order in orders:
                order["strategy"] = "IS-Balanced"

        return orders

    def pov(self,
            total_quantity: float,
            target_participation: float = 0.10,
            max_participation: float = 0.20) -> Dict[str, Any]:
        """
        Percentage of Volume (POV) algorithm.

        Maintains target participation rate.

        Args:
            total_quantity: Total order size
            target_participation: Target participation rate
            max_participation: Maximum allowed participation

        Returns:
            POV execution parameters
        """
        return {
            "strategy": "POV",
            "total_quantity": total_quantity,
            "target_participation": target_participation,
            "max_participation": max_participation,
            "description": f"Maintain {target_participation*100:.1f}% of volume"
        }


class AdvancedBacktestEngine:
    """
    Advanced backtesting engine with realistic execution simulation.
    """

    def __init__(self,
                 initial_capital: float = 1000000,
                 commission_bps: float = 5.0,
                 min_commission: float = 1.0):
        """
        Args:
            initial_capital: Starting capital
            commission_bps: Commission in basis points
            min_commission: Minimum commission per trade
        """
        self.initial_capital = initial_capital
        self.commission_bps = commission_bps
        self.min_commission = min_commission

        # Models
        self.impact_model = MarketImpactModel()
        self.slippage_model = SlippageModel()
        self.execution_algo = ExecutionAlgorithm(self.impact_model)

        # State
        self.cash = initial_capital
        self.positions = {}
        self.order_books = {}
        self.trade_history = []
        self.pnl_history = []

    def simulate_order(self,
                      symbol: str,
                      side: str,
                      quantity: float,
                      current_price: float,
                      adv: float,
                      volatility: float,
                      execution_strategy: str = "market",
                      urgency: float = 0.5) -> Dict[str, Any]:
        """
        Simulate realistic order execution.

        Args:
            symbol: Instrument symbol
            side: 'buy' or 'sell'
            quantity: Order quantity
            current_price: Current market price
            adv: Average daily volume
            volatility: Daily volatility
            execution_strategy: 'market', 'twap', 'vwap', 'is'
            urgency: Urgency level for IS algorithm

        Returns:
            Execution results
        """
        # Get or create order book
        if symbol not in self.order_books:
            self.order_books[symbol] = OrderBook(current_price)

        order_book = self.order_books[symbol]
        order_book.update_price(current_price)

        # Calculate market impact
        impact = self.impact_model.calculate_impact(
            quantity, adv, volatility
        )

        # Calculate slippage
        slippage = self.slippage_model.calculate_slippage(
            quantity, adv, volatility,
            order_type="market" if execution_strategy == "market" else "limit"
        )

        # Execute based on strategy
        if execution_strategy == "market":
            # Immediate execution
            exec_price, total_cost = order_book.get_execution_price(
                side, quantity, aggressive=True
            )

            # Add market impact
            if side == 'buy':
                exec_price *= (1 + impact['total'])
            else:
                exec_price *= (1 - impact['total'])

            executions = [{
                "quantity": quantity,
                "price": exec_price,
                "timestamp": datetime.now(),
                "strategy": "market"
            }]

        elif execution_strategy == "twap":
            # TWAP execution
            num_intervals = 10
            child_orders = self.execution_algo.twap(quantity, num_intervals)

            executions = []
            cumulative_impact = 0.0

            for child in child_orders:
                # Each slice experiences less impact
                child_qty = child["quantity"]
                child_impact = self.impact_model.calculate_impact(
                    child_qty, adv, volatility
                )

                exec_price = current_price * (1 + cumulative_impact)
                if side == 'buy':
                    exec_price *= (1 + child_impact['total'])
                else:
                    exec_price *= (1 - child_impact['total'])

                executions.append({
                    "quantity": child_qty,
                    "price": exec_price,
                    "timestamp": datetime.now(),
                    "strategy": "twap",
                    "interval": child["interval"]
                })

                cumulative_impact += child_impact['permanent']

        else:  # Implementation Shortfall
            child_orders = self.execution_algo.implementation_shortfall(
                quantity, adv, urgency
            )

            executions = []
            cumulative_impact = 0.0

            for child in child_orders:
                child_qty = child["quantity"]
                child_impact = self.impact_model.calculate_impact(
                    child_qty, adv, volatility
                )

                exec_price = current_price * (1 + cumulative_impact)
                if side == 'buy':
                    exec_price *= (1 + child_impact['total'])
                else:
                    exec_price *= (1 - child_impact['total'])

                executions.append({
                    "quantity": child_qty,
                    "price": exec_price,
                    "timestamp": datetime.now(),
                    "strategy": "is",
                    "aggressive": child["aggressive"]
                })

                cumulative_impact += child_impact['permanent']

        # Calculate total execution
        total_qty = sum(e["quantity"] for e in executions)
        total_cost = sum(e["quantity"] * e["price"] for e in executions)
        avg_price = total_cost / total_qty if total_qty > 0 else current_price

        # Commission
        commission = max(
            self.min_commission,
            total_cost * (self.commission_bps / 10000)
        )

        # Total slippage (realized)
        realized_slippage = abs(avg_price - current_price) / current_price * 10000

        return {
            "success": True,
            "symbol": symbol,
            "side": side,
            "quantity": total_qty,
            "avg_price": avg_price,
            "total_cost": total_cost,
            "commission": commission,
            "market_impact_bps": impact['total'] * 10000,
            "expected_slippage_bps": slippage['total_bps'],
            "realized_slippage_bps": realized_slippage,
            "execution_strategy": execution_strategy,
            "num_executions": len(executions),
            "executions": executions
        }

    def calculate_transaction_costs(self,
                                    trades: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Calculate total transaction costs for a set of trades.

        Args:
            trades: List of trade executions

        Returns:
            Breakdown of transaction costs
        """
        total_commission = sum(t.get("commission", 0) for t in trades)
        total_slippage = sum(
            t.get("total_cost", 0) * t.get("realized_slippage_bps", 0) / 10000
            for t in trades
        )
        total_impact = sum(
            t.get("total_cost", 0) * t.get("market_impact_bps", 0) / 10000
            for t in trades
        )

        total_costs = total_commission + total_slippage + total_impact
        total_traded = sum(t.get("total_cost", 0) for t in trades)

        return {
            "total_costs": total_costs,
            "commission": total_commission,
            "slippage": total_slippage,
            "market_impact": total_impact,
            "total_traded_value": total_traded,
            "cost_bps": (total_costs / total_traded * 10000) if total_traded > 0 else 0,
            "num_trades": len(trades)
        }


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "check_status":
            result = {
                "success": True,
                "pandas_available": PANDAS_AVAILABLE,
                "features": [
                    "Order book simulation",
                    "Market impact modeling",
                    "Slippage calculation",
                    "TWAP execution",
                    "VWAP execution",
                    "Implementation Shortfall",
                    "POV execution",
                    "Transaction cost analysis"
                ]
            }
            print(json.dumps(result))
        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
