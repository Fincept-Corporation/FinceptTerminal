import pytest

from finagent_core.modules.memory_module import AgenticMemoryModule


def test_init_requires_agent_id():
    with pytest.raises(TypeError):
        AgenticMemoryModule(user_id="alice")  # missing agent_id


def test_from_config_requires_agent_id():
    with pytest.raises(ValueError, match="agent_id"):
        AgenticMemoryModule.from_config({"user_id": "alice"})


def test_same_user_different_agents_are_isolated(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    buffett = AgenticMemoryModule.from_config(
        {"user_id": "alice", "agent_id": "warren_buffett_agent"}
    )
    ackman = AgenticMemoryModule.from_config(
        {"user_id": "alice", "agent_id": "bill_ackman_agent"}
    )

    buffett.store("prefer wonderful businesses", memory_type="preference")
    ackman.store("short poorly-run companies", memory_type="preference")

    buffett_recalls = buffett.recall(memory_type="preference", limit=10)
    ackman_recalls = ackman.recall(memory_type="preference", limit=10)

    buffett_content = [m["content"] for m in buffett_recalls]
    ackman_content = [m["content"] for m in ackman_recalls]

    assert "prefer wonderful businesses" in buffett_content
    assert "short poorly-run companies" not in buffett_content
    assert "short poorly-run companies" in ackman_content
    assert "prefer wonderful businesses" not in ackman_content


def test_explicit_db_path_overrides_default(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    custom = tmp_path / "custom.db"
    mod = AgenticMemoryModule.from_config(
        {"user_id": "alice", "agent_id": "x", "db_path": str(custom)}
    )
    assert mod.db_path == str(custom)
    mod.store("hello", memory_type="fact")
    assert custom.exists()
