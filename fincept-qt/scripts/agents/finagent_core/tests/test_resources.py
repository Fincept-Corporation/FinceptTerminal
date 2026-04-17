import os
from pathlib import Path

import pytest

from finagent_core import resources


def test_sanitize_allows_alphanumerics_underscore_dash():
    assert resources.sanitize("warren_buffett_agent") == "warren_buffett_agent"
    assert resources.sanitize("user-42") == "user-42"


def test_sanitize_replaces_unsafe_chars():
    assert resources.sanitize("user@host") == "user_host"
    assert resources.sanitize("a/b\\c") == "a_b_c"
    assert resources.sanitize("spaces here") == "spaces_here"


def test_sanitize_rejects_empty():
    with pytest.raises(ValueError):
        resources.sanitize("")
    with pytest.raises(ValueError):
        resources.sanitize("   ")


def test_base_dir_respects_env_var(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    # Force re-read
    base = resources.base_dir()
    assert Path(base) == tmp_path


def test_persona_dir_structure(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    d = resources.persona_dir("alice", "warren_buffett_agent")
    expected = tmp_path / "users" / "alice" / "personas" / "warren_buffett_agent"
    assert Path(d) == expected


def test_all_per_persona_files(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    u, a = "alice", "warren_buffett_agent"
    base = tmp_path / "users" / "alice" / "personas" / "warren_buffett_agent"
    assert Path(resources.sessions_db(u, a))       == base / "sessions.db"
    assert Path(resources.memory_db(u, a))         == base / "memory.db"
    assert Path(resources.knowledge_dir(u, a))     == base / "knowledge"
    assert Path(resources.agentic_memory_db(u, a)) == base / "agentic_memory.db"


def test_ensure_persona_dir_creates_tree(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    d = resources.ensure_persona_dir("alice", "warren_buffett_agent")
    assert Path(d).is_dir()


def test_two_personas_have_disjoint_paths(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    a1 = resources.memory_db("alice", "warren_buffett_agent")
    a2 = resources.memory_db("alice", "bill_ackman_agent")
    assert a1 != a2
