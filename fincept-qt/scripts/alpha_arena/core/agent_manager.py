"""
Agent Manager

Manages agent lifecycle and connections, inspired by ValueCell's RemoteConnections.
"""

import asyncio
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any, Type

from alpha_arena.core.base_agent import BaseAgent, BaseTradingAgent, LLMTradingAgent
from alpha_arena.config.agent_cards import AgentCard, get_agent_card, list_agent_cards
from alpha_arena.utils.logging import get_logger

logger = get_logger("agent_manager")


@dataclass
class AgentContext:
    """Context for a managed agent."""

    name: str
    card: Optional[AgentCard] = None
    instance: Optional[BaseAgent] = None
    is_running: bool = False
    api_key: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    _lock: Optional[asyncio.Lock] = field(default=None, repr=False)

    def __post_init__(self):
        """Initialize the lock after instance creation."""
        if self._lock is None:
            self._lock = asyncio.Lock()


class AgentManager:
    """
    Manager for agent lifecycle and connections.

    Inspired by ValueCell's RemoteConnections pattern.
    """

    def __init__(self):
        self._contexts: Dict[str, AgentContext] = {}
        self._initialized = False

    async def initialize(self):
        """Initialize the agent manager."""
        if self._initialized:
            return

        # Load agent cards
        cards = list_agent_cards()
        for card in cards:
            self._contexts[card.name] = AgentContext(
                name=card.name,
                card=card,
            )

        self._initialized = True
        logger.info(f"Agent manager initialized with {len(self._contexts)} agents")

    def _ensure_initialized(self):
        """Ensure the manager is initialized."""
        if not self._initialized:
            # Sync initialization fallback
            cards = list_agent_cards()
            for card in cards:
                if card.name not in self._contexts:
                    self._contexts[card.name] = AgentContext(
                        name=card.name,
                        card=card,
                    )
            self._initialized = True

    async def start_agent(
        self,
        agent_name: str,
        api_key: Optional[str] = None,
        mode: str = "baseline",
        **kwargs,
    ) -> Optional[BaseAgent]:
        """
        Start an agent by name.

        Args:
            agent_name: Name of the agent to start
            api_key: Optional API key override
            mode: Competition mode
            **kwargs: Additional arguments

        Returns:
            The started agent instance
        """
        self._ensure_initialized()

        if agent_name not in self._contexts:
            # Try to get from cards
            card = get_agent_card(agent_name)
            if not card:
                logger.error(f"Agent not found: {agent_name}")
                return None

            self._contexts[agent_name] = AgentContext(
                name=agent_name,
                card=card,
            )

        ctx = self._contexts[agent_name]

        async with ctx._lock:
            # Return existing if running
            if ctx.is_running and ctx.instance:
                return ctx.instance

            # Store API key
            if api_key:
                ctx.api_key = api_key

            # Create agent instance
            instance = await self._create_agent(ctx, mode, **kwargs)
            if instance is None:
                return None

            # Initialize agent
            if await instance.initialize():
                ctx.instance = instance
                ctx.is_running = True
                logger.info(f"Started agent: {agent_name}")
                return instance
            else:
                logger.error(f"Failed to initialize agent: {agent_name}")
                return None

    async def _create_agent(
        self,
        ctx: AgentContext,
        mode: str = "baseline",
        **kwargs,
    ) -> Optional[BaseAgent]:
        """Create an agent instance from context."""
        if not ctx.card:
            return None

        card = ctx.card
        api_key = ctx.api_key or kwargs.get("api_key")

        # Create LLM trading agent
        agent = LLMTradingAgent(
            name=card.name,
            provider=card.provider,
            model_id=card.model_id,
            api_key=api_key,
            temperature=card.temperature,
            instructions=card.instructions if card.instructions else None,
            mode=mode,
        )

        return agent

    async def stop_agent(self, agent_name: str):
        """Stop an agent."""
        self._ensure_initialized()

        if agent_name not in self._contexts:
            return

        ctx = self._contexts[agent_name]

        async with ctx._lock:
            if ctx.instance:
                await ctx.instance.shutdown()
                ctx.instance = None
            ctx.is_running = False
            logger.info(f"Stopped agent: {agent_name}")

    async def stop_all(self):
        """Stop all running agents."""
        for name in list(self._contexts.keys()):
            await self.stop_agent(name)

    def get_agent(self, agent_name: str) -> Optional[BaseAgent]:
        """Get a running agent instance."""
        self._ensure_initialized()

        ctx = self._contexts.get(agent_name)
        if ctx and ctx.is_running:
            return ctx.instance
        return None

    def list_running_agents(self) -> List[str]:
        """List running agents."""
        self._ensure_initialized()
        return [name for name, ctx in self._contexts.items() if ctx.is_running]

    def list_available_agents(self) -> List[str]:
        """List all available agents."""
        self._ensure_initialized()
        return list(self._contexts.keys())

    def get_agent_card(self, agent_name: str) -> Optional[AgentCard]:
        """Get the agent card for a given agent."""
        self._ensure_initialized()
        ctx = self._contexts.get(agent_name)
        return ctx.card if ctx else None

    def get_all_agent_cards(self) -> Dict[str, AgentCard]:
        """Get all agent cards."""
        self._ensure_initialized()
        return {
            name: ctx.card
            for name, ctx in self._contexts.items()
            if ctx.card is not None
        }

    async def start_agents_for_competition(
        self,
        agent_names: List[str],
        api_keys: Dict[str, str],
        mode: str = "baseline",
    ) -> Dict[str, BaseAgent]:
        """
        Start multiple agents for a competition.

        Args:
            agent_names: Names of agents to start
            api_keys: Dict of provider -> api_key
            mode: Competition mode

        Returns:
            Dict of agent name to agent instance
        """
        agents = {}

        for name in agent_names:
            card = self.get_agent_card(name)
            if not card:
                logger.warning(f"Agent card not found: {name}")
                continue

            # Get API key for this provider
            api_key = api_keys.get(card.provider)

            agent = await self.start_agent(
                agent_name=name,
                api_key=api_key,
                mode=mode,
            )

            if agent:
                agents[name] = agent

        return agents


# Singleton instance
_manager: Optional[AgentManager] = None


async def get_agent_manager() -> AgentManager:
    """Get the global agent manager."""
    global _manager

    if _manager is None:
        _manager = AgentManager()
        await _manager.initialize()

    return _manager
