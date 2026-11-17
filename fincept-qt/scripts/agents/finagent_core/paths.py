"""
paths.py - where finagent_core keeps durable agent state.

Agentic Mode state (tasks, schedules, skills, archival memory, reflexions) lives
in ONE SQLite file, agent_tasks.db, inside a per-profile directory the terminal
passes down:

  * env FINCEPT_AGENT_DATA_DIR   - set by AgentService on every spawn
  * payload "agent_data_dir"     - main.py copies it into the env before dispatch

Both point at <profile data dir>/agents. The scripts directory is NOT a valid
home for it: dev builds prune *.db out of the synced scripts tree, installs keep
scripts next to the executable (read-only on macOS / Program Files, and shared by
every OS user and terminal profile).

The fallback (variable unset: run by hand from a CLI) is the default profile's
folder in the edition's data root (fincept_paths.data_root()/data/agents) -
never the read-only, shared scripts folder.

Resolved at call time, never at import time: main.py may set the variable after
these modules were imported.
"""

import os
import sys
from pathlib import Path

AGENT_DATA_DIR_ENV = "FINCEPT_AGENT_DATA_DIR"

# finagent_core/paths.py -> parents[2] is <scripts>
_SCRIPTS_DIR = Path(__file__).resolve().parents[2]


def _default_dir() -> Path:
    if str(_SCRIPTS_DIR) not in sys.path:
        sys.path.append(str(_SCRIPTS_DIR))
    import fincept_paths

    return fincept_paths.data_root() / "data" / "agents"


def agent_data_dir() -> Path:
    """Directory for durable agent state; created on demand."""
    raw = os.environ.get(AGENT_DATA_DIR_ENV, "").strip()
    path = Path(raw) if raw else _default_dir()
    path.mkdir(parents=True, exist_ok=True)
    return path


def agent_tasks_db_path() -> str:
    """Path of the shared agentic SQLite database (agent_tasks.db)."""
    return str(agent_data_dir() / "agent_tasks.db")
