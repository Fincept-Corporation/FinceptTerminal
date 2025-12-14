"""
Market Analyst Agent

Specialized agent for market analysis, technical analysis, and trend identification.
"""

from typing import Dict, Any, Optional
from core.base_agent import BaseAgent, AgentConfig
from tools.market_data import get_market_data_tools
from tools.technical_indicators import get_technical_analysis_tools


class MarketAnalystAgent(BaseAgent):
    """
    Market Analyst Agent

    Specializes in:
    - Market data analysis
    - Technical indicator interpretation
    - Trend identification
    - Price action analysis
    - Support/resistance levels
    - Chart pattern recognition
    """

    def __init__(
        self,
        model_provider: str = "openai",
        model_name: str = "gpt-4o-mini",
        symbols: list = None,
        temperature: float = 0.3  # Lower temperature for more analytical responses
    ):
        """
        Initialize Market Analyst Agent

        Args:
            model_provider: LLM provider (openai, anthropic, google, etc.)
            model_name: Model name
            symbols: List of symbols to analyze
            temperature: Model temperature (lower = more analytical)
        """
        if symbols is None:
            symbols = ["BTC/USD", "ETH/USD"]

        config = AgentConfig(
            name="MarketAnalyst",
            role="Market Analysis Specialist",
            description=(
                "Expert market analyst specializing in technical analysis, trend identification, "
                "and price action interpretation. Provides data-driven insights using technical "
                "indicators and chart patterns."
            ),
            instructions=[
                "Analyze market data using technical indicators (SMA, EMA, RSI, MACD, Bollinger Bands)",
                "Identify trends, support/resistance levels, and chart patterns",
                "Provide objective, data-driven analysis without emotional bias",
                "Use multiple timeframes for comprehensive analysis",
                "Explain technical indicators in clear, understandable terms",
                "Highlight key levels and potential turning points",
                "Consider volume and momentum in your analysis",
                "Always cite specific indicator values and price levels",
                "Acknowledge limitations and uncertainties in market predictions"
            ],
            model_provider=model_provider,
            model_name=model_name,
            temperature=temperature,
            tools=["market_data", "technical_analysis"],
            symbols=symbols,
            enable_memory=True,
            show_tool_calls=True,
            markdown=True
        )

        super().__init__(config)

    def _initialize_tools(self):
        """Initialize market analyst specific tools"""
        tools = []
        tools.extend(get_market_data_tools())
        tools.extend(get_technical_analysis_tools())
        return tools

    def analyze_market(
        self,
        symbol: str,
        analysis_type: str = "comprehensive",
        timeframe: str = "1d",
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Perform market analysis for a symbol

        Args:
            symbol: Trading symbol to analyze
            analysis_type: Type of analysis (quick, comprehensive, deep)
            timeframe: Timeframe for analysis
            session_id: Optional session ID for context

        Returns:
            Analysis response
        """
        # Construct analysis prompt based on type
        if analysis_type == "quick":
            prompt = f"""
            Provide a quick market analysis for {symbol}:
            1. Current price and trend direction
            2. Key support and resistance levels
            3. RSI and MACD signals
            4. Brief outlook (bullish/bearish/neutral)

            Keep it concise and actionable.
            """

        elif analysis_type == "comprehensive":
            prompt = f"""
            Provide a comprehensive market analysis for {symbol}:

            1. **Current Market State**:
               - Current price and 24h change
               - Trend analysis (short-term and medium-term)

            2. **Technical Indicators**:
               - Moving averages (SMA 20, 50, 200)
               - RSI (14) - identify overbought/oversold
               - MACD - trend and momentum
               - Bollinger Bands - volatility and price position

            3. **Key Levels**:
               - Major support levels
               - Major resistance levels

            4. **Market Structure**:
               - Higher highs/lower lows analysis
               - Volume analysis

            5. **Outlook**:
               - Overall bias (bullish/bearish/neutral)
               - Key levels to watch
               - Potential scenarios

            Use actual market data and technical indicators. Be specific with numbers.
            """

        elif analysis_type == "deep":
            prompt = f"""
            Provide a deep technical analysis for {symbol}:

            1. **Multi-Timeframe Analysis**:
               - Daily, 4H, and 1H trends
               - Alignment or divergence between timeframes

            2. **Detailed Technical Indicators**:
               - Multiple moving averages and their relationships
               - RSI with divergence analysis
               - MACD with histogram analysis
               - Bollinger Bands with width analysis
               - Volume profile

            3. **Advanced Price Action**:
               - Chart patterns (if any)
               - Candlestick patterns
               - Support/resistance confluence
               - Fibonacci levels

            4. **Market Dynamics**:
               - Order book analysis (if available)
               - Volume analysis across timeframes
               - Momentum shifts

            5. **Trading Implications**:
               - Potential entry points
               - Risk levels (stop loss areas)
               - Target levels (take profit areas)
               - Risk/reward analysis

            6. **Outlook & Scenarios**:
               - Base case scenario
               - Bull case scenario
               - Bear case scenario
               - Probability assessment

            Be thorough and use all available technical tools.
            """

        else:
            prompt = f"Analyze the market for {symbol} on {timeframe} timeframe."

        # Run the analysis
        response = self.run(prompt, session_id=session_id)

        return {
            "symbol": symbol,
            "analysis_type": analysis_type,
            "timeframe": timeframe,
            "analysis": self._extract_response_content(response),
            "agent": "MarketAnalyst"
        }

    def compare_symbols(
        self,
        symbols: list,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Compare multiple symbols

        Args:
            symbols: List of symbols to compare
            session_id: Optional session ID

        Returns:
            Comparison analysis
        """
        symbols_str = ", ".join(symbols)

        prompt = f"""
        Compare the following symbols: {symbols_str}

        For each symbol, provide:
        1. Current price and trend
        2. Technical strength (based on indicators)
        3. Momentum (bullish/bearish)
        4. Relative performance

        Then rank them by:
        - Technical strength
        - Risk/reward profile
        - Short-term potential

        Provide a clear recommendation on which ones show the best technical setup.
        """

        response = self.run(prompt, session_id=session_id)

        return {
            "symbols": symbols,
            "comparison": self._extract_response_content(response),
            "agent": "MarketAnalyst"
        }

    def identify_opportunities(
        self,
        symbols: list,
        criteria: Dict[str, Any] = None,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Identify trading opportunities across symbols

        Args:
            symbols: List of symbols to scan
            criteria: Criteria for opportunities (e.g., {"rsi_below": 30, "trend": "bullish"})
            session_id: Optional session ID

        Returns:
            Identified opportunities
        """
        symbols_str = ", ".join(symbols)
        criteria_str = str(criteria) if criteria else "default technical criteria"

        prompt = f"""
        Scan the following symbols for trading opportunities: {symbols_str}

        Criteria: {criteria_str}

        For each symbol:
        1. Check if it meets the criteria
        2. Analyze technical setup
        3. Identify specific entry points
        4. Note risk levels

        List the best opportunities ranked by quality of setup and risk/reward.
        Only include setups that have clear technical confirmation.
        """

        response = self.run(prompt, session_id=session_id)

        return {
            "symbols": symbols,
            "criteria": criteria,
            "opportunities": self._extract_response_content(response),
            "agent": "MarketAnalyst"
        }
