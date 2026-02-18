"""
Alpha Arena Data Models

Pydantic models for all Alpha Arena data structures with full validation.
Inspired by ValueCell's type system architecture.
"""

from datetime import datetime
from enum import Enum
from typing import Any, Dict, List, Optional, Union
from pydantic import BaseModel, Field, field_validator, model_validator


class CompetitionMode(str, Enum):
    """Competition trading modes."""
    BASELINE = "baseline"
    MONK = "monk"
    SITUATIONAL = "situational"
    MAX_LEVERAGE = "max_leverage"


class CompetitionStatus(str, Enum):
    """Competition lifecycle states."""
    CREATED = "created"
    RUNNING = "running"
    PAUSED = "paused"
    COMPLETED = "completed"
    FAILED = "failed"


class TradeAction(str, Enum):
    """Possible trading actions."""
    BUY = "buy"
    SELL = "sell"
    HOLD = "hold"
    SHORT = "short"


class ModelProvider(str, Enum):
    """Supported LLM providers."""
    OPENAI = "openai"
    ANTHROPIC = "anthropic"
    GOOGLE = "google"
    DEEPSEEK = "deepseek"
    GROQ = "groq"
    OPENROUTER = "openrouter"
    OLLAMA = "ollama"


class AgentConfig(BaseModel):
    """Configuration for an LLM agent."""

    name: str = Field(..., description="Display name for the agent")
    provider: ModelProvider = Field(..., description="LLM provider")
    model_id: str = Field(..., description="Model identifier")
    api_key: Optional[str] = Field(None, description="API key (optional, can use env)")
    temperature: float = Field(0.7, ge=0.0, le=2.0, description="Sampling temperature")
    max_tokens: Optional[int] = Field(None, description="Max tokens for response")

    class Config:
        use_enum_values = True


class AgentAdvancedConfig(BaseModel):
    """Advanced agent configuration for Agno features and trading controls."""

    # LLM params
    temperature: float = Field(0.7, ge=0.0, le=2.0, description="Sampling temperature")
    max_tokens: int = Field(2000, ge=100, le=8000, description="Max response tokens")

    # Agno feature toggles
    enable_memory: bool = Field(False, description="Enable trade history memory")
    enable_reasoning: bool = Field(False, description="Enable step-by-step reasoning")
    enable_tools: bool = Field(False, description="Enable Agno tools (tech indicators)")
    enable_knowledge: bool = Field(False, description="Enable knowledge base")
    enable_guardrails: bool = Field(True, description="Enable position sizing guardrails")
    enable_features_pipeline: bool = Field(True, description="Enable technical features pipeline")
    enable_sentiment: bool = Field(False, description="Enable sentiment analysis")
    enable_research: bool = Field(False, description="Enable SEC research")

    # Guardrail params
    max_position_pct: float = Field(0.5, ge=0.1, le=1.0, description="Max position as % of portfolio")
    max_single_trade_pct: float = Field(0.25, ge=0.05, le=0.5, description="Max single trade as % of portfolio")
    stop_loss_pct: Optional[float] = Field(None, ge=0.01, le=0.5, description="Stop loss percentage")
    take_profit_pct: Optional[float] = Field(None, ge=0.01, le=1.0, description="Take profit percentage")

    # Reasoning params
    reasoning_min_steps: int = Field(1, ge=1, le=5, description="Min reasoning steps")
    reasoning_max_steps: int = Field(10, ge=3, le=15, description="Max reasoning steps")

    # Custom prompt
    custom_system_prompt: str = Field("", description="Custom system prompt override")
    custom_instructions: List[str] = Field(default_factory=list, description="Custom instructions")


class CompetitionModel(BaseModel):
    """Model configuration within a competition."""

    name: str = Field(..., description="Display name for the model")
    provider: str = Field(..., description="LLM provider name")
    model_id: str = Field(..., description="Model identifier")
    api_key: Optional[str] = Field(None, description="API key for this model")
    initial_capital: float = Field(10000.0, gt=0, description="Starting capital")

    # Trading style (allows using same provider with different behaviors)
    trading_style: Optional[str] = Field(
        None,
        description="Trading style ID (e.g., 'aggressive', 'conservative', 'momentum')"
    )
    metadata: Optional[Dict[str, Any]] = Field(
        default_factory=dict,
        description="Additional metadata including style configuration"
    )

    # Advanced agent config (Agno features, guardrails, reasoning, etc.)
    advanced_config: Optional[AgentAdvancedConfig] = Field(
        None,
        description="Advanced agent configuration for Agno features"
    )

    # Runtime state (populated during competition)
    capital: Optional[float] = Field(None, description="Current cash balance")
    total_pnl: float = Field(0.0, description="Total profit/loss")

    @model_validator(mode='after')
    def set_capital_default(self):
        if self.capital is None:
            self.capital = self.initial_capital
        return self


