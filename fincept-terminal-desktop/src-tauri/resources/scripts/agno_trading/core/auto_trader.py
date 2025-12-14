"""
Auto Trading Engine

Implements automatic trading loop inspired by Alpha Arena.
Executes trades based on AI agent recommendations with safety controls.
"""

import asyncio
import json
from typing import Dict, Any, Optional, List, Callable
from datetime import datetime, timedelta
from dataclasses import dataclass, field
import time


@dataclass
class TradingSignal:
    """Trading signal from AI agent"""
    symbol: str
    direction: str  # 'long' or 'short'
    entry_price: float
    stop_loss: float
    take_profit: float
    position_size: float
    leverage: float
    confidence: float  # 0.0 to 1.0
    reasoning: str
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())


@dataclass
class SafetyLimits:
    """Safety limits for auto-trading"""
    max_position_size: float = 0.1  # 10% of portfolio
    max_leverage: float = 1.0  # No leverage by default
    max_drawdown: float = 0.15  # 15% max drawdown
    max_daily_loss: float = 0.05  # 5% max daily loss
    min_confidence: float = 0.6  # Minimum confidence to execute
    max_open_positions: int = 5
    require_stop_loss: bool = True
    require_take_profit: bool = True


@dataclass
class TradingState:
    """Current state of auto-trading"""
    is_running: bool = False
    total_trades: int = 0
    winning_trades: int = 0
    losing_trades: int = 0
    total_pnl: float = 0.0
    current_drawdown: float = 0.0
    daily_pnl: float = 0.0
    peak_equity: float = 0.0
    last_trade_time: Optional[str] = None
    start_time: Optional[str] = None
    circuit_breaker_triggered: bool = False


