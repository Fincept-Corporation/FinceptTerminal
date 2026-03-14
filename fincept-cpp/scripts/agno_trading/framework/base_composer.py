"""
Base interface for decision composers.
"""

from abc import ABC, abstractmethod
from .types import ComposeContext, ComposeResult


class BaseComposer(ABC):
    """
    Base class for all composers.

    A composer takes market context and generates trade instructions.
    Different implementations can use:
    - LLM-based decisions (GPT-4, Claude, etc.)
    - Rule-based strategies (grid trading, momentum, mean reversion)
    - ML models (LSTM, transformers, reinforcement learning)
    """

    @abstractmethod
    async def compose(self, context: ComposeContext) -> ComposeResult:
        """
        Generate trading instructions based on context.

        Args:
            context: Market data, portfolio state, and historical performance

        Returns:
            ComposeResult with trade instructions
        """
        pass

    async def validate_instructions(self, result: ComposeResult) -> ComposeResult:
        """
        Optional validation hook for generated instructions.
        Can apply guardrails, risk limits, etc.

        Override in subclasses if needed.
        """
        return result
