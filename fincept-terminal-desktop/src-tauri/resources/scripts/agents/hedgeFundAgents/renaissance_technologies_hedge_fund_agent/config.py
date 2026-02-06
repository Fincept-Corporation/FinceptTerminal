"""
Renaissance Technologies Multi-Agent System Configuration

Central configuration for all agents, teams, and system settings.
"""

from dataclasses import dataclass, field
from typing import Dict, Any, Optional, List
from enum import Enum


class ModelProvider(str, Enum):
    """Supported model providers"""
    OPENAI = "openai"
    ANTHROPIC = "anthropic"
    OLLAMA = "ollama"
    GROQ = "groq"
    ZHIPUAI = "zhipuai"


@dataclass
class ModelConfig:
    """
    Configuration for AI models.

    These are fallback/template values only.
    Actual model settings are loaded dynamically from frontend config via model_factory.
    """
    provider: ModelProvider = ModelProvider.OPENAI
    model_id: str = "gpt-4o"  # Fallback only, loaded from frontend
    temperature: float = 0.7
    max_tokens: int = 4096

    # Reasoning model (for complex decisions)
    reasoning_model_id: str = "gpt-4o"  # Fallback only
    reasoning_min_steps: int = 3
    reasoning_max_steps: int = 10

    # Memory model (for memory operations)
    memory_model_id: str = "gpt-4o-mini"  # Fallback only

    # API credentials (loaded from frontend config or environment)
    anthropic_api_key: str = ""
    anthropic_base_url: str = ""

    zhipuai_api_key: str = ""
    zhipuai_base_url: str = ""


@dataclass
class DatabaseConfig:
    """
    Database configuration.

    All paths use proper application data directories:
    - Windows: %APPDATA%\\fincept-terminal\\rentech\\
    - macOS: ~/Library/Application Support/fincept-terminal/rentech/
    - Linux: ~/.local/share/fincept-terminal/rentech/
    """
    # SQLite for local development (uses AppData)
    sqlite_path: Optional[str] = None  # Auto-set to AppData location

    # Frontend SQLite database (for LLM configs)
    frontend_db_path: Optional[str] = None  # Auto-detected from AppData

    # PostgreSQL for production (optional)
    postgres_url: Optional[str] = None

    # Vector database for knowledge
    vector_db_type: str = "lancedb"  # or "pgvector", "chromadb"
    vector_db_path: Optional[str] = None  # Auto-set to AppData location


@dataclass
class RiskLimits:
    """Risk management limits (RenTech style)"""
    # Position limits
    max_position_size_pct: float = 5.0  # Max 5% per position
    max_sector_exposure_pct: float = 25.0  # Max 25% per sector
    max_single_name_pct: float = 2.0  # Max 2% in single name

    # Portfolio limits
    max_leverage: float = 12.5  # RenTech uses high leverage
    max_drawdown_pct: float = 15.0  # Max 15% drawdown
    max_daily_var_pct: float = 2.0  # Max 2% daily VaR

    # Trade limits
    max_daily_trades: int = 150000  # RenTech does 150k+ trades/day
    max_order_size_adv_pct: float = 1.0  # Max 1% of ADV

    # Signal thresholds
    min_signal_confidence: float = 0.5075  # "Right 50.75% of the time"
    min_p_value: float = 0.01  # Statistical significance threshold


@dataclass
class ExecutionConfig:
    """Trade execution configuration"""
    # Execution algorithms
    default_algo: str = "twap"  # TWAP, VWAP, Arrival Price
    max_participation_rate: float = 0.05  # Max 5% of volume

    # Timing
    execution_window_hours: float = 4.0  # Spread orders over 4 hours
    urgency_threshold: float = 0.8  # When to use aggressive execution

    # Cost thresholds
    max_slippage_bps: float = 5.0  # Max 5 bps slippage
    max_market_impact_bps: float = 10.0  # Max 10 bps impact


@dataclass
class WorkflowConfig:
    """Workflow execution configuration"""
    # Timeouts (seconds)
    step_timeout: int = 60
    workflow_timeout: int = 300

    # Retries
    max_retries: int = 3
    retry_delay: float = 1.0

    # Parallel execution
    max_parallel_tasks: int = 5


