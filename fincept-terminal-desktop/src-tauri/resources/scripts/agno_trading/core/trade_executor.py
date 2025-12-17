"""
Trade Executor
Executes trades based on agent signals with risk management
"""

import json
import sys
from typing import Dict, Any, Optional
from datetime import datetime


class TradeExecutor:
    """
    Executes trades with risk validation and position management
    """

    def __init__(self, risk_config: Optional[Dict] = None):
        """
        Initialize trade executor

        Args:
            risk_config: Risk management configuration
        """
        self.risk_config = risk_config or {
            'max_position_size_pct': 0.10,  # 10% of portfolio
            'max_leverage': 2.0,
            'max_daily_loss_pct': 0.05,  # 5% max daily loss
            'min_risk_reward_ratio': 1.5,
            'max_open_positions': 5,
            'enable_stop_loss': True,
            'enable_take_profit': True
        }

    def validate_trade_signal(
        self,
        signal: Dict[str, Any],
        portfolio_data: Dict[str, Any]
    ) -> Dict[str, Any]:
        """
        Validate trade signal against risk management rules

        Args:
            signal: Trade signal from agent
            portfolio_data: Current portfolio state

        Returns:
            Validation result with approved status and adjusted parameters
        """
        validation = {
            'approved': True,
            'reasons': [],
            'warnings': [],
            'adjusted_signal': signal.copy()
        }

        portfolio_value = portfolio_data.get('total_value', 10000.0)
        daily_pnl = portfolio_data.get('daily_pnl', 0)
        open_positions = len(portfolio_data.get('positions', []))

        # Check 1: Daily loss limit
        if daily_pnl < 0:
            daily_loss_pct = abs(daily_pnl / portfolio_value)
            if daily_loss_pct >= self.risk_config['max_daily_loss_pct']:
                validation['approved'] = False
                validation['reasons'].append(
                    f"Daily loss limit reached ({daily_loss_pct*100:.1f}% >= {self.risk_config['max_daily_loss_pct']*100}%)"
                )
                return validation

        # Check 2: Max open positions
        if open_positions >= self.risk_config['max_open_positions']:
            validation['approved'] = False
            validation['reasons'].append(
                f"Maximum open positions reached ({open_positions}/{self.risk_config['max_open_positions']})"
            )
            return validation

        # Check 3: Position size limit
        intended_value = signal.get('quantity', 0) * signal.get('entry_price', 0)
        position_size_pct = intended_value / portfolio_value

        max_allowed_pct = self.risk_config['max_position_size_pct']

        if position_size_pct > max_allowed_pct:
            # Adjust quantity to fit within limits
            adjusted_quantity = (portfolio_value * max_allowed_pct) / signal.get('entry_price', 1)
            validation['adjusted_signal']['quantity'] = adjusted_quantity
            validation['warnings'].append(
                f"Position size reduced from {position_size_pct*100:.1f}% to {max_allowed_pct*100}%"
            )

        # Check 4: Risk/Reward ratio (if stop loss and take profit provided)
        if signal.get('stop_loss') and signal.get('take_profit'):
            entry_price = signal.get('entry_price', 0)
            stop_loss = signal['stop_loss']
            take_profit = signal['take_profit']
            side = signal.get('side', 'buy')

            if side == 'buy':
                risk = abs(entry_price - stop_loss)
                reward = abs(take_profit - entry_price)
            else:  # sell/short
                risk = abs(stop_loss - entry_price)
                reward = abs(entry_price - take_profit)

            rr_ratio = reward / risk if risk > 0 else 0

            if rr_ratio < self.risk_config['min_risk_reward_ratio']:
                validation['approved'] = False
                validation['reasons'].append(
                    f"Risk/Reward ratio too low ({rr_ratio:.2f} < {self.risk_config['min_risk_reward_ratio']})"
                )
                return validation

        # Check 5: Mandatory stop loss
        if self.risk_config['enable_stop_loss'] and not signal.get('stop_loss'):
            # Auto-calculate stop loss (2% from entry)
            entry_price = signal.get('entry_price', 0)
            side = signal.get('side', 'buy')

            if side == 'buy':
                auto_stop_loss = entry_price * 0.98  # 2% below
            else:
                auto_stop_loss = entry_price * 1.02  # 2% above

            validation['adjusted_signal']['stop_loss'] = auto_stop_loss
            validation['warnings'].append(f"Auto stop-loss added at {auto_stop_loss:.2f}")

        # Check 6: Mandatory take profit
        if self.risk_config['enable_take_profit'] and not signal.get('take_profit'):
            # Auto-calculate take profit (4% from entry for 2:1 R:R)
            entry_price = signal.get('entry_price', 0)
            side = signal.get('side', 'buy')

            if side == 'buy':
                auto_take_profit = entry_price * 1.04  # 4% above
            else:
                auto_take_profit = entry_price * 0.96  # 4% below

            validation['adjusted_signal']['take_profit'] = auto_take_profit
            validation['warnings'].append(f"Auto take-profit added at {auto_take_profit:.2f}")

        return validation

    def execute_trade(
        self,
        signal: Dict[str, Any],
        portfolio_data: Dict[str, Any],
        agent_id: str,
        model: str,
        paper_trading: bool = True
    ) -> Dict[str, Any]:
        """
        Execute trade with risk validation

        Args:
            signal: Trade signal
            portfolio_data: Current portfolio state
            agent_id: Agent identifier
            model: Model name
            paper_trading: Whether this is paper trading

        Returns:
            Execution result
        """
        start_time = datetime.now()

        # Validate signal
        validation = self.validate_trade_signal(signal, portfolio_data)

        if not validation['approved']:
            return {
                'success': False,
                'error': 'Trade rejected by risk management',
                'reasons': validation['reasons'],
                'warnings': validation['warnings']
            }

        # Use adjusted signal
        adjusted_signal = validation['adjusted_signal']

        # Prepare trade data
        trade_data = {
            'agent_id': agent_id,
            'model': model,
            'symbol': adjusted_signal['symbol'],
            'side': adjusted_signal.get('side', 'buy'),
            'order_type': adjusted_signal.get('order_type', 'market'),
            'quantity': adjusted_signal.get('quantity', 0),
            'entry_price': adjusted_signal.get('entry_price', 0),
            'stop_loss': adjusted_signal.get('stop_loss'),
            'take_profit': adjusted_signal.get('take_profit'),
            'reasoning': adjusted_signal.get('reasoning', ''),
            'metadata': {
                'strategy': adjusted_signal.get('strategy'),
                'confidence': adjusted_signal.get('confidence'),
                'indicators': adjusted_signal.get('indicators', {}),
                'original_signal': signal
            },
            'is_paper_trade': paper_trading,
            'entry_timestamp': int(datetime.now().timestamp()),
            'execution_time_ms': 0
        }

        # Record in database
        try:
            from agno_trading.db.database_manager import get_db_manager
            db = get_db_manager()
            trade_id = db.record_trade(trade_data)

            # Log decision
            db.log_decision({
                'agent_id': agent_id,
                'model': model,
                'decision_type': 'trade',
                'symbol': adjusted_signal['symbol'],
                'decision': f"{adjusted_signal['side'].upper()} {adjusted_signal['quantity']:.4f} @ {adjusted_signal['entry_price']:.2f}",
                'reasoning': adjusted_signal.get('reasoning'),
                'confidence': adjusted_signal.get('confidence'),
                'timestamp': int(datetime.now().timestamp())
            })

            execution_time_ms = int((datetime.now() - start_time).total_seconds() * 1000)

            return {
                'success': True,
                'trade_id': trade_id,
                'trade_data': trade_data,
                'warnings': validation['warnings'],
                'execution_time_ms': execution_time_ms
            }

        except Exception as e:
            return {
                'success': False,
                'error': f"Trade execution failed: {str(e)}"
            }

    def close_position(
        self,
        trade_id: int,
        exit_price: float,
        reason: str = 'manual'
    ) -> Dict[str, Any]:
        """
        Close an open position

        Args:
            trade_id: Trade ID to close
            exit_price: Exit price
            reason: Reason for closing

        Returns:
            Close result
        """
        try:
            from agno_trading.db.database_manager import get_db_manager
            db = get_db_manager()

            db.close_trade(trade_id, exit_price)

            return {
                'success': True,
                'trade_id': trade_id,
                'exit_price': exit_price,
                'reason': reason
            }

        except Exception as e:
            return {
                'success': False,
                'error': f"Failed to close position: {str(e)}"
            }


# Global instance
_trade_executor = None

def get_trade_executor() -> TradeExecutor:
    """Get singleton trade executor"""
    global _trade_executor
    if _trade_executor is None:
        _trade_executor = TradeExecutor()
    return _trade_executor
