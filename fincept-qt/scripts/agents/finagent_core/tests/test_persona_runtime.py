import os
import sys
import types
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from finagent_core.persona_runtime import PersonaRuntime
from finagent_core import persona_runtime


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

    fake_backend = MagicMock()
    rt.memory_backend = fake_backend
    rt._closed = False  # force re-enable for this test after stubbed build

    rt.close()
    rt.close()  # must not raise, must not double-close
    assert fake_backend.close.call_count == 1


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


def test_build_wires_knowledge_module(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    knowledge_module = MagicMock()
    knowledge_module.to_agent_config.return_value = {"knowledge": object()}
    agno_agent = MagicMock()

    with patch("finagent_core.persona_runtime._build_knowledge_module", return_value=knowledge_module) as build_knowledge, \
         patch("finagent_core.persona_runtime.build_agno_agent", return_value=agno_agent) as build_agent:
        rt = PersonaRuntime.build(
            "alice",
            "buffett",
            {"knowledge": {"type": "local"}},
            api_keys={"openai": "sk-test"},
        )

    assert rt.knowledge_module is knowledge_module
    build_knowledge.assert_called_once_with(
        {"type": "local"},
        "alice",
        "buffett",
        {"openai": "sk-test"},
    )
    assert (
        build_agent.call_args.kwargs["knowledge_agent_kwargs"]
        == knowledge_module.to_agent_config.return_value
    )


def test_build_continues_when_knowledge_module_unavailable(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    agno_agent = MagicMock()

    with patch("finagent_core.persona_runtime._build_knowledge_module", return_value=None), \
         patch("finagent_core.persona_runtime.build_agno_agent", return_value=agno_agent):
        rt = PersonaRuntime.build(
            "alice",
            "buffett",
            {"knowledge": {"type": "local"}},
            api_keys={},
        )

    assert rt.knowledge_module is None
    assert rt.agno_agent is agno_agent


def test_backend_builders_return_none_when_agno_backend_missing(monkeypatch, tmp_path):
    monkeypatch.setitem(sys.modules, "agno.memory", None)
    monkeypatch.setitem(sys.modules, "agno.storage.sqlite", None)

    assert persona_runtime._build_memory_backend(str(tmp_path / "memory.db")) is None
    assert persona_runtime._build_storage_backend(str(tmp_path / "sessions.db")) is None


def test_memory_backend_builds_with_configured_sqlite_db(monkeypatch, tmp_path):
    memory_mod = types.ModuleType("agno.memory")
    memory_mod.__path__ = []
    db_mod = types.ModuleType("agno.memory.db")
    db_mod.__path__ = []
    sqlite_mod = types.ModuleType("agno.memory.db.sqlite")

    class SqliteMemoryDb:
        def __init__(self, table_name, db_file):
            self.table_name = table_name
            self.db_file = db_file

    class AgentMemory:
        def __init__(self, db, create_user_memories, create_session_summary):
            self.db = db
            self.create_user_memories = create_user_memories
            self.create_session_summary = create_session_summary

    memory_mod.AgentMemory = AgentMemory
    sqlite_mod.SqliteMemoryDb = SqliteMemoryDb
    monkeypatch.setitem(sys.modules, "agno.memory", memory_mod)
    monkeypatch.setitem(sys.modules, "agno.memory.db", db_mod)
    monkeypatch.setitem(sys.modules, "agno.memory.db.sqlite", sqlite_mod)

    backend = persona_runtime._build_memory_backend(
        str(tmp_path / "memory.db"),
        table_name="custom_memory",
        create_user_memories=False,
        create_session_summary=False,
    )

    assert isinstance(backend, AgentMemory)
    assert backend.db.table_name == "custom_memory"
    assert backend.db.db_file == str(tmp_path / "memory.db")
    assert backend.create_user_memories is False
    assert backend.create_session_summary is False


def test_memory_backend_build_failure_returns_none(monkeypatch, tmp_path):
    memory_mod = types.ModuleType("agno.memory")
    memory_mod.__path__ = []
    db_mod = types.ModuleType("agno.memory.db")
    db_mod.__path__ = []
    sqlite_mod = types.ModuleType("agno.memory.db.sqlite")

    class AgentMemory:
        pass

    class SqliteMemoryDb:
        def __init__(self, table_name, db_file):
            raise RuntimeError("db failed")

    memory_mod.AgentMemory = AgentMemory
    sqlite_mod.SqliteMemoryDb = SqliteMemoryDb
    monkeypatch.setitem(sys.modules, "agno.memory", memory_mod)
    monkeypatch.setitem(sys.modules, "agno.memory.db", db_mod)
    monkeypatch.setitem(sys.modules, "agno.memory.db.sqlite", sqlite_mod)

    assert persona_runtime._build_memory_backend(str(tmp_path / "memory.db")) is None


def test_storage_backend_builds_with_configured_sqlite_db(monkeypatch, tmp_path):
    storage_mod = types.ModuleType("agno.storage")
    storage_mod.__path__ = []
    sqlite_mod = types.ModuleType("agno.storage.sqlite")

    class SqliteStorage:
        def __init__(self, table_name, db_file):
            self.table_name = table_name
            self.db_file = db_file

    sqlite_mod.SqliteStorage = SqliteStorage
    monkeypatch.setitem(sys.modules, "agno.storage", storage_mod)
    monkeypatch.setitem(sys.modules, "agno.storage.sqlite", sqlite_mod)

    backend = persona_runtime._build_storage_backend(
        str(tmp_path / "sessions.db"),
        table_name="custom_sessions",
    )

    assert isinstance(backend, SqliteStorage)
    assert backend.table_name == "custom_sessions"
    assert backend.db_file == str(tmp_path / "sessions.db")


def test_storage_backend_build_failure_returns_none(monkeypatch, tmp_path):
    storage_mod = types.ModuleType("agno.storage")
    storage_mod.__path__ = []
    sqlite_mod = types.ModuleType("agno.storage.sqlite")

    class SqliteStorage:
        def __init__(self, table_name, db_file):
            raise RuntimeError("storage failed")

    sqlite_mod.SqliteStorage = SqliteStorage
    monkeypatch.setitem(sys.modules, "agno.storage", storage_mod)
    monkeypatch.setitem(sys.modules, "agno.storage.sqlite", sqlite_mod)

    assert persona_runtime._build_storage_backend(str(tmp_path / "sessions.db")) is None


def test_knowledge_module_gets_persona_path_and_api_keys(tmp_path, monkeypatch):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    fake_module = MagicMock()

    with patch("finagent_core.modules.KnowledgeModule.from_config", return_value=fake_module) as from_config:
        out = persona_runtime._build_knowledge_module(
            {"type": "local"},
            user_id="alice",
            agent_id="buffett",
            api_keys={"openai": "sk-test"},
        )

    cfg = from_config.call_args.args[0]
    assert out is fake_module
    assert cfg["api_keys"] == {"openai": "sk-test"}
    assert Path(cfg["path"]) == tmp_path / "users" / "alice" / "personas" / "buffett" / "knowledge"


def test_knowledge_module_failure_returns_none(tmp_path, monkeypatch):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    with patch("finagent_core.modules.KnowledgeModule.from_config", side_effect=RuntimeError("bad config")):
        assert persona_runtime._build_knowledge_module({}, "alice", "buffett", {}) is None


def test_build_agno_agent_injects_existing_backends():
    memory_backend = object()
    storage_backend = object()
    knowledge_kwargs = {"knowledge": object()}

    with patch("finagent_core.core_agent.CoreAgent._create_agent", return_value="agent") as create_agent:
        out = persona_runtime.build_agno_agent(
            config={"model": {"provider": "openai"}},
            api_keys={"openai": "sk-test"},
            memory_backend=memory_backend,
            storage_backend=storage_backend,
            knowledge_agent_kwargs=knowledge_kwargs,
        )

    cfg = create_agent.call_args.args[0]
    assert out == "agent"
    assert cfg["storage"] == {}
    assert cfg["knowledge"] == {}


def test_run_forwards_session_and_stream_flags():
    agno_agent = MagicMock()
    agno_agent.run.return_value = "ok"
    rt = PersonaRuntime("alice", "buffett", agno_agent)

    assert rt.run("query", session_id="s1", stream=True) == "ok"
    agno_agent.run.assert_called_once_with(input="query", session_id="s1", stream=True)


def test_close_uses_available_lifecycle_method_and_ignores_errors():
    class ShutdownOnly:
        def __init__(self):
            self.calls = 0

        def shutdown(self):
            self.calls += 1

    class DisposeOnly:
        def __init__(self):
            self.calls = 0

        def dispose(self):
            self.calls += 1

    class FailingClose:
        def close(self):
            raise RuntimeError("close failed")

    memory_backend = ShutdownOnly()
    storage_backend = DisposeOnly()
    rt = PersonaRuntime(
        "alice",
        "buffett",
        MagicMock(),
        memory_backend=memory_backend,
        storage_backend=storage_backend,
        agentic_memory=FailingClose(),
    )

    rt.close()

    assert memory_backend.calls == 1
    assert storage_backend.calls == 1
    assert rt.agno_agent is None
