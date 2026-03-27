"""
mcp_tools.py — Helpers for wiring the Fincept MCP server into RD-Agent loops.

Usage:
    from mcp_tools import get_mcp_toolset, start_mcp_server_process, MCP_AVAILABLE

    # Check availability
    if MCP_AVAILABLE:
        toolset = get_mcp_toolset(port=18765)
        # Pass toolset to PAIAgent or loop config

    # Or start the MCP server as a subprocess and get a toolset pointing at it
    proc, toolset = start_mcp_server_process(port=18765)
"""

from __future__ import annotations

import logging
import os
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

_HERE = Path(__file__).resolve().parent

# ---------------------------------------------------------------------------
# Availability checks
# ---------------------------------------------------------------------------

MCP_AVAILABLE = False
PAI_MCP_AVAILABLE = False

try:
    from pydantic_ai.mcp import MCPServerStreamableHTTP
    PAI_MCP_AVAILABLE = True
    MCP_AVAILABLE = True
except ImportError:
    MCPServerStreamableHTTP = None  # type: ignore

try:
    from mcp.server.fastmcp import FastMCP  # noqa: F401
    MCP_AVAILABLE = True
except ImportError:
    try:
        from fastmcp import FastMCP  # type: ignore  # noqa: F401
        MCP_AVAILABLE = True
    except ImportError:
        pass


# ---------------------------------------------------------------------------
# Port utilities
# ---------------------------------------------------------------------------

DEFAULT_PORT = int(os.environ.get("FINCEPT_MCP_PORT", "18765"))


