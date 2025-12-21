"""
Base interface for features pipelines.
"""

from abc import ABC, abstractmethod
from .types import FeaturesResult


class BaseFeaturesPipeline(ABC):
    """
    Base class for feature extraction pipelines.

    A features pipeline fetches market data and computes technical indicators.
    """

    @abstractmethod
    async def build(self) -> FeaturesResult:
        """
        Build feature vectors from market data.

        Returns:
            FeaturesResult with computed features
        """
        pass

    @abstractmethod
    async def update_symbols(self, symbols: list[str]) -> None:
        """
        Update the list of symbols to track.

        Args:
            symbols: List of trading symbols
        """
        pass
