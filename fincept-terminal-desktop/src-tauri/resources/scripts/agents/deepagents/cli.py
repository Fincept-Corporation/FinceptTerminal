"""
DeepAgents CLI - Command-line interface for Fincept DeepAgents
Exposes DeepAgents functionality to Tauri/Rust backend

Dual-path routing:
  - Tool-calling LLMs (OpenAI, Anthropic, Google, Groq, etc.) -> deepagents library
  - Fincept LLM / Ollama / unknown -> FinceptOrchestrator (prompt loop)
"""

import json
import sys
import os
import traceback
from typing import Dict, Any, Optional, List

# Add parent directory to path for imports when run as script
if __name__ == "__main__":
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from agent_factory import (
    create_fincept_deep_agent,
    create_agent_by_type,
    AGENT_FACTORIES
)
from deep_agent_wrapper import check_availability, DeepAgentWrapper
from subagent_templates import list_available_templates, get_subagent_template
from fincept_orchestrator import FinceptOrchestrator

# Providers that support tool calling (bind_tools / tool_calls in AIMessage)
TOOL_CALLING_PROVIDERS = {
    "openai", "anthropic", "google", "groq", "deepseek", "openrouter",
    "azure", "mistral", "cohere", "fireworks", "together"
}

# Map agent types to their subagent names for the orchestrator
AGENT_SUBAGENTS = {
    "research": ["data_analyst", "reporter"],
    "trading_strategy": ["data_analyst", "trading", "backtester", "risk_analyzer", "reporter"],
    "portfolio_management": ["data_analyst", "portfolio_optimizer", "risk_analyzer", "reporter"],
    "risk_assessment": ["data_analyst", "risk_analyzer", "reporter"],
    "general": ["research", "data_analyst", "trading", "risk_analyzer", "portfolio_optimizer", "backtester", "reporter"]
}


def _parse_config(raw_config) -> dict:
    """Parse config from various formats (dict, JSON string, None) to dict."""
    if isinstance(raw_config, dict):
        return raw_config
    if isinstance(raw_config, str):
        try:
            return json.loads(raw_config)
        except (json.JSONDecodeError, ValueError):
            return {}
    return {}


def _supports_tool_calling(config: dict) -> bool:
    """Check if the configured LLM provider supports tool calling."""
    provider = config.get("llm_provider", "fincept").lower().strip()
    return provider in TOOL_CALLING_PROVIDERS


def _create_langchain_model(config: dict):
    """
    Create a LangChain chat model from the user's LLM config.
    Returns None if creation fails (caller should fall back to orchestrator).
    """
    provider = config.get("llm_provider", "").lower().strip()
    api_key = config.get("llm_api_key", "")
    model_name = config.get("llm_model", "")
    base_url = config.get("llm_base_url")

    try:
        if provider == "openai":
            from langchain_openai import ChatOpenAI
            kwargs = {"api_key": api_key, "model": model_name or "gpt-4o-mini"}
            if base_url:
                kwargs["base_url"] = base_url
            return ChatOpenAI(**kwargs)

        elif provider == "anthropic":
            from langchain_anthropic import ChatAnthropic
            return ChatAnthropic(
                api_key=api_key,
                model_name=model_name or "claude-sonnet-4-20250514"
            )

        elif provider == "google":
            from langchain_google_genai import ChatGoogleGenerativeAI
            return ChatGoogleGenerativeAI(
                google_api_key=api_key,
                model=model_name or "gemini-2.0-flash"
            )

        elif provider == "groq":
            from langchain_groq import ChatGroq
            return ChatGroq(
                api_key=api_key,
                model_name=model_name or "llama-3.3-70b-versatile"
            )

        elif provider in ("deepseek", "openrouter", "azure", "fireworks", "together", "mistral"):
            # These all support OpenAI-compatible API
            from langchain_openai import ChatOpenAI
            default_urls = {
                "deepseek": "https://api.deepseek.com/v1",
                "openrouter": "https://openrouter.ai/api/v1",
                "fireworks": "https://api.fireworks.ai/inference/v1",
                "together": "https://api.together.xyz/v1",
                "mistral": "https://api.mistral.ai/v1",
            }
            url = base_url or default_urls.get(provider, base_url)
            return ChatOpenAI(
                api_key=api_key,
                model=model_name or "default",
                base_url=url
            )

        elif provider == "cohere":
            from langchain_cohere import ChatCohere
            return ChatCohere(
                cohere_api_key=api_key,
                model=model_name or "command-r-plus"
            )

    except ImportError as e:
        # LangChain provider package not installed
        return None
    except Exception as e:
        return None

    return None


