from types import SimpleNamespace

from finagent_core.super_agent import SuperAgent


def test_execute_missing_model_error_mentions_programmatic_path(monkeypatch):
    monkeypatch.setitem(
        __import__("sys").modules,
        "finagent_core.core_agent",
        SimpleNamespace(CoreAgent=object),
    )
    monkeypatch.setitem(
        __import__("sys").modules,
        "finagent_core.agent_loader",
        SimpleNamespace(get_loader=lambda: None),
    )

    agent = SuperAgent(api_keys={})

    result = agent.execute("Analyze AAPL")

    assert result["success"] is False
    assert "Settings > LLM Configuration" in result["error"]
    assert 'user_config={"model": {' in result["error"]
