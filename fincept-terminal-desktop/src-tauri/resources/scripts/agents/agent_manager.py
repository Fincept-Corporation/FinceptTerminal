"""
Agent Manager - Pure Agno Framework Implementation
Handles agent configuration, execution, and management using Agno v2.3.23+
Returns JSON output for Rust integration
"""

import sys
import json
import os
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
    """Get available LLM providers from Agno"""
    return {
        "success": True,
        "providers": {
            "openai": {
                "name": "OpenAI",
                "models": ["gpt-4o", "gpt-4-turbo", "gpt-4", "gpt-3.5-turbo"],
                "api_key_required": True
            },
            "anthropic": {
                "name": "Anthropic",
                "models": ["claude-sonnet-4-5", "claude-3-5-sonnet-20241022", "claude-3-opus-20240229", "claude-3-sonnet-20240229", "claude-3-haiku-20240307"],
                "api_key_required": True
            },
            "google": {
                "name": "Google",
                "models": ["gemini-2.0-flash-exp", "gemini-1.5-pro", "gemini-1.5-flash"],
                "api_key_required": True
            },
            "ollama": {
                "name": "Ollama",
                "models": ["llama3.3", "llama3.1", "mistral", "mixtral"],
                "api_key_required": False
            },
            "groq": {
                "name": "Groq",
                "models": ["llama-3.3-70b-versatile", "mixtral-8x7b-32768"],
                "api_key_required": True
            }
        }
    }

def _create_agno_agent(agent_config: Dict[str, Any], llm_config: Dict[str, Any], api_keys: Dict[str, str]):
    """
    Create Agno Agent from configuration

    Args:
        agent_config: Agent configuration dict
        llm_config: LLM override config
        api_keys: API keys dict

    Returns:
        Agno Agent instance
    """
    from agno.agent import Agent

    # Get LLM config
    config = agent_config.get("llm_config", {})
    if llm_config:
        config["provider"] = llm_config.get("provider", config.get("provider", "openai"))
        config["model_id"] = llm_config.get("model", config.get("model_id", "gpt-4-turbo"))

    provider = config.get("provider", "openai").lower()
    model_id = config.get("model_id", "gpt-4-turbo")
    temperature = config.get("temperature", 0.7)

    # Get API key
    api_key = _get_api_key_for_provider(provider, api_keys)

    # Create model based on provider
    model = _create_model(provider, model_id, api_key, temperature)

    # Build instructions
    instructions = _build_instructions(agent_config)

    # Create Agno Agent
    agent = Agent(
        name=agent_config.get("name", "Agent"),
        model=model,
        description=agent_config.get("description", ""),
        instructions=instructions,
        markdown=True
    )

    return agent

def _create_model(provider: str, model_id: str, api_key: Optional[str], temperature: float):
    """Create Agno model instance"""
    if provider == "openai":
        from agno.models.openai import OpenAIChat
        return OpenAIChat(id=model_id, api_key=api_key, temperature=temperature)

    elif provider == "anthropic":
        from agno.models.anthropic import Claude
        return Claude(id=model_id, api_key=api_key, temperature=temperature)

    elif provider == "google" or provider == "gemini":
        from agno.models.google import Gemini
        return Gemini(id=model_id, api_key=api_key, temperature=temperature)

    elif provider == "ollama":
        from agno.models.ollama import Ollama
        return Ollama(id=model_id, temperature=temperature)

    elif provider == "groq":
        from agno.models.groq import Groq
        return Groq(id=model_id, api_key=api_key, temperature=temperature)

    else:
        # Default to OpenAI
        from agno.models.openai import OpenAIChat
        return OpenAIChat(id=model_id, api_key=api_key, temperature=temperature)

def _get_api_key_for_provider(provider: str, api_keys: Dict[str, str]) -> Optional[str]:
    """Get API key for specific provider"""
    key_map = {
        "openai": "OPENAI_API_KEY",
        "anthropic": "ANTHROPIC_API_KEY",
        "google": "GOOGLE_API_KEY",
        "gemini": "GOOGLE_API_KEY",
        "groq": "GROQ_API_KEY",
        "ollama": None  # Ollama doesn't need API key
    }
    key_name = key_map.get(provider)
    if not key_name:
        return None

    # Try api_keys dict first, then environment
    return api_keys.get(key_name) or os.getenv(key_name)

