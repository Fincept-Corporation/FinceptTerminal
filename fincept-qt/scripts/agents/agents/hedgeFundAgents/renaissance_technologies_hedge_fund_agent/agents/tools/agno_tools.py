"""
Agno Built-in Tools Integration

Free tools (no API keys required):
- YFinance: Stock data, fundamentals
- DuckDuckGo: Web search
- Wikipedia: Company/concept research
- Calculator: Math calculations
- Python: Run Python code
- HackerNews: Tech/market news
"""

from typing import Optional, List
from agno.tools.yfinance import YFinanceTools
from agno.tools.duckduckgo import DuckDuckGoTools
from agno.tools.wikipedia import WikipediaTools
from agno.tools.calculator import CalculatorTools
from agno.tools.python import PythonTools
from agno.tools.hackernews import HackerNewsTools


# =============================================================================
# TOOL FACTORIES
# =============================================================================

def get_yfinance_tools() -> YFinanceTools:
    """
    Get YFinance tools for stock data and fundamentals.

    Capabilities:
    - Real-time stock prices
    - Company fundamentals
    - Income statements
    - Financial ratios
    - Analyst recommendations
    - Company news
    - Technical indicators
    - Historical prices
    - Company profiles
    """
    return YFinanceTools()


def get_duckduckgo_tools(
    search: bool = True,
    news: bool = True,
) -> DuckDuckGoTools:
    """
    Get DuckDuckGo tools for web search.

    Capabilities:
    - Web search
    - News search
    """
    return DuckDuckGoTools(
        enable_search=search,
        enable_news=news,
    )


def get_wikipedia_tools() -> WikipediaTools:
    """
    Get Wikipedia tools for research.

    Capabilities:
    - Search Wikipedia articles
    - Get article content
    """
    return WikipediaTools()


def get_calculator_tools() -> CalculatorTools:
    """
    Get Calculator tools for math.

    Capabilities:
    - Basic arithmetic
    - Complex calculations
    """
    return CalculatorTools()


def get_python_tools(
    safe_globals: Optional[dict] = None,
    safe_locals: Optional[dict] = None,
) -> PythonTools:
    """
    Get Python tools for code execution.

    Capabilities:
    - Run Python code
    - Data analysis
    - Calculations
    """
    return PythonTools(
        safe_globals=safe_globals,
        safe_locals=safe_locals,
    )


def get_hackernews_tools(
    get_top_stories: bool = True,
    get_user_details: bool = True,
) -> HackerNewsTools:
    """
    Get HackerNews tools for tech/market news.

    Capabilities:
    - Get top stories
    - Get user details
    """
    return HackerNewsTools(
        enable_get_top_stories=get_top_stories,
        enable_get_user_details=get_user_details,
    )


# =============================================================================
# TOOL BUNDLES
# =============================================================================

def get_research_tools() -> List:
    """
    Get all tools for research agents.

    Includes: YFinance, DuckDuckGo, Wikipedia, Calculator
    """
    return [
        get_yfinance_tools(),
        get_duckduckgo_tools(),
        get_wikipedia_tools(),
        get_calculator_tools(),
    ]


def get_quant_tools() -> List:
    """
    Get all tools for quant agents.

    Includes: YFinance, Calculator, Python
    """
    return [
        get_yfinance_tools(),
        get_calculator_tools(),
        get_python_tools(),
    ]


def get_news_tools() -> List:
    """
    Get all tools for news/sentiment analysis.

    Includes: DuckDuckGo, HackerNews, YFinance
    """
    return [
        get_duckduckgo_tools(search=True, news=True),
        get_hackernews_tools(),
        get_yfinance_tools(),
    ]


def get_all_free_tools() -> List:
    """
    Get all free tools (no API keys required).

    Includes: YFinance, DuckDuckGo, Wikipedia, Calculator, Python, HackerNews
    """
    return [
        get_yfinance_tools(),
        get_duckduckgo_tools(),
        get_wikipedia_tools(),
        get_calculator_tools(),
        get_python_tools(),
        get_hackernews_tools(),
    ]


# =============================================================================
# EXPORTS
# =============================================================================

__all__ = [
    # Individual tools
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