def _execute_with_deepagents(task: str, agent_type: str, config: dict, thread_id: str = "default") -> Dict[str, Any]:
    """
    Execute task using the real deepagents library with a tool-calling LLM.
    This enables proper sub-agent spawning, todo management, and file ops.
    """
    model = _create_langchain_model(config)
    if model is None:
        return {"success": False, "error": "Failed to create LangChain model for provider"}

    subagent_names = AGENT_SUBAGENTS.get(agent_type, ["data_analyst", "reporter"])

    wrapper = DeepAgentWrapper(
        model=model,
        system_prompt=None,  # use default
        enable_checkpointing=True,
        enable_summarization=True,
        recursion_limit=100
    )

    # Add subagents from templates
    for name in subagent_names:
        try:
            template = get_subagent_template(name)
            if template:
                wrapper.add_subagent(
                    name=template["name"],
                    description=template["description"],
                    prompt=template["system_prompt"],
                    model=model  # same model for subagents
                )
        except ValueError:
            pass  # skip unknown template names

    wrapper.build()
    result = wrapper.invoke(task=task, thread_id=thread_id)

    # Extract text result from messages
    if result.get("success") and result.get("messages"):
        messages = result["messages"]
        # Get the last AI message content
        text_parts = []
        for msg in reversed(messages):
            if hasattr(msg, "content") and msg.content:
                text_parts.insert(0, msg.content)
                break
        if text_parts:
            result["result"] = "\n".join(text_parts)

    # Format todos from deepagents state
    todos = result.get("todos", [])
    formatted_todos = []
    if isinstance(todos, list):
        for i, t in enumerate(todos):
            if isinstance(t, dict):
                formatted_todos.append(t)
            elif isinstance(t, str):
                formatted_todos.append({
                    "id": f"todo-{i+1}",
                    "task": t,
                    "status": "completed",
                    "subtasks": []
                })

    return {
        "success": result.get("success", False),
        "result": result.get("result", ""),
        "todos": formatted_todos,
        "files": result.get("files", {}),
        "thread_id": thread_id,
        "error": result.get("error")
    }


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
    """Create a configured DeepAgent."""
    try:
        agent_type = config.get("agent_type", "general")

        if agent_type and agent_type in AGENT_FACTORIES:
            agent = create_agent_by_type(
                agent_type=agent_type,
                model=config.get("model"),
                api_key=config.get("api_key")
            )
        else:
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

        agent.build()

        return {
            "success": True,
            "message": f"Created {agent_type} agent",
            "agent_id": id(agent),
            "config": {
                "agent_type": agent_type,
                "subagents": len(agent.subagents),
                "checkpointing": agent.enable_checkpointing,
                "longterm_memory": agent.enable_longterm_memory
            }
        }

    except Exception as e:
        return {"success": False, "error": str(e)}


