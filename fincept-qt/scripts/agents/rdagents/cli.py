"""
cli.py - Entry point for the Fincept RD-Agent integration.

Called by AIQuantLabService.cpp:
  python agents/rdagents/cli.py <command> [json_params | --stdin]

All stdout is JSON; all logs go to stderr. Parameters that carry credentials
(run_task's LLM config) are passed on stdin (`--stdin`), never in argv.

Commands:
  check_status             - rdagent version + which loops are importable
  run_task  --stdin        - run ONE factor-mining / model-optimization / quant-research
                             loop to completion in this process, streaming JSON-line
                             progress (see task_runner.py). The terminal owns this
                             process: task status, results and stop (= kill) are
                             served by AIQuantLabService, not by further CLI calls.
  start_ui                 - launch rdagent's Streamlit log viewer (background process)
  start_mcp_server  [json] - start the Fincept MCP tool server used by RD-Agent loops
  mcp_status               - MCP availability + running servers
"""

from __future__ import annotations

import json
import logging
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

# Ensure this directory is on sys.path for sibling imports
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))
# Also add scripts/agents/ so deepagents imports work if needed
sys.path.insert(0, str(_HERE.parent))

logging.basicConfig(
    stream=sys.stderr,
    level=logging.INFO,
    format="%(asctime)s [rdagents] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

_KINDS = ("factor_mining", "model_optimization", "quant_research")


def _out(obj: Any) -> None:
    """Print one JSON line to stdout and flush."""
    print(json.dumps(obj, default=str), flush=True)


def _err(msg: str, code: int = 1) -> None:
    _out({"success": False, "error": msg})
    sys.exit(code)


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_check_status(_params: dict[str, Any]) -> dict[str, Any]:
    try:
        import importlib.metadata
        rdagent_version = importlib.metadata.version("rdagent")
    except Exception:
        rdagent_version = "unknown"

    try:
        from loops import loop_availability
        avail = loop_availability()
    except Exception as e:
        avail = {"error": str(e)}

    return {
        "success": True,
        "rdagent_version": rdagent_version,
        "availability": avail,
    }


def cmd_run_task(params: dict[str, Any]) -> None:
    """Run one RD loop synchronously; emits JSON lines (progress, then result/error)."""
    kind = params.get("kind", "")
    task_id = params.get("task_id") or f"{kind or 'task'}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    if kind not in _KINDS:
        _out({"event": "error", "task_id": task_id,
              "error": f"Unknown task kind {kind!r}; expected one of {list(_KINDS)}"})
        sys.exit(1)
    workspace = params.get("workspace")
    if not workspace:
        _out({"event": "error", "task_id": task_id, "error": "workspace is required"})
        sys.exit(1)

    try:
        from config import apply_llm_config
        from loops import (
            FACTOR_LOOP_AVAILABLE, MODEL_LOOP_AVAILABLE, QUANT_LOOP_AVAILABLE,
            build_factor_loop, build_model_loop, build_quant_loop, load_checkpoint,
        )
        from task_runner import run_loop_sync
    except Exception as e:
        _out({"event": "error", "task_id": task_id, "error": f"RD-Agent is not installed or failed to import: {e}"})
        sys.exit(1)

    available = {"factor_mining": FACTOR_LOOP_AVAILABLE, "model_optimization": MODEL_LOOP_AVAILABLE,
                 "quant_research": QUANT_LOOP_AVAILABLE}[kind]
    if not available:
        _out({"event": "error", "task_id": task_id,
              "error": f"The RD-Agent {kind} loop is not available - install rdagent with qlib extras."})
        sys.exit(1)

    apply_llm_config(params.get("config", {}))
    max_iterations = max(1, int(params.get("max_iterations", params.get("iterations", 10))))
    enable_mcp = bool(params.get("enable_mcp", False))
    mcp_port = int(params.get("mcp_port", 18765))

    try:
        if kind == "factor_mining":
            rd_loop = build_factor_loop(
                workspace,
                params.get("task_description", "Discover alpha factors for equity trading"),
                params.get("target_market", "US"),
                float(params.get("target_ic", 0.05)),
                enable_mcp=enable_mcp, mcp_port=mcp_port)
        elif kind == "model_optimization":
            rd_loop = build_model_loop(
                workspace,
                params.get("model_type", "lightgbm"),
                params.get("optimization_target", "sharpe"),
                enable_mcp=enable_mcp, mcp_port=mcp_port)
        else:
            rd_loop = build_quant_loop(
                workspace,
                params.get("research_goal", params.get("task_description", "Quantitative research")),
                params.get("target_market", "US"),
                enable_mcp=enable_mcp, mcp_port=mcp_port)

        resume_from = params.get("resume_from")
        if resume_from:
            if not load_checkpoint(rd_loop, resume_from):
                _out({"event": "error", "task_id": task_id,
                      "error": f"Could not load checkpoint {resume_from}"})
                sys.exit(1)

        result = run_loop_sync(task_id, rd_loop, max_iterations, Path(workspace), _out)
        _out(result)
    except SystemExit:
        raise
    except Exception as exc:
        logger.exception("RD-Agent task %s failed", task_id)
        _out({"event": "error", "task_id": task_id, "error": str(exc)})
        sys.exit(1)


def cmd_start_mcp_server(params: dict[str, Any]) -> dict[str, Any]:
    """Start the Fincept MCP tool server for RD-Agent loop use."""
    try:
        from mcp_tools import start_mcp_server_process, MCP_AVAILABLE, mcp_availability
    except ImportError as e:
        return {"success": False, "error": f"mcp_tools not available: {e}"}

    if not MCP_AVAILABLE:
        return {
            "success": False,
            "error": "MCP not available. Install: pip install mcp[cli] pydantic-ai[mcp] yfinance",
            "install": "pip install 'mcp[cli]' 'pydantic-ai[mcp]' yfinance requests",
        }

    port = int(params.get("port", 18765))
    proc, toolset = start_mcp_server_process(port=port)
    avail = mcp_availability()

    return {
        "success": toolset is not None,
        "url": f"http://127.0.0.1:{avail['running_ports'][0] if avail['running_ports'] else port}/mcp",
        "port": avail["running_ports"][0] if avail["running_ports"] else port,
        "pid": proc.pid if proc else None,
        "tools": avail["tools"],
        "message": "Fincept MCP server started" if toolset else "MCP server failed to start",
        "already_running": proc is None and toolset is not None,
    }


def cmd_mcp_status(_params: dict[str, Any]) -> dict[str, Any]:
    """Return MCP availability and running server info."""
    try:
        from mcp_tools import mcp_availability
        avail = mcp_availability()
        return {"success": True, **avail}
    except ImportError:
        return {
            "success": False,
            "mcp_server_available": False,
            "pydantic_ai_mcp": False,
            "error": "mcp_tools module not importable",
        }


def cmd_start_ui(params: dict[str, Any]) -> dict[str, Any]:
    """Launch rdagent's built-in Streamlit log viewer as a background subprocess."""
    import socket
    import subprocess

    port = int(params.get("port", 19999))

    # Check if port is free; if not, increment
    for _attempt in range(10):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            if s.connect_ex(("127.0.0.1", port)) != 0:
                break  # port is free
            port += 1
    else:
        return {"success": False, "error": "No free port found in range 19999-20009"}

    try:
        proc = subprocess.Popen(
            [sys.executable, "-m", "rdagent.app.streamlit", "--server.port", str(port),
             "--server.headless", "true"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        url = f"http://localhost:{port}"
        logger.info("RD-Agent Streamlit UI started at %s (pid=%s)", url, proc.pid)
        return {"success": True, "url": url, "port": port, "pid": proc.pid,
                "message": f"Log viewer started at {url}"}
    except FileNotFoundError:
        # Fallback: try `rdagent ui` CLI entry point
        try:
            proc = subprocess.Popen(
                ["rdagent", "ui", "--port", str(port)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True,
            )
            url = f"http://localhost:{port}"
            return {"success": True, "url": url, "port": port, "pid": proc.pid,
                    "message": f"Log viewer started at {url}"}
        except FileNotFoundError:
            return {"success": False, "error": "rdagent UI not available. Install rdagent and streamlit."}
    except Exception as e:
        return {"success": False, "error": str(e)}


# ---------------------------------------------------------------------------
# Command dispatch
# ---------------------------------------------------------------------------

_COMMANDS = {
    "check_status": cmd_check_status,
    "run_task": cmd_run_task,
    "start_ui": cmd_start_ui,
    "start_mcp_server": cmd_start_mcp_server,
    "mcp_status": cmd_mcp_status,
}

# Commands that print their own JSON lines (and exit code) instead of returning one dict.
_STREAMING = {"run_task"}


def main() -> None:
    if len(sys.argv) < 2:
        _err("No command specified. Usage: cli.py <command> [json_params | --stdin]")

    command = sys.argv[1]
    handler = _COMMANDS.get(command)
    if not handler:
        _err(f"Unknown command: {command!r}. Available: {list(_COMMANDS)}")

    params: dict[str, Any] = {}
    raw = sys.argv[2] if len(sys.argv) >= 3 else ""
    if raw == "--stdin":
        raw = sys.stdin.read()
    if raw.strip():
        try:
            params = json.loads(raw)
        except json.JSONDecodeError as e:
            _err(f"Invalid JSON params: {e}")
        if not isinstance(params, dict):
            _err("JSON params must be an object")

    if command in _STREAMING:
        handler(params)
        return
    try:
        _out(handler(params))
    except Exception as exc:
        logger.exception("Command %r failed", command)
        _err(str(exc))


if __name__ == "__main__":
    main()