def _is_port_open(host: str, port: int, timeout: float = 0.5) -> bool:
    """Return True if something is listening on host:port."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(timeout)
        return s.connect_ex((host, port)) == 0


def _find_free_port(start: int = DEFAULT_PORT) -> int:
    """Find the first free port starting from start."""
    for port in range(start, start + 20):
        if not _is_port_open("127.0.0.1", port):
            return port
    raise RuntimeError(f"No free port found in range {start}–{start + 20}")


# ---------------------------------------------------------------------------
# MCP toolset factory
# ---------------------------------------------------------------------------

def get_mcp_toolset(host: str = "127.0.0.1", port: int = DEFAULT_PORT) -> Any:
    """
    Return an MCPServerStreamableHTTP instance pointing at the Fincept MCP server.

    The server must already be running (use start_mcp_server_process() to launch it).

    Args:
        host: Server host (default 127.0.0.1)
        port: Server port (default 18765 or FINCEPT_MCP_PORT env var)

    Returns:
        MCPServerStreamableHTTP instance for use as a pydantic-ai toolset,
        or None if pydantic-ai MCP is not available.
    """
    if not PAI_MCP_AVAILABLE:
        logger.warning("pydantic-ai MCP not available. Install: pip install pydantic-ai[mcp]")
        return None

    url = f"http://{host}:{port}/mcp"
    logger.info("Creating MCPServerStreamableHTTP toolset → %s", url)
    return MCPServerStreamableHTTP(url)


# ---------------------------------------------------------------------------
# Server subprocess management
# ---------------------------------------------------------------------------

# Module-level registry of running MCP server processes
_mcp_processes: dict[int, subprocess.Popen] = {}


def start_mcp_server_process(
    port: int | None = None,
    host: str = "127.0.0.1",
    wait_timeout: float = 8.0,
) -> tuple[subprocess.Popen | None, Any]:
    """
    Start the Fincept MCP server as a background subprocess.

    If a server is already running on the chosen port, reuses it.

    Args:
        port:         Port to listen on. Auto-selects a free port if None.
        host:         Host to bind to.
        wait_timeout: Seconds to wait for the server to become ready.

    Returns:
        (process, toolset) — process is None if server was already running.
        toolset is MCPServerStreamableHTTP | None.
    """
    if port is None:
        port = DEFAULT_PORT

    # Already running on this port?
    if _is_port_open(host, port):
        logger.info("MCP server already running on port %d — reusing", port)
        return None, get_mcp_toolset(host, port)

    # Find a free port
    try:
        port = _find_free_port(port)
    except RuntimeError as e:
        logger.error("start_mcp_server_process: %s", e)
        return None, None

    server_script = str(_HERE / "mcp_server.py")
    if not Path(server_script).exists():
        logger.error("mcp_server.py not found at %s", server_script)
        return None, None

    logger.info("Starting Fincept MCP server on %s:%d", host, port)
    try:
        proc = subprocess.Popen(
            [sys.executable, server_script, "--host", host, "--port", str(port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            start_new_session=True,
        )
        _mcp_processes[port] = proc
    except Exception as e:
        logger.error("Failed to start MCP server: %s", e)
        return None, None

    # Wait for server to become ready
    deadline = time.monotonic() + wait_timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            # Process exited early
            stderr = proc.stderr.read().decode(errors="replace") if proc.stderr else ""
            logger.error("MCP server exited early. stderr: %s", stderr)
            return None, None
        if _is_port_open(host, port):
            logger.info("MCP server ready on port %d", port)
            return proc, get_mcp_toolset(host, port)
        time.sleep(0.2)

    logger.warning("MCP server did not become ready within %.1fs", wait_timeout)
    return proc, get_mcp_toolset(host, port)  # return anyway — may still start


def stop_mcp_server_process(port: int = DEFAULT_PORT) -> bool:
    """Stop the MCP server subprocess running on the given port."""
    proc = _mcp_processes.pop(port, None)
    if proc is None:
        return False
    try:
        proc.terminate()
        proc.wait(timeout=5)
    except Exception:
        proc.kill()
    logger.info("MCP server on port %d stopped", port)
    return True


def get_running_mcp_ports() -> list[int]:
    """Return list of ports with active MCP server subprocesses."""
    return [p for p, proc in list(_mcp_processes.items()) if proc.poll() is None]


# ---------------------------------------------------------------------------
# Loop injection helpers
# ---------------------------------------------------------------------------

def inject_mcp_into_loop(loop: Any, toolset: Any) -> bool:
    """
    Attempt to inject the MCP toolset into an rdagent loop.

    rdagent loops use PAIAgent internally. We try to find the agent
    attribute and append the toolset to its toolsets list.

    Args:
        loop:    LoopBase subclass instance (FactorRDLoop, ModelRDLoop, etc.)
        toolset: MCPServerStreamableHTTP instance from get_mcp_toolset()

    Returns:
        True if injection succeeded, False otherwise.
    """
    if toolset is None:
        return False

    injected = False

    # Strategy 1: loop has a direct .agent attribute (PAIAgent)
    agent = getattr(loop, "agent", None)
    if agent is not None and hasattr(agent, "toolsets"):
        if isinstance(agent.toolsets, list):
            agent.toolsets.append(toolset)
            injected = True
            logger.info("MCP toolset injected into loop.agent.toolsets")

    # Strategy 2: loop has .hypothesis_gen / .coder / .evaluator sub-agents
    for attr in ("hypothesis_gen", "coder", "evaluator", "runner"):
        sub = getattr(loop, attr, None)
        if sub is None:
            continue
        sub_agent = getattr(sub, "agent", None)
        if sub_agent is not None and hasattr(sub_agent, "toolsets"):
            if isinstance(sub_agent.toolsets, list):
                sub_agent.toolsets.append(toolset)
                injected = True
                logger.info("MCP toolset injected into loop.%s.agent.toolsets", attr)

    # Strategy 3: loop exposes add_toolset() directly
    if hasattr(loop, "add_toolset"):
        try:
            loop.add_toolset(toolset)
            injected = True
            logger.info("MCP toolset injected via loop.add_toolset()")
        except Exception as e:
            logger.warning("loop.add_toolset() failed: %s", e)

    # Strategy 4: set env var so any PAIAgent created later picks it up
    if not injected:
        current = os.environ.get("RDAGENT_MCP_SERVERS", "")
        port = DEFAULT_PORT
        for p, proc in _mcp_processes.items():
            if proc.poll() is None:
                port = p
                break
        url = f"http://127.0.0.1:{port}/mcp"
        if url not in current:
            os.environ["RDAGENT_MCP_SERVERS"] = (current + "," + url).lstrip(",")
            logger.info("Set RDAGENT_MCP_SERVERS=%s as fallback injection", os.environ["RDAGENT_MCP_SERVERS"])
        injected = True  # env var set, counts as soft injection

    return injected


def mcp_availability() -> dict[str, Any]:
    """Return a dict describing MCP availability for use in CLI status checks."""
    return {
        "mcp_server_available":  MCP_AVAILABLE,
        "pydantic_ai_mcp":       PAI_MCP_AVAILABLE,
        "default_port":          DEFAULT_PORT,
        "running_ports":         get_running_mcp_ports(),
        "tools": [
            "market_data",
            "financial_news",
            "economics_data",
            "factor_backtest",
            "symbol_search",
        ],
    }
