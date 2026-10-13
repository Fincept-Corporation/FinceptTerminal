"""
cli.py — C++ entry point for Fincept Deep Agents.

Single responsibility:
  - Receive a command + JSON params from C++ (AIQuantLabService::run_deep_agent)
  - Dispatch to agent.py (tool-calling LLMs) or orchestrator.py (fallback)
  - Return one JSON object on stdout; all logs go to stderr

Usage:
  python cli.py <command> --stdin        (params JSON on stdin — used by the terminal,
                                           because execute_task carries the LLM API key)
  python cli.py <command> '<json_params>' (manual use without credentials)

Commands:
  check_status      Check library availability and list capabilities
  list_capabilities List all agent types and subagents
  execute_task      Run a full agent task

Output schema (execute_task):
  { "success": bool, "result": str, "todos": [...], "files": {...},
    "thread_id": str, "error": str|null }
"""

from __future__ import annotations

import json
import logging
import sys
import traceback
from pathlib import Path
from typing import Any

# Force all logging to stderr — stdout is reserved for JSON output to C++
logging.basicConfig(
    level=logging.WARNING,
    format="%(levelname)-8s %(name)s: %(message)s",
    stream=sys.stderr,
    force=True,
)

# Add parent dirs to path
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

logger = logging.getLogger(__name__)

from models import create_model, extract_text, supports_tool_calling
from subagents import get_subagents_for_type, list_agent_types, list_subagent_names, AGENT_SUBAGENTS

# ---------------------------------------------------------------------------
# Availability check (import deepagents lazily so CLI starts fast)
# ---------------------------------------------------------------------------

def _check_deepagents() -> dict[str, Any]:
    try:
        from deepagents import create_deep_agent  # noqa: F401
        return {"available": True, "error": None}
    except ImportError as exc:
        return {"available": False, "error": str(exc)}


# ---------------------------------------------------------------------------
# Command handlers
# ---------------------------------------------------------------------------

def cmd_check_status(_params: dict[str, Any]) -> dict[str, Any]:
    info = _check_deepagents()
    return {
        "success":          True,
        "available":        info["available"],
        "error":            info["error"],
        "agent_types":      list_agent_types(),
        "subagent_names":   list_subagent_names(),
    }


def cmd_list_capabilities(_params: dict[str, Any]) -> dict[str, Any]:
    return {
        "success":     True,
        "agent_types": {k: v for k, v in AGENT_SUBAGENTS.items()},
        "subagents":   list_subagent_names(),
        "features": [
            "Hierarchical subagent spawning",
            "Built-in todo management",
            "Filesystem tools (ls/read/write/edit/glob/grep)",
            "Shell execution via LocalShellBackend (opt-in)",
            "Persistent memory via AGENTS.md",
            "Skills via SKILL.md (add skills/ directory to enable)",
            "Human-in-the-loop interrupt_on",
            "AnthropicPromptCaching (auto)",
            "Summarization middleware (auto)",
            "Structured output via response_schema",
        ],
    }


def cmd_execute_task(params: dict[str, Any]) -> dict[str, Any]:
    task       = params.get("task")
    agent_type = params.get("agent_type", "general")
    thread_id  = params.get("thread_id",  "default-thread")
    config     = _parse_config(params.get("config", {}))

    if not task:
        return _error("task is required")

    context_data = params.get("context", {})

    if supports_tool_calling(config):
        return _execute_with_deepagents(
            task=task,
            agent_type=agent_type,
            config=config,
            thread_id=thread_id,
            context_data=context_data,
        )

    return _execute_with_orchestrator(
        task=task,
        agent_type=agent_type,
        config=config,
        thread_id=thread_id,
    )


# ---------------------------------------------------------------------------
# Internal execution helpers
# ---------------------------------------------------------------------------

