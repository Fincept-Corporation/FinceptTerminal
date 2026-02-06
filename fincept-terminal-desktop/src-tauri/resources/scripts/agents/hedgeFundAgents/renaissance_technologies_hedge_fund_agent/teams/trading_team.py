"""
Trading Team - Execution Excellence

The execution team responsible for converting signals into trades
with minimal market impact and optimal execution quality.

Team Structure:
- Execution Trader (team leader, optimal execution)
- Market Maker (liquidity provision)
"""

from typing import Optional
from agno.team import Team

from ..agents import (
    create_execution_trader,
    create_market_maker,
)
from ..config import get_config, RenTechConfig
from ..utils.model_factory import create_model_from_config


# Global team instance
_trading_team: Optional[Team] = None


def create_trading_team(config: Optional[RenTechConfig] = None) -> Team:
    """
    Create the Trading Team.

    This team is responsible for:
    - Optimal trade execution
    - Market impact minimization
    - Smart order routing
    - Market making activities

    The team operates in real-time, coordinating execution
    across multiple venues and strategies.
    """
    cfg = config or get_config()

    # Create individual agents
    execution_trader = create_execution_trader(cfg)
    market_maker = create_market_maker(cfg)

    # Create the team
    # Note: Execution Trader is first in members list to act as coordinator
    team = Team(
        name="Execution & Trading Team",
        description="""Responsible for executing trades with minimal market
        impact and managing market-making activities. Focus on preserving
        alpha through optimal execution.""",

        # Team composition (no 'leader' param in Agno - first member coordinates)
        members=[
            execution_trader,  # Acts as team coordinator
            market_maker,
        ],

        # Model for team-level decisions
        model=create_model_from_config({
            "temperature": cfg.models.temperature,
            "max_tokens": cfg.models.max_tokens,
        }),

        # Collaboration settings
        share_member_interactions=cfg.share_interactions,
        add_team_history_to_members=True,  # Correct param name

        # Memory
        enable_agentic_memory=cfg.enable_memory,

        # Instructions
        instructions="""
## Trading Team Mission

You are the Execution & Trading Team at Renaissance Technologies.
Your mission is to convert trading signals into actual positions
while minimizing execution costs and market impact.

## Team Workflow

1. **Pre-Trade Analysis**
   - Assess market conditions
   - Calculate expected impact
   - Select execution algorithm
   - Plan order schedule

2. **Execution**
   - Route orders optimally
   - Monitor real-time fills
   - Adjust for market conditions
   - Coordinate with market maker

3. **Post-Trade Analysis**
   - Calculate actual vs expected costs
   - Update execution models
   - Report performance

## Execution Priorities

1. **Preserve Alpha** - Don't leak information
2. **Minimize Impact** - Trade without moving market
3. **Achieve Best Price** - Beat benchmarks
4. **Ensure Fill** - Complete the order

## Coordination

The Execution Trader leads all execution decisions.
The Market Maker provides liquidity insights and helps
with spread capture when appropriate.

## Real-Time Requirements

Trading requires immediate responses. Always:
- Monitor current market state
- Adjust in real-time
- Escalate issues immediately
- Log all decisions for audit

## Performance Metrics

Track and optimize:
- Implementation shortfall
- VWAP/TWAP performance
- Market impact
- Fill rates
- Alpha preservation
""",

        markdown=True,
        show_members_responses=True,
    )

    return team


def get_trading_team() -> Team:
    """Get or create the global Trading Team instance"""
    global _trading_team
    if _trading_team is None:
        _trading_team = create_trading_team()
    return _trading_team


def reset_trading_team():
    """Reset the global Trading Team instance"""
    global _trading_team
    _trading_team = None
