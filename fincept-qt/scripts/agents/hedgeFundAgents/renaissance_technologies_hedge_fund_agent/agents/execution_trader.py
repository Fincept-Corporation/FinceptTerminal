"""
Execution Trader Agent

Order execution optimization specialist.
Background: Speech recognition/Signal processing (like RenTech's IBM hires).
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import EXECUTION_TRADER
from ..config import get_config
from .tools.execution_tools import ExecutionTools
from .tools.market_data_tools import MarketDataTools
from .tools.agno_tools import get_yfinance_tools, get_calculator_tools


def create_execution_trader(config=None) -> Agent:
    """
    Create the Execution Trader agent.

    Responsibilities:
    - Minimize market impact
    - Optimal execution timing
    - Transaction cost analysis
    - Smart order routing
    """
    cfg = config or get_config()

    tools = [
        ExecutionTools(),
        MarketDataTools(),
        # Agno built-in tools
        get_yfinance_tools(),
        get_calculator_tools(),
    ]

    additional_instructions = """
## Execution Excellence Standards

### 1. Execution Goals (Priority Order)
1. **Preserve Alpha** - Don't leak information
2. **Minimize Impact** - Trade without moving market
3. **Achieve Best Price** - Beat benchmarks
4. **Ensure Fill** - Complete the order

### 2. Algorithm Selection

| Urgency | Market Conditions | Algorithm |
|---------|------------------|-----------|
| High | Normal | Arrival Price |
| High | Volatile | Aggressive TWAP |
| Low | Normal | Patient VWAP |
| Low | Low Vol | Passive Dark |
| Any | Illiquid | Careful Iceberg |

### 3. Market Impact Model

Use the Almgren-Chriss framework:
- Temporary impact: σ * γ * (v/V)^α
- Permanent impact: λ * X
- Optimize: min(impact + risk)

### 4. Execution Reporting

```
EXECUTION REPORT

Order ID: [id]
Ticker: [symbol]
Direction: [BUY/SELL]
Quantity: [shares]

EXECUTION SUMMARY:
- Algo Used: [name]
- Duration: [minutes]
- Fill Rate: [%]
- VWAP: $[price]
- Our Avg: $[price]

BENCHMARKS:
- vs Arrival: [bps] [BEAT/MISS]
- vs VWAP: [bps] [BEAT/MISS]
- vs TWAP: [bps] [BEAT/MISS]

COST BREAKDOWN:
- Spread Cost: [bps]
- Market Impact: [bps]
- Timing Cost: [bps]
- Total: [bps]

ALPHA PRESERVED: [%]

VENUE ANALYSIS:
- [Venue]: [fill %], [price quality]
...

LEARNINGS:
- [What worked]
- [What to improve]
```

### 5. Real-Time Adjustments

Monitor and adjust for:
- Sudden volume changes
- Spread widening
- News events
- Price momentum
- Order book changes

### 6. Post-Trade Analysis

Every trade informs the next:
- Update impact models
- Refine algo parameters
- Track execution quality trends
"""

    agent = create_agent_from_persona(
        persona=EXECUTION_TRADER,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_execution_trader() -> Agent:
    """Get or create the Execution Trader agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=EXECUTION_TRADER,
        tools=[
            ExecutionTools(),
            MarketDataTools(),
            get_yfinance_tools(),
            get_calculator_tools(),
        ],
    )
