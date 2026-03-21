"""
Signal Scientist Agent

Pattern recognition specialist, discovers trading signals.
Background: Mathematician/Physicist (like many RenTech scientists).
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import SIGNAL_SCIENTIST
from ..config import get_config
from .tools.signal_tools import SignalAnalysisTools
from .tools.market_data_tools import MarketDataTools
from .tools.agno_tools import get_yfinance_tools, get_calculator_tools, get_python_tools


def create_signal_scientist(config=None) -> Agent:
    """
    Create the Signal Scientist agent.

    Responsibilities:
    - Discover non-random patterns
    - Calculate statistical significance
    - Develop predictive features
    - Backtest signals rigorously
    """
    cfg = config or get_config()

    tools = [
        SignalAnalysisTools(),
        MarketDataTools(),
        # Agno built-in tools
        get_yfinance_tools(),
        get_calculator_tools(),
        get_python_tools(),
    ]

    additional_instructions = """
## Signal Discovery Methodology

### 1. Pattern Detection
- Use statistical tests, not visual patterns
- Calculate p-values rigorously
- Account for multiple testing (Bonferroni correction)
- Look for non-obvious relationships

### 2. Signal Types to Explore
- Mean reversion (various horizons)
- Momentum (cross-sectional and time-series)
- Microstructure patterns
- Cross-asset relationships
- Alternative data signals

### 3. Validation Requirements
Before presenting any signal:
- P-value < 0.01 (strict threshold)
- Backtest over multiple periods
- Test across market regimes
- Calculate out-of-sample performance
- Estimate capacity and decay

### 4. Output Format

```
SIGNAL DISCOVERY REPORT

Signal Name: [descriptive name]
Signal Type: [mean_reversion/momentum/stat_arb/etc]

DISCOVERY:
- Pattern Description: [what was found]
- Economic Rationale: [why it might work]
- Data Used: [features/data sources]

STATISTICAL ANALYSIS:
- Sample Size: [n observations]
- P-value: [value]
- T-statistic: [value]
- Effect Size: [Cohen's d or similar]

BACKTEST RESULTS:
- Period: [start] to [end]
- Sharpe Ratio: [value]
- Win Rate: [value]
- Max Drawdown: [value]
- Profit Factor: [value]

OUT-OF-SAMPLE:
- OOS Period: [dates]
- OOS Sharpe: [value]
- Performance Decay: [percentage]

CAPACITY ESTIMATE:
- Max AUM: [$X billion]
- Liquidity Score: [value]

RECOMMENDATION: [PROMISING/NEEDS_MORE_WORK/ABANDON]
```

## Scientific Rigor

Remember: We are scientists, not traders.
- Let the data speak
- Be skeptical of your own findings
- Publish internally, encourage critique
- Document everything for reproducibility
"""

    agent = create_agent_from_persona(
        persona=SIGNAL_SCIENTIST,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_signal_scientist() -> Agent:
    """Get or create the Signal Scientist agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=SIGNAL_SCIENTIST,
        tools=[
            SignalAnalysisTools(),
            MarketDataTools(),
            get_yfinance_tools(),
            get_calculator_tools(),
            get_python_tools(),
        ],
    )
