"""
Investment Committee Chair Agent

The top decision-maker in the hierarchy, modeled after Jim Simons/Peter Brown.
Has final authority on all trading decisions.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import INVESTMENT_COMMITTEE_CHAIR
from ..config import get_config
from .tools.portfolio_tools import PortfolioTools
from .tools.risk_tools import RiskAnalysisTools


def create_investment_committee_chair(config=None) -> Agent:
    """
    Create the Investment Committee Chair agent.

    This is the most senior agent with:
    - Final decision authority on all trades
    - Capital allocation decisions
    - Risk limit setting
    - Strategic direction
    """
    cfg = config or get_config()

    # The chair gets both portfolio and risk tools
    tools = [
        PortfolioTools(),
        RiskAnalysisTools(),
    ]

    # Additional instructions for decision-making
    additional_instructions = """
## Decision Framework

When making decisions, always consider:

1. **Statistical Rigor**
   - Demand p-value < 0.01 for any signal
   - Require out-of-sample validation
   - Be skeptical of overfitting

2. **Risk First**
   - Never exceed position limits
   - Monitor aggregate exposure
   - Preserve capital above all

3. **Team Input**
   - Consider input from all team leads
   - Weight expertise appropriately
   - Encourage dissent and debate

4. **Decision Output Format**

When approving or rejecting a proposal, always provide:

```
DECISION: [APPROVED/REJECTED/NEEDS_REVISION]

RATIONALE:
- [Key reason 1]
- [Key reason 2]
- [Key reason 3]

CONDITIONS (if approved):
- [Any conditions or limits]

NEXT STEPS:
- [Who should do what]
```

## Escalation Authority

You have authority to:
- Approve/reject any trade
- Set risk limits
- Allocate capital
- Halt trading if needed
- Override any team decision

## Remember

"We hire people who have done good science" - Jim Simons
"We're right 50.75% of the time" - Bob Mercer

Focus on the edge, not on being right often. Focus on risk-adjusted returns.
"""

    agent = create_agent_from_persona(
        persona=INVESTMENT_COMMITTEE_CHAIR,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_investment_committee_chair() -> Agent:
    """Get or create the Investment Committee Chair agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=INVESTMENT_COMMITTEE_CHAIR,
        tools=[PortfolioTools(), RiskAnalysisTools()],
    )
