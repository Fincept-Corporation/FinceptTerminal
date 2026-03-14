"""
Alpha Arena Core Module

Contains the core components for the trading competition system.

Components:
- Base agents for trading (LLM-based and rule-based)
- Features pipeline for technical analysis
- Portfolio metrics with Sharpe ratio
- Grid trading strategy
- Research agent for SEC filings
- Human-in-the-Loop approval system
"""

from alpha_arena.core.base_agent import BaseAgent, BaseTradingAgent, LLMTradingAgent
from alpha_arena.core.competition import AlphaArenaCompetition
from alpha_arena.core.paper_trading import PaperTradingEngine  # Legacy, kept for compatibility
from alpha_arena.core.paper_trading_bridge import (
    PaperTradingBridge,
    create_paper_trading_bridge,
)
from alpha_arena.core.agent_manager import AgentManager
from alpha_arena.core.market_data import MarketDataProvider

# New components
from alpha_arena.core.features_pipeline import (
    DefaultFeaturesPipeline,
    MarketFeatures,
    TechnicalFeatures,
    SentimentFeatures,
    get_features_pipeline,
)
from alpha_arena.core.portfolio_metrics import (
    PortfolioAnalyzer,
    PortfolioMetrics,
    get_analyzer,
)
from alpha_arena.core.grid_agent import (
    GridStrategyAgent,
    GridConfig,
    GridState,
)
from alpha_arena.core.hitl import (
    HITLManager,
    ApprovalRequest,
    ApprovalStatus,
    RiskLevel,
    get_hitl_manager,
    check_decision_approval,
    approve_decision,
    reject_decision,
    get_pending_approvals,
)
from alpha_arena.core.research_agent import (
    ResearchAgent,
    ResearchReport,
    get_research_agent,
    get_research_context,
)
from alpha_arena.core.broker_adapter import (
    BrokerAdapter,
    BrokerInfo,
    BrokerType,
    BrokerRegion,
    MultiExchangeDataProvider,
    OrderRequest,
    OrderResult,
    get_supported_brokers,
    get_broker_info,
    get_brokers_by_type,
    get_brokers_by_region,
    create_broker_adapter,
    get_multi_exchange_provider,
)
from alpha_arena.core.sentiment_agent import (
    SentimentAgent,
    SentimentResult,
    SentimentLevel,
    NewsArticle,
    get_sentiment_agent,
    get_sentiment_context,
)
from alpha_arena.core.memory_adapter import (
    AgentMemory,
    TradeMemory,
    MarketInsight,
    get_agent_memory,
    clear_all_memories,
)

__all__ = [
    # Base agents
    "BaseAgent",
    "BaseTradingAgent",
    "LLMTradingAgent",
    # Competition
    "AlphaArenaCompetition",
    "PaperTradingEngine",  # Legacy, kept for compatibility
    "PaperTradingBridge",  # New database-backed bridge
    "create_paper_trading_bridge",
    "AgentManager",
    "MarketDataProvider",
    # Features pipeline
    "DefaultFeaturesPipeline",
    "MarketFeatures",
    "TechnicalFeatures",
    "SentimentFeatures",
    "get_features_pipeline",
    # Portfolio metrics
    "PortfolioAnalyzer",
    "PortfolioMetrics",
    "get_analyzer",
    # Grid trading
    "GridStrategyAgent",
    "GridConfig",
    "GridState",
    # HITL
    "HITLManager",
    "ApprovalRequest",
    "ApprovalStatus",
    "RiskLevel",
    "get_hitl_manager",
    "check_decision_approval",
    "approve_decision",
    "reject_decision",
    "get_pending_approvals",
    # Research
    "ResearchAgent",
    "ResearchReport",
    "get_research_agent",
    "get_research_context",
    # Broker adapters
    "BrokerAdapter",
    "BrokerInfo",
    "BrokerType",
    "BrokerRegion",
    "MultiExchangeDataProvider",
    "OrderRequest",
    "OrderResult",
    "get_supported_brokers",
    "get_broker_info",
    "get_brokers_by_type",
    "get_brokers_by_region",
    "create_broker_adapter",
    "get_multi_exchange_provider",
    # Sentiment analysis
    "SentimentAgent",
    "SentimentResult",
    "SentimentLevel",
    "NewsArticle",
    "get_sentiment_agent",
    "get_sentiment_context",
    # Memory adapter
    "AgentMemory",
    "TradeMemory",
    "MarketInsight",
    "get_agent_memory",
    "clear_all_memories",
]
