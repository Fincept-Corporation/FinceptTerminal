"""
Alpha Arena Response Types

Streaming and notification response types inspired by ValueCell's architecture.
Provides event-driven communication between agents and frontend.
"""

from abc import ABC
from enum import Enum
from typing import Any, Dict, Literal, Optional, Union
from datetime import datetime
from pydantic import BaseModel, Field

from alpha_arena.utils.uuid import generate_item_id


class Role(str, Enum):
    """Message role enumeration."""
    USER = "user"
    AGENT = "agent"
    SYSTEM = "system"


class SystemResponseEvent(str, Enum):
    """Events related to system-level responses and status updates."""
    COMPETITION_STARTED = "competition_started"
    COMPETITION_PAUSED = "competition_paused"
    COMPETITION_COMPLETED = "competition_completed"
    CYCLE_STARTED = "cycle_started"
    CYCLE_COMPLETED = "cycle_completed"
    SYSTEM_FAILED = "system_failed"
    DONE = "done"


class TaskStatusEvent(str, Enum):
    """Events related to task lifecycle status changes."""
    TASK_STARTED = "task_started"
    TASK_COMPLETED = "task_completed"
    TASK_FAILED = "task_failed"
    TASK_CANCELLED = "task_cancelled"


class CommonResponseEvent(str, Enum):
    """Common response events shared across different response types."""
    COMPONENT_GENERATOR = "component_generator"
    LEADERBOARD_UPDATE = "leaderboard_update"
    PORTFOLIO_UPDATE = "portfolio_update"


class StreamResponseEvent(str, Enum):
    """Events specific to streaming agent responses."""
    MESSAGE_CHUNK = "message_chunk"
    DECISION_STARTED = "decision_started"
    DECISION_COMPLETED = "decision_completed"
    TRADE_EXECUTED = "trade_executed"
    REASONING_STARTED = "reasoning_started"
    REASONING = "reasoning"
    REASONING_COMPLETED = "reasoning_completed"
    MARKET_DATA = "market_data"
    ERROR = "error"


class NotifyResponseEvent(str, Enum):
    """Events specific to notification agent responses."""
    MESSAGE = "message"
    ALERT = "alert"
    TRADE_NOTIFICATION = "trade_notification"
    PERFORMANCE_UPDATE = "performance_update"


class StreamResponse(BaseModel):
    """Response model for streaming agent responses.

    Used by agents that stream progress, decisions, reasoning, or
    component-generation updates. `event` determines how the response
    should be interpreted.
    """

    content: Optional[str] = Field(
        None,
        description="The content of the stream response",
    )
    event: Union[StreamResponseEvent, TaskStatusEvent, CommonResponseEvent, SystemResponseEvent] = Field(
        ...,
        description="The type of stream response",
    )
    metadata: Optional[Dict[str, Any]] = Field(
        None,
        description="Optional metadata providing additional context",
    )

    class Config:
        use_enum_values = True


class NotifyResponse(BaseModel):
    """Response model for notification agent responses."""

    content: str = Field(
        ...,
        description="The content of the notification response",
    )
    event: Union[NotifyResponseEvent, TaskStatusEvent, CommonResponseEvent] = Field(
        ...,
        description="The type of notification response",
    )
    metadata: Optional[Dict[str, Any]] = Field(
        None,
        description="Optional metadata",
    )

    class Config:
        use_enum_values = True


class ResponsePayload(BaseModel):
    """Base payload for response data."""

    content: Optional[str] = Field(None, description="The message content")


class DecisionPayload(ResponsePayload):
    """Payload for trading decision responses."""

    model_name: str = Field(..., description="Model making the decision")
    action: str = Field(..., description="Trading action")
    symbol: str = Field(..., description="Trading symbol")
    quantity: float = Field(0.0, description="Trade quantity")
    confidence: float = Field(0.5, description="Decision confidence")
    reasoning: str = Field("", description="Decision reasoning")


class TradePayload(ResponsePayload):
    """Payload for trade execution responses."""

    model_name: str = Field(..., description="Model that traded")
    action: str = Field(..., description="Trade action")
    symbol: str = Field(..., description="Trading symbol")
    quantity: float = Field(..., description="Quantity traded")
    price: float = Field(..., description="Execution price")
    pnl: Optional[float] = Field(None, description="Realized P&L")
    status: str = Field(..., description="Trade status")


class LeaderboardPayload(ResponsePayload):
    """Payload for leaderboard updates."""

    leaderboard: list = Field(..., description="Current leaderboard entries")
    cycle_number: int = Field(..., description="Current cycle")


class UnifiedResponseData(BaseModel):
    """Unified response data structure with optional hierarchy fields."""

    competition_id: str = Field(..., description="Competition identifier")
    cycle_number: Optional[int] = Field(None, description="Current cycle number")
    model_name: Optional[str] = Field(None, description="Agent/model name")
    payload: Optional[ResponsePayload] = Field(None, description="Response payload")
    metadata: Optional[Dict[str, Any]] = Field(None, description="Additional metadata")
    role: Role = Field(Role.AGENT, description="Message role")
    item_id: str = Field(default_factory=generate_item_id)
    timestamp: datetime = Field(default_factory=datetime.now)

    class Config:
        use_enum_values = True


