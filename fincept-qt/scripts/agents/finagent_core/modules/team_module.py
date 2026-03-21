"""
Team Module - Multi-agent collaboration and orchestration

Provides:
- Agent team creation
- Team coordination modes
- Parallel and sequential execution
- Role-based agent assignment
"""

from typing import Dict, Any, Optional, List, Callable
import logging

logger = logging.getLogger(__name__)


class TeamModule:
    """
    Multi-agent team management.

    Supports:
    - Creating teams of specialized agents
    - Coordinate, route, and collaborate modes
    - Event handling for team operations
    """

    # Team coordination modes
    COORDINATION_MODES = {
        "coordinate": "Team coordinates agents to work together",
        "route": "Team routes queries to the most appropriate agent",
        "collaborate": "Agents collaborate on complex tasks",
    }

    def __init__(
        self,
        name: str = "Agent Team",
        description: Optional[str] = None,
        mode: str = "coordinate",
        leader_agent: Optional[Any] = None,
        model: Optional[Any] = None,
        show_members_responses: bool = False,
        leader_index: Optional[int] = None,
        **kwargs
    ):
        """
        Initialize team module.

        Args:
            name: Team name
            description: Team description
            mode: Coordination mode ('coordinate', 'route', 'collaborate')
            leader_agent: Optional leader/coordinator agent
            model: Coordinator model for team decisions
            show_members_responses: Show individual member responses in output
            leader_index: Index of the leader agent in the members list
            **kwargs: Additional configuration
        """
        self.name = name
        self.description = description
        self.mode = mode
        self.leader_agent = leader_agent
        self.model = model
        self.show_members_responses = show_members_responses
        self.leader_index = leader_index
        self.config = kwargs

        self._agents: List[Any] = []
        self._team = None
        self._event_handlers: Dict[str, List[Callable]] = {}

    def add_agent(
        self,
        agent: Any,
        role: Optional[str] = None,
        **metadata
    ) -> "TeamModule":
        """
        Add an agent to the team.

        Args:
            agent: Agno Agent instance
            role: Agent's role in the team
            **metadata: Additional agent metadata

        Returns:
            self for chaining
        """
        agent_info = {
            "agent": agent,
            "role": role,
            **metadata
        }
        self._agents.append(agent_info)
        return self

    def add_agents(self, agents: List[Any]) -> "TeamModule":
        """
        Add multiple agents to the team.

        Args:
            agents: List of Agno Agent instances

        Returns:
            self for chaining
        """
        for agent in agents:
            self.add_agent(agent)
        return self

    def build(self) -> Any:
        """
        Build the Agno Team.

        Returns:
            Agno Team instance
        """
        if self._team:
            return self._team

        if not self._agents:
            raise ValueError("Cannot build team without agents")

        try:
            from agno.team import Team

            # Extract agent instances
            # Agno Team uses 'members' not 'agents'
            members = [info["agent"] for info in self._agents]

            # If leader_index is set, pick that agent as leader
            leader = self.leader_agent
            if leader is None and self.leader_index is not None and 0 <= self.leader_index < len(members):
                leader = members[self.leader_index]

            team_kwargs = {
                "members": members,  # Required parameter
            }

            if self.name:
                team_kwargs["name"] = self.name

            # Add coordinator model (required for team to make decisions)
            if self.model:
                team_kwargs["model"] = self.model

            # Show individual member responses
            if self.show_members_responses:
                team_kwargs["show_members_responses"] = True

            # Add mode-specific configuration
            if self.mode == "coordinate":
                # Default: leader delegates to chosen members
                pass
            elif self.mode == "route":
                team_kwargs["respond_directly"] = False
            elif self.mode == "collaborate":
                team_kwargs["delegate_to_all_members"] = True
                team_kwargs["share_member_interactions"] = True

            # If a leader agent is specified, use its instructions/role for the team
            # In Agno, the Team itself acts as the leader — members are workers
            if leader:
                if hasattr(leader, 'instructions') and leader.instructions:
                    team_kwargs["instructions"] = leader.instructions
                if hasattr(leader, 'description') and leader.description:
                    team_kwargs["description"] = leader.description

            self._team = Team(**team_kwargs)

            # Register event handlers
            self._register_event_handlers()

            logger.debug(f"Built team '{self.name}' with {len(members)} agents, mode={self.mode}")
            return self._team

        except ImportError as e:
            raise ImportError(f"Agno Team not available: {e}")
        except Exception as e:
            raise RuntimeError(f"Failed to build team: {e}")

    def _register_event_handlers(self) -> None:
        """Register event handlers with the team"""
        if not self._team:
            return

        # Event types from Agno
        event_types = [
            "RunStartedEvent",
            "RunCompletedEvent",
            "RunErrorEvent",
            "ToolCallStartedEvent",
            "ToolCallCompletedEvent",
            "ReasoningStartedEvent",
            "ReasoningCompletedEvent",
        ]

        for event_type in event_types:
            handlers = self._event_handlers.get(event_type, [])
            for handler in handlers:
                try:
                    self._team.on(event_type, handler)
                except Exception as e:
                    logger.warning(f"Failed to register handler for {event_type}: {e}")

    def on(self, event_type: str, handler: Callable) -> "TeamModule":
        """
        Register an event handler.

        Args:
            event_type: Type of event to handle
            handler: Callback function

        Returns:
            self for chaining
        """
        if event_type not in self._event_handlers:
            self._event_handlers[event_type] = []
        self._event_handlers[event_type].append(handler)
        return self

    def run(
        self,
        query: str,
        session_id: Optional[str] = None,
        **kwargs
    ) -> Any:
        """
        Run the team on a query.

        Args:
            query: User query
            session_id: Optional session ID
            **kwargs: Additional arguments

        Returns:
            Team response
        """
        team = self.build()

        # Agno Team uses 'input' not 'message'
        run_kwargs = {"input": query}
        if session_id:
            run_kwargs["session_id"] = session_id
        run_kwargs.update(kwargs)

        return team.run(**run_kwargs)

    async def arun(
        self,
        query: str,
        session_id: Optional[str] = None,
        **kwargs
    ) -> Any:
        """
        Run the team asynchronously.

        Args:
            query: User query
            session_id: Optional session ID
            **kwargs: Additional arguments

        Returns:
            Team response
        """
        team = self.build()

        # Agno Team uses 'input' not 'message'
        run_kwargs = {"input": query}
        if session_id:
            run_kwargs["session_id"] = session_id
        run_kwargs.update(kwargs)

        return await team.arun(**run_kwargs)

    def stream(
        self,
        query: str,
        session_id: Optional[str] = None,
        **kwargs
    ):
        """
        Stream team responses.

        Args:
            query: User query
            session_id: Optional session ID
            **kwargs: Additional arguments

        Yields:
            Response chunks
        """
        team = self.build()

        # Agno Team uses 'input' not 'message'
        run_kwargs = {"input": query, "stream": True}
        if session_id:
            run_kwargs["session_id"] = session_id
        run_kwargs.update(kwargs)

        return team.run(**run_kwargs)

    def get_team(self) -> Optional[Any]:
        """Get the built team instance"""
        return self._team

    def get_agents(self) -> List[Dict[str, Any]]:
        """Get list of agents with their metadata"""
        return self._agents.copy()

    @classmethod
    def from_config(
        cls,
        config: Dict[str, Any],
        agents: List[Any],
        model: Optional[Any] = None
    ) -> "TeamModule":
        """
        Create TeamModule from configuration.

        Args:
            config: Team configuration
            agents: List of Agno Agent instances
            model: Optional coordinator model

        Returns:
            TeamModule instance
        """
        module = cls(
            name=config.get("name", "Agent Team"),
            description=config.get("description"),
            mode=config.get("mode", "coordinate"),
            model=model,
            show_members_responses=config.get("show_members_responses", False),
            leader_index=config.get("leader_index"),
        )

        for i, agent in enumerate(agents):
            role = config.get("roles", {}).get(str(i))
            module.add_agent(agent, role=role)

        return module

    @classmethod
    def list_modes(cls) -> Dict[str, str]:
        """List available coordination modes"""
        return cls.COORDINATION_MODES.copy()


