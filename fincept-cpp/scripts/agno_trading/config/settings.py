"""
Configuration Settings for Agno Trading Agents
===============================================

Manages all configuration for LLM providers, trading parameters,
knowledge bases, memory systems, and agent behaviors.
"""

import os
from typing import Optional, Dict, Any, List
from dataclasses import dataclass, field
from enum import Enum


class LLMProvider(str, Enum):
    """Supported LLM providers"""
    OPENAI = "openai"
    ANTHROPIC = "anthropic"
    GOOGLE = "google"
    GROQ = "groq"
    DEEPSEEK = "deepseek"
    XAI = "xai"  # Grok
    OLLAMA = "ollama"
    COHERE = "cohere"
    MISTRAL = "mistral"
    TOGETHER = "together"
    FIREWORKS = "fireworks"


class TradingMode(str, Enum):
    """Trading execution modes"""
    PAPER = "paper"
    LIVE = "live"
    BACKTEST = "backtest"
    ANALYSIS_ONLY = "analysis_only"


@dataclass
class LLMConfig:
    """LLM Provider Configuration"""

    # Provider details
    provider: LLMProvider = LLMProvider.OPENAI
    model: str = "gpt-4"

    # API Configuration
    api_key: Optional[str] = None
    api_base: Optional[str] = None
    organization: Optional[str] = None

    # Model parameters
    temperature: float = 0.7
    max_tokens: Optional[int] = 4096
    top_p: float = 1.0
    top_k: Optional[int] = None
    frequency_penalty: float = 0.0
    presence_penalty: float = 0.0

    # Reasoning parameters (for reasoning models like o1)
    reasoning_effort: Optional[str] = None  # "low", "medium", "high"

    # Streaming
    stream: bool = False

    # Timeout
    timeout: int = 60

    # Model-specific settings
    model_kwargs: Dict[str, Any] = field(default_factory=dict)

    def __post_init__(self):
        """Load API key from environment if not provided"""
        if not self.api_key:
            env_var_map = {
                LLMProvider.OPENAI: "OPENAI_API_KEY",
                LLMProvider.ANTHROPIC: "ANTHROPIC_API_KEY",
                LLMProvider.GOOGLE: "GOOGLE_API_KEY",
                LLMProvider.GROQ: "GROQ_API_KEY",
                LLMProvider.DEEPSEEK: "DEEPSEEK_API_KEY",
                LLMProvider.XAI: "XAI_API_KEY",
                LLMProvider.COHERE: "COHERE_API_KEY",
                LLMProvider.MISTRAL: "MISTRAL_API_KEY",
            }
            env_var = env_var_map.get(self.provider)
            if env_var:
                self.api_key = os.getenv(env_var)

    def to_agno_model_string(self) -> str:
        """Convert to Agno model string format: provider:model"""
        return f"{self.provider.value}:{self.model}"


@dataclass
class KnowledgeConfig:
    """Knowledge base configuration"""

    # Vector database settings
    vector_db: str = "lancedb"  # lancedb, chromadb, pgvector, etc.
    vector_db_path: Optional[str] = None

    # Embedder settings
    embedder_provider: str = "openai"
    embedder_model: str = "text-embedding-3-small"
    embedder_api_key: Optional[str] = None
    embedder_dimensions: int = 1536

    # Search settings
    search_type: str = "hybrid"  # "vector", "keyword", "hybrid"
    search_limit: int = 5

    # Content sources
    enable_market_news: bool = True
    enable_economic_data: bool = True
    enable_company_filings: bool = False
    enable_research_reports: bool = False

    # Chunking strategy
    chunk_size: int = 1000
    chunk_overlap: int = 200


@dataclass
class MemoryConfig:
    """Memory system configuration"""

    # Memory persistence
    enable_memory: bool = True
    memory_db: str = "sqlite"  # sqlite, postgres, redis
    memory_db_path: Optional[str] = None

    # Session management
    enable_sessions: bool = True
    max_session_history: int = 100

    # Memory optimization
    enable_summarization: bool = True
    summarization_threshold: int = 50  # messages

    # User-specific memory
    enable_user_memory: bool = True


@dataclass
class TradingConfig:
    """Trading-specific configuration"""

    # Trading mode
    mode: TradingMode = TradingMode.PAPER

    # Risk parameters
    max_position_size: float = 0.1  # 10% of portfolio
    max_leverage: float = 1.0  # No leverage by default
    max_drawdown: float = 0.2  # 20% max drawdown
    stop_loss_pct: float = 0.05  # 5% stop loss
    take_profit_pct: float = 0.15  # 15% take profit

    # Portfolio parameters
    initial_capital: float = 10000.0
    commission_rate: float = 0.001  # 0.1%
    slippage_pct: float = 0.0005  # 0.05%

    # Market data
    market_data_provider: str = "ccxt"  # ccxt, yfinance, polygon
    exchange: str = "kraken"  # For crypto
    symbols: List[str] = field(default_factory=lambda: ["BTC/USD", "ETH/USD"])
    timeframe: str = "1h"

    # Order execution
    order_type: str = "limit"  # limit, market
    time_in_force: str = "gtc"  # gtc, ioc, fok

    # Trading hours
    trading_hours_start: str = "00:00"
    trading_hours_end: str = "23:59"
    timezone: str = "UTC"

    # Backtesting
    backtest_start_date: Optional[str] = None
    backtest_end_date: Optional[str] = None


