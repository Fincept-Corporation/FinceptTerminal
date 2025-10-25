"""
Economic Agents Package
Contains various economic ideology agents for analyzing economic data
"""

from .base_agent import EconomicAgent, EconomicData, PolicyRecommendation
from .capitalism_agent import CapitalismAgent
from .socialism_agent import SocialismAgent
from .mixed_economy_agent import MixedEconomyAgent
from .keynesian_agent import KeynesianAgent
from .neoliberal_agent import NeoliberalAgent
from .mercantilist_agent import MercantilistAgent
from .data_processor import EconomicDataProcessor
from .comparison import EconomicAgentComparator
from .config import EconomicConfig, get_config
from .utils import *
from .cli import EconomicCLI

__version__ = "1.0.0"
__author__ = "Economic Analysis Team"

# Import required packages
try:
    import pandas as pd
    HAS_PANDAS = True
except ImportError:
    HAS_PANDAS = False

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False

__all__ = [
    # Core classes
    'EconomicAgent',
    'EconomicData',
    'PolicyRecommendation',

    # Individual agents
    'CapitalismAgent',
    'SocialismAgent',
    'MixedEconomyAgent',
    'KeynesianAgent',
    'NeoliberalAgent',
    'MercantilistAgent',

    # Utilities and processors
    'EconomicDataProcessor',
    'EconomicAgentComparator',
    'EconomicConfig',
    'EconomicCLI',

    # Configuration
    'get_config',

    # Legacy functions
    'get_agent',
    'get_all_agents',
    'list_agents'
]

# Factory function to get agents by name
def get_agent(ideology: str) -> EconomicAgent:
    """
    Factory function to get an economic agent by ideology name

    Args:
        ideology (str): Name of the economic ideology

    Returns:
        EconomicAgent: The corresponding economic agent

    Raises:
        ValueError: If ideology is not supported
    """
    agents = {
        'capitalism': CapitalismAgent(),
        'socialism': SocialismAgent(),
        'mixed_economy': MixedEconomyAgent(),
        'keynesian': KeynesianAgent(),
        'neoliberal': NeoliberalAgent(),
        'mercantilism': MercantilistAgent()
    }

    ideology_lower = ideology.lower().replace(' ', '_')

    if ideology_lower in agents:
        return agents[ideology_lower]
    else:
        available = ', '.join(agents.keys())
        raise ValueError(f"Ideology '{ideology}' not supported. Available: {available}")

# List all available agents
def list_agents():
    """Return list of all available economic agents"""
    return [
        'capitalism',
        'socialism',
        'mixed_economy',
        'keynesian',
        'neoliberal',
        'mercantilism'
    ]

# Get all agents
def get_all_agents():
    """Return dictionary of all available economic agents"""
    return {
        'capitalism': CapitalismAgent(),
        'socialism': SocialismAgent(),
        'mixed_economy': MixedEconomyAgent(),
        'keynesian': KeynesianAgent(),
        'neoliberal': NeoliberalAgent(),
        'mercantilism': MercantilistAgent()
    }