"""
Renaissance Technologies Knowledge Base

Provides shared knowledge across all agents using Agno's Knowledge system.
Includes market data, research papers, strategy documentation, and trade history.

Knowledge Categories:
1. Market Data - Historical prices, fundamentals, alternative data
2. Research Papers - Academic research and internal findings
3. Strategy Documentation - Trading strategy specs and parameters
4. Trade History - Historical trades and outcomes
"""

from .base import (
    RenTechKnowledge,
    get_knowledge_base,
    reset_knowledge_base,
    KnowledgeCategory,
)
from .market_data import (
    MarketDataKnowledge,
    get_market_data_knowledge,
)
from .research_papers import (
    ResearchKnowledge,
    get_research_knowledge,
)
from .strategy_docs import (
    StrategyKnowledge,
    get_strategy_knowledge,
)
from .trade_history import (
    TradeHistoryKnowledge,
    get_trade_history_knowledge,
)

__all__ = [
    # Base
    "RenTechKnowledge",
    "get_knowledge_base",
    "reset_knowledge_base",
    "KnowledgeCategory",
    # Market Data
    "MarketDataKnowledge",
    "get_market_data_knowledge",
    # Research
    "ResearchKnowledge",
    "get_research_knowledge",
    # Strategy
    "StrategyKnowledge",
    "get_strategy_knowledge",
    # Trade History
    "TradeHistoryKnowledge",
    "get_trade_history_knowledge",
]
