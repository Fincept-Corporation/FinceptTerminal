"""
A requested tool that cannot work here (package missing, or no API key) is
swapped for its free substitute — Tavily -> DuckDuckGo — and the run says so in
its `warnings`. Synthetic modules stand in for both integrations, so the result
does not depend on what this interpreter has installed. No network, no LLM.
"""
import sys
import textwrap
from types import SimpleNamespace
from unittest.mock import patch

import pytest

NOTE = "web search: using DuckDuckGo because Tavily is not configured"


@pytest.fixture(autouse=True)
def _isolated(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path / "finagent"))
    monkeypatch.setenv("FINCEPT_AGENT_DATA_DIR", str(tmp_path / "agents"))
    monkeypatch.delenv("TAVILY_API_KEY", raising=False)


@pytest.fixture
def fake_search(tmp_path, monkeypatch):
    """zzfake_search.{paid_ok,paid_missing,free_ok,free_missing} on sys.path."""
    pkg = tmp_path / "zzfake_search"
    pkg.mkdir()
    (pkg / "__init__.py").write_text("")
    ok = "class {name}:\n    def __init__(self, api_key=None):\n        self.api_key = api_key\n"
    missing = textwrap.dedent("""
        try:
            import zz_not_installed_search_pkg
        except ImportError:
            raise ImportError("`zz-search` not installed. Please install using `pip install zz-search`")

        class {name}:
            pass
    """)
    (pkg / "paid_ok.py").write_text(ok.format(name="PaidSearchTools"))
    (pkg / "paid_missing.py").write_text(missing.format(name="PaidSearchTools"))
    (pkg / "free_ok.py").write_text(ok.format(name="FreeSearchTools"))
    (pkg / "free_missing.py").write_text(missing.format(name="FreeSearchTools"))
    monkeypatch.syspath_prepend(str(tmp_path))
    from finagent_core.registries import availability
    monkeypatch.setattr(availability, "_cache", {})
    monkeypatch.setattr(availability, "_static_cache", {})
    yield "zzfake_search"
    for name in [m for m in sys.modules if m.startswith("zzfake_search")]:
        del sys.modules[name]


def _catalog(pkg, tavily="paid_missing", duckduckgo="free_ok"):
    """Patch the registry so 'tavily' / 'duckduckgo' load the synthetic classes."""
    catalog = {"search": {"tavily": f"{pkg}.{tavily}.PaidSearchTools",
                          "duckduckgo": f"{pkg}.{duckduckgo}.FreeSearchTools"}}
    from finagent_core.registries import ToolsRegistry
    return patch.multiple(ToolsRegistry, TOOL_CATALOG=catalog, _tool_map={}, _loaded_tools={})


def _resolve(names, api_keys=None):
    from finagent_core.registries import ToolsRegistry
    notes = []
    tools, problems = ToolsRegistry.resolve_tools(names, api_keys=api_keys, notes=notes)
    return [type(t).__name__ for t in tools], problems, notes


def test_missing_tavily_package_uses_duckduckgo(fake_search):
    with _catalog(fake_search):
        tools, problems, notes = _resolve(["tavily"], api_keys={"TAVILY_API_KEY": "k"})
    assert tools == ["FreeSearchTools"]
    assert problems == {}
    assert notes == [NOTE]


def test_tavily_without_key_uses_duckduckgo(fake_search):
    with _catalog(fake_search, tavily="paid_ok"):
        tools, problems, notes = _resolve(["tavily", "duckduckgo"], api_keys={"OTHER_KEY": "x"})
    # Requested anyway: loaded once, in its own place, and the swap is still noted.
    assert tools == ["FreeSearchTools"]
    assert problems == {} and notes == [NOTE]


def test_configured_tavily_is_kept(fake_search, monkeypatch):
    with _catalog(fake_search, tavily="paid_ok"):
        tools, problems, notes = _resolve(["tavily"], api_keys={"TAVILY_API_KEY": "k"})
        assert tools == ["PaidSearchTools"] and problems == {} and notes == []
        monkeypatch.setenv("TAVILY_API_KEY", "from-env")  # the environment counts as configured too
        tools, problems, notes = _resolve(["tavily"])
        assert tools == ["PaidSearchTools"] and problems == {} and notes == []


def test_without_duckduckgo_tavily_is_reported_as_before(fake_search):
    with _catalog(fake_search, duckduckgo="free_missing"):
        tools, problems, notes = _resolve(["tavily"])
    assert tools == [] and notes == []
    assert set(problems) == {"tavily"} and "zz-search" in problems["tavily"]


def test_run_warnings_carry_the_substitution_note(fake_search):
    from agno.agent import Agent
    from finagent_core.main import dispatch_action
    ok = SimpleNamespace(status=SimpleNamespace(value="COMPLETED"), content="answer")
    config = {"model": {"provider": "openai", "model_id": "gpt-4o-mini", "api_key": "test"},
              "tools": ["tavily", "duckduckgo"]}
    with _catalog(fake_search), patch.object(Agent, "run", return_value=ok):
        out = dispatch_action("run", {}, {"query": "hi"}, config)
    assert out["success"] is True and out["response"] == "answer"
    assert out["warnings"] == [NOTE]


def test_installed_agno_substitutes_its_duckduckgo_tools():
    """The real catalogue entries: when Tavily cannot work here and agno's
    DuckDuckGo toolkit can load, a Tavily request gets DuckDuckGoTools
    (constructing it makes no network call)."""
    from finagent_core.registries import ToolsRegistry
    if ToolsRegistry.unavailable_reason("duckduckgo"):
        pytest.skip("agno's DuckDuckGo toolkit cannot load here (needs the `ddgs` package)")
    if ToolsRegistry.unavailable_reason("tavily") is None:
        pytest.skip("tavily-python is installed here; without a key it is still substituted (covered above)")
    tools, problems, notes = _resolve(["tavily"])
    assert tools == ["DuckDuckGoTools"] and problems == {} and notes == [NOTE]
