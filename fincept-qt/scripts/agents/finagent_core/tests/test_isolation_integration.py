"""
End-to-end: two personas run through CoreAgent; agentic memory written by
one is not recallable by the other.

Uses AgenticMemoryModule directly for the assertion (not the LLM) to avoid
network calls. The LLM-layer Agno memory is exercised in a separate manual
smoke test.
"""
from unittest.mock import patch, MagicMock

from finagent_core.core_agent import CoreAgent
from finagent_core.modules import AgenticMemoryModule


def test_personas_do_not_share_agentic_memory(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    # Stub Agno Agent construction — we don't want real LLM calls here.
    with patch("finagent_core.persona_runtime.build_agno_agent",
               return_value=MagicMock(run=MagicMock(return_value="ok"))):

        agent = CoreAgent(api_keys={})

        buffett_cfg = {
            "id": "warren_buffett_agent",
            "model": {"provider": "openai", "model_id": "gpt-4o", "api_key": "k"},
            "instructions": "Buffett",
            "memory": {"enabled": True},
        }
        ackman_cfg = {
            "id": "bill_ackman_agent",
            "model": {"provider": "openai", "model_id": "gpt-4o", "api_key": "k"},
            "instructions": "Ackman",
            "memory": {"enabled": True},
        }

        # Boot both runtimes (creates their AgenticMemoryModule instances).
        agent.run("hi", buffett_cfg, user_id="alice")
        agent.run("hi", ackman_cfg, user_id="alice")

    # Now write a memory into Buffett's module and try to read it from Ackman's.
    buffett_mem = AgenticMemoryModule.from_config(
        {"user_id": "alice", "agent_id": "warren_buffett_agent"}
    )
    ackman_mem = AgenticMemoryModule.from_config(
        {"user_id": "alice", "agent_id": "bill_ackman_agent"}
    )

    buffett_mem.store("Coca-Cola has a moat", memory_type="fact")
    ackman_mem.store("Netflix is overvalued", memory_type="fact")

    buffett_reads = [m["content"] for m in buffett_mem.recall(limit=10)]
    ackman_reads = [m["content"] for m in ackman_mem.recall(limit=10)]

    assert "Coca-Cola has a moat" in buffett_reads
    assert "Coca-Cola has a moat" not in ackman_reads
    assert "Netflix is overvalued" in ackman_reads
    assert "Netflix is overvalued" not in buffett_reads


def test_same_persona_same_user_sees_own_memory(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    m1 = AgenticMemoryModule.from_config(
        {"user_id": "alice", "agent_id": "warren_buffett_agent"}
    )
    m1.store("prefer long-term holds", memory_type="preference")

    m2 = AgenticMemoryModule.from_config(
        {"user_id": "alice", "agent_id": "warren_buffett_agent"}
    )
    reads = [m["content"] for m in m2.recall(limit=10)]
    assert "prefer long-term holds" in reads
