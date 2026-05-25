import sqlite3
import sys
import types
from pathlib import Path

from finagent_core.modules.memory_module import AgenticMemoryModule, MemoryModule


def _memory(tmp_path, user_id="alice", agent_id="buffett", **kwargs):
    return AgenticMemoryModule(
        user_id=user_id,
        agent_id=agent_id,
        db_path=str(tmp_path / "memory.db"),
        **kwargs,
    )


def test_recall_filters_by_type_and_query_and_decodes_metadata(tmp_path):
    memory = _memory(tmp_path)
    memory.store(
        "AAPL margin expansion",
        memory_type="analysis",
        metadata={"symbol": "AAPL"},
        relevance_score=0.9,
    )
    memory.store("AAPL price alert", memory_type="alert", relevance_score=1.0)
    memory.store("MSFT margin expansion", memory_type="analysis", relevance_score=0.8)

    found = memory.recall(query="AAPL", memory_type="analysis", limit=5)

    assert [item["content"] for item in found] == ["AAPL margin expansion"]
    assert found[0]["metadata"] == {"symbol": "AAPL"}

    found_again = memory.recall(query="AAPL", memory_type="analysis", limit=5)
    assert found_again[0]["access_count"] == found[0]["access_count"] + 1


def test_update_relevance_is_scoped_to_persona(tmp_path):
    buffett = _memory(tmp_path, agent_id="buffett")
    ackman = _memory(tmp_path, agent_id="ackman")
    buffett_id = buffett.store("KO moat", relevance_score=0.2)
    ackman_id = ackman.store("NFLX short", relevance_score=0.4)

    ackman.update_relevance(buffett_id, 1.0)
    buffett.update_relevance(buffett_id, 0.7)

    with sqlite3.connect(buffett.db_path) as conn:
        rows = conn.execute(
            "SELECT id, agent_id, relevance_score FROM memories ORDER BY id"
        ).fetchall()

    assert rows == [(buffett_id, "buffett", 0.7), (ackman_id, "ackman", 0.4)]


def test_forget_is_scoped_to_persona(tmp_path):
    buffett = _memory(tmp_path, agent_id="buffett")
    ackman = _memory(tmp_path, agent_id="ackman")
    buffett_id = buffett.store("KO moat")
    ackman.store("NFLX short")

    ackman.forget(buffett_id)
    assert [item["content"] for item in buffett.recall(limit=5)] == ["KO moat"]

    buffett.forget(buffett_id)
    assert buffett.recall(limit=5) == []
    assert [item["content"] for item in ackman.recall(limit=5)] == ["NFLX short"]


def test_max_memories_enforced_per_persona(tmp_path):
    buffett = _memory(tmp_path, agent_id="buffett", max_memories=2)
    ackman = _memory(tmp_path, agent_id="ackman", max_memories=2)

    buffett.store("low", relevance_score=0.1)
    buffett.store("high", relevance_score=0.9)
    buffett.store("mid", relevance_score=0.5)
    ackman.store("other persona survives", relevance_score=0.1)

    assert [item["content"] for item in buffett.recall(limit=5)] == ["high", "mid"]
    assert [item["content"] for item in ackman.recall(limit=5)] == ["other persona survives"]


def test_context_helpers_include_relevant_financial_memory(tmp_path):
    memory = _memory(tmp_path)
    memory_id = memory.store_financial_context(
        "AAPL",
        "analysis",
        {"rating": "bullish", "reason": "cash generation"},
    )

    symbol_context = memory.get_symbol_context("AAPL", limit=5)
    context_text = memory.get_context_for_query("AAPL", limit=5)

    assert symbol_context[0]["id"] == memory_id
    assert symbol_context[0]["metadata"]["symbol"] == "AAPL"
    assert symbol_context[0]["metadata"]["context_type"] == "analysis"
    assert context_text.startswith("Relevant memories:")
    assert "- [analysis] AAPL:" in context_text


