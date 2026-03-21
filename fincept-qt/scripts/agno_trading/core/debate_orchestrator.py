"""
Debate Orchestrator
Manages Bull/Bear/Analyst debate system for trading decisions
"""

import sys
import json
from typing import Dict, Any, List, Optional
from datetime import datetime
import time

try:
    from agno.agent import Agent
    from agno.models.response import ModelResponse
    AGNO_AVAILABLE = True
except ImportError:
    AGNO_AVAILABLE = False
    Agent = None
    ModelResponse = None


class DebateOrchestrator:
    """
    Orchestrates multi-agent debates for trading decisions
    Bull/Bear/Analyst format inspired by Alpha Arena
    """

    def __init__(self):
        """Initialize debate orchestrator"""
        if not AGNO_AVAILABLE:
            raise ImportError("Agno framework not available")

    def create_debate_agents(
        self,
        bull_model: str,
        bear_model: str,
        analyst_model: str,
        api_keys: Dict[str, str]
    ) -> Dict[str, Agent]:
        """
        Create Bull, Bear, and Analyst agents

        Args:
            bull_model: Model for bull agent (e.g., "openai:gpt-4o")
            bear_model: Model for bear agent
            analyst_model: Model for analyst agent
            api_keys: API keys for providers

        Returns:
            Dictionary of agents
        """
        # Set API keys
        import os
        for key, value in api_keys.items():
            os.environ[key] = value

        # Parse model strings
        bull_provider, bull_model_name = self._parse_model_string(bull_model)
        bear_provider, bear_model_name = self._parse_model_string(bear_model)
        analyst_provider, analyst_model_name = self._parse_model_string(analyst_model)

        # Initialize models
        bull_llm = self._get_model(bull_provider, bull_model_name, api_keys)
        bear_llm = self._get_model(bear_provider, bear_model_name, api_keys)
        analyst_llm = self._get_model(analyst_provider, analyst_model_name, api_keys)

        # Create Bull Agent (Optimistic)
        bull_agent = Agent(
            name="Bull Agent",
            role="Bullish Market Analyst",
            model=bull_llm,
            description="Analyzes markets from a bullish perspective, identifying opportunities for long positions",
            instructions=[
                "You are a bullish trading analyst. Your role is to identify REASONS TO BUY.",
                "Analyze market data, technical indicators, and sentiment to build a BULLISH case.",
                "Focus on: upward momentum, support levels, positive catalysts, bullish patterns.",
                "Be persuasive but use data and reasoning to support your arguments.",
                "Structure your argument: 1) Technical Analysis 2) Fundamental Catalysts 3) Risk Factors 4) Entry Strategy",
                "End with a clear BUY recommendation and target price if the case is strong."
            ],
            markdown=True
        )

        # Create Bear Agent (Pessimistic)
        bear_agent = Agent(
            name="Bear Agent",
            role="Bearish Market Analyst",
            model=bear_llm,
            description="Analyzes markets from a bearish perspective, identifying risks for short positions or avoiding longs",
            instructions=[
                "You are a bearish trading analyst. Your role is to identify REASONS TO SELL or AVOID.",
                "Analyze market data, technical indicators, and sentiment to build a BEARISH case.",
                "Focus on: downward momentum, resistance levels, negative catalysts, bearish patterns.",
                "Be persuasive but use data and reasoning to support your arguments.",
                "Structure your argument: 1) Technical Weaknesses 2) Fundamental Risks 3) Market Conditions 4) Exit/Short Strategy",
                "End with a clear SELL/AVOID recommendation and target price if the case is strong."
            ],
            markdown=True
        )

        # Create Analyst Agent (Neutral Decision Maker)
        analyst_agent = Agent(
            name="Analyst Agent",
            role="Chief Trading Analyst",
            model=analyst_llm,
            description="Synthesizes bull and bear arguments to make final trading decision",
            instructions=[
                "You are the CHIEF ANALYST. You listen to both BULL and BEAR arguments and make the FINAL DECISION.",
                "Be OBJECTIVE and DATA-DRIVEN. Do not favor either side - evaluate based on strength of evidence.",
                "Your decision process:",
                "  1) Summarize bull's strongest points",
                "  2) Summarize bear's strongest points",
                "  3) Identify which side has stronger evidence",
                "  4) Consider risk/reward ratio",
                "  5) Make FINAL CALL: BUY, SELL, or HOLD",
                "Provide: Action (BUY/SELL/HOLD), Entry Price, Stop Loss, Take Profit, Position Size %, Confidence (0-100%)",
                "Be decisive - the market waits for no one."
            ],
            markdown=True
        )

        return {
            'bull': bull_agent,
            'bear': bear_agent,
            'analyst': analyst_agent
        }

    def run_debate(
        self,
        symbol: str,
        market_data: Dict[str, Any],
        bull_model: str,
        bear_model: str,
        analyst_model: str,
        api_keys: Dict[str, str]
    ) -> Dict[str, Any]:
        """
        Run a full debate session

        Args:
            symbol: Trading symbol
            market_data: Market data context
            bull_model: Bull agent model
            bear_model: Bear agent model
            analyst_model: Analyst agent model
            api_keys: API keys

        Returns:
            Debate result with final decision
        """
        start_time = time.time()

        try:
            # Create agents
            agents = self.create_debate_agents(bull_model, bear_model, analyst_model, api_keys)

            # Prepare market context
            context = self._format_market_context(symbol, market_data)

            # Step 1: Bull makes case
            print(f"[Debate] Bull agent analyzing {symbol}...", file=sys.stderr)
            bull_prompt = f"""
Analyze {symbol} and make a BULLISH case for why traders should BUY.

Market Context:
{context}

Provide your analysis with supporting data and reasoning.
"""
            bull_response = agents['bull'].run(bull_prompt, stream=False)
            bull_argument = self._extract_response_content(bull_response)

            print(f"[Debate] Bull argument: {len(bull_argument)} chars", file=sys.stderr)

            # Step 2: Bear makes case
            print(f"[Debate] Bear agent analyzing {symbol}...", file=sys.stderr)
            bear_prompt = f"""
Analyze {symbol} and make a BEARISH case for why traders should SELL or AVOID.

Market Context:
{context}

Provide your analysis with supporting data and reasoning.
"""
            bear_response = agents['bear'].run(bear_prompt, stream=False)
            bear_argument = self._extract_response_content(bear_response)

            print(f"[Debate] Bear argument: {len(bear_argument)} chars", file=sys.stderr)

            # Step 3: Analyst makes final decision
            print(f"[Debate] Analyst making final decision...", file=sys.stderr)
            analyst_prompt = f"""
You have heard both sides of the debate for {symbol}.

**BULL ARGUMENT:**
{bull_argument}

**BEAR ARGUMENT:**
{bear_argument}

**MARKET DATA:**
{context}

As the Chief Analyst, make your FINAL TRADING DECISION.

Format your response as:
DECISION: [BUY/SELL/HOLD]
CONFIDENCE: [0-100]%
ENTRY PRICE: $[price]
STOP LOSS: $[price]
TAKE PROFIT: $[price]
POSITION SIZE: [0-100]%
REASONING: [Brief explanation of why you made this decision]
"""
            analyst_response = agents['analyst'].run(analyst_prompt, stream=False)
            analyst_decision = self._extract_response_content(analyst_response)

            print(f"[Debate] Analyst decision: {len(analyst_decision)} chars", file=sys.stderr)

            # Parse analyst decision
            final_decision = self._parse_analyst_decision(analyst_decision)

            # Calculate execution time
            execution_time_ms = int((time.time() - start_time) * 1000)

            # Record debate in database
            debate_data = {
                'id': f"debate_{symbol}_{int(datetime.now().timestamp())}",
                'symbol': symbol,
                'bull_model': bull_model,
                'bear_model': bear_model,
                'analyst_model': analyst_model,
                'bull_argument': bull_argument,
                'bear_argument': bear_argument,
                'analyst_decision': analyst_decision,
                'final_action': final_decision.get('action', 'HOLD'),
                'confidence': final_decision.get('confidence', 50),
                'timestamp': int(datetime.now().timestamp()),
                'execution_time_ms': execution_time_ms
            }

            from agno_trading.db.database_manager import get_db_manager
            db = get_db_manager()
            debate_id = db.record_debate(debate_data)

            return {
                'success': True,
                'debate_id': debate_id,
                'symbol': symbol,
                'bull_argument': bull_argument,
                'bear_argument': bear_argument,
                'analyst_decision': analyst_decision,
                'final_action': final_decision.get('action'),
                'confidence': final_decision.get('confidence'),
                'entry_price': final_decision.get('entry_price'),
                'stop_loss': final_decision.get('stop_loss'),
                'take_profit': final_decision.get('take_profit'),
                'position_size': final_decision.get('position_size'),
                'reasoning': final_decision.get('reasoning'),
                'execution_time_ms': execution_time_ms
            }

        except Exception as e:
            print(f"[Debate] Error: {str(e)}", file=sys.stderr)
            import traceback
            traceback.print_exc(file=sys.stderr)

            return {
                'success': False,
                'error': f"Debate failed: {str(e)}"
            }

    def _parse_model_string(self, model_string: str) -> tuple:
        """Parse model string like 'openai:gpt-4o' into provider and model name"""
        if ':' in model_string:
            provider, model = model_string.split(':', 1)
        else:
            provider = 'openai'
            model = model_string

        # Normalize provider
        provider = provider.lower()
        if provider == 'gemini':
            provider = 'google'

        return provider, model

    def _get_model(self, provider: str, model_name: str, api_keys: Dict[str, str]):
        """Get LLM model instance"""
        from agno.models.openai import OpenAIChat

        api_key_map = {
            'openai': 'OPENAI_API_KEY',
            'google': 'GOOGLE_API_KEY',
            'anthropic': 'ANTHROPIC_API_KEY',
            'groq': 'GROQ_API_KEY',
            'deepseek': 'DEEPSEEK_API_KEY',
            'xai': 'XAI_API_KEY'
        }

        key_name = api_key_map.get(provider, f"{provider.upper()}_API_KEY")
        api_key = api_keys.get(key_name, api_keys.get(f"GEMINI_API_KEY" if provider == "google" else ""))

        if provider == 'openai':
            from agno.models.openai import OpenAIChat
            return OpenAIChat(id=model_name, api_key=api_key)
        elif provider == 'anthropic':
            from agno.models.anthropic import Claude
            return Claude(id=model_name, api_key=api_key)
        elif provider == 'google':
            return OpenAIChat(
                id=model_name,
                api_key=api_key,
                base_url="https://generativelanguage.googleapis.com/v1beta/openai/"
            )
        elif provider == 'groq':
            from agno.models.groq import Groq
            return Groq(id=model_name, api_key=api_key)
        elif provider == 'deepseek':
            return OpenAIChat(
                id=model_name,
                api_key=api_key,
                base_url="https://api.deepseek.com"
            )
        elif provider == 'xai':
            return OpenAIChat(
                id=model_name,
                api_key=api_key,
                base_url="https://api.x.ai/v1"
            )
        else:
            return OpenAIChat(id=model_name, api_key=api_key)

    def _format_market_context(self, symbol: str, market_data: Dict[str, Any]) -> str:
        """Format market data into readable context"""
        context_parts = [f"Symbol: {symbol}"]

        if 'price' in market_data:
            context_parts.append(f"Current Price: ${market_data['price']:.2f}")

        if 'change_24h' in market_data:
            context_parts.append(f"24h Change: {market_data['change_24h']:+.2f}%")

        if 'volume_24h' in market_data:
            context_parts.append(f"24h Volume: ${market_data['volume_24h']:,.0f}")

        if 'indicators' in market_data:
            context_parts.append("\nTechnical Indicators:")
            for key, value in market_data['indicators'].items():
                context_parts.append(f"  - {key}: {value}")

        return "\n".join(context_parts)

    def _extract_response_content(self, response: ModelResponse) -> str:
        """Extract content from model response"""
        if hasattr(response, 'content'):
            return response.content
        elif hasattr(response, 'message'):
            return response.message
        else:
            return str(response)

    def _parse_analyst_decision(self, decision_text: str) -> Dict[str, Any]:
        """Parse analyst decision text into structured format"""
        import re

        result = {
            'action': 'HOLD',
            'confidence': 50,
            'entry_price': None,
            'stop_loss': None,
            'take_profit': None,
            'position_size': 10,
            'reasoning': ''
        }

        # Extract DECISION
        decision_match = re.search(r'DECISION:\s*(\w+)', decision_text, re.IGNORECASE)
        if decision_match:
            result['action'] = decision_match.group(1).upper()

        # Extract CONFIDENCE
        confidence_match = re.search(r'CONFIDENCE:\s*(\d+)', decision_text, re.IGNORECASE)
        if confidence_match:
            result['confidence'] = int(confidence_match.group(1))

        # Extract prices
        entry_match = re.search(r'ENTRY PRICE:\s*\$?([0-9,.]+)', decision_text, re.IGNORECASE)
        if entry_match:
            result['entry_price'] = float(entry_match.group(1).replace(',', ''))

        sl_match = re.search(r'STOP LOSS:\s*\$?([0-9,.]+)', decision_text, re.IGNORECASE)
        if sl_match:
            result['stop_loss'] = float(sl_match.group(1).replace(',', ''))

        tp_match = re.search(r'TAKE PROFIT:\s*\$?([0-9,.]+)', decision_text, re.IGNORECASE)
        if tp_match:
            result['take_profit'] = float(tp_match.group(1).replace(',', ''))

        # Extract position size
        pos_match = re.search(r'POSITION SIZE:\s*(\d+)', decision_text, re.IGNORECASE)
        if pos_match:
            result['position_size'] = int(pos_match.group(1))

        # Extract reasoning
        reasoning_match = re.search(r'REASONING:\s*(.+?)(?:\n\n|\Z)', decision_text, re.IGNORECASE | re.DOTALL)
        if reasoning_match:
            result['reasoning'] = reasoning_match.group(1).strip()
        else:
            # Use full text as reasoning if no specific section found
            result['reasoning'] = decision_text[:500]

        return result


# Global instance
_debate_orchestrator = None

def get_debate_orchestrator() -> DebateOrchestrator:
    """Get singleton debate orchestrator"""
    global _debate_orchestrator
    if _debate_orchestrator is None:
        _debate_orchestrator = DebateOrchestrator()
    return _debate_orchestrator
