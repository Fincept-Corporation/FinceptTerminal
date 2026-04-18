"""
Research Team - Alpha Generation

The quantitative research team responsible for discovering and validating
trading signals. Mirrors RenTech's actual research organization.

Team Structure:
- Research Lead (team leader)
- Signal Scientist (pattern discovery)
- Data Scientist (data infrastructure)
- Quant Researcher (statistical modeling)
"""

from typing import Optional
from agno.team import Team

from ..agents import (
    create_research_lead,
    create_signal_scientist,
    create_data_scientist,
    create_quant_researcher,
)
from ..config import get_config, RenTechConfig
from ..utils.model_factory import create_model_from_config


# Global team instance
_research_team: Optional[Team] = None


def create_research_team(config: Optional[RenTechConfig] = None) -> Team:
    """
    Create the Research Team.

    This team is responsible for:
    - Signal discovery and validation
    - Data preparation and feature engineering
    - Statistical modeling and backtesting
    - Research quality control

    The team collaborates closely, sharing discoveries and critiquing
    each other's work - just like the real RenTech research environment.
    """
    cfg = config or get_config()

    # Create individual agents
    research_lead = create_research_lead(cfg)
    signal_scientist = create_signal_scientist(cfg)
    data_scientist = create_data_scientist(cfg)
    quant_researcher = create_quant_researcher(cfg)

    # Create the team
    # Note: Research Lead is first in members list to act as coordinator
    team = Team(
        name="Quantitative Research Team",
        description="""Alpha generation team responsible for discovering and
        validating trading signals. Combines expertise in mathematics,
        statistics, and data science to identify market inefficiencies.""",

        # Team composition (no 'leader' param in Agno - first member coordinates)
        members=[
            research_lead,  # Acts as team coordinator
            signal_scientist,
            data_scientist,
            quant_researcher,
        ],

        # Model for team-level decisions (uses dynamic factory)
        model=create_model_from_config({
            "temperature": cfg.models.temperature,
            "max_tokens": cfg.models.max_tokens,
        }),

        # Collaboration settings
        share_member_interactions=cfg.share_interactions,
        add_team_history_to_members=True,  # Correct param name

        # Memory (will be enhanced in Phase 5)
        enable_agentic_memory=cfg.enable_memory,

        # Instructions for the team as a whole
        instructions="""
## Research Team Mission

You are the Quantitative Research Team at Renaissance Technologies.
Your mission is to discover statistically significant trading signals
that can generate alpha while maintaining rigorous scientific standards.

## Team Workflow

1. **Data Preparation** (Data Scientist)
   - Clean and validate market data
   - Create derived features
   - Ensure point-in-time correctness

2. **Signal Discovery** (Signal Scientist)
   - Explore patterns in the data
   - Calculate statistical significance
   - Initial signal development

3. **Statistical Validation** (Quant Researcher)
   - Rigorous backtesting
   - Cross-validation
   - Regime analysis

4. **Quality Control** (Research Lead)
   - Review all findings
   - Ensure standards are met
   - Approve for Investment Committee

## Standards

- P-value < 0.01 for any signal
- Out-of-sample validation required
- Economic rationale must exist
- Transaction costs must be considered
- Capacity must be sufficient

## Collaboration

Share findings openly within the team. Encourage constructive
criticism. Better to kill a bad idea early than waste resources.

Remember: "We hire people who have done good science" - Jim Simons
""",

        # Markdown output for readability
        markdown=True,
        show_members_responses=True,
    )

    return team


def get_research_team() -> Team:
    """Get or create the global Research Team instance"""
    global _research_team
    if _research_team is None:
        _research_team = create_research_team()
    return _research_team


def reset_research_team():
    """Reset the global Research Team instance"""
    global _research_team
    _research_team = None