def execute_task(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Execute a task. Routes to deepagents library or orchestrator based on LLM provider.
    """
    try:
        if isinstance(params, str):
            params = json.loads(params)

        agent_type = params.get("agent_type", "general")
        task = params.get("task")
        thread_id = params.get("thread_id", "default-thread")
        context = params.get("context")
        config = _parse_config(params.get("config", {}))

        if not task:
            return {"success": False, "error": "No task provided"}

        full_task = task
        if context:
            ctx = context if isinstance(context, str) else json.dumps(context)
            full_task = f"{task}\n\nAdditional context: {ctx}"

        # Route based on LLM provider
        if _supports_tool_calling(config):
            try:
                return _execute_with_deepagents(full_task, agent_type, config, thread_id)
            except Exception as e:
                # Fall back to orchestrator on any deepagents failure
                pass

        # Fincept LLM / Ollama / fallback -> orchestrator
        api_key = config.get("api_key")
        orchestrator = FinceptOrchestrator(api_key=api_key)
        subagent_names = AGENT_SUBAGENTS.get(agent_type, ["data_analyst", "reporter"])

        result = orchestrator.execute(
            task=full_task,
            agent_type=agent_type,
            subagent_names=subagent_names
        )

        return {
            "success": result.get("success", False),
            "result": result.get("result", ""),
            "todos": result.get("todos", []),
            "files": result.get("files", {}),
            "thread_id": thread_id,
            "error": result.get("error")
        }

    except Exception as e:
        return {"success": False, "error": str(e), "details": traceback.format_exc()}


def create_plan(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Create an execution plan (list of steps/todos) without executing them.
    Uses orchestrator for planning (works with any LLM).
    """
    try:
        if isinstance(params, str):
            params = json.loads(params)

        agent_type = params.get("agent_type", "general")
        task = params.get("task")
        config = _parse_config(params.get("config", {}))

        if not task:
            return {"success": False, "error": "No task provided"}

        # If tool-calling LLM, use deepagents for the whole flow via execute_task
        # But for incremental UI, we still create a plan via orchestrator
        api_key = config.get("api_key")
        orchestrator = FinceptOrchestrator(api_key=api_key)
        subagent_names = AGENT_SUBAGENTS.get(agent_type, ["data_analyst", "reporter"])

        plan = orchestrator._create_plan(task, agent_type, subagent_names)

        todos = []
        for i, step in enumerate(plan):
            todos.append({
                "id": f"todo-{i+1}",
                "task": step.get("step", f"Step {i+1}"),
                "status": "pending",
                "specialist": step.get("specialist", "data_analyst"),
                "prompt": step.get("prompt", step.get("step", task)),
                "subtasks": []
            })

        return {
            "success": True,
            "todos": todos,
            "plan": plan
        }

    except Exception as e:
        return {"success": False, "error": str(e), "details": traceback.format_exc()}


def execute_step(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Execute a single step from the plan.
    Uses tool-calling LLM directly if available, otherwise Fincept orchestrator.
    """
    try:
        if isinstance(params, str):
            params = json.loads(params)

        task = params.get("task")
        step_prompt = params.get("step_prompt")
        specialist = params.get("specialist", "data_analyst")
        config = _parse_config(params.get("config", {}))

        raw_prev = params.get("previous_results", [])
        if isinstance(raw_prev, str):
            try:
                previous_results = json.loads(raw_prev)
            except (json.JSONDecodeError, ValueError):
                previous_results = []
        elif isinstance(raw_prev, list):
            previous_results = raw_prev
        else:
            previous_results = []

        if not task or not step_prompt:
            return {"success": False, "error": "task and step_prompt are required"}

        # Try tool-calling LLM for direct inference (richer output)
        if _supports_tool_calling(config):
            model = _create_langchain_model(config)
            if model:
                try:
                    from langchain_core.messages import SystemMessage, HumanMessage

                    system_context = FinceptOrchestrator.SUBAGENT_PROMPTS.get(
                        specialist, FinceptOrchestrator.SUBAGENT_PROMPTS["data_analyst"]
                    )

                    prev_context = ""
                    if previous_results:
                        prev_context = "\n\nPrevious findings from the team:\n"
                        for pr in previous_results:
                            prev_context += f"--- {pr.get('step', '')} ---\n{pr.get('result', '')}\n\n"

                    messages = [
                        SystemMessage(content=system_context),
                        HumanMessage(content=f"""Context: You are working as part of a multi-agent team analyzing a financial task.
Overall task: {task}
Your specific assignment: {step_prompt}
{prev_context}
Provide a thorough, detailed analysis. Include specific data points, company names, numbers, and actionable insights where relevant. Be comprehensive.""")
                    ]

                    response = model.invoke(messages)
                    return {"success": True, "result": response.content}
                except Exception:
                    pass  # Fall through to orchestrator

        # Fincept LLM / fallback -> orchestrator
        api_key = config.get("api_key")
        orchestrator = FinceptOrchestrator(api_key=api_key)

        system_context = orchestrator.SUBAGENT_PROMPTS.get(
            specialist, orchestrator.SUBAGENT_PROMPTS["data_analyst"]
        )

        prev_context = ""
        if previous_results:
            prev_context = "\n\nPrevious findings from the team:\n"
            for pr in previous_results:
                prev_context += f"--- {pr.get('step', '')} ---\n{pr.get('result', '')}\n\n"

        full_prompt = f"""{system_context}

Context: You are working as part of a multi-agent team analyzing a financial task.
Overall task: {task}
Your specific assignment: {step_prompt}
{prev_context}
Provide a thorough, detailed analysis. Include specific data points, company names, numbers, and actionable insights where relevant. Be comprehensive."""

        result = orchestrator._call_llm(full_prompt, max_tokens=1500)

        return {"success": True, "result": result}

    except Exception as e:
        return {"success": False, "error": str(e), "details": traceback.format_exc()}


def synthesize_results(params: Dict[str, Any]) -> Dict[str, Any]:
    """
    Synthesize all step results into a final report.
    Uses tool-calling LLM if available, otherwise Fincept orchestrator.
    """
    try:
        if isinstance(params, str):
            params = json.loads(params)

        task = params.get("task")
        config = _parse_config(params.get("config", {}))

        raw_steps = params.get("step_results", [])
        if isinstance(raw_steps, str):
            try:
                step_results = json.loads(raw_steps)
            except (json.JSONDecodeError, ValueError):
                step_results = []
        elif isinstance(raw_steps, list):
            step_results = raw_steps
        else:
            step_results = []

        if not task or not step_results:
            return {"success": False, "error": "task and step_results are required"}

        synthesis_prompt = f"""You are a senior financial analyst writing a comprehensive report.

Original task: {task}

Below are the results from your specialist team:

"""
        for sr in step_results:
            synthesis_prompt += f"--- {sr.get('step', '')} (by {sr.get('specialist', '')}) ---\n{sr.get('result', '')}\n\n"

        synthesis_prompt += """
Now synthesize all the above into a single, cohesive, well-structured report.
Use clear headings, bullet points, and organize the information logically.
Include a brief executive summary at the top and key recommendations at the bottom.
Be thorough and professional."""

        # Try tool-calling LLM
        if _supports_tool_calling(config):
            model = _create_langchain_model(config)
            if model:
                try:
                    from langchain_core.messages import HumanMessage
                    response = model.invoke([HumanMessage(content=synthesis_prompt)])
                    return {"success": True, "result": response.content}
                except Exception:
                    pass  # Fall through

        # Fincept LLM / fallback
        api_key = config.get("api_key")
        orchestrator = FinceptOrchestrator(api_key=api_key)
        final_report = orchestrator._call_llm(synthesis_prompt, max_tokens=3000)

        return {"success": True, "result": final_report}

    except Exception as e:
        return {"success": False, "error": str(e), "details": traceback.format_exc()}


def stream_task(params: Dict[str, Any]) -> None:
    """Stream task execution (yields JSON chunks to stdout)."""
    try:
        if isinstance(params, str):
            params = json.loads(params)

        agent_type = params.get("agent_type", "general")
        task = params.get("task")
        thread_id = params.get("thread_id")
        context = params.get("context")
        config = _parse_config(params.get("config", {}))

        if not task:
            print(json.dumps({"success": False, "error": "No task provided"}))
            return

        agent = create_agent_by_type(
            agent_type=agent_type,
            model=config.get("model"),
            api_key=config.get("api_key")
        )
        agent.build()

        for chunk in agent.stream(task=task, thread_id=thread_id, context=context):
            print(json.dumps(chunk))
            sys.stdout.flush()

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))


def get_agent_state(params: Dict[str, Any]) -> Dict[str, Any]:
    """Get agent state for a thread."""
    try:
        thread_id = params.get("thread_id")
        if not thread_id:
            return {"success": False, "error": "No thread_id provided"}
        return {
            "success": False,
            "error": "State retrieval requires persistent agent instance"
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


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
            "Streaming execution",
            "Dual-path: tool-calling LLMs use deepagents library, Fincept LLM uses orchestrator"
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

        elif command == "create_plan":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = create_plan(params)

        elif command == "execute_step":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = execute_step(params)

        elif command == "synthesize_results":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = synthesize_results(params)

        elif command == "stream_task":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            stream_task(params)
            return

        elif command == "get_state":
            params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
            result = get_agent_state(params)

        elif command == "list_capabilities":
            result = list_capabilities()

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
