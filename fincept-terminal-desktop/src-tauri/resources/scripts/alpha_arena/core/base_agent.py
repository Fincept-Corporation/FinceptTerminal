"""
Base Agent Abstract Class

Defines the interface for all trading agents in Alpha Arena.
Inspired by ValueCell's BaseAgent architecture with stream/notify methods.
"""

from abc import ABC, abstractmethod
from typing import AsyncGenerator, Dict, List, Optional, Any
from datetime import datetime

from alpha_arena.types.models import (
    MarketData,
    ModelDecision,
    PortfolioState,
    TradeAction,
    TradeResult,
)
from alpha_arena.types.responses import StreamResponse, NotifyResponse
from alpha_arena.utils.logging import get_logger

logger = get_logger("base_agent")


class BaseAgent(ABC):
    """
    Abstract base class for all agents in Alpha Arena.

    All agents must implement the stream() method for user-initiated requests.
    The notify() method is optional for agent-initiated notifications.
    """

    def __init__(self, name: str = "BaseAgent"):
        self.name = name
        self._initialized = False

    @abstractmethod
    async def stream(
        self,
        query: str,
        competition_id: str,
        cycle_number: int,
        dependencies: Optional[Dict] = None,
    ) -> AsyncGenerator[StreamResponse, None]:
        """
        Process queries and return streaming responses (user-initiated).

        Args:
            query: User query or prompt
            competition_id: Competition identifier
            cycle_number: Current cycle number
            dependencies: Optional context (market data, portfolio, etc.)

        Yields:
            StreamResponse: Stream responses with content and events
        """
        raise NotImplementedError

    async def notify(
        self,
        query: str,
        competition_id: str,
        cycle_number: int,
        dependencies: Optional[Dict] = None,
    ) -> AsyncGenerator[NotifyResponse, None]:
        """
        Send proactive notifications (agent-initiated).

        Args:
            query: Query content (can be empty for some agents)
            competition_id: Competition identifier
            cycle_number: Current cycle number
            dependencies: Optional context

        Yields:
            NotifyResponse: Notification content and status
        """
        raise NotImplementedError

    async def initialize(self) -> bool:
        """Initialize the agent. Override for custom initialization."""
        self._initialized = True
        logger.info(f"Agent '{self.name}' initialized")
        return True

    async def shutdown(self) -> None:
        """Shutdown the agent. Override for cleanup."""
        self._initialized = False
        logger.info(f"Agent '{self.name}' shutdown")

    @property
    def is_initialized(self) -> bool:
        return self._initialized


class BaseTradingAgent(BaseAgent):
    """
    Base class for trading agents that make buy/sell decisions.

    Provides common functionality for:
    - Processing market data
    - Making trading decisions
    - Managing portfolio state
    """

    def __init__(
        self,
        name: str,
        provider: str,
        model_id: str,
        api_key: Optional[str] = None,
        temperature: float = 0.7,
        instructions: Optional[List[str]] = None,
    ):
        super().__init__(name)
        self.provider = provider
        self.model_id = model_id
        self.api_key = api_key
        self.temperature = temperature
        self.instructions = instructions or self._default_instructions()
        self._agent = None
        self._decisions: List[ModelDecision] = []

    def _default_instructions(self) -> List[str]:
        """Default trading instructions."""
        return [
            "You are an autonomous trading agent in Alpha Arena.",
            "Your goal is to maximize P&L through systematic trading.",
            "Always respond with valid JSON only - no markdown, no explanations.",
            "Consider risk management and position sizing carefully.",
        ]

    @abstractmethod
    async def make_decision(
        self,
        market_data: MarketData,
        portfolio: PortfolioState,
        cycle_number: int,
    ) -> ModelDecision:
        """
        Make a trading decision based on market data and portfolio state.

        Args:
            market_data: Current market data
            portfolio: Current portfolio state
            cycle_number: Current cycle number

        Returns:
            ModelDecision with action, quantity, confidence, and reasoning
        """
        raise NotImplementedError

    async def stream(
        self,
        query: str,
        competition_id: str,
        cycle_number: int,
        dependencies: Optional[Dict] = None,
    ) -> AsyncGenerator[StreamResponse, None]:
        """
        Stream trading decision process.

        Dependencies should contain:
        - market_data: MarketData
        - portfolio: PortfolioState
        """
        from alpha_arena.types.responses import (
            decision_started,
            decision_completed,
            reasoning,
            error,
            done,
        )

        dependencies = dependencies or {}
        market_data = dependencies.get("market_data")
        portfolio = dependencies.get("portfolio")

        if not market_data or not portfolio:
            yield error("Missing market_data or portfolio in dependencies")
            return

        # Emit decision started
        yield decision_started(self.name, market_data.symbol)

        try:
            # Make the decision
            decision = await self.make_decision(market_data, portfolio, cycle_number)
            decision.competition_id = competition_id

            # Store decision
            self._decisions.append(decision)

            # Emit reasoning if available
            if decision.reasoning:
                yield reasoning(self.name, decision.reasoning)

            # Emit decision completed
            yield decision_completed(
                model_name=self.name,
                action=decision.action.value if hasattr(decision.action, 'value') else str(decision.action),
                symbol=decision.symbol,
                quantity=decision.quantity,
                confidence=decision.confidence,
                reasoning=decision.reasoning,
            )

        except Exception as e:
            logger.exception(f"Error making decision: {e}")
            yield error(f"Decision error: {str(e)}")

        yield done()

    def get_decisions(self, limit: int = 50) -> List[ModelDecision]:
        """Get recent decisions."""
        return self._decisions[-limit:]

    def clear_decisions(self) -> None:
        """Clear decision history."""
        self._decisions = []


