"""
LLM-based composer for multi-model trading competitions.
"""

import json
import asyncio
import time
import sys
from typing import List, Dict, Optional
from dataclasses import dataclass

from agno.agent import Agent as AgnoAgent
from agno.models.openai import OpenAIChat

try:
    from agno.models.anthropic import Claude
except ImportError:
    Claude = None

try:
    from agno.models.google import Gemini
except ImportError:
    Gemini = None

from .base_composer import BaseComposer
from .types import ComposeContext, ComposeResult, TradeInstruction, Side


@dataclass
class ModelConfig:
    """Configuration for a single LLM model."""
    name: str
    provider: str  # openai, google, anthropic, openrouter, deepseek, groq
    model_id: str
    api_key: str
    temperature: float = 0.7
    max_tokens: int = 1000
    timeout_seconds: int = 30


class LLMComposer(BaseComposer):
    """
    LLM-based composer for trading decisions.

    Supports multiple LLM providers and models competing simultaneously.
    """

    def __init__(
        self,
        models: List[ModelConfig],
        custom_prompt: Optional[str] = None,
        max_positions: int = 3,
        max_leverage: float = 10.0
    ):
        """
        Initialize LLM composer.

        Args:
            models: List of model configurations
            custom_prompt: Optional custom trading instructions
            max_positions: Maximum concurrent positions (guardrail)
            max_leverage: Maximum leverage allowed (guardrail)
        """
        self.models = models
        self.custom_prompt = custom_prompt
        self.max_positions = max_positions
        self.max_leverage = max_leverage
        self._agents: Dict[str, AgnoAgent] = {}

    def _create_agent(self, model_config: ModelConfig) -> AgnoAgent:
        """Create Agno agent for a model."""

        # Create model based on provider
        if model_config.provider == "openai":
            # o1/o4 models use max_completion_tokens and don't support custom temperature
            if model_config.model_id.startswith(('o1-', 'o4-')):
                model = OpenAIChat(
                    id=model_config.model_id,
                    api_key=model_config.api_key,
                    max_completion_tokens=model_config.max_tokens
                    # temperature not supported for o1/o4 models
                )
            else:
                model = OpenAIChat(
                    id=model_config.model_id,
                    api_key=model_config.api_key,
                    temperature=model_config.temperature,
                    max_tokens=model_config.max_tokens
                )
        elif model_config.provider == "google" and Gemini:
            model = Gemini(
                id=model_config.model_id,
                api_key=model_config.api_key
            )
        elif model_config.provider == "anthropic" and Claude:
            model = Claude(
                id=model_config.model_id,
                api_key=model_config.api_key
            )
        else:
            # Use OpenAI-compatible for other providers (OpenRouter, DeepSeek, Groq)
            base_url_map = {
                "openrouter": "https://openrouter.ai/api/v1",
                "deepseek": "https://api.deepseek.com/v1",
                "groq": "https://api.groq.com/openai/v1"
            }
            base_url = base_url_map.get(model_config.provider, "https://api.openai.com/v1")

            model = OpenAIChat(
                id=model_config.model_id,
                api_key=model_config.api_key,
                base_url=base_url,
                temperature=model_config.temperature,
                max_tokens=model_config.max_tokens
            )

        # Create agent with structured output
        agent = AgnoAgent(
            model=model,
            markdown=False
        )

        return agent

    def _build_prompt(self, context: ComposeContext) -> str:
        """
        Build trading prompt from context.
        """
        # Portfolio summary
        portfolio_summary = f"""
PORTFOLIO SUMMARY:
- Cash: ${context.portfolio.cash:.2f}
- Total Value: ${context.portfolio.total_value:.2f}
- Unrealized P&L: ${context.portfolio.total_unrealized_pnl:.2f}
- Realized P&L: ${context.portfolio.total_realized_pnl:.2f}
- Trades: {context.portfolio.trades_count}
"""

        # Current positions
        positions_str = ""
        if context.portfolio.positions:
            positions_str = "\nCURRENT POSITIONS:\n"
            for symbol, pos in context.portfolio.positions.items():
                pnl_pct = (pos.unrealized_pnl / (pos.avg_entry_price * abs(pos.quantity))) * 100 if pos.avg_entry_price > 0 else 0
                positions_str += f"- {symbol}: {pos.quantity:.4f} @ ${pos.avg_entry_price:.2f}, Current: ${pos.current_price:.2f}, P&L: ${pos.unrealized_pnl:.2f} ({pnl_pct:+.2f}%)\n"
        else:
            positions_str = "\nNo open positions.\n"

        # Market data
        market_data = "\nMARKET DATA:\n"
        for feat in context.features:
            market_data += f"\n{feat.symbol}:\n"
            market_data += f"  Price: ${feat.close:.2f} (Bid: ${feat.bid:.2f}, Ask: ${feat.ask:.2f})\n"
            market_data += f"  24h Change: {((feat.close - feat.open) / feat.open * 100) if feat.open > 0 else 0:.2f}%\n"

            if feat.rsi_14:
                market_data += f"  RSI(14): {feat.rsi_14:.2f}\n"
            if feat.macd:
                market_data += f"  MACD: {feat.macd:.4f}, Signal: {feat.macd_signal:.4f}, Histogram: {feat.macd_histogram:.4f}\n"
            if feat.bb_upper:
                market_data += f"  Bollinger Bands: Upper ${feat.bb_upper:.2f}, Middle ${feat.bb_middle:.2f}, Lower ${feat.bb_lower:.2f}\n"
            if feat.atr_14:
                market_data += f"  ATR(14): ${feat.atr_14:.2f}\n"

        # Performance digest
        digest_str = ""
        if context.digest and context.digest.total_trades > 0:
            digest_str = f"""
PERFORMANCE HISTORY:
- Total Trades: {context.digest.total_trades}
- Total Realized P&L: ${context.digest.total_realized_pnl:.2f}
- Win Rate: {context.digest.overall_win_rate:.1f}%
- Sharpe Ratio: {context.digest.sharpe_ratio:.2f}
- Max Drawdown: {context.digest.max_drawdown:.2f}%
"""

        # Custom instructions
        custom_instructions = ""
        if self.custom_prompt:
            custom_instructions = f"\nCUSTOM TRADING INSTRUCTIONS:\n{self.custom_prompt}\n"

        # Combine into full prompt
        prompt = f"""You are an expert cryptocurrency trader. Analyze the market data and make a trading decision.

{portfolio_summary}
{positions_str}
{market_data}
{digest_str}
{custom_instructions}

RULES:
1. You can BUY, SELL, HOLD, or WAIT for each symbol
2. Consider technical indicators (RSI, MACD, Bollinger Bands, ATR)
3. Manage risk - don't over-leverage
4. Maximum {self.max_positions} positions at once
5. Maximum {self.max_leverage}x leverage
6. Provide clear reasoning for your decision

DECISION TYPES:
- BUY: Enter a long position or add to existing long
- SELL: Enter a short position or close/reduce long position
- HOLD: Maintain current positions (use when you have open positions but don't want to trade)
- WAIT: No action, waiting for better opportunity (use when you have NO positions)

Respond with a JSON object in this exact format:
{{
    "decision": "BUY" or "SELL" or "HOLD" or "WAIT",
    "symbol": "BTC/USD",
    "quantity": 0.01,
    "leverage": 1.0,
    "confidence": 0.75,
    "reasoning": "Detailed explanation of your decision"
}}

If you decide to HOLD or WAIT, set quantity to 0.
"""

        return prompt

    async def compose(self, context: ComposeContext) -> ComposeResult:
        """
        Generate trading instruction using LLM.

        Runs a single model specified in context.model_name.
        """
        model_config = next((m for m in self.models if m.name == context.model_name), None)

        if not model_config:
            # Return empty result if model not found
            return ComposeResult(
                instructions=[],
                compose_id=context.compose_id,
                model_name=context.model_name,
                rationale="Model not found",
                decision_type="WAIT"
            )

        # Get or create agent
        if model_config.name not in self._agents:
            self._agents[model_config.name] = self._create_agent(model_config)

        agent = self._agents[model_config.name]

        # Build prompt
        prompt = self._build_prompt(context)

        try:
            print(f"[LLMComposer] Calling {model_config.name} for trading decision...", flush=True, file=sys.stderr)

            # Call LLM (agent.run() is synchronous in Agno SDK)
            # Run it in executor to avoid blocking the event loop
            loop = asyncio.get_event_loop()
            response = await asyncio.wait_for(
                loop.run_in_executor(None, lambda: agent.run(prompt)),
                timeout=model_config.timeout_seconds
            )

            # Parse JSON response
            response_text = response.content if hasattr(response, 'content') else str(response)
            print(f"[LLMComposer] {model_config.name} response: {response_text[:200]}...", flush=True, file=sys.stderr)

            # Extract JSON from response
            decision_data = self._extract_json(response_text)
            print(f"[LLMComposer] {model_config.name} decision: {decision_data}", flush=True, file=sys.stderr)

            # Convert to TradeInstruction
            instructions = []
            decision_type = 'WAIT'  # Default

            if decision_data:
                decision = decision_data.get('decision', 'WAIT').upper()
                decision_type = decision
                symbol = decision_data.get('symbol', context.features[0].symbol if context.features else 'BTC/USD')
                quantity = float(decision_data.get('quantity', 0))
                leverage = min(float(decision_data.get('leverage', 1.0)), self.max_leverage)
                confidence = float(decision_data.get('confidence', 0.5))
                reasoning = decision_data.get('reasoning', '')

                # Only create trade instructions for BUY/SELL with quantity > 0
                if decision in ['BUY', 'SELL'] and quantity > 0:
                    instructions.append(TradeInstruction(
                        symbol=symbol,
                        side=Side.BUY if decision == 'BUY' else Side.SELL,
                        quantity=quantity,
                        leverage=leverage,
                        max_slippage_bps=50,
                        model_name=model_config.name,
                        reasoning=reasoning,
                        confidence=confidence
                    ))

            return ComposeResult(
                instructions=instructions,
                compose_id=context.compose_id,
                model_name=model_config.name,
                rationale=decision_data.get('reasoning', '') if decision_data else '',
                decision_type=decision_type  # Store the decision type (BUY/SELL/HOLD/WAIT)
            )

        except asyncio.TimeoutError:
            print(f"[LLMComposer] TIMEOUT for model {model_config.name} after {model_config.timeout_seconds}s", flush=True, file=sys.stderr)
            return ComposeResult(
                instructions=[],
                compose_id=context.compose_id,
                model_name=model_config.name,
                rationale="Model timed out",
                decision_type="WAIT"
            )
        except Exception as e:
            print(f"[LLMComposer] ERROR in {model_config.name}: {type(e).__name__}: {e}", flush=True, file=sys.stderr)
            import traceback
            traceback.print_exc(file=sys.stderr)
            return ComposeResult(
                instructions=[],
                compose_id=context.compose_id,
                model_name=model_config.name,
                rationale=f"Error: {str(e)}",
                decision_type="WAIT"
            )

    def _extract_json(self, text: str) -> Optional[Dict]:
        """Extract JSON object from text response."""
        try:
            # Try direct JSON parse
            return json.loads(text)
        except:
            pass

        # Try to find JSON block in markdown
        import re
        json_match = re.search(r'```json\s*(\{.*?\})\s*```', text, re.DOTALL)
        if json_match:
            try:
                return json.loads(json_match.group(1))
            except:
                pass

        # Try to find any JSON object
        json_match = re.search(r'\{.*?\}', text, re.DOTALL)
        if json_match:
            try:
                return json.loads(json_match.group(0))
            except:
                pass

        return None
