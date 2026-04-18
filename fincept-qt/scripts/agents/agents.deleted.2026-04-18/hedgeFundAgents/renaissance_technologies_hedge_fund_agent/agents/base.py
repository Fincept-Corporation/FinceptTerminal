"""
Base Agent Factory for Renaissance Technologies

Provides factory functions to create Agno agents with consistent configuration.
Now uses dynamic model factory that reads from frontend SQLite database.
"""

from typing import Optional, List, Any, Callable
from agno.agent import Agent
from agno.tools import Toolkit

from ..organization.personas import AgentPersona, get_persona_instructions
from ..config import get_config, RenTechConfig
from ..utils.model_factory import create_model_from_config


def create_model(config: Optional[RenTechConfig] = None):
    """
    Create the appropriate model based on frontend configuration.

    Reads active LLM config from frontend SQLite database.
    Supports 40+ providers including OpenAI, Anthropic, Google, Groq, Ollama, etc.
    """
    cfg = config or get_config()

    # Use dynamic model factory - reads from frontend DB
    # Falls back to default if no config found
    return create_model_from_config({
        "temperature": cfg.models.temperature,
        "max_tokens": cfg.models.max_tokens,
    })


def create_reasoning_model(config: Optional[RenTechConfig] = None):
    """
    Create model for reasoning tasks.

    Uses same provider as main model but with lower temperature.
    """
    cfg = config or get_config()

    # Use dynamic model factory with reasoning-optimized settings
    return create_model_from_config({
        "temperature": 0.3,  # Lower temperature for reasoning
        "max_tokens": cfg.models.max_tokens,
    })


def create_agent_from_persona(
    persona: AgentPersona,
    tools: Optional[List[Toolkit]] = None,
    config: Optional[RenTechConfig] = None,
    additional_instructions: Optional[str] = None,
) -> Agent:
    """
    Create an Agno Agent from a persona definition.

    Args:
        persona: The AgentPersona defining the agent's role
        tools: Optional list of tools for the agent
        config: Optional configuration override
        additional_instructions: Extra instructions to append

    Returns:
        Configured Agno Agent
    """
    cfg = config or get_config()

    # Build full instructions from persona
    instructions = get_persona_instructions(persona.id)
    if additional_instructions:
        instructions = f"{instructions}\n\n{additional_instructions}"

    # Create the agent
    agent = Agent(
        name=persona.name,
        role=persona.title,
        model=create_model(cfg),
        instructions=instructions,
        tools=tools or [],

        # Enable reasoning if configured
        reasoning=cfg.enable_reasoning,
        reasoning_model=create_reasoning_model(cfg) if cfg.enable_reasoning else None,
        reasoning_min_steps=cfg.models.reasoning_min_steps,
        reasoning_max_steps=cfg.models.reasoning_max_steps,

        # Memory settings (will be configured when memory manager is added)
        enable_agentic_memory=cfg.enable_memory,
        add_memories_to_context=cfg.enable_memory,

        # Knowledge settings (will be configured when knowledge base is added)
        search_knowledge=cfg.enable_knowledge,
        add_knowledge_to_context=cfg.enable_knowledge,

        # Output settings
        markdown=True,

        # Metadata
        description=persona.title,
    )

    return agent


class AgentFactory:
    """
    Factory for creating all Renaissance Technologies agents.

    Provides a centralized way to create and configure agents
    with consistent settings.
    """

    def __init__(self, config: Optional[RenTechConfig] = None):
        self.config = config or get_config()
        self._agents = {}

    def get_or_create(
        self,
        persona: AgentPersona,
        tools: Optional[List[Toolkit]] = None,
    ) -> Agent:
        """Get existing agent or create new one"""
        if persona.id not in self._agents:
            self._agents[persona.id] = create_agent_from_persona(
                persona=persona,
                tools=tools,
                config=self.config,
            )
        return self._agents[persona.id]

    def get_agent(self, persona_id: str) -> Optional[Agent]:
        """Get an agent by persona ID"""
        return self._agents.get(persona_id)

    def list_agents(self) -> List[str]:
        """List all created agent IDs"""
        return list(self._agents.keys())

    def reset(self):
        """Reset all agents"""
        self._agents = {}


# Global factory instance
_factory: Optional[AgentFactory] = None


def get_agent_factory() -> AgentFactory:
    """Get the global agent factory"""
    global _factory
    if _factory is None:
        _factory = AgentFactory()
    return _factory


def reset_agent_factory():
    """Reset the global agent factory"""
    global _factory
    _factory = None
