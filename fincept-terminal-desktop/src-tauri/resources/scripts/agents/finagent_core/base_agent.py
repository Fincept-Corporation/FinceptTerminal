"""
Base Agent Module - Simplified JSON-based agent configuration loader
No external dependencies - just loads and manages agent configs
"""

import json
from pathlib import Path
from typing import Dict, Any, List, Optional


class AgentConfig:
    """Simple agent configuration container"""
    def __init__(self, config_dict: Dict[str, Any]):
        self.id = config_dict.get('id', '')
        self.name = config_dict.get('name', '')
        self.role = config_dict.get('role', '')
        self.goal = config_dict.get('goal', '')
        self.description = config_dict.get('description', '')
        self.llm_config = config_dict.get('llm_config', {})
        self.tools = config_dict.get('tools', [])
        self.enable_memory = config_dict.get('enable_memory', False)
        self.enable_agentic_memory = config_dict.get('enable_agentic_memory', False)
        self.instructions = config_dict.get('instructions', '')
        self._raw_config = config_dict

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return self._raw_config


class AgentRegistry:
    """Registry for loading and managing agent configurations"""

    def __init__(self, config_path: Optional[Path] = None):
        self.config_path = config_path
        self.agents: Dict[str, AgentConfig] = {}

        if config_path and config_path.exists():
            self.load_configs_from_json(config_path)

    def load_configs_from_json(self, json_path: Path) -> None:
        """Load agent configurations from JSON file"""
        try:
            with open(json_path, 'r', encoding='utf-8') as f:
                data = json.load(f)

            agents_list = data.get('agents', [])
            for agent_dict in agents_list:
                agent_config = AgentConfig(agent_dict)
                self.agents[agent_config.id] = agent_config

        except Exception as e:
            print(f"Error loading agent configs from {json_path}: {e}")

    def get_agent(self, agent_id: str) -> Optional[AgentConfig]:
        """Get agent configuration by ID"""
        return self.agents.get(agent_id)

    def list_agents(self) -> List[AgentConfig]:
        """List all registered agents"""
        return list(self.agents.values())

    def update_agent_config(self, agent_id: str, updates: Dict[str, Any]) -> bool:
        """Update agent configuration"""
        if agent_id in self.agents:
            self.agents[agent_id] = AgentConfig(updates)
            return True
        return False

    def save_configs_to_json(self, json_path: Path) -> bool:
        """Save all agent configurations back to JSON file"""
        try:
            agents_list = [agent.to_dict() for agent in self.agents.values()]
            data = {'agents': agents_list}

            with open(json_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2)

            return True
        except Exception as e:
            print(f"Error saving agent configs to {json_path}: {e}")
            return False


# Compatibility alias
ConfigurableAgent = AgentConfig
