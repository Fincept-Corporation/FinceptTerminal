"""
Renaissance Technologies Organizational Hierarchy

Defines the team structure and reporting relationships.
Mirrors the actual RenTech organization.

Structure:
    Investment Committee (Decision Authority)
        ├── Research Team (Alpha Generation)
        │   ├── Signal Scientist
        │   ├── Data Scientist
        │   └── Quant Researcher
        ├── Trading Team (Execution)
        │   ├── Execution Trader
        │   └── Market Maker
        ├── Risk Team (Controls)
        │   ├── Risk Quant
        │   └── Compliance Officer
        └── Portfolio Manager (Allocation)
"""

from dataclasses import dataclass, field
from typing import List, Dict, Any, Optional
from enum import Enum

from .personas import (
    Department,
    AgentPersona,
    ALL_PERSONAS,
    INVESTMENT_COMMITTEE_CHAIR,
    RESEARCH_LEAD,
    SIGNAL_SCIENTIST,
    DATA_SCIENTIST,
    QUANT_RESEARCHER,
    EXECUTION_TRADER,
    MARKET_MAKER,
    RISK_QUANT,
    COMPLIANCE_OFFICER,
    PORTFOLIO_MANAGER,
)


class TeamType(str, Enum):
    """Types of teams"""
    COMMITTEE = "committee"      # Decision-making body
    FUNCTIONAL = "functional"    # Operational team
    WORKING_GROUP = "working_group"  # Cross-functional


@dataclass
class TeamDefinition:
    """Definition of a team in the hierarchy"""
    id: str
    name: str
    team_type: TeamType
    description: str
    leader: AgentPersona
    members: List[AgentPersona]
    sub_teams: List['TeamDefinition'] = field(default_factory=list)
    parent_team: Optional[str] = None

    # Collaboration settings
    share_interactions: bool = True  # Members see each other's work
    require_consensus: bool = False  # All must agree
    delegation_mode: str = "intelligent"  # intelligent, round_robin, broadcast

    # Communication protocols
    escalation_path: List[str] = field(default_factory=list)
    meeting_frequency: str = "as_needed"

    def get_all_members(self) -> List[AgentPersona]:
        """Get all members including sub-team members"""
        all_members = list(self.members)
        for sub_team in self.sub_teams:
            all_members.extend(sub_team.get_all_members())
        return all_members

    def get_member_ids(self) -> List[str]:
        """Get IDs of direct members"""
        return [m.id for m in self.members]


# =============================================================================
# TEAM DEFINITIONS
# =============================================================================

RESEARCH_TEAM = TeamDefinition(
    id="research_team",
    name="Quantitative Research Team",
    team_type=TeamType.FUNCTIONAL,
    description="""Alpha generation team responsible for discovering and validating
    trading signals. Combines expertise in mathematics, statistics, and data science
    to identify market inefficiencies.""",
    leader=RESEARCH_LEAD,
    members=[
        SIGNAL_SCIENTIST,
        DATA_SCIENTIST,
        QUANT_RESEARCHER,
    ],
    share_interactions=True,  # Research is collaborative
    require_consensus=False,  # Lead makes final call
    delegation_mode="intelligent",  # Route based on expertise
    escalation_path=["research_lead", "investment_committee_chair"],
    meeting_frequency="daily",
)

TRADING_TEAM = TeamDefinition(
    id="trading_team",
    name="Execution & Trading Team",
    team_type=TeamType.FUNCTIONAL,
    description="""Responsible for executing trades with minimal market impact
    and managing market-making activities. Focus on preserving alpha through
    optimal execution.""",
    leader=EXECUTION_TRADER,  # Execution trader leads
    members=[
        EXECUTION_TRADER,
        MARKET_MAKER,
    ],
    share_interactions=True,
    require_consensus=False,
    delegation_mode="intelligent",
    escalation_path=["execution_trader", "investment_committee_chair"],
    meeting_frequency="continuous",  # Real-time coordination
)

