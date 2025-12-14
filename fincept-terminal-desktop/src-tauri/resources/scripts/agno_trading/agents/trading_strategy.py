"""
Trading Strategy Agent

Specialized agent for developing trading strategies and generating trade signals.
"""

from typing import Dict, Any, Optional
from core.base_agent import BaseAgent, AgentConfig
from tools.market_data import get_market_data_tools
from tools.technical_indicators import get_technical_analysis_tools


class TradingStrategyAgent(BaseAgent):
    """
    Trading Strategy Agent

    Specializes in:
    - Strategy development
    - Trade signal generation
    - Entry/exit point identification
    - Strategy optimization
    - Backtesting recommendations
    """

    def __init__(
        self,
        model_provider: str = "anthropic",
        model_name: str = "claude-sonnet-4",
        symbols: list = None,
        temperature: float = 0.4
    ):
        """Initialize Trading Strategy Agent"""
        if symbols is None:
            symbols = ["BTC/USD", "ETH/USD"]

        config = AgentConfig(
            name="TradingStrategy",
            role="Trading Strategy Specialist",
            description=(
                "Expert trading strategist specializing in developing, optimizing, and executing "
                "trading strategies. Generates high-quality trade signals based on technical analysis "
                "and market conditions."
            ),
            instructions=[
                "Develop clear, actionable trading strategies with specific entry/exit rules",
                "Generate trade signals based on multiple technical confirmations",
                "Always specify entry price, stop loss, and take profit levels",
                "Consider risk/reward ratio (minimum 1:2 preferred)",
                "Adapt strategies to current market conditions (trending vs ranging)",
                "Provide clear reasoning for each trade signal",
                "Include position sizing recommendations",
                "Monitor and adjust strategies based on performance",
                "Be conservative and prioritize capital preservation"
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
        """Initialize trading strategy specific tools"""
        tools = []
        tools.extend(get_market_data_tools())
        tools.extend(get_technical_analysis_tools())
        return tools

    def generate_trade_signal(
        self,
        symbol: str,
        strategy: str = "momentum",
        timeframe: str = "1h",
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """Generate a trade signal for a symbol"""

        strategy_prompts = {
            "momentum": f"""
            Generate a momentum trading signal for {symbol} on {timeframe} timeframe:

            1. Analyze current momentum using:
               - MACD
               - RSI
               - Moving average crossovers
               - Volume confirmation

            2. If a setup exists, provide:
               - Direction: LONG or SHORT
               - Entry price
               - Stop loss level
               - Take profit targets (TP1, TP2)
               - Risk/reward ratio
               - Position size recommendation (% of portfolio)

            3. Trade rationale:
               - Why this setup?
               - What confirms it?
               - What invalidates it?

            If no clear setup exists, state "NO SIGNAL" and explain why.
            """,

            "breakout": f"""
            Generate a breakout trading signal for {symbol}:

            1. Identify key levels:
               - Resistance levels for long breakout
               - Support levels for short breakdown
               - Consolidation zones

            2. If breakout setup exists:
               - Direction and breakout level
               - Entry strategy (immediate or retest)
               - Stop loss placement
               - Target levels
               - Volume confirmation needed

            3. Provide clear entry rules and invalidation criteria.
            """,

            "reversal": f"""
            Generate a reversal trading signal for {symbol}:

            1. Look for reversal signals:
               - RSI divergence
               - Support/resistance levels
               - Candlestick patterns
               - Oversold/overbought conditions

            2. If reversal setup exists:
               - Entry point
               - Stop loss (tight, near invalidation)
               - Target levels
               - Confidence level

            3. Note: Reversals are higher risk - require strong confirmation.
            """,

            "trend_following": f"""
            Generate a trend-following signal for {symbol}:

            1. Confirm the trend:
               - Moving average alignment
               - Higher highs/higher lows (uptrend)
               - Lower highs/lower lows (downtrend)

            2. Find entry on pullback:
               - Wait for retracement to support (uptrend)
               - Entry near moving averages
               - Stop loss below support

            3. Ride the trend with trailing stop.
            """
        }

        prompt = strategy_prompts.get(strategy, f"Generate a {strategy} trading signal for {symbol}")

        response = self.run(prompt, session_id=session_id)

        return {
            "symbol": symbol,
            "strategy": strategy,
            "timeframe": timeframe,
            "signal": self._extract_response_content(response),
            "agent": "TradingStrategy"
        }
