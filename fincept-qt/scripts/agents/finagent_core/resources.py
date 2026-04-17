"""
Path derivation for per-persona runtime state.

Every stateful resource (sessions, memory, knowledge, agentic memory) lives
under a path keyed by (user_id, agent_id). This module is the single source
of truth for those paths — no other module should compose them by hand.
"""

import os
import re
from pathlib import Path
from typing import Optional


_UNSAFE_RE = re.compile(r"[^A-Za-z0-9_\-]")


def sanitize(raw: str) -> str:
    """Sanitise an id for safe filesystem use."""
    if raw is None or not str(raw).strip():
        raise ValueError("id must be non-empty")
    return _UNSAFE_RE.sub("_", str(raw).strip())


def base_dir() -> str:
    """
    Root directory for all finagent persona state.

    Overridable via FINAGENT_DATA_DIR env var. Otherwise defaults to
    <LOCALAPPDATA>/com.fincept.terminal/finagent on Windows, or
    $XDG_DATA_HOME/com.fincept.terminal/finagent (~/.local/share fallback)
    elsewhere.
    """
    env = os.getenv("FINAGENT_DATA_DIR")
    if env:
        return env
    if os.name == "nt":
        root = os.getenv("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
        return str(Path(root) / "com.fincept.terminal" / "finagent")
    xdg = os.getenv("XDG_DATA_HOME") or str(Path.home() / ".local" / "share")
    return str(Path(xdg) / "com.fincept.terminal" / "finagent")


def persona_dir(user_id: str, agent_id: str) -> str:
    u = sanitize(user_id)
    a = sanitize(agent_id)
    return str(Path(base_dir()) / "users" / u / "personas" / a)


def ensure_persona_dir(user_id: str, agent_id: str) -> str:
    d = persona_dir(user_id, agent_id)
    Path(d).mkdir(parents=True, exist_ok=True)
    return d


def sessions_db(user_id: str, agent_id: str) -> str:
    return str(Path(persona_dir(user_id, agent_id)) / "sessions.db")


def memory_db(user_id: str, agent_id: str) -> str:
    return str(Path(persona_dir(user_id, agent_id)) / "memory.db")


def knowledge_dir(user_id: str, agent_id: str) -> str:
    return str(Path(persona_dir(user_id, agent_id)) / "knowledge")


def agentic_memory_db(user_id: str, agent_id: str) -> str:
    return str(Path(persona_dir(user_id, agent_id)) / "agentic_memory.db")