class LLMTradingAgent(BaseTradingAgent):
    """
    Trading agent powered by an LLM (OpenAI, Anthropic, etc.).

    Uses the Agno library for LLM interactions when available.
    """

    def __init__(
        self,
        name: str,
        provider: str,
        model_id: str,
        api_key: Optional[str] = None,
        temperature: float = 0.7,
        instructions: Optional[List[str]] = None,
        mode: str = "baseline",
    ):
        super().__init__(name, provider, model_id, api_key, temperature, instructions)
        self.mode = mode
        self._llm = None

    async def initialize(self) -> bool:
        """Initialize the LLM agent."""
        try:
            self._llm = await self._create_llm()
            if self._llm is None:
                logger.warning(f"LLM agent '{self.name}' initialized in mock mode (agno library not available)")
            await super().initialize()
            return True
        except Exception as e:
            logger.error(f"Failed to initialize LLM agent '{self.name}': {e}")
            # Still return True but use mock mode - don't block competition
            self._llm = None
            await super().initialize()
            logger.warning(f"Agent '{self.name}' will use mock decisions due to initialization error")
            return True

    async def _create_llm(self):
        """Create the LLM instance based on provider."""
        try:
            from agno.agent import Agent
            from agno.models.openai import OpenAIChat

            # Create model based on provider
            if self.provider == "openai":
                model = OpenAIChat(id=self.model_id, api_key=self.api_key)
            elif self.provider == "anthropic":
                try:
                    from agno.models.anthropic import Claude
                    model = Claude(id=self.model_id, api_key=self.api_key)
                except ImportError:
                    # Fallback to OpenAI-compatible
                    model = OpenAIChat(
                        id=self.model_id,
                        api_key=self.api_key,
                        base_url="https://api.anthropic.com/v1",
                    )
            elif self.provider == "google" or self.provider == "gemini":
                # Google Gemini - use OpenAI-compatible endpoint directly (more reliable)
                # Model IDs: gemini-2.0-flash, gemini-1.5-flash, gemini-1.5-pro, etc.
                logger.info(f"Using Google Gemini OpenAI-compatible endpoint for {self.model_id}")
                model = OpenAIChat(
                    id=self.model_id,
                    api_key=self.api_key,
                    base_url="https://generativelanguage.googleapis.com/v1beta/openai/",
                )
            elif self.provider == "deepseek":
                model = OpenAIChat(
                    id=self.model_id,
                    api_key=self.api_key,
                    base_url="https://api.deepseek.com/v1",
                )
            elif self.provider == "groq":
                model = OpenAIChat(
                    id=self.model_id,
                    api_key=self.api_key,
                    base_url="https://api.groq.com/openai/v1",
                )
            elif self.provider == "openrouter":
                model = OpenAIChat(
                    id=self.model_id,
                    api_key=self.api_key,
                    base_url="https://openrouter.ai/api/v1",
                )
            elif self.provider == "fincept":
                # Fincept uses a custom API, not OpenAI-compatible
                # We'll handle this specially in make_decision
                logger.info("Fincept provider will use custom API handler")
                return "fincept_custom"  # Special marker
            elif self.provider == "ollama":
                # Local Ollama instance
                model = OpenAIChat(
                    id=self.model_id,
                    api_key="ollama",  # Ollama doesn't need real API key
                    base_url="http://localhost:11434/v1",
                )
            else:
                # Default to OpenAI-compatible endpoint
                logger.warning(f"Unknown provider '{self.provider}', treating as OpenAI-compatible")
                model = OpenAIChat(id=self.model_id, api_key=self.api_key)

            # Create agent
            agent = Agent(
                name=f"Trader-{self.name}",
                role="Autonomous Trading Agent",
                model=model,
                instructions=self._get_mode_instructions(),
                markdown=False,
            )

            logger.info(f"Created LLM agent '{self.name}' with {self.provider}:{self.model_id}")
            return agent

        except ImportError as e:
            logger.warning(f"Agno not available, using mock agent: {e}")
            return None

    def _get_mode_instructions(self) -> List[str]:
        """Get instructions based on competition mode."""
        base = self.instructions.copy()

        if self.mode == "baseline":
            base.extend([
                "Use comprehensive analysis considering all market data.",
                "Manage risk carefully with appropriate position sizing.",
            ])
        elif self.mode == "monk":
            base.extend([
                "Capital preservation is paramount.",
                "Only take high-conviction trades (confidence > 0.75).",
                "Use strict stop-losses and conservative position sizing.",
            ])
        elif self.mode == "situational":
            base.extend([
                "You are competing against other AI models.",
                "Consider the competitive dynamics in your decisions.",
            ])
        elif self.mode == "max_leverage":
            base.extend([
                "Use maximum available capital on every trade.",
                "Set tight stop-losses to manage risk.",
            ])

        return base

    async def make_decision(
        self,
        market_data: MarketData,
        portfolio: PortfolioState,
        cycle_number: int,
    ) -> ModelDecision:
        """Make a trading decision using the LLM."""
        import json

        # Build prompt
        prompt = self._build_prompt(market_data, portfolio, cycle_number)

        if self._llm is None:
            # Mock decision for testing
            logger.warning(f"Agent '{self.name}' using mock decision (no LLM available)")
            return self._mock_decision(market_data, cycle_number)

        # Handle Fincept custom API
        if self._llm == "fincept_custom":
            return await self._fincept_decision(prompt, market_data, cycle_number)

        try:
            logger.info(f"Agent '{self.name}' requesting decision from {self.provider}:{self.model_id}...")

            # Get response from LLM
            response = self._llm.run(prompt)

            # Detailed response logging for debugging
            resp_type = type(response).__name__
            logger.info(f"Agent '{self.name}' received response type: {resp_type}")

            # Log response structure for debugging
            if hasattr(response, 'content'):
                content_type = type(response.content).__name__ if response.content else 'None'
                logger.debug(f"Response.content type: {content_type}")
                if isinstance(response.content, list):
                    logger.debug(f"Response.content list length: {len(response.content)}")
                    if response.content:
                        first_item = response.content[0]
                        logger.debug(f"First content item type: {type(first_item).__name__}")

            # Parse response content
            content = self._extract_content(response)
            content_preview = content[:200] if content else 'EMPTY'
            logger.info(f"Agent '{self.name}' extracted content (first 200 chars): {content_preview}")

            decision_data = self._parse_json_response(content)
            logger.info(f"Agent '{self.name}' parsed decision: {decision_data}")

            action_str = decision_data.get("action", "hold").lower()
            # Validate action
            if action_str not in ["buy", "sell", "hold", "short"]:
                logger.warning(f"Invalid action '{action_str}', defaulting to 'hold'")
                action_str = "hold"

            return ModelDecision(
                competition_id="",  # Set by caller
                model_name=self.name,
                cycle_number=cycle_number,
                action=TradeAction(action_str),
                symbol=decision_data.get("symbol", market_data.symbol),
                quantity=float(decision_data.get("quantity", 0.01)),
                confidence=float(decision_data.get("confidence", 0.5)),
                reasoning=decision_data.get("reasoning", ""),
                price_at_decision=market_data.price,
                portfolio_value_before=portfolio.portfolio_value,
            )

        except Exception as e:
            import traceback
            error_details = traceback.format_exc()
            logger.error(f"Error in LLM decision for '{self.name}' ({self.provider}:{self.model_id}): {e}")
            logger.error(f"Traceback: {error_details}")

            # Create a more descriptive error message
            error_msg = str(e)
            if "getaddrinfo failed" in error_msg or "Connection" in error_msg:
                error_msg = f"Connection error to {self.provider} API"
            elif "401" in error_msg or "Unauthorized" in error_msg:
                error_msg = f"Invalid API key for {self.provider}"
            elif "429" in error_msg or "rate limit" in error_msg.lower():
                error_msg = f"Rate limit exceeded for {self.provider}"

            return ModelDecision(
                competition_id="",
                model_name=self.name,
                cycle_number=cycle_number,
                action=TradeAction.HOLD,
                symbol=market_data.symbol,
                quantity=0,
                confidence=0,
                reasoning=f"Error: {error_msg}",
                price_at_decision=market_data.price,
            )

    def _build_prompt(
        self,
        market_data: MarketData,
        portfolio: PortfolioState,
        cycle_number: int,
    ) -> str:
        """Build the trading prompt."""
        has_positions = len(portfolio.positions) > 0
        num_trades = portfolio.trades_count
        pnl = portfolio.portfolio_value - 10000  # Assuming 10k initial
        pnl_pct = (pnl / 10000) * 100

        if not has_positions and num_trades == 0:
            action_guidance = """
IMPORTANT: This is the start of the competition. You have NO open positions.
You MUST take a position (BUY or SELL) to participate. HOLD is NOT allowed at the start.
Analyze the market data and decide whether to go LONG (buy) or SHORT (sell).
Use approximately 10-20% of your capital for the initial position.
"""
        elif not has_positions:
            action_guidance = """
NOTE: You currently have NO open positions. Consider entering a new position.
Being fully in cash means you're not participating in market movements.
"""
        else:
            action_guidance = """
You have open positions. Decide whether to:
- BUY: Add to position or enter new long
- SELL: Close position or enter short
- HOLD: Maintain current position
"""

        return f"""ALPHA ARENA TRADING DECISION - Cycle #{cycle_number}

MARKET DATA:
Symbol: {market_data.symbol}
Price: ${market_data.price:,.2f}
Bid: ${market_data.bid:,.2f}
Ask: ${market_data.ask:,.2f}
24h High: ${market_data.high_24h:,.2f}
24h Low: ${market_data.low_24h:,.2f}
Volume: {market_data.volume_24h:,.2f}

YOUR PORTFOLIO:
Cash: ${portfolio.cash:,.2f}
Portfolio Value: ${portfolio.portfolio_value:,.2f}
P&L: ${pnl:+,.2f} ({pnl_pct:+.2f}%)
Open Positions: {len(portfolio.positions)}
Total Trades Made: {num_trades}
{action_guidance}
RESPONSE FORMAT (JSON ONLY - NO MARKDOWN, NO CODE BLOCKS):
{{
  "action": "buy" or "sell" or "hold",
  "symbol": "{market_data.symbol}",
  "quantity": 0.01,
  "confidence": 0.0 to 1.0,
  "reasoning": "brief explanation"
}}

Respond ONLY with the JSON object, nothing else.
"""

    def _extract_content(self, response) -> str:
        """Extract text content from LLM response (Agno RunOutput or other formats)."""
        try:
            # Handle string response directly
            if isinstance(response, str):
                return response

            # Log response type for debugging
            logger.debug(f"Extracting content from response type: {type(response).__name__}")

            # Handle Agno RunOutput object (main response type from agent.run())
            # RunOutput has: content (Any), messages (List[Message]), etc.
            if hasattr(response, 'content'):
                content = response.content
                if content is not None:
                    if isinstance(content, str):
                        return content
                    elif isinstance(content, list):
                        # Content might be a list of content parts (e.g., from Gemini)
                        texts = []
                        for part in content:
                            text = self._extract_text_from_part(part)
                            if text:
                                texts.append(text)
                        if texts:
                            return '\n'.join(texts)
                    elif isinstance(content, dict):
                        # Content might be a dict with text/content key
                        return content.get('text', content.get('content', str(content)))
                    else:
                        # Try to convert to string
                        return str(content)

            # Handle response with messages attribute (fallback for some providers)
            if hasattr(response, 'messages') and response.messages:
                messages = response.messages
                if isinstance(messages, list) and len(messages) > 0:
                    # Get the last assistant message
                    for msg in reversed(messages):
                        msg_content = self._extract_message_content(msg)
                        if msg_content:
                            return msg_content

            # Handle dict response (raw API response)
            if isinstance(response, dict):
                # Check common response keys
                for key in ['content', 'text', 'message', 'output', 'response']:
                    if key in response:
                        val = response[key]
                        if isinstance(val, str):
                            return val
                        elif isinstance(val, list):
                            texts = [self._extract_text_from_part(p) for p in val]
                            return '\n'.join(filter(None, texts))
                        elif val is not None:
                            return str(val)

            # Fallback to string representation
            logger.warning(f"Unknown response format: {type(response)}, converting to string")
            return str(response)

        except Exception as e:
            logger.error(f"Error extracting content from response: {e}")
            logger.error(f"Response type: {type(response)}")
            return str(response)

    def _extract_text_from_part(self, part) -> Optional[str]:
        """Extract text from a content part (handles various formats)."""
        if part is None:
            return None
        if isinstance(part, str):
            return part
        if hasattr(part, 'text'):
            return part.text
        if isinstance(part, dict):
            return part.get('text', part.get('content', None))
        # For other objects, try common text attributes
        for attr in ['text', 'content', 'value', 'data']:
            if hasattr(part, attr):
                val = getattr(part, attr)
                if isinstance(val, str):
                    return val
        return str(part)

    def _extract_message_content(self, msg) -> Optional[str]:
        """Extract content from a message object."""
        if isinstance(msg, str):
            return msg
        if hasattr(msg, 'content'):
            content = msg.content
            if isinstance(content, str):
                return content
            elif isinstance(content, list):
                texts = [self._extract_text_from_part(p) for p in content]
                return '\n'.join(filter(None, texts))
            elif content is not None:
                return str(content)
        if isinstance(msg, dict):
            return msg.get('content', msg.get('text', None))
        return None

    def _parse_json_response(self, content: str) -> Dict[str, Any]:
        """Parse JSON from LLM response."""
        import json
        import re

        if not content:
            logger.warning("Empty content received from LLM")
            return {"action": "hold", "quantity": 0, "confidence": 0, "reasoning": "Empty response from LLM"}

        original_content = content
        content = content.strip()

        # Remove markdown code blocks
        if '```' in content:
            # Find content between ``` markers
            parts = content.split('```')
            for part in parts:
                part = part.strip()
                if part.startswith('json'):
                    part = part[4:].strip()
                if part.startswith('{') and part.endswith('}'):
                    content = part
                    break

        # Try direct JSON parsing first
        try:
            return json.loads(content)
        except json.JSONDecodeError:
            pass

        # Find JSON object in content using regex - handle nested braces
        json_match = re.search(r'\{[^{}]*(?:\{[^{}]*\}[^{}]*)*\}', content)
        if json_match:
            try:
                return json.loads(json_match.group())
            except json.JSONDecodeError:
                pass

        # Try to find simpler JSON pattern
        simple_match = re.search(r'\{[^{}]+\}', content)
        if simple_match:
            try:
                return json.loads(simple_match.group())
            except json.JSONDecodeError:
                pass

        # If all parsing fails, try to extract key-value pairs manually
        logger.warning(f"Failed to parse JSON, attempting manual extraction from: {content[:200]}")

        result = {"action": "hold", "quantity": 0, "confidence": 0, "reasoning": "Failed to parse LLM response"}

        # Try to extract action
        action_match = re.search(r'"action"\s*:\s*"(buy|sell|hold)"', content, re.IGNORECASE)
        if action_match:
            result["action"] = action_match.group(1).lower()

        # Try to extract quantity
        qty_match = re.search(r'"quantity"\s*:\s*([\d.]+)', content)
        if qty_match:
            result["quantity"] = float(qty_match.group(1))

        # Try to extract confidence
        conf_match = re.search(r'"confidence"\s*:\s*([\d.]+)', content)
        if conf_match:
            result["confidence"] = float(conf_match.group(1))

        # Try to extract reasoning
        reason_match = re.search(r'"reasoning"\s*:\s*"([^"]*)"', content)
        if reason_match:
            result["reasoning"] = reason_match.group(1)

        return result

    async def _fincept_decision(
        self,
        prompt: str,
        market_data: MarketData,
        cycle_number: int,
    ) -> ModelDecision:
        """Make a decision using Fincept's custom LLM API."""
        import httpx
        import json

        FINCEPT_URL = "https://finceptbackend.share.zrok.io/research/llm"

        try:
            logger.info(f"Agent '{self.name}' calling Fincept LLM API...")

            async with httpx.AsyncClient(timeout=60.0) as client:
                response = await client.post(
                    FINCEPT_URL,
                    json={
                        "prompt": prompt,
                        "max_tokens": 1024,
                        "temperature": self.temperature,
                    },
                    headers={
                        "Content-Type": "application/json",
                        "X-API-Key": self.api_key or "",  # Fincept uses X-API-Key header
                    },
                )

                if response.status_code != 200:
                    error_msg = f"Fincept API error: {response.status_code}"
                    logger.error(f"{error_msg} - {response.text[:200]}")
                    return ModelDecision(
                        competition_id="",
                        model_name=self.name,
                        cycle_number=cycle_number,
                        action=TradeAction.HOLD,
                        symbol=market_data.symbol,
                        quantity=0,
                        confidence=0,
                        reasoning=f"Error: {error_msg}",
                        price_at_decision=market_data.price,
                    )

                result = response.json()
                # Fincept API returns: {"success": true, "data": {"response": "..."}}
                data = result.get("data", result)
                content = data.get("response", data.get("content", data.get("text", str(result))))
                logger.info(f"Fincept response: {content[:200]}")

                decision_data = self._parse_json_response(content)

                action_str = decision_data.get("action", "hold").lower()
                if action_str not in ["buy", "sell", "hold", "short"]:
                    action_str = "hold"

                return ModelDecision(
                    competition_id="",
                    model_name=self.name,
                    cycle_number=cycle_number,
                    action=TradeAction(action_str),
                    symbol=decision_data.get("symbol", market_data.symbol),
                    quantity=float(decision_data.get("quantity", 0.01)),
                    confidence=float(decision_data.get("confidence", 0.5)),
                    reasoning=decision_data.get("reasoning", ""),
                    price_at_decision=market_data.price,
                )

        except Exception as e:
            logger.error(f"Fincept API error: {e}")
            return ModelDecision(
                competition_id="",
                model_name=self.name,
                cycle_number=cycle_number,
                action=TradeAction.HOLD,
                symbol=market_data.symbol,
                quantity=0,
                confidence=0,
                reasoning=f"Error: Fincept API - {str(e)}",
                price_at_decision=market_data.price,
            )

    def _mock_decision(self, market_data: MarketData, cycle_number: int) -> ModelDecision:
        """Generate a mock decision for testing."""
        import random

        actions = [TradeAction.BUY, TradeAction.SELL, TradeAction.HOLD]
        action = random.choice(actions)

        return ModelDecision(
            competition_id="",
            model_name=self.name,
            cycle_number=cycle_number,
            action=action,
            symbol=market_data.symbol,
            quantity=0.01 if action != TradeAction.HOLD else 0,
            confidence=random.uniform(0.5, 0.9),
            reasoning=f"Mock decision: {action.value}",
            price_at_decision=market_data.price,
        )
