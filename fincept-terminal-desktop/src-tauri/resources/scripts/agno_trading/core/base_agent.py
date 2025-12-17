"""
Base Agent Class

This module implements the base trading agent class using the Agno framework.
All specialized trading agents inherit from this base class.
"""

import os
import sys
import json
from typing import List, Optional, Dict, Any, Union
from datetime import datetime
from dataclasses import dataclass, field

try:
    from agno.agent import Agent
    from agno.models.response import ModelResponse
    from agno.tools.toolkit import Toolkit
    from agno.knowledge import Knowledge
    from agno.memory import MemoryManager
    AGNO_AVAILABLE = True
except ImportError:
    AGNO_AVAILABLE = False
    # Fallback types for when Agno is not installed
    Agent = None
    ModelResponse = None
    Toolkit = None
    Knowledge = None
    MemoryManager = None


@dataclass
class AgentConfig:
    """Configuration for a trading agent"""
    name: str
    role: str
    description: str
    instructions: List[str] = field(default_factory=list)

    # LLM Configuration
    model_provider: str = "openai"
    model_name: str = "gpt-4o-mini"
    temperature: float = 0.7
    max_tokens: Optional[int] = 4096

    # Agent Capabilities
    tools: List[str] = field(default_factory=list)
    enable_knowledge: bool = False
    enable_memory: bool = False

    # Trading Configuration
    symbols: List[str] = field(default_factory=lambda: ["BTC/USD"])
    risk_tolerance: str = "moderate"  # conservative, moderate, aggressive

    # Behavior
    show_tool_calls: bool = True
    markdown: bool = True
    stream: bool = False

    # Advanced
    reasoning_effort: Optional[str] = None  # For o1/o3/o4 models
    structured_outputs: bool = False
    response_format: Optional[Dict[str, Any]] = None