RISK_TEAM = TeamDefinition(
    id="risk_team",
    name="Risk Management Team",
    team_type=TeamType.FUNCTIONAL,
    description="""Monitors and controls risk across all activities. Has authority
    to halt trading if risk limits are breached. Reports directly to Investment
    Committee.""",
    leader=RISK_QUANT,
    members=[
        RISK_QUANT,
        COMPLIANCE_OFFICER,
    ],
    share_interactions=True,
    require_consensus=True,  # Both must agree on risk issues
    delegation_mode="broadcast",  # Both see all risk queries
    escalation_path=["risk_quant", "compliance_officer", "investment_committee_chair"],
    meeting_frequency="daily",
)

INVESTMENT_COMMITTEE = TeamDefinition(
    id="investment_committee",
    name="Medallion Investment Committee",
    team_type=TeamType.COMMITTEE,
    description="""Top-level decision-making body. Approves all trading signals,
    allocates capital, and sets risk limits. Synthesizes input from all teams
    to make systematic investment decisions.""",
    leader=INVESTMENT_COMMITTEE_CHAIR,
    members=[
        INVESTMENT_COMMITTEE_CHAIR,
        PORTFOLIO_MANAGER,
    ],
    sub_teams=[
        RESEARCH_TEAM,
        TRADING_TEAM,
        RISK_TEAM,
    ],
    share_interactions=True,
    require_consensus=False,  # Chair has final authority
    delegation_mode="intelligent",
    escalation_path=["investment_committee_chair"],
    meeting_frequency="continuous",
)


# =============================================================================
# HIERARCHY CONFIGURATION
# =============================================================================

@dataclass
class OrganizationConfig:
    """Configuration for the entire organization"""
    name: str
    description: str
    top_level_team: TeamDefinition
    all_teams: Dict[str, TeamDefinition]
    all_personas: Dict[str, AgentPersona]

    # Global settings
    default_model: str = "gpt-4o"
    enable_memory: bool = True
    enable_knowledge: bool = True
    enable_reasoning: bool = True

    # Communication settings
    max_delegation_depth: int = 3
    timeout_seconds: int = 120

    def get_team(self, team_id: str) -> TeamDefinition:
        """Get team by ID"""
        if team_id not in self.all_teams:
            raise ValueError(f"Unknown team: {team_id}")
        return self.all_teams[team_id]

    def get_persona(self, persona_id: str) -> AgentPersona:
        """Get persona by ID"""
        if persona_id not in self.all_personas:
            raise ValueError(f"Unknown persona: {persona_id}")
        return self.all_personas[persona_id]

    def get_reporting_chain(self, persona_id: str) -> List[str]:
        """Get the reporting chain for a persona (up to top)"""
        persona = self.get_persona(persona_id)
        chain = [persona_id]

        current = persona.reports_to
        while current:
            chain.append(current)
            current_persona = self.get_persona(current)
            current = current_persona.reports_to

        return chain


# Build the organization configuration
RENTECH_ORGANIZATION = OrganizationConfig(
    name="Renaissance Technologies",
    description="""Multi-agent system modeling Renaissance Technologies' organizational
    structure. Implements systematic quantitative trading with proper hierarchy,
    collaboration, and decision-making processes.""",
    top_level_team=INVESTMENT_COMMITTEE,
    all_teams={
        "investment_committee": INVESTMENT_COMMITTEE,
        "research_team": RESEARCH_TEAM,
        "trading_team": TRADING_TEAM,
        "risk_team": RISK_TEAM,
    },
    all_personas=ALL_PERSONAS,
    default_model="gpt-4o",
    enable_memory=True,
    enable_knowledge=True,
    enable_reasoning=True,
    max_delegation_depth=3,
    timeout_seconds=120,
)


# =============================================================================
# HELPER FUNCTIONS
# =============================================================================

def get_team_for_persona(persona_id: str) -> Optional[TeamDefinition]:
    """Find which team a persona belongs to"""
    for team in RENTECH_ORGANIZATION.all_teams.values():
        if persona_id in [m.id for m in team.members]:
            return team
    return None


def get_collaboration_partners(persona_id: str) -> List[str]:
    """Get IDs of agents this persona collaborates with"""
    persona = RENTECH_ORGANIZATION.get_persona(persona_id)
    return persona.collaborates_with


def get_escalation_path(persona_id: str) -> List[str]:
    """Get escalation path for a persona"""
    team = get_team_for_persona(persona_id)
    if team:
        return team.escalation_path
    return [INVESTMENT_COMMITTEE_CHAIR.id]


