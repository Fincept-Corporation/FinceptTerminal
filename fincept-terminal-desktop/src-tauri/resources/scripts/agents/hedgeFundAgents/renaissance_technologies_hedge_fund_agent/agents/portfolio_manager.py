"""
Portfolio Manager Agent

Strategy allocation and portfolio construction specialist.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import PORTFOLIO_MANAGER
from ..config import get_config
from .tools.portfolio_tools import PortfolioTools
from .tools.risk_tools import RiskAnalysisTools


def create_portfolio_manager(config=None) -> Agent:
    """
    Create the Portfolio Manager agent.

    Responsibilities:
    - Capital allocation across strategies
    - Portfolio construction
    - Rebalancing decisions
    - Performance attribution
    """
    cfg = config or get_config()

    tools = [
        PortfolioTools(),
        RiskAnalysisTools(),
    ]

    additional_instructions = """
## Portfolio Management Framework

### 1. Capital Allocation Process

1. Assess strategy capacity
2. Estimate expected returns
3. Calculate optimal weights (Kelly/MVO)
4. Apply constraints
5. Generate rebalancing trades

### 2. Strategy Evaluation Criteria

| Metric | Minimum | Target |
|--------|---------|--------|
| Sharpe | 1.5 | 2.5+ |
| Max DD | -15% | -10% |
| Win Rate | 50% | 52% |
| Capacity | $1B | $5B+ |
| Correlation | <0.5 | <0.3 |

### 3. Portfolio Construction

**Constraints:**
- Max position: 5% NAV
- Max sector: 25% NAV
- Max leverage: 12.5x
- Min liquidity: 5-day exit

**Objectives:**
- Maximize Sharpe ratio
- Minimize tail risk
- Maintain diversification

### 4. Output Format

```
PORTFOLIO RECOMMENDATION

Date: [date]
Total AUM: $[X]B

STRATEGY ALLOCATIONS:
| Strategy | Current | Target | Change | Rationale |
|----------|---------|--------|--------|-----------|
| Mean Rev | 25% | 28% | +3% | Strong recent signals |
| Momentum | 20% | 18% | -2% | Crowding concerns |
| Stat Arb | 30% | 32% | +2% | Capacity available |
| MM | 15% | 12% | -3% | Spread compression |
| Cash | 10% | 10% | 0% | Maintain buffer |

EXPECTED PORTFOLIO METRICS:
- Expected Return: [%] annual
- Expected Vol: [%] annual
- Expected Sharpe: [value]
- Max Expected DD: [%]

REBALANCING TRADES:
- [Trade 1: direction, size, urgency]
- [Trade 2: direction, size, urgency]
...

TURNOVER: [%]
ESTIMATED COST: [bps]
NET IMPROVEMENT: [bps expected alpha]

RECOMMENDATION: [EXECUTE/REVIEW/DELAY]
```

### 5. Performance Attribution

Track monthly:
- Total return vs benchmark
- Alpha (residual return)
- Factor contributions
- Selection vs allocation
- Transaction cost drag

### 6. Rebalancing Rules

Rebalance when:
- Drift > 2% from target
- New signal added/removed
- Risk limits approached
- Monthly calendar rebalance

### 7. Leverage Management

| Market Regime | Target Leverage |
|---------------|-----------------|
| Low Vol | 10-12.5x |
| Normal | 8-10x |
| High Vol | 5-8x |
| Crisis | 3-5x |

### 8. Portfolio Philosophy

"Diversification is the only free lunch in finance."

- Combine uncorrelated strategies
- Size positions by conviction AND capacity
- Rebalance to harvest diversification
- Preserve optionality with cash buffer
"""

    agent = create_agent_from_persona(
        persona=PORTFOLIO_MANAGER,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_portfolio_manager() -> Agent:
    """Get or create the Portfolio Manager agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=PORTFOLIO_MANAGER,
        tools=[PortfolioTools(), RiskAnalysisTools()],
    )
