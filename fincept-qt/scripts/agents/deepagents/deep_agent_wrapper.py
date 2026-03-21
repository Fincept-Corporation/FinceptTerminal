"""
DeepAgent Wrapper - Complete wrapper around DeepAgents library
Provides simplified interface for Fincept Terminal integration
"""

import sys
import os
from typing import Any, Dict, List, Optional, Callable
from datetime import datetime
import json
import logging

# Add current directory to path for imports
if __name__ == "__main__" or "." not in __name__:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

try:
    from deepagents import create_deep_agent, SubAgent, CompiledSubAgent
    from deepagents.graph import (
        TodoListMiddleware,
        SummarizationMiddleware,
        FilesystemMiddleware,
        SubAgentMiddleware
    )
    from langgraph.checkpoint.memory import MemorySaver
    DEEPAGENTS_AVAILABLE = True
except ImportError as e:
    DEEPAGENTS_AVAILABLE = False
    IMPORT_ERROR = str(e)

from fincept_llm_adapter import FinceptLLMAdapter

logger = logging.getLogger(__name__)


class DeepAgentWrapper:
    """
    High-level wrapper for DeepAgents with Fincept Terminal integration.

    Features:
    - Hierarchical task delegation
    - Built-in todo management
    - File operations (mock filesystem)
    - Subagent spawning
    - State checkpointing
    - Long-term memory
    - Summarization for long contexts
    """

    def __init__(
        self,
        model: Optional[Any] = None,
        api_key: Optional[str] = None,
        system_prompt: Optional[str] = None,
        enable_checkpointing: bool = True,
        enable_longterm_memory: bool = False,
        enable_summarization: bool = True,
        recursion_limit: int = 100
    ):
        """
        Initialize DeepAgent wrapper.

        Args:
            model: LLM model (FinceptLLMAdapter or model name string)
            api_key: API key for external models
            system_prompt: Base system instructions
            enable_checkpointing: Enable state persistence
            enable_longterm_memory: Enable memory across sessions
            enable_summarization: Auto-summarize long contexts
            recursion_limit: Max iterations before stopping
        """

        if not DEEPAGENTS_AVAILABLE:
            raise ImportError(f"DeepAgents not available: {IMPORT_ERROR}")

        self.model = model
        self.api_key = api_key
        self.system_prompt = system_prompt or self._default_system_prompt()
        self.enable_checkpointing = enable_checkpointing
        self.enable_longterm_memory = enable_longterm_memory
        self.enable_summarization = enable_summarization
        self.recursion_limit = recursion_limit

        self.agent = None
        self.subagents: List[SubAgent] = []
        self.custom_tools: List[Any] = []
        self.middleware: List[Any] = []

    def _default_system_prompt(self) -> str:
        """Default system prompt for Fincept agents"""
        return """You are an advanced financial intelligence agent for Fincept Terminal.

Your capabilities:
- Analyze financial markets, securities, and economic data
- Perform quantitative research and factor discovery
- Execute multi-step workflows autonomously
- Delegate tasks to specialized subagents
- Maintain todo lists for complex tasks
- Generate reports and recommendations

Always:
- Break complex tasks into subtasks using todo lists
- Delegate specialized work to subagents when appropriate
- Provide data-driven insights with proper citations
- Consider risk factors in financial recommendations
- Maintain professional, CFA-level analysis standards

Available tools: write_todos, read_file, write_file, edit_file, ls, call_subagent
"""

    def add_subagent(
        self,
        name: str,
        description: str,
        prompt: str,
        tools: Optional[List[Any]] = None,
        model: Optional[Any] = None
    ) -> None:
        """
        Add a specialized subagent.

        Args:
            name: Subagent identifier
            description: What this subagent does (used for routing)
            prompt: System prompt for subagent
            tools: Optional custom tools for subagent
            model: Optional different model for subagent
        """
        subagent = {
            "name": name,
            "description": description,
            "prompt": prompt,
            "system_prompt": prompt  # DeepAgents library expects this key
        }

        if tools:
            subagent["tools"] = tools
        if model:
            subagent["model"] = model

        self.subagents.append(subagent)
        logger.info(f"Added subagent: {name}")

    def add_tool(self, tool: Any) -> None:
        """Add custom tool to agent"""
        self.custom_tools.append(tool)
        logger.info(f"Added custom tool")

    def add_middleware(self, middleware: Any) -> None:
        """Add custom middleware"""
        self.middleware.append(middleware)
        logger.info(f"Added middleware")

    def build(self) -> Any:
        """
        Build and compile the DeepAgent graph.

        Returns:
            Compiled agent ready for execution
        """

        # Setup checkpointing
        checkpointer = MemorySaver() if self.enable_checkpointing else None

        # Build middleware stack
        # NOTE: SummarizationMiddleware causes duplicate middleware error
        # DeepAgents may add it automatically, so we don't add it manually
        middleware = list(self.middleware)

        # Create agent
        # Note: memory parameter is a list of memory types in newer deepagents versions
        memory_config = ["longterm"] if self.enable_longterm_memory else None

        self.agent = create_deep_agent(
            model=self.model,
            tools=self.custom_tools if self.custom_tools else None,
            system_prompt=self.system_prompt,
            middleware=tuple(middleware) if middleware else (),  # Empty tuple if no middleware
            subagents=self.subagents if self.subagents else None,
            checkpointer=checkpointer,
            memory=memory_config,
            debug=False
        )

        logger.info(f"Built DeepAgent with {len(self.subagents)} subagents")
        return self.agent

    def invoke(
        self,
        task: str,
        thread_id: Optional[str] = None,
        context: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """
        Execute agent on a task.

        Args:
            task: Task description
            thread_id: Optional thread ID for checkpointing
            context: Optional additional context

        Returns:
            Agent execution result
        """

        if not self.agent:
            self.build()

        # Prepare config
        config = {
            "recursion_limit": self.recursion_limit
        }
        if thread_id:
            config["configurable"] = {"thread_id": thread_id}

        # Prepare input
        input_data = {
            "messages": [("user", task)]
        }
        if context:
            input_data["context"] = context

        try:
            result = self.agent.invoke(input_data, config=config)

            return {
                "success": True,
                "result": result,
                "messages": result.get("messages", []),
                "todos": result.get("todos", []),
                "files": result.get("files", {}),
                "thread_id": thread_id
            }

        except Exception as e:
            import traceback
            error_details = traceback.format_exc()
            logger.error(f"Agent execution error: {str(e)}\n{error_details}")
            return {
                "success": False,
                "error": str(e),
                "details": error_details,
                "thread_id": thread_id
            }

    def stream(
        self,
        task: str,
        thread_id: Optional[str] = None,
        context: Optional[Dict[str, Any]] = None
    ):
        """
        Stream agent execution (yields intermediate results).

        Args:
            task: Task description
            thread_id: Optional thread ID for checkpointing
            context: Optional additional context

        Yields:
            Intermediate execution steps
        """

        if not self.agent:
            self.build()

        config = {
            "recursion_limit": self.recursion_limit
        }
        if thread_id:
            config["configurable"] = {"thread_id": thread_id}

        input_data = {
            "messages": [("user", task)]
        }
        if context:
            input_data["context"] = context

        try:
            for chunk in self.agent.stream(input_data, config=config):
                yield {
                    "success": True,
                    "chunk": chunk,
                    "thread_id": thread_id
                }
        except Exception as e:
            logger.error(f"Agent streaming error: {str(e)}")
            yield {
                "success": False,
                "error": str(e),
                "thread_id": thread_id
            }

    def get_state(self, thread_id: str) -> Dict[str, Any]:
        """Get agent state for a thread"""
        if not self.agent or not self.enable_checkpointing:
            return {"error": "Checkpointing not enabled"}

        try:
            state = self.agent.get_state({"configurable": {"thread_id": thread_id}})
            return {
                "success": True,
                "state": state
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e)
            }

    def resume(
        self,
        thread_id: str,
        new_input: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Resume agent from checkpoint.

        Args:
            thread_id: Thread ID to resume
            new_input: Optional new input to continue with

        Returns:
            Resumed execution result
        """

        if new_input:
            return self.invoke(new_input, thread_id=thread_id)
        else:
            # Resume without new input
            config = {"configurable": {"thread_id": thread_id}}
            try:
                result = self.agent.invoke(None, config=config)
                return {
                    "success": True,
                    "result": result,
                    "thread_id": thread_id
                }
            except Exception as e:
                return {
                    "success": False,
                    "error": str(e),
                    "thread_id": thread_id
                }


def check_availability() -> Dict[str, Any]:
    """Check if DeepAgents is available"""
    return {
        "available": DEEPAGENTS_AVAILABLE,
        "error": IMPORT_ERROR if not DEEPAGENTS_AVAILABLE else None
    }