class TeamBuilder:
    """
    Fluent builder for creating agent teams.

    Example:
        team = (TeamBuilder("Research Team")
            .with_mode("collaborate")
            .add_researcher(researcher_agent)
            .add_writer(writer_agent)
            .build())
    """

    def __init__(self, name: str):
        """Initialize team builder"""
        self._module = TeamModule(name=name)

    def with_description(self, description: str) -> "TeamBuilder":
        """Set team description"""
        self._module.description = description
        return self

    def with_mode(self, mode: str) -> "TeamBuilder":
        """Set coordination mode"""
        self._module.mode = mode
        return self

    def with_leader(self, agent: Any) -> "TeamBuilder":
        """Set leader agent"""
        self._module.leader_agent = agent
        return self

    def add_agent(
        self,
        agent: Any,
        role: Optional[str] = None
    ) -> "TeamBuilder":
        """Add an agent to the team"""
        self._module.add_agent(agent, role=role)
        return self

    def on_event(self, event_type: str, handler: Callable) -> "TeamBuilder":
        """Register event handler"""
        self._module.on(event_type, handler)
        return self

    def build(self) -> Any:
        """Build and return the team"""
        return self._module.build()

    def get_module(self) -> TeamModule:
        """Get the underlying TeamModule"""
        return self._module
