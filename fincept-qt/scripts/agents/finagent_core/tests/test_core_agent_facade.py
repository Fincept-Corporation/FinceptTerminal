from unittest.mock import MagicMock, patch

import pytest

from finagent_core.core_agent import CoreAgent


def _cfg(agent_id="warren_buffett_agent"):
    return {
        "id": agent_id,
        "model": {"provider": "openai", "model_id": "gpt-4o", "api_key": "k"},
        "instructions": "test",
    }


def test_run_routes_to_registry(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    agent = CoreAgent(api_keys={})

    with patch.object(agent._registry, "get_or_create") as mock_get:
        mock_rt = MagicMock()
        mock_rt.run.return_value = "ok"
        mock_get.return_value = mock_rt

        out = agent.run("hi", _cfg("buffett"), agent_id="buffett", user_id="alice")

    assert out == "ok"
    call = mock_get.call_args
    assert call.kwargs["user_id"] == "alice"
    assert call.kwargs["agent_id"] == "buffett"


def test_run_infers_agent_id_from_config(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    agent = CoreAgent(api_keys={})

    with patch.object(agent._registry, "get_or_create") as mock_get:
        mock_get.return_value = MagicMock(run=MagicMock(return_value="ok"))
        agent.run("q", _cfg("buffett"))

    assert mock_get.call_args.kwargs["agent_id"] == "buffett"


def test_run_defaults_user_id_to_default(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    agent = CoreAgent(api_keys={})

    with patch.object(agent._registry, "get_or_create") as mock_get:
        mock_get.return_value = MagicMock(run=MagicMock(return_value="ok"))
        agent.run("q", _cfg())

    assert mock_get.call_args.kwargs["user_id"] == "default"


def test_different_personas_get_different_runtimes(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    agent = CoreAgent(api_keys={})

    rt1 = MagicMock(run=MagicMock(return_value="r1"))
    rt2 = MagicMock(run=MagicMock(return_value="r2"))

    def _pick(user_id, agent_id, config, api_keys):
        return rt1 if agent_id == "buffett" else rt2

    with patch.object(agent._registry, "get_or_create", side_effect=_pick):
        r1 = agent.run("q", _cfg("buffett"))
        r2 = agent.run("q", _cfg("ackman"))

    assert r1 == "r1"
    assert r2 == "r2"