class BaseResponse(BaseModel, ABC):
    """Top-level response envelope used for all events."""

    event: Union[
        StreamResponseEvent,
        NotifyResponseEvent,
        SystemResponseEvent,
        CommonResponseEvent,
        TaskStatusEvent
    ] = Field(..., description="The event type")
    data: UnifiedResponseData = Field(..., description="The response data")

    class Config:
        use_enum_values = True


class ConversationItem(BaseModel):
    """Message item structure for conversation/decision history."""

    item_id: str = Field(..., description="Unique message identifier")
    role: Role = Field(..., description="Role of the sender")
    model_name: Optional[str] = Field(None, description="Model name if applicable")
    event: str = Field(..., description="Event type")
    competition_id: str = Field(..., description="Competition ID")
    cycle_number: Optional[int] = Field(None, description="Cycle number")
    payload: str = Field(..., description="JSON payload")
    metadata: str = Field("{}", description="JSON metadata")
    timestamp: datetime = Field(default_factory=datetime.now)

    class Config:
        use_enum_values = True


# Response factory functions
def message_chunk(content: str, metadata: Optional[Dict] = None) -> StreamResponse:
    """Create a message chunk response."""
    return StreamResponse(
        content=content,
        event=StreamResponseEvent.MESSAGE_CHUNK,
        metadata=metadata,
    )


def decision_started(model_name: str, symbol: str) -> StreamResponse:
    """Create a decision started response."""
    return StreamResponse(
        content=f"{model_name} analyzing {symbol}...",
        event=StreamResponseEvent.DECISION_STARTED,
        metadata={"model_name": model_name, "symbol": symbol},
    )


def decision_completed(
    model_name: str,
    action: str,
    symbol: str,
    quantity: float,
    confidence: float,
    reasoning: str,
) -> StreamResponse:
    """Create a decision completed response."""
    return StreamResponse(
        content=f"{model_name}: {action.upper()} {quantity} {symbol} (confidence: {confidence:.0%})",
        event=StreamResponseEvent.DECISION_COMPLETED,
        metadata={
            "model_name": model_name,
            "action": action,
            "symbol": symbol,
            "quantity": quantity,
            "confidence": confidence,
            "reasoning": reasoning,
        },
    )


def trade_executed(
    model_name: str,
    action: str,
    symbol: str,
    quantity: float,
    price: float,
    pnl: Optional[float] = None,
) -> StreamResponse:
    """Create a trade executed response."""
    content = f"{model_name}: {action.upper()} {quantity} {symbol} @ ${price:,.2f}"
    if pnl is not None:
        content += f" (P&L: ${pnl:+,.2f})"

    return StreamResponse(
        content=content,
        event=StreamResponseEvent.TRADE_EXECUTED,
        metadata={
            "model_name": model_name,
            "action": action,
            "symbol": symbol,
            "quantity": quantity,
            "price": price,
            "pnl": pnl,
        },
    )


def reasoning(model_name: str, text: str) -> StreamResponse:
    """Create a reasoning response."""
    return StreamResponse(
        content=text,
        event=StreamResponseEvent.REASONING,
        metadata={"model_name": model_name},
    )


def market_data(symbol: str, price: float, change_pct: float) -> StreamResponse:
    """Create a market data response."""
    return StreamResponse(
        content=f"{symbol}: ${price:,.2f} ({change_pct:+.2f}%)",
        event=StreamResponseEvent.MARKET_DATA,
        metadata={"symbol": symbol, "price": price, "change_pct": change_pct},
    )


def error(message: str, details: Optional[Dict] = None) -> StreamResponse:
    """Create an error response."""
    return StreamResponse(
        content=message,
        event=StreamResponseEvent.ERROR,
        metadata={"error": message, **(details or {})},
    )


def leaderboard_update(leaderboard: list, cycle_number: int) -> StreamResponse:
    """Create a leaderboard update response."""
    return StreamResponse(
        content=f"Leaderboard updated for cycle {cycle_number}",
        event=CommonResponseEvent.LEADERBOARD_UPDATE,
        metadata={"leaderboard": leaderboard, "cycle_number": cycle_number},
    )


def cycle_completed(cycle_number: int, cycle_time: float) -> StreamResponse:
    """Create a cycle completed response."""
    return StreamResponse(
        content=f"Cycle {cycle_number} completed in {cycle_time:.2f}s",
        event=SystemResponseEvent.CYCLE_COMPLETED,
        metadata={"cycle_number": cycle_number, "cycle_time_seconds": cycle_time},
    )


def done() -> StreamResponse:
    """Create a done response."""
    return StreamResponse(
        content="",
        event=SystemResponseEvent.DONE,
    )