class PositionInfo(BaseModel):
    """Information about an open position."""

    symbol: str = Field(..., description="Trading symbol")
    quantity: float = Field(..., description="Position size")
    entry_price: float = Field(..., description="Average entry price")
    side: str = Field("long", description="Position side: long or short")
    margin_reserved: Optional[float] = Field(None, description="Margin for short positions")
    unrealized_pnl: Optional[float] = Field(None, description="Current unrealized P&L")


class PortfolioState(BaseModel):
    """Complete portfolio state for a model."""

    model_name: str = Field(..., description="Name of the model")
    cash: float = Field(..., description="Available cash")
    portfolio_value: float = Field(..., description="Total portfolio value")
    positions: Dict[str, PositionInfo] = Field(default_factory=dict, description="Open positions")
    total_pnl: float = Field(0.0, description="Realized P&L")
    unrealized_pnl: float = Field(0.0, description="Unrealized P&L")
    trades_count: int = Field(0, description="Total trades executed")

    @property
    def total_value(self) -> float:
        """Total value including unrealized P&L."""
        return self.portfolio_value


class MarketData(BaseModel):
    """Market data snapshot."""

    symbol: str = Field(..., description="Trading symbol")
    price: float = Field(..., gt=0, description="Current price")
    bid: float = Field(..., gt=0, description="Best bid price")
    ask: float = Field(..., gt=0, description="Best ask price")
    high_24h: float = Field(..., description="24h high")
    low_24h: float = Field(..., description="24h low")
    volume_24h: float = Field(..., description="24h volume")
    timestamp: datetime = Field(default_factory=datetime.now, description="Data timestamp")

    @property
    def spread(self) -> float:
        """Bid-ask spread."""
        return self.ask - self.bid

    @property
    def spread_pct(self) -> float:
        """Spread as percentage."""
        return (self.spread / self.price) * 100 if self.price > 0 else 0


class TradeResult(BaseModel):
    """Result of a trade execution."""

    status: str = Field(..., description="executed, rejected, or partial")
    action: TradeAction = Field(..., description="Trade action taken")
    symbol: str = Field(..., description="Trading symbol")
    quantity: float = Field(..., description="Quantity traded")
    price: float = Field(..., description="Execution price")
    cost: Optional[float] = Field(None, description="Total cost for buys")
    proceeds: Optional[float] = Field(None, description="Proceeds for sells")
    pnl: Optional[float] = Field(None, description="Realized P&L")
    margin_reserved: Optional[float] = Field(None, description="Margin for shorts")
    reason: Optional[str] = Field(None, description="Rejection reason if failed")
    timestamp: datetime = Field(default_factory=datetime.now, description="Execution time")

    class Config:
        use_enum_values = True


class ModelDecision(BaseModel):
    """Trading decision from an AI model."""

    competition_id: str = Field(..., description="Competition identifier")
    model_name: str = Field(..., description="Model that made the decision")
    cycle_number: int = Field(..., ge=0, description="Competition cycle number")
    action: TradeAction = Field(..., description="Decided action")
    symbol: str = Field(..., description="Trading symbol")
    quantity: float = Field(0.0, ge=0, description="Quantity to trade")
    confidence: float = Field(0.5, ge=0.0, le=1.0, description="Decision confidence")
    reasoning: str = Field("", description="Explanation for the decision")
    trade_executed: Optional[TradeResult] = Field(None, description="Execution result")
    timestamp: datetime = Field(default_factory=datetime.now, description="Decision time")

    # Additional context
    price_at_decision: Optional[float] = Field(None, description="Price when decision was made")
    portfolio_value_before: Optional[float] = Field(None, description="Portfolio before trade")
    portfolio_value_after: Optional[float] = Field(None, description="Portfolio after trade")

    class Config:
        use_enum_values = True

    @field_validator('action', mode='before')
    @classmethod
    def normalize_action(cls, v):
        if isinstance(v, str):
            return v.lower()
        return v


