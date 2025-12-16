"""
Tauri Bridge - Interface between Rust Tauri commands and agent configs

Simple JSON config loader - no complex dependencies
"""

import json
import sys
import argparse
from typing import Dict, Any
from pathlib import Path

# Add parent directory to Python path
sys.path.insert(0, str(Path(__file__).parent.parent))

from finagent_core.base_agent import AgentRegistry, AgentConfig


class TauriBridge:
    """Bridge between Tauri and agent configurations"""

    def __init__(self, base_path: str = None):
        self.base_path = Path(base_path) if base_path else Path(__file__).parent.parent

    def get_config_path(self, category: str) -> Path:
        """Get path to agent config file for category"""
        category_path = self.base_path / category / "configs"

        # Check for agent_definitions.json or team_config.json
        agent_defs = category_path / "agent_definitions.json"
        team_config = category_path / "team_config.json"

        if agent_defs.exists():
            return agent_defs
        elif team_config.exists():
            return team_config
        else:
            raise FileNotFoundError(f"No config found for category: {category}")

    def list_agents(self, parameters: Dict) -> Dict:
        """List all agents in a category"""
        try:
            category = parameters.get("category", "TraderInvestorsAgent")
            config_path = self.get_config_path(category)

            registry = AgentRegistry(config_path)
            agents = registry.list_agents()

            return {
                "success": True,
                "agents": [agent.to_dict() for agent in agents],
                "count": len(agents)
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "agents": []
            }

    def get_config(self, parameters: Dict) -> Dict:
        """Get configuration for a specific agent"""
        try:
            agent_id = parameters.get("agent_id")
            category = parameters.get("category", "TraderInvestorsAgent")

            config_path = self.get_config_path(category)
            registry = AgentRegistry(config_path)
            agent = registry.get_agent(agent_id)

            if agent:
                return {
                    "success": True,
                    "config": agent.to_dict()
                }
            else:
                return {
                    "success": False,
                    "error": f"Agent not found: {agent_id}"
                }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def update_config(self, parameters: Dict) -> Dict:
        """Update agent configuration"""
        try:
            agent_id = parameters.get("agent_id")
            category = parameters.get("category", "TraderInvestorsAgent")
            updates = parameters.get("updates", {})

            config_path = self.get_config_path(category)
            registry = AgentRegistry(config_path)

            if registry.update_agent_config(agent_id, updates):
                if registry.save_configs_to_json(config_path):
                    return {
                        "success": True,
                        "message": "Configuration updated successfully"
                    }
                else:
                    return {
                        "success": False,
                        "error": "Failed to save configuration"
                    }
            else:
                return {
                    "success": False,
                    "error": f"Agent not found: {agent_id}"
                }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def get_providers(self, parameters: Dict) -> Dict:
        """Get available LLM providers"""
        return {
            "success": True,
            "providers": {
                "openai": {
                    "name": "OpenAI",
                    "models": ["gpt-4-turbo", "gpt-4", "gpt-3.5-turbo"],
                    "api_key_required": True
                },
                "anthropic": {
                    "name": "Anthropic",
                    "models": ["claude-3-opus-20240229", "claude-3-sonnet-20240229", "claude-3-haiku-20240307"],
                    "api_key_required": True
                },
                "google": {
                    "name": "Google",
                    "models": ["gemini-pro", "gemini-1.5-pro-latest"],
                    "api_key_required": True
                },
                "ollama": {
                    "name": "Ollama",
                    "models": ["llama2", "mistral", "mixtral"],
                    "api_key_required": False
                }
            }
        }

    def execute_agent(self, parameters: Dict, inputs: Dict) -> Dict:
        """Execute an agent (placeholder - actual execution requires LLM setup)"""
        try:
            agent_id = parameters.get("agent_id")
            category = parameters.get("category", "TraderInvestorsAgent")
            query = parameters.get("query", "")

            config_path = self.get_config_path(category)
            registry = AgentRegistry(config_path)
            agent = registry.get_agent(agent_id)

            if not agent:
                return {
                    "success": False,
                    "error": f"Agent not found: {agent_id}"
                }

            # For now, return agent info (actual LLM execution would go here)
            return {
                "success": True,
                "agent_id": agent_id,
                "response": f"Agent '{agent.name}' received query: {query}\n\nNote: Full LLM execution requires API keys and Agno framework setup.",
                "config": agent.to_dict()
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def execute(self, action: str, parameters: Dict, inputs: Dict) -> Dict:
        """Execute an action"""
        actions = {
            "list_agents": self.list_agents,
            "get_config": self.get_config,
            "update_config": self.update_config,
            "get_providers": self.get_providers,
            "execute_agent": self.execute_agent,
        }

        handler = actions.get(action)
        if handler:
            if action == "execute_agent":
                return handler(parameters, inputs)
            else:
                return handler(parameters)
        else:
            return {
                "success": False,
                "error": f"Unknown action: {action}"
            }


def main():
    """CLI entry point"""
    parser = argparse.ArgumentParser(description="Tauri Bridge for Agent Configuration")
    parser.add_argument("--action", required=True, help="Action to perform")
    parser.add_argument("--parameters", default="{}", help="JSON parameters")
    parser.add_argument("--inputs", default="{}", help="JSON inputs")
    parser.add_argument("--base-path", help="Base path for agent configs")

    args = parser.parse_args()

    try:
        parameters = json.loads(args.parameters)
        inputs = json.loads(args.inputs)
    except json.JSONDecodeError as e:
        print(json.dumps({
            "success": False,
            "error": f"Invalid JSON: {str(e)}"
        }))
        sys.exit(1)

    bridge = TauriBridge(args.base_path)
    result = bridge.execute(args.action, parameters, inputs)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
