"""
Memory, storage and knowledge on the installed agno 2.x API.

The 1.x classes the code used (agno.memory.AgentMemory, agno.storage.*,
agno.knowledge.reader.PDFReader, embedders taking `model=`) no longer exist, so
all three options were silently off. These tests build the real agno objects
(no network, no LLM; a deterministic fake embedder stands in for the API).
"""
import hashlib
import importlib.util
import math
from pathlib import Path

import pytest

from finagent_core.run_errors import AgentFeatureUnavailable


@pytest.fixture(autouse=True)
def _isolated(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path / "finagent"))
    monkeypatch.setenv("FINCEPT_AGENT_DATA_DIR", str(tmp_path / "agents"))
    for var in ("OPENAI_API_KEY", "GOOGLE_API_KEY", "COHERE_API_KEY"):
        monkeypatch.delenv(var, raising=False)


# ── memory / storage ─────────────────────────────────────────────────────────

def test_memory_backend_stores_and_reads_memories(tmp_path):
    from agno.db.sqlite import SqliteDb
    from agno.memory import MemoryManager, UserMemory
    from finagent_core.persona_runtime import _build_memory_backend

    db_file = tmp_path / "mem.db"
    mm = _build_memory_backend(db_path=str(db_file), table_name="agent_memory")
    assert isinstance(mm, MemoryManager) and isinstance(mm.db, SqliteDb)
    mm.add_user_memory(UserMemory(memory="Prefers dividend stocks"), user_id="alice")

    reread = _build_memory_backend(db_path=str(db_file))  # a later process
    memories = reread.get_user_memories(user_id="alice")
    assert [m.memory for m in memories] == ["Prefers dividend stocks"]
    assert reread.get_user_memories(user_id="bob") in ([], None)


def test_storage_backend_is_agno2_db(tmp_path):
    from agno.db.sqlite import SqliteDb
    from finagent_core.persona_runtime import _build_storage_backend
    db = _build_storage_backend(db_path=str(tmp_path / "s.db"), table_name="my_sessions")
    assert isinstance(db, SqliteDb) and db.session_table_name == "my_sessions"


def test_postgres_storage_without_url_is_a_clear_error(tmp_path):
    from finagent_core.persona_runtime import _build_storage_backend
    with pytest.raises(AgentFeatureUnavailable, match="db_url"):
        _build_storage_backend(db_path=str(tmp_path / "x.db"), storage_type="postgres")


def test_persona_agent_gets_memory_and_session_db():
    from agno.db.sqlite import SqliteDb
    from finagent_core import resources
    from finagent_core.persona_runtime import PersonaRuntime

    cfg = {"model": {"provider": "openai", "model_id": "gpt-4o-mini", "api_key": "test"},
           "instructions": "t", "memory": True}
    rt = PersonaRuntime.build("alice", "memo_agent", cfg, api_keys={})
    agent = rt.agno_agent
    assert agent.enable_agentic_memory is True
    assert isinstance(agent.db, SqliteDb)
    assert agent.db.db_file == resources.sessions_db("alice", "memo_agent")
    assert agent.memory_manager.db.db_file == resources.memory_db("alice", "memo_agent")
    rt.close()


def test_relative_memory_path_lands_in_persona_dir():
    from finagent_core import resources
    from finagent_core.persona_runtime import _persona_path
    p = _persona_path("alice", "a1", "agent_memory.db", "unused")
    assert p.startswith(resources.persona_dir("alice", "a1"))


# ── embedders ────────────────────────────────────────────────────────────────

def test_embedder_takes_id_not_model():
    from finagent_core.registries import EmbedderRegistry
    emb = EmbedderRegistry.create_embedder("openai", model="text-embedding-3-large", api_key="k")
    assert emb.id == "text-embedding-3-large"
    oll = EmbedderRegistry.create_embedder("ollama")
    assert oll.id == "nomic-embed-text" and oll.host == "http://localhost:11434" and oll.dimensions == 768


def test_embedder_key_found_by_provider_name():
    from finagent_core.registries import EmbedderRegistry
    assert EmbedderRegistry.resolve_api_key("openai", {"openai": "k1"}) == "k1"
    assert EmbedderRegistry.missing_requirement("openai", {}) is not None
    assert EmbedderRegistry.missing_requirement("openai", {"OPENAI_API_KEY": "k"}) is None


# ── knowledge ────────────────────────────────────────────────────────────────

