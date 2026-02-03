"""
DeepAgents CLI - Command-line interface for Fincept DeepAgents
Exposes DeepAgents functionality to Tauri/Rust backend
"""

import json
import sys
import os
from typing import Dict, Any, Optional

# Add parent directory to path for imports when run as script
if __name__ == "__main__":
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_factory import (
    create_fincept_deep_agent,
    create_agent_by_type,
    AGENT_FACTORIES
)
from deep_agent_wrapper import check_availability
from subagent_templates import list_available_templates


def check_status() -> Dict[str, Any]:
    """Check if DeepAgents is available"""
    availability = check_availability()
    return {
        "success": True,
        "available": availability["available"],
        "error": availability.get("error"),
        "agent_types": list(AGENT_FACTORIES.keys()),
        "subagent_templates": list_available_templates()
    }


def create_agent(config: Dict[str, Any]) -> Dict[str, Any]:
    """
    Create a configured DeepAgent.

    Config params:
        - agent_type: Type of agent (research, trading_strategy, etc.)
        - model: Model name or config
        - api_key: API key
        - system_prompt: Custom prompt
        - subagents: List of subagent names
        - enable_checkpointing: bool
        - enable_longterm_memory: bool
        - enable_summarization: bool
        - recursion_limit: int
    """
    try:
        agent_type = config.get("agent_type", "general")

        if agent_type and agent_type in AGENT_FACTORIES:
            # Use factory
            agent = create_agent_by_type(
                agent_type=agent_type,
                model=config.get("model"),
                api_key=config.get("api_key")
            )
        else:
            # Create custom agent
            agent = create_fincept_deep_agent(
                model=config.get("model"),
                api_key=config.get("api_key"),
                use_fincept_llm=config.get("use_fincept_llm", True),
                system_prompt=config.get("system_prompt"),
                enable_checkpointing=config.get("enable_checkpointing", True),
                enable_longterm_memory=config.get("enable_longterm_memory", False),
                enable_summarization=config.get("enable_summarization", True),
                recursion_limit=config.get("recursion_limit", 100),
                subagents=config.get("subagents")
            )

        # Build agent
        agent.build()

        return {
            "success": True,
            "message": f"Created {agent_type} agent",
            "agent_id": id(agent),  # Simple ID for tracking
            "config": {
                "agent_type": agent_type,
                "subagents": len(agent.subagents),
                "checkpointing": agent.enable_checkpointing,
                "longterm_memory": agent.enable_longterm_memory
            }
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }


def execute_task(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Execute a task with DeepAgent.

    Params:
        - agent_type: Type of agent to create
        - task: Task description
        - thread_id: Optional thread ID
        - context: Optional context dict
        - config: Agent configuration
    """
    import traceback
    try:
        # Ensure params is a dict
        if isinstance(params, str):
            params = json.loads(params)

        agent_type = params.get("agent_type", "general")
        task = params.get("task")
        thread_id = params.get("thread_id", "default-thread")  # Default thread_id if not provided
        context = params.get("context")
        config = params.get("config", {}) if isinstance(params.get("config"), dict) else {}

        if not task:
            return {"success": False, "error": "No task provided"}

        # Create agent
        agent = create_agent_by_type(
            agent_type=agent_type,
            model=config.get("model"),
            api_key=config.get("api_key")
        )
        agent.build()

        # Execute
        result = agent.invoke(
            task=task,
            thread_id=thread_id,
            context=context
        )

        # Extract final message
        messages = result.get("messages", [])
        final_message = ""
        if messages:
            last_msg = messages[-1]
            if hasattr(last_msg, "content"):
                final_message = last_msg.content
            elif isinstance(last_msg, tuple) and len(last_msg) > 1:
                final_message = last_msg[1]

        return {
            "success": result.get("success", False),
            "result": final_message,
            "todos": result.get("todos", []),
            "files": result.get("files", {}),
            "thread_id": result.get("thread_id"),
            "error": result.get("error")
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "details": traceback.format_exc()
        }


def stream_task(params: Dict[str, Any]) -> None:
    """
    Stream task execution (yields JSON chunks to stdout).

    Params:
        - agent_type: Type of agent
        - task: Task description
        - thread_id: Optional thread ID
        - context: Optional context
        - config: Agent configuration
    """
    try:
        # Ensure params is a dict
        if isinstance(params, str):
            params = json.loads(params)

        agent_type = params.get("agent_type", "general")
        task = params.get("task")
        thread_id = params.get("thread_id")
        context = params.get("context")
        config = params.get("config", {})

        if not task:
            print(json.dumps({"success": False, "error": "No task provided"}))
            return

        # Create agent
        agent = create_agent_by_type(
            agent_type=agent_type,
            model=config.get("model"),
            api_key=config.get("api_key")
        )
        agent.build()

        # Stream execution
        for chunk in agent.stream(task=task, thread_id=thread_id, context=context):
            print(json.dumps(chunk))
            sys.stdout.flush()

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))


def get_agent_state(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Get agent state for a thread.

    Params:
        - thread_id: Thread ID
    """
    try:
        thread_id = params.get("thread_id")
        if not thread_id:
            return {"success": False, "error": "No thread_id provided"}

        # This is a simplified implementation
        # In production, you'd need to maintain agent instances
        return {
            "success": False,
            "error": "State retrieval requires persistent agent instance"
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }


def list_capabilities() -> Dict[str, Any]:
    """List all agent types and subagent templates"""
    return {
        "success": True,
        "agent_types": {
            name: "Pre-configured agent for specific use case"
            for name in AGENT_FACTORIES.keys()
        },
        "subagent_templates": list_available_templates(),
        "features": [
            "Hierarchical task delegation",
            "Built-in todo management",
            "Mock file operations",
            "State checkpointing",
            "Long-term memory (optional)",
            "Context summarization",
            "Streaming execution"
        ]
    }


def main():
    """CLI entry point"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "No command specified. Available: check_status, create_agent, execute_task, stream_task, list_capabilities"
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "check_status":
            result = check_status()

        elif command == "create_agent":
            config = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = create_agent(config)

        elif command == "execute_task":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = execute_task(params)

        elif command == "stream_task":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            stream_task(params)
            return  # streaming handles its own output

        elif command == "get_state":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = get_agent_state(params)

        elif command == "list_capabilities":
            result = list_capabilities()

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}"
            }

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": str(e)
        }))
        sys.exit(1)


if __name__ == "__main__":
    main()
