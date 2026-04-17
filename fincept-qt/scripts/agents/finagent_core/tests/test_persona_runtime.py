import os
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from finagent_core.persona_runtime import PersonaRuntime


def _minimal_config(with_memory=False):
    cfg = {
        "model": {"provider": "openai", "model_id": "gpt-4o", "api_key": "test"},
        "instructions": "test",
    }
    if with_memory:
        cfg["memory"] = {"enabled": True}
    return cfg


def test_build_creates_persona_dir(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    with patch("finagent_core.persona_runtime.build_agno_agent") as mock_build:
        mock_build.return_value = MagicMock()
        rt = PersonaRuntime.build(
            user_id="alice",
            agent_id="warren_buffett_agent",
            config=_minimal_config(),
            api_keys={},
        )

    assert Path(tmp_path / "users" / "alice" / "personas" / "warren_buffett_agent").is_dir()
    assert rt.user_id == "alice"
    assert rt.agent_id == "warren_buffett_agent"


def test_memory_enabled_creates_persona_scoped_files(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    with patch("finagent_core.persona_runtime.build_agno_agent") as mock_build, \
         patch("finagent_core.persona_runtime._build_memory_backend") as mock_mem, \
         patch("finagent_core.persona_runtime._build_storage_backend") as mock_sto:
        mock_build.return_value = MagicMock()
        mock_mem.return_value = MagicMock()
        mock_sto.return_value = MagicMock()

        rt = PersonaRuntime.build(
            user_id="alice",
            agent_id="warren_buffett_agent",
            config=_minimal_config(with_memory=True),
            api_keys={},
        )

    mem_call = mock_mem.call_args
    sto_call = mock_sto.call_args
    assert "warren_buffett_agent" in mem_call.kwargs["db_path"]
    assert "alice" in mem_call.kwargs["db_path"]
    assert "warren_buffett_agent" in sto_call.kwargs["db_path"]


def test_close_is_idempotent(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    with patch("finagent_core.persona_runtime.build_agno_agent") as mock_build:
        mock_build.return_value = MagicMock()
        rt = PersonaRuntime.build("u", "a", _minimal_config(), api_keys={})

    rt.close()
    rt.close()  # must not raise


def test_two_personas_same_user_use_different_paths(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    with patch("finagent_core.persona_runtime.build_agno_agent") as mock_build, \
         patch("finagent_core.persona_runtime._build_memory_backend") as mock_mem:
        mock_build.return_value = MagicMock()
        mock_mem.return_value = MagicMock()

        PersonaRuntime.build("alice", "warren_buffett_agent", _minimal_config(with_memory=True), api_keys={})
        PersonaRuntime.build("alice", "bill_ackman_agent", _minimal_config(with_memory=True), api_keys={})

    paths = [c.kwargs["db_path"] for c in mock_mem.call_args_list]
    assert paths[0] != paths[1]