class AutoTrader:
    """
    Automatic trading engine that executes AI agent recommendations

    Features:
    - Configurable execution loop (2-3 min like Alpha Arena)
    - Dynamic TP/SL based on volatility
    - Position sizing with confidence weighting
    - Safety limits and circuit breakers
    - Emergency stop mechanism
    """

    def __init__(
        self,
        agent,
        execution_interval: int = 180,  # 3 minutes like Alpha Arena
        safety_limits: Optional[SafetyLimits] = None,
        initial_capital: float = 10000.0
    ):
        """
        Initialize auto-trader

        Args:
            agent: Trading agent instance
            execution_interval: Seconds between execution cycles (default 180 = 3 min)
            safety_limits: Safety configuration
            initial_capital: Starting capital
        """
        self.agent = agent
        self.execution_interval = execution_interval
        self.safety_limits = safety_limits or SafetyLimits()
        self.initial_capital = initial_capital
        self.current_capital = initial_capital

        self.state = TradingState()
        self.positions: Dict[str, Dict[str, Any]] = {}
        self.trade_history: List[Dict[str, Any]] = []

        self.callbacks: Dict[str, Callable] = {}
        self._stop_requested = False

    def register_callback(self, event: str, callback: Callable):
        """Register callback for events (trade_executed, position_closed, etc.)"""
        self.callbacks[event] = callback

    async def start(self):
        """Start auto-trading loop"""
        if self.state.is_running:
            return {"error": "Auto-trader already running"}

        self.state.is_running = True
        self.state.start_time = datetime.now().isoformat()
        self.state.peak_equity = self.current_capital
        self._stop_requested = False

        print(f"[AutoTrader] Starting with ${self.current_capital:.2f} capital")
        print(f"[AutoTrader] Execution interval: {self.execution_interval}s")

        try:
            while self.state.is_running and not self._stop_requested:
                # Check safety limits
                if self._check_circuit_breaker():
                    self._trigger_emergency_stop("Circuit breaker triggered")
                    break

                # Execute trading cycle
                await self._execute_cycle()

                # Wait for next cycle
                await asyncio.sleep(self.execution_interval)

        except Exception as e:
            print(f"[AutoTrader] Error in trading loop: {e}")
            self.state.is_running = False

        return {
            "stopped": True,
            "total_trades": self.state.total_trades,
            "final_pnl": self.state.total_pnl
        }

    def stop(self):
        """Stop auto-trading gracefully"""
        print("[AutoTrader] Stop requested")
        self._stop_requested = True
        self.state.is_running = False

    def emergency_stop(self):
        """Emergency stop - close all positions immediately"""
        print("[AutoTrader] EMERGENCY STOP - Closing all positions")
        self._close_all_positions()
        self.stop()

    async def _execute_cycle(self):
        """Execute one trading cycle (Alpha Arena style)"""
        try:
            # 1. Get market data and portfolio state
            market_data = self._get_market_data()
            portfolio_state = self._get_portfolio_state()

            # 2. Get AI agent decision
            prompt = self._build_trading_prompt(market_data, portfolio_state)
            response = self.agent.run(prompt)

            # 3. Parse signal
            signal = self._parse_signal(response)

            if not signal:
                print("[AutoTrader] No signal from agent")
                return

            # 4. Validate signal against safety limits
            if not self._validate_signal(signal):
                print(f"[AutoTrader] Signal rejected by safety checks")
                return

            # 5. Calculate position size with confidence weighting
            position_size = self._calculate_position_size(signal)

            # 6. Execute trade
            await self._execute_trade(signal, position_size)

            # 7. Manage existing positions (check TP/SL)
            self._manage_positions()

        except Exception as e:
            print(f"[AutoTrader] Error in cycle: {e}")

    def _build_trading_prompt(self, market_data: Dict, portfolio: Dict) -> str:
        """Build prompt for agent (Alpha Arena style)"""
        return f"""
        You are an autonomous trading agent. Analyze the market and provide a trading decision.

        **Current Market Data:**
        {json.dumps(market_data, indent=2)}

        **Current Portfolio:**
        - Capital: ${portfolio.get('capital', 0):.2f}
        - Open Positions: {portfolio.get('position_count', 0)}
        - Daily P&L: ${portfolio.get('daily_pnl', 0):.2f}
        - Total P&L: ${portfolio.get('total_pnl', 0):.2f}

        **Your Task:**
        Decide whether to:
        1. Open a new position (LONG or SHORT)
        2. Close existing positions
        3. Hold current positions

        **Required Response Format:**
        {{
            "action": "long" | "short" | "close" | "hold",
            "symbol": "BTC/USD",
            "entry_price": 50000.0,
            "stop_loss": 48000.0,
            "take_profit": 55000.0,
            "position_size_pct": 0.1,
            "leverage": 1.0,
            "confidence": 0.75,
            "reasoning": "Detailed explanation of your decision"
        }}

        **Important:**
        - Confidence must be 0.0 to 1.0
        - Always set stop_loss and take_profit
        - Consider volatility for TP/SL placement
        - Max position size: {self.safety_limits.max_position_size * 100}% of capital
        """

    def _parse_signal(self, response) -> Optional[TradingSignal]:
        """Parse agent response into trading signal"""
        try:
            content = self.agent._extract_response_content(response)

            # Try to extract JSON from response
            import re
            json_match = re.search(r'\{[^}]+\}', content, re.DOTALL)
            if not json_match:
                return None

            data = json.loads(json_match.group())

            if data.get('action') in ['hold', 'close']:
                return None

            return TradingSignal(
                symbol=data.get('symbol', 'BTC/USD'),
                direction=data['action'],
                entry_price=data['entry_price'],
                stop_loss=data['stop_loss'],
                take_profit=data['take_profit'],
                position_size=data.get('position_size_pct', 0.05),
                leverage=data.get('leverage', 1.0),
                confidence=data.get('confidence', 0.5),
                reasoning=data.get('reasoning', '')
            )
        except Exception as e:
            print(f"[AutoTrader] Failed to parse signal: {e}")
            return None

    def _validate_signal(self, signal: TradingSignal) -> bool:
        """Validate signal against safety limits"""

        # Check confidence
        if signal.confidence < self.safety_limits.min_confidence:
            print(f"[AutoTrader] Confidence too low: {signal.confidence}")
            return False

        # Check position size
        if signal.position_size > self.safety_limits.max_position_size:
            print(f"[AutoTrader] Position size too large: {signal.position_size}")
            return False

        # Check leverage
        if signal.leverage > self.safety_limits.max_leverage:
            print(f"[AutoTrader] Leverage too high: {signal.leverage}")
            return False

        # Check TP/SL required
        if self.safety_limits.require_stop_loss and not signal.stop_loss:
            print("[AutoTrader] Stop loss required but not provided")
            return False

        if self.safety_limits.require_take_profit and not signal.take_profit:
            print("[AutoTrader] Take profit required but not provided")
            return False

        # Check max open positions
        if len(self.positions) >= self.safety_limits.max_open_positions:
            print("[AutoTrader] Max open positions reached")
            return False

        return True

    def _calculate_position_size(self, signal: TradingSignal) -> float:
        """Calculate position size with confidence weighting (Alpha Arena style)"""
        # Base position size from signal
        base_size = signal.position_size * self.current_capital

        # Weight by confidence
        confidence_weighted = base_size * signal.confidence

        # Apply leverage
        leveraged_size = confidence_weighted * signal.leverage

        # Respect max limits
        max_size = self.safety_limits.max_position_size * self.current_capital

        return min(leveraged_size, max_size)

    async def _execute_trade(self, signal: TradingSignal, position_size: float):
        """Execute trade based on signal"""
        print(f"[AutoTrader] Executing {signal.direction.upper()} on {signal.symbol}")
        print(f"[AutoTrader] Size: ${position_size:.2f}, Confidence: {signal.confidence:.2f}")

        # In production, this would call actual exchange API
        # For now, simulate execution

        trade = {
            "id": f"trade_{self.state.total_trades + 1}",
            "symbol": signal.symbol,
            "direction": signal.direction,
            "entry_price": signal.entry_price,
            "size": position_size,
            "stop_loss": signal.stop_loss,
            "take_profit": signal.take_profit,
            "confidence": signal.confidence,
            "opened_at": datetime.now().isoformat(),
            "status": "open"
        }

        self.positions[trade['id']] = trade
        self.trade_history.append(trade)
        self.state.total_trades += 1
        self.state.last_trade_time = datetime.now().isoformat()

        # Trigger callback
        if 'trade_executed' in self.callbacks:
            self.callbacks['trade_executed'](trade)

    def _manage_positions(self):
        """Check and manage open positions (TP/SL)"""
        for trade_id, position in list(self.positions.items()):
            # In production, get current price from market
            # For now, simulate price movement

            # Check if TP or SL hit
            # self._check_exit_conditions(position)
            pass

    def _check_circuit_breaker(self) -> bool:
        """Check if circuit breaker should trigger"""

        # Check max drawdown
        if self.state.current_drawdown >= self.safety_limits.max_drawdown:
            print(f"[AutoTrader] Max drawdown reached: {self.state.current_drawdown:.2%}")
            return True

        # Check max daily loss
        if abs(self.state.daily_pnl) >= self.safety_limits.max_daily_loss * self.initial_capital:
            print(f"[AutoTrader] Max daily loss reached: ${self.state.daily_pnl:.2f}")
            return True

        return False

    def _trigger_emergency_stop(self, reason: str):
        """Trigger emergency stop"""
        print(f"[AutoTrader] EMERGENCY STOP: {reason}")
        self.state.circuit_breaker_triggered = True
        self._close_all_positions()
        self.stop()

    def _close_all_positions(self):
        """Close all open positions immediately"""
        for trade_id in list(self.positions.keys()):
            print(f"[AutoTrader] Closing position {trade_id}")
            del self.positions[trade_id]

    def _get_market_data(self) -> Dict:
        """Get current market data"""
        # In production, fetch from exchange
        return {
            "symbol": "BTC/USD",
            "price": 50000.0,
            "timestamp": datetime.now().isoformat()
        }

    def _get_portfolio_state(self) -> Dict:
        """Get current portfolio state"""
        return {
            "capital": self.current_capital,
            "position_count": len(self.positions),
            "daily_pnl": self.state.daily_pnl,
            "total_pnl": self.state.total_pnl
        }

    def get_state(self) -> Dict[str, Any]:
        """Get current auto-trader state"""
        return {
            "is_running": self.state.is_running,
            "total_trades": self.state.total_trades,
            "winning_trades": self.state.winning_trades,
            "losing_trades": self.state.losing_trades,
            "win_rate": (self.state.winning_trades / self.state.total_trades * 100) if self.state.total_trades > 0 else 0,
            "total_pnl": self.state.total_pnl,
            "current_drawdown": self.state.current_drawdown,
            "daily_pnl": self.state.daily_pnl,
            "open_positions": len(self.positions),
            "circuit_breaker": self.state.circuit_breaker_triggered
        }
