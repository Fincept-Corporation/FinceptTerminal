import importlib.util
import sys
import types
from pathlib import Path


def _has_module(name: str) -> bool:
    try:
        return importlib.util.find_spec(name) is not None
    except ModuleNotFoundError:
        return False


def _install_agno_agent_stub() -> None:
    agno = sys.modules.setdefault("agno", types.ModuleType("agno"))
    agno.__path__ = getattr(agno, "__path__", [])

    agent_mod = types.ModuleType("agno.agent")

    class Agent:
        def __init__(self, **kwargs):
            self.kwargs = kwargs

        def run(self, **kwargs):
            return kwargs

    agent_mod.Agent = Agent
    sys.modules["agno.agent"] = agent_mod


_HAS_AGNO_AGENT = _has_module("agno.agent")
_HAS_AGNO_TEAM = _has_module("agno.team")

if not _HAS_AGNO_AGENT:
    _install_agno_agent_stub()


def pytest_ignore_collect(collection_path, config):
    path = Path(str(collection_path))
    normalized = path.as_posix()
    if (
        path.name == "test_glm_model.py"
        and "renaissance_technologies_hedge_fund_agent/tests" in normalized
        and not _HAS_AGNO_TEAM
    ):
        return True
    return None
