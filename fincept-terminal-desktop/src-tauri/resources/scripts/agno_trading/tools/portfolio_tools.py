"""
Portfolio Management Tools

Provides tools for portfolio analysis and management.
"""

from typing import List, Dict, Any, Optional
from datetime import datetime
import math


def calculate_portfolio_metrics(
    positions: List[Dict[str, Any]],
    initial_capital: float = 10000.0
) -> Dict[str, Any]:
    """
    Calculate portfolio performance metrics

    Args:
        positions: List of positions with symbol, quantity, entry_price, current_price
        initial_capital: Initial portfolio capital

    Returns:
        Portfolio metrics including returns, sharpe ratio, etc.
    """
    try:
        total_value = 0
        total_pnl = 0
        total_pnl_pct = 0

        position_details = []

        for pos in positions:
            quantity = pos.get('quantity', 0)
            entry_price = pos.get('entry_price', 0)
            current_price = pos.get('current_price', 0)

            position_value = quantity * current_price
            position_cost = quantity * entry_price
            position_pnl = position_value - position_cost
            position_pnl_pct = (position_pnl / position_cost * 100) if position_cost > 0 else 0

            total_value += position_value
            total_pnl += position_pnl

            position_details.append({
                "symbol": pos.get('symbol'),
                "quantity": quantity,
                "entry_price": entry_price,
                "current_price": current_price,
                "value": position_value,
                "cost": position_cost,
                "pnl": position_pnl,
                "pnl_pct": position_pnl_pct
            })

        total_return_pct = (total_pnl / initial_capital * 100) if initial_capital > 0 else 0

        return {
            "total_value": total_value,
            "total_pnl": total_pnl,
            "total_return_pct": total_return_pct,
            "position_count": len(positions),
            "positions": position_details,
            "timestamp": datetime.now().isoformat()
        }

    except Exception as e:
        return {"error": f"Portfolio metrics calculation failed: {str(e)}"}


def calculate_position_size(
    account_balance: float,
    risk_per_trade: float,
    entry_price: float,
    stop_loss_price: float
) -> Dict[str, Any]:
    """
    Calculate optimal position size based on risk management

    Args:
        account_balance: Total account balance
        risk_per_trade: Risk percentage per trade (e.g., 0.02 for 2%)
        entry_price: Planned entry price
        stop_loss_price: Stop loss price

    Returns:
        Recommended position size
    """
    try:
        risk_amount = account_balance * risk_per_trade
        price_risk = abs(entry_price - stop_loss_price)

        if price_risk == 0:
            return {"error": "Entry price and stop loss cannot be the same"}

        position_size = risk_amount / price_risk
        position_value = position_size * entry_price
        position_pct = (position_value / account_balance * 100) if account_balance > 0 else 0

        return {
            "position_size": position_size,
            "position_value": position_value,
            "position_pct": position_pct,
            "risk_amount": risk_amount,
            "risk_pct": risk_per_trade * 100,
            "entry_price": entry_price,
            "stop_loss_price": stop_loss_price,
            "price_risk": price_risk
        }

    except Exception as e:
        return {"error": f"Position size calculation failed: {str(e)}"}


def analyze_portfolio_risk(
    positions: List[Dict[str, Any]],
    correlations: Optional[Dict[str, float]] = None
) -> Dict[str, Any]:
    """
    Analyze portfolio risk and diversification

    Args:
        positions: List of positions
        correlations: Optional correlation matrix

    Returns:
        Risk analysis
    """
    try:
        total_value = sum(pos.get('value', 0) for pos in positions)

        # Calculate concentration
        concentrations = []
        for pos in positions:
            value = pos.get('value', 0)
            concentration = (value / total_value * 100) if total_value > 0 else 0
            concentrations.append({
                "symbol": pos.get('symbol'),
                "value": value,
                "concentration_pct": concentration
            })

        # Find max concentration
        max_concentration = max(concentrations, key=lambda x: x['concentration_pct']) if concentrations else None

        # Assess diversification
        diversification_score = 0
        if len(positions) >= 10:
            diversification_score = 100
        elif len(positions) >= 5:
            diversification_score = 75
        elif len(positions) >= 3:
            diversification_score = 50
        else:
            diversification_score = 25

        return {
            "total_value": total_value,
            "position_count": len(positions),
            "concentrations": concentrations,
            "max_concentration": max_concentration,
            "diversification_score": diversification_score,
            "risk_level": "High" if (max_concentration and max_concentration['concentration_pct'] > 30) else "Medium" if (max_concentration and max_concentration['concentration_pct'] > 20) else "Low"
        }

    except Exception as e:
        return {"error": f"Risk analysis failed: {str(e)}"}


def get_portfolio_tools() -> List[Any]:
    """Get portfolio management tools"""
    return [
        calculate_portfolio_metrics,
        calculate_position_size,
        analyze_portfolio_risk,
    ]


TOOL_DESCRIPTIONS = {
    "calculate_portfolio_metrics": "Calculate comprehensive portfolio metrics including total value, P&L, and returns.",
    "calculate_position_size": "Calculate optimal position size based on risk management rules and stop loss.",
    "analyze_portfolio_risk": "Analyze portfolio risk, concentration, and diversification levels.",
}
