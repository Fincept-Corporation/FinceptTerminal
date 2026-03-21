"""
Renaissance Technologies Agent Implementations

All 10 agents that make up the Medallion Fund organization.
Uses Agno framework for multi-agent teams.

Note: Analysis functions (run_signal_analysis, etc.) have been moved to
the `strategies` module. Import them from `strategies.analysis` instead.
"""

from .base import (
    create_agent_from_persona,
    AgentFactory,
    get_agent_factory,
    reset_agent_factory,
)

from .investment_committee import (
    create_investment_committee_chair,
    get_investment_committee_chair,
)
from .research_lead import create_research_lead, get_research_lead
from .signal_scientist import create_signal_scientist, get_signal_scientist
from .data_scientist import create_data_scientist, get_data_scientist
from .quant_researcher import create_quant_researcher, get_quant_researcher
from .execution_trader import create_execution_trader, get_execution_trader
from .market_maker import create_market_maker, get_market_maker
from .risk_quant import create_risk_quant, get_risk_quant
from .compliance_officer import create_compliance_officer, get_compliance_officer
from .portfolio_manager import create_portfolio_manager, get_portfolio_manager

__all__ = [
    # Base Factory
    "create_agent_from_persona",
    "AgentFactory",
    "get_agent_factory",
    "reset_agent_factory",
    # Investment Committee
    "create_investment_committee_chair",
    "get_investment_committee_chair",
    # Research Team
    "create_research_lead",
    "get_research_lead",
    "create_signal_scientist",
    "get_signal_scientist",
    "create_data_scientist",
    "get_data_scientist",
    "create_quant_researcher",
    "get_quant_researcher",
    # Trading Team
    "create_execution_trader",
    "get_execution_trader",
    "create_market_maker",
    "get_market_maker",
    # Risk Team
    "create_risk_quant",
    "get_risk_quant",
    "create_compliance_officer",
    "get_compliance_officer",
    # Portfolio
    "create_portfolio_manager",
    "get_portfolio_manager",
]
