"""
Risk Management Team - Controls & Compliance

The risk team responsible for monitoring and controlling
risk across all activities. Has authority to halt trading.

Team Structure:
- Risk Quant (team leader, risk measurement)
- Compliance Officer (regulatory compliance)
"""

from typing import Optional
from agno.team import Team

from ..agents import (
    create_risk_quant,
    create_compliance_officer,
)
from ..config import get_config, RenTechConfig
from ..utils.model_factory import create_model_from_config


# Global team instance
_risk_team: Optional[Team] = None


def create_risk_team(config: Optional[RenTechConfig] = None) -> Team:
    """
    Create the Risk Management Team.

    This team is responsible for:
    - Risk measurement and monitoring
    - Position and exposure limits
    - Regulatory compliance
    - Trade surveillance

    Both members must agree on risk issues - this is a
    consensus-required team for critical decisions.
    """
    cfg = config or get_config()

    # Create individual agents
    risk_quant = create_risk_quant(cfg)
    compliance_officer = create_compliance_officer(cfg)

    # Create the team
    # Note: Risk Quant is first in members list to act as coordinator
    team = Team(
        name="Risk Management Team",
        description="""Monitors and controls risk across all activities.
        Has authority to halt trading if risk limits are breached.
        Reports directly to Investment Committee.""",

        # Team composition (no 'leader' param in Agno - first member coordinates)
        members=[
            risk_quant,  # Acts as team coordinator
            compliance_officer,
        ],

        # Model for team-level decisions
        model=create_model_from_config({
            "temperature": cfg.models.temperature,
            "max_tokens": cfg.models.max_tokens,
        }),

        # Both see all risk queries
        share_member_interactions=True,
        add_team_history_to_members=True,  # Correct param name

        # Memory for learning from past events
        enable_agentic_memory=cfg.enable_memory,

        # Instructions
        instructions="""
## Risk Management Team Mission

You are the Risk Management Team at Renaissance Technologies.
Your mission is to protect the firm from excessive risk while
enabling profitable trading within defined limits.

## Team Authority

You have the authority to:
- APPROVE trades within risk limits
- REJECT trades that breach limits
- HALT trading if systemic risk detected
- ESCALATE to Investment Committee

## Risk Framework

### Hard Limits (Cannot be breached)
- Daily VaR: 2% of NAV
- Max Drawdown: 15%
- Position Size: 5% of NAV
- Sector Exposure: 25% of NAV
- Leverage: 12.5x gross

### Soft Limits (Warning triggers)
- Daily VaR: 1.5% (warning)
- Drawdown: 10% (escalation)
- Concentration: approaching limits

## Team Workflow

1. **Pre-Trade Risk Check** (Both members)
   - Risk Quant: Calculate marginal VaR, exposures
   - Compliance: Check position limits, restricted list

2. **Real-Time Monitoring** (Continuous)
   - Monitor aggregate risk metrics
   - Watch for unusual patterns
   - Track limit utilization

3. **Post-Trade Analysis** (Daily)
   - Review all trades
   - Update risk models
   - Generate reports

## Consensus Requirement

For risk REJECTIONS, both team members should agree.
If either member identifies a critical issue, the trade
should be blocked pending further review.

## Escalation Protocol

Escalate immediately to Investment Committee:
- Any hard limit breach
- Drawdown > 10%
- Unusual market conditions
- Regulatory concerns
- Potential compliance issues

## Risk Philosophy

"Capital preservation is paramount. We can always
make money tomorrow if we protect capital today."

Never compromise on risk limits. When in doubt,
reduce exposure.
""",

        markdown=True,
        show_members_responses=True,
    )

    return team


def get_risk_team() -> Team:
    """Get or create the global Risk Team instance"""
    global _risk_team
    if _risk_team is None:
        _risk_team = create_risk_team()
    return _risk_team


def reset_risk_team():
    """Reset the global Risk Team instance"""
    global _risk_team
    _risk_team = None
