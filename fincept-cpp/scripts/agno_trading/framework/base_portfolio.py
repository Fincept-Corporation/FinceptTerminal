"""
Base interface for portfolio services.
"""

from abc import ABC, abstractmethod
from typing import Dict
from .types import TxResult, PortfolioView


class BasePortfolioService(ABC):
    """
    Base class for portfolio management.

    Tracks cash, positions, realized/unrealized P&L.
    """

    @abstractmethod
    async def apply_trade(self, tx: TxResult) -> None:
        """
        Apply a completed trade to the portfolio.

        Args:
            tx: Transaction result from execution gateway
        """
        pass

    @abstractmethod
    async def mark_to_market(self, prices: Dict[str, float]) -> None:
        """
        Update unrealized P&L based on current market prices.

        Args:
            prices: Dict of symbol -> current price
        """
        pass

    @abstractmethod
    async def get_portfolio_view(self) -> PortfolioView:
        """
        Get current portfolio snapshot.

        Returns:
            PortfolioView with cash, positions, and P&L
        """
        pass

    @abstractmethod
    async def reset(self, initial_capital: float) -> None:
        """
        Reset portfolio to initial state.

        Args:
            initial_capital: Starting cash balance
        """
        pass
