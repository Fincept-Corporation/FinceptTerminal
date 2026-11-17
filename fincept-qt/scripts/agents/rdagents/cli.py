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
import os
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
    """
    rdagent version, importable loops, and what each task kind still needs on
    this machine. Shape (additive to the original {success, rdagent_version,
    availability}):
      availability: {factor_loop, model_loop, quant_loop, conda, docker: bool, mcp: {...}}
      requirements: {platform, platform_supported, conda, conda_default_env, docker,
                     docker_detail, model_env_type, factor_data_ready, factor_data_folder}
      ready:   {factor_mining|model_optimization|quant_research: bool}
      missing: {<kind>: [human-readable lines]}   (the LLM key is checked per run)
    """
    try:
        import importlib.metadata
        rdagent_version = importlib.metadata.version("rdagent")
    except Exception:
        rdagent_version = "unknown"

    try:
        from loops import loop_availability, preflight, runtime_requirements
        avail = loop_availability()
        req = runtime_requirements()
        # The factor data folder is per profile (next to the task workspaces),
        # which this call does not know; run_task checks it for real.
        req["factor_data_ready"] = None
        req["factor_data_folder"] = None
        avail["conda"] = req["conda"]
        avail["docker"] = req["docker"]
        missing = {k: preflight(k, None, req) for k in _KINDS}
    except Exception as e:
        return {"success": True, "rdagent_version": rdagent_version,
                "availability": {"error": str(e)},
                "ready": {k: False for k in _KINDS},
                "missing": {k: [f"rdagent failed to import: {e}"] for k in _KINDS}}

    return {
        "success": True,
        "rdagent_version": rdagent_version,
        "availability": avail,
        "requirements": req,
        "ready": {k: not v for k, v in missing.items()},
        "missing": missing,
    }


def _prepare_process_env(workspace: Path) -> None:
    """
    rdagent derives its workspace, log, prompt-cache and data folders from the
    current directory when it is IMPORTED. The terminal starts this process in
    the scripts directory (read-only in installs), so move into the task
    workspace first. The factor source data (generated once with Docker) is
    shared by all tasks: <...>/rdagents/factor_data when the workspace sits in
    <...>/rdagents/workspaces/<id> (AIQuantLabService layout).
    """
    workspace.mkdir(parents=True, exist_ok=True)
    os.chdir(workspace)
    shared = workspace.parent.parent if workspace.parent.name == "workspaces" else workspace
    data_root = shared / "factor_data"
    os.environ.setdefault("FACTOR_CoSTEER_DATA_FOLDER", str(data_root / "full"))
    os.environ.setdefault("FACTOR_CoSTEER_DATA_FOLDER_DEBUG", str(data_root / "debug"))


def cmd_run_task(params: dict[str, Any]) -> None:
    """Run one RD loop synchronously; emits JSON lines (progress, then result/error)."""
    kind = params.get("kind", "")
    task_id = params.get("task_id") or f"{kind or 'task'}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"

    def fail(msg: str, **extra: Any) -> None:
        _out({"event": "error", "task_id": task_id, "error": msg, **extra})
        sys.exit(1)

    if kind not in _KINDS:
        fail(f"Unknown task kind {kind!r}; expected one of {list(_KINDS)}")
    workspace = params.get("workspace")
    if not workspace:
        fail("workspace is required")
    workspace_path = Path(workspace).resolve()

    llm_config = params.get("config") or {}
    try:
        _prepare_process_env(workspace_path)
        # Must precede the rdagent import: rdagent's LLM settings (CHAT_MODEL ...)
        # are read once, when rdagent.oai.backend.litellm is imported.
        from config import apply_llm_config
        apply_llm_config(llm_config)
        from loops import (
            build_factor_loop, build_model_loop, build_quant_loop, load_loop_checkpoint, preflight,
        )
        from task_runner import run_loop_sync
    except Exception as e:
        fail(f"RD-Agent is not installed or failed to import: {e}")

    # Preflight before constructing anything: building a Qlib scenario already
    # shells out to conda / Docker and fails deep inside rdagent otherwise.
    missing = preflight(kind, llm_config)
    if missing:
        fail("RD-Agent cannot run on this machine yet. Missing: " + "; ".join(missing), missing=missing)

    max_iterations = max(1, int(params.get("max_iterations", params.get("iterations", 10))))
    enable_mcp = bool(params.get("enable_mcp", False))
    mcp_port = int(params.get("mcp_port", 18765))

    try:
        resume_from = params.get("resume_from")
        ignored: list[str] = []
        if resume_from:
            rd_loop = load_loop_checkpoint(kind, resume_from)
        elif kind == "factor_mining":
            target_ic = params.get("target_ic")
            rd_loop, ignored = build_factor_loop(
                params.get("task_description", ""),
                params.get("target_market", ""),
                float(target_ic) if target_ic is not None else None,
                enable_mcp=enable_mcp, mcp_port=mcp_port)
        elif kind == "model_optimization":
            rd_loop, ignored = build_model_loop(
                params.get("model_type", ""),
                params.get("optimization_target", ""),
                enable_mcp=enable_mcp, mcp_port=mcp_port)
        else:
            rd_loop, ignored = build_quant_loop(
                params.get("research_goal", params.get("task_description", "")),
                params.get("target_market", ""),
                enable_mcp=enable_mcp, mcp_port=mcp_port)
        if ignored:
            logger.warning("RD-Agent task %s: parameters not applied: %s", task_id, "; ".join(ignored))

        result = run_loop_sync(task_id, rd_loop, max_iterations, workspace_path, _out,
                               extra={"ignored_params": ignored} if ignored else None)
        _out(result)
    except SystemExit:
        raise
    except Exception as exc:
        logger.exception("RD-Agent task %s failed", task_id)
        fail(str(exc))


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
    # The port the server was actually started on (start_mcp_server_process moves to
    # the next free port when the requested one is taken); the first open port in
    # the range may belong to something else.
    if proc is not None:
        try:
            port = int(proc.args[-1])
        except (TypeError, ValueError, IndexError):
            pass

    return {
        "success": toolset is not None,
        "url": f"http://127.0.0.1:{port}/mcp",
        "port": port,
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

    # Loopback only: Streamlit listens on every network interface by default, which
    # put the agent logs on the LAN. The variable reaches both launch forms below.
    import os
    ui_env = dict(os.environ, STREAMLIT_SERVER_ADDRESS="127.0.0.1")

    try:
        proc = subprocess.Popen(
            [sys.executable, "-m", "rdagent.app.streamlit", "--server.port", str(port),
             "--server.headless", "true", "--server.address", "127.0.0.1"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
            env=ui_env,
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
                env=ui_env,
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
