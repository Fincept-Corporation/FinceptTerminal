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
    "agents/TraderInvestorsAgent/configs/agent_definitions.json",
    "agents/GeopoliticsAgents/configs/agent_definitions.json",
    "agents/EconomicAgents/configs/agent_definitions.json",
]


def _scripts_agents_dir() -> Path:
    # tests live in finagent_core/tests/; agents/ is sibling of finagent_core.
    return Path(__file__).resolve().parents[2]


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
