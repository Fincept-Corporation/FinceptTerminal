"""
Smoke test: every persona JSON in the 4 canonical agent groups can be
booted through CoreAgent.run without raising (Agno Agent construction stubbed).
"""
import json
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from finagent_core.core_agent import CoreAgent


CONFIGS = [
    "TraderInvestorsAgent/configs/agent_definitions.json",
    "GeopoliticsAgents/configs/agent_definitions.json",
    "EconomicAgents/configs/agent_definitions.json",
]


def _scripts_agents_dir() -> Path:
    # tests live in scripts/agents/finagent_core/tests/; the persona groups are
    # siblings of finagent_core under scripts/agents/. (The entries used to start
    # with "agents/", which resolved to scripts/agents/agents/... — every file
    # was missing, the parameter set was empty and the test never ran.)
    return Path(__file__).resolve().parents[2]


def test_persona_configs_found():
    """Guard against the path drifting again: an empty parameter set is a skip,
    not a failure, so assert the files are really there."""
    missing = [rel for rel in CONFIGS if not (_scripts_agents_dir() / rel).exists()]
    assert not missing, f"persona config files not found: {missing}"


def _load_all_personas():
    base = _scripts_agents_dir()
    out = []
    for rel in CONFIGS:
        p = base / rel
        if not p.exists():
            continue
        d = json.loads(p.read_text(encoding="utf-8"))
        for a in d.get("agents", []):
            out.append(a)
    return out


@pytest.mark.parametrize("persona", _load_all_personas(),
                         ids=lambda a: a.get("id", "?"))
def test_persona_config_boots(monkeypatch, tmp_path, persona):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))

    persona = dict(persona)
    persona.setdefault("model", {}).setdefault("api_key", "test")
    # Force memory + knowledge off so the smoke test doesn't try to open
    # real vector DBs. Isolation is covered by other tests.
    persona["memory"] = False
    persona.pop("knowledge", None)

    with patch("finagent_core.persona_runtime.build_agno_agent",
               return_value=MagicMock(run=MagicMock(return_value="ok"))):
        agent = CoreAgent(api_keys={})
        out = agent.run("hello", persona, user_id="default")
    assert out == "ok"
