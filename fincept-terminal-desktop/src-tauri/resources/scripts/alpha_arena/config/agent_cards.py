"""
AgentCard Configuration System

JSON-based agent configuration inspired by ValueCell's AgentCard architecture.
Provides discovery and configuration for trading agents.
"""

import json
from pathlib import Path
from typing import Dict, List, Optional, Any
from pydantic import BaseModel, Field

from alpha_arena.utils.logging import get_logger

logger = get_logger("agent_cards")


class AgentCapabilities(BaseModel):
    """Capabilities of an agent."""

    streaming: bool = Field(True, description="Supports streaming responses")
    trading: bool = Field(True, description="Can execute trades")
    analysis: bool = Field(True, description="Can perform market analysis")
    notifications: bool = Field(False, description="Can send notifications")
    short_selling: bool = Field(False, description="Can short sell")
    leverage: bool = Field(False, description="Can use leverage")


class AgentSkill(BaseModel):
    """Skill definition for an agent."""

    id: str = Field(..., description="Skill identifier")
    name: str = Field(..., description="Display name")
    description: str = Field(..., description="Skill description")
    parameters: Dict[str, Any] = Field(default_factory=dict, description="Skill parameters")


class AgentCard(BaseModel):
    """
    Agent configuration card following A2A Protocol patterns.

    Provides all configuration needed to instantiate and connect to an agent.
    """

    name: str = Field(..., description="Agent display name")
    description: str = Field("", description="Agent description")
    version: str = Field("1.0.0", description="Agent version")

    # Provider configuration
    provider: str = Field(..., description="LLM provider (openai, anthropic, etc.)")
    model_id: str = Field(..., description="Model identifier")

    # Agent class
    agent_class: Optional[str] = Field(
        None,
        description="Python class path (e.g., 'alpha_arena.agents.trader:TraderAgent')"
    )

    # Runtime configuration
    url: Optional[str] = Field(None, description="Agent URL if remote")
    enabled: bool = Field(True, description="Whether agent is enabled")
    capabilities: AgentCapabilities = Field(
        default_factory=AgentCapabilities,
        description="Agent capabilities"
    )
    skills: List[AgentSkill] = Field(default_factory=list, description="Agent skills")

    # Trading configuration
    temperature: float = Field(0.7, ge=0.0, le=2.0, description="LLM temperature")
    max_tokens: Optional[int] = Field(None, description="Max response tokens")
    default_mode: str = Field("baseline", description="Default competition mode")
    risk_level: str = Field("medium", description="Risk level: low, medium, high")

    # Instructions
    system_prompt: Optional[str] = Field(None, description="Custom system prompt")
    instructions: List[str] = Field(default_factory=list, description="Agent instructions")

    # Metadata
    metadata: Dict[str, Any] = Field(default_factory=dict, description="Additional metadata")

    class Config:
        extra = "allow"


# Default agent cards
DEFAULT_AGENT_CARDS: List[AgentCard] = [
    AgentCard(
        name="GPT-4o Mini",
        description="Fast and cost-effective OpenAI model",
        provider="openai",
        model_id="gpt-4o-mini",
        capabilities=AgentCapabilities(streaming=True, trading=True, analysis=True),
        temperature=0.7,
        instructions=["Focus on quick analysis and decisive action"],
    ),
    AgentCard(
        name="GPT-4o",
        description="Most capable OpenAI model",
        provider="openai",
        model_id="gpt-4o",
        capabilities=AgentCapabilities(streaming=True, trading=True, analysis=True),
        temperature=0.7,
        instructions=["Perform thorough analysis before making decisions"],
    ),
    AgentCard(
        name="Claude 3.5 Sonnet",
        description="Anthropic's Claude 3.5 Sonnet",
        provider="anthropic",
        model_id="claude-3-5-sonnet-20241022",
        capabilities=AgentCapabilities(streaming=True, trading=True, analysis=True),
        temperature=0.7,
        instructions=["Use careful reasoning for trading decisions"],
    ),
    AgentCard(
        name="Gemini 2.0 Flash",
        description="Google's fast Gemini model",
        provider="google",
        model_id="gemini-2.0-flash-exp",
        capabilities=AgentCapabilities(streaming=True, trading=True, analysis=True),
        temperature=0.7,
        instructions=["Prioritize speed and efficiency"],
    ),
    AgentCard(
        name="DeepSeek Chat",
        description="DeepSeek's chat model",
        provider="deepseek",
        model_id="deepseek-chat",
        capabilities=AgentCapabilities(streaming=True, trading=True, analysis=True),
        temperature=0.7,
        instructions=["Use deep analytical reasoning"],
    ),
    AgentCard(
        name="Llama 3.3 70B",
        description="Meta's Llama 3.3 via Groq",
        provider="groq",
        model_id="llama-3.3-70b-versatile",
        capabilities=AgentCapabilities(streaming=True, trading=True, analysis=True),
        temperature=0.7,
        instructions=["Balance speed with thorough analysis"],
    ),
    AgentCard(
        name="Qwen 2.5 72B",
        description="Alibaba's Qwen via Groq",
        provider="groq",
        model_id="qwen-2.5-72b",
        capabilities=AgentCapabilities(streaming=True, trading=True, analysis=True),
        temperature=0.7,
        instructions=["Consider global market perspectives"],
    ),
]


