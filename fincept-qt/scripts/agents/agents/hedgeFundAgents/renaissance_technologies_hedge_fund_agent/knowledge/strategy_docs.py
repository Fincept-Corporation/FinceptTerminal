"""
Strategy Documentation Knowledge

Knowledge base for trading strategy documentation including:
- Strategy specifications
- Risk parameters
- Execution guidelines
- Position sizing rules
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
import uuid

from .base import (
    RenTechKnowledge,
    KnowledgeDocument,
    KnowledgeCategory,
    get_knowledge_base,
)


class StrategyKnowledge:
    """
    Strategy Documentation Manager.

    Provides structured access to trading strategy specifications,
    risk parameters, and operational guidelines.
    """

    def __init__(self, knowledge_base: Optional[RenTechKnowledge] = None):
        self.kb = knowledge_base or get_knowledge_base()
        self.category = KnowledgeCategory.STRATEGY

    def add_strategy_spec(
        self,
        strategy_name: str,
        strategy_type: str,
        description: str,
        universe: List[str],
        signals_used: List[str],
        entry_rules: List[str],
        exit_rules: List[str],
        risk_parameters: Dict[str, Any],
        allocation: float,
        status: str = "active",
    ) -> str:
        """
        Add a strategy specification document.

        Args:
            strategy_name: Name of the strategy
            strategy_type: Type (mean_reversion, momentum, stat_arb, etc.)
            description: Strategy description
            universe: Trading universe
            signals_used: Signals incorporated
            entry_rules: Entry criteria
            exit_rules: Exit criteria
            risk_parameters: Risk limits and parameters
            allocation: Target allocation (% of portfolio)
            status: Strategy status (active, paused, retired)

        Returns:
            Document ID
        """
        content = f"""
# Strategy Specification: {strategy_name}

## Overview
- **Type**: {strategy_type}
- **Status**: {status}
- **Target Allocation**: {allocation}%

## Description
{description}

## Trading Universe
{chr(10).join([f"- {u}" for u in universe])}

## Signals Used
{chr(10).join([f"- {s}" for s in signals_used])}

## Entry Rules
{chr(10).join([f"{i+1}. {r}" for i, r in enumerate(entry_rules)])}

## Exit Rules
{chr(10).join([f"{i+1}. {r}" for i, r in enumerate(exit_rules)])}

## Risk Parameters
- Max Position Size: {risk_parameters.get('max_position_pct', 'N/A')}%
- Max Sector Exposure: {risk_parameters.get('max_sector_pct', 'N/A')}%
- Stop Loss: {risk_parameters.get('stop_loss_pct', 'N/A')}%
- Take Profit: {risk_parameters.get('take_profit_pct', 'N/A')}%
- Max Drawdown: {risk_parameters.get('max_drawdown_pct', 'N/A')}%
- Max Leverage: {risk_parameters.get('max_leverage', 'N/A')}x
- Turnover Target: {risk_parameters.get('turnover_target', 'N/A')}x annual

## Execution Guidelines
- Urgency: {risk_parameters.get('execution_urgency', 'normal')}
- Participation Rate: {risk_parameters.get('participation_rate', 'N/A')}%
- Algorithm: {risk_parameters.get('default_algo', 'TWAP')}

## Performance Targets
- Target Sharpe: {risk_parameters.get('target_sharpe', 'N/A')}
- Target Annual Return: {risk_parameters.get('target_return', 'N/A')}%
"""

        doc = KnowledgeDocument(
            id=f"strategy_{strategy_name.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Strategy: {strategy_name}",
            content=content,
            metadata={
                "strategy_name": strategy_name,
                "strategy_type": strategy_type,
                "status": status,
                "allocation": allocation,
                "risk_parameters": risk_parameters,
                "doc_type": "strategy_spec",
            },
            tags=["strategy", strategy_type.lower(), strategy_name.lower()],
            importance=0.95,  # Strategy specs are critical
        )

        return self.kb.add_document(doc)

    def add_risk_policy(
        self,
        policy_name: str,
        policy_type: str,
        description: str,
        rules: List[str],
        limits: Dict[str, Any],
        escalation_procedure: List[str],
        effective_date: str,
    ) -> str:
        """Add a risk policy document"""
        content = f"""
# Risk Policy: {policy_name}

## Policy Type: {policy_type}
## Effective Date: {effective_date}

## Description
{description}

## Rules
{chr(10).join([f"{i+1}. {r}" for i, r in enumerate(rules)])}

## Limits
| Metric | Soft Limit | Hard Limit |
|--------|------------|------------|
{chr(10).join([f"| {k} | {v.get('soft', 'N/A')} | {v.get('hard', 'N/A')} |" for k, v in limits.items()])}

## Breach Response
1. Soft limit breach: Warning, monitor closely
2. Hard limit breach: Immediate action required
3. Escalation to IC if breach persists

## Escalation Procedure
{chr(10).join([f"{i+1}. {e}" for i, e in enumerate(escalation_procedure)])}

