"""
Agent Evolution System
Self-learning agents that improve from trading history
"""

import json
import sys
from typing import Dict, Any, List, Optional
from datetime import datetime
from collections import Counter


class AgentEvolution:
    """
    Manages agent self-evolution through performance analysis
    """

    def __init__(self):
        """Initialize evolution system"""
        self.evolution_triggers = {
            'loss_streak': 3,  # Evolve after 3 consecutive losses
            'poor_win_rate': 0.40,  # Evolve if win rate below 40%
            'large_drawdown': 0.10,  # Evolve if drawdown > 10%
            'consistent_wins': 5,  # Reinforce after 5 consecutive wins
            'trades_milestone': 20  # Evolve every 20 trades
        }

    def should_evolve(self, agent_id: str, performance: Dict[str, Any]) -> tuple[bool, str]:
        """
        Check if agent should evolve based on performance

        Args:
            agent_id: Agent ID
            performance: Performance metrics

        Returns:
            (should_evolve, trigger_reason)
        """
        # Check loss streak
        if performance.get('consecutive_losses', 0) >= self.evolution_triggers['loss_streak']:
            return True, 'loss_streak'

        # Check poor win rate
        if performance.get('total_trades', 0) >= 10:  # Need minimum trades
            if performance.get('win_rate', 100) < self.evolution_triggers['poor_win_rate'] * 100:
                return True, 'poor_win_rate'

        # Check large drawdown
        if performance.get('current_drawdown', 0) >= self.evolution_triggers['large_drawdown']:
            return True, 'large_drawdown'

        # Check trade milestones
        if performance.get('total_trades', 0) > 0 and performance.get('total_trades', 0) % self.evolution_triggers['trades_milestone'] == 0:
            return True, 'trades_milestone'

        return False, None

    def analyze_patterns(self, agent_id: str) -> Dict[str, Any]:
        """
        Analyze trading patterns from history

        Args:
            agent_id: Agent ID

        Returns:
            Pattern analysis
        """
        from agno_trading.db.database_manager import get_db_manager
        db = get_db_manager()

        # Get trade history
        trades = db.get_agent_trades(agent_id, limit=100)

        if not trades:
            return {
                'total_analyzed': 0,
                'patterns': {}
            }

        # Separate winners and losers
        winners = [t for t in trades if t['pnl'] > 0]
        losers = [t for t in trades if t['pnl'] < 0]

        analysis = {
            'total_analyzed': len(trades),
            'winners_count': len(winners),
            'losers_count': len(losers),
            'patterns': {}
        }

        # Pattern 1: Winning vs Losing Symbols
        winning_symbols = Counter([t['symbol'] for t in winners])
        losing_symbols = Counter([t['symbol'] for t in losers])

        analysis['patterns']['best_symbols'] = [sym for sym, _ in winning_symbols.most_common(3)]
        analysis['patterns']['worst_symbols'] = [sym for sym, _ in losing_symbols.most_common(3)]

        # Pattern 2: Winning vs Losing Sides
        winning_sides = Counter([t['side'] for t in winners])
        losing_sides = Counter([t['side'] for t in losers])

        analysis['patterns']['best_side'] = winning_sides.most_common(1)[0][0] if winning_sides else 'buy'
        analysis['patterns']['worst_side'] = losing_sides.most_common(1)[0][0] if losing_sides else None

        # Pattern 3: Trade timing (hour of day if available)
        # Pattern 4: Position sizing patterns
        avg_winning_size = sum(t['quantity'] * t['entry_price'] for t in winners) / len(winners) if winners else 0
        avg_losing_size = sum(t['quantity'] * t['entry_price'] for t in losers) / len(losers) if losers else 0

        analysis['patterns']['avg_winning_size'] = avg_winning_size
        analysis['patterns']['avg_losing_size'] = avg_losing_size

        if avg_winning_size < avg_losing_size:
            analysis['patterns']['sizing_issue'] = 'winning_trades_too_small'
        elif avg_losing_size > avg_winning_size * 1.5:
            analysis['patterns']['sizing_issue'] = 'losing_trades_too_large'
        else:
            analysis['patterns']['sizing_issue'] = None

        # Pattern 5: Stop loss hit rate
        trades_with_sl = [t for t in trades if t['stop_loss'] is not None]
        if trades_with_sl:
            sl_hit_count = sum(1 for t in losers if t['stop_loss'] and abs(t['exit_price'] - t['stop_loss']) / t['stop_loss'] < 0.01)
            analysis['patterns']['stop_loss_hit_rate'] = sl_hit_count / len(losers) if losers else 0
        else:
            analysis['patterns']['stop_loss_hit_rate'] = 0

        # Pattern 6: Take profit hit rate
        trades_with_tp = [t for t in trades if t['take_profit'] is not None]
        if trades_with_tp:
            tp_hit_count = sum(1 for t in winners if t['take_profit'] and abs(t['exit_price'] - t['take_profit']) / t['take_profit'] < 0.01)
            analysis['patterns']['take_profit_hit_rate'] = tp_hit_count / len(winners) if winners else 0
        else:
            analysis['patterns']['take_profit_hit_rate'] = 0

        return analysis

    def generate_new_instructions(
        self,
        agent_id: str,
        current_instructions: List[str],
        patterns: Dict[str, Any],
        trigger: str
    ) -> List[str]:
        """
        Generate improved instructions based on patterns

        Args:
            agent_id: Agent ID
            current_instructions: Current agent instructions
            patterns: Pattern analysis
            trigger: Evolution trigger

        Returns:
            New instructions
        """
        new_instructions = current_instructions.copy()

        # Add learning-based instructions
        learning_instructions = []

        # From symbol patterns
        if patterns.get('best_symbols'):
            learning_instructions.append(
                f"LEARNED: You have better win rates with {', '.join(patterns['best_symbols'][:2])}. Favor these symbols when possible."
            )

        if patterns.get('worst_symbols'):
            learning_instructions.append(
                f"LEARNED: You have poor performance with {', '.join(patterns['worst_symbols'][:2])}. Be more cautious or avoid these symbols."
            )

        # From side patterns
        if patterns.get('best_side') and patterns.get('worst_side'):
            if patterns['best_side'] != patterns['worst_side']:
                learning_instructions.append(
                    f"LEARNED: Your {patterns['best_side'].upper()} trades outperform {patterns['worst_side'].upper()} trades. Focus on {patterns['best_side'].upper()} setups."
                )

        # From sizing patterns
        sizing_issue = patterns.get('sizing_issue')
        if sizing_issue == 'winning_trades_too_small':
            learning_instructions.append(
                "LEARNED: Your winning trades are smaller than losing trades. Increase position size on high-confidence setups."
            )
        elif sizing_issue == 'losing_trades_too_large':
            learning_instructions.append(
                "LEARNED: Your losing trades are too large. Reduce position size to manage risk better."
            )

        # From stop loss patterns
        sl_hit_rate = patterns.get('stop_loss_hit_rate', 0)
        if sl_hit_rate > 0.7:
            learning_instructions.append(
                "LEARNED: Your stop losses are being hit frequently (70%+). Consider wider stops or better entry timing."
            )
        elif sl_hit_rate < 0.3:
            learning_instructions.append(
                "LEARNED: Your stop losses are rarely hit. Your risk management is working well - maintain this approach."
            )

        # From take profit patterns
        tp_hit_rate = patterns.get('take_profit_hit_rate', 0)
        if tp_hit_rate > 0.7:
            learning_instructions.append(
                "LEARNED: Your take profit targets are being hit consistently. Consider setting more ambitious targets."
            )
        elif tp_hit_rate < 0.3:
            learning_instructions.append(
                "LEARNED: Your take profit targets are rarely hit. Consider more conservative targets or trailing stops."
            )

        # Trigger-specific instructions
        if trigger == 'loss_streak':
            learning_instructions.append(
                "CAUTION: You're currently in a losing streak. Reduce position sizes by 50% until you have 2 consecutive wins."
            )
        elif trigger == 'poor_win_rate':
            learning_instructions.append(
                "STRATEGY SHIFT: Your win rate is below 40%. Focus on HIGH PROBABILITY setups only. Skip marginal trades."
            )
        elif trigger == 'large_drawdown':
            learning_instructions.append(
                "RISK ALERT: Drawdown is over 10%. STOP TRADING until you review your strategy. Reduce position sizes when resuming."
            )

        # Prepend learning instructions
        new_instructions = learning_instructions + new_instructions

        return new_instructions

    def evolve_agent(
        self,
        agent_id: str,
        model: str,
        current_instructions: List[str],
        trigger: str,
        notes: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Evolve an agent based on performance

        Args:
            agent_id: Agent ID
            model: Model name
            current_instructions: Current instructions
            trigger: Evolution trigger
            notes: Optional notes

        Returns:
            Evolution result
        """
        from agno_trading.db.database_manager import get_db_manager
        db = get_db_manager()

        try:
            # Get current performance
            performance = db.get_agent_performance(agent_id)
            if not performance:
                return {'success': False, 'error': 'Agent performance not found'}

            # Analyze patterns
            patterns = self.analyze_patterns(agent_id)

            # Generate new instructions
            new_instructions = self.generate_new_instructions(
                agent_id,
                current_instructions,
                patterns['patterns'],
                trigger
            )

            # Record evolution
            evolution_data = {
                'agent_id': agent_id,
                'model': model,
                'evolution_trigger': trigger,
                'old_instructions': json.dumps(current_instructions),
                'new_instructions': json.dumps(new_instructions),
                'metrics_before': json.dumps({
                    'total_pnl': performance['total_pnl'],
                    'win_rate': performance['win_rate'],
                    'total_trades': performance['total_trades'],
                    'sharpe_ratio': performance['sharpe_ratio']
                }),
                'metrics_after': json.dumps({}),  # Will be updated later
                'timestamp': int(datetime.now().timestamp()),
                'notes': notes or json.dumps(patterns)
            }

            db.log_evolution(evolution_data)

            return {
                'success': True,
                'agent_id': agent_id,
                'trigger': trigger,
                'old_instructions': current_instructions,
                'new_instructions': new_instructions,
                'patterns': patterns,
                'metrics_before': {
                    'total_pnl': performance['total_pnl'],
                    'win_rate': performance['win_rate'],
                    'total_trades': performance['total_trades']
                }
            }

        except Exception as e:
            print(f"[Evolution] Error: {str(e)}", file=sys.stderr)
            import traceback
            traceback.print_exc(file=sys.stderr)

            return {
                'success': False,
                'error': f"Evolution failed: {str(e)}"
            }

    def get_evolution_summary(self, agent_id: str) -> Dict[str, Any]:
        """
        Get evolution summary for an agent

        Args:
            agent_id: Agent ID

        Returns:
            Evolution summary
        """
        from agno_trading.db.database_manager import get_db_manager
        db = get_db_manager()

        history = db.get_evolution_history(agent_id, limit=10)
        performance = db.get_agent_performance(agent_id)

        return {
            'agent_id': agent_id,
            'total_evolutions': len(history),
            'evolution_history': history,
            'current_performance': performance,
            'last_evolution': history[0] if history else None
        }


# Global instance
_agent_evolution = None

def get_agent_evolution() -> AgentEvolution:
    """Get singleton agent evolution system"""
    global _agent_evolution
    if _agent_evolution is None:
        _agent_evolution = AgentEvolution()
    return _agent_evolution