def _fake_embedder():
    from agno.knowledge.embedder.base import Embedder

    class HashEmbedder(Embedder):
        """Bag-of-words vectors: shared words -> high cosine similarity."""

        def __init__(self):
            super().__init__(dimensions=64)

        def get_embedding(self, text):
            v = [0.0] * 64
            for w in text.lower().split():
                v[int(hashlib.md5(w.strip(".,").encode()).hexdigest(), 16) % 64] += 1.0
            n = math.sqrt(sum(x * x for x in v)) or 1.0
            return [x / n for x in v]

        def get_embedding_and_usage(self, text):
            return self.get_embedding(text), None

    return HashEmbedder()


def _module(tmp_path, documents, **kw):
    from finagent_core.modules import KnowledgeModule
    m = KnowledgeModule.from_config({"documents": documents, "path": str(tmp_path / "kb"), **kw})
    m._embedder = _fake_embedder()
    return m


def test_knowledge_indexes_files_and_searches(tmp_path):
    a = tmp_path / "dividends.txt"
    a.write_text("Dividend policy: the company pays a quarterly dividend of 25 cents.")
    b = tmp_path / "factory.txt"
    b.write_text("The new factory in Ohio doubles battery output next year.")
    m = _module(tmp_path, [{"source": str(a), "type": "text"}, {"source": str(b), "type": "text"}])
    cfg = m.to_agent_config()
    from agno.knowledge.knowledge import Knowledge
    from finagent_core.registries.local_vectordb import LocalVectorDb
    assert isinstance(cfg["knowledge"], Knowledge)
    assert isinstance(cfg["knowledge"].vector_db, LocalVectorDb)
    hits = m.search("quarterly dividend", limit=1)
    assert hits and "dividend" in hits[0]["content"].lower()
    assert Path(tmp_path / "kb" / "knowledge_vectors.db").exists()


def test_knowledge_missing_embedder_key_is_a_clear_error(tmp_path):
    from finagent_core.modules import KnowledgeModule
    m = KnowledgeModule.from_config({"documents": [{"source": "https://example.com", "type": "url"}],
                                     "path": str(tmp_path / "kb")})
    with pytest.raises(AgentFeatureUnavailable, match="OPENAI_API_KEY"):
        m.to_agent_config()


def test_knowledge_unreadable_sources_is_a_clear_error(tmp_path):
    m = _module(tmp_path, [{"source": str(tmp_path / "missing.pdf"), "type": "pdf"}])
    with pytest.raises(AgentFeatureUnavailable, match="could be read"):
        m.to_agent_config()


@pytest.mark.skipif(importlib.util.find_spec("lancedb") is not None, reason="lancedb is installed here")
def test_ui_default_lancedb_falls_back_to_local_store_with_warning(tmp_path):
    f = tmp_path / "n.txt"
    f.write_text("Revenue grew 12 percent.")
    m = _module(tmp_path, [{"source": str(f), "type": "text"}], vectordb={"type": "lancedb"})
    m.to_agent_config()
    assert m.vectordb_type == "local"
    assert any("lancedb" in w for w in m.warnings)


@pytest.mark.skipif(importlib.util.find_spec("qdrant_client") is not None, reason="qdrant-client is installed here")
def test_unavailable_vector_db_names_the_package(tmp_path):
    m = _module(tmp_path, [{"source": "https://example.com", "type": "url"}], vectordb={"type": "qdrant"})
    with pytest.raises(AgentFeatureUnavailable, match="qdrant"):
        m.to_agent_config()


def test_ui_knowledge_config_reaches_the_agent(tmp_path, monkeypatch):
    """CREATE-tab shape (knowledge: true + knowledge_config) -> a built agent
    with a searchable knowledge base."""
    from finagent_core.persona_runtime import PersonaRuntime
    from finagent_core.registries import EmbedderRegistry

    doc = tmp_path / "memo.txt"
    doc.write_text("Board memo: buyback of 2 billion approved.")
    monkeypatch.setattr(EmbedderRegistry, "create_embedder", classmethod(lambda cls, **kw: _fake_embedder()))
    cfg = {"model": {"provider": "openai", "model_id": "gpt-4o-mini", "api_key": "test"},
           "instructions": "t", "knowledge": True,
           "knowledge_config": {"type": "text", "urls": [str(doc)], "vector_db": "lancedb",
                                "embedder": "openai"}}
    from finagent_core.core_agent import normalize_ui_config
    rt = PersonaRuntime.build("u", "kb_agent", normalize_ui_config(cfg), api_keys={"OPENAI_API_KEY": "k"})
    assert rt.agno_agent.knowledge is not None and rt.agno_agent.search_knowledge is True
    assert rt.knowledge_module.search("buyback")[0]["content"].startswith("Board memo")
