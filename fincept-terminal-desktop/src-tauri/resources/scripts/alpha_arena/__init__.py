"""
Alpha Arena - AI Trading Competition System

A production-ready multi-agent trading competition platform with:
- Async-first design
- Pydantic type validation
- A2A Protocol-inspired agent communication
- Streaming responses
- Comprehensive evaluation and tracing
"""

__version__ = "2.0.0"
__author__ = "Fincept Corporation"

from alpha_arena.types.models import (
    CompetitionConfig,
    CompetitionModel,
    CompetitionStatus,
    LeaderboardEntry,
    ModelDecision,
    TradeAction,
    TradeResult,
    PortfolioState,
    CycleResult,
)

from alpha_arena.types.responses import (
    StreamResponse,
    StreamResponseEvent,
    NotifyResponse,
    NotifyResponseEvent,
    BaseResponse,
)

from alpha_arena.core.base_agent import BaseAgent
from alpha_arena.core.competition import AlphaArenaCompetition

__all__ = [
    # Models
    "CompetitionConfig",
    "CompetitionModel",
    "CompetitionStatus",
    "LeaderboardEntry",
    "ModelDecision",
    "TradeAction",
    "TradeResult",
    "PortfolioState",
    "CycleResult",
    # Responses
    "StreamResponse",
    "StreamResponseEvent",
    "NotifyResponse",
    "NotifyResponseEvent",
    "BaseResponse",
    # Core
    "BaseAgent",
    "AlphaArenaCompetition",
]
