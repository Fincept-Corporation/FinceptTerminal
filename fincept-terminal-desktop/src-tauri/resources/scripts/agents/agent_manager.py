"""
Agent Manager - Unified agent operations following yfinance pattern
Handles agent configuration, execution, and management
Returns JSON output for Rust integration
"""

import sys
import json
from pathlib import Path
from typing import Dict, Any, List, Optional

def load_agent_configs(category: str) -> Dict[str, Any]:
    """Load agent configurations from JSON file"""
    try:
        base_path = Path(__file__).parent / category / "configs"

        # Check for agent_definitions.json or team_config.json
        agent_defs = base_path / "agent_definitions.json"
        team_config = base_path / "team_config.json"

        config_file = agent_defs if agent_defs.exists() else team_config

        if not config_file.exists():
            return {"success": False, "error": f"No config found for category: {category}"}

        with open(config_file, 'r', encoding='utf-8') as f:
            data = json.load(f)

        return {
            "success": True,
            "agents": data.get("agents", []),
            "team_name": data.get("team_name"),
            "team_description": data.get("team_description"),
            "count": len(data.get("agents", []))
        }
    except Exception as e:
        return {"success": False, "error": str(e)}

def get_agent_config(category: str, agent_id: str) -> Dict[str, Any]:
    """Get specific agent configuration"""
    try:
        configs = load_agent_configs(category)
        if not configs.get("success"):
            return configs

        agents = configs.get("agents", [])
        agent = next((a for a in agents if a.get("id") == agent_id), None)

        if agent:
            return {"success": True, "config": agent}
        else:
            return {"success": False, "error": f"Agent not found: {agent_id}"}
    except Exception as e:
        return {"success": False, "error": str(e)}

def save_agent_config(category: str, agent_id: str, updates: Dict[str, Any]) -> Dict[str, Any]:
    """Update and save agent configuration"""
    try:
        base_path = Path(__file__).parent / category / "configs"

        # Find config file
        agent_defs = base_path / "agent_definitions.json"
        team_config = base_path / "team_config.json"
        config_file = agent_defs if agent_defs.exists() else team_config

        if not config_file.exists():
            return {"success": False, "error": f"No config found for category: {category}"}

        # Load current config
        with open(config_file, 'r', encoding='utf-8') as f:
            data = json.load(f)

        # Find and update agent
        agents = data.get("agents", [])
        agent_index = next((i for i, a in enumerate(agents) if a.get("id") == agent_id), None)

        if agent_index is None:
            return {"success": False, "error": f"Agent not found: {agent_id}"}

        # Update agent config
        agents[agent_index] = updates
        data["agents"] = agents

        # Save back to file
        with open(config_file, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2)

        return {"success": True, "message": "Configuration updated successfully"}
    except Exception as e:
        return {"success": False, "error": str(e)}

def get_llm_providers() -> Dict[str, Any]:
    """Get available LLM providers (static list)"""
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

def execute_agent(category: str, agent_id: str, query: str, llm_config: Dict[str, Any], api_keys: Dict[str, str]) -> Dict[str, Any]:
    """Execute an agent with LLM"""
    try:
        # Import LLM executor
        sys.path.insert(0, str(Path(__file__).parent / "finagent_core"))
        from llm_executor import LLMExecutor

        # Get agent config
        agent_result = get_agent_config(category, agent_id)
        if not agent_result.get("success"):
            return agent_result

        agent_config = agent_result.get("config", {})

        # Override LLM config if provided
        if llm_config:
            if "llm_config" not in agent_config:
                agent_config["llm_config"] = {}
            agent_config["llm_config"]["provider"] = llm_config.get("provider")
            agent_config["llm_config"]["model_id"] = llm_config.get("model")

        # Execute with LLM
        executor = LLMExecutor(api_keys)
        result = executor.execute(agent_config, query)

        # Add agent metadata
        if result.get("success"):
            result["agent_id"] = agent_id
            result["agent_name"] = agent_config.get("name")
            result["config"] = agent_config

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}

