from unittest.mock import MagicMock, patch
import logging

import pytest

from finagent_core.persona_registry import PersonaRegistry


def _cfg():
    return {"model": {"provider": "openai", "model_id": "x", "api_key": "k"},
            "instructions": ""}


def test_cap_default_is_8(monkeypatch):
    monkeypatch.delenv("FINAGENT_RUNTIME_CACHE_SIZE", raising=False)
    reg = PersonaRegistry()
    assert reg.cap == 8


def test_cap_respects_env(monkeypatch):
    monkeypatch.setenv("FINAGENT_RUNTIME_CACHE_SIZE", "3")
    reg = PersonaRegistry()
    assert reg.cap == 3


def test_invalid_cap_env_falls_back_to_default(monkeypatch, caplog):
    monkeypatch.setenv("FINAGENT_RUNTIME_CACHE_SIZE", "many")
    reg = PersonaRegistry()
    assert reg.cap == 8
    assert "not an int" in caplog.text


def test_cap_clamped_to_at_least_one(monkeypatch, caplog):
    monkeypatch.setenv("FINAGENT_RUNTIME_CACHE_SIZE", "0")
    reg = PersonaRegistry()
    assert reg.cap == 1
    assert "clamping to 1" in caplog.text


def test_get_or_create_caches(monkeypatch):
    reg = PersonaRegistry()
    with patch("finagent_core.persona_registry.PersonaRuntime") as mock_rt:
        mock_rt.build.return_value = MagicMock()
        r1 = reg.get_or_create("u", "a", _cfg(), api_keys={})
        r2 = reg.get_or_create("u", "a", _cfg(), api_keys={})
    assert r1 is r2
    assert mock_rt.build.call_count == 1


def test_eviction_calls_close(monkeypatch):
    monkeypatch.setenv("FINAGENT_RUNTIME_CACHE_SIZE", "2")
    reg = PersonaRegistry()
    closed = []
    def _mk_runtime(user_id, agent_id, config, api_keys):
        rt = MagicMock()
        rt.close = lambda a=agent_id: closed.append(a)
        return rt
    with patch("finagent_core.persona_registry.PersonaRuntime") as mock_rt:
        mock_rt.build.side_effect = lambda user_id, agent_id, config, api_keys: _mk_runtime(
            user_id, agent_id, config, api_keys
        )
        reg.get_or_create("u", "a1", _cfg(), api_keys={})
        reg.get_or_create("u", "a2", _cfg(), api_keys={})
        reg.get_or_create("u", "a3", _cfg(), api_keys={})  # evicts a1
    assert closed == ["a1"]


def test_eviction_close_error_does_not_abort(monkeypatch, caplog):
    monkeypatch.setenv("FINAGENT_RUNTIME_CACHE_SIZE", "1")
    reg = PersonaRegistry()
    with patch("finagent_core.persona_registry.PersonaRuntime") as mock_rt:
        first = MagicMock()
        first.close.side_effect = RuntimeError("boom")
        second = MagicMock()
        mock_rt.build.side_effect = [first, second]

        assert reg.get_or_create("u", "a1", _cfg(), api_keys={}) is first
        assert reg.get_or_create("u", "a2", _cfg(), api_keys={}) is second

    assert len(reg) == 1
    assert ("u", "a2") in reg
    assert "close() raised boom" in caplog.text


def test_access_updates_lru_order(monkeypatch):
    monkeypatch.setenv("FINAGENT_RUNTIME_CACHE_SIZE", "2")
    reg = PersonaRegistry()
    closed = []
    with patch("finagent_core.persona_registry.PersonaRuntime") as mock_rt:
        mock_rt.build.side_effect = lambda user_id, agent_id, config, api_keys: MagicMock(
            close=lambda a=agent_id: closed.append(a)
        )
        reg.get_or_create("u", "a1", _cfg(), api_keys={})
        reg.get_or_create("u", "a2", _cfg(), api_keys={})
        reg.get_or_create("u", "a1", _cfg(), api_keys={})  # refresh a1 → a2 is now LRU
        reg.get_or_create("u", "a3", _cfg(), api_keys={})  # evicts a2
    assert closed == ["a2"]


def test_clear_closes_all():
    reg = PersonaRegistry()
    closed = []
    with patch("finagent_core.persona_registry.PersonaRuntime") as mock_rt:
        mock_rt.build.side_effect = lambda user_id, agent_id, config, api_keys: MagicMock(
            close=lambda a=agent_id: closed.append(a)
        )
        reg.get_or_create("u", "a1", _cfg(), api_keys={})
        reg.get_or_create("u", "a2", _cfg(), api_keys={})
    reg.clear()
    assert set(closed) == {"a1", "a2"}
    assert len(reg) == 0


def test_clear_close_error_does_not_abort(caplog):
    caplog.set_level(logging.DEBUG, logger="finagent_core.persona_registry")
    reg = PersonaRegistry()
    with patch("finagent_core.persona_registry.PersonaRuntime") as mock_rt:
        first = MagicMock()
        first.close.side_effect = RuntimeError("boom")
        second = MagicMock()
        mock_rt.build.side_effect = [first, second]
        reg.get_or_create("u", "a1", _cfg(), api_keys={})
        reg.get_or_create("u", "a2", _cfg(), api_keys={})

    reg.clear()
    assert len(reg) == 0
    assert "close() raised boom" in caplog.text
