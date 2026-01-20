"""
Alpha Arena Configuration Module

AgentCard-based configuration system for trading agents.
"""

from alpha_arena.config.agent_cards import (
    AgentCard,
    AgentCapabilities,
    load_agent_cards,
    save_agent_card,
    get_agent_card,
    list_agent_cards,
)

__all__ = [
    "AgentCard",
    "AgentCapabilities",
    "load_agent_cards",
    "save_agent_card",
    "get_agent_card",
    "list_agent_cards",
]