def execute_team_collaboration(category: str, query: str, llm_config: Dict[str, Any], api_keys: Dict[str, str], agent_ids: Optional[List[str]] = None) -> Dict[str, Any]:
    """Execute multi-agent collaboration"""
    try:
        # Import LLM executor
        sys.path.insert(0, str(Path(__file__).parent / "finagent_core"))
        from llm_executor import LLMExecutor

        # Load agents
        configs = load_agent_configs(category)
        if not configs.get("success"):
            return configs

        all_agents = configs.get("agents", [])

        # Filter agents if specific IDs provided
        if agent_ids:
            agents_list = [a for a in all_agents if a.get("id") in agent_ids]
        else:
            agents_list = all_agents

        if not agents_list:
            return {"success": False, "error": "No agents found"}

        executor = LLMExecutor(api_keys)
        discussion = []

        # Round 1: Each agent analyzes query
        for agent_config in agents_list:
            # Override LLM config
            if llm_config:
                if "llm_config" not in agent_config:
                    agent_config["llm_config"] = {}
                agent_config["llm_config"]["provider"] = llm_config.get("provider")
                agent_config["llm_config"]["model_id"] = llm_config.get("model")

            result = executor.execute(agent_config, query)
            if result.get("success"):
                discussion.append({
                    "agent": agent_config.get("name"),
                    "role": agent_config.get("role"),
                    "response": result.get("response", ""),
                    "round": 1
                })

        # Round 2: Agents respond to discussion
        context = f"Original Query: {query}\n\nDiscussion so far:\n"
        for d in discussion:
            context += f"\n{d['agent']}: {d['response'][:150]}...\n"
        context += "\n\nBased on the above discussion, provide your additional insights:"

        for agent_config in agents_list:
            result = executor.execute(agent_config, context)
            if result.get("success"):
                discussion.append({
                    "agent": agent_config.get("name"),
                    "role": agent_config.get("role"),
                    "response": result.get("response", ""),
                    "round": 2
                })

        return {
            "success": True,
            "discussion": discussion,
            "total_agents": len(agents_list),
            "rounds": 2,
            "query": query
        }
    except Exception as e:
        return {"success": False, "error": str(e)}

def main(args=None):
    """CLI entry point following yfinance pattern"""
    # Support both PyO3 (args parameter) and subprocess (sys.argv)
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        return json.dumps({"success": False, "error": "Usage: python agent_manager.py <command> <args>"})

    command = args[0]

    try:
        if command == "list_agents":
            category = args[1] if len(args) > 1 else "TraderInvestorsAgent"
            result = load_agent_configs(category)

        elif command == "get_config":
            if len(args) < 3:
                result = {"success": False, "error": "Usage: agent_manager.py get_config <category> <agent_id>"}
            else:
                category = args[1]
                agent_id = args[2]
                result = get_agent_config(category, agent_id)

        elif command == "save_config":
            if len(args) < 4:
                result = {"success": False, "error": "Usage: agent_manager.py save_config <category> <agent_id> <json_config>"}
            else:
                category = args[1]
                agent_id = args[2]
                updates = json.loads(args[3])
                result = save_agent_config(category, agent_id, updates)

        elif command == "get_providers":
            result = get_llm_providers()

        elif command == "execute_agent":
            if len(args) < 6:
                result = {"success": False, "error": "Usage: agent_manager.py execute_agent <category> <agent_id> <query> <llm_config_json> <api_keys_json>"}
            else:
                category = args[1]
                agent_id = args[2]
                query = args[3]
                llm_config = json.loads(args[4])
                api_keys = json.loads(args[5])
                result = execute_agent(category, agent_id, query, llm_config, api_keys)

        elif command == "execute_team":
            if len(args) < 5:
                result = {"success": False, "error": "Usage: agent_manager.py execute_team <category> <query> <llm_config_json> <api_keys_json> [agent_ids_json]"}
            else:
                category = args[1]
                query = args[2]
                llm_config = json.loads(args[3])
                api_keys = json.loads(args[4])
                agent_ids = json.loads(args[5]) if len(args) > 5 else None
                result = execute_team_collaboration(category, query, llm_config, api_keys, agent_ids)

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        result_json = json.dumps(result)
        return result_json

    except json.JSONDecodeError as e:
        error_result = {"success": False, "error": f"Invalid JSON: {str(e)}"}
        result_json = json.dumps(error_result)
        return result_json
    except Exception as e:
        error_result = {"success": False, "error": str(e)}
        result_json = json.dumps(error_result)
        return result_json

if __name__ == "__main__":
    main()
