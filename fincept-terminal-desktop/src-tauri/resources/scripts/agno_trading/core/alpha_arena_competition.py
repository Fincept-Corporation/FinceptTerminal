"""
Alpha Arena Competition System
Manages multi-model trading competition with continuous 2-3 minute execution cycles
"""

import asyncio
import json
import time
import os
import sys
from typing import List, Dict, Any, Optional
from datetime import datetime
from dataclasses import dataclass, field

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

AGNO_AVAILABLE = False
GEMINI_AVAILABLE = False

try:
    from agno.agent import Agent
    from agno.models.openai import OpenAIChat
    AGNO_AVAILABLE = True
    print("[AlphaArena] Agno library loaded successfully", file=sys.stderr)
except ImportError as e:
    print(f"[AlphaArena] Agno library not available: {e}", file=sys.stderr)

try:
    from agno.models.google import Gemini
    GEMINI_AVAILABLE = True
    print("[AlphaArena] Gemini model available", file=sys.stderr)
except ImportError as e:
    print(f"[AlphaArena] Gemini model not available (will use OpenAI-compatible API): {e}", file=sys.stderr)


@dataclass
class CompetitionModel:
    """Model configuration for competition"""
    name: str
    provider: str
    model_id: str
    initial_capital: float = 10000.0

    # Runtime state - use field(default_factory=...) for mutable defaults
    capital: float = field(default=None, repr=False)
    positions: Dict[str, Any] = field(default_factory=dict, repr=False)
    trades: List[Dict[str, Any]] = field(default_factory=list, repr=False)
    decisions: List[Dict[str, Any]] = field(default_factory=list, repr=False)
    total_pnl: float = 0.0

    def __post_init__(self):
        if self.capital is None:
            self.capital = self.initial_capital


