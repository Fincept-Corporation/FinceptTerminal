"""
Base interface for execution gateways.
"""

from abc import ABC, abstractmethod
from typing import List
from .types import TradeInstruction, TxResult, FeatureVector


class BaseExecutionGateway(ABC):
    """
    Base class for all execution gateways.

    An execution gateway takes trade instructions and executes them,
    either in paper trading mode or against a live exchange.
    """

    @abstractmethod
    async def execute(
        self,
        instructions: List[TradeInstruction],
        market_features: List[FeatureVector]
    ) -> List[TxResult]:
        """
        Execute trade instructions.

        Args:
            instructions: List of trades to execute
            market_features: Current market data (for paper trading price reference)

        Returns:
            List of transaction results
        """
        pass

    @abstractmethod
    async def test_connection(self) -> bool:
        """
        Test connection to execution venue.

        Returns:
            True if connected, False otherwise
        """
        pass

    @abstractmethod
    async def close(self) -> None:
        """
        Close connection and clean up resources.
        """
        pass
