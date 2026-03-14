"""
Research Lead Agent

Head of the Research Team, coordinates signal discovery and validation.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import RESEARCH_LEAD
from ..config import get_config
from .tools.signal_tools import SignalAnalysisTools
from .tools.market_data_tools import MarketDataTools
from .tools.agno_tools import get_yfinance_tools, get_duckduckgo_tools, get_wikipedia_tools


def create_research_lead(config=None) -> Agent:
    """
    Create the Research Lead agent.

    Responsibilities:
    - Coordinate research team
    - Validate signal quality
    - Prioritize research directions
    - Final research approval before IC
    """
    cfg = config or get_config()

    tools = [
        SignalAnalysisTools(),
        MarketDataTools(),
        # Agno built-in tools
        get_yfinance_tools(),
        get_duckduckgo_tools(),
        get_wikipedia_tools(),
    ]

    additional_instructions = """
## Research Validation Framework

When validating signals from your team:

1. **Statistical Requirements**
   - P-value must be < 0.01 (our threshold)
   - Information Coefficient > 0.03
   - Out-of-sample Sharpe > 1.0
   - No evidence of overfitting

2. **Signal Quality Checklist**
   □ Economically sensible explanation
   □ Robust across time periods
   □ Robust across market regimes
   □ Not already crowded/decayed
   □ Capacity sufficient for our AUM
   □ Transaction costs considered

3. **Team Coordination**
   - Assign tasks based on expertise
   - Encourage collaboration
   - Share learnings across team
   - Escalate to IC when needed

## Output Format for Signal Reviews

```
SIGNAL REVIEW: [Signal Name]

STATISTICAL ASSESSMENT:
- P-value: [value] [PASS/FAIL]
- IC: [value] [PASS/FAIL]
- OOS Sharpe: [value] [PASS/FAIL]

QUALITATIVE ASSESSMENT:
- Economic rationale: [STRONG/MODERATE/WEAK]
- Regime robustness: [PASS/FAIL]
- Crowding risk: [LOW/MODERATE/HIGH]

RECOMMENDATION: [ADVANCE_TO_IC/REVISE/REJECT]

REQUIRED CHANGES (if any):
- [Change 1]
- [Change 2]
```

## Research Priorities

Focus research on:
1. Non-obvious patterns (avoid crowded factors)
2. Alternative data integration
3. Cross-asset relationships
4. Microstructure alpha
5. Regime-adaptive signals
"""

    agent = create_agent_from_persona(
        persona=RESEARCH_LEAD,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_research_lead() -> Agent:
    """Get or create the Research Lead agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=RESEARCH_LEAD,
        tools=[
            SignalAnalysisTools(),
            MarketDataTools(),
            get_yfinance_tools(),
            get_duckduckgo_tools(),
            get_wikipedia_tools(),
        ],
    )
