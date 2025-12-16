"""
Tool Registry - Central registry for all agent tools

This module provides a centralized registry for managing tools that agents can use.
Tools are converted from regular Python functions into Agno-compatible tools.
"""

from typing import List, Dict, Any, Callable, Optional
from finagent_core.utils.logger import get_logger

logger = get_logger(__name__)


class ToolRegistry:
    """
    Central registry for all agent tools

    Manages:
    - Financial data tools
    - Geopolitical analysis tools
    - Economic data tools
    - Custom utility tools
    """

    def __init__(self):
        """Initialize tool registry"""
        self._tools: Dict[str, Callable] = {}
        self._toolkits: Dict[str, Any] = {}
        self._initialized = False

    def register_tool(self, name: str, tool_func: Callable):
        """
        Register a single tool

        Args:
            name: Tool name identifier
            tool_func: Python function to register as tool
        """
        self._tools[name] = tool_func
        logger.info(f"Registered tool: {name}")

    def register_toolkit(self, name: str, toolkit: Any):
        """
        Register a toolkit (collection of related tools)

        Args:
            name: Toolkit name identifier
            toolkit: Agno toolkit instance
        """
        self._toolkits[name] = toolkit
        logger.info(f"Registered toolkit: {name}")

    def get_tool(self, name: str) -> Optional[Callable]:
        """
        Get a single tool by name

        Args:
            name: Tool name

        Returns:
            Tool function or None
        """
        if not self._initialized:
            self._initialize_default_tools()

        return self._tools.get(name)

    def get_tools(self, tool_names: List[str]) -> List[Any]:
        """
        Get multiple tools by name

        Args:
            tool_names: List of tool names

        Returns:
            List of tool functions/toolkits
        """
        if not self._initialized:
            self._initialize_default_tools()

        tools = []
        for name in tool_names:
            # Check if it's a registered tool
            if name in self._tools:
                tools.append(self._tools[name])
            # Check if it's a registered toolkit
            elif name in self._toolkits:
                tools.append(self._toolkits[name])
            else:
                logger.warning(f"Tool not found: {name}")

        return tools

    def list_tools(self) -> List[str]:
        """List all registered tool names"""
        if not self._initialized:
            self._initialize_default_tools()

        return list(self._tools.keys())

    def list_toolkits(self) -> List[str]:
        """List all registered toolkit names"""
        if not self._initialized:
            self._initialize_default_tools()

        return list(self._toolkits.keys())

    def _initialize_default_tools(self):
        """Initialize default tools and toolkits"""
        if self._initialized:
            return

        logger.info("Initializing default tools...")

        # Register built-in Agno toolkits
        self._register_builtin_toolkits()

        # Register custom financial tools
        self._register_financial_tools()

        # Register geopolitical tools
        self._register_geopolitical_tools()

        # Register economic tools
        self._register_economic_tools()

        self._initialized = True
        logger.info(
            f"Initialized {len(self._tools)} tools and {len(self._toolkits)} toolkits"
        )

    def _register_builtin_toolkits(self):
        """Register built-in Agno toolkits"""
        try:
            # Web search toolkit
            from agno.tools.duckduckgo import DuckDuckGoTools
            self.register_toolkit("web_search", DuckDuckGoTools())
            logger.info("Registered DuckDuckGo search toolkit")
        except ImportError:
            logger.warning("DuckDuckGo toolkit not available")

        try:
            # File tools
            from agno.tools.file import FileTools
            self.register_toolkit("file_tools", FileTools())
            logger.info("Registered file tools")
        except ImportError:
            logger.warning("File tools not available")

        try:
            # Python tools
            from agno.tools.python import PythonTools
            self.register_toolkit("python_tools", PythonTools())
            logger.info("Registered Python tools")
        except ImportError:
            logger.warning("Python tools not available")

    def _register_financial_tools(self):
        """Register custom financial analysis tools"""

        def get_financial_metrics_tool(ticker: str, end_date: str, period: str = "ttm") -> str:
            """
            Get financial metrics for a ticker

            Args:
                ticker: Stock ticker symbol
                end_date: End date (YYYY-MM-DD)
                period: Period type (ttm, annual, quarterly)

            Returns:
                JSON string with financial metrics
            """
            try:
                import json
                import os
                import requests

                api_key = os.getenv("FINANCIAL_DATASETS_API_KEY")
                if not api_key:
                    return json.dumps({"error": "FINANCIAL_DATASETS_API_KEY not set"})

                url = f"https://api.financialdatasets.ai/financial-metrics/"
                params = {
                    "ticker": ticker,
                    "report_period_lte": end_date,
                    "period": period,
                    "limit": 10
                }
                headers = {"X-API-KEY": api_key}

                response = requests.get(url, params=params, headers=headers)
                if response.status_code == 200:
                    return json.dumps(response.json())
                else:
                    return json.dumps({"error": f"API error: {response.status_code}"})

            except Exception as e:
                return json.dumps({"error": str(e)})

        def get_stock_price_tool(ticker: str, start_date: str, end_date: str) -> str:
            """
            Get historical stock prices

            Args:
                ticker: Stock ticker symbol
                start_date: Start date (YYYY-MM-DD)
                end_date: End date (YYYY-MM-DD)

            Returns:
                JSON string with price data
            """
            try:
                import json
                import os
                import requests

                api_key = os.getenv("FINANCIAL_DATASETS_API_KEY")
                if not api_key:
                    return json.dumps({"error": "FINANCIAL_DATASETS_API_KEY not set"})

                url = f"https://api.financialdatasets.ai/prices/"
                params = {
                    "ticker": ticker,
                    "interval": "day",
                    "interval_multiplier": 1,
                    "start_date": start_date,
                    "end_date": end_date
                }
                headers = {"X-API-KEY": api_key}

                response = requests.get(url, params=params, headers=headers)
                if response.status_code == 200:
                    return json.dumps(response.json())
                else:
                    return json.dumps({"error": f"API error: {response.status_code}"})

            except Exception as e:
                return json.dumps({"error": str(e)})

        # Register financial tools
        self.register_tool("financial_metrics_tool", get_financial_metrics_tool)
        self.register_tool("stock_price_tool", get_stock_price_tool)
        logger.info("Registered custom financial tools")

    def _register_geopolitical_tools(self):
        """Register geopolitical analysis tools"""

        def geopolitical_news_tool(query: str, max_results: int = 10) -> str:
            """
            Search for geopolitical news

            Args:
                query: Search query
                max_results: Maximum number of results

            Returns:
                JSON string with news results
            """
            try:
                import json
                from agno.tools.duckduckgo import DuckDuckGoTools

                search = DuckDuckGoTools()
                # This is a placeholder - actual implementation would use the search
                return json.dumps({
                    "query": query,
                    "results": [],
                    "note": "News search functionality"
                })
            except Exception as e:
                return json.dumps({"error": str(e)})

        self.register_tool("geopolitical_news_tool", geopolitical_news_tool)
        logger.info("Registered geopolitical tools")

    def _register_economic_tools(self):
        """Register economic data tools"""

        def economic_indicators_tool(indicator: str, country: str = "US") -> str:
            """
            Get economic indicators

            Args:
                indicator: Indicator type (gdp, inflation, unemployment, etc.)
                country: Country code

            Returns:
                JSON string with economic data
            """
            try:
                import json
                return json.dumps({
                    "indicator": indicator,
                    "country": country,
                    "note": "Economic indicator data would be fetched here"
                })
            except Exception as e:
                return json.dumps({"error": str(e)})

        self.register_tool("economic_indicators_tool", economic_indicators_tool)
        logger.info("Registered economic tools")


# Global singleton instance
_tool_registry = None


def get_tool_registry() -> ToolRegistry:
    """Get global tool registry instance"""
    global _tool_registry
    if _tool_registry is None:
        _tool_registry = ToolRegistry()
    return _tool_registry
