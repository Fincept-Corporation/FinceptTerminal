"""
Path derivation for per-persona runtime state.

Every stateful resource (sessions, memory, knowledge, agentic memory) lives
under a path keyed by (user_id, agent_id). This module is the single source
of truth for those paths — no other module should compose them by hand.
"""

import os
import re
import sys
from pathlib import Path
from typing import Optional


_UNSAFE_RE = re.compile(r"[^A-Za-z0-9_\-]")


def sanitize(raw: Optional[str]) -> str:
    """Sanitise an id for safe filesystem use.

    Raises ValueError on None or empty/whitespace-only input.
    """
    if raw is None or not str(raw).strip():
        raise ValueError("id must be non-empty")
    return _UNSAFE_RE.sub("_", str(raw).strip())


def base_dir() -> str:
    """
    Root directory for all finagent persona state.

    Overridable via FINAGENT_DATA_DIR env var (the terminal always sets it to
    <FINCEPT_DATA_DIR>/finagent). Otherwise <FINCEPT_DATA_DIR>/finagent, then
    <the edition's default data root>/finagent (fincept_paths.data_root(): the
    folder the terminal itself uses) — never com.fincept.terminal, the folder a
    separate private build of Fincept Terminal uses.
    """
    env = os.getenv("FINAGENT_DATA_DIR")
    if env:
        return str(Path(env).expanduser())
    data_dir = os.getenv("FINCEPT_DATA_DIR", "").strip()
    if data_dir:
        return str(Path(data_dir).expanduser() / "finagent")
    scripts_root = str(Path(__file__).resolve().parents[2])  # finagent_core -> agents -> scripts
    if scripts_root not in sys.path:
        sys.path.append(scripts_root)
    import fincept_paths
    return str(fincept_paths.data_root() / "finagent")


def persona_dir(user_id: str, agent_id: str) -> str:
    """Return the directory for one persona's state. Does not create it."""
    u = sanitize(user_id)
    a = sanitize(agent_id)
    return str(Path(base_dir()) / "users" / u / "personas" / a)


def ensure_persona_dir(user_id: str, agent_id: str) -> str:
    """Create the persona directory tree (if missing) and return the path."""
    d = persona_dir(user_id, agent_id)
    Path(d).mkdir(parents=True, exist_ok=True)
    return d


def sessions_db(user_id: str, agent_id: str) -> str:
    """Path to the SQLite file storing Agno session history for this persona."""
    return str(Path(persona_dir(user_id, agent_id)) / "sessions.db")


def memory_db(user_id: str, agent_id: str) -> str:
    """Path to the SQLite file for Agno long-term memory for this persona."""
    return str(Path(persona_dir(user_id, agent_id)) / "memory.db")


def knowledge_dir(user_id: str, agent_id: str) -> str:
    """Directory for this persona's knowledge-base vector store."""
    return str(Path(persona_dir(user_id, agent_id)) / "knowledge")


def agentic_memory_db(user_id: str, agent_id: str) -> str:
    """Path to the SQLite file for AgenticMemoryModule storage for this persona."""
    return str(Path(persona_dir(user_id, agent_id)) / "agentic_memory.db")
