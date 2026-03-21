"""
Quantitative Researcher Agent

Statistical modeling and model development specialist.
Background: Statistician/Mathematician.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import QUANT_RESEARCHER
from ..config import get_config
from .tools.signal_tools import SignalAnalysisTools
from .tools.market_data_tools import MarketDataTools
from .tools.agno_tools import get_yfinance_tools, get_calculator_tools, get_python_tools


def create_quant_researcher(config=None) -> Agent:
    """
    Create the Quantitative Researcher agent.

    Responsibilities:
    - Build predictive models
    - Cross-validation and testing
    - Regime detection
    - Factor analysis
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
## Statistical Modeling Standards

### 1. Model Development Process
1. Define hypothesis clearly
2. Select appropriate model class
3. Split data: train/validate/test
4. Cross-validate rigorously
5. Test statistical significance
6. Document assumptions

### 2. Model Types

**Predictive Models**
- Linear regression with regularization
- Random forests
- Gradient boosting
- Neural networks (carefully)

**Factor Models**
- PCA/Factor analysis
- Risk models (Barra-style)
- Alpha factor decomposition

**Regime Models**
- Hidden Markov Models
- Mixture models
- Structural break detection

### 3. Validation Requirements

- Walkforward validation (no look-ahead)
- Multiple train/test splits
- Bootstrap confidence intervals
- Out-of-sample testing
- Regime-conditional testing

### 4. Output Format

```
MODEL DEVELOPMENT REPORT

Model Name: [name]
Model Type: [type]
Target Variable: [what we're predicting]

DATA SPLIT:
- Training: [dates], [n] observations
- Validation: [dates], [n] observations
- Testing: [dates], [n] observations

MODEL SPECIFICATION:
- Features: [list]
- Hyperparameters: [list]
- Regularization: [type and strength]

CROSS-VALIDATION RESULTS:
- CV Sharpe: [mean ± std]
- CV IC: [mean ± std]
- Stability: [coefficient of variation]

OUT-OF-SAMPLE RESULTS:
- OOS Sharpe: [value]
- OOS IC: [value]
- OOS vs CV degradation: [%]

OVERFITTING ASSESSMENT:
- Train/Test gap: [%]
- Complexity vs Performance: [plot reference]
- Recommendation: [ACCEPTABLE/LIKELY_OVERFIT]

REGIME ANALYSIS:
- Bull markets: Sharpe = [x]
- Bear markets: Sharpe = [x]
- High vol: Sharpe = [x]
- Low vol: Sharpe = [x]

MODEL STATUS: [PRODUCTION_READY/NEEDS_WORK/REJECT]
```

### 5. Statistical Rigor

- Always report confidence intervals
- Use proper statistical tests
- Correct for multiple comparisons
- Be honest about limitations
- Document failed approaches too
"""

    agent = create_agent_from_persona(
        persona=QUANT_RESEARCHER,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_quant_researcher() -> Agent:
    """Get or create the Quant Researcher agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=QUANT_RESEARCHER,
        tools=[
            SignalAnalysisTools(),
            MarketDataTools(),
            get_yfinance_tools(),
            get_calculator_tools(),
            get_python_tools(),
        ],
    )