def _build_instructions(agent_config: Dict[str, Any]) -> str:
    """Build agent instructions from config"""
    parts = []

    # Role and goal
    role = agent_config.get("role", "")
    goal = agent_config.get("goal", "")
    if role:
        parts.append(f"**Role**: {role}")
    if goal:
        parts.append(f"**Goal**: {goal}")

    # Custom instructions
    instructions = agent_config.get("instructions", "")
    if instructions:
        parts.append(f"\n{instructions}")

    return "\n".join(parts)

def execute_agent(category: str, agent_id: str, query: str, llm_config: Dict[str, Any], api_keys: Dict[str, str]) -> Dict[str, Any]:
    """Execute an agent using Agno framework"""
    try:
        # Get agent config
        agent_result = get_agent_config(category, agent_id)
        if not agent_result.get("success"):
            return agent_result

        agent_config = agent_result.get("config", {})

        # Create Agno Agent
        agent = _create_agno_agent(agent_config, llm_config, api_keys)

        # Execute agent
        response = agent.run(query)

        # Extract response content
        content = response.content if hasattr(response, 'content') else str(response)

        return {
            "success": True,
            "response": content,
            "agent_id": agent_id,
            "agent_name": agent_config.get("name"),
            "provider": llm_config.get("provider") if llm_config else agent_config.get("llm_config", {}).get("provider", "openai"),
            "model": llm_config.get("model") if llm_config else agent_config.get("llm_config", {}).get("model_id", "unknown"),
            "finish_reason": "complete"
        }
    except ImportError as e:
        return {
            "success": False,
            "error": f"Agno not installed or missing dependency: {str(e)}. Install: pip install agno"
        }
    except Exception as e:
        return {"success": False, "error": str(e)}

def execute_team_collaboration(category: str, query: str, llm_config: Dict[str, Any], api_keys: Dict[str, str], agent_ids: Optional[List[str]] = None) -> Dict[str, Any]:
    """Execute multi-agent collaboration using Agno"""
    try:
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

        # Create Agno agents
        agno_agents = []
        for agent_config in agents_list:
            try:
                agent = _create_agno_agent(agent_config, llm_config, api_keys)
                agno_agents.append({
                    "agent": agent,
                    "config": agent_config
                })
            except Exception as e:
                # Skip agents that fail to initialize
                continue

        if not agno_agents:
            return {"success": False, "error": "Failed to initialize any agents"}

        discussion = []

        # Round 1: Each agent analyzes query
        for agent_data in agno_agents:
            try:
                response = agent_data["agent"].run(query)
                content = response.content if hasattr(response, 'content') else str(response)

                discussion.append({
                    "agent": agent_data["config"].get("name"),
                    "role": agent_data["config"].get("role"),
                    "response": content,
                    "round": 1
                })
            except Exception as e:
                # Log error but continue
                discussion.append({
                    "agent": agent_data["config"].get("name"),
                    "role": agent_data["config"].get("role"),
                    "response": f"Error: {str(e)}",
                    "round": 1
                })

        # Round 2: Agents respond to discussion
        context = f"Original Query: {query}\n\nDiscussion so far:\n"
        for d in discussion:
            context += f"\n{d['agent']}: {d['response'][:150]}...\n"
        context += "\n\nBased on the above discussion, provide your additional insights:"

        for agent_data in agno_agents:
            try:
                response = agent_data["agent"].run(context)
                content = response.content if hasattr(response, 'content') else str(response)

                discussion.append({
                    "agent": agent_data["config"].get("name"),
                    "role": agent_data["config"].get("role"),
                    "response": content,
                    "round": 2
                })
            except Exception as e:
                discussion.append({
                    "agent": agent_data["config"].get("name"),
                    "role": agent_data["config"].get("role"),
                    "response": f"Error: {str(e)}",
                    "round": 2
                })

        return {
            "success": True,
            "discussion": discussion,
            "total_agents": len(agno_agents),
            "rounds": 2,
            "query": query
        }
    except ImportError as e:
        return {
            "success": False,
            "error": f"Agno not installed: {str(e)}. Install: pip install agno"
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
    result = main()
    print(result)
