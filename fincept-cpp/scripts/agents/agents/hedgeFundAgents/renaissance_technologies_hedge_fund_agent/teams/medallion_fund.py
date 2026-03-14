"""
Medallion Fund - Top-Level Investment Committee

The top-level organization that brings together all teams
under the Investment Committee's leadership.

This is the main entry point for running the multi-agent system.

Organization Structure:
┌─────────────────────────────────┐
│     Investment Committee        │
│  (Chair + Portfolio Manager)    │
└───────────────┬─────────────────┘
                │
    ┌───────────┼───────────┐
    │           │           │
    ▼           ▼           ▼
┌────────┐ ┌────────┐ ┌────────┐
│Research│ │Trading │ │  Risk  │
│  Team  │ │  Team  │ │  Team  │
└────────┘ └────────┘ └────────┘
"""

from typing import Optional, Any, Dict
from agno.team import Team

from ..agents import (
    create_investment_committee_chair,
    create_portfolio_manager,
)
from .research_team import create_research_team
from .trading_team import create_trading_team
from .risk_team import create_risk_team
from ..config import get_config, RenTechConfig
from ..utils.model_factory import create_model_from_config


# Global fund instance
_medallion_fund: Optional[Team] = None


def create_medallion_fund(config: Optional[RenTechConfig] = None) -> Team:
    """
    Create the Medallion Fund - the complete organization.

    This is a nested Team structure:
    - Investment Committee Chair (leader)
    - Portfolio Manager (member)
    - Research Team (nested team)
    - Trading Team (nested team)
    - Risk Team (nested team)

    The Investment Committee Chair has final decision authority
    on all trading decisions, capital allocation, and risk limits.
    """
    cfg = config or get_config()

    # Create the Investment Committee members
    ic_chair = create_investment_committee_chair(cfg)
    portfolio_manager = create_portfolio_manager(cfg)

    # Create the sub-teams
    research_team = create_research_team(cfg)
    trading_team = create_trading_team(cfg)
    risk_team = create_risk_team(cfg)

    # Create the top-level team
    # Note: IC Chair is first in members list to act as coordinator
    medallion_fund = Team(
        name="Medallion Fund Investment Committee",
        description="""Top-level decision-making body for the Medallion Fund.
        Approves all trading signals, allocates capital, and sets risk limits.
        Synthesizes input from Research, Trading, and Risk teams to make
        systematic investment decisions.""",

        # Members include both individuals and teams (no 'leader' param in Agno)
        # IC Chair is first to act as coordinator
        members=[
            ic_chair,  # Acts as team coordinator
            portfolio_manager,
            research_team,
            trading_team,
            risk_team,
        ],

        # Model for committee-level decisions (uses dynamic factory)
        model=create_model_from_config({
            "temperature": cfg.models.temperature,
            "max_tokens": cfg.models.max_tokens,
        }),

        # Full collaboration - all members see all interactions
        share_member_interactions=cfg.share_interactions,
        add_team_history_to_members=True,  # Correct param name

        # Memory for learning from decisions
        enable_agentic_memory=cfg.enable_memory,

        # Instructions for the Investment Committee
        instructions="""
## Medallion Fund Investment Committee

You are the Investment Committee of Renaissance Technologies' Medallion Fund,
one of the most successful quantitative hedge funds in history.

## Your Mission

Make systematic, data-driven investment decisions that maximize risk-adjusted
returns while maintaining strict risk controls. We are scientists who happen
to trade, not traders who use science.

## Decision Authority

The Investment Committee Chair has final authority on:
- Signal approval/rejection
- Capital allocation across strategies
- Risk limit setting and modification
- Trading halts and resumptions

## Team Coordination Protocol

When a task arrives, route it appropriately:

### Signal Research
→ Route to **Research Team**
- Signal discovery
- Data preparation
- Statistical validation
- Backtest requests

### Trade Execution
→ Route to **Trading Team**
- Execution planning
- Order routing
- Market making
- Post-trade analysis

### Risk Assessment
→ Route to **Risk Team**
- VaR calculations
- Stress tests
- Compliance checks
- Position monitoring

### Portfolio Decisions
→ Route to **Portfolio Manager**
- Capital allocation
- Strategy weights
- Rebalancing
- Performance attribution

### Final Approval
→ **Investment Committee Chair** reviews and decides

## Standard Operating Procedure

For a typical signal-to-trade workflow:

1. **Research Team** discovers and validates signal
2. **Risk Team** assesses risk implications
3. **Portfolio Manager** determines sizing
4. **Investment Committee Chair** approves/rejects
5. **Trading Team** executes if approved
6. **Risk Team** monitors positions

## Communication Protocol

All inter-team communication should be:
- Precise and quantitative
- Include confidence levels
- Reference supporting data
- Note any concerns or caveats

## Decision Output Format

When making decisions, the Chair should provide:

```
══════════════════════════════════════
INVESTMENT COMMITTEE DECISION
══════════════════════════════════════

REQUEST: [What was requested]

TEAM INPUTS:
- Research: [Summary of research input]
- Risk: [Summary of risk assessment]
- Trading: [Summary of execution plan]
- Portfolio: [Summary of sizing/allocation]

DECISION: [APPROVED / REJECTED / PENDING]

RATIONALE:
1. [Key reason 1]
2. [Key reason 2]
3. [Key reason 3]

CONDITIONS (if approved):
- [Condition 1]
- [Condition 2]

NEXT STEPS:
- [Who does what next]

══════════════════════════════════════
```

## Core Principles

1. **Statistical Rigor**: P-value < 0.01 or we don't trade
2. **Risk First**: Capital preservation is paramount
3. **Edge Focus**: We're right 50.75% of the time - that's enough
4. **Systematic**: No discretionary overrides of the model
5. **Continuous Learning**: Update models with every trade

## Remember

"We hire people who have done good science" - Jim Simons
"We're right 50.75% of the time" - Bob Mercer

Focus on the process. Let the edge compound.
""",

        markdown=True,
        show_members_responses=True,
    )

    return medallion_fund


