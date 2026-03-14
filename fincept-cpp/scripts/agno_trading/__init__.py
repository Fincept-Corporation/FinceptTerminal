"""
Agno Trading Agent System
==========================

A comprehensive AI trading agent framework powered by Agno.
Supports multiple LLM providers, trading strategies, and market analysis.

Architecture:
- core/: Base classes and agent management
- agents/: Specialized trading agents (analyst, trader, risk manager, etc.)
- tools/: Market data, technical analysis, order execution tools
- knowledge/: Market research, historical data, documentation
- utils/: Helper functions and utilities
- config/: Configuration management

Author: Fincept Terminal
Version: 1.0.0
"""

__version__ = "1.0.0"
__author__ = "Fincept Terminal"

from .core.agent_manager import AgentManager
from .core.team_orchestrator import TeamOrchestrator
from .agents import (
    MarketAnalystAgent,
    TradingStrategyAgent,
    RiskManagerAgent,
    PortfolioManagerAgent,
    SentimentAnalystAgent
)

__all__ = [
    "AgentManager",
    "TeamOrchestrator",
    "MarketAnalystAgent",
    "TradingStrategyAgent",
    "RiskManagerAgent",
    "PortfolioManagerAgent",
    "SentimentAnalystAgent"
]
