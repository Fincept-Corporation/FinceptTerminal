"""
Alpha Arena Core Module

Contains the core components for the trading competition system.
"""

from alpha_arena.core.base_agent import BaseAgent, BaseTradingAgent
from alpha_arena.core.competition import AlphaArenaCompetition
from alpha_arena.core.paper_trading import PaperTradingEngine
from alpha_arena.core.agent_manager import AgentManager
from alpha_arena.core.market_data import MarketDataProvider

__all__ = [
    "BaseAgent",
    "BaseTradingAgent",
    "AlphaArenaCompetition",
    "PaperTradingEngine",
    "AgentManager",
    "MarketDataProvider",
]