def should_involve_team(query: str, team_id: str) -> bool:
    """Determine if a query should involve a specific team"""
    team = RENTECH_ORGANIZATION.get_team(team_id)

    # Keywords that indicate team involvement
    team_keywords = {
        "research_team": ["signal", "alpha", "pattern", "backtest", "research", "discover"],
        "trading_team": ["execute", "trade", "order", "fill", "market impact", "execution"],
        "risk_team": ["risk", "limit", "compliance", "var", "drawdown", "regulatory"],
        "investment_committee": ["approve", "decide", "allocate", "portfolio", "capital"],
    }

    keywords = team_keywords.get(team_id, [])
    query_lower = query.lower()

    return any(kw in query_lower for kw in keywords)


def get_decision_authority(decision_type: str) -> str:
    """Get the persona with authority for a decision type"""
    authority_map = {
        "signal_approval": "investment_committee_chair",
        "trade_execution": "execution_trader",
        "risk_limit_change": "investment_committee_chair",
        "compliance_issue": "compliance_officer",
        "capital_allocation": "portfolio_manager",
        "research_direction": "research_lead",
        "data_quality": "data_scientist",
    }
    return authority_map.get(decision_type, "investment_committee_chair")


# =============================================================================
# WORKFLOW ROUTING
# =============================================================================

@dataclass
class WorkflowStep:
    """A step in a workflow"""
    name: str
    assigned_to: str  # persona_id
    requires_approval_from: Optional[str] = None
    can_escalate_to: Optional[str] = None
    timeout_seconds: int = 60


SIGNAL_APPROVAL_WORKFLOW = [
    WorkflowStep(
        name="data_preparation",
        assigned_to="data_scientist",
        timeout_seconds=30,
    ),
    WorkflowStep(
        name="signal_analysis",
        assigned_to="signal_scientist",
        can_escalate_to="research_lead",
        timeout_seconds=60,
    ),
    WorkflowStep(
        name="statistical_validation",
        assigned_to="quant_researcher",
        can_escalate_to="research_lead",
        timeout_seconds=60,
    ),
    WorkflowStep(
        name="research_review",
        assigned_to="research_lead",
        requires_approval_from="research_lead",
        timeout_seconds=30,
    ),
    WorkflowStep(
        name="risk_assessment",
        assigned_to="risk_quant",
        requires_approval_from="risk_quant",
        timeout_seconds=30,
    ),
    WorkflowStep(
        name="compliance_check",
        assigned_to="compliance_officer",
        requires_approval_from="compliance_officer",
        timeout_seconds=30,
    ),
    WorkflowStep(
        name="final_approval",
        assigned_to="investment_committee_chair",
        requires_approval_from="investment_committee_chair",
        timeout_seconds=30,
    ),
    WorkflowStep(
        name="execution_planning",
        assigned_to="execution_trader",
        timeout_seconds=30,
    ),
]


TRADE_EXECUTION_WORKFLOW = [
    WorkflowStep(
        name="pre_trade_compliance",
        assigned_to="compliance_officer",
        requires_approval_from="compliance_officer",
        timeout_seconds=10,
    ),
    WorkflowStep(
        name="execution_planning",
        assigned_to="execution_trader",
        timeout_seconds=20,
    ),
    WorkflowStep(
        name="risk_check",
        assigned_to="risk_quant",
        requires_approval_from="risk_quant",
        timeout_seconds=10,
    ),
    WorkflowStep(
        name="execute_trade",
        assigned_to="execution_trader",
        timeout_seconds=60,
    ),
    WorkflowStep(
        name="post_trade_analysis",
        assigned_to="execution_trader",
        timeout_seconds=30,
    ),
]


def get_workflow(workflow_name: str) -> List[WorkflowStep]:
    """Get workflow steps by name"""
    workflows = {
        "signal_approval": SIGNAL_APPROVAL_WORKFLOW,
        "trade_execution": TRADE_EXECUTION_WORKFLOW,
    }
    if workflow_name not in workflows:
        raise ValueError(f"Unknown workflow: {workflow_name}")
    return workflows[workflow_name]