class AlphaArenaCompetition:
    """
    Alpha Arena Competition Manager

    Runs multiple LLM models in parallel, each trading independently.
    Executes every 2-3 minutes, tracks decisions, executes trades, calculates P&L.
    """

    def __init__(
        self,
        competition_id: str,
        models: List[CompetitionModel],
        symbol: str = "BTC/USD",
        cycle_interval: int = 150,  # 2.5 minutes (150 seconds)
        mode: str = "baseline"
    ):
        """
        Initialize competition

        Args:
            competition_id: Unique competition ID
            models: List of models to compete
            symbol: Trading symbol
            cycle_interval: Seconds between execution cycles
            mode: Competition mode (baseline, monk, situational, max_leverage)
        """
        print(f"[AlphaArena] Initializing competition: {competition_id}", file=sys.stderr)
        print(f"[AlphaArena] AGNO_AVAILABLE: {AGNO_AVAILABLE}", file=sys.stderr)
        print(f"[AlphaArena] Models: {[m.name for m in models]}", file=sys.stderr)

        if not AGNO_AVAILABLE:
            raise ImportError("Agno library not available. Install with: pip install agno")

        self.competition_id = competition_id
        self.models = {m.name: m for m in models}
        self.symbol = symbol
        self.cycle_interval = cycle_interval
        self.mode = mode

        self.is_running = False
        self.cycle_count = 0
        self.start_time = None

        # Import tools using absolute imports
        try:
            from tools.kraken_api import get_kraken_api
            from tools.order_execution import PaperTradingEngine
            from db.database_manager import DatabaseManager

            self.kraken = get_kraken_api()
            self.db = DatabaseManager()
            print("[AlphaArena] Tools loaded successfully", file=sys.stderr)
        except ImportError as e:
            print(f"[AlphaArena] Failed to import tools: {e}", file=sys.stderr)
            raise

        # Create paper trading engine for each model
        self.engines = {
            name: PaperTradingEngine(initial_capital=model.initial_capital)
            for name, model in self.models.items()
        }

        # Load saved state from database if exists
        self._load_model_states()

        # Create Agno agents for each model
        self.agents = self._create_agents()

        print(f"[AlphaArena] Competition {competition_id} initialized with {len(models)} models", file=sys.stderr)

    def _load_model_states(self):
        """Load saved model states from database"""
        try:
            states = self.db.get_model_states(self.competition_id)
            print(f"[AlphaArena] Loading {len(states)} saved model states from DB", file=sys.stderr)

            for state in states:
                model_name = state['model_name']
                if model_name in self.engines and model_name in self.models:
                    engine = self.engines[model_name]
                    model = self.models[model_name]

                    # Restore engine state
                    engine.cash = state['capital']
                    engine.positions = json.loads(state['positions_json']) if state['positions_json'] else {}

                    # Restore model trades count
                    model.trades = [{}] * state['trades_count']  # Placeholder list

                    print(f"[AlphaArena] Restored state for {model_name}: cash=${engine.cash:.2f}, positions={len(engine.positions)}, trades={state['trades_count']}", file=sys.stderr)
        except Exception as e:
            print(f"[AlphaArena] Error loading model states: {e}", file=sys.stderr)

    def _save_model_states(self):
        """Save model states to database for persistence"""
        try:
            for name, model in self.models.items():
                engine = self.engines[name]

                # Calculate portfolio value
                portfolio_value = engine.cash
                current_price = 0
                try:
                    ticker = self.kraken.get_ticker("XBTUSD")
                    current_price = ticker.get('price', 0)
                    for symbol, pos in engine.positions.items():
                        portfolio_value += pos['quantity'] * current_price
                except Exception as e:
                    print(f"[AlphaArena] Error getting price for portfolio calc: {e}", file=sys.stderr)

                self.db.save_model_state(
                    competition_id=self.competition_id,
                    model_name=name,
                    capital=engine.cash,
                    positions_json=json.dumps(engine.positions),
                    trades_count=len(model.trades),
                    total_pnl=model.total_pnl,
                    portfolio_value=portfolio_value
                )

            print(f"[AlphaArena] Saved model states to DB", file=sys.stderr)
        except Exception as e:
            print(f"[AlphaArena] Error saving model states: {e}", file=sys.stderr)

    def _create_agents(self) -> Dict[str, Agent]:
        """Create Agno agents for each model"""
        agents = {}

        for name, model in self.models.items():
            try:
                print(f"[AlphaArena] Creating agent for: {name} ({model.provider}:{model.model_id})", file=sys.stderr)

                # Create model instance based on provider
                if model.provider == "openai":
                    llm = OpenAIChat(id=model.model_id)
                    print(f"[AlphaArena] Using OpenAIChat for {name}", file=sys.stderr)
                elif model.provider == "google":
                    if GEMINI_AVAILABLE:
                        # Use native Gemini model for Google
                        llm = Gemini(id=model.model_id)
                        print(f"[AlphaArena] Using native Gemini for {name}", file=sys.stderr)
                    else:
                        # Fall back to OpenAI-compatible API for Gemini
                        google_api_key = os.environ.get('GOOGLE_API_KEY')
                        if not google_api_key:
                            print(f"[AlphaArena] WARNING: GOOGLE_API_KEY not set for {name}", file=sys.stderr)
                        llm = OpenAIChat(
                            id=model.model_id,
                            api_key=google_api_key,
                            base_url="https://generativelanguage.googleapis.com/v1beta/openai/"
                        )
                        print(f"[AlphaArena] Using OpenAI-compatible API for Gemini {name}", file=sys.stderr)
                elif model.provider == "openrouter":
                    # OpenRouter uses OpenAI-compatible API with special base URL
                    openrouter_api_key = os.environ.get('OPENROUTER_API_KEY')
                    if not openrouter_api_key:
                        print(f"[AlphaArena] WARNING: OPENROUTER_API_KEY not set for {name}", file=sys.stderr)
                    llm = OpenAIChat(
                        id=model.model_id,
                        api_key=openrouter_api_key,
                        base_url="https://openrouter.ai/api/v1"
                    )
                    print(f"[AlphaArena] Using OpenRouter for {name}", file=sys.stderr)
                elif model.provider == "deepseek":
                    # DeepSeek uses OpenAI-compatible API
                    deepseek_api_key = os.environ.get('DEEPSEEK_API_KEY')
                    if not deepseek_api_key:
                        print(f"[AlphaArena] WARNING: DEEPSEEK_API_KEY not set for {name}", file=sys.stderr)
                    llm = OpenAIChat(
                        id=model.model_id,
                        api_key=deepseek_api_key,
                        base_url="https://api.deepseek.com/v1"
                    )
                    print(f"[AlphaArena] Using DeepSeek for {name}", file=sys.stderr)
                elif model.provider == "groq":
                    # Groq uses OpenAI-compatible API
                    groq_api_key = os.environ.get('GROQ_API_KEY')
                    if not groq_api_key:
                        print(f"[AlphaArena] WARNING: GROQ_API_KEY not set for {name}", file=sys.stderr)
                    llm = OpenAIChat(
                        id=model.model_id,
                        api_key=groq_api_key,
                        base_url="https://api.groq.com/openai/v1"
                    )
                    print(f"[AlphaArena] Using Groq for {name}", file=sys.stderr)
                else:
                    # Default to OpenAI for unknown providers
                    llm = OpenAIChat(id=model.model_id)
                    print(f"[AlphaArena] Using OpenAIChat (default) for {name}", file=sys.stderr)

                # Create agent
                agent = Agent(
                    name=f"Trader-{name}",
                    role="Autonomous Trading Agent",
                    model=llm,
                    instructions=self._get_instructions(self.mode),
                    markdown=False
                )

                agents[name] = agent
                print(f"[AlphaArena] Created agent successfully: {name}", file=sys.stderr)

            except Exception as e:
                print(f"[AlphaArena] ERROR creating agent {name}: {e}", file=sys.stderr)
                import traceback
                traceback.print_exc(file=sys.stderr)

        return agents

    def _get_instructions(self, mode: str) -> List[str]:
        """Get competition mode instructions"""
        base_instructions = [
            "You are an autonomous trading agent competing in Alpha Arena.",
            "Your goal is to maximize P&L through systematic trading.",
            "Always respond with valid JSON only - no markdown, no explanations.",
        ]

        if mode == "baseline":
            return base_instructions + [
                "Use comprehensive analysis considering all market data.",
                "Manage risk carefully with appropriate position sizing.",
            ]
        elif mode == "monk":
            return base_instructions + [
                "Capital preservation is paramount.",
                "Only take high-conviction trades (confidence > 0.75).",
                "Use strict stop-losses and conservative position sizing.",
            ]
        elif mode == "situational":
            return base_instructions + [
                "You are competing against other AI models.",
                "Consider the competitive dynamics in your decisions.",
            ]
        elif mode == "max_leverage":
            return base_instructions + [
                "Use maximum available capital on every trade.",
                "Set tight stop-losses to manage risk.",
            ]
        else:
            return base_instructions

    async def run_cycle(self) -> Dict[str, Any]:
        """
        Execute one trading cycle for all models

        Returns:
            Cycle results with all model decisions and trades
        """
        self.cycle_count += 1
        cycle_start = time.time()

        print(f"\n{'='*80}", file=sys.stderr)
        print(f"[AlphaArena] Cycle #{self.cycle_count} - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}", file=sys.stderr)
        print(f"{'='*80}", file=sys.stderr)

        # 1. Fetch market data
        ticker = self.kraken.get_ticker("XBTUSD")
        if "error" in ticker:
            print(f"[ERROR] Failed to fetch market data: {ticker['error']}", file=sys.stderr)
            return {"error": ticker['error']}

        current_price = ticker['price']
        print(f"[Market] BTC/USD: ${current_price:,.2f}", file=sys.stderr)

        # 2. Run each model sequentially (NO parallel - respect rate limits!)
        results = {}

        for model_name in self.models.keys():
            try:
                # Wait 1 second between models to respect rate limits
                if model_name != list(self.models.keys())[0]:
                    await asyncio.sleep(1.0)

                decision = await self._run_model(model_name, ticker)
                results[model_name] = decision

            except Exception as e:
                print(f"[ERROR] Model {model_name} failed: {e}", file=sys.stderr)
                results[model_name] = {"error": str(e)}

        cycle_time = time.time() - cycle_start
        print(f"\n[AlphaArena] Cycle completed in {cycle_time:.2f}s", file=sys.stderr)

        # Save model states to database for persistence between process invocations
        self._save_model_states()

        # Save performance snapshots for charting
        self._save_performance_snapshots(ticker['price'])

        return {
            "cycle": self.cycle_count,
            "timestamp": datetime.now().isoformat(),
            "market_data": ticker,
            "decisions": results,
            "cycle_time_seconds": cycle_time
        }

    def _save_performance_snapshots(self, current_price: float):
        """Save performance snapshots to database for charting"""
        try:
            for name, model in self.models.items():
                engine = self.engines[name]

                # Calculate portfolio value
                portfolio_value = engine.cash
                for symbol, pos in engine.positions.items():
                    portfolio_value += pos['quantity'] * current_price

                pnl = portfolio_value - model.initial_capital
                return_pct = (pnl / model.initial_capital) * 100

                # Save to database via SQL
                conn = self.db.get_connection()
                snapshot_id = f"{self.competition_id}_{name}_cycle_{self.cycle_count}_{int(time.time() * 1000)}"

                conn.execute(
                    """INSERT INTO alpha_performance_snapshots
                       (id, competition_id, cycle_number, model_name, portfolio_value, cash, pnl, return_pct, positions_count, trades_count, timestamp)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (
                        snapshot_id,
                        self.competition_id,
                        self.cycle_count,
                        name,
                        portfolio_value,
                        engine.cash,
                        pnl,
                        return_pct,
                        len(engine.positions),
                        len(model.trades),
                        datetime.now().isoformat()
                    )
                )
                conn.commit()
                conn.close()

            print(f"[AlphaArena] Saved performance snapshots for cycle {self.cycle_count}", file=sys.stderr)
        except Exception as e:
            print(f"[AlphaArena] Error saving performance snapshots: {e}", file=sys.stderr)
            import traceback
            traceback.print_exc(file=sys.stderr)

    async def _run_model(self, model_name: str, ticker: Dict[str, Any]) -> Dict[str, Any]:
        """Run a single model's trading decision"""
        model = self.models[model_name]
        engine = self.engines[model_name]

        # Check if agent exists for this model
        if model_name not in self.agents:
            print(f"[{model_name}] ERROR: Agent not created!", file=sys.stderr)
            return {
                "error": f"Agent not created for {model_name}",
                "action": "hold",
                "model": model_name,
                "timestamp": datetime.now().isoformat()
            }

        agent = self.agents[model_name]

        # Build prompt
        prompt = self._build_trading_prompt(model_name, ticker, engine)

        # Get decision from agent
        print(f"[{model_name}] Requesting decision...", file=sys.stderr)

        try:
            # agent.run() is synchronous in Agno
            response = agent.run(prompt)
            print(f"[{model_name}] Got response from agent", file=sys.stderr)

            # Get content from response
            if hasattr(response, 'content'):
                content = response.content
            elif hasattr(response, 'messages') and response.messages:
                content = response.messages[-1].content if response.messages else ""
            elif isinstance(response, str):
                content = response
            else:
                content = str(response)

            if not content:
                print(f"[{model_name}] Empty response from agent", file=sys.stderr)
                return {
                    "error": "Empty response from agent",
                    "action": "hold",
                    "model": model_name,
                    "timestamp": datetime.now().isoformat()
                }

            content = content.strip()
            print(f"[{model_name}] Response content: {content[:200]}...", file=sys.stderr)

            # Remove markdown code blocks if present
            if content.startswith('```'):
                parts = content.split('```')
                if len(parts) >= 2:
                    content = parts[1]
                    if content.startswith('json'):
                        content = content[4:]
                    content = content.strip()

            # Try to find JSON in the response
            if not content.startswith('{'):
                # Look for JSON object in the content
                import re
                json_match = re.search(r'\{[^{}]*\}', content)
                if json_match:
                    content = json_match.group()

            decision = json.loads(content)
            decision['model'] = model_name
            decision['timestamp'] = datetime.now().isoformat()

            # Normalize action to lowercase
            if 'action' in decision:
                decision['action'] = decision['action'].lower()

            print(f"[{model_name}] Decision: {decision.get('action', 'unknown')} (confidence: {decision.get('confidence', 0):.2f})", file=sys.stderr)

            # Execute trade if action is buy/sell
            if decision.get('action') in ['buy', 'sell']:
                trade_result = self._execute_trade(model_name, decision, ticker['price'])
                decision['trade_executed'] = trade_result
            else:
                decision['trade_executed'] = {"status": "hold"}

            # Log decision to database (alpha_decision_logs table for frontend)
            try:
                # Calculate portfolio value before and after
                portfolio_value_before = engine.cash
                for symbol, pos in engine.positions.items():
                    portfolio_value_before += pos['quantity'] * ticker['price']

                # After trade execution, recalculate
                portfolio_value_after = engine.cash
                for symbol, pos in engine.positions.items():
                    portfolio_value_after += pos['quantity'] * ticker['price']

                conn = self.db.get_connection()
                conn.execute(
                    """INSERT INTO alpha_decision_logs
                    (id, competition_id, model_name, cycle_number, action, symbol, quantity,
                     confidence, reasoning, trade_executed, price_at_decision,
                     portfolio_value_before, portfolio_value_after, timestamp)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
                    (
                        f"{self.competition_id}_{model_name}_{self.cycle_count}_{int(time.time() * 1000)}",  # unique ID
                        self.competition_id,
                        model_name,
                        self.cycle_count,
                        decision.get('action', 'hold'),
                        decision.get('symbol', self.symbol),
                        decision.get('quantity', None),
                        decision.get('confidence', 0),
                        decision.get('reasoning', ''),
                        json.dumps(decision.get('trade_executed', {})),
                        ticker['price'],
                        portfolio_value_before,
                        portfolio_value_after,
                        datetime.now().isoformat()
                    )
                )
                conn.commit()
                conn.close()
                print(f"[{model_name}] Decision logged to alpha_decision_logs", file=sys.stderr)
            except Exception as e:
                print(f"[{model_name}] WARN: Failed to log decision to DB: {e}", file=sys.stderr)
                import traceback
                traceback.print_exc(file=sys.stderr)

            # Add to model's decision history
            model.decisions.append(decision)

            return decision

        except json.JSONDecodeError as e:
            print(f"[{model_name}] Invalid JSON response: {e}", file=sys.stderr)
            error_decision = {
                "error": f"Invalid JSON response: {str(e)}",
                "raw_response": content[:500] if 'content' in dir() else "No content",
                "action": "hold",
                "model": model_name,
                "timestamp": datetime.now().isoformat(),
                "confidence": 0,
                "reasoning": "Failed to parse LLM response as JSON"
            }
            model.decisions.append(error_decision)
            return error_decision

        except Exception as e:
            print(f"[{model_name}] ERROR in _run_model: {e}", file=sys.stderr)
            import traceback
            traceback.print_exc(file=sys.stderr)
            error_decision = {
                "error": str(e),
                "action": "hold",
                "model": model_name,
                "timestamp": datetime.now().isoformat(),
                "confidence": 0,
                "reasoning": f"Error: {str(e)}"
            }
            model.decisions.append(error_decision)
            return error_decision

    def _build_trading_prompt(self, model_name: str, ticker: Dict[str, Any], engine) -> str:
        """Build trading prompt for a model"""
        model = self.models[model_name]

        # Calculate portfolio value
        portfolio_value = engine.cash
        for symbol, pos in engine.positions.items():
            portfolio_value += pos['quantity'] * ticker['price']

        pnl = portfolio_value - model.initial_capital
        pnl_pct = (pnl / model.initial_capital) * 100

        # Check if portfolio is empty (no trades yet)
        has_positions = len(engine.positions) > 0
        num_trades = len(model.trades)

        # Build different prompts based on state
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

        prompt = f"""ALPHA ARENA TRADING DECISION - Cycle #{self.cycle_count}

MARKET DATA:
Symbol: {self.symbol}
Price: ${ticker['price']:,.2f}
Bid: ${ticker['bid']:,.2f}
Ask: ${ticker['ask']:,.2f}
24h High: ${ticker['high_24h']:,.2f}
24h Low: ${ticker['low_24h']:,.2f}
Volume: {ticker['volume_24h']:,.2f}

YOUR PORTFOLIO:
Cash: ${engine.cash:,.2f}
Portfolio Value: ${portfolio_value:,.2f}
P&L: ${pnl:+,.2f} ({pnl_pct:+.2f}%)
Open Positions: {len(engine.positions)}
Total Trades Made: {num_trades}
{action_guidance}
RESPONSE FORMAT (JSON ONLY - NO MARKDOWN, NO CODE BLOCKS):
{{
  "action": "buy" or "sell" or "hold",
  "symbol": "BTC/USD",
  "quantity": 0.01,
  "confidence": 0.0 to 1.0,
  "reasoning": "brief explanation"
}}

Respond ONLY with the JSON object, nothing else.
"""
        return prompt

    def _execute_trade(self, model_name: str, decision: Dict[str, Any], current_price: float) -> Dict[str, Any]:
        """Execute a trade decision"""
        engine = self.engines[model_name]
        model = self.models[model_name]

        action = decision['action']
        quantity = decision.get('quantity', 0.01)

        # Ensure quantity is reasonable (between 0.001 and 1 BTC for safety)
        quantity = max(0.001, min(quantity, 1.0))

        if action == 'buy':
            cost = quantity * current_price
            if engine.cash >= cost:
                engine.cash -= cost

                if self.symbol not in engine.positions:
                    engine.positions[self.symbol] = {
                        'symbol': self.symbol,
                        'quantity': 0,
                        'entry_price': 0,
                        'side': 'long'
                    }

                pos = engine.positions[self.symbol]

                # Handle case where we're closing a short position
                if pos.get('side') == 'short':
                    # Closing short - calculate PnL
                    pnl = (pos['entry_price'] - current_price) * min(quantity, pos['quantity'])
                    model.total_pnl += pnl
                    pos['quantity'] -= quantity
                    if pos['quantity'] <= 0:
                        # Flip to long if we bought more than the short
                        remaining = abs(pos['quantity'])
                        if remaining > 0:
                            pos['quantity'] = remaining
                            pos['side'] = 'long'
                            pos['entry_price'] = current_price
                        else:
                            del engine.positions[self.symbol]
                else:
                    # Adding to long position
                    pos['quantity'] += quantity
                    pos['entry_price'] = current_price
                    pos['side'] = 'long'

                # Track trade
                trade = {
                    "action": "buy",
                    "quantity": quantity,
                    "price": current_price,
                    "timestamp": datetime.now().isoformat()
                }
                model.trades.append(trade)

                print(f"[{model_name}] BOUGHT {quantity} BTC @ ${current_price:,.2f}", file=sys.stderr)

                return {
                    "status": "executed",
                    "action": "buy",
                    "quantity": quantity,
                    "price": current_price,
                    "cost": cost
                }
            else:
                return {"status": "rejected", "reason": f"Insufficient cash. Need ${cost:,.2f}, have ${engine.cash:,.2f}"}

        elif action == 'sell':
            # Allow SHORT selling (opening a short position with no existing position)
            # Use margin from cash (require 50% margin for shorts)
            margin_required = quantity * current_price * 0.5

            if self.symbol in engine.positions and engine.positions[self.symbol].get('side') == 'long':
                # Closing or reducing long position
                pos = engine.positions[self.symbol]
                sell_quantity = min(quantity, pos['quantity'])
                proceeds = sell_quantity * current_price
                engine.cash += proceeds

                pnl = (current_price - pos['entry_price']) * sell_quantity
                model.total_pnl += pnl

                pos['quantity'] -= sell_quantity
                if pos['quantity'] <= 0:
                    del engine.positions[self.symbol]

                # Track trade
                trade = {
                    "action": "sell",
                    "quantity": sell_quantity,
                    "price": current_price,
                    "pnl": pnl,
                    "timestamp": datetime.now().isoformat()
                }
                model.trades.append(trade)

                print(f"[{model_name}] SOLD {sell_quantity} BTC @ ${current_price:,.2f} (P&L: ${pnl:+,.2f})", file=sys.stderr)

                return {
                    "status": "executed",
                    "action": "sell",
                    "quantity": sell_quantity,
                    "price": current_price,
                    "proceeds": proceeds,
                    "pnl": pnl
                }

            elif engine.cash >= margin_required:
                # Opening a new SHORT position
                engine.cash -= margin_required  # Reserve margin

                if self.symbol not in engine.positions:
                    engine.positions[self.symbol] = {
                        'symbol': self.symbol,
                        'quantity': quantity,
                        'entry_price': current_price,
                        'side': 'short',
                        'margin_reserved': margin_required
                    }
                else:
                    pos = engine.positions[self.symbol]
                    pos['quantity'] += quantity
                    pos['entry_price'] = current_price
                    pos['margin_reserved'] = pos.get('margin_reserved', 0) + margin_required

                # Track trade
                trade = {
                    "action": "short",
                    "quantity": quantity,
                    "price": current_price,
                    "timestamp": datetime.now().isoformat()
                }
                model.trades.append(trade)

                print(f"[{model_name}] SHORTED {quantity} BTC @ ${current_price:,.2f} (margin: ${margin_required:,.2f})", file=sys.stderr)

                return {
                    "status": "executed",
                    "action": "short",
                    "quantity": quantity,
                    "price": current_price,
                    "margin_reserved": margin_required
                }
            else:
                return {"status": "rejected", "reason": f"Insufficient margin. Need ${margin_required:,.2f}, have ${engine.cash:,.2f}"}

        return {"status": "rejected", "reason": "Invalid action"}

    def get_leaderboard(self) -> List[Dict[str, Any]]:
        """Get current leaderboard rankings"""
        leaderboard = []

        # Get latest price
        ticker = self.kraken.get_ticker("XBTUSD")
        current_price = ticker.get('price', 0)

        for name, model in self.models.items():
            engine = self.engines[name]

            # Calculate portfolio value
            portfolio_value = engine.cash
            for symbol, pos in engine.positions.items():
                portfolio_value += pos['quantity'] * current_price

            pnl = portfolio_value - model.initial_capital
            return_pct = (pnl / model.initial_capital) * 100

            leaderboard.append({
                "model": name,
                "pnl": pnl,
                "return_pct": return_pct,
                "portfolio_value": portfolio_value,
                "cash": engine.cash,
                "trades": len(model.trades),
                "positions": len(engine.positions)
            })

        # Sort by P&L descending
        leaderboard.sort(key=lambda x: x['pnl'], reverse=True)

        return leaderboard

    async def start(self, num_cycles: Optional[int] = None):
        """
        Start competition (runs continuously or for num_cycles)

        Args:
            num_cycles: Number of cycles to run (None = infinite)
        """
        self.is_running = True
        self.start_time = datetime.now()

        print(f"\n[AlphaArena] Starting competition: {self.competition_id}", file=sys.stderr)
        print(f"[AlphaArena] Mode: {self.mode}", file=sys.stderr)
        print(f"[AlphaArena] Symbol: {self.symbol}", file=sys.stderr)
        print(f"[AlphaArena] Cycle interval: {self.cycle_interval}s", file=sys.stderr)
        print(f"[AlphaArena] Models: {', '.join(self.models.keys())}", file=sys.stderr)

        cycle = 0
        while self.is_running:
            if num_cycles and cycle >= num_cycles:
                break

            cycle += 1

            try:
                result = await self.run_cycle()

                # Show leaderboard every 5 cycles
                if cycle % 5 == 0:
                    leaderboard = self.get_leaderboard()
                    print(f"\n[Leaderboard] After {cycle} cycles:", file=sys.stderr)
                    for rank, entry in enumerate(leaderboard, 1):
                        print(f"  {rank}. {entry['model']}: ${entry['pnl']:+,.2f} ({entry['return_pct']:+.2f}%)", file=sys.stderr)

                # Wait for next cycle
                await asyncio.sleep(self.cycle_interval)

            except KeyboardInterrupt:
                print(f"\n[AlphaArena] Stopped by user", file=sys.stderr)
                break
            except Exception as e:
                print(f"[ERROR] Cycle failed: {e}", file=sys.stderr)
                import traceback
                traceback.print_exc()

        self.is_running = False
        print(f"\n[AlphaArena] Competition ended after {cycle} cycles", file=sys.stderr)

    def stop(self):
        """Stop competition"""
        self.is_running = False


# Fix missing import
import os