## Monitoring
- Frequency: Real-time
- Reports: Daily risk report
- Reviews: Weekly risk committee
"""

        doc = KnowledgeDocument(
            id=f"policy_{policy_name.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Policy: {policy_name}",
            content=content,
            metadata={
                "policy_name": policy_name,
                "policy_type": policy_type,
                "limits": limits,
                "effective_date": effective_date,
                "doc_type": "risk_policy",
            },
            tags=["policy", "risk", policy_type.lower()],
            importance=0.95,
        )

        return self.kb.add_document(doc)

    def add_execution_playbook(
        self,
        playbook_name: str,
        scenario: str,
        description: str,
        steps: List[str],
        algorithms: List[str],
        parameters: Dict[str, Any],
        examples: List[str],
    ) -> str:
        """Add an execution playbook"""
        content = f"""
# Execution Playbook: {playbook_name}

## Scenario: {scenario}

## Description
{description}

## When to Use
This playbook applies when:
{chr(10).join([f"- {e}" for e in examples])}

## Execution Steps
{chr(10).join([f"{i+1}. {s}" for i, s in enumerate(steps)])}

## Recommended Algorithms
{chr(10).join([f"- {a}" for a in algorithms])}

## Parameters
- Participation Rate: {parameters.get('participation_rate', 'N/A')}%
- Duration: {parameters.get('duration_minutes', 'N/A')} minutes
- Urgency Level: {parameters.get('urgency', 'normal')}
- Max Spread to Pay: {parameters.get('max_spread_bps', 'N/A')} bps
- Price Limit Offset: {parameters.get('limit_offset_bps', 'N/A')} bps

## Venue Preferences
- Primary: {parameters.get('primary_venue', 'Best available')}
- Dark Pool: {parameters.get('dark_pool_pct', 'N/A')}%

## Risk Controls
- Cancel if: {parameters.get('cancel_condition', 'N/A')}
- Pause if: {parameters.get('pause_condition', 'N/A')}
"""

        doc = KnowledgeDocument(
            id=f"playbook_{playbook_name.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Playbook: {playbook_name}",
            content=content,
            metadata={
                "playbook_name": playbook_name,
                "scenario": scenario,
                "algorithms": algorithms,
                "parameters": parameters,
                "doc_type": "execution_playbook",
            },
            tags=["execution", "playbook", scenario.lower().replace(" ", "_")],
            importance=0.8,
        )

        return self.kb.add_document(doc)

    def add_position_sizing_guide(
        self,
        guide_name: str,
        methodology: str,
        description: str,
        formula: str,
        parameters: Dict[str, Any],
        examples: List[Dict[str, Any]],
    ) -> str:
        """Add position sizing methodology"""
        content = f"""
# Position Sizing Guide: {guide_name}

## Methodology: {methodology}

## Description
{description}

## Formula
{formula}

## Parameters
{chr(10).join([f"- {k}: {v}" for k, v in parameters.items()])}

## Examples

{chr(10).join([f'''
### Example {i+1}
- Signal Strength: {ex.get('signal_strength', 'N/A')}
- Confidence: {ex.get('confidence', 'N/A')}
- Volatility: {ex.get('volatility', 'N/A')}%
- **Calculated Size**: {ex.get('size_pct', 'N/A')}%
''' for i, ex in enumerate(examples)])}

## Constraints Applied
- Max position: 5% of NAV (hard limit)
- Min position: 0.1% of NAV (to be meaningful)
- Liquidity adjustment: Size ≤ 1% of ADV
- Correlation penalty: Reduce if >0.7 corr with existing

## Kelly Fraction
We use {parameters.get('kelly_fraction', 0.25)} of full Kelly criterion
to account for estimation error and fat tails.
"""

        doc = KnowledgeDocument(
            id=f"sizing_{guide_name.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Sizing: {guide_name}",
            content=content,
            metadata={
                "guide_name": guide_name,
                "methodology": methodology,
                "parameters": parameters,
                "doc_type": "position_sizing",
            },
            tags=["sizing", "position", methodology.lower()],
            importance=0.85,
        )

        return self.kb.add_document(doc)

    def search_strategies(
        self,
        query: str,
        strategy_type: Optional[str] = None,
        status: Optional[str] = None,
        limit: int = 10,
    ) -> List[Dict[str, Any]]:
        """Search strategy documents"""
        results = self.kb.search(
            query=query,
            category=self.category,
            limit=limit,
        )

        if strategy_type:
            results = [r for r in results if r.get("metadata", {}).get("strategy_type") == strategy_type]

        if status:
            results = [r for r in results if r.get("metadata", {}).get("status") == status]

        return results

    def get_active_strategies(self) -> List[KnowledgeDocument]:
        """Get all active strategies"""
        docs = self.kb.get_documents_by_category(self.category)
        return [
            d for d in docs
            if d.metadata.get("doc_type") == "strategy_spec"
            and d.metadata.get("status") == "active"
        ]

    def get_risk_policies(self) -> List[KnowledgeDocument]:
        """Get all risk policies"""
        docs = self.kb.get_documents_by_category(self.category)
        return [d for d in docs if d.metadata.get("doc_type") == "risk_policy"]


# Global instance
_strategy_knowledge: Optional[StrategyKnowledge] = None


def get_strategy_knowledge() -> StrategyKnowledge:
    """Get the global strategy knowledge instance"""
    global _strategy_knowledge
    if _strategy_knowledge is None:
        _strategy_knowledge = StrategyKnowledge()
    return _strategy_knowledge