def test_context_helpers_return_empty_when_no_match(tmp_path):
    memory = _memory(tmp_path)
    assert memory.get_context_for_query("AAPL") == ""
    assert memory.get_symbol_context("AAPL") == []


def test_to_agent_config_enables_agentic_memory(tmp_path):
    memory = _memory(tmp_path)
    assert memory.to_agent_config() == {
        "enable_agentic_memory": True,
        "enable_user_memories": True,
        "add_memories_to_context": True,
    }


def test_from_config_uses_default_user_and_custom_options(tmp_path):
    db_path = tmp_path / "custom.db"
    memory = AgenticMemoryModule.from_config(
        {
            "agent_id": "buffett",
            "db_path": str(db_path),
            "enable_auto_store": False,
            "max_memories": 2,
        }
    )

    assert memory.user_id == "default"
    assert memory.agent_id == "buffett"
    assert memory.db_path == str(db_path)
    assert memory.enable_auto_store is False
    assert memory.max_memories == 2
    assert Path(memory.db_path).exists()


def test_memory_module_handles_missing_agno_backend(monkeypatch):
    monkeypatch.setitem(sys.modules, "agno.memory", None)
    memory = MemoryModule()

    assert memory._create_optimization_strategy() is None
    assert memory.get_memory_manager() is None
    assert memory.get_user_memory() is None
    memory.add_message("user", "hello", session_id="s1")
    assert memory.get_messages(session_id="s1") == []
    memory.clear_session("s1")
    assert memory.get_user_preference("theme", default="dark") == "dark"
    assert memory.to_agent_config() == {}
    assert MemoryModule.list_strategies() == ["summarize", "sliding_window", "token_limit"]


def test_memory_module_handles_missing_optimization_strategy_backend(monkeypatch):
    monkeypatch.setitem(sys.modules, "agno.memory", None)
    memory = MemoryModule(optimization_strategy="summarize")

    assert memory._create_optimization_strategy() is None


def test_memory_module_delegates_to_agno_memory_and_user_memory(monkeypatch, tmp_path):
    agno_memory = types.ModuleType("agno.memory")

    class MemoryOptimizationStrategyType:
        SUMMARIZE = "summarize"

    class MemoryOptimizationStrategyFactory:
        @staticmethod
        def create(strategy_type):
            return {"strategy": strategy_type}

    class MemoryManager:
        def __init__(self, **kwargs):
            self.kwargs = kwargs
            self.messages = []
            self.cleared = []

        def add_message(self, **kwargs):
            self.messages.append(kwargs)

        def get_messages(self, **kwargs):
            return [{"kwargs": kwargs, "messages": list(self.messages)}]

        def clear_session(self, session_id):
            self.cleared.append(session_id)

    class UserMemory:
        def __init__(self, user_id, db_path):
            self.user_id = user_id
            self.db_path = db_path
            self.values = {}

        def store(self, key, value, category=None):
            self.values[key] = {"value": value, "category": category}

        def get(self, key, default=None):
            return self.values.get(key, {}).get("value", default)

    agno_memory.MemoryManager = MemoryManager
    agno_memory.UserMemory = UserMemory
    agno_memory.MemoryOptimizationStrategyType = MemoryOptimizationStrategyType
    agno_memory.MemoryOptimizationStrategyFactory = MemoryOptimizationStrategyFactory
    monkeypatch.setitem(sys.modules, "agno.memory", agno_memory)

    memory = MemoryModule.from_config({
        "db_path": str(tmp_path / "memory.db"),
        "user_id": "alice",
        "enable_user_memory": True,
        "optimization_strategy": "summarize",
    })
    memory.add_message("user", "hello", session_id="s1", source="test")
    messages = memory.get_messages(session_id="s1", limit=1)
    memory.clear_session("s1")
    memory.store_user_preference("theme", "dark", category="ui")

    agent_config = memory.to_agent_config()

    assert memory.get_user_preference("theme") == "dark"
    assert memory.get_user_preference("missing", default="fallback") == "fallback"
    assert messages[0]["kwargs"] == {"session_id": "s1", "limit": 1}
    assert messages[0]["messages"][0]["content"] == "hello"
    assert memory._memory_manager.cleared == ["s1"]
    assert memory._memory_manager.kwargs == {"optimization_strategy": {"strategy": "summarize"}}
    assert agent_config == {
        "memory": memory._memory_manager,
        "user_memory": memory._user_memory,
    }


