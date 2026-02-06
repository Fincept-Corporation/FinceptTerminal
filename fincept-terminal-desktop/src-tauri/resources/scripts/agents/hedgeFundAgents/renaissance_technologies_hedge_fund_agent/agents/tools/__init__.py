"""
Custom Tools for Renaissance Technologies Agents

Each agent has specialized tools matching their responsibilities.

Includes:
- Custom tools (internal calculations)
- Agno built-in tools (yfinance, duckduckgo, wikipedia, etc.)
"""

from .market_data_tools import MarketDataTools
from .signal_tools import SignalAnalysisTools
from .risk_tools import RiskAnalysisTools
from .execution_tools import ExecutionTools
from .portfolio_tools import PortfolioTools
from .compliance_tools import ComplianceTools

# Agno built-in tools (free, no API keys)
from .agno_tools import (
    get_yfinance_tools,
    get_duckduckgo_tools,
    get_wikipedia_tools,
    get_calculator_tools,
    get_python_tools,
    get_hackernews_tools,
    get_research_tools,
    get_quant_tools,
    get_news_tools,
    get_all_free_tools,
)

__all__ = [
    # Custom tools
    "MarketDataTools",
    "SignalAnalysisTools",
    "RiskAnalysisTools",
    "ExecutionTools",
    "PortfolioTools",
    "ComplianceTools",
    # Agno built-in tools
    "get_yfinance_tools",
    "get_duckduckgo_tools",
    "get_wikipedia_tools",
    "get_calculator_tools",
    "get_python_tools",
    "get_hackernews_tools",
    # Tool bundles
    "get_research_tools",
    "get_quant_tools",
    "get_news_tools",
    "get_all_free_tools",
]
