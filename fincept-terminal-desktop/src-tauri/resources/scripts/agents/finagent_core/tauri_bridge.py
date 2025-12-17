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
from finagent_core.llm_executor import LLMExecutor


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

            # Check if this is a team configuration
            team_name = registry.config_data.get("team_name", None)
            team_description = registry.config_data.get("team_description", None)

            return {
                "success": True,
                "agents": [agent.to_dict() for agent in agents],
                "count": len(agents),
                "team_name": team_name,
                "team_description": team_description
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
        """Execute an agent with real LLM execution"""
        try:
            agent_id = parameters.get("agent_id")
            category = parameters.get("category", "TraderInvestorsAgent")
            query = parameters.get("query", "")
            api_keys = inputs.get("api_keys", {})
            llm_override = inputs.get("llm_override", None)

            config_path = self.get_config_path(category)
            registry = AgentRegistry(config_path)
            agent = registry.get_agent(agent_id)

            if not agent:
                return {
                    "success": False,
                    "error": f"Agent not found: {agent_id}"
                }

            # Override LLM config with active provider from Settings
            agent_config = agent.to_dict()
            if llm_override:
                if "llm_config" not in agent_config:
                    agent_config["llm_config"] = {}
                agent_config["llm_config"]["provider"] = llm_override.get("provider")
                agent_config["llm_config"]["model_id"] = llm_override.get("model")

            # Execute with real LLM
            executor = LLMExecutor(api_keys)
            result = executor.execute(agent_config, query)

            # Add agent metadata to result
            if result.get("success"):
                result["agent_id"] = agent_id
                result["agent_name"] = agent.name
                result["config"] = agent_config

            return result
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def execute_team_collaboration(self, parameters: Dict, inputs: Dict) -> Dict:
        """Execute multi-agent collaboration discussion - SEQUENTIAL, one at a time"""
        try:
            category = parameters.get("category", "GeopoliticsAgents")
            query = parameters.get("query", "")
            api_keys = inputs.get("api_keys", {})
            llm_override = inputs.get("llm_override", None)
            agent_ids = parameters.get("agent_ids", None)  # NEW: filter by specific agents

            config_path = self.get_config_path(category)
            registry = AgentRegistry(config_path)
            all_agents = registry.list_agents()

            # Filter agents if specific IDs provided (for book-specific discussion)
            if agent_ids:
                agents_list = [a for a in all_agents if a.id in agent_ids]
            else:
                agents_list = all_agents

            if not agents_list:
                return {"success": False, "error": "No agents found"}

            executor = LLMExecutor(api_keys)
            discussion = []

            # ROUND 1: Each agent analyzes query ONE BY ONE
            import sys
            sys.stderr.write(f"\n=== ROUND 1: Initial Analysis ({len(agents_list)} agents) ===\n")
            for idx, agent in enumerate(agents_list, 1):
                sys.stderr.write(f"Agent {idx}/{len(agents_list)}: {agent.name}\n")

                agent_config = agent.to_dict()
                if llm_override:
                    if "llm_config" not in agent_config:
                        agent_config["llm_config"] = {}
                    agent_config["llm_config"]["provider"] = llm_override.get("provider")
                    agent_config["llm_config"]["model_id"] = llm_override.get("model")

                result = executor.execute(agent_config, query)
                if result.get("success"):
                    response = result.get("response", "")
                    discussion.append({
                        "agent": agent.name,
                        "role": agent.role,
                        "response": response,
                        "round": 1
                    })
                    sys.stderr.write(f"[OK] {agent.name}: {response[:100]}...\n")
                else:
                    sys.stderr.write(f"[FAIL] {agent.name}: Failed - {result.get('error', 'Unknown error')}\n")

            # ROUND 2: Agents respond to discussion ONE BY ONE
            sys.stderr.write(f"\n=== ROUND 2: Response to Discussion ===\n")
            context = f"Original Query: {query}\n\nDiscussion so far:\n"
            for d in discussion:
                context += f"\n{d['agent']}: {d['response'][:150]}...\n"
            context += "\n\nBased on the above discussion, provide your additional insights:"

            for idx, agent in enumerate(agents_list, 1):
                sys.stderr.write(f"Agent {idx}/{len(agents_list)}: {agent.name}\n")

                agent_config = agent.to_dict()
                if llm_override:
                    agent_config["llm_config"]["provider"] = llm_override.get("provider")
                    agent_config["llm_config"]["model_id"] = llm_override.get("model")

                result = executor.execute(agent_config, context)
                if result.get("success"):
                    response = result.get("response", "")
                    discussion.append({
                        "agent": agent.name,
                        "role": agent.role,
                        "response": response,
                        "round": 2
                    })
                    sys.stderr.write(f"[OK] {agent.name}: {response[:100]}...\n")
                else:
                    sys.stderr.write(f"[FAIL] {agent.name}: Failed - {result.get('error', 'Unknown error')}\n")

            return {
                "success": True,
                "discussion": discussion,
                "total_agents": len(agents_list),
                "rounds": 2,
                "query": query
            }
        except Exception as e:
            import traceback
            sys.stderr.write(f"ERROR in execute_team_collaboration: {str(e)}\n")
            sys.stderr.write(traceback.format_exc())
            return {"success": False, "error": str(e)}

    def execute(self, action: str, parameters: Dict, inputs: Dict) -> Dict:
        """Execute an action"""
        actions = {
            "list_agents": self.list_agents,
            "get_config": self.get_config,
            "update_config": self.update_config,
            "get_providers": self.get_providers,
            "execute_agent": self.execute_agent,
            "execute_team_collaboration": self.execute_team_collaboration,
        }

        handler = actions.get(action)
        if handler:
            if action in ["execute_agent", "execute_team_collaboration"]:
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