def get_medallion_fund() -> Team:
    """Get or create the global Medallion Fund instance"""
    global _medallion_fund
    if _medallion_fund is None:
        _medallion_fund = create_medallion_fund()
    return _medallion_fund


def reset_medallion_fund():
    """Reset the global Medallion Fund instance"""
    global _medallion_fund
    _medallion_fund = None


async def run_medallion_fund(
    task: str,
    context: Optional[Dict[str, Any]] = None,
    stream: bool = False,
) -> str:
    """
    Run a task through the Medallion Fund organization.

    This is the main entry point for the multi-agent system.
    Tasks are routed to appropriate teams based on content.

    Args:
        task: The task or question to process
        context: Optional additional context
        stream: Whether to stream the response

    Returns:
        The response from the organization

    Example:
        >>> response = await run_medallion_fund(
        ...     "Analyze AAPL for mean reversion signals and recommend a trade"
        ... )
    """
    fund = get_medallion_fund()

    # Build the message with context if provided
    message = task
    if context:
        context_str = "\n".join([f"- {k}: {v}" for k, v in context.items()])
        message = f"{task}\n\nContext:\n{context_str}"

    # Run through the team
    if stream:
        response_parts = []
        async for chunk in fund.run_stream(message):
            response_parts.append(chunk)
        return "".join(response_parts)
    else:
        response = fund.run(message)
        return response.content if hasattr(response, 'content') else str(response)


# Convenience function for synchronous usage
def run_medallion_fund_sync(
    task: str,
    context: Optional[Dict[str, Any]] = None,
) -> str:
    """Synchronous version of run_medallion_fund"""
    import asyncio
    return asyncio.run(run_medallion_fund(task, context, stream=False))
