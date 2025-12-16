"""
Tauri Bridge - Interface between Rust Tauri commands and Agno agents

This module provides the bridge layer that allows Rust Tauri commands
to execute Agno agents seamlessly, replacing the old static agent system.

Usage from Rust:
    python finagents/tauri_bridge.py --action execute_agent --parameters '{"agent_id": "buffett", "query": "Analyze AAPL"}' --inputs '{"api_keys": {...}}'
"""

import json
import sys
import argparse
from typing import Dict, Any, Optional
from pathlib import Path

from finagent_core.base_agent import AgentRegistry, ConfigurableAgent
from finagent_core.config.llm_providers import LLMProviderManager
from finagent_core.tools.tool_registry import get_tool_registry
from finagent_core.database.db_manager import DatabaseManager
from finagent_core.utils.path_resolver import PathResolver
from finagent_core.utils.logger import get_logger, setup_logger

logger = get_logger(__name__)


class TauriBridge:
    """
    Bridge between Tauri Rust commands and Agno agents

    Handles:
    - Agent execution from Rust
    - Agent configuration management
    - API key injection
    - Error handling and logging
    """

    def __init__(self, base_path: Optional[str] = None):
        """
        Initialize Tauri bridge

        Args:
            base_path: Base path from Rust (via get_script_path)
        """
        # Set up path resolver
        self.path_resolver = PathResolver(base_path=base_path)

        # Initialize managers
        self.llm_provider_manager = LLMProviderManager()
        self.tool_registry = get_tool_registry()
        self.db_manager = DatabaseManager(base_path=self.path_resolver.base_path)

        # Initialize agent registries (lazy loading)
        self._agent_registries: Dict[str, AgentRegistry] = {}

        logger.info(f"TauriBridge initialized with base_path: {self.path_resolver.base_path}")

    def get_agent_registry(self, category: str) -> AgentRegistry:
        """
        Get agent registry for a category

        Args:
            category: Agent category (GeopoliticsAgents, EconomicAgents, etc.)

        Returns:
            AgentRegistry for the category
        """
        if category in self._agent_registries:
            return self._agent_registries[category]

        # Create new registry
        registry = AgentRegistry(
            base_path=self.path_resolver.base_path,
            llm_provider_manager=self.llm_provider_manager,
            tool_registry=self.tool_registry,
            db_manager=self.db_manager
        )

        # Load configurations from JSON
        config_path = self.path_resolver.get_agent_config_path(category)
        if config_path.exists():
            registry.load_configs_from_json(config_path)
            logger.info(f"Loaded {len(registry._configs)} agents for {category}")
        else:
            logger.warning(f"No configuration file found at: {config_path}")

        # Cache registry
        self._agent_registries[category] = registry

        return registry

    def execute_agent(self, parameters: Dict[str, Any], inputs: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute a single agent (called from Rust)

        Args:
            parameters: Execution parameters
                - agent_id: Agent identifier
                - query: User query
                - user_id: Optional user ID
                - session_id: Optional session ID
                - category: Agent category (default: "TraderInvestorsAgent")
            inputs: Runtime inputs
                - api_keys: Dictionary of API keys

        Returns:
            Execution result dictionary
        """
        try:
            agent_id = parameters.get("agent_id")
            if not agent_id:
                return {"success": False, "error": "agent_id is required"}

            query = parameters.get("query")
            if not query:
                return {"success": False, "error": "query is required"}

            user_id = parameters.get("user_id")
            session_id = parameters.get("session_id")
            category = parameters.get("category", "TraderInvestorsAgent")
            api_keys = inputs.get("api_keys", {})

            logger.info(f"Executing agent: {agent_id} in category: {category}")

            # Get agent registry for category
            registry = self.get_agent_registry(category)

            # Get agent
            agent = registry.get_agent(agent_id, api_keys=api_keys)

            # Run agent
            response = agent.run(
                query=query,
                user_id=user_id,
                session_id=session_id
            )

            # Extract response content
            content = response.content if hasattr(response, 'content') else str(response)

            return {
                "success": True,
                "agent_id": agent_id,
                "category": category,
                "response": content,
                "run_id": response.run_id if hasattr(response, 'run_id') else None,
                "session_id": response.session_id if hasattr(response, 'session_id') else session_id,
                "metrics": response.metrics if hasattr(response, 'metrics') else {}
            }

        except Exception as e:
            logger.error(f"Error executing agent: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e),
                "agent_id": parameters.get("agent_id", "unknown"),
                "category": parameters.get("category", "unknown")
            }

    def list_agents(self, parameters: Dict[str, Any] = None) -> Dict[str, Any]:
        """
        List all available agents

        Args:
            parameters: Optional parameters
                - category: Filter by category

        Returns:
            List of available agents
        """
        try:
            category = (parameters or {}).get("category")

            if category:
                # List agents for specific category
                registry = self.get_agent_registry(category)
                agents = registry.list_agents()
                return {
                    "success": True,
                    "category": category,
                    "agents": agents,
                    "count": len(agents)
                }
            else:
                # List all agents from all categories
                all_categories = [
                    "GeopoliticsAgents",
                    "EconomicAgents",
                    "hedgeFundAgents",
                    "TraderInvestorsAgent"
                ]

                result = {}
                total_count = 0

                for cat in all_categories:
                    try:
                        registry = self.get_agent_registry(cat)
                        agents = registry.list_agents()
                        result[cat] = agents
                        total_count += len(agents)
                    except Exception as e:
                        logger.warning(f"Error loading category {cat}: {e}")
                        result[cat] = []

                return {
                    "success": True,
                    "categories": result,
                    "total_count": total_count
                }

        except Exception as e:
            logger.error(f"Error listing agents: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e)
            }

    def get_agent_config(self, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """
        Get agent configuration for UI

        Args:
            parameters: Parameters
                - agent_id: Agent identifier
                - category: Agent category

        Returns:
            Agent configuration
        """
        try:
            agent_id = parameters.get("agent_id")
            category = parameters.get("category", "TraderInvestorsAgent")

            if not agent_id:
                return {"success": False, "error": "agent_id is required"}

            registry = self.get_agent_registry(category)
            config = registry.get_config(agent_id)

            return {
                "success": True,
                "agent_id": agent_id,
                "category": category,
                "config": config
            }

        except Exception as e:
            logger.error(f"Error getting agent config: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e),
                "agent_id": parameters.get("agent_id", "unknown")
            }

    def update_agent_config(self, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """
        Update agent configuration

        Args:
            parameters: Parameters
                - agent_id: Agent identifier
                - category: Agent category
                - updates: Configuration updates

        Returns:
            Update result
        """
        try:
            agent_id = parameters.get("agent_id")
            category = parameters.get("category", "TraderInvestorsAgent")
            updates = parameters.get("updates", {})

            if not agent_id:
                return {"success": False, "error": "agent_id is required"}

            registry = self.get_agent_registry(category)
            registry.update_config(agent_id, updates)

            return {
                "success": True,
                "message": f"Agent {agent_id} configuration updated",
                "agent_id": agent_id,
                "category": category
            }

        except Exception as e:
            logger.error(f"Error updating agent config: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e),
                "agent_id": parameters.get("agent_id", "unknown")
            }

    def get_provider_info(self) -> Dict[str, Any]:
        """
        Get information about LLM providers

        Returns:
            Provider information
        """
        try:
            info = self.llm_provider_manager.get_provider_info()
            return {
                "success": True,
                **info
            }
        except Exception as e:
            logger.error(f"Error getting provider info: {e}", exc_info=True)
            return {
                "success": False,
                "error": str(e)
            }


def main():
    """
    CLI entry point for Tauri Rust commands

    This function is called from Rust via subprocess execution.

    Example Rust call:
        python finagents/tauri_bridge.py \
            --action execute_agent \
            --parameters '{"agent_id": "warren_buffett_agent", "query": "Analyze AAPL"}' \
            --inputs '{"api_keys": {"OPENAI_API_KEY": "sk-..."}}' \
            --base-path /path/to/agents
    """
    parser = argparse.ArgumentParser(description='Agno Agent Execution Bridge for Tauri')

    parser.add_argument(
        '--action',
        type=str,
        required=True,
        choices=['execute_agent', 'list_agents', 'get_config', 'update_config', 'get_providers'],
        help='Action to perform'
    )

    parser.add_argument(
        '--parameters',
        type=str,
        default='{}',
        help='JSON parameters for the action'
    )

    parser.add_argument(
        '--inputs',
        type=str,
        default='{}',
        help='JSON runtime inputs (API keys, etc.)'
    )

    parser.add_argument(
        '--base-path',
        type=str,
        default=None,
        help='Base path for agents (provided by Rust get_script_path)'
    )

    parser.add_argument(
        '--log-level',
        type=str,
        default='INFO',
        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'],
        help='Logging level'
    )

    args = parser.parse_args()

    # Set up logging
    import logging
    log_level = getattr(logging, args.log_level)
    setup_logger(__name__, level=log_level, console=True)

    try:
        # Parse parameters and inputs
        parameters = json.loads(args.parameters)
        inputs = json.loads(args.inputs)

        # Create bridge
        bridge = TauriBridge(base_path=args.base_path)

        # Execute action
        if args.action == 'execute_agent':
            result = bridge.execute_agent(parameters, inputs)
        elif args.action == 'list_agents':
            result = bridge.list_agents(parameters)
        elif args.action == 'get_config':
            result = bridge.get_agent_config(parameters)
        elif args.action == 'update_config':
            result = bridge.update_agent_config(parameters)
        elif args.action == 'get_providers':
            result = bridge.get_provider_info()
        else:
            result = {"success": False, "error": f"Unknown action: {args.action}"}

        # Output result as JSON
        print(json.dumps(result, indent=2))
        sys.exit(0)

    except json.JSONDecodeError as e:
        error_result = {
            "success": False,
            "error": f"Invalid JSON: {e}",
            "action": args.action
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        error_result = {
            "success": False,
            "error": str(e),
            "action": args.action
        }
        logger.error(f"Fatal error: {e}", exc_info=True)
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
