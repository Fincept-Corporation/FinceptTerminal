"""
Alpha Arena Trading Framework

A modular, extensible trading framework for multi-LLM competitions.
Based on ValueCell architecture with clean separation of concerns.
"""

from .types import (
    TradeInstruction,
    TxResult,
    PortfolioView,
    PositionSnapshot,
    ComposeContext,
    ComposeResult,
    DecisionCycleResult,
    FeaturesResult,
    FeatureVector,
    Candle,
    MarketSnapshot,
    TradeDigest,
    TradeDigestEntry,
    Side,
    TxStatus,
    TradingMode,
)

from .base_composer import BaseComposer
from .base_execution import BaseExecutionGateway
from .base_portfolio import BasePortfolioService
from .base_features import BaseFeaturesPipeline
from .portfolio_service import InMemoryPortfolioService
from .paper_execution import PaperExecutionGateway
from .market_data_source import CCXTMarketDataSource
from .features_pipeline import TechnicalFeaturesPipeline
from .llm_composer import LLMComposer, ModelConfig
from .decision_coordinator import DecisionCoordinator
from .competition_runtime import CompetitionRuntime

__all__ = [
    # Types
    "TradeInstruction",
    "TxResult",
    "PortfolioView",
    "PositionSnapshot",
    "ComposeContext",
    "ComposeResult",
    "DecisionCycleResult",
    "FeaturesResult",
    "FeatureVector",
    "Candle",
    "MarketSnapshot",
    "TradeDigest",
    "TradeDigestEntry",
    "Side",
    "TxStatus",
    "TradingMode",
    # Base Classes
    "BaseComposer",
    "BaseExecutionGateway",
    "BasePortfolioService",
    "BaseFeaturesPipeline",
    # Implementations
    "InMemoryPortfolioService",
    "PaperExecutionGateway",
    "CCXTMarketDataSource",
    "TechnicalFeaturesPipeline",
    "LLMComposer",
    "ModelConfig",
    "DecisionCoordinator",
    "CompetitionRuntime",
]