class BaseAgent:
    """
    Base Trading Agent using Agno framework

    This class provides common functionality for all trading agents:
    - Agent initialization with LLM configuration
    - Tool registration and execution
    - Knowledge base integration
    - Memory management
    - Session handling
    - Response streaming
    """

    def __init__(self, config: AgentConfig):
        """
        Initialize the base trading agent

        Args:
            config: Agent configuration object
        """
        if not AGNO_AVAILABLE:
            raise ImportError(
                "Agno framework is not installed. "
                "Install it with: pip install agno>=0.3.0"
            )

        self.config = config
        self.agent_id = f"{config.name}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        self.created_at = datetime.now()

        # Initialize LLM model
        self.model = self._initialize_model()

        # Initialize tools
        self.tools = self._initialize_tools()

        # Initialize knowledge base
        self.knowledge = self._initialize_knowledge() if config.enable_knowledge else None

        # Initialize memory
        self.memory = self._initialize_memory() if config.enable_memory else None

        # Create Agno agent
        self.agent = self._create_agent()

        # Session tracking
        self.sessions: Dict[str, List[Dict[str, Any]]] = {}

    def _initialize_model(self) -> Any:
        """Initialize the LLM model based on configuration"""
        provider = self.config.model_provider.lower()
        model_name = self.config.model_name

        # Get API key from environment
        api_key_map = {
            "openai": os.getenv("OPENAI_API_KEY"),
            "anthropic": os.getenv("ANTHROPIC_API_KEY"),
            "google": os.getenv("GOOGLE_API_KEY"),
            "groq": os.getenv("GROQ_API_KEY"),
            "deepseek": os.getenv("DEEPSEEK_API_KEY"),
            "xai": os.getenv("XAI_API_KEY"),
            "cohere": os.getenv("COHERE_API_KEY"),
            "mistral": os.getenv("MISTRAL_API_KEY"),
            "together": os.getenv("TOGETHER_API_KEY"),
            "fireworks": os.getenv("FIREWORKS_API_KEY"),
        }

        api_key = api_key_map.get(provider)

        # Model configuration
        model_config = {
            "id": model_name,
            "api_key": api_key,
        }

        # Add optional parameters
        if self.config.reasoning_effort:
            model_config["reasoning_effort"] = self.config.reasoning_effort

        # Provider-specific model initialization
        if provider == "openai":
            from agno.models.openai import OpenAIChat
            return OpenAIChat(**model_config)
        elif provider == "anthropic":
            from agno.models.anthropic import Claude
            return Claude(**model_config)
        elif provider == "google":
            # Use OpenAI-compatible API for Google Gemini
            from agno.models.openai import OpenAIChat
            return OpenAIChat(
                id=model_name,
                api_key=api_key,
                base_url="https://generativelanguage.googleapis.com/v1beta/openai/"
            )
        elif provider == "groq":
            from agno.models.groq import Groq
            return Groq(**model_config)
        elif provider == "deepseek":
            from agno.models.openai import OpenAIChat
            return OpenAIChat(
                id=model_name,
                api_key=api_key,
                base_url="https://api.deepseek.com"
            )
        elif provider == "xai":
            from agno.models.openai import OpenAIChat
            return OpenAIChat(
                id=model_name,
                api_key=api_key,
                base_url="https://api.x.ai/v1"
            )
        elif provider == "ollama":
            from agno.models.ollama import Ollama
            return Ollama(id=model_name)
        else:
            # Default to OpenAI-compatible
            from agno.models.openai import OpenAIChat
            return OpenAIChat(**model_config)

    def _initialize_tools(self) -> List[Any]:
        """Initialize agent tools based on configuration"""
        tools = []

        # Tool registry mapping
        tool_registry = {
            "market_data": self._get_market_data_tools,
            "technical_analysis": self._get_technical_analysis_tools,
            "order_execution": self._get_order_execution_tools,
            "portfolio": self._get_portfolio_tools,
            "news_sentiment": self._get_news_sentiment_tools,
        }

        # Load configured tools
        for tool_name in self.config.tools:
            if tool_name in tool_registry:
                tool_fn = tool_registry[tool_name]
                tools.extend(tool_fn())

        return tools

    def _get_market_data_tools(self) -> List[Any]:
        """Get market data tools"""
        try:
            from tools.market_data import get_market_data_tools
            return get_market_data_tools()
        except ImportError:
            return []

    def _get_technical_analysis_tools(self) -> List[Any]:
        """Get technical analysis tools"""
        try:
            from tools.technical_indicators import get_technical_analysis_tools
            return get_technical_analysis_tools()
        except ImportError:
            return []

    def _get_order_execution_tools(self) -> List[Any]:
        """Get order execution tools"""
        # Placeholder - will be implemented in tools/order_execution.py
        return []

    def _get_portfolio_tools(self) -> List[Any]:
        """Get portfolio management tools"""
        # Placeholder - will be implemented in tools/portfolio_tools.py
        return []

    def _get_news_sentiment_tools(self) -> List[Any]:
        """Get news and sentiment tools"""
        # Placeholder - will be implemented in tools/news_sentiment.py
        return []

    def _initialize_knowledge(self) -> Optional[Knowledge]:
        """Initialize knowledge base for the agent"""
        if not self.config.enable_knowledge:
            return None

        # Will be implemented with proper vector DB configuration
        # from agno.knowledge.lancedb import LanceDBKnowledge
        # return LanceDBKnowledge(...)
        return None

    def _initialize_memory(self) -> Optional[MemoryManager]:
        """Initialize memory for the agent"""
        if not self.config.enable_memory:
            return None

        # Will be implemented with proper database configuration
        # from agno.memory.db.sqlite import SqliteMemory
        # return SqliteMemory(...)
        return None

    def _create_agent(self) -> Agent:
        """Create the Agno agent instance"""
        agent_params = {
            "name": self.config.name,
            "role": self.config.role,
            "model": self.model,
            "description": self.config.description,
            "instructions": self.config.instructions,
            "tools": self.tools if self.tools else None,
            "knowledge": self.knowledge,
            "markdown": self.config.markdown,
        }

        # Add optional parameters
        if self.config.structured_outputs:
            agent_params["structured_outputs"] = True

        if self.config.response_format:
            agent_params["response_format"] = self.config.response_format

        return Agent(**agent_params)

    def run(
        self,
        prompt: str,
        session_id: Optional[str] = None,
        stream: bool = False,
        **kwargs
    ) -> Union[ModelResponse, Any]:
        """
        Run the agent with a prompt

        Args:
            prompt: User prompt/query
            session_id: Optional session ID for conversation continuity
            stream: Whether to stream the response
            **kwargs: Additional parameters to pass to agent.run()

        Returns:
            ModelResponse object or streaming response
        """
        # Log what's being sent to the model
        print(f"\n{'='*80}", file=sys.stderr)
        print(f"[Agent: {self.config.name}] Running with model: {self.config.model_provider}:{self.config.model_name}", file=sys.stderr)
        print(f"Prompt length: {len(prompt)} chars", file=sys.stderr)
        print(f"Prompt preview: {prompt[:500]}...", file=sys.stderr)
        print(f"{'='*80}\n", file=sys.stderr)

        # Track in session
        if session_id:
            if session_id not in self.sessions:
                self.sessions[session_id] = []
            self.sessions[session_id].append({
                "timestamp": datetime.now().isoformat(),
                "prompt": prompt,
                "type": "user"
            })

        # Run agent (Agno uses 'input' parameter)
        response = self.agent.run(prompt, stream=stream or self.config.stream, **kwargs)

        # Track response in session
        if session_id and not stream:
            self.sessions[session_id].append({
                "timestamp": datetime.now().isoformat(),
                "response": self._extract_response_content(response),
                "type": "assistant"
            })

        return response

    def _extract_response_content(self, response: ModelResponse) -> str:
        """Extract content from ModelResponse"""
        if hasattr(response, 'content'):
            return response.content
        elif hasattr(response, 'message'):
            return response.message
        else:
            return str(response)

    def get_session_history(self, session_id: str) -> List[Dict[str, Any]]:
        """Get conversation history for a session"""
        return self.sessions.get(session_id, [])

    def clear_session(self, session_id: str) -> None:
        """Clear a specific session"""
        if session_id in self.sessions:
            del self.sessions[session_id]

    def to_dict(self) -> Dict[str, Any]:
        """Convert agent to dictionary representation"""
        return {
            "agent_id": self.agent_id,
            "name": self.config.name,
            "role": self.config.role,
            "description": self.config.description,
            "model_provider": self.config.model_provider,
            "model_name": self.config.model_name,
            "tools": self.config.tools,
            "symbols": self.config.symbols,
            "created_at": self.created_at.isoformat(),
            "active_sessions": len(self.sessions),
        }

    def __repr__(self) -> str:
        return f"<BaseAgent(id={self.agent_id}, name={self.config.name}, role={self.config.role})>"
