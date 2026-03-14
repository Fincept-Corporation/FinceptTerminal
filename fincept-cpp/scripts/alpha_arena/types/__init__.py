"""
Alpha Arena Type System

Pydantic-based type definitions for the entire Alpha Arena system.
"""

from alpha_arena.types.models import (
    CompetitionConfig,
    CompetitionModel,
    CompetitionStatus,
    CompetitionMode,
    LeaderboardEntry,
    ModelDecision,
    TradeAction,
    TradeResult,
    PortfolioState,
    PositionInfo,
    CycleResult,
    MarketData,
    AgentConfig,
)

from alpha_arena.types.responses import (
    StreamResponse,
    StreamResponseEvent,
    NotifyResponse,
    NotifyResponseEvent,
    TaskStatusEvent,
    CommonResponseEvent,
    BaseResponse,
    ConversationItem,
    Role,
)

__all__ = [
    # Core Models
    "CompetitionConfig",
    "CompetitionModel",
    "CompetitionStatus",
    "CompetitionMode",
    "LeaderboardEntry",
    "ModelDecision",
    "TradeAction",
    "TradeResult",
    "PortfolioState",
    "PositionInfo",
    "CycleResult",
    "MarketData",
    "AgentConfig",
    # Response Types
    "StreamResponse",
    "StreamResponseEvent",
    "NotifyResponse",
    "NotifyResponseEvent",
    "TaskStatusEvent",
    "CommonResponseEvent",
    "BaseResponse",
    "ConversationItem",
    "Role",
]