class LeaderboardEntry(BaseModel):
    """Leaderboard entry for a competing model."""

    rank: int = Field(..., ge=0, description="Current rank (0 = unranked, will be set after sorting)")
    model_name: str = Field(..., description="Model name")
    portfolio_value: float = Field(..., description="Current portfolio value")
    total_pnl: float = Field(0.0, description="Total P&L")
    return_pct: float = Field(0.0, description="Return percentage")
    trades_count: int = Field(0, ge=0, description="Total trades")
    cash: float = Field(0.0, description="Available cash")
    positions_count: int = Field(0, ge=0, description="Open positions count")
    win_rate: Optional[float] = Field(None, description="Win rate percentage")
    sharpe_ratio: Optional[float] = Field(None, description="Sharpe ratio")


class CycleResult(BaseModel):
    """Result of a competition cycle."""

    cycle_number: int = Field(..., ge=1, description="Cycle number")
    timestamp: datetime = Field(default_factory=datetime.now, description="Cycle completion time")
    market_data: MarketData = Field(..., description="Market data for this cycle")
    decisions: Dict[str, ModelDecision] = Field(default_factory=dict, description="Model decisions")
    cycle_time_seconds: float = Field(0.0, description="Time taken for cycle")
    errors: List[str] = Field(default_factory=list, description="Any errors during cycle")


class CompetitionConfig(BaseModel):
    """Configuration for creating a competition."""

    competition_id: str = Field(..., description="Unique competition identifier")
    competition_name: str = Field(..., description="Display name")
    models: List[CompetitionModel] = Field(..., min_length=1, description="Competing models")
    symbols: List[str] = Field(default=["BTC/USD"], description="Trading symbols")
    initial_capital: float = Field(10000.0, gt=0, description="Starting capital per model")
    mode: CompetitionMode = Field(CompetitionMode.BASELINE, description="Competition mode")
    cycle_interval_seconds: int = Field(150, ge=10, description="Seconds between cycles")
    exchange_id: str = Field("kraken", description="Exchange for market data")
    max_cycles: Optional[int] = Field(None, description="Max cycles (None = unlimited)")
    custom_prompt: Optional[str] = Field(None, description="Custom system prompt")
    temperature: float = Field(0.7, ge=0.0, le=2.0, description="LLM temperature from Settings")
    max_tokens: int = Field(2000, ge=100, description="LLM max tokens from Settings")

    class Config:
        use_enum_values = True

    @field_validator('models')
    @classmethod
    def validate_unique_models(cls, v):
        names = [m.name for m in v]
        if len(names) != len(set(names)):
            raise ValueError("Model names must be unique")
        return v


class Competition(BaseModel):
    """Full competition state."""

    config: CompetitionConfig = Field(..., description="Competition configuration")
    status: CompetitionStatus = Field(CompetitionStatus.CREATED, description="Current status")
    cycle_count: int = Field(0, ge=0, description="Completed cycles")
    start_time: Optional[datetime] = Field(None, description="Competition start time")
    end_time: Optional[datetime] = Field(None, description="Competition end time")
    portfolios: Dict[str, PortfolioState] = Field(default_factory=dict, description="Model portfolios")
    leaderboard: List[LeaderboardEntry] = Field(default_factory=list, description="Current rankings")

    class Config:
        use_enum_values = True


class PerformanceSnapshot(BaseModel):
    """Performance snapshot for charting."""

    id: str = Field(..., description="Unique snapshot ID")
    competition_id: str = Field(..., description="Competition ID")
    cycle_number: int = Field(..., description="Cycle number")
    model_name: str = Field(..., description="Model name")
    portfolio_value: float = Field(..., description="Portfolio value")
    cash: float = Field(..., description="Cash balance")
    pnl: float = Field(..., description="Current P&L")
    return_pct: float = Field(..., description="Return percentage")
    positions_count: int = Field(0, description="Open positions")
    trades_count: int = Field(0, description="Total trades")
    timestamp: datetime = Field(default_factory=datetime.now, description="Snapshot time")
