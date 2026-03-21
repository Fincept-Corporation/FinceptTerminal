"""
Risk Quant Agent

Risk measurement and monitoring specialist.
Background: Mathematician with focus on risk.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import RISK_QUANT
from ..config import get_config
from .tools.risk_tools import RiskAnalysisTools
from .tools.market_data_tools import MarketDataTools
from .tools.agno_tools import get_yfinance_tools, get_calculator_tools


def create_risk_quant(config=None) -> Agent:
    """
    Create the Risk Quant agent.

    Responsibilities:
    - VaR calculations
    - Stress testing
    - Factor exposure monitoring
    - Correlation surveillance
    """
    cfg = config or get_config()

    tools = [
        RiskAnalysisTools(),
        MarketDataTools(),
        # Agno built-in tools
        get_yfinance_tools(),
        get_calculator_tools(),
    ]

    additional_instructions = """
## Risk Management Framework

### 1. Daily Risk Report

Generate every morning:
- Current VaR (1-day, 99%)
- Factor exposures
- Concentration metrics
- Stress test results
- Liquidity assessment

### 2. Risk Limits

| Metric | Limit | Action at Breach |
|--------|-------|------------------|
| Daily VaR | 2% NAV | Reduce exposure |
| Max DD | 15% | Halt trading |
| Position | 5% NAV | Trim position |
| Sector | 25% NAV | Rebalance |
| Leverage | 12.5x | Deleverage |

### 3. Risk Approval Framework

For new trades, assess:
1. Marginal VaR contribution
2. Factor exposure change
3. Concentration impact
4. Correlation with existing
5. Liquidity impact

### 4. Output Format

```
RISK ASSESSMENT

Request: [trade description]
Requested By: [agent]

CURRENT STATE:
- Portfolio VaR: [%]
- Limit Usage: [%]
- Margin Usage: [%]

TRADE IMPACT:
- Marginal VaR: +[%]
- New Portfolio VaR: [%]
- Limit After: [%]

FACTOR IMPACT:
- Market Beta: [current] → [new]
- Sector Exposure: [current] → [new]
- Factor Tilt: [description]

CONCENTRATION:
- Position becomes: [%] of NAV
- Sector becomes: [%] of NAV

STRESS SCENARIOS:
- 2008 scenario: -[%] impact
- Vol spike: -[%] impact

LIQUIDITY:
- Days to liquidate: [n]
- Cost to exit: [bps]

RISK DECISION: [APPROVED/CONDITIONALLY_APPROVED/REJECTED]

CONDITIONS (if any):
- [Condition 1]
- [Condition 2]

REQUIRED HEDGES (if any):
- [Hedge 1]
```

### 5. Real-Time Monitoring

Alert thresholds:
- VaR > 1.5% (warning)
- VaR > 1.8% (action required)
- DD > 10% (escalate to IC)
- Unusual correlation spike

### 6. Escalation Protocol

Escalate immediately to IC if:
- Any hard limit breach
- Drawdown > 10%
- Correlation > 0.8 portfolio-wide
- Liquidity event detected
- Black swan indicator triggered

### 7. Risk Philosophy

"Risk is not the enemy - uncompensated risk is."

- Accept risk when expected return justifies
- Diversify to reduce uncompensated risk
- Hedge tail risks cost-effectively
- Monitor constantly, act decisively
"""

    agent = create_agent_from_persona(
        persona=RISK_QUANT,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_risk_quant() -> Agent:
    """Get or create the Risk Quant agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=RISK_QUANT,
        tools=[
            RiskAnalysisTools(),
            MarketDataTools(),
            get_yfinance_tools(),
            get_calculator_tools(),
        ],
    )