@dataclass
class AgentConfig:
    """Individual agent configuration"""

    # Agent identity
    name: str = "TradingAgent"
    role: str = "trader"
    description: str = "An AI trading agent"

    # Instructions
    instructions: List[str] = field(default_factory=list)
    system_prompt: Optional[str] = None

    # Tools
    tools: List[str] = field(default_factory=list)
    enable_all_tools: bool = False

    # Knowledge
    enable_knowledge: bool = True

    # Memory
    enable_memory: bool = True

    # Model
    llm_config: Optional[LLMConfig] = None

    # Execution
    show_tool_calls: bool = True
    markdown: bool = True
    debug_mode: bool = False

    # Output schema
    structured_outputs: bool = False
    output_model: Optional[str] = None


@dataclass
class TeamConfig:
    """Team of agents configuration"""

    # Team identity
    name: str = "TradingTeam"
    description: str = "A team of AI trading agents"

    # Leader
    lead_agent: Optional[str] = None

    # Members
    members: List[str] = field(default_factory=list)

    # Delegation
    delegation_strategy: str = "sequential"  # sequential, parallel, all

    # Model (inherited by all members if not specified)
    llm_config: Optional[LLMConfig] = None

    # Team memory
    enable_shared_memory: bool = True

    # Execution
    show_tool_calls: bool = True
    markdown: bool = True


@dataclass
class WorkflowConfig:
    """Workflow configuration"""

    # Workflow identity
    name: str = "TradingWorkflow"
    description: str = "AI trading workflow"

    # Steps
    steps: List[Dict[str, Any]] = field(default_factory=list)

    # Execution
    parallel_steps: bool = False
    early_stop: bool = True

    # Session
    enable_session: bool = True

    # Model
    llm_config: Optional[LLMConfig] = None


@dataclass
class AgnoConfig:
    """
    Main configuration class for Agno trading system
    Aggregates all sub-configurations
    """

    # Sub-configurations
    llm: LLMConfig = field(default_factory=LLMConfig)
    knowledge: KnowledgeConfig = field(default_factory=KnowledgeConfig)
    memory: MemoryConfig = field(default_factory=MemoryConfig)
    trading: TradingConfig = field(default_factory=TradingConfig)

    # Global settings
    debug: bool = False
    log_level: str = "INFO"

    # Database
    db_url: Optional[str] = None  # For AgentOS

    # API Keys (loaded from environment)
    api_keys: Dict[str, str] = field(default_factory=dict)

    def __post_init__(self):
        """Load environment variables"""
        # Load API keys
        api_key_vars = [
            "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "GOOGLE_API_KEY",
            "GROQ_API_KEY", "DEEPSEEK_API_KEY", "XAI_API_KEY",
            "COHERE_API_KEY", "MISTRAL_API_KEY"
        ]
        for var in api_key_vars:
            value = os.getenv(var)
            if value:
                self.api_keys[var] = value

    @classmethod
    def from_dict(cls, config_dict: Dict[str, Any]) -> "AgnoConfig":
        """Create configuration from dictionary"""
        llm_config = LLMConfig(**config_dict.get("llm", {}))
        knowledge_config = KnowledgeConfig(**config_dict.get("knowledge", {}))
        memory_config = MemoryConfig(**config_dict.get("memory", {}))
        trading_config = TradingConfig(**config_dict.get("trading", {}))

        return cls(
            llm=llm_config,
            knowledge=knowledge_config,
            memory=memory_config,
            trading=trading_config,
            debug=config_dict.get("debug", False),
            log_level=config_dict.get("log_level", "INFO"),
            db_url=config_dict.get("db_url")
        )

    def to_dict(self) -> Dict[str, Any]:
        """Convert configuration to dictionary"""
        return {
            "llm": {
                "provider": self.llm.provider.value,
                "model": self.llm.model,
                "temperature": self.llm.temperature,
                "max_tokens": self.llm.max_tokens,
                "top_p": self.llm.top_p,
                "reasoning_effort": self.llm.reasoning_effort,
                "stream": self.llm.stream,
            },
            "knowledge": {
                "vector_db": self.knowledge.vector_db,
                "search_type": self.knowledge.search_type,
                "search_limit": self.knowledge.search_limit,
            },
            "memory": {
                "enable_memory": self.memory.enable_memory,
                "enable_sessions": self.memory.enable_sessions,
                "max_session_history": self.memory.max_session_history,
            },
            "trading": {
                "mode": self.trading.mode.value,
                "max_position_size": self.trading.max_position_size,
                "max_leverage": self.trading.max_leverage,
                "stop_loss_pct": self.trading.stop_loss_pct,
                "symbols": self.trading.symbols,
            },
            "debug": self.debug,
            "log_level": self.log_level,
        }


# Default configurations for different use cases
DEFAULT_ANALYST_CONFIG = AgentConfig(
    name="MarketAnalyst",
    role="market_analyst",
    description="Analyzes market data and provides insights",
    tools=["get_market_data", "calculate_technical_indicators", "get_news"],
    llm_config=LLMConfig(provider=LLMProvider.OPENAI, model="gpt-4", temperature=0.3)
)

DEFAULT_TRADER_CONFIG = AgentConfig(
    name="TradingStrategy",
    role="trader",
    description="Develops and executes trading strategies",
    tools=["get_market_data", "place_order", "get_positions", "calculate_technical_indicators"],
    llm_config=LLMConfig(provider=LLMProvider.ANTHROPIC, model="claude-sonnet-4", temperature=0.5)
)

DEFAULT_RISK_MANAGER_CONFIG = AgentConfig(
    name="RiskManager",
    role="risk_manager",
    description="Manages portfolio risk and monitors positions",
    tools=["get_positions", "calculate_var", "get_portfolio_metrics"],
    llm_config=LLMConfig(provider=LLMProvider.OPENAI, model="gpt-4", temperature=0.2)
)
