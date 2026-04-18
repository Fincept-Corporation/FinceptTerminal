"""
Data Scientist Agent

Data infrastructure and feature engineering specialist.
Manages the petabyte-scale data warehouse.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import DATA_SCIENTIST
from ..config import get_config
from .tools.market_data_tools import MarketDataTools
from .tools.agno_tools import get_yfinance_tools, get_python_tools, get_hackernews_tools, get_duckduckgo_tools


def create_data_scientist(config=None) -> Agent:
    """
    Create the Data Scientist agent.

    Responsibilities:
    - Manage data warehouse
    - Clean and validate data
    - Create derived features
    - Integrate alternative data
    """
    cfg = config or get_config()

    tools = [
        MarketDataTools(),
        # Agno built-in tools
        get_yfinance_tools(),
        get_python_tools(),
        get_hackernews_tools(),
        get_duckduckgo_tools(),
    ]

    additional_instructions = """
## Data Management Standards

### 1. Data Quality
Every data point must be:
- Validated for accuracy
- Checked for outliers
- Adjusted for corporate actions
- Time-stamped precisely
- Stored with full provenance

### 2. Feature Engineering
Create features that are:
- Stationary (or properly transformed)
- Not look-ahead biased
- Normalized appropriately
- Documented with formulas

### 3. Data Categories

**Market Data**
- Price, volume, OHLC
- Order book snapshots
- Trade prints
- Corporate actions

**Fundamental Data**
- Earnings, revenue
- Balance sheet items
- Analyst estimates
- Guidance

**Alternative Data**
- Satellite imagery
- Web traffic
- Social sentiment
- Credit card data
- Patent filings

### 4. Output Format

```
DATA PREPARATION REPORT

Dataset: [name]
Ticker(s): [list]
Period: [start] to [end]

DATA QUALITY:
- Total Records: [n]
- Missing Values: [n] ([%])
- Outliers Detected: [n]
- Outliers Handled: [method]
- Corporate Actions: [n adjustments]

FEATURES CREATED:
1. [feature_name]: [formula] - [description]
2. [feature_name]: [formula] - [description]
...

STATIONARITY TESTS:
- [feature]: ADF stat=[x], p-value=[y] [PASS/FAIL]
...

DATA QUALITY SCORE: [0-100]

READY FOR ANALYSIS: [YES/NO]
```

### 5. Data Principles

- **Point-in-time**: Never use future data
- **Survivorship bias**: Include delisted securities
- **Backfill bias**: Mark when data became available
- **Consistency**: Same methodology across history
"""

    agent = create_agent_from_persona(
        persona=DATA_SCIENTIST,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_data_scientist() -> Agent:
    """Get or create the Data Scientist agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=DATA_SCIENTIST,
        tools=[
            MarketDataTools(),
            get_yfinance_tools(),
            get_python_tools(),
            get_hackernews_tools(),
            get_duckduckgo_tools(),
        ],
    )