# Global registry
_AGENT_CARDS: Dict[str, AgentCard] = {}
_CARDS_LOADED = False


def _get_config_dir() -> Path:
    """Get the configuration directory."""
    # Check for user config first
    config_dir = Path.home() / ".fincept" / "alpha_arena" / "agent_cards"
    if config_dir.exists():
        return config_dir

    # Fall back to script directory
    return Path(__file__).parent / "cards"


def load_agent_cards(config_dir: Optional[Path] = None) -> Dict[str, AgentCard]:
    """
    Load agent cards from JSON configuration files.

    Args:
        config_dir: Optional directory to load from

    Returns:
        Dictionary of agent name to AgentCard
    """
    global _AGENT_CARDS, _CARDS_LOADED

    if config_dir is None:
        config_dir = _get_config_dir()

    # Load defaults first
    for card in DEFAULT_AGENT_CARDS:
        _AGENT_CARDS[card.name] = card

    # Load from config directory
    if config_dir.exists():
        for json_file in config_dir.glob("*.json"):
            try:
                with open(json_file, "r", encoding="utf-8") as f:
                    data = json.load(f)

                # Skip disabled agents
                if not data.get("enabled", True):
                    continue

                card = AgentCard.model_validate(data)
                _AGENT_CARDS[card.name] = card
                logger.debug(f"Loaded agent card: {card.name}")

            except Exception as e:
                logger.warning(f"Failed to load agent card from {json_file}: {e}")

    _CARDS_LOADED = True
    logger.info(f"Loaded {len(_AGENT_CARDS)} agent cards")
    return _AGENT_CARDS


def save_agent_card(card: AgentCard, config_dir: Optional[Path] = None) -> bool:
    """
    Save an agent card to a JSON file.

    Args:
        card: AgentCard to save
        config_dir: Optional directory to save to

    Returns:
        True if saved successfully
    """
    if config_dir is None:
        config_dir = _get_config_dir()

    config_dir.mkdir(parents=True, exist_ok=True)

    file_name = card.name.lower().replace(" ", "_") + ".json"
    file_path = config_dir / file_name

    try:
        with open(file_path, "w", encoding="utf-8") as f:
            json.dump(card.model_dump(), f, indent=2)

        _AGENT_CARDS[card.name] = card
        logger.info(f"Saved agent card: {card.name}")
        return True

    except Exception as e:
        logger.error(f"Failed to save agent card {card.name}: {e}")
        return False


def get_agent_card(name: str) -> Optional[AgentCard]:
    """
    Get an agent card by name.

    Args:
        name: Agent name

    Returns:
        AgentCard if found, None otherwise
    """
    if not _CARDS_LOADED:
        load_agent_cards()

    return _AGENT_CARDS.get(name)


def list_agent_cards() -> List[AgentCard]:
    """
    List all available agent cards.

    Returns:
        List of AgentCard objects
    """
    if not _CARDS_LOADED:
        load_agent_cards()

    return list(_AGENT_CARDS.values())


def get_enabled_agents() -> List[AgentCard]:
    """Get only enabled agents."""
    return [card for card in list_agent_cards() if card.enabled]


def get_agents_by_provider(provider: str) -> List[AgentCard]:
    """Get agents by provider."""
    return [card for card in list_agent_cards() if card.provider == provider]


def create_agent_from_card(
    card: AgentCard,
    api_key: Optional[str] = None,
    mode: str = "baseline",
):
    """
    Create an agent instance from an AgentCard.

    Args:
        card: AgentCard configuration
        api_key: Optional API key override
        mode: Competition mode

    Returns:
        BaseTradingAgent instance
    """
    from alpha_arena.core.base_agent import LLMTradingAgent

    return LLMTradingAgent(
        name=card.name,
        provider=card.provider,
        model_id=card.model_id,
        api_key=api_key,
        temperature=card.temperature,
        instructions=card.instructions if card.instructions else None,
        mode=mode,
    )
