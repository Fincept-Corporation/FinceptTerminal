"""
Market Maker Agent

Liquidity provision and spread capture specialist.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import MARKET_MAKER
from ..config import get_config
from .tools.execution_tools import ExecutionTools
from .tools.market_data_tools import MarketDataTools


def create_market_maker(config=None) -> Agent:
    """
    Create the Market Maker agent.

    Responsibilities:
    - Quote management
    - Inventory risk management
    - Bid-ask optimization
    - Rebate optimization
    """
    cfg = config or get_config()

    tools = [
        ExecutionTools(),
        MarketDataTools(),
    ]

    additional_instructions = """
## Market Making Framework

### 1. Quoting Strategy

**Fair Value Calculation**
- Mid = (best_bid + best_ask) / 2
- Adjust for: inventory, momentum, information
- Skew quotes to manage inventory

**Spread Setting**
- Base spread = f(volatility, tick_size)
- Widen for: illiquidity, uncertainty, inventory
- Narrow for: rebate capture, competition

### 2. Inventory Management

| Inventory Level | Action |
|-----------------|--------|
| -80% to -50% | Aggressive buy quotes |
| -50% to -20% | Skewed buy |
| -20% to +20% | Neutral |
| +20% to +50% | Skewed sell |
| +50% to +80% | Aggressive sell quotes |
| Beyond ±80% | Pause quoting |

### 3. Risk Controls

- Max inventory: $X per symbol
- Max aggregate: $Y total
- Stop loss: -$Z per day
- Correlation limits: Don't hold correlated inventory

### 4. Output Format

```
MARKET MAKING STATUS

Symbol: [ticker]
Time: [timestamp]

CURRENT QUOTES:
- Bid: $[price] x [size]
- Ask: $[price] x [size]
- Spread: [bps]
- Fair Value: $[price]

INVENTORY:
- Position: [shares] ($[value])
- Inventory %: [% of limit]
- Risk: $[VaR]

P&L TODAY:
- Realized: $[amount]
- Unrealized: $[amount]
- Total: $[amount]
- Sharpe (today): [value]

MARKET CONDITIONS:
- Volatility: [low/normal/high]
- Spread regime: [tight/normal/wide]
- Volume: [low/normal/high]
- Adversity: [low/normal/high]

STRATEGY ADJUSTMENTS:
- [Adjustment 1]
- [Adjustment 2]
```

### 5. Adverse Selection Defense

Watch for informed flow:
- Unusual order sizes
- Aggressive taking
- Pre-news activity
- Correlated flow across symbols

Response:
- Widen spreads
- Reduce size
- Pause quoting
- Alert compliance

### 6. Rebate Optimization

- Track maker/taker fees by venue
- Route to maximize net rebate
- Consider queue priority value
- Balance rebate vs fill probability
"""

    agent = create_agent_from_persona(
        persona=MARKET_MAKER,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_market_maker() -> Agent:
    """Get or create the Market Maker agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=MARKET_MAKER,
        tools=[ExecutionTools(), MarketDataTools()],
    )