def test_memory_module_continues_when_user_memory_unavailable(monkeypatch, tmp_path):
    agno_memory = types.ModuleType("agno.memory")

    class MemoryManager:
        def __init__(self, **kwargs):
            self.kwargs = kwargs

    agno_memory.MemoryManager = MemoryManager
    monkeypatch.setitem(sys.modules, "agno.memory", agno_memory)

    memory = MemoryModule(
        db_path=str(tmp_path / "memory.db"),
        user_id="alice",
        enable_user_memory=True,
    )

    assert isinstance(memory.get_memory_manager(), MemoryManager)
    assert memory.get_user_memory() is None


def test_memory_module_logs_backend_and_operation_errors(monkeypatch, caplog):
    agno_memory = types.ModuleType("agno.memory")

    class MemoryOptimizationStrategyType:
        SUMMARIZE = "summarize"

    class MemoryOptimizationStrategyFactory:
        @staticmethod
        def create(strategy_type):
            raise RuntimeError("strategy failed")

    class BrokenMemoryManager:
        def add_message(self, **kwargs):
            raise RuntimeError("add failed")

        def get_messages(self, **kwargs):
            raise RuntimeError("get failed")

        def clear_session(self, session_id):
            raise RuntimeError("clear failed")

    class BrokenUserMemory:
        def store(self, **kwargs):
            raise RuntimeError("store failed")

        def get(self, **kwargs):
            raise RuntimeError("preference failed")

    agno_memory.MemoryOptimizationStrategyType = MemoryOptimizationStrategyType
    agno_memory.MemoryOptimizationStrategyFactory = MemoryOptimizationStrategyFactory
    monkeypatch.setitem(sys.modules, "agno.memory", agno_memory)

    assert MemoryModule(optimization_strategy="summarize")._create_optimization_strategy() is None

    memory = MemoryModule()
    memory._initialized = True
    memory._memory_manager = BrokenMemoryManager()
    memory._user_memory = BrokenUserMemory()

    memory.add_message("user", "hello")
    assert memory.get_messages() == []
    memory.clear_session("s1")
    memory.store_user_preference("theme", "dark")
    assert memory.get_user_preference("theme", default="fallback") == "fallback"

    assert "strategy failed" in caplog.text
    assert "add failed" in caplog.text
    assert "get failed" in caplog.text
    assert "clear failed" in caplog.text
    assert "store failed" in caplog.text
    assert "preference failed" in caplog.text


def test_memory_module_initialization_error_is_logged(monkeypatch, caplog):
    agno_memory = types.ModuleType("agno.memory")

    class MemoryManager:
        def __init__(self, **kwargs):
            raise RuntimeError("init failed")

    agno_memory.MemoryManager = MemoryManager
    monkeypatch.setitem(sys.modules, "agno.memory", agno_memory)

    memory = MemoryModule()

    assert memory.get_memory_manager() is None
    assert "init failed" in caplog.text


def test_agentic_memory_database_errors_return_safe_defaults(tmp_path):
    bad_db_path = tmp_path / "not-a-db-dir"
    bad_db_path.mkdir()
    memory = AgenticMemoryModule("alice", "buffett", db_path=str(bad_db_path))

    assert memory.store("cannot write") == -1
    assert memory.recall() == []
    memory.update_relevance(1, 0.5)
    memory.forget(1)
