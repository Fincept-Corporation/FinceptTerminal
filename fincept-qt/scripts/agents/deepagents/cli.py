"""
cli.py — C++ entry point for Fincept Deep Agents.

Single responsibility:
  - Receive a command + JSON params from C++ via argv
  - Dispatch to agent.py (tool-calling LLMs) or orchestrator.py (fallback)
  - Return newline-delimited JSON to stdout
  - Send all logs to stderr

Usage:
  python cli.py <command> '<json_params>'

Commands:
  check_status      Check library availability and list capabilities
  execute_task      Run a full agent task
  stream_task       Run a task with streaming output (one JSON chunk per line)
  create_plan       Planning pass only — returns todo list without executing
  execute_step      Execute a single step (for incremental UI updates)
  synthesize        Combine step results into a final report
  list_capabilities List all agent types and subagents
  resume_thread     Resume an agent from a saved checkpoint

Output schema (all commands):
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

from models import create_model, extract_text, supports_tool_calling, TOOL_CALLING_PROVIDERS
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
            "Thread checkpointing via MemorySaver",
            "Cross-session storage via StoreBackend",
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


def cmd_stream_task(params: dict[str, Any]) -> None:
    """Stream task execution — yields one JSON object per line to stdout."""
    task       = params.get("task")
    agent_type = params.get("agent_type", "general")
    thread_id  = params.get("thread_id",  "default-thread")
    config     = _parse_config(params.get("config", {}))
    context_data = params.get("context", {})

    if not task:
        _print_json(_error("task is required"))
        return

    if not supports_tool_calling(config):
        # Orchestrator doesn't stream — run normally and emit one chunk
        result = _execute_with_orchestrator(task, agent_type, config, thread_id)
        _print_json(result)
        return

    try:
        from agent import create_agent, build_fincept_context
        from models import create_model

        model    = create_model(config)
        compiled = create_agent(model=model, config={**config, "agent_type": agent_type})
        ctx      = build_fincept_context({**params, "agent_type": agent_type})

        run_config = {"recursion_limit": 1000}
        if thread_id:
            run_config["configurable"] = {"thread_id": thread_id}

        for chunk in compiled.stream(
            {"messages": [("user", task)]},
            config=run_config,
            context=ctx,
            stream_mode="updates",   # incremental diffs only, not full state each step
            subgraphs=True,          # expose subagent execution as separate chunks
        ):
            _print_json({"success": True, "chunk": _serialise_chunk(chunk), "thread_id": thread_id})

    except Exception as exc:
        _print_json({"success": False, "error": str(exc), "thread_id": thread_id})


def cmd_create_plan(params: dict[str, Any]) -> dict[str, Any]:
    task       = params.get("task")
    agent_type = params.get("agent_type", "general")
    config     = _parse_config(params.get("config", {}))

    if not task:
        return _error("task is required")

    # Use tool-calling LLM for richer plan generation if available
    if supports_tool_calling(config):
        try:
            from langchain_core.messages import HumanMessage
            from subagents import AGENT_SUBAGENTS

            model = create_model(config)
            subagent_names = AGENT_SUBAGENTS.get(agent_type, list(AGENT_SUBAGENTS.keys()))
            prompt = (
                f"You are a planning agent for a financial analysis task.\n\n"
                f"Task: {task}\n\n"
                f"Available specialists: {', '.join(subagent_names)}\n\n"
                f"Create a concise execution plan as a JSON array. Each item must have:\n"
                f'  "step": short step title\n'
                f'  "specialist": one of the available specialists\n'
                f'  "prompt": specific instructions for that specialist\n\n'
                f"Return ONLY the JSON array, no other text."
            )
            response = model.invoke([HumanMessage(content=prompt)])
            raw = extract_text(response.content)

            import json as _json
            start = raw.find("[")
            end   = raw.rfind("]") + 1
            plan  = _json.loads(raw[start:end]) if start >= 0 and end > start else []

            todos = [
                {
                    "id":         f"todo-{i+1}",
                    "task":       step.get("step", f"Step {i+1}"),
                    "status":     "pending",
                    "specialist": step.get("specialist", "data-analyst"),
                    "prompt":     step.get("prompt", task),
                    "subtasks":   [],
                }
                for i, step in enumerate(plan)
            ]
            return {"success": True, "todos": todos, "plan": plan, "error": None}

        except Exception as exc:
            logger.warning("Tool-calling create_plan failed: %s", exc)

    from orchestrator import FinceptOrchestrator
    subagent_names = [s["name"] for s in get_subagents_for_type(agent_type)]
    orch = FinceptOrchestrator(api_key=config.get("llm_api_key"))
    return orch.create_plan(task, agent_type, subagent_names)


def cmd_execute_step(params: dict[str, Any]) -> dict[str, Any]:
    task             = params.get("task")
    step_prompt      = params.get("step_prompt")
    specialist       = params.get("specialist", "data-analyst")
    config           = _parse_config(params.get("config", {}))
    previous_results = _parse_list(params.get("previous_results", []))

    if not task or not step_prompt:
        return _error("task and step_prompt are required")

    # Prefer tool-calling LLM for richer step output
    if supports_tool_calling(config):
        try:
            from langchain_core.messages import SystemMessage, HumanMessage
            from subagents import AGENT_SUBAGENTS
            from orchestrator import SUBAGENT_PROMPTS

            model = create_model(config)
            system = SUBAGENT_PROMPTS.get(specialist, SUBAGENT_PROMPTS["data-analyst"])

            prev = ""
            if previous_results:
                prev = "\n\nPrevious findings:\n" + "".join(
                    f"--- {r.get('step','')} ---\n{r.get('result','')}\n\n"
                    for r in previous_results
                )

            msgs = [
                SystemMessage(content=system),
                HumanMessage(content=(
                    f"Overall task: {task}\n"
                    f"Your assignment: {step_prompt}"
                    f"{prev}\n\n"
                    f"Provide thorough analysis with specific data points."
                )),
            ]
            response = model.invoke(msgs)
            return {"success": True, "result": extract_text(response.content), "error": None}

        except Exception as exc:
            logger.warning("Tool-calling step failed, falling back to orchestrator: %s", exc)

    from orchestrator import FinceptOrchestrator
    orch = FinceptOrchestrator(api_key=config.get("llm_api_key"))
    return orch.execute_step(task, step_prompt, specialist, previous_results)


def cmd_synthesize(params: dict[str, Any]) -> dict[str, Any]:
    task         = params.get("task")
    step_results = _parse_list(params.get("step_results", []))
    config       = _parse_config(params.get("config", {}))

    if not task or not step_results:
        return _error("task and step_results are required")

    if supports_tool_calling(config):
        try:
            from langchain_core.messages import HumanMessage

            model = create_model(config)
            findings = "".join(
                f"--- {r.get('step','')} (by {r.get('specialist','')}) ---\n{r.get('result','')}\n\n"
                for r in step_results
            )
            prompt = (
                f"You are a senior financial analyst.\n\n"
                f"Task: {task}\n\nSpecialist findings:\n{findings}\n"
                f"Synthesize into a professional report with: "
                f"Executive Summary, Analysis, Risks, Recommendations."
            )
            response = model.invoke([HumanMessage(content=prompt)])
            return {"success": True, "result": extract_text(response.content), "error": None}

        except Exception as exc:
            logger.warning("Tool-calling synthesize failed, falling back to orchestrator: %s", exc)

    from orchestrator import FinceptOrchestrator
    orch = FinceptOrchestrator(api_key=config.get("llm_api_key"))
    return orch.synthesize(task, step_results)


def cmd_resume_thread(params: dict[str, Any]) -> dict[str, Any]:
    thread_id = params.get("thread_id")
    new_input = params.get("input")
    config    = _parse_config(params.get("config", {}))

    if not thread_id:
        return _error("thread_id is required")

    if not supports_tool_calling(config):
        return _error("resume_thread requires a tool-calling LLM provider")

    try:
        from agent import create_agent, build_fincept_context

        model    = create_model(config)
        compiled = create_agent(model=model, config=config)
        ctx      = build_fincept_context(params)

        run_config = {
            "recursion_limit": 1000,
            "configurable":    {"thread_id": thread_id},
        }

        inp = {"messages": [("user", new_input)]} if new_input else None
        result = compiled.invoke(inp, config=run_config, context=ctx)
        return _format_result(result, thread_id)

    except Exception as exc:
        return _error(str(exc))


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
        # Never silently fall through — log and raise
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


def _serialise_chunk(chunk: Any) -> Any:
    """Make a streaming chunk JSON-serialisable."""
    if isinstance(chunk, dict):
        out = {}
        for k, v in chunk.items():
            if hasattr(v, "content"):
                out[k] = {"content": extract_text(v.content), "type": type(v).__name__}
            elif isinstance(v, (str, int, float, bool, list, dict, type(None))):
                out[k] = v
            else:
                out[k] = str(v)
        return out
    return str(chunk)


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


def _parse_list(raw: Any) -> list[Any]:
    if isinstance(raw, list):
        return raw
    if isinstance(raw, str):
        try:
            return json.loads(raw)
        except (json.JSONDecodeError, ValueError):
            return []
    return []


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
    "stream_task":       cmd_stream_task,
    "create_plan":       cmd_create_plan,
    "execute_step":      cmd_execute_step,
    "synthesize":        cmd_synthesize,
    "resume_thread":     cmd_resume_thread,
}


def main() -> None:
    if len(sys.argv) < 2:
        _print_json(_error(
            f"No command specified. Available: {', '.join(_COMMANDS)}"
        ))
        sys.exit(1)

    command = sys.argv[1]
    raw_params = sys.argv[2] if len(sys.argv) > 2 else "{}"

    try:
        params = json.loads(raw_params)
    except (json.JSONDecodeError, ValueError) as exc:
        _print_json(_error(f"Invalid JSON params: {exc}"))
        sys.exit(1)

    if command not in _COMMANDS:
        _print_json(_error(f"Unknown command '{command}'. Available: {', '.join(_COMMANDS)}"))
        sys.exit(1)

    if command == "stream_task":
        # stream_task prints its own output and returns None
        _COMMANDS[command](params)
    else:
        result = _COMMANDS[command](params)
        _print_json(result)


if __name__ == "__main__":
    main()
