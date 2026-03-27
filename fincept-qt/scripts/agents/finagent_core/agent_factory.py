"""
Agent Factory - Create Agno agents from configurations

Single factory to create configured Agno agents for any terminal tab.
Fully leverages Agno framework capabilities.
"""

import os
from typing import Dict, Any, Optional, List
from agno.agent import Agent


class AgentFactory:
    """Factory for creating configured Agno agents"""

    def __init__(self, api_keys: Optional[Dict[str, str]] = None):
        """
        Initialize agent factory

        Args:
            api_keys: Dictionary of API keys {provider_name: key}
                     Falls back to environment variables
        """
        self.api_keys = api_keys or {}

    def create_agent(
        self,
        config: Dict[str, Any],
        session_id: Optional[str] = None,
        user_id: Optional[str] = None
    ) -> Agent:
        """
        Create Agno agent from configuration

        Args:
            config: Agent configuration dict
            session_id: Optional session ID for memory
            user_id: Optional user ID for personalization

        Returns:
            Configured Agno Agent instance

        Example:
            >>> factory = AgentFactory(api_keys={"OPENAI_API_KEY": "..."})
            >>> agent = factory.create_agent(config)
            >>> response = agent.run("What is AI?")
        """
        # Create model
        model = self._create_model(config["model"])

        # Get tools
        tools = self._get_tools(config.get("tools", []))

        # Build agent kwargs
        agent_kwargs = {
            "name": config.get("name", "Agent"),
            "model": model,
            "instructions": config.get("instructions", ""),
            "markdown": config.get("output_format") == "markdown",
        }

        # Add tools if any
        if tools:
            agent_kwargs["tools"] = tools

        # Add memory if enabled
        if config.get("memory", False):
            agent_kwargs["add_history_to_context"] = True

        # Add knowledge if configured
        if "knowledge" in config:
            knowledge = self._create_knowledge(config["knowledge"])
            if knowledge:
                agent_kwargs["knowledge"] = knowledge

        # Add session storage if memory enabled
        if config.get("memory", False) and session_id:
            storage = self._create_storage(config.get("storage", {}))
            if storage:
                agent_kwargs["storage"] = storage

        # Create agent
        agent = Agent(**agent_kwargs)

        return agent

    def _create_model(self, model_config: Dict[str, Any]):
        """
        Create Agno model from config.

        model_config fields:
          provider    — e.g. "openai", "anthropic", "gemini", "groq", "deepseek", "ollama"
          model_id    — e.g. "gpt-4o", "claude-3-5-sonnet-20241022"
          api_key     — resolved key passed directly from LlmService (takes priority)
          base_url    — custom endpoint; if set, forces OpenAI-compatible path for any provider
          temperature — float
          max_tokens  — int
        """
        provider    = model_config.get("provider", "openai").lower()
        model_id    = model_config.get("model_id", "")
        temperature = model_config.get("temperature", 0.7)
        max_tokens  = model_config.get("max_tokens", 4096)
        base_url    = model_config.get("base_url", "") or ""
        # api_key passed directly takes priority; fall back to env-var lookup
        api_key     = model_config.get("api_key") or self._get_api_key(provider) or ""

        # ── Custom base_url → OpenAI-compatible endpoint ──────────────────────
        # Any provider configured with a custom base_url (e.g. minimax served
        # on an Anthropic-compatible endpoint, OpenRouter, LM Studio, etc.)
        # is routed through OpenAIChat with that base_url.
        if base_url:
            from agno.models.openai import OpenAIChat
            # Override Agno's default role_map which maps "system"→"developer".
            # Custom endpoints (MiniMax, OpenRouter, etc.) expect standard roles.
            _STANDARD_ROLE_MAP = {
                "system": "system",
                "user": "user",
                "assistant": "assistant",
                "tool": "tool",
                "model": "assistant",
            }
            kwargs = dict(id=model_id, api_key=api_key,
                          temperature=temperature, max_tokens=max_tokens,
                          base_url=base_url, role_map=_STANDARD_ROLE_MAP)
            return OpenAIChat(**{k: v for k, v in kwargs.items() if v})

        # ── Native provider instances ─────────────────────────────────────────
        if provider == "openai":
            from agno.models.openai import OpenAIChat
            return OpenAIChat(id=model_id, api_key=api_key,
                              temperature=temperature, max_tokens=max_tokens)

        if provider == "anthropic":
            from agno.models.anthropic import Claude
            return Claude(id=model_id, api_key=api_key,
                          temperature=temperature, max_tokens=max_tokens)

        if provider in ("google", "gemini"):
            from agno.models.google import Gemini
            return Gemini(id=model_id, api_key=api_key,
                          temperature=temperature, max_tokens=max_tokens)

        if provider == "groq":
            from agno.models.groq import Groq
            return Groq(id=model_id, api_key=api_key,
                        temperature=temperature, max_tokens=max_tokens)

        if provider == "deepseek":
            from agno.models.deepseek import DeepSeek
            return DeepSeek(id=model_id, api_key=api_key,
                            temperature=temperature, max_tokens=max_tokens)

        if provider == "ollama":
            from agno.models.ollama import Ollama
            return Ollama(id=model_id, temperature=temperature, num_predict=max_tokens)

        if provider in ("openrouter",):
            from agno.models.openai import OpenAIChat
            return OpenAIChat(id=model_id, api_key=api_key,
                              base_url="https://openrouter.ai/api/v1",
                              temperature=temperature, max_tokens=max_tokens)

        # Fallback: OpenAI-compatible
        from agno.models.openai import OpenAIChat
        return OpenAIChat(id=model_id, api_key=api_key,
                          temperature=temperature, max_tokens=max_tokens)

    def _get_tools(self, tool_names: List[str]) -> List:
        """
        Get Agno tool instances from names

        Args:
            tool_names: List of tool names

        Returns:
            List of tool instances
        """
        tools = []

        for tool_name in tool_names:
            tool = self._load_tool(tool_name)
            if tool:
                tools.append(tool)

        return tools

    def _load_tool(self, tool_name: str):
        """Load specific Agno tool by name"""
        try:
            # Web search
            if tool_name == "duckduckgo":
                from agno.tools.duckduckgo import DuckDuckGo
                return DuckDuckGo()

            elif tool_name == "tavily":
                from agno.tools.tavily import TavilyTools
                return TavilyTools()

            # Finance
            elif tool_name == "yfinance":
                from agno.tools.yfinance import YFinanceTools
                return YFinanceTools()

            elif tool_name == "financial_datasets":
                from agno.tools.financial_datasets import FinancialDatasetsTools
                api_key = self._get_api_key("financial_datasets")
                return FinancialDatasetsTools(api_key=api_key)

            # File operations
            elif tool_name == "file":
                from agno.tools.file import FileTools
                return FileTools()

            # Python
            elif tool_name == "python":
                from agno.tools.python import PythonTools
                return PythonTools()

            # Calculator
            elif tool_name == "calculator":
                from agno.tools.calculator import Calculator
                return Calculator()

            # Shell
            elif tool_name == "shell":
                from agno.tools.shell import ShellTools
                return ShellTools()

            # Knowledge
            elif tool_name == "knowledge":
                from agno.tools.knowledge import KnowledgeTools
                return KnowledgeTools()

            # Add more tools as needed...

        except ImportError as e:
            print(f"Warning: Tool '{tool_name}' not available: {e}")
            return None

        return None

    def _create_knowledge(self, knowledge_config: Dict[str, Any]):
        """
        Create Agno knowledge base from config

        Args:
            knowledge_config: {path, vector_db?, embedder?}

        Returns:
            AssistantKnowledge instance or None
        """
        if not knowledge_config:
            return None

        try:
            from agno.knowledge.knowledge import AssistantKnowledge

            # Create vector DB if specified
            vector_db = None
            if "vector_db" in knowledge_config:
                vector_db = self._create_vector_db(knowledge_config["vector_db"])

            # Create embedder if specified
            embedder = None
            if "embedder" in knowledge_config:
                embedder = self._create_embedder(knowledge_config["embedder"])

            knowledge = AssistantKnowledge(
                path=knowledge_config.get("path"),
                vector_db=vector_db,
                embedder=embedder
            )

            return knowledge

        except ImportError:
            print("Warning: Knowledge base dependencies not installed")
            return None

    def _create_vector_db(self, vector_db_config: Dict[str, Any]):
        """Create vector database from config"""
        db_type = vector_db_config.get("type", "pgvector")

        if db_type == "pgvector":
            from agno.vectordb.pgvector import PgVector
            return PgVector(
                table_name=vector_db_config.get("table", "agent_knowledge"),
                db_url=vector_db_config.get("url", os.getenv("DATABASE_URL"))
            )
        # Add more vector DBs as needed

        return None

    def _create_embedder(self, embedder_config: Dict[str, Any]):
        """Create embedder from config"""
        embedder_type = embedder_config.get("type", "openai")

        if embedder_type == "openai":
            from agno.knowledge.embedder.openai import OpenAIEmbedder
            return OpenAIEmbedder(
                api_key=self._get_api_key("openai")
            )
        # Add more embedders as needed

        return None

    def _create_storage(self, storage_config: Dict[str, Any]):
        """Create storage backend for agent memory"""
        storage_type = storage_config.get("type", "sqlite")

        if storage_type == "sqlite":
            from agno.db.sqlite import SqliteDb
            return SqliteDb(
                db_file=storage_config.get("db_file", "agent_sessions.db")
            )

        elif storage_type == "postgres":
            from agno.db.postgres import PostgresDb
            return PostgresDb(
                db_url=storage_config.get("db_url", os.getenv("DATABASE_URL"))
            )

        return None

    def _get_api_key(self, provider: str) -> Optional[str]:
        """
        Get API key for provider

        Priority:
        1. api_keys dict passed to factory
        2. Environment variables
        """
        key_map = {
            "openai": "OPENAI_API_KEY",
            "anthropic": "ANTHROPIC_API_KEY",
            "google": "GOOGLE_API_KEY",
            "gemini": "GOOGLE_API_KEY",
            "groq": "GROQ_API_KEY",
            "deepseek": "DEEPSEEK_API_KEY",
            "financial_datasets": "FINANCIAL_DATASETS_API_KEY",
            "tavily": "TAVILY_API_KEY"
        }

        key_name = key_map.get(provider)
        if not key_name:
            return None

        # Try api_keys dict first
        if key_name in self.api_keys:
            return self.api_keys[key_name]

        # Fall back to environment
        return os.getenv(key_name)
