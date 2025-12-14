"""
Sentiment Analyst Agent

Specialized agent for market sentiment analysis.
"""

from typing import Dict, Any, Optional
from core.base_agent import BaseAgent, AgentConfig
from tools.news_sentiment import get_news_sentiment_tools


class SentimentAnalystAgent(BaseAgent):
    """
    Sentiment Analyst Agent

    Specializes in:
    - News sentiment analysis
    - Social media sentiment
    - Market mood assessment
    - Sentiment-based signals
    """

    def __init__(
        self,
        model_provider: str = "google",
        model_name: str = "gemini-1.5-pro",
        temperature: float = 0.4
    ):
        """Initialize Sentiment Analyst Agent"""

        config = AgentConfig(
            name="SentimentAnalyst",
            role="Market Sentiment Specialist",
            description=(
                "Expert in analyzing market sentiment from news, social media, and market behavior. "
                "Identifies sentiment shifts that may impact price action."
            ),
            instructions=[
                "Analyze sentiment from multiple sources (news, social media, on-chain data)",
                "Identify sentiment extremes (euphoria or panic)",
                "Detect sentiment divergences from price action",
                "Assess credibility and impact of news events",
                "Track sentiment changes over time",
                "Provide context: is current sentiment justified?",
                "Warn of potential sentiment-driven volatility",
                "Distinguish between noise and significant sentiment shifts"
            ],
            model_provider=model_provider,
            model_name=model_name,
            temperature=temperature,
            tools=["news_sentiment"],
            enable_memory=True,
            show_tool_calls=True,
            markdown=True
        )

        super().__init__(config)

    def _initialize_tools(self):
        """Initialize sentiment analysis tools"""
        return get_news_sentiment_tools()

    def analyze_sentiment(
        self,
        symbol: str,
        sources: list = None,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """Analyze market sentiment for a symbol"""

        if sources is None:
            sources = ["news", "social"]

        prompt = f"""
        Analyze market sentiment for {symbol}:

        **Sources to analyze**: {", ".join(sources)}

        **Sentiment Analysis**:
        1. Overall sentiment score (0-100, where 50 is neutral)
        2. Sentiment trend (improving, deteriorating, stable)
        3. Key themes in recent news/discussions
        4. Sentiment vs price divergence (if any)
        5. Credibility assessment of sentiment drivers

        **Trading Implications**:
        - Does sentiment support current price?
        - Potential sentiment-driven moves?
        - Risk of sentiment reversal?

        Note: Sentiment integration is currently limited. This is a placeholder for future implementation.
        """

        response = self.run(prompt, session_id=session_id)

        return {
            "symbol": symbol,
            "sources": sources,
            "sentiment_analysis": self._extract_response_content(response),
            "agent": "SentimentAnalyst"
        }