@dataclass
class MemoryConfig:
    """Memory system configuration"""
    # Memory retention
    short_term_memory_hours: int = 24
    long_term_memory_days: int = 365

    # Memory limits
    max_memories_per_agent: int = 1000
    max_context_memories: int = 20

    # Learning rate
    memory_importance_threshold: float = 0.5


@dataclass
class TracingConfig:
    """Observability and tracing configuration"""
    enabled: bool = True
    service_name: str = "rentech_agents"

    # Export destinations
    export_to_db: bool = True
    export_to_console: bool = False
    export_to_jaeger: bool = False
    jaeger_endpoint: Optional[str] = None

    # Sampling
    sample_rate: float = 1.0  # 100% sampling


@dataclass
class RenTechConfig:
    """
    Master configuration for Renaissance Technologies Multi-Agent System.

    This mirrors the actual configuration parameters that a real
    quantitative trading firm would use.
    """
    # Core settings
    name: str = "Renaissance Technologies Medallion Fund"
    environment: str = "development"  # development, staging, production

    # Sub-configurations
    models: ModelConfig = field(default_factory=ModelConfig)
    database: DatabaseConfig = field(default_factory=DatabaseConfig)
    risk_limits: RiskLimits = field(default_factory=RiskLimits)
    execution: ExecutionConfig = field(default_factory=ExecutionConfig)
    workflow: WorkflowConfig = field(default_factory=WorkflowConfig)
    memory: MemoryConfig = field(default_factory=MemoryConfig)
    tracing: TracingConfig = field(default_factory=TracingConfig)

    # Feature flags
    enable_reasoning: bool = True
    enable_memory: bool = True
    enable_knowledge: bool = True
    enable_guardrails: bool = True
    enable_evaluation: bool = True
    enable_tracing: bool = True

    # Team settings
    share_interactions: bool = True
    enable_delegation: bool = True
    delegation_mode: str = "intelligent"  # intelligent, round_robin, broadcast

    @classmethod
    def development(cls) -> "RenTechConfig":
        """
        Configuration for development environment.

        Model settings loaded dynamically from frontend config.
        """
        return cls(
            environment="development",
            models=ModelConfig(),  # Use defaults (loaded from frontend)
            tracing=TracingConfig(
                export_to_console=True,
            ),
        )

    @classmethod
    def production(cls) -> "RenTechConfig":
        """
        Configuration for production environment.

        Model settings loaded dynamically from frontend config.
        """
        return cls(
            environment="production",
            models=ModelConfig(
                temperature=0.3,  # More deterministic in production
            ),
            tracing=TracingConfig(
                export_to_jaeger=True,
                sample_rate=0.1,  # 10% sampling in prod
            ),
        )

    @classmethod
    def with_openai(cls) -> "RenTechConfig":
        """Configuration using OpenAI models"""
        return cls(
            environment="development",
            models=ModelConfig(
                provider=ModelProvider.OPENAI,
                model_id="gpt-4o",
                reasoning_model_id="gpt-4o",
                memory_model_id="gpt-4o-mini",
            ),
        )


# Global configuration instance
_config: Optional[RenTechConfig] = None


def get_config() -> RenTechConfig:
    """
    Get the global configuration.

    Automatically sets paths to proper application data directories.
    """
    global _config
    if _config is None:
        _config = RenTechConfig.development()

        # Auto-configure paths using cross-platform utilities
        try:
            from .utils.paths import (
                get_memory_path,
                get_knowledge_path,
                get_frontend_db_path,
            )

            # Set database paths if not already configured
            if _config.database.sqlite_path is None:
                _config.database.sqlite_path = get_memory_path()

            if _config.database.vector_db_path is None:
                _config.database.vector_db_path = get_knowledge_path()

            if _config.database.frontend_db_path is None:
                frontend_db = get_frontend_db_path()
                if frontend_db:
                    _config.database.frontend_db_path = str(frontend_db)

        except ImportError as e:
            # Fallback to relative paths if utils not available (shouldn't happen)
            import logging
            logging.warning(f"Could not import path utilities: {e}")
            if _config.database.sqlite_path is None:
                _config.database.sqlite_path = "rentech_memory.db"
            if _config.database.vector_db_path is None:
                _config.database.vector_db_path = "./rentech_knowledge"

    return _config


def set_config(config: RenTechConfig):
    """Set the global configuration"""
    global _config
    _config = config


def reset_config():
    """Reset configuration to default"""
    global _config
    _config = None
