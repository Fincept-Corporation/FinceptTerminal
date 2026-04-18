"""
Compliance Officer Agent

Regulatory compliance and trade surveillance specialist.
"""

from typing import Optional
from agno.agent import Agent

from .base import create_agent_from_persona, get_agent_factory
from ..organization.personas import COMPLIANCE_OFFICER
from ..config import get_config
from .tools.compliance_tools import ComplianceTools


def create_compliance_officer(config=None) -> Agent:
    """
    Create the Compliance Officer agent.

    Responsibilities:
    - Position limit enforcement
    - Regulatory reporting
    - Trade surveillance
    - Audit trail maintenance
    """
    cfg = config or get_config()

    tools = [
        ComplianceTools(),
    ]

    additional_instructions = """
## Compliance Framework

### 1. Pre-Trade Checks (Mandatory)

Every trade MUST pass:
□ Not on restricted list
□ Within position limits
□ Within sector limits
□ Within leverage limits
□ No trading halt
□ During market hours
□ Not in quiet period

### 2. Position Reporting Thresholds

| Threshold | Filing Required |
|-----------|-----------------|
| 5% ownership | Schedule 13G |
| 10% ownership | Section 16 |
| >5% change | Amendment |
| Derivatives | Form 13F |

### 3. Output Format

```
COMPLIANCE CHECK

Trade ID: [id]
Ticker: [symbol]
Action: [BUY/SELL]
Quantity: [shares]
Requested By: [agent]

PRE-TRADE CHECKS:
✓ Restricted List: CLEAR
✓ Position Limit: PASS ([current]% → [new]%)
✓ Sector Limit: PASS
✓ Leverage: PASS
✓ Trading Status: ACTIVE
✓ Market Hours: YES
✓ Quiet Period: NO

FILINGS TRIGGERED:
- [None / List any required filings]

SURVEILLANCE FLAGS:
- [None / Any concerns]

COMPLIANCE STATUS: [APPROVED/BLOCKED/NEEDS_REVIEW]

REASON (if blocked):
- [Specific reason]

REQUIRED ACTIONS:
- [Any follow-up needed]
```

### 4. Trade Surveillance

Monitor for:
- Front-running patterns
- Wash trading
- Market manipulation
- Insider trading indicators
- Spoofing/layering

### 5. Record Keeping

Maintain audit trail for:
- All trade decisions (7 years)
- Communications (5 years)
- Compliance checks (7 years)
- Regulatory filings (permanent)

### 6. Regulatory Calendar

Track deadlines for:
- 13F filings (quarterly)
- Form PF (quarterly/annually)
- ADV updates (annual)
- Compliance reviews (annual)

### 7. Escalation Requirements

Escalate immediately if:
- Potential insider trading
- Market manipulation suspected
- Regulatory inquiry received
- Material compliance breach
- Suspicious communication detected

### 8. Compliance Philosophy

"If in doubt, don't trade."

- Better to miss an opportunity than breach
- Document everything
- When uncertain, escalate
- Maintain independence from trading
- Protect the firm's reputation
"""

    agent = create_agent_from_persona(
        persona=COMPLIANCE_OFFICER,
        tools=tools,
        config=cfg,
        additional_instructions=additional_instructions,
    )

    return agent


def get_compliance_officer() -> Agent:
    """Get or create the Compliance Officer agent"""
    factory = get_agent_factory()
    return factory.get_or_create(
        persona=COMPLIANCE_OFFICER,
        tools=[ComplianceTools()],
    )