def _execute_with_deepagents(
    task: str,
    agent_type: str,
    config: dict[str, Any],
    thread_id: str,
    context_data: dict[str, Any],
) -> dict[str, Any]:
    """Execute via deepagents library (tool-calling LLM path)."""
    try:
        from agent import create_agent, build_fincept_context

        model    = create_model(config)
        compiled = create_agent(
            model=model,
            config={**config, "agent_type": agent_type},
        )
        ctx = build_fincept_context({"agent_type": agent_type, "context": context_data})

        run_config = {"recursion_limit": 1000}
        if thread_id:
            run_config["configurable"] = {"thread_id": thread_id}

        result = compiled.invoke(
            {"messages": [("user", task)]},
            config=run_config,
            context=ctx,
        )
        return _format_result(result, thread_id)

    except Exception as exc:
        # Never silently fall through — log and report
        logger.error("deepagents execution failed: %s\n%s", exc, traceback.format_exc())
        return _error(str(exc))


def _execute_with_orchestrator(
    task: str,
    agent_type: str,
    config: dict[str, Any],
    thread_id: str,
) -> dict[str, Any]:
    """Execute via FinceptOrchestrator (non-tool-calling LLM fallback)."""
    from orchestrator import FinceptOrchestrator
    subagent_names = [s["name"] for s in get_subagents_for_type(agent_type)]
    orch = FinceptOrchestrator(api_key=config.get("llm_api_key"))
    result = orch.execute(task, agent_type, subagent_names)
    result["thread_id"] = thread_id
    return result


def _format_result(raw: dict[str, Any], thread_id: str) -> dict[str, Any]:
    """Normalise a deepagents invoke() result to the standard output schema."""
    messages = raw.get("messages", [])
    result_text = ""
    for msg in reversed(messages):
        content = getattr(msg, "content", None)
        if content:
            result_text = extract_text(content)
            break

    todos = []
    raw_todos = raw.get("todos", [])
    if isinstance(raw_todos, list):
        for i, t in enumerate(raw_todos):
            if isinstance(t, dict):
                todos.append(t)
            elif isinstance(t, str):
                todos.append({"id": f"todo-{i+1}", "task": t, "status": "completed", "subtasks": []})

    return {
        "success":   True,
        "result":    result_text,
        "todos":     todos,
        "files":     raw.get("files", {}),
        "thread_id": thread_id,
        "error":     None,
    }


# ---------------------------------------------------------------------------
# Utilities
# ---------------------------------------------------------------------------

def _parse_config(raw: Any) -> dict[str, Any]:
    if isinstance(raw, dict):
        return raw
    if isinstance(raw, str):
        try:
            return json.loads(raw)
        except (json.JSONDecodeError, ValueError):
            return {}
    return {}


def _error(msg: str) -> dict[str, Any]:
    return {"success": False, "result": "", "todos": [], "files": {}, "thread_id": "", "error": msg}


def _print_json(obj: Any) -> None:
    print(json.dumps(obj, default=str), flush=True)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

_COMMANDS = {
    "check_status":      cmd_check_status,
    "list_capabilities": cmd_list_capabilities,
    "execute_task":      cmd_execute_task,
}


def main() -> None:
    if len(sys.argv) < 2:
        _print_json(_error(
            f"No command specified. Available: {', '.join(_COMMANDS)}"
        ))
        sys.exit(1)

    command = sys.argv[1]
    raw_params = sys.argv[2] if len(sys.argv) > 2 else "{}"
    if raw_params == "--stdin":
        # The terminal sends execute_task's payload (LLM API key included) on
        # stdin: argv is readable by any same-user process.
        raw_params = sys.stdin.read() or "{}"

    try:
        params = json.loads(raw_params)
    except (json.JSONDecodeError, ValueError) as exc:
        _print_json(_error(f"Invalid JSON params: {exc}"))
        sys.exit(1)
    if not isinstance(params, dict):
        _print_json(_error("JSON params must be an object"))
        sys.exit(1)

    if command not in _COMMANDS:
        _print_json(_error(f"Unknown command '{command}'. Available: {', '.join(_COMMANDS)}"))
        sys.exit(1)

    _print_json(_COMMANDS[command](params))


if __name__ == "__main__":
    main()
